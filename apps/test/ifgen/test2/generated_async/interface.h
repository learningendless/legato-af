/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */

/**
 * @page ifgentest Testing IfGen Doxygen Support
 *
 * @ref interface.h "Click here for the API Reference documentation."
 *
 * Example interface file
 */
/**
 * @file interface.h
 *
 * Legato @ref ifgentest include file.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013.  All rights reserved. Use of this work is subject to license.
 */

#ifndef EXAMPLE_H_INCLUDE_GUARD
#define EXAMPLE_H_INCLUDE_GUARD


#include "legato.h"

// Interface specific includes
#include "common_interface.h"


//--------------------------------------------------------------------------------------------------
/**
 * Connect the client to the service
 */
//--------------------------------------------------------------------------------------------------
void ConnectService
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Disconnect the client from the service
 */
//--------------------------------------------------------------------------------------------------
void DisconnectService
(
    void
);


//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
#define TEN 10


//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
#define TWENTY 20


//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
#define SOME_STRING "some string"


//--------------------------------------------------------------------------------------------------
/**
 * BITMASK example
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    A = 0x1,
        ///< first

    B = 0x2,
        ///< second

    C = 0x4
        ///< third
}
BitMaskExample_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type for TestA handler ADD/REMOVE functions
 */
//--------------------------------------------------------------------------------------------------
typedef struct TestA* TestARef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type for BugTest handler ADD/REMOVE functions
 */
//--------------------------------------------------------------------------------------------------
typedef struct BugTest* BugTestRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Handler definition
 *
 * @param x
 *        First parameter for the handler
 *        Second comment line
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
typedef void (*TestAFunc_t)
(
    int32_t x,
    void* contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Handler definition for testing bugs
 *
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
typedef void (*BugTestFunc_t)
(
    void* contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * TestA handler ADD function
 */
//--------------------------------------------------------------------------------------------------
TestARef_t AddTestA
(
    TestAFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * TestA handler REMOVE function
 */
//--------------------------------------------------------------------------------------------------
void RemoveTestA
(
    TestARef_t addHandlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Function takes all the possible kinds of parameters, but returns nothing
 */
//--------------------------------------------------------------------------------------------------
void allParameters
(
    common_EnumExample_t a,
        ///< [IN]
        ///< first one-line comment
        ///< second one-line comment

    uint32_t* bPtr,
        ///< [OUT]

    const uint32_t* dataPtr,
        ///< [IN]

    size_t dataNumElements,
        ///< [IN]

    uint32_t* outputPtr,
        ///< [OUT]
        ///< some more comments here
        ///< and some comments here as well

    size_t* outputNumElementsPtr,
        ///< [INOUT]

    const char* label,
        ///< [IN]

    char* response,
        ///< [OUT]
        ///< comments on final parameter, first line
        ///< and more comments

    size_t responseNumElements,
        ///< [IN]

    char* more,
        ///< [OUT]
        ///< This parameter tests a bug fix

    size_t moreNumElements
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Test file descriptors as IN and OUT parameters
 */
//--------------------------------------------------------------------------------------------------
void FileTest
(
    int dataFile,
        ///< [IN]
        ///< file descriptor as IN parameter

    int* dataOutPtr
        ///< [OUT]
        ///< file descriptor as OUT parameter
);

//--------------------------------------------------------------------------------------------------
/**
 * This function fakes an event, so that the handler will be called.
 * Only needed for testing.  Would never exist on a real system.
 */
//--------------------------------------------------------------------------------------------------
void TriggerTestA
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * BugTest handler ADD function
 */
//--------------------------------------------------------------------------------------------------
BugTestRef_t AddBugTest
(
    const char* newPathPtr,
        ///< [IN]

    BugTestFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * BugTest handler REMOVE function
 */
//--------------------------------------------------------------------------------------------------
void RemoveBugTest
(
    BugTestRef_t addHandlerRef
        ///< [IN]
);


#endif // EXAMPLE_H_INCLUDE_GUARD

