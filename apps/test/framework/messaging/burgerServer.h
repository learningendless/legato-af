//--------------------------------------------------------------------------------------------------
/**
 * Automated unit test for the Low-Level Messaging APIs.
 *
 * Burger Protocol Server API.
 *
 * Copyright (C) 2013, Sierra Wireless, Inc. - All rights reserved. Use of this work is subject to license.
 **/
//--------------------------------------------------------------------------------------------------

#ifndef BURGER_SERVER_H_INCLUDE_GUARD
#define BURGER_SERVER_H_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * Starts an instance of the Burger Protocol server in the calling thread.
 **/
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t burgerServer_Start
(
    const char* serviceInstanceName,
    size_t maxRequests
);

#endif // BURGER_SERVER_H_INCLUDE_GUARD
