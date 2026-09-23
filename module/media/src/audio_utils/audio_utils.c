#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "anj_mw_log.h"
#include "audio_utils.h"
#include "anj_audio_codec_provider.h"
#include "AuPcm2G711u.h"
#include "AuPcm2G711a.h"
#include "AuPcm2Pcm.h"
#include "AuG711u2Pcm.h"
#include "AuG711a2Pcm.h"
#include "AuMp32Pcm.h"

static int anj_audio_is_aac_codec_pair(media_codec_type_e srcType, media_codec_type_e desType)
{
    return (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_AAC) ||
           (srcType == MEDIA_CODEC_AUDIO_AAC && desType == MEDIA_CODEC_AUDIO_PCM);
}

void AU_Destroy(void *handle, media_codec_type_e srcType, media_codec_type_e desType)
{
    if (!handle)
        return;

    if (anj_audio_is_aac_codec_pair(srcType, desType))
    {
        anj_audio_aac_codec_provider_destroy(handle, srcType, desType);
        return;
    }

    if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_G711U)
    {
        AuPcm2G711u_Destroy((AuPcm2G711u *)handle);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_G711U && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        AuG711u2Pcm_Destroy((AuG711u2Pcm *)handle);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_G711A)
    {
        AuPcm2G711a_Destroy((AuPcm2G711a *)handle);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_G711A && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        AuG711a2Pcm_Destroy((AuG711a2Pcm *)handle);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        AuPcm2Pcm_Destroy((AuPcm2Pcm *)handle);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_MP3 && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        AuMp32Pcm_Destroy((AuMp32Pcm *)handle);
    }
    else
    {
        __ERR("Unsupported codec conversion: %d to %d\n", srcType, desType);
    }
}

void *AU_Create(void *pUser, media_codec_type_e srcType,
                AU_SampleRate_e srcSampleRate,
                AudioChannel_e srcChnNum,
                media_codec_type_e desType,
                AU_SampleRate_e desSampleRate,
                AudioChannel_e desChnNum,
                int32_t samplesPerFrame,
                cb_out_data cb)
{
    void *handle = NULL;

    __INFO("Creating: %d->%d\n", srcType, desType);

    if (anj_audio_is_aac_codec_pair(srcType, desType))
    {
        handle = anj_audio_aac_codec_provider_create(pUser, srcType, srcSampleRate, srcChnNum,
                                                     desType, desSampleRate, desChnNum,
                                                     samplesPerFrame, cb);
        if (handle == NULL)
        {
            __ERR("AAC codec provider unavailable: %d to %d\n", srcType, desType);
        }
        return handle;
    }

    if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_G711U)
    {
        return AuPcm2G711u_Create(pUser, srcSampleRate, srcChnNum,
                                  desSampleRate, desChnNum,
                                  samplesPerFrame, cb);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_G711U && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuG711u2Pcm_Create(pUser, srcSampleRate, srcChnNum,
                                  desSampleRate, desChnNum,
                                  samplesPerFrame, cb);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_G711A)
    {
        return AuPcm2G711a_Create(pUser, srcSampleRate, srcChnNum,
                                  desSampleRate, desChnNum,
                                  samplesPerFrame, cb);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_G711A && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuG711a2Pcm_Create(pUser, srcSampleRate, srcChnNum,
                                  desSampleRate, desChnNum,
                                  samplesPerFrame, cb);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuPcm2Pcm_Create(pUser, srcSampleRate, srcChnNum,
                                desSampleRate, desChnNum,
                                samplesPerFrame, cb);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_MP3 && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuMp32Pcm_Create(pUser, srcSampleRate, srcChnNum,
                                desSampleRate, desChnNum,
                                samplesPerFrame, cb);
    }
    else
    {
        __ERR("Unsupported codec conversion: %d to %d\n", srcType, desType);
        return NULL;
    }

    return NULL;
}

int32_t AU_Codec(void *handle, media_codec_type_e srcType, media_codec_type_e desType,
                 uint8_t *in_frame, int32_t frame_len,
                 unsigned long long int timestamp, unsigned int u32Seq)
{
    if (!handle)
        return 0;

    if (anj_audio_is_aac_codec_pair(srcType, desType))
    {
        return anj_audio_aac_codec_provider_codec(handle, srcType, desType,
                                                  in_frame, frame_len, timestamp, u32Seq);
    }

    if (desType == MEDIA_CODEC_AUDIO_G711U)
    {
        return AuPcm2G711u_Process((AuPcm2G711u *)handle, in_frame, frame_len, timestamp, u32Seq);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_G711U && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuG711u2Pcm_Process((AuG711u2Pcm *)handle, in_frame, frame_len, timestamp, u32Seq);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_G711A)
    {
        return AuPcm2G711a_Process((AuPcm2G711a *)handle, in_frame, frame_len, timestamp, u32Seq);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_G711A && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuG711a2Pcm_Process((AuG711a2Pcm *)handle, in_frame, frame_len, timestamp, u32Seq);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_PCM && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuPcm2Pcm_Process((AuPcm2Pcm *)handle, in_frame, frame_len, timestamp, u32Seq);
    }
    else if (srcType == MEDIA_CODEC_AUDIO_MP3 && desType == MEDIA_CODEC_AUDIO_PCM)
    {
        return AuMp32Pcm_Process((AuMp32Pcm *)handle, in_frame, frame_len, timestamp, u32Seq);
    }
    else
    {
        __ERR("Unsupported codec conversion: %d to %d\n", srcType, desType);
        return -1;
    }
    return 0;
}
