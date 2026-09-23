#include <stdlib.h>
#include <string.h>
#include "anj_mw_log.h"
#include "AuMp32Pcm.h"

#define MP3_ID3V2_TAG_SIZE 10
#define READ_MP3_PACKET_SIZE 640

AuMp32Pcm *AuMp32Pcm_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
							AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
							int32_t samplesPerFrame, cb_out_data cb)
{
	int outBufLen = AUDIO_MP3_OUT_BUFFER_LEN;
	AuMp32Pcm *ctx = (AuMp32Pcm *)malloc(sizeof(AuMp32Pcm));
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(AuMp32Pcm));

	ctx->codec = AuCodec_Create(user, srcSampleRate, srcChnNum,
								desSampleRate, desChnNum,
								samplesPerFrame, outBufLen, cb);
	if (!ctx->codec)
	{
		free(ctx);
		return NULL;
	}

	ctx->hip = hip_decode_init();
	if (!ctx->hip)
	{
		AuCodec_Destroy(ctx->codec);
		free(ctx);
		return NULL;
	}

	return ctx;
}

void AuMp32Pcm_Destroy(AuMp32Pcm *ctx)
{
	if (!ctx)
		return;

	if (ctx->hip)
	{
		hip_decode_exit(ctx->hip);
	}

	if (ctx->codec)
	{
		AuCodec_Destroy(ctx->codec);
	}

	free(ctx);
}

int32_t AuMp32Pcm_Process(AuMp32Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq)
{
	if (!ctx || frame_len <= 0)
		return 0;

	int offset = 0;
	int bMp3HeaderDecode = 0;

	if ((in_frame[0] == 'I') && (in_frame[1] == 'D') && (in_frame[2] == '3'))
	{
		unsigned int tag_size = (in_frame[9]) | (in_frame[8] << 7) | (in_frame[7] << 14) | (in_frame[6] << 28);
		__INFO("mp3 file header has id3v2 tag size:%d.\n", tag_size);
		offset = MP3_ID3V2_TAG_SIZE + tag_size;
	}

	int remainSize = frame_len - offset;
	while (remainSize > 0)
	{
		int decodeLen = (remainSize > READ_MP3_PACKET_SIZE) ? READ_MP3_PACKET_SIZE : remainSize;
		unsigned char mp3buf[READ_MP3_PACKET_SIZE] = {0};
		memcpy(mp3buf, (unsigned char *)&in_frame[offset], decodeLen);

		// MP3解码
		int nsamples = hip_decode1_headers(ctx->hip, mp3buf, READ_MP3_PACKET_SIZE,
										   ctx->out_lbuf, ctx->out_rbuf, &ctx->mp3data);
		offset += decodeLen;
		remainSize -= decodeLen;
		// __INFO("######### nsamples:%d offset:%d remainSize:%d frame_len:%d\n", nsamples, offset, remainSize, frame_len);
		while (nsamples > 0)
		{
			if (nsamples > MAX_MP3_FRAME_SAMPLES_NUM)
			{
				__WARN("Excessive samples per frame: %d", nsamples);
				break;
			}

			if (bMp3HeaderDecode == 0)
			{
				if (ctx->mp3data.header_parsed == 0)
				{
					nsamples = hip_decode1_headers(ctx->hip, NULL, 0,
												   ctx->out_lbuf, ctx->out_rbuf, &ctx->mp3data);
					continue;
				}
				bMp3HeaderDecode = 1;

				unsigned int in_chnNum = (ctx->mp3data.stereo == 2) ? 2 : 1;
				unsigned int in_sampleRate = (ctx->mp3data.samplerate == 0) ? 16000 : ctx->mp3data.samplerate;

				// 更新采样率/通道数变化
				if (in_chnNum != ctx->codec->in_chnNum || in_sampleRate != ctx->codec->in_sampleRate)
				{
					ctx->codec->in_chnNum = in_chnNum;
					ctx->codec->in_sampleRate = in_sampleRate;
					AuCodec_Init(ctx->codec);
				}
			}
			// 混音处理
			for (int i = 0; i < nsamples; i++)
			{
				if (ctx->mp3data.stereo == 2)
				{
					if (ctx->codec->in_chnNum == 2)
					{
						ctx->out_buf[i * 2] = ctx->out_lbuf[i];
						ctx->out_buf[i * 2 + 1] = ctx->out_rbuf[i];
					}
					else
					{
						ctx->out_buf[i] = (ctx->out_lbuf[i] + ctx->out_rbuf[i]) >> 1;
					}
				}
				else
				{
					ctx->out_buf[i] = ctx->out_lbuf[i];
				}
			}

			// 准备输出数据
			int in_len = nsamples * ctx->codec->in_chnNum * sizeof(short);
			int8_t *outbuf = (int8_t *)ctx->out_buf;
			int outlen = in_len;

			// 重采样处理
			if (ctx->codec->resampler_handle != 0)
			{
				outlen = start_PCM_resample(ctx->codec->resampler_handle,
											(short *)ctx->codec->resample_buffer,
											ctx->out_buf,
											in_len);
				outbuf = (int8_t *)ctx->codec->resample_buffer;
			}

			AuBase_PutOutputData(ctx->codec->base, (char *)outbuf, outlen, timestamp, u32Seq);
			AuBase_DataOutput(ctx->codec->base, timestamp, u32Seq);

			nsamples = hip_decode1_headers(ctx->hip, NULL, 0,
										   ctx->out_lbuf, ctx->out_rbuf, &ctx->mp3data);
		}
	}
	return 0;
}
