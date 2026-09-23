#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_service.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_systime.h"
#include "anj_ser_api.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "dev_bind.h"
#include "eventhub.h"
#include "function_list.h"
#include "anj_ser_provider.h"
#include "cct_api.h"
#include "cct_common.h"
#include "cct_dd_api.h"
#include "cct_callback.h"

typedef struct
{
    char szBindUser[256];
    char enable;
} anj_cloud_ct_info;

static anj_cloud_ct_info s_stCloudInfo = {0};

static int anj_cloud_ct_feature_init(void);
static int anj_cloud_ct_feature_uninit(void);
static void anj_cloud_ct_binduser_check(char *pstBindUser);
static void anj_cloud_ct_push_alarm(int channel, int eventype, int buploadcloud);
static void anj_cloud_ct_push_video(int camera_type, int streamtype, int iskey,
                                    unsigned char *frameBuf, unsigned int frameLen);
static void anj_cloud_ct_push_audio(int camera_type, unsigned char *frameBuf, unsigned int frameLen);
static void anj_cloud_ct_check_support(int status);

static const anj_cloud_feature_ops s_stCtCloudOps = {
    .init = anj_cloud_ct_feature_init,
    .uninit = anj_cloud_ct_feature_uninit,
    .check_support = anj_cloud_ct_check_support,
    .binduser_check = anj_cloud_ct_binduser_check,
    .push_alarm = anj_cloud_ct_push_alarm,
    .push_video = anj_cloud_ct_push_video,
    .push_audio = anj_cloud_ct_push_audio,
};

ANJ_LINK_KEEP(anj_keep_cloud_ct_provider);

__attribute__((constructor)) static void anj_cloud_ct_provider_register(void)
{
    anj_cloud_feature_register(&s_stCtCloudOps);
}

__attribute__((destructor)) static void anj_cloud_ct_provider_unregister(void)
{
    anj_cloud_feature_unregister(&s_stCtCloudOps);
}

static CCT_VOID CLNK_StreamInfo_Callback(const CCT_UINT32 nChannelNo, const CCT_UINT32 streamtype, cct_stream_data_format *pstream_info)
{
    MediaConfig *pstMediaCfg = getMediaConfig();
    AudioConfig *pAudioCfg = &pstMediaCfg->audioConfig;
    VideoCaptureCfg *pVideoCaptureCfg = &pstMediaCfg->videoConfig[nChannelNo].videoCapture;
    VideoEncodeCfg *pstVideoEnc = &pstMediaCfg->videoConfig[nChannelNo].videoEncode.encodeCfg[streamtype];
    ANJ_SIZE_S resolution =
        getPicSize(pstVideoEnc->resolution.name, pVideoCaptureCfg->tvsystem, pVideoCaptureCfg->rotate, 0);
    cct_video_data_format *videoFormat = &pstream_info->videoFormat;
    cct_audio_data_format *audioFormat = &pstream_info->audioFormat;

    videoFormat->bitrate = 0;
    videoFormat->frameInterval = pstVideoEnc->initQuant;
    videoFormat->framerate = pstVideoEnc->frameRate;
    videoFormat->height = resolution.u32Height;
    videoFormat->width = resolution.u32Width;
    videoFormat->reserve = 0;

    media_codec_type_e video_type = video_encode_type_get(pstVideoEnc->encodeFormat.name);
    if (video_type == MEDIA_CODEC_VIDEO_H265 || video_type == MEDIA_CODEC_VIDEO_H265_PLUS)
        videoFormat->euCCT_VIDEO_CODEC_TYPE = CCT_VIDEO_CODEC_TYPE_H265;
    else if (video_type == MEDIA_CODEC_VIDEO_H264)
        videoFormat->euCCT_VIDEO_CODEC_TYPE = CCT_VIDEO_CODEC_TYPE_H264;
    else if (video_type == MEDIA_CODEC_VIDEO_MJPG)
        videoFormat->euCCT_VIDEO_CODEC_TYPE = CCT_VIDEO_CODEC_TYPE_MJPEG;
    else
        return;

    audioFormat->bitrate = pAudioCfg->audioEncode.bitRate / 1000;
    audioFormat->bitsPerSample = pAudioCfg->audioCapture.bitspersample;
    audioFormat->channelNumber = pAudioCfg->audioCapture.channels;
    audioFormat->samplesRate = pAudioCfg->audioCapture.samplerate;
    audioFormat->reserve = 0;

    media_codec_type_e audio_type = audio_encode_type_get(pAudioCfg->audioEncode.audioEncodeType.typeName);
    if (audio_type == MEDIA_CODEC_AUDIO_G711U)
        audioFormat->euCCT_AUDIO_CODEC_TYPE = CCT_AUDIO_CODEC_TYPE_G711U;
    else if (audio_type == MEDIA_CODEC_AUDIO_G711A)
        audioFormat->euCCT_AUDIO_CODEC_TYPE = CCT_AUDIO_CODEC_TYPE_G711A;
    else if (audio_type == MEDIA_CODEC_AUDIO_AAC)
        audioFormat->euCCT_AUDIO_CODEC_TYPE = CCT_AUDIO_CODEC_TYPE_AAC;
    else if (audio_type == MEDIA_CODEC_AUDIO_MP3)
        audioFormat->euCCT_AUDIO_CODEC_TYPE = CCT_AUDIO_CODEC_TYPE_MP3;
    else
        audioFormat->euCCT_AUDIO_CODEC_TYPE = CCT_AUDIO_CODEC_TYPE_PCM;
}

