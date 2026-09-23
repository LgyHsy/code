
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>

#include "anj_mw_media_common.h"

TS_S32 TS_COMMON_VI_SetParam(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 i;
    TS_S32 s32Ret;
    VI_PIPE ViPipe;
    VI_VPSS_MODE_E aeMode[VI_MAX_PIPE_NUM];
    VI_VPSS_MODE_S stVIVPSSMode = {0, aeMode};

    if (!pstViConfig)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    if (pstViConfig->s32WorkingViNum > VI_MAX_PIPE_NUM)
    {
        __ERR("WorkingViNum[%d] is invalid, maxValid=%d\n", pstViConfig->s32WorkingViNum, VI_MAX_PIPE_NUM);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_SYS_GetVIVPSSMode(&stVIVPSSMode);

    if (TS_SUCCESS != s32Ret)
    {
        __ERR("Get VI-VPSS mode Param failed with %#x!\n", s32Ret);

        return TS_FAILURE;
    }

    for (i = 0; i < pstViConfig->s32WorkingViNum; i++)
    {
        ViPipe = pstViConfig->astViInfo[i].stPipeInfo.aPipe;
        if (ViPipe >= VI_MAX_PIPE_NUM)
        {
            __ERR("Vi[%d] : pipe[%d] is invalid, maxValid=%d\n", i, ViPipe, VI_MAX_PIPE_NUM);
            return TS_FAILURE;
        }
        stVIVPSSMode.aenMode[ViPipe] = pstViConfig->astViInfo[i].stPipeInfo.enMastPipeMode;
    }
    stVIVPSSMode.s32ModeCount = pstViConfig->s32WorkingViNum;

    s32Ret = TS_MPI_SYS_SetVIVPSSMode(&stVIVPSSMode);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("Set VI-VPSS mode Param failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    TS_MPI_SYS_SetCamNum(pstViConfig->s32WorkingViNum);

    return TS_SUCCESS;
}

static TS_S32 TS_COMMON_VI_StopSingleViPipe(VI_PIPE ViPipe)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_VI_StopPipe(ViPipe);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_StopPipe failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_VI_DestroyPipe(ViPipe);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_DestroyPipe failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    return s32Ret;
}

TS_S32 TS_COMMON_VI_StartViPipe(TS_Common_ViInfo_t *pstViInfo)
{
    TS_S32 s32Ret = TS_SUCCESS;
    VI_PIPE ViPipe;
    VI_PIPE_ATTR_S stPipeAttr = {0};

    ViPipe = pstViInfo->stPipeInfo.aPipe;

    stPipeAttr.u32MaxW = pstViInfo->stPipeInfo.width;
    stPipeAttr.u32MaxH = pstViInfo->stPipeInfo.height;
    stPipeAttr.enPixFmt = pstViInfo->stPipeInfo.enPixFmt;
    stPipeAttr.enBitWidth = pstViInfo->stPipeInfo.enBitWid;
    stPipeAttr.enPipeBypassMode = VI_PIPE_BYPASS_NONE;
    stPipeAttr.stFrameRate.s32SrcFrameRate = pstViInfo->stPipeInfo.frameRate;
    stPipeAttr.stFrameRate.s32DstFrameRate = pstViInfo->stPipeInfo.frameRate;
    /*stPipeAttr.bIspBypass = TS_FALSE;*/

    __INFO("vipipe[%d]: wh=[%d,%d], ispBypass=%d, yuvSkip=%d, bitWid=%d, pix=%d, frm-[%d,%d]\n", ViPipe,
          stPipeAttr.u32MaxW, stPipeAttr.u32MaxH, stPipeAttr.bIspBypass, stPipeAttr.bYuvSkip,
          stPipeAttr.enBitWidth, stPipeAttr.enPixFmt, stPipeAttr.stFrameRate.s32SrcFrameRate,
          stPipeAttr.stFrameRate.s32DstFrameRate);

    s32Ret = TS_MPI_VI_CreatePipe(ViPipe, &stPipeAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_CreatePipe[%d] failed with %#x!\n", ViPipe, s32Ret);
        return TS_FAILURE;
    }
    __INFO("TS_MPI_VI_CreatePipe[%d] success!\n", ViPipe);

    s32Ret = TS_MPI_VI_StartPipe(ViPipe);
    if (s32Ret != TS_SUCCESS)
    {
        TS_MPI_VI_DestroyPipe(ViPipe);
        __ERR("TS_MPI_VI_StartPipe[%d] failed with %#x!\n", ViPipe, s32Ret);
        goto EXIT;
    }
    __INFO("TS_MPI_VI_StartPipe[%d] success!\n", ViPipe);

    /* StartTuningToolServer */
    s32Ret = TS_Common_IspIqStart();
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_IspIqStart failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    return s32Ret;

EXIT:
    TS_COMMON_VI_StopSingleViPipe(ViPipe);

    return s32Ret;
}

TS_S32 TS_COMMON_VI_StopViPipe(TS_Common_ViInfo_t *pstViInfo)
{
    if (pstViInfo->stPipeInfo.aPipe >= 0 && pstViInfo->stPipeInfo.aPipe < VI_MAX_PIPE_NUM)
    {
        VI_PIPE ViPipe = pstViInfo->stPipeInfo.aPipe;
        TS_COMMON_VI_StopSingleViPipe(ViPipe);
    }

    return TS_SUCCESS;
}
TS_S32 TS_COMMON_VI_StartViChn(TS_Common_ViInfo_t *pstViInfo)
{
    TS_S32 s32Ret = TS_SUCCESS;
    return s32Ret;
}

TS_S32 TS_COMMON_VI_StopViChn(TS_Common_ViInfo_t *pstViInfo)
{
    TS_S32 s32Ret = TS_SUCCESS;
    return s32Ret;
}

static TS_S32 TS_COMMON_VI_CreateSingleVi(TS_Common_ViInfo_t *pstViInfo)
{
    TS_S32 s32Ret = TS_SUCCESS;

    s32Ret = TS_COMMON_VI_StartViPipe(pstViInfo);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_StartViPipe failed !\n");
        goto EXIT2;
    }
    __INFO("TS_COMMON_VI_StartViPipe success !\n");

    if (VI_OFFLINE_VPSS_OFFLINE == pstViInfo->stPipeInfo.enMastPipeMode ||
        VI_ONLINE_VPSS_OFFLINE == pstViInfo->stPipeInfo.enMastPipeMode)
    {
        s32Ret = TS_COMMON_VI_StartViChn(pstViInfo);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_COMMON_VI_StartViChn failed !\n");
            goto EXIT2;
        }
        __INFO("TS_COMMON_VI_StartViChn success !\n");
    }

    return TS_SUCCESS;

