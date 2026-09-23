#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "anj_mw_media_common.h"
#include "anj_mw_media_video.h"
#include "anj_mw_comm.h"
#include "anj_mw_smart.h"

/* 对齐 st_single_camera 200W@60: VI 1920x1080, fps>30 时进该pipe */
#define ANJ_PIPE_200W_WIDTH (1920)
#define ANJ_PIPE_200W_HEIGHT (1080)
#define ANJ_PIPE_HIGH_FPS_TH (30)

#define ISP_VALID_FPS 13
#define ISP_VALID_FPS_TIME (int)(1000 / ISP_VALID_FPS)
#define ISP_VALID_WIDTH 20
#define ISP_VALID_MID_WIDTH 10 // 30(dsync_ms) - ISP_VALID_WIDTH
#define ISP_VALID_DELTA 8      // related to encode time joggle

#define ANJ_MW_SNAP_JPEG_BASE (MAX_VENC_CHN * ANJ_CAMERA_MAX_NUMS)
#define ANJ_MW_SNAP_GET_FRAME_MS (100)
#define ANJ_MW_SNAP_GET_STREAM_MS (500)
#define ANJ_MW_SNAP_WARMUP_TRY (20)
#define ANJ_MW_SNAP_WARMUP_QUALITY (70)

typedef struct
{
    TS_BOOL bViVpssOnline;
    TS_BOOL bAiispEnabled;
} AnjMwMediaVideoCtx_t;

static AnjMwMediaVideoCtx_t s_stMwMediaVideoCtx = {
    .bViVpssOnline = TS_TRUE,
    .bAiispEnabled = TS_TRUE,
};

static TS_Common_VideoAttr_t gstVideoAttr;
static anj_thread_s s_stVencStreamThread[ANJ_CAMERA_MAX_NUMS];
static anj_thread_s s_stVpssYuvThread[ANJ_CAMERA_MAX_NUMS];

typedef struct
{
    VPSS_GRP vpss_group;
    VPSS_CHN vpss_channel;
    VENC_CHN jpeg_channel;
    unsigned int width;
    unsigned int height;
    int initialized;
    int cam;
    int stream;
    anj_thread_s warmup_thread;
} AnjMwSnapContext;

static AnjMwSnapContext s_astSnapCtx[ANJ_CAMERA_MAX_NUMS][MAX_VENC_CHN];
static pthread_mutex_t s_astSnapLock[ANJ_CAMERA_MAX_NUMS][MAX_VENC_CHN];
static int s_snapLockInited = 0;

static void anj_mw_media_snap_lock_init_once(void)
{
    int cam;
    int stream;

    if (s_snapLockInited)
    {
        return;
    }
    for (cam = 0; cam < ANJ_CAMERA_MAX_NUMS; cam++)
    {
        for (stream = 0; stream < MAX_VENC_CHN; stream++)
        {
            pthread_mutex_init(&s_astSnapLock[cam][stream], NULL);
        }
    }
    s_snapLockInited = 1;
}

static int anj_mw_media_video_jpg_capture_impl(int cam, int stream, int quality, const char *output_file)
{
    AnjMwSnapContext *pstCtx = NULL;
    VENC_PACK_S astVencPack[8];
    VENC_STREAM_S stStream;
    VIDEO_FRAME_INFO_S stFrameInfo;
    VENC_JPEG_PARAM_S stJpegParam;
    FILE *pFile = NULL;
    TS_S32 s32Ret;
    TS_S32 got_frame = 0;
    TS_S32 got_stream = 0;
    int qclamp;

    if ((cam < 0) || (cam >= ANJ_CAMERA_MAX_NUMS) || (stream < 0) || (stream >= MAX_VENC_CHN))
    {
        return TS_FAILURE;
    }

    pstCtx = &s_astSnapCtx[cam][stream];

    anj_mw_media_snap_lock_init_once();
    pthread_mutex_lock(&s_astSnapLock[cam][stream]);

    memset(&stStream, 0, sizeof(stStream));
    memset(&stFrameInfo, 0, sizeof(stFrameInfo));
    memset(&stJpegParam, 0, sizeof(stJpegParam));

    s32Ret = TS_MPI_VENC_GetJpegParam(pstCtx->jpeg_channel, &stJpegParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("snap GetJpegParam cam=%d stream=%d failed %#x\n", cam, stream, s32Ret);
        goto fail_unlock;
    }

    qclamp = quality;
    if (qclamp < 1)
    {
        qclamp = 1;
    }
    if (qclamp > 100)
    {
        qclamp = 100;
    }
    stJpegParam.u32Qfactor = (100 - qclamp) * 90 / 99;
    s32Ret = TS_MPI_VENC_SetJpegParam(pstCtx->jpeg_channel, &stJpegParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("snap SetJpegParam cam=%d stream=%d failed %#x\n", cam, stream, s32Ret);
        goto fail_unlock;
    }

    s32Ret = TS_MPI_VPSS_GetChnFrame(pstCtx->vpss_group, pstCtx->vpss_channel, &stFrameInfo,
                                     ANJ_MW_SNAP_GET_FRAME_MS);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("snap GetChnFrame cam=%d stream=%d failed %#x\n", cam, stream, s32Ret);
        goto fail_unlock;
    }
    got_frame = 1;

    s32Ret = TS_MPI_VENC_SendFrame(pstCtx->jpeg_channel, &stFrameInfo, -1);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("snap SendFrame cam=%d stream=%d failed %#x\n", cam, stream, s32Ret);
        goto fail_release_frame;
    }

    memset(astVencPack, 0, sizeof(astVencPack));
    stStream.u32PackCount = 8;
    stStream.pstPack = astVencPack;
    s32Ret = TS_MPI_VENC_GetStream(pstCtx->jpeg_channel, &stStream, ANJ_MW_SNAP_GET_STREAM_MS);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("snap GetStream cam=%d stream=%d failed %#x\n", cam, stream, s32Ret);
        goto fail_release_frame;
    }
    got_stream = 1;

    /* output_file 为空：仅预热，不落盘 */
    if ((output_file != NULL) && (output_file[0] != '\0'))
    {
        pFile = fopen(output_file, "wb");
        if (pFile == NULL)
        {
            __ERR("snap open %s failed\n", output_file);
            s32Ret = TS_FAILURE;
            goto fail_release_stream;
        }

        for (TS_U32 u32PackIdx = 0; u32PackIdx < stStream.u32PackCount; u32PackIdx++)
        {
            VENC_PACK_S *pstPack = &stStream.pstPack[u32PackIdx];
            TS_U32 u32DataLen = pstPack->u32Len - pstPack->u32Offset;

            if ((pstPack->pu8Addr != NULL) && (u32DataLen > 0))
            {
                fwrite(pstPack->pu8Addr + pstPack->u32Offset, 1, u32DataLen, pFile);
            }
        }
        fflush(pFile);
        fclose(pFile);
        __INFO("snap save %s ok, cam=%d stream=%d quality=%d size=%ux%u\n", output_file, cam, stream, qclamp,
               pstCtx->width, pstCtx->height);
    }
    s32Ret = TS_SUCCESS;

