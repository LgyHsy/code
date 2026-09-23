#include "anj_mw_media_common.h"
#include "anj_mw_media_audio_aec_provider.h"

#define AUDIO_ALGO_DEFALUT_POINT_NUMBER (128)

typedef struct
{
    char *aec_buff;
    AEC_HANDLE aec_handle;
} AnjMwMediaAudioAecParam_t;

static AnjMwMediaAudioAecParam_t s_stAudioAecParam;

static int anj_mw_media_audio_aec_init(void *pstAudioAttr)
{
    int iRet = 0;
    ST_Common_AudioAttr_t *pstAttr = (ST_Common_AudioAttr_t *)pstAudioAttr;
    AudioAecInit stAecInit = {0};
    AudioAecConfig stAecConfig = {0};

    unsigned int uSupModeBand[6] = {20, 40, 60, 80, 100, 120};
    unsigned int uSupMode[7] = {8, 8, 8, 8, 8, 8, 8};

    if (!pstAttr)
    {
        return -1;
    }

    stAecConfig.delay_sample = 0;
    stAecConfig.comfort_noise_enable = IAA_AEC_TRUE;
    memcpy(&(stAecConfig.suppression_mode_freq[0]), uSupModeBand, sizeof(uSupModeBand));
    memcpy(&(stAecConfig.suppression_mode_intensity[0]), uSupMode, sizeof(uSupMode));

    do
    {
        stAecInit.point_number = AUDIO_ALGO_DEFALUT_POINT_NUMBER;

        if (pstAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_MONO)
        {
            stAecInit.nearend_channel = 1;
            stAecInit.farend_channel = 1;
        }
        else if (pstAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_STEREO)
        {
            stAecInit.nearend_channel = 2;
            stAecInit.farend_channel = 2;
        }
        else
        {
            __ERR("audio iaa aec don't support soundmode:%d!\n", pstAttr->enSoundMode);
            break;
        }

        if (pstAttr->enSampleRate == 8000)
        {
            stAecInit.sample_rate = IAA_AEC_SAMPLE_RATE_8000;
        }
        else if (pstAttr->enSampleRate == 16000)
        {
            stAecInit.sample_rate = IAA_AEC_SAMPLE_RATE_16000;
        }
        else
        {
            __ERR("audio iaa aec don't support samplerate:%d!\n", pstAttr->enSampleRate);
            break;
        }

        unsigned int buffer_size = IaaAec_GetBufferSize();
        s_stAudioAecParam.aec_buff = (char *)anj_mw_malloc(buffer_size);
        if (s_stAudioAecParam.aec_buff == NULL)
        {
            __ERR("audio iaa aec malloc buf failed, buf_size:%d!\n", buffer_size);
            return -1;
        }

        s_stAudioAecParam.aec_handle = IaaAec_Init(s_stAudioAecParam.aec_buff, &stAecInit);
        if (s_stAudioAecParam.aec_handle == NULL)
        {
            __ERR("audio iaa aec handle init failed\n");
        }

        iRet = IaaAec_Config(s_stAudioAecParam.aec_handle, &stAecConfig);
        if (iRet != ALGO_AEC_RET_SUCCESS)
        {
            __ERR("audio iaa aec config failed\n");
            break;
        }

        __INFO("audio iaa aec init and config success, buf_size:%u!\n", buffer_size);
    } while (0);

    return 0;
}

static int anj_mw_media_audio_aec_uninit(void)
{
    if (s_stAudioAecParam.aec_handle)
    {
        IaaAec_Free(s_stAudioAecParam.aec_handle);
        s_stAudioAecParam.aec_handle = NULL;
    }

    if (s_stAudioAecParam.aec_buff)
    {
        anj_mw_free(s_stAudioAecParam.aec_buff);
        s_stAudioAecParam.aec_buff = NULL;
    }

    return 0;
}

static int anj_mw_media_audio_aec_trans(char *data_near, char *data_far, int len, int audio_chn)
{
    MI_S32 iRet = 0;
    MI_U32 uNum = 0;

    do
    {
        if (s_stAudioAecParam.aec_handle == NULL)
        {
            break;
        }

        if (uNum >= len / 2)
        {
            break;
        }

        iRet = IaaAec_Run(s_stAudioAecParam.aec_handle, (short *)data_near + uNum, (short *)data_far + uNum);
        if (iRet)
        {
            __ERR("audio iaa IaaAec_Run failed:%d!\n", iRet);
            break;
        }

        if (audio_chn == 1)
        {
            uNum += AUDIO_ALGO_DEFALUT_POINT_NUMBER;
        }
        else if (audio_chn == 2)
        {
            uNum += 2 * AUDIO_ALGO_DEFALUT_POINT_NUMBER;
        }
        else
        {
            break;
        }
    } while (1);

    return iRet;
}

static const anj_mw_media_audio_aec_ops s_stMwMediaAudioAecOps = {
    .provider_name = "mstar_aec",
    .provider_priority = 100,
    .init = anj_mw_media_audio_aec_init,
    .uninit = anj_mw_media_audio_aec_uninit,
    .trans = anj_mw_media_audio_aec_trans,
};

ANJ_LINK_KEEP(anj_keep_mw_media_audio_aec_provider);

__attribute__((constructor)) static void anj_mw_media_audio_aec_provider_register_constructor(void)
{
    anj_mw_media_audio_aec_provider_register(&s_stMwMediaAudioAecOps);
}

__attribute__((destructor)) static void anj_mw_media_audio_aec_provider_unregister_constructor(void)
{
    anj_mw_media_audio_aec_provider_unregister(&s_stMwMediaAudioAecOps);
}
