#include <string.h>
#include <unistd.h>

#include "eventhub.h"
#include "anj_service_provider.h"
#include "anj_mw_comm.h"
#include "anj_mw_net.h"
#include "anj_config.h"
#include "anj_net.h"
#include "anj_mbuf.h"
#include "anj_record.h"
#include "media_util.h"
#include "anj_sysctl.h"
#include "function_list.h"
#include "alarm_link.h"

#include "gb28181_proc.h"
#include "gb28181.h"
#include "gb28181/gb_pu_param.h"

#define GB28181_START_DELAY_SEC 10
#define GB28181_CHECK_IP_CHANGES (10)
#define RECORD_SEGMENT_DURATION (10 * 60)

typedef struct
{
    int bStart;        /* 当前运行状态 0关闭，1开启28181 */
    int bStartTmp;     /* 需要配置的开启状态 0关闭，1开启 */
    int bChangeUpdate; /* 是否需要更新（IP变化或配置变化触发） */
    int bUpdateInfo;   /* 是否更新信息 */
} GB28181Status;

static GB28181Config s_cfg;    /* GB28181配置 */
static GB28181Status s_status; /* GB28181状态 */
static anj_thread_s s_thread = {0};
static ANJ_MBUF_POPER s_poper = {0};
static REC_HANDLE s_stream_pb = NULL;

/*****************************************************************************
  函 数 名  : anj_gb28181_audio_enc_change
  功能描述  : 音频编码变更回调
  输入参数  : 无
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
static int anj_gb28181_audio_enc_change(void)
{
    Service_GB28181_AudioEncChange();
    return 0;
}

static const anj_service_provider_ops s_provider_ops = {
    .provider_name = "gb28181",
    .provider_type = ANJ_SERVICE_PROVIDER_GB28181,
    .provider_priority = ANJ_SERVICE_PROVIDER_GB28181,
    .capability_flags = ANJ_SERVICE_PROVIDER_CAP_GB28181,
    .init = gb28181_init,
    .uninit = gb28181_uninit,
    .alarm_event_notify = Service_GB28181_AlarmHandle,
    .audio_enc_change = anj_gb28181_audio_enc_change,
};

ANJ_LINK_KEEP(anj_keep_gb28181_provider);

__attribute__((constructor)) static void anj_gb28181_provider_register(void)
{
    anj_service_provider_register(&s_provider_ops);
}

__attribute__((destructor)) static void anj_gb28181_provider_unregister(void)
{
    anj_service_provider_unregister(&s_provider_ops);
}

/*****************************************************************************
  函 数 名  : gb28181PtzCtrl
  功能描述  : GB28181云台控制命令解析与转发
  输入参数  : cmdcode 云台命令字符串
              speed   云台速度
              param1  预置位编号等参数
  输出参数  : 无
  返 回 值  : 0成功，其他失败
*****************************************************************************/
static int gb28181PtzCtrl(char *cmdcode, int speed, int param1)
{
    EventResult event_result = {0};
    PtzCmdParse ptz = {0};

    if (cmdcode == NULL)
    {
        return -1;
    }

    if (strcmp(cmdcode, "STOP") == 0)
    {
        strncpy(ptz.ptzCmd, "stop", sizeof(ptz.ptzCmd) - 1);
    }
    else if (strcmp(cmdcode, "IC") == 0)
    {
        strncpy(ptz.ptzCmd, "IrisCloseAutoOff", sizeof(ptz.ptzCmd) - 1);
    }
    else if (strcmp(cmdcode, "IO") == 0)
    {
        strncpy(ptz.ptzCmd, "IrisOpenAutoOff", sizeof(ptz.ptzCmd) - 1);
    }
    else if (strcmp(cmdcode, "ZIN") == 0)
    {
        strncpy(ptz.ptzCmd, "zoomtele", sizeof(ptz.ptzCmd) - 1);
    }
    else if (strcmp(cmdcode, "ZOUT") == 0)
    {
        strncpy(ptz.ptzCmd, "zoomwide", sizeof(ptz.ptzCmd) - 1);
    }
    else if (strcmp(cmdcode, "FR") == 0)
    {
        strncpy(ptz.ptzCmd, "FocusFarAutoOff", sizeof(ptz.ptzCmd) - 1);
    }
    else if (strcmp(cmdcode, "FN") == 0)
    {
        strncpy(ptz.ptzCmd, "FocusNearAutoOff", sizeof(ptz.ptzCmd) - 1);
    }
    else if (strcmp(cmdcode, "TU") == 0)
    {
        strncpy(ptz.ptzCmd, "up", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "TD") == 0)
    {
        strncpy(ptz.ptzCmd, "down", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "PL") == 0)
    {
        strncpy(ptz.ptzCmd, "left", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "PR") == 0)
    {
        strncpy(ptz.ptzCmd, "right", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "TUPL") == 0)
    {
        strncpy(ptz.ptzCmd, "left_up", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "TDPL") == 0)
    {
        strncpy(ptz.ptzCmd, "left_down", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "TUPR") == 0)
    {
        strncpy(ptz.ptzCmd, "right_up", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "TDPR") == 0)
    {
        strncpy(ptz.ptzCmd, "right_down", sizeof(ptz.ptzCmd) - 1);
        ptz.panSpeed = speed;
        ptz.tiltSpeed = speed;
    }
    else if (strcmp(cmdcode, "SET_PRESET") == 0)
    {
        strncpy(ptz.ptzCmd, "setpreset", sizeof(ptz.ptzCmd) - 1);
        ptz.presetID = param1;
        ptz.flag = 1;
    }
    else if (strcmp(cmdcode, "DEL_PRESET") == 0)
    {
        strncpy(ptz.ptzCmd, "clearpreset", sizeof(ptz.ptzCmd) - 1);
        ptz.presetID = param1;
    }
    else if (strcmp(cmdcode, "GOTO_PRESET") == 0)
    {
        strncpy(ptz.ptzCmd, "callpreset", sizeof(ptz.ptzCmd) - 1);
        ptz.presetID = param1;
    }
    else if (strcmp(cmdcode, "SetWatchGuard") == 0)
    {
        strncpy(ptz.ptzCmd, "SetGuardPreset", sizeof(ptz.ptzCmd) - 1);
        ptz.presetID = param1;
    }
    else
    {
        return 0;
    }

    eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&ptz);
    return 0;
}