fail_release_stream:
    if (got_stream)
    {
        TS_MPI_VENC_ReleaseStream(pstCtx->jpeg_channel, &stStream);
    }
fail_release_frame:
    if (got_frame)
    {
        TS_MPI_VPSS_ReleaseChnFrame(pstCtx->vpss_group, pstCtx->vpss_channel, &stFrameInfo);
    }
fail_unlock:
    pthread_mutex_unlock(&s_astSnapLock[cam][stream]);
    return s32Ret;
}

static int anj_mw_media_snap_warmup_thread(void *ctx, int *bStart)
{
    AnjMwSnapContext *pstCtx = (AnjMwSnapContext *)ctx;

    if (pstCtx == NULL)
    {
        return -1;
    }
    pstCtx->initialized = 1;

    for (int i = 0; i < ANJ_MW_SNAP_WARMUP_TRY && *bStart == 1; i++)
    {
        if (bStart == NULL || *bStart == 0)
        {
            return 0;
        }
        /* output_file=NULL：走完整编解码路径但不落盘 */
        if (anj_mw_media_video_jpg_capture_impl(pstCtx->cam, pstCtx->stream, ANJ_MW_SNAP_WARMUP_QUALITY,
                                           NULL) == 0)
        {
            __INFO("hard snap warmup ok cam=%d stream=%d try=%d\n", pstCtx->cam, pstCtx->stream, i + 1);
            return 0;
        }
        sleep(1);
    }
    __ERR("hard snap warmup failed cam=%d stream=%d after %d tries\n", pstCtx->cam, pstCtx->stream,
           ANJ_MW_SNAP_WARMUP_TRY);
    return -1;
}

int anj_mw_media_video_jpg_capture(int cam, int stream, int quality, const char *output_file)
{
    AnjMwSnapContext *pstCtx = NULL;

    if ((cam < 0) || (cam >= ANJ_CAMERA_MAX_NUMS) || (stream < 0) || (stream >= MAX_VENC_CHN))
    {
        return TS_FAILURE;
    }

    pstCtx = &s_astSnapCtx[cam][stream];
    if (!pstCtx->initialized)
    {
        __ERR("snap not initialized, cam=%d stream=%d\n", cam, stream);
        return TS_FAILURE;
    }

    return anj_mw_media_video_jpg_capture_impl(cam, stream, quality, output_file);
}

int anj_mw_media_video_jpg_init(int iCameraIdex)
{
    int stream;
    SIZE_S stSize = {0};
    VENC_RECV_PIC_PARAM_S stRecvParam = {0};

    if ((iCameraIdex < 0) || (iCameraIdex >= ANJ_CAMERA_MAX_NUMS))
    {
        return TS_FAILURE;
    }

    if (gstVideoAttr.pVpssAttr == NULL)
    {
        __ERR("snap jpg_init: vpss attr not ready\n");
        return TS_FAILURE;
    }

    anj_mw_media_snap_lock_init_once();

    for (stream = 0; stream < MAX_VENC_CHN; stream++)
    {
        AnjMwSnapContext *pstCtx = &s_astSnapCtx[iCameraIdex][stream];
        TS_Common_VpssAttr_t *pstVpssAttr = &gstVideoAttr.pVpssAttr[iCameraIdex * MAX_VPSS_CHN + stream];
        anj_thread_s *pstThread = &pstCtx->warmup_thread;

        if (pstCtx->initialized)
        {
            __ERR("snap already initialized, cam=%d stream=%d\n", iCameraIdex, stream);
            continue;
        }

        memset(pstCtx, 0, sizeof(*pstCtx));
        pstCtx->cam = iCameraIdex;
        pstCtx->stream = stream;
        pstCtx->vpss_group = iCameraIdex;
        pstCtx->vpss_channel = stream;
        pstCtx->jpeg_channel = ANJ_MW_SNAP_JPEG_BASE + iCameraIdex * MAX_VENC_CHN + stream;
        pstCtx->width = pstVpssAttr->u32Width;
        pstCtx->height = pstVpssAttr->u32Height;

        if ((pstCtx->width == 0) || (pstCtx->height == 0))
        {
            __ERR("snap size invalid cam=%d stream=%d\n", iCameraIdex, stream);
            return TS_FAILURE;
        }

        stSize.u32Width = pstCtx->width;
        stSize.u32Height = pstCtx->height;
        STCHECKRESULT(TS_Common_VENC_SnapStart(pstCtx->jpeg_channel, &stSize, TS_FALSE));

        memset(&stRecvParam, 0, sizeof(stRecvParam));
        stRecvParam.s32RecvPicNum = -1;
        if (TS_MPI_VENC_StartRecvFrame(pstCtx->jpeg_channel, &stRecvParam) != TS_SUCCESS)
        {
            __ERR("snap StartRecvFrame(-1) cam=%d stream=%d jpeg=%d failed\n", iCameraIdex, stream,
                  pstCtx->jpeg_channel);
            TS_Common_VENC_SnapStop(pstCtx->jpeg_channel);
            return TS_FAILURE;
        }
        pstCtx->initialized = 1;

        __INFO("snap init ok: vpss[%d,%d] jpeg_venc=%d size=%ux%u\n", pstCtx->vpss_group,
               pstCtx->vpss_channel, pstCtx->jpeg_channel, pstCtx->width, pstCtx->height);

        /* 预热：通道就绪后立即起分离线程 */
        memset(pstThread, 0, sizeof(*pstThread));
        pstThread->bAutoDestroy = 0;
        snprintf(pstThread->iThreadName, sizeof(pstThread->iThreadName), "snap_wu_%d_%d", iCameraIdex,
                 stream);
        pstThread->iThreadjob.ctx = pstCtx;
        pstThread->iThreadjob.func = anj_mw_media_snap_warmup_thread;
        if (anj_thread_task_create(pstThread) != 0)
        {
            __WARN("hard snap warmup thread create failed cam=%d stream=%d\n", iCameraIdex, stream);
        }
    }

    return TS_SUCCESS;
}

