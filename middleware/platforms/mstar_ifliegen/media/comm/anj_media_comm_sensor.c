
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "anj_mw_media_common.h"

MI_S32 ST_Common_SensorInit(MI_SNR_PADID eSnrPad, MI_U32 u32Fps, MI_U8 u8ResIdx)
{
    MI_SNR_PADID eSnrPadId = eSnrPad;
    MI_SNR_PADInfo_t stPad0Info;
    MI_SNR_PlaneInfo_t stSnrPlane0Info;
    MI_U32 u32ResCount = 0;
    MI_U8 u8ResIndex = 0;
    MI_SNR_Res_t stRes;

    memset(&stRes, 0x0, sizeof(MI_SNR_Res_t));
    memset(&stPad0Info, 0x0, sizeof(MI_SNR_PADInfo_t));
    memset(&stSnrPlane0Info, 0x0, sizeof(MI_SNR_PlaneInfo_t));

    /************************************************
    Step2:  init Sensor
    *************************************************/

    STCHECKRESULT(MI_SNR_SetPlaneMode(eSnrPad, FALSE));

    STCHECKRESULT(MI_SNR_QueryResCount(eSnrPadId, &u32ResCount));
    for (u8ResIndex = 0; u8ResIndex < u32ResCount; u8ResIndex++)
    {
        STCHECKRESULT(MI_SNR_GetRes(eSnrPadId, u8ResIndex, &stRes));
        __INFO("index %d, Crop(%d,%d,%d,%d), outputsize(%d,%d), maxfps %d, minfps %d, ResDesc %s\n",
               u8ResIndex,
               stRes.stCropRect.u16X, stRes.stCropRect.u16Y, stRes.stCropRect.u16Width, stRes.stCropRect.u16Height,
               stRes.stOutputSize.u16Width, stRes.stOutputSize.u16Height,
               stRes.u32MaxFps, stRes.u32MinFps,
               stRes.strResDesc);
    }

    __INFO("sensor(%d)(max:%d): %d\n", eSnrPadId, u32ResCount - 1, u8ResIdx);

    STCHECKRESULT(MI_SNR_GetRes(eSnrPadId, u8ResIdx, &stRes));
    if (u32Fps > stRes.u32MaxFps)
    {
        u32Fps = stRes.u32MaxFps;
    }
    STCHECKRESULT(MI_SNR_SetFps(eSnrPadId, u32Fps));
    STCHECKRESULT(MI_SNR_SetRes(eSnrPadId, u8ResIdx));
    STCHECKRESULT(MI_SNR_Enable(eSnrPadId));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SensorUnInit(MI_SNR_PADID eSnrPadId)
{
    STCHECKRESULT(MI_SNR_Disable(eSnrPadId));
    return MI_SUCCESS;
}

MI_S32 ST_Common_SensorGetRectInfo(MI_SNR_PADID eSnrPad, MI_SYS_WindowRect_t *pstSnrRes)
{
    MI_U32 u32PlaneId = 0;
    MI_SNR_PlaneInfo_t stSnrPlane0Info;
    memset(&stSnrPlane0Info, 0x0, sizeof(MI_SNR_PlaneInfo_t));

    STCHECKRESULT(MI_SNR_GetPlaneInfo(eSnrPad, u32PlaneId, &stSnrPlane0Info));

    memcpy(pstSnrRes, &stSnrPlane0Info.stCapRect, sizeof(MI_SYS_WindowRect_t));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SensorMirrorSet(MI_SNR_PADID eSnrPadId, MI_BOOL bHFlip, MI_BOOL bVFlip)
{
    MI_S32 s32Ret = MI_SUCCESS;
    s32Ret = MI_SNR_SetOrien(eSnrPadId, bHFlip, bVFlip);
    #if 0
    int times = 0;

    do 
    {
        bVFlip = !bVFlip;
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("MI_SNR_SetOrien %d %d %d failed, ret:%x!\n", eSnrPadId, bHFlip, bVFlip, s32Ret);
        }
        else
        {
            __INFO("MI_SNR_SetOrien %d %d %d OK\n", eSnrPadId, bHFlip, bVFlip);
            break;
        }
        times++;
        usleep(100 * 1000);
    } while( times < 10);
    #endif
    return s32Ret;
}


MI_S32 ST_Common_SensorFpsGet(MI_SNR_PADID eSnrPadId, int *fps)
{
    MI_U32 tmpfps = 0;
    STCHECKRESULT(MI_SNR_GetFps(eSnrPadId, &tmpfps));

    *fps = (int )tmpfps;
    return MI_SUCCESS;
}

MI_S32 ST_Common_SensorFpsSet(MI_SNR_PADID eSnrPadId, int fps)
{
    MI_U32 tmpfps = (MI_U32)fps;
    STCHECKRESULT(MI_SNR_SetFps(eSnrPadId, tmpfps));

    return MI_SUCCESS;
}

