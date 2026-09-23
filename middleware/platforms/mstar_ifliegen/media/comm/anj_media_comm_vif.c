
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "anj_mw_media_common.h"

MI_S32 ST_Common_VifInit(ST_Common_VifAttr_t *pstVifAttr, MI_SNR_PADID eSnrPadId)
{
    if (NULL == pstVifAttr)
    {
        __ERR("Snr:%d pstVifAttr is NULL\n", eSnrPadId);
        return MI_FAILED;
    }

    MI_U32 u32PlaneId = 0;
    MI_SNR_PADInfo_t stPad0Info;
    memset(&stPad0Info, 0x0, sizeof(MI_SNR_PADInfo_t));
    STCHECKRESULT(MI_SNR_GetPadInfo(eSnrPadId, &stPad0Info));

    MI_SNR_PlaneInfo_t stSnrPlane0Info;
    memset(&stSnrPlane0Info, 0x0, sizeof(MI_SNR_PlaneInfo_t));
    STCHECKRESULT(MI_SNR_GetPlaneInfo(eSnrPadId, u32PlaneId, &stSnrPlane0Info));

    __INFO("get sensor pad :%d info\n", eSnrPadId);

    MI_VIF_GroupAttr_t stGroupAttr;
    memset(&stGroupAttr, 0x0, sizeof(MI_VIF_GroupAttr_t));
    stGroupAttr.eIntfMode = (MI_VIF_IntfMode_e)stPad0Info.eIntfMode;
    stGroupAttr.eWorkMode = E_MI_VIF_WORK_MODE_1MULTIPLEX;
    stGroupAttr.eScanMode = E_MI_SYS_FRAME_SCAN_MODE_PROGRESSIVE;
    stGroupAttr.eHDRType = E_MI_VIF_HDR_TYPE_OFF;
    stGroupAttr.u32GroupStitchMask = 0;
    if (stGroupAttr.eIntfMode == E_MI_VIF_MODE_BT656)
    {
        stGroupAttr.eClkEdge = (MI_VIF_ClkEdge_e)stPad0Info.unIntfAttr.stBt656Attr.eClkEdge;
    }
    else
    {
        stGroupAttr.eClkEdge = E_MI_VIF_CLK_EDGE_DOUBLE;
    }

    STCHECKRESULT(MI_VIF_CreateDevGroup(pstVifAttr->VifGroupId, &stGroupAttr));

    MI_VIF_DevAttr_t stVifDevAttr;
    memset(&stVifDevAttr, 0x0, sizeof(MI_VIF_DevAttr_t));
    stVifDevAttr.stInputRect.u16X = stSnrPlane0Info.stCapRect.u16X;
    stVifDevAttr.stInputRect.u16Y = stSnrPlane0Info.stCapRect.u16Y;
    stVifDevAttr.stInputRect.u16Width = stSnrPlane0Info.stCapRect.u16Width;
    stVifDevAttr.stInputRect.u16Height = stSnrPlane0Info.stCapRect.u16Height;
    stVifDevAttr.eField = E_MI_SYS_FIELDTYPE_NONE;
    stVifDevAttr.bEnH2T1PMode = FALSE;
    if (stSnrPlane0Info.eBayerId >= E_MI_SYS_PIXEL_BAYERID_MAX)
    {
        stVifDevAttr.eInputPixel = stSnrPlane0Info.ePixel;
    }
    else
    {
        stVifDevAttr.eInputPixel = (MI_SYS_PixelFormat_e)RGB_BAYER_PIXEL(stSnrPlane0Info.ePixPrecision, stSnrPlane0Info.eBayerId);
    }

    __INFO("set vif dev attr (%d,%d,%d,%d) \n", stVifDevAttr.stInputRect.u16X,
                   stVifDevAttr.stInputRect.u16Y, stVifDevAttr.stInputRect.u16Width,
                   stVifDevAttr.stInputRect.u16Height);
    STCHECKRESULT(MI_VIF_SetDevAttr(pstVifAttr->VifDevId, &stVifDevAttr));

    STCHECKRESULT(MI_VIF_EnableDev(pstVifAttr->VifDevId));

    MI_VIF_OutputPortAttr_t stVifPortInfo;
    memset(&stVifPortInfo, 0, sizeof(MI_VIF_OutputPortAttr_t));
    stVifPortInfo.stCapRect.u16X = stVifDevAttr.stInputRect.u16X;
    stVifPortInfo.stCapRect.u16Y = stVifDevAttr.stInputRect.u16Y;
    stVifPortInfo.stCapRect.u16Width = stVifDevAttr.stInputRect.u16Width;
    stVifPortInfo.stCapRect.u16Height = stVifDevAttr.stInputRect.u16Height;
    stVifPortInfo.stDestSize.u16Width = stVifDevAttr.stInputRect.u16Width;
    stVifPortInfo.stDestSize.u16Height = stVifDevAttr.stInputRect.u16Height;
    stVifPortInfo.ePixFormat = stVifDevAttr.eInputPixel;
    stVifPortInfo.eFrameRate = E_MI_VIF_FRAMERATE_FULL;
    stVifPortInfo.eCompressMode = E_MI_SYS_COMPRESS_MODE_NONE;
    STCHECKRESULT(MI_VIF_SetOutputPortAttr(pstVifAttr->VifDevId, pstVifAttr->VifOutPortId, &stVifPortInfo));

    STCHECKRESULT(MI_VIF_EnableOutputPort(pstVifAttr->VifDevId, pstVifAttr->VifOutPortId));
    return MI_SUCCESS;
}

MI_S32 ST_Common_VifUnInit(ST_Common_VifAttr_t *pstVifAttr)
{
    if (NULL == pstVifAttr)
    {
        __ERR("pstVifAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_VIF_DisableOutputPort(pstVifAttr->VifDevId, pstVifAttr->VifOutPortId));

    STCHECKRESULT(MI_VIF_DisableDev(pstVifAttr->VifDevId));

    STCHECKRESULT(MI_VIF_DestroyDevGroup(pstVifAttr->VifOutPortId));

    return MI_SUCCESS;
}


MI_S32 ST_Common_VifOutputPortDisable(ST_Common_VifAttr_t *pstVifAttr)
{
    if (NULL == pstVifAttr)
    {
        __ERR("pstVifAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_VIF_DisableOutputPort(pstVifAttr->VifDevId, pstVifAttr->VifOutPortId));
    return MI_SUCCESS;
}

MI_S32 ST_Common_VifOutputPortEnable(ST_Common_VifAttr_t *pstVifAttr)
{
    if (NULL == pstVifAttr)
    {
        __ERR("pstVifAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_VIF_EnableOutputPort(pstVifAttr->VifDevId, pstVifAttr->VifOutPortId));
    return MI_SUCCESS;
}

