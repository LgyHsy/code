#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "g7xx.h"
#include "AuG711a2Pcm.h"
#include "anj_mw_log.h"

int32_t AuG711a2Pcm_Process(AuG711a2Pcm *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq)
{
    if (frame_len <= 0)
        return 0;

    char *p = (char *)in_frame;
    int32_t nDataLen = frame_len;

    AuCodec *codec = ctx->codec;
    int32_t nEncodedLen = 0;
    int32_t nNeedBytes = nDataLen << 1; // G711->PCM 放大一倍
    nNeedBytes = (int)(nNeedBytes * codec->resample_rate);
    char *pEncodeBuffer = AuBase_GetWritePos(codec->base, nNeedBytes);
    if (!pEncodeBuffer)
    {
        __ERR("Error: no write position\n");
        return 0;
    }

    if (codec->base->resampler_handle == 0)
    {
        nEncodedLen = G711_ALawDecodeBuf((int16_t *)pEncodeBuffer, (uint8_t *)p, nDataLen);
        if (nEncodedLen > 0)
        {
            codec->base->out_datalen += nEncodedLen;
        }
    }
    else
    {
        nEncodedLen = G711_ALawDecodeBuf((int16_t *)codec->resample_buffer, (uint8_t *)p, nDataLen);
        if (nEncodedLen > 0)
        {
            nDataLen = AuBase_Resample(codec->base, (uint8_t *)codec->resample_buffer, nEncodedLen / 2,
                                       (uint8_t *)pEncodeBuffer, nNeedBytes);
            if (nDataLen  > 0)
            {
                codec->base->out_datalen += nEncodedLen;
            }
        }
    }

    AuBase_DataOutput(codec->base, timestamp, u32Seq);
    return 0;
}

AuG711a2Pcm *AuG711a2Pcm_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
                                AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
                                int32_t samplesPerFrame, cb_out_data cb)
{
    int outBufLen = AUDIO_CODEC_G711U_PCM_OUT_BUFFER_LEN;
    AuG711a2Pcm *ctx = (AuG711a2Pcm *)malloc(sizeof(AuG711a2Pcm));
    if (!ctx)
        return NULL;

    ctx->codec = AuCodec_Create(user, srcSampleRate, srcChnNum,
                                desSampleRate, desChnNum,
                                samplesPerFrame, outBufLen, cb);
    return ctx;
}

void AuG711a2Pcm_Destroy(AuG711a2Pcm *ctx)
{
    AuCodec_Destroy(ctx->codec);
    free(ctx);
}