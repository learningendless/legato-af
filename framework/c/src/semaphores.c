/**
 * @file semaphores.c
 *
 * Legato @ref c_semaphore implementation.
 *
 * Each semaphore is represented by a <b> Semaphore object </b>.  They are dynamically allocated
 * from the <b> Semaphore Pool </b> and are stored on the <b> Semaphore List </b> until they are
 * destroyed.
 *
 * In addition, each thread has a <b> Per-Thread Semaphore Record </b>, which is kept in the
 * Thread object inside the thread module and is fetched through a call to
 * thread_GetSemaphoreRecPtr().
 * That Per-Thread Semaphore Record holds a pointer to a semaphore that the thread is waiting on
 * (or NULL if not waiting on a semaphore).
 *
 * Some of the tricky features of the Semaphore have to do with the diagnostic capabilities
 * provided by command-line tools.  That is, the command-line tools can ask:
 *  -# What semaphore is a given thread currently waiting on?
 *    - A single semaphore reference per thread keeps track of this (NULL if not waiting).
 *  -# What semaphores currently exist in the process?
 *    - A single per-process list of all semaphores keeps track of this (the Semaphore List).
 *  -# What threads, if any, are currently waiting on a given semaphore?
 *    - Each Semaphore object has a list of Per-Thread Semaphore Records for this.
 *  -# What type of semaphore is a given semaphore? (traceable?)
 *    - These are stored in each Semaphore object as boolean flags.
 *
 * The command-line tools communicate with the semaphore module using IPC datagram messages.
 * This file implements handling functions for those messages and sends back responses.
 *
 * @todo Implement the command-line diagnostic tools and the IPC messaging between them and
 *       the semaphore module.
 *
 * @todo Implement the traceable semaphores.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to license.
 */

#include "legato.h"
#include "limit.h"
#include "semaphores.h"
#include "thread.h"

#include <pthread.h>
#include <semaphore.h>

// ==============================
//  PRIVATE DATA
// ==============================

/// Number of objects in the Semaphore Pool to start with.
/// TODO: Change this to be configurable per-process.
#define DEFAULT_POOL_SIZE 4


//--------------------------------------------------------------------------------------------------
/**
 * Semaphore object.
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_sem_t
{
    le_dls_Link_t       semaphoreListLink;   ///< Used to link onto the process's Semaphore List.
    le_dls_List_t       waitingList;         ///< List of threads waiting for this semaphore.
    pthread_mutex_t     waitingListMutex;    ///< Pthreads mutex used to protect the waiting list.
    bool                isTraceable;         ///< true if traceable, false otherwise.
    sem_t               semaphore;           ///< Pthreads semaphore that does the real work. :)
    char                nameStr[LIMIT_MAX_SEMAPHORE_NAME_BYTES]; ///< The name of the semaphore (UTF8 string).
}
Semaphore_t;

//--------------------------------------------------------------------------------------------------
/**
 * Semaphore Pool.
 *
 * Memory pool from which Semaphore objects are allocated.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t SemaphorePoolRef;

//--------------------------------------------------------------------------------------------------
/**
 * Semaphore List.
 *
 * List on which all Semaphore objects in the process are kept.
 */
//--------------------------------------------------------------------------------------------------
static le_dls_List_t SemaphoreList = LE_DLS_LIST_INIT;

//--------------------------------------------------------------------------------------------------
/**
 * Semaphore List Mutex.
 *
 * Basic pthreads mutex used to protect the Semaphore List from multi-threaded race conditions.
 * Helper macro functions LOCK_MUTEX_LIST() and UNLOCK_MUTEX_LIST() are provided to lock and unlock
 * this mutex respectively.
 */
//--------------------------------------------------------------------------------------------------
static pthread_mutex_t SemaphoreListMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;


// ==============================
//  PRIVATE FUNCTIONS
// ==============================

/// Lock the Semaphore List Mutex.
#define LOCK_SEMAPHORE_LIST()   LE_ASSERT(pthread_mutex_lock(&SemaphoreListMutex) == 0)

