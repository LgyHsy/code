#ifndef __ANJ_AUDIO_CODEC_PROVIDER_H__
#define __ANJ_AUDIO_CODEC_PROVIDER_H__

#include "AuBase.h"
#include "media_util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *provider_name;
    int provider_priority;
    void (*destroy)(void *handle, media_codec_type_e srcType, media_codec_type_e desType);
    void *(*create)(void *pUser, media_codec_type_e srcType,
                    AU_SampleRate_e srcSampleRate,
                    AudioChannel_e srcChnNum,
                    media_codec_type_e desType,
                    AU_SampleRate_e desSampleRate,
                    AudioChannel_e desChnNum,
                    int32_t samplesPerFrame,
                    cb_out_data cb);
    int32_t (*codec)(void *handle, media_codec_type_e srcType, media_codec_type_e desType,
                     uint8_t *in_frame, int32_t frame_len,
                     unsigned long long int timestamp, unsigned int u32Seq);
} anj_audio_aac_codec_ops;

int anj_audio_aac_codec_provider_register(const anj_audio_aac_codec_ops *ops);
void anj_audio_aac_codec_provider_unregister(const anj_audio_aac_codec_ops *ops);

int anj_audio_aac_codec_provider_available(void);

void anj_audio_aac_codec_provider_destroy(void *handle, media_codec_type_e srcType, media_codec_type_e desType);
void *anj_audio_aac_codec_provider_create(void *pUser, media_codec_type_e srcType,
                                          AU_SampleRate_e srcSampleRate,
                                          AudioChannel_e srcChnNum,
                                          media_codec_type_e desType,
                                          AU_SampleRate_e desSampleRate,
                                          AudioChannel_e desChnNum,
                                          int32_t samplesPerFrame,
                                          cb_out_data cb);
int32_t anj_audio_aac_codec_provider_codec(void *handle, media_codec_type_e srcType, media_codec_type_e desType,
                                           uint8_t *in_frame, int32_t frame_len,
                                           unsigned long long int timestamp, unsigned int u32Seq);

#ifdef __cplusplus
}
#endif

#endif
