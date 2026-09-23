#include "anj_mw_media_common.h"
#include "anj_mw_media_video.h"
#include "anj_mw_media_audio.h"
#include "anj_mw_media_audio_aec_provider.h"

#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_thread.h"
#include "protocol_queue.h"

#include <string.h>

#define ST_AUDIO_AI_MAX_VOLUME (14) //[-60,30]
#define ST_AUDIO_AI_MAX_VOLUME_AMPLIFY (4)
#define ST_AUDIO_AI_MIN_VOLUME (0)   //[-60,30]
#define ST_AUDIO_AO_MAX_VOLUME (7)   //[-60,30]
#define ST_AUDIO_AO_MIN_VOLUME (-60) //[-60,30]

#define AUDIO_ALGO_DEFALUT_POINT_NUMBER (128)
#define MW_AUDIO_PROCESS_BUF_SIZE (2560)
#define MW_AO_REF_QUEUE_MAX (50)

typedef struct
{
    // anr降噪
    char *anr_buff;
    ANR_HANDLE anr_handle;

    // agc 自动增益
    char *agc_buff;
    AGC_HANDLE agc_handle;

    // eq 均衡器
    char *eq_buff;
    EQ_HANDLE eq_handle;

    // bf 波束成形，用于多mic的语音增强
    char *bf_buff;
    BF_HANDLE bf_handle;
    int bf_point_number;
} AnjAudioAlgoParam_t;

static AnjAudioAlgoParam_t s_stAudioAlgoParam;
static FRAME_BUFFER_MANAGER s_stAoRefMgr = {0};
static char s_aiProcessBuf[MW_AUDIO_PROCESS_BUF_SIZE];

static int gst_bAudioInit = 0;

static anj_thread_s s_stAudioAiThread;

int anj_mw_media_audio_ai_capture_rate(int config_samplerate)
{
    (void)config_samplerate;
    return 16000;
}

static int anj_mw_media_audio_thread(void *ctx, int *bStart)
{
    __LOG_ENTER();
    if (!ctx)
    {
        __ERR("input invalid!\n");
        goto endFunc;
    }
    AnjAudioConfig *pstAnjAudioCfg = (AnjAudioConfig *)ctx;
    MI_AUDIO_DEV AiDevId = 0;
    MI_U8 u8ChnGrpId = 0;
    ST_Common_AudioAi_GetStream(AiDevId, u8ChnGrpId, pstAnjAudioCfg->pcm_data_cb, bStart);

endFunc:
    __LOG_LEAVE();
    return 0;
}

static int audio_iaa_aec_init(ST_Common_AudioAttr_t *pstAudioAttr)
{
    return anj_mw_media_audio_aec_provider_init(pstAudioAttr);
}

static int audio_iaa_aec_uninit()
{
    return anj_mw_media_audio_aec_provider_uninit();
}

