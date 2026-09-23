#include "anj_mw_log.h"
#include "AuBase.h"
#include "AuCodec.h"

int AuCodec_Init(AuCodec *ctx)
{
    if (!ctx)
        return -1;

    if (ctx->in_sampleRate == 0 || ctx->in_chnNum == 0)
        return 0;

    ctx->resample_rate = (float)((float)(ctx->base->out_sampleRate * ctx->base->out_chnNum) /
                                   (float)(ctx->in_sampleRate * ctx->in_chnNum));
    ctx->base->resample_rate = ctx->resample_rate;

    __INFO("ChnNum:%d->%d SampleRate:%d->%d, resample_rate %.2f\n",
           ctx->in_chnNum, ctx->base->out_chnNum,
           ctx->in_sampleRate, ctx->base->out_sampleRate,
           ctx->resample_rate);

    if (ctx->resampler_handle != 0)
    {
        uninit_PCM_resample(ctx->resampler_handle);
        ctx->resampler_handle = 0;
        ctx->base->resampler_handle = 0;
    }

    if (ctx->resample_buffer != NULL)
    {
        free(ctx->resample_buffer);
        ctx->resample_buffer = NULL;
    }

    if (ctx->base->out_sampleRate != ctx->in_sampleRate ||
        ctx->base->out_chnNum != ctx->in_chnNum)
    {
        __INFO("resample to ChnNum:%d->%d SampleRate:%d->%d\n",
               ctx->in_chnNum, ctx->base->out_chnNum,
               ctx->in_sampleRate, ctx->base->out_sampleRate);

        ctx->resampler_handle = init_PCM_resample(ctx->base->out_chnNum,
                                                  ctx->in_chnNum,
                                                  ctx->base->out_sampleRate,
                                                  ctx->in_sampleRate);
        if (ctx->resampler_handle == 0)
        {
            __ERR("resampler creat failed.\n");
            return -1;
        }
        else
        {
            ctx->base->resampler_handle = ctx->resampler_handle;
            ctx->resample_buffer = (char *)malloc(ctx->resample_buffer_len);
            if (ctx->resample_buffer == NULL)
            {
                __ERR("malloc length %d failed.\n", ctx->resample_buffer_len);
                return -1;
            }
        }
    }
    else
    {
        ctx->base->resampler_handle = 0;
    }

    return 0;
}

AuCodec *AuCodec_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
                        AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
                        int32_t samplesPerFrame, int outBufLen, cb_out_data cb)
{
    AuCodec *ctx = (AuCodec *)malloc(sizeof(AuCodec));
    if (!ctx)
        return NULL;

    memset(ctx, 0, sizeof(AuCodec));

    ctx->base = AuBase_Create(user, desSampleRate, desChnNum,
                              samplesPerFrame, outBufLen, cb);
    if (!ctx->base)
    {
        __ERR("AuBase_Create failed.\n");
        AuCodec_Destroy(ctx);
        return NULL;
    }
    ctx->in_sampleRate = srcSampleRate;
    ctx->in_chnNum = srcChnNum;
    ctx->resample_buffer_len = RESAMPLE_BUFFER_LEN;

    if (0 != AuCodec_Init(ctx))
    {
        __ERR("AuCodec_Init failed.\n");
        AuCodec_Destroy(ctx);
        return NULL;
    }
    return ctx;
}

void AuCodec_Destroy(AuCodec *ctx)
{
    if (!ctx)
        return;

    if (ctx->resampler_handle)
    {
        uninit_PCM_resample(ctx->resampler_handle);
        ctx->resampler_handle = 0;
    }

    if (ctx->base)
    {
        /* Resampler already released above; avoid releasing twice in AuBase_Destroy. */
        ctx->base->resampler_handle = 0;
        AuBase_Destroy(ctx->base);
    }

    if (ctx->resample_buffer != NULL)
    {
        free(ctx->resample_buffer);
        ctx->resample_buffer = NULL;
    }

    free(ctx);
}

