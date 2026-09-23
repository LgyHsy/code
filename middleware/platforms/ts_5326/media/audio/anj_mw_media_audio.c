#include "anj_mw_media_common.h"

#include "anj_mw_comm.h"

#include <sys/select.h>
#include "ts_alg_vqe_sol_webrtc3a.h"

#define TS_INNER_AI_DEV (0)
#define TS_INNER_AO_DEV (0)
#define TS_AUDIO_AI_CHN (0)
#define TS_AUDIO_AO_CHN (0)
#define TS_AUDIO_AI_CHN_CNT (1)
#define TS_AUDIO_AO_CHN_CNT (1)

/* AI 模拟音量：-6~39dB step 3 */
#define TS_AUDIO_AI_ANA_DB_MIN (-6)
#define TS_AUDIO_AI_ANA_DB_MAX (39)
#define TS_AUDIO_AI_ANA_DB_STEP (3)
#define TS_AUDIO_AI_MAX_VOLUME (14)
#define TS_AUDIO_AI_MAX_VOLUME_AMPLIFY (4)
#define TS_AUDIO_AI_MIN_VOLUME (0)

/* AO 数字音量：对齐 sample_audio soXX，-69~26dB step 1 */
#define TS_AUDIO_AO_DB_MIN (-69)
#define TS_AUDIO_AO_DB_MAX (26)

static int s_bAudioInit = 0;
static anj_thread_s s_stAudioAiThread;
static TS_S32 s_s32AiAnaVolDb = TS_AUDIO_AI_ANA_DB_MIN;
static AnjAudioConfig s_stAnjAudioCfg;

static AUDIO_SAMPLE_RATE_E ts_audio_int_to_sample_rate(int samplerate)
{
    switch (samplerate)
    {
    case 8000:
        return AUDIO_SAMPLE_RATE_8000;
    case 12000:
        return AUDIO_SAMPLE_RATE_12000;
    case 11025:
        return AUDIO_SAMPLE_RATE_11025;
    case 16000:
        return AUDIO_SAMPLE_RATE_16000;
    case 22050:
        return AUDIO_SAMPLE_RATE_22050;
    case 24000:
        return AUDIO_SAMPLE_RATE_24000;
    case 32000:
        return AUDIO_SAMPLE_RATE_32000;
    case 44100:
        return AUDIO_SAMPLE_RATE_44100;
    case 48000:
        return AUDIO_SAMPLE_RATE_48000;
    default:
        return AUDIO_SAMPLE_RATE_8000;
    }
}

int anj_mw_media_audio_ai_capture_rate(int config_samplerate)
{
    return config_samplerate;
}

static int anj_mw_media_audio_thread(void *ctx, int *bStart)
{
    if (!ctx || !bStart)
        return 0;

    AnjAudioConfig *pstCfg = (AnjAudioConfig *)ctx;
    return TS_Common_AudioAi_GetStream(TS_INNER_AI_DEV, TS_AUDIO_AI_CHN, pstCfg->pcm_data_cb, bStart);
}

int anj_mw_media_audio_ai_pcm_process(char *data, int len, int chn, const AnjAudioConfig *cfg,
                                      char **out_data, int *out_len)
{
    (void)chn;
    (void)cfg;

    if (!data || len <= 0 || !out_data || !out_len)
        return -1;

    *out_data = data;
    *out_len = len;
    return 0;
}

