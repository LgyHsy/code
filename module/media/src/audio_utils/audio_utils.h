#ifndef _AUDIO_UTILS_H
#define _AUDIO_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "AuBase.h"
#include "media_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

void *AU_Create(void *pUser, media_codec_type_e srcType,
                AU_SampleRate_e srcSampleRate,
                AudioChannel_e srcChnNum,
                media_codec_type_e desType,
                AU_SampleRate_e desSampleRate,
                AudioChannel_e desChnNum,
                int32_t samplesPerFrame,
                cb_out_data cb);

int32_t AU_Codec(void *handle, media_codec_type_e srcType, media_codec_type_e desType,
                    uint8_t *in_frame, int32_t frame_len,
                    unsigned long long int timestamp, unsigned int u32Seq);

void AU_Destroy(void *handle, media_codec_type_e srcType, media_codec_type_e desType);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_UTILS_H