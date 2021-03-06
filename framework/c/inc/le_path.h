
//--------------------------------------------------------------------------------------------------
/**
 * @page c_path Path API
 *
 * @ref le_path.h "Click here for the API reference documentation."
 *
 * <HR>
 *
 * @ref c_path_dirAndBasename <br>
 * @ref c_path_threads <br>
 *
 * Paths are text strings that contain nodes separated by character separators.  Paths are used in
 * many common applications like file system addressing, URLs, etc. so being able to parse them
 * is quite important.
 *
 * The Path API is intended for general purpose use and supports UTF-8 null-terminated
 * strings and multi-character separators.
 *
 * @section c_path_dirAndBasename Directory and Basename
 *
 * The function @c le_path_GetDir() is a convenient way to get the path's directory without having
 * to create an iterator.  The directory is the portion of the path up to and including the last
 * separator.  le_path_GetDir() does not modify the path in anyway (i.e., consecutive paths are left
 * as is), except to drop the node after the last separator.
 *
 * The function le_path_GetBasenamePtr() is an efficient and convenient function for accessing the
 * last node in the path without having to create an iterator.  The returned pointer points to the
 * character following the last separator in the path.  Because the basename is actually a portion
 * of the path string, not a copy, any changes to the returned basename will also affect the
 * original path string.
 *
 * @section c_path_threads Thread Safety
 *
 * All the functions in this API are thread safe and reentrant unless of course the path iterators
 * or the buffers passed into the functions are shared between threads.  If the path iterators or
 * buffers are shared by multiple threads then some other mechanism must be used to ensure these
 * functions are thread safe.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to
 * license.
 */

//--------------------------------------------------------------------------------------------------
/** @file le_path.h
 *
 * Legato @ref c_path include file.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to
 * license.
 */

#ifndef LEGATO_PATH_INCLUDE_GUARD
#define LEGATO_PATH_INCLUDE_GUARD


//--------------------------------------------------------------------------------------------------
/**
 * Gets the directory, which is the entire path up to and including the last separator.
 *
 * @return
 *      LE_OK if succesful.
 *      LE_OVERFLOW if the dirPtr buffer is too small.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_path_GetDir
(
    const char* pathPtr,            ///< [IN] Path string.
    const char* separatorPtr,       ///< [IN] Separator string.
    char* dirPtr,                   ///< [OUT] Buffer to store the directory string.
    size_t dirBuffSize              ///< [IN] Size of the directory buffer in bytes.
);


//--------------------------------------------------------------------------------------------------
/**
 * Gets a pointer to the basename (the last node in the path).  This function gets the basename by
 * returning a pointer to the character following the last separator.
 *
 * @return
 *      Pointer to the character following the last separator.
 */
//--------------------------------------------------------------------------------------------------
char* le_path_GetBasenamePtr
(
    const char* pathPtr,            ///< [IN] Path string.
    const char* separatorPtr        ///< [IN] Separator string.
);


//--------------------------------------------------------------------------------------------------
/**
 * Concatenates multiple path segments together.
 *
 * Concatenates the path in the pathPtr buffer with all path segments and stores the result in the
 * pathPtr.  Ensures that where path segments are joined there is only one separator between them.
 * Duplicate trailing separators in the resultant path are also dropped.
 *
 * If there is not enough space in pathPtr for all segments, as many characters from the segments
 * that will fit in the buffer will be copied and LE_OVERFLOW will be returned.  Partial UTF-8
 * characters and partial separators will never be copied.
 *
 * @warning  The (char*)NULL at the end of the list of path segments is mandatory.  If this NULL is
 *           omitted the behaviour is undefined.
 *
 * @return
 *      LE_OK if successful.
 *      LE_OVERFLOW if there was not enough buffer space in pathPtr for all segments.
 */
//--------------------------------------------------------------------------------------------------

le_result_t le_path_Concat
(
    const char* separatorPtr,       ///< [IN] Separator string.
    char* pathPtr,                  ///< [IN/OUT] Buffer containing the first segment and where
                                    ///           the resultant path will be stored.
    size_t pathSize,                ///< [IN] Buffer size.
    ...                             ///< [IN] Path segments to concatenate.  The list of segments
                                    ///       must be terminated by (char*)NULL.
)
__attribute__((__sentinel__));


#endif // LEGATO_PATH_INCLUDE_GUARD
