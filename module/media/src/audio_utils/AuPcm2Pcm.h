#ifndef _AU_PCM2PCM_H_
#define _AU_PCM2PCM_H_

#include "AuCodec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuPcm2Pcm {
    AuCodec *codec;  // 基类编解码器
} AuPcm2Pcm;

AuPcm2Pcm *AuPcm2Pcm_Create(void *user, 
                            unsigned int srcSampleRate, 
                            unsigned int srcChnNum,
                            unsigned int desSampleRate, 
                            unsigned int desChnNum,
                            int32_t samplesPerFrame, 
                            cb_out_data cb);

void AuPcm2Pcm_Destroy(AuPcm2Pcm *ctx);

int AuPcm2Pcm_Process(AuPcm2Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

#ifdef __cplusplus
}
#endif

#endif // _AU_PCM2PCM_H_