EXIT2:
    TS_COMMON_VI_StopViPipe(pstViInfo);

    return s32Ret;
}

static TS_S32 TS_COMMON_VI_DestroySingleVi(TS_Common_ViInfo_t *pstViInfo)
{
    TS_S32 s32Ret = TS_SUCCESS;

    s32Ret = TS_COMMON_VI_StopViChn(pstViInfo);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_StopViChn failed !\n");
        return TS_FAILURE;
    }

    s32Ret = TS_COMMON_VI_StopViPipe(pstViInfo);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_StopViPipe failed !\n");
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_CreateVi(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 i;
    TS_S32 s32Ret = TS_SUCCESS;
    TS_Common_ViInfo_t *pstViInfo = NULL;
    if (!pstViConfig)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    for (i = 0; i < pstViConfig->s32WorkingViNum; i++)
    {
        pstViInfo = &pstViConfig->astViInfo[i];

        if (pstViInfo->stPipeInfo.aPipe >= 0 && pstViInfo->stPipeInfo.aPipe < VI_MAX_PIPE_NUM)
        {
            s32Ret = TS_COMMON_VI_CreateSingleVi(pstViInfo);

            if (s32Ret != TS_SUCCESS)
            {
                __ERR("TS_COMMON_VI_CreateSingleVi failed !\n");
                goto EXIT;
            }
        }
    }

    return TS_SUCCESS;
EXIT:

    for (i = 0; i < pstViConfig->s32WorkingViNum; i++)
    {
        pstViInfo = &pstViConfig->astViInfo[i];

        TS_COMMON_VI_DestroySingleVi(pstViInfo);
    }

    return s32Ret;
}

