#include "anj_mw_comm.h"
#include "anj_record.h"
#include "anj_ser_api.h"
#include "anj_aiot.h"
#include "anj_aiot_stream.h"
#include "anj_aiot_report.h"

#include "gct_apiv4.h"
#include "gct_sdcard_playback_callback.h"
#include "gct_sdcard_playback_apiv4.h"

#define LIVE_STAT_MAX       16      /* 同时统计的预览会话上限 */
#define LIVE_SLOT_NONE      (-1)    /* 表中无空位 */

typedef struct
{
    int bUsed;
    GCT_UINT32 sid;
    unsigned long long start_ms;    /* 预览开始：上电单调时间(ms)，避免校时导致 duration 超大 */
} AiotLiveStat_t;

static REC_HANDLE stream_pb[ANJ_CAMERA_MAX_NUMS][PB_STREAM_MAX];
static unsigned long long s_pb_start_ms[ANJ_CAMERA_MAX_NUMS][PB_STREAM_MAX]; /* 回放开始：上电单调时间(ms)，与 stream_pb 下标对齐 */
static AiotLiveStat_t s_live_stat[LIVE_STAT_MAX];
static pthread_mutex_t s_live_stat_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 上电单调时间相减；start 无效或取时失败返回 0，避免无符号下溢出大值 */
static unsigned long long anj_aiot_stat_elapsed_ms(unsigned long long start_ms)
{
    unsigned long long now_ms = anj_mw_get_cputime_ms(NULL);
    if (start_ms == 0 || now_ms < start_ms)
        return 0;
    return now_ms - start_ms;
}

static int anj_aiot_stream_pb_video(GCT_UINT32 userdata, unsigned long long tStartTime, media_frame_info_t *pFrameInfo)
{
    struct tm l_time;
    localtime_r(&pFrameInfo->frameParam.frameTime, &l_time);

    gct_sdcard_playback_param sdcard_playback_param;
    memset(&sdcard_playback_param, 0, sizeof(sdcard_playback_param));
    sdcard_playback_param.nIsVideo = 1;
    sdcard_playback_param.nSessionId = userdata;
    sdcard_playback_param.nIsIFrame = (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I) ? 1 : 0;
    sdcard_playback_param.nDataLen = pFrameInfo->frameParam.frameLen;
    sdcard_playback_param.pData = (char *)pFrameInfo->frameBuf;
    sdcard_playback_param.nYear = l_time.tm_year + 1900;
    sdcard_playback_param.nMonth = l_time.tm_mon + 1;
    sdcard_playback_param.nDay = l_time.tm_mday;
    sdcard_playback_param.nHour = l_time.tm_hour;
    sdcard_playback_param.nMin = l_time.tm_min;
    sdcard_playback_param.nSec = l_time.tm_sec;
    sdcard_playback_param.nMs = pFrameInfo->frameParam.frameTimeMs - (pFrameInfo->frameParam.frameTime * 1000);

    int nRetryTimes = 0;
    while (nRetryTimes < 10)
    {
        nRetryTimes++;
        if (0 == gct_sdcard_playback_apiv4_push_avstream(sdcard_playback_param))
        {
            break;
        }
        usleep(500 * 1000);
    }

#if 0
	if(sdcard_playback_param.nIsIFrame)
	{
    	printf("VIDDO iskey %d: %04d-%02d-%02d %02d:%02d:%02d %03d, length=%d", 
    			sdcard_playback_param.nIsIFrame,
    			  sdcard_playback_param.nYear,
    			  sdcard_playback_param.nMonth,
    			  sdcard_playback_param.nDay,
    			  sdcard_playback_param.nHour,
    			  sdcard_playback_param.nMin,
    			  sdcard_playback_param.nSec,
    			  sdcard_playback_param.nMs,
    			  sdcard_playback_param.nDataLen);
	}
#endif
    return 0;
}