static int compare_record_file(const void *a, const void *b)
{
    const NET_SERVER_RECORD_FILE *file_a = (const NET_SERVER_RECORD_FILE *)a;
    const NET_SERVER_RECORD_FILE *file_b = (const NET_SERVER_RECORD_FILE *)b;
    
    if (file_a->start_time.year != file_b->start_time.year)
        return file_a->start_time.year - file_b->start_time.year;
    if (file_a->start_time.month != file_b->start_time.month)
        return file_a->start_time.month - file_b->start_time.month;
    if (file_a->start_time.date != file_b->start_time.date)
        return file_a->start_time.date - file_b->start_time.date;
    if (file_a->start_time.hour != file_b->start_time.hour)
        return file_a->start_time.hour - file_b->start_time.hour;
    if (file_a->start_time.minute != file_b->start_time.minute)
        return file_a->start_time.minute - file_b->start_time.minute;
    return file_a->start_time.second - file_b->start_time.second;
}

/*****************************************************************************
  函 数 名  : HistoryQuery
  功能描述  : 历史录像文件查询
  输入参数  : buf 查询条件缓冲区
  输出参数  : 无
  返 回 值  : 0成功，其他失败
*****************************************************************************/
static int HistoryQuery(char *buf)
{
    LPNET_SERVER_QUERY_STORE_FILE query = (LPNET_SERVER_QUERY_STORE_FILE)buf;
    rec_pb_date_s pb_date = {0};
    rec_pb_list_s pb_list = {0};
    rec_pb_segment_s *seg = NULL;
    struct tm start_tm = {0};
    struct tm end_tm = {0};
    time_t start_time;
    time_t end_time;
    int valid_count = 0;
    int index = 0;

    if (query == NULL)
    {
        return -1;
    }
    
    __INFO("HistoryQuery from_date=%d-%d-%d %d:%d:%d to_date=%d-%d-%d %d:%d:%d\n", 
           query->from_date.year, query->from_date.month, query->from_date.date, 
           query->from_date.hour, query->from_date.minute, query->from_date.second,
           query->to_date.year, query->to_date.month, query->to_date.date, 
           query->to_date.hour, query->to_date.minute, query->to_date.second);

    start_tm.tm_year = query->from_date.year - 1900;
    start_tm.tm_mon = query->from_date.month - 1;
    start_tm.tm_mday = query->from_date.date;
    start_tm.tm_hour = query->from_date.hour;
    start_tm.tm_min = query->from_date.minute;
    start_tm.tm_sec = query->from_date.second;
    end_tm.tm_year = query->to_date.year - 1900;
    end_tm.tm_mon = query->to_date.month - 1;
    end_tm.tm_mday = query->to_date.date;
    end_tm.tm_hour = query->to_date.hour;
    end_tm.tm_min = query->to_date.minute;
    end_tm.tm_sec = query->to_date.second;
    start_time = mktime(&start_tm);
    end_time = mktime(&end_tm);

    pb_date.tEvent = REC_EVENT_ALL;
    pb_date.year = query->from_date.year;
    pb_date.month = query->from_date.month;
    pb_date.day = query->from_date.date;

    if (anj_record_pb_query_day_create(0, &pb_date, &pb_list) != 0)
    {
        query->total_num = 0;
        return -1;
    }

    for (seg = pb_list.pstSegment; seg != NULL; seg = seg->ptNext)
    {
        if (seg->begin_time_s <= end_time && seg->end_time_s >= start_time)
        {
            time_t seg_start = seg->begin_time_s > start_time ? seg->begin_time_s : start_time;
            time_t seg_end = seg->end_time_s < end_time ? seg->end_time_s : end_time;
            time_t duration = seg_end - seg_start;
            valid_count += (duration + RECORD_SEGMENT_DURATION - 1) / RECORD_SEGMENT_DURATION;
        }
    }

    if (valid_count <= 0)
    {
        query->total_num = 0;
        anj_record_pb_query_day_release(0, &pb_list);
        return 0;
    }

    query->record_file_lists = (LPNET_SERVER_RECORD_FILE)anj_mw_malloc(valid_count * sizeof(NET_SERVER_RECORD_FILE));
    if (query->record_file_lists == NULL)
    {
        anj_record_pb_query_day_release(0, &pb_list);
        return -1;
    }
    memset(query->record_file_lists, 0, valid_count * sizeof(NET_SERVER_RECORD_FILE));
    query->total_num = valid_count;
    query->file_list_cnt = valid_count;
    query->leave_num = 0;

    for (seg = pb_list.pstSegment; seg != NULL; seg = seg->ptNext)
    {
        if (seg->begin_time_s <= end_time && seg->end_time_s >= start_time)
        {
            time_t seg_start = seg->begin_time_s > start_time ? seg->begin_time_s : start_time;
            time_t seg_end = seg->end_time_s < end_time ? seg->end_time_s : end_time;
            time_t current_start = seg_start;

            while (current_start < seg_end && index < valid_count)
            {
                time_t current_end = current_start + RECORD_SEGMENT_DURATION;
                struct tm tm_start = {0};
                struct tm tm_end = {0};
                LPNET_SERVER_RECORD_FILE item = &query->record_file_lists[index];

                if (current_end > seg_end)
                {
                    current_end = seg_end;
                }

                localtime_r(&current_start, &tm_start);
                localtime_r(&current_end, &tm_end);
                item->start_time.year = tm_start.tm_year + 1900;
                item->start_time.month = tm_start.tm_mon + 1;
                item->start_time.date = tm_start.tm_mday;
                item->start_time.hour = tm_start.tm_hour;
                item->start_time.minute = tm_start.tm_min;
                item->start_time.second = tm_start.tm_sec;
                item->stop_time.year = tm_end.tm_year + 1900;
                item->stop_time.month = tm_end.tm_mon + 1;
                item->stop_time.date = tm_end.tm_mday;
                item->stop_time.hour = tm_end.tm_hour;
                item->stop_time.minute = tm_end.tm_min;
                item->stop_time.second = tm_end.tm_sec;
                item->size = (current_end - current_start) * 1024;
                snprintf(item->file_name, sizeof(item->file_name),
                         "falloc_%04d%02d%02d_%02d%02d%02d_%04d%02d%02d_%02d%02d%02d",
                         tm_start.tm_year + 1900, tm_start.tm_mon + 1, tm_start.tm_mday,
                         tm_start.tm_hour, tm_start.tm_min, tm_start.tm_sec,
                         tm_end.tm_year + 1900, tm_end.tm_mon + 1, tm_end.tm_mday,
                         tm_end.tm_hour, tm_end.tm_min, tm_end.tm_sec);
                index++;
                current_start = current_end;
            }
        }
    }

    qsort(query->record_file_lists, valid_count, sizeof(NET_SERVER_RECORD_FILE), compare_record_file);

    anj_record_pb_query_day_release(0, &pb_list);
    return 0;
}