/// Unlock the Semaphore List Mutex.
#define UNLOCK_SEMAPHORE_LIST() LE_ASSERT(pthread_mutex_unlock(&SemaphoreListMutex) == 0)


/// Lock a semaphore's Waiting List Mutex.
#define LOCK_WAITING_LIST(semaphorePtr) \
LE_ASSERT(pthread_mutex_lock(&(semaphorePtr)->waitingListMutex) == 0)

/// Unlock a semaphore's Waiting List Mutex.
#define UNLOCK_WAITING_LIST(semaphorePtr) \
LE_ASSERT(pthread_mutex_unlock(&(semaphorePtr)->waitingListMutex) == 0)

//--------------------------------------------------------------------------------------------------
/**
 * Creates a semaphore.
 *
 * @return  Returns a reference to the semaphore.
 *
 * @note Terminates the process on failure, so no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_sem_Ref_t CreateSemaphore
(
    const char* nameStr,
    int         initialCount,
    bool        isTraceable
)
//--------------------------------------------------------------------------------------------------
{
    // Allocate a semaphore object and initialize it.
    Semaphore_t* semaphorePtr = le_mem_ForceAlloc(SemaphorePoolRef);
    semaphorePtr->semaphoreListLink = LE_DLS_LINK_INIT;
    semaphorePtr->waitingList = LE_DLS_LIST_INIT;
    pthread_mutex_init(&semaphorePtr->waitingListMutex, NULL);  // Default attributes = Fast mutex.
    semaphorePtr->isTraceable = isTraceable;

    // Initialize the underlying POSIX semaphore shared between thread.
    int result = sem_init(&semaphorePtr->semaphore,0, initialCount);
    if (result != 0)
    {
        LE_FATAL("Failed to set the semaphore . errno = %d (%m).", errno);
    }

    // Add the semaphore to the process's Semaphore List.
    LOCK_SEMAPHORE_LIST();
    le_dls_Queue(&SemaphoreList, &semaphorePtr->semaphoreListLink);
    UNLOCK_SEMAPHORE_LIST();

    return semaphorePtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Adds a thread's Semaphore Record to a Semaphore object's waiting list.
 */
