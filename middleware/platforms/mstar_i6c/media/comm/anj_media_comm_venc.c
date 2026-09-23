#include "anj_mw_media_common.h"
#include "media_util.h"

#ifdef _USE_MODULE_AOV_
#include "anj_mw_aov_common.h"
#endif

#define SENSOR0_MAIN_VENC_CHN 0
#define SENSOR1_MAIN_VENC_CHN 2

/*
编码的等级。
H264:取值范围：[0, 3]。
0：Baseline。
1：Main Profile。
2：High Profile。
3：svc-t。

H265:取值范围：0。
0：Main Profile。
*/
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

MI_S32 ST_Common_VencInit(ST_Common_VencAttr_t *pVencAttr)
{
    MI_VENC_InitParam_t stInitParam;
    memset(&stInitParam, 0, sizeof(MI_VENC_InitParam_t));

    stInitParam.u32MaxWidth = pVencAttr->stVencRes.u16Width;
    stInitParam.u32MaxHeight = pVencAttr->stVencRes.u16Height;

    STCHECKRESULT(MI_VENC_CreateDev(pVencAttr->VencDevId, &stInitParam));

    return MI_SUCCESS;
}

MI_S32 ST_Common_VencUnInit(MI_VENC_DEV VencDevId)
{
    STCHECKRESULT(MI_VENC_DestroyDev(VencDevId));
    return MI_SUCCESS;
}

