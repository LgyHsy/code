#ifndef _AU_MP32PCM_H_
#define _AU_MP32PCM_H_

#include "AuCodec.h"
#include "g7xx.h"
#include "lame.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_MP3_FRAME_SAMPLES_NUM 1152 * 2 // most time 576
#define MAX_MP3_FRAME_SIZE (MAX_MP3_FRAME_SAMPLES_NUM << 1)
#define MAX_OUT_BUF_SIZE (MAX_MP3_FRAME_SAMPLES_NUM << 1)

	typedef struct AuMp32Pcm
	{
		AuCodec *codec;
		short out_lbuf[MAX_MP3_FRAME_SAMPLES_NUM];
		short out_rbuf[MAX_MP3_FRAME_SAMPLES_NUM];
		short out_buf[MAX_OUT_BUF_SIZE];
		hip_global_flags *hip;
		mp3data_struct mp3data;
	} AuMp32Pcm;

	AuMp32Pcm *AuMp32Pcm_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
								AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
								int32_t samplesPerFrame, cb_out_data cb);

	void AuMp32Pcm_Destroy(AuMp32Pcm *ctx);

	int32_t AuMp32Pcm_Process(AuMp32Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

#ifdef __cplusplus
}
#endif

#endif /* _AU_MP32PCM_H_ */