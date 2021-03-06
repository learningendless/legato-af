 /**
  * This module implements the le_mrc's unit tests.
  *
  *
  * Copyright (C) Sierra Wireless, Inc. 2013-2014. Use of this work is subject to license.
  *
  */

#include "legato.h"
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <semaphore.h>

// Header files for CUnit
#include "Console.h"
#include <Basic.h>

#include "interfaces.h"
#include "main.h"



//--------------------------------------------------------------------------------------------------
/**
 * Handler function for RAT change Notifications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void TestRatHandler
(
    le_mrc_Rat_t rat,
    void*        contextPtr
)
{
    LE_INFO("New RAT: %d", rat);

    if (rat == LE_MRC_RAT_CDMA)
    {
        LE_INFO("Check RatHandler passed, RAT is LE_MRC_RAT_CDMA.");
    }
    else if (rat == LE_MRC_RAT_GSM)
    {
        LE_INFO("Check RatHandler passed, RAT is LE_MRC_RAT_GSM.");
    }
    else if (rat == LE_MRC_RAT_UMTS)
    {
        LE_INFO("Check RatHandler passed, RAT is LE_MRC_RAT_UMTS.");
    }
    else if (rat == LE_MRC_RAT_LTE)
    {
        LE_INFO("Check RatHandler passed, RAT is LE_MRC_RAT_LTE.");
    }
    else
    {
        LE_INFO("Check RatHandler failed, bad RAT.");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for Network Registration Notifications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void TestNetRegHandler
(
    le_mrc_NetRegState_t state,
    void*                contextPtr
)
{
    LE_INFO("New Network Registration state: %d", state);

    if (state == LE_MRC_REG_NONE)
    {
        LE_INFO("Check NetRegHandler passed, state is LE_MRC_REG_NONE.");
    }
    else if (state == LE_MRC_REG_HOME)
    {
        LE_INFO("Check NetRegHandler passed, state is LE_MRC_REG_HOME.");
    }
    else if (state == LE_MRC_REG_SEARCHING)
    {
        LE_INFO("Check NetRegHandler passed, state is LE_MRC_REG_SEARCHING.");
    }
    else if (state == LE_MRC_REG_DENIED)
    {
        LE_INFO("Check NetRegHandler passed, state is LE_MRC_REG_DENIED.");
    }
    else if (state == LE_MRC_REG_ROAMING)
    {
        LE_INFO("Check NetRegHandler passed, state is LE_MRC_REG_ROAMING.");
    }
    else if (state == LE_MRC_REG_UNKNOWN)
    {
        LE_INFO("Check NetRegHandler passed, state is LE_MRC_REG_UNKNOWN.");
    }
    else
    {
        LE_INFO("Check NetRegHandler failed, bad Network Registration state.");
    }
}



//--------------------------------------------------------------------------------------------------
//                                       Test Functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Test: Radio Power Management.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_mrc_Power()
{
    le_result_t   res;
    le_onoff_t    onoff;

    res=le_mrc_SetRadioPower(LE_OFF);
    CU_ASSERT_EQUAL(res, LE_OK);

    sleep(3);

    res=le_mrc_GetRadioPower(&onoff);
    CU_ASSERT_EQUAL(res, LE_OK);
    CU_ASSERT_EQUAL(onoff, LE_OFF);

    res=le_mrc_SetRadioPower(LE_ON);
    CU_ASSERT_EQUAL(res, LE_OK);

    sleep(3);

    res=le_mrc_GetRadioPower(&onoff);
    CU_ASSERT_EQUAL(res, LE_OK);
    CU_ASSERT_EQUAL(onoff, LE_ON);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Radio Access Technology.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_mrc_GetRat()
{
    le_result_t   res;
    le_mrc_Rat_t  rat;

    res=le_mrc_GetRadioAccessTechInUse(&rat);
    CU_ASSERT_EQUAL(res, LE_OK);
    if (res == LE_OK)
    {
        CU_ASSERT_TRUE((rat>=LE_MRC_RAT_UNKNOWN) && (rat<=LE_MRC_RAT_LTE));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Network Registration State + Signal Quality.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_mrc_GetStateAndQual()
{
    le_result_t           res;
    le_mrc_NetRegState_t  state;
    uint32_t              quality;

    res=le_mrc_GetNetRegState(&state);
    CU_ASSERT_EQUAL(res, LE_OK);
    if (res == LE_OK)
    {
        CU_ASSERT_TRUE((state>=LE_MRC_REG_NONE) && (state<=LE_MRC_REG_UNKNOWN));
    }

    res=le_mrc_GetSignalQual(&quality);
    CU_ASSERT_EQUAL(res, LE_OK);
    if (res == LE_OK)
    {
        CU_ASSERT_TRUE((quality>=0) && (quality<=5));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Neighbor Cells Information.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_mrc_GetNeighboringCellsInfo()
{
    le_mrc_NeighborCellsRef_t   ngbrRef;
    le_mrc_CellInfoRef_t        cellRef;
    uint32_t                    i=0;
    uint32_t                    cid=0;
    uint32_t                    lac=0;
    int32_t                     rxLevel=0;

    LE_INFO("Start Testle_mrc_GetNeighborCellsInfo");

    ngbrRef=le_mrc_GetNeighborCellsInfo();
    CU_ASSERT_PTR_NOT_NULL(ngbrRef);

    if (ngbrRef)
    {
        i=0;

        cellRef = le_mrc_GetFirstNeighborCellInfo(ngbrRef);
        CU_ASSERT_PTR_NOT_NULL(cellRef);
        cid=le_mrc_GetNeighborCellId(cellRef);
        lac=le_mrc_GetNeighborCellLocAreaCode(cellRef);
        rxLevel=le_mrc_GetNeighborCellRxLevel(cellRef);
        LE_INFO("Cell #%d, cid=%d, lac=%d, rxLevel=%d", i, cid, lac, rxLevel);

        while((cellRef =  le_mrc_GetNextNeighborCellInfo(ngbrRef)) != NULL)
        {
            i++;
            cid=le_mrc_GetNeighborCellId(cellRef);
            lac=le_mrc_GetNeighborCellLocAreaCode(cellRef);
            rxLevel=le_mrc_GetNeighborCellRxLevel(cellRef);
            LE_INFO("Cell #%d, cid=%d, lac=%d, rxLevel=%d", i, cid, lac, rxLevel);
        }

        le_mrc_DeleteNeighborCellsInfo(ngbrRef);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: Network Registration notification handling.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_mrc_NetRegHdlr()
{
    le_mrc_NetRegStateHandlerRef_t testHdlrRef;

    testHdlrRef=le_mrc_AddNetRegStateHandler(TestNetRegHandler, NULL);
    CU_ASSERT_PTR_NOT_NULL(testHdlrRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Test: RAT change handling.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_mrc_RatHdlr()
{
    le_mrc_RatChangeHandlerRef_t testHdlrRef;

    testHdlrRef=le_mrc_AddRatChangeHandler(TestRatHandler, NULL);
    CU_ASSERT_PTR_NOT_NULL(testHdlrRef);
}
