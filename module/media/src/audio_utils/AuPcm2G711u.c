#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "g7xx.h"
#include "AuPcm2G711u.h"
#include "anj_mw_log.h"

int32_t AuPcm2G711u_Process(AuPcm2G711u *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq)
{
    if (frame_len <= 0)
        return 0;

    char *p = (char *)in_frame;
    int32_t nDataLen = frame_len;

    // 重采样处理
    AuCodec *codec = ctx->codec;
    if (codec->base->resampler_handle != 0)
    {
        nDataLen = AuBase_Resample(codec->base, in_frame, frame_len,
                                   (uint8_t *)codec->resample_buffer,
                                   codec->resample_buffer_len);
        p = (char *)codec->resample_buffer;
    }

    int32_t nNeedBytes = nDataLen >> 1; // PCM->G711压缩
    char *pEncodeBuffer = AuBase_GetWritePos(codec->base, nNeedBytes);
    if (!pEncodeBuffer)
    {
        __ERR("Error: no write position\n");
        return 0;
    }

    int32_t nEncodedLen = G711_ULawEncodeBuf((uint8_t *)pEncodeBuffer, (int16_t *)p, nDataLen);
    if (nEncodedLen != nNeedBytes)
    {
        __ERR("Warning: encoded len %d != needed %d\n", nEncodedLen, nNeedBytes);
    }

    AuBase_PutOutputData(codec->base, pEncodeBuffer, nEncodedLen, timestamp, u32Seq);
    AuBase_DataOutput(codec->base, timestamp, u32Seq);
    return 0;
}

AuPcm2G711u *AuPcm2G711u_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
                                AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
                                int32_t samplesPerFrame, cb_out_data cb)
{
    int outBufLen = AUDIO_CODEC_OUT_BUFFER_LEN;
    AuPcm2G711u *ctx = (AuPcm2G711u *)malloc(sizeof(AuPcm2G711u));
    if (!ctx)
        return NULL;

    ctx->codec = AuCodec_Create(user, srcSampleRate, srcChnNum,
                                desSampleRate, desChnNum,
                                samplesPerFrame, outBufLen, cb);
    return ctx;
}

void AuPcm2G711u_Destroy(AuPcm2G711u *ctx)
{
    AuCodec_Destroy(ctx->codec);
    free(ctx);
}