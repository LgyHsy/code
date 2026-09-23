#ifndef __ANJ_MW_MEDIA_AUDIO_AEC_PROVIDER_H__
#define __ANJ_MW_MEDIA_AUDIO_AEC_PROVIDER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *provider_name;
    int provider_priority;
    int (*init)(void *pstAudioAttr);
    int (*uninit)(void);
    int (*trans)(char *data_near, char *data_far, int len, int audio_chn);
} anj_mw_media_audio_aec_ops;

int anj_mw_media_audio_aec_provider_register(const anj_mw_media_audio_aec_ops *ops);
void anj_mw_media_audio_aec_provider_unregister(const anj_mw_media_audio_aec_ops *ops);

int anj_mw_media_audio_aec_provider_available(void);
int anj_mw_media_audio_aec_provider_init(void *pstAudioAttr);
int anj_mw_media_audio_aec_provider_uninit(void);
int anj_mw_media_audio_aec_provider_trans(char *data_near, char *data_far, int len, int audio_chn);

#ifdef __cplusplus
}
#endif

#endif