//--------------------------------------------------------------------------------------------------
static void AddToWaitingList
(
    Semaphore_t*        semaphorePtr,
    sem_ThreadRec_t*    perThreadRecPtr
)
//--------------------------------------------------------------------------------------------------
{
    LOCK_WAITING_LIST(semaphorePtr);

    le_dls_Queue(&semaphorePtr->waitingList, &perThreadRecPtr->waitingListLink);

    UNLOCK_WAITING_LIST(semaphorePtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Removes a thread's Semaphore Record from a Semaphore object's waiting list.
 */
//--------------------------------------------------------------------------------------------------
static void RemoveFromWaitingList
(
    Semaphore_t*        semaphorePtr,
    sem_ThreadRec_t*    perThreadRecPtr
)
//--------------------------------------------------------------------------------------------------
{
    LOCK_WAITING_LIST(semaphorePtr);

    le_dls_Remove(&semaphorePtr->waitingList, &perThreadRecPtr->waitingListLink);

    UNLOCK_WAITING_LIST(semaphorePtr);
}

// ==============================
//  INTRA-FRAMEWORK FUNCTIONS
// ==============================

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Semaphore module.
 *
 * This function must be called exactly once at process start-up before any other semaphore module
 * functions are called.
 */
//--------------------------------------------------------------------------------------------------
void sem_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    SemaphorePoolRef = le_mem_CreatePool("semaphore", sizeof(Semaphore_t));
    le_mem_ExpandPool(SemaphorePoolRef, DEFAULT_POOL_SIZE);
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the thread-specific parts of the semaphore module.
 *
 * This function must be called once by each thread when it starts, before any other semaphore module
 * functions are called by that thread.
 */
//--------------------------------------------------------------------------------------------------
void sem_ThreadInit
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    sem_ThreadRec_t* perThreadRecPtr = thread_GetSemaphoreRecPtr();

    perThreadRecPtr->waitingOnSemaphore  = NULL;
    perThreadRecPtr->waitingListLink     = LE_DLS_LINK_INIT;

    // TODO: Register a thread destructor function to check that everything has been cleaned up
    //       properly.  (Is it possible to release semaphores inside the thread destructors?)
}


// ==============================
//  PUBLIC API FUNCTIONS
// ==============================
//--------------------------------------------------------------------------------------------------
/**
 * Create a semaphore shared by threads within the same process
 *
 * @return Upon successful completion, it shall return reference to the semaphore, otherwise,
 * assert with LE_FATAL and log.
 */
//--------------------------------------------------------------------------------------------------
le_sem_Ref_t le_sem_Create
(
    const char*     name,               ///< [IN] Name of the semaphore
    int32_t         initialCount        ///< [IN] initial number of semaphore
)
{
    return CreateSemaphore(name,initialCount, false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a traceable semaphore shared by threads within the same process
 *
 * @return Upon successful completion, it shall return reference to the semaphore, otherwise,
 * assert with LE_FATAL and log.
 */
//--------------------------------------------------------------------------------------------------
le_sem_Ref_t le_sem_CreateTraceable
(
    const char*     name,               ///< [IN] Name of the semaphore
    int32_t         initialCount        ///< [IN] initial number of semaphore
)
{
    return CreateSemaphore(name,initialCount, true);

    // TODO: Implement tracing.
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a semaphore
 *
 */
//--------------------------------------------------------------------------------------------------
void le_sem_Delete
(
    le_sem_Ref_t    semaphorePtr   ///< [IN] Pointer to the semaphore
)
{
    // TODO: Implement traceable semaphore deletion.

    // Remove the Semaphore object from the Semaphore List.
    LOCK_SEMAPHORE_LIST();
    le_dls_Remove(&SemaphoreList, &semaphorePtr->semaphoreListLink);
    UNLOCK_SEMAPHORE_LIST();

    LOCK_WAITING_LIST(semaphorePtr);
    if ( le_dls_Peek(&semaphorePtr->waitingList)==NULL ) {
        UNLOCK_WAITING_LIST(semaphorePtr);
        if (pthread_mutex_destroy(&semaphorePtr->waitingListMutex) != 0)
        {
            LE_FATAL(   "Semaphore '%s' could not destroy internal mutex!",
                        semaphorePtr->nameStr);
        }
            // Destroy the semaphore.
        if (sem_destroy(&semaphorePtr->semaphore) != 0)
        {
            LE_FATAL(   "Semaphore '%s' is not a valid semaphore!",
                        semaphorePtr->nameStr);
        }
    } else {
        UNLOCK_WAITING_LIST(semaphorePtr);
        // TODO print more information
        LE_FATAL("Semaphore '%s' deleted while threads are still waiting for it!",
                 semaphorePtr->nameStr);
    }


    // Release the semaphore object back to the Semaphore Pool.
    le_mem_Release(semaphorePtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Finds a semaphore given the semaphore's name.
 *
 * @return
 *      Reference to the semaphore, or NULL if the semaphore doesn't exist.
 */
//--------------------------------------------------------------------------------------------------
le_sem_Ref_t le_sem_FindSemaphore
(
    const char* name    ///< [IN] The name of the semaphore.
)
{
    // TODO
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Wait for a semaphore.
 *
 * @return Nothing.
 */
//--------------------------------------------------------------------------------------------------
void le_sem_Wait
(
    le_sem_Ref_t    semaphorePtr   ///< [IN] Pointer to the semaphore
)
{
//     TODO: Implement this:
//     if (semaphorePtr->isTraceable)
//     {
//     }
//     else
    {
        int result;

        sem_ThreadRec_t* perThreadRecPtr = thread_GetSemaphoreRecPtr();

        perThreadRecPtr->waitingOnSemaphore = semaphorePtr;
        AddToWaitingList(semaphorePtr, perThreadRecPtr);

        result = sem_wait(&semaphorePtr->semaphore);

        RemoveFromWaitingList(semaphorePtr, perThreadRecPtr);
        perThreadRecPtr->waitingOnSemaphore = NULL;

        LE_FATAL_IF( (result!=0), "Thread '%s' failed to wait on semaphore '%s'. Error code %d (%m).",
                    le_thread_GetMyName(),
                    semaphorePtr->nameStr,
                    result);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Try to wait for a semaphore.
 *
 * it is the same as @ref le_sem_Wait, exept that if the decrement cannot be immediately performes,
 * then call returns an LE_WOULD_BLOCK instead of blocking
 *
 * @return Upon successful completion, it shall return LE_OK (0), otherwise,
 * LE_WOULD_BLOCK as the call would block if it was a blocking call.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sem_TryWait
(
    le_sem_Ref_t    semaphorePtr   ///< [IN] Pointer to the semaphore
)
{
//     TODO: Implement this.
//     if (semaphorePtr->isTraceable)
//     {
//     }
//     else
    {
        int result;

        result = sem_trywait(&semaphorePtr->semaphore);

        if (result != 0)
        {
            if ( errno == EAGAIN ) {
                return LE_WOULD_BLOCK;
            } else {
                LE_FATAL("Thread '%s' failed to trywait on semaphore '%s'. Error code %d (%m).",
                        le_thread_GetMyName(),
                        semaphorePtr->nameStr,
                        result);
            }
        }
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Wait for a semaphore with a limit on how long to wait.
 *
 * @return
 *      - LE_OK         The function succeed
 *      - LE_TIMEOUT    timeToWait elapsed
 *
 * @note When LE_TIMEOUT occurs the semaphore is not decremented.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sem_WaitWithTimeOut
(
    le_sem_Ref_t    semaphorePtr,   ///< [IN] Pointer to the semaphore.
    le_clk_Time_t   timeToWait      ///< [IN] Time to wait
)
{
//     TODO: Implement this.
//     if (semaphorePtr->isTraceable)
//     {
//     }
//     else
    {
        struct timespec timeOut;
        int result;

        // Prepare the timer
        le_clk_Time_t currentUtcTime = le_clk_GetAbsoluteTime();
        le_clk_Time_t wakeUpTime = le_clk_Add(currentUtcTime,timeToWait);
        timeOut.tv_sec = wakeUpTime.sec;
        timeOut.tv_nsec = wakeUpTime.usec;

        // Retrieve reference thread
        sem_ThreadRec_t* perThreadRecPtr = thread_GetSemaphoreRecPtr();
        // Save into waiting list
        perThreadRecPtr->waitingOnSemaphore = semaphorePtr;
        AddToWaitingList(semaphorePtr, perThreadRecPtr);

        result = sem_timedwait(&semaphorePtr->semaphore,&timeOut);

        // Remove from waiting list
        RemoveFromWaitingList(semaphorePtr, perThreadRecPtr);
        perThreadRecPtr->waitingOnSemaphore = NULL;

        if (result != 0)
        {
            if ( errno == ETIMEDOUT ) {
                return LE_TIMEOUT;
            } else {
                LE_FATAL("Thread '%s' failed to wait on semaphore '%s'. Error code %d (%m).",
                        le_thread_GetMyName(),
                        semaphorePtr->nameStr,
                        result);
            }
        }
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Post a semaphore.
 *
 * @return Nothing.
 */
//--------------------------------------------------------------------------------------------------
void le_sem_Post
(
    le_sem_Ref_t    semaphorePtr      ///< [IN] Pointer to the semaphore
)
{
    //     TODO: Implement this.
    //     if (semaphorePtr->isTraceable)
    //     {
    //     }
    //     else
    {
        int result;

        result = sem_post (&semaphorePtr->semaphore);

        LE_FATAL_IF((result!=0),"Failed to post on semaphore '%s'. Errno = %d (%m).",
                    semaphorePtr->nameStr,
                    result);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the value of a semaphore.
 *
 * @return value of the semaphore
 */
//--------------------------------------------------------------------------------------------------
int le_sem_GetValue
(
    le_sem_Ref_t    semaphorePtr   ///< [IN] Pointer to the semaphore
)
{
    int value;

    if ( sem_getvalue(&(semaphorePtr->semaphore),&value) != 0)
    {
        LE_FATAL("Cannot get %s semaphore value",semaphorePtr->nameStr);
    }

    return value;
}
