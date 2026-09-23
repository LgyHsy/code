#include <math.h>

#include "anj_mw_media_common.h"
#include "anj_mw_media_video.h"

#include "anj_mw_comm.h"
#include "anj_mw_thread.h"

#define MAX_STREAM_CNT (4)

static ST_Common_VideoAttr_t gstVideoAttr[ANJ_CAMERA_MAX_NUMS];
static anj_thread_s s_stVencStreamThread[ANJ_CAMERA_MAX_NUMS * MAX_VENC_CHN];
static anj_thread_s s_stSclYuvThread[ANJ_CAMERA_MAX_NUMS];
static anj_thread_s s_stJpgThread[ANJ_CAMERA_MAX_NUMS] = {0};

static MI_SYS_GlobalPrivPoolConfig_t stBasePool;
static MI_SYS_GlobalPrivPoolConfig_t stVencPool;

static int anj_mw_media_video_attr_init(AnjVideoConfig *pstAnjVideoCfg)
{
    int iCameraIdex = 0;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        AnjVideoConfig *pstAnjVideoConfig = &pstAnjVideoCfg[iCameraIdex];
        if (iCameraIdex == 0)
        {
            gstVideoAttr[iCameraIdex].SnrPadId = E_MI_VIF_SNRPAD_ID_0;
        }
        else
        {
            gstVideoAttr[iCameraIdex].SnrPadId = E_MI_VIF_SNRPAD_ID_2;
        }
        gstVideoAttr[iCameraIdex].VifAttr.VifGroupId = (gstVideoAttr[iCameraIdex].SnrPadId == E_MI_VIF_SNRPAD_ID_2) ? 1 : 0;
        gstVideoAttr[iCameraIdex].VifAttr.VifDevId = 4 * gstVideoAttr[iCameraIdex].VifAttr.VifGroupId;
        gstVideoAttr[iCameraIdex].VifAttr.VifOutPortId = 0;
        gstVideoAttr[iCameraIdex].IspAttr.IspDevId = MI_ISP_DEV0;
        gstVideoAttr[iCameraIdex].IspAttr.IspChnId = iCameraIdex;
        gstVideoAttr[iCameraIdex].IspAttr.IspOutPortId = 0;
        gstVideoAttr[iCameraIdex].IspAttr.u32SensorBindId = (gstVideoAttr[iCameraIdex].SnrPadId == E_MI_VIF_SNRPAD_ID_2) ? E_MI_ISP_SENSOR2 : E_MI_ISP_SENSOR0;
        gstVideoAttr[iCameraIdex].IspAttr.ePixelFormat = E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420;

        memset(gstVideoAttr[iCameraIdex].SclAttr, 0, sizeof(gstVideoAttr[iCameraIdex].SclAttr));
        for (int i = 0; i < MAX_SCL_PORT; i++)
        {
            int width = 0;
            int height = 0;
            int fps = 0;
            int depth = 0;
            int isjpeg = 0;
            ST_Common_Scl_DataCb datacb = NULL;
            ST_Common_SclAttr_t *pstSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[i];
            pstSclAttr->bEnable = 1;
            pstSclAttr->SclDevId = MI_SCL_DEV_ISP_REALTIME0;
            pstSclAttr->SclChnId = iCameraIdex;
            pstSclAttr->SclOutPortId = i;

            if (i == 0)
            {
                width = pstAnjVideoConfig->stVencCfg[0].width;
                height = pstAnjVideoConfig->stVencCfg[0].height;
                fps = pstAnjVideoConfig->stVencCfg[0].fps;
                depth = 0;
            }
            else if (i == (MAX_SCL_PORT - 1))
            {
                width = DEFAULT_SMART_WIDTH;
                height = DEFAULT_SMART_HEIGHT;
                fps = DEFAULT_SMART_FPS;
                depth = DEFAULT_SMART_DEPTH;
                datacb = pstAnjVideoConfig->yuv_data_cb;
                /* 与 smart 共用口时不标 isjpeg，避免 jpg_start 再起线程；YUV 由 smart_cb 喂 snap */
            }
            else
            {
                width = pstAnjVideoConfig->stVencCfg[1].width;
                height = pstAnjVideoConfig->stVencCfg[1].height;
                fps = pstAnjVideoConfig->stVencCfg[1].fps;
                depth = 3;
                datacb = pstAnjVideoConfig->jpg_data_cb;
#ifdef MODULE_SNAP_SOFT
                isjpeg = 1;
#endif
            }
            pstSclAttr->stSCLOutputSize.u16Width = width;
            pstSclAttr->stSCLOutputSize.u16Height = height;
            pstSclAttr->u32SrcFrmRateNum = fps;
            pstSclAttr->bMirror = 0;
            pstSclAttr->bFlip = 0;
            pstSclAttr->u32DepthSet = depth;
            pstSclAttr->datacb = datacb;
            pstSclAttr->isjpeg = isjpeg;
        }

        gstVideoAttr[iCameraIdex].pVencAttr = anj_mw_malloc(sizeof(ST_Common_VencAttr_t) * MAX_VENC_CHN);
        memset(gstVideoAttr[iCameraIdex].pVencAttr, 0, sizeof(ST_Common_VencAttr_t) * MAX_VENC_CHN);
        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            ST_Common_VencAttr_t *pstVencAttr = &gstVideoAttr[iCameraIdex].pVencAttr[i];
            pstVencAttr->bEnable = pstAnjVideoConfig->stVencCfg[i].enable;
            pstVencAttr->VencDevId = MI_VENC_DEV_ID_H264_H265_0;
            pstVencAttr->VencChnId = pstAnjVideoConfig->stVencCfg[i].chn;

            pstVencAttr->stVencRes.u16Width = pstAnjVideoConfig->stVencCfg[i].width;
            pstVencAttr->stVencRes.u16Height = pstAnjVideoConfig->stVencCfg[i].height;
            pstVencAttr->u32Bufsize = pstAnjVideoConfig->stVencCfg[i].bufszie;
            pstVencAttr->u32Gop = pstAnjVideoConfig->stVencCfg[i].gop;
            pstVencAttr->u32SrcFrmRateNum = pstAnjVideoConfig->stVencCfg[i].fps;
            pstVencAttr->u32BitRate = pstAnjVideoConfig->stVencCfg[i].bitrate;
            pstVencAttr->u32Profile = pstAnjVideoConfig->stVencCfg[i].profile;
            pstVencAttr->u32Qfactor = pstAnjVideoConfig->stVencCfg[i].qfactor;
            pstVencAttr->s32IPQPDelta = pstAnjVideoConfig->stVencCfg[i].qp_delta;
            pstVencAttr->u32MaxISize = pstAnjVideoConfig->stVencCfg[i].maxIsize;
            pstVencAttr->u32MaxPSize = pstAnjVideoConfig->stVencCfg[i].maxPsize;
            pstVencAttr->datacb = (ST_Common_Venc_DataCb)pstAnjVideoConfig->stVencCfg[i].venc_data_cb;
            if (pstAnjVideoConfig->stVencCfg[i].encodeType == MEDIA_CODEC_VIDEO_H264)
            {
                pstVencAttr->eVencType = E_MI_VENC_MODTYPE_H264E;
                if (pstAnjVideoConfig->stVencCfg[i].rcMode == ANJ_VIDEO_CBR)
                {
                    pstVencAttr->eRcMode = E_MI_VENC_RC_MODE_H264CBR;
                }
                else if (pstAnjVideoConfig->stVencCfg[i].rcMode == ANJ_VIDEO_VBR)
                {
                    pstVencAttr->eRcMode = E_MI_VENC_RC_MODE_H264VBR;
                }
                else if (pstAnjVideoConfig->stVencCfg[i].rcMode == ANJ_VIDEO_AVBR)
                {
                    pstVencAttr->eRcMode = E_MI_VENC_RC_MODE_H264AVBR;
                }
                else
                {
                    __ERR("unsupport rcmode:%d\n", pstAnjVideoConfig->stVencCfg[i].rcMode);
                    return -1;
                }
            }
            else if (pstAnjVideoConfig->stVencCfg[i].encodeType == MEDIA_CODEC_VIDEO_H265 ||
                     pstAnjVideoConfig->stVencCfg[i].encodeType == MEDIA_CODEC_VIDEO_H265_PLUS)
            {
                pstVencAttr->eVencType = E_MI_VENC_MODTYPE_H265E;
                if (pstAnjVideoConfig->stVencCfg[i].rcMode == ANJ_VIDEO_CBR)
                {
                    pstVencAttr->eRcMode = E_MI_VENC_RC_MODE_H265CBR;
                }
                else if (pstAnjVideoConfig->stVencCfg[i].rcMode == ANJ_VIDEO_VBR)
                {
                    pstVencAttr->eRcMode = E_MI_VENC_RC_MODE_H265VBR;
                }
                else if (pstAnjVideoConfig->stVencCfg[i].rcMode == ANJ_VIDEO_AVBR)
                {
                    pstVencAttr->eRcMode = E_MI_VENC_RC_MODE_H265AVBR;
                }
                else
                {
                    __ERR("unsupport rcmode:%d\n", pstAnjVideoConfig->stVencCfg[i].rcMode);
                    return -1;
                }
            }
            else if (pstAnjVideoConfig->stVencCfg[i].encodeType == MEDIA_CODEC_VIDEO_JPG)
            {
                pstVencAttr->eVencType = E_MI_VENC_MODTYPE_JPEGE;
                if (pstAnjVideoConfig->stVencCfg[i].rcMode == ANJ_VIDEO_FIXQP)
                {
                    pstVencAttr->eRcMode = E_MI_VENC_RC_MODE_MJPEGFIXQP;
                }
                else
                {
                    __ERR("unsupport rcmode:%d\n", pstAnjVideoConfig->stVencCfg[i].rcMode);
                    return -1;
                }
            }
            else
            {
                __ERR("unsupport encodeType:%d\n", pstAnjVideoConfig->stVencCfg[i].encodeType);
                return -1;
            }
            pstVencAttr->u32MaxQp = pstAnjVideoConfig->stVencCfg[i].maxqp;
            pstVencAttr->u32MinQp = pstAnjVideoConfig->stVencCfg[i].minqp;
            __INFO("qp:%d, %d set to cfg qp:%d, %d\n",
                  pstVencAttr->u32MaxQp, pstVencAttr->u32MinQp,
                  pstAnjVideoConfig->stVencCfg[i].maxqp, pstAnjVideoConfig->stVencCfg[i].minqp);
        }
    }
    return 0;
}

