#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_module.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "anj_mw_time.h"
#include "anj_mw_mutex.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_smart.h"
#include "anj_mbuf.h"
#include "anj_mw_media_video.h"
#include "anj_mw_media_isp.h"
#include "anj_config.h"
#include "anj_sdcard.h"
#include "anj_net.h"
#include "anj_mw_net.h"
#include "anj_sysmng.h"
#include "anj_ser_api.h"
#include "anj_ser_stream.h"
#include "anj_record.h"
#include "record_log.h"
#include "eventhub.h"
#include "anj_config_system.h"

typedef struct
{
    int min_qp;
    int max_qp;
} VIDEO_RC_ENTRY;

#define VIDEO_QOS_CHECK_INTERVAL_S (5)
#define VIDEO_QOS_INIT_SLEEP_S (10)
#define VIDEO_QOS_STREAM_BREAK_MS (60 * 1000)
#define VIDEO_QOS_MAX_FPS (15)
#define VIDEO_QOS_MIN_PERCENT (35)
#define VIDEO_QOS_MAX_PERCENT (100)

static anj_thread_s s_stVideoTesthread;
static anj_thread_s s_stVideoQosThread = {0};

static AnjVideoConfig gstAnjVideoCfg[ANJ_CAMERA_MAX_NUMS] = {0};

static pthread_mutex_t s_gVideoCfgMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_gVideoRestartMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_iVideoRestartBusy = 0;
static VideoEncode *s_stUserVideoEnc = NULL;
static int s_bQosVideo = 0;
static int s_nQosPercent = 100;
static int s_bQosForceRecheck = 0;
static int s_stVideoInit = 0;

static int anj_video_init(void);
static int anj_video_uninit(void);
static int anj_video_attr_init(AnjVideoConfig *pstAnjVideoCfg);

static int anj_video_qos_target_bitrate(const VideoEncodeCfg *pMainEnc, int bitRatePercent, int tvsystem)
{
    int width = 0;
    int height = 0;
    int wh = 0;
    int targetbitRate = 500;

    GetVideoSize(pMainEnc->resolution.name, tvsystem, &width, &height);
    wh = width * height;
    if (wh > 5038848)
    {
        targetbitRate = 3000;
    }
    else if (wh >= 3686400)
    {
        targetbitRate = 1500;
    }
    else if (wh >= 2073600)
    {
        targetbitRate = 800;
    }

    if (bitRatePercent > VIDEO_QOS_MAX_PERCENT)
    {
        bitRatePercent = VIDEO_QOS_MAX_PERCENT;
    }
    else if (bitRatePercent < VIDEO_QOS_MIN_PERCENT)
    {
        bitRatePercent = VIDEO_QOS_MIN_PERCENT;
    }

    return (int)((double)targetbitRate * (double)bitRatePercent / 100.0);
}

static void anj_video_qos_modify_enc(VideoEncode *pstVideoEncode, VideoQoSConfig *pQos,
                                     int bitRatePercent, int tvsystem)
{
    VideoEncodeCfg *pMainEnc = &pstVideoEncode->encodeCfg[0];

    if (pstVideoEncode == NULL || pQos == NULL || pMainEnc->enable == 0)
    {
        return;
    }

    if (pQos->adjust_bitrate)
    {
        int targetbitRate = anj_video_qos_target_bitrate(pMainEnc, bitRatePercent, tvsystem);

        if (strcmp(pMainEnc->encodeFormat.name, "H265") == 0)
        {
            strncpy(pMainEnc->encodeFormat.name, "H265+", sizeof(pMainEnc->encodeFormat.name) - 1);
            pMainEnc->encodeFormat.name[sizeof(pMainEnc->encodeFormat.name) - 1] = '\0';
        }
        strncpy(pMainEnc->bitRateControl.name, "VBR", sizeof(pMainEnc->bitRateControl.name) - 1);
        pMainEnc->bitRateControl.name[sizeof(pMainEnc->bitRateControl.name) - 1] = '\0';
        if (pMainEnc->bitRate > targetbitRate)
        {
            pMainEnc->bitRate = targetbitRate;
        }
    }

    if (pQos->adjust_fps)
    {
        if (pMainEnc->frameRate > VIDEO_QOS_MAX_FPS)
        {
            pMainEnc->frameRate = VIDEO_QOS_MAX_FPS;
            pMainEnc->initQuant = 60;
        }
        if (pMainEnc->display_frameRate > VIDEO_QOS_MAX_FPS)
        {
            pMainEnc->display_frameRate = VIDEO_QOS_MAX_FPS;
        }
    }
}

static int anj_video_qos_encode_rc_changed(const VideoEncode *pstOldEnc, const VideoEncode *pstNewEnc)
{
    if (strcmp(pstOldEnc->encodeCfg[0].encodeFormat.name, pstNewEnc->encodeCfg[0].encodeFormat.name) != 0)
    {
        return 1;
    }
    if (strcmp(pstOldEnc->encodeCfg[0].bitRateControl.name, pstNewEnc->encodeCfg[0].bitRateControl.name) != 0)
    {
        return 1;
    }
    return 0;
}

static int anj_video_qos_runtime_apply(MediaConfig *pstMediaCfg)
{
    int iRet = 0;
    int iCameraIdex = 0;
    int iStreamIdx = 0;

    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        VideoEncode *pstVideoEncode = &pstMediaCfg->videoConfig[iCameraIdex].videoEncode;
        for (iStreamIdx = 0; iStreamIdx < MAX_VENC_CHN; iStreamIdx++)
        {
            int fps = pstVideoEncode->encodeCfg[iStreamIdx].frameRate;
            int bitrate = pstVideoEncode->encodeCfg[iStreamIdx].bitRate;

            if (pstVideoEncode->encodeCfg[iStreamIdx].enable == 0)
            {
                continue;
            }

            if (gstAnjVideoCfg[iCameraIdex].stVencCfg[iStreamIdx].fps != fps)
            {
                iRet |= anj_mw_media_video_fps_set(iCameraIdex, iStreamIdx, fps);
                if (iRet == 0)
                {
                    gstAnjVideoCfg[iCameraIdex].stVencCfg[iStreamIdx].fps = fps;
                }
            }

            if (gstAnjVideoCfg[iCameraIdex].stVencCfg[iStreamIdx].bitrate != bitrate)
            {
                iRet |= anj_mw_media_video_bitrate_set(iCameraIdex, iStreamIdx, bitrate);
                if (iRet == 0)
                {
                    gstAnjVideoCfg[iCameraIdex].stVencCfg[iStreamIdx].bitrate = bitrate;
                }
            }
        }
    }
    return iRet;
}

