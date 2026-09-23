
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>

#include "anj_mw_media_common.h"
#include "media_util.h"

static unsigned int getEncodeProfile(int profile)
{
    unsigned int retValue = 1;

    if (0 == profile)
    {
        // high profile
        retValue = 2;
    }
    else if (1 == profile)
    {
        // main profile
        retValue = 1;
    }
    else if (2 == profile)
    {
        // base profile
        retValue = 0;
    }

    return retValue;
}

/* 临时限制 4000kbps 省内存，内存够了删掉 */
#define TS_VENC_BITRATE_MAX_KBPS 4000u

static TS_U32 TS_Common_VencClampBitrate(TS_U32 u32BitRate)
{
    if (u32BitRate > TS_VENC_BITRATE_MAX_KBPS)
    {
        __INFO("bitrate %u too high, clamp to %ukbps\n", u32BitRate, TS_VENC_BITRATE_MAX_KBPS);
        return TS_VENC_BITRATE_MAX_KBPS;
    }
    return u32BitRate;
}

static TS_VOID TS_Common_VencSetPrivateAttr(PAYLOAD_TYPE_E enType, VENC_RC_MODE_E enRcMode,
                                            tsVENC_ATTR_PRI_S *pstAttrPrivate)
{
    if (enType != PT_H265 && enType != PT_H264)
    {
        return;
    }

    pstAttrPrivate->s32RcTuneVisual = 0;
    pstAttrPrivate->s32RcQpDeltaIP = 2;

    /* 色度qp补偿 */
    if (enType == PT_H265)
    {
        pstAttrPrivate->s32RcCbQpOffset = -3;
        pstAttrPrivate->s32RcCrQpOffset = -3;
    }
    else if (enType == PT_H264)
    {
        pstAttrPrivate->s32RcChromaDqp = -6;
    }

    if (enRcMode == VENC_RC_MODE_H264VBR || enRcMode == VENC_RC_MODE_H265VBR ||
        enRcMode == VENC_RC_MODE_H264QVBR || enRcMode == VENC_RC_MODE_H265QVBR)
    {
        /* VBR/QVBR：冻结私有第三轮 + RC（ChangePos/Avg/MaxIprop） */
        pstAttrPrivate->s32RcTuneVisual = 0;
        pstAttrPrivate->s32RcQpDeltaIP = 1;
        pstAttrPrivate->s32RcBestQuality = 20;
        pstAttrPrivate->s32RcWorstQuality = 100;
        pstAttrPrivate->s32RcMinBitratePos = 70;

        pstAttrPrivate->s32RcAqSsimEn = 1;
        pstAttrPrivate->s32RcAqInitFrmAvgSvar = 6;
        pstAttrPrivate->s32RcAqSrc = 0;
        pstAttrPrivate->s32RcAqNegRatio = 10;
        pstAttrPrivate->s32RcAqPosRatio = 40;
        pstAttrPrivate->s32RcAqQpdeltaLmt = 4;
        pstAttrPrivate->s32RcQpmapClipTop = 25;
        pstAttrPrivate->s32RcQpmapClipBottom = 45;
        pstAttrPrivate->s32RcBitRatioI = 8;
    }
    else if (enRcMode == VENC_RC_MODE_H264AVBR || enRcMode == VENC_RC_MODE_H265AVBR)
    {
        /* AVBR参数 */
        pstAttrPrivate->s32RcAqSsimEn = 1;
        pstAttrPrivate->s32RcAqInitFrmAvgSvar = 6;
        pstAttrPrivate->s32RcAqSrc = 0;
        pstAttrPrivate->s32RcAqNegRatio = 24;
        pstAttrPrivate->s32RcAqPosRatio = 56;
        pstAttrPrivate->s32RcAqQpdeltaLmt = 6;
        pstAttrPrivate->s32RcQpmapClipTop = 25;
        pstAttrPrivate->s32RcQpmapClipBottom = 51;
        pstAttrPrivate->s32RcBitRatioI = -1;
    }
    else if (enRcMode == VENC_RC_MODE_H264CBR || enRcMode == VENC_RC_MODE_H265CBR)
    {
        /* CBR参数 */
        pstAttrPrivate->s32RcAqSsimEn = 2;
        pstAttrPrivate->s32RcAqInitFrmAvgSvar = 6;
        pstAttrPrivate->s32RcAqSrc = 63;
        pstAttrPrivate->s32RcAqNegRatio = 24;
        pstAttrPrivate->s32RcAqPosRatio = 56;
        pstAttrPrivate->s32RcAqQpdeltaLmt = 6;
        pstAttrPrivate->s32RcQpmapClipTop = 1;
        pstAttrPrivate->s32RcQpmapClipBottom = 51;
        pstAttrPrivate->s32RcBitRatioI = -1;
    }
    else if (enRcMode == VENC_RC_MODE_H264FIXQP || enRcMode == VENC_RC_MODE_H265FIXQP)
    {
        pstAttrPrivate->s32RcQpi = 37;
        pstAttrPrivate->s32RcQpp = 37;
    }
}

