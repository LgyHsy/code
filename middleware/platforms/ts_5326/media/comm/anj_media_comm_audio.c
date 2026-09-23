#include "anj_mw_media_common.h"

#include <sys/select.h>
#include "ts_alg_vqe_sol_webrtc3a.h"

static TS_S32 TS_Common_AudioAiVqeOpen(TS_VOID **ppHandle, TS_VOID *pAttr)
{
    TS_S32 s32Err = TS_ALG_VQESolWebRTC3A_Init(ppHandle, (ALG_VQE_SOL_WEBRTC3A_CONF_PARAM_S *)pAttr);
    if ((!*ppHandle) || (s32Err != 0))
    {
        __ERR("TS_ALG_VQESolWebRTC3A_Init failed: err=%d\n", (int)s32Err);
        return TS_FAILURE;
    }
    return TS_SUCCESS;
}

static TS_S32 TS_Common_AudioAiVqeSetProcAddr(TS_VOID *pHandle, TS_VOID *pAddr)
{
    (void)pHandle;
    (void)pAddr;
    return TS_SUCCESS;
}

static TS_S32 TS_Common_AudioAiVqeProcess(TS_VOID *pHandle, TS_U8 *pu8InBuf, TS_U8 *pu8RefBuf, TS_U32 *pu32InLen,
                                          TS_U8 *pu8OutBuf, TS_U32 *pu32OutLen)
{
    TS_S32 s32Ret;
    TS_U32 u32InLen = *pu32InLen / sizeof(TS_U16);
    TS_U32 u32OutLen = 0;

    s32Ret = TS_ALG_VQESolWebRTC3A_Process(pHandle, (TS_S16 *)pu8InBuf, (TS_S16 *)pu8RefBuf, &u32InLen,
                                           (TS_S16 *)pu8OutBuf, &u32OutLen);
    if (s32Ret != 0)
    {
        __ERR("TS_ALG_VQESolWebRTC3A_Process failed: ret=%d\n", (int)s32Ret);
        return TS_FAILURE;
    }

    *pu32OutLen = u32OutLen * sizeof(TS_U16);
    return TS_SUCCESS;
}