MI_S32 ST_Common_VencCreateChannel(ST_Common_VencAttr_t *pVencAttr)
{
    if (pVencAttr == NULL)
    {
        __ERR("pVencAttr is NULL!\n");

        return MI_FAILED;
    }

    MI_VENC_DEV VencDevId = pVencAttr->VencDevId;
    MI_VENC_CHN VencChn = pVencAttr->VencChnId;

    MI_VENC_ChnAttr_t stVencChnAttr;
    memset(&stVencChnAttr, 0, sizeof(MI_VENC_ChnAttr_t));
    MI_VENC_RcParam_t stRcParam;
    memset(&stRcParam, 0, sizeof(MI_VENC_RcParam_t));
    // STCHECKRESULT(MI_VENC_GetRcParam(VencDevId, VencChn, &stRcParam));

    stVencChnAttr.stVeAttr.eType = pVencAttr->eVencType;
    stVencChnAttr.stRcAttr.eRcMode = pVencAttr->eRcMode;

    __INFO("VencChn:%d width:%d height:%d bufsize:%d\n", VencChn, pVencAttr->stVencRes.u16Width, pVencAttr->stVencRes.u16Height, pVencAttr->u32Bufsize);
    switch (stVencChnAttr.stVeAttr.eType)
    {
    case E_MI_VENC_MODTYPE_H264E:
    {
        stVencChnAttr.stVeAttr.stAttrH264e.u32PicWidth = pVencAttr->stVencRes.u16Width;
        stVencChnAttr.stVeAttr.stAttrH264e.u32PicHeight = pVencAttr->stVencRes.u16Height;
        stVencChnAttr.stVeAttr.stAttrH264e.u32MaxPicWidth = pVencAttr->stVencRes.u16Width;
        stVencChnAttr.stVeAttr.stAttrH264e.u32MaxPicHeight = pVencAttr->stVencRes.u16Height;
        stVencChnAttr.stVeAttr.stAttrH264e.u32BufSize = pVencAttr->u32Bufsize;
        stVencChnAttr.stVeAttr.stAttrH264e.bByFrame = TRUE;
        stVencChnAttr.stVeAttr.stAttrH264e.u32Profile = getEncodeProfile(pVencAttr->u32Profile);
        if (stVencChnAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264CBR)
        {
            stVencChnAttr.stRcAttr.stAttrH264Cbr.u32BitRate = (pVencAttr->u32BitRate << 10);
            stVencChnAttr.stRcAttr.stAttrH264Cbr.u32Gop = pVencAttr->u32Gop;
            stVencChnAttr.stRcAttr.stAttrH264Cbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stVencChnAttr.stRcAttr.stAttrH264Cbr.u32SrcFrmRateDen = 1;
            stVencChnAttr.stRcAttr.stAttrH264Cbr.u32StatTime = 0;
            stVencChnAttr.stRcAttr.stAttrH264Cbr.u32FluctuateLevel = 0;

            stRcParam.stParamH264Cbr.u32MaxQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH264Cbr.u32MinQp = pVencAttr->u32MinQp;
            stRcParam.stParamH264Cbr.u32MaxIQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH264Cbr.u32MinIQp = pVencAttr->u32MinQp;
            stRcParam.stParamH264Cbr.u32MaxIPProp = 20;
            stRcParam.stParamH264Cbr.s32IPQPDelta = -4;
        }
        else if (stVencChnAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264VBR)
        {
            stVencChnAttr.stRcAttr.stAttrH264Vbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stVencChnAttr.stRcAttr.stAttrH264Vbr.u32Gop = pVencAttr->u32Gop;
            stVencChnAttr.stRcAttr.stAttrH264Vbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stVencChnAttr.stRcAttr.stAttrH264Vbr.u32SrcFrmRateDen = 1;
            stVencChnAttr.stRcAttr.stAttrH264Vbr.u32StatTime = 0;
            stVencChnAttr.stRcAttr.stAttrH264Vbr.u32MinQp = pVencAttr->u32MinQp;
            stVencChnAttr.stRcAttr.stAttrH264Vbr.u32MaxQp = pVencAttr->u32MaxQp;

            stRcParam.stParamH264VBR.s32ChangePos = 80;
            stRcParam.stParamH264VBR.u32MaxIQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH264VBR.u32MinIQp = pVencAttr->u32MinQp;
            stRcParam.stParamH264VBR.u32MaxIPProp = 12;
            stRcParam.stParamH264VBR.s32IPQPDelta = pVencAttr->s32IPQPDelta;
            stRcParam.stParamH264VBR.u32MaxISize = pVencAttr->u32MaxISize;
            stRcParam.stParamH264VBR.u32MaxPSize = pVencAttr->u32MaxPSize;
        }
        else if (stVencChnAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264AVBR)
        {
            stVencChnAttr.stRcAttr.stAttrH264Avbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stVencChnAttr.stRcAttr.stAttrH264Avbr.u32Gop = pVencAttr->u32Gop;
            stVencChnAttr.stRcAttr.stAttrH264Avbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stVencChnAttr.stRcAttr.stAttrH264Avbr.u32SrcFrmRateDen = 1;
            stVencChnAttr.stRcAttr.stAttrH264Avbr.u32StatTime = 0;
            stVencChnAttr.stRcAttr.stAttrH264Avbr.u32MinQp = pVencAttr->u32MinQp;
            stVencChnAttr.stRcAttr.stAttrH264Avbr.u32MaxQp = pVencAttr->u32MaxQp;

            stRcParam.stParamH264Avbr.s32ChangePos = 80;
            stRcParam.stParamH264Avbr.u32MaxIQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH264Avbr.u32MinIQp = pVencAttr->u32MinQp;
            stRcParam.stParamH264Avbr.u32MaxIPProp = 12;
            stRcParam.stParamH264Avbr.s32IPQPDelta = pVencAttr->s32IPQPDelta;
            stRcParam.stParamH264Avbr.u32MinStillPercent = 25;
        }
        else
        {
            __ERR("unsupport rcmode:%d\n", stVencChnAttr.stRcAttr.eRcMode);
            return -1;
        }
    }
    break;

    case E_MI_VENC_MODTYPE_H265E:
    {
        stVencChnAttr.stVeAttr.stAttrH265e.u32PicWidth = pVencAttr->stVencRes.u16Width;
        stVencChnAttr.stVeAttr.stAttrH265e.u32PicHeight = pVencAttr->stVencRes.u16Height;
        stVencChnAttr.stVeAttr.stAttrH265e.u32MaxPicWidth = pVencAttr->stVencRes.u16Width;
        stVencChnAttr.stVeAttr.stAttrH265e.u32MaxPicHeight = pVencAttr->stVencRes.u16Height;
        stVencChnAttr.stVeAttr.stAttrH265e.u32BufSize = pVencAttr->u32Bufsize;
        stVencChnAttr.stVeAttr.stAttrH265e.bByFrame = TRUE;
        stVencChnAttr.stVeAttr.stAttrH265e.u32Profile = 0;//getEncodeProfile(pVencAttr->u32Profile);
        stVencChnAttr.stVeAttr.stAttrH265e.u32BFrameNum = 0;
        stVencChnAttr.stVeAttr.stAttrH265e.u32RefNum = 1;
        if (stVencChnAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265CBR)
        {
            stVencChnAttr.stRcAttr.stAttrH265Cbr.u32BitRate = (pVencAttr->u32BitRate << 10);
            stVencChnAttr.stRcAttr.stAttrH265Cbr.u32Gop = pVencAttr->u32Gop;
            stVencChnAttr.stRcAttr.stAttrH265Cbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stVencChnAttr.stRcAttr.stAttrH265Cbr.u32SrcFrmRateDen = 1;
            stVencChnAttr.stRcAttr.stAttrH265Cbr.u32StatTime = 0;
            stVencChnAttr.stRcAttr.stAttrH265Cbr.u32FluctuateLevel = 0;

            stRcParam.stParamH265Cbr.u32MaxQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH265Cbr.u32MinQp = pVencAttr->u32MinQp;
            stRcParam.stParamH265Cbr.u32MaxIQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH265Cbr.u32MinIQp = pVencAttr->u32MinQp;
            stRcParam.stParamH265Cbr.u32MaxIPProp = 20;
            stRcParam.stParamH265Cbr.s32IPQPDelta = pVencAttr->s32IPQPDelta;
            // stRcParam.stParamH265Cbr.u32MaxISize = 0;
            // stRcParam.stParamH265Cbr.u32MaxPSize = 0;
        }
        else if (stVencChnAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265VBR)
        {
            stVencChnAttr.stRcAttr.stAttrH265Vbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stVencChnAttr.stRcAttr.stAttrH265Vbr.u32Gop = pVencAttr->u32Gop;
            stVencChnAttr.stRcAttr.stAttrH265Vbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stVencChnAttr.stRcAttr.stAttrH265Vbr.u32SrcFrmRateDen = 1;
            stVencChnAttr.stRcAttr.stAttrH265Vbr.u32StatTime = 0;
            stVencChnAttr.stRcAttr.stAttrH265Vbr.u32MinQp = pVencAttr->u32MinQp;
            stVencChnAttr.stRcAttr.stAttrH265Vbr.u32MaxQp = pVencAttr->u32MaxQp;

            stRcParam.stParamH265Vbr.s32ChangePos = 80;
            stRcParam.stParamH265Vbr.u32MaxIQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH265Vbr.u32MinIQp = pVencAttr->u32MinQp;
            stRcParam.stParamH265Vbr.u32MaxIPProp = 20;
            stRcParam.stParamH265Vbr.s32IPQPDelta = pVencAttr->s32IPQPDelta;
            stRcParam.stParamH265Vbr.u32MaxISize = pVencAttr->u32MaxISize;
            stRcParam.stParamH265Vbr.u32MaxPSize = pVencAttr->u32MaxPSize;
        }
        else if (stVencChnAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265AVBR)
        {
            stVencChnAttr.stRcAttr.stAttrH265Avbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stVencChnAttr.stRcAttr.stAttrH265Avbr.u32Gop = pVencAttr->u32Gop;
            stVencChnAttr.stRcAttr.stAttrH265Avbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stVencChnAttr.stRcAttr.stAttrH265Avbr.u32SrcFrmRateDen = 1;
            stVencChnAttr.stRcAttr.stAttrH265Avbr.u32StatTime = 0;
            stVencChnAttr.stRcAttr.stAttrH265Avbr.u32MinQp = pVencAttr->u32MinQp;
            stVencChnAttr.stRcAttr.stAttrH265Avbr.u32MaxQp = pVencAttr->u32MaxQp;

            stRcParam.stParamH265Avbr.s32ChangePos = 80;
            stRcParam.stParamH265Avbr.u32MaxIQp = pVencAttr->u32MaxQp;
            stRcParam.stParamH265Avbr.u32MinIQp = pVencAttr->u32MinQp;
            stRcParam.stParamH265Avbr.u32MaxIPProp = 20;
            stRcParam.stParamH265Avbr.s32IPQPDelta = pVencAttr->s32IPQPDelta;
            // stRcParam.stParamH265Avbr.u32MinStillPercent = 25;
            stRcParam.stParamH265Avbr.u32MaxISize = pVencAttr->u32MaxISize;
            stRcParam.stParamH265Avbr.u32MaxPSize = pVencAttr->u32MaxPSize;
        }
        else
        {
            __ERR("unsupport rcmode:%d\n", stVencChnAttr.stRcAttr.eRcMode);
            return -1;
        }
    }
    break;

    case E_MI_VENC_MODTYPE_JPEGE:
    {
        stVencChnAttr.stVeAttr.stAttrJpeg.u32PicWidth = pVencAttr->stVencRes.u16Width;
        stVencChnAttr.stVeAttr.stAttrJpeg.u32PicHeight = pVencAttr->stVencRes.u16Height;
        stVencChnAttr.stVeAttr.stAttrJpeg.u32MaxPicWidth = pVencAttr->stVencRes.u16Width;
        stVencChnAttr.stVeAttr.stAttrJpeg.u32MaxPicHeight = pVencAttr->stVencRes.u16Height;
        stVencChnAttr.stVeAttr.stAttrJpeg.u32BufSize = pVencAttr->u32Bufsize;
        if (stVencChnAttr.stRcAttr.eRcMode != E_MI_VENC_RC_MODE_MJPEGFIXQP)
        {
            __ERR("unsupport rcmode:%d\n", stVencChnAttr.stRcAttr.eRcMode);
            return -1;
        }
        stVencChnAttr.stRcAttr.stAttrMjpegFixQp.u32Qfactor = pVencAttr->u32Qfactor;
        stVencChnAttr.stRcAttr.stAttrMjpegFixQp.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
        stVencChnAttr.stRcAttr.stAttrMjpegFixQp.u32SrcFrmRateDen = 1;
    }
    break;

    default:
    {
        __ERR("Invalid venc type:%d\n", pVencAttr->eVencType);

        return MI_FAILED;
    }
    }

    STCHECKRESULT(MI_VENC_CreateChn(VencDevId, VencChn, &stVencChnAttr));
    STCHECKRESULT(MI_VENC_SetRcParam(VencDevId, VencChn, &stRcParam));

    return MI_SUCCESS;
}

