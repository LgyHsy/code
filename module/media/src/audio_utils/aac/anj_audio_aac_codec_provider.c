#include "anj_mw_comm.h"
#include "anj_audio_codec_provider.h"
#include "AuPcm2Aaclc.h"
#include "AuAaclc2Pcm.h"

static void anj_audio_aac_codec_destroy(void *handle, media_codec_type_e srcType, media_codec_type_e desType)
{
    if (!handle)
    {
        return;
    }

    if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_AAC)
    {
        AuPcm2Aaclc_Destroy((AuPcm2Aaclc *)handle);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_AAC && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        AuAaclc2Pcm_Destroy((AuAaclc2Pcm *)handle);
    }
}

static void *anj_audio_aac_codec_create(void *pUser, media_codec_type_e srcType,
                                        AU_SampleRate_e srcSampleRate,
                                        AudioChannel_e srcChnNum,
                                        media_codec_type_e desType,
                                        AU_SampleRate_e desSampleRate,
                                        AudioChannel_e desChnNum,
                                        int32_t samplesPerFrame,
                                        cb_out_data cb)
{
    if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_AAC)
    {
        return AuPcm2Aaclc_Create(pUser, srcSampleRate, srcChnNum,
                                  desSampleRate, desChnNum,
                                  samplesPerFrame, cb);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_AAC && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuAaclc2Pcm_Create(pUser, srcSampleRate, srcChnNum,
                                  desSampleRate, desChnNum,
                                  samplesPerFrame, cb);
    }

    return NULL;
}

static int32_t anj_audio_aac_codec_process(void *handle, media_codec_type_e srcType, media_codec_type_e desType,
                                           uint8_t *in_frame, int32_t frame_len,
                                           unsigned long long int timestamp, unsigned int u32Seq)
{
    if (!handle)
    {
        return 0;
    }

    if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_AAC)
    {
        return AuPcm2Aaclc_Process((AuPcm2Aaclc *)handle, in_frame, frame_len, timestamp, u32Seq);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_AAC && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuAaclc2Pcm_Process((AuAaclc2Pcm *)handle, in_frame, frame_len, timestamp, u32Seq);
    }

    return -1;
}

static const anj_audio_aac_codec_ops s_stAudioAacCodecOps = {
    .provider_name = "aac",
    .provider_priority = 100,
    .destroy = anj_audio_aac_codec_destroy,
    .create = anj_audio_aac_codec_create,
    .codec = anj_audio_aac_codec_process,
};

ANJ_LINK_KEEP(anj_keep_audio_aac_codec_provider);

__attribute__((constructor)) static void anj_audio_aac_codec_provider_register_constructor(void)
{
    anj_audio_aac_codec_provider_register(&s_stAudioAacCodecOps);
}

__attribute__((destructor)) static void anj_audio_aac_codec_provider_unregister_constructor(void)
{
    anj_audio_aac_codec_provider_unregister(&s_stAudioAacCodecOps);
}
