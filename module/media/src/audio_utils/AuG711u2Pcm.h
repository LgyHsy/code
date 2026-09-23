#ifndef _AU_G711U2PCM_H_
#define _AU_G711U2PCM_H_

#include "AuCodec.h"
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	AuCodec *codec;
} AuG711u2Pcm;

int32_t AuG711u2Pcm_Process(AuG711u2Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

AuG711u2Pcm *AuG711u2Pcm_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
								AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
								int32_t samplesPerFrame, cb_out_data cb);
								
void AuG711u2Pcm_Destroy(AuG711u2Pcm *ctx);

#ifdef __cplusplus
}
#endif
#endif
// class CAuG711u2Pcm : public CAuCodec
// {
// public:
// 	CAuG711u2Pcm(void *pUser, 
// 		unsigned int srcSampleRate, unsigned int srcChnNum,
// 		unsigned int desSampleRate,	unsigned int desChnNum,
// 		int32_t samplesPerFrame, cb_out_data cb);
// 	virtual	~CAuG711u2Pcm();	


// public:
// 	virtual int Init();
// 	virtual int ProcCodec(uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq) ;	


// private:	

// };

// #endif