static void anj_video_qos_apply_locked(int bActive, int nQosPercent)
{
    MediaConfig *pstMediaCfg = getMediaConfig();
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    VideoQoSConfig *pQosCfg = &pstSystemCfg->videoQosCfg;
    int needSwitch = 0;
    int iCameraIdex = 0;

    if (s_stUserVideoEnc == NULL)
    {
        return;
    }

    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        VideoEncode stBeforeEnc = pstMediaCfg->videoConfig[iCameraIdex].videoEncode;
        int tvsystem = pstMediaCfg->videoConfig[iCameraIdex].videoCapture.tvsystem;

        pstMediaCfg->videoConfig[iCameraIdex].videoEncode = s_stUserVideoEnc[iCameraIdex];
        if (bActive)
        {
            anj_video_qos_modify_enc(&pstMediaCfg->videoConfig[iCameraIdex].videoEncode,
                                     pQosCfg, nQosPercent, tvsystem);
        }
        if (anj_video_qos_encode_rc_changed(&stBeforeEnc, &pstMediaCfg->videoConfig[iCameraIdex].videoEncode))
        {
            needSwitch = 1;
        }
    }

    if (needSwitch)
    {
        anj_mutex_unlock(&s_gVideoCfgMutex);
        if (anj_video_encode_switch() != 0)
        {
            __ERR("video qos encode switch failed\n");
        }
        anj_mutex_lock(&s_gVideoCfgMutex);
    }
    else if (anj_video_qos_runtime_apply(pstMediaCfg) != 0)
    {
        __ERR("video qos runtime apply failed\n");
    }
}

static int anj_video_qos_wireless_up(void)
{
    anj_net_status_e eStatus = anj_net_status_check();

    if (eStatus == ANJ_NET_STATUS_4G)
    {
        return is_network_connect(WIRE_INTERFACE_NAME1) != 0;
    }

    if (eStatus == ANJ_NET_STATUS_WIFI)
    {
        const char *pIfname = net_get_wireless_name();

        if (pIfname == NULL || pIfname[0] == '\0')
        {
            return 0;
        }
        return is_network_interface_up(pIfname);
    }

    return 0;
}

static int anj_video_qos_link_percent(void)
{
    if (anj_net_status_check() == ANJ_NET_STATUS_4G)
    {
        return 100;
    }

    NETWORK_STATUS_DATA stNetStatus = {0};

    if (anj_net_info_get(&stNetStatus) != 0)
    {
        return 100;
    }
    if (stNetStatus.linkquality < 60)
    {
        return 50;
    }
    if (stNetStatus.linkquality < 80)
    {
        return 80;
    }
    return 100;
}

static void anj_video_qos_check(void)
{
    if (s_stVideoInit == 0 || s_stUserVideoEnc == NULL)
    {
        return;
    }

    VideoQoSConfig *pQosCfg = &((SystemConfig *)getSystemConfig())->videoQosCfg;
    P2pLoginState_t *pP2pState = &getSerInfo()->stP2pLoginState;
    const int bForceRecheck = s_bQosForceRecheck;
    const int appBinded = anj_mw_file_exists(P2P_DEVICEBIND_FLAG);
    const int sdOrRec = (anj_record_status_get() == REC_STATUS_NORMAL) ||
                        (anj_sdcard_status_get() != ANJ_SDCARD_STATUS_NOT_INSERT);
    const int p2pOnline = pP2pState->logined > 0 && appBinded;
    int bNeedSet = 0;
    int nQosPercent = 100;

    s_bQosForceRecheck = 0;

    if (pQosCfg->enable_by_network && appBinded && anj_video_qos_wireless_up())
    {
        bNeedSet = 1;
        nQosPercent = anj_video_qos_link_percent();
    }
    else if (sdOrRec && (pQosCfg->enable_by_sdcard ||
                          (pQosCfg->enable_by_sdcardandcloud && p2pOnline)))
    {
        bNeedSet = 1;
    }
    else if (pQosCfg->enable_by_cloud && p2pOnline)
    {
        bNeedSet = 1;
    }
    else if (pQosCfg->enable_by_cloudstorage && p2pOnline && pP2pState->cloudstorage > 0)
    {
        bNeedSet = 1;
    }

    if (bNeedSet != s_bQosVideo || nQosPercent != s_nQosPercent || bForceRecheck)
    {
        __INFO("video qos active:%d->%d percent:%d->%d\n",
               s_bQosVideo, bNeedSet, s_nQosPercent, nQosPercent);
        s_bQosVideo = bNeedSet;
        s_nQosPercent = nQosPercent;
        anj_mutex_lock(&s_gVideoCfgMutex);
        anj_video_qos_apply_locked(bNeedSet, nQosPercent);
        anj_mutex_unlock(&s_gVideoCfgMutex);
    }
}

void anj_video_qos_notify(void)
{
    s_bQosForceRecheck = 1;
}

static void anj_video_qos_check_stream(void)
{
    AnjVencConfig *pstVencCfg = NULL;
    int iCameraIdex = 0;
    int iVencIdx = 0;

    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        for (iVencIdx = 0; iVencIdx < MAX_VENC_CHN; iVencIdx++)
        {
            pstVencCfg = &gstAnjVideoCfg[iCameraIdex].stVencCfg[iVencIdx];
            if (pstVencCfg->enable == 0)
            {
                continue;
            }
            unsigned long long ullLastFrameTimeMs = pstVencCfg->stream_state.last_frame_time_ms;
            if (ullLastFrameTimeMs == 0)
            {
                continue;
            }
            unsigned long long ullIntervalMs = anj_mw_get_cputime_ms(NULL) - ullLastFrameTimeMs;
            if (ullIntervalMs >= VIDEO_QOS_STREAM_BREAK_MS)
            {
                __ERR("Venc chn:%d break %dms, reboot\n", pstVencCfg->chn, ullIntervalMs);
                __RECORD_LOG_INFO("Venc chn:%d break %dms, reboot\n", pstVencCfg->chn, ullIntervalMs);
                anj_sysmng_delay_reboot(1);
            }
        }
    }
}

static int anj_video_qos_thread(void *ctx, int *bStart)
{
    (void)ctx;
    sleep(VIDEO_QOS_INIT_SLEEP_S);

    while (bStart && *bStart)
    {
        anj_video_qos_check();
        anj_video_qos_check_stream();
        sleep(VIDEO_QOS_CHECK_INTERVAL_S);
    }
    return 0;
}

