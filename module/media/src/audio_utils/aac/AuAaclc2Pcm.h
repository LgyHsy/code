#ifndef _AU_AACLC2PCM_H_
#define _AU_AACLC2PCM_H_

#include "AuCodec.h"
#include "fdk-aac/aacdecoder_lib.h"
#include "fdk-aac/aacenc_lib.h"
#include "fdk-aac/FDK_audio.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define AUDIO_AACLCPCM_OUT_BUFFER_LEN 20480
#define MAX_AAC_PACKET_SIZE 8192
#define ADTS_HEADER_LEN 7

typedef struct AuAaclc2Pcm
{
	AuCodec *codec;
	HANDLE_AACDECODER aacDecHandle;
	unsigned int inBufSize;
	unsigned int inBufValid;
	unsigned char *inBuf;
} AuAaclc2Pcm;

AuAaclc2Pcm *AuAaclc2Pcm_Create(void *user,
								AU_SampleRate_e srcSampleRate,
								AudioChannel_e srcChnNum,
								AU_SampleRate_e desSampleRate,
								AudioChannel_e desChnNum,
								int32_t samplesPerFrame,
								cb_out_data cb);

void AuAaclc2Pcm_Destroy(AuAaclc2Pcm *ctx);

int AuAaclc2Pcm_Init(AuAaclc2Pcm *ctx);

int AuAaclc2Pcm_Process(AuAaclc2Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

void *AuAaclc2Pcm_GetAacFrame(AuAaclc2Pcm *ctx, int *framelen);
void AuAaclc2Pcm_MoveData(AuAaclc2Pcm *ctx, void *p, unsigned int nAacFrameLen);

#ifdef __cplusplus
}
#endif

#endif