#include <pthread.h>
#include "anj_mw_media_common.h"

#define ST_AUDIO_AI_DPGA_GAIN (10) //[-60,30]
#define ST_AUDIO_AI_ECHO_GAIN0 (5) //[-60,30]
#define ST_AUDIO_AI_ECHO_GAIN1 (5) //[-60,30]

pthread_mutex_t s_STAudioPlayMutex = PTHREAD_MUTEX_INITIALIZER;

MI_S32 ST_Common_AudioAi_SetVolume(ST_Common_AudioAttr_t *pstAudioAttr, int volume)
{
    MI_AUDIO_DEV AiDevId = pstAudioAttr->AiDevId;
    MI_U8 u8ChnGrpId = pstAudioAttr->u8ChnGrpId;
    MI_BOOL abMutes[1];

    if (volume == 0)
    {
        abMutes[0] = 1;
        STCHECKRESULT(MI_AI_SetMute(AiDevId, u8ChnGrpId, abMutes, (sizeof(abMutes) / sizeof(abMutes[0]))));
    }
    else
    {
        abMutes[0] = 0;
        STCHECKRESULT(MI_AI_SetMute(AiDevId, u8ChnGrpId, abMutes, (sizeof(abMutes) / sizeof(abMutes[0]))));
        if (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_STEREO)
        {
            STCHECKRESULT(MI_AI_SetIfGain(E_MI_AI_IF_ADC_AB, volume, volume));
        }
        else
        {
            STCHECKRESULT(MI_AI_SetIfGain(E_MI_AI_IF_ADC_AB, volume, 0));
        }
    }
    return 0;
}

MI_S32 ST_Common_AudioAo_SetVolume(MI_AUDIO_DEV AoDevId, int volume)
{
    STCHECKRESULT(MI_AO_SetVolume(AoDevId, volume, volume, E_MI_AO_GAIN_FADING_OFF));
    return 0;
}

MI_S32 ST_Common_AudioAi_SetMute(MI_AUDIO_DEV AiDevId, MI_U8 u8ChnGrpId, int enable)
{
    MI_BOOL abMutes[MI_AI_MAX_CHN_NUM];
    MI_U8 u8MuteSize = 0;
    STCHECKRESULT(MI_AI_GetMute(AiDevId, u8ChnGrpId, abMutes, &u8MuteSize));
    if (abMutes[0] != enable)
    {
        abMutes[0] = enable;
        STCHECKRESULT(MI_AI_SetMute(AiDevId, u8ChnGrpId, abMutes, u8MuteSize));
    }
    return 0;
}


MI_S32 ST_Common_AudioAi_GetStream(MI_AUDIO_DEV AiDevId, MI_U8 u8ChnGrpId, anj_mw_media_audio_pcm_data pcm_data_cb, int *bStart)
{
    MI_S32 s32Ret = 0;
    MI_AI_Data_t stMicFrame;
    MI_AI_Data_t stEchoFrame;

    __INFO("audio ai thread start\n");
    while (bStart && *bStart)
    {
        memset(&stMicFrame, 0, sizeof(MI_AI_Data_t));
        memset(&stEchoFrame, 0, sizeof(MI_AI_Data_t));

        s32Ret = MI_AI_Read(AiDevId, u8ChnGrpId, &stMicFrame, &stEchoFrame, -1);
        if (MI_SUCCESS == s32Ret)
        {
            pcm_data_cb(stMicFrame.apvBuffer[0], stMicFrame.u32Byte[0], stMicFrame.u64Pts, (unsigned int)stMicFrame.u64Seq);
            s32Ret = MI_AI_ReleaseData(AiDevId, u8ChnGrpId, &stMicFrame, &stEchoFrame);
            if (s32Ret != MI_SUCCESS)
            {
                __ERR("Failed to release frame to Ai Device %d ChnGrp %d, error:0x%x\n", AiDevId, u8ChnGrpId,
                      s32Ret);
            }
        }
        else
        {
            __ERR("Failed to get frame from Ai Device %d ChnGrp %d, error:0x%x\n", AiDevId, u8ChnGrpId, s32Ret);
            break;
        }
    }
    return s32Ret;
}

