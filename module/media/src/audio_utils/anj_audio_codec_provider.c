#include <stddef.h>

#include "anj_audio_codec_provider.h"

static const anj_audio_aac_codec_ops *s_pstAudioAacCodecOps = 0;

static int anj_audio_is_aac_codec_pair(media_codec_type_e srcType, media_codec_type_e desType)
{
    return (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_AAC) ||
           (srcType == MEDIA_CODEC_AUDIO_AAC && desType == MEDIA_CODEC_AUDIO_PCM);
}

int anj_audio_aac_codec_provider_register(const anj_audio_aac_codec_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstAudioAacCodecOps == 0 ||
        ops->provider_priority >= s_pstAudioAacCodecOps->provider_priority)
    {
        s_pstAudioAacCodecOps = ops;
    }

    return 0;
}

void anj_audio_aac_codec_provider_unregister(const anj_audio_aac_codec_ops *ops)
{
    if (s_pstAudioAacCodecOps == ops)
    {
        s_pstAudioAacCodecOps = 0;
    }
}

int anj_audio_aac_codec_provider_available(void)
{
    return (s_pstAudioAacCodecOps != 0) ? 1 : 0;
}

void anj_audio_aac_codec_provider_destroy(void *handle, media_codec_type_e srcType, media_codec_type_e desType)
{
    if (!anj_audio_is_aac_codec_pair(srcType, desType))
    {
        return;
    }

    if (s_pstAudioAacCodecOps && s_pstAudioAacCodecOps->destroy)
    {
        s_pstAudioAacCodecOps->destroy(handle, srcType, desType);
    }
}

void *anj_audio_aac_codec_provider_create(void *pUser, media_codec_type_e srcType,
                                          AU_SampleRate_e srcSampleRate,
                                          AudioChannel_e srcChnNum,
                                          media_codec_type_e desType,
                                          AU_SampleRate_e desSampleRate,
                                          AudioChannel_e desChnNum,
                                          int32_t samplesPerFrame,
                                          cb_out_data cb)
{
    if (!anj_audio_is_aac_codec_pair(srcType, desType))
    {
        return NULL;
    }

    if (s_pstAudioAacCodecOps && s_pstAudioAacCodecOps->create)
    {
        return s_pstAudioAacCodecOps->create(pUser, srcType, srcSampleRate, srcChnNum,
                                             desType, desSampleRate, desChnNum,
                                             samplesPerFrame, cb);
    }

    return NULL;
}

int32_t anj_audio_aac_codec_provider_codec(void *handle, media_codec_type_e srcType, media_codec_type_e desType,
                                             uint8_t *in_frame, int32_t frame_len,
                                             unsigned long long int timestamp, unsigned int u32Seq)
{
    if (!anj_audio_is_aac_codec_pair(srcType, desType))
    {
        return -1;
    }

    if (s_pstAudioAacCodecOps && s_pstAudioAacCodecOps->codec)
    {
        return s_pstAudioAacCodecOps->codec(handle, srcType, desType, in_frame, frame_len, timestamp, u32Seq);
    }

    return -1;
}