static int anj_aiot_stream_pb_audio(GCT_UINT32 userdata, unsigned long long tStartTime, media_frame_info_t *pFrameInfo)
{
    struct tm l_time;
    localtime_r(&pFrameInfo->frameParam.frameTime, &l_time);

    gct_sdcard_playback_param sdcard_playback_param;
    memset(&sdcard_playback_param, 0, sizeof(sdcard_playback_param));
    sdcard_playback_param.nIsVideo = 0;
    sdcard_playback_param.nSessionId = userdata;
    sdcard_playback_param.nIsIFrame = 0;
    sdcard_playback_param.nDataLen = pFrameInfo->frameParam.frameLen;
    sdcard_playback_param.pData = (char *)pFrameInfo->frameBuf;
    sdcard_playback_param.nYear = l_time.tm_year + 1900;
    sdcard_playback_param.nMonth = l_time.tm_mon + 1;
    sdcard_playback_param.nDay = l_time.tm_mday;
    sdcard_playback_param.nHour = l_time.tm_hour;
    sdcard_playback_param.nMin = l_time.tm_min;
    sdcard_playback_param.nSec = l_time.tm_sec;
    sdcard_playback_param.nMs = pFrameInfo->frameParam.frameTimeMs - (pFrameInfo->frameParam.frameTime * 1000);

    int nRetryTimes = 0;
    while (nRetryTimes < 10)
    {
        nRetryTimes++;
        if (0 == gct_sdcard_playback_apiv4_push_avstream(sdcard_playback_param))
        {
            break;
        }
        usleep(500 * 1000);
    }

#if 0
    P2P_TRACE("audio: %04d-%02d-%02d %02d:%02d:%02d %03d, length=%d", 
              sdcard_playback_param.nYear,
              sdcard_playback_param.nMonth,
              sdcard_playback_param.nDay,
              sdcard_playback_param.nHour,
              sdcard_playback_param.nMin,
              sdcard_playback_param.nSec,
              sdcard_playback_param.nMs,
              sdcard_playback_param.nDataLen);
#endif
    return 0;
}