int anj_mw_media_video_jpg_uninit(int iCameraIdex)
{
    int stream;

    if ((iCameraIdex < 0) || (iCameraIdex >= ANJ_CAMERA_MAX_NUMS))
    {
        return TS_SUCCESS;
    }

    for (stream = 0; stream < MAX_VENC_CHN; stream++)
    {
        AnjMwSnapContext *pstCtx = &s_astSnapCtx[iCameraIdex][stream];
        anj_thread_s *pstThread = &pstCtx->warmup_thread;


        anj_thread_task_destroy(pstThread, -1);
        memset(pstThread, 0, sizeof(*pstThread));

        if (!pstCtx->initialized)
        {
            continue;
        }

        TS_MPI_VENC_StopRecvFrame(pstCtx->jpeg_channel);
        STCHECKRESULT(TS_Common_VENC_SnapStop(pstCtx->jpeg_channel));
        pstCtx->initialized = 0;
        pstCtx->vpss_group = 0;
        pstCtx->vpss_channel = 0;
        pstCtx->jpeg_channel = 0;
        pstCtx->width = 0;
        pstCtx->height = 0;
        pstCtx->cam = 0;
        pstCtx->stream = 0;
    }
    return TS_SUCCESS;
}

static TS_U32 anj_mw_media_video_sensor_fps_get()
{
    return DEFAULT_SENSOR_FPS;
}

static void anj_mw_media_video_attr_uninit(void)
{
    if (gstVideoAttr.pVpssAttr != NULL)
    {
        anj_mw_free(gstVideoAttr.pVpssAttr);
        gstVideoAttr.pVpssAttr = NULL;
    }
    if (gstVideoAttr.pVencAttr != NULL)
    {
        anj_mw_free(gstVideoAttr.pVencAttr);
        gstVideoAttr.pVencAttr = NULL;
    }
}

/* 单路 AnjVencConfig → TS_Common_VencAttr_t，供 attr_init 使用 */
static TS_S32 anj_mw_media_video_venc_attr_set(AnjVencConfig *pVenCfg, TS_Common_VencAttr_t *pstVencAttr,
                                                    TS_S32 s32CameraIndex, TS_S32 s32VencIndex)
{
    TS_S32 iRet = TS_SUCCESS;

    if (pVenCfg == NULL || pstVencAttr == NULL)
    {
        return TS_FAILURE;
    }

    pstVencAttr->bEnable = pVenCfg->enable;
    pstVencAttr->VpssGrp = s32CameraIndex;
    pstVencAttr->VpssChn = s32VencIndex;
    pstVencAttr->VencChnId = pVenCfg->chn;
    pstVencAttr->u32Width = pVenCfg->width;
    pstVencAttr->u32Height = pVenCfg->height;
    pstVencAttr->u32SrcFrameRate = pVenCfg->fps;
    pstVencAttr->u32Bufsize = pVenCfg->bufszie;
    pstVencAttr->u32BitRate = pVenCfg->bitrate;
    pstVencAttr->u32Gop = pVenCfg->gop;
    pstVencAttr->u32StatTime = pVenCfg->gop / pVenCfg->fps;
    pstVencAttr->u32Qfactor = pVenCfg->qfactor;
    pstVencAttr->s32IPQPDelta = pVenCfg->qp_delta;
    pstVencAttr->u32MaxISize = pVenCfg->maxIsize;
    pstVencAttr->u32MaxPSize = pVenCfg->maxPsize;
    pstVencAttr->u32Profile = pVenCfg->profile;
    pstVencAttr->datacb = (TS_Common_Venc_DataCb)pVenCfg->venc_data_cb;

    /* 未使能通道不做 type/rc 映射，避免 enable=0 时 encodeType 未填导致 attr_init 失败 */
    if (!pVenCfg->enable)
    {
        return TS_SUCCESS;
    }

    if (pVenCfg->encodeType == MEDIA_CODEC_VIDEO_H264)
    {
        pstVencAttr->eVencType = PT_H264;
        if (pVenCfg->rcMode == ANJ_VIDEO_CBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_H264CBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_VBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_H264VBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_AVBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_H264AVBR;
        }
        else
        {
            iRet = TS_FAILURE;
        }
    }
    else if ((pVenCfg->encodeType == MEDIA_CODEC_VIDEO_H265) ||
             (pVenCfg->encodeType == MEDIA_CODEC_VIDEO_H265_PLUS))
    {
        pstVencAttr->eVencType = PT_H265;
        if (pVenCfg->rcMode == ANJ_VIDEO_CBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_H265CBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_VBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_H265VBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_AVBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_H265AVBR;
        }
        else
        {
            iRet = TS_FAILURE;
        }
    }
    else if (pVenCfg->encodeType == MEDIA_CODEC_VIDEO_JPG)
    {
        pstVencAttr->eVencType = PT_JPEG;
        if (pVenCfg->rcMode == ANJ_VIDEO_FIXQP)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_MJPEGFIXQP;
        }
        else
        {
            iRet = TS_FAILURE;
        }
    }
    else if (pVenCfg->encodeType == MEDIA_CODEC_VIDEO_MJPG)
    {
        pstVencAttr->eVencType = PT_MJPEG;
        if (pVenCfg->rcMode == ANJ_VIDEO_CBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_MJPEGCBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_VBR)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_MJPEGVBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_FIXQP)
        {
            pstVencAttr->eRcMode = VENC_RC_MODE_MJPEGFIXQP;
        }
        else
        {
            iRet = TS_FAILURE;
        }
    }
    else
    {
        iRet = TS_FAILURE;
    }

    return iRet;
}