static TS_S32 TS_Common_VencAttrSet(VENC_CHN_ATTR_S *pstChnAttr, TS_Common_VencAttr_t *pstVencAttr)
{
    VENC_ATTR_JPEG_S stJpegAttr = {0};
    TS_U32 u32StatTime;
    TS_U32 u32Gop;
    PAYLOAD_TYPE_E enType;
    TS_U32 u32FrameRate;
    VENC_RC_MODE_E enRcMode;

    if (!pstChnAttr || !pstVencAttr)
    {
        return TS_FAILURE;
    }

    pstVencAttr->u32BitRate = TS_Common_VencClampBitrate(pstVencAttr->u32BitRate);
    u32Gop = pstVencAttr->u32Gop;
    enType = pstVencAttr->eVencType;
    u32FrameRate = pstVencAttr->u32SrcFrameRate;
    enRcMode = pstVencAttr->eRcMode;
    u32StatTime = pstVencAttr->u32StatTime;
    pstChnAttr->stRcAttr.enRcMode = enRcMode;

    switch (enType)
    {
    case PT_H265:
    {
        if (VENC_RC_MODE_H265CBR == enRcMode)
        {
            VENC_H265_CBR_S stH265Cbr = {0};

            stH265Cbr.u32Gop = u32Gop;
            stH265Cbr.u32StatTime = u32StatTime;       /* stream rate statics time(s) */
            stH265Cbr.u32SrcFrameRate = u32FrameRate;  /* input (vi) frame rate */
            stH265Cbr.fr32DstFrameRate = u32FrameRate; /* target frame rate */
            stH265Cbr.u32BitRate = pstVencAttr->u32BitRate;
            stH265Cbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH265Cbr, &stH265Cbr, sizeof(VENC_H265_CBR_S));
        }
        else if (VENC_RC_MODE_H265FIXQP == enRcMode)
        {
            VENC_H265_FIXQP_S stH265FixQp = {0};

            stH265FixQp.u32Gop = u32Gop;
            stH265FixQp.u32SrcFrameRate = u32FrameRate;
            stH265FixQp.fr32DstFrameRate = u32FrameRate;
            stH265FixQp.u32IQp = 25;
            stH265FixQp.u32PQp = 30;
            stH265FixQp.u32BQp = 32;
            memcpy(&pstChnAttr->stRcAttr.stH265FixQp, &stH265FixQp, sizeof(VENC_H265_FIXQP_S));
        }
        else if (VENC_RC_MODE_H265VBR == enRcMode)
        {
            VENC_H265_VBR_S stH265Vbr = {0};

            stH265Vbr.u32Gop = u32Gop;
            stH265Vbr.u32StatTime = u32StatTime;
            stH265Vbr.u32SrcFrameRate = u32FrameRate;
            stH265Vbr.fr32DstFrameRate = u32FrameRate;
            stH265Vbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            stH265Vbr.u32AvgBitRate = pstVencAttr->u32BitRate * 80 / 100;
            memcpy(&pstChnAttr->stRcAttr.stH265Vbr, &stH265Vbr, sizeof(VENC_H265_VBR_S));
        }
        else if (VENC_RC_MODE_H265AVBR == enRcMode)
        {
            VENC_H265_AVBR_S stH265AVbr = {0};

            stH265AVbr.u32Gop = u32Gop;
            stH265AVbr.u32StatTime = u32StatTime;
            stH265AVbr.u32SrcFrameRate = u32FrameRate;
            stH265AVbr.fr32DstFrameRate = u32FrameRate;
            stH265AVbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH265AVbr, &stH265AVbr, sizeof(VENC_H265_AVBR_S));
        }
        else if (VENC_RC_MODE_H265QVBR == enRcMode)
        {
            VENC_H265_QVBR_S stH265QVbr = {0};

            stH265QVbr.u32Gop = u32Gop;
            stH265QVbr.u32StatTime = u32StatTime;
            stH265QVbr.u32SrcFrameRate = u32FrameRate;
            stH265QVbr.fr32DstFrameRate = u32FrameRate;
            stH265QVbr.u32TargetBitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH265QVbr, &stH265QVbr, sizeof(VENC_H265_QVBR_S));
        }
        else if (VENC_RC_MODE_H265CVBR == enRcMode)
        {
            VENC_H265_CVBR_S stH265CVbr = {0};

            stH265CVbr.u32Gop = u32Gop;
            stH265CVbr.u32StatTime = u32StatTime;
            stH265CVbr.u32SrcFrameRate = u32FrameRate;
            stH265CVbr.fr32DstFrameRate = u32FrameRate;
            stH265CVbr.u32LongTermStatTime = 1;
            stH265CVbr.u32ShortTermStatTime = u32StatTime;
            stH265CVbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            stH265CVbr.u32LongTermMaxBitrate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH265CVbr, &stH265CVbr, sizeof(VENC_H265_CVBR_S));
        }
        else if (VENC_RC_MODE_H265QPMAP == enRcMode)
        {
            VENC_H265_QPMAP_S stH265QpMap = {0};

            stH265QpMap.u32Gop = u32Gop;
            stH265QpMap.u32StatTime = u32StatTime;
            stH265QpMap.u32SrcFrameRate = u32FrameRate;
            stH265QpMap.fr32DstFrameRate = u32FrameRate;
            stH265QpMap.enQpMapMode = VENC_RC_QPMAP_MODE_MEANQP;
            memcpy(&pstChnAttr->stRcAttr.stH265QpMap, &stH265QpMap, sizeof(VENC_H265_QPMAP_S));
        }
        else
        {
            __ERR("enRcMode(%d) not support\n", enRcMode);
            return TS_FAILURE;
        }
    }
        pstChnAttr->stVencAttr.stAttrH265e.bRcnRefShareBuf = TS_FALSE;
        break;
    case PT_H264:
    {
        if (VENC_RC_MODE_H264CBR == enRcMode)
        {
            VENC_H264_CBR_S stH264Cbr = {0};

            stH264Cbr.u32Gop = u32Gop;                 /*the interval of IFrame*/
            stH264Cbr.u32StatTime = u32StatTime;       /* stream rate statics time(s) */
            stH264Cbr.u32SrcFrameRate = u32FrameRate;  /* input (vi) frame rate */
            stH264Cbr.fr32DstFrameRate = u32FrameRate; /* target frame rate */
            stH264Cbr.u32BitRate = pstVencAttr->u32BitRate;
            stH264Cbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH264Cbr, &stH264Cbr, sizeof(VENC_H264_CBR_S));
        }
        else if (VENC_RC_MODE_H264FIXQP == enRcMode)
        {
            VENC_H264_FIXQP_S stH264FixQp = {0};

            stH264FixQp.u32Gop = u32Gop;
            stH264FixQp.u32SrcFrameRate = u32FrameRate;
            stH264FixQp.fr32DstFrameRate = u32FrameRate;
            stH264FixQp.u32IQp = 25;
            stH264FixQp.u32PQp = 30;
            stH264FixQp.u32BQp = 32;
            memcpy(&pstChnAttr->stRcAttr.stH264FixQp, &stH264FixQp, sizeof(VENC_H264_FIXQP_S));
        }
        else if (VENC_RC_MODE_H264VBR == enRcMode)
        {
            VENC_H264_VBR_S stH264Vbr = {0};

            stH264Vbr.u32Gop = u32Gop;
            stH264Vbr.u32StatTime = u32StatTime;
            stH264Vbr.u32SrcFrameRate = u32FrameRate;
            stH264Vbr.fr32DstFrameRate = u32FrameRate;
            stH264Vbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            stH264Vbr.u32AvgBitRate = pstVencAttr->u32BitRate * 80 / 100;
            memcpy(&pstChnAttr->stRcAttr.stH264Vbr, &stH264Vbr, sizeof(VENC_H264_VBR_S));
        }
        else if (VENC_RC_MODE_H264AVBR == enRcMode)
        {
            VENC_H264_AVBR_S stH264AVbr = {0};

            stH264AVbr.u32Gop = u32Gop;
            stH264AVbr.u32StatTime = u32StatTime;
            stH264AVbr.u32SrcFrameRate = u32FrameRate;
            stH264AVbr.fr32DstFrameRate = u32FrameRate;
            stH264AVbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH264AVbr, &stH264AVbr, sizeof(VENC_H264_AVBR_S));
        }
        else if (VENC_RC_MODE_H264QVBR == enRcMode)
        {
            VENC_H264_QVBR_S stH264QVbr = {0};

            stH264QVbr.u32Gop = u32Gop;
            stH264QVbr.u32StatTime = u32StatTime;
            stH264QVbr.u32SrcFrameRate = u32FrameRate;
            stH264QVbr.fr32DstFrameRate = u32FrameRate;
            stH264QVbr.u32TargetBitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH264QVbr, &stH264QVbr, sizeof(VENC_H264_QVBR_S));
        }
        else if (VENC_RC_MODE_H264CVBR == enRcMode)
        {
            VENC_H264_CVBR_S stH264CVbr = {0};

            stH264CVbr.u32Gop = u32Gop;
            stH264CVbr.u32StatTime = u32StatTime;
            stH264CVbr.u32SrcFrameRate = u32FrameRate;
            stH264CVbr.fr32DstFrameRate = u32FrameRate;
            stH264CVbr.u32LongTermStatTime = 1;
            stH264CVbr.u32ShortTermStatTime = u32StatTime;
            stH264CVbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            stH264CVbr.u32LongTermMaxBitrate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stH264CVbr, &stH264CVbr, sizeof(VENC_H264_CVBR_S));
        }
        else if (VENC_RC_MODE_H264QPMAP == enRcMode)
        {
            VENC_H264_QPMAP_S stH264QpMap = {0};

            stH264QpMap.u32Gop = u32Gop;
            stH264QpMap.u32StatTime = u32StatTime;
            stH264QpMap.u32SrcFrameRate = u32FrameRate;
            stH264QpMap.fr32DstFrameRate = u32FrameRate;
            memcpy(&pstChnAttr->stRcAttr.stH264QpMap, &stH264QpMap, sizeof(VENC_H264_QPMAP_S));
        }
        else
        {
            __ERR("enRcMode(%d) not support\n", enRcMode);
            return TS_FAILURE;
        }
    }
        pstChnAttr->stVencAttr.stAttrH264e.bRcnRefShareBuf = TS_FALSE;
        break;
    case PT_MJPEG:
    {
        if (VENC_RC_MODE_MJPEGFIXQP == enRcMode)
        {
            VENC_MJPEG_FIXQP_S stMjpegeFixQp = {0};

            stMjpegeFixQp.u32Qfactor = 95;
            stMjpegeFixQp.u32SrcFrameRate = u32FrameRate;
            stMjpegeFixQp.fr32DstFrameRate = u32FrameRate;

            memcpy(&pstChnAttr->stRcAttr.stMjpegFixQp, &stMjpegeFixQp, sizeof(VENC_MJPEG_FIXQP_S));
        }
        else if (VENC_RC_MODE_MJPEGCBR == enRcMode)
        {
            VENC_MJPEG_CBR_S stMjpegeCbr = {0};

            stMjpegeCbr.u32StatTime = u32StatTime;
            stMjpegeCbr.u32SrcFrameRate = u32FrameRate;
            stMjpegeCbr.fr32DstFrameRate = u32FrameRate;
            stMjpegeCbr.u32BitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stMjpegCbr, &stMjpegeCbr, sizeof(VENC_MJPEG_CBR_S));
        }
        else if (VENC_RC_MODE_MJPEGVBR == enRcMode)
        {
            VENC_MJPEG_VBR_S stMjpegVbr = {0};

            stMjpegVbr.u32StatTime = u32StatTime;
            stMjpegVbr.u32SrcFrameRate = u32FrameRate;
            stMjpegVbr.fr32DstFrameRate = 5;
            stMjpegVbr.u32MaxBitRate = pstVencAttr->u32BitRate;
            memcpy(&pstChnAttr->stRcAttr.stMjpegVbr, &stMjpegVbr, sizeof(VENC_MJPEG_VBR_S));
        }
        else
        {
            __ERR("cann't support other mode(%d) in this version!\n", enRcMode);
            return TS_FAILURE;
        }
    }
    break;

    case PT_JPEG:
        stJpegAttr.bSupportDCF = TS_FALSE;
        stJpegAttr.stMPFCfg.u8LargeThumbNailNum = 0;
        stJpegAttr.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
        memcpy(&pstChnAttr->stVencAttr.stAttrJpege, &stJpegAttr, sizeof(VENC_ATTR_JPEG_S));
        break;
    default:
        __ERR("cann't support this enType (%d) in this version!\n", enType);
        return TS_ERR_VENC_NOT_SUPPORT;
    }

    TS_Common_VencSetPrivateAttr(enType, enRcMode, &pstChnAttr->stVencAttr.stAttrPrivate);

    return TS_SUCCESS;
}

