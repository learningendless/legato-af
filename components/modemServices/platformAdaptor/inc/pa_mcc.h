/**
 * @page c_pa_mcc Modem Call Control Platform Adapter API
 *
 * @ref pa_mcc.h "Click here for the API reference documentation."
 *
 * <HR>
 *
 * @section pa_mcc_toc Table of Contents
 *
 *  - @ref pa_mcc_intro
 *  - @ref pa_mcc_rational
 *
 *
 * @section pa_mcc_intro Introduction
 * These APIs are on the top of the platform-dependent adapter layer. They are independent of the
 * implementation. They guarantee the portability on different kind of platform without any changes
 * for the components developped upon these APIs.
 *
 *
 * @section pa_mcc_rational Rational
 * These functions are all blocking functions, so that they return when the modem has answered or
 * when a timeout has occured due to an interrupted communication with the modem.
 *
 * They all verify the validity and the range of the input parameters before performing the modem
 * operation.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. Use of this work is subject to license.
 */


/** @file pa_mcc.h
 *
 * Legato @ref c_pa_mcc include file.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. Use of this work is subject to license.
 */

#ifndef LEGATO_PAMCC_INCLUDE_GUARD
#define LEGATO_PAMCC_INCLUDE_GUARD


#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
// Symbol and Enum definitions.
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
/**
 * CLIR supplementary service subscription.
 * If present, the CLIR supplementary service subscription is overridden temporary for this call
 * only.
 *
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    PA_MCC_ACTIVATE_CLIR   = 0, ///< Disable presentation of own phone number to remote.
    PA_MCC_DEACTIVATE_CLIR = 1, ///< Enable presentation of own phone number to remote.
}
pa_mcc_clir_t;

//--------------------------------------------------------------------------------------------------
/**
 * CUG supplementary service information.
 * If present, the CUG supplementary service information is overridden temporary for this call only.
 *
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    PA_MCC_ACTIVATE_CUG   = 0, ///< Activate
    PA_MCC_DEACTIVATE_CUG = 1, ///< Deactivate
}
pa_mcc_cug_t;

//--------------------------------------------------------------------------------------------------
/**
 * Structure provided to session state handler
 */
//--------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t                         callId;                               ///< Outgoing call Id
    le_mcc_call_Event_t             event;                                ///< Event generated
    char                            phoneNumber[LE_MDMDEFS_PHONE_NUM_MAX_LEN];     ///< the phone number
    le_mcc_call_TerminationReason_t TerminationEvent;                     ///< the termination reason
}
pa_mcc_CallEventData_t;

//--------------------------------------------------------------------------------------------------
/**
 * A handler that is called whenever a call event is received by the modem.
 *
 * @param pData       Data information that the handler may require.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*pa_mcc_CallEventHandlerFunc_t)
(
    pa_mcc_CallEventData_t*  dataPtr  ///< Data received with the handler
);

//--------------------------------------------------------------------------------------------------
// APIs.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register a handler for Call event notifications.
 *
 * @return LE_NOT_POSSIBLE  The function failed to register the handler.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_SetCallEventHandler
(
    pa_mcc_CallEventHandlerFunc_t   handlerFuncPtr ///< [IN] The event handler function.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister the handler for incoming calls handling.
 *
 *
 */
//--------------------------------------------------------------------------------------------------
void pa_mcc_ClearCallEventHandler
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to set a voice call.
 *
 * @return LE_NOT_POSSIBLE  The function failed.
 * @return LE_BUSY          A call is already ongoing.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_VoiceDial
(
    const char*    pn,        ///< [IN] The phone number.
    pa_mcc_clir_t  clir,      ///< [IN] The CLIR supplementary service subscription.
    pa_mcc_cug_t   cug,       ///< [IN] The CUG supplementary service information.
    uint8_t*       callIdPtr  ///< [OUT] The outgoing call ID.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to answer a call.
 *
 * @return LE_NOT_POSSIBLE  The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_Answer
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to disconnect the remote user.
 *
 * @return LE_NOT_POSSIBLE  The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_HangUp
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to end all the ongoing calls.
 *
 * @return LE_NOT_POSSIBLE  The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_mcc_HangUpAll
(
    void
);

#endif // LEGATO_PAMCC_INCLUDE_GUARD
