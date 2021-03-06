/** @file le_sim.c
 *
 * This file contains the data structures and the source code of the high level SIM APIs.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to license.
 */


#include "legato.h"
#include "interfaces.h"
#include "pa_sim.h"


//--------------------------------------------------------------------------------------------------
// Symbols and enums.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Maximum number of SIM objects we expect to have at one time.
 */
//--------------------------------------------------------------------------------------------------
#define SIM_MAX_CARDS    2

//--------------------------------------------------------------------------------------------------
/**
 * ICCID maximum length.
 *
 */
//--------------------------------------------------------------------------------------------------
#define ICCID_MAX_LEN   20

//--------------------------------------------------------------------------------------------------
/**
 * IMSI maximum length.
 *
 */
//--------------------------------------------------------------------------------------------------
#define IMSI_MAX_LEN   16


//--------------------------------------------------------------------------------------------------
// Data structures.
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
/**
 * SIM structure.
 *
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_sim_Obj
{
    uint32_t        num;                       ///< The SIM card number.
    char            ICCID[LE_SIM_ICCID_LEN];   ///< The integrated circuit card identifier.
    char            IMSI[LE_SIM_IMSI_LEN];     ///< The international mobile subscriber identity.
    char            PIN[LE_SIM_PIN_MAX_LEN+1]; ///< The PIN code.
    char            PUK[LE_SIM_PUK_LEN+1];     ///< The PUK code.
    char            phoneNumber[LE_MDMDEFS_PHONE_NUM_MAX_LEN]; /// < The Phone Number.
    bool            isPresent;                 ///< The 'isPresent' flag.
    le_dls_Link_t   link;                      ///< The Sim Object node link.
    void*           ref;                       ///< The safe reference for this object.
}Sim_t;

//--------------------------------------------------------------------------------------------------
/**
 * Create and initialize the SIM list.
 *
 */
//--------------------------------------------------------------------------------------------------
le_dls_List_t SimList;

//--------------------------------------------------------------------------------------------------
//                                       Static declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Memory Pool for SIM objects.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t   SimPool;

//--------------------------------------------------------------------------------------------------
/**
 * Safe Reference Map for SIM objects.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t SimRefMap;

//--------------------------------------------------------------------------------------------------
/**
 * Current selected SIM card.
 *
 */
//--------------------------------------------------------------------------------------------------
static uint32_t  SelectedCard;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID for New SIM state notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t NewSimStateEventId;


//--------------------------------------------------------------------------------------------------
/**
 * Number of card slots.
 *
 */
//--------------------------------------------------------------------------------------------------
static uint32_t NumOfSlots;

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM card information.
 *
 */