static void anj_mw_media_video_attr_uninit()
{
    int iCameraIdex = 0;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        if (gstVideoAttr[iCameraIdex].pVencAttr)
        {
            anj_mw_free(gstVideoAttr[iCameraIdex].pVencAttr);
            gstVideoAttr[iCameraIdex].pVencAttr = NULL;
        }
    }
}

static int anj_mw_media_scl_yuv_thread(void *ctx, int *bStart)
{
    __LOG_ENTER();
    ST_Common_SclAttr_t *pSclAttr = (ST_Common_SclAttr_t *)ctx;
    MI_SYS_ChnPort_t stSclChnPort = {0};
    stSclChnPort.eModId = E_MI_MODULE_ID_SCL;
    stSclChnPort.u32DevId = pSclAttr->SclDevId;
    stSclChnPort.u32ChnId = pSclAttr->SclChnId;
    stSclChnPort.u32PortId = pSclAttr->SclOutPortId;

    ST_Common_SclGetYuv(&stSclChnPort, pSclAttr->datacb, pSclAttr->param, bStart);
    __LOG_LEAVE();
    return 0;
}

static int anj_mw_media_video_stream_thread(void *ctx, int *bStart)
{
    __LOG_ENTER();
    ST_Common_VencAttr_t *pVencAttr = (ST_Common_VencAttr_t *)ctx;

    ST_Common_VencGetStream(pVencAttr->VencChnId, pVencAttr->eVencType, pVencAttr->datacb, bStart);
    __LOG_LEAVE();
    return 0;
}

