#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "AuAaclc2Pcm.h"
#include "anj_mw_log.h"

AuAaclc2Pcm *AuAaclc2Pcm_Create(void *user,
								AU_SampleRate_e srcSampleRate,
								AudioChannel_e srcChnNum,
								AU_SampleRate_e desSampleRate,
								AudioChannel_e desChnNum,
								int32_t samplesPerFrame,
								cb_out_data cb)
{
	int outBufLen = AUDIO_AACLCPCM_OUT_BUFFER_LEN;
	AuAaclc2Pcm *ctx = (AuAaclc2Pcm *)malloc(sizeof(AuAaclc2Pcm));
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(AuAaclc2Pcm));

	ctx->codec = AuCodec_Create(user, srcSampleRate, srcChnNum,
								desSampleRate, desChnNum,
								samplesPerFrame,
								outBufLen,
								cb);

	ctx->inBufSize = 10240;
	ctx->inBufValid = 0;
	ctx->aacDecHandle = NULL;
	ctx->inBuf = NULL;
	if (0 != AuAaclc2Pcm_Init(ctx))
	{
		AuAaclc2Pcm_Destroy(ctx);
		return NULL;
	}

	return ctx;
}

void AuAaclc2Pcm_Destroy(AuAaclc2Pcm *ctx)
{
	if (!ctx)
		return;

	// 关闭AAC解码器
	if (ctx->aacDecHandle)
	{
		aacDecoder_Close(ctx->aacDecHandle);
		ctx->aacDecHandle = NULL;
	}

	if (ctx->inBuf)
	{
		free(ctx->inBuf);
		ctx->inBuf = NULL;
	}

	if (ctx->codec)
	{
		AuCodec_Destroy(ctx->codec);
		ctx->codec = NULL;
	}

	free(ctx);
}

int AuAaclc2Pcm_Init(AuAaclc2Pcm *ctx)
{
	if (!ctx)
		return -1;


	ctx->inBuf = (uint8_t *)malloc(ctx->inBufSize);
	if (!ctx->inBuf)
	{
		__ERR("Failed to allocate input buffer\n");
		return -1;
	}

	// 打开AAC解码器
	ctx->aacDecHandle = aacDecoder_Open(TT_MP4_ADTS, 1);
	if (!ctx->aacDecHandle)
	{
		__ERR("aacDecoder_Open failed\n");
		free(ctx->inBuf);
		ctx->inBuf = NULL;
		return -1;
	}

	return 0;
}

void *AuAaclc2Pcm_GetAacFrame(AuAaclc2Pcm *ctx, int *framelen)
{
	if (!ctx || !ctx->inBuf || ctx->inBufValid < ADTS_HEADER_LEN)
	{
		*framelen = 0;
		return NULL;
	}

	size_t size = 0;
	int offset = 0;
	unsigned char *buffer = ctx->inBuf;

	while (offset <= (int)(ctx->inBufValid - ADTS_HEADER_LEN))
	{
		if ((buffer[0] == 0xff) && ((buffer[1] & 0xf0) == 0xf0))
		{
			// 解析ADTS头获取帧长度
			size = ((buffer[3] & 0x03) << 11) |
				   (buffer[4] << 3) |
				   ((buffer[5] & 0xe0) >> 5);
			break;
		}
		offset++;
		buffer++;
	}

	if (size == 0 || size > ctx->inBufValid - offset)
	{
		*framelen = 0;
		return NULL;
	}

	*framelen = size;
	return buffer;
}

void AuAaclc2Pcm_MoveData(AuAaclc2Pcm *ctx, void *p, unsigned int nAacFrameLen)
{
	if (!ctx || !p)
		return;

	if ((uint8_t *)p + nAacFrameLen > ctx->inBuf + ctx->inBufValid)
	{
		__ERR("Invalid AAC frame length: %u > %u\n", nAacFrameLen, ctx->inBufValid);
		ctx->inBufValid = 0;
		return;
	}

	// 计算剩余数据长度
	int nRemainLen = ctx->inBufValid - (nAacFrameLen + ((uint8_t *)p - ctx->inBuf));
	if (nRemainLen > 0)
	{
		// 移动剩余数据到缓冲区头部
		memmove(ctx->inBuf, (uint8_t *)p + nAacFrameLen, nRemainLen);
		ctx->inBufValid = nRemainLen;
	}
	else
	{
		ctx->inBufValid = 0;
	}
}