static TS_S32 TS_Common_VencSetRcQp(VENC_CHN VencChn, VENC_RC_MODE_E enRcMode, TS_U32 u32MinQp, TS_U32 u32MaxQp)
{
    TS_S32 s32Ret;
    VENC_RC_PARAM_S venc_rc_param = {0};

    if (u32MaxQp == 0 || u32MaxQp > 51 || u32MinQp > u32MaxQp)
    {
        return TS_SUCCESS;
    }

    s32Ret = TS_MPI_VENC_GetRcParam(VencChn, &venc_rc_param);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VENC_GetRcParam ret %#x chn:%d\n", s32Ret, VencChn);
        return TS_FAILURE;
    }

    if (enRcMode == VENC_RC_MODE_H265CBR)
    {
        venc_rc_param.stParamH265Cbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH265Cbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH265Cbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH265Cbr.u32MaxQp = u32MaxQp;
    }
    else if (enRcMode == VENC_RC_MODE_H265VBR)
    {
        venc_rc_param.stParamH265Vbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH265Vbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH265Vbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH265Vbr.u32MaxQp = u32MaxQp;
        /* Align with MStar SetRcParam: ChangePos / MaxIPProp */
        venc_rc_param.stParamH265Vbr.s32ChangePos = 80;
        venc_rc_param.stParamH265Vbr.u32MinIprop = 1;
        venc_rc_param.stParamH265Vbr.u32MaxIprop = 20;
    }
    else if (enRcMode == VENC_RC_MODE_H265AVBR)
    {
        venc_rc_param.stParamH265AVbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH265AVbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH265AVbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH265AVbr.u32MaxQp = u32MaxQp;
    }
    else if (enRcMode == VENC_RC_MODE_H265QVBR)
    {
        venc_rc_param.stParamH265QVbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH265QVbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH265QVbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH265QVbr.u32MaxQp = u32MaxQp;
    }
    else if (enRcMode == VENC_RC_MODE_H264CBR)
    {
        venc_rc_param.stParamH264Cbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH264Cbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH264Cbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH264Cbr.u32MaxQp = u32MaxQp;
    }
    else if (enRcMode == VENC_RC_MODE_H264VBR)
    {
        venc_rc_param.stParamH264Vbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH264Vbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH264Vbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH264Vbr.u32MaxQp = u32MaxQp;
        /* Align with MStar SetRcParam: ChangePos / MaxIPProp */
        venc_rc_param.stParamH264Vbr.s32ChangePos = 80;
        venc_rc_param.stParamH264Vbr.u32MinIprop = 1;
        venc_rc_param.stParamH264Vbr.u32MaxIprop = 20;
    }
    else if (enRcMode == VENC_RC_MODE_H264AVBR)
    {
        venc_rc_param.stParamH264AVbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH264AVbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH264AVbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH264AVbr.u32MaxQp = u32MaxQp;
    }
    else if (enRcMode == VENC_RC_MODE_H264QVBR)
    {
        venc_rc_param.stParamH264QVbr.u32MinIQp = u32MinQp;
        venc_rc_param.stParamH264QVbr.u32MaxIQp = u32MaxQp;
        venc_rc_param.stParamH264QVbr.u32MinQp = u32MinQp;
        venc_rc_param.stParamH264QVbr.u32MaxQp = u32MaxQp;
    }
    else
    {
        return TS_SUCCESS;
    }

    s32Ret = TS_MPI_VENC_SetRcParam(VencChn, &venc_rc_param);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VENC_SetRcParam ret %#x chn:%d\n", s32Ret, VencChn);
        return TS_FAILURE;
    }

    __INFO("set rc qp chn:%d mode:%d min:%u max:%u\n", VencChn, enRcMode, u32MinQp, u32MaxQp);
    return TS_SUCCESS;
}