int anj_mw_media_video_init(AnjVideoConfig *pstAnjVideoCfg)
{
    MI_U32 iCameraIdex = 0;
    ST_Common_VifAttr_t *pVifAttr = NULL;
    ST_Common_IspAttr_t *pIspAttr = NULL;
    ST_Common_SclAttr_t *pSclAttr = NULL;
    ST_Common_VencAttr_t *pVencAttr = NULL;
    MI_SYS_CompressMode_e eCompressMode = E_MI_SYS_COMPRESS_MODE_NONE;
    MI_VENC_InputSourceConfig_t stInputSourceConfig;

    STCHECKRESULT(anj_mw_media_video_attr_init(pstAnjVideoCfg));

    // sys
    STCHECKRESULT(ST_Common_SysInit());

    MI_SYS_WindowRect_t stSnrRes;
    memset(&stSnrRes, 0, sizeof(MI_SYS_WindowRect_t));
    ST_Common_SensorGetRectInfo(gstVideoAttr[0].SnrPadId, &stSnrRes);

    memset(&stBasePool, 0x0, sizeof(MI_SYS_GlobalPrivPoolConfig_t));
    pSclAttr = &gstVideoAttr[0].SclAttr[0];
    // stBasePool.eConfigType = E_MI_SYS_PER_DEV_PRIVATE_RING_POOL;
    // stBasePool.bCreate = FALSE;
    // stBasePool.uConfig.stpreDevPrivRingPoolConfig.eModule = E_MI_MODULE_ID_SCL;
    // stBasePool.uConfig.stpreDevPrivRingPoolConfig.u32Devid = pSclAttr->SclDevId;
    // stBasePool.uConfig.stpreDevPrivRingPoolConfig.u16MaxWidth = stSnrRes.u16Width;
    // stBasePool.uConfig.stpreDevPrivRingPoolConfig.u16MaxHeight = stSnrRes.u16Height;
    // stBasePool.uConfig.stpreDevPrivRingPoolConfig.u16RingLine =
    //     (eCompressMode == E_MI_SYS_COMPRESS_MODE_IFC) ? (stSnrRes.u16Height / 4) : stSnrRes.u16Height;

    // // MI_SYS_ConfigPrivateMMAPool should be called before scl create.
    // STCHECKRESULT(MI_SYS_ConfigPrivateMMAPool(0, &stBasePool));

    // sensor + vif
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        pVifAttr = &gstVideoAttr[iCameraIdex].VifAttr;
        STCHECKRESULT(ST_Common_SensorInit(gstVideoAttr[iCameraIdex].SnrPadId, pSclAttr->u32SrcFrmRateNum, 1));
        STCHECKRESULT(ST_Common_VifInit(pVifAttr, gstVideoAttr[iCameraIdex].SnrPadId));
    }

    // isp
    pIspAttr = &gstVideoAttr[0].IspAttr;
    STCHECKRESULT(ST_Common_IspInit(pIspAttr->IspDevId));
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        pIspAttr = &gstVideoAttr[iCameraIdex].IspAttr;
        STCHECKRESULT(ST_Common_IspStart(pIspAttr, &stSnrRes));
        STCHECKRESULT(ST_Common_IspAiStart(pIspAttr, 0));
    }

    // scl
    STCHECKRESULT(ST_Common_SclInit(pSclAttr->SclDevId));
    ST_Common_SclStartParam_t stSclStartParam;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];
        memset(&stSnrRes, 0, sizeof(stSnrRes));
        ST_Common_SensorGetRectInfo(gstVideoAttr[iCameraIdex].SnrPadId, &stSnrRes);

        memset(&stSclStartParam, 0, sizeof(ST_Common_SclStartParam_t));
        stSclStartParam.SclDevId = pSclAttr->SclDevId;
        stSclStartParam.SclChnId = iCameraIdex;

        stSclStartParam.stSclChnCropInfo.u16Width = stSnrRes.u16Width;
        stSclStartParam.stSclChnCropInfo.u16Height = stSnrRes.u16Height;

        stSclStartParam.stSclOutputPortCropRect.u16Width = stSnrRes.u16Width;
        stSclStartParam.stSclOutputPortCropRect.u16Height = stSnrRes.u16Height;

        stSclStartParam.stSclOutputSize.u16Width = stSnrRes.u16Width;
        stSclStartParam.stSclOutputSize.u16Height = stSnrRes.u16Height;

        stSclStartParam.ePixelFormat = E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420;

        STCHECKRESULT(ST_Common_SclStart(&stSclStartParam));
        MI_U32 u32SrcFrmrate = pSclAttr->u32SrcFrmRateNum;
        for (MI_U32 i = 0; i < MAX_SCL_PORT; i++)
        {
            pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[i];
            if (pSclAttr->bEnable == 0)
            {
                continue;
            }
            MI_SCL_OutPortParam_t stSclOutputParam;
            memset(&stSclOutputParam, 0, sizeof(MI_SCL_OutPortParam_t));

            stSclStartParam.SclOutPortId = pSclAttr->SclOutPortId;
            stSclStartParam.stSclOutputSize.u16Width = pSclAttr->stSCLOutputSize.u16Width;
            stSclStartParam.stSclOutputSize.u16Height = pSclAttr->stSCLOutputSize.u16Height;
            stSclOutputParam.stSCLOutputSize.u16Width = stSclStartParam.stSclOutputSize.u16Width;
            stSclOutputParam.stSCLOutputSize.u16Height = stSclStartParam.stSclOutputSize.u16Height;

            stSclOutputParam.bFlip = pSclAttr->bFlip;
            stSclOutputParam.bMirror = pSclAttr->bMirror;
            stSclOutputParam.ePixelFormat = stSclStartParam.ePixelFormat;
            stSclOutputParam.eCompressMode = (i == 0) ? eCompressMode : E_MI_SYS_COMPRESS_MODE_NONE;

            ST_Common_SclPortStart(&stSclStartParam, &stSclOutputParam);
            ST_Common_SclPortDepthSet(&stSclStartParam, pSclAttr->u32DepthSet);
            ST_Common_SclPortFrmrateSet(&stSclStartParam, u32SrcFrmrate, pSclAttr->u32SrcFrmRateNum);

            if (i == MAX_SCL_PORT - 1)
            {
                char name[16];
                memset(name, 0, sizeof(name));
                snprintf(name, sizeof(name), "rm_vyuv%d\n", iCameraIdex);
                s_stSclYuvThread[iCameraIdex].bAutoDestroy = 0;
                strncpy(s_stSclYuvThread[iCameraIdex].iThreadName, name, sizeof(s_stSclYuvThread[iCameraIdex].iThreadName) - 1);
                s_stSclYuvThread[iCameraIdex].iThreadjob.ctx = pSclAttr;
                s_stSclYuvThread[iCameraIdex].iThreadjob.func = anj_mw_media_scl_yuv_thread;
                anj_thread_task_create(&s_stSclYuvThread[iCameraIdex]);
            }
        }
    }

    // venc
    pVencAttr = &gstVideoAttr[0].pVencAttr[0];
    STCHECKRESULT(ST_Common_VencInit(pVencAttr));

    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            pVencAttr = &gstVideoAttr[iCameraIdex].pVencAttr[i];
            if ((pVencAttr->bEnable == 0))
            {
                continue;
            }
            if (i != 0 && stVencPool.bCreate != TRUE)
            {
                memset(&stVencPool, 0x0, sizeof(MI_SYS_GlobalPrivPoolConfig_t));
                stVencPool.eConfigType = E_MI_SYS_PER_DEV_PRIVATE_RING_POOL;
                stVencPool.bCreate = TRUE;
                stVencPool.uConfig.stpreDevPrivRingPoolConfig.eModule = E_MI_MODULE_ID_VENC;
                stVencPool.uConfig.stpreDevPrivRingPoolConfig.u32Devid = pVencAttr->VencDevId;
                stVencPool.uConfig.stpreDevPrivRingPoolConfig.u16MaxWidth = pVencAttr->stVencRes.u16Width;
                stVencPool.uConfig.stpreDevPrivRingPoolConfig.u16MaxHeight = pVencAttr->stVencRes.u16Height;
                // here venc ring buffer must be one frame size, that is set to height.
                stVencPool.uConfig.stpreDevPrivRingPoolConfig.u16RingLine = pVencAttr->stVencRes.u16Height;
                STCHECKRESULT(MI_SYS_ConfigPrivateMMAPool(0, &stVencPool)); // config mma pool must before MI_VENC_StartRecvPic;
            }
            STCHECKRESULT(ST_Common_VencCreateChannel(pVencAttr));

            if (i == 0)
            {
                memset(&stInputSourceConfig, 0, sizeof(MI_VENC_InputSourceConfig_t));
                stInputSourceConfig.eInputSrcBufferMode = E_MI_VENC_INPUT_MODE_RING_UNIFIED_DMA;
                STCHECKRESULT(MI_VENC_SetInputSourceConfig(pVencAttr->VencDevId, pVencAttr->VencChnId, &stInputSourceConfig));
            }
        }
    }

    // vif->scl->venc bind
    /*vif bind vpss 双sensor是fram base, 单sensor是realtime */
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        STCHECKRESULT(anj_mw_media_video_bind(iCameraIdex));
    }

    // get venc stream
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            pVencAttr = &gstVideoAttr[iCameraIdex].pVencAttr[i];
            if (pVencAttr->bEnable == 0)
            {
                continue;
            }

            STCHECKRESULT(MI_VENC_StartRecvPic(pVencAttr->VencDevId, pVencAttr->VencChnId));

            STCHECKRESULT(MI_VENC_SetMaxStreamCnt(pVencAttr->VencDevId, pVencAttr->VencChnId, MAX_STREAM_CNT));

            MI_VENC_CHN VencChnId = pVencAttr->VencChnId;
            char name[16];
            memset(name, 0, sizeof(name));
            snprintf(name, sizeof(name), "rm_vstream%d\n", VencChnId);
            s_stVencStreamThread[VencChnId].bAutoDestroy = 0;
            strncpy(s_stVencStreamThread[VencChnId].iThreadName, name, sizeof(s_stVencStreamThread[VencChnId].iThreadName) - 1);
            s_stVencStreamThread[VencChnId].iThreadjob.ctx = pVencAttr;
            s_stVencStreamThread[VencChnId].iThreadjob.func = anj_mw_media_video_stream_thread;
            anj_thread_task_create(&s_stVencStreamThread[VencChnId]);
        }
    }

    return 0;
}