// CAuCodec *CAuCodec_create(void *pUser,
//                           unsigned int srcSampleRate,
//                           unsigned int srcChnNum,
//                           unsigned int desSampleRate,
//                           unsigned int desChnNum,
//                           int32_t samplesPerFrame,
//                           cb_out_data cb)
// {
//     CAuCodec *codec = (CAuCodec *)malloc(sizeof(CAuCodec));
//     if (!codec)
//         return NULL;

//     memset(codec, 0, sizeof(CAuCodec));

//     codec->base.pUser = pUser;
//     codec->base.m_out_sampleRate = desSampleRate;
//     codec->base.m_out_chnNum = desChnNum;
//     codec->base.m_samplesPerFrame = samplesPerFrame;
//     codec->base.m_cb_out_data = cb;

//     codec->in_sampleRate = srcSampleRate;
//     codec->in_chnNum = srcChnNum;
//     codec->m_resample_buffer = NULL;
//     codec->m_resample_buffer_len = RESAMPLE_BUFFER_LEN;
//     codec->m_resampler_handle = NULL;

//     return codec;
// }

// int CAuCodec_Init(CAuCodec *ctx)
// {
//     if (!ctx)
//         return -1;

//     /* 基类初始化模拟 */
//     /* 实际项目中这里应有CAuBase的初始化逻辑 */

//     if (ctx->in_sampleRate == 0 || ctx->in_chnNum == 0)
//         return 0;

//     ctx->m_resample_rate = (float)((float)(ctx->base.m_out_sampleRate * ctx->base.m_out_chnNum) /
//                                    (float)(ctx->in_sampleRate * ctx->in_chnNum));

//     __INFO("ChnNum:%d->%d SampleRate:%d->%d, m_resample_rate %.2f\n",
//            ctx->in_chnNum, ctx->base.m_out_chnNum,
//            ctx->in_sampleRate, ctx->base.m_out_sampleRate,
//            ctx->m_resample_rate);

//     if (ctx->m_resampler_handle != NULL)
//     {
//         uninit_PCM_resample(ctx->m_resampler_handle);
//         ctx->m_resampler_handle = NULL;
//     }

//     if (ctx->m_resample_buffer != NULL)
//     {
//         free(ctx->m_resample_buffer);
//         ctx->m_resample_buffer = NULL;
//     }

//     if (ctx->base.m_out_sampleRate != ctx->in_sampleRate ||
//         ctx->base.m_out_chnNum != ctx->in_chnNum)
//     {
//         __INFO("resample to ChnNum:%d->%d SampleRate:%d->%d\n",
//                ctx->in_chnNum, ctx->base.m_out_chnNum,
//                ctx->in_sampleRate, ctx->base.m_out_sampleRate);

//         ctx->m_resampler_handle = init_PCM_resample(ctx->base.m_out_chnNum,
//                                                     ctx->in_chnNum,
//                                                     ctx->base.m_out_sampleRate,
//                                                     ctx->in_sampleRate);
//         if (ctx->m_resampler_handle == NULL)
//         {
//             __INFO("resampler creat failed.\n");
//             return -1;
//         }

//         if (ctx->m_resampler_handle != NULL)
//         {
//             ctx->m_resample_buffer = (char *)malloc(ctx->m_resample_buffer_len);
//             if (ctx->m_resample_buffer == NULL)
//             {
//                 __INFO("malloc length %d failed.\n", ctx->m_resample_buffer_len);
//                 return -1;
//             }
//         }
//     }

//     return 0;
// }

// void CAuCodec_Destroy(AuCodec *ctx)
// {
//     if (!ctx)
//         return;

//     if (ctx->m_resampler_handle != NULL)
//     {
//         uninit_PCM_resample(ctx->m_resampler_handle);
//         ctx->m_resampler_handle = NULL;
//     }

//     if (ctx->m_resample_buffer != NULL)
//     {
//         free(ctx->m_resample_buffer);
//         ctx->m_resample_buffer = NULL;
//     }

//     free(ctx);
// }