static int anj_aiot_stream_pb_cb(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e EventID)
{
    int iRet = -1;
    if (pHandle == NULL)
    {
        printf("input param invalid");
        return iRet;
    }
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        printf("Invalid poper %p\n", pHandle);
        return iRet;
    }
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;
    if (pPoper->iPopId < 0 || NULL == pFrameInfo)
    {
        printf("input param invalid");
        return iRet;
    }

    if (PB_CB_NONE == EventID)
    {
        gct_stream_data_format stream_data_format;
        gct_video_data_format *pstVideoFormat = &stream_data_format.videoFormat;
        gct_audio_data_format *pstAudioFormat = &stream_data_format.audioFormat;

        if (pPoper->tPbMediaParam.vcodecType == MEDIA_CODEC_VIDEO_H264)
        {
            pstVideoFormat->euGCT_VIDEO_CODEC_TYPE = GCT_VIDEO_CODEC_TYPE_H264;
        }
        else
        {
            pstVideoFormat->euGCT_VIDEO_CODEC_TYPE = GCT_VIDEO_CODEC_TYPE_H265;
        }
        pstVideoFormat->bitrate = pPoper->tPbMediaParam.bitrate;
        pstVideoFormat->frameInterval = pPoper->tPbMediaParam.gop;
        pstVideoFormat->framerate = pPoper->tPbMediaParam.framerate;
        pstVideoFormat->height = pPoper->tPbMediaParam.height;
        pstVideoFormat->reserve = 0;
        pstVideoFormat->width = pPoper->tPbMediaParam.width;

        if (pPoper->tPbMediaParam.sampleRate != 0)
        {
            pstAudioFormat->bitrate = 64000;
            if (pPoper->tPbMediaParam.acodecType == MEDIA_CODEC_AUDIO_G711A)
            {
                pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_G711A;
            }
            else if (pPoper->tPbMediaParam.acodecType == MEDIA_CODEC_AUDIO_G711U)
            {
                pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_G711U;
            }
            else if (pPoper->tPbMediaParam.acodecType == MEDIA_CODEC_AUDIO_AAC)
            {
                pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_AAC;
                pstAudioFormat->bitrate = 16000;
            }
            else
            {
                pstAudioFormat->euGCT_AUDIO_CODEC_TYPE = GCT_AUDIO_CODEC_TYPE_PCM;
            }
            pstAudioFormat->bitsPerSample = pPoper->tPbMediaParam.bitWidth;
            pstAudioFormat->channelNumber = pPoper->tPbMediaParam.channels;
            pstAudioFormat->reserve = 0;
            pstAudioFormat->samplesRate = pPoper->tPbMediaParam.sampleRate;
        }

        gct_sdcard_playback_apiv4_clearbuffer(pPoper->iPopId);
        gct_sdcard_playback_apiv4_avformat_change(pPoper->iPopId, stream_data_format);

        __INFO("video params codec:%#x %uX%u bps:%u fps:%d frameInterval:%d. speed:%d\n",
               pstVideoFormat->euGCT_VIDEO_CODEC_TYPE,
               pstVideoFormat->width,
               pstVideoFormat->height,
               pstVideoFormat->bitrate,
               pstVideoFormat->framerate,
               pstVideoFormat->frameInterval,
               pPoper->iSpeed);
        __INFO("audio params codec:%#x bps:%u samplesRate:%d channelNumber:%u. bitsPerSample:%u\n",
               pstAudioFormat->euGCT_AUDIO_CODEC_TYPE,
               pstAudioFormat->bitrate,
               pstAudioFormat->samplesRate,
               pstAudioFormat->channelNumber,
               pstAudioFormat->bitsPerSample);
    }
    else if (PB_CB_START == EventID)
    {
        if ((NULL == pFrameInfo) || (NULL == pFrameInfo->frameBuf) || (0 >= pFrameInfo->frameParam.frameLen) || (REC_MAX_FRAME_BUF_SIZE < pFrameInfo->frameParam.frameLen))
        {
            __ERR("Invalid Input Frame\n");
            return iRet;
        }

        int iSpeed = pPoper->iSpeed;
        if (iSpeed <= PB_SPEED_1)
        {
            iSpeed = PB_SPEED_1;
            if (pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A)
            {
                anj_aiot_stream_pb_video(pPoper->iPopId, pPoper->tDayStartTime, pFrameInfo);
            }
            else
            {
                anj_aiot_stream_pb_audio(pPoper->iPopId, pPoper->tDayStartTime, pFrameInfo);
            }
        }
        else
        {
            if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
            {
                anj_aiot_stream_pb_video(pPoper->iPopId, pPoper->tDayStartTime, pFrameInfo);
            }
            if (iSpeed < PB_SPEED_4)
            {
                if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_P)
                {
                    anj_aiot_stream_pb_video(pPoper->iPopId, pPoper->tDayStartTime, pFrameInfo);
                }
            }
        }

        if (pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A)
        {
            if ((pPoper->tLastPts > 0) && (pPoper->tSendTime > 0) && (pFrameInfo->frameParam.framePts > pPoper->tLastPts))
            {
                unsigned int iDiffTime = (pFrameInfo->frameParam.framePts - pPoper->tLastPts) / 90;
                unsigned long long iElapsedTime = anj_mw_get_cputime_ms(NULL) - pPoper->tSendTime;
                if (iElapsedTime >= iDiffTime)
                {
                    iDiffTime = 0;
                }
                else
                {
                    iDiffTime = iDiffTime - iElapsedTime;
                }
                if (iSpeed > PB_SPEED_0)
                {
                    iDiffTime = iDiffTime / iSpeed;
                    if (iSpeed > PB_SPEED_2)
                    {
                        iDiffTime = iDiffTime / (iSpeed / PB_SPEED_4);
                    }
                }
                if (iDiffTime > 1000)
                {
                    iDiffTime = 0;
                }
                if (iDiffTime == 0)
                {
                }
                else if (iDiffTime < 10)
                {
                    usleep(10 * 1000);
                }
                else
                {
                    usleep((iDiffTime - 1) * 1000);
                }
            }
            else
            {
                usleep(10 * 1000);
            }
            pPoper->tLastPts = pFrameInfo->frameParam.framePts;
            pPoper->tSendTime = anj_mw_get_cputime_ms(NULL);
        }
    }
    else if (PB_CB_FINISH == EventID)
    {
        iRet = 0;
        __INFO("pb cb End\n");
    }
    else
    {
        __INFO("pb cb error\n");
    }

    return iRet;
}
// nChannelNo 通道号，从0开始
// ppgct_sdcardplayback_callback_daylist_node_list 日期列表，上层malloc SDK释放
static GCT_VOID anj_aiot_stream_pb_daylist(const GCT_UINT32 nChannelNo, gct_sdcardplayback_callback_daylist_node **ppgct_sdcardplayback_callback_daylist_node_list)
{
    __INFO("CH=%u: ready to hd_get_rec_date_list\n", nChannelNo);
}