static TS_S32 anj_mw_media_video_attr_init(AnjVideoConfig *pstAnjVideoCfg)
{
    int iRet = TS_SUCCESS;
    TS_S32 s32CameraIndex = 0;

    memset(&gstVideoAttr, 0, sizeof(TS_Common_VideoAttr_t));

    gstVideoAttr.ViAttr.s32WorkingViNum = ANJ_CAMERA_MAX_NUMS;
    gstVideoAttr.pVpssAttr = anj_mw_malloc(sizeof(TS_Common_VpssAttr_t) * ANJ_CAMERA_MAX_NUMS * MAX_VPSS_CHN);
    if (gstVideoAttr.pVpssAttr == NULL)
    {
        iRet = TS_FAILURE;
        goto endFunc;
    }
    memset(gstVideoAttr.pVpssAttr, 0, sizeof(TS_Common_VpssAttr_t) * ANJ_CAMERA_MAX_NUMS * MAX_VPSS_CHN);
    gstVideoAttr.pVencAttr = anj_mw_malloc(sizeof(TS_Common_VencAttr_t) * ANJ_CAMERA_MAX_NUMS * MAX_VENC_CHN);
    if (gstVideoAttr.pVencAttr == NULL)
    {
        iRet = TS_FAILURE;
        goto endFunc;
    }
    memset(gstVideoAttr.pVencAttr, 0, sizeof(TS_Common_VencAttr_t) * ANJ_CAMERA_MAX_NUMS * MAX_VENC_CHN);

    for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
    {
        AnjVideoConfig *pstVideoCfg = &pstAnjVideoCfg[s32CameraIndex];
        TS_Common_ViPipe_t *pstPipeInfo = &gstVideoAttr.ViAttr.astViInfo[s32CameraIndex].stPipeInfo;
        TS_Common_ViChn_t *pstChnInfo = &gstVideoAttr.ViAttr.astViInfo[s32CameraIndex].stChnInfo;
        TS_U32 u32SensorFps = anj_mw_media_video_sensor_fps_get();
        ANJ_SIZE_S stSensorSize = anj_mw_sensor_get_size();
        TS_U32 u32PipeWidth = stSensorSize.u32Width;
        TS_U32 u32PipeHeight = stSensorSize.u32Height;

        pstPipeInfo->aPipe = s32CameraIndex;
        pstPipeInfo->enBitWid = DATA_BITWIDTH_8;
        pstPipeInfo->u8PixCut = 0;
        pstPipeInfo->u8RowCut = 0;
        pstPipeInfo->enBayer = E_BAYER_RGGB;
        pstPipeInfo->width = u32PipeWidth;
        pstPipeInfo->height = u32PipeHeight;
        pstPipeInfo->bHFlip = pstVideoCfg->hflip;
        pstPipeInfo->bVFlip = pstVideoCfg->vflip;

        /* 上电按配置进线性/stagger；运行时改 wdr_mode 走 encode_apply 重建 */
        if (pstVideoCfg->wdr_enable && anj_mw_sensor_support_wdr())
        {
            pstPipeInfo->enWdrMode = WDR_MODE_2To1_FRAME_FULL_RATE;
            pstPipeInfo->frameRate = WDR_SENSOR_FPS;
        }
        else
        {
            pstPipeInfo->enWdrMode = WDR_MODE_NONE;
            pstPipeInfo->frameRate = u32SensorFps;
        }
        pstPipeInfo->bIspByFly = TS_FALSE;
        pstPipeInfo->bDynFpsSync = TS_FALSE;
        if (s_stMwMediaVideoCtx.bViVpssOnline == TS_TRUE)
        {
            pstPipeInfo->enMastPipeMode = VI_ONLINE_VPSS_ONLINE;
            pstPipeInfo->enPixFmt = PIXEL_FORMAT_RGB_888;
        }
        else
        {
            pstPipeInfo->enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;
            pstPipeInfo->enPixFmt = PIXEL_FORMAT_NV_12;
        }

        pstChnInfo->ViChn[0] = 0;
        pstChnInfo->width[0] = u32PipeWidth;
        pstChnInfo->height[0] = u32PipeHeight;
        pstChnInfo->validChnlNum = 1;
        pstChnInfo->enPixFormat = PIXEL_FORMAT_NV_12;

        for (TS_S32 s32ChnIndex = 0; s32ChnIndex < MAX_VPSS_CHN; s32ChnIndex++)
        {
            TS_S32 s32Width = 0;
            TS_S32 s32Height = 0;
            TS_S32 s32Fps = 0;
            TS_S32 s32Depth = 0;
            TS_S32 s32MemCnt = 0;
            TS_Common_Vpss_DataCb pfnDataCb = NULL;

            TS_Common_VpssAttr_t *pstVpssAttr = &gstVideoAttr.pVpssAttr[s32CameraIndex * MAX_VPSS_CHN + s32ChnIndex];

            pstVpssAttr->bEnable = TS_TRUE;
            pstVpssAttr->VpssGrpId = s32CameraIndex;
            pstVpssAttr->VpssChnId = s32ChnIndex;

            if (s32ChnIndex == 0)
            {
                s32Width = pstVideoCfg->stVencCfg[0].width;
                s32Height = pstVideoCfg->stVencCfg[0].height;
                s32Fps = pstVideoCfg->stVencCfg[0].fps;
                s32Depth = 0;
                s32MemCnt = 2;
            }
            else
            {
                /* 多 VPSS 口：子流分辨率 & VENC JPEG*/
                s32Width = pstAnjVideoCfg->stVencCfg[1].width;
                s32Height = pstAnjVideoCfg->stVencCfg[1].height;
                s32Fps = pstAnjVideoCfg->stVencCfg[1].fps;
                s32Depth = 0;
                s32MemCnt = 3;
            }

            pstVpssAttr->u32Width = s32Width;
            pstVpssAttr->u32Height = s32Height;
            pstVpssAttr->u32SrcFrmRateNum = s32Fps;
            pstVpssAttr->u32DepthSet = s32Depth;
            pstVpssAttr->u32MemCnt = s32MemCnt;
            pstVpssAttr->datacb = pfnDataCb;
        }

        for (TS_S32 s32VencIndex = 0; s32VencIndex < MAX_VENC_CHN; s32VencIndex++)
        {
            TS_Common_VencAttr_t *pstVencAttr = &gstVideoAttr.pVencAttr[s32CameraIndex * MAX_VENC_CHN + s32VencIndex];
            AnjVencConfig *pVenCfg = &pstVideoCfg->stVencCfg[s32VencIndex];

            if (anj_mw_media_video_venc_attr_set(pVenCfg, pstVencAttr, s32CameraIndex, s32VencIndex) != TS_SUCCESS)
            {
                iRet = TS_FAILURE;
            }
        }
    }

endFunc:
    if (iRet != TS_SUCCESS)
    {
        anj_mw_media_video_attr_uninit();
    }
    return iRet;
}