int anj_mw_media_video_uninit(void)
{
    MI_U32 iCameraIdex = 0;
    ST_Common_VencAttr_t *pVencAttr = NULL;

    ST_Common_IspIqStop();

    // venc stop
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            pVencAttr = &gstVideoAttr[iCameraIdex].pVencAttr[i];
            if ((pVencAttr == NULL) || (pVencAttr->bEnable == 0))
            {
                continue;
            }
            MI_VENC_CHN VencChnId = pVencAttr->VencChnId;
            anj_thread_task_destroy(&s_stVencStreamThread[VencChnId], 0);
        }
    }

    // unbind + venc chn destroy
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        STCHECKRESULT(anj_mw_media_video_unbind(iCameraIdex));
        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            pVencAttr = &gstVideoAttr[iCameraIdex].pVencAttr[i];
            if ((pVencAttr == NULL) || (pVencAttr->bEnable == 0))
            {
                continue;
            }
            if (stVencPool.bCreate == TRUE)
            {
                stVencPool.bCreate = FALSE;
                STCHECKRESULT(MI_SYS_ConfigPrivateMMAPool(0, &stVencPool)); // config mma pool must before MI_VENC_StartRecvPic;
            }
            STCHECKRESULT(MI_VENC_DestroyChn(0, pVencAttr->VencChnId));
        }
    }
    STCHECKRESULT(ST_Common_VencUnInit(gstVideoAttr[0].pVencAttr[0].VencDevId));

    // scl
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        anj_thread_task_destroy(&s_stSclYuvThread[iCameraIdex], 0);
        for (int i = 0; i < MAX_SCL_PORT; i++)
        {
            STCHECKRESULT(ST_Common_SclPortStop(&gstVideoAttr[iCameraIdex].SclAttr[i]));
        }
        STCHECKRESULT(ST_Common_SclChnStop(&gstVideoAttr[iCameraIdex].SclAttr[0]));
    }
    STCHECKRESULT(ST_Common_SclUnInit(gstVideoAttr[0].SclAttr[0].SclDevId));
    // isp
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        STCHECKRESULT(ST_Common_IspAiStop(&gstVideoAttr[iCameraIdex].IspAttr));
        STCHECKRESULT(ST_Common_IspStop(&gstVideoAttr[iCameraIdex].IspAttr));
    }
    STCHECKRESULT(ST_Common_IspUnInit(gstVideoAttr[0].IspAttr.IspDevId));

    // vif+sensor
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        STCHECKRESULT(ST_Common_VifUnInit(&gstVideoAttr[iCameraIdex].VifAttr));
        STCHECKRESULT(ST_Common_SensorUnInit(gstVideoAttr[iCameraIdex].SnrPadId));
    }

    if (stBasePool.bCreate == TRUE)
    {
        stBasePool.bCreate = FALSE;
        STCHECKRESULT(MI_SYS_ConfigPrivateMMAPool(0, &stBasePool));
    }
    STCHECKRESULT(ST_Common_SysUnInit());

    anj_mw_media_video_attr_uninit();
    return 0;
}