static int audio_iaa_anr_init(ST_Common_AudioAttr_t *pstAudioAttr)
{
    int iRet = 0;
    AudioProcessInit stAudioProInit = {0};
    AudioAnrConfig stAudioAnrCfg = {0};

    int intensity_band[6] = {3, 24, 40, 64, 80, 128};
    int intensity[7] = {27, 27, 27, 27, 27, 27, 27};


    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
    {
        int ws_intensity_band[6] = {4, 10, 36, 48, 80, 127};
        int ws_intensity[7] = {28, 28, 28, 28, 28, 28, 28};
        memcpy(intensity_band, ws_intensity_band, sizeof(intensity_band));
        memcpy(intensity, ws_intensity, sizeof(intensity));
    }
    do
    {
        // 算法处理一次的数据量（取值范围：128或256）
        stAudioProInit.point_number = AUDIO_ALGO_DEFALUT_POINT_NUMBER;
        if (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_MONO)
        {
            stAudioProInit.channel = 1;
        }
        else if (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_STEREO)
        {
            stAudioProInit.channel = 2;
        }
        else
        {
            __ERR("audio iaa anr don't support soundmode:%d\n", pstAudioAttr->enSoundMode);
            break;
        }

        if (pstAudioAttr->enSampleRate == 8000)
        {
            stAudioProInit.sample_rate = IAA_APC_SAMPLE_RATE_8000;
        }
        else if (pstAudioAttr->enSampleRate == 16000)
        {
            stAudioProInit.sample_rate = IAA_APC_SAMPLE_RATE_16000;
        }
        else if (pstAudioAttr->enSampleRate == 48000)
        {
            stAudioProInit.sample_rate = IAA_APC_SAMPLE_RATE_48000;
        }
        else
        {
            // ANR 算法只支持 8K/16K/48K 采样率。
            __ERR("audio iaa anr don't support samplerate:%d\n", pstAudioAttr->enSampleRate);
            break;
        }

        stAudioAnrCfg.anr_enable = 1;
        stAudioAnrCfg.user_mode = 2;                                                      // anr算法运行模式
        stAudioAnrCfg.anr_smooth_level = 10;                                              // 频域平滑程度
        stAudioAnrCfg.anr_converge_speed = NR_SPEED_MID;                                  // 噪声收敛速度
        stAudioAnrCfg.anr_filter_mode = 0;                                                // 频域降噪滤波器，范围[0,4]，步长1
        memcpy(stAudioAnrCfg.anr_intensity_band, intensity_band, sizeof(intensity_band)); // 降噪频率范围
        memcpy(stAudioAnrCfg.anr_intensity, intensity, sizeof(intensity));                // 降噪强度，值越大降噪强度越高

        unsigned int buffer_size = IaaAnr_GetBufferSize();
        s_stAudioAlgoParam.anr_buff = (char *)anj_mw_malloc(buffer_size);
        if (s_stAudioAlgoParam.anr_buff == NULL)
        {
            __ERR("audio iaa anr buf malloc failed, buf_size:%u!\n", buffer_size);
            return -1;
        }

        s_stAudioAlgoParam.anr_handle = IaaAnr_Init(s_stAudioAlgoParam.anr_buff, &stAudioProInit);
        if (s_stAudioAlgoParam.anr_handle == NULL)
        {
            __ERR("audio iaa anr handle init failed\n");
            return -1;
        }

        iRet = IaaAnr_Config(s_stAudioAlgoParam.anr_handle, &stAudioAnrCfg);
        if (iRet)
        {
            __ERR("audio iaa anr config failed\n");
            break;
        }

        __INFO("audio iaa anr init and config success, buf_size:%u\n", buffer_size);
    } while (0);

    return 0;
}

static int audio_iaa_anr_uninit()
{
    if (s_stAudioAlgoParam.anr_handle)
    {
        IaaAnr_Free(s_stAudioAlgoParam.anr_handle);
        s_stAudioAlgoParam.anr_handle = NULL;
    }

    if (s_stAudioAlgoParam.anr_buff)
    {
        anj_mw_free(s_stAudioAlgoParam.anr_buff);
    }

    return 0;
}