//--------------------------------------------------------------------------------------------------
static void GetSimCardInformation
(
    Sim_t*          simPtr,
    le_sim_States_t state
)
{
    pa_sim_CardId_t   iccid;
    pa_sim_Imsi_t     imsi;

    switch(state)
    {
        case LE_SIM_ABSENT:
            simPtr->ICCID[0] = '\0';
            simPtr->IMSI[0] = '\0';
            simPtr->isPresent = false;
            break;

        case LE_SIM_INSERTED:
        case LE_SIM_BLOCKED:
            simPtr->isPresent = true;
            simPtr->IMSI[0] = '\0';
            if(pa_sim_GetCardIdentification(iccid) != LE_OK)
            {
                LE_ERROR("Failed to get the ICCID of card number %d.", simPtr->num);
                simPtr->ICCID[0] = '\0';
            }
            else
            {
                le_utf8_Copy(simPtr->ICCID, iccid, sizeof(simPtr->ICCID), NULL);
            }
            break;

        case LE_SIM_READY:
            simPtr->isPresent = true;
            // Get identification information
            if(pa_sim_GetCardIdentification(iccid) != LE_OK)
            {
                LE_ERROR("Failed to get the ICCID of card number %d.", simPtr->num);
                simPtr->ICCID[0] = '\0';
            }
            else
            {
                le_utf8_Copy(simPtr->ICCID, iccid, sizeof(simPtr->ICCID), NULL);
            }

            if(pa_sim_GetIMSI(imsi) != LE_OK)
            {
                LE_ERROR("Failed to get the IMSI of card number %d.", simPtr->num);
                simPtr->IMSI[0] = '\0';
            }
            else
            {
                le_utf8_Copy(simPtr->IMSI, imsi, sizeof(simPtr->IMSI), NULL);
            }
            break;

        case LE_SIM_BUSY:
        case LE_SIM_STATE_UNKNOWN:
            simPtr->isPresent = true;
            break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Creates a new SIM object for a given card slot number.
 *
 * @return A pointer to the SIM object.
 */
//--------------------------------------------------------------------------------------------------
static Sim_t* CreateSim
(
    uint32_t        cardNum,    ///< [in] The SIM card index (0 = first card in the device).
    le_sim_States_t state       ///< [in] The current state of the SIM.
)
{
    Sim_t            *simPtr=NULL;

    // Create the message node.
    simPtr = (Sim_t*)le_mem_ForceAlloc(SimPool);

    simPtr->num= cardNum;
    simPtr->ICCID[0] = '\0';
    simPtr->IMSI[0] = '\0';
    simPtr->isPresent = false;
    simPtr->ref = NULL;
    simPtr->link = LE_DLS_LINK_INIT;
    GetSimCardInformation(simPtr, state);

    // Create the safeRef for the SIM object
    simPtr->ref = le_ref_CreateRef(SimRefMap, simPtr);
    LE_DEBUG("Created ref=%p for ptr=%p", simPtr->ref, simPtr);

    // Insert the message in the Sim List.
    le_dls_Queue(&SimList, &(simPtr->link));

    return simPtr;
}

//--------------------------------------------------------------------------------------------------
/**
 * Sim destructor.
 *
 */
//--------------------------------------------------------------------------------------------------
static void SimDestructor
(
    void* objPtr
)
{
    // Invalidate the Safe Reference.
    le_ref_DeleteRef(SimRefMap, ((Sim_t*)objPtr)->ref);

    // Remove the SIM object from the SIM List.
    le_dls_Remove(&SimList, &(((Sim_t*)objPtr)->link));
}

//--------------------------------------------------------------------------------------------------
/**
 * Searches the SIM List for a SIM matching a given card number.
 *
 * @return A pointer to the SIM object, or NULL if not found.
 */
//--------------------------------------------------------------------------------------------------
static Sim_t* FindSim
(
    uint32_t cardNum    ///< [in] The card slot number (0 = first slot on the device).
)
{
    Sim_t*          simPtr;
    le_dls_Link_t*  linkPtr;

    linkPtr = le_dls_Peek(&SimList);
    while (linkPtr != NULL)
    {
        // Get the node from SimList
        simPtr = CONTAINER_OF(linkPtr, Sim_t, link);
        // Check the node.
        if (simPtr->num == cardNum)
        {
            return simPtr;
        }

        // Move to the next node.
        linkPtr = le_dls_PeekNext(&SimList, linkPtr);
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * SIM card selector.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SelectSIMCard
(
    uint32_t simNum
)
{
    if((simNum != SelectedCard) && (NumOfSlots > 1))
    {
        // Select the SIM card
        LE_DEBUG("Try to select card number.%d", simNum);
        if(pa_sim_SelectCard(simNum) != LE_OK)
        {
            LE_ERROR("Failed to select card number.%d", simNum);
            return LE_NOT_FOUND;
        }
        SelectedCard = simNum;
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer New SIM state notification Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerNewSimStateHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    le_sim_ObjRef_t*             referencePtr = reportPtr;
    le_sim_NewStateHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    clientHandlerFunc(*referencePtr, le_event_GetContextPtr());
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for new SIM state notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static void NewSimStateHandler
(
    pa_sim_Event_t* eventPtr
)
{
    LE_DEBUG("New SIM state.%d for card.%d (eventPtr %p)", eventPtr->state, eventPtr->num, eventPtr);

    Sim_t* simPtr = FindSim(eventPtr->num);

    // Create the SIM object if it does not exist yet
    if (simPtr == NULL)
    {
        LE_INFO("NO SIM object found, create a new one");
        simPtr = CreateSim(eventPtr->num, eventPtr->state);
    }
    // Otherwise, just update the one we found.
    else
    {
        LE_DEBUG("Found SIM object for card number %d.", eventPtr->num);
        GetSimCardInformation(simPtr, eventPtr->state);
    }

    // Discard transitional states
    switch (eventPtr->state)
    {
        case LE_SIM_BUSY:
        case LE_SIM_STATE_UNKNOWN:
            LE_DEBUG("Discarding report for card.%d, state.%d", eventPtr->num, eventPtr->state);
            return;

        default:
            break;
    }

    // Notify all the registered client handlers
    le_event_Report(NewSimStateEventId, &simPtr->ref, sizeof(le_sim_ObjRef_t));
    LE_DEBUG("Report on SIM reference %p", simPtr->ref);

    le_mem_Release(eventPtr);
}

//--------------------------------------------------------------------------------------------------
// APIs.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the SIM operations component
 *
 * @note If the initialization failed, it is a fatal error, the function will not return.
 */
//--------------------------------------------------------------------------------------------------
void le_sim_Init
(
    void
)
{
    // Create a pool for SIM objects
    SimPool = le_mem_CreatePool("SimPool", sizeof(Sim_t));
    le_mem_SetDestructor(SimPool, SimDestructor);
    le_mem_ExpandPool(SimPool, SIM_MAX_CARDS);

    // Create the Safe Reference Map to use for SIM object Safe References.
    SimRefMap = le_ref_CreateMap("SimMap", SIM_MAX_CARDS);

    NumOfSlots = pa_sim_CountSlots();
    LE_FATAL_IF((pa_sim_GetSelectedCard(&SelectedCard) != LE_OK), "Unable to get selected card.");

    LE_DEBUG("Modem has %u SIM slots and SIM %u is selected.", NumOfSlots, SelectedCard);

    // Create an event Id for new SIM state notifications
    NewSimStateEventId = le_event_CreateId("NewSimState", sizeof(le_sim_ObjRef_t));

    // Register a handler function for new SIM state notification
    LE_FATAL_IF((pa_sim_AddNewStateHandler(NewSimStateHandler) == NULL),
                "Add new SIM state handler failed");
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the current selected card.
 *
 * @return The number of the current selected SIM card.
 */
//--------------------------------------------------------------------------------------------------
uint32_t le_sim_GetSelectedCard
(
    void
)
{
    return SelectedCard;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to count the number of SIM card slots.
 *
 * @return The number of the SIM card slots mounted on the device.
 */
//--------------------------------------------------------------------------------------------------
uint32_t le_sim_CountSlots
(
    void
)
{
    return NumOfSlots;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to create a SIM object.
 *
 * @return A reference to the SIM object.
 *
 * @note
 *      On failure, the process exits, so you don't have to worry about checking the returned
 *      reference for validity.
 */
//--------------------------------------------------------------------------------------------------
le_sim_ObjRef_t le_sim_Create
(
    uint32_t cardNum  ///< [IN] The SIM card number (1 or 2, it depends on the platform).
)
{
    // TODO: manage several slots
    if ((cardNum > 1) && (NumOfSlots == 1))
    {
        LE_KILL_CLIENT("Only 1 slot is available !");
        return NULL;
    }
    if (cardNum == 0)
    {
        LE_KILL_CLIENT("Invalid card number (%d) !", cardNum);
        return NULL;
    }

    // Select the SIM card
    if (SelectSIMCard(cardNum) != LE_OK)
    {
        LE_ERROR("Unable to select Sim Card slot %d !", cardNum);
        return NULL;
    }

    Sim_t* simPtr = FindSim(cardNum);
    // Create the SIM object if it does not exist yet
    if (simPtr == NULL)
    {
        le_sim_States_t   state;

        if (pa_sim_GetState(&state) != LE_OK)
        {
            state = LE_SIM_STATE_UNKNOWN;
        }
        LE_INFO("NO SIM object found, create a new one");

        simPtr = CreateSim(cardNum, state);
    }
    // If the SIM already exists, then just increment the reference count on it.
    else
    {
        le_mem_AddRef(simPtr);
    }

    // Return a Safe Reference for this Sim object.
    return (simPtr->ref);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to delete a SIM object.
 *
 * It deletes the SIM object, all the allocated memory is freed. However if several Users own the
 * SIM object (for example in the case of several handler functions registered for SIM state
 * notification) the SIM object will be actually deleted only if it remains one User owning the SIM
 * object.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
void le_sim_Delete
(
    le_sim_ObjRef_t  simRef    ///< [IN] The SIM object.
)
{
    Sim_t*                  simPtr;

    simPtr = le_ref_Lookup(SimRefMap, simRef);
    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return;
    }

    // release the SIM object
    le_mem_Release(simPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to retrieve the slot number of the SIM card.
 *
 * @return The slot number of the SIM card.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
uint32_t le_sim_GetSlotNumber
(
    le_sim_ObjRef_t simRef   ///< [IN] The SIM object.
)
{
    Sim_t* simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return 0;
    }

    return simPtr->num;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function retrieves the integrated circuit card identifier (ICCID) of the SIM card (20 digits)
 *
 * @return LE_OK            The ICCID was successfully retrieved.
 * @return LE_OVERFLOW      The iccidPtr buffer was too small for the ICCID.
 * @return LE_NOT_POSSIBLE  The ICCID could not be retrieved.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_GetICCID
(
    le_sim_ObjRef_t simRef, ///< [IN] The SIM object.
    char * iccidPtr,        ///< [OUT] Buffer to hold the ICCID.
    size_t iccidLen         ///< [IN] Buffer length
)
{
    le_sim_States_t  state;
    pa_sim_CardId_t  iccid;
    Sim_t*           simPtr = le_ref_Lookup(SimRefMap, simRef);
    le_result_t      res = LE_NOT_POSSIBLE;

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }
    if (iccidPtr == NULL)
    {
        LE_KILL_CLIENT("iccidPtr is NULL !");
        return LE_FAULT;
    }

    if (strlen(simPtr->ICCID) == 0)
    {
        if (SelectSIMCard(simPtr->num) == LE_OK)
        {
            if (pa_sim_GetState(&state) == LE_OK)
            {
                if ((state == LE_SIM_INSERTED) ||
                    (state == LE_SIM_READY)    ||
                    (state == LE_SIM_BLOCKED))
                {
                    // Get identification information
                    if(pa_sim_GetCardIdentification(iccid) != LE_OK)
                    {
                        LE_ERROR("Failed to get the ICCID of card number.%d", simPtr->num);
                        simPtr->ICCID[0] = '\0';
                    }
                    else
                    {
                        // Note that it is not valid to truncate the ICCID.
                        res = le_utf8_Copy(simPtr->ICCID, iccid, sizeof(simPtr->ICCID), NULL);
                    }
                }
            }
        }
        else
        {
            LE_ERROR("Failed to get the ICCID of card number.%d", simPtr->num);
            simPtr->ICCID[0] = '\0';
        }
    }
    else
    {
        res = LE_OK;
    }

    // The ICCID is available. Copy it to the result buffer.
    if (res == LE_OK)
    {
        res = le_utf8_Copy(iccidPtr, simPtr->ICCID, iccidLen, NULL);
    }

    // If the ICCID could not be retrieved for some reason, or a truncation error occurred when
    // copying the result, then ensure the cache is cleared.
    if (res != LE_OK)
    {
        simPtr->ICCID[0] = '\0';
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function retrieves the identification number (IMSI) of the SIM card. (max 15 digits)
 *
 * @return LE_OK            The IMSI was successfully retrieved.
 * @return LE_OVERFLOW      The imsiPtr buffer was too small for the IMSI.
 * @return LE_NOT_POSSIBLE  The IMSI could not be retrieved.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_GetIMSI
(
    le_sim_ObjRef_t simRef, ///< [IN] The SIM object.
    char * imsiPtr,         ///< [OUT] Buffer to hold the IMSI.
    size_t imsiLen          ///< [IN] Buffer length
)
{
    le_sim_States_t  state;
    pa_sim_Imsi_t    imsi;
    Sim_t*           simPtr = le_ref_Lookup(SimRefMap, simRef);
    le_result_t      res = LE_NOT_POSSIBLE;

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }
    if (imsiPtr == NULL)
    {
        LE_KILL_CLIENT("imsiPtr is NULL !");
        return LE_FAULT;
    }

    if (strlen(simPtr->IMSI) == 0)
    {
        if (SelectSIMCard(simPtr->num) == LE_OK)
        {
            if (pa_sim_GetState(&state) == LE_OK)
            {
                if (state == LE_SIM_READY)
                {
                    // Get identification information
                    if(pa_sim_GetIMSI(imsi) != LE_OK)
                    {
                        LE_ERROR("Failed to get the IMSI of card number.%d", simPtr->num);
                    }
                    else
                    {
                        // Note that it is not valid to truncate the IMSI.
                        res = le_utf8_Copy(simPtr->IMSI, imsi, sizeof(simPtr->IMSI), NULL);
                    }
                }
            }
        }
        else
        {
            LE_ERROR("Failed to get the IMSI of card number.%d", simPtr->num);
        }
    }
    else
    {
        res = LE_OK;
    }

    // The IMSI is available. Copy it to the result buffer.
    if (res == LE_OK)
    {
        res = le_utf8_Copy(imsiPtr, simPtr->IMSI, imsiLen, NULL);
    }

    // If the IMSI could not be retrieved for some reason, or a truncation error occurred when
    // copying the result, then ensure the cache is cleared.
    if (res != LE_OK)
    {
        simPtr->IMSI[0] = '\0';
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to verify if the SIM card is present or not.
 *
 * @return true  The SIM card is present.
 * @return false The SIM card is absent.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
bool le_sim_IsPresent
(
    le_sim_ObjRef_t simRef    ///< [IN] The SIM object.
)
{
    le_sim_States_t  state;
    Sim_t*           simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return false;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return false;
    }

    if (pa_sim_GetState(&state) == LE_OK)
    {
        if((state != LE_SIM_ABSENT) && (state != LE_SIM_STATE_UNKNOWN))
        {
            simPtr->isPresent = true;
            return true;
        }
        else
        {
            simPtr->isPresent = false;
            return false;
        }
    }
    else
    {
        simPtr->isPresent = false;
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to verify if the SIM is ready (PIN code correctly inserted or not
 * required).
 *
 * @return true  The PIN is correctly inserted or not required.
 * @return false The PIN must be inserted.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
bool le_sim_IsReady
(
    le_sim_ObjRef_t simRef    ///< [IN] The SIM object.
)
{
    le_sim_States_t state;
    Sim_t*          simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return false;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return false;
    }

    if (pa_sim_GetState(&state) == LE_OK)
    {
        if(state == LE_SIM_READY)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to enter the PIN code.
 *
 * @return LE_NOT_FOUND     The function failed to select the SIM card for this operation.
 * @return LE_UNDERFLOW     The PIN code is not long enough (min 4 digits).
 * @return LE_NOT_POSSIBLE  The function failed to enter the PIN code.
 * @return LE_OK            The function succeeded.
 *
 * @note If PIN code is too long (max 8 digits), it is a fatal error, the
 *       function will not return.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_EnterPIN
(
    le_sim_ObjRef_t simRef, ///< [IN] The SIM object.
    const char*  pinPtr     ///< [IN] The PIN code.
)
{
    pa_sim_Pin_t pinloc;
    Sim_t*       simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }

    if (pinPtr == NULL)
    {
        LE_KILL_CLIENT("pinPtr is NULL !");
        return LE_FAULT;
    }

    if (strlen(pinPtr) > LE_SIM_PIN_MAX_LEN )
    {
        LE_KILL_CLIENT("strlen(pin) > %d", LE_SIM_PIN_MAX_LEN);
        return LE_FAULT;
    }

    if(strlen(pinPtr) < LE_SIM_PIN_MIN_LEN)
    {
        return LE_UNDERFLOW;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return LE_NOT_FOUND;
    }

    if (!simPtr->isPresent)
    {
        return LE_NOT_FOUND;
    }

    // Enter PIN
    le_utf8_Copy(pinloc, pinPtr, sizeof(pinloc), NULL);
    if(pa_sim_EnterPIN(PA_SIM_PIN,pinloc) != LE_OK)
    {
        LE_ERROR("Failed to enter PIN.%s card number.%d", pinPtr, simPtr->num);
        return LE_NOT_POSSIBLE;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to change the PIN code.
 *
 * @return LE_NOT_FOUND     The function failed to select the SIM card for this operation.
 * @return LE_UNDERFLOW     The PIN code is/are not long enough (min 4 digits).
 * @return LE_NOT_POSSIBLE  The function failed to change the PIN code.
 * @return LE_OK            The function succeeded.
 *
 * @note If PIN code is/are too long (max 8 digits), it is a fatal error, the
 *       function will not return.
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_ChangePIN
(
    le_sim_ObjRef_t simRef, ///< [IN] The SIM object.
    const char*  oldpinPtr, ///< [IN] The old PIN code.
    const char*  newpinPtr  ///< [IN] The new PIN code.
)
{
    pa_sim_Pin_t oldpinloc;
    pa_sim_Pin_t newpinloc;
    Sim_t*       simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }
    if (oldpinPtr == NULL)
    {
        LE_KILL_CLIENT("oldpinPtr is NULL !");
        return LE_FAULT;
    }
    if (newpinPtr == NULL)
    {
        LE_KILL_CLIENT("newpinPtr is NULL !");
        return LE_FAULT;
    }

    if(strlen(oldpinPtr) > LE_SIM_PIN_MAX_LEN)
    {
        LE_KILL_CLIENT("strlen(oldpin) > %d", LE_SIM_PIN_MAX_LEN);
        return LE_FAULT;
    }

    if(strlen(newpinPtr) > LE_SIM_PIN_MAX_LEN)
    {
        LE_KILL_CLIENT("strlen(newpin) > %d", LE_SIM_PIN_MAX_LEN);
        return LE_FAULT;
    }

    if((strlen(oldpinPtr) < LE_SIM_PIN_MIN_LEN) || (strlen(newpinPtr) < LE_SIM_PIN_MIN_LEN))
    {
        return LE_UNDERFLOW;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return LE_NOT_FOUND;
    }

    if (!simPtr->isPresent)
    {
        return LE_NOT_FOUND;
    }

    // Change PIN
    le_utf8_Copy(oldpinloc, oldpinPtr, sizeof(oldpinloc), NULL);
    le_utf8_Copy(newpinloc, newpinPtr, sizeof(newpinloc), NULL);
    if(pa_sim_ChangePIN(PA_SIM_PIN, oldpinloc, newpinloc) != LE_OK)
    {
        LE_ERROR("Failed to set new PIN.%s of card number.%d", newpinPtr, simPtr->num);
        return LE_NOT_POSSIBLE;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the number of remaining PIN insertion tries.
 *
 * @return LE_NOT_FOUND     The function failed to select the SIM card for this operation.
 * @return LE_NOT_POSSIBLE  The function failed to get the number of remaining PIN insertion tries.
 * @return A positive value The function succeeded. The number of remaining PIN insertion tries.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
int32_t le_sim_GetRemainingPINTries
(
    le_sim_ObjRef_t simRef    ///< [IN] The SIM object.
)
{
    uint32_t  attempts=0;
    Sim_t*    simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return LE_NOT_FOUND;
    }

    if (!simPtr->isPresent)
    {
        return LE_NOT_FOUND;
    }

    if(pa_sim_GetPINRemainingAttempts(PA_SIM_PIN, &attempts) != LE_OK)
    {
        LE_ERROR("Failed to get reamining attempts for card number.%d", simPtr->num);
        return LE_NOT_POSSIBLE;
    }

    return attempts;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unlock the SIM card: it disables the request of the PIN code.
 *
 * @return LE_NOT_FOUND     The function failed to select the SIM card for this operation.
 * @return LE_UNDERFLOW     The PIN code is not long enough (min 4 digits).
 * @return LE_NOT_POSSIBLE  The function failed to unlock the SIM card.
 * @return LE_OK            The function succeeded.
 *
 * @note If PIN code is too long (max 8 digits), it is a fatal error, the
 *       function will not return.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_Unlock
(
    le_sim_ObjRef_t simRef, ///< [IN] The SIM object.
    const char*     pinPtr  ///< [IN] The PIN code.
)
{
    pa_sim_Pin_t pinloc;
    Sim_t*       simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }

    if (pinPtr == NULL)
    {
        LE_KILL_CLIENT("pinPtr is NULL !");
        return LE_FAULT;
    }

    if(strlen(pinPtr) > LE_SIM_PIN_MAX_LEN)
    {
        LE_KILL_CLIENT("strlen(pinPtr) > %d", LE_SIM_PIN_MAX_LEN);
        return LE_FAULT;
    }

    if(strlen(pinPtr) < LE_SIM_PIN_MIN_LEN)
    {
        return LE_UNDERFLOW;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return LE_NOT_FOUND;
    }

    if (!simPtr->isPresent)
    {
        return LE_NOT_FOUND;
    }

    // Unlock card
    le_utf8_Copy(pinloc, pinPtr, sizeof(pinloc), NULL);
    if(pa_sim_DisablePIN(PA_SIM_PIN, pinloc) != LE_OK)
    {
        LE_ERROR("Failed to unlock card number.%d", simPtr->num);
        return LE_NOT_POSSIBLE;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to Lock the SIM card: it enables the request of the PIN code.
 *
 * @return LE_NOT_FOUND     The function failed to select the SIM card for this operation.
 * @return LE_UNDERFLOW     The PIN code is not long enough (min 4 digits).
 * @return LE_NOT_POSSIBLE  The function failed to unlock the SIM card.
 * @return LE_OK            The function succeeded.
 *
 * @note If PIN code is too long (max 8 digits), it is a fatal error, the
 *       function will not return.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_Lock
(
    le_sim_ObjRef_t simRef, ///< [IN] The SIM object.
    const char*     pinPtr  ///< [IN] The PIN code.
)
{
    pa_sim_Pin_t pinloc;
    Sim_t*       simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }
    if (pinPtr == NULL)
    {
        LE_KILL_CLIENT("pinPtr is NULL !");
        return LE_FAULT;
    }

    if(strlen(pinPtr) > LE_SIM_PIN_MAX_LEN)
    {
        LE_KILL_CLIENT("strlen(pinPtr) > %d", LE_SIM_PIN_MAX_LEN);
        return LE_FAULT;
    }

    if(strlen(pinPtr) < LE_SIM_PIN_MIN_LEN)
    {
        return LE_UNDERFLOW;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return LE_NOT_FOUND;
    }

    if (!simPtr->isPresent)
    {
        return LE_NOT_FOUND;
    }

    // Unlock card
    le_utf8_Copy(pinloc, pinPtr, sizeof(pinloc), NULL);
    if(pa_sim_EnablePIN(PA_SIM_PIN, pinloc) != LE_OK)
    {
        LE_ERROR("Failed to Lock card number.%d", simPtr->num);
        return LE_NOT_POSSIBLE;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unblock the SIM card.
 *
 * @return LE_NOT_FOUND     The function failed to select the SIM card for this operation.
 * @return LE_UNDERFLOW     The PIN code is not long enough (min 4 digits).
 * @return LE_OUT_OF_RANGE  The PUK code length is not correct (8 digits).
 * @return LE_NOT_POSSIBLE  The function failed to unlock the SIM card.
 * @return LE_OK            The function succeeded.
 *
 * @note If new PIN or puk code are too long (max 8 digits), it is a fatal error, the
 *       function will not return.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_Unblock
(
    le_sim_ObjRef_t simRef,    ///< [IN] The SIM object.
    const char*     pukPtr,    ///< [IN] The PUK code.
    const char*     newpinPtr  ///< [IN] The new PIN code.
)
{
    pa_sim_Puk_t pukloc;
    pa_sim_Pin_t newpinloc;
    Sim_t*       simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }
    if (pukPtr == NULL)
    {
        LE_KILL_CLIENT("pukPtr is NULL !");
        return LE_FAULT;
    }
    if (newpinPtr == NULL)
    {
        LE_KILL_CLIENT("newpinPtr is NULL !");
        return LE_FAULT;
    }

    if(strlen(pukPtr) != LE_SIM_PUK_LEN)
    {
        return LE_OUT_OF_RANGE;
    }


    if(strlen(newpinPtr) > LE_SIM_PIN_MAX_LEN)
    {
        LE_KILL_CLIENT("strlen(newpinPtr) > %d", LE_SIM_PIN_MAX_LEN);
        return LE_FAULT;
    }

    if(strlen(pukPtr) > LE_SIM_PIN_MAX_LEN)
    {
        LE_KILL_CLIENT("strlen(pukPtr) > %d", LE_SIM_PIN_MAX_LEN);
        return LE_FAULT;
    }

    if(strlen(newpinPtr) < LE_SIM_PIN_MIN_LEN)
    {
        return LE_UNDERFLOW;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return LE_NOT_FOUND;
    }

    if (!simPtr->isPresent)
    {
        return LE_NOT_FOUND;
    }

    // Unblock card
    le_utf8_Copy(pukloc, pukPtr, sizeof(pukloc), NULL);
    le_utf8_Copy(newpinloc, newpinPtr, sizeof(newpinloc), NULL);
    if(pa_sim_EnterPUK(PA_SIM_PUK,pukloc, newpinloc) != LE_OK)
    {
        LE_ERROR("Failed to unblock card number.%d", simPtr->num);
        return LE_NOT_POSSIBLE;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the SIM state.
 *
 * @return The current SIM state.
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_sim_States_t le_sim_GetState
(
    le_sim_ObjRef_t simRef    ///< [IN] The SIM object.
)
{
    le_sim_States_t state;
    Sim_t*          simPtr = le_ref_Lookup(SimRefMap, simRef);

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_SIM_STATE_UNKNOWN;
    }

    if (SelectSIMCard(simPtr->num) != LE_OK)
    {
        return LE_SIM_STATE_UNKNOWN;
    }

    if (pa_sim_GetState(&state) == LE_OK)
    {
        return state;
    }
    else
    {
        return LE_SIM_STATE_UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register an handler function for New State notification.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_sim_NewStateHandlerRef_t le_sim_AddNewStateHandler
(
    le_sim_NewStateHandlerFunc_t handlerFuncPtr, ///< [IN] The handler function for New State notification.
    void*                        contextPtr      ///< [IN] The handler's context.
)
{
    le_event_HandlerRef_t handlerRef;

    if (handlerFuncPtr == NULL)
    {
        LE_KILL_CLIENT("Handler function is NULL !");
        return NULL;
    }

    handlerRef = le_event_AddLayeredHandler("NewSimStateHandler",
                                            NewSimStateEventId,
                                            FirstLayerNewSimStateHandler,
                                            (le_event_HandlerFunc_t)handlerFuncPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (le_sim_NewStateHandlerRef_t)(handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to unregister a handler function
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
void le_sim_RemoveNewStateHandler
(
    le_sim_NewStateHandlerRef_t   handlerRef ///< [IN] The handler reference.
)
{
    le_event_RemoveHandler((le_event_HandlerRef_t)handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SIM Phone Number.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the Phone Number can't fit in phoneNumberStr
 *      - LE_NOT_POSSIBLE on any other failure
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_GetSubscriberPhoneNumber
(
    le_sim_ObjRef_t simRef,            ///< [IN]  SIM object.
    char           *phoneNumberStr,    ///< [OUT] The phone Number
    size_t          phoneNumberStrSize ///< [IN]  Size of phoneNumberStr
)
{
    le_sim_States_t  state;
    char             phoneNumber[LE_MDMDEFS_PHONE_NUM_MAX_LEN] = {0} ;
    Sim_t*           simPtr = le_ref_Lookup(SimRefMap, simRef);
    le_result_t      res = LE_NOT_POSSIBLE;

    if (simPtr == NULL)
    {
        LE_KILL_CLIENT("Invalid reference (%p) provided!", simRef);
        return LE_FAULT;
    }
    if (phoneNumberStr == NULL)
    {
        LE_KILL_CLIENT("phoneNumberStr is NULL !");
        return LE_FAULT;
    }

    if (strlen(simPtr->phoneNumber) == 0)
    {
        if (SelectSIMCard(simPtr->num) == LE_OK)
        {
            if (pa_sim_GetState(&state) == LE_OK)
            {
                LE_DEBUG("Try get the Phone Number of card number.%d in state %d", simPtr->num, state);
                if ((state == LE_SIM_INSERTED) ||
                    (state == LE_SIM_READY)    ||
                    (state == LE_SIM_BLOCKED))
                {
                    // Get identification information
                    if(pa_sim_GetSubscriberPhoneNumber(phoneNumber, sizeof(phoneNumber)) != LE_OK)
                    {
                        LE_ERROR("Failed to get the Phone Number of card number.%d", simPtr->num);
                        simPtr->phoneNumber[0] = '\0';
                    }
                    else
                    {
                        // Note that it is not valid to truncate the ICCID.
                        res = le_utf8_Copy(simPtr->phoneNumber,
                                           phoneNumber, sizeof(simPtr->phoneNumber), NULL);
                    }
                }
            }
        }
        else
        {
            LE_ERROR("Failed to get the Phone Number of card number.%d", simPtr->num);
            simPtr->phoneNumber[0] = '\0';
        }
    }
    else
    {
        res = LE_OK;
    }

    // The ICCID is available. Copy it to the result buffer.
    if (res == LE_OK)
    {
        res = le_utf8_Copy(phoneNumberStr,simPtr->phoneNumber,phoneNumberStrSize,NULL);
    }

    // If the ICCID could not be retrieved for some reason, or a truncation error occurred when
    // copying the result, then ensure the cache is cleared.
    if (res != LE_OK)
    {
        simPtr->phoneNumber[0] = '\0';
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Home Network Name information.
 *
 * @return
 *      - LE_OK on success
 *      - LE_OVERFLOW if the Home Network Name can't fit in nameStr
 *      - LE_NOT_POSSIBLE on any other failure
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_sim_GetHomeNetworkOperator
(
    char       *nameStr,               ///< [OUT] the home network Name
    size_t      nameStrSize            ///< [IN] the nameStr size
)
{
    if (nameStr == NULL)
    {
        LE_KILL_CLIENT("nameStr is NULL !");
        return LE_FAULT;
    }

    return pa_sim_GetHomeNetworkOperator(nameStr,nameStrSize);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to get the Home Network MCC MNC.
 *
 * @return
 *      - LE_OK on success
 *      - LE_NOT_FOUND if Home Network has not been provisioned
 *      - LE_FAULT for unexpected error
 *
 * @note If the caller is passing a bad pointer into this function, it is a fatal error, the
 *       function will not return.
 *
 */
//--------------------------------------------------------------------------------------------------
 le_result_t le_sim_GetHomeNetworkMccMnc
(
    char     *mccPtr,                ///< [OUT] Mobile Country Code
    size_t    mccPtrSize,            ///< [IN] mccPtr buffer size
    char     *mncPtr,                ///< [OUT] Mobile Network Code
    size_t    mncPtrSize             ///< [IN] mncPtr buffer size
)
{
    if (mccPtr == NULL)
    {
        LE_KILL_CLIENT("mccPtr is NULL");
        return LE_FAULT;
    }

    if (mncPtr == NULL)
    {
        LE_KILL_CLIENT("mncPtr is NULL");
        return LE_FAULT;
    }

    return pa_sim_GetHomeNetworkMccMnc(mccPtr, mccPtrSize, mncPtr, mncPtrSize);
}