static GCT_VOID anj_aiot_stream_pb_search(const gct_sdplayback_callback_search_req_param search_req_param, gct_sdcardplayback_callback_search_rsp_param **ppsearch_param_list)
{
    __INFO("%d-%d-%d\n", search_req_param.nYear, search_req_param.nMonth, search_req_param.nDay);
}

static GCT_VOID anj_aiot_stream_pb_start(const gct_sdcardplayback_callback_startplay_param param)
{
    __INFO("Session:%u, ch:%u %d-%d-%d %d:%d:%d\n",
           param.nSessionId, param.nChannelNo,
           param.nYear, param.nMonth, param.nDay,
           param.nHour, param.nMin, param.nSec);

    if (param.nChannelNo >= ANJ_CAMERA_MAX_NUMS)
    {
        __ERR("Invalid nChannelNo:%d\n", param.nChannelNo);
        return;
    }

    struct tm tmSeektime;
    memset(&tmSeektime, 0, sizeof(struct tm));

    tmSeektime.tm_sec = param.nSec;
    tmSeektime.tm_min = param.nMin;
    tmSeektime.tm_hour = param.nHour;
    tmSeektime.tm_mday = param.nDay;
    tmSeektime.tm_mon = param.nMonth - 1;
    tmSeektime.tm_year = param.nYear - 1900;
    time_t nSeektime = mktime(&tmSeektime);

    int i = 0;
    for (i = 0; i < PB_STREAM_MAX; i++)
    {
        rec_pb_poper *pstPoper = (rec_pb_poper *)stream_pb[param.nChannelNo][i];
        if (pstPoper)
        {
            if (pstPoper->iPopId == param.nSessionId)
            {
                anj_record_pb_seek((REC_HANDLE)pstPoper, nSeektime);
            }
            else
            {
                __ERR("PB invalid nSessionId:%d iPopId:%d\n", param.nSessionId, pstPoper->iPopId);
            }
            break;
        }
        else
        {
            stream_pb[param.nChannelNo][i] = (void *)anj_record_pb_create(param.nChannelNo, nSeektime, 0, 0, param.nSessionId, anj_aiot_stream_pb_cb);
            if (stream_pb[param.nChannelNo][i] != NULL)
            {
                /* 第一次 create 才计回放次数，seek 已有会话不计 */
                s_pb_start_ms[param.nChannelNo][i] = anj_mw_get_cputime_ms(NULL);
                anj_aiot_stat_add(ANJ_AIOT_STAT_PLAYBACK_CNT, 1);
            }
            break;
        }
    }
    if (i >= PB_STREAM_MAX)
    {
        __ERR("PB FULL nSessionId:%d\n", param.nSessionId);
    }
}