static int anj_mw_media_vpss_yuv_thread(void *ctx, int *bStart)
{
    TS_Common_VpssAttr_t *pstVpssAttr = (TS_Common_VpssAttr_t *)ctx;

    while (*bStart)
    {
        TS_S32 s32Ret = TS_SUCCESS;
        VIDEO_FRAME_INFO_S stFrameInfo = {0};
        unsigned long long u64PhyAddr = 0;
        unsigned int u32MapSize = 0;
        void *pVirAddr = NULL;

        s32Ret = TS_MPI_VPSS_GetChnFrame(pstVpssAttr->VpssGrpId, pstVpssAttr->VpssChnId, &stFrameInfo, 1000);
        if (s32Ret != TS_SUCCESS)
        {
            continue;
        }

        u64PhyAddr = stFrameInfo.stVFrame.u64PhyAddr[0];
        u32MapSize = ANJ_ALIGN_UP(stFrameInfo.stVFrame.s32Size, 1024 * 1024);
        if ((u64PhyAddr != 0) && (pstVpssAttr->datacb != NULL))
        {
            pVirAddr = TS_Common_SysMmap(u64PhyAddr, u32MapSize);
            if (pVirAddr != NULL)
            {
                pstVpssAttr->datacb(pstVpssAttr->VpssGrpId, pVirAddr, u64PhyAddr, stFrameInfo.stVFrame.s32Size, pstVpssAttr->param);
                TS_Common_SysMunmap(pVirAddr, u32MapSize);
            }
        }

        TS_MPI_VPSS_ReleaseChnFrame(pstVpssAttr->VpssGrpId, pstVpssAttr->VpssChnId, &stFrameInfo);
    }

    return 0;
}

static int anj_mw_media_video_stream_thread(void *ctx, int *bStart)
{
    TS_Common_VencAttr_t *pstVencAttr = (TS_Common_VencAttr_t *)ctx;
    TS_Common_VencGetStream(pstVencAttr, bStart);
    return 0;
}

static TS_S32 anj_mw_media_video_venc_thread_start(TS_S32 s32CameraIndex)
{
    anj_thread_s *pstThread = &s_stVencStreamThread[s32CameraIndex];
    TS_Common_VencAttr_t *pstVencAttr = &gstVideoAttr.pVencAttr[s32CameraIndex * MAX_VENC_CHN];

    char szName[16] = {0};
    snprintf(szName, sizeof(szName), "rm_vstream%d", s32CameraIndex);
    pstThread->bAutoDestroy = 0;
    strncpy(pstThread->iThreadName, szName, sizeof(pstThread->iThreadName) - 1);
    pstThread->iThreadjob.ctx = pstVencAttr;
    pstThread->iThreadjob.func = anj_mw_media_video_stream_thread;
    STCHECKRESULT(anj_thread_task_create(pstThread));
    return TS_SUCCESS;
}

static TS_S32 anj_mw_media_video_vpss_thread_start(TS_S32 s32CameraIndex)
{
    anj_thread_s *pstThread = &s_stVpssYuvThread[s32CameraIndex];
    TS_Common_VpssAttr_t *pstVpssAttr = &gstVideoAttr.pVpssAttr[s32CameraIndex * MAX_VPSS_CHN + MAX_VPSS_CHN - 1];
    if ((pstVpssAttr->datacb == NULL) || pstThread->start)
    {
        return TS_SUCCESS;
    }

    char szName[16] = {0};
    snprintf(szName, sizeof(szName), "rm_vyuv%d", s32CameraIndex);
    pstThread->bAutoDestroy = 0;
    strncpy(pstThread->iThreadName, szName, sizeof(pstThread->iThreadName) - 1);
    pstThread->iThreadjob.ctx = pstVpssAttr;
    pstThread->iThreadjob.func = anj_mw_media_vpss_yuv_thread;
    return anj_thread_task_create(pstThread);
}

int anj_mw_media_vpss_uninit(TS_S32 s32CameraIndex)
{
    STCHECKRESULT(TS_COMMON_VPSS_Stop(s32CameraIndex, MAX_VPSS_CHN));

    return TS_SUCCESS;
}