MI_S32 ST_Common_VencGetStream(MI_VENC_CHN VencChn, MI_VENC_ModType_e eVencType, ST_Common_Venc_DataCb datacb, int *bStart)
{
    MI_VENC_DEV VencDevId = 0;
    int GetCount = 0;
    char FileName[192] = {'0'};
    FILE *pFile = NULL;

    if ((eVencType == E_MI_VENC_MODTYPE_H264E) || (eVencType == E_MI_VENC_MODTYPE_H265E))
    {
        VencDevId = MI_VENC_DEV_ID_H264_H265_0;
        GetCount = 1000;

        STCHECKRESULT(MI_VENC_RequestIdr(VencDevId, VencChn, TRUE));

        if (eVencType == E_MI_VENC_MODTYPE_H264E)
        {
            sprintf(FileName, "/tmp/nfs/Channel%d%s", VencChn, "stream.h264");
        }
        else
        {
            sprintf(FileName, "/tmp/nfs/Channel%d%s", VencChn, "stream.h265");
        }

#if 0
        pFile = fopen(FileName, "wb");
        if (pFile == NULL)
        {
            return E_MI_ERR_FAILED;
        }
#endif
    }
    else if (eVencType == E_MI_VENC_MODTYPE_JPEGE)
    {
        VencDevId = MI_VENC_DEV_ID_JPEG_0;
        GetCount = 3;
    }
    else
    {
        __ERR("Invalid venc type:%d\n", eVencType);

        return MI_FAILED;
    }

#if defined(SUPPORT_AOV)
    aov_com_wait_notify_encode_start();
#endif

    MI_VENC_ChnStat_t stStat;
    MI_VENC_Stream_t stStream;
    fd_set read_fds;

    MI_S32 s32VencFd = MI_VENC_GetFd(VencDevId, VencChn);

    struct timeval TimeoutVal;

    int bufsize = IsMainStream(VencChn) ? ANJ_CAMERA_VIDEO_MAX_SIZE : ANJ_CAMERA_VIDEO_SUB_MAX_SIZE;
    char *vencBuf = anj_mw_malloc(bufsize);
    if (vencBuf == NULL)
    {
        __ERR("VencChn:%d malloc failed!\n", VencChn);
        return MI_FAILED;
    }

    while (bStart && *bStart && GetCount)
    {
        FD_ZERO(&read_fds);
        FD_SET(s32VencFd, &read_fds);
        TimeoutVal.tv_sec = 0;
        TimeoutVal.tv_usec = 500 * 1000;

        MI_S32 s32Ret = select(s32VencFd + 1, &read_fds, NULL, NULL, &TimeoutVal);

        if (s32Ret < 0)
        {
            printf("select err\n");
            break;
        }
        else if (0 == s32Ret)
        {
            __INFO("oh no!!! select time out, maybe it is no data! try it again! %d, %d, %ld, %ld\n", GetCount, s32VencFd, TimeoutVal.tv_sec, TimeoutVal.tv_usec);

#ifdef _USE_MODULE_AOV_
            aov_com_notify_encode_done();
#endif
            usleep(100 * 1000);
            continue;
        }
        else
        {
            if (FD_ISSET(s32VencFd, &read_fds))
            {
                s32Ret = MI_VENC_Query(VencDevId, VencChn, &stStat);
                if (s32Ret != MI_SUCCESS)
                {
                    __ERR("MI_VENC_Query faild! s32Ret:%d\n", s32Ret);
                    break;
                }

                if (0 == stStat.u32CurPacks)
                {
                    usleep(500 * 1000);
                    continue;
                }

                if (1 != stStat.u32CurPacks)
                    __INFO("stStat.u32CurPacks:%d\n", stStat.u32CurPacks);

                stStream.pstPack = (MI_VENC_Pack_t *)anj_mw_malloc(sizeof(MI_VENC_Pack_t) * stStat.u32CurPacks);
                if (NULL == stStream.pstPack)
                {
                    __ERR("anj_mw_malloc faild!\n");
                    break;
                }

                stStream.u32PackCount = stStat.u32CurPacks;

                s32Ret = MI_VENC_GetStream(VencDevId, VencChn, &stStream, -1);
                if (MI_SUCCESS != s32Ret)
                {
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;

                    break;
                }

                if (eVencType == E_MI_VENC_MODTYPE_JPEGE)
                {
                    sprintf(FileName, "/tmp/nfs/%dChannel%d%s", GetCount, VencChn, ".jpg");

                    pFile = fopen(FileName, "wb");
                    if (pFile == NULL)
                    {
                        break;
                    }
                }
                if (0 == access("/tmp/save", F_OK))
                {
                    if (pFile == NULL)
                        pFile = fopen(FileName, "wb");
                }
                else
                {
                    if (pFile)
                    {
                        fclose(pFile);
                        pFile = NULL;
                    }
                }
                // stFrameInfo.frameBuf = vencBuf;
                // stFrameInfo.frameParam.frameLen = 0;

                int offset = 0;
                int iskey = 0;
                int codec_type = MEDIA_CODEC_VIDEO_H265;
                memset(vencBuf, 0, bufsize);
                for (int i = 0; i < stStream.u32PackCount; i++)
                {
                    if (pFile)
                    {
                        fwrite(stStream.pstPack[i].pu8Addr + stStream.pstPack[i].u32Offset, 1,
                               stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset, pFile);
                    }
                    int len = stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset + offset;
                    if (len > bufsize)
                    {
                        vencBuf = anj_mw_realloc(vencBuf, len);
                        __WARN("len:%d > vencBuf len:%d !!!!!\n", len, bufsize);
                        bufsize = len;
                    }
                    memcpy(vencBuf + offset, stStream.pstPack[i].pu8Addr + stStream.pstPack[i].u32Offset, stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset);
                    offset += stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset;
                }

                if ((eVencType == E_MI_VENC_MODTYPE_H264E) || (eVencType == E_MI_VENC_MODTYPE_H265E))
                {
                    if (eVencType == E_MI_VENC_MODTYPE_H264E)
                    {
                        codec_type = MEDIA_CODEC_VIDEO_H264;
                        if (E_MI_VENC_H264E_NALU_ISLICE == stStream.pstPack[0].stDataType.eH264EType)
                        {
                            iskey = 1;
                        }
                    }
                    else if (eVencType == E_MI_VENC_MODTYPE_H265E)
                    {
                        codec_type = MEDIA_CODEC_VIDEO_H265;
                        if (E_MI_VENC_H265E_NALU_ISLICE == stStream.pstPack[0].stDataType.eH265EType)
                        {
                            iskey = 1;
                        }
                    }
                    if (datacb)
                    {
                        datacb(VencChn, iskey, vencBuf, offset, stStream.u32Seq, codec_type, stStream.pstPack[0].u64PTS);
                    }
                }
                else if (eVencType == E_MI_VENC_MODTYPE_JPEGE)
                {
                    if (pFile)
                    {
                        fclose(pFile);
                        pFile = NULL;
                    }
                }

                s32Ret = MI_VENC_ReleaseStream(VencDevId, VencChn, &stStream);
                if (MI_SUCCESS != s32Ret)
                {
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;

                    break;
                }

                free(stStream.pstPack);
                stStream.pstPack = NULL;
            }
        }
    }

    if (vencBuf)
    {
        anj_mw_free(vencBuf);
    }
    STCHECKRESULT(MI_VENC_CloseFd(VencDevId, s32VencFd));

    if ((eVencType == E_MI_VENC_MODTYPE_H264E) || (eVencType == E_MI_VENC_MODTYPE_H265E))
    {
        if (pFile)
        {
            fclose(pFile);
            pFile = NULL;
        }
    }

#ifdef _USE_MODULE_AOV_
    aov_com_notify_encode_done();
#endif

    return MI_SUCCESS;
}