static void anj_video_qos_start(void)
{
    MediaConfig *pstMediaCfg = getMediaConfig();
    int iCameraIdex = 0;

    if (s_stUserVideoEnc == NULL)
    {
        s_stUserVideoEnc = anj_mw_malloc(sizeof(VideoEncode) * ANJ_CAMERA_MAX_NUMS);
    }
    if (s_stUserVideoEnc != NULL)
    {
        for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
        {
            s_stUserVideoEnc[iCameraIdex] = pstMediaCfg->videoConfig[iCameraIdex].videoEncode;
        }
    }

    memset(&s_stVideoQosThread, 0, sizeof(anj_thread_s));
    s_stVideoQosThread.bAutoDestroy = 1;
    strncpy(s_stVideoQosThread.iThreadName, "video_qos_thread",
            sizeof(s_stVideoQosThread.iThreadName) - 1);
    s_stVideoQosThread.iThreadjob.ctx = &s_stVideoQosThread;
    s_stVideoQosThread.iThreadjob.func = anj_video_qos_thread;
    anj_thread_task_create(&s_stVideoQosThread);
}

static void anj_video_qos_stop(void)
{
    anj_thread_task_destroy(&s_stVideoQosThread, -1);
    s_bQosVideo = 0;
    s_nQosPercent = 100;
    s_bQosForceRecheck = 0;
}

static int anj_video_calc_bitrate(int iCameraIdex, int VencChn, int len, int isKey)
{
    STREAM_STATE *pstStreamState = &gstAnjVideoCfg[iCameraIdex].stVencCfg[VencChn].stream_state;
    /* 记录一个gop的数据总量来计算实际码率和帧率，向前滑动1s数据重新估算码率和帧率 */
    int fps = gstAnjVideoCfg[iCameraIdex].stVencCfg[VencChn].fps;
    int gop = gstAnjVideoCfg[iCameraIdex].stVencCfg[VencChn].gop;
    /*一个gop的整数秒数*/
    int gop_in_second = gop / fps; /*gop_in_second 每个gop总共秒数*/
    /*一个gop的精确秒数*/
    float gop_in_second_f = gop * 1.0 / fps;

    double bitrate = 0.0;
    double frame_rate = 0.0;

    /*一个完整的gop*/
    if (isKey && (pstStreamState->gop_sequence_number != gop && pstStreamState->gop_sequence_number != 0))
    {
        pstStreamState->gop_sequence_number = 0;
        pstStreamState->is_reset_gop = 1;
        pstStreamState->second_seq_in_gop = 0;
        memset(pstStreamState->gop_second_byte, 0, sizeof(pstStreamState->gop_second_byte));
        memset(pstStreamState->gop_second_frame, 0, sizeof(pstStreamState->gop_second_frame));
    }

    pstStreamState->gop_sequence_number++;
    if (pstStreamState->second_seq_in_gop >= sizeof(pstStreamState->gop_second_byte) / sizeof(unsigned long))
    {
        __ERR("isKey = %d, gop = %d, second_seq_in_gop = %d gop_in_second = %d, pstStreamState->gop_sequence_number = %d\n",
              isKey, gop, pstStreamState->second_seq_in_gop, gop_in_second, pstStreamState->gop_sequence_number);
    }
    pstStreamState->gop_second_byte[pstStreamState->second_seq_in_gop] += len;
    pstStreamState->gop_second_frame[pstStreamState->second_seq_in_gop]++;

    /*第一个GOP 或者强制IDR帧后的重新复位的第一个GOP*/
    if (pstStreamState->is_reset_gop)
    {
        /*每秒更新GOP 中秒的序列号，从0开始*/
        if (pstStreamState->gop_sequence_number % fps == 0)
        {
            /*当GOP不是帧率的整数倍时，GOP的最后一秒需要跟随后面剩余的帧一起计算码率
              距离下一个GOP不足帧率数的帧  判断是否是GOP的最后一秒
              此时GOP 中秒的序列号不能增加，最后不足一秒帧率的数据和最后一秒存放到一个gop_second_byte
             */
            if ((gop - pstStreamState->gop_sequence_number) >= fps)
            {
                pstStreamState->second_seq_in_gop++;
                /*当GOP 很长时，还没到达一个GOP的帧数时，累计多少数据，算平均值，每秒更新*/
                bitrate = 0.0;
                for (int i = 0; i < pstStreamState->second_seq_in_gop; i++)
                {
                    bitrate += pstStreamState->gop_second_byte[i];
                }
                bitrate = bitrate * 8 / (pstStreamState->second_seq_in_gop) / 1000;
                pstStreamState->bitrate = (int)(bitrate + 0.5);
                frame_rate = pstStreamState->gop_sequence_number * 1.0 / pstStreamState->second_seq_in_gop;
                pstStreamState->frame_rate = (int)(frame_rate + 0.5);
            }
        }
        /*第一个GOP需要一个GOP的数据才能计算码率*/
        if (pstStreamState->gop_sequence_number >= gop)
        {
            pstStreamState->is_reset_gop = 0;
            pstStreamState->second_seq_in_gop = 0;
            /*超过一个GOP复位计数*/
            bitrate = 0.0;
            for (int i = 0; i < gop_in_second; i++)
            {
                bitrate += pstStreamState->gop_second_byte[i];
            }
            /*包含一个GOP的数据*/
            bitrate = bitrate * 8 / (gop_in_second_f) / 1000;
            pstStreamState->bitrate = (int)(bitrate + 0.5);
            frame_rate = pstStreamState->gop_sequence_number * 1.0 / gop_in_second_f;
            pstStreamState->frame_rate = (int)(frame_rate + 0.5);
            /*清零第一秒的数据*/
            pstStreamState->gop_second_byte[pstStreamState->second_seq_in_gop] = 0;
            pstStreamState->gop_second_frame[pstStreamState->second_seq_in_gop] = 0;
        }
    }
    else
    {
        if (pstStreamState->gop_sequence_number % fps == 0)
        {
            /*当GOP不是帧率的整数倍时，GOP的最后一秒需要跟随后面剩余的帧一起计算码率
              距离下一个GOP不足帧率数的帧  判断是否是GOP的最后一秒
              此时GOP 中秒的序列号不能增加，最后不足一秒帧率的数据和最后一秒存放到一个gop_second_byte
             */
            if ((gop - pstStreamState->gop_sequence_number) >= fps)
            {
                pstStreamState->second_seq_in_gop++;

                bitrate = 0.0;
                for (int i = 0; i < gop_in_second; i++)
                {
                    bitrate += pstStreamState->gop_second_byte[i];
                }
                /*gop_in_second_f是一个gop的精确秒数*/
                bitrate = bitrate * 8 / (gop_in_second_f) / 1000;
                pstStreamState->bitrate = (int)(bitrate + 0.5);
                frame_rate = 0.0;
                for (int i = 0; i < gop_in_second; i++)
                {
                    frame_rate += pstStreamState->gop_second_frame[i];
                }
                frame_rate = frame_rate / gop_in_second_f;
                pstStreamState->frame_rate = (int)(frame_rate + 0.5);
                /*清零上个GOP的秒的序列号数据留给当前GOP，等效于覆盖
                上个GOP一秒的帧数据实现每次滑动一秒的数据*/
                pstStreamState->gop_second_byte[pstStreamState->second_seq_in_gop] = 0;
                pstStreamState->gop_second_frame[pstStreamState->second_seq_in_gop] = 0;
            }
        }
        if (pstStreamState->gop_sequence_number >= gop)
        {
            pstStreamState->second_seq_in_gop = 0;

            bitrate = 0.0;
            for (int i = 0; i < gop_in_second; i++)
            {
                bitrate += pstStreamState->gop_second_byte[i];
            }
            bitrate = bitrate * 8 / (gop_in_second_f) / 1000;
            pstStreamState->bitrate = (int)(bitrate + 0.5);
            frame_rate = 0.0;
            for (int i = 0; i < gop_in_second; i++)
            {
                frame_rate += pstStreamState->gop_second_frame[i];
            }
            frame_rate = frame_rate / gop_in_second_f;
            pstStreamState->frame_rate = (int)(frame_rate + 0.5);

            pstStreamState->gop_second_byte[pstStreamState->second_seq_in_gop] = 0;
            pstStreamState->gop_second_frame[pstStreamState->second_seq_in_gop] = 0;
        }
    }

    if (pstStreamState->gop_sequence_number >= gop)
    {
        pstStreamState->gop_sequence_number = 0;
    }

    return 0;
}