int anj_mw_media_vi_uninit(int iCameraIdex)
{
    STCHECKRESULT(ST_Common_VifUnInit(&gstVideoAttr[iCameraIdex].VifAttr));
    STCHECKRESULT(ST_Common_SensorUnInit(gstVideoAttr[iCameraIdex].SnrPadId));
    return MI_SUCCESS;
}

int anj_mw_media_vi_init(int iCameraIdex, int choice)
{
    ST_Common_VifAttr_t *pVifAttr = &gstVideoAttr[iCameraIdex].VifAttr;
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];
    STCHECKRESULT(ST_Common_SensorInit(gstVideoAttr[iCameraIdex].SnrPadId, pSclAttr->u32SrcFrmRateNum, (MI_U8)choice));
    STCHECKRESULT(ST_Common_VifInit(pVifAttr, gstVideoAttr[iCameraIdex].SnrPadId));
    return MI_SUCCESS;
}

int anj_mw_media_sensor_uninit(int iCameraIdex)
{
    STCHECKRESULT(ST_Common_SensorUnInit(gstVideoAttr[iCameraIdex].SnrPadId));
    return MI_SUCCESS;
}

int anj_mw_media_sensor_init(int iCameraIdex, int choice)
{
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];
    STCHECKRESULT(ST_Common_SensorInit(gstVideoAttr[iCameraIdex].SnrPadId, pSclAttr->u32SrcFrmRateNum, (MI_U8)choice));
    return MI_SUCCESS;
}

int anj_mw_media_venc_bind_scl(int iCameraIdex, int iSclPortIdx)
{
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];
    ST_Common_VencAttr_t *pVencAttr = &gstVideoAttr[iCameraIdex].pVencAttr[iSclPortIdx];
    if (pVencAttr->bEnable == 0)
    {
        return 0;
    }

    int iSrcFps = pSclAttr->u32SrcFrmRateNum;

    ST_Sys_BindInfo_T stBindInfo;
    memset(&stBindInfo, 0x0, sizeof(ST_Sys_BindInfo_T));

    if (MAX_SCL_PORT < 3)
    {
        if (pVencAttr->VencChnId == 0) // 第一个VENC通道绑定到SCL
        {
            stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_SCL;
            stBindInfo.stSrcChnPort.u32DevId = pSclAttr->SclDevId;
            stBindInfo.stSrcChnPort.u32ChnId = pSclAttr->SclChnId;
            stBindInfo.stSrcChnPort.u32PortId = 0;

            stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_VENC;
            stBindInfo.stDstChnPort.u32DevId = pVencAttr->VencDevId;
            stBindInfo.stDstChnPort.u32ChnId = pVencAttr->VencChnId;
            stBindInfo.stDstChnPort.u32PortId = 0;
        }
        else // 其他VENC通道级联绑定
        {
            stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_VENC;
            stBindInfo.stSrcChnPort.u32DevId = pVencAttr->VencDevId;
            stBindInfo.stSrcChnPort.u32ChnId = pVencAttr->VencChnId - 1;
            stBindInfo.stSrcChnPort.u32PortId = 0;

            stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_VENC;
            stBindInfo.stDstChnPort.u32DevId = pVencAttr->VencDevId;
            stBindInfo.stDstChnPort.u32ChnId = pVencAttr->VencChnId;
            stBindInfo.stDstChnPort.u32PortId = 0;
        }
        stBindInfo.u32SrcFrmrate = iSrcFps;
        stBindInfo.u32DstFrmrate = pVencAttr->u32SrcFrmRateNum;
        stBindInfo.eBindType = E_MI_SYS_BIND_TYPE_REALTIME;
    }
    else
    {
        stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_SCL;
        stBindInfo.stSrcChnPort.u32DevId = pSclAttr->SclDevId;
        stBindInfo.stSrcChnPort.u32ChnId = pSclAttr->SclChnId;
        stBindInfo.stSrcChnPort.u32PortId = iSclPortIdx;

        stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_VENC;
        stBindInfo.stDstChnPort.u32DevId = pVencAttr->VencDevId;
        stBindInfo.stDstChnPort.u32ChnId = pVencAttr->VencChnId;
        stBindInfo.stDstChnPort.u32PortId = 0;

        stBindInfo.u32SrcFrmrate = iSrcFps;
        stBindInfo.u32DstFrmrate = pVencAttr->u32SrcFrmRateNum;
        stBindInfo.eBindType = (pVencAttr->VencChnId == 0) ? E_MI_SYS_BIND_TYPE_HW_RING : E_MI_SYS_BIND_TYPE_FRAME_BASE;
    }

    return ST_Common_SysBind(&stBindInfo);
}

