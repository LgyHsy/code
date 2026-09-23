#ifndef _AU_G711A2PCM_H_
#define _AU_G711A2PCM_H_

#include "AuCodec.h"
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	AuCodec *codec;
	int out_databuffer_len;
} AuG711a2Pcm;

int32_t AuG711a2Pcm_Process(AuG711a2Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

AuG711a2Pcm *AuG711a2Pcm_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
								AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
								int32_t samplesPerFrame, cb_out_data cb);
								
void AuG711a2Pcm_Destroy(AuG711a2Pcm *ctx);

#ifdef __cplusplus
}
#endif
#endif