MI_S32 ST_Common_AudioAo_PcmPlay(char *data, int len)
{
    if ((NULL == data) || (len <= 0))
    {
        __ERR("param invalid!data:%p, len:%d\n", data, len);

        return MI_FAILED;
    }

    MI_S32 s32Ret = MI_SUCCESS;
    MI_S32 offset = 0;
    MI_S32 iAudioLen = 0;
    MI_AUDIO_DEV AoDevId = 0;

    pthread_mutex_lock(&s_STAudioPlayMutex);
    while (len > 0)
    {
        if (len > ST_AUDIO_PLAY_PERIOD_SIZE)
        {
            iAudioLen = ST_AUDIO_PLAY_PERIOD_SIZE;
        }
        else
        {
            iAudioLen = len;
        }
        s32Ret = MI_AO_Write(AoDevId, data + offset, iAudioLen, 0, -1);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("Failed to call MI_AO_Write of Ao DevICE %d , error is 0x%x.\n", AoDevId, s32Ret);
            break;
        }
        len -= iAudioLen;
        offset += iAudioLen;
    }
    pthread_mutex_unlock(&s_STAudioPlayMutex);
    return s32Ret;
}

MI_S32 ST_Common_AudioAo_AencPlay(char *data, int len)
{
    if ((NULL == data) || (len <= 0))
    {
        __ERR("param invalid!data:%p, len:%d\n", data, len);

        return MI_FAILED;
    }
    // int pcmLen = len * 2;
    // char *pcmData = tsml_mem_malloc(pcmLen);
    // memset(pcmData, 0, sizeof(pcmLen));
    // g711_decode(pcmData, pcmLen, data, len, TP_ALAW);
    // ST_Common_AudioAo_PcmPlay(pcmData, pcmLen);
    // free(pcmData);
    // pcmData = NULL;
    return MI_SUCCESS;
}

MI_S32 ST_Common_AudioAiInit(ST_Common_AudioAttr_t *pstAudioAttr)
{
    if (NULL == pstAudioAttr)
    {
        __ERR("pstAudioAttr is NULL!\n");

        return MI_FAILED;
    }
    MI_AUDIO_DEV AiDevId = pstAudioAttr->AiDevId;
    MI_U8 u8ChnGrpIdx = pstAudioAttr->u8ChnGrpIdx;
    MI_U8 u8ChnGrpId = pstAudioAttr->u8ChnGrpId;

    MI_AI_Attr_t stAttr;
    memset(&stAttr, 0, sizeof(MI_AI_Attr_t));
    stAttr.enFormat = pstAudioAttr->enFormat;
    stAttr.enSoundMode = pstAudioAttr->enSoundMode;
    stAttr.enSampleRate = pstAudioAttr->enSampleRate;
    stAttr.u32PeriodSize = pstAudioAttr->u32PeriodSize;
    stAttr.bInterleaved = pstAudioAttr->bInterleaved;
    STCHECKRESULT(MI_AI_Open(AiDevId, &stAttr));

    MI_AI_If_e enAiIf[] = {E_MI_AI_IF_ADC_AB, E_MI_AI_IF_ECHO_A};
    STCHECKRESULT(MI_AI_AttachIf(AiDevId, enAiIf, sizeof(enAiIf) / sizeof(enAiIf[0])));

    if (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_STEREO)
    {
        STCHECKRESULT(MI_AI_SetIfGain(enAiIf[0], 15, 15)); // set adc gain
    }
    else
    {
        STCHECKRESULT(MI_AI_SetIfGain(enAiIf[0], 15, 0)); // set adc gain
    }
    STCHECKRESULT(MI_AI_SetIfGain(enAiIf[1], 0, 0)); // set echo gain

    MI_S8 s8dpgaGain[] = {ST_AUDIO_AI_DPGA_GAIN};
    STCHECKRESULT(MI_AI_SetGain(AiDevId, u8ChnGrpId, s8dpgaGain, sizeof(s8dpgaGain) / sizeof(s8dpgaGain[0]))); // only support  E_MI_AI_IF_ADC_AB
    // MI_S8 as8EchoGain[] = {ST_AUDIO_AI_ECHO_GAIN0, ST_AUDIO_AI_ECHO_GAIN1};
    // STCHECKRESULT(MI_AI_SetGain(AiDevId, MI_AI_ECHO_CHN_GROUP_ID, as8EchoGain, sizeof(as8EchoGain) / sizeof(as8EchoGain[0])));

    MI_SYS_ChnPort_t stChnOutputPort;
    memset(&stChnOutputPort, 0, sizeof(stChnOutputPort));
    stChnOutputPort.eModId = E_MI_MODULE_ID_AI;
    stChnOutputPort.u32DevId = AiDevId;
    stChnOutputPort.u32ChnId = u8ChnGrpIdx;
    stChnOutputPort.u32PortId = 0;
    STCHECKRESULT(MI_SYS_SetChnOutputPortDepth(0, &stChnOutputPort, 4, 8));

    STCHECKRESULT(MI_AI_EnableChnGroup(AiDevId, 0));
    // STCHECKRESULT(MI_AI_EnableChnGroup(AiDevId, 1));

    return MI_SUCCESS;
}

