/** @file mutex.c
 *
 * Legato @ref c_mutex implementation.
 *
 * Each mutex is represented by a <b> Mutex object </b>.  They are dynamically allocated from the
 * <b> Mutex Pool </b> and are stored on the <b> Mutex List </b> until they are destroyed.
 *
 * In addition, each thread has a <b> Per-Thread Mutex Record </b>, which is kept in the Thread
 * object inside the thread module and is fetched through a call to thread_GetMutexRecPtr().
 * That Per-Thread Mutex Record holds a pointer to a mutex that the thread is waiting on
 * (or NULL if not waiting on a mutex).  It also holds a list of mutexes that the thread
 * currently holds the lock for.
 *
 * Some of the tricky features of the Mutexes have to do with the diagnostic capabilities provided
 * by command-line tools.  That is, the command-line tools can ask:
 *  -# What mutexes are currently held by a given thread?
 *    - To support this, a list of locked mutexes is kept per-thread.
 *  -# What mutex is a given thread currently waiting on?
 *    - A single mutex reference per thread keeps track of this (NULL if not waiting).
 *  -# What mutexes currently exist in the process?
 *    - A single per-process list of all mutexes keeps track of this (the Mutex List).
 *  -# What threads, if any, are currently waiting on a given mutex?
 *    - Each Mutex object has a list of Per-Thread Mutex Records for this.
 *  -# What thread holds the lock on a given mutex?
 *    - Each Mutex object has a single thread reference for this (NULL if no one holds the lock).
 *  -# What is a given mutex's lock count?
 *    - Each Mutex object keeps track of its lock count.
 *  -# What type of mutex is a given mutex? (recursive? traceable?)
 *    - These are stored in each Mutex object as boolean flags.
 *
 * The command-line tools communicate with the mutex module using IPC datagram messages.
 * This file implements handling functions for those messages and sends back responses.
 *
 * @todo Implement the command-line diagnostic tools and the IPC messaging between them and
 *       the mutex module.
 *
 * @todo Implement the traceable mutexes.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to license.
 */

#include "legato.h"
#include "limit.h"
#include "mutex.h"
#include "thread.h"

#include <pthread.h>


// ==============================
//  PRIVATE DATA
// ==============================

/// Maximum number of bytes in a mutex name (including null terminator).
#define MAX_NAME_BYTES 24

/// Number of objects in the Mutex Pool to start with.
/// TODO: Change this to be configurable per-process.
#define DEFAULT_POOL_SIZE 4


//--------------------------------------------------------------------------------------------------
/**
 * Mutex object.
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_mutex
{
    le_dls_Link_t       mutexListLink;      ///< Used to link onto the process's Mutex List.
    le_thread_Ref_t     lockingThreadRef;   ///< Reference to the thread that holds the lock.
    le_dls_Link_t       lockedByThreadLink; ///< Used to link onto the thread's locked mutexes list.
    le_dls_List_t       waitingList;        ///< List of threads waiting for this mutex.
    pthread_mutex_t     waitingListMutex;   ///< Pthreads mutex used to protect the waiting list.
    bool                isTraceable;        ///< true if traceable, false otherwise.
    bool                isRecursive;        ///< true if recursive, false otherwise.
    int                 lockCount;      ///< Number of lock calls not yet matched by unlock calls.
    pthread_mutex_t     mutex;          ///< Pthreads mutex that does the real work. :)
    char                name[MAX_NAME_BYTES]; ///< The name of the mutex (UTF8 string).
}
Mutex_t;

//--------------------------------------------------------------------------------------------------
/**
 * Mutex Pool.
 *
 * Memory pool from which Mutex objects are allocated.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t MutexPoolRef;

//--------------------------------------------------------------------------------------------------
/**
 * Mutex List.
 *
 * List on which all Mutex objects in the process are kept.
 */
//--------------------------------------------------------------------------------------------------
static le_dls_List_t MutexList = LE_DLS_LIST_INIT;