static void anj_video_data_cb(int VencChn, int iskey, char *data, int len, unsigned int u32Seq, int codec, unsigned long long int timestamp)
{
    if (s_stVideoInit == 0)
        return;
    static unsigned int suLastIndex[MAX_VENC_CHN] = {0};
    static unsigned int suLastKeyIndex[MAX_VENC_CHN] = {0};

    if (data && (len > 0))
    {
        media_frame_info_t stFrameInfo = {0};
        stFrameInfo.frameBuf = (unsigned char *)data;
        stFrameInfo.frameParam.frameLen = len;
        stFrameInfo.frameParam.vframeIndex = suLastIndex[VencChn]++;
        if (iskey)
        {
            suLastKeyIndex[VencChn] = stFrameInfo.frameParam.vframeIndex;
            stFrameInfo.frameParam.frameKeyIndex = stFrameInfo.frameParam.vframeIndex;
            stFrameInfo.frameParam.frameType = MEDIA_VFRAME_I;
        }
        else
        {
            stFrameInfo.frameParam.frameKeyIndex = suLastKeyIndex[VencChn];
            stFrameInfo.frameParam.frameType = MEDIA_VFRAME_P;
        }
        stFrameInfo.frameParam.frameCodec = (media_codec_type_e)codec;
        stFrameInfo.frameParam.framePts = timestamp / 1000;
        stFrameInfo.frameParam.frameTime = time(NULL);

#if 0
        __INFO("VencChn:%d Get frame(%u) %d. pts:%llu,%lu, codec:%d key:%d\n", VencChn, stFrameInfo.frameParam.vframeIndex,
                    stFrameInfo.frameParam.frameLen, stFrameInfo.frameParam.framePts, stFrameInfo.frameParam.frameTime,
                    codec, iskey);
#endif
        anj_mbuf_video_write_frame(VencChn, &stFrameInfo);
        int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (VencChn / ANJ_CAMERA_MAX_NUMS);
        int vencChn = (ANJ_CAMERA_MAX_NUMS == 1) ? VencChn : (VencChn % ANJ_CAMERA_MAX_NUMS);
        anj_video_calc_bitrate(iCameraIdex, vencChn, len, iskey);

        STREAM_STATE *pstStreamState = &gstAnjVideoCfg[iCameraIdex].stVencCfg[vencChn].stream_state;
        pstStreamState->last_frame_time_ms = anj_mw_get_cputime_ms(NULL);
    }
}

static int anj_video_encode_switch_thread(void *ctx, int *bStart)
{
    anj_thread_s *pThread = (anj_thread_s *)ctx;
    int iRet = 0;

#if 0
    anj_record_uninit();
    module_uninit_single("anj_nfs");
    module_uninit_single("anj_snap");
    module_uninit_single("anj_smart");
    module_uninit_single("anj_audio");
    module_uninit_single("anj_osd");
    module_uninit_single("anj_ispctl");
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_RTSP_RESTART, &event_result, NULL);

    anj_ser_stream_release();

    /* 刷新 attr；busy 期间跳过 mbuf；保持 s_stVideoInit / test / adapt */
    iRet = anj_video_attr_init(gstAnjVideoCfg);
    if (iRet)
    {
        __ERR("anj_video_attr_init failed:%d!\n", iRet);
        iRet = -1;
        goto end;
    }

    iRet = anj_mw_media_video_encode_apply(gstAnjVideoCfg);
    if (iRet)
    {
        __ERR("anj_mw_media_video_encode_apply failed:%d!\n", iRet);
        iRet = -1;
        goto end;
    }

    module_init_single("anj_ispctl");
    module_init_single("anj_osd");
    module_init_single("anj_audio");
    module_init_single("anj_smart");
    module_init_single("anj_snap");
    module_init_single("anj_nfs");
    anj_ser_stream_create();

    anj_sdcard_info *pstSdInfo = anj_sdcard_info_get();
    if (pstSdInfo->eStatus >= ANJ_SDCARD_STATUS_NORMAL)
    {
        char stMountPath[64] = {0};
        snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, anj_sdcard_mount_index_get());
        anj_record_init(stMountPath, 1, 0);
    }

    __INFO("anj_video_encode_switch_thread end\n");
#else
    /* 与 killall anjcam 相同：SIGTERM -> main 优雅退出，anjdaemon 再拉起 */
    __INFO("video encode switch: SIGTERM anjcam for daemon restart\n");
    kill(getpid(), SIGTERM);