static CCT_VOID CLNK_Screenshots_Callback(const CCT_UINT32 nChannelNo, const CCT_UINT32 nAlarmType, CCT_CHAR **ppData, CCT_UINT32 *pnLen)
{
    int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (nChannelNo / ANJ_CAMERA_MAX_NUMS);
    char filename[128] = {0};
    struct timeval tv;
    SystemGetTimeofRun(&tv, NULL);
    struct tm ptm;
    SystemLocalTime(&ptm);
    snprintf(filename, sizeof(filename), "snap_ch%d_%04d%02d%02d_%02d%02d%02d_%03d.jpg", nChannelNo, 
        ptm.tm_year + 1900,
        ptm.tm_mon + 1,
        ptm.tm_mday,
        ptm.tm_hour, ptm.tm_min, ptm.tm_sec, (int)(tv.tv_usec / 1000));
    anj_snap_jpg(iCameraIdex, 1, 50, "/tmp", filename, NULL);

    char pathname[256] = {0};
    snprintf(pathname, sizeof(pathname), "/tmp/%s", filename);
    if (0 == anj_snap_wait_complete(pathname, 500))
    {
        unsigned long long len = 0;
        anj_mw_read_file_len(pathname, &len);
        *ppData = malloc(len);
        anj_mw_read_file_limit_len(pathname, *ppData, len);
        *pnLen = (CCT_UINT32)len;
        remove(pathname);
    }
    else
    {
        *ppData = NULL;
        *pnLen = 0;
    }
    (void)nAlarmType;
}

static CCT_VOID CLNK_CallForIFrame_CallBack(const CCT_UINT32 nChannelNo, const CCT_UINT32 streamtype)
{
    anj_video_request_idr(nChannelNo, streamtype);
}

static CCT_VOID CLNK_SvrTs_CallBack(const CCT_UINT64 nSvrTs)
{
    struct tm time;
    SystemConfig *pSystemConfig = (SystemConfig *)getSystemConfig();
    TimeConfig *pTimeConfig = &pSystemConfig->timeCfg;
    localtime_r((time_t *)&nSvrTs, &time);
    anj_systime_set_only(time, pTimeConfig->timeZone, 1);
}

