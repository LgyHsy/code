#include <stdlib.h>
#include <string.h>
#include "anj_mw_log.h"
#include "AuPcm2Pcm.h"

// 创建PCM到PCM编解码器
AuPcm2Pcm *AuPcm2Pcm_Create(void *user,
							unsigned int srcSampleRate,
							unsigned int srcChnNum,
							unsigned int desSampleRate,
							unsigned int desChnNum,
							int32_t samplesPerFrame,
							cb_out_data cb)
{
	int outBufLen = AUDIO_CODEC_PCM_PCM_OUT_BUFFER_LEN;
	AuPcm2Pcm *ctx = (AuPcm2Pcm *)malloc(sizeof(AuPcm2Pcm));
	if (!ctx)
	{
		__ERR("Memory allocation failed for AuPcm2Pcm\n");
		return NULL;
	}

	memset(ctx, 0, sizeof(AuPcm2Pcm));

	ctx->codec = AuCodec_Create(user, srcSampleRate, srcChnNum,
								desSampleRate, desChnNum,
								samplesPerFrame,
								outBufLen,
								cb);

	return ctx;
}

void AuPcm2Pcm_Destroy(AuPcm2Pcm *ctx)
{
	if (!ctx)
		return;

	if (ctx->codec)
	{
		AuCodec_Destroy(ctx->codec);
	}

	free(ctx);
}

int AuPcm2Pcm_Process(AuPcm2Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq)
{
	if (!ctx || !ctx->codec || frame_len <= 0)
	{
		return -1;
	}

	char *p = (char *)in_frame;
	int nDataLen = frame_len;

	// 重采样处理
    AuCodec *codec = ctx->codec;
    if (codec->base->resampler_handle != 0)
    {
        nDataLen = AuBase_Resample(codec->base, in_frame, frame_len,
                                   (uint8_t *)codec->resample_buffer,
                                   codec->resample_buffer_len);
        p = (char *)codec->resample_buffer;
    }

	char *pEncodeBuffer = AuBase_GetWritePos(codec->base, nDataLen);
	if (!pEncodeBuffer)
	{
		__ERR("No space in output buffer. Frame len: %d, Resampled len: %d\n",
			  frame_len, nDataLen);
		return -1;
	}

	memcpy(pEncodeBuffer, p, nDataLen);
	codec->base->out_datalen += nDataLen;

	AuBase_DataOutput(codec->base, timestamp, u32Seq);

	return 0;
}