int anj_mw_media_audio_init(AnjAudioConfig *pstAnjAudioCfg)
{
    TS_S32 s32Ret;
    TS_Common_AudioAttr_t stAiAudioAttr;
    TS_Common_AudioAttr_t stAoAudioAttr;

    if (!pstAnjAudioCfg)
    {
        __ERR("anj_mw_media_audio_init cfg null\n");
        return -1;
    }

    if (s_bAudioInit)
    {
        __ERR("audio already init\n");
        return -1;
    }

    memcpy(&s_stAnjAudioCfg, pstAnjAudioCfg, sizeof(AnjAudioConfig));

    memset(&stAiAudioAttr, 0, sizeof(stAiAudioAttr));
    memset(&stAoAudioAttr, 0, sizeof(stAoAudioAttr));
    stAiAudioAttr.AiDevId = TS_INNER_AI_DEV;
    stAiAudioAttr.AoDevId = TS_INNER_AO_DEV;
    stAiAudioAttr.enFormat = PT_LPCM;
    stAiAudioAttr.enSoundMode = (pstAnjAudioCfg->chn > 1) ? AUDIO_SOUND_MODE_STEREO : AUDIO_SOUND_MODE_MONO;
    stAiAudioAttr.enSampleRate = ts_audio_int_to_sample_rate(pstAnjAudioCfg->samplerate);
    stAiAudioAttr.u32PeriodSize = (pstAnjAudioCfg->persize > 0) ? (TS_U32)pstAnjAudioCfg->persize : 320;
    stAiAudioAttr.enBitwidth = (pstAnjAudioCfg->bitwidth == 8) ? AUDIO_BIT_WIDTH_8 : AUDIO_BIT_WIDTH_16;
    if (pstAnjAudioCfg->wave_active)
    {
        /* 声波配网：16K 原始 PCM，不走 VQE */
        stAiAudioAttr.bAecEnable = 0;
        stAiAudioAttr.bAnrEnable = 0;
        stAiAudioAttr.bAgcEnable = 0;
        stAiAudioAttr.bAiVqeEnable = 0;
    }
    else
    {
        stAiAudioAttr.bAecEnable = pstAnjAudioCfg->algo_aec_enable;
        stAiAudioAttr.bAnrEnable = pstAnjAudioCfg->algo_anr_enable;
        stAiAudioAttr.bAgcEnable = pstAnjAudioCfg->algo_agc_enable;
        stAiAudioAttr.bAiVqeEnable = pstAnjAudioCfg->algo_agc_enable || pstAnjAudioCfg->algo_anr_enable
                                       || pstAnjAudioCfg->algo_aec_enable;
    }
    stAiAudioAttr.enChannelMode = AIO_INNER_CODEC;
    memcpy(&stAoAudioAttr, &stAiAudioAttr, sizeof(stAoAudioAttr));
    stAoAudioAttr.enSampleRate = ts_audio_int_to_sample_rate(pstAnjAudioCfg->ao_samplerate);
    stAoAudioAttr.u32PeriodSize = (pstAnjAudioCfg->persize > 0) ? (TS_U32)pstAnjAudioCfg->persize : 320;
    stAoAudioAttr.bAecEnable = 0;
    stAoAudioAttr.bAiVqeEnable = 0;

    s32Ret = TS_Common_AudioAiInit(&stAiAudioAttr);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_Common_AudioAiInit failed: ret=0x%x\n", (unsigned int)s32Ret);
        return (int)s32Ret;
    }

    s32Ret = TS_Common_AudioAoInit(&stAoAudioAttr);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_Common_AudioAoInit failed: ret=0x%x\n", (unsigned int)s32Ret);
        TS_Common_AudioAiUnInit();
        return (int)s32Ret;
    }

    anj_mw_media_audio_ai_volume_set(pstAnjAudioCfg->ai_volume, pstAnjAudioCfg->ai_amplify);

    s_stAudioAiThread.bAutoDestroy = 0;
    strncpy(s_stAudioAiThread.iThreadName, "aistream", sizeof(s_stAudioAiThread.iThreadName) - 1);
    s_stAudioAiThread.iThreadjob.ctx = &s_stAnjAudioCfg;
    s_stAudioAiThread.iThreadjob.func = anj_mw_media_audio_thread;
    STCHECKRESULT(anj_thread_task_create(&s_stAudioAiThread));

    s_bAudioInit = 1;
    anj_mw_media_audio_ao_volume_set(pstAnjAudioCfg->ao_volume);
    return 0;
}