TS_S32 TS_Common_VencCreateChannel(TS_Common_VencAttr_t *pstVencAttr)
{
    TS_S32 s32Ret;
    SIZE_S stPicSize;
    VENC_CHN_ATTR_S stVencChnAttr = {0};
    PAYLOAD_TYPE_E enType;
    VENC_RC_MODE_E enRcMode;
    TS_U32 u32FrameRate;

    if (!pstVencAttr)
    {
        __ERR("null ptr\n");
        return TS_FAILURE;
    }

    VENC_CHN VencChn = pstVencAttr->VencChnId;
    enType = pstVencAttr->eVencType;
    enRcMode = pstVencAttr->eRcMode;
    u32FrameRate = pstVencAttr->u32SrcFrameRate;
    stPicSize.u32Width = pstVencAttr->u32Width;
    stPicSize.u32Height = pstVencAttr->u32Height;

    /******************************************
     step 1:  Create Venc Channel
    ******************************************/
    stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    stVencChnAttr.stGopAttr.stNormalP.s32IPQpDelta = pstVencAttr->s32IPQPDelta;
    stVencChnAttr.stVencAttr.enType = enType;
    /* MaxPic 用实际分辨率，避免双路按 RESOLUTION_MAX 占满 MMZ 导致后创建通道失败 */
    stVencChnAttr.stVencAttr.u32MaxPicWidth = VencChn == 0 ? RESOLUTION_MAX_WIDTH : RESOLUTION_MAX_SUB_WIDTH;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = VencChn == 0 ? RESOLUTION_MAX_HEIGHT : RESOLUTION_MAX_SUB_HEIGHT;
    stVencChnAttr.stVencAttr.u32PicWidth = stPicSize.u32Width;   /*the picture width*/
    stVencChnAttr.stVencAttr.u32PicHeight = stPicSize.u32Height; /*the picture height*/
    stVencChnAttr.stVencAttr.stAttrPrivate.s32RotateDegree = 0;
    stVencChnAttr.stVencAttr.stAttrPrivate.bReencode = 0;
    stVencChnAttr.stVencAttr.stAttrPrivate.bMirror = 0;
    stVencChnAttr.stVencAttr.stAttrPrivate.bAfbc = 0;
    stVencChnAttr.stVencAttr.stAttrPrivate.enFastFlowMode = 0;
    stVencChnAttr.stVencAttr.stAttrPrivate.bSharedRingBuf = TS_FALSE;
    stVencChnAttr.stVencAttr.stAttrPrivate.s32SharedRingBufSize = 0;
    stVencChnAttr.stVencAttr.u32BufSize = pstVencAttr->u32Bufsize >= TS_VENC_BUFSIZE_MIN ? pstVencAttr->u32Bufsize : TS_VENC_BUFSIZE_MIN;
    stVencChnAttr.stVencAttr.u32Profile = getEncodeProfile(pstVencAttr->u32Profile);
    stVencChnAttr.stVencAttr.bByFrame = TS_TRUE; /*get stream mode is slice mode or frame mode?*/

    s32Ret = TS_Common_VencAttrSet(&stVencChnAttr, pstVencAttr);
    if (TS_SUCCESS != s32Ret)
    {
        return s32Ret;
    }

    __INFO("create venChn[%d], enType:%d, fps:%d MaxWH=[%d,%d], w=h[%d,%d], bufSize=%d profile:%d\n", VencChn,
           stVencChnAttr.stVencAttr.enType, u32FrameRate, stVencChnAttr.stVencAttr.u32MaxPicWidth,
           stVencChnAttr.stVencAttr.u32MaxPicHeight, stVencChnAttr.stVencAttr.u32PicWidth,
           stVencChnAttr.stVencAttr.u32PicHeight, stVencChnAttr.stVencAttr.u32BufSize, stVencChnAttr.stVencAttr.u32Profile);

    s32Ret = TS_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VENC_CreateChn [%d] faild with %#x! ===\n", VencChn, s32Ret);
        return s32Ret;
    }

    s32Ret = TS_Common_VencSetRcQp(VencChn, enRcMode, 32, 48);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_VencSetRcQp failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_VencGetMaxFd(TS_Common_VencAttr_t *pstVencAttr)
{
    TS_S32 maxfd = -1;
    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        TS_S32 s32Fd = TS_MPI_VENC_GetFd(pstVencAttr[i].VencChnId);
        if (s32Fd < 0)
        {
            return TS_FAILURE;
        }
        if (maxfd <= s32Fd)
        {
            maxfd = s32Fd;
        }
    }
    return maxfd;
}

