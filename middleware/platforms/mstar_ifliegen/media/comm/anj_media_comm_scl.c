#include "anj_mw_media_common.h"

#ifdef _USE_MODULE_AOV_
#include "anj_mw_aov_common.h"
#endif

MI_S32 ST_Common_SclGetYuv(MI_SYS_ChnPort_t *pstChnPort, ST_Common_Scl_DataCb datacb, void *param, int *bStart)
{
    MI_S32 iRet = MI_SUCCESS;
    MI_SYS_BufInfo_t stBufInfo = {0};
    MI_SYS_BUF_HANDLE hHandle = {0};
    MI_S32 s32Fd = -1;
    fd_set read_fds;
    struct timeval TimeoutVal;
    
    iRet = MI_SYS_GetFd(pstChnPort, &s32Fd);
    if (MI_SUCCESS != iRet || s32Fd < 0)
    {
        __ERR("MI_SYS_GetFd failed, ret:%#x\n", iRet);
        return MI_FAILED;
    }
    
    while (bStart && *bStart)
    {
        FD_ZERO(&read_fds);
        FD_SET(s32Fd, &read_fds);
        TimeoutVal.tv_sec  = 0;
        TimeoutVal.tv_usec = 200 * 1000;
        
        iRet = select(s32Fd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (iRet <= 0)
        {
            continue;
        }
        if (!FD_ISSET(s32Fd, &read_fds))
        {
            continue;
        }

#ifdef _USE_MODULE_AOV_
        aov_com_wait_notify_algo_detect_start();
#endif
        iRet = MI_SYS_ChnOutputPortGetBuf(pstChnPort, &stBufInfo, &hHandle);
        if (iRet == MI_SUCCESS)
        {
            if (datacb)
            {
                datacb(pstChnPort->u32ChnId, (void *)stBufInfo.stFrameData.pVirAddr[0], stBufInfo.stFrameData.phyAddr[0], stBufInfo.stFrameData.u32BufSize, param);
            }
            MI_SYS_ChnOutputPortPutBuf(hHandle);
        }
        else
        {
            __ERR("MI_SYS_ChnOutputPortGetBuf failed, iRet:%#x, u32ChnId:%d, u32PortId:%d \n", 
                iRet, pstChnPort->u32ChnId, pstChnPort->u32PortId);
            usleep(10 * 1000);
        }

#ifdef _USE_MODULE_AOV_
        aov_com_notify_algo_detect_done();
#endif
    }

    if (s32Fd > 0)
    {
        MI_SYS_CloseFd(s32Fd);
        s32Fd = -1;
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_SclInit(MI_SCL_DEV SclDevId)
{
    MI_SCL_DevAttr_t stSclDevAttr;
    memset(&stSclDevAttr, 0, sizeof(MI_ISP_DevAttr_t));

    for (int i = 0; i < MAX_SCL_PORT; i++)
    {
        stSclDevAttr.u32NeedUseHWOutPortMask |= (1 << i);
    }

    STCHECKRESULT(MI_SCL_CreateDevice(SclDevId, &stSclDevAttr));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SclUnInit(MI_SCL_DEV SclDevId)
{
    STCHECKRESULT(MI_SCL_DestroyDevice(SclDevId));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SclPortStart(ST_Common_SclStartParam_t *pstSclStartParam, MI_SCL_OutPortParam_t *pstSclOutputParam)
{
    MI_SCL_DEV SclDevId = pstSclStartParam->SclDevId;
    MI_SCL_CHANNEL SclChnId = pstSclStartParam->SclChnId;
    MI_SCL_PORT SclOutPortId = pstSclStartParam->SclOutPortId;

    STCHECKRESULT(MI_SCL_SetOutputPortParam(SclDevId, SclChnId, SclOutPortId, pstSclOutputParam));

    STCHECKRESULT(MI_SCL_EnableOutputPort(SclDevId, SclChnId, SclOutPortId));
    return MI_SUCCESS;
}

MI_S32 ST_Common_SclPortFrmrateSet(ST_Common_SclStartParam_t *pstSclStartParam, MI_U32 u32SrcFrmrate, MI_U32 u32DstFrmrate)
{
    MI_SCL_DEV SclDevId = pstSclStartParam->SclDevId;
    MI_SCL_CHANNEL SclChnId = pstSclStartParam->SclChnId;
    MI_SCL_PORT SclOutPortId = pstSclStartParam->SclOutPortId;

    MI_SYS_ChnPort_t stSclChnPort;
    memset(&stSclChnPort, 0x0, sizeof(MI_SYS_ChnPort_t));
    stSclChnPort.eModId = E_MI_MODULE_ID_SCL;
    stSclChnPort.u32DevId = SclDevId;
    stSclChnPort.u32ChnId = SclChnId;
    stSclChnPort.u32PortId = SclOutPortId;
    STCHECKRESULT(MI_SYS_SetChnOutputPortUserFrc(&stSclChnPort, u32SrcFrmrate, u32DstFrmrate));
    return MI_SUCCESS;
}

MI_S32 ST_Common_SclPortDepthSet(ST_Common_SclStartParam_t *pstSclStartParam, MI_U32 u32Depth)
{
    MI_SCL_DEV SclDevId = pstSclStartParam->SclDevId;
    MI_SCL_CHANNEL SclChnId = pstSclStartParam->SclChnId;
    MI_SCL_PORT SclOutPortId = pstSclStartParam->SclOutPortId;

    MI_SYS_ChnPort_t stSclChnPort;
    memset(&stSclChnPort, 0x0, sizeof(MI_SYS_ChnPort_t));
    stSclChnPort.eModId = E_MI_MODULE_ID_SCL;
    stSclChnPort.u32DevId = SclDevId;
    stSclChnPort.u32ChnId = SclChnId;
    stSclChnPort.u32PortId = SclOutPortId;

    __INFO("portid:%d u32Depth:%d\n", SclOutPortId, u32Depth);
    if (u32Depth)
    {
        STCHECKRESULT(MI_SYS_SetChnOutputPortDepth(0, &stSclChnPort, 1, u32Depth));
    }
    else
    {
        STCHECKRESULT(MI_SYS_SetChnOutputPortDepth(0, &stSclChnPort, 0, 1));
    }
    return MI_SUCCESS;
}

MI_S32 ST_Common_SclStart(ST_Common_SclStartParam_t *pstSclStartParam)
{
    MI_SCL_DEV SclDevId = pstSclStartParam->SclDevId;
    MI_SCL_CHANNEL SclChnId = pstSclStartParam->SclChnId;

    MI_SCL_ChannelAttr_t stSclChnAttr;
    memset(&stSclChnAttr, 0, sizeof(MI_SCL_ChannelAttr_t));

    STCHECKRESULT(MI_SCL_CreateChannel(SclDevId, SclChnId, &stSclChnAttr));

    if (SclDevId != MI_SCL_DEV_ISP_REALTIME0)
    {
        MI_SYS_WindowRect_t stSclChnCropInfo;
        memset(&stSclChnCropInfo, 0, sizeof(MI_SYS_WindowRect_t));

        stSclChnCropInfo.u16X = pstSclStartParam->stSclChnCropInfo.u16X;
        stSclChnCropInfo.u16Y = pstSclStartParam->stSclChnCropInfo.u16Y;
        stSclChnCropInfo.u16Width = pstSclStartParam->stSclChnCropInfo.u16Width;
        stSclChnCropInfo.u16Height = pstSclStartParam->stSclChnCropInfo.u16Height;

        STCHECKRESULT(MI_SCL_SetInputPortCrop(SclDevId, SclChnId, &stSclChnCropInfo));
    }

    MI_SCL_ChnParam_t stSclChnParam;
    memset(&stSclChnParam, 0, sizeof(MI_SCL_ChnParam_t));

    stSclChnParam.eRot = E_MI_SYS_ROTATE_NONE;

    STCHECKRESULT(MI_SCL_SetChnParam(SclDevId, SclChnId, &stSclChnParam));

    STCHECKRESULT(MI_SCL_StartChannel(SclDevId, SclChnId));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SclPortStop(ST_Common_SclAttr_t *pSclAttr)
{
    if (NULL == pSclAttr)
    {
        __ERR("pSclAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_SCL_DisableOutputPort(pSclAttr->SclDevId, pSclAttr->SclChnId, pSclAttr->SclOutPortId));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SclChnStop(ST_Common_SclAttr_t *pSclAttr)
{
    if (NULL == pSclAttr)
    {
        __ERR("pSclAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_SCL_StopChannel(pSclAttr->SclDevId, pSclAttr->SclChnId));

    STCHECKRESULT(MI_SCL_DestroyChannel(pSclAttr->SclDevId, pSclAttr->SclChnId));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SclCropSet(ST_Common_SclAttr_t *pSclAttr, MI_SYS_WindowRect_t *pstOutCropInfo)
{
    MI_S32 s32Ret = MI_SUCCESS;
    MI_SCL_OutPortParam_t stSclOutputParam = {0};
    MI_SCL_GetOutputPortParam(pSclAttr->SclDevId, pSclAttr->SclChnId, pSclAttr->SclOutPortId, &stSclOutputParam);

    memcpy(&stSclOutputParam.stSCLOutCropRect, pstOutCropInfo, sizeof(MI_SYS_WindowRect_t));

    STCHECKRESULT(MI_SCL_SetOutputPortParam(pSclAttr->SclDevId, pSclAttr->SclChnId, pSclAttr->SclOutPortId, &stSclOutputParam));

    if (MI_SUCCESS != s32Ret)
    {
        __ERR("%u:%u:%u MI_SCL_SetOutputPortParam [%u %u %u %u] failed %#x\n",
              pSclAttr->SclDevId, pSclAttr->SclChnId, pSclAttr->SclOutPortId,
              pstOutCropInfo->u16X, pstOutCropInfo->u16Y, pstOutCropInfo->u16Width, pstOutCropInfo->u16Height, s32Ret);
    }
    else
    {
        // __ERR("%u:%u:%u MI_SCL_SetOutputPortParam [%u %u %u %u] OK\n",
        //       pSclAttr->SclDevId, pSclAttr->SclChnId, pSclAttr->SclOutPortId,
        //       pstOutCropInfo->u16X, pstOutCropInfo->u16Y, pstOutCropInfo->u16Width, pstOutCropInfo->u16Height);
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_SclPause(ST_Common_SclAttr_t *pSclAttr)
{
    if (NULL == pSclAttr)
    {
        __ERR("pSclAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_SCL_StopChannel(pSclAttr->SclDevId, pSclAttr->SclChnId));
    return MI_SUCCESS;
}

MI_S32 ST_Common_SclRecover(ST_Common_SclAttr_t *pSclAttr)
{
    if (NULL == pSclAttr)
    {
        __ERR("pSclAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_SCL_StartChannel(pSclAttr->SclDevId, pSclAttr->SclChnId));
    return MI_SUCCESS;
}