static int audio_iaa_agc_init(ST_Common_AudioAttr_t *pstAudioAttr)
{
    int iRet = 0;
    AudioAgcConfig stAudioAgcCfg = {0};
    AudioProcessInit stAudioProInit = {0};

    do
    {
        stAudioProInit.point_number = AUDIO_ALGO_DEFALUT_POINT_NUMBER;
        stAudioProInit.channel = (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_MONO) ? 1 : 2;
        if (pstAudioAttr->enSampleRate == 8000)
        {
            stAudioProInit.sample_rate = IAA_APC_SAMPLE_RATE_8000;
        }
        else if (pstAudioAttr->enSampleRate == 16000)
        {
            stAudioProInit.sample_rate = IAA_APC_SAMPLE_RATE_16000;
        }
        else if (pstAudioAttr->enSampleRate == 32000)
        {
            stAudioProInit.sample_rate = IAA_APC_SAMPLE_RATE_32000;
        }
        else if (pstAudioAttr->enSampleRate == 48000)
        {
            stAudioProInit.sample_rate = IAA_APC_SAMPLE_RATE_48000;
        }
        else
        {
            __ERR("audio iaa agc don't support samplerate:%d\n", pstAudioAttr->enSampleRate);
            break;
        }

        stAudioAgcCfg.agc_enable = 1;
        stAudioAgcCfg.user_mode = 1;
        stAudioAgcCfg.gain_info.gain_max = 20;
        stAudioAgcCfg.gain_info.gain_min = -20;
        stAudioAgcCfg.gain_info.gain_init = 10;
        stAudioAgcCfg.drop_gain_max = 30;
        stAudioAgcCfg.attack_time = 1;
        stAudioAgcCfg.release_time = 1;

        short compression_ration_input[7] = {-80, -70, -40, -30, -20, -12, 0};
        short compression_ration_output[7] = {-75, -60, -30, -20, -15, -9, -3};
        memcpy(stAudioAgcCfg.compression_ratio_input, compression_ration_input, sizeof(compression_ration_input));
        memcpy(stAudioAgcCfg.compression_ratio_output, compression_ration_output, sizeof(compression_ration_output));
        stAudioAgcCfg.drop_gain_threshold = -3;

        stAudioAgcCfg.noise_gate_db = -75;
        stAudioAgcCfg.noise_gate_attenuation_db = 0;
        stAudioAgcCfg.gain_step = 1;

        unsigned int buff_size = IaaAgc_GetBufferSize();
        s_stAudioAlgoParam.agc_buff = (char *)anj_mw_malloc(buff_size);
        if (s_stAudioAlgoParam.agc_buff == NULL)
        {
            __ERR("audio iaa agc malloc buf failed, buf_size:%d!\n", buff_size);
            return -1;
        }

        s_stAudioAlgoParam.agc_handle = IaaAgc_Init(s_stAudioAlgoParam.agc_buff, &stAudioProInit);
        if (s_stAudioAlgoParam.agc_handle == NULL)
        {
            __ERR("audio iaa agc init handle failed!\n");
            return -1;
        }

        iRet = IaaAgc_Config(s_stAudioAlgoParam.agc_handle, &stAudioAgcCfg);
        if (iRet)
        {
            __ERR("audio iaa agc config failed, ret:0x%x\n", iRet);
            break;
        }
        __INFO("audio iaa agc init and config success, buf_size:%u!\n", buff_size);
    } while (0);

    return 0;
}

static int audio_iaa_agc_uninit()
{
    if (s_stAudioAlgoParam.agc_handle)
    {
        IaaAgc_Free(s_stAudioAlgoParam.agc_handle);
        s_stAudioAlgoParam.agc_handle = NULL;
    }

    if (s_stAudioAlgoParam.agc_buff)
    {
        anj_mw_free(s_stAudioAlgoParam.agc_buff);
    }

    return 0;
}

static int audio_iaa_eq_init(ST_Common_AudioAttr_t *pstAudioAttr)
{
    int iRet = 0;
    AudioHpfConfig stAudioHpfCfg = {0};
    AudioEqConfig stAudioEqCfg = {0};
    AudioProcessInit stAudioEqInit = {0};

    short eq_table[129] = {-20, -12, -6, -6, -4, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3,
                           -3, -3, -3, -3, -3, -3, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6, -6,
                           -6, -6, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12, -12};

    do
    {
        stAudioEqInit.point_number = AUDIO_ALGO_DEFALUT_POINT_NUMBER;
        stAudioEqInit.channel = (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_MONO) ? 1 : 2;
        if (pstAudioAttr->enSampleRate == 8000)
        {
            stAudioEqInit.sample_rate = IAA_APC_SAMPLE_RATE_8000;
        }
        else if (pstAudioAttr->enSampleRate == 16000)
        {
            stAudioEqInit.sample_rate = IAA_APC_SAMPLE_RATE_16000;
        }
        else if (pstAudioAttr->enSampleRate == 32000)
        {
            stAudioEqInit.sample_rate = IAA_APC_SAMPLE_RATE_32000;
        }
        else if (pstAudioAttr->enSampleRate == 48000)
        {
            stAudioEqInit.sample_rate = IAA_APC_SAMPLE_RATE_48000;
        }
        else
        {
            __ERR("audio iaa eq don't support samplerate:%d\n", pstAudioAttr->enSampleRate);
            break;
        }

        if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
        {
            stAudioHpfCfg.hpf_enable = 1;
        }
        else
        {
            stAudioHpfCfg.hpf_enable = 0;
        }
        stAudioHpfCfg.user_mode = 1;
        stAudioHpfCfg.cutoff_frequency = AUDIO_HPF_FREQ_80;

        stAudioEqCfg.eq_enable = 1;
        stAudioEqCfg.user_mode = 1;
        memcpy(stAudioEqCfg.eq_gain_db, eq_table, sizeof(eq_table));

        unsigned int buff_size = IaaEq_GetBufferSize();
        s_stAudioAlgoParam.eq_buff = (char *)anj_mw_malloc(buff_size);
        if (s_stAudioAlgoParam.eq_buff == NULL)
        {
            __ERR("audio iaa eq buf malloc failed!\n");
            return -1;
        }

        s_stAudioAlgoParam.eq_handle = IaaEq_Init(s_stAudioAlgoParam.eq_buff, &stAudioEqInit);
        if (s_stAudioAlgoParam.eq_handle == NULL)
        {
            __ERR("audio iaa eq handle init failed!\n");
            return -1;
        }

        iRet = IaaEq_Config(s_stAudioAlgoParam.eq_handle, &stAudioHpfCfg, &stAudioEqCfg);
        if (iRet)
        {
            __ERR("audio iaa eq config failed!\n");
            break;
        }

        __INFO("audio iaa eq init and config success, buf_size:%u!\n", buff_size);

    } while (0);

    return 0;
}

