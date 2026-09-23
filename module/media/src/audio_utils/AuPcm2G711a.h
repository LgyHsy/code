#ifndef _AU_PCM2G711A_H_
#define _AU_PCM2G711A_H_

#include "AuCodec.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	AuCodec *codec;
} AuPcm2G711a;

int32_t AuPcm2G711a_Process(AuPcm2G711a *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

AuPcm2G711a *AuPcm2G711a_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
								AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
								int32_t samplesPerFrame, cb_out_data cb);
								
void AuPcm2G711a_Destroy(AuPcm2G711a *ctx);

#ifdef __cplusplus
}
#endif
#endif
