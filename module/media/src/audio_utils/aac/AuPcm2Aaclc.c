#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "AuPcm2Aaclc.h"
#include "anj_mw_log.h"

#define AUDIO_CODEC_BITWIDTH 16

AuPcm2Aaclc *AuPcm2Aaclc_Create(void *user,
								unsigned int srcSampleRate,
								unsigned int srcChnNum,
								unsigned int desSampleRate,
								unsigned int desChnNum,
								int32_t samplesPerFrame,
								cb_out_data cb)
{
	int outBufLen = AUDIO_CODEC_OUT_BUFFER_LEN;
	AuPcm2Aaclc *ctx = (AuPcm2Aaclc *)malloc(sizeof(AuPcm2Aaclc));
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(AuPcm2Aaclc));

	ctx->codec = AuCodec_Create(user, srcSampleRate, srcChnNum,
								desSampleRate, desChnNum,
								samplesPerFrame, outBufLen, cb);

	ctx->aacEncHandle = NULL;
	ctx->remain_pcmlen = 0;

	if (0 != AuPcm2Aaclc_Init(ctx))
	{
		AuPcm2Aaclc_Destroy(ctx);
		return NULL;
	}

	/* AAC 帧对齐与尾部 PCM 依赖缓存；无重采样时 AuCodec_Init 不会分配 resample_buffer。 */
	if (ctx->codec->resample_buffer == NULL)
	{
		if (ctx->codec->resample_buffer_len == 0)
		{
			__ERR("AuPcm2Aaclc: resample_buffer_len is 0\n");
			AuPcm2Aaclc_Destroy(ctx);
			return NULL;
		}
		ctx->codec->resample_buffer = (char *)malloc(ctx->codec->resample_buffer_len);
		if (!ctx->codec->resample_buffer)
		{
			__ERR("AuPcm2Aaclc: malloc resample_buffer %u failed\n", ctx->codec->resample_buffer_len);
			AuPcm2Aaclc_Destroy(ctx);
			return NULL;
		}
	}

	return ctx;
}

void AuPcm2Aaclc_Destroy(AuPcm2Aaclc *ctx)
{
	if (!ctx)
		return;

	if (ctx->aacEncHandle)
	{
		aacEncClose(&ctx->aacEncHandle);
	}

	AuCodec_Destroy(ctx->codec);
	free(ctx);
}

int AuPcm2Aaclc_Init(AuPcm2Aaclc *ctx)
{
	if (!ctx)
		return -1;

	ctx->codec->base->out_bytes_per_frame = 0;

	CHANNEL_MODE mode;
	switch (ctx->codec->base->out_chnNum)
	{
	case AudioChannel_Mono:
		mode = MODE_1;
		break;
	case AudioChannel_Stereo:
		mode = MODE_2;
		break;
	default:
		mode = MODE_1;
	}

	if (aacEncOpen(&ctx->aacEncHandle, 0, ctx->codec->base->out_chnNum) != AACENC_OK)
	{
		return -1;
	}

	// 设置编码器参数
	AACENC_ERROR err;
	if ((err = aacEncoder_SetParam(ctx->aacEncHandle, AACENC_AOT, 2)) != AACENC_OK ||
		(err = aacEncoder_SetParam(ctx->aacEncHandle, AACENC_SAMPLERATE, ctx->codec->base->out_sampleRate)) != AACENC_OK ||
		(err = aacEncoder_SetParam(ctx->aacEncHandle, AACENC_CHANNELMODE, mode)) != AACENC_OK ||
		(err = aacEncoder_SetParam(ctx->aacEncHandle, AACENC_CHANNELORDER, 1)) != AACENC_OK ||
		(err = aacEncoder_SetParam(ctx->aacEncHandle, AACENC_BITRATEMODE, 1)) != AACENC_OK ||
		(err = aacEncoder_SetParam(ctx->aacEncHandle, AACENC_TRANSMUX, 2)) != AACENC_OK ||
		(err = aacEncoder_SetParam(ctx->aacEncHandle, AACENC_AFTERBURNER, 1)) != AACENC_OK)
	{
		__ERR("AAC encoder config failed: %d\n", err);
		return -1;
	}

	// 初始化编码器
	if (aacEncEncode(ctx->aacEncHandle, NULL, NULL, NULL, NULL) != AACENC_OK)
	{
		__ERR("Unable to initialize AAC encoder\n");
		return -1;
	}

	// 获取编码器信息
	AACENC_InfoStruct info = {0};
	if (aacEncInfo(ctx->aacEncHandle, &info) != AACENC_OK)
	{
		__ERR("Unable to get AAC encoder info\n");
		return -1;
	}

	return 0;
}