/*****************************************************************************
  函 数 名  : HistoryChannelPauseCapture
  功能描述  : 历史通道暂停/恢复捕获
  输入参数  : dwUserID 用户ID
              status   0恢复 1暂停
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
static int HistoryChannelPauseCapture(unsigned long dwUserID, int status)
{
    (void)dwUserID;
    if (s_stream_pb != NULL)
    {
        anj_record_pb_pause_set(s_stream_pb, status);
    }
    return 0;
}

/*****************************************************************************
  函 数 名  : HistoryChannelStartCapture
  功能描述  : 历史通道开始捕获（回放/下载）
  输入参数  : dwUserID 用户ID
              buf      定位时间信息
  输出参数  : 无
  返 回 值  : 0成功，其他失败
*****************************************************************************/
static int HistoryChannelStartCapture(unsigned long dwUserID, char *buf)
{
    HistoryChannelPositionInfo pos = {0};
    struct tm start_tm = {0};
    struct tm end_tm = {0};
    time_t start_time;
    time_t end_time;

    if (buf == NULL)
    {
        return -1;
    }

    memcpy(&pos, buf, sizeof(pos));
    start_tm.tm_year = pos.cSeekTime.wYear - 1900;
    start_tm.tm_mon = pos.cSeekTime.wMonth - 1;
    start_tm.tm_mday = pos.cSeekTime.wDay;
    start_tm.tm_hour = pos.cSeekTime.wHour;
    start_tm.tm_min = pos.cSeekTime.wMinute;
    start_tm.tm_sec = pos.cSeekTime.wSecond;
    start_time = mktime(&start_tm);
    end_tm.tm_year = pos.cEndTime.wYear - 1900;
    end_tm.tm_mon = pos.cEndTime.wMonth - 1;
    end_tm.tm_mday = pos.cEndTime.wDay;
    end_tm.tm_hour = pos.cEndTime.wHour;
    end_tm.tm_min = pos.cEndTime.wMinute;
    end_tm.tm_sec = pos.cEndTime.wSecond;
    end_time = mktime(&end_tm);

    if (s_stream_pb != NULL)
    {
        anj_record_pb_release(s_stream_pb);
        s_stream_pb = NULL;
    }
    
    s_stream_pb = anj_record_pb_create(0, start_time, end_time, 0, (int)dwUserID, Service_GB2818_PbSendVStream);
    if (s_stream_pb && pos.bDownload)
    {
        anj_record_pb_download_set(s_stream_pb, 1); // download mode enable
    }
    return (s_stream_pb == NULL) ? -1 : 0;
}

