#ifndef _ANJ_MW_MEDIA_AUDIO_H_
#define _ANJ_MW_MEDIA_AUDIO_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*anj_mw_media_audio_pcm_data)(char *data, int len, unsigned long long timestamp, unsigned int seq);

typedef struct
{
    int encodeType;
    int samplerate;
    int ao_samplerate;
    int chn;
    int persize;
    int bitwidth;
    int bitrate;
    int ai_volume;
    int ai_amplify;
    int ao_volume;
    int aec_enable;
    int aenc_enable;

    int algo_aec_enable;
    int algo_anr_enable;
    int algo_agc_enable;
    int algo_eq_enable;
    int algo_bf_enable;
    int wave_active;

    anj_mw_media_audio_pcm_data pcm_data_cb;
} AnjAudioConfig;

/*
 * 返回 AI 实际采集 PCM 采样率。
 * mstar：固定 16K，编码链路再重采样。
 * ts：与 config samplerate 一致。
 */
int anj_mw_media_audio_ai_capture_rate(int config_samplerate);

int anj_mw_media_audio_init(AnjAudioConfig *pstAnjAudioCfg);
int anj_mw_media_audio_uninit(void);
int anj_mw_media_audio_ai_volume_set(int volume, int amplify);
int anj_mw_media_audio_ao_volume_set(int volume);
int anj_mw_media_audio_play(char *data, int len);
int anj_mw_media_audio_ai_mute_set(int enable);
int anj_mw_media_audio_ao_play_check(void);

/*
 * AI PCM 用户态处理（BF/AEC/ANR/EQ/AGC）。
 * mstar：在用户态完成；wave_active 为 1 时跳过 ANR/EQ/AGC。
 * ts 等：VQE 已在驱动内完成，直接透传输入 PCM。
 * 返回 0 表示成功，out_data/out_len 指向编码用 PCM。
 */
int anj_mw_media_audio_ai_pcm_process(char *data, int len, int chn, const AnjAudioConfig *cfg,
                                      char **out_data, int *out_len);

#ifdef __cplusplus
}
#endif

#endif