static int audio_iaa_eq_uninit()
{
    if (s_stAudioAlgoParam.eq_handle)
    {
        IaaEq_Free(s_stAudioAlgoParam.eq_handle);
        s_stAudioAlgoParam.eq_handle = NULL;
    }

    if (s_stAudioAlgoParam.eq_buff)
    {
        anj_mw_free(s_stAudioAlgoParam.eq_buff);
    }

    return 0;
}

static int audio_iaa_bf_init(ST_Common_AudioAttr_t *pstAudioAttr)
{
    int iRet = 0;
    AudioBfConfig stAudioBfCfg = {0};
    AudioBfInit stAudioBfInit = {0};

    do
    {

        stAudioBfInit.mic_distance = 5.0;                             // 相邻麦克风之间的间距，单位为cm
        stAudioBfInit.point_number = AUDIO_ALGO_DEFALUT_POINT_NUMBER; // 算法处理一次的数据量(取值范围：128 或 256)

        if (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_MONO)
        {
            __ERR("audio iaa bf only support two mics\n");
            break;
        }
        else if (pstAudioAttr->enSoundMode == E_MI_AUDIO_SOUND_MODE_STEREO)
        {
            stAudioBfInit.channel = 2; // bf多mic语音增强只在mic通道 >=2时生效，目前只设置通道为2
        }
        else
        {
            __ERR("audio iaa bf only support two mics\n");
            break;
        }

        if (pstAudioAttr->enSampleRate == 8000)
        {
            stAudioBfInit.sample_rate = IAA_APC_SAMPLE_RATE_8000;
        }
        else if (pstAudioAttr->enSampleRate == 16000)
        {
            stAudioBfInit.sample_rate = IAA_APC_SAMPLE_RATE_16000;
        }
        else
        {
            // BF 算法只支持 8K/16K/ 采样率
            __ERR("audio iaa bf don't support samplerate:%d\n", pstAudioAttr->enSampleRate);
            break;
        }

        stAudioBfCfg.noise_gate_dbfs = -20;
        stAudioBfCfg.temperature = 20;
        stAudioBfCfg.noise_estimation = 0;
        stAudioBfCfg.output_gain = 0.7;
        stAudioBfCfg.vad_enable = 0;
        stAudioBfCfg.diagonal_loading = 10;

        s_stAudioAlgoParam.bf_point_number = stAudioBfInit.point_number;

        unsigned int buff_size = IaaBf_GetBufferSize();
        s_stAudioAlgoParam.bf_buff = (char *)anj_mw_malloc(buff_size);
        if (s_stAudioAlgoParam.bf_buff == NULL)
        {
            __ERR("audio iaa bf buf malloc failed, buf_size:%u\n", buff_size);
            return -1;
        }

        s_stAudioAlgoParam.bf_handle = IaaBf_Init(s_stAudioAlgoParam.bf_buff, &stAudioBfInit);
        if (s_stAudioAlgoParam.bf_handle == NULL)
        {
            __ERR("audio iaa bf handle init failed!\n");
            return -1;
        }

        iRet = IaaBf_SetConfig(s_stAudioAlgoParam.bf_handle, &stAudioBfCfg);
        if (iRet)
        {
            __ERR("audio iaa bf conifg set failed\n");
            break;
        }

        // 指派数组形状，0:均匀线性数组，1：均匀圆形数组
        iRet = IaaBf_SetShape(s_stAudioAlgoParam.bf_handle, 0);
        if (iRet)
        {
            __ERR("audio iaa bf set shape failed\n");
            break;
        }

        __INFO("audio iaa bf init and config success, buf_size:%u\n", buff_size);

    } while (0);

    return 0;
}