/*****************************************************************************
  函 数 名  : HistoryChannelStopCapture
  功能描述  : 历史通道停止捕获
  输入参数  : dwUserID 用户ID
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
static int HistoryChannelStopCapture(unsigned long dwUserID)
{
    (void)dwUserID;
    if (s_stream_pb != NULL)
    {
        anj_record_pb_release(s_stream_pb);
        anj_record_pb_download_set(s_stream_pb, 0);
        s_stream_pb = NULL;
    }
    return 0;
}

/*****************************************************************************
  函 数 名  : HistoryChannelPosition
  功能描述  : 历史通道定位（seek）
  输入参数  : dwUserID 用户ID
              buf      定位时间信息
  输出参数  : 无
  返 回 值  : 0成功，其他失败
*****************************************************************************/
static int HistoryChannelPosition(unsigned long dwUserID, char *buf)
{
    HistoryChannelPositionInfo pos = {0};
    struct tm seek_tm = {0};
    time_t seek_time;

    (void)dwUserID;
    if (buf == NULL || s_stream_pb == NULL)
    {
        return -1;
    }

    memcpy(&pos, buf, sizeof(pos));
    seek_tm.tm_year = pos.cSeekTime.wYear - 1900;
    seek_tm.tm_mon = pos.cSeekTime.wMonth - 1;
    seek_tm.tm_mday = pos.cSeekTime.wDay;
    seek_tm.tm_hour = pos.cSeekTime.wHour;
    seek_tm.tm_min = pos.cSeekTime.wMinute;
    seek_tm.tm_sec = pos.cSeekTime.wSecond;
    seek_time = mktime(&seek_tm);
    return anj_record_pb_seek(s_stream_pb, seek_time);
}

