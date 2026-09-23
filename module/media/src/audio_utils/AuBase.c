#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/prctl.h>

#include "anj_mw_log.h"
#include "AuBase.h"

void AuBase_PutOutputData(AuBase *base, char *ouput_buff, int len, unsigned long long int timestamp, unsigned u32Seq)
{
    if (!base)
        return;

    // 直接处理完整帧
    if (base->out_bytes_per_frame == 0)
    {
        base->cb(base->user, ouput_buff, len, timestamp, u32Seq);
        return;
    }

    // 检查数据头有效性
    if (!(base->out_datahead <= base->out_databuffer + base->out_databuffer_len &&
          base->out_datahead >= base->out_databuffer))
    {
        __WARN("error: m_out_datahead invalid, reset position.");
        base->out_datahead = base->out_databuffer;
        base->out_datalen = 0;
    }

    // 计算缓冲区空间
    int iTailSideSize = (base->out_databuffer + base->out_databuffer_len) -
                        (base->out_datahead + base->out_datalen);
    int iHeadSideSize = base->out_datahead - base->out_databuffer;

    if (len <= iTailSideSize)
    {
        memcpy(base->out_datahead + base->out_datalen, ouput_buff, len);
        base->out_datalen += len;
    }
    else
    {
        if (iHeadSideSize >= base->out_datalen + len)
        {
            // 移动数据到头部
            memmove(base->out_databuffer, base->out_datahead, base->out_datalen);
            base->out_datahead = base->out_databuffer;
            memcpy(base->out_datahead + base->out_datalen, ouput_buff, len);
            base->out_datalen += len;
        }
        else
        {
            __ERR("error: m_out_datahead offset=%d, m_out_datalen %d, len %d, reset position.\n",
                  base->out_datahead - base->out_databuffer, base->out_datalen, len);
            base->out_datahead = base->out_databuffer;
            base->out_datalen = 0;
        }
    }
}

void AuBase_DataOutput(AuBase *base, unsigned long long int timestamp, unsigned int seq)
{
    if (!base)
        return;

    if (base->out_bytes_per_frame == 0)
    {
        if (base->out_datalen > 0)
        {
            base->cb(base->user, base->out_datahead, base->out_datalen, timestamp, seq);
        }
        base->out_datalen = 0;
        return;
    }

    while (base->out_datalen >= (int)base->out_bytes_per_frame)
    {
        int nDataLen = base->out_bytes_per_frame;
        base->cb(base->user, base->out_datahead, nDataLen, timestamp, seq);

        base->out_datahead += nDataLen;
        base->out_datalen -= nDataLen;

        if (base->out_datalen < 0)
        {
            __ERR("### error %d ###\n", base->out_datalen);
            base->out_datahead = base->out_databuffer;
            base->out_datalen = 0;
        }
    }
}

char *AuBase_GetWritePos(AuBase *base, unsigned int nNeedBytes)
{
    if (!base)
        return NULL;

    if ((int)nNeedBytes > base->out_databuffer_len)
    {
        __ERR("error: needed encoded len %d > %d.\n", nNeedBytes, base->out_databuffer_len);
        return NULL;
    }

    // 检查数据头指针有效性
    if (!(base->out_datahead <= base->out_databuffer + base->out_databuffer_len &&
          base->out_datahead >= base->out_databuffer))
    {
        __ERR("error: m_out_datahead invalid, reset position.\n");
        base->out_datahead = base->out_databuffer;
        base->out_datalen = 0;
    }

    // 计算缓冲区空间
    int iTailSideSize = (base->out_databuffer + base->out_databuffer_len) -
                        (base->out_datahead + base->out_datalen);
    int iHeadSideSize = base->out_datahead - base->out_databuffer;

    if ((int)nNeedBytes <= iTailSideSize)
    {
        // 尾部空间足够
    }
    else
    {
        if (iHeadSideSize >= (int)(base->out_datalen + nNeedBytes))
        {
            // 移动数据到头部
            if (base->out_datalen > 0)
            {
                memmove(base->out_databuffer, base->out_datahead, base->out_datalen);
            }
            base->out_datahead = base->out_databuffer;
        }
        else
        {
            __ERR("error: m_out_datahead offset=%d, m_out_datalen %d, need bytes %d, reset position.\n",
                  base->out_datahead - base->out_databuffer, base->out_datalen, nNeedBytes);
            base->out_datahead = base->out_databuffer;
            base->out_datalen = 0;
        }
    }

    return base->out_datahead + base->out_datalen;
}

int AuBase_Resample(AuBase *base, uint8_t *in_frame, int32_t frame_len,
                    uint8_t *out_frame, int32_t out_buf_len)
{
    if (!base || base->resampler_handle == 0)
        return 0;

    int nNeededBuflen = (int)((float)frame_len * base->resample_rate);
    if (out_buf_len < nNeededBuflen)
    {
        __ERR("error. bufferlen %d < %d needed.\n", out_buf_len, nNeededBuflen);
        return 0;
    }

    return start_PCM_resample(base->resampler_handle,
                              (short *)out_frame,
                              (short *)in_frame,
                              frame_len);
}

int AuBase_Init(AuBase *ctx)
{
    if (!ctx)
        return -1;

    if (ctx->out_databuffer)
    {
        free(ctx->out_databuffer);
        ctx->out_databuffer = NULL;
    }

    ctx->out_databuffer = (char *)malloc(ctx->out_databuffer_len);
    if (!ctx->out_databuffer)
    {
        __ERR("error, malloc length %d failed.\n", ctx->out_databuffer_len);
        return -1;
    }

    ctx->out_datahead = ctx->out_databuffer;
    ctx->out_datalen = 0;
    return 0;
}

AuBase *AuBase_Create(void *user, AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
                      int32_t samplesPerFrame,  int outBufLen, cb_out_data cb)
{
    AuBase *ctx = (AuBase *)malloc(sizeof(AuBase));
    if (!ctx)
        return NULL;

    memset(ctx, 0, sizeof(AuBase));
    ctx->user = user;
    ctx->out_sampleRate = desSampleRate;
    ctx->out_chnNum = desChnNum;
    ctx->out_bytes_per_frame = samplesPerFrame;
    ctx->cb = cb;
    ctx->out_databuffer_len = outBufLen;
    if (0 != AuBase_Init(ctx))
    {
        __ERR("AuBase_Init failed.\n");
        AuBase_Destroy(ctx);
        return NULL;
    }
    return ctx;
}

void AuBase_Destroy(AuBase *ctx)
{
    if (!ctx)
        return;

    if (ctx->out_databuffer)
    {
        free(ctx->out_databuffer);
    }

    if (ctx->resampler_handle)
    {
        uninit_PCM_resample(ctx->resampler_handle);
    }

    free(ctx);
}