#endif
end:
    pThread->start = 0;
    anj_mutex_lock(&s_gVideoRestartMutex);
    s_iVideoRestartBusy = 0;
    anj_mutex_unlock(&s_gVideoRestartMutex);
    return iRet;
}

static int anj_video_attr_init(AnjVideoConfig *pstAnjVideoCfg)
{
    int vencChn = 0;
    MediaConfig *pstMediaCfg = getMediaConfig();
    VideoConfig stVideoCfg = {0};
    memcpy(&stVideoCfg, &pstMediaCfg->videoConfig, sizeof(VideoConfig));

    VIDEO_RC_ENTRY video_rc_list[MEDIA_CODEC_VIDEO_JPG][ANJ_VIDEO_FIXQP] = {VIDEO_RC_LIST};
    for (int iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        AnjVideoConfig *pstAnjVideoConfig = &pstAnjVideoCfg[iCameraIdex];
        /* 独立 SCL 时 MW 用此回调喂软抓拍；硬编/无独立口时不会走到 */
        pstAnjVideoConfig->jpg_data_cb = anj_snap_on_yuv;
        pstAnjVideoConfig->yuv_data_cb = anj_smart_data_cb;
        pstAnjVideoConfig->wdr_enable = pstMediaCfg->videoConfig[iCameraIdex].videoCapture.wdr_mode;
        pstAnjVideoConfig->hflip = pstMediaCfg->videoConfig[iCameraIdex].videoCapture.hflip;
        pstAnjVideoConfig->vflip = pstMediaCfg->videoConfig[iCameraIdex].videoCapture.vflip;
        for (int i = 0; i < MAX_VENC_CHN; i++)
        {
            VideoEncodeCfg *pVideoEncCfg = &stVideoCfg.videoEncode.encodeCfg[i];
            if (pVideoEncCfg->enable == 0)
            {
                if (i == 0)
                {
                    __ERR("VideoEncodeCfg error!\n");
                    return -1;
                }
                continue;
            }
            ANJ_SIZE_S picSize =
                getPicSize(pVideoEncCfg->resolution.name, stVideoCfg.videoCapture.tvsystem, stVideoCfg.videoCapture.rotate, 0);

            // VENC ATTR
            pstAnjVideoConfig->stVencCfg[i].enable = 1;
            pstAnjVideoConfig->stVencCfg[i].chn = MAX_VENC_CHN * iCameraIdex + i;
            pstAnjVideoConfig->stVencCfg[i].width = picSize.u32Width;
            pstAnjVideoConfig->stVencCfg[i].height = picSize.u32Height;
            pstAnjVideoConfig->stVencCfg[i].fps = pVideoEncCfg->frameRate;
            pstAnjVideoConfig->stVencCfg[i].gop = pVideoEncCfg->initQuant;
            pstAnjVideoConfig->stVencCfg[i].bitrate = pVideoEncCfg->bitRate;
            pstAnjVideoConfig->stVencCfg[i].profile = stVideoCfg.videoEncode.encode_profile;
            pstAnjVideoConfig->stVencCfg[i].venc_data_cb = anj_video_data_cb;
            pstAnjVideoConfig->stVencCfg[i].qp_delta = 0;

            if (i == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].bufszie = picSize.u32Width * picSize.u32Height * BUFSZIE_VENC0_COEF;
            }
            else if (i == 1)
            {
                pstAnjVideoConfig->stVencCfg[i].bufszie = picSize.u32Width * picSize.u32Height * BUFSZIE_VENC1_COEF;
            }

            if (strstr(pVideoEncCfg->encodeFormat.name, "H265"))
            {
                pstAnjVideoConfig->stVencCfg[i].encodeType = MEDIA_CODEC_VIDEO_H265;
            }
            else if (strcmp(pVideoEncCfg->encodeFormat.name, "H264") == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].encodeType = MEDIA_CODEC_VIDEO_H264;
            }
            else if (strcmp(pVideoEncCfg->encodeFormat.name, "JPEG") == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].encodeType = MEDIA_CODEC_VIDEO_JPG;
            }
            else if (strcmp(pVideoEncCfg->encodeFormat.name, "MJPEG") == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].encodeType = MEDIA_CODEC_VIDEO_MJPG;
            }
            else
            {
                __ERR("invalid encodeFormat:%s\n", pVideoEncCfg->encodeFormat.name);
                return -1;
            }

            if (strcmp(pVideoEncCfg->bitRateControl.name, "CBR") == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].rcMode = ANJ_VIDEO_CBR;
            }
            else if (strcmp(pVideoEncCfg->bitRateControl.name, "VBR") == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].rcMode = ANJ_VIDEO_VBR;
            }
            else if (strcmp(pVideoEncCfg->bitRateControl.name, "AVBR") == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].rcMode = ANJ_VIDEO_AVBR;
            }
            else if (strcmp(pVideoEncCfg->bitRateControl.name, "FIXQP") == 0)
            {
                pstAnjVideoConfig->stVencCfg[i].rcMode = ANJ_VIDEO_FIXQP;
            }
            else
            {
                __ERR("invalid bitRateControl:%s\n", pVideoEncCfg->bitRateControl.name);
                return -1;
            }

            media_codec_type_e encodeType = pstAnjVideoConfig->stVencCfg[i].encodeType;
            AnjVideoRcMode_E rc_mode = pstAnjVideoConfig->stVencCfg[i].rcMode;
            if (pVideoEncCfg->qp.qp_enable && (pVideoEncCfg->qp.qp_min <= pVideoEncCfg->qp.qp_max))
            {
                pstAnjVideoConfig->stVencCfg[i].qpenable = pVideoEncCfg->qp.qp_enable;
                pstAnjVideoConfig->stVencCfg[i].minqp = pVideoEncCfg->qp.qp_min;
                pstAnjVideoConfig->stVencCfg[i].maxqp = pVideoEncCfg->qp.qp_max;
            }
            else
            {
                if (encodeType < MEDIA_CODEC_VIDEO_JPG && rc_mode < ANJ_VIDEO_FIXQP)
                {
                    pstAnjVideoConfig->stVencCfg[i].minqp = video_rc_list[encodeType][rc_mode].min_qp;
                    pstAnjVideoConfig->stVencCfg[i].maxqp = video_rc_list[encodeType][rc_mode].max_qp;
                }
            }

            if (encodeType == MEDIA_CODEC_VIDEO_H264 && rc_mode == ANJ_VIDEO_VBR)
            {
                // s32IPQPDelta减少，平均每一帧size
                // s32IPQPDelta增加，I帧质量增加，减少呼吸效应
                if (pVideoEncCfg->initQuant / pVideoEncCfg->frameRate <= 1)
                {
                    pstAnjVideoConfig->stVencCfg[i].qp_delta = 12;
                    pstAnjVideoConfig->stVencCfg[i].maxIsize = (pVideoEncCfg->bitRate << 10) * 0.9;
                    pstAnjVideoConfig->stVencCfg[i].maxPsize = (pVideoEncCfg->bitRate << 10) * 0.1;
                }
                else if (pVideoEncCfg->initQuant / pVideoEncCfg->frameRate <= 2)
                {
                    pstAnjVideoConfig->stVencCfg[i].qp_delta = 6;
                    pstAnjVideoConfig->stVencCfg[i].maxIsize = pVideoEncCfg->bitRate << 10;
                    pstAnjVideoConfig->stVencCfg[i].maxPsize = (pVideoEncCfg->bitRate << 10) * 0.2;
                }
                else
                {
                    pstAnjVideoConfig->stVencCfg[i].maxIsize = (pVideoEncCfg->bitRate << 10) * 1.2;
                    pstAnjVideoConfig->stVencCfg[i].maxPsize = (pVideoEncCfg->bitRate << 10) * 0.2;
                }
            }
            else if (encodeType == MEDIA_CODEC_VIDEO_H265 &&
                     (rc_mode == ANJ_VIDEO_VBR || rc_mode == ANJ_VIDEO_AVBR))
            {
                // s32IPQPDelta减少，平均每一帧size
                // s32IPQPDelta增加，I帧质量增加，减少呼吸效应
                if (pVideoEncCfg->initQuant / pVideoEncCfg->frameRate <= 1)
                {
                    pstAnjVideoConfig->stVencCfg[i].qp_delta = 12;
                    pstAnjVideoConfig->stVencCfg[i].maxIsize = (pVideoEncCfg->bitRate << 10) * 0.9 / 8;
                    pstAnjVideoConfig->stVencCfg[i].maxPsize = (pVideoEncCfg->bitRate << 10) * 0.1 / 8;
                }
                else if (pVideoEncCfg->initQuant / pVideoEncCfg->frameRate <= 2)
                {
                    pstAnjVideoConfig->stVencCfg[i].qp_delta = 6;
                    pstAnjVideoConfig->stVencCfg[i].maxIsize = (pVideoEncCfg->bitRate << 10) / 8;
                    pstAnjVideoConfig->stVencCfg[i].maxPsize = (pVideoEncCfg->bitRate << 10) * 0.2 / 8;
                }
                else
                {
                    pstAnjVideoConfig->stVencCfg[i].maxIsize = (pVideoEncCfg->bitRate << 10) * 1.2 / 8;
                    pstAnjVideoConfig->stVencCfg[i].maxPsize = (pVideoEncCfg->bitRate << 10) * 0.2 / 8;
                }
                if (pVideoEncCfg->bitRate < 1000)
                {
                    pstAnjVideoConfig->stVencCfg[i].minqp = 20;
                }
            }
            memset(&pstAnjVideoConfig->stVencCfg[i].stream_state, 0, sizeof(STREAM_STATE));

            vencChn++;
        }
    }

    return 0;
}