int anj_mw_media_venc_unbind_scl(int iCameraIdex, int iSclPortIdx)
{
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];
    ST_Common_VencAttr_t *pVencAttr = &gstVideoAttr[iCameraIdex].pVencAttr[iSclPortIdx];
    if (pVencAttr->bEnable == 0)
    {
        return 0;
    }

    ST_Sys_BindInfo_T stBindInfo;
    memset(&stBindInfo, 0x0, sizeof(ST_Sys_BindInfo_T));

    if (MAX_SCL_PORT < 3)
    {
        if (pVencAttr->VencChnId == 0)
        {
            stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_SCL;
            stBindInfo.stSrcChnPort.u32DevId = pSclAttr->SclDevId;
            stBindInfo.stSrcChnPort.u32ChnId = pSclAttr->SclChnId;
            stBindInfo.stSrcChnPort.u32PortId = 0;

            stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_VENC;
            stBindInfo.stDstChnPort.u32DevId = pVencAttr->VencDevId;
            stBindInfo.stDstChnPort.u32ChnId = pVencAttr->VencChnId;
            stBindInfo.stDstChnPort.u32PortId = 0;
        }
        else
        {
            stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_VENC;
            stBindInfo.stSrcChnPort.u32DevId = pVencAttr->VencDevId;
            stBindInfo.stSrcChnPort.u32ChnId = pVencAttr->VencChnId - 1;
            stBindInfo.stSrcChnPort.u32PortId = 0;

            stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_VENC;
            stBindInfo.stDstChnPort.u32DevId = pVencAttr->VencDevId;
            stBindInfo.stDstChnPort.u32ChnId = pVencAttr->VencChnId;
            stBindInfo.stDstChnPort.u32PortId = 0;
        }
    }
    else
    {
        stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_SCL;
        stBindInfo.stSrcChnPort.u32DevId = pSclAttr->SclDevId;
        stBindInfo.stSrcChnPort.u32ChnId = pSclAttr->SclChnId;
        stBindInfo.stSrcChnPort.u32PortId = iSclPortIdx;

        stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_VENC;
        stBindInfo.stDstChnPort.u32DevId = pVencAttr->VencDevId;
        stBindInfo.stDstChnPort.u32ChnId = pVencAttr->VencChnId;
        stBindInfo.stDstChnPort.u32PortId = 0;
    }

    return ST_Common_SysUnBind(&stBindInfo);
}

int anj_mw_media_isp_bind_scl(int iCameraIdex)
{
    ST_Common_IspAttr_t *pIspAttr = &gstVideoAttr[iCameraIdex].IspAttr;
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];

    ST_Sys_BindInfo_T stBindInfo;
    memset(&stBindInfo, 0x0, sizeof(ST_Sys_BindInfo_T));

    stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_ISP;
    stBindInfo.stSrcChnPort.u32DevId = pIspAttr->IspDevId;
    stBindInfo.stSrcChnPort.u32ChnId = pIspAttr->IspChnId;
    stBindInfo.stSrcChnPort.u32PortId = pIspAttr->IspOutPortId;

    stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_SCL;
    stBindInfo.stDstChnPort.u32DevId = pSclAttr->SclDevId;
    stBindInfo.stDstChnPort.u32ChnId = pSclAttr->SclChnId;
    stBindInfo.stDstChnPort.u32PortId = pSclAttr->SclOutPortId;

    stBindInfo.u32SrcFrmrate = pSclAttr->u32SrcFrmRateNum;
    stBindInfo.u32DstFrmrate = pSclAttr->u32SrcFrmRateNum;
    stBindInfo.eBindType = E_MI_SYS_BIND_TYPE_REALTIME;

    return ST_Common_SysBind(&stBindInfo);
}

int anj_mw_media_isp_unbind_scl(int iCameraIdex)
{
    ST_Common_IspAttr_t *pIspAttr = &gstVideoAttr[iCameraIdex].IspAttr;
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];

    ST_Sys_BindInfo_T stBindInfo;
    memset(&stBindInfo, 0x0, sizeof(ST_Sys_BindInfo_T));

    stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_ISP;
    stBindInfo.stSrcChnPort.u32DevId = pIspAttr->IspDevId;
    stBindInfo.stSrcChnPort.u32ChnId = pIspAttr->IspChnId;
    stBindInfo.stSrcChnPort.u32PortId = pIspAttr->IspOutPortId;

    stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_SCL;
    stBindInfo.stDstChnPort.u32DevId = pSclAttr->SclDevId;
    stBindInfo.stDstChnPort.u32ChnId = pSclAttr->SclChnId;
    stBindInfo.stDstChnPort.u32PortId = pSclAttr->SclOutPortId;

    return ST_Common_SysUnBind(&stBindInfo);
}