//--------------------------------------------------------------------------------------------------
/**
 * Mutex List Mutex.
 *
 * Basic pthreads mutex used to protect the Mutex List from multi-threaded race conditions.
 * Helper macro functions LOCK_MUTEX_LIST() and UNLOCK_MUTEX_LIST() are provided to lock and unlock
 * this mutex respectively.
 */
//--------------------------------------------------------------------------------------------------
static pthread_mutex_t MutexListMutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;


// ==============================
//  PRIVATE FUNCTIONS
// ==============================

/// Lock the Mutex List Mutex.
#define LOCK_MUTEX_LIST()   LE_ASSERT(pthread_mutex_lock(&MutexListMutex) == 0)

/// Unlock the Mutex List Mutex.
#define UNLOCK_MUTEX_LIST() LE_ASSERT(pthread_mutex_unlock(&MutexListMutex) == 0)


/// Lock a mutex's Waiting List Mutex.
#define LOCK_WAITING_LIST(mutexPtr) \
            LE_ASSERT(pthread_mutex_lock(&(mutexPtr)->waitingListMutex) == 0)

/// Unlock a mutex's Waiting List Mutex.
#define UNLOCK_WAITING_LIST(mutexPtr) \
            LE_ASSERT(pthread_mutex_unlock(&(mutexPtr)->waitingListMutex) == 0)


//--------------------------------------------------------------------------------------------------
/**
 * Creates a mutex.
 *
 * @return  Returns a reference to the mutex.
 *
 * @note Terminates the process on failure, so no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_mutex_Ref_t CreateMutex
(
    const char* nameStr,
    bool        isRecursive,
    bool        isTraceable
)
//--------------------------------------------------------------------------------------------------
{
    // Allocate a Mutex object and initialize it.
    Mutex_t* mutexPtr = le_mem_ForceAlloc(MutexPoolRef);
    mutexPtr->mutexListLink = LE_DLS_LINK_INIT;
    mutexPtr->lockingThreadRef = NULL;
    mutexPtr->lockedByThreadLink = LE_DLS_LINK_INIT;
    mutexPtr->waitingList = LE_DLS_LIST_INIT;
    pthread_mutex_init(&mutexPtr->waitingListMutex, NULL);  // Default attributes = Fast mutex.
    mutexPtr->isTraceable = isTraceable;
    mutexPtr->isRecursive = isRecursive;
    mutexPtr->lockCount = 0;
    if (le_utf8_Copy(mutexPtr->name, nameStr, sizeof(mutexPtr->name), NULL) == LE_OVERFLOW)
    {
        LE_WARN("Mutex name '%s' truncated to '%s'.", nameStr, mutexPtr->name);
    }

    // Initialize the underlying POSIX mutex according to whether the mutex is recursive or not.
    pthread_mutexattr_t mutexAttrs;
    pthread_mutexattr_init(&mutexAttrs);
    int mutexType;
    if (isRecursive)
    {
        mutexType = PTHREAD_MUTEX_RECURSIVE_NP;
    }
    else
    {
        mutexType = PTHREAD_MUTEX_ERRORCHECK_NP;
    }
    int result = pthread_mutexattr_settype(&mutexAttrs, mutexType);
    if (result != 0)
    {
        LE_FATAL("Failed to set the mutex type to %d.  errno = %d (%m).", mutexType, errno);
    }
    pthread_mutex_init(&mutexPtr->mutex, &mutexAttrs);
    pthread_mutexattr_destroy(&mutexAttrs);

    // Add the mutex to the process's Mutex List.
    LOCK_MUTEX_LIST();
    le_dls_Queue(&MutexList, &mutexPtr->mutexListLink);
    UNLOCK_MUTEX_LIST();

    return mutexPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Adds a thread's Mutex Record to a Mutex object's waiting list.
 */