static int audio_iaa_bf_uninit()
{
    if (s_stAudioAlgoParam.bf_handle)
    {
        IaaBf_Free(s_stAudioAlgoParam.bf_handle);
        s_stAudioAlgoParam.bf_handle = NULL;
    }

    if (s_stAudioAlgoParam.bf_buff)
    {
        anj_mw_free(s_stAudioAlgoParam.bf_buff);
    }

    return 0;
}

static int audio_iaa_bf_trans(char *data_in, char *data_out, int len)
{
    // 数据量必须和初始化时设定的point_number相对应
    MI_S32 iRet = 0;
    MI_U32 uSrcPos = 0;
    MI_U32 uDstPos = 0;
    float direction = 0.0;

    do
    {
        if (s_stAudioAlgoParam.bf_handle == NULL)
        {
            break;
        }

        iRet = IaaBf_Run(s_stAudioAlgoParam.bf_handle, (short *)(data_in + uSrcPos), (short *)(data_out + uDstPos), &direction);
        if (iRet)
        {
            __ERR("audio iaa IaaBf_Run failed:%d\n", iRet);
            break;
        }

        uSrcPos += s_stAudioAlgoParam.bf_point_number * 2;
        uDstPos += s_stAudioAlgoParam.bf_point_number;

        if (uSrcPos >= len)
        {
            break;
        }
    } while (1);

    return iRet;
}

static int audio_iaa_aec_trans(char *data_near, char *data_far, int len, int audio_chn)
{
    return anj_mw_media_audio_aec_provider_trans(data_near, data_far, len, audio_chn);
}