MI_S32 ST_Common_VencRequestIdr(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn)
{
    __INFO("Request IDR!\n");
    STCHECKRESULT(MI_VENC_RequestIdr(VencDevId, VencChn, TRUE));
    return MI_SUCCESS;
}

MI_S32 ST_Common_VencSetGop(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, int gop)
{
    MI_S32 u32Gop = 0;
    MI_VENC_ChnAttr_t stAttr = {0};
    STCHECKRESULT(MI_VENC_GetChnAttr(VencDevId, VencChn, &stAttr));
    if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265CBR)
    {
        u32Gop = stAttr.stRcAttr.stAttrH265Cbr.u32Gop;
        stAttr.stRcAttr.stAttrH265Cbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265AVBR)
    {
        u32Gop = stAttr.stRcAttr.stAttrH265Avbr.u32Gop;
        stAttr.stRcAttr.stAttrH265Avbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265VBR)
    {
        u32Gop = stAttr.stRcAttr.stAttrH265Vbr.u32Gop;
        stAttr.stRcAttr.stAttrH265Vbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264CBR)
    {
        u32Gop = stAttr.stRcAttr.stAttrH264Cbr.u32Gop;
        stAttr.stRcAttr.stAttrH264Cbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264AVBR)
    {
        u32Gop = stAttr.stRcAttr.stAttrH264Avbr.u32Gop;
        stAttr.stRcAttr.stAttrH264Avbr.u32Gop = gop;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264VBR)
    {
        u32Gop = stAttr.stRcAttr.stAttrH264Vbr.u32Gop;
        stAttr.stRcAttr.stAttrH264Vbr.u32Gop = gop;
    }
    if (u32Gop != gop)
    {
        STCHECKRESULT(MI_VENC_SetChnAttr(VencDevId, VencChn, &stAttr));
    }
    return MI_SUCCESS;
}