TS_S32 TS_Common_VencGetStream(TS_Common_VencAttr_t *pstVencAttr, int *bStart)
{
    TS_S32 maxfd = 0;
    TS_S32 s32Fd[MAX_VENC_CHN];
    TS_S32 s32Ret;
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream = {0};
    int bufsize = ANJ_CAMERA_VIDEO_MAX_SIZE;

    char FileName[256] = {0};
    FILE *pFile = NULL;

    if ((pstVencAttr == NULL) || (bStart == NULL))
    {
        return TS_FAILURE;
    }

    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        s32Fd[i] = TS_MPI_VENC_GetFd(pstVencAttr[i].VencChnId);
        if (s32Fd < 0)
        {
            return TS_FAILURE;
        }
        if (maxfd <= s32Fd[i])
        {
            maxfd = s32Fd[i];
        }
    }

    sprintf(FileName, "/tmp/nfs/stream.h265");

    char *vencBuf = anj_mw_malloc(bufsize);
    if (vencBuf == NULL)
    {
        __ERR("malloc failed!\n");
        return TS_FAILURE;
    }
    while (*bStart)
    {
        fd_set readFds;
        struct timeval stTimeout;

        FD_ZERO(&readFds);
        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            FD_SET(s32Fd[i], &readFds);
        }
        stTimeout.tv_sec = 0;
        stTimeout.tv_usec = 200 * 1000;

        s32Ret = TS_MPI_VENC_Select(maxfd + 1, &readFds, NULL, NULL, &stTimeout);
        if (s32Ret < 0)
        {
            __ERR("TS_MPI_VENC_Select failed with:%#x!\n", s32Ret);
            break;
        }
        else if (s32Ret == 0)
        {
            __ERR("TS_MPI_VENC_Select timeout!\n");
            continue;
        }

        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            if (FD_ISSET(s32Fd[i], &readFds))
            {
                s32Ret = TS_MPI_VENC_QueryStatus(pstVencAttr[i].VencChnId, &stStat);
                if (TS_SUCCESS != s32Ret)
                {
                    __ERR("TS_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", pstVencAttr[i].VencChnId, s32Ret);
                    break;
                }

                if (0 == stStat.u32CurPacks)
                {
                    __ERR("NOTE: Current  frame is NULL!\n");
                    continue;
                }
                /*******************************************************
                 step 2.3 : malloc corresponding number of pack nodes.
                *******************************************************/
                stStream.pstPack = (VENC_PACK_S *)anj_mw_malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                if (NULL == stStream.pstPack)
                {
                    __ERR("malloc stream pack failed!\n");
                    break;
                }

                s32Ret = TS_MPI_VENC_GetStream(pstVencAttr[i].VencChnId, &stStream, 1000);
                if (s32Ret != TS_SUCCESS)
                {
                    continue;
                }
                if (stStream.u32PackCount > 0)
                {
                    int offset = 0;
                    int iskey = 0;
                    int codec_type = MEDIA_CODEC_VIDEO_H265;
                    // memset(vencBuf, 0, bufsize);
                    for (int j = 0; j < stStream.u32PackCount; j++)
                    {
                        int len = stStream.pstPack[j].u32Len - stStream.pstPack[j].u32Offset + offset;
                        if (len > 2 * 1024 * 1024)
                        {
                            __ERR("len:%d > vencBuf len:%d !!!!!\n", len, 2 * 1024 * 1024);
                            TS_MPI_VENC_ReleaseStream(pstVencAttr[i].VencChnId, &stStream);
                            if (TS_SUCCESS != s32Ret)
                            {
                                __ERR("TS_MPI_VENC_ReleaseStream ret 0x%x\n", s32Ret);
                                anj_mw_free(stStream.pstPack);
                                break;
                            }
                            continue;
                        }
                        if (len > bufsize)
                        {
                            vencBuf = anj_mw_realloc(vencBuf, len);
                            __WARN("len:%d > vencBuf len:%d !!!!!\n", len, bufsize);
                            bufsize = len;
                        }
                        memcpy(vencBuf + offset, stStream.pstPack[j].pu8Addr + stStream.pstPack[j].u32Offset, stStream.pstPack[j].u32Len - stStream.pstPack[j].u32Offset);
                        offset += stStream.pstPack[j].u32Len - stStream.pstPack[j].u32Offset;
                    }

                    if ((pstVencAttr[i].eVencType == PT_H264) || (pstVencAttr[i].eVencType == PT_H265) || (pstVencAttr[i].eVencType == PT_MJPEG))
                    {
                        if (pstVencAttr[i].eVencType == PT_H264)
                        {
                            codec_type = MEDIA_CODEC_VIDEO_H264;
                            if ((stStream.pstPack[0].DataType.enH264EType == H264E_NALU_ISLICE) ||
                                (stStream.pstPack[0].DataType.enH264EType == H264E_NALU_IDRSLICE))
                            {
                                iskey = 1;
                            }
                        }
                        else if (pstVencAttr[i].eVencType == PT_H265)
                        {
                            codec_type = MEDIA_CODEC_VIDEO_H265;
                            if ((stStream.pstPack[0].DataType.enH265EType == H265E_NALU_ISLICE) ||
                                (stStream.pstPack[0].DataType.enH265EType == H265E_NALU_IDRSLICE))
                            {
                                iskey = 1;
                            }
                        }
                        else if (pstVencAttr[i].eVencType == PT_MJPEG)
                        {
                            codec_type = MEDIA_CODEC_VIDEO_MJPG;
                            iskey = 1;
                        }

                        /* 仅 chn0：等 I 帧再开始落盘，避免裸流从 P 帧起播不了 */
                        if (i == 0)
                        {
                            if (0 == access("/tmp/save", F_OK))
                            {
                                if (pFile == NULL && iskey)
                                    pFile = fopen(FileName, "wb");
                                if (pFile)
                                    fwrite(vencBuf, 1, offset, pFile);
                            }
                            else if (pFile)
                            {
                                fclose(pFile);
                                pFile = NULL;
                            }
                        }
                        if (pstVencAttr[i].datacb)
                        {
                            pstVencAttr[i].datacb(pstVencAttr[i].VencChnId, iskey, vencBuf, offset, stStream.u32Seq, codec_type, stStream.pstPack[0].u64PTS);
                        }
                    }
                }
                TS_MPI_VENC_ReleaseStream(pstVencAttr[i].VencChnId, &stStream);
                if (TS_SUCCESS != s32Ret)
                {
                    __ERR("TS_MPI_VENC_ReleaseStream ret 0x%x\n", s32Ret);
                    anj_mw_free(stStream.pstPack);
                    break;
                }

                anj_mw_free(stStream.pstPack);
            }
        }
    }
    if (vencBuf)
    {
        anj_mw_free(vencBuf);
    }
    if (pFile)
    {
        fclose(pFile);
        pFile = NULL;
    }

    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        TS_MPI_VENC_CloseFd(pstVencAttr[i].VencChnId);
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_VencRequestIdr(VENC_CHN VencChn)
{
    return TS_MPI_VENC_RequestIDR(VencChn, 1);
}