MI_S32 ST_Common_AudioAiUnInit(void)
{
    MI_AUDIO_DEV AiDevId = 0;

    STCHECKRESULT(MI_AI_DisableChnGroup(0, 0));

    STCHECKRESULT(MI_AI_Close(AiDevId));

    return 0;
}

MI_S32 ST_Common_AudioAoInit(ST_Common_AudioAttr_t *pstAudioAttr)
{
    if (NULL == pstAudioAttr)
    {
        __ERR("pstAudioAttr is NULL!\n");

        return MI_FAILED;
    }
    MI_AUDIO_DEV AoDevId = pstAudioAttr->AoDevId;

    MI_AO_Attr_t stAoSetAttr;
    memset(&stAoSetAttr, 0, sizeof(MI_AO_Attr_t));
    stAoSetAttr.enFormat = pstAudioAttr->enFormat;
    stAoSetAttr.enSoundMode = pstAudioAttr->enSoundMode;
    stAoSetAttr.enSampleRate = pstAudioAttr->enSampleRate;
    stAoSetAttr.u32PeriodSize = pstAudioAttr->u32PeriodSize;
    stAoSetAttr.enChannelMode = pstAudioAttr->enChannelMode;
    STCHECKRESULT(MI_AO_Open(AoDevId, &stAoSetAttr));

    MI_AO_If_e aenAoIfs[] = {E_MI_AO_IF_DAC_AB};
    STCHECKRESULT(MI_AO_AttachIf(AoDevId, aenAoIfs[0], 0));

    STCHECKRESULT(MI_AO_SetVolume(AoDevId, 0, 0, 0));

    STCHECKRESULT(MI_AO_SetIfVolume(aenAoIfs[0], 0, 0));

    return MI_SUCCESS;
}

MI_S32 ST_Common_AudioAoUnInit(void)
{
    STCHECKRESULT(MI_AO_Close(0));
    return MI_SUCCESS;
}


MI_S32 ST_Common_AudioAo_PlayEndingCheck()
{
    MI_U64 u64TStamp = 0;
    MI_U32 u32Remaining = 0;
    MI_AUDIO_DEV AoDevId = 0;

    STCHECKRESULT(MI_AO_GetTimestamp(AoDevId, &u32Remaining, &u64TStamp));
    if (u32Remaining == 0)
    {
        return MI_SUCCESS;
    }
    else
    {
        return MI_FAILED;
    }
}