int AuPcm2Aaclc_Process(AuPcm2Aaclc *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq)
{
	if (!ctx || frame_len <= 0)
		return 0;

	char *p = (char *)in_frame;
	int nDataLen = frame_len;
	AuBase *base = ctx->codec->base;

	/* Step1: 累计待编码PCM。若需要重采样，先写入resample_buffer；否则按原始PCM累计。 */
	if (ctx->codec->resampler_handle != 0)
	{
		if (!ctx->codec->resample_buffer || ctx->codec->resample_buffer_len == 0)
		{
			__ERR("Resample buffer invalid: ptr=%p len=%u\n", ctx->codec->resample_buffer, ctx->codec->resample_buffer_len);
			ctx->remain_pcmlen = 0;
			return 0;
		}

		/* remain 已异常时先清零，避免本次写入越界。 */
		if (ctx->remain_pcmlen >= ctx->codec->resample_buffer_len)
		{
			__WARN("remain pcm overflow before resample: remain=%u buf_len=%u, reset remain to 0\n",
				ctx->remain_pcmlen, ctx->codec->resample_buffer_len);
			ctx->remain_pcmlen = 0;
		}

		int nExpectedResampLen = (int)((float)frame_len * ctx->codec->resample_rate);
		int nOutAvail = (int)ctx->codec->resample_buffer_len - (int)ctx->remain_pcmlen;
		/* 本次重采样写不下时丢弃历史remain，优先保留最新输入，避免内存踩踏。 */
		if (nOutAvail < nExpectedResampLen)
		{
			__WARN("resample buffer may overflow: remain=%u expected_append=%d avail=%d, drop remain and restart accumulation\n",
				ctx->remain_pcmlen, nExpectedResampLen, nOutAvail);
			ctx->remain_pcmlen = 0;
			nOutAvail = (int)ctx->codec->resample_buffer_len;
		}

		int nResampLen = AuBase_Resample(base, in_frame, frame_len,
										 (uint8_t *)ctx->codec->resample_buffer + ctx->remain_pcmlen,
										 nOutAvail);
		if (nResampLen <= 0)
		{
			__WARN("Resample returns %d, skip encode this round. remain_pcm=%u\n", nResampLen, ctx->remain_pcmlen);
			return 0;
		}
		ctx->remain_pcmlen += nResampLen;
		p = ctx->codec->resample_buffer;
		nDataLen = (int)ctx->remain_pcmlen;
	}
	else if (ctx->remain_pcmlen > 0)
	{
		if (!ctx->codec->resample_buffer || ctx->codec->resample_buffer_len == 0)
		{
			__WARN("No resampler and no cache buffer for remain data, drop remain=%u\n", ctx->remain_pcmlen);
			ctx->remain_pcmlen = 0;
			p = (char *)in_frame;
			nDataLen = frame_len;
		}
		else
		{
			/* 无重采样时，remain 与新输入拼接后统一送编码。 */
			if ((int)ctx->remain_pcmlen + frame_len > (int)ctx->codec->resample_buffer_len)
			{
				__WARN("append pcm overflow: remain=%u append=%d buf_len=%u, drop remain and keep latest frame\n",
					ctx->remain_pcmlen, frame_len, ctx->codec->resample_buffer_len);
				ctx->remain_pcmlen = 0;
			}
			memcpy(ctx->codec->resample_buffer + ctx->remain_pcmlen, in_frame, frame_len);
			ctx->remain_pcmlen += frame_len;
			p = ctx->codec->resample_buffer;
			nDataLen = (int)ctx->remain_pcmlen;
		}
	}
	else
	{
		/* 直通路径：当前帧直接作为待编码数据。 */
		ctx->remain_pcmlen = frame_len;
	}

	AACENC_InfoStruct enc_info = {0};
	if (aacEncInfo(ctx->aacEncHandle, &enc_info) != AACENC_OK || enc_info.frameLength <= 0)
	{
		__ERR("aacEncInfo invalid, frameLength=%d\n", enc_info.frameLength);
		return 0;
	}

	int pcm_frame_bytes = enc_info.frameLength * AUDIO_CODEC_BITWIDTH / 8 * base->out_chnNum ;
	if (pcm_frame_bytes <= 0)
	{
		__ERR("Invalid pcm_frame_bytes=%d\n", pcm_frame_bytes);
		return 0;
	}

	if (ctx->codec->resampler_handle == 0)
	{
		ctx->remain_pcmlen = nDataLen;
	}

	if ((int)ctx->remain_pcmlen < pcm_frame_bytes)
	{
		/* 不足一个AAC帧时先缓存，等待后续输入。 */
		return 0;
	}

	int total_out_bytes = 0;
	int loop_count = 0;
	/* Step2: 按AAC固定帧长循环编码，直到不足一帧，避免每次只消费一帧导致持续积压。 */
	while ((int)ctx->remain_pcmlen >= pcm_frame_bytes)
	{
		int in_identifier = IN_AUDIO_DATA;
		int out_identifier = OUT_BITSTREAM_DATA;
		int in_elem_size = 2; // 16-bit samples
		int out_elem_size = 1; // bytes

		int beEncBytes = pcm_frame_bytes;
		uint8_t *beEncData = (uint8_t *)p;
		int nNeedBytes = pcm_frame_bytes;
		char *pEncodeBuffer = AuBase_GetWritePos(base, nNeedBytes);
		if (!pEncodeBuffer)
		{
			__ERR("No buffer space available for encoding loop=%d need=%d\n", loop_count, nNeedBytes);
			break;
		}

		AACENC_BufDesc in_buf = {0}, out_buf = {0};
		AACENC_InArgs in_args = {0};
		AACENC_OutArgs out_args = {0};

		in_args.numInSamples = beEncBytes >> 1;
		in_args.numAncBytes = 0;

		in_buf.numBufs = 1;
		in_buf.bufs = (void **)&beEncData;
		in_buf.bufferIdentifiers = &in_identifier;
		in_buf.bufSizes = &beEncBytes;
		in_buf.bufElSizes = &in_elem_size;

		out_buf.numBufs = 1;
		out_buf.bufs = (void **)&pEncodeBuffer;
		out_buf.bufferIdentifiers = &out_identifier;
		out_buf.bufSizes = &nNeedBytes;
		out_buf.bufElSizes = &out_elem_size;

		AACENC_ERROR err = aacEncEncode(ctx->aacEncHandle, &in_buf, &out_buf, &in_args, &out_args);
		if (err != AACENC_OK)
		{
			__ERR("AAC encoding failed loop=%d err=%d remain=%u\n", loop_count, err, ctx->remain_pcmlen);
			break;
		}

		int consume_resampled_bytes = (out_args.numInSamples << 1);
		if (consume_resampled_bytes <= 0)
		{
			__WARN("AAC encoder consumed no pcm at loop=%d, break to avoid dead loop\n", loop_count);
			break;
		}

		int nRemainLen = (int)ctx->remain_pcmlen - consume_resampled_bytes;
		if (nRemainLen < 0)
		{
			nRemainLen = 0;
		}

		base->out_datalen += out_args.numOutBytes;
		AuBase_DataOutput(base, timestamp, u32Seq);
		total_out_bytes += out_args.numOutBytes;

		if (nRemainLen > 0)
		{
			/* Step3: 将未消费PCM搬回缓冲区头部，供下次循环/下次调用继续编码。 */
			if (ctx->codec->resample_buffer && nRemainLen <= (int)ctx->codec->resample_buffer_len)
			{
				if (p == ctx->codec->resample_buffer)
				{
					memmove(ctx->codec->resample_buffer, p + consume_resampled_bytes, nRemainLen);
				}
				else
				{
					memcpy(ctx->codec->resample_buffer, p + consume_resampled_bytes, nRemainLen);
				}
				p = ctx->codec->resample_buffer;
			}
			else
			{
				__WARN("Cannot cache remain pcm, drop remain=%d buffer=%p len=%u\n",
					nRemainLen, ctx->codec->resample_buffer, ctx->codec->resample_buffer_len);
				nRemainLen = 0;
			}
		}

		ctx->remain_pcmlen = (unsigned int)nRemainLen;
		loop_count++;
	}

	return total_out_bytes;
}