static FILE *pFile = NULL;
static FILE *pFile1 = NULL;
static int anj_video_test_thread(void *ctx, int *bStart)
{
    int vencchn = 0;
    media_frame_info_t stReadFrameInfo = {0};
    ANJ_MBUF_HANDLE *readerid = NULL;
    double multiple = 1.0;
    while (bStart && *bStart)
    {
        if (0 == access("/tmp/zoom+", F_OK))
        {
            remove("/tmp/zoom+");
            multiple += 0.5;
            if (DOUBLE_GREATER(multiple, 1.9))
            {
                multiple = 1.9;
            }
            anj_mw_media_video_scl_crop(0, multiple);
        }
        if (0 == access("/tmp/zoom-", F_OK))
        {
            remove("/tmp/zoom-");
            multiple -= 0.5;
            if (DOUBLE_LESS(multiple, 1.0))
            {
                multiple = 1.0;
            }

            anj_mw_media_video_scl_crop(0, multiple);
        }
        if (0 == access("/tmp/jpg", F_OK))
        {
            remove("/tmp/jpg");
            char filename[64] = {0};
            struct timeval tv;
            SystemGetTimeofRun(&tv, NULL);
            struct tm ptm;
            SystemLocalTime(&ptm);
            snprintf(filename, sizeof(filename), "snap_%04d%02d%02d_%02d%02d%02d_%03d.jpg",
                     ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,
                     ptm.tm_hour, ptm.tm_min, ptm.tm_sec,
                     (int)(tv.tv_usec / 1000));
            anj_snap_jpg(0, 1, 70, "/mnt/mmc0/", filename, NULL);
        }
        if (0 == access("/tmp/venc", F_OK))
        {
            FILE *fp = fopen("/tmp/venc", "r");
            if (fp)
            {
                char buf[8] = {0};
                fread(buf, sizeof(buf), 1, fp);
                fclose(fp);
                vencchn = atoi(buf);
            }
            if (pFile == NULL)
            {
                if (vencchn == 0)
                    pFile = fopen("/tmp/nfs/venc0.h265", "wb");
                else
                    pFile = fopen("/tmp/nfs/venc1.h265", "wb");
                pFile1 = fopen("/tmp/nfs/mbuf.pcm", "wb");
                readerid = anj_mbuf_create_reader(vencchn, 1);
                __INFO("anj_video_test_thread readerid:%p OK!\n", readerid);
            }
        }
        else
        {
            if (readerid)
            {
                anj_mbuf_destory_reader(readerid);
                readerid = NULL;
            }
            if (pFile)
            {
                fclose(pFile);
                pFile = NULL;
            }
            if (pFile1)
            {
                fclose(pFile1);
                pFile1 = NULL;
            }
        }

        if (pFile)
        {
            if (readerid)
            {
                if (0 < anj_mbuf_read_frame(readerid, 0, &stReadFrameInfo, 100))
                {
                    if (stReadFrameInfo.frameParam.frameType != MEDIA_AFRAME_A)
                    {
                        fwrite(stReadFrameInfo.frameBuf, stReadFrameInfo.frameParam.frameLen, 1, pFile);
                    }
                    else
                    {
                        if (pFile1)
                            fwrite(stReadFrameInfo.frameBuf, stReadFrameInfo.frameParam.frameLen, 1, pFile1);
                    }
                    anj_mbuf_read_release(readerid, &stReadFrameInfo);
                }
            }
        }

        usleep(20 * 1000);
    }

    if (readerid)
    {
        anj_mbuf_destory_reader(readerid);
        readerid = NULL;
    }
    if (pFile)
    {
        fclose(pFile);
        pFile = NULL;
    }
    return 0;
}
int anj_video_test(void)
{
    int iRet = 0;
    memset(&s_stVideoTesthread, 0, sizeof(anj_thread_s));

    s_stVideoTesthread.bAutoDestroy = 1;
    strncpy(s_stVideoTesthread.iThreadName, "anj_video_test_thread", sizeof(s_stVideoTesthread.iThreadName) - 1);
    s_stVideoTesthread.iThreadjob.ctx = &s_stVideoTesthread;
    s_stVideoTesthread.iThreadjob.func = anj_video_test_thread;
    iRet = anj_thread_task_create(&s_stVideoTesthread);

    return iRet;
}

