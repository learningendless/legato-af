 /**
  * This file contains the prototypes definitions for SMS Sample code.
  *
  * Copyright (C) Sierra Wireless, Inc. 2013.  All rights reserved.
  *
  * Use of this work is subject to license.
  *
  */

 #ifndef SMS_SAMPLE_INCLUDE_GUARD
 #define SMS_SAMPLE_INCLUDE_GUARD

#include "legato.h"



//--------------------------------------------------------------------------------------------------
/**
 * This function simply sends a text message.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t smsmo_SendMessage
(
    const char*   destinationPtr, ///< [IN] The destination number.
    const char*   textPtr         ///< [IN] The SMS message text.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function installs an handler for message reception.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t smsmt_Receiver
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function removes the handler for message reception.
 *
 */
//--------------------------------------------------------------------------------------------------
void smsmt_HandlerRemover
(
    void
);

#endif // SMS_SAMPLE_INCLUDE_GUARD