static TS_S32 TS_Common_AudioAiVqeClose(TS_VOID *pHandle)
{
    TS_S32 s32Ret = TS_ALG_VQESolWebRTC3A_Exit(pHandle);
    if (s32Ret != 0)
    {
        __ERR("TS_ALG_VQESolWebRTC3A_Exit failed: ret=%d\n", (int)s32Ret);
        return TS_FAILURE;
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_AudioAi_SetVolume(AUDIO_DEV AiDevId, AI_CHN AiChnId, TS_S32 s32VolumeDb)
{
    TS_S32 s32Ret = TS_MPI_AI_SetAnaVolume(AiDevId, AiChnId, s32VolumeDb);
    if (TS_SUCCESS != s32Ret)
        __ERR("TS_MPI_AI_SetAnaVolume failed: dev=%d chn=%d vol=%d ret=0x%x\n", AiDevId, AiChnId, s32VolumeDb, (unsigned int)s32Ret);
    return s32Ret;
}

TS_S32 TS_Common_AudioAo_SetVolume(AUDIO_DEV AoDevId, TS_S32 s32VolumeDb)
{
    TS_S32 s32Ret = TS_MPI_AO_SetVolume(AoDevId, s32VolumeDb);
    if (TS_SUCCESS != s32Ret)
        __ERR("TS_MPI_AO_SetVolume failed: dev=%d vol=%d ret=0x%x\n", AoDevId, s32VolumeDb, (unsigned int)s32Ret);
    return s32Ret;
}

TS_S32 TS_Common_AudioAo_SetMute(AUDIO_DEV AoDevId, int enable)
{
    AUDIO_FADE_S stFade;
    memset(&stFade, 0, sizeof(stFade));
    TS_S32 s32Ret = TS_MPI_AO_SetMute(AoDevId, enable ? TS_TRUE : TS_FALSE, &stFade);
    if (TS_SUCCESS != s32Ret)
        __ERR("TS_MPI_AO_SetMute failed: dev=%d mute=%d ret=0x%x\n", AoDevId, enable, (unsigned int)s32Ret);
    return s32Ret;
}

TS_S32 TS_Common_AudioAi_GetStream(AUDIO_DEV AiDevId, AI_CHN AiChnId, anj_mw_media_audio_pcm_data pcm_data_cb,
                                   int *bStart)
{
    TS_S32 s32Ret;
    TS_S32 s32AiFd = -1;
    fd_set stReadFds;
    struct timeval stTimeoutVal;
    AUDIO_FRAME_S stFrame;

    if (AiDevId < 0 || bStart == NULL)
    {
        __ERR("TS_Common_AudioAi_GetStream invalid param: dev=%d bStart=%p\n", AiDevId, bStart);
        return TS_FAILURE;
    }

    if (pcm_data_cb == NULL)
    {
        __ERR("TS_Common_AudioAi_GetStream pcm_data_cb is NULL\n");
        return TS_FAILURE;
    }

    s32AiFd = TS_MPI_AI_GetFd(AiDevId, AiChnId);
    if (s32AiFd < 0)
    {
        __ERR("TS_MPI_AI_GetFd failed: dev=%d chn=%d fd=%d\n", AiDevId, AiChnId, s32AiFd);
        return TS_FAILURE;
    }

    while (*bStart)
    {
        FD_ZERO(&stReadFds);
        FD_SET(s32AiFd, &stReadFds);

        stTimeoutVal.tv_sec = 1;
        stTimeoutVal.tv_usec = 0;

        s32Ret = TS_MPI_AI_Select(s32AiFd + 1, &stReadFds, NULL, NULL, &stTimeoutVal);
        if (s32Ret <= 0)
            continue;

        if (!FD_ISSET(s32AiFd, &stReadFds))
            continue;

        memset(&stFrame, 0, sizeof(stFrame));
        s32Ret = TS_MPI_AI_GetFrame(AiDevId, AiChnId, &stFrame, NULL, TS_FALSE);
        if (TS_SUCCESS != s32Ret)
            continue;

        if (stFrame.u32Len > 0 && stFrame.pVirAddr[0])
        {
            pcm_data_cb((char *)stFrame.pVirAddr[0], (int)stFrame.u32Len, stFrame.u64TimeStamp, stFrame.u32Seq);
        }

        s32Ret = TS_MPI_AI_ReleaseFrame(AiDevId, AiChnId, &stFrame, NULL);
        if (TS_SUCCESS != s32Ret)
        {
            __ERR("TS_MPI_AI_ReleaseFrame failed: 0x%x\n", (unsigned int)s32Ret);
        }
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_AudioAo_PcmPlay(char *data, int len)
{
    if (data == NULL || len <= 0)
    {
        __ERR("TS_Common_AudioAo_PcmPlay invalid param: data=%p len=%d\n", data, len);
        return TS_FAILURE;
    }

    AUDIO_FRAME_S stFrame;
    memset(&stFrame, 0, sizeof(stFrame));
    stFrame.pVirAddr[0] = data;
    stFrame.u32Len = (TS_U32)len;

    TS_S32 s32Ret = TS_MPI_AO_SendFrame(0, 0, &stFrame, 1000);
    if (TS_SUCCESS != s32Ret)
        __ERR("TS_MPI_AO_SendFrame failed: len=%d ret=0x%x\n", len, (unsigned int)s32Ret);
    return s32Ret;
}

TS_S32 TS_Common_AudioAo_PlayEndingCheck(void)
{
    AO_CHN_STATE_S stStat;
    memset(&stStat, 0, sizeof(stStat));

    TS_S32 s32Ret = TS_MPI_AO_QueryChnStat(0, 0, &stStat);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AO_QueryChnStat failed: ret=0x%x\n", (unsigned int)s32Ret);
        return TS_FAILURE;
    }

    return (TS_S32)stStat.u32ChnBusyNum;
}

TS_S32 TS_Common_AudioAi_SetTalkVqeAttr(AUDIO_DEV AiDevId, AI_CHN AiChnId, AIO_ATTR_S *pstAioAttr,
                                        AUDIO_VQE_CONFIG_S *pstAiVqeAttr)
{
    TS_S32 s32Ret = TS_SUCCESS;

    if (pstAioAttr == NULL || pstAiVqeAttr == NULL)
    {
        __ERR("TS_Common_AudioAi_SetTalkVqeAttr invalid param: aio=%p vqe=%p\n", pstAioAttr, pstAiVqeAttr);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_AI_SetTalkVqeAttr(AiDevId, AiChnId, pstAiVqeAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_AI_SetTalkVqeAttr failed: dev=%d chn=%d ret=0x%x\n", AiDevId, AiChnId, (unsigned int)s32Ret);
        return s32Ret;
    }

    AUDIO_VQE_S aivqe;
    memset(&aivqe, 0, sizeof(aivqe));
    snprintf(aivqe.aszVqeName, sizeof(aivqe.aszVqeName), "%s", "sol_vqe_nn");
    if (AUDIO_SOUND_MODE_MONO == pstAioAttr->enSoundmode)
    {
        aivqe.pfnVqeOpen = TS_Common_AudioAiVqeOpen;
        aivqe.pfnVqeSetProcAddr = TS_Common_AudioAiVqeSetProcAddr;
        aivqe.pfnVqeProcess = TS_Common_AudioAiVqeProcess;
        aivqe.pfnVqeClose = TS_Common_AudioAiVqeClose;
    }

    s32Ret = TS_MPI_AI_RegisteredVqe(AiDevId, AiChnId, &aivqe);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_AI_RegisteredVqe failed: dev=%d chn=%d ret=0x%x\n", AiDevId, AiChnId, (unsigned int)s32Ret);
        return s32Ret;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_AudioAoInit(TS_Common_AudioAttr_t *pstAudioAttr)
{
    if (pstAudioAttr == NULL)
    {
        __ERR("TS_Common_AudioAoInit pstAudioAttr is NULL\n");
        return TS_FAILURE;
    }

    AIO_ATTR_S stAioAttr = {0};
    stAioAttr.u8CardNum = 0;
    stAioAttr.u8DeviceNum = 0;
    stAioAttr.enSamplerate = pstAudioAttr->enSampleRate;
    stAioAttr.enBitwidth = pstAudioAttr->enBitwidth;
    stAioAttr.enSoundmode = pstAudioAttr->enSoundMode;
    stAioAttr.u32FrmNum = 4;
    stAioAttr.u32PtNumPerFrm = pstAudioAttr->u32PeriodSize;
    /* 参考 sample_audio，u32ChnCnt 保持默认 0 由驱动内部决定，避免 hw params 失败 */
    stAioAttr.u32ChnCnt = 0;
    stAioAttr.enAioMode = pstAudioAttr->enChannelMode;
    stAioAttr.bAecEnable = TS_FALSE;

    TS_S32 s32Ret = TS_MPI_AO_SetPubAttr(0, &stAioAttr);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AO_SetPubAttr failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    s32Ret = TS_MPI_AO_Enable(0);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AO_Enable failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    s32Ret = TS_MPI_AO_EnableChn(0, 0);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AO_EnableChn failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_AudioAoUnInit()
{
    TS_S32 s32Ret;
    s32Ret = TS_MPI_AO_DisableChn(0, 0);
    if (TS_SUCCESS != s32Ret)
        __ERR("TS_MPI_AO_DisableChn failed: 0x%x\n", s32Ret);

    s32Ret = TS_MPI_AO_Disable(0);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AO_Disable failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_AudioAiInit(TS_Common_AudioAttr_t *pstAudioAttr)
{
    if (pstAudioAttr == NULL)
    {
        __ERR("TS_Common_AudioAiInit pstAudioAttr is NULL\n");
        return TS_FAILURE;
    }

    AIO_ATTR_S stAioAttr = {0};
    stAioAttr.u8CardNum = 0;
    stAioAttr.u8DeviceNum = 0;
    stAioAttr.enSamplerate = pstAudioAttr->enSampleRate;
    stAioAttr.enBitwidth = pstAudioAttr->enBitwidth;
    stAioAttr.enSoundmode = pstAudioAttr->enSoundMode;
    stAioAttr.u32FrmNum = 4;
    stAioAttr.u32PtNumPerFrm = pstAudioAttr->u32PeriodSize;
    /* 参考 sample_audio，u32ChnCnt 保持默认 0 由驱动内部决定，避免 hw params 失败 */
    stAioAttr.u32ChnCnt = 0;
    stAioAttr.enAioMode = pstAudioAttr->enChannelMode;
    stAioAttr.bAecEnable = pstAudioAttr->bAecEnable;

    TS_S32 s32Ret = TS_MPI_AI_SetPubAttr(0, &stAioAttr);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AI_SetPubAttr failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    if (pstAudioAttr->bAiVqeEnable)
    {
        TS_U32 u32OpenMask = 0;
        TS_S32 s32VqeFrmLen = 160;
        ALG_VQE_SOL_WEBRTC3A_CONF_PARAM_S stAiSolConf = {0};
        AUDIO_VQE_CONFIG_S stVqeConfig = {0};
        if (pstAudioAttr->bAnrEnable)
        {
            u32OpenMask |= TS_ALG_VQE_SOL_WEBRTC_ANR_MASK;
            u32OpenMask |= TS_ALG_VQE_MMSENS_MASK;
            u32OpenMask |= TS_ALG_VQE_NOISEGATE_MASK;
        }
        if (pstAudioAttr->bAgcEnable)
            u32OpenMask |= TS_ALG_VQE_SOL_WEBRTC_AGC_MASK;
        if (pstAudioAttr->bAecEnable)
            u32OpenMask |= TS_ALG_VQE_SOL_WEBRTC_AEC_MASK;

        if (pstAudioAttr->enSampleRate == AUDIO_SAMPLE_RATE_8000)
            s32VqeFrmLen = 80;
        else if (pstAudioAttr->enSampleRate == AUDIO_SAMPLE_RATE_16000)
            s32VqeFrmLen = 160;
        else
            s32VqeFrmLen = 160;

        stAiSolConf.u32OpenMask = u32OpenMask;
        stAiSolConf.u32SmplRate = pstAudioAttr->enSampleRate;
        stAiSolConf.u16FrameLen = (TS_U16)s32VqeFrmLen;
        stAiSolConf.u8ChanNum = (pstAudioAttr->enSoundMode == AUDIO_SOUND_MODE_STEREO) ? 2 : 1;
        stAiSolConf.s16AgcMode = 3;
        stAiSolConf.s32NoiseSuppress = -15;
        stAiSolConf.f32AttackTime = 0.005f;
        stAiSolConf.f32ReleaseTime = 0.005f;
        stAiSolConf.s32Threshold = -65;
        stAiSolConf.stSolVqeProc.s16MsInSndCardBuf = 0;
        stAiSolConf.stSolVqeProc.s32Skew = 0;
        stAiSolConf.stSolVqeSetParam.stAlgAecParams.s16NlpMode = 2;
        stAiSolConf.stSolVqeSetParam.stAlgAecParams.s16SkewMode = 0;
        stAiSolConf.stSolVqeSetParam.stAlgAecParams.s16MetricsMode = 0;
        stAiSolConf.stSolVqeSetParam.stAlgAecParams.s32DelayLogging = 0;
        stAiSolConf.stSolVqeSetParam.stAlgAnrParams.s32Mode = 2;
        stAiSolConf.stSolVqeSetParam.stAlgAgcParams.s16TargetLevelDbfs = 3;
        stAiSolConf.stSolVqeSetParam.stAlgAgcParams.s16CompressionGaindB = 20;
        stAiSolConf.stSolVqeSetParam.stAlgAgcParams.u8LimiterEnable = 1;

        stVqeConfig.s32InLen = s32VqeFrmLen * 2;
        stVqeConfig.s32InCnt = s32VqeFrmLen;
        stVqeConfig.s32OutLen = s32VqeFrmLen * 2;
        stVqeConfig.s32OutCnt = s32VqeFrmLen;
        stVqeConfig.pVqeConfig = &stAiSolConf;

        __INFO("AI VQE enable mask=0x%x sample=%d frame=%d\n",
               (unsigned int)u32OpenMask, (int)pstAudioAttr->enSampleRate, (int)s32VqeFrmLen);

        s32Ret = TS_Common_AudioAi_SetTalkVqeAttr(0, 0, &stAioAttr, &stVqeConfig);
        if (TS_SUCCESS != s32Ret)
            return s32Ret;
        if (pstAudioAttr->bAecEnable)
        {
            s32Ret = TS_MPI_AI_EnableAecRefFrame(0, 0, 0, 0);
            if (TS_SUCCESS != s32Ret)
                __ERR("TS_MPI_AI_EnableAecRefFrame failed: ret=0x%x\n", (unsigned int)s32Ret);
        }
    }

    s32Ret = TS_MPI_AI_Enable(0);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AI_Enable failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    s32Ret = TS_MPI_AI_EnableChn(0, 0);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AI_EnableChn failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    if (pstAudioAttr->bAiVqeEnable)
    {
        s32Ret = TS_MPI_AI_EnableVqe(0, 0);
        if (TS_SUCCESS != s32Ret)
        {
            __ERR("TS_MPI_AI_EnableVqe failed: ret=0x%x\n", (unsigned int)s32Ret);
            return s32Ret;
        }
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_AudioAiUnInit(void)
{
    TS_S32 s32Ret;
    s32Ret = TS_MPI_AI_DisableVqe(0, 0);
    if (TS_SUCCESS != s32Ret)
        __INFO("TS_MPI_AI_DisableVqe ret=0x%x\n", (unsigned int)s32Ret);

    s32Ret = TS_MPI_AI_UnRegisteredVqe(0, 0);
    if (TS_SUCCESS != s32Ret)
        __INFO("TS_MPI_AI_UnRegisteredVqe ret=0x%x\n", (unsigned int)s32Ret);

    s32Ret = TS_MPI_AI_DisableChn(0, 0);
    if (TS_SUCCESS != s32Ret)
        __ERR("TS_MPI_AI_DisableChn failed: 0x%x\n", s32Ret);

    s32Ret = TS_MPI_AI_Disable(0);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_AI_Disable failed: ret=0x%x\n", (unsigned int)s32Ret);
        return s32Ret;
    }

    return TS_SUCCESS;
}