int anj_mw_media_vpss_init(TS_S32 s32CameraIndex)
{
    LOOP_ENC_CFG_S stLoopEncCfg[4] = {
        {0, 1, 1}, // inst_idx 0 sensorId 是否是主码流 下一帧编码通道
        {0, 0, 2}, // inst_idx 1
        {1, 1, 3}, // inst_idx 2
        {1, 0, 0}  // inst_idx 3
    };

    TS_Common_ViAttr_t *pViAttr = &gstVideoAttr.ViAttr;
    TS_Common_ViPipe_t *pstPipeInfo = &pViAttr->astViInfo[s32CameraIndex].stPipeInfo;
    VPSS_GRP_ATTR_S stVpssGrpAttr = {0};
    VPSS_CHN_ATTR_S astVpssChnAttr[MAX_VPSS_CHN] = {0};
    stVpssGrpAttr.enGrpMode = VPSS_GRP_MODE_STREAM;
    stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stVpssGrpAttr.enPixelFormat = PIXEL_FORMAT_NV_12;
    stVpssGrpAttr.u32MaxW = pstPipeInfo->width;
    stVpssGrpAttr.u32MaxH = pstPipeInfo->height;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;

    for (TS_S32 s32ChnIndex = 0; s32ChnIndex < MAX_VPSS_CHN; s32ChnIndex++)
    {
        TS_Common_VpssAttr_t *pstVpssAttr = &gstVideoAttr.pVpssAttr[s32CameraIndex * MAX_VPSS_CHN + s32ChnIndex];
        astVpssChnAttr[s32ChnIndex].u32Width = pstVpssAttr->u32Width;
        astVpssChnAttr[s32ChnIndex].u32Height = pstVpssAttr->u32Height;
        astVpssChnAttr[s32ChnIndex].enChnMode = VPSS_CHN_MODE_USER;
        astVpssChnAttr[s32ChnIndex].enCompressMode = COMPRESS_MODE_NONE;
        astVpssChnAttr[s32ChnIndex].enDynamicRange = DYNAMIC_RANGE_SDR8;
        astVpssChnAttr[s32ChnIndex].enVideoFormat = VIDEO_FORMAT_LINEAR;
        astVpssChnAttr[s32ChnIndex].enPixelFormat = PIXEL_FORMAT_NV_12;
        astVpssChnAttr[s32ChnIndex].stFrameRate.s32SrcFrameRate = -1;
        astVpssChnAttr[s32ChnIndex].stFrameRate.s32DstFrameRate = -1;
        astVpssChnAttr[s32ChnIndex].u32Depth = pstVpssAttr->u32DepthSet;
        astVpssChnAttr[s32ChnIndex].bMirror = pstVpssAttr->bMirror;
        astVpssChnAttr[s32ChnIndex].bFlip = pstVpssAttr->bFlip;
        astVpssChnAttr[s32ChnIndex].stAspectRatio.enMode = ASPECT_RATIO_NONE;
        astVpssChnAttr[s32ChnIndex].u32MaxW = pstVpssAttr->u32Width;
        astVpssChnAttr[s32ChnIndex].u32MaxH = pstVpssAttr->u32Height;
        astVpssChnAttr[s32ChnIndex].u32MemCount = pstVpssAttr->u32MemCnt;
        astVpssChnAttr[s32ChnIndex].u32Align = pstVpssAttr->u32Width & 0x1f ? 8 : 16;

        /* Align with st_single_camera --loopDep=1,0:
         * main channel enables loop fast flow, sub channel keeps normal mode.
         */

        if (access("/mnt/nand/loopdep", F_OK) != 0)
        {
            continue;
        }
        TS_Common_VencAttr_t *pstVencAttr = &gstVideoAttr.pVencAttr[s32CameraIndex * MAX_VENC_CHN + s32ChnIndex];
        if (s32ChnIndex == 0 && (pstVencAttr->eVencType != PT_MJPEG))
        {
            if (pstVpssAttr->u32Width == 2592 && pstVpssAttr->u32Height == 1944)
            {
                astVpssChnAttr[s32ChnIndex].u32MaxW = 2624;
                astVpssChnAttr[s32ChnIndex].u32MaxH = 1984;
                astVpssChnAttr[s32ChnIndex].stFastFlow.u32Depth = 32;
            }
            else if (pstVpssAttr->u32Width == 3840 && pstVpssAttr->u32Height == 2160)
            {
                astVpssChnAttr[s32ChnIndex].stFastFlow.u32Depth = 40;
            }
            else
            {
                astVpssChnAttr[s32ChnIndex].stFastFlow.u32Depth = 24;
            }
            astVpssChnAttr[s32ChnIndex].u32Align = 16;
            astVpssChnAttr[s32ChnIndex].stFastFlow.s8SyncChannel = 2;
            astVpssChnAttr[s32ChnIndex].stFastFlow.stLoopEncCfg = stLoopEncCfg[s32CameraIndex * 2 + s32ChnIndex];
            astVpssChnAttr[s32ChnIndex].stFastFlow.enFastFlow = FAST_FLOW_MODE_LOOP1;
            astVpssChnAttr[s32ChnIndex].stFastFlow.s8Index = (s32CameraIndex == 0) ? 0 : 2;
            astVpssChnAttr[s32ChnIndex].stFastFlow.u32MinTiming =
                (s32CameraIndex == 0) ? (ISP_VALID_WIDTH + ISP_VALID_MID_WIDTH) : 0;
            astVpssChnAttr[s32ChnIndex].stFastFlow.u32MaxTiming =
                (ISP_VALID_FPS_TIME - ISP_VALID_WIDTH - ISP_VALID_MID_WIDTH + ISP_VALID_DELTA);
        }
    }

    STCHECKRESULT(TS_COMMON_VPSS_Start(s32CameraIndex, &stVpssGrpAttr, astVpssChnAttr, MAX_VPSS_CHN));
    return TS_SUCCESS;
}

int anj_mw_media_vi_uninit()
{
    if (s_stMwMediaVideoCtx.bAiispEnabled == TS_TRUE)
    {
        STCHECKRESULT(TS_COMMON_VI_StopVi_And_Aiisp(&gstVideoAttr.ViAttr));
    }
    else
    {
        STCHECKRESULT(TS_COMMON_VI_StopVi(&gstVideoAttr.ViAttr));
    }
    return TS_SUCCESS;
}

int anj_mw_media_vi_init()
{
    STCHECKRESULT(TS_COMMON_SYS_BwLimitInit(&gstVideoAttr.ViAttr));
    STCHECKRESULT(TS_Common_SensorMirrorSet(0, gstVideoAttr.ViAttr.astViInfo[0].stPipeInfo.bHFlip, gstVideoAttr.ViAttr.astViInfo[0].stPipeInfo.bVFlip));
    if (s_stMwMediaVideoCtx.bAiispEnabled == TS_TRUE)
    {
        STCHECKRESULT(TS_COMMON_VI_StartVi_And_Aiisp(&gstVideoAttr.ViAttr));
    }
    else
    {
        STCHECKRESULT(TS_COMMON_VI_StartVi(&gstVideoAttr.ViAttr));
    }
    return TS_SUCCESS;
}

int anj_mw_media_sensor_uninit(int iCameraIdex)
{
    return TS_SUCCESS;
}

int anj_mw_media_sensor_init(int iCameraIdex, int choice)
{
    return TS_SUCCESS;
}

int anj_mw_media_vpss_bind_venc(TS_Common_VencAttr_t *pstVencAttr)
{
    if ((pstVencAttr == NULL) || (pstVencAttr->bEnable == TS_FALSE))
    {
        return TS_SUCCESS;
    }

    return TS_COMMON_VPSS_Bind_VENC(pstVencAttr->VpssGrp, pstVencAttr->VpssChn, pstVencAttr->VencChnId);
}

int anj_mw_media_vpss_unbind_venc(TS_Common_VencAttr_t *pstVencAttr)
{
    if ((pstVencAttr == NULL) || (pstVencAttr->bEnable == TS_FALSE))
    {
        return TS_SUCCESS;
    }

    return TS_COMMON_VPSS_UnBind_VENC(pstVencAttr->VpssGrp, pstVencAttr->VpssChn, pstVencAttr->VencChnId);
}

int anj_mw_media_vi_bind_vpss(int iCameraIdex)
{
    if (s_stMwMediaVideoCtx.bViVpssOnline == TS_TRUE)
    {
        return TS_SUCCESS;
    }

    return TS_COMMON_VI_Bind_VPSS(iCameraIdex, 0, iCameraIdex);
}

int anj_mw_media_vi_unbind_vpss(int iCameraIdex)
{
    if (s_stMwMediaVideoCtx.bViVpssOnline == TS_TRUE)
    {
        return TS_SUCCESS;
    }

    return TS_COMMON_VI_UnBind_VPSS(iCameraIdex, 0, iCameraIdex);
}