TS_S32 TS_Common_VencSetGop(VENC_CHN VencChn, int gop)
{
    TS_S32 u32Gop = 0;
    VENC_CHN_ATTR_S stAttr = {0};
    STCHECKRESULT(TS_MPI_VENC_GetChnAttr(VencChn, &stAttr));
    if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
    {
        u32Gop = stAttr.stRcAttr.stH265Cbr.u32Gop;
        stAttr.stRcAttr.stH265Cbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265AVBR)
    {
        u32Gop = stAttr.stRcAttr.stH265AVbr.u32Gop;
        stAttr.stRcAttr.stH265AVbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265VBR)
    {
        u32Gop = stAttr.stRcAttr.stH265Vbr.u32Gop;
        stAttr.stRcAttr.stH265Vbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264CBR)
    {
        u32Gop = stAttr.stRcAttr.stH264Cbr.u32Gop;
        stAttr.stRcAttr.stH264Cbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264AVBR)
    {
        u32Gop = stAttr.stRcAttr.stH264AVbr.u32Gop;
        stAttr.stRcAttr.stH264AVbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264VBR)
    {
        u32Gop = stAttr.stRcAttr.stH264Vbr.u32Gop;
        stAttr.stRcAttr.stH264Vbr.u32Gop = gop;
    }
    if (u32Gop != gop)
    {
        STCHECKRESULT(TS_MPI_VENC_SetChnAttr(VencChn, &stAttr));
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_VencSetBitrate(VENC_CHN VencChn, int Bitrate)
{
    TS_S32 u32BitRate = 0;
    VENC_CHN_ATTR_S stAttr = {0};
    STCHECKRESULT(TS_MPI_VENC_GetChnAttr(VencChn, &stAttr));

    Bitrate = (int)TS_Common_VencClampBitrate((TS_U32)Bitrate);

    if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
    {
        u32BitRate = stAttr.stRcAttr.stH265Cbr.u32BitRate;
        stAttr.stRcAttr.stH265Cbr.u32BitRate = Bitrate;
        stAttr.stRcAttr.stH265Cbr.u32MaxBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265AVBR)
    {
        u32BitRate = stAttr.stRcAttr.stH265AVbr.u32MaxBitRate;
        stAttr.stRcAttr.stH265AVbr.u32MaxBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265VBR)
    {
        u32BitRate = stAttr.stRcAttr.stH265Vbr.u32MaxBitRate;
        stAttr.stRcAttr.stH265Vbr.u32MaxBitRate = Bitrate;
        stAttr.stRcAttr.stH265Vbr.u32AvgBitRate = (TS_U32)Bitrate * 80 / 100;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265QVBR)
    {
        u32BitRate = stAttr.stRcAttr.stH265QVbr.u32TargetBitRate;
        stAttr.stRcAttr.stH265QVbr.u32TargetBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CVBR)
    {
        u32BitRate = stAttr.stRcAttr.stH265CVbr.u32MaxBitRate;
        stAttr.stRcAttr.stH265CVbr.u32MaxBitRate = Bitrate;
        stAttr.stRcAttr.stH265CVbr.u32LongTermMaxBitrate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264CBR)
    {
        u32BitRate = stAttr.stRcAttr.stH264Cbr.u32BitRate;
        stAttr.stRcAttr.stH264Cbr.u32BitRate = Bitrate;
        stAttr.stRcAttr.stH264Cbr.u32MaxBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264AVBR)
    {
        u32BitRate = stAttr.stRcAttr.stH264AVbr.u32MaxBitRate;
        stAttr.stRcAttr.stH264AVbr.u32MaxBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264VBR)
    {
        u32BitRate = stAttr.stRcAttr.stH264Vbr.u32MaxBitRate;
        stAttr.stRcAttr.stH264Vbr.u32MaxBitRate = Bitrate;
        stAttr.stRcAttr.stH264Vbr.u32AvgBitRate = (TS_U32)Bitrate * 80 / 100;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264QVBR)
    {
        u32BitRate = stAttr.stRcAttr.stH264QVbr.u32TargetBitRate;
        stAttr.stRcAttr.stH264QVbr.u32TargetBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264CVBR)
    {
        u32BitRate = stAttr.stRcAttr.stH264CVbr.u32MaxBitRate;
        stAttr.stRcAttr.stH264CVbr.u32MaxBitRate = Bitrate;
        stAttr.stRcAttr.stH264CVbr.u32LongTermMaxBitrate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_MJPEGCBR)
    {
        u32BitRate = stAttr.stRcAttr.stMjpegCbr.u32BitRate;
        stAttr.stRcAttr.stMjpegCbr.u32BitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_MJPEGVBR)
    {
        u32BitRate = stAttr.stRcAttr.stMjpegVbr.u32MaxBitRate;
        stAttr.stRcAttr.stMjpegVbr.u32MaxBitRate = Bitrate;
    }

    if (u32BitRate != Bitrate)
    {
        STCHECKRESULT(TS_MPI_VENC_SetChnAttr(VencChn, &stAttr));
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_VencSetFps(VENC_CHN VencChn, int fps)
{
    TS_S32 u32Fps = 0;
    VENC_CHN_ATTR_S stAttr = {0};
    STCHECKRESULT(TS_MPI_VENC_GetChnAttr(VencChn, &stAttr));

    if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
    {
        u32Fps = stAttr.stRcAttr.stH265Cbr.u32SrcFrameRate;
        stAttr.stRcAttr.stH265Cbr.fr32DstFrameRate = fps;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265AVBR)
    {
        u32Fps = stAttr.stRcAttr.stH265AVbr.u32SrcFrameRate;
        stAttr.stRcAttr.stH265AVbr.fr32DstFrameRate = fps;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265VBR)
    {
        u32Fps = stAttr.stRcAttr.stH265Vbr.u32SrcFrameRate;
        stAttr.stRcAttr.stH265Vbr.fr32DstFrameRate = fps;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264CBR)
    {
        u32Fps = stAttr.stRcAttr.stH264Cbr.u32SrcFrameRate;
        stAttr.stRcAttr.stH264Cbr.fr32DstFrameRate = fps;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264AVBR)
    {
        u32Fps = stAttr.stRcAttr.stH264AVbr.u32SrcFrameRate;
        stAttr.stRcAttr.stH264AVbr.fr32DstFrameRate = fps;
    }
    else if (stAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264VBR)
    {
        u32Fps = stAttr.stRcAttr.stH264Vbr.u32SrcFrameRate;
        stAttr.stRcAttr.stH264Vbr.fr32DstFrameRate = fps;
    }

    if (u32Fps != fps)
    {
        STCHECKRESULT(TS_MPI_VENC_SetChnAttr(VencChn, &stAttr));
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_VencSetChnAttr(VENC_CHN VencChn, TS_Common_VencAttr_t *pVencAttr)
{
    TS_S32 s32Ret;
    VENC_CHN_ATTR_S stVencChnAttr = {0};
    VENC_RECV_PIC_PARAM_S stRecvParam = {-1};

    if (!pVencAttr)
    {
        return TS_FAILURE;
    }

    pVencAttr->u32BitRate = TS_Common_VencClampBitrate(pVencAttr->u32BitRate);

    s32Ret = TS_MPI_VENC_StopRecvFrame(VencChn);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VENC_StopRecvFrame VencChn[%d] failed %#x!\n", VencChn, s32Ret);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VENC_GetChnAttr VencChn[%d] failed %#x!\n", VencChn, s32Ret);
        TS_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
        return TS_FAILURE;
    }

    stVencChnAttr.stVencAttr.u32Profile = pVencAttr->u32Profile;

    if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CBR)
    {
        stVencChnAttr.stRcAttr.stH265Cbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH265Cbr.u32BitRate = pVencAttr->u32BitRate;
        stVencChnAttr.stRcAttr.stH265Cbr.u32MaxBitRate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265AVBR)
    {
        stVencChnAttr.stRcAttr.stH265AVbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH265AVbr.u32MaxBitRate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265VBR)
    {
        stVencChnAttr.stRcAttr.stH265Vbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH265Vbr.u32MaxBitRate = pVencAttr->u32BitRate;
        stVencChnAttr.stRcAttr.stH265Vbr.u32AvgBitRate = pVencAttr->u32BitRate * 80 / 100;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265QVBR)
    {
        stVencChnAttr.stRcAttr.stH265QVbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH265QVbr.u32TargetBitRate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H265CVBR)
    {
        stVencChnAttr.stRcAttr.stH265CVbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH265CVbr.u32MaxBitRate = pVencAttr->u32BitRate;
        stVencChnAttr.stRcAttr.stH265CVbr.u32LongTermMaxBitrate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264CBR)
    {
        stVencChnAttr.stRcAttr.stH264Cbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH264Cbr.u32BitRate = pVencAttr->u32BitRate;
        stVencChnAttr.stRcAttr.stH264Cbr.u32MaxBitRate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264AVBR)
    {
        stVencChnAttr.stRcAttr.stH264AVbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH264AVbr.u32MaxBitRate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264VBR)
    {
        stVencChnAttr.stRcAttr.stH264Vbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH264Vbr.u32MaxBitRate = pVencAttr->u32BitRate;
        stVencChnAttr.stRcAttr.stH264Vbr.u32AvgBitRate = pVencAttr->u32BitRate * 80 / 100;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264QVBR)
    {
        stVencChnAttr.stRcAttr.stH264QVbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH264QVbr.u32TargetBitRate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_H264CVBR)
    {
        stVencChnAttr.stRcAttr.stH264CVbr.u32Gop = pVencAttr->u32Gop;
        stVencChnAttr.stRcAttr.stH264CVbr.u32MaxBitRate = pVencAttr->u32BitRate;
        stVencChnAttr.stRcAttr.stH264CVbr.u32LongTermMaxBitrate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_MJPEGCBR)
    {
        stVencChnAttr.stRcAttr.stMjpegCbr.u32BitRate = pVencAttr->u32BitRate;
    }
    else if (stVencChnAttr.stRcAttr.enRcMode == VENC_RC_MODE_MJPEGVBR)
    {
        stVencChnAttr.stRcAttr.stMjpegVbr.u32MaxBitRate = pVencAttr->u32BitRate;
    }

    s32Ret = TS_MPI_VENC_SetChnAttr(VencChn, &stVencChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VENC_SetChnAttr VencChn[%d] failed %#x!\n", VencChn, s32Ret);
        TS_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
        return TS_FAILURE;
    }

    s32Ret = TS_Common_VencSetRcQp(VencChn, stVencChnAttr.stRcAttr.enRcMode,
                                   pVencAttr->u32MinQp, pVencAttr->u32MaxQp);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_VencSetRcQp failed with %#x!\n", s32Ret);
        TS_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VENC_StartRecvFrame VencChn[%d] failed %#x!\n", VencChn, s32Ret);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_VENC_Stop(VENC_CHN VencChn)
{
    TS_S32 s32Ret;
    /******************************************
     step 1:  Stop Recv Pictures
    ******************************************/
    s32Ret = TS_MPI_VENC_StopRecvFrame(VencChn);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VENC_StopRecvFrame VencChn[%d] failed with %#x!\n", VencChn, s32Ret);
        return TS_FAILURE;
    }
    __ERR("TS_MPI_VENC_StopRecvFrame VencChn[%d] success!\n", VencChn);

    /******************************************
     step 2:  Distroy Venc Channel
    ******************************************/
    s32Ret = TS_MPI_VENC_DestroyChn(VencChn);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VENC_DestroyChn VencChn[%d] failed with %#x!\n", VencChn, s32Ret);
        return TS_FAILURE;
    }
    __ERR("TS_MPI_VENC_DestroyChn VencChn[%d] success!\n", VencChn);
    return TS_SUCCESS;
}

/******************************************************************************
 * funciton : Start snap
 ******************************************************************************/

TS_S32 TS_Common_VENC_SnapStart(VENC_CHN VencChn, SIZE_S *pstSize, TS_BOOL bSupportDCF)
{
    TS_S32 s32Ret;
    VENC_CHN_ATTR_S stVencChnAttr = {0};
    /******************************************
     step 1:  Create Venc Channel
    ******************************************/
    stVencChnAttr.stVencAttr.enType = PT_JPEG;
    stVencChnAttr.stVencAttr.u32Profile = 0;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = pstSize->u32Width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = pstSize->u32Height;
    stVencChnAttr.stVencAttr.u32PicWidth = pstSize->u32Width;
    stVencChnAttr.stVencAttr.u32PicHeight = pstSize->u32Height;
    stVencChnAttr.stVencAttr.u32BufSize = pstSize->u32Width * pstSize->u32Height;
    stVencChnAttr.stVencAttr.bByFrame = TS_TRUE; /*get stream mode is field mode  or frame mode*/
    stVencChnAttr.stVencAttr.stAttrJpege.bSupportDCF = bSupportDCF;
    // stVencChnAttr.stVencAttr.stAttrJpege.bSupportXMP = TS_FALSE;
    stVencChnAttr.stVencAttr.stAttrJpege.stMPFCfg.u8LargeThumbNailNum = 0;
    stVencChnAttr.stVencAttr.stAttrJpege.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
    stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    stVencChnAttr.stGopAttr.stNormalP.s32IPQpDelta = 0;

    s32Ret = TS_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VENC_CreateChn [%d] faild with %#x!\n", VencChn, s32Ret);
        return s32Ret;
    }
    return TS_SUCCESS;
}

/******************************************************************************
 * funciton : Stop snap
 ******************************************************************************/
TS_S32 TS_Common_VENC_SnapStop(VENC_CHN VencChn)
{
    TS_S32 s32Ret;
    s32Ret = TS_MPI_VENC_StopRecvFrame(VencChn);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VENC_StopRecvPic vechn[%d] failed with %#x!\n", VencChn, s32Ret);
    }
    s32Ret = TS_MPI_VENC_DestroyChn(VencChn);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VENC_DestroyChn vechn[%d] failed with %#x!\n", VencChn, s32Ret);
        return TS_FAILURE;
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_VENC_SnapProcess(VENC_CHN VencChn, TS_U32 SnapCnt, TS_BOOL bSaveJpg)
{
    VENC_STREAM_S stStream;
    TS_S32 s32Ret;
    VENC_RECV_PIC_PARAM_S stRecvParam;
    TS_U32 i;

    /******************************************
     step 1:  Start Recv Venc Pictures
    ******************************************/
    stRecvParam.s32RecvPicNum = SnapCnt;
    s32Ret = TS_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
        return TS_FAILURE;
    }

    /******************************************
     step 2:  recv picture
    ******************************************/
    for (i = 0; i < SnapCnt; i++)
    {
        stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
        if (NULL == stStream.pstPack)
        {
            __ERR("malloc memory failed!\n");
            return TS_FAILURE;
        }

        stStream.u32PackCount = 1;
        s32Ret = TS_MPI_VENC_GetStream(VencChn, &stStream, -1);
        if (TS_SUCCESS != s32Ret)
        {
            __ERR("TS_MPI_VENC_GetStream failed with %#x!\n", s32Ret);

            free(stStream.pstPack);
            stStream.pstPack = NULL;
            return TS_FAILURE;
        }

        s32Ret = TS_MPI_VENC_ReleaseStream(VencChn, &stStream);
        if (TS_SUCCESS != s32Ret)
        {
            __ERR("TS_MPI_VENC_ReleaseStream failed with %#x!\n", s32Ret);

            free(stStream.pstPack);
            stStream.pstPack = NULL;

            return TS_FAILURE;
        }

        free(stStream.pstPack);
        stStream.pstPack = NULL;
    }
    /******************************************
     step 4:  stop recv picture
    ******************************************/
    s32Ret = TS_MPI_VENC_StopRecvFrame(VencChn);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VENC_StopRecvPic failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return TS_SUCCESS;
}
