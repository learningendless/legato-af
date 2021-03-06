/**
 * @file mdcUnitaryTest.c
 *
 * Unitary test for of @ref pa_mdc API.
 *
 *
 * TEST1: basic tests around le_mdc_GetProfile and le_mdc_RemoveProfile
 * 1.1: Try to get a profile
 * 1.2: Allocate the same profile as in test 1.1: le_mdc shouldn't allocate a new profile but returns
 * the profile allocated into 1.1
 * 1.3: allocate 3gpp2 profiles until the PA_MDC_MAX_PROFILE are allocated
 * 1.4: allocate a new profile: le_mdc_GetProfile returns null as no profile left
 * 1.5: remove the first profile. Test that the profile is really deleted, and other are available
 * 1.6: allocate a new profile, a second new one is failed (PA_MDC_MAX_PROFILE is reached)
 * 1.7: remove all handlers (returned to initiale state)
 *
 *
 * TEST2: Get profile/Remove profile while handlers are subscribed
 * 2.1: Get a profile, allocate a handler. check that removing the profile is not allowed while
 * the handler is allocated: profile can be deallocated after removing the handler
 * 2.2: same as 2.1, with 2 handlers allocated
 *
 * TEST3: test le_mdc_GetAvailableProfile() API. Profiles PA_MDC_MIN_INDEX_3GPP_PROFILE+1,
 * PA_MDC_MIN_INDEX_3GPP_PROFILE + 3 to PA_MDC_MAX_INDEX_3GPP_PROFILE,
 * PA_MDC_MIN_INDEX_3GPP2_PROFILE+1 to PA_MDC_MAX_INDEX_3GPP2_PROFILE-1 are not available
 * 3.1: No profile is allocated. Try to get a profile using le_mdc_GetAvailableProfile(). Check
 * That the profile index is equal to PA_MDC_MIN_INDEX_3GPP_PROFILE
 * 3.2: Try to get a new profile using le_mdc_GetAvailableProfile().
 * Check that the profile index is equal to PA_MDC_MIN_INDEX_3GPP_PROFILE+2
 * 3.3: Try to get a new profile using le_mdc_GetAvailableProfile().
 * Check that the profile index is equal to PA_MDC_MIN_INDEX_3GPP2_PROFILE
 * 3.4: Try to get a new profile using le_mdc_GetAvailableProfile().
 * Check that the profile index is equal to PA_MDC_MAX_INDEX_3GPP2_PROFILE
 * 3.5: Try to get a new profile using le_mdc_GetAvailableProfile(). No more profile is available,
 * the function returns LE_FAULT
 *
 * TEST4: test le_mdc_StartSession/le_mdc_StopSession API
 * 4.1: Get a profile using le_mdc_GetProfile, then try to open a session using le_mdc_StartSession
 * result code should be LE_OK, internal callRef mustn't be null
 * 4.2: try again to start the same profile: result code should be an error, internal callRef mustn't
 * by modified
 * 4.3: stop the session using le_mdc_StopSession: return code is LE_OK
 * Remove the profile
 *
 * Copyright (C) Sierra Wireless, Inc. 2014. Use of this work is subject to license.
 */


#include "le_mdc_interface.h"
#include "pa_mdc.h"
#include "le_mdc_local.h"
#include "le_mdc.c"