TS_S32 TS_COMMON_VI_DestroyVi(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 i;
    TS_Common_ViInfo_t *pstViInfo = TS_NULL;

    if (!pstViConfig)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    for (i = 0; i < pstViConfig->s32WorkingViNum; i++)
    {
        pstViInfo = &pstViConfig->astViInfo[i];

        TS_COMMON_VI_DestroySingleVi(pstViInfo);
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_StartIsp(TS_Common_ViInfo_t *pstViInfo)
{
    TS_S32 s32Ret = TS_SUCCESS;
    VI_PIPE ViPipe = pstViInfo->stPipeInfo.aPipe;
    ISP_PUB_ATTR_S stPubAttr;

    if (ViPipe >= 0 && ViPipe < VI_MAX_PIPE_NUM)
    {
        stPubAttr.enWDRMode = pstViInfo->stPipeInfo.enWdrMode;
        stPubAttr.enBayer = pstViInfo->stPipeInfo.enBayer;
        stPubAttr.stWndRect.u32Width = pstViInfo->stPipeInfo.width;
        stPubAttr.stWndRect.u32Height = pstViInfo->stPipeInfo.height;
        stPubAttr.stWndRect.s32X = 0;
        stPubAttr.stWndRect.s32Y = 0;
        stPubAttr.stSnsSize.u32Width = pstViInfo->stPipeInfo.width;
        stPubAttr.stSnsSize.u32Height = pstViInfo->stPipeInfo.height;
        stPubAttr.f32FrameRate = pstViInfo->stPipeInfo.frameRate;
        stPubAttr.bDynFpsSync = pstViInfo->stPipeInfo.bDynFpsSync;
        stPubAttr.stOnlineParam.bOnline = pstViInfo->stPipeInfo.bIspByFly;
        stPubAttr.stOnlineParam.u32VinoutHtotalMargin = 0;
        stPubAttr.stOnlineParam.u32VinoutVtotalMargin = 0;
        stPubAttr.bTnrCompress = 0;

        /* s32Ret = TS_MPI_ISP_MemInit(ViPipe); */

        /* if (s32Ret != TS_SUCCESS) */
        /* { */
        /*     __ERR("Init Ext memory failed with %#x!\n", s32Ret); */
        /*     TS_MPI_ISP_Exit(ViPipe); */
        /*     return TS_FAILURE; */
        /* } */
        /* } */

        s32Ret = TS_MPI_ISP_SetPubAttr(ViPipe, &stPubAttr);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("SetPubAttr failed with %#x!\n", s32Ret);
            TS_MPI_ISP_Exit(ViPipe);
            return TS_FAILURE;
        }

        s32Ret = TS_MPI_ISP_Init(ViPipe);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("ISP Init failed with %#x!\n", s32Ret);
            TS_MPI_ISP_Exit(ViPipe);
            return TS_FAILURE;
        }
    }

    return s32Ret;
}