int anj_mw_media_vi_bind_isp(int iCameraIdex)
{
    ST_Common_VifAttr_t *pVifAttr = &gstVideoAttr[iCameraIdex].VifAttr;
    ST_Common_IspAttr_t *pIspAttr = &gstVideoAttr[iCameraIdex].IspAttr;
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];

    ST_Sys_BindInfo_T stBindInfo;
    memset(&stBindInfo, 0x0, sizeof(ST_Sys_BindInfo_T));

    stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_VIF;
    stBindInfo.stSrcChnPort.u32DevId = pVifAttr->VifDevId;
    stBindInfo.stSrcChnPort.u32ChnId = 0;
    stBindInfo.stSrcChnPort.u32PortId = pVifAttr->VifOutPortId;

    stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_ISP;
    stBindInfo.stDstChnPort.u32DevId = pIspAttr->IspDevId;
    stBindInfo.stDstChnPort.u32ChnId = pIspAttr->IspChnId;
    stBindInfo.stDstChnPort.u32PortId = pIspAttr->IspOutPortId;

    stBindInfo.u32SrcFrmrate = pSclAttr->u32SrcFrmRateNum;
    stBindInfo.u32DstFrmrate = pSclAttr->u32SrcFrmRateNum;
    stBindInfo.eBindType = E_MI_SYS_BIND_TYPE_REALTIME;

    return ST_Common_SysBind(&stBindInfo);
}

int anj_mw_media_vi_unbind_isp(int iCameraIdex)
{
    ST_Common_VifAttr_t *pVifAttr = &gstVideoAttr[iCameraIdex].VifAttr;
    ST_Common_IspAttr_t *pIspAttr = &gstVideoAttr[iCameraIdex].IspAttr;

    ST_Sys_BindInfo_T stBindInfo;
    memset(&stBindInfo, 0x0, sizeof(ST_Sys_BindInfo_T));

    stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_VIF;
    stBindInfo.stSrcChnPort.u32DevId = pVifAttr->VifDevId;
    stBindInfo.stSrcChnPort.u32ChnId = 0;
    stBindInfo.stSrcChnPort.u32PortId = pVifAttr->VifOutPortId;

    stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_ISP;
    stBindInfo.stDstChnPort.u32DevId = pIspAttr->IspDevId;
    stBindInfo.stDstChnPort.u32ChnId = pIspAttr->IspChnId;
    stBindInfo.stDstChnPort.u32PortId = pIspAttr->IspOutPortId;

    return ST_Common_SysUnBind(&stBindInfo);
}

int anj_mw_media_video_bind(int iCameraIdex)
{
    // 1. VI绑定ISP
    STCHECKRESULT(anj_mw_media_vi_bind_isp(iCameraIdex));

    // 2. ISP绑定SCL
    STCHECKRESULT(anj_mw_media_isp_bind_scl(iCameraIdex));

    // 3. SCL绑定各个VENC通道
    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        STCHECKRESULT(anj_mw_media_venc_bind_scl(iCameraIdex, i));
    }

    return MI_SUCCESS;
}

int anj_mw_media_video_unbind(int iCameraIdex)
{
    // 1. 解绑各个VENC通道（反向顺序）
    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        STCHECKRESULT(anj_mw_media_venc_unbind_scl(iCameraIdex, i));
    }

    // 2. 解绑SCL和ISP
    STCHECKRESULT(anj_mw_media_isp_unbind_scl(iCameraIdex));

    // 3. 解绑ISP和VI
    STCHECKRESULT(anj_mw_media_vi_unbind_isp(iCameraIdex));

    return MI_SUCCESS;
}

int anj_mw_media_video_requeset_idr(int Chn, int VencId)
{
    if (Chn >= ANJ_CAMERA_MAX_NUMS || VencId >= MAX_VENC_CHN)
    {
        __ERR("input invalid\n");
        return -1;
    }
    ST_Common_VencAttr_t *pVencAttr = &gstVideoAttr[Chn].pVencAttr[VencId];
    if (pVencAttr == NULL)
    {
        __ERR("input invalid\n");
        return -1;
    }
    return ST_Common_VencRequestIdr(MI_VENC_DEV_ID_H264_H265_0, pVencAttr->VencChnId);
}

int anj_mw_media_video_gop_set(int Chn, int VencId, int gop)
{
    ST_Common_VencAttr_t *pVencAttr = &gstVideoAttr[Chn].pVencAttr[VencId];
    if (pVencAttr == NULL)
    {
        __ERR("input invalid\n");
        return -1;
    }
    return ST_Common_VencSetGop(MI_VENC_DEV_ID_H264_H265_0, pVencAttr->VencChnId, gop);
}

int anj_mw_media_video_bitrate_set(int Chn, int VencId, int bitrate)
{
    ST_Common_VencAttr_t *pVencAttr = &gstVideoAttr[Chn].pVencAttr[VencId];
    if (pVencAttr == NULL)
    {
        __ERR("input invalid\n");
        return -1;
    }
    return ST_Common_VencSetBitrate(MI_VENC_DEV_ID_H264_H265_0, pVencAttr->VencChnId, bitrate << 10);
}

int anj_mw_media_video_fps_set(int Chn, int VencId, int fps)
{
    ST_Common_VencAttr_t *pVencAttr = &gstVideoAttr[Chn].pVencAttr[VencId];
    if (pVencAttr == NULL)
    {
        __ERR("input invalid\n");
        return -1;
    }
    return ST_Common_VencSetFps(MI_VENC_DEV_ID_H264_H265_0, pVencAttr->VencChnId, fps);
}

void *anj_mw_media_video_attr_get()
{
    return (void *)gstVideoAttr;
}