int AuAaclc2Pcm_Process(AuAaclc2Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq)
{
	if (!ctx || frame_len <= 0)
	{
		__WARN("Invalid parameters");
		return 0;
	}

	if (frame_len > (int)ctx->inBufSize)
	{
		__ERR("Frame too large: %d > %d\n", frame_len, ctx->inBufSize);
		return 0;
	}

	if (ctx->inBufValid + frame_len > ctx->inBufSize)
	{
		__WARN("Input buffer overflow, resetting: %u + %d > %d\n",
			   ctx->inBufValid, frame_len, ctx->inBufSize);
		ctx->inBufValid = 0;
	}

	memcpy(ctx->inBuf + ctx->inBufValid, in_frame, frame_len);
	ctx->inBufValid += frame_len;

	while (1)
	{
		int nAacFrameLen = 0;
		uint8_t *aacFrame = AuAaclc2Pcm_GetAacFrame(ctx, &nAacFrameLen);
		if (!aacFrame)
			break;

		// 计算重采样后需要的输出大小
		int nNeedBytes = (int)((float)nAacFrameLen * ctx->codec->resample_rate * 4); //PCM <-- aac 不确定AAC->PCM会有多少字节，按16倍来预估
		char *outBuf = AuBase_GetWritePos(ctx->codec->base, nNeedBytes);
		if (!outBuf)
		{
			__WARN("No space in output buffer, skipping frame");
			AuAaclc2Pcm_MoveData(ctx, aacFrame, nAacFrameLen);
			continue;
		}

		// 填充AAC数据到解码器
		UINT valid = nAacFrameLen;
		uint8_t *packet_ptr = aacFrame;
		AAC_DECODER_ERROR err = aacDecoder_Fill(ctx->aacDecHandle, &packet_ptr, (const UINT *)&nAacFrameLen, &valid);
		if (err != AAC_DEC_OK)
		{
			__ERR("aacDecoder_Fill failed: 0x%x\n", err);
			AuAaclc2Pcm_MoveData(ctx, aacFrame, nAacFrameLen);
			continue;
		}

		// 解码PCM数据
		int16_t *pcmBuffer = NULL;
		INT pcmBufferSize = 0;

		// 根据是否使用重采样决定输出位置
		if (ctx->codec->resampler_handle == 0)
		{
			// 直接输出到最终缓冲区
			pcmBuffer = (int16_t *)outBuf;
			pcmBufferSize = (ctx->codec->base->out_databuffer_len - (outBuf - ctx->codec->base->out_databuffer)) / sizeof(int16_t);
		}
		else
		{
			// 输出到重采样中间缓冲区
			pcmBuffer = (int16_t *)ctx->codec->resample_buffer;
			pcmBufferSize = ctx->codec->resample_buffer_len / sizeof(int16_t);
		}

		// 解码帧
		err = aacDecoder_DecodeFrame(ctx->aacDecHandle, pcmBuffer, pcmBufferSize, 0);
		if (err != AAC_DEC_OK)
		{
			__ERR("aacDecoder_DecodeFrame failed: 0x%x\n", err);
			AuAaclc2Pcm_MoveData(ctx, aacFrame, nAacFrameLen);
			continue;
		}

		// 获取解码信息
		CStreamInfo *decInfo = aacDecoder_GetStreamInfo(ctx->aacDecHandle);
		if (!decInfo || decInfo->sampleRate <= 0)
		{
			__ERR("Invalid stream info\n");
			AuAaclc2Pcm_MoveData(ctx, aacFrame, nAacFrameLen);
			continue;
		}

		// 计算解码后的PCM数据长度
		int32_t pcmDataLen = decInfo->frameSize * decInfo->numChannels * sizeof(int16_t);

		// 处理重采样
		if (ctx->codec->resampler_handle == 0)
		{
			// 直接输出PCM数据
			AuBase_PutOutputData(ctx->codec->base, (char *)pcmBuffer, pcmDataLen, timestamp, u32Seq);
		}
		else
		{
			// 执行重采样
			int resampledLen = AuBase_Resample(ctx->codec->base,
											   (uint8_t *)pcmBuffer,
											   pcmDataLen / (decInfo->numChannels * sizeof(int16_t)),
											   (uint8_t *)outBuf,
											   nNeedBytes);
			if (resampledLen > 0)
			{
				AuBase_PutOutputData(ctx->codec->base, outBuf, resampledLen, timestamp, u32Seq);
			}
		}

		// 移动缓冲区数据
		AuAaclc2Pcm_MoveData(ctx, aacFrame, nAacFrameLen);
	}

	// 输出处理后的数据
	AuBase_DataOutput(ctx->codec->base, timestamp, u32Seq);
	return 0;
}