TS_S32 TS_COMMON_VI_StopIsp(TS_Common_ViInfo_t *pstViInfo)
{
    VI_PIPE ViPipe = pstViInfo->stPipeInfo.aPipe;

    if (ViPipe >= 0 && ViPipe < VI_MAX_PIPE_NUM)
    {
        TS_MPI_ISP_Exit(ViPipe);
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_CreateIsp(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 i;
    TS_S32 s32Ret = TS_SUCCESS;

    TS_Common_ViInfo_t *pstViInfo = TS_NULL;

    if (!pstViConfig)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    __INFO("working ViNum=%d\n", pstViConfig->s32WorkingViNum);
    for (i = 0; i < pstViConfig->s32WorkingViNum; i++)
    {
        pstViInfo = &pstViConfig->astViInfo[i];

        __INFO("is going to start isp of vipipe[%d]\n", pstViInfo->stPipeInfo.aPipe);

        s32Ret = TS_COMMON_VI_StartIsp(pstViInfo);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_COMMON_VI_StartIsp failed !\n");
            return TS_FAILURE;
        }
        __INFO("TS_COMMON_VI_StartIsp of vipipe[%d] success !\n", pstViInfo->stPipeInfo.aPipe);
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_DestroyIsp(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 i;
    TS_S32 s32Ret = TS_SUCCESS;
    TS_Common_ViInfo_t *pstViInfo = TS_NULL;

    if (!pstViConfig)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    for (i = 0; i < pstViConfig->s32WorkingViNum; i++)
    {
        pstViInfo = &pstViConfig->astViInfo[i];

        s32Ret = TS_COMMON_VI_StopIsp(pstViInfo);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_COMMON_VI_StopIsp failed !\n");
            return TS_FAILURE;
        }
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_StartVi(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 s32Ret = TS_SUCCESS;

    if (!pstViConfig)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    s32Ret = TS_COMMON_VI_SetParam(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_SetParam failed!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_COMMON_VI_CreateVi(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_CreateVi failed!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_COMMON_VI_CreateIsp(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        TS_COMMON_VI_DestroyVi(pstViConfig);
        __ERR("TS_COMMON_VI_CreateIsp failed!\n");
        return TS_FAILURE;
    }

    return s32Ret;
}

TS_S32 TS_COMMON_VI_StopVi(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 s32Ret = TS_SUCCESS;

    s32Ret = TS_COMMON_VI_DestroyIsp(pstViConfig);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_DestroyIsp failed !\n");
        return TS_FAILURE;
    }

    __ERR("TS_COMMON_VI_DestroyIsp success!\n");

    s32Ret = TS_COMMON_VI_DestroyVi(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_DestroyVi failed !\n");
        return TS_FAILURE;
    }

    __ERR("TS_COMMON_VI_DestroyVi success!\n");

    return s32Ret;
}

// static TS_S32 TS_COMMON_VI_Aiisp_Set_Sensor_Param(TS_S32 camId, TS_S32 current_model)
// {
//     __INFO("aiisp set sensor param camId:%d current_model:%x\n", camId, current_model);
//     return TS_SUCCESS;
// }

TS_S32 TS_COMMON_VI_Aiisp_SetAutoAttr(TS_U32 u32Width)
{
    TS_S32 s32Ret;
    #if 0
    VI_AIISP_AUTO_ATTR_S aiispAutoAttr = {0};

    /* Align with SAMPLE_COMM_VI_StartVi_And_Aiisp DX5326 auto switch. */
    aiispAutoAttr.enSwitchRef = VI_AIISP_MODEL_SWITCH_BY_GAIN;
    if (u32Width == 2592)
    {
        aiispAutoAttr.u8SwitchDataCnt = 1;
        aiispAutoAttr.stSwitchDataGroup[0].u8ModId = 5 | (1 << 7);
        aiispAutoAttr.stSwitchDataGroup[0].enModeType = VI_AIISP_MODEL_TYPE_AIMVD;
        aiispAutoAttr.stSwitchDataGroup[0].s32MinValue = 1024;
        aiispAutoAttr.stSwitchDataGroup[0].s32MaxValue = 0xFFFFFFF;
    }
    else
    {
        aiispAutoAttr.u8SwitchDataCnt = 3;
        aiispAutoAttr.stSwitchDataGroup[0].u8ModId = 1;
        aiispAutoAttr.stSwitchDataGroup[0].enModeType = VI_AIISP_MODEL_TYPE_RFR;
        aiispAutoAttr.stSwitchDataGroup[0].s32MinValue = 102400;
        aiispAutoAttr.stSwitchDataGroup[0].s32MaxValue = 1250000;
        aiispAutoAttr.stSwitchDataGroup[1].u8ModId = 2;
        aiispAutoAttr.stSwitchDataGroup[1].enModeType = VI_AIISP_MODEL_TYPE_RFR;
        aiispAutoAttr.stSwitchDataGroup[1].s32MinValue = 1250000;
        aiispAutoAttr.stSwitchDataGroup[1].s32MaxValue = 0xFFFFFFF;
        // aiispAutoAttr.stSwitchDataGroup[2].u8ModId = 0; /* bypass */
        // aiispAutoAttr.stSwitchDataGroup[2].enModeType = VI_AIISP_MODEL_TYPE_RFR;
        // aiispAutoAttr.stSwitchDataGroup[2].s32MinValue = 0;
        // aiispAutoAttr.stSwitchDataGroup[2].s32MaxValue = 0; //128000;
        aiispAutoAttr.stSwitchDataGroup[2].u8ModId = 3 | (1 << 7);
        aiispAutoAttr.stSwitchDataGroup[2].enModeType = VI_AIISP_MODEL_TYPE_AIMVD;
        aiispAutoAttr.stSwitchDataGroup[2].s32MinValue = 1000;
        aiispAutoAttr.stSwitchDataGroup[2].s32MaxValue = 102400;
    }
    aiispAutoAttr.stUserFunc.pfn_set_sensor_param = TS_COMMON_VI_Aiisp_Set_Sensor_Param;

    s32Ret = TS_MPI_VI_AIISP_SetAutoAttr(0, &aiispAutoAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_AIISP_SetAutoAttr for %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    #else
    s32Ret = TS_MPI_VI_AIISP_SetManualModel(0, VI_AIISP_MODEL_TYPE_AIMVD, 129);
    if (s32Ret != TS_SUCCESS)
    {
        __WARN("TS_MPI_VI_AIISP_SetManualModel %#x\n", (unsigned int)s32Ret);
        return -1;
    }
    #endif
    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_StartVi_And_Aiisp(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 s32Ret = TS_SUCCESS;
    TS_U32 u32Width;

    if (!pstViConfig)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    pstViConfig->astViInfo[0].stPipeInfo.enBitWid = DATA_BITWIDTH_10;
    s32Ret = TS_COMMON_VI_SetParam(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_SetParam failed!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_COMMON_VI_CreateVi(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_CreateVi failed!\n");
        return TS_FAILURE;
    }
    s32Ret = TS_MPI_VI_AIISP_Init(0);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_AIISP_Init for %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    VI_AIISP_ATTR_S aiispAttr = {0};
    VI_AIISP_MODEL_GROUP_S aiispModelGroup = {0};

    u32Width = pstViConfig->astViInfo[0].stPipeInfo.width;

    aiispAttr.bModelAutoSwitch = 0;
    aiispAttr.stSensorInfo[0].enBitWid = pstViConfig->astViInfo[0].stPipeInfo.enBitWid;
    aiispAttr.stSensorInfo[0].u8PixCut = pstViConfig->astViInfo[0].stPipeInfo.u8PixCut;
    aiispAttr.stSensorInfo[0].u8RowCut = pstViConfig->astViInfo[0].stPipeInfo.u8RowCut;
    aiispAttr.stSensorInfo[0].u32Width = u32Width;
    aiispAttr.stSensorInfo[0].u32Height = pstViConfig->astViInfo[0].stPipeInfo.height;

    s32Ret = TS_MPI_VI_AIISP_SetAttr(0, &aiispAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_AIISP_SetAttr for %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    s32Ret = TS_COMMON_VI_Aiisp_SetAutoAttr(u32Width);
    if (s32Ret != TS_SUCCESS)
    {
        return TS_FAILURE;
    }
    ts_u8 sc450ai_ai_proc_lut[VI_AIISP_MODEL_LUT_MAX] = {
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   11,  15,
        19,  21,  23,  25,  27,  29,  31,  32,  33,  35,  36,  37,  38,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  49,  50,  51,  52,  53,  54,  54,  55,  56,
        57,  57,  58,  59,  59,  60,  61,  61,  62,  63,  63,  64,  65,  65,  66,  67,  67,  68,  68,  69,  69,  70,  71,  71,  72,  72,  73,  73,  74,  74,  75,  75,
        76,  76,  77,  78,  78,  79,  79,  79,  80,  80,  81,  81,  82,  82,  83,  83,  84,  84,  85,  85,  86,  86,  86,  87,  87,  88,  88,  89,  89,  89,  90,  90,
        91,  91,  92,  92,  92,  93,  93,  94,  94,  94,  95,  95,  96,  96,  96,  97,  97,  98,  98,  98,  99,  99,  99,  100, 100, 101, 101, 101, 102, 102, 102, 103,
        103, 103, 104, 104, 104, 105, 105, 106, 106, 106, 107, 107, 107, 108, 108, 108, 109, 109, 109, 110, 110, 110, 111, 111, 111, 112, 112, 112, 113, 113, 113, 113,
        114, 114, 114, 115, 115, 115, 116, 116, 116, 117, 117, 117, 118, 118, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 123, 123, 123,
        123, 124, 124, 124, 125, 125, 125, 125, 126, 126, 126, 127, 127, 127, 127, 128, 128, 128, 129, 129, 129, 129, 130, 130, 130, 130, 131, 131, 131, 132, 132, 132,
        132, 133, 133, 133, 133, 134, 134, 134, 134, 135, 135, 135, 135, 136, 136, 136, 136, 137, 137, 137, 137, 138, 138, 138, 138, 139, 139, 139, 139, 140, 140, 140,
        140, 141, 141, 141, 141, 142, 142, 142, 142, 143, 143, 143, 143, 144, 144, 144, 144, 145, 145, 145, 145, 146, 146, 146, 146, 146, 147, 147, 147, 147, 148, 148,
        148, 148, 149, 149, 149, 149, 149, 150, 150, 150, 150, 151, 151, 151, 151, 152, 152, 152, 152, 152, 153, 153, 153, 153, 154, 154, 154, 154, 154, 155, 155, 155,
        155, 156, 156, 156, 156, 156, 157, 157, 157, 157, 157, 158, 158, 158, 158, 159, 159, 159, 159, 159, 160, 160, 160, 160, 160, 161, 161, 161, 161, 162, 162, 162,
        162, 162, 163, 163, 163, 163, 163, 164, 164, 164, 164, 164, 165, 165, 165, 165, 165, 166, 166, 166, 166, 166, 167, 167, 167, 167, 167, 168, 168, 168, 168, 168,
        169, 169, 169, 169, 169, 170, 170, 170, 170, 170, 171, 171, 171, 171, 171, 172, 172, 172, 172, 172, 173, 173, 173, 173, 173, 173, 174, 174, 174, 174, 174, 175,
        175, 175, 175, 175, 176, 176, 176, 176, 176, 177, 177, 177, 177, 177, 177, 178, 178, 178, 178, 178, 179, 179, 179, 179, 179, 179, 180, 180, 180, 180, 180, 181,
        181, 181, 181, 181, 181, 182, 182, 182, 182, 182, 183, 183, 183, 183, 183, 183, 184, 184, 184, 184, 184, 185, 185, 185, 185, 185, 185, 186, 186, 186, 186, 186,
        186, 187, 187, 187, 187, 187, 188, 188, 188, 188, 188, 188, 189, 189, 189, 189, 189, 189, 190, 190, 190, 190, 190, 190, 191, 191, 191, 191, 191, 191, 192, 192,
        192, 192, 192, 193, 193, 193, 193, 193, 193, 194, 194, 194, 194, 194, 194, 195, 195, 195, 195, 195, 195, 196, 196, 196, 196, 196, 196, 197, 197, 197, 197, 197,
        197, 197, 198, 198, 198, 198, 198, 198, 199, 199, 199, 199, 199, 199, 200, 200, 200, 200, 200, 200, 201, 201, 201, 201, 201, 201, 202, 202, 202, 202, 202, 202,
        202, 203, 203, 203, 203, 203, 203, 204, 204, 204, 204, 204, 204, 205, 205, 205, 205, 205, 205, 205, 206, 206, 206, 206, 206, 206, 207, 207, 207, 207, 207, 207,
        208, 208, 208, 208, 208, 208, 208, 209, 209, 209, 209, 209, 209, 210, 210, 210, 210, 210, 210, 210, 211, 211, 211, 211, 211, 211, 211, 212, 212, 212, 212, 212,
        212, 213, 213, 213, 213, 213, 213, 213, 214, 214, 214, 214, 214, 214, 214, 215, 215, 215, 215, 215, 215, 216, 216, 216, 216, 216, 216, 216, 217, 217, 217, 217,
        217, 217, 217, 218, 218, 218, 218, 218, 218, 218, 219, 219, 219, 219, 219, 219, 219, 220, 220, 220, 220, 220, 220, 220, 221, 221, 221, 221, 221, 221, 221, 222,
        222, 222, 222, 222, 222, 222, 223, 223, 223, 223, 223, 223, 223, 224, 224, 224, 224, 224, 224, 224, 225, 225, 225, 225, 225, 225, 225, 226, 226, 226, 226, 226,
        226, 226, 227, 227, 227, 227, 227, 227, 227, 227, 228, 228, 228, 228, 228, 228, 228, 229, 229, 229, 229, 229, 229, 229, 230, 230, 230, 230, 230, 230, 230, 231,
        231, 231, 231, 231, 231, 231, 231, 232, 232, 232, 232, 232, 232, 232, 233, 233, 233, 233, 233, 233, 233, 233, 234, 234, 234, 234, 234, 234, 234, 235, 235, 235,
        235, 235, 235, 235, 235, 236, 236, 236, 236, 236, 236, 236, 237, 237, 237, 237, 237, 237, 237, 237, 238, 238, 238, 238, 238, 238, 238, 239, 239, 239, 239, 239,
        239, 239, 239, 240, 240, 240, 240, 240, 240, 240, 240, 241, 241, 241, 241, 241, 241, 241, 241, 242, 242, 242, 242, 242, 242, 242, 243, 243, 243, 243, 243, 243,
        243, 243, 244, 244, 244, 244, 244, 244, 244, 244, 245, 245, 245, 245, 245, 245, 245, 245, 246, 246, 246, 246, 246, 246, 246, 246, 247, 247, 247, 247, 247, 247,
        247, 247, 248, 248, 248, 248, 248, 248, 248, 248, 249, 249, 249, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250, 250, 250, 251, 251, 251, 251, 251, 251,
        251, 251, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255,
    };

    /* Align with SAMPLE_COMM_VI_StartVi_And_Aiisp DX5326 LoadModel. */
    aiispModelGroup.stAiispModeInfo[0].enModeType = VI_AIISP_MODEL_TYPE_AIMVD;
    if (u32Width == 2592)
    {
        aiispModelGroup.u8ModelNum = 1;
        aiispModelGroup.stAiispModeInfo[0].u8ModId = 5 | (1 << 7);
        aiispModelGroup.stAiispModeInfo[0].enCamSession = VI_AIISP_MODEL_CAM_SESSION_WHITELIGHT;
        memcpy((ts_u8 *)aiispModelGroup.stAiispModeInfo[0].au8AiLut, sc450ai_ai_proc_lut, sizeof(sc450ai_ai_proc_lut));
        strcpy(aiispModelGroup.stAiispModeInfo[0].aModePath, "/config/model");
        __INFO("use model: %s\n", aiispModelGroup.stAiispModeInfo[0].aModePath);
    }
    else
    {
        aiispModelGroup.u8ModelNum = 2;
        aiispModelGroup.stAiispModeInfo[0].enModeType = VI_AIISP_MODEL_TYPE_AIMVD;
        aiispModelGroup.stAiispModeInfo[0].u8ModId = 1 | (1 << 7);
        aiispModelGroup.stAiispModeInfo[0].enCamSession = 0;
        memcpy((ts_u8 *)aiispModelGroup.stAiispModeInfo[0].au8AiLut, sc450ai_ai_proc_lut, sizeof(sc450ai_ai_proc_lut));
        strcpy(aiispModelGroup.stAiispModeInfo[0].aModePath, "/config/model3");
    
        aiispModelGroup.stAiispModeInfo[1].enModeType = VI_AIISP_MODEL_TYPE_RFR;
        aiispModelGroup.stAiispModeInfo[1].u8ModId = 2;
        aiispModelGroup.stAiispModeInfo[1].enCamSession = 2;
        memcpy((ts_u8 *)aiispModelGroup.stAiispModeInfo[1].au8AiLut, sc450ai_ai_proc_lut, sizeof(sc450ai_ai_proc_lut));
        strcpy(aiispModelGroup.stAiispModeInfo[1].aModePath, "/config/model1");
    }

    s32Ret = TS_MPI_VI_AIISP_LoadModel(0, &aiispModelGroup);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_AIISP_LoadModel for %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    s32Ret = TS_MPI_VI_AIISP_Enable(0);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_AIISP_Enable for %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    s32Ret = TS_COMMON_VI_CreateIsp(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        TS_COMMON_VI_DestroyVi(pstViConfig);
        __ERR("TS_COMMON_VI_CreateIsp failed!\n");
        return TS_FAILURE;
    }
    /* StartTuningToolServer */
    s32Ret = TS_Common_IspIqStart();
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_IspIqStart failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    return s32Ret;
}

TS_S32 TS_COMMON_VI_StopVi_And_Aiisp(TS_Common_ViAttr_t *pstViConfig)
{
    TS_S32 s32Ret = TS_SUCCESS;

    s32Ret = TS_COMMON_VI_DestroyIsp(pstViConfig);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_DestroyIsp failed !\n");
        return TS_FAILURE;
    }

    s32Ret = TS_COMMON_VI_DestroyVi(pstViConfig);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_COMMON_VI_DestroyVi failed !\n");
        return TS_FAILURE;
    }
    s32Ret = TS_MPI_VI_AIISP_Exit(0);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VI_AIISP_Exit for %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    return s32Ret;
}