int anj_mw_media_video_init(AnjVideoConfig *pstAnjVideoCfg)
{
    if (pstAnjVideoCfg == NULL)
    {
        return TS_FAILURE;
    }

    TS_COMMON_SYS_ShowVersion();

    TS_S32 s32CameraIndex = 0;
    STCHECKRESULT(anj_mw_media_video_attr_init(pstAnjVideoCfg));
    STCHECKRESULT(TS_COMMON_SYS_Init());
    STCHECKRESULT(TS_COMMON_MMZ_Init());

    /*
     * SYS -> VI -> VPSS_Start(各 Grp) -> [离线则 VI_Bind_VPSS] -> TS_MPI_VPSS_Start_Camera
     * -> 各路 VENC：Create 后 H26x 立即 StartRecv + VPSS_Bind_VENC；
     *    JPEG 通道仅 Create + Bind，Recv 见 anj_mw_media_video_jpg_start。
     */
    STCHECKRESULT(anj_mw_media_vi_init());
    for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
    {
        STCHECKRESULT(anj_mw_media_vpss_init(s32CameraIndex));
    }

    if (s_stMwMediaVideoCtx.bViVpssOnline == TS_FALSE)
    {
        for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
        {
            STCHECKRESULT(anj_mw_media_vi_bind_vpss(s32CameraIndex));
        }
    }

    STCHECKRESULT(TS_MPI_VPSS_Start_Camera());

    for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
    {
        STCHECKRESULT(anj_mw_media_video_vpss_thread_start(s32CameraIndex));
        for (TS_S32 s32VencIndex = 0; s32VencIndex < MAX_VENC_CHN; s32VencIndex++)
        {
            TS_Common_VencAttr_t *pstVencAttr = &gstVideoAttr.pVencAttr[s32CameraIndex * MAX_VENC_CHN + s32VencIndex];
            VENC_RECV_PIC_PARAM_S stRecvParam = {0};

            if (pstVencAttr->bEnable == TS_FALSE)
            {
                continue;
            }

            STCHECKRESULT(TS_Common_VencCreateChannel(pstVencAttr));
            stRecvParam.s32RecvPicNum = -1;
            STCHECKRESULT(TS_MPI_VENC_StartRecvFrame(pstVencAttr->VencChnId, &stRecvParam));

            if ((MAX_VPSS_CHN - 1) == pstVencAttr->VpssChn)
            {
                STCHECKRESULT(anj_mw_smart_cpm_start(s32CameraIndex, pstVencAttr->VencChnId, pstVencAttr->u32Width, pstVencAttr->u32Height));
            }
            else
            {
                STCHECKRESULT(anj_mw_media_vpss_bind_venc(pstVencAttr));
            }
        }
        anj_mw_media_video_venc_thread_start(s32CameraIndex);
    }

    return TS_SUCCESS;
}

int anj_mw_media_video_uninit(void)
{
    TS_S32 s32CameraIndex = 0;

    for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
    {
        for (TS_S32 s32VencIndex = 0; s32VencIndex < MAX_VENC_CHN; s32VencIndex++)
        {
            TS_Common_VencAttr_t *pstVencAttr = &gstVideoAttr.pVencAttr[s32CameraIndex * MAX_VENC_CHN + s32VencIndex];

            if ((pstVencAttr == NULL) || (pstVencAttr->bEnable == TS_FALSE))
            {
                continue;
            }

            TS_MPI_VENC_StopRecvFrame(pstVencAttr->VencChnId);
        }
    }

    // stop get-stream / worker threads
    for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
    {
        anj_thread_task_destroy(&s_stVencStreamThread[s32CameraIndex], 0);
        anj_thread_task_destroy(&s_stVpssYuvThread[s32CameraIndex], 0);
    }

    // unbind + destroy VENC
    for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
    {
        for (TS_S32 s32VencIndex = 0; s32VencIndex < MAX_VENC_CHN; s32VencIndex++)
        {
            TS_Common_VencAttr_t *pstVencAttr = &gstVideoAttr.pVencAttr[s32CameraIndex * MAX_VENC_CHN + s32VencIndex];

            if ((pstVencAttr == NULL) || (pstVencAttr->bEnable == TS_FALSE))
            {
                continue;
            }
            if ((MAX_VPSS_CHN - 1) == pstVencAttr->VpssChn)
            {
                anj_mw_smart_cpm_stop(s32CameraIndex, pstVencAttr->VencChnId);
            }
            else
            {
                anj_mw_media_vpss_unbind_venc(pstVencAttr);
            }
            TS_MPI_VENC_DestroyChn(pstVencAttr->VencChnId);
        }
    }

    if (s_stMwMediaVideoCtx.bViVpssOnline == TS_TRUE)
    {
        STCHECKRESULT(anj_mw_media_vi_uninit());
        for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
        {
            STCHECKRESULT(anj_mw_media_vpss_uninit(s32CameraIndex));
        }
    }
    else
    {
        for (s32CameraIndex = 0; s32CameraIndex < ANJ_CAMERA_MAX_NUMS; s32CameraIndex++)
        {
            anj_mw_media_vi_unbind_vpss(s32CameraIndex);
            STCHECKRESULT(anj_mw_media_vpss_uninit(s32CameraIndex));
        }
        STCHECKRESULT(anj_mw_media_vi_uninit());
    }

    // SYS exit
    TS_COMMON_SYS_Exit();
    anj_mw_media_video_attr_uninit();
    return TS_SUCCESS;
}

int anj_mw_media_video_requeset_idr(int Chn, int VencId)
{
    if ((Chn >= ANJ_CAMERA_MAX_NUMS) || (VencId >= MAX_VENC_CHN))
    {
        return TS_FAILURE;
    }

    VENC_CHN VencChn = gstVideoAttr.pVencAttr[VencId].VencChnId;
    return TS_Common_VencRequestIdr(VencChn);
}

int anj_mw_media_video_gop_set(int Chn, int VencId, int gop)
{
    if ((Chn >= ANJ_CAMERA_MAX_NUMS) || (VencId >= MAX_VENC_CHN))
    {
        return TS_FAILURE;
    }

    VENC_CHN VencChn = gstVideoAttr.pVencAttr[VencId].VencChnId;
    return TS_Common_VencSetGop(VencChn, gop);
}

int anj_mw_media_video_bitrate_set(int Chn, int VencId, int bitrate)
{
    if ((Chn >= ANJ_CAMERA_MAX_NUMS) || (VencId >= MAX_VENC_CHN))
    {
        return TS_FAILURE;
    }

    VENC_CHN VencChn = gstVideoAttr.pVencAttr[VencId].VencChnId;
    return TS_Common_VencSetBitrate(VencChn, bitrate);
}