static int audio_iaa_anr_trans(short *data_in, int len, int audio_chn)
{
    MI_S32 iRet = 0;
    MI_U32 uNum = 0;

    do
    {
        if (s_stAudioAlgoParam.anr_handle == NULL)
        {
            break;
        }

        if (uNum >= len / 2)
        {
            break;
        }

        // 数据量必须和调用IaaAnr_Init时的point_number相对应（一次IaaAnr_Run所需要的采样点数）
        iRet = IaaAnr_Run(s_stAudioAlgoParam.anr_handle, data_in + uNum);
        if (iRet)
        {
            __ERR("audio iaa IaaAnr_Run failed:%d!\n", iRet);
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

    } while (1);

    return iRet;
}

static int audio_iaa_eq_trans(short *data_in, int len, int audio_chn)
{
    MI_S32 iRet = 0;
    MI_U32 uNum = 0;

    do
    {
        if (s_stAudioAlgoParam.eq_handle == NULL)
        {
            break;
        }

        if (uNum >= len / 2)
        {
            break;
        }

        iRet = IaaEq_Run(s_stAudioAlgoParam.eq_handle, data_in + uNum);
        if (iRet)
        {
            __ERR("audio iaa IaaEq_Run failed:%d!\n", iRet);
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
    } while (1);

    return iRet;
}

static int audio_iaa_agc_trans(short *data_in, int len, int audio_chn)
{
    MI_S32 iRet = 0;
    MI_U32 uNum = 0;

    do
    {
        if (s_stAudioAlgoParam.agc_handle == NULL)
        {
            break;
        }

        if (uNum >= len / 2)
        {
            break;
        }

        iRet = IaaAgc_Run(s_stAudioAlgoParam.agc_handle, data_in + uNum);
        if (iRet)
        {
            __ERR("audio iaa IaaAgc_Run failed:%d!\n", iRet);
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
    } while (1);

    return iRet;
}

int anj_mw_media_audio_ai_pcm_process(char *data, int len, int chn, const AnjAudioConfig *cfg,
                                      char **out_data, int *out_len)
{
    char *pcm = data;
    int pcm_len = len;

    if (!data || len <= 0 || !out_data || !out_len)
        return -1;

    if (cfg && cfg->algo_bf_enable)
    {
        pcm_len = len / 2;
        audio_iaa_bf_trans(data, s_aiProcessBuf, len);
        pcm = s_aiProcessBuf;
    }

    if (cfg && cfg->algo_aec_enable && s_stAoRefMgr.max > 0)
    {
        FRAME_ENTRY frame = {0};
        if (frame_mgr_pop(&s_stAoRefMgr, &frame) > 0)
        {
            audio_iaa_aec_trans(data, frame.pFrame, len, chn);
            if (frame.pFrame)
                anj_mw_free(frame.pFrame);
        }
    }

    if (cfg)
    {
        if (cfg->algo_anr_enable)
            audio_iaa_anr_trans((short *)pcm, pcm_len, chn);
        if (cfg->algo_eq_enable)
            audio_iaa_eq_trans((short *)pcm, pcm_len, chn);
        if (cfg->algo_agc_enable)
            audio_iaa_agc_trans((short *)pcm, pcm_len, chn);
    }

    *out_data = pcm;
    *out_len = pcm_len;
    return 0;
}

int anj_mw_media_audio_init(AnjAudioConfig *pstAnjAudioCfg)
{
    if (gst_bAudioInit)
    {
        __ERR("audio already init!\n");
        return -1;
    }

    int iRet = 0;
    ST_Common_AudioAttr_t stAiAudioAttr;
    ST_Common_AudioAttr_t stAoAudioAttr;
    memset(&stAiAudioAttr, 0, sizeof(ST_Common_AudioAttr_t));
    memset(&stAoAudioAttr, 0, sizeof(ST_Common_AudioAttr_t));
    stAiAudioAttr.AiDevId = 0;
    stAiAudioAttr.AoDevId = 0;
    stAiAudioAttr.u8ChnGrpIdx = 0;
    stAiAudioAttr.u8ChnGrpId = 0;
    stAiAudioAttr.enFormat = E_MI_AUDIO_FORMAT_PCM_S16_LE;
    stAiAudioAttr.enSoundMode = (pstAnjAudioCfg->chn > 1) ? E_MI_AUDIO_SOUND_MODE_STEREO : E_MI_AUDIO_SOUND_MODE_MONO;
    stAiAudioAttr.enSampleRate = pstAnjAudioCfg->samplerate;
    stAiAudioAttr.u32PeriodSize = pstAnjAudioCfg->persize;
    stAiAudioAttr.bInterleaved = TRUE;
    stAiAudioAttr.enChannelMode = E_MI_AO_CHANNEL_MODE_DOUBLE_MONO;
    memcpy(&stAoAudioAttr, &stAiAudioAttr, sizeof(ST_Common_AudioAttr_t));
    stAoAudioAttr.enSampleRate = pstAnjAudioCfg->ao_samplerate;
    STCHECKRESULT(ST_Common_AudioAiInit(&stAiAudioAttr));
    STCHECKRESULT(ST_Common_AudioAoInit(&stAoAudioAttr));

    if (pstAnjAudioCfg->algo_aec_enable)
    {
        iRet = audio_iaa_aec_init(&stAiAudioAttr);
    }

    if (pstAnjAudioCfg->algo_anr_enable)
    {
        iRet = audio_iaa_anr_init(&stAiAudioAttr);
    }

    if (iRet == 0 && pstAnjAudioCfg->algo_agc_enable)
    {
        iRet = audio_iaa_agc_init(&stAiAudioAttr);
    }

    if (iRet == 0 && pstAnjAudioCfg->algo_eq_enable)
    {
        iRet = audio_iaa_eq_init(&stAiAudioAttr);
    }

    if (pstAnjAudioCfg->algo_bf_enable)
    {
        iRet = audio_iaa_bf_init(&stAiAudioAttr);
    }

    if (iRet == 0 && pstAnjAudioCfg->algo_aec_enable && anj_mw_media_audio_aec_provider_available())
    {
        frame_mgr_init(&s_stAoRefMgr, MW_AO_REF_QUEUE_MAX);
    }

    anj_mw_media_audio_ai_volume_set(pstAnjAudioCfg->ai_volume, pstAnjAudioCfg->ai_amplify);
    anj_mw_media_audio_ao_volume_set(pstAnjAudioCfg->ao_volume);

    s_stAudioAiThread.bAutoDestroy = 0;
    strncpy(s_stAudioAiThread.iThreadName, "aistream", sizeof(s_stAudioAiThread.iThreadName) - 1);
    s_stAudioAiThread.iThreadjob.ctx = pstAnjAudioCfg;
    s_stAudioAiThread.iThreadjob.func = anj_mw_media_audio_thread;
    STCHECKRESULT(anj_thread_task_create(&s_stAudioAiThread));

    gst_bAudioInit = 1;

    return 0;
}

int anj_mw_media_audio_uninit()
{
    if (gst_bAudioInit == 0)
    {
        __ERR("audio not init!\n");
        return -1;
    }
    anj_thread_task_destroy(&s_stAudioAiThread, 0);
    ST_Common_AudioAiUnInit();
    ST_Common_AudioAoUnInit();

    if (s_stAoRefMgr.max > 0)
        frame_mgr_release(&s_stAoRefMgr);

    audio_iaa_aec_uninit();
    audio_iaa_agc_uninit();
    audio_iaa_eq_uninit();
    audio_iaa_anr_uninit();
    audio_iaa_bf_uninit();

    gst_bAudioInit = 0;
    return 0;
}

int anj_mw_media_audio_ai_volume_set(int volume, int amplify)
{
    volume = anj_mw_check_value_in_range(volume, 0, 100);
    int ai_max_volume = ST_AUDIO_AI_MAX_VOLUME;
    if (amplify == 0) // 有源输入
    {
        ai_max_volume = ST_AUDIO_AI_MAX_VOLUME_AMPLIFY;
    }

    int tmpVolume = (volume * (ai_max_volume - ST_AUDIO_AI_MIN_VOLUME)) / 10;
    // 四舍五入
    int st_volume = ALIGN_FRONT(tmpVolume, 5) / 10;

    ST_Common_AudioAttr_t stAudioAttr;
    memset(&stAudioAttr, 0, sizeof(ST_Common_AudioAttr_t));
    stAudioAttr.AiDevId = 0;
    stAudioAttr.u8ChnGrpIdx = 0;
    stAudioAttr.u8ChnGrpId = 0;

    return ST_Common_AudioAi_SetVolume(&stAudioAttr, st_volume);
}

int anj_mw_media_audio_ao_volume_set(int volume)
{
    int iRet = 0;

    int ao_max_volume = ST_AUDIO_AO_MAX_VOLUME;
    volume = anj_mw_check_value_in_range(volume, 0, 100);
    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
        ao_max_volume = 30;

    int tmpVolume = (volume * (ao_max_volume - ST_AUDIO_AO_MIN_VOLUME)) / 10;
    // 四舍五入
    int st_volume = ALIGN_FRONT(tmpVolume, 5) / 10;
    st_volume = st_volume + ST_AUDIO_AO_MIN_VOLUME;
    st_volume = anj_mw_check_value_in_range(st_volume, ST_AUDIO_AO_MIN_VOLUME, ST_AUDIO_AO_MAX_VOLUME);

    if (st_volume == ST_AUDIO_AO_MIN_VOLUME)
    {
        iRet = ST_Common_AudioAo_SetMute(0, 1);
    }
    else
    {
        iRet = ST_Common_AudioAo_SetVolume(0, st_volume);
    }

    return iRet;
}

int anj_mw_media_audio_ai_mute_set(int enable)
{
    if (gst_bAudioInit == 0)
    {
        __ERR("audio not init!\n");
        return -1;
    }

    MI_AUDIO_DEV AiDevId = 0;
    MI_U8 u8ChnGrpId = 0;

    return ST_Common_AudioAi_SetMute(AiDevId, u8ChnGrpId, enable);
}

int anj_mw_media_audio_play(char *data, int len)
{
    if (s_stAoRefMgr.max > 0 && data && len > 0)
    {
        FRAME_ENTRY frame = {0};

        frame.pFrame = (char *)anj_mw_malloc(len);
        if (frame.pFrame)
        {
            memcpy(frame.pFrame, data, len);
            frame.nFrameLen = len;
            frame.nFrameType = 2;
            frame_mgr_push(&s_stAoRefMgr, &frame);
        }
    }

    return ST_Common_AudioAo_PcmPlay(data, len);
}

int anj_mw_media_audio_ao_play_check()
{
    return ST_Common_AudioAo_PlayEndingCheck();
}