/* TEST 1 */
void test1( void )
{
    uint32_t i;
    le_mdc_ProfileRef_t profileTmp;
    le_mdc_ProfileRef_t profileRef[PA_MDC_MAX_PROFILE] = {0};
    le_result_t result;

    /* TEST 1.1 */
    /* get a 3gpp profile */
    profileRef[0] = le_mdc_GetProfile(1);

    LE_ASSERT(profileRef[0] != NULL);

    /* TEST 1.2*/
    /* get the profile again: try it PA_MDC_MAX_PROFILE times */
    for (i = 0; i < PA_MDC_MAX_PROFILE; i++)
    {
        profileTmp = le_mdc_GetProfile(1);

        LE_ASSERT(profileTmp == profileRef[0]);
    }

    /* TEST 1.3 */
    /* get 3gpp2 profiles: we tried to allocate PA_MDC_MAX_PROFILE+1 profile, but all the same */
    /* so we should be able to allocate PA_MDC_MAX_PROFILE-1 profile */
    for (i = 0; i < PA_MDC_MAX_PROFILE-1; i++)
    {
         profileRef[1+i] = le_mdc_GetProfile(PA_MDC_MIN_INDEX_3GPP2_PROFILE+i);

        LE_ASSERT(profileRef[1+i] != NULL);
    }

    /* TEST 1.4 */
    /* Try a new profile allocation: it should be failed as the max has been reached */
    profileTmp = le_mdc_GetProfile(PA_MDC_MAX_INDEX_3GPP_PROFILE);
    LE_ASSERT(profileTmp == NULL);

    /* TEST 1.5 */
    /* Remove the first profile */
    result = le_mdc_RemoveProfile(profileRef[0]);
    LE_ASSERT(result == LE_OK);

    for (i = 1; i < PA_MDC_MAX_PROFILE; i++)
    {
         uint32_t index = le_mdc_GetProfileIndex( profileRef[i] );

        LE_ASSERT( PA_MDC_MIN_INDEX_3GPP2_PROFILE+i-1 == index );
    }

    /* TEST 1.6 */
    /* Try a new profile allocation: it should be possible to get a new one */
    profileRef[0] = le_mdc_GetProfile(PA_MDC_MAX_INDEX_3GPP_PROFILE);
    LE_ASSERT(profileRef[0] != NULL);

    /* a new profile allocation should be failed (max reached) */
    profileTmp = le_mdc_GetProfile(PA_MDC_MAX_INDEX_3GPP2_PROFILE);
    LE_ASSERT(profileTmp == NULL);

    /* TEST 1.7 */
    for (i = 0; i < PA_MDC_MAX_PROFILE; i++)
    {
        result = le_mdc_RemoveProfile(profileRef[i]);
        LE_ASSERT(result == LE_OK);
    }

    LE_INFO("Test 1 passed");
}


void handlerFunc
(
    bool isConnected,
    void* contextPtr
)
{

}

/* TEST 2 */
void test2( void )
{
    le_mdc_ProfileRef_t profile;
    le_mdc_SessionStateHandlerRef_t sessionStateHandler1,sessionStateHandler2;
    le_result_t result;

    /* TEST 2.1 */
    /* Allocate profile */
    profile = le_mdc_GetProfile(1);

    /* Add a handler */
    sessionStateHandler1 = le_mdc_AddSessionStateHandler (profile, handlerFunc, NULL);
    LE_ASSERT(sessionStateHandler1 != NULL);

    /* Remove the profile must be failed (handler is allocated) */
    result = le_mdc_RemoveProfile(profile);
    LE_ASSERT(result == LE_NOT_POSSIBLE);

    /* Remove the handler */
    le_mdc_RemoveSessionStateHandler(sessionStateHandler1);

    /* Remove the profile */
    result = le_mdc_RemoveProfile(profile);
    LE_ASSERT(result == LE_OK);

    /* TEST 2.2 */
    /* Allocate profile */
    profile = le_mdc_GetProfile(1);

    /* Add handlers */
    sessionStateHandler1 = le_mdc_AddSessionStateHandler (profile, handlerFunc, NULL);
    sessionStateHandler2 = le_mdc_AddSessionStateHandler (profile, handlerFunc, NULL);
    LE_ASSERT(sessionStateHandler1 != NULL);
    LE_ASSERT(sessionStateHandler2 != NULL);

    /* Remove the profile must be failed (handlers are allocated) */
    result = le_mdc_RemoveProfile(profile);
    LE_ASSERT(result == LE_NOT_POSSIBLE);

    /* Remove the handler */
    le_mdc_RemoveSessionStateHandler(sessionStateHandler1);

    /* Remove the profile must be failed (handler is allocated) */
    result = le_mdc_RemoveProfile(profile);
    LE_ASSERT(result == LE_NOT_POSSIBLE);

    /* Remove the handler */
    le_mdc_RemoveSessionStateHandler(sessionStateHandler2);

    /* Remove the profile */
    result = le_mdc_RemoveProfile(profile);
    LE_ASSERT(result == LE_OK);

    LE_INFO("Test 2 passed");
}