/*****************************************************************************
  函 数 名  : HistoryChannelSetSpeed
  功能描述  : 历史通道设置播放速度
  输入参数  : dwUserID 用户ID
              dwSpeed  播放速度
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
static int HistoryChannelSetSpeed(unsigned long dwUserID, unsigned long dwSpeed)
{
    (void)dwUserID;
    if (s_stream_pb != NULL)
    {
        anj_record_pb_speed_set(s_stream_pb, (int)dwSpeed);
    }
    return 0;
}

/*****************************************************************************
  函 数 名  : gb28181StartStream
  功能描述  : GB28181开始实时流
  输入参数  : gb28181Id 流ID
              bH265Type 输出参数，是否H265编码
  输出参数  : bH265Type
  返 回 值  : 0成功
*****************************************************************************/
static int gb28181StartStream(int gb28181Id, int *bH265Type)
{
    int streamIdx = s_cfg.nstreams ? 1 : 0;

    anj_mbuf_poper_create(0, streamIdx, &s_poper, Service_GB2818_SendVStream, gb28181Id, ANJ_MBUF_POPER_REQUEST_GB28181);

    if (bH265Type)
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        VideoEncodeCfg *pstVideoEncodeCfg = &pstMediaConfig->videoConfig[0].videoEncode.encodeCfg[streamIdx];
        media_codec_type_e video_type = video_encode_type_get(pstVideoEncodeCfg->encodeFormat.name);
        *bH265Type = (video_type == MEDIA_CODEC_VIDEO_H265_PLUS || video_type == MEDIA_CODEC_VIDEO_H265) ? 1 : 0;
    }
    return 0;
}