static GCT_VOID anj_aiot_stream_pb_seek(const gct_sdcardplayback_callback_seekto_param param)
{
    __INFO("Session:%u, %d-%d-%d %d:%d:%d\n",
           param.nSessionId,
           param.nYear, param.nMonth, param.nDay,
           param.nHour, param.nMin, param.nSec);

    struct tm tmSeektime;
    memset(&tmSeektime, 0, sizeof(struct tm));

    tmSeektime.tm_sec = param.nSec;
    tmSeektime.tm_min = param.nMin;
    tmSeektime.tm_hour = param.nHour;
    tmSeektime.tm_mday = param.nDay;
    tmSeektime.tm_mon = param.nMonth - 1;
    tmSeektime.tm_year = param.nYear - 1900;
    time_t nSeektime = mktime(&tmSeektime);

    int iIndex = 0;
    int bFound = 0;
    for (iIndex = 0; iIndex < ANJ_CAMERA_MAX_NUMS; iIndex++)
    {
        int i = 0;
        for (i = 0; i < PB_STREAM_MAX; i++)
        {
            rec_pb_poper *pstPoper = (rec_pb_poper *)stream_pb[iIndex][i];
            if (pstPoper)
            {
                if (pstPoper->iPopId == param.nSessionId)
                {
                    bFound = 1;
                    anj_record_pb_seek((REC_HANDLE)pstPoper, nSeektime);
                    break;
                }
                else
                {
                    __ERR("PB invalid nSessionId:%d iPopId:%d\n", param.nSessionId, pstPoper->iPopId);
                }
            }
        }
        if (i >= PB_STREAM_MAX)
        {
            __ERR("PB FULL nSessionId:%d\n", param.nSessionId);
        }
    }
    if (!bFound)
    {
        __ERR("Cannot found playback session %u\n", param.nSessionId);
    }
}

static GCT_VOID anj_aiot_stream_pb_ctrl(const GCT_UINT32 nSessionId, const GCT_SDCARDPLAYBACK_CTRL_TYPE euGCT_SDCARDPLAYBACK_CTRL_TYPE, const GCT_INT32 nValue)
{
    __INFO("Session:%u, euGCT_SDCARDPLAYBACK_CTRL_TYPE = %d, VALUE=%d\n", nSessionId, euGCT_SDCARDPLAYBACK_CTRL_TYPE, nValue);
    int iIndex = 0;
    int bFound = 0;
    for (iIndex = 0; iIndex < ANJ_CAMERA_MAX_NUMS; iIndex++)
    {
        for (int i = 0; i < PB_STREAM_MAX; i++)
        {
            rec_pb_poper *pstPoper = (rec_pb_poper *)stream_pb[iIndex][i];
            if (pstPoper)
            {
                if (pstPoper->iPopId == nSessionId)
                {
                    bFound = 1;
                    switch (euGCT_SDCARDPLAYBACK_CTRL_TYPE)
                    {
                    case GCT_SDCARDPLAYBACK_CTRL_TYPE_PAUSE:
                        anj_record_pb_pause_set((REC_HANDLE)pstPoper, 1);
                        break;
                    case GCT_SDCARDPLAYBACK_CTRL_TYPE_RESUME:
                        anj_record_pb_pause_set((REC_HANDLE)pstPoper, 0);
                        break;
                    case GCT_SDCARDPLAYBACK_CTRL_TYPE_PLUS:
                        anj_record_pb_speed_set((REC_HANDLE)pstPoper, nValue);
                        break;
                    case GCT_SDCARDPLAYBACK_CTRL_TYPE_MINUS:
                        anj_record_pb_speed_set((REC_HANDLE)pstPoper, nValue);
                        break;
                    case GCT_SDCARDPLAYBACK_CTRL_TYPE_CLOSE:
                    {
                        unsigned long long duration_ms = anj_aiot_stat_elapsed_ms(s_pb_start_ms[iIndex][i]);
                        if (duration_ms != 0)
                            anj_aiot_stat_add(ANJ_AIOT_STAT_PLAYBACK_DURATION, duration_ms);
                        s_pb_start_ms[iIndex][i] = 0;
                        anj_record_pb_release((REC_HANDLE)pstPoper);
                        stream_pb[iIndex][i] = NULL;
                        break;
                    }
                    default:
                        return;
                    }
                    return;
                }
            }
        }
    }
    if (!bFound)
    {
        __ERR("Cannot found playback session %u\n", nSessionId);
    }
}