int anj_mw_media_video_config_set(AnjVencConfig *pVenCfg, int iCameraIdx, int chn)
{
    int iRet = 0;

    ST_Common_VencAttr_t *pVencAttr = &gstVideoAttr[iCameraIdx].pVencAttr[chn];
    if (pVencAttr == NULL)
    {
        __ERR("input invalid\n");
        return -1;
    }

    ST_Common_VencAttr_t stVencAttr = {0};
    memcpy(&stVencAttr, pVencAttr, sizeof(ST_Common_VencAttr_t));

    if (stVencAttr.eVencType == E_MI_VENC_MODTYPE_H264E)
    {
        if (pVenCfg->rcMode == ANJ_VIDEO_CBR)
        {
            stVencAttr.eRcMode = E_MI_VENC_RC_MODE_H264CBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_VBR)
        {
            stVencAttr.eRcMode = E_MI_VENC_RC_MODE_H264VBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_AVBR)
        {
            stVencAttr.eRcMode = E_MI_VENC_RC_MODE_H264AVBR;
        }
    }
    else if (stVencAttr.eVencType == E_MI_VENC_MODTYPE_H265E)
    {
        if (pVenCfg->rcMode == ANJ_VIDEO_CBR)
        {
            stVencAttr.eRcMode = E_MI_VENC_RC_MODE_H265CBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_VBR)
        {
            stVencAttr.eRcMode = E_MI_VENC_RC_MODE_H265VBR;
        }
        else if (pVenCfg->rcMode == ANJ_VIDEO_AVBR)
        {
            stVencAttr.eRcMode = E_MI_VENC_RC_MODE_H265AVBR;
        }
    }

    stVencAttr.u32BitRate = pVenCfg->bitrate;
    stVencAttr.u32Profile = pVenCfg->profile;
    stVencAttr.u32Gop = pVenCfg->gop;
    stVencAttr.u32SrcFrmRateNum = pVenCfg->fps;

    if (1 == pVenCfg->qpenable)
    {
        stVencAttr.u32MaxQp = pVenCfg->maxqp;
        stVencAttr.u32MinQp = pVenCfg->minqp;
    }
    else
    {
        if (E_MI_VENC_RC_MODE_H265AVBR == stVencAttr.eRcMode || E_MI_VENC_RC_MODE_H265VBR == stVencAttr.eRcMode)
        {
            if (stVencAttr.u32BitRate < 1000 * 1024 && 1 == chn)
            {
                stVencAttr.u32MinQp = 20;
            }
        }
    }

    iRet = ST_Common_VencSetChnAttr(MI_VENC_DEV_ID_H264_H265_0, stVencAttr.VencChnId, &stVencAttr);
    if (iRet)
    {
        __ERR("ST_Common_VencSetChnAttr chn:%d failed:%d!\n", chn, iRet);
        return -1;
    }

    memcpy(pVencAttr, &stVencAttr, sizeof(ST_Common_VencAttr_t));
    return 0;
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

int anj_mw_media_video_scl_pause(void)
{
    // scl
    int iCameraIdex = 0;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        for (int i = 0; i < MAX_SCL_PORT; i++)
        {
            if (gstVideoAttr[iCameraIdex].SclAttr[i].bEnable)
            {
                STCHECKRESULT(ST_Common_SclPause(&gstVideoAttr[iCameraIdex].SclAttr[i]));
            }
        }
    }

    return 0;
}

int anj_mw_media_video_scl_recover(void)
{
    // scl
    int iCameraIdex = 0;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        for (int i = 0; i < MAX_SCL_PORT; i++)
        {
            if (gstVideoAttr[iCameraIdex].SclAttr[i].bEnable)
            {
                STCHECKRESULT(ST_Common_SclRecover(&gstVideoAttr[iCameraIdex].SclAttr[i]));
            }
        }
    }

    return 0;
}

int anj_mw_media_video_scl_crop(int iCameraIdex, double multiple)
{
    ST_Common_SclAttr_t *pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[0];
    if (pSclAttr->bEnable)
    {
        ANJ_SIZE_S tmpSize;
        tmpSize.u32Width = round(pSclAttr->stSCLOutputSize.u16Width / multiple);
        tmpSize.u32Height = round(pSclAttr->stSCLOutputSize.u16Height / multiple);

        MI_SYS_WindowRect_t stOutCropInfo = {0};
        stOutCropInfo.u16X = (pSclAttr->stSCLOutputSize.u16Width - tmpSize.u32Width) / 2;
        stOutCropInfo.u16Y = (pSclAttr->stSCLOutputSize.u16Height - tmpSize.u32Height) / 2;
        stOutCropInfo.u16Width = tmpSize.u32Width;
        stOutCropInfo.u16Height = tmpSize.u32Height;
        // AI通常用最后一个port 不需要crop
        for (int i = 0; i < MAX_SCL_PORT - 1; i++)
        {
            ST_Common_SclAttr_t *SclAttr = &gstVideoAttr[iCameraIdex].SclAttr[i];
            ST_Common_SclCropSet(SclAttr, &stOutCropInfo);
        }
    }
    return 0;
}

int anj_mw_media_video_jpg_start(int iCameraIdex, void *param)
{
    int iRet = 0;
    if (s_stJpgThread[iCameraIdex].pid)
    {
        __ERR("jpg thread already start\n");
        return iRet;
    }

    int i = 0;
    ST_Common_SclAttr_t *pSclAttr = NULL;
    for (i = 0; i < MAX_SCL_PORT; i++)
    {
        pSclAttr = &gstVideoAttr[iCameraIdex].SclAttr[i];
        if (pSclAttr->isjpeg)
        {
            break;
        }
    }
    if (i >= MAX_SCL_PORT)
    {
        /* 与 smart 共用口：无独立 isjpeg，由 smart YUV 喂 snap，此处 no-op */
        return 0;
    }

    pSclAttr->param = param;

    char name[16];
    memset(name, 0, sizeof(name));
    snprintf(name, sizeof(name), "rm_jpg%d\n", iCameraIdex);
    s_stJpgThread[iCameraIdex].bAutoDestroy = 0;
    strncpy(s_stJpgThread[iCameraIdex].iThreadName, name, sizeof(s_stJpgThread[iCameraIdex].iThreadName) - 1);
    s_stJpgThread[iCameraIdex].iThreadjob.ctx = pSclAttr;
    s_stJpgThread[iCameraIdex].iThreadjob.func = anj_mw_media_scl_yuv_thread;
    iRet = anj_thread_task_create(&s_stJpgThread[iCameraIdex]);
    return iRet;
}

int anj_mw_media_video_jpg_stop(int iCameraIdex)
{
    anj_thread_task_destroy(&s_stJpgThread[iCameraIdex], 0);
    memset(&s_stJpgThread[iCameraIdex], 0, sizeof(anj_thread_s));
    return 0;
}