int anj_mw_media_video_fps_set(int Chn, int VencId, int fps)
{
    if ((Chn >= ANJ_CAMERA_MAX_NUMS) || (VencId >= MAX_VENC_CHN))
    {
        return TS_FAILURE;
    }

    VENC_CHN VencChn = gstVideoAttr.pVencAttr[VencId].VencChnId;
    return TS_Common_VencSetFps(VencChn, fps);
}

void *anj_mw_media_video_attr_get(void)
{
    return (void *)&gstVideoAttr;
}

int anj_mw_media_video_config_set(AnjVencConfig *pVenCfg, int iCameraIdx, int chn)
{
    TS_Common_VencAttr_t stVencAttr = {0};
    TS_Common_VencAttr_t *pVencAttr = NULL;

    if ((pVenCfg == NULL) || (iCameraIdx >= ANJ_CAMERA_MAX_NUMS) || (chn >= MAX_VENC_CHN))
    {
        return TS_FAILURE;
    }

    pVencAttr = &gstVideoAttr.pVencAttr[chn];
    memcpy(&stVencAttr, pVencAttr, sizeof(stVencAttr));
    stVencAttr.u32BitRate = pVenCfg->bitrate;
    stVencAttr.u32Profile = pVenCfg->profile;
    stVencAttr.u32Gop = pVenCfg->gop;
    if (pVenCfg->qpenable == 1)
    {
        stVencAttr.u32MaxQp = pVenCfg->maxqp;
        stVencAttr.u32MinQp = pVenCfg->minqp;
    }

    if (TS_Common_VencSetChnAttr(stVencAttr.VencChnId, &stVencAttr) != TS_SUCCESS)
    {
        return TS_FAILURE;
    }

    memcpy(pVencAttr, &stVencAttr, sizeof(stVencAttr));
    return TS_SUCCESS;
}

int anj_mw_media_video_encode_apply(AnjVideoConfig *pstAnjVideoCfg)
{
    int iRet = 0;

    if (pstAnjVideoCfg == NULL)
    {
        return -1;
    }

    iRet = anj_mw_media_video_uninit();
    if (iRet)
    {
        __ERR("encode_apply uninit failed:%d\n", iRet);
        return iRet;
    }

    usleep(500 * 1000);

    iRet = anj_mw_media_video_init(pstAnjVideoCfg);
    if (iRet)
    {
        __ERR("encode_apply init failed:%d\n", iRet);
    }
    return iRet;
}

int anj_mw_media_video_vpss_pause(void)
{
    return TS_SUCCESS;
}

int anj_mw_media_video_vpss_recover(void)
{
    return TS_SUCCESS;
}

int anj_mw_media_video_vpss_crop(int iCameraIdex, double multiple)
{
    TS_U32 u32PipeWidth = gstVideoAttr.ViAttr.astViInfo[iCameraIdex].stPipeInfo.width;
    TS_U32 u32PipeHeight = gstVideoAttr.ViAttr.astViInfo[iCameraIdex].stPipeInfo.height;
    TS_U32 u32BaseWidth = 0;
    TS_U32 u32BaseHeight = 0;
    TS_U32 u32OriginX = 0;
    TS_U32 u32OriginY = 0;
    TS_U32 u32Width = 0;
    TS_U32 u32Height = 0;
    TS_S32 s32X = 0;
    TS_S32 s32Y = 0;
    TS_Common_VpssAttr_t *pstVpssAttr = NULL;

    if (multiple <= 0.0)
    {
        return TS_FAILURE;
    }

    if (gstVideoAttr.pVpssAttr != NULL)
    {
        pstVpssAttr = &gstVideoAttr.pVpssAttr[iCameraIdex * MAX_VPSS_CHN + 0];
        u32BaseWidth = pstVpssAttr->u32Width;
        u32BaseHeight = pstVpssAttr->u32Height;
    }
    if (u32BaseWidth == 0 || u32BaseHeight == 0 || u32BaseWidth > u32PipeWidth || u32BaseHeight > u32PipeHeight)
    {
        u32BaseWidth = u32PipeWidth;
        u32BaseHeight = u32PipeHeight;
    }
    u32BaseWidth = ANJ_ALIGN_DOWN(u32BaseWidth, 4);
    u32BaseHeight = ANJ_ALIGN_DOWN(u32BaseHeight, 4);
    u32OriginX = ANJ_ALIGN_DOWN((u32PipeWidth - u32BaseWidth) / 2, 4);
    u32OriginY = ANJ_ALIGN_DOWN((u32PipeHeight - u32BaseHeight) / 2, 4);

    u32Width = ANJ_ALIGN_DOWN((TS_U32)round((double)u32BaseWidth / multiple), 4);
    u32Height = ANJ_ALIGN_DOWN((TS_U32)round((double)u32BaseHeight / multiple), 4);
    if (u32Width < 4)
    {
        u32Width = 4;
    }
    if (u32Height < 4)
    {
        u32Height = 4;
    }
    if (u32Width > u32BaseWidth)
    {
        u32Width = u32BaseWidth;
    }
    if (u32Height > u32BaseHeight)
    {
        u32Height = u32BaseHeight;
    }
    s32X = (TS_S32)(u32OriginX + ANJ_ALIGN_DOWN((u32BaseWidth - u32Width) / 2, 4));
    s32Y = (TS_S32)(u32OriginY + ANJ_ALIGN_DOWN((u32BaseHeight - u32Height) / 2, 4));

    STCHECKRESULT(TS_COMMON_VPSS_GrpCrop(iCameraIdex, s32X, s32Y, u32Width, u32Height));
    return TS_SUCCESS;
}

int anj_mw_media_video_scl_crop(int iCameraIdex, double multiple)
{
    return anj_mw_media_video_vpss_crop(iCameraIdex, multiple);
}

int anj_mw_media_video_scl_pause(void)
{
    return anj_mw_media_video_vpss_pause();
}

int anj_mw_media_video_scl_recover(void)
{
    return anj_mw_media_video_vpss_recover();
}

/* 软抓拍由 anj_video worker 处理；ts 平台 jpg_start/stop 为空实现 */
int anj_mw_media_video_jpg_start(int iCameraIdex, void *param)
{
    (void)iCameraIdex;
    (void)param;
    return TS_SUCCESS;
}

int anj_mw_media_video_jpg_stop(int iCameraIdex)
{
    (void)iCameraIdex;
    return TS_SUCCESS;
}
