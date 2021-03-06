/**
 * @page c_basics Basic Type and Constant Definitions
 *
 * Cardinal types and commonly-used constants form the basic
 * foundation on which everything else is built. These include
 * error codes, portable integer types, and helpful macros that make things easier to use.
 *
 * See @ref le_basics.h for basic cardinal types and commonly-used constants info.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. 2014. All rights reserved. Use of this work is subject to license.
 */


/** @file le_basics.h
 *
 * Legato @ref c_basics include file.
 *
 * Copyright (C) Sierra Wireless, Inc. 2014. All rights reserved. Use of this work is subject to license.
 */

#ifndef LEGATO_BASICS_INCLUDE_GUARD
#define LEGATO_BASICS_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * Standard result codes.
 *
 * @note All error codes are negative integers. They allow functions with signed
 *       integers to return non-negative values when successful or standard error codes on failure.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LE_OK = 0,              ///< Successful.
    LE_NOT_FOUND = -1,      ///< Referenced item does not exist or could not be found.
    LE_NOT_POSSIBLE = -2,   ///< It is not possible to perform the requested action.
    LE_OUT_OF_RANGE = -3,   ///< An index or other value is out of range.
    LE_NO_MEMORY = -4,      ///< Insufficient memory is available.
    LE_NOT_PERMITTED = -5,  ///< Current user does not have permission to perform requested action.
    LE_FAULT = -6,          ///< Unspecified internal error.
    LE_COMM_ERROR = -7,     ///< Communications error.
    LE_TIMEOUT = -8,        ///< A time-out occurred.
    LE_OVERFLOW = -9,       ///< An overflow occurred or would have occurred.
    LE_UNDERFLOW = -10,     ///< An underflow occurred or would have occurred.
    LE_WOULD_BLOCK = -11,   ///< Would have blocked if non-blocking behaviour was not requested.
    LE_DEADLOCK = -12,      ///< Would have caused a deadlock.
    LE_FORMAT_ERROR = -13,  ///< Format error.
    LE_DUPLICATE = -14,     ///< Duplicate entry found or operation already performed.
    LE_BAD_PARAMETER = -15, ///< Parameter is invalid.
    LE_CLOSED = -16,        ///< The resource is closed.
    LE_BUSY = -17,          ///< The resource is busy or unavailable.
}
le_result_t;


//--------------------------------------------------------------------------------------------------
/**
 * ON/OFF type.
 *
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LE_OFF = 0,
    LE_ON  = 1,
}
le_onoff_t;


//--------------------------------------------------------------------------------------------------
/** @name Bit Masks
 *
 * Single byte bit definitions that can be used for bit masking.
 * @{
 */
//--------------------------------------------------------------------------------------------------
#define BIT0        0x01
#define BIT1        0x02
#define BIT2        0x04
#define BIT3        0x08
#define BIT4        0x10
#define BIT5        0x20
#define BIT6        0x40
#define BIT7        0x80
// @}

//--------------------------------------------------------------------------------------------------
/**
 * Find the address of a containing structure or union, based on the address of one of its members.
 *
 * If @c countPtr points to the @c count member of an object type
 * @c my_class_t, then a pointer to that object should use this:
 *
 * @code
 * my_class_t* myObjPtr = CONTAINER_OF(countPtr, my_class_t, count);
 * @endcode
 */
//--------------------------------------------------------------------------------------------------
#define CONTAINER_OF(memberPtr, type, member)                                                       \
    ((type*)(((uint8_t*)(memberPtr))-((size_t)(&(((type*)0)->member)))))


//--------------------------------------------------------------------------------------------------
/**
 * Computes  number of members in an array at compile time.
 *
 * @warning Does NOT work for pointers to arrays.
 *
 * Here's a code sample:
 *
 * @code
 * const char message[] = "Hello world!";
 *
 * size_t x = NUM_ARRAY_MEMBERS(message);
 *
 * printf("%lu\n", x);
 * @endcode
 *
 * Will print @c 13 on a 32-bit target.
 *
 * But this example is @b incorrect:
 *
 * @code
 * const char message[] = "Hello world!";
 * const char* messagePtr = &message;
 *
 * size_t x = NUM_ARRAY_MEMBERS(messagePtr);    // NO! BAD PROGRAMMER! BAD!
 *
 * printf("%lu\n", x);
 * @endcode
 */
//--------------------------------------------------------------------------------------------------
#define NUM_ARRAY_MEMBERS(array) \
    (sizeof(array) / sizeof((array)[0]))


//--------------------------------------------------------------------------------------------------
/**
 * Computes the index of a member within an array.
 *
 * This code sample prints out "The 'w' is at index 6.":
 *
 * @code
 * const char message[] = "Hello world!";
 * const char* charPtr;
 * int i;
 *
 * for (i = 0; i < sizeof(message); i++)
 * {
 *     if (message[i] == 'w')
 *     {
 *         charPtr = &message[i];
 *     }
 * }
 *
 * printf("The 'w' is at index %zu.\n", INDEX_OF_ARRAY_MEMBER(message, charPtr));
 * @endcode
 **/
//--------------------------------------------------------------------------------------------------
#define INDEX_OF_ARRAY_MEMBER(array, memberPtr) \
    ((((size_t)memberPtr) - ((size_t)array)) / sizeof(*(memberPtr)))


//--------------------------------------------------------------------------------------------------
/**
 * This function takes the characters as an argument and puts quotes around them.  
 * 
 * Code sample:
 *
 * @code
 *    const char name[] = STRINGIZE(foo);
 * @endcode
 *
 * Is seen by the compiler as
 *
 * @code
 *    const char name[] = "foo";
 * @endcode
 *
 * The STRINGIZE() macro function passes names like macro definitions
 * on the compiler's command-line.  If the above code were changed to:
 *
 * @code
 *    const char name[] = STRINGIZE(NAME);
 * @endcode
 *
 * then compiling it using the <b>gcc -c</b> command makes it equivalent to the 
 * example above:
 *
 * @code
 *    $ gcc -c -DNAME=foo file.c
 * @endcode
 */
//--------------------------------------------------------------------------------------------------
#define STRINGIZE(x)  STRINGIZE_EXPAND(x)

//--------------------------------------------------------------------------------------------------
/**
 * Helper macro for @ref STRINGIZE(x).
 */
//--------------------------------------------------------------------------------------------------
#define STRINGIZE_EXPAND(x)     #x   // Needed to expand macros.


#endif // LEGATO_BASICS_INCLUDE_GUARD