MI_S32 ST_Common_VencSetBitrate(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, int Bitrate)
{
    MI_S32 u32Bitrate = 0;
    MI_VENC_ChnAttr_t stAttr = {0};
    STCHECKRESULT(MI_VENC_GetChnAttr(VencDevId, VencChn, &stAttr));

    if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265CBR)
    {
        u32Bitrate = stAttr.stRcAttr.stAttrH265Cbr.u32BitRate;
        stAttr.stRcAttr.stAttrH265Cbr.u32BitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265AVBR)
    {
        u32Bitrate = stAttr.stRcAttr.stAttrH265Avbr.u32MaxBitRate;
        stAttr.stRcAttr.stAttrH265Avbr.u32MaxBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265VBR)
    {
        u32Bitrate = stAttr.stRcAttr.stAttrH265Vbr.u32MaxBitRate;
        stAttr.stRcAttr.stAttrH265Vbr.u32MaxBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264CBR)
    {
        u32Bitrate = stAttr.stRcAttr.stAttrH264Cbr.u32BitRate;
        stAttr.stRcAttr.stAttrH264Cbr.u32BitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264AVBR)
    {
        u32Bitrate = stAttr.stRcAttr.stAttrH264Avbr.u32MaxBitRate;
        stAttr.stRcAttr.stAttrH264Avbr.u32MaxBitRate = Bitrate;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264VBR)
    {
        u32Bitrate = stAttr.stRcAttr.stAttrH264Vbr.u32MaxBitRate;
        stAttr.stRcAttr.stAttrH264Vbr.u32MaxBitRate = Bitrate;
    }

    if (u32Bitrate != Bitrate)
    {
        STCHECKRESULT(MI_VENC_SetChnAttr(VencDevId, VencChn, &stAttr));
    }
    return MI_SUCCESS;
}