static CCT_VOID CLNK_Cloud_Packet_Callback(const CCT_INT32 nCloudP)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    pstSerInfo->stP2pLoginState.cloudstorage = nCloudP;
}

static void cct_demo_callback_init(void)
{
    cct_cb_reg_streaminfo(CLNK_StreamInfo_Callback);
    cct_cb_reg_screenshots(CLNK_Screenshots_Callback);
    cct_cb_reg_iframe(CLNK_CallForIFrame_CallBack);
    cct_cb_reg_svr_ts(CLNK_SvrTs_CallBack);
    cct_cb_reg_cloud_p(CLNK_Cloud_Packet_Callback);
}

static int anj_cloud_ct_feature_init(void)
{
    int iRet = 0;
    anj_ser_info *pstSerInfo = getSerInfo();
    DevInfo *pstDevInfo = getDevInfo();
    cct_param cctparam;
    char szRealVersion[64] = {0};

    anj_sysctl_capability_add(FUNCTION_CLOUD_CTYUN);
    cct_api_log_savelocal(CCT_FALSE);
    cct_api_log_savepath(LOCAL_PATH_LOG);
    cct_demo_callback_init();
    cct_common_time_syn_type(CCT_TIME_SYN_TYPE_SYN_ZONE_RIGHT);

    sprintf(szRealVersion, "%s_V%s", pstDevInfo->devType, pstDevInfo->productVersion);
    memset(&cctparam, 0, sizeof(cctparam));
    strcpy(cctparam.szGid, pstSerInfo->uid);
    sprintf(cctparam.szModel, szRealVersion);
    strcpy(cctparam.szVersion, pstDevInfo->version_name);
    cctparam.bEnableCloud = 1;
    cctparam.nChannelCount = 1;
    strcpy(cctparam.szLocalCfgFilePath, LOCAL_CFG_PATH);
    strcpy(cctparam.szSdcardAbsPath, SDCARD_PATH);
    iRet = cct_api_init(cctparam);
    return iRet;
}

static int anj_cloud_ct_feature_uninit(void)
{
    cct_api_release();
    return 0;
}

static void anj_cloud_ct_binduser_check(char *pstBindUser)
{
    if (strcmp(pstBindUser, s_stCloudInfo.szBindUser) != 0)
    {
        snprintf(s_stCloudInfo.szBindUser, sizeof(s_stCloudInfo.szBindUser), "%s", pstBindUser);
        cct_api_set_binduser(pstBindUser);
    }
}

static void anj_cloud_ct_push_alarm(int channel, int eventype, int buploadcloud)
{
    if (s_stCloudInfo.enable)
        cct_svr_api_push_alarm(channel, (CCT_ALARM_TYPE)eventype, buploadcloud, NULL, 0);
}

static void anj_cloud_ct_push_video(int camera_type, int streamtype, int iskey,
                                    unsigned char *frameBuf, unsigned int frameLen)
{
    if (s_stCloudInfo.enable)
        cct_api_stream_push_video_stream(camera_type, streamtype, iskey, frameBuf, frameLen);
}

static void anj_cloud_ct_push_audio(int camera_type, unsigned char *frameBuf, unsigned int frameLen)
{
    if (s_stCloudInfo.enable)
        cct_api_stream_push_audio_stream(camera_type, frameBuf, frameLen);
}

static void anj_cloud_ct_check_support(int status)
{
    s_stCloudInfo.enable = status;
    if (status == 0)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
        if (!event_result.result)
            return;

        G4InfoStruct get4gNetworkInfo = *(G4InfoStruct *)event_result.result;
        char szIccid[G4_STR_LEN_256] = {0};
        if (IPC_4G_SIMCARD_NUM > 1)
            strcpy(szIccid, get4gNetworkInfo.IccidList);
        else
            strcpy(szIccid, get4gNetworkInfo.ICCID);

        s_stCloudInfo.enable = dev_bind_check_iccid_cs(szIccid);
    }
}