static GCT_VOID anj_aiot_stream_pb_info(const GCT_UINT32 nSessionId, const GCT_UINT32 nChannelNo, gct_stream_data_format *pstream_info)
{
}

GCT_VOID anj_aiot_stream_add(const GCT_UINT32 sid, const GCT_UINT32 nChannelNo, const GCT_UINT32 streamtype)
{
    int i = 0;
    int iEmpty = LIVE_SLOT_NONE;
    int bExist = 0;

    __INFO("login sid=%u ch=%u type=%u\n", sid, nChannelNo, streamtype);

    anj_mutex_lock(&s_live_stat_mutex);
    for (i = 0; i < LIVE_STAT_MAX; i++)
    {
        if (s_live_stat[i].bUsed && s_live_stat[i].sid == sid)
        {
            s_live_stat[i].start_ms = anj_mw_get_cputime_ms(NULL);
            bExist = 1;
            break;
        }
        if (iEmpty == LIVE_SLOT_NONE && s_live_stat[i].bUsed == 0)
            iEmpty = i;
    }
    if (!bExist)
    {
        if (iEmpty >= 0)
        {
            s_live_stat[iEmpty].bUsed = 1;
            s_live_stat[iEmpty].sid = sid;
            s_live_stat[iEmpty].start_ms = anj_mw_get_cputime_ms(NULL);
        }
        else
        {
            __WARN("preview stat table full, sid=%u\n", sid);
        }
    }
    anj_mutex_unlock(&s_live_stat_mutex);
    anj_aiot_stat_add(ANJ_AIOT_STAT_PREVIEW_CNT, 1);
}

GCT_VOID anj_aiot_stream_del(const GCT_UINT32 sid)
{
    int i = 0;
    unsigned long long start_ms = 0;
    unsigned long long duration_ms = 0;

    __INFO("logout sid=%u\n", sid);

    anj_mutex_lock(&s_live_stat_mutex);
    for (i = 0; i < LIVE_STAT_MAX; i++)
    {
        if (s_live_stat[i].bUsed && s_live_stat[i].sid == sid)
        {
            start_ms = s_live_stat[i].start_ms;
            s_live_stat[i].bUsed = 0;
            s_live_stat[i].sid = 0;
            s_live_stat[i].start_ms = 0;
            break;
        }
    }
    anj_mutex_unlock(&s_live_stat_mutex);

    duration_ms = anj_aiot_stat_elapsed_ms(start_ms);
    if (duration_ms != 0)
        anj_aiot_stat_add(ANJ_AIOT_STAT_PREVIEW_DURATION, duration_ms);

    anj_aiot_stream_pb_ctrl(sid, GCT_SDCARDPLAYBACK_CTRL_TYPE_CLOSE, 0);
}

GCT_VOID anj_aiot_stream_switch(const GCT_UINT32 sid, const GCT_UINT32 nFromChannelNo,
                                const GCT_UINT32 nFromStreamtype, const GCT_UINT32 nChannelNo, const GCT_UINT32 streamtype)
{
    __INFO("login sid=%u, %u:%u --> %u:%u\n", sid, nFromChannelNo, nFromStreamtype, nChannelNo, streamtype);
    //	*  streamtype :    为码流类型（主、次码流）,0->主,1->次 2= sd卡回放，3 = 透明通道 7= 长链接
}

GCT_VOID anj_aiot_stream_pb_init()
{
    gct_sdcard_playback_callback_reg_search(anj_aiot_stream_pb_search);
    gct_sdcard_playback_callback_reg_day_list(anj_aiot_stream_pb_daylist);
    gct_sdcard_playback_callback_reg_startplay(anj_aiot_stream_pb_start);
    gct_sdcard_playback_callback_reg_seekto(anj_aiot_stream_pb_seek);
    gct_sdcard_playback_callback_reg_ctrl(anj_aiot_stream_pb_ctrl);
    gct_sdcard_playback_callback_reg_streaminfo(anj_aiot_stream_pb_info);
}