MI_S32 ST_Common_VencSetFps(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, int fps)
{
    MI_S32 u32Fps = 0;
    MI_VENC_ChnAttr_t stAttr = {0};
    STCHECKRESULT(MI_VENC_GetChnAttr(VencDevId, VencChn, &stAttr));

    if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265CBR)
    {
        u32Fps = stAttr.stRcAttr.stAttrH265Cbr.u32SrcFrmRateNum;
        stAttr.stRcAttr.stAttrH265Cbr.u32SrcFrmRateNum = fps;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265AVBR)
    {
        u32Fps = stAttr.stRcAttr.stAttrH265Avbr.u32SrcFrmRateNum;
        stAttr.stRcAttr.stAttrH265Avbr.u32SrcFrmRateNum = fps;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265VBR)
    {
        u32Fps = stAttr.stRcAttr.stAttrH265Vbr.u32SrcFrmRateNum;
        stAttr.stRcAttr.stAttrH265Vbr.u32SrcFrmRateNum = fps;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264CBR)
    {
        u32Fps = stAttr.stRcAttr.stAttrH264Cbr.u32SrcFrmRateNum;
        stAttr.stRcAttr.stAttrH264Cbr.u32SrcFrmRateNum = fps;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264AVBR)
    {
        u32Fps = stAttr.stRcAttr.stAttrH264Avbr.u32SrcFrmRateNum;
        stAttr.stRcAttr.stAttrH264Avbr.u32SrcFrmRateNum = fps;
    }
    else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264VBR)
    {
        u32Fps = stAttr.stRcAttr.stAttrH264Vbr.u32SrcFrmRateNum;
        stAttr.stRcAttr.stAttrH264Vbr.u32SrcFrmRateNum = fps;
    }

    if (u32Fps != fps)
    {
        STCHECKRESULT(MI_VENC_SetChnAttr(VencDevId, VencChn, &stAttr));
    }
    return MI_SUCCESS;
}