int anj_mw_media_audio_uninit(void)
{
    if (!s_bAudioInit)
    {
        __ERR("audio not init\n");
        return -1;
    }

    anj_thread_task_destroy(&s_stAudioAiThread, 0);
    TS_Common_AudioAiUnInit();
    TS_Common_AudioAoUnInit();

    s_bAudioInit = 0;
    memset(&s_stAnjAudioCfg, 0, sizeof(s_stAnjAudioCfg));
    return 0;
}

int anj_mw_media_audio_ai_volume_set(int volume, int amplify)
{
    int ai_max_volume = TS_AUDIO_AI_MAX_VOLUME;
    int tmpVolume;
    int st_volume;
    TS_S32 s32AnaDb;

    volume = anj_mw_check_value_in_range(volume, 0, 100);
    if (amplify == 0) // 有源输入
    {
        ai_max_volume = TS_AUDIO_AI_MAX_VOLUME_AMPLIFY;
    }

    tmpVolume = (volume * (ai_max_volume - TS_AUDIO_AI_MIN_VOLUME)) / 10;
    // 四舍五入
    st_volume = ALIGN_FRONT(tmpVolume, 5) / 10;

    if (st_volume <= 0)
    {
        s_s32AiAnaVolDb = TS_AUDIO_AI_ANA_DB_MIN;
        return TS_MPI_AI_SetMute(TS_INNER_AI_DEV, TS_TRUE);
    }

    s32AnaDb = TS_AUDIO_AI_ANA_DB_MIN + st_volume * TS_AUDIO_AI_ANA_DB_STEP;
    s32AnaDb = (TS_S32)anj_mw_check_value_in_range((int)s32AnaDb, TS_AUDIO_AI_ANA_DB_MIN, TS_AUDIO_AI_ANA_DB_MAX);
    s_s32AiAnaVolDb = s32AnaDb;

    TS_MPI_AI_SetMute(TS_INNER_AI_DEV, TS_FALSE);
    return TS_Common_AudioAi_SetVolume(TS_INNER_AI_DEV, TS_AUDIO_AI_CHN, s32AnaDb);
}

int anj_mw_media_audio_ao_volume_set(int volume)
{
    int pct;
    TS_S32 s32Db;

    pct = anj_mw_check_value_in_range(volume, 0, 100);
    s32Db = TS_AUDIO_AO_DB_MIN + (pct * (TS_AUDIO_AO_DB_MAX - TS_AUDIO_AO_DB_MIN)) / 100;

    if (!s_bAudioInit)
        return 0;

    if (pct <= 0)
        return TS_Common_AudioAo_SetMute(TS_INNER_AO_DEV, 1);

    TS_Common_AudioAo_SetMute(TS_INNER_AO_DEV, 0);
    return TS_Common_AudioAo_SetVolume(TS_INNER_AO_DEV, s32Db);
}

int anj_mw_media_audio_ai_mute_set(int enable)
{
    if (!s_bAudioInit)
        return -1;

    if (enable)
        return TS_MPI_AI_SetMute(TS_INNER_AI_DEV, TS_TRUE);

    if (s_s32AiAnaVolDb <= TS_AUDIO_AI_ANA_DB_MIN)
        return TS_MPI_AI_SetMute(TS_INNER_AI_DEV, TS_TRUE);

    TS_MPI_AI_SetMute(TS_INNER_AI_DEV, TS_FALSE);
    return TS_Common_AudioAi_SetVolume(TS_INNER_AI_DEV, TS_AUDIO_AI_CHN, s_s32AiAnaVolDb);
}

int anj_mw_media_audio_play(char *data, int len)
{
    if (!data || len <= 0)
        return -1;
    if (!s_bAudioInit)
        return -1;
    return TS_Common_AudioAo_PcmPlay(data, len);
}

int anj_mw_media_audio_ao_play_check(void)
{
    if (!s_bAudioInit)
        return 0;
    return (int)TS_Common_AudioAo_PlayEndingCheck();
}