/* TEST 3 */
void test3( void )
{
    /* TEST 3.1 */
    /* allocate a profile */
    le_mdc_ProfileRef_t profileRef;

    le_result_t result;

    result = le_mdc_GetAvailableProfile(&profileRef);
    LE_ASSERT(profileRef != NULL);
    LE_ASSERT(result == LE_OK);
    LE_ASSERT(le_mdc_GetProfileIndex(profileRef) == PA_MDC_MIN_INDEX_3GPP_PROFILE);

    /* TEST 3.2 */
    result = le_mdc_GetAvailableProfile(&profileRef);
    LE_ASSERT(profileRef != NULL);
    LE_ASSERT(result == LE_OK);
    LE_ASSERT(le_mdc_GetProfileIndex(profileRef) == PA_MDC_MIN_INDEX_3GPP_PROFILE);

    /* release the profile */
    result = le_mdc_RemoveProfile(profileRef);
    LE_ASSERT(result == LE_OK);

    LE_INFO("Test 3 passed");
}

/* TEST 4 */
void test4( void )
{
    le_mdc_ProfileRef_t profile;
    le_mdc_Profile_t* profilePtr;
    void* callRefPtr;
    le_result_t result;

    /* TEST 4.1 */
    /* Allocate profile */
    profile = le_mdc_GetProfile(1);

    /* start the session associated to the profile */
    result = le_mdc_StartSession(profile);
    LE_ASSERT(result == LE_OK);

    /* check internal handler */
    profilePtr = le_ref_Lookup(DataProfileRefMap, profile);
    LE_ASSERT(profilePtr->callRef != NULL);
    callRefPtr = profilePtr->callRef;

    /* TEST 4.2 */
    /* start again the profile */
    result = le_mdc_StartSession(profile);
    LE_ASSERT(result == LE_NOT_POSSIBLE);
    profilePtr = le_ref_Lookup(DataProfileRefMap, profile);

    /* check that the callRef is not modified */
    LE_ASSERT(profilePtr->callRef == callRefPtr);

    /* TEST 4.3 */
    /* stop the session */
    result = le_mdc_StopSession(profile);
    LE_ASSERT(result == LE_OK);

    /* remove the profile */
    result = le_mdc_RemoveProfile(profile);
    LE_ASSERT(result == LE_OK);

    LE_INFO("Test 4 passed");
}

le_mrc_Rat_t CurrentRat = LE_MRC_RAT_GSM;

/* Stub GetRAT to return an internal RAT */
le_result_t le_mrc_GetRadioAccessTechInUse
(
    le_mrc_Rat_t*   ratPtr  ///< [OUT] The Radio Access Technology.
)
{
    *ratPtr = CurrentRat;
    return LE_OK;
}

/* TEST 5 */
void test5( void )
{
    /* TEST 5.1 */
    /* allocate a profile on 3GPP */
    le_mdc_ProfileRef_t profileRef;

    le_result_t result;

    CurrentRat = LE_MRC_RAT_GSM;
    result = le_mdc_GetAvailableProfile(&profileRef);
    LE_ASSERT(profileRef != NULL);
    LE_ASSERT(result == LE_OK);
    LE_ASSERT(le_mdc_GetProfileIndex(profileRef) == PA_MDC_MIN_INDEX_3GPP_PROFILE);

    /* TEST 5.2 */
    /* allocate a profile on 3GPP2 */
    CurrentRat = LE_MRC_RAT_CDMA;
    result = le_mdc_GetAvailableProfile(&profileRef);
    LE_ASSERT(profileRef != NULL);
    LE_ASSERT(result == LE_OK);
    LE_ASSERT(le_mdc_GetProfileIndex(profileRef) == PA_MDC_MIN_INDEX_3GPP2_PROFILE);

    LE_INFO("Test 5 passed");
}

/* main() of the tests */
COMPONENT_INIT
{
    le_log_SetFilterLevel(LE_LOG_DEBUG);

    /* init the service */
    le_mdc_Init();

    /* TEST 1 */
    test1();

    /* TEST 2 */
    test2();

    /* TEST 3 */
    test3();

    /* TEST 4 */
    test4();

    /* TEST 5 */
    test5();

    exit(EXIT_SUCCESS);
}