MI_S32 ST_Common_VencSetChnAttr(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, ST_Common_VencAttr_t *pVencAttr)
{
    MI_VENC_ChnAttr_t stAttr = {0};
    MI_VENC_GetChnAttr(VencDevId, VencChn, &stAttr);
    stAttr.stVeAttr.eType = pVencAttr->eVencType;
    stAttr.stRcAttr.eRcMode = pVencAttr->eRcMode;

    switch (stAttr.stVeAttr.eType)
    {
    case E_MI_VENC_MODTYPE_H264E:
    {
        stAttr.stVeAttr.stAttrH264e.u32PicWidth = pVencAttr->stVencRes.u16Width;
        stAttr.stVeAttr.stAttrH264e.u32PicHeight = pVencAttr->stVencRes.u16Height;
        stAttr.stVeAttr.stAttrH264e.u32MaxPicWidth = pVencAttr->stVencRes.u16Width;
        stAttr.stVeAttr.stAttrH264e.u32MaxPicHeight = pVencAttr->stVencRes.u16Height;
        stAttr.stVeAttr.stAttrH264e.u32BufSize = pVencAttr->u32Bufsize;
        stAttr.stVeAttr.stAttrH264e.u32BFrameNum = 0;
        stAttr.stVeAttr.stAttrH264e.bByFrame = TRUE;
        stAttr.stVeAttr.stAttrH264e.u32Profile = getEncodeProfile(pVencAttr->u32Profile);
        stAttr.stVeAttr.stAttrH264e.u32RefNum = 1;
        if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264CBR)
        {
            stAttr.stRcAttr.stAttrH264Cbr.u32BitRate = (pVencAttr->u32BitRate << 10);
            stAttr.stRcAttr.stAttrH264Cbr.u32Gop = pVencAttr->u32Gop;
            stAttr.stRcAttr.stAttrH264Cbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stAttr.stRcAttr.stAttrH264Cbr.u32SrcFrmRateDen = 1;
            stAttr.stRcAttr.stAttrH264Cbr.u32StatTime = 0;
            stAttr.stRcAttr.stAttrH264Cbr.u32FluctuateLevel = 0;
        }
        else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264VBR)
        {
            stAttr.stRcAttr.stAttrH264Vbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stAttr.stRcAttr.stAttrH264Vbr.u32Gop = pVencAttr->u32Gop;
            stAttr.stRcAttr.stAttrH264Vbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stAttr.stRcAttr.stAttrH264Vbr.u32SrcFrmRateDen = 1;
            stAttr.stRcAttr.stAttrH264Vbr.u32StatTime = 0;
            stAttr.stRcAttr.stAttrH264Vbr.u32MinQp = pVencAttr->u32MinQp;
            stAttr.stRcAttr.stAttrH264Vbr.u32MaxQp = pVencAttr->u32MaxQp;
        }
        else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H264AVBR)
        {
            stAttr.stRcAttr.stAttrH264Avbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stAttr.stRcAttr.stAttrH264Avbr.u32Gop = pVencAttr->u32Gop;
            stAttr.stRcAttr.stAttrH264Avbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stAttr.stRcAttr.stAttrH264Avbr.u32SrcFrmRateDen = 1;
            stAttr.stRcAttr.stAttrH264Avbr.u32StatTime = 0;
            stAttr.stRcAttr.stAttrH264Avbr.u32MinQp = pVencAttr->u32MinQp;
            stAttr.stRcAttr.stAttrH264Avbr.u32MaxQp = pVencAttr->u32MaxQp;
        }
        else
        {
            __ERR("unsupport rcmode:%d\n", stAttr.stRcAttr.eRcMode);
            return -1;
        }
    }
    break;

    case E_MI_VENC_MODTYPE_H265E:
    {
        stAttr.stVeAttr.stAttrH265e.u32PicWidth = pVencAttr->stVencRes.u16Width;
        stAttr.stVeAttr.stAttrH265e.u32PicHeight = pVencAttr->stVencRes.u16Height;
        stAttr.stVeAttr.stAttrH265e.u32MaxPicWidth = pVencAttr->stVencRes.u16Width;
        stAttr.stVeAttr.stAttrH265e.u32MaxPicHeight = pVencAttr->stVencRes.u16Height;
        stAttr.stVeAttr.stAttrH265e.u32BufSize = pVencAttr->u32Bufsize;
        stAttr.stVeAttr.stAttrH265e.bByFrame = TRUE;
        stAttr.stVeAttr.stAttrH265e.u32Profile = 0;//getEncodeProfile(pVencAttr->u32Profile);
        stAttr.stVeAttr.stAttrH265e.u32BFrameNum = 0;
        stAttr.stVeAttr.stAttrH265e.u32RefNum = 1;

        if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265CBR)
        {
            stAttr.stRcAttr.stAttrH265Cbr.u32BitRate = (pVencAttr->u32BitRate << 10);
            stAttr.stRcAttr.stAttrH265Cbr.u32Gop = pVencAttr->u32Gop;
            stAttr.stRcAttr.stAttrH265Cbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stAttr.stRcAttr.stAttrH265Cbr.u32SrcFrmRateDen = 1;
            stAttr.stRcAttr.stAttrH265Cbr.u32StatTime = 0;
            stAttr.stRcAttr.stAttrH265Cbr.u32FluctuateLevel = 0;
        }
        else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265VBR)
        {
            stAttr.stRcAttr.stAttrH265Vbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stAttr.stRcAttr.stAttrH265Vbr.u32Gop = pVencAttr->u32Gop;
            stAttr.stRcAttr.stAttrH265Vbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stAttr.stRcAttr.stAttrH265Vbr.u32SrcFrmRateDen = 1;
            stAttr.stRcAttr.stAttrH265Vbr.u32StatTime = 0;
            stAttr.stRcAttr.stAttrH265Vbr.u32MinQp = pVencAttr->u32MinQp;
            stAttr.stRcAttr.stAttrH265Vbr.u32MaxQp = pVencAttr->u32MaxQp;
        }
        else if (stAttr.stRcAttr.eRcMode == E_MI_VENC_RC_MODE_H265AVBR)
        {
            stAttr.stRcAttr.stAttrH265Avbr.u32MaxBitRate = (pVencAttr->u32BitRate << 10);
            stAttr.stRcAttr.stAttrH265Avbr.u32Gop = pVencAttr->u32Gop;
            stAttr.stRcAttr.stAttrH265Avbr.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
            stAttr.stRcAttr.stAttrH265Avbr.u32SrcFrmRateDen = 1;
            stAttr.stRcAttr.stAttrH265Avbr.u32StatTime = 0;
            stAttr.stRcAttr.stAttrH265Avbr.u32MinQp = pVencAttr->u32MinQp;
            stAttr.stRcAttr.stAttrH265Avbr.u32MaxQp = pVencAttr->u32MaxQp;
        }
        else
        {
            __ERR("unsupport rcmode:%d\n", stAttr.stRcAttr.eRcMode);
            return -1;
        }
    }
    break;

    case E_MI_VENC_MODTYPE_JPEGE:
    {
        stAttr.stVeAttr.stAttrJpeg.u32PicWidth = pVencAttr->stVencRes.u16Width;
        stAttr.stVeAttr.stAttrJpeg.u32PicHeight = pVencAttr->stVencRes.u16Height;
        stAttr.stVeAttr.stAttrJpeg.u32MaxPicWidth = pVencAttr->stVencRes.u16Width;
        stAttr.stVeAttr.stAttrJpeg.u32MaxPicHeight = pVencAttr->stVencRes.u16Height;
        stAttr.stVeAttr.stAttrJpeg.u32BufSize = pVencAttr->u32Bufsize;
        stAttr.stVeAttr.stAttrJpeg.bByFrame = TRUE;

        if (stAttr.stRcAttr.eRcMode != E_MI_VENC_RC_MODE_MJPEGFIXQP)
        {
            __ERR("unsupport rcmode:%d\n", stAttr.stRcAttr.eRcMode);
            return -1;
        }
        stAttr.stRcAttr.stAttrMjpegFixQp.u32Qfactor = pVencAttr->u32Qfactor;
        stAttr.stRcAttr.stAttrMjpegFixQp.u32SrcFrmRateNum = pVencAttr->u32SrcFrmRateNum;
        stAttr.stRcAttr.stAttrMjpegFixQp.u32SrcFrmRateDen = 1;
    }
    break;

    default:
    {
        __ERR("Invalid venc type:%d\n", pVencAttr->eVencType);

        return MI_FAILED;
    }
    }

    STCHECKRESULT(MI_VENC_SetChnAttr(VencDevId, VencChn, &stAttr));

    return MI_SUCCESS;
}
