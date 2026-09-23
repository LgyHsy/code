#ifndef _AU_PCM2AACLC_H
#define _AU_PCM2AACLC_H

#include "AuCodec.h"
#include "fdk-aac/aacdecoder_lib.h"
#include "fdk-aac/aacenc_lib.h"
#include "fdk-aac/FDK_audio.h"

// 结构体定义

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct AuPcm2Aaclc
{
	AuCodec *codec;
	HANDLE_AACENCODER aacEncHandle; // AAC编码器句柄
	unsigned int remain_pcmlen;		// 剩余PCM数据长度
} AuPcm2Aaclc;

AuPcm2Aaclc *AuPcm2Aaclc_Create(void *user,
								unsigned int srcSampleRate,
								unsigned int srcChnNum,
								unsigned int desSampleRate,
								unsigned int desChnNum,
								int32_t samplesPerFrame,
								cb_out_data cb);

void AuPcm2Aaclc_Destroy(AuPcm2Aaclc *ctx);
int AuPcm2Aaclc_Init(AuPcm2Aaclc *ctx);
int AuPcm2Aaclc_Process(AuPcm2Aaclc *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

#ifdef __cplusplus
}
#endif

#endif // SUPPORT_AAC