//--------------------------------------------------------------------------------------------------
static void AddToWaitingList
(
    Mutex_t*            mutexPtr,
    mutex_ThreadRec_t*  perThreadRecPtr
)
//--------------------------------------------------------------------------------------------------
{
    LOCK_WAITING_LIST(mutexPtr);

    le_dls_Queue(&mutexPtr->waitingList, &perThreadRecPtr->waitingListLink);

    UNLOCK_WAITING_LIST(mutexPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Removes a thread's Mutex Record from a Mutex object's waiting list.
 */
//--------------------------------------------------------------------------------------------------
static void RemoveFromWaitingList
(
    Mutex_t*            mutexPtr,
    mutex_ThreadRec_t*  perThreadRecPtr
)
//--------------------------------------------------------------------------------------------------
{
    LOCK_WAITING_LIST(mutexPtr);

    le_dls_Remove(&mutexPtr->waitingList, &perThreadRecPtr->waitingListLink);

    UNLOCK_WAITING_LIST(mutexPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Mark a mutex "locked".
 *
 * This updates all the data structures to reflect the fact that this mutex was just locked
 * by the calling thread.
 *
 * @note    Assumes that the lock count has already been updated before this function was called.
 *
 * @warning Assumes that the calling thread already holds the pthreads mutex lock.
 */
//--------------------------------------------------------------------------------------------------
static void MarkLocked
(
    mutex_ThreadRec_t*  perThreadRecPtr,    ///< [in] Pointer to the thread's mutex info record.
    Mutex_t*            mutexPtr            ///< [in] Pointer to the Mutex object that was locked.
)
//--------------------------------------------------------------------------------------------------
{
    // Push it onto the calling thread's list of locked mutexes.
    // NOTE: Mutexes tend to be locked and unlocked in a nested manner, so treat this like a stack.
    le_dls_Stack(&perThreadRecPtr->lockedMutexList, &mutexPtr->lockedByThreadLink);

    // Record the current thread in the Mutex object as the thread that currently holds the lock.
    mutexPtr->lockingThreadRef = le_thread_GetCurrent();
}


//--------------------------------------------------------------------------------------------------
/**
 * Mark a mutex "unlocked".
 *
 * This updates all the data structures to reflect the fact that this mutex is about to be unlocked
 * by the calling thread.
 *
 * @note    Assumes that the lock count has already been updated before this function was called.
 *
 * @warning Assumes that the calling thread actually still holds the pthreads mutex lock.
 */
//--------------------------------------------------------------------------------------------------
static void MarkUnlocked
(
    Mutex_t* mutexPtr
)
//--------------------------------------------------------------------------------------------------
{
    mutex_ThreadRec_t* perThreadRecPtr = thread_GetMutexRecPtr();

    // Remove it the calling thread's list of locked mutexes.
    // TODO: Should we warn if mutexes are not unlocked in the reverse order of being locked?
    le_dls_Remove(&perThreadRecPtr->lockedMutexList, &mutexPtr->lockedByThreadLink);

    // Record in the Mutex object that no thread currently holds the lock.
    mutexPtr->lockingThreadRef = NULL;
}


// ==============================
//  INTRA-FRAMEWORK FUNCTIONS
// ==============================

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Mutex module.
 *
 * This function must be called exactly once at process start-up before any other mutex module
 * functions are called.
 */
//--------------------------------------------------------------------------------------------------
void mutex_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    MutexPoolRef = le_mem_CreatePool("mutex", sizeof(Mutex_t));
    le_mem_ExpandPool(MutexPoolRef, DEFAULT_POOL_SIZE);
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the thread-specific parts of the mutex module.
 *
 * This function must be called once by each thread when it starts, before any other mutex module
 * functions are called by that thread.
 */
//--------------------------------------------------------------------------------------------------
void mutex_ThreadInit
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    mutex_ThreadRec_t* perThreadRecPtr = thread_GetMutexRecPtr();

    perThreadRecPtr->waitingOnMutex = NULL;
    perThreadRecPtr->lockedMutexList = LE_DLS_LIST_INIT;
    perThreadRecPtr->waitingListLink = LE_DLS_LINK_INIT;

    // TODO: Register a thread destructor function to check that everything has been cleaned up
    //       properly.  (Is it possible to release mutexes inside the thread destructors?)
}


// ==============================
//  PUBLIC API FUNCTIONS
// ==============================

//--------------------------------------------------------------------------------------------------
/**
 * Create a Normal, Recursive mutex
 *
 * @return  Returns a reference to the mutex.
 *
 * @note Terminates the process on failure, so no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_mutex_Ref_t le_mutex_CreateRecursive
(
    const char* nameStr     ///< [in] Name of the mutex
)
//--------------------------------------------------------------------------------------------------
{
    return CreateMutex(nameStr, true, false);
}


//--------------------------------------------------------------------------------------------------
/**
 * Create a Normal, Non-Recursive mutex
 *
 * @return  Returns a reference to the mutex.
 *
 * @note Terminates the process on failure, so no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_mutex_Ref_t le_mutex_CreateNonRecursive
(
    const char* nameStr     ///< [in] Name of the mutex
)
//--------------------------------------------------------------------------------------------------
{
    return CreateMutex(nameStr, false, false);
}


//--------------------------------------------------------------------------------------------------
/**
 * Create a Traceable, Recursive mutex
 *
 * @return  Returns a reference to the mutex.
 *
 * @note Terminates the process on failure, so no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_mutex_Ref_t le_mutex_CreateTraceableRecursive
(
    const char* nameStr     ///< [in] Name of the mutex
)
//--------------------------------------------------------------------------------------------------
{
    return CreateMutex(nameStr, true, true);

    // TODO: Implement tracing.
}


//--------------------------------------------------------------------------------------------------
/**
 * Create a Traceable, Non-Recursive mutex
 *
 * @return  Returns a reference to the mutex.
 *
 * @note Terminates the process on failure, so no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_mutex_Ref_t le_mutex_CreateTraceableNonRecursive
(
    const char* nameStr     ///< [in] Name of the mutex
)
//--------------------------------------------------------------------------------------------------
{
    return CreateMutex(nameStr, false, true);

    // TODO: Implement tracing.
}


//--------------------------------------------------------------------------------------------------
/**
 * Delete a mutex
 *
 * @return  Nothing.
 */
//--------------------------------------------------------------------------------------------------
void le_mutex_Delete
(
    le_mutex_Ref_t    mutexRef   ///< [in] Mutex reference
)
//--------------------------------------------------------------------------------------------------
{
    // TODO: Implement traceable mutex deletion.

    // Remove the Mutex object from the Mutex List.
    LOCK_MUTEX_LIST();
    le_dls_Remove(&MutexList, &mutexRef->mutexListLink);
    UNLOCK_MUTEX_LIST();

    if (mutexRef->lockingThreadRef != NULL)

    // Destroy the pthreads mutex.
    if (pthread_mutex_destroy(&mutexRef->mutex) != 0)
    {
        char threadName[LIMIT_MAX_THREAD_NAME_BYTES];
        le_thread_GetName(mutexRef->lockingThreadRef, threadName, sizeof(threadName));
        LE_FATAL(   "Mutex '%s' deleted while still locked by thread '%s'!",
                    mutexRef->name,
                    threadName  );
    }

    // Release the Mutex object back to the Mutex Pool.
    le_mem_Release(mutexRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Lock a mutex
 *
 * @return  Nothing.
 */
//--------------------------------------------------------------------------------------------------
void le_mutex_Lock
(
    le_mutex_Ref_t    mutexRef   ///< [in] Mutex reference
)
//--------------------------------------------------------------------------------------------------
{
    /* TODO: Implement this:
    if (mutexRef->isTraceable)
    {
        LockTraceable(mutexRef);
    }
    else
    */
    {
        int result;

        mutex_ThreadRec_t* perThreadRecPtr = thread_GetMutexRecPtr();

        perThreadRecPtr->waitingOnMutex = mutexRef;
        AddToWaitingList(mutexRef, perThreadRecPtr);

        result = pthread_mutex_lock(&mutexRef->mutex);

        RemoveFromWaitingList(mutexRef, perThreadRecPtr);
        perThreadRecPtr->waitingOnMutex = NULL;

        if (result == 0)
        {
            // Got the lock!

            // NOTE: the lock count is protected by the mutex itself.  That is, it can never be
            //       updated by anyone who doesn't hold the lock on the mutex.

            // If the mutex wasn't already locked by this thread before, we need to update
            // the data structures to indicate that it now holds the lock.
            if (mutexRef->lockCount == 0)
            {
                MarkLocked(perThreadRecPtr, mutexRef);
            }

            // Update the lock count.
            mutexRef->lockCount++;
        }
        else
        {
            if (result == EDEADLK)
            {
                LE_FATAL("DEADLOCK DETECTED! Thread '%s' attempting to re-lock mutex '%s'.",
                         le_thread_GetMyName(),
                         mutexRef->name);
            }
            else
            {
                LE_FATAL("Thread '%s' failed to lock mutex '%s'. Error code %d (%m).",
                         le_thread_GetMyName(),
                         mutexRef->name,
                         result );
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Try a lock on a mutex
 *
 * Locks a mutex, if no other thread holds the mutex.  Otherwise, returns without locking.
 *
 * @return
 *  - LE_OK if mutex was locked.
 *  - LE_WOULD_BLOCK if mutex was already held by someone else.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_mutex_TryLock
(
    le_mutex_Ref_t    mutexRef   ///< [in] Mutex reference
)
//--------------------------------------------------------------------------------------------------
{
    /* TODO: Implement this.
    if (mutexRef->isTraceable)
    {
        return TryLockTraceable(mutexRef);
    }
    else
    */
    {
        int result = pthread_mutex_trylock(&mutexRef->mutex);

        if (result == 0)
        {
            // Got the lock!

            // NOTE: the lock count is protected by the mutex itself.  That is, it can never be
            //       updated by anyone who doesn't hold the lock on the mutex.

            // If the mutex wasn't already locked by this thread before, we need to update
            // the data structures to indicate that it now holds the lock.
            if (mutexRef->lockCount == 0)
            {
                MarkLocked(thread_GetMutexRecPtr(), mutexRef);
            }

            // Update the lock count.
            mutexRef->lockCount++;
        }
        else if (result == EBUSY)
        {
            // The mutex is already held by someone else.
            return LE_WOULD_BLOCK;
        }
        else
        {
            LE_FATAL("Thread '%s' failed to trylock mutex '%s'. Error code %d (%m).",
                     le_thread_GetMyName(),
                     mutexRef->name,
                     result );
        }

        return LE_OK;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Unlock a mutex
 *
 * @return  Nothing.
 */
//--------------------------------------------------------------------------------------------------
void le_mutex_Unlock
(
    le_mutex_Ref_t    mutexRef   ///< [in] Mutex reference
)
//--------------------------------------------------------------------------------------------------
{
    /* TODO: Implement this.
    if (mutexRef->isTraceable)
    {
        UnlockTraceable(mutexRef);
    }
    else
    */
    {
        int result;

        le_thread_Ref_t lockingThread = mutexRef->lockingThreadRef;
        le_thread_Ref_t currentThread = le_thread_GetCurrent();

        // Make sure that the lock count is at least 1.
        LE_FATAL_IF(mutexRef->lockCount <= 0,
                    "Mutex '%s' unlocked too many times!",
                    mutexRef->name);

        // Make sure that the current thread is the one holding the mutex lock.
        if (lockingThread != currentThread)
        {
            char threadName[LIMIT_MAX_THREAD_NAME_BYTES];
            le_thread_GetName(lockingThread, threadName, sizeof(threadName));
            LE_FATAL("Attempt to unlock mutex '%s' held by other thread '%s'.",
                     mutexRef->name,
                     threadName);
        }

        // Update the lock count.
        // NOTE: the lock count is protected by the mutex itself.  That is, it can never be
        //       updated by anyone who doesn't hold the lock on the mutex.
        mutexRef->lockCount--;

        // If we have now reached a lock count of zero, the mutex is about to be unlocked, so
        // Update the data structures to reflect that the current thread no longer holds the
        // mutex.
        if (mutexRef->lockCount == 0)
        {
            MarkUnlocked(mutexRef);
        }

        // Warning!  If the lock count is now zero, then as soon as we call this function another
        // thread may grab the lock.
        result = pthread_mutex_unlock(&mutexRef->mutex);
        if (result != 0)
        {
            LE_FATAL("Failed to unlock mutex '%s'. Errno = %d (%m).",
                     mutexRef->name,
                     result );
        }
    }
}