/*****************************************************************************
  函 数 名  : gb28181StopStream
  功能描述  : GB28181停止实时流
  输入参数  : gb28181Id 流ID
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
static int gb28181StopStream(int gb28181Id)
{
    (void)gb28181Id;
    anj_mbuf_poper_release(&s_poper);
    return 0;
}

/*****************************************************************************
  函 数 名  : anj_gb28181_param_init
  功能描述  : GB28181参数初始化，支持DNS域名解析
  输入参数  : 无（使用全局配置s_cfg）
  输出参数  : 无
  返 回 值  : 0成功，其他失败
*****************************************************************************/
static int anj_gb28181_param_init(void)
{
    int iRet = -1;

    if (isValidIp4_on(s_cfg.hcIp) != 1)
    {
        char hostname[100] = {0};
        char ipstr[100] = {0};

        if (strstr(s_cfg.hcIp, "http") != NULL ||
            strstr(s_cfg.hcIp, ":") != NULL ||
            strstr(s_cfg.hcIp, "//") != NULL ||
            strstr(s_cfg.hcIp, "/") != NULL ||
            strstr(s_cfg.hcIp, "https") != NULL)
        {
            if (getrel_hostname(s_cfg.hcIp, hostname, sizeof(hostname)) != 0)
            {
                __INFO("hostname:%s\n", hostname);
                return iRet;
            }
            else
            {
                strcpy(s_cfg.hcIp, hostname);
            }
        }

        if (decodeDNS(s_cfg.hcIp, ipstr, 100) != 0)
        {
            return iRet;
        }

        __INFO("s_cfg.hcIp: %s \n", s_cfg.hcIp);
        memset(s_cfg.hcIp, 0, 32);
        strcpy(s_cfg.hcIp, ipstr);
        __INFO("s_cfg.hcIp: %s \n", s_cfg.hcIp);
    }
    iRet = 0;
    return iRet;
}