static int anj_video_init(void)
{
    int iRet = 0;
    if (s_stVideoInit)
    {
        __ERR("had been init!\n");
        return 0;
    }
    anj_video_attr_init(gstAnjVideoCfg);
    if (!anj_video_restart_is_busy())
    {
        for (int iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
        {
            anj_mbuf_init(&gstAnjVideoCfg[iCameraIdex]);
        }
    }
    anj_video_test();

    iRet = anj_mw_media_video_init(gstAnjVideoCfg);
    if (iRet != 0)
    {
        return iRet;
    }

    anj_video_qos_start();

    s_stVideoInit = 1;
    return iRet;
}

static int anj_video_uninit(void)
{
    if (s_stVideoInit == 0)
    {
        __ERR("not init!\n");
        return 0;
    }
    int iCameraIndex = 0;
    anj_ser_stream_release();
    anj_video_qos_stop();
    anj_thread_task_destroy(&s_stVideoTesthread, -1);

    s_stVideoInit = 0;
    int iRet = anj_mw_media_video_uninit();

    if (!anj_video_restart_is_busy())
    {
        for (iCameraIndex = 0; iCameraIndex < ANJ_CAMERA_MAX_NUMS; iCameraIndex++)
        {
            iRet |= anj_mbuf_uninit(&gstAnjVideoCfg[iCameraIndex]);
        }
    }

    return iRet;
}

void anj_video_request_idr(int Chn, int VencId)
{
    if (s_stVideoInit == 0 || anj_video_restart_is_busy())
    {
        return;
    }
    anj_mw_media_video_requeset_idr(Chn, VencId);
}

int anj_video_set_config(void *data)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stVideoInit), -1, "not init");
    ANJ_CHK(data != NULL, -1, "input null");
    AnjVencConfig *pVEncCfg = (AnjVencConfig *)data;
    int i = 0;
    int j = 0;
    anj_mutex_lock(&s_gVideoCfgMutex);

    AnjVideoConfig stTmpstVideoConfig[ANJ_CAMERA_MAX_NUMS] = {0};
    memcpy(stTmpstVideoConfig, gstAnjVideoCfg, sizeof(gstAnjVideoCfg));

    for (i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        for (j = 0; j < MAX_VENC_CHN; j++)
        {
            int fpsChanged = (stTmpstVideoConfig[i].stVencCfg[j].fps != pVEncCfg[j].fps);
            int rcModeChanged = (stTmpstVideoConfig[i].stVencCfg[j].rcMode != pVEncCfg[j].rcMode);
            int vencAttrChanged = (stTmpstVideoConfig[i].stVencCfg[j].profile != pVEncCfg[j].profile ||
                                   stTmpstVideoConfig[i].stVencCfg[j].rcMode != pVEncCfg[j].rcMode ||
                                   stTmpstVideoConfig[i].stVencCfg[j].bitrate != pVEncCfg[j].bitrate ||
                                   stTmpstVideoConfig[i].stVencCfg[j].gop != pVEncCfg[j].gop ||
                                   stTmpstVideoConfig[i].stVencCfg[j].qpenable != pVEncCfg[j].qpenable ||
                                   stTmpstVideoConfig[i].stVencCfg[j].maxqp != pVEncCfg[j].maxqp ||
                                   stTmpstVideoConfig[i].stVencCfg[j].minqp != pVEncCfg[j].minqp);
            if (fpsChanged || vencAttrChanged)
            {
                // 分别设置单个通道的动态编码参数，避免纯参数修改触发整路重启
                stTmpstVideoConfig[i].stVencCfg[j].fps = pVEncCfg[j].fps;
                stTmpstVideoConfig[i].stVencCfg[j].profile = pVEncCfg[j].profile;
                stTmpstVideoConfig[i].stVencCfg[j].rcMode = pVEncCfg[j].rcMode;
                stTmpstVideoConfig[i].stVencCfg[j].bitrate = pVEncCfg[j].bitrate;
                stTmpstVideoConfig[i].stVencCfg[j].gop = pVEncCfg[j].gop;
                stTmpstVideoConfig[i].stVencCfg[j].qpenable = pVEncCfg[j].qpenable;
                stTmpstVideoConfig[i].stVencCfg[j].maxqp = pVEncCfg[j].maxqp;
                stTmpstVideoConfig[i].stVencCfg[j].minqp = pVEncCfg[j].minqp;

                if (vencAttrChanged)
                {
                    AnjVencConfig *stTmpVenCfg = &stTmpstVideoConfig[i].stVencCfg[j];
                    iRet |= anj_mw_media_video_config_set(stTmpVenCfg, i, j);
                }

                if (fpsChanged)
                {
                    iRet |= anj_mw_media_video_fps_set(i, j, pVEncCfg[j].fps);
                }

                if (rcModeChanged)
                {
                    __INFO("camera:%d stream:%d rc mode changed to:%d\n", i, j, pVEncCfg[j].rcMode);
                }
            }
        }
    }

    if (0 == iRet)
    {
        memcpy(gstAnjVideoCfg, stTmpstVideoConfig, sizeof(gstAnjVideoCfg));
    }

    anj_mutex_unlock(&s_gVideoCfgMutex);
endFunc:
    return iRet;
}

int anj_video_restart_is_busy(void)
{
    int busy = 0;

    anj_mutex_lock(&s_gVideoRestartMutex);
    busy = s_iVideoRestartBusy;
    anj_mutex_unlock(&s_gVideoRestartMutex);

    return busy;
}