/*****************************************************************************
  函 数 名  : anj_gb28181_thread
  功能描述  : GB28181主线程，负责服务生命周期管理
  输入参数  : ctx    线程上下文
              bStart 运行标志
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
static int anj_gb28181_thread(void *ctx, int *bStart)
{
    int run_sec = 0;
    char local_ip[MAX_IP_NAME_LEN] = {0};
    char last_ip[MAX_IP_NAME_LEN] = {0};
    (void)ctx;
    memset(&s_status, 0, sizeof(s_status));
    s_status.bUpdateInfo = 1;

    while (bStart && *bStart)
    {
        if (run_sec == 0)
        {
            GB28181_stream_CB mInterface;
            GB28181_ptz_CB ptz = {0};
            HistoryQueryInterfaceMultiType hqi = {0};
            HistoryChannelInterfaceMultiTypeCB hci = {0};

            memset(&mInterface, 0, sizeof(mInterface));
            mInterface.ifStartStreamCb = gb28181StartStream;
            mInterface.ifStopStreamCb = gb28181StopStream;
            Service_GB2818_SetStreamIf(&mInterface);

            ptz.ifCtrlPtzCb = gb28181PtzCtrl;
            Service_GB2818_SetPtzIf(&ptz);

            hqi.ifHistoryQuery = HistoryQuery;
            Service_GB2818_HistoryQueryMultiTypeIf(&hqi);

            hci.ifHistoryChannelStartCapture = HistoryChannelStartCapture;
            hci.ifHistoryChannelStopCapture = HistoryChannelStopCapture;
            hci.ifHistoryChannelPauseCapture = HistoryChannelPauseCapture;
            hci.ifHistoryChannelSetSpeed = HistoryChannelSetSpeed;
            hci.ifHistoryChannelPosition = HistoryChannelPosition;
            Service_GB2818_HistoryChannelMultiTypeIf(&hci);
        }

        if (s_status.bUpdateInfo)
        {
            GB28181Config *cfg = getGb28181Config();
            if (cfg != NULL)
            {
                if (0 != memcmp(&s_cfg, cfg, sizeof(s_cfg)))
                {
                    memset(local_ip, 0, sizeof(local_ip));
                    anj_net_ip_get(local_ip, sizeof(local_ip));
                    if (strlen(local_ip) == 0)
                    {
                        __ERR("failed to anj_net_ip_get\n");
                        sleep(1);
                        run_sec++;
                        continue;
                    }
                    memcpy(&s_cfg, cfg, sizeof(s_cfg));
                    anj_gb28181_param_init();
                    snprintf(last_ip, sizeof(last_ip), "%s", local_ip);
                    __INFO("init local ip baseline=%s\n", last_ip);
                    s_status.bChangeUpdate = 1;
                }
                s_status.bStartTmp = s_cfg.enable;
            }
            s_status.bUpdateInfo = 0;
        }

        if (s_status.bStart && (0 == (run_sec % GB28181_CHECK_IP_CHANGES)))
        {
            memset(local_ip, 0, sizeof(local_ip));
            anj_net_ip_get(local_ip, sizeof(local_ip));
            if (strlen(local_ip) == 0)
            {
                __ERR("failed to anj_net_ip_get\n");
                sleep(1);
                run_sec++;
                continue;
            }
            if (last_ip[0] == '\0')
            {
                snprintf(last_ip, sizeof(last_ip), "%s", local_ip);
                __INFO("init local ip baseline=%s\n", last_ip);
            }
            else if (0 != strcmp(last_ip, local_ip))
            {
                __INFO("real ip change Ip=%s --> %s!\n", last_ip, local_ip);
                snprintf(last_ip, sizeof(last_ip), "%s", local_ip);
                s_status.bChangeUpdate = 1;
            }
        }

        if (run_sec > GB28181_START_DELAY_SEC &&
            (s_status.bStart != s_status.bStartTmp || (s_status.bChangeUpdate && s_status.bStart)))
        {
            const char *reload_reason = (s_status.bStart != s_status.bStartTmp) ? "start-state-change" : "config-or-ip-change";
            s_status.bChangeUpdate = 0;

            if (s_status.bStart)
            {
                __INFO("reload gb28181_lib reason=%s action=close\n", reload_reason);
                Service_GB2818_Close();
            }
            if (s_status.bStartTmp)
            {
                if (s_cfg.enable)
                {
                    Service_GB28181_Cfg open_cfg;
                    memset(&open_cfg, 0, sizeof(open_cfg));
                    if (last_ip[0] == '\0')
                    {
                        memset(local_ip, 0, sizeof(local_ip));
                        anj_net_ip_get(local_ip, sizeof(local_ip));
                        if (strlen(local_ip) == 0)
                        {
                            __ERR("failed to anj_net_ip_get\n");
                            sleep(1);
                            run_sec++;
                            continue;
                        }
                        snprintf(last_ip, sizeof(last_ip), "%s", local_ip);
                        __INFO("init local ip baseline=%s\n", last_ip);
                    }
                    open_cfg.hcPort = s_cfg.hcPort;
                    open_cfg.lcPort = s_cfg.lcPort;
                    snprintf(open_cfg.hcIP, sizeof(open_cfg.hcIP), "%s", s_cfg.hcIp);
                    snprintf(open_cfg.hcName, sizeof(open_cfg.hcName), "%s", s_cfg.hcName);
                    snprintf(open_cfg.hcID, sizeof(open_cfg.hcID), "%s", s_cfg.hcId);
                    snprintf(open_cfg.lcIp, sizeof(open_cfg.lcIp), "%s", last_ip);
                    snprintf(open_cfg.lcId, sizeof(open_cfg.lcId), "%s", s_cfg.lcId);
                    snprintf(open_cfg.Username, sizeof(open_cfg.Username), "%s", s_cfg.lcName);
                    snprintf(open_cfg.Pwd, sizeof(open_cfg.Pwd), "%s", s_cfg.lcPwd);
                    snprintf(open_cfg.AlarmId, sizeof(open_cfg.AlarmId), "%s", s_cfg.alarmId);
                    open_cfg.expires = 3600;
                    open_cfg.keepAlive = 15;
                    open_cfg.keepAliveNum = 3;

                    __INFO("reload gb28181_lib reason=%s action=open local_ip=%s\n", reload_reason, open_cfg.lcIp);
                    sleep(1); /* 限制启动间隔防止频繁重启 */
                    Service_GB2818_Open(&open_cfg);
                }
            }
            s_status.bStart = s_status.bStartTmp;
        }

        sleep(1);
        run_sec++;
    }

    Service_GB2818_Close();
    if (s_stream_pb != NULL)
    {
        anj_record_pb_release(s_stream_pb);
        s_stream_pb = NULL;
    }
    return 0;
}