int anj_video_encode_switch(void)
{
    int iRet = 0;
    if (s_stVideoInit == 0)
        return iRet;

    static anj_thread_s stVideoEncodeSwitchThread = {0};

    __INFO("video encode switch start\n");
    anj_mutex_lock(&s_gVideoRestartMutex);
    if (s_iVideoRestartBusy != 0)
    {
        anj_mutex_unlock(&s_gVideoRestartMutex);
        __ERR("video encode switch is running, ignore duplicated request\n");
        return -1;
    }
    s_iVideoRestartBusy = 1;
    anj_mutex_unlock(&s_gVideoRestartMutex);

    stVideoEncodeSwitchThread.bAutoDestroy = 1;
    strncpy(stVideoEncodeSwitchThread.iThreadName, "video_enc_switch",
            sizeof(stVideoEncodeSwitchThread.iThreadName) - 1);
    stVideoEncodeSwitchThread.iThreadjob.ctx = &stVideoEncodeSwitchThread;
    stVideoEncodeSwitchThread.iThreadjob.func = anj_video_encode_switch_thread;

    iRet = anj_thread_task_create(&stVideoEncodeSwitchThread);
    if (iRet != 0)
    {
        anj_mutex_lock(&s_gVideoRestartMutex);
        s_iVideoRestartBusy = 0;
        anj_mutex_unlock(&s_gVideoRestartMutex);
        __ERR("create video encode switch thread failed:%d\n", iRet);
    }
    __INFO("video encode switch end\n");
    return iRet;
}

int anj_video_scl_set(int enable)
{
    int iRet = 0;
    if (s_stVideoInit == 0)
        return iRet;
    __INFO("video set scl enable:%d\n", enable);
    if (0 == enable)
    {
        iRet = anj_mw_media_video_scl_pause();
    }
    else
    {
        iRet = anj_mw_media_video_scl_recover();
    }
    return iRet;
}

int anj_video_adjust_gop(int iCameraIdex, int fps)
{
    int iRet = 0;
    if (s_stVideoInit == 0)
        return iRet;
    MediaConfig *pstMediaCfg = getMediaConfig();
    VideoConfig *pstVideoConfig = &pstMediaCfg->videoConfig[iCameraIdex];
    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        VideoEncodeCfg *pVideoEncCfg = &pstVideoConfig->videoEncode.encodeCfg[i];
        if (pVideoEncCfg->enable == 0)
        {
            continue;
        }
        int nSetGopValue;
        if (pVideoEncCfg->frameRate <= 0)
            pVideoEncCfg->frameRate = 1;
        if (pVideoEncCfg->initQuant > pVideoEncCfg->frameRate)
        {
            int multiple;
            multiple = pVideoEncCfg->initQuant / pVideoEncCfg->frameRate;
            nSetGopValue = multiple * fps;
        }
        else
        {
            nSetGopValue = pVideoEncCfg->initQuant;
        }
        iRet = anj_mw_media_video_gop_set(iCameraIdex, i, nSetGopValue);
    }
    return iRet;
}

int anj_video_adjust_bitrate(int iCameraIdex, int fps)
{
    int iRet = 0;
    if (s_stVideoInit == 0)
        return iRet;
    MediaConfig *pstMediaCfg = getMediaConfig();
    VideoConfig *pstVideoConfig = &pstMediaCfg->videoConfig[iCameraIdex];
    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        VideoEncodeCfg *pVideoEncCfg = &pstVideoConfig->videoEncode.encodeCfg[i];
        if (pVideoEncCfg->enable == 0)
        {
            return iRet;
        }
        if (3 == fps)
        {
            __INFO("set bitrate:%u\n", 256);
            iRet = anj_mw_media_video_bitrate_set(iCameraIdex, 0, 256);
        }
        else if (15 == fps)
        {
            __INFO("set bitrate:%u\n", 800);
            iRet = anj_mw_media_video_bitrate_set(iCameraIdex, 0, 800);
        }
    }

    return iRet;
}

int anj_video_set_bitrate(int VencChn, int iBitrate)
{
    int iRet = 0;
    if (s_stVideoInit == 0)
        return iRet;
    int iCameraIdex = 0;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        iRet = anj_mw_media_video_bitrate_set(iCameraIdex, VencChn, iBitrate);
    }

    return iRet;
}

int anj_video_set_gop(int VencChn, int gop)
{
    int iRet = 0;
    if (s_stVideoInit == 0)
        return iRet;
    int iCameraIdex = 0;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        iRet = anj_mw_media_video_gop_set(iCameraIdex, VencChn, gop);
    }

    return iRet;
}

int anj_video_set_fps(int VencChn, int fps)
{
    if (s_stVideoInit == 0)
        return 0;
    anj_mutex_lock(&s_gVideoCfgMutex);
    int iRet = 0;
    int iCameraIdex = 0;
    for (iCameraIdex = 0; iCameraIdex < ANJ_CAMERA_MAX_NUMS; iCameraIdex++)
    {
        iRet = anj_mw_media_video_fps_set(iCameraIdex, VencChn, fps);
        if (iRet == 0)
        {
            gstAnjVideoCfg[iCameraIdex].stVencCfg[VencChn].fps = fps;
        }
    }
    anj_mutex_unlock(&s_gVideoCfgMutex);

    return iRet;
}

int anj_video_bitrate_get(int iCameraIdex, int VencChn)
{
    if (s_stVideoInit == 0)
        return 0;
    return gstAnjVideoCfg[iCameraIdex].stVencCfg[VencChn].stream_state.bitrate;
}

int anj_video_fps_get(int iCameraIdex, int VencChn)
{
    if (s_stVideoInit == 0)
        return 0;
    return gstAnjVideoCfg[iCameraIdex].stVencCfg[VencChn].stream_state.frame_rate;
}

void anj_video_snap_yuv_size_get(int iCameraIdex, int *width, int *height)
{
    if (width == NULL || height == NULL)
    {
        return;
    }
    if (iCameraIdex < 0 || iCameraIdex >= ANJ_CAMERA_MAX_NUMS)
    {
        *width = DEFAULT_SMART_WIDTH;
        *height = DEFAULT_SMART_HEIGHT;
        return;
    }
    if (MAX_SCL_PORT >= 3 && gstAnjVideoCfg[iCameraIdex].stVencCfg[1].width > 0)
    {
        *width = gstAnjVideoCfg[iCameraIdex].stVencCfg[1].width;
        *height = gstAnjVideoCfg[iCameraIdex].stVencCfg[1].height;
    }
    else
    {
        *width = DEFAULT_SMART_WIDTH;
        *height = DEFAULT_SMART_HEIGHT;
    }
}

REGISTER_MODULE(anj_video, MODULE_PRIORITY_VIDEO);