/*****************************************************************************
  函 数 名  : Service_GB28181_AlarmHandle
  功能描述  : 报警事件处理入口（由服务框架回调）
  输入参数  : alarm_data 报警事件数据
  输出参数  : 无
  返 回 值  : 0成功，其他失败
*****************************************************************************/
int Service_GB28181_AlarmHandle(void *alarm_data)
{
    alarm_event_data *alarm_event = (alarm_event_data *)alarm_data;
    SYSTEM_TIME stSystemTime = {0};

    if (alarm_data == NULL)
    {
        return -1;
    }

    if (!s_status.bStart)
    {
        return 0;
    }

    stSystemTime.year = alarm_event->year;
    stSystemTime.month = alarm_event->month;
    stSystemTime.day = alarm_event->day;
    stSystemTime.hour = alarm_event->hour;
    stSystemTime.minute = alarm_event->minute;
    stSystemTime.second = alarm_event->second;

    return Service_GB28181_AlarmEventDeal(alarm_event->alarm_code, &stSystemTime);
}

/*****************************************************************************
  函 数 名  : Service_GB28181_AudioEncChange
  功能描述  : GB28181音频编码更新
  输入参数  : 无
  输出参数  : 无
  返 回 值  : 无
*****************************************************************************/
void Service_GB28181_AudioEncChange()
{
    Service_GB28181_UpdateAudioType();
}

/*****************************************************************************
  函 数 名  : Service_GB2818_Update
  功能描述  : GB28181配置更新事件处理
  输入参数  : event_result 事件结果
              data         事件数据
  输出参数  : 无
  返 回 值  : 无
*****************************************************************************/
static void Service_GB2818_Update(EventResult *event_result, void *data)
{
    (void)event_result;
    (void)data;
    s_status.bUpdateInfo = 1;
}

/*****************************************************************************
  函 数 名  : Service_GB2818_stop
  功能描述  : 暂停GB28181服务
  输入参数  : bBlock 是否阻塞
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
int Service_GB2818_stop(int bBlock)
{
    (void)bBlock;
    s_status.bStartTmp = 0;
    return 0;
}

/*****************************************************************************
  函 数 名  : Service_GB2818_start
  功能描述  : 启动GB28181服务
  输入参数  : bBlock 是否阻塞
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
int Service_GB2818_start(int bBlock)
{
    (void)bBlock;
    s_status.bStartTmp = 1;
    return 0;
}

/*****************************************************************************
  函 数 名  : Service_GB2818_restart
  功能描述  : 重新启动GB28181服务
  输入参数  : bBlock 是否阻塞
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
int Service_GB2818_restart(int bBlock)
{
    (void)bBlock;
    s_status.bChangeUpdate = 1;
    return 0;
}

/*****************************************************************************
  函 数 名  : gb28181_init
  功能描述  : GB28181模块初始化
  输入参数  : 无
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
int gb28181_init()
{
    if (s_thread.start == 0)
    {
        s_thread.bAutoDestroy = 0;
        strncpy(s_thread.iThreadName, "gb28181_lib", sizeof(s_thread.iThreadName) - 1);
        s_thread.iThreadjob.ctx = &s_thread;
        s_thread.iThreadjob.func = anj_gb28181_thread;
        anj_thread_task_create(&s_thread);
        eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_GB28181_UPDATE, Service_GB2818_Update);
    }
    anj_sysctl_capability_add(FUNCTION_SUPPORT_28181);
    return 0;
}

/*****************************************************************************
  函 数 名  : gb28181_uninit
  功能描述  : GB28181模块反初始化
  输入参数  : 无
  输出参数  : 无
  返 回 值  : 0成功
*****************************************************************************/
int gb28181_uninit()
{
    anj_thread_task_destroy(&s_thread, -1);
    memset(&s_thread, 0, sizeof(s_thread));
    return 0;
}
