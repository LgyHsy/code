
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "anj_mw_comm.h"
#include "anj_module.h"
#include "anj_config.h"
#include "anj_pri.h"
#include "anj_pri_cmd.h"
#include "anj_alarm.h"
#include "anj_sysmng.h"
#include "anj_systime.h"
#include "anj_sysctl.h"
#include "anj_sdcard.h"
#include "anj_smart.h"
#include "anj_zoom.h"
#include "anj_osd.h"
#include "anj_record.h"
#include "record_log.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_net.h"
#include "anj_ispctl.h"
#include "anj_factory.h"
#include "cmd_def.h"
#include "eventhub.h"
#include "anj_mw_hwctrl.h"
#include "alarm_link.h"

#include "file_sender.h"
#include "file_receiver.h"
#include "audio_receiver.h"
#include "ota_update.h"

#include "anj_service.h"
#include "anj_service_provider.h"
#include "anj_search.h"
#include "anj_audio.h"
#include "anj_config_ptz.h"
#include "anj_ftpemail.h"
#include "util_font.h"

typedef enum
{
    SearchModeByFiles = 0,
    SearchModeByTimeDistribute,
    SearchModeByTimeInterval,
    SearchModeGetDateList = 10,
    SearchModeGetAlarmList
} SearchMode_e;

#define LOG_NUM_PER_PAGE 500
/* 单条/头尾长度：用最长样例串 sizeof-1 */
#define ALARM_ITEM_XML_SIZE \
    (sizeof("<Item Time=\"20240311 19:11:43\" Alarm=\"al_in\" Du=\"180\" Chn=\"99\"/> ") - 1)
#define DATE_ITEM_XML_SIZE \
    (sizeof("<DATE\nUtcTime=\"4294967295\"\nDateStr=\"20250101\"\n/>\n") - 1)
#define RESPONSE_XML_OVERHEAD \
    (sizeof("<RESPONSE_PARAM SearchMode=\"99\" Page=\"9999\" SearchHandle=\"1\" AlarmCount=\"500\"> </RESPONSE_PARAM>") - 1)

static int g_ptz_test_lstep = 0;
static int g_ptz_test_vstep = 0;

int anj_service_alarm_event_notify(void *alarm_event)
{
    anj_pri_alarm_event_notify(alarm_event);
    anj_service_provider_alarm_event_notify(alarm_event);

    return 0;
}

int anj_service_audio_enc_change()
{
    anj_service_provider_audio_enc_change();

    return 0;
}

static int anj_service_init(void)
{
    anj_pri_init();
    anj_search_init();
    anj_service_provider_init_all();

    return 0;
}

static int anj_service_uninit(void)
{
    anj_service_provider_uninit_all();
    anj_search_uninit();
    anj_pri_uninit();
    return 0;
}

int anj_service_partner_proc(char *cmdbuf, int cmdlen, int lognum, IXML_Document *pDoc, char *MsgType, char *MsgCode)
{
    int ret = 0;
    char *str1 = GetRequestParamValue(pDoc, "parter");
    char *str2 = GetRequestParamValue(pDoc, "datestr");
    char *str3 = GetRequestParamValue(pDoc, "macaddr");

    if (str1 == NULL || str2 == NULL || str3 == NULL)
    {
        if (str1)
            anj_mw_free(str1);
        if (str2)
            anj_mw_free(str2);
        if (str3)
            anj_mw_free(str3);

        ret = -1;
        __ERR("CMD_SET_PARTNER_NAME params NULL error.\n");
        return ret;
    }

    if (*str1 == 0 || *str2 == 0 || *str3 == 0)
    {
        if (str1)
            anj_mw_free(str1);
        if (str2)
            anj_mw_free(str2);
        if (str3)
            anj_mw_free(str3);
        __ERR("CMD_SET_PARTNER_NAME params EMPTY error.\n");
        ret = -1;
        return ret;
    }

    char szPartner[128] = {0}, szDatestr[128] = {0}, szMacaddr[128] = {0};
    anj_sysmng_partner_info_get(szPartner, szDatestr, szMacaddr);
    if (strlen(szPartner) > 0 || strlen(szDatestr) > 0 || strlen(szMacaddr) > 0)
    {
        if (strcmp(szPartner, str1) == 0)
        {
            ret = 0;
        }
        else
        {
            __ERR("CMD_SET_PARTNER_NAME: already exist %s %s %s.\n", szPartner, szDatestr, szMacaddr);
            ret = -1;
        }
        if (str1)
            anj_mw_free(str1);
        if (str2)
            anj_mw_free(str2);
        if (str3)
            anj_mw_free(str3);
        return ret;
    }

    ret = 0;
    anj_sysmng_partner_info_set(str1, str2, str3);

    if (str1)
        anj_mw_free(str1);
    if (str2)
        anj_mw_free(str2);
    if (str3)
        anj_mw_free(str3);
    return ret;
}

int anj_service_export_record(char *cmdbuf, int cmdlen, int lognum, IXML_Document *pDoc, char *MsgType, char *MsgCode, int timelapse)
{
    int ret = 0;
    DevInfo *pstDevInfo = getDevInfo();
    char *strReCreate = GetRequestParamValue(pDoc, "recreate");
    char *strTargetSecond = GetRequestParamValue(pDoc, "targetsec");
    char *strYear = GetRequestParamValue(pDoc, "year");
    char *strMonth = GetRequestParamValue(pDoc, "month");
    char *strDay = GetRequestParamValue(pDoc, "day");
    char *strStarttime = GetRequestParamValue(pDoc, "StartTime");
    char *strEndTime = GetRequestParamValue(pDoc, "EndTime");

    int bRecreate = 0;
    int nTargetSeconds = 120;
    if (strReCreate != NULL)
        bRecreate = atoi(strReCreate);
    if (strTargetSecond != NULL)
        nTargetSeconds = atoi(strTargetSecond);

    if (strYear != NULL && strMonth != NULL && strDay != NULL)
    {
        DayTimeSpan timespan = {0};
        if (strStarttime != NULL)
            GetDayTimeFromStr(strStarttime, &timespan.startTime);
        if (strEndTime != NULL)
            GetDayTimeFromStr(strEndTime, &timespan.endTime);

        char stMountPath[32] = {0};
        snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, anj_sdcard_mount_index_get());

        unsigned int tBeginTime = 0;
        unsigned int tEndTime = 0;
        struct tm tm;
        tm.tm_year = atoi(strYear) - 1900;
        tm.tm_mon = atoi(strMonth) - 1;
        tm.tm_mday = atoi(strDay);
        tm.tm_hour = timespan.startTime.hour;
        tm.tm_min = timespan.startTime.minute;
        tm.tm_sec = timespan.startTime.sec;
        tBeginTime = mktime(&tm);
        tm.tm_year = atoi(strYear) - 1900;
        tm.tm_mon = atoi(strMonth) - 1;
        tm.tm_mday = atoi(strDay);
        tm.tm_hour = timespan.endTime.hour;
        tm.tm_min = timespan.endTime.minute;
        tm.tm_sec = timespan.endTime.sec;
        tEndTime = mktime(&tm);

        char szOutputDirPath[64] = {0};
        snprintf(szOutputDirPath, sizeof(szOutputDirPath), "%s/timelapse", stMountPath);
        char szOutputFileName[256] = {0};
        sprintf(szOutputFileName, "%s/timelapse_%s_%04d%02d%02d_%02d%02d%02d-%02d%02d%02d_%03d.mp4",
                szOutputDirPath, pstDevInfo->sn,
                atoi(strYear), atoi(strMonth), atoi(strDay),
                timespan.startTime.hour, timespan.startTime.minute, timespan.startTime.sec,
                timespan.endTime.hour, timespan.endTime.minute, timespan.endTime.sec,
                nTargetSeconds);

        if (bRecreate == 0 && anj_mw_file_exists(szOutputFileName))
        {
            anj_alarm_event_handle(0, ALARM_CODE_FILE_READY_FOR_DOWNLOAD, ALARM_FLAG_OCCUR,
                                   ALARM_LEVEL_EVENT, 0, szOutputFileName, NULL);
        }
        else
        {
            mkdir(szOutputDirPath, 0777);
            anj_record_pb_download_mp4(0, szOutputFileName, tBeginTime, tEndTime, timelapse);
        }
        __INFO("got create timelapse: %s %s %s\n", strYear, strMonth, strDay);
    }
    else
    {
        ret = -1;
        __ERR("CMD_CREATE_TIMELAPSE params error.");
    }

    if (strReCreate)
        anj_mw_free(strReCreate);
    if (strTargetSecond)
        anj_mw_free(strTargetSecond);
    if (strYear)
        anj_mw_free(strYear);
    if (strMonth)
        anj_mw_free(strMonth);
    if (strDay)
        anj_mw_free(strDay);
    if (strStarttime)
        anj_mw_free(strStarttime);
    if (strEndTime)
        anj_mw_free(strEndTime);
    return ret;
}

int anj_service_task_system_get(int cmd, void *data, int *len, int channel)
{
    int iRet = 0;
    char *buf = NULL;
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();
    pthread_rwlock_rdlock(rwlock);
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    RecordConfig *pstRecordConfig = (RecordConfig *)getRecordConfig();
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    GB28181Config *pstGb28181Config = (GB28181Config *)getGb28181Config();
    GAT1400Config *pstGat1400Config = (GAT1400Config *)getGat1400Config();
    ServerConfig *pstServerConfig = (ServerConfig *)getServerConfig();

    if (channel < 0 || channel >= ANJ_CAMERA_MAX_NUMS)
    {
        channel = 0;
    }
    switch (cmd)
    {
    case CMD_GET_SYSTEM_CONFIG:
    {
        buf = anj_config_system_conver_xml(pstSystemCfg);
        break;
    }
    case CMD_GET_SYSTEM_PTZ_CONFIG:
    {
        buf = anj_config_system_ptz_conver_xml(&pstSystemCfg->ptzCfg);
        break;
    }
    case CMD_GET_SYSTEM_PTZ_DZOOM_CONFIG:
    {
        buf = anj_config_system_ptz_dzoom_conver_xml(&pstSystemCfg->ptzCfg.dzoomCfg);
        break;
    }
    case CMD_GET_SYSTEM_PTZ_SCAN_CONFIG:
    {
        buf = anj_config_system_ptz_scan_conver_xml(&pstSystemCfg->ptzCfg.scanConfig);
        break;
    }
    case CMD_GET_SYSTEM_VIDEO_QOS_CONFIG:
    {
        buf = anj_config_system_videoq_conver_xml(&pstSystemCfg->videoQosCfg);
        break;
    }
    case CMD_GET_SYSTEM_AUDIOPROMPT_CONFIG:
    {
        buf = anj_config_system_audioprompt_conver_xml(&pstSystemCfg->audioPromptCfg);
        break;
    }
    case CMD_GET_SYSTEM_TAMPERPROOF_CONFIG:
    {
        buf = anj_config_system_tamperproof_conver_xml(&pstSystemCfg->tamperProofCfg);
        break;
    }
    case CMD_GET_SYSTEM_LOCATION_CONFIG:
    {
        buf = anj_config_system_location_conver_xml(&pstSystemCfg->locationCfg);
        break;
    }
    case CMD_GET_SYSTEM_TIME_CONFIG:
    {
        buf = anj_config_system_time_conver_xml(&pstSystemCfg->timeCfg);
        break;
    }
    case CMD_GET_SYSTEM_USER_CONFIG:
    {
        buf = anj_config_system_user_conver_xml(&pstSystemCfg->userCfg, 0);
        break;
    }
    case CMD_GET_SYSTEM_LOG_CONFIG:
    {
        buf = anj_config_system_syslog_conver_xml(&pstSystemCfg->syslogCfg);
        break;
    }
    case CMD_GET_SYSTEM_MISC_CONFIG:
    {
        buf = anj_config_system_misc_conver_xml(&pstSystemCfg->miscCfg);
        break;
    }
    case CMD_GET_SYSTEM_MAINTAIN_CONFIG:
    {
        buf = anj_config_system_maintain_conver_xml(&pstSystemCfg->maintainCfg);
        break;
    }
    case CMD_GET_SYSTEM_ALOWIP_CONFIG:
    {
        buf = anj_config_system_allowip_conver_xml(&pstSystemCfg->alowipCfg);
        break;
    }
    case CMD_GET_SYSTEM_ALARMCLOCK_CONFIG:
    {
        buf = anj_config_alarm_clock_conver_xml(&pstSystemCfg->clockSetting.oclock);
        break;
    }
    case CMD_GET_NETWORK_CONFIG:
    {
        buf = anj_config_network_conver_xml(pstNetworkConfig);
        break;
    }
    case CMD_GET_NETWORK_LAN_CONFIG:
    {
        buf = anj_config_network_lan_conver_xml(&pstNetworkConfig->lanCfg);
        break;
    }
    case CMD_GET_DEFAULT_NETWORK_LAN_CONFIG:
    {
        GlobalConfig stGlobalConfig = {0};
        char configDefaultPath[128] = {0};
        char *pDefFile = anj_config_default_file_get();
        if (pDefFile != NULL && pDefFile[0] != 0)
        {
            snprintf(configDefaultPath, sizeof(configDefaultPath), "%s", pDefFile);
            anj_config_load(NULL, &stGlobalConfig, toLowerStr(configDefaultPath));
        }
        buf = anj_config_network_lan_conver_xml(&stGlobalConfig.networkCfgNew.lanCfg);
        break;
    }
    case CMD_GET_NETWORK_WIFI_CONFIG:
    {
        if (pstNetworkConfig->wifiCfg.dhcpEnable > 0)
        {
            struct NET_CONFIG stNetConfig = {0};
            int ret = net_get_info(net_get_wireless_name(), &stNetConfig);
            if (ret == 0)
            {
                get_ip_str(stNetConfig.ifaddr, pstNetworkConfig->wifiCfg.IPAddress, MAX_IP_NAME_LEN);
                get_ip_str(stNetConfig.netmask, pstNetworkConfig->wifiCfg.netMask, MAX_IP_NAME_LEN);
                get_ip_str(stNetConfig.gateway, pstNetworkConfig->wifiCfg.gateWay, MAX_IP_NAME_LEN);
            }
        }

        buf = anj_config_network_wifi_conver_xml(&pstNetworkConfig->wifiCfg);
        break;
    }
    case CMD_GET_NETWORK_WIFIAP_CONFIG:
    {
        buf = anj_config_network_wifiap_conver_xml(&pstNetworkConfig->wifiApCfg);
        break;
    }
    case CMD_GET_NETWORK_ALARM_SERVER_CONFIG:
    {
        buf = anj_config_network_alarmserver_conver_xml(&pstNetworkConfig->alarmServerCfg, 0);
        break;
    }
    case CMD_GET_NETWORK_ADSL_CONFIG:
    {
        buf = anj_config_network_adsl_conver_xml(&pstNetworkConfig->adslCfg, 0);
        break;
    }
    case CMD_GET_NETWORK_DDNS_CONFIG:
    {
        buf = anj_config_network_ddns_conver_xml(&pstNetworkConfig->ddnsCfg, 0);
        break;
    }
    case CMD_GET_NETWORK_UPNP_CONFIG:
    {
        buf = anj_config_network_upnp_conver_xml(&pstNetworkConfig->upnpCfg);
        break;
    }
    case CMD_GET_NETWORK_P2P_CONFIG:
    {
        buf = anj_config_network_p2p_conver_xml(&pstNetworkConfig->p2pCfg);
        break;
    }
    case CMD_GET_NETWORK_G4_CONFIG:
    {
        buf = anj_config_network_g4_conver_xml(&pstNetworkConfig->g4Cfg);
        break;
    }
    case CMD_GET_NETWORK_PPTP_CONFIG:
    {
        buf = anj_config_network_pptp_conver_xml(&pstNetworkConfig->pptpCfg, 0);
        break;
    }
    case CMD_GET_SERVER_CONFIG:
    {
        buf = anj_config_server_conver_xml(pstServerConfig);
        break;
    }
    case CMD_GET_SERVER_FTP_CONFIG:
    {
        FtpServerList stftpServerList;
        memcpy(&stftpServerList, pstServerConfig->ftpServers, sizeof(FtpServerList));
        buf = anj_config_server_ftp_conver_xml(&stftpServerList, 0);
        break;
    }
    case CMD_GET_SERVER_SMTP_CONFIG:
    {
        buf = anj_config_server_smtp_conver_xml(&pstServerConfig->smtpServers, 0);
        break;
    }
    case CMD_GET_MEDIA_CONFIG:
    {
        buf = anj_config_media_conver_xml(pstMediaConfig, channel, 1);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_CONFIG:
    {
        buf = anj_config_video_conver_xml(pstMediaConfig->videoConfig, channel, 1);
        break;
    }
    case CMD_GET_MEDIA_AUDIO_CONFIG:
    {
        buf = anj_config_audio_conver_xml(&pstMediaConfig->audioConfig);
        break;
    }
    case CMD_GET_MEDIA_AUDIO_CAPTURE:
    {
        buf = anj_config_audio_capture_conver_xml(&pstMediaConfig->audioConfig.audioCapture);
        break;
    }
    case CMD_GET_MEDIA_AUDIO_ENCODE:
    {
        buf = anj_config_audio_encode_conver_xml(&pstMediaConfig->audioConfig.audioEncode);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_CAPTURE:
    {
        buf = anj_config_video_capture_conver_xml(&pstMediaConfig->videoConfig[channel].videoCapture);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_JPEG:
    {
        buf = anj_config_jpeg_conver_xml(&pstMediaConfig->videoConfig[channel].jpegCfg);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_ENCODE:
    {
        buf = anj_config_video_encode_conver_xml(&pstMediaConfig->videoConfig[channel].videoEncode);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_OSD:
    {
        buf = anj_config_overlay_conver_xml(&pstMediaConfig->videoConfig[channel].overlay);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_USEROSD:
    {
        buf = anj_config_user_overlay_conver_xml(&pstMediaConfig->videoConfig[channel].useroverlay);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_MASK:
    {
        buf = anj_config_video_mask_conver_xml(&pstMediaConfig->videoConfig[channel].videoMask);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_ROI:
    {
        buf = anj_config_video_roi_conver_xml(&pstMediaConfig->videoConfig[channel].roiCfg);
        break;
    }
    case CMD_GET_MEDIA_VIDEO_YUV:
    {
        buf = anj_config_video_yuv_conver_xml(&pstMediaConfig->videoConfig[channel].yuvCfg);
        break;
    }
    case CMD_GET_MEDIASTREAM_CONFIG:
    {
        buf = anj_config_stream_conver_xml(pstMediaStreamConfig);
        break;
    }
    case CMD_GET_GB28181_CONFIG:
    {
        buf = anj_config_gb28181_conver_xml(pstGb28181Config);
        break;
    }
    case CMD_GET_GAT1400_CONFIG:
    {
        buf = anj_config_gat1400_conver_xml(pstGat1400Config);
        break;
    }
    case CMD_GET_ALARM_CONFIG:
    {
        buf = anj_config_alarm_conver_xml(pstAlarmConfig);
        break;
    }
    case CMD_GET_ALARM_INPUT_CONFIG:
    {
        buf = anj_config_alarm_input_conver_xml(&pstAlarmConfig->normalAlarm.inputAlarm);
        break;
    }
    case CMD_GET_ALARM_OUTPUT_CONFIG:
    {
        buf = anj_config_alarm_output_conver_xml(&pstAlarmConfig->normalAlarm.outputAlarm);
        break;
    }
    case CMD_GET_ALARM_MOTIONDETECT_CONFIG:
    {
        buf = anj_config_alarm_motion_conver_xml(&pstAlarmConfig->normalAlarm.motionDetectAlarm[channel], channel, 1);
        break;
    }
    case CMD_GET_ALARM_VIDEOCOVER_CONFIG:
    {
        buf = anj_config_alarm_video_cover_conver_xml(&pstAlarmConfig->normalAlarm.videoCoverAlarm[channel], channel, 1);
        break;
    }
    case CMD_GET_ALARM_VIDEOLOST_CONFIG:
    {
        buf = anj_config_alarm_video_lost_conver_xml(&pstAlarmConfig->normalAlarm.videoLostAlarm[channel], channel, 1);
        break;
    }
    case CMD_GET_ALARM_SF_CONFIG:
    {
        buf = anj_config_alarm_storage_full_conver_xml(&pstAlarmConfig->normalAlarm.storageFullAlarm);
        break;
    }
    case CMD_GET_ALARM_AUDIO_CONFIG:
    {
        buf = anj_config_alarm_audio_conver_xml(&pstAlarmConfig->aiAlarm.audioAlarm);
        break;
    }
    case CMD_GET_ALARM_VIDEO_GATE:
    {
        buf = anj_config_alarm_video_gate_conver_xml(&pstAlarmConfig->aiAlarm.vgAlarm[channel], channel, 1);
        break;
    }
    case CMD_GET_ALARM_PD:
    {
        buf = anj_config_alarm_pd_conver_xml(&pstAlarmConfig->aiAlarm.pdAlarm[channel], channel, 1);
        break;
    }
    case CMD_GET_ALARM_LPR:
    {
        buf = anj_config_alarm_lpr_conver_xml(&pstAlarmConfig->aiAlarm.lprAlarm[channel]);
        break;
    }
    case CMD_GET_ALARM_FD:
    {
        buf = anj_config_alarm_fd_conver_xml(&pstAlarmConfig->aiAlarm.fdAlarm[channel], channel, 1);
        break;
    }
    case CMD_GET_TEMP_HUMIDITY_INFO:
    {
        buf = anj_config_alarm_temp_humidity_conver_xml(&pstAlarmConfig->normalAlarm.temphumidityAlarm);
        break;
    }
    case CMD_GET_ALARM_REGION_AI:
    {
        buf = anj_config_alarm_region_ai_conver_xml(&pstAlarmConfig->aiAlarm.regionAiAlarm[channel], channel, 1);
        break;
    }
    case CMD_GET_ALARM_SMS:
    {
        buf = anj_config_alarm_sms_conver_xml(&pstAlarmConfig->normalAlarm.smsAlarm);
        break;
    }
    case CMD_GET_ALARM_FLAMEANDFLUMES:
    {
        buf = anj_config_alarm_fire_conver_xml(&pstAlarmConfig->aiAlarm.fireAlarm);
        break;
    }
    case CMD_GET_RECORD_CONFIG:
    {
        buf = anj_config_record_conver_xml(pstRecordConfig, channel, 1);
        break;
    }
    case CMD_GET_SYSTEMCONTROLSTRING:
    {
        char *cap = anj_sysctl_get_capability_string();
        buf = anj_mw_malloc(strlen(cap) + 1);
        memset(buf, 0, strlen(cap) + 1);
        snprintf(buf, strlen(cap), "%s", cap);
        break;
    }
    default:
    {
        break;
    }
    }
    pthread_rwlock_unlock(rwlock);
    if (buf)
    {
        strcpy((char *)data, buf);
        *len = strlen(buf);
        anj_mw_free(buf);
    }
    return iRet;
}

int anj_service_task_system_set(int cmd, char *data, int channel, int MsgSrc)
{
    int iRet = 0;
    switch (cmd)
    {
    case CMD_SET_SYSTEM_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        SystemConfig stSystemConfig = *pstSystemConfig;
        iRet = anj_config_system_get_by_xml(&stSystemConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_set(&stSystemConfig);
        }
        break;
    }
    case CMD_SET_SYSTEM_PTZ_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        PTZConfig stPtzConfig = pstSystemConfig->ptzCfg;
        iRet = anj_config_system_ptz_get_by_xml(&stPtzConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_ptz_set(&stPtzConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_TIME_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        TimeConfig stTimeConfig = pstSystemConfig->timeCfg;
        iRet = anj_config_system_time_get_by_xml(&stTimeConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_time_set(&stTimeConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_USER_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        UserConfig stUserConfig = pstSystemConfig->userCfg;
        iRet = anj_config_system_user_get_by_xml(&stUserConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_user_set(&stUserConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_LOG_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        SyslogConfig stSyslogConfig = pstSystemConfig->syslogCfg;
        iRet = anj_config_system_syslog_get_by_xml(&stSyslogConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_syslog_set(&stSyslogConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_PTZ_COMMON:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        PTZCommonConfig stPtzCommonConfig = pstSystemConfig->ptzCfg.commonCfg;
        iRet = anj_config_system_ptz_common_get_by_xml(&stPtzCommonConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_ptz_comm_set(&stPtzCommonConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_PTZ_ADVANCE:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        PTZAdvanceConfig stPtzAdvanceConfig = pstSystemConfig->ptzCfg.advanceCfg;
        iRet = anj_config_system_ptz_advance_get_by_xml(&stPtzAdvanceConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_ptz_advance_set(&stPtzAdvanceConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_MISC_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        MiscConfig stMiscConfig = pstSystemConfig->miscCfg;
        iRet = anj_config_system_misc_get_by_xml(&stMiscConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_misc_set(&stMiscConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_MAINTAIN_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        MaintainConfig stMaintainConfig = pstSystemConfig->maintainCfg;
        iRet = anj_config_system_maintain_get_by_xml(&stMaintainConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_maintain_set(&stMaintainConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_ALOWIP_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        SysAlowIpConfig stAlowIpConfig = pstSystemConfig->alowipCfg;
        iRet = anj_config_system_allowip_get_by_xml(&stAlowIpConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_allowip_set(&stAlowIpConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_ALARMCLOCK_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        AlarmClockConfig stlarmClockConfig = pstSystemConfig->clockSetting;
        iRet = anj_config_system_alarmclock_get_by_xml(&stlarmClockConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_alarmclock_set(&stlarmClockConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_PTZ_AF:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        AfConfig stAfConfig = pstSystemConfig->ptzCfg.afCfg;
        iRet = anj_config_system_ptz_af_get_by_xml(&stAfConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_ptz_af_set(&stAfConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_PTZ_DZOOM:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        DZoomConfig stDZoomConfig = pstSystemConfig->ptzCfg.dzoomCfg;
        iRet = anj_config_system_ptz_dzoom_get_by_xml(&stDZoomConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_ptz_dzoom_set(&stDZoomConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_PTZ_SCAN_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        PTZScanConfig stScanConfig = pstSystemConfig->ptzCfg.scanConfig;
        iRet = anj_config_system_ptz_scan_get_by_xml(&stScanConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_ptz_scan_set(&stScanConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_VIDEO_QOS_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        VideoQoSConfig stVideoQosConfig = pstSystemConfig->videoQosCfg;
        iRet = anj_config_system_videoq_get_by_xml(&stVideoQosConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_videoq_set(&stVideoQosConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_AUDIOPROMPT_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        AudioPromptConfig stAudioPromptConfig = pstSystemConfig->audioPromptCfg;
        iRet = anj_config_system_audioprompt_get_by_xml(&stAudioPromptConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_audioprompt_set(&stAudioPromptConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_TAMPERPROOF_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        TamperProofConfig stTamperProofConfig = pstSystemConfig->tamperProofCfg;
        iRet = anj_config_system_tamperproof_get_by_xml(&stTamperProofConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_tamperproof_set(&stTamperProofConfig);
        }
    }
    break;
    case CMD_SET_SYSTEM_LOCATION_CONFIG:
    {
        SystemConfig *pstSystemConfig = getSystemConfig();
        LocationConfig stLocationConfig = pstSystemConfig->locationCfg;
        iRet = anj_config_system_location_get_by_xml(&stLocationConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_system_location_set(&stLocationConfig);
        }
    }
    break;
    case CMD_SET_NETWORK_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        NetworkConfigNew stNetworkConfig = *pstNetworkConfig;
        iRet = anj_config_network_get_by_xml(&stNetworkConfig, data, 1);
        if (iRet == 0)
        {
            iRet = anj_config_network_set(pstNetworkConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_LAN_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        LANConfig stLanConfig = pstNetworkConfig->lanCfg;
        iRet = anj_config_network_lan_get_by_xml(&stLanConfig, data, 1);
        if (iRet == 0)
        {
            iRet = anj_config_network_lan_set(&stLanConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_WIFI_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        WIFIConfig stWifiConfig = pstNetworkConfig->wifiCfg;
        iRet = anj_config_network_wifi_get_by_xml(&stWifiConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_wifi_set(&stWifiConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_WIFIAP_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        WIFIApConfig stWifiApConfig = pstNetworkConfig->wifiApCfg;
        iRet = anj_config_network_wifiap_get_by_xml(&stWifiApConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_wifiap_set(&stWifiApConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_ALARM_SERVER_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        AlarmServerConfig stAlarmServer = pstNetworkConfig->alarmServerCfg;
        iRet = anj_config_network_alarmserver_get_by_xml(&stAlarmServer, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_alarmserver_set(&stAlarmServer);
        }
        break;
    }
    case CMD_SET_NETWORK_ADSL_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        ADSLConfigNew stAdslConfig = pstNetworkConfig->adslCfg;
        iRet = anj_config_network_adsl_get_by_xml(&stAdslConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_adsl_set(&stAdslConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_DDNS_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        DDNSConfig stDdnsConfig = pstNetworkConfig->ddnsCfg;
        iRet = anj_config_network_ddns_get_by_xml(&stDdnsConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_ddns_set(&stDdnsConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_UPNP_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        UPNPConfig stUpnpConfig = pstNetworkConfig->upnpCfg;
        iRet = anj_config_network_upnp_get_by_xml(&stUpnpConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_upnp_set(&stUpnpConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_G4_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        G4Config stG4Config = pstNetworkConfig->g4Cfg;
        iRet = anj_config_network_g4_get_by_xml(&stG4Config, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_g4_set(&stG4Config);
        }
        break;
    }
    case CMD_SET_NETWORK_PPTP_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        PPTPConfig pstPptpConfig = pstNetworkConfig->pptpCfg;
        iRet = anj_config_network_pptp_get_by_xml(&pstPptpConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_pptp_set(&pstPptpConfig);
        }
        break;
    }
    case CMD_SET_NETWORK_P2P_CONFIG:
    {
        NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
        P2PConfig stP2pConfig = pstNetworkConfig->p2pCfg;
        P2PConfig stOldP2pConfig = stP2pConfig;
        iRet = anj_config_network_p2p_get_by_xml(&stP2pConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_network_p2p_set(&stP2pConfig);
            if (iRet == 0 && memcmp(&stOldP2pConfig, &stP2pConfig, sizeof(P2PConfig)) != 0)
            {
                __INFO("p2p config changed, restart anj_ser\n");
                module_uninit_single("anj_ser");
                usleep(10 * 1000);
                module_init_single("anj_ser");
            }
        }
        break;
    }
    case CMD_SET_SERVER_CONFIG:
    {
        ServerConfig *pstServerConfig = (ServerConfig *)getServerConfig();
        ServerConfig stServerConfig = *pstServerConfig;
        iRet = anj_config_server_get_by_xml(&stServerConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_server_set(&stServerConfig);
        }
        break;
    }
    case CMD_SET_SERVER_FTP_CONFIG:
    {
        ServerConfig *pstServerConfig = (ServerConfig *)getServerConfig();
        ServerConfig stServerConfig = *pstServerConfig;
        FtpServerList stFtpList = {0};

        iRet = anj_config_server_ftp_get_by_xml(&stServerConfig, data);
        if (iRet == 0)
        {
            memcpy(stFtpList.ftpServers, stServerConfig.ftpServers, sizeof(stFtpList.ftpServers));
            iRet = anj_config_server_ftp_set(&stFtpList);
        }
        break;
    }
    case CMD_SET_SERVER_SMTP_CONFIG:
    {
        ServerConfig *pstServerConfig = (ServerConfig *)getServerConfig();
        ServerConfig stServerConfig = *pstServerConfig;
        iRet = anj_config_server_smtp_get_by_xml(&stServerConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_server_smtp_list_set(&stServerConfig.smtpServers);
        }
        break;
    }
    case CMD_SET_MEDIA_CONFIG:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        MediaConfig stMediaConfig = *pstMediaConfig;
        iRet = anj_config_media_get_by_xml(&stMediaConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_media_set(&stMediaConfig);
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_CONFIG:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        VideoConfig stVideoConfigArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stVideoConfigArray, pstMediaConfig->videoConfig, sizeof(VideoConfig) * ANJ_CAMERA_MAX_NUMS);
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }

            iRet |= anj_config_video_get_by_xml(&stVideoConfigArray[cameraIndex], data);
        }
        if (iRet == 0)
        {
            iRet = anj_config_video_set(stVideoConfigArray);
        }
        break;
    }
    case CMD_SET_MEDIA_AUDIO_CONFIG:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        AudioConfig stAudioConfig = pstMediaConfig->audioConfig;
        iRet = anj_config_audio_get_by_xml(&stAudioConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_audio_set(&stAudioConfig);
        }
        break;
    }
    case CMD_SET_MEDIA_AUDIO_CAPTURE:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        AudioCapture stAudioCapture = pstMediaConfig->audioConfig.audioCapture;
        iRet = anj_config_audio_capture_get_by_xml(&stAudioCapture, data);
        if (iRet == 0)
        {
            iRet = anj_config_audio_capture_set(&stAudioCapture);
        }
        break;
    }
    case CMD_SET_MEDIA_AUDIO_ENCODE:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        AudioEncode stAudioEncode = pstMediaConfig->audioConfig.audioEncode;
        iRet = anj_config_audio_encode_get_by_xml(&stAudioEncode, data);
        if (iRet == 0)
        {
            iRet = anj_config_audio_encode_set(&stAudioEncode);
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_ENCODE:
    {
        if (anj_video_restart_is_busy())
        {
            __ERR("video restart is running, reject CMD_SET_MEDIA_VIDEO_ENCODE\n");
            iRet = -1;
            break;
        }

        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        int iNeedSwitch = 0;
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }

            VideoEncode stVideoEncode = pstMediaConfig->videoConfig[cameraIndex].videoEncode;
            iRet = anj_config_video_encode_get_by_xml(&stVideoEncode, data);
            if (iRet == 0)
            {
                iNeedSwitch |= anj_config_video_encode_set(&stVideoEncode, cameraIndex);
            }
        }
        if (iNeedSwitch)
        {
            anj_video_encode_switch();
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_JPEG:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }

            JpegEncodeCfg stJpegEncodeCfg = pstMediaConfig->videoConfig[cameraIndex].jpegCfg;
            iRet = anj_config_jpeg_encode_get_by_xml(&stJpegEncodeCfg, data);
            if (iRet == 0)
            {
                iRet = anj_config_jpeg_encode_set(&stJpegEncodeCfg, cameraIndex);
            }
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_CAPTURE:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;
            iRet = anj_config_video_capture_get_by_xml(&stVideoCapture, data, MsgSrc);
            if (iRet == 0)
            {
                iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
            }
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_OSD:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            VideoOverlay stVideoOverlay = pstMediaConfig->videoConfig[cameraIndex].overlay;
            iRet = anj_config_overlay_get_by_xml(&stVideoOverlay, data);
            if (iRet == 0)
            {
                iRet = anj_config_overlay_set(&stVideoOverlay, cameraIndex);
            }
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_USEROSD:
    case CMD_SET_USEROSD_TO_ENCODE:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            VideoUserOverlay stVideoUserOverlay = pstMediaConfig->videoConfig[cameraIndex].useroverlay;
            iRet = anj_config_user_overlay_get_by_xml(&stVideoUserOverlay, data);
            if (iRet == 0)
            {
                iRet = anj_config_user_overlay_set(&stVideoUserOverlay, cameraIndex);
            }
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_MASK:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }

            VideoMaskConfig stVideoMaskConfig = pstMediaConfig->videoConfig[cameraIndex].videoMask;
            iRet = anj_config_video_mask_get_by_xml(&stVideoMaskConfig, data);
            if (iRet == 0)
            {
                iRet = anj_config_video_mask_set(&stVideoMaskConfig, cameraIndex);
            }
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_ROI:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            VideoROI stVideoRoi = pstMediaConfig->videoConfig[cameraIndex].roiCfg;
            iRet = anj_config_video_roi_get_by_xml(&stVideoRoi, data);
            if (iRet == 0)
            {
                iRet = anj_config_video_roi_set(&stVideoRoi, cameraIndex);
            }
        }
        break;
    }
    case CMD_SET_MEDIA_VIDEO_YUV:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            YuvEncodeCfg stYuvEncodeCfg = pstMediaConfig->videoConfig[cameraIndex].yuvCfg;
            iRet = anj_config_video_yuv_get_by_xml(&stYuvEncodeCfg, data);
            if (iRet == 0)
            {
                iRet = anj_config_video_yuv_set(&stYuvEncodeCfg, cameraIndex);
            }
        }
        break;
    }
    case CMD_SET_MEDIASTREAM_CONFIG:
    {
        MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
        MediaStreamConfig stMediaStreamConfig = *pstMediaStreamConfig;
        iRet = anj_config_stream_get_by_xml(&stMediaStreamConfig, data, 0);
        if (iRet == 0)
        {
            iRet = anj_config_stream_set(&stMediaStreamConfig);
        }
        break;
    }
    case CMD_SET_PLATFORM_CONFIG:
    {
        PlatformConfig *pstPlatformConfig = (PlatformConfig *)getPlatformConfig();
        PlatformConfig stPlatformConfig = *pstPlatformConfig;
        iRet = anj_config_platform_get_by_xml(&stPlatformConfig, data);
        if (iRet == 0)
        {
            iRet = anj_config_platform_set(&stPlatformConfig);
        }
        break;
    }
    case CMD_SET_GB28181_CONFIG:
    {
        GB28181Config *pstGb28181Config = (GB28181Config *)getGb28181Config();
        GB28181Config stGb28181Config = *pstGb28181Config;
        iRet = anj_config_gb28181_get_by_xml(&stGb28181Config, data);
        if (iRet == 0)
        {
            iRet = anj_config_gb28181_set(&stGb28181Config);
        }
        break;
    }
    case CMD_SET_GAT1400_CONFIG:
    {
        GAT1400Config *pstGat1400Config = (GAT1400Config *)getGat1400Config();
        GAT1400Config stGat1400Config = *pstGat1400Config;
        iRet = anj_config_gat1400_get_by_xml(&stGat1400Config, data);
        if (iRet == 0)
        {
            iRet = anj_config_gat1400_set(&stGat1400Config);
        }
        break;
    }
    case CMD_SET_RECORD_CONFIG:
    {
        RecordConfig *pstRecordConfig = (RecordConfig *)getRecordConfig();
        RecordConfig stRecordConfigArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stRecordConfigArray, pstRecordConfig, sizeof(RecordConfig) * ANJ_CAMERA_MAX_NUMS);
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet |= anj_config_record_get_by_xml(&stRecordConfigArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_record_set(stRecordConfigArray);
        }
        break;
    }
    case CMD_SET_ALARM_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        AlarmConfig stAlarmConfig = *pstAlarmConfig;
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_get_by_xml(&stAlarmConfig, data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_set(&stAlarmConfig);
        }
        break;
    }
    case CMD_SET_ALARM_INPUT_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        InputAlarm stInputAlarm = pstAlarmConfig->normalAlarm.inputAlarm;
        iRet = anj_config_alarm_input_get_by_xml(&stInputAlarm, data);
        if (iRet == 0)
        {
            iRet = anj_config_alarm_input_set(&stInputAlarm);
        }
        break;
    }
    case CMD_SET_ALARM_MOTIONDETECT_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        MotionDetectAlarm stMotionDetectAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stMotionDetectAlarmArray, pstAlarmConfig->normalAlarm.motionDetectAlarm,
               sizeof(MotionDetectAlarm) * ANJ_CAMERA_MAX_NUMS);
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_motion_get_by_xml(&stMotionDetectAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_motion_set(stMotionDetectAlarmArray);
        }
        break;
    }
    case CMD_SET_ALARM_VIDEOCOVER_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        VideoCoverAlarm stVideoCoverAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stVideoCoverAlarmArray, pstAlarmConfig->normalAlarm.videoCoverAlarm,
               sizeof(VideoCoverAlarm) * ANJ_CAMERA_MAX_NUMS);
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_video_cover_get_by_xml(&stVideoCoverAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_video_cover_set(stVideoCoverAlarmArray);
        }
        break;
    }
    case CMD_SET_ALARM_VIDEOLOST_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        VideoLostAlarm stVideoLostAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stVideoLostAlarmArray, pstAlarmConfig->normalAlarm.videoLostAlarm,
               sizeof(VideoLostAlarm) * ANJ_CAMERA_MAX_NUMS);

        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_video_lost_get_by_xml(&stVideoLostAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_video_lost_set(stVideoLostAlarmArray);
        }
        break;
    }
    case CMD_SET_ALARM_SF_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        StorageFullAlarm stStorageFullAlarm = pstAlarmConfig->normalAlarm.storageFullAlarm;
        iRet = anj_config_alarm_storage_full_get_by_xml(&stStorageFullAlarm, data);
        if (iRet == 0)
        {
            iRet = anj_config_alarm_storage_full_set(&stStorageFullAlarm);
        }
        break;
    }
    case CMD_SET_ALARM_AUDIO_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        AudioAlarm stAudioAlarm = pstAlarmConfig->aiAlarm.audioAlarm;
        iRet = anj_config_alarm_audio_get_by_xml(&stAudioAlarm, data);
        if (iRet == 0)
        {
            iRet = anj_config_alarm_audio_set(&stAudioAlarm);
        }
        break;
    }
    case CMD_SET_ALARM_VIDEO_GATE:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stVideoGateAlarmArray, pstAlarmConfig->aiAlarm.vgAlarm, sizeof(VideoGateAlarm) * ANJ_CAMERA_MAX_NUMS);

        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_video_gate_get_by_xml(&stVideoGateAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
        }
        break;
    }
    case CMD_SET_ALARM_PD:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stPdAlarmArray, pstAlarmConfig->aiAlarm.pdAlarm, sizeof(PdAlarm) * ANJ_CAMERA_MAX_NUMS);

        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_pd_get_by_xml(&stPdAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_pd_set(stPdAlarmArray);
        }
        break;
    }
    case CMD_SET_ALARM_REGION_AI:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        VideoRegionAiAlarm stVideoRegionAiAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stVideoRegionAiAlarmArray, pstAlarmConfig->aiAlarm.regionAiAlarm,
               sizeof(VideoRegionAiAlarm) * ANJ_CAMERA_MAX_NUMS);

        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_region_get_by_xml(&stVideoRegionAiAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_region_set(stVideoRegionAiAlarmArray);
        }
        break;
    }
    case CMD_SET_ALARM_LPR:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        LprAlarm stLprAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stLprAlarmArray, pstAlarmConfig->aiAlarm.lprAlarm,
               sizeof(LprAlarm) * ANJ_CAMERA_MAX_NUMS);
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_lpr_get_by_xml(&stLprAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_lpr_set(stLprAlarmArray);
        }
        break;
    }
    case CMD_SET_ALARM_FD:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        FaceDetectAlarm stFdAlarmArray[ANJ_CAMERA_MAX_NUMS];
        memcpy(stFdAlarmArray, pstAlarmConfig->aiAlarm.fdAlarm,
               sizeof(FaceDetectAlarm) * ANJ_CAMERA_MAX_NUMS);
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            if (channel >= 0 && channel <= ANJ_CAMERA_MAX_NUMS)
            {
                if (cameraIndex != channel)
                    continue;
            }
            iRet = anj_config_alarm_fd_get_by_xml(&stFdAlarmArray[cameraIndex], data, cameraIndex);
        }
        if (iRet == 0)
        {
            iRet = anj_config_alarm_fd_set(stFdAlarmArray);
        }
        break;
    }
    case CMD_SET_TEMP_HUMIDITY:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        TempHumidityAlarm stHumidityAlarm = pstAlarmConfig->normalAlarm.temphumidityAlarm;
        iRet = anj_config_alarm_temp_humidity_get_by_xml(&stHumidityAlarm, data);
        if (iRet == 0)
        {
            iRet = anj_config_alarm_temphumidity_set(&stHumidityAlarm);
        }
        break;
    }
    case CMD_SET_ALARM_INPUT_CHANNEL_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        AlarmChannel stAlarmChannel = {0};
        iRet = anj_config_alarm_input_channel_get_by_xml(&stAlarmChannel, data);
        if (iRet == 0)
        {
            int i = 0;
            for (i = 0; i < pstAlarmConfig->normalAlarm.inputAlarm.channelCnt; i++)
            {
                if (pstAlarmConfig->normalAlarm.inputAlarm.alarmChannels[i].portIndex == stAlarmChannel.portIndex)
                {
                    iRet = anj_config_alarm_input_channel_set(&stAlarmChannel, i);
                    break;
                }
            }
        }
        break;
    }
    case CMD_SET_ALARM_OUTPUT_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        OutPutAlarm stOutPutAlarm = pstAlarmConfig->normalAlarm.outputAlarm;
        iRet = anj_config_alarm_output_get_by_xml(&stOutPutAlarm, data);
        if (iRet == 0)
        {
            iRet = anj_config_alarm_output_set(&stOutPutAlarm);
        }
        break;
    }
    case CMD_SET_ALARM_OUTPUT_CHANNEL_CONFIG:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        OutputChannel stOutputChannel = {0};
        iRet = anj_config_alarm_output_channel_get_by_xml(&stOutputChannel, data);
        if (iRet == 0)
        {
            int i = 0;
            for (i = 0; i < pstAlarmConfig->normalAlarm.outputAlarm.channelCnt; i++)
            {
                if (pstAlarmConfig->normalAlarm.outputAlarm.outputChannels[i].portIndex == stOutputChannel.portIndex)
                {
                    iRet = anj_config_alarm_output_channel_set(&stOutputChannel, i);
                    break;
                }
            }
        }
        break;
    }
    case CMD_SET_ALARM_SMS:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        SMSAlarm stSMSAlarm = pstAlarmConfig->normalAlarm.smsAlarm;
        iRet = anj_config_alarm_sms_get_by_xml(&stSMSAlarm, data);
        if (iRet == 0)
        {
            iRet = anj_config_alarm_sms_set(&stSMSAlarm);
        }
        break;
    }
    case CMD_SET_ALARM_FLAMEANDFLUMES:
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        FlameAndFlumesAlarm stFireAlarm = pstAlarmConfig->aiAlarm.fireAlarm;
        iRet = anj_config_alarm_fire_get_by_xml(&stFireAlarm, data);
        if (iRet == 0)
        {
            iRet = anj_config_alarm_fire_set(&stFireAlarm);
        }
        break;
    }
    default:
    {
        break;
    }
    }
    return iRet;
}

char *anj_service_task_media(char *cmdbuf, int cmdlen, IXML_Document *pDoc,
                             char *MsgCode, char *MsgRoot, int MsgSrc)
{
    char *pBuffer = NULL;
    int payloadlen = 0;
    int dataerror = 0;

    int msgcode = atoi(MsgCode);
    char *payload = cmdbuf + strlen(cmdbuf);

    //__ERR("%x, %x, %x, %x, %x\n", *(payload - 1), *payload, *(payload + 1), *(payload + 2), *(payload + 3));

    if (*payload == 0 && *(payload + 1) == 0 && *(payload + 2) == 0 && *(payload + 3) == 0)
    {
        payload += 4; // skip 4 zero
    }
    else
    {
        __ERR("data format error !!\n");
        dataerror = 1;
    }

    if (!dataerror)
    {
        char *length_param = anj_config_pos_value_get(pDoc, (char *)"DataLen");
        if (length_param == NULL)
        {
            __ERR("no DataLen field found!!!\n");
        }
        else
        {
            payloadlen = atoi(length_param);
            anj_mw_free(length_param);

            if (payloadlen < 0) // client want to stop transport
            {
                __ERR("got command to stop transport, msgcode = %d\n", msgcode);
                file_recver_uninit(1);
            }
            else if (MsgSrc == MSG_SRC_PRI && (payload - cmdbuf + payloadlen != cmdlen))
            {
                __ERR("datalen error, payloadlen = %d, cmdlen = %d, payload - cmdbuf = %d, payload = %s, totallen should be %d\n",
                      payloadlen,
                      cmdlen,
                      payload - cmdbuf,
                      payload,
                      payload - cmdbuf + payloadlen);

                dataerror = 1;
            }

            if (!dataerror)
            {
                if (msgcode == EVENT_UPLOAD) // upload data
                {
                    dataerror = file_recver_proc(dataerror, payload, payloadlen, MsgSrc);
                    file_recver_t *pFileReceiver = getFileRecver();
                    if (pFileReceiver && (payloadlen == 0))
                    {
                        pBuffer = anj_mw_malloc(1024);
                        if (pBuffer == NULL)
                        {
                            __ERR("pBuffer == NULL while handle file transport!!!\n");
                        }
                        else
                        {
                            if (pFileReceiver->filetype == UPLOAD_CONFIG_FILE_TYPE)
                            {
                                snprintf(pBuffer, 1024,
                                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                                         "<%s>\n"
                                         "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                                         "/>\n"
                                         "<MESSAGE_BODY></MESSAGE_BODY>\n"
                                         "</%s>",
                                         MsgRoot, CMD_CONFIG_UPDATE, dataerror, MsgRoot);
                            }
                            else if (pFileReceiver->filetype == UPLOAD_FIRMWARE_FILE_TYPE)
                            {
                                snprintf(pBuffer, 1024,
                                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                                         "<%s>\n"
                                         "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                                         "/>\n"
                                         "<MESSAGE_BODY></MESSAGE_BODY>\n"
                                         "</%s>",
                                         MsgRoot, CMD_APPBIN_LOCAL_UPDATE, dataerror, MsgRoot);
                            }
                            file_recver_uninit(0);
                        }
                    }
                }
                else if (msgcode == EVENT_TALKBACK) // audio talkback
                {
                    if (!dataerror)
                    {
                        MEDIA_DATA_HEADER *pMediaDataHeader = (MEDIA_DATA_HEADER *)payload;

                        audio_talk_feed_packet(payload + sizeof(MEDIA_DATA_HEADER),
                                               (int)pMediaDataHeader->payload_size);
                    }
                }
            }
        }
    }

    return pBuffer;
}

int anj_service_task_sysctl(IXML_Document *pDoc, char *cmdbuf, int cmdlen, char *MsgType, char *MsgCode,
                            int MsgSrc, int lognum, int channel, char **data)
{
    int iRet = 0;
    int msg_code = atoi(MsgCode);

    char *msg_body = NULL;
    char *request_param = NULL;

    __INFO("pri cmd task sysctl msgcode:%d\n", msg_code);
    switch (msg_code)
    {
    case CMD_PTZ_SPEED_RESET:
    {
        char *pParam = GetRequestParamValue(pDoc, (char *)"manualspeed");
        if (pParam == NULL)
        {
            iRet = -1;
        }
        else
        {
            IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
            IotPtzConfig stIotPtzConfig = *pstIotPtzConfig;
            stIotPtzConfig.m_ptzSpeed.HSpeed = atoi(pParam);
            anj_ptz_config_save(&stIotPtzConfig);
            iRet = 0;
        }

        if (pParam)
        {
            anj_mw_free(pParam);
        }
    }
    break;
    case CMD_TEST_SMTP:
    {
        ServerConfig stServerConfig = {0};

        iRet = anj_config_server_smtp_get_by_xml(&stServerConfig, cmdbuf);
        if (iRet != 0)
        {
            __ERR("Get SMTP params error.\n");
        }
        else
        {
            iRet = anj_ftpemail_smtp_test(&stServerConfig.smtpServers, SMTP_INDEX_FOR_ALARM_UPLOAD, NULL);
            msg_body = anj_mw_malloc(512);
            if (msg_body != NULL)
            {
                sprintf(msg_body, "<RESPONSE_PARAM  Status=\"%d\" />", iRet);
            }
        }
    }
    break;
    case CMD_TEST_FTP:
    {
        ServerConfig stServerConfig = {0};

        iRet = anj_config_server_ftp_get_by_xml(&stServerConfig, cmdbuf);
        if (iRet != 0)
        {
            __ERR("Get FTP params error.\n");
        }
        else
        {
            FtpServer *pstFtpCfg = anj_config_server_ftp_get_by_id(&stServerConfig, FTP_INDEX_FOR_ALARM_UPLOAD);
            if (pstFtpCfg == NULL)
            {
                __ERR("Get FTP server by id failed.\n");
                iRet = 1;
            }
            else
            {
                char IPAddress[MAX_IP_NAME_LEN] = {0};

                anj_net_ip_get(IPAddress, sizeof(IPAddress));

                __INFO("ftp test: server ip:%s, port:%d, username:%s, ipaddress:%s\n",
                       pstFtpCfg->serverIP, pstFtpCfg->serverPort,
                       pstFtpCfg->userName, IPAddress);

                iRet = anj_ftpemail_ftp_test(pstFtpCfg, IPAddress);
            }

            msg_body = anj_mw_malloc(512);
            if (msg_body != NULL)
            {
                sprintf(msg_body, "<RESPONSE_PARAM  Status=\"%d\" />", iRet);
            }
        }
    }
    break;
    case CMD_SET_PLAYAUDIO_START:
    {
        char *filename = GetRequestParamValue(pDoc, (char *)"FileName");
        char *times = GetRequestParamValue(pDoc, (char *)"PlayTimes");

        if (filename && times)
        {
            iRet = 0;

            int play_times = atoi(times);
            audioplay_info audio_info = {0};
            audio_info.playtimes = play_times;
            snprintf(audio_info.pAudioFile, sizeof(audio_info.pAudioFile), "%s", filename);
            audio_info.playaction = AUDIO_PLAY_ACTION_WAIT_PREV;
            audio_info.priority = AUDIO_PLAY_PRIORITY_MAX;
            audio_info.encodeType = MEDIA_CODEC_AUDIO_MP3;
            iRet = anj_audio_play_file(&audio_info);
        }
        else
        {
            iRet = -1;
        }

        if (filename)
            anj_mw_free(filename);
        if (times)
            anj_mw_free(times);
    }
    break;
    case CMD_SET_PLAYAUDIO_STOP:
    {
        audioplay_info audio_info = {0};
        anj_audio_play_file(&audio_info);

        iRet = 0;
    }
    break;
    case CMD_GET_IO_INPUT_STATUS:
    {
        char *channelno = GetRequestParamValue(pDoc, (char *)"ChannelNo");
        if (channelno == NULL)
        {
            iRet = -1;
        }
        else
        {
            int chn = atoi(channelno);
            int status = 0;
            /* App 下发 0~3，与 CMD_SET_IO_OUTPUT 一致，转成 1~4 */
            chn = chn + 1;
            status = anj_mw_hwctrl_alarmin_chn_status_get(chn);

            msg_body = anj_mw_malloc(1024);
            sprintf(msg_body, "<RESPONSE_PARAM  ChannelNo=\"%s\" Status=\"%d\" />", channelno, status);
            anj_mw_free(channelno);

            iRet = 0;
        }
    }
    break;
    case CMD_GET_IO_OUTPUT_STATUS:
    {
        char *channelno = GetRequestParamValue(pDoc, (char *)"ChannelNo");
        if (channelno == NULL)
        {
            iRet = -1;
        }
        else
        {
            int status = 0;
            int chn = atoi(channelno);
            chn = chn + 1;
            status = anj_mw_hwctrl_alarmout_chn_status_get(chn);

            msg_body = anj_mw_malloc(1024);
            sprintf(msg_body, "<RESPONSE_PARAM  ChannelNo=\"%s\" Status=\"%d\" />", channelno, status);
            anj_mw_free(channelno);

            iRet = 0;
        }
    }
    break;
    case CMD_GET_AUDIO_STATUS:
    {
        int status = 0;
        status = anj_audio_ao_play_file_status_get();
        iRet = 0;
        msg_body = anj_mw_malloc(1024);
        sprintf(msg_body, "<RESPONSE_PARAM  Status=\"%d\" />", status);
    }
    break;
    case CMD_GET_PTZ_CAPABILITY:
    {
        __ERR("Got CMD_GET_PTZ_CAPABILITY command\n");

        msg_body = anj_mw_malloc(1024);
        strcpy(msg_body, "<RESPONSE_PARAM>\n<PTZ_PROTOCOL Name=\"PECO-P\" Type=\"PECO-P\">\n<PTZ_PROTOCOL Name=\"PECO-D\" Type=\"PECO-D\">\n</RESPONSE_PARAM>");
        iRet = 0;
    }
    break;
    case CMD_GET_SYSTEM_TIME:
    {
        __ERR("Got CMD_GET_IPC_DATETIME command\n");
        struct timeval tv;
        SystemGetTimeofRun(&tv, NULL);
        struct tm ptm;
        SystemLocalTime(&ptm);
        int timezone = GetTimeZoneBySystem();
        msg_body = anj_mw_malloc(1024);
        sprintf(msg_body, "<RESPONSE_PARAM TimeZone=\"%d\" Year=\"%d\" Month=\"%d\" Day=\"%d\" Hour=\"%d\" Minute=\"%d\" Second=\"%d\" />",
                timezone, // tz.tz_minuteswest/60,
                ptm.tm_year + 1900,
                ptm.tm_mon + 1,
                ptm.tm_mday,
                ptm.tm_hour,
                ptm.tm_min,
                ptm.tm_sec);

        iRet = 0;
    }
    break;
    case CMD_IRCUT_CONTROL:
    {
        __ERR("Got CMD_IRCUT_CONTROL command\n");

        char *modeStr = GetRequestParamValue(pDoc, (char *)"Mode");

        if (modeStr == NULL)
        {
            iRet = -1;
        }
        else
        {
            if (!strcmp(modeStr, "DayMode"))
                anj_ispctl_ircut_manual_ctrl(1, 0);
            else if (!strcmp(modeStr, "NightMode"))
                anj_ispctl_ircut_manual_ctrl(0, 0);

            iRet = 0;
            anj_mw_free(modeStr);
        }
    }
    break;
    case CMD_SET_KEYFRAME:
    {
        char *camIndex = GetRequestParamValue(pDoc, (char *)"Camera");
        char *stmIndex = GetRequestParamValue(pDoc, (char *)"Stream");

        if (camIndex == NULL || stmIndex == NULL)
        {
            iRet = -1;
        }
        else
        {
            iRet = 0;
            anj_video_request_idr(atoi(camIndex), atoi(stmIndex));
            anj_mw_free(camIndex);
            anj_mw_free(stmIndex);
        }
    }
    break;
    case CMD_SET_USER_DEV_DATA:
    {
        char *userData = GetRequestParamValue(pDoc, (char *)"UserDevData");
        if (userData)
        {
            if (anj_mw_create_file(USER_DATA_BIN_PATH, userData) != 0)
            {
                __ERR("create %s failed! err = %s\n", USER_DATA_BIN_PATH, strerror(errno));
                iRet = -2;
            }

            anj_mw_free(userData);
        }
        else
        {
            iRet = -1;
        }
    }
    break;
    case CMD_GET_USER_DEV_DATA:
    {
        char *userdata = anj_mw_read_file_buffer(USER_DATA_BIN_PATH);
        if (userdata)
        {
            if (strlen(userdata) > 0)
            {
                msg_body = anj_mw_malloc(2048);
                sprintf(msg_body, "<RESPONSE_PARAM\nUserDevData=\"%s\"/>\n", userdata);
                iRet = 0;
            }
            else
            {
                __ERR("read file %s failed, err = %s\n", USER_DATA_BIN_PATH, strerror(errno));
                iRet = -2;
            }
            anj_mw_free(userdata);
        }
        else
        {
            __ERR("open %s failed, err = %s\n", USER_DATA_BIN_PATH, strerror(errno));
            iRet = -1;
        }
    }
    break;
    case CMD_GET_MEDIA_CAPABILITY:
    {
        char vidCap[4096] = {0};
        char audCap[256] = {0};
        char yuvCap[512] = {0};

        msg_body = anj_mw_malloc(sizeof(vidCap) + sizeof(audCap) + sizeof(yuvCap) + 128);

        anj_sysmng_videolist_get(vidCap, sizeof(vidCap));
        anj_sysmng_audiolist_get(audCap, sizeof(audCap));
        anj_sysmng_yuvlist_get(yuvCap, sizeof(yuvCap));

        sprintf(msg_body,
                "<RESPONSE_PARAM\n><VideoCap CapList=\"%s\"/>\n<AudioCap CapList=\"%s\"/>\n<YuvCap CapList=\"%s\"/>\n</RESPONSE_PARAM>\n",
                vidCap, audCap, yuvCap);
    }
    break;
    case CMD_GET_IPC_TYPE:
    {
        // 101: 720P
        // 102: VGA
        // 100: d1

        // other platform
        // 1: 720P
        // 2: VGA
        // 0: d1
        msg_body = anj_mw_malloc(1024);
        strcpy(msg_body, "<RESPONSE_PARAM\nIPC_Type=\"101\"\n/>");
    }
    break;
    case CMD_CONFIG_UPDATE:
    {
        char *filePath = GetRequestParamValue(pDoc, (char *)"FilePath");
        if (filePath != NULL && strlen(filePath) > 0)
        {
            __INFO("config update file path = %s\n", filePath);
            iRet = anj_sysmng_config_update(filePath);
            anj_mw_free(filePath);
        }
        else
        {
            if (filePath)
                anj_mw_free(filePath);
            iRet = 0;
        }
    }
    break;
    case CMD_CONFIG_BACKUP:
    {
        char way[256] = {0};
        char *pWay = GetRequestParamValue(pDoc, (char *)"Way");

        if (pWay == NULL)
            pWay = GetRequestParamValue(pDoc, (char *)"way");

        if (pWay != NULL)
        {
            strncpy(way, pWay, sizeof(way) - 1);
            anj_mw_free(pWay);
        }
        else if (cmdbuf != NULL && cmdlen > (int)strlen(cmdbuf))
        {
            const char *payload = cmdbuf + strlen(cmdbuf) + 1;
            int payload_len = cmdlen - (int)strlen(cmdbuf) - 1;

            if (payload_len > 0)
            {
                int copy_len = payload_len < (int)sizeof(way) - 1 ? payload_len : (int)sizeof(way) - 1;
                memcpy(way, payload, copy_len);
                way[copy_len] = '\0';
            }
        }

        if (strlen(way) == 0)
        {
            __ERR("config backup way is empty\n");
            iRet = -1;
            break;
        }

        __INFO("config backup start, way=%s\n", way);

        char infomation[ANJ_FTPEMAIL_INFO_MAX_LEN] = {0};
        char sn_str[32] = {0};
        char szValue[TITLE_MAX_LEN] = {0};
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        ServerConfig *pstServerConfig = (ServerConfig *)getServerConfig();

        anj_sysmng_load_sn(sn_str, sizeof(sn_str));
        utf8_to_gb2312(pstMediaConfig->videoConfig[0].overlay.titleOverlay.title_utf8, szValue);
        snprintf(infomation, sizeof(infomation), "[%s][%s]", szValue, sn_str);

        struct timeval tv;
        struct tm ptm;
        SystemGetTimeofRun(&tv, NULL);
        SystemLocalTime(&ptm);

        char fileName[256] = {0};
        const char *localPath = "/tmp";
        snprintf(fileName, sizeof(fileName), "%04d%02d%02d%02d%02d%02d_%s",
                 ptm.tm_year + 1900, ptm.tm_mon + 1, ptm.tm_mday,
                 ptm.tm_hour, ptm.tm_min, ptm.tm_sec, "config.xml");

        char fullName[512] = {0};
        snprintf(fullName, sizeof(fullName), "%s/%s", localPath, fileName);

        if (anj_mw_file_copy(CONFIG_FILE_PATH, fullName) != 0)
        {
            __ERR("config backup copy %s failed\n", CONFIG_FILE_PATH);
            iRet = -1;
            break;
        }

        if (strcmp(way, "ftp") == 0)
        {
            iRet = anj_ftpemail_ftp_file(&pstServerConfig->ftpServers[FTP_INDEX_FOR_CONFIG_BACKUP],
                                         localPath, fileName,
                                         pstServerConfig->ftpServers[FTP_INDEX_FOR_CONFIG_BACKUP].filePath,
                                         fileName);
        }
        else if (strcmp(way, "email") == 0)
        {
            iRet = anj_ftpemail_email_file(&pstServerConfig->smtpServers, SMTP_INDEX_FOR_CONFIG_BACKUP,
                                           localPath, fileName, infomation);
        }
        else
        {
            __ERR("unknown config backup way: %s\n", way);
            remove(fullName);
            iRet = -1;
        }
    }
    break;
    case CMD_CONFIG_RESTORE:
    {
        unsigned int reserved_bits = 0;
        char *pRetain = GetRequestParamValue(pDoc, (char *)"retain");
        if (pRetain != NULL)
        {
            if (strstr(pRetain, "network") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_NETWORK);
            if (strstr(pRetain, "language") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_LANGUAGE);
            if (strstr(pRetain, "time") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_TIME);
            if (strstr(pRetain, "usercfg") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_USER);
            if (strstr(pRetain, "mediacode") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_MEDIACODE);
            if (strstr(pRetain, "ptzcfg") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_PTZ);
            if (strstr(pRetain, "streamaccess") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_STREAMACCESS);
            if (strstr(pRetain, "record") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_RECORD);
            if (strstr(pRetain, "gb28181") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_GB28181);
            if (strstr(pRetain, "alarm") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_ALARM);
            if (strstr(pRetain, "title") != NULL)
                BIT_SET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_TITLE);

            __ERR("reserved_bits=%u, %s\n", reserved_bits, pRetain);
            anj_mw_free(pRetain);
        }
        __RECORD_LOG_INFO("restore config reserved_bits=%u\n", reserved_bits); 
        iRet = anj_sysmng_config_restore(reserved_bits);
    }
    break;
    case CMD_SET_MODE_FACTORY:
    {
        char *msgflag = GetRequestParamValue(pDoc, (char *)"Mode");
        if (msgflag)
        {
            // todo
            // anj_factory_mode_set(atoi(msgflag));
            anj_mw_free(msgflag);
        }
        iRet = 0;
    }
    break;
    case CMD_GET_PTZ_MAX_STEP:
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        msg_body = anj_mw_malloc(256);
        memset(msg_body, 0, 256);

        int maxHstep = pstIotPtzConfig->m_MaxStep.HStep;
        int maxVstep = pstIotPtzConfig->m_MaxStep.VStep;
        __INFO("CMD_GET_PTZ_MAX_STEP:[%d, %d] %d", maxHstep, maxVstep, strlen(msg_body));
        sprintf(msg_body, "<RESPONSE_PARAM maxlstep=\"%d\" maxvstep=\"%d\" />", maxHstep, maxVstep);
        iRet = 0;
    }
    break;
    case CMD_SET_PTZ_CALIB_MODE:
    {
        int mode = 0;

        char *pStr = GetRequestParamValue(pDoc, (char *)"ptz_calib_mode");
        if (pStr != NULL)
        {
            mode = atoi(pStr);
            __INFO("ptz_calib_mode: %s, %d\n", pStr, mode);

            anj_mw_free(pStr);
        }

        if (mode == 1)
        {
            anj_mw_create_file("/tmp/motor_test_mode", NULL);
        }
        else if (mode == 2)
        {
        }
        else
        {
            IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
            IotPtzConfig stIotPtzConfig = *pstIotPtzConfig;
            stIotPtzConfig.m_MaxStep.HStep = g_ptz_test_lstep;
            stIotPtzConfig.m_MaxStep.VStep = g_ptz_test_vstep;
            anj_ptz_config_save(&stIotPtzConfig);
        }
    }
    break;
    case CMD_SET_PTZ_TEST_LSTEP:
    {
        char *pStr = GetRequestParamValue(pDoc, (char *)"ptz_test_lstep");
        if (pStr != NULL)
        {
            g_ptz_test_lstep = atoi(pStr);
            __INFO("ptz_test_lstep: %s, %d\n", pStr, g_ptz_test_lstep);
            anj_mw_free(pStr);
        }
    }
    break;
    case CMD_SET_PTZ_TEST_VSTEP:
    {
        char *pStr = GetRequestParamValue(pDoc, (char *)"ptz_test_vstep");
        if (pStr != NULL)
        {
            g_ptz_test_vstep = atoi(pStr);
            __INFO("ptz_test_vstep: %s, %d\n", pStr, g_ptz_test_vstep);
            anj_mw_free(pStr);
        }
    }
    break;
    case CMD_SYSTEM_REBOOT:
        __WARN("system reboot service request from http\n");
        anj_sysmng_delay_reboot(1);
        break;
    case CMD_Eraseall_MP3:
        iRet = anj_sysmng_eraseall_mp3();
        break;
    case CMD_SET_SYSTEM_TIME:
    {
        char *now_time = GetRequestParamValue(pDoc, (char *)"Time");
        char *time_zone = GetRequestParamValue(pDoc, (char *)"TimeZone");
        if ((now_time == NULL) || (time_zone == NULL))
        {
            if (now_time != NULL)
                anj_mw_free(now_time);
            if (time_zone != NULL)
                anj_mw_free(time_zone);

            iRet = -1;
        }
        else
        {
            int tz = atoi(time_zone);
            struct tm time;
            if (GetTimeFromString(now_time, &time) < 0)
            {
                iRet = -2;
            }
            else
            {
                __INFO("time=%04d%02d%02d %02d:%02d:%02d, tz=%d\n",
                      time.tm_year,
                      time.tm_mon,
                      time.tm_mday,
                      time.tm_hour,
                      time.tm_min,
                      time.tm_sec, tz);

                iRet = anj_systime_set_time_and_zone(time, tz, 1);
            }

            anj_mw_free(now_time);
            anj_mw_free(time_zone);
        }
    }
    break;
    case CMD_SET_SYSTEM_TIME_EX:
    {
        char *tv_sec = GetRequestParamValue(pDoc, (char *)"tv_sec");
        char *tv_usec = GetRequestParamValue(pDoc, (char *)"tv_usec");
        if ((tv_sec == NULL) || (tv_usec == NULL))
        {
            if (tv_sec != NULL)
                anj_mw_free(tv_sec);
            if (tv_usec != NULL)
                anj_mw_free(tv_usec);

            iRet = -1;
        }
        else
        {
            struct timeval tv;
            tv.tv_sec = (time_t)atoi(tv_sec);
            tv.tv_usec = (unsigned int)atoi(tv_usec);

            __ERR("time=%lld:%u\n", (long long)tv.tv_sec, (unsigned int)tv.tv_usec);

            iRet = anj_systime_set_ex(tv);

            anj_mw_free(tv_sec);
            anj_mw_free(tv_usec);
        }
    }
    break;
    case CMD_RECORD_START:
        break;
    case CMD_RECORD_STOP:
        break;
    case CMD_CREATE_TIMELAPSE:
        anj_service_export_record(cmdbuf, cmdlen, lognum, pDoc, MsgType, MsgCode, 1);
        break;
    case CMD_CREATE_EXPORT_RECORD:
        anj_service_export_record(cmdbuf, cmdlen, lognum, pDoc, MsgType, MsgCode, 0);
        break;
    case CMD_SET_PARTNER_NAME:
        iRet = anj_service_partner_proc(cmdbuf, cmdlen, lognum, pDoc, MsgType, MsgCode);
        break;
    case CMD_GET_PARTNER_NAME:
    {
        char szPartner[128] = {0}, szDatestr[128] = {0}, szMacaddr[128] = {0};
        anj_sysmng_partner_info_get(szPartner, szDatestr, szMacaddr);
        msg_body = anj_mw_malloc(2048);
        sprintf(msg_body,
                "<RESPONSE_PARAM parter=\"%s\" datestr=\"%s\" macaddr=\"%s\" />",
                szPartner, szDatestr, szMacaddr);
    }
    break;
    case CMD_STORAGE_DEVICE_FORMAT:
        request_param = GetRequestParamValue(pDoc, "DevName");
        if (request_param != NULL)
        {
            __INFO("got message to format: %s\n", request_param);

            anj_sdcard_format(lognum);
            anj_mw_free(request_param);
            iRet = CMD_UNSUPPORT_RESPONSE;
        }
        else
        {
            iRet = -1;
        }
        break;
    case CMD_STORAGE_DEVICE_UNMOUNT:
        anj_sdcard_umount();
        break;
    case CMD_STORAGE_DEVICE_REMOUNT:
        anj_sdcard_umount();
        anj_sdcard_mount();
        break;
    case CMD_STORAGE_DEVICE_REPARTITION:
    {
        anj_sdcard_umount();
        break;
    }
    break;
    case CMD_GET_STORAGE_INFO:
    {
        anj_sdcard_info stSdInfo = {0};
        anj_sdcard_info_query(&stSdInfo);
        int status = Storage_NONE;
        if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_INSERT)
            status = Storage_Mounting;
        else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_FORMAT)
            status = Storage_FORMATING;
        else if ((stSdInfo.eStatus == ANJ_SDCARD_STATUS_MOUNT) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NOT_INIT))
            status = Storage_UNINITED;
        else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NORMAL)
            status = Storage_OK;
        else if ((stSdInfo.eStatus == ANJ_SDCARD_STATUS_RWERROR) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_RONLY))
            status = Storage_EXEPTION;

        int bMounted = (stSdInfo.eStatus >= ANJ_SDCARD_STATUS_MOUNT) ? 1 : 0;
        int iUseSize = stSdInfo.iSize - stSdInfo.iRemainSize;
        int percent = 0;

        if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_FORMAT)
        {
            percent = stSdInfo.iFormatPercent;
        }
        else
        {
            if (stSdInfo.iSize > 0) 
            {
                percent = (int)(((long long)iUseSize * 100) / stSdInfo.iSize);
            }
        }

        EventResult er = {0};
        event_nfs_storage_s *pstNfsInfo = NULL;
        int nfsMounted = 0;
        int nfsTotal = 0;
        int nfsUsed = 0;
        int nfsFree = 0;
        int nfsPercent = 0;

        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_NFS_STORAGE_GET, &er, NULL);
        pstNfsInfo = (event_nfs_storage_s *)er.result;
        if (pstNfsInfo != NULL)
        {
            nfsMounted = pstNfsInfo->mounted;
            nfsTotal = pstNfsInfo->total;
            nfsUsed = pstNfsInfo->used;
            nfsFree = pstNfsInfo->free;
            nfsPercent = pstNfsInfo->percent;
        }

        msg_body = anj_mw_malloc(2048);
        sprintf(msg_body,
                "<RESPONSE_PARAM>\n"
                "<DeviceInfo DevName=\"sd1\" Status=\"%d\" Mounted=\"%d\" Total=\"%d\" Used=\"%d\" Free=\"%d\" Percent=\"%d\" />\n"
                "<DeviceInfo DevName=\"sd2\" Status=\"0\" Mounted=\"0\" Total=\"0\" Used=\"0\" Free=\"0\" Percent=\"0\" />\n"
                "<DeviceInfo DevName=\"usb\" Status=\"0\" Mounted=\"0\" Total=\"0\" Used=\"0\" Free=\"0\" Percent=\"0\" />\n"
                "<DeviceInfo DevName=\"network\" Mounted=\"%d\" Total=\"%d\" Used=\"%d\" Free=\"%d\" Percent=\"%d\" />\n"
                "</RESPONSE_PARAM>",
                status, bMounted, stSdInfo.iSize, iUseSize, stSdInfo.iRemainSize, percent,
                nfsMounted, nfsTotal, nfsUsed, nfsFree, nfsPercent);
    }
    break;
    case CMD_GET_SERIALNUMBER:
    {
        unsigned char sn[256];
        char sn_str[64];

        iRet = anj_sysmng_get_sn(sn, sizeof(sn));
        sprintf(sn_str, "%02X%02X%02X%02X%02X%02X%02X%02X", sn[0], sn[1], sn[2], sn[3], sn[4], sn[5], sn[6], sn[7]);

        if (iRet == 0)
        {
            msg_body = anj_mw_malloc(256);
            sprintf(msg_body, "<RESPONSE_PARAM SerialNumber=\"%s\" />",
                    sn_str);
        }
    }
    break;
    // todo
    case CMD_PTZ_CALIBRATION_START:
    {
        __ERR("CMD_PTZ_CALIBRATION_START\n");
        // MsgPtzCalibrationStart();
    }
    break;
    case CMD_PTZ_CALIBRATION_STOP:
    {
        __ERR("CMD_PTZ_CALIBRATION_STOP\n");
        // mysystem("touch /tmp/flag.ptzreset.test");
        // mysystem("killall comm_server");
        // MsgPtzCalibrationStop();
    }
    break;
    case CMD_PTZ_CALIBRATION_RESTORE:
    {
        __ERR("CMD_PTZ_CALIBRATION_RESTORE\n");
        // mysystem("rm /mnt/nand/ptzstep.cal.cfg -rf");
        // mysystem("touch /tmp/flag.ptzreset.test");
        // mysystem("killall comm_server");
        // MsgPtzCalibrationStop();
    }
    break;
    case CMD_GET_DEVIDE_INFO:
    {
        msg_body = anj_config_devinfo_conver_xml();
    }
    break;
    case CMD_GET_SYSTEMCONTROLSTRING:
    {
        msg_body = anj_mw_malloc(MAX_SYSTEM_CONTROL_STRING_LEN * 2);
        sprintf(msg_body, "<RESPONSE_PARAM SystemConfigString=\"%s\" />", anj_sysctl_get_capability_string());
    }
    break;
    case CMD_GET_SYSTEM_VERSION_INFO:
    {
        DevInfo *pstDevInfo = getDevInfo();
        SYSTEM_VERSION_DATA *pstVersionInfo = &pstDevInfo->stVersionInfo;
        msg_body = anj_mw_malloc(1024);
        sprintf(msg_body, "<RESPONSE_PARAM\nKernelVersion=\"%s\"\nFileSystemVersion=\"%s\"\n/>",
                pstVersionInfo->kernelVersion,
                pstVersionInfo->fsVersion);
    }
    break;
    case CMD_GET_REALY_VERSION_INFO:
    {
        DevInfo *pstDevInfo = getDevInfo();
        SYSTEM_VERSION_DATA *pstVersionInfo = &pstDevInfo->stVersionInfo;
        msg_body = anj_mw_malloc(1024);
        sprintf(msg_body, "<RESPONSE_PARAM\nKernelVersion=\"%s\"\nFileSystemVersion=\"%s\"\n/>",
                pstVersionInfo->kernelVersion,
                pstVersionInfo->fsVersion);
    }
    break;
    case CMD_CONFIG_NETWORK:
        break;
    case CMD_GET_NETWORK_STATUS:
    {
        NETWORK_STATUS_DATA networkStatus;
        iRet = anj_net_info_get(&networkStatus);
        if (iRet == 0)
        {
            char szCloudType[64];
            char file_ver[64] = {0};
            read_file_to_string("/etc/filesys.ver", file_ver, 64);

            if (strstr(file_ver, "_HST") != NULL)
            {
                strcpy(szCloudType, "AC");
            }
            else if (strstr(file_ver, "_HoneyWell") != NULL)
            {
                strcpy(szCloudType, "HONEYWELL");
            }
            else
            {
                switch (networkStatus.cloudType)
                {
                case P2P_TYPE_DANALE:
                    strcpy(szCloudType, "DANALE");
                    break;
                case P2P_TYPE_ANKO:
                    strcpy(szCloudType, "ANKO");
                    break;
                case P2P_TYPE_GOOLINK:
                    strcpy(szCloudType, "GOOLINK");
                    break;
                case P2P_TYPE_YUECAM:
                    strcpy(szCloudType, "YUECAM");
                    break;
                case P2P_TYPE_QQCONNECT:
                    strcpy(szCloudType, "QQ");
                    break;
                case P2P_TYPE_TUTK:
                    strcpy(szCloudType, "TUTK");
                    break;
                case P2P_TYPE_EYEPLUS:
                    strcpy(szCloudType, "EYEPLUS");
                    break;
                case P2P_TYPE_SKYWORTH:
                    strcpy(szCloudType, "AC18Plus");
                    break;
                case P2P_TYPE_TUYA:
                    strcpy(szCloudType, "TUYA");
                    break;
                case P2P_TYPE_AC18PRO:
                    strcpy(szCloudType, "AC18Pro");
                    break;
                case P2P_TYPE_TENCENT:
                    strcpy(szCloudType, "TencentIOT");
                    break;
                case P2P_TYPE_AIOT:
                    strcpy(szCloudType, "AIOT");
                    break;
                default:
                    strcpy(szCloudType, "");
                    break;
                }
            }
            char essid_gb_str[MAX_WIRELESS_ESSID_NAME_LEN * 2 + 4] = {0};
            int ret = utf8_to_gb2312(networkStatus.essid, essid_gb_str);
            if (ret != 0 && strlen(networkStatus.essid) > 0)
            {
                StrCpy(essid_gb_str, sizeof(essid_gb_str), networkStatus.essid);
                __INFO("wifi essid:%s maybe use gb format, copy for xml!\n", essid_gb_str);
            }

            msg_body = anj_mw_malloc(5120);
            if (networkStatus.isWirelessUp)
            {
                sprintf(msg_body,
                        "<RESPONSE_PARAM>\n"
                        "<WIRE_NETWORK\n"
                        "MacAddress=\"%s\"\n"
                        "IPType=\"%s\"\n"
                        "IPAddress=\"%s\"\n"
                        "Netmask=\"%s\"\n"
                        "Gateway=\"%s\"\n"
                        "Dns1=\"%s\"\n"
                        "Dns2=\"%s\"\n"
                        "/>\n"
                        "<WIRELESS_NETWORK\n"
                        "MacAddress=\"%s\"\n"
                        "IPAddress=\"%s\"\n"
                        "Netmask=\"%s\"\n"
                        "Gateway=\"%s\"\n"
                        "ESSID=\"%s\"\n"
                        "OperationMode=\"%s\"\n"
                        "AccessPoint=\"%s\"\n"
                        "Bitrate=\"%s\"\n"
                        "Frequency=\"%s\"\n"
                        "EncrptType=\"%s\"\n"
                        "LinkQuality=\"%d\"\n"
                        "SignalLevel=\"%d\"\n"
                        "Noise=\"%d\"\n"
                        "/>\n"
                        "<CLOUD\n"
                        "Enable=\"%d\"\n"
                        "Type=\"%d\"\n"
                        "szType=\"%s\"\n"
                        "LoginStatus=\"%d\"\n"
                        "ID=\"%s\"\n"
                        "/>\n"
                        "</RESPONSE_PARAM>",
                        networkStatus.wireMac,
                        networkStatus.ipType,
                        networkStatus.ip,
                        networkStatus.netmask,
                        networkStatus.gateway,
                        networkStatus.dns1,
                        networkStatus.dns2,
                        networkStatus.wirelessMac,
                        networkStatus.wirelessIp,
                        networkStatus.wirelessNetmask,
                        networkStatus.wirelessGateway,
                        essid_gb_str,
                        networkStatus.operationMode,
                        networkStatus.accessPoint,
                        networkStatus.bitRate,
                        networkStatus.freq,
                        networkStatus.encryptType,
                        networkStatus.linkquality,
                        networkStatus.signallevel,
                        networkStatus.noise,
                        networkStatus.cloudEnable,
                        networkStatus.cloudType,
                        szCloudType,
                        networkStatus.cloudLogined,
                        networkStatus.cloudId);
            }
            else
            {
                sprintf(msg_body,
                        "<RESPONSE_PARAM>\n"
                        "<WIRE_NETWORK\n"
                        "MacAddress=\"%s\"\n"
                        "IPType=\"%s\"\n"
                        "IPAddress=\"%s\"\n"
                        "Netmask=\"%s\"\n"
                        "Gateway=\"%s\"\n"
                        "Dns1=\"%s\"\n"
                        "Dns2=\"%s\"\n"
                        "/>\n"
                        "<CLOUD\n"
                        "Enable=\"%d\"\n"
                        "Type=\"%d\"\n"
                        "szType=\"%s\"\n"
                        "LoginStatus=\"%d\"\n"
                        "ID=\"%s\"\n"
                        "/>\n"
                        "</RESPONSE_PARAM>",
                        networkStatus.wireMac,
                        networkStatus.ipType,
                        networkStatus.ip,
                        networkStatus.netmask,
                        networkStatus.gateway,
                        networkStatus.dns1,
                        networkStatus.dns2,
                        networkStatus.cloudEnable,
                        networkStatus.cloudType,
                        szCloudType,
                        networkStatus.cloudLogined,
                        networkStatus.cloudId);
            }
        }
    }
    break;
    case CMD_GET_4G_NETWORK_STATUS:
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
        if (event_result.result)
        {
            G4InfoStruct networkStatus = *(G4InfoStruct *)event_result.result;
            msg_body = anj_config_network_status_4g_conver_xml(&networkStatus);
        }
    }
    break;
    case CMD_4G_NETWORK_STATUS_OVERLAY:
    {
        // Mode = 0: close 1: simple 2: full
        char *msgflag = GetRequestParamValue(pDoc, (char *)"Mode");
        if (msgflag != NULL)
        {
            EventResult event_result = {0};
            int flag = atoi(msgflag);
            eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_OSD_SET, &event_result, (void *)&flag);
            anj_mw_free(msgflag);
        }
    }
    break;
    case CMD_LSA_P2P_GET_REGISTER_INFO:
    {
        __ERR("GET CMD_LSA_P2P_GET_REGISTER_INFO\n");

        // todo
        // LsaP2pRegisterInfo_t regInfo;
        // iRet = P2pMsgGetLsaAccountInfo(&regInfo);
        // if (iRet == 0)
        // {

        //     msg_body = anj_mw_malloc(1024);
        //     sprintf(msg_body,
        //             "<RESPONSE_PARAM>\n"
        //             "<LSAP2PAccount \n"
        //             "SerailNum=\"%s\"\n"
        //             "UserName=\"%s\"\n"
        //             "Passwd=\"%s\"\n"
        //             "/>\n"
        //             "</RESPONSE_PARAM>",
        //             regInfo.sn,
        //             regInfo.username,
        //             regInfo.passwd);
        // }
    }
    break;
    case CMD_LSA_P2P_REGISTER:
    {
        __ERR("GET CMD_LSA_P2P_REGISTER\n");
        char *strUserName = GetRequestParamValue(pDoc, (char *)"UserName");
        char *strPasswd = GetRequestParamValue(pDoc, (char *)"Passwd");

        __ERR("lsa userName :%s, passwd:%s\n", strUserName, strPasswd);

        if (strUserName == NULL || strPasswd == NULL)
        {
            __ERR("param error, user default\n");
            strUserName = "lsatestuser";
            strPasswd = "lsatestpasswd";
            iRet = -1;
            break;
        }

        // todo
        // iRet = P2pMsgRegisterLsaAccount(strUserName, strPasswd);
        // if (iRet == 0)
        // {
        //     __ERR("register success , killall stream_caster\n");
        //     anj_mw_system("killall stream_caster");
        // }
        if (strUserName)
            anj_mw_free(strUserName);
        if (strPasswd)
            anj_mw_free(strPasswd);
    }
    break;
    case CMD_GET_RECORD_FILE_LIST:
    {
        SearchMode_e searchMode = SearchModeByFiles;
        char *strChn = GetRequestParamValue(pDoc, (char *)"Channel");
        char *strSearchMode = GetRequestParamValue(pDoc, (char *)"SearchMode");
        char *strRecordMode = GetRequestParamValue(pDoc, (char *)"RecordMode");
        char *strStartTime = GetRequestParamValue(pDoc, (char *)"StartTime");
        char *strEndTime = GetRequestParamValue(pDoc, (char *)"EndTime");
        char *strMediaType = GetRequestParamValue(pDoc, (char *)"MediaType");
        char *strStreamIndex = GetRequestParamValue(pDoc, (char *)"StreamIndex");
        char *strMinSize = GetRequestParamValue(pDoc, (char *)"MinSize");
        char *strMaxSize = GetRequestParamValue(pDoc, (char *)"MaxSize");
        char *strPage = GetRequestParamValue(pDoc, (char *)"Page");
        char *strPageSize = GetRequestParamValue(pDoc, (char *)"PageSize");
        int chn = (strChn == NULL) ? 0 : atoi(strChn);

        if (strSearchMode != NULL)
        {
            searchMode = atoi(strSearchMode);
        }

        if (searchMode == SearchModeByTimeDistribute) // 返回分布列表
        {
            struct tm start_time;
            GetTimeFromString(strStartTime, &start_time);
            char recordDistribute[VS_RECORD_DISTRIBUTE_LEN + 4];
            memset(recordDistribute, 0, VS_RECORD_DISTRIBUTE_LEN + 4);

            rec_pb_date_s pbDate;
            memset(&pbDate, 0, sizeof(rec_pb_date_s));
            pbDate.tEvent = REC_EVENT_ALL;

            if (strRecordMode)
            {
                if (strstr(strRecordMode, "MANUAL"))
                {
                }
                else if (strstr(strRecordMode, "SCHEDULE"))
                {
                }
                else if (strstr(strRecordMode, "MOTION"))
                {
                }
                else if (strstr(strRecordMode, "ALARM"))
                {
                }
                else
                {
                    pbDate.tEvent = REC_EVENT_ALL;
                }
            }
            pbDate.year = start_time.tm_year + 1900;
            pbDate.month = start_time.tm_mon + 1;
            pbDate.day = start_time.tm_mday;
            rec_pb_list_s stPbList = {0};
            memset(recordDistribute, VS_REC_TYPE_NO_RECORDING, VS_RECORD_DISTRIBUTE_LEN);
            int querySuccess = 0;
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                if (anj_record_pb_query_day_create(i, &pbDate, &stPbList) == 0)
                {
                    continue;
                }
                querySuccess = 1;
                rec_pb_segment_s *pHead = stPbList.pstSegment;

                while (pHead != NULL)
                {
                    struct tm stSegTmBegin;
                    struct tm stSegTmEnd;
                    memset(&stSegTmBegin, 0, sizeof(stSegTmBegin));
                    memset(&stSegTmEnd, 0, sizeof(stSegTmEnd));
                    localtime_r((time_t *)&pHead->begin_time_s, &stSegTmBegin);
                    localtime_r((time_t *)&pHead->end_time_s, &stSegTmEnd);
                    for (int startMin = (stSegTmBegin.tm_hour * 60 + stSegTmBegin.tm_min); startMin <= (stSegTmEnd.tm_hour * 60 + stSegTmEnd.tm_min); startMin++)
                    {
                        if (REC_EVENT_CHECK_MASK(pHead->tEvent, REC_EVENT_HUMEN_ALARM_MASK))
                        {
                            recordDistribute[startMin] = VS_REC_TYPE_PD;
                        }
                        else if (REC_EVENT_CHECK_MASK(pHead->tEvent, REC_EVENT_MOTION_ALARM_MASK))
                        {
                            if (recordDistribute[startMin] != VS_REC_TYPE_PD)
                            {
                                recordDistribute[startMin] = VS_REC_TYPE_MD;
                            }
                        }
                        else
                        {
                            if (recordDistribute[startMin] == VS_REC_TYPE_NO_RECORDING)
                            {
                                recordDistribute[startMin] = VS_REC_TYPE_UNCONDITIONAL;
                            }
                        }
                    }

                    pHead = pHead->ptNext;
                }
                anj_record_pb_query_day_release(i, &stPbList);
            }
            if (querySuccess)
            {
                char *pe;
                char *pb;
                int body_size = VS_RECORD_DISTRIBUTE_LEN + 256;
                msg_body = anj_mw_malloc(body_size);
                pb = msg_body;
                pe = msg_body + body_size - 1;

                // response
                pb += snprintf(pb, pe - pb, "<RESPONSE_PARAM SearchMode=\"%d\">\n", searchMode);
                pb += snprintf(pb, pe - pb, "<RECORD_DISTRIBUTE\n");
                pb += snprintf(pb, pe - pb, "Distribute=\"%s\"\n", recordDistribute);
                pb += snprintf(pb, pe - pb, "/>\n");

                pb += snprintf(pb, pe - pb, "</RESPONSE_PARAM>");
            }
            else
            {
                msg_body = anj_mw_malloc(1024);
                strcpy(msg_body, "<RESPONSE_PARAM>\n</RESPONSE_PARAM>");
            }
        }
        else if (searchMode == SearchModeGetDateList) // 返回有录像的日期列表
        {
            struct tm start_time;
            GetTimeFromString(strStartTime, &start_time);

            rec_pb_date_s pbDate;
            memset(&pbDate, 0, sizeof(rec_pb_date_s));
            if (strRecordMode)
            {
                if (strstr(strRecordMode, "MANUAL"))
                {
                }
                else if (strstr(strRecordMode, "SCHEDULE"))
                {
                }
                else if (strstr(strRecordMode, "MOTION"))
                {
                }
                else if (strstr(strRecordMode, "ALARM"))
                {
                }
                else
                {
                    pbDate.tEvent = REC_EVENT_SET_MASK(pbDate.tEvent, REC_EVENT_NULL_MASK);
                }
            }
            pbDate.year = start_time.tm_year + 1900;
            pbDate.month = start_time.tm_mon + 1;
            pbDate.day = start_time.tm_mday;

            anj_record_pb_query_mounth(chn, &pbDate);

            int DateCount = 0;
            unsigned int MonthDays = pbDate.day;
            if (MonthDays)
            {
                for (int i = 0; i < 31; i++)
                {
                    if ((MonthDays & (1 << i)))
                    {
                        DateCount++;
                    }
                }
            }

            int body_size = DateCount * DATE_ITEM_XML_SIZE + RESPONSE_XML_OVERHEAD;
            char *pe;
            char *pb;
            msg_body = anj_mw_malloc(body_size);
            if (msg_body == NULL)
            {
                __ERR("SearchModeGetDateList malloc failed, DateCount:%d\n", DateCount);
                break;
            }
            pb = msg_body;
            pe = msg_body + body_size - 1;

            pb += snprintf(pb, pe - pb, "<RESPONSE_PARAM  SearchMode=\"%d\" DateCount=\"%d\">\n", searchMode, DateCount);
            if (DateCount)
            {
                for (int i = 0; i < 31; i++)
                {
                    if ((MonthDays & (1 << i)))
                    {
                        start_time.tm_mday = i + 1;
                        start_time.tm_hour = 0;
                        start_time.tm_min = 0;
                        start_time.tm_sec = 0;
                        start_time.tm_isdst = -1;

                        pb += snprintf(pb, pe - pb, "<DATE\n");
                        pb += snprintf(pb, pe - pb, "UtcTime=\"%u\"\n", (unsigned int)mktime(&start_time));
                        pb += snprintf(pb, pe - pb, "DateStr=\"%04d%02d%02d\"\n",
                                       start_time.tm_year + 1900, start_time.tm_mon + 1, start_time.tm_mday);
                        pb += snprintf(pb, pe - pb, "/>\n");
                    }
                }
            }
            pb += snprintf(pb, pe - pb, "</RESPONSE_PARAM>");
        }
        else if (searchMode == SearchModeGetAlarmList)
        {
            // 这里查询组包返回
            __INFO("SearchModeGetAlarmList");
            unsigned long long ch_mask = 0;
            unsigned long long ch_one = 1;
            ch_mask |= (ch_one << channel);
            int page = Str2Num(strPage);

            static time_t queryEndTime[2] = {0};
            static int s_alarm_list_eof = 0;

            // 获取时间
            struct tm start_time = {0};
            struct tm end_time = {0};
            GetTimeFromString(strStartTime, &start_time);
            GetTimeFromString(strEndTime, &end_time);
            unsigned handle = 1;
            char *pe;
            char *pb;
            unsigned out_num = 0;
            result_node_t result_list_[LOG_NUM_PER_PAGE] = {0};
            if (page <= 0)
            {
                queryEndTime[0] = 0;
                queryEndTime[1] = 0;
                s_alarm_list_eof = 0;
            }
            int i = 0;
            for (i = 0; !s_alarm_list_eof && i < ANJ_CAMERA_MAX_NUMS && i < 2; i++)
            {
                rec_pb_date_s pbDate;
                memset(&pbDate, 0, sizeof(rec_pb_date_s));
                pbDate.tEvent = REC_EVENT_SET_MASK(pbDate.tEvent, REC_EVENT_MOTION_ALARM_MASK) |
                                REC_EVENT_SET_MASK(pbDate.tEvent, REC_EVENT_HUMEN_ALARM_MASK) |
                                REC_EVENT_SET_MASK(pbDate.tEvent, REC_EVENT_CAR_ALARM_MASK) |
                                REC_EVENT_SET_MASK(pbDate.tEvent, REC_EVENT_IO_ALARM_MASK);
                pbDate.year = start_time.tm_year + 1900;
                pbDate.month = start_time.tm_mon + 1;
                pbDate.day = start_time.tm_mday;
                rec_pb_list_s stPbList = {0};
                if (anj_record_pb_query_day_create(i, &pbDate, &stPbList) == 0 && out_num < LOG_NUM_PER_PAGE)
                {
                    rec_pb_segment_s *pHead = stPbList.pstSegment;
                    while (pHead != NULL)
                    {
                        if (REC_EVENT_CHECK_MASK(pHead->tEvent, REC_EVENT_MOTION_ALARM_MASK))
                        {
                            result_list_[out_num].type = LOG_ALARM_MD;
                        }
                        else if (REC_EVENT_CHECK_MASK(pHead->tEvent, REC_EVENT_HUMEN_ALARM_MASK))
                        {
                            result_list_[out_num].type = LOG_ALARM_PD;
                        }
                        else
                        {
                            result_list_[out_num].type = LOG_ALARM_ALL;
                        }

                        result_list_[out_num].start_time = pHead->begin_time_s;
                        if (queryEndTime[i] > result_list_[out_num].start_time)
                        {
                            result_list_[out_num].start_time = queryEndTime[i];
                        }

                        if (((time_t)pHead->end_time_s - result_list_[out_num].start_time) <= 0)
                        {
                            pHead = pHead->ptNext;
                            continue;
                        }

                        if (ANJ_CAMERA_MAX_NUMS > 1)
                        {
                            result_list_[out_num].channel = 0;
                            if (0 == i)
                            {
                                result_list_[out_num].channel = 1;
                            }
                        }
                        else
                        {
                            result_list_[out_num].channel = i;
                        }
                        while (((time_t)pHead->end_time_s - result_list_[out_num].start_time) > 3 * 60)
                        {
                            result_list_[out_num].end_time = result_list_[out_num].start_time + 3 * 60;
                            result_list_[out_num].duration = result_list_[out_num].end_time - result_list_[out_num].start_time;
                            if (out_num >= LOG_NUM_PER_PAGE - 1)
                            {
                                break;
                            }
                            out_num++;

                            result_list_[out_num].channel = result_list_[out_num - 1].channel;
                            result_list_[out_num].type = result_list_[out_num - 1].type;
                            result_list_[out_num].start_time = result_list_[out_num - 1].end_time;
                        }
                        if (out_num >= LOG_NUM_PER_PAGE - 1)
                        {
                            break;
                        }
                        result_list_[out_num].end_time = pHead->end_time_s;
                        result_list_[out_num].duration = result_list_[out_num].end_time - result_list_[out_num].start_time;
                        if (result_list_[out_num].duration < 5)
                        {
                            result_list_[out_num].duration = 5;
                        }
                        out_num++;

                        pHead = pHead->ptNext;
                    }

                    if (out_num)
                    {
                        queryEndTime[i] = result_list_[out_num - 1].end_time;
                        __INFO("queryEndTime[%d]:%ld out_num:%d\n", i, queryEndTime[i], out_num);
                    }
                    anj_record_pb_query_day_release(i, &stPbList);
                }
            }
            if (out_num < LOG_NUM_PER_PAGE - 1)
            {
                s_alarm_list_eof = 1;
            }
            __INFO("out_num:%d page:%d\n", out_num, page);

            int body_size = (int)out_num * ALARM_ITEM_XML_SIZE + RESPONSE_XML_OVERHEAD;
            msg_body = anj_mw_malloc(body_size);
            if (msg_body == NULL)
            {
                __ERR("SearchModeGetAlarmList malloc failed, out_num:%u\n", out_num);
                break;
            }
            pb = msg_body;
            pe = msg_body + body_size - 1;

            // 查询有多少条数以及返回的页数
            pb += snprintf(pb, pe - pb, "<RESPONSE_PARAM	SearchMode=\"%d\" Page=\"%d\" SearchHandle=\"%d\" AlarmCount=\"%d\"> ",
                           searchMode, page,
                           handle,
                           out_num);

            struct tm *local = NULL;
            char timeStr[128] = {0};

            for (int i = 0; i < (int)out_num; ++i)
            {
                const char *alarm;
                result_node_t *pResult = &result_list_[i];
                if (pResult->type == LOG_ALARM_MD)
                {
                    alarm = "md";
                }
                else if (pResult->type == LOG_ALARM_PD)
                {
                    alarm = "pd";
                }
                else if (pResult->type == LOG_ALARM_COVER)
                {
                    alarm = "cv";
                }
                else if (pResult->type == LOG_ALARM_FIRE)
                {
                    alarm = "fl";
                }
                else if (pResult->type == LOG_ALARM_CAR)
                {
                    alarm = "car";
                }
                else if (pResult->type == LOG_ALARM_FD)
                {
                    alarm = "fd";
                }
                else if (pResult->type == LOG_ALARM_IO)
                {
                    alarm = "al_in";
                }
                else
                {
                    alarm = "other";
                }
                local = localtime(&pResult->start_time);
                sprintf(timeStr, "%04d%02d%02d %02d:%02d:%02d",
                        local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                        local->tm_hour, local->tm_min, local->tm_sec);
                pb += snprintf(pb, pe - pb, "<Item Time=\"%s\" Alarm=\"%s\" Du=\"%d\" Chn=\"%d\"/> ", timeStr, alarm, pResult->duration, pResult->channel);
            }

            pb += snprintf(pb, pe - pb, "</RESPONSE_PARAM>");
        }
        else
        {
		    // 返回文件列表或间隔列表
            int search_file_type = -1;
            if (strMediaType)
            {
                if (strstr(strMediaType, "AUDIOVIDEO") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_AUDIO;
                else if (strstr(strMediaType, "VIDEO") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_VIDEO;
                else if (strstr(strMediaType, "AUDIO") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_AUDIO;
                else if (strstr(strMediaType, "PICTURE") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_IMAGE;
                else if (strstr(strMediaType, "OEMMP3") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_OEMMP3;
                else if (strstr(strMediaType, "OEMAPP") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_OEMAPP;
                else if (strstr(strMediaType, "OEMLOGO") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_OEMLOGO;
                else if (strstr(strMediaType, "CERTIFICATION") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_CERTIFICATION;
                else if (strstr(strMediaType, "KEY") != NULL)
                    search_file_type = SERACH_FILE_MEDIA_TYPE_KEY;
                else
                    search_file_type = -1;
            }

            int stream_index = -1;
            if (strStreamIndex)
                stream_index = atoi(strStreamIndex);

            int page_size = 0;
            if (strPageSize)
                page_size = atoi(strPageSize);
            else
                page_size = MAX_QUERY_FILE_COUNT;

            if (page_size < 10)
            {
                page_size = 10;
            }

            int min_size = 0;
            if (strMinSize)
            {
                min_size = atoi(strMinSize);
            }

            int max_size = 0;
            if (strMaxSize)
            {
                max_size = atoi(strMaxSize);
            }

            int skip_count = 0;
            if (strPage)
                skip_count = atoi(strPage) * page_size;
            if (skip_count < 0)
                skip_count = 0;

            __INFO("get file list: searchMode:%d, media_type:%d, stream_index:%d, min_size:%d, max_size:%d, skip_count:%d, pagesize:%d\n",
                   searchMode, search_file_type, stream_index, min_size, max_size, skip_count, page_size);

            if (searchMode == SearchModeByTimeInterval)
            {
            }
            else
            {
                char szPath[64] = {0};

                int file_count = 0;
                file_query_result *pstFileQueryResult = anj_mw_malloc(sizeof(file_query_result));
                if (pstFileQueryResult == NULL)
                {
                    __ERR("pstFileQueryResult malloc failed\n");
                }
                else
                {
                    memset(pstFileQueryResult, 0, sizeof(file_query_result));
                }

                if (search_file_type == SERACH_FILE_MEDIA_TYPE_OEMMP3 && pstFileQueryResult != NULL)
                {
                    anj_audio_mp3_file_list_query(pstFileQueryResult, skip_count, page_size);
                }
                else if (search_file_type == SERACH_FILE_MEDIA_TYPE_OEMLOGO && pstFileQueryResult != NULL)
                {
                    snprintf(szPath, sizeof(szPath), "%s", DATA_BLOCK_MOUNT_PATH);
                    file_count = query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".bmp");

                    memset(szPath, 0, sizeof(szPath));
                    snprintf(szPath, sizeof(szPath), "%s/logo", OEM_MOUNT_PATH);
                    file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".bmp");

                    memset(szPath, 0, sizeof(szPath));
                    snprintf(szPath, sizeof(szPath), "%s/logo", AJ_APP_PATH);
                    file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".bmp");
                }
                else if (search_file_type == SERACH_FILE_MEDIA_TYPE_CERTIFICATION)
                {
                    if (skip_count == 0)
                    {
                        snprintf(szPath, sizeof(szPath), "%s", AJ_APP_PATH);
                        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".crt");

                        memset(szPath, 0, sizeof(szPath));
                        snprintf(szPath, sizeof(szPath), "%s", DATA_BLOCK_MOUNT_PATH);
                        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".crt");
                    }
                }
                else if (search_file_type == SERACH_FILE_MEDIA_TYPE_KEY)
                {
                    if (skip_count == 0)
                    {
                        snprintf(szPath, sizeof(szPath), "%s", AJ_APP_PATH);
                        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".key");

                        memset(szPath, 0, sizeof(szPath));
                        snprintf(szPath, sizeof(szPath), "%s", DATA_BLOCK_MOUNT_PATH);
                        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".key");
                    }
                }
                else if (search_file_type == SERACH_FILE_MEDIA_TYPE_OEMAPP)
                {
                    snprintf(szPath, sizeof(szPath), "%s/app", OEM_MOUNT_PATH);
                    file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, NULL);
                }
                else
                {
                }

                if (pstFileQueryResult && pstFileQueryResult->count > 0)
                {
                    __INFO("query file type:%d count:%d, %d!\n", search_file_type, pstFileQueryResult->count, file_count);

                    int body_size = MAX_QUERY_FILE_COUNT * 256 + 256;
                    char *pe = NULL;
                    char *pb = NULL;
                    msg_body = anj_mw_malloc(body_size);
                    pb = msg_body;
                    pe = msg_body + body_size - 1;

                    pb += snprintf(pb, pe - pb, "<RESPONSE_PARAM FileCount=\"%d\">\n", pstFileQueryResult->count);
                    int i = 0;
                    for (i = 0; i < pstFileQueryResult->count; i++)
                    {
                        pb += snprintf(pb, pe - pb, "<RECORD_FILE\n");
                        pb += snprintf(pb, pe - pb, "FilePath=\"%s\"\n", pstFileQueryResult->file_info[i].filepath);
                        pb += snprintf(pb, pe - pb, "FileLength=\"%lu\"\n", pstFileQueryResult->file_info[i].filesize);
                        pb += snprintf(pb, pe - pb, "FileDuration=\"%lu\"\n", pstFileQueryResult->file_info[i].filesize);

                        if (search_file_type == SERACH_FILE_MEDIA_TYPE_OEMMP3)
                        {
                            char szDescrptionFileName[MAX_FILE_PATH_LEN] = {0};
                            char *dot_pos = NULL;
                            char *outbuffer = NULL;

                            StrCpy(szDescrptionFileName, MAX_FILE_PATH_LEN, pstFileQueryResult->file_info[i].filepath);
                            dot_pos = strrchr(szDescrptionFileName, '.');
                            if (dot_pos != NULL)
                            {
                                if ((dot_pos - szDescrptionFileName + 5) <= sizeof(szDescrptionFileName))
                                {
                                    strcpy(dot_pos, ".txt");
                                }
                                else
                                {
                                    __ERR("filename:%s change suffix failed\n", pstFileQueryResult->file_info[i].filepath);
                                    continue;
                                }

                                if (anj_mw_file_exists(szDescrptionFileName))
                                {
                                    unsigned char buffer[512] = {0};
                                    int readlen = read_file_to_buffer(szDescrptionFileName, (char *)buffer, sizeof(buffer) - 1);

                                    if (readlen > 0)
                                    {
                                        buffer[readlen] = '\0';
                                    }
                                    __INFO("open and read file:%s ok, readlen:%d\n", szDescrptionFileName, readlen);

                                    if (readlen > 2)
                                    {
                                        __INFO("buffer:%#x %#x\n", buffer[0], buffer[1]);
                                        if (buffer[0] == 0xFF && buffer[1] == 0xFE)
                                        {
                                            // UNICODE原始信息
                                            outbuffer = (char *)anj_mw_malloc(1024);
                                            if (outbuffer != NULL)
                                            {
                                                memset(outbuffer, 0, 1024);
                                                hexdataTohexStr((const char *)buffer, readlen, outbuffer, 1024);
                                                pb += snprintf(pb, pe - pb, "UnicodeDesc=\"%s\"\n", outbuffer);
                                                anj_mw_free(outbuffer);
                                                outbuffer = NULL;
                                            }
                                        }
                                        // 检查HEX字符串格式
                                        if (readlen > 4 &&
                                            tolower(buffer[0]) == 'f' &&
                                            tolower(buffer[1]) == 'f' &&
                                            tolower(buffer[2]) == 'f' &&
                                            tolower(buffer[3]) == 'e')
                                        {
                                            pb += snprintf(pb, pe - pb, "UnicodeDesc=\"%s\"\n", buffer);
                                        }
                                    }
                                }
                            }
                        }

                        pb += snprintf(pb, pe - pb, "/>\n");
                    }
                    pb += snprintf(pb, pe - pb, "</RESPONSE_PARAM>");
                    __INFO("search file msg_body: \n%s\n", msg_body);
                }
                else
                {
                    msg_body = anj_mw_malloc(1024);
                    if (msg_body)
                        strcpy(msg_body, "<RESPONSE_PARAM>\n</RESPONSE_PARAM>");
                }

                if (pstFileQueryResult)
                {
                    anj_mw_free(pstFileQueryResult);
                    pstFileQueryResult = NULL;
                }
            }
        }

        if (strSearchMode)
            anj_mw_free(strSearchMode);
        if (strRecordMode)
            anj_mw_free(strRecordMode);
        if (strStartTime)
            anj_mw_free(strStartTime);
        if (strEndTime)
            anj_mw_free(strEndTime);
        if (strMediaType)
            anj_mw_free(strMediaType);
        if (strStreamIndex)
            anj_mw_free(strStreamIndex);
        if (strMinSize)
            anj_mw_free(strMinSize);
        if (strMaxSize)
            anj_mw_free(strMaxSize);
        if (strPage)
            anj_mw_free(strPage);
        if (strPageSize)
            anj_mw_free(strPageSize);
    }
    break;
    case CMD_UPLOAD_FILE:
    {
        request_param = GetRequestParamValue(pDoc, (char *)"FileType");
        if (request_param == NULL)
            iRet = -1;
        else
        {
            long file_type = atol(request_param);
            anj_mw_free(request_param);

            char *upload_file = GetRequestParamValue(pDoc, (char *)"FilePath");
            if (upload_file == NULL)
                iRet = -2;
            else if ((file_type != UPLOAD_CONFIG_FILE_TYPE) && (file_type != UPLOAD_FIRMWARE_FILE_TYPE) &&
                     (file_type != UPLOAD_OEM_APP_FILE_TYPE) && (file_type != UPLOAD_OEM_MP3_FILE_TYPE) &&
                     (file_type != UPLOAD_OEM_LOGO_FILE_TYPE) && (file_type != UPLOAD_CERTIFICATION_FILE_TYPE) &&
                     (file_type != UPLOAD_KEY_FILE_TYPE) && (file_type != UPLOAD_OEM_CFG_FILE_TYPE) &&
                     (file_type != UPLOAD_CONFIG_XML_FILE_TYPE))
            {
                iRet = -3;
            }
            else
            {
                if (file_type == UPLOAD_FIRMWARE_FILE_TYPE)
                {
                    anj_audio_prompt_play(ANJ_MP3_OTA_PATH, ANJ_MP3_DEVICE_START_UPDATE, 1);
                    int totalmen = anj_sysmng_get_totalmem();
                    int freemem = anj_sysmng_get_freemem();
                    __INFO("freemem=%d kB, totalmen=%d kB, msgSrc:%d.\n", freemem, totalmen, MsgSrc);
                    if (MsgSrc == MSG_SRC_PRI)
                    {
                        anj_service_provider_uninit_all();
                        anj_search_uninit();
                        modules_uninit("anj_service", "anj_net");
                    }
                    else if (MsgSrc == MSG_SRC_SER)
                    {
                        modules_uninit("anj_ser", "anj_net");
                    }

                    anj_mw_system("echo 3 > /proc/sys/vm/drop_caches");
                    freemem = anj_sysmng_get_freemem();
                    sleep(1);
                    __INFO("freemem is %d kB\n", freemem);
                }

                char local_file[256] = {0};

                struct timeval tv;
                SystemGetTimeofRun(&tv, NULL);

                sprintf(local_file,
                        "/tmp/upfile_%d_%d.dat",
                        (int)tv.tv_sec,
                        (int)tv.tv_usec);

                char *length_param = GetRequestParamValue(pDoc, (char *)"FileLength");
                if (length_param == NULL) // old version client
                {
                    unsigned short port = 0;
                    if (MsgSrc == MSG_SRC_PRI)
                    {
                        port = anj_pri_cmd_port_get(0); // for tcp; find_free_tcp_port(StreamCfg.commConfig.ptzPort);
                    }
                    if (port == 0)
                    {
                        __ERR("find anj_mw_free tcp port failed.\n");
                        iRet = -4;
                    }
                    else
                    {
                        iRet = file_sender_transport(1, file_type, local_file, upload_file, port, lognum);
                        anj_mw_free(upload_file);

                        // tell client which port we use to transport file
                        msg_body = anj_mw_malloc(1024);
                        sprintf(msg_body, "<RESPONSE_PARAM\nPort=\"%d\"\n/>", port);
                    }
                }
                else
                {
                    unsigned long filelen = atoi(length_param);
                    anj_mw_free(length_param);
                    __ERR("filelen = %lu, using same port to recv file!!!\n", filelen);

                    iRet = 0;

                    if (file_type == UPLOAD_CONFIG_FILE_TYPE)
                    {
                        if (filelen > (1024 * 1024))
                        {
                            __ERR("filelen = %lu > 1024*1024 for config file!!!\n", filelen);
                            iRet = -5;
                        }
                    }
                    else if (file_type == UPLOAD_OEM_MP3_FILE_TYPE || file_type == UPLOAD_OEM_APP_FILE_TYPE)
                    {
                        char *szFileName = GetFileNameFromFullName(upload_file);
                        __ERR("filename %s\n", szFileName);

                        if (strlen(szFileName) == 0)
                        {
                            iRet = -7;
                        }
                        else
                        {
                            if (file_type == UPLOAD_OEM_MP3_FILE_TYPE)
                            {
                                if (filelen > UPLOAD_MP3_TO_CFG_MTD_MAX_FILE_SIZE)
                                {
                                    __ERR("filelen %ld too big.\n", filelen);
                                    iRet = -6;
                                }
                                else
                                {
                                    unsigned int freespace = (unsigned int)GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
                                    if (freespace < UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE)
                                    {
                                        __ERR("freespace of data partition is %u, too dangrous, now alowed to upload!\n", freespace);
                                        iRet = -7;
                                    }
                                    else
                                    {
                                        char targetName[128] = {0};
                                        snprintf(targetName, sizeof(targetName), "%s/mp3/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_MP3_FILE_NAME);
                                        snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_MP3_FILE_NAME);
                                        if (!anj_mw_file_exists(local_file))
                                        {
                                            __ERR("File %s isn't exist, will upload.\n", local_file);
                                            remove(targetName);
                                            readlink(targetName, local_file, sizeof(local_file));
                                        }
                                        else
                                        {
                                            if (!anj_mw_file_exists(targetName))
                                            {
                                                readlink(targetName, local_file, sizeof(local_file));
                                            }
                                        }

                                        __INFO("local_file %s\n", local_file);
                                    }
                                }
                            }
                            else
                            {
                                snprintf(local_file, sizeof(local_file), "/tmp/%s", szFileName);

                                if (strstr(szFileName, "ispdaybin_"))
                                    snprintf(local_file, sizeof(local_file), "/tmp/isp_day.bin");
                                else if (strstr(szFileName, "ispnightbin_"))
                                    snprintf(local_file, sizeof(local_file), "/tmp/isp_night.bin");
                                else if (strstr(szFileName, "aiispbin_"))
                                    snprintf(local_file, sizeof(local_file), "/tmp/aiisp.bin");

                                __ERR("local_file %s\n", local_file);
                            }
                            if (iRet >= 0 && strlen(local_file) > 0 && file_type == UPLOAD_OEM_MP3_FILE_TYPE)
                            {
                                char *pUnicodeDesc = GetRequestParamValue(pDoc, (char *)"UnicodeDesc");
                                if (NULL != pUnicodeDesc)
                                {
                                    char szDescrptionFileName[256] = {0};
                                    snprintf(szDescrptionFileName, sizeof(szDescrptionFileName), "%s", local_file);
                                    char *last_dot = strrchr(szDescrptionFileName, '.');
                                    if (last_dot != NULL)
                                    {
                                        strcpy(last_dot, ".txt");
                                    }
                                    else
                                    {
                                        strncat(szDescrptionFileName, ".txt", sizeof(szDescrptionFileName) - strlen(szDescrptionFileName) - 1);
                                    }
                                    __INFO("szDescrptionFileName %s\n", szDescrptionFileName);

                                    int hexstrlen = strlen(pUnicodeDesc);
                                    if (hexstrlen > 4 &&
                                        tolower(pUnicodeDesc[0]) == 'f' &&
                                        tolower(pUnicodeDesc[1]) == 'f' &&
                                        tolower(pUnicodeDesc[2]) == 'f' &&
                                        tolower(pUnicodeDesc[3]) == 'e')
                                    {
                                        // UNICODE 16进制HEX字符串明文转换成原始UNICODE
                                        int outbufferlen = (hexstrlen >> 1);
                                        char *outbuffer = anj_mw_malloc(outbufferlen);
                                        memset(outbuffer, 0, outbufferlen);
                                        hexStrToUInt(pUnicodeDesc, hexstrlen, (unsigned char *)outbuffer);
                                        anj_mw_write_file(szDescrptionFileName, 0, outbuffer, outbufferlen);
                                        anj_mw_free(outbuffer);
                                    }

                                    anj_mw_free(pUnicodeDesc);
                                }
                            }
                        }
                    }
                    else if (file_type == UPLOAD_OEM_LOGO_FILE_TYPE)
                    {
                        char *szFileName = GetFileNameFromFullName(upload_file);
                        if (strlen(szFileName) == 0)
                        {
                            iRet = -7;
                        }
                        else
                        {
                            unsigned long long freespace = -1;

                            freespace = GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
                            __ERR("anj_mw_free space %llu\n", freespace);

                            if (filelen > freespace + 100 * 1024)
                            {
                                __ERR("filelen = %lu, freespace %lld, dangerous, egnore!\n", filelen, freespace);
                                iRet = -5;
                            }
                            else
                            {
                                snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, szFileName);
                                __ERR("local_file %s\n", local_file);
                            }
                        }
                    }
                    else if (file_type == UPLOAD_CERTIFICATION_FILE_TYPE)
                    {
                        unsigned int freespace = (unsigned int)GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
                        if (freespace < UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE)
                        {
                            __ERR("freespace of data partition is %u, too dangrous, now alowed to upload!\n", freespace);
                            iRet = -7;
                        }
                        else
                        {
                            snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_CERTIFICATION_FILE_NAME);
                            __ERR("local_file %s\n", local_file);
                        }
                    }
                    else if (file_type == UPLOAD_KEY_FILE_TYPE)
                    {
                        unsigned int freespace = (unsigned int)GetPathFreeSpace(DATA_BLOCK_MOUNT_PATH);
                        if (freespace < UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE)
                        {
                            __ERR("freespace of data partition is %u, too dangrous, now alowed to upload!\n", freespace);
                            iRet = -7;
                        }
                        else
                        {
                            snprintf(local_file, sizeof(local_file), "%s/%s", DATA_BLOCK_MOUNT_PATH, UPLOAD_KEY_FILE_NAME);
                            __ERR("local_file %s\n", local_file);
                        }
                    }
                    else if (file_type == UPLOAD_FIRMWARE_FILE_TYPE)
                    {
                        if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
                        {
                            char cmd[100] = {0};
                            snprintf(cmd, sizeof(cmd), "echo -e -n \"\\xFC\\x01\\xA3\\x01\\x00\\xA1\" > /dev/ttyS2");
                            anj_mw_system(cmd);
                        }
                        char *szFileName = GetFileNameFromFullName(upload_file);
                        if (szFileName && szFileName[0])
                        {
                            snprintf(local_file, sizeof(local_file), "%s", szFileName);
                        }
                        if (szFileName)
                            free(szFileName);
                    }

                    if (iRet == 0)
                    {
                        iRet = file_recver_init(local_file, filelen, file_type, lognum, MsgSrc);
                        if (iRet == 0)
                        {
                            // tell client which port we use to transport file
                            msg_body = anj_mw_malloc(1024);
                            MediaStreamConfig *pstStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
                            int port = (MsgSrc == MSG_SRC_SER) ? 8091 : pstStreamCfg->commConfig.ptzPort;
                            sprintf(msg_body, "<RESPONSE_PARAM\nPort=\"%d\"\nType=\"1\"\n/>", port);
                        }
                    }
                }
            }
        }
    }
    break;
    case CMD_DOWNLOAD_FILE:
    {
        if (MsgSrc == MSG_SRC_PRI)
        {
            request_param = GetRequestParamValue(pDoc, (char *)"FileName");
            if (request_param == NULL)
                iRet = -1;
            else
            {
                __ERR("download file: %s\n", request_param);

                if (strncmp(request_param, "/mnt/", 4) != 0 &&
                    strncmp(request_param, "/tmp/", 4) != 0 &&
                    strncmp(request_param, "/data/", 6) != 0 &&
                    strstr(request_param, ".mp3") == NULL &&
                    strstr(request_param, ".wav") == NULL &&
                    strstr(request_param, ".xml") == NULL)
                {
                    __ERR("try to download nodata file, rejected it.\n");
                    iRet = -3;
                }
                else
                {
                    unsigned long long flen = 0;
                    char *filename = request_param;
                    char defaultConfigFile[256] = {0};

                    // check file existed or not
                    if (strstr(filename, "defaultconfig.xml") != 0)
                    {
                        char *pDefFile = anj_config_default_file_get();
                        if (pDefFile != NULL && pDefFile[0] != 0)
                        {
                            snprintf(defaultConfigFile, sizeof(defaultConfigFile), "%s", pDefFile);
                            filename = toLowerStr(defaultConfigFile);
                        }
                    }

                    iRet = anj_mw_read_file_len(filename, &flen);

                    if (iRet >= 0) // else
                    {
                        char *startpos_param = GetRequestParamValue(pDoc, (char *)"StartPos");
                        if (startpos_param == NULL) // old version client
                        {
                            unsigned short port = anj_pri_cmd_port_get(0); // for tcp; find_free_tcp_port(StreamCfg.commConfig.ptzPort);
                            if (port == 0)
                            {
                                __ERR("find anj_mw_free tcp port failed.\n");
                                iRet = -4;
                            }
                            else
                            {
                                iRet = file_sender_transport(0, 0, filename, NULL, port, lognum);

                                // tell client which port we use to transport file
                                msg_body = anj_mw_malloc(1024);
                                sprintf(msg_body, "<RESPONSE_PARAM\nPort=\"%d\"\n/>", port);
                            }
                        }
                        else
                        {
                            unsigned long startpos = atol(startpos_param);
                            anj_mw_free(startpos_param);

                            __ERR("startpos = %lu, using same port to send file!!!\n", startpos);
                            iRet = file_sender_init(filename, startpos, lognum);
                            if (iRet == 0)
                            {
                                msg_body = anj_mw_malloc(1024);
                                MediaStreamConfig *pstStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
                                int port = pstStreamCfg->commConfig.ptzPort;
                                sprintf(msg_body, "<RESPONSE_PARAM\nPort=\"%d\"\nType=\"1\"\nFileLength=\"%u\"\n/>",
                                        port, (unsigned int)flen);
                            }
                        }
                    }
                }
                anj_mw_free(request_param);
            }
        }
    }
    break;
    case CMD_REMOVE_FILE:
    {
        request_param = GetRequestParamValue(pDoc, (char *)"FileName");
        if (request_param != NULL)
        {
            __ERR("Got cmd to remove file: %s\n", request_param);

            remove(request_param);
            anj_mw_free(request_param);
        }
        else
        {
            iRet = -1;
        }
    }
    break;
    case CMD_OTA_CHECK_UPDATE:
    {
        msg_body = anj_mw_malloc(4096);
        int status = 0;
        char latestversion[32] = {0};
        char releasenotes[512] = {0};

        ota_check_version(0);
        ota_version_get(&status, latestversion, releasenotes);

        snprintf(msg_body, 4096 - 1,
                 "<RESPONSE_PARAM Result=\"%d\" LatestVersion=\"%s\" ReleaseNotes=\"%s\"></RESPONSE_PARAM>\n",
                 status, latestversion, releasenotes);
    }
    break;
    case CMD_OTA_START_UPGRADE:
    {
        msg_body = anj_mw_malloc(1024);
        sprintf(msg_body, "<RESPONSE_PARAM\nResult=\"%d\"\n/>", ota_check_version(1));
    }
    break;
    case CMD_SET_REPLAY_FILENAME:
    {
    }
    break;
    case CMD_GET_LOGFILE_LIST:
    {
        int count;
        struct tm log_start_time;
        struct tm log_end_time;

        GetTimeFromString((char *)"19000101 00:00:00", &log_start_time);
        GetTimeFromString((char *)"29000101 00:00:00", &log_end_time);

        LOG_INDEX_FILE_ENTRY *pLogFileList = anj_mw_log_filelist_get(log_start_time, log_end_time, &count);
        if ((pLogFileList != NULL) && (count > 0))
        {
            int body_size = count * 256 + 256;
            char *pe;
            char *pb;

            msg_body = anj_mw_malloc(body_size);
            pb = msg_body;
            pe = msg_body + body_size - 1;

            __ERR("log file count: %d\n", count);

            pb += snprintf(pb, pe - pb, "<RESPONSE_PARAM FileCount=\"%d\">\n", count);
            for (int i = 0; i < count; i++)
            {
                pb += snprintf(pb, pe - pb, "<LOG_FILE\n");
                pb += snprintf(pb, pe - pb, "FilePath=\"%s\"\n", pLogFileList[i].log_filename);
                pb += snprintf(pb, pe - pb, "FileLength=\"%d\"\n", pLogFileList[i].file_length);
                pb += snprintf(pb, pe - pb, "/>\n");
            }
            pb += snprintf(pb, pe - pb, "</RESPONSE_PARAM>");

            anj_mw_free(pLogFileList);
        }
        else
        {
            msg_body = anj_mw_malloc(1024);
            strcpy(msg_body, "<RESPONSE_PARAM>\n</RESPONSE_PARAM>");
        }
    }
    break;
    case CMD_ALARM_OUTPUT_CONTROL:
    {
        char *strChannel = GetRequestParamValue(pDoc, (char *)"ChannelNo");
        char *strOutput = GetRequestParamValue(pDoc, (char *)"Output");
        int chn = 0;
        int output = 0;

        if (strChannel)
        {
            chn = atoi(strChannel);
            anj_mw_free(strChannel);
        }

        if (strOutput)
        {
            output = atoi(strOutput);
            anj_mw_free(strOutput);

            // 这里实测下发chn为0~3，需要转换成1~4
            chn = chn + 1;
            anj_mw_hwctrl_alarmout_chn_status_set(chn, output);

            iRet = 0;
        }
        else
        {
            iRet = -1;
        }
    }
    break;
    case CMD_SNAP_JPEG_PICTURE:
    {
        char *strStream = GetRequestParamValue(pDoc, (char *)"Stream");
        char *strCamera = GetRequestParamValue(pDoc, (char *)"Camera");
        char *strQuality = GetRequestParamValue(pDoc, (char *)"Quality");
        char *strPicId = GetRequestParamValue(pDoc, (char *)"PictureId");

        char *strftpServer = GetRequestParamValue(pDoc, (char *)"FtpServer");
        char *strftpPort = GetRequestParamValue(pDoc, (char *)"FtpPort");
        char *strftpUser = GetRequestParamValue(pDoc, (char *)"FtpUser");
        char *strftpPass = GetRequestParamValue(pDoc, (char *)"FtpPass");
        char *strftpPath = GetRequestParamValue(pDoc, (char *)"FtpPath");

        int Stream = 0;
        int Camera = 0;
        int Quality = 80;
        char jpgFile[256];
        unsigned short ftpPort = 25;

        if (strCamera)
        {
            Camera = atoi(strCamera);
            if (Camera < 0 || Camera >= ANJ_CAMERA_MAX_NUMS)
                Camera = 0;
        }

        if (strStream)
        {
            Stream = atoi(strStream);
            if (Stream < 0 || Stream > 1)
                Stream = 0;
        }

        if (strQuality)
        {
            Quality = atoi(strQuality);
            if (Quality < 50 || Quality > 99)
                Quality = 80;
        }

        struct timeval tv;
        SystemGetTimeofRun(&tv, NULL);
        struct tm ptm;
        SystemLocalTime(&ptm);

        if (strPicId)
        {
            sprintf(jpgFile, "%s_%04d%02d%02d%02d%02d%02d_%03d.jpg",
                    strPicId,
                    ptm.tm_year + 1900,
                    ptm.tm_mon + 1,
                    ptm.tm_mday,
                    ptm.tm_hour,
                    ptm.tm_min,
                    ptm.tm_sec,
                    (int)(tv.tv_usec / 1000));
        }
        else
        {
            sprintf(jpgFile, "%04d%02d%02d%02d%02d%02d_%03d.jpg",
                    ptm.tm_year + 1900,
                    ptm.tm_mon + 1,
                    ptm.tm_mday,
                    ptm.tm_hour,
                    ptm.tm_min,
                    ptm.tm_sec,
                    (int)(tv.tv_usec / 1000));
        }

        if (strftpServer && strftpUser && strftpPass && strftpPath)
        {
            if (strftpPort)
                ftpPort = atoi(strftpPort);
            if (ftpPort <= 0 || ftpPort > 65535)
                ftpPort = 25;
        }

        anj_mw_system("rm /tmp/*.jpg");

        if (anj_snap_jpg(Camera, Stream, Quality, (char *)"/tmp", jpgFile, NULL)) // success return 0
            iRet = -1;

        if (strStream)
            anj_mw_free(strStream);
        if (strCamera)
            anj_mw_free(strCamera);
        if (strQuality)
            anj_mw_free(strQuality);
        if (strPicId)
            anj_mw_free(strPicId);
        if (strftpServer)
            anj_mw_free(strftpServer);
        if (strftpPort)
            anj_mw_free(strftpPort);
        if (strftpUser)
            anj_mw_free(strftpUser);
        if (strftpPass)
            anj_mw_free(strftpPass);
        if (strftpPath)
            anj_mw_free(strftpPath);
    }
    break;
    case CMD_GET_WIFI_AP_INFO:
    {
        WIFI_AP_SCAN wifiApScan = {0};
        iRet = anj_net_wifi_ap_info_get(&wifiApScan);
        if (iRet == 0 && wifiApScan.apCnt > 0)
        {
            int body_size = wifiApScan.apCnt * 256 + 256;
            char *pe;
            char *pb;
            char essid_gb_str[MAX_WIRELESS_ESSID_NAME_LEN * 2 + 4];

            msg_body = anj_mw_malloc(body_size);
            if (msg_body != NULL)
            {
                pb = msg_body;
                pe = msg_body + body_size - 1;

                pb += snprintf(pb, pe - pb, "<RESPONSE_PARAM ApCount=\"%d\">\n", wifiApScan.apCnt);
                for (int i = 0; i < wifiApScan.apCnt; i++)
                {
                    int conv_ret = utf8_to_gb2312(wifiApScan.apInfos[i].ssid, essid_gb_str);
                    if (conv_ret != 0 && strlen(wifiApScan.apInfos[i].ssid) > 0)
                    {
                        StrCpy(essid_gb_str, sizeof(essid_gb_str), wifiApScan.apInfos[i].ssid);
                        __INFO("wifi[%d] essid:%s maybe use gb format, copy for xml!\n", i, essid_gb_str);
                    }

                    char escapeBuf[4096] = {0};
                    pb += snprintf(pb, pe - pb, "<WifiAp\n");
                    pb += snprintf(pb, pe - pb, "Ssid=\"%s\"\n", copy_with_escape(escapeBuf, essid_gb_str));
                    pb += snprintf(pb, pe - pb, "WirelessMode=\"%s\"\n", wifiApScan.apInfos[i].wirelessMode);
                    pb += snprintf(pb, pe - pb, "AuthMode=\"%s\"\n", anj_net_wifi_auth_str(wifiApScan.apInfos[i].authMode));
                    pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\n", anj_net_wifi_encrypt_str(wifiApScan.apInfos[i].encryType));
                    pb += snprintf(pb, pe - pb, "Quality=\"%d\"\n", wifiApScan.apInfos[i].quality);
                    pb += snprintf(pb, pe - pb, "SignalLevel=\"%d\"\n", wifiApScan.apInfos[i].signalLevel);
                    pb += snprintf(pb, pe - pb, "NoiseLevel=\"%d\"\n", wifiApScan.apInfos[i].noiseLevel);
                    pb += snprintf(pb, pe - pb, "/>\n");
                }
                pb += snprintf(pb, pe - pb, "</RESPONSE_PARAM>");
            }
        }
        else
        {
            msg_body = anj_mw_malloc(1024);
            if (msg_body != NULL)
                strcpy(msg_body, "<RESPONSE_PARAM ApCount=\"0\">\n</RESPONSE_PARAM>");
        }

        iRet = (msg_body != NULL) ? iRet : -1;
    }
    break;
    case CMD_LIGHT_CTRL:
    {
        char *strIndex = NULL;
        char *strOpen = NULL;
        char *strBrightness = NULL;

        int index = 0;
        int open = 0;
        int brightness = 0;

        strIndex = GetRequestParamValue(pDoc, (char *)"index");
        strOpen = GetRequestParamValue(pDoc, "open");
        strBrightness = GetRequestParamValue(pDoc, "brightness");

        if (strIndex && strOpen && strBrightness)
        {
            iRet = 0;
            index = atoi(strIndex);
            open = atoi(strOpen);
            brightness = atoi(strBrightness);

            if (open == 0 && brightness != 0)
            {
                brightness = 0;
            }
            else if (open == 1 && brightness == 0)
            {
                brightness = 100;
            }

            __INFO("light ctrl: index:%d, open:%d, brightness:%d\n", index, open, brightness);

            if (index == LIGHT_INDEX_WLED || index == LIGHT_INDEX_RLED)
            {
                anj_ispctl_light_manual_ctrl(index, brightness);
            }
            else if (index == LIGHT_INDEX_RB_ALARM)
            {
                if (brightness > 0)
                    anj_mw_hwctrl_alarmled_open();
                else
                    anj_mw_hwctrl_alarmled_close();
            }
        }
        else
        {
            iRet = -1;
        }

        if (strIndex)
            anj_mw_free(strIndex);
        if (strOpen)
            anj_mw_free(strOpen);
        if (strBrightness)
            anj_mw_free(strBrightness);
    }
    break;
    case CMD_MSG_SCARE_OFF:
    {
        char *strPlaySound = NULL;
        int playSound = 0;

        char *strPlayTime = NULL;
        int playtime = 0;

        char *strLedOn = NULL;
        int ledon = 0;

        strPlaySound = GetRequestParamValue(pDoc, (char *)"play_sound");
        if (strPlaySound)
        {
            playSound = atoi(strPlaySound);
            anj_mw_free(strPlaySound);
        }

        strPlayTime = GetRequestParamValue(pDoc, (char *)"time");
        if (strPlayTime)
        {
            playtime = atoi(strPlayTime);
            anj_mw_free(strPlayTime);
        }

        strLedOn = GetRequestParamValue(pDoc, (char *)"led_on");
        if (strLedOn)
        {
            ledon = atoi(strLedOn);
            anj_mw_free(strLedOn);
        }

        // 老架构中 time参数没有处理，调用一次就播放一次语音和亮一次报警灯
        if (playtime != 1)
        {
            playtime = 1;
        }

        if (1 == playSound)
        {
            AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
            const char *pFileName = strGetFilename(pstAlarmCfg->aiAlarm.pdAlarm[0].alarmAction.audioAction.filename);
            if (pFileName && strcmp(pFileName, UPLOAD_MP3_FILE_NAME) == 0)
            {
                anj_audio_prompt_play(DATA_BLOCK_MOUNT_PATH, UPLOAD_MP3_FILE_NAME, playtime);
            }
            else
            {
                anj_audio_prompt_play(ANJ_MP3_ALARM_PATH, (char *)pFileName, playtime);
            }
        }

        if (1 == ledon)
        {
            anj_mw_hwctrl_alarmled_toggle_delay(10 * 1000);
        }

        iRet = 0;
    }
    break;
    case CMD_SET_STITCH_MODE:
    {
        // char *strWorkMode = NULL;
        // int workMode = -1;

        // char *strOptimumDistance = NULL;
        // int optimumDistance = -1;

        // char *strLedOn = NULL;
        // int ledon = 0;

        // strWorkMode = GetRequestParamValue(pDoc, (char *)"TwoLensWorkMode");
        // if (strWorkMode)
        // {
        //     workMode = atoi(strWorkMode);
        //     anj_mw_free(strWorkMode);
        // }

        // strOptimumDistance = GetRequestParamValue(pDoc, (char *)"OptimumDistance");
        // if (strOptimumDistance)
        // {
        //     optimumDistance = atoi(strOptimumDistance);
        //     anj_mw_free(strOptimumDistance);
        // }

        // MsgSetStitchMode(workMode, optimumDistance);
    }
    break;
    case CMD_SET_PTZ_DIRECTION:
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        IotPtzConfig stIotPtzConfig = *pstIotPtzConfig;
        char *strLevelDire = NULL;
        char *strVertDire = NULL;

        int levelDire = 0;
        int vertDire = 0;

        strLevelDire = GetRequestParamValue(pDoc, (char *)"level_direction");
        if (strLevelDire)
        {
            levelDire = atoi(strLevelDire);
            anj_mw_free(strLevelDire);
            strLevelDire = NULL;
        }

        strVertDire = GetRequestParamValue(pDoc, (char *)"vert_direction");
        if (strVertDire)
        {
            vertDire = atoi(strVertDire);
            anj_mw_free(strVertDire);
            strVertDire = NULL;
        }

        stIotPtzConfig.m_ptzDir.HDir = levelDire;
        stIotPtzConfig.m_ptzDir.VDir = vertDire;
        anj_ptz_config_save(&stIotPtzConfig);
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_DIRECTION, &event_result, NULL);
    }
    break;
    case CMD_SET_PTZ_SPEED:
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        IotPtzConfig stIotPtzConfig = *pstIotPtzConfig;

        char *strLevelStep = GetRequestParamValue(pDoc, (char *)"level_step");
        if (strLevelStep)
        {
            stIotPtzConfig.m_MaxStep.HStep = atoi(strLevelStep);
            anj_mw_free(strLevelStep);
            strLevelStep = NULL;
        }
        char *strLevelSpeed = GetRequestParamValue(pDoc, (char *)"level_speed");
        if (strLevelSpeed)
        {
            stIotPtzConfig.m_ptzSpeed.HSpeed = atoi(strLevelSpeed);
            anj_mw_free(strLevelSpeed);
            strLevelSpeed = NULL;
        }

        char *strVertStep = GetRequestParamValue(pDoc, (char *)"vert_step");
        if (strVertStep)
        {
            stIotPtzConfig.m_MaxStep.VStep = atoi(strVertStep);
            anj_mw_free(strVertStep);
            strVertStep = NULL;
        }
        char *strVertSpeed = GetRequestParamValue(pDoc, (char *)"vert_speed");
        if (strVertSpeed)
        {
            stIotPtzConfig.m_ptzSpeed.VSpeed = atoi(strVertSpeed);
            anj_mw_free(strVertSpeed);
            strVertSpeed = NULL;
        }

        char *str_left_down_x = NULL;
        char *str_left_down_y = NULL;
        char *str_right_up_x = NULL;
        char *str_right_up_y = NULL;

        str_left_down_x = GetRequestParamValue(pDoc, (char *)"left_down_x");
        if (str_left_down_x)
        {
            stIotPtzConfig.m_3dOrientStep.left_down_point.HStep = atoi(str_left_down_x);
            anj_mw_free(str_left_down_x);
            str_left_down_x = NULL;
        }
        str_left_down_y = GetRequestParamValue(pDoc, (char *)"left_down_y");
        if (str_left_down_y)
        {
            stIotPtzConfig.m_3dOrientStep.left_down_point.VStep = atoi(str_left_down_y);
            anj_mw_free(str_left_down_y);
            str_left_down_y = NULL;
        }
        str_right_up_x = GetRequestParamValue(pDoc, (char *)"right_up_x");
        if (str_right_up_x)
        {
            stIotPtzConfig.m_3dOrientStep.right_up_point.HStep = atoi(str_right_up_x);
            anj_mw_free(str_right_up_x);
            str_right_up_x = NULL;
        }
        str_right_up_y = GetRequestParamValue(pDoc, (char *)"right_up_y");
        if (str_right_up_y)
        {
            stIotPtzConfig.m_3dOrientStep.right_up_point.VStep = atoi(str_right_up_y);
            anj_mw_free(str_right_up_y);
            str_right_up_y = NULL;
        }

        char *str_level_ratio = NULL;
        char *str_vert_ratio = NULL;

        // 电机减速比暂时不适配 后期看是否通过maxstep和speed就可以调节
        str_level_ratio = GetRequestParamValue(pDoc, (char *)"level_ratio");
        if (str_level_ratio)
        {
            anj_mw_free(str_level_ratio);
            str_level_ratio = NULL;
        }

        str_vert_ratio = GetRequestParamValue(pDoc, (char *)"vert_ratio");
        if (str_vert_ratio)
        {
            anj_mw_free(str_vert_ratio);
            str_vert_ratio = NULL;
        }

        anj_ptz_config_save(&stIotPtzConfig);
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_CHANGE_SPEED, &event_result, NULL);

        iRet = 0;
    }
    break;

    case CMD_PTZ_CTRL_TEST:
    {
        EventResult event_result = {0};
        PtzCmdParse stPtzCmdParse = {0};
        strncpy(stPtzCmdParse.ptzCmd, "PtzRestore", sizeof(stPtzCmdParse.ptzCmd));
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);

        iRet = 0;
    }
    break;

    case CMD_PTZ_GET_USRER_CONFIG:
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        IotPtzConfig stIotPtzConfig = *pstIotPtzConfig;

        msg_body = anj_mw_malloc(256);
        sprintf(msg_body, "<RESPONSE_PARAM level_step=\"%d\" vert_step=\"%d\"  level_speed=\"%d\"  vert_speed=\"%d\" default_level_step=\"%d\" default_vert_step=\"%d\"  default_level_speed=\"%d\"  default_vert_speed=\"%d\"   vert_direction=\"%d\" level_direction=\"%d\" > </RESPONSE_PARAM>\r\n",
                stIotPtzConfig.m_MaxStep.HStep, stIotPtzConfig.m_MaxStep.VStep,
                stIotPtzConfig.m_ptzSpeed.HSpeed, stIotPtzConfig.m_ptzSpeed.VSpeed,
                stIotPtzConfig.m_resetStep.HStep, stIotPtzConfig.m_resetStep.VStep,
                stIotPtzConfig.m_ptzSpeed.HSpeed, stIotPtzConfig.m_ptzSpeed.VSpeed,
                stIotPtzConfig.m_ptzDir.VDir, stIotPtzConfig.m_ptzDir.HDir);

        iRet = 0;
    }
    break;
    case CMD_SET_IMAGE_FLIP:
    {
        char *strFLip = NULL;
        int flip = 0;
        strFLip = GetRequestParamValue(pDoc, (char *)"Flip");
        if (strFLip)
        {
            flip = atoi(strFLip);
            anj_mw_free(strFLip);
        }
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            VideoCaptureCfg *pstVideoCaptureCfg = &pstMediaConfig->videoConfig[cameraIndex].videoCapture;
            int hflip = 0;
            int vflip = 0;
            if (flip == 3 || flip == 1)
            {
                hflip = !pstVideoCaptureCfg->hflip;
            }

            if (flip == 3 || flip == 2)
            {
                vflip = !pstVideoCaptureCfg->vflip;
            }
            iRet = anj_config_meida_flip_set(hflip, vflip, cameraIndex);
        }

        anj_factory_image_flip_cfg_save(flip);
    }
    break;
    case CMD_GET_PTZ_PRESET:
    {
        msg_body = anj_mw_malloc(2048);
        sprintf(msg_body, "<RESPONSE_PARAM> </RESPONSE_PARAM>\r\n");
        iRet = 0;
    }
    break;
    case CMD_GET_PTZ_STATUS:
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_MOVE_STATUS, &event_result, NULL);

        msg_body = anj_mw_malloc(128);
        sprintf(msg_body, "<RESPONSE_PARAM ptz_status=\"%d\" />\r\n", event_result.ret);

        iRet = 0;
    }
    break;
    case CMD_TEST_MIC_AND_SPEAKER:
    {
        anj_audio_aiao_test_start();
        iRet = 0;
    }
    break;
    case CMD_RPIVACY_PROTECTION:
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        int bLensCover = anj_osd_lens_cover_get();

        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            VideoMaskConfig stVideoMaskConfig = pstMediaConfig->videoConfig[cameraIndex].videoMask;
            if (bLensCover)
            {
                memset(&stVideoMaskConfig.mainStreamMaskList[0], 0, sizeof(stVideoMaskConfig.mainStreamMaskList[0]));
                memset(&stVideoMaskConfig.subStreamMaskList[0], 0, sizeof(stVideoMaskConfig.subStreamMaskList[0]));
            }
            else
            {
                stVideoMaskConfig.mainStreamMaskList[0].xPos = 0;
                stVideoMaskConfig.mainStreamMaskList[0].yPos = 0;
                stVideoMaskConfig.mainStreamMaskList[0].width = OSD_IOT_COORDINATE_RATIO;
                stVideoMaskConfig.mainStreamMaskList[0].height = OSD_IOT_COORDINATE_RATIO;

                stVideoMaskConfig.subStreamMaskList[0].xPos = 0;
                stVideoMaskConfig.subStreamMaskList[0].yPos = 0;
                stVideoMaskConfig.subStreamMaskList[0].width = OSD_IOT_COORDINATE_RATIO;
                stVideoMaskConfig.subStreamMaskList[0].height = OSD_IOT_COORDINATE_RATIO;
            }
            iRet |= anj_config_video_mask_set(&stVideoMaskConfig, cameraIndex);
        }
    }
    break;
    case CMD_SET_WEB_DEFAULT_LANGUAGE_KC:
    {
    }
    break;
    case CMD_GET_ZOOM_CFG:
    {
        int max_mutiple = 30;
        int disp_multiple = 6;
        DOUBLE_AREA_ENTRY cur_area = {0};
        int cur_mutiple = 10;
        int iRet = anj_zoom_run_get(0, &cur_area, NULL);
        if (iRet == 0)
        {
            cur_mutiple = cur_area.width * 10;
        }
        msg_body = anj_mw_malloc(256);
        sprintf(msg_body, "<RESPONSE_PARAM cur_multiple=\"%d\" max_multiple=\"%d\"  multiple_disp=\"%d\" > </RESPONSE_PARAM>\r\n",
                cur_mutiple, max_mutiple, disp_multiple);

        iRet = 0;
    }
    break;
    case CMD_SET_BR_LIGHT: // 设置红蓝警灯使能状态
    {
        char *modeStr = GetRequestParamValue(pDoc, "br_light");
        int nMode = -1;
        if (modeStr != NULL)
        {
            nMode = atoi(modeStr) > 0 ? 1 : 0;
            anj_mw_free(modeStr);
        }

        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        AlarmConfig stAlarmCfg = *pstAlarmConfig;
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];
            if (nMode > 0 && pstPdAlarm->alarmAction.alarm_led_enable.enable_flag != nMode)
            {
                pstPdAlarm->alarmAction.alarm_led_enable.enable_flag = (ArmingMode)nMode;
            }
        }
        iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
        break;
    }
    case CMD_GET_BR_LIGHT: // 获取红蓝警灯使能状态
    {
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        int bEnable = 0;
        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            PdAlarm *pstPdAlarm = &pstAlarmConfig->aiAlarm.pdAlarm[cameraIndex];
            if (pstPdAlarm->alarmAction.alarm_led_enable.enable_flag)
            {
                bEnable = 1;
                break;
            }
        }
        msg_body = anj_mw_malloc(256);
        sprintf(msg_body, "<RESPONSE_PARAM\nbr_light=\"%u\"\n/>", bEnable);
    }
    break;
    case CMD_SET_MTU_VALUE: // 设置MTU值
    {
        // 解析XML的请求
        char *mtuStr = GetRequestParamValue(pDoc, (char *)"mtu_value");
        int mtu_value = -1;
        if (mtuStr != NULL)
        {
            mtu_value = atoi(mtuStr);
            anj_mw_free(mtuStr);
        }
        const char *ifname = WIRE_INTERFACE_NAME;
        int iRet = net_set_mtu(ifname, mtu_value);
        if (iRet != -1)
            __INFO("net_set_mtu cuccess!!\n");

        int mtu = net_get_mtu(ifname);
        __INFO("mtu:%d\n", mtu);
    }
    break;
    case CMD_GET_MTU_VALUE: // 获取MTU值
    {
        msg_body = anj_mw_malloc(256);
        const char *ifname = WIRE_INTERFACE_NAME;
        int mtu = net_get_mtu(ifname);
        __INFO("mtu:%d\n", mtu);
        if(-1 == mtu)
        {
            mtu = 1400; //获取不到的时候，默认1400
        }
        sprintf(msg_body, "<RESPONSE_PARAM\nmtu_value=\"%d\"\n/>", mtu);
    }
    break;
    case CMD_SET_PTZ_RE_CALIBRAT:
    {
        char *numStr = GetRequestParamValue(pDoc, "ptz_calibrat");

        if (numStr != NULL)
        {
            __INFO("ptz_calibrat: %s", numStr);
            // todo
            // int fix_x = 0;
            // int fix_y = 0;
            // char buf[64] = {0};

            // if (numStr == "0")
            // {
            //     mysystem("rm /tmp/flag.ptzCalibrat.test");

            //     mysystem("echo 3 > /mnt/nand/manual_ptz_speed");
            //     AuxMsgPTZSpeedReset();
            // }

            // if (numStr == "1")
            // {
            //     mysystem("echo 1 > /mnt/nand/manual_ptz_speed");
            //     AuxMsgPTZSpeedReset();

            //     mysystem("touch /tmp/flag.ptzreset.test");
            //     mysystem("touch /tmp/flag.ptzCalibrat.test");
            // }

            // if (numStr != "2")
            // {
            //     break;
            // }

            // FILE *pfile = fopen("/mnt/nand/ptzCalibrat.cfg", "w+");
            // if (pfile == NULL)
            // {
            //     printf("ptzCalibrat.cfg open file fail!\n");
            //     break;
            // }

            // AuxMsgPTZGetStepXY(&fix_x, &fix_y);

            // sprintf(buf, "centerpos(%d,%d)", fix_x, fix_y);

            // fwrite(buf, 1, strlen(buf), pfile);

            // fclose(pfile);
            // pfile = NULL;

            // mysystem("rm /tmp/flag.ptzCalibrat.test");

            // mysystem("echo 3 > /mnt/nand/manual_ptz_speed");
            // AuxMsgPTZSpeedReset();
            anj_mw_free(numStr);
        }
    }
    break;
    case CMD_GET_POWER_VALUE:
    {
        msg_body = anj_mw_malloc(256);
        int cap = 0;

        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_BATTERY_CAP_GET, &event_result, &cap);
        sprintf(msg_body, "<RESPONSE_PARAM\npower_value=\"%d\"\n/>", cap);
    }
    break;
    case CMD_SEND_CMD: // 请求发送报警
    {
        // SYSTEM_TIME system_time;
        // struct timeval tv;
        // struct tm *ptm;
        // gettimeofday(&tv, NULL);
        // ptm = localtime(&tv.tv_sec);

        // system_time.year = (1900 + ptm->tm_year);
        // system_time.month = (1 + ptm->tm_mon);
        // system_time.day = ptm->tm_mday;
        // system_time.hour = ptm->tm_hour;
        // system_time.minute = ptm->tm_min;
        // system_time.second = ptm->tm_sec;

        // char *AlarmStr = GetRequestParamValue(pDoc, "AlarmStr");
        // if (AlarmStr != NULL)
        // {
        //     __INFO("AlarmStr:%s\n", AlarmStr);
        //     todo
        //     P2pMsgReportAlarm(&system_time, ALARM_CODE_VIDEO_AI, ALARM_FLAG_OCCUR, ALARM_AI_FACEDETECT, AlarmStr, "");
        //     anj_mw_free(AlarmStr);
        // }
    }
    break;
    case CMD_CTRL_PTZ: // 控制云台
    {
        char *modeStr_x = GetRequestParamValue(pDoc, "ptz_ctnrol_x");
        char *modeStr_y = GetRequestParamValue(pDoc, "ptz_ctnrol_y");
        if ((modeStr_x != NULL) && (modeStr_y != NULL))
        {
            EventResult event_result = {0};
            PtzCmdParse stPtzCmdParse = {0};
            strncpy(stPtzCmdParse.ptzCmd, "move3DPoint", sizeof(stPtzCmdParse.ptzCmd));
            stPtzCmdParse.posX = atoi(modeStr_x);
            stPtzCmdParse.posY = atoi(modeStr_y);
            eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
            anj_mw_free(modeStr_x);
            anj_mw_free(modeStr_y);
        }
    }
    break;
    case CMD_SET_APP_TYPE: // 设置APP类型:xxx:0  --- anj 后台        1  --- 创维后台
    {
        char *pApptype = GetRequestParamValue(pDoc, (char *)"appType");
        if (pApptype != NULL)
        {
            __INFO("Set APP type %s\n", pApptype);
            anj_mw_write_file(BIND_APP_TYPE_FILE, 0, pApptype, strlen(pApptype));
            anj_mw_free(pApptype);
        }
    }
    break;
    case CMD_SET_TEMP_HUMIDITY:
    {
        __ERR("case CMD_SET_TEMP_HUMIDITY-----\n");
        // TempHumidityAlarm pAlm;
        // char *modeStr_temp_enable = GetRequestParamValue(pDoc, (char *)"temp_enable");
        // if (modeStr_temp_enable != NULL)
        // {
        //     int temp_enable = atoi(modeStr_temp_enable);
        //     pAlm.temp_enable = temp_enable;
        //     anj_mw_free(modeStr_temp_enable);
        // }

        // char *modeStr_temp_upper_limit = GetRequestParamValue(pDoc, (char *)"temp_upper_limit");
        // if (modeStr_temp_upper_limit != NULL)
        // {
        //     int temp_upper_limit = atoi(modeStr_temp_upper_limit);
        //     pAlm.temp_upper_limit = temp_upper_limit;
        //     anj_mw_free(modeStr_temp_upper_limit);
        // }

        // char *modeStr_temp_lower_limit = GetRequestParamValue(pDoc, (char *)"temp_lower_limit");
        // if (modeStr_temp_lower_limit != NULL)
        // {
        //     int temp_lower_limit = atoi(modeStr_temp_lower_limit);
        //     pAlm.temp_upper_limit = temp_lower_limit;
        //     anj_mw_free(modeStr_temp_lower_limit);
        // }

        // char *modeStr_humidity_enable = GetRequestParamValue(pDoc, (char *)"humidity_enable");
        // if (modeStr_humidity_enable != NULL)
        // {
        //     int humidity_enable = atoi(modeStr_humidity_enable);
        //     pAlm.humidity_enable = humidity_enable;
        //     anj_mw_free(modeStr_humidity_enable);
        // }

        // char *modeStr_humidity_upper_limit = GetRequestParamValue(pDoc, (char *)"humidity_upper_limit");
        // if (modeStr_humidity_upper_limit != NULL)
        // {
        //     int humidity_upper_limit = atoi(modeStr_humidity_upper_limit);
        //     pAlm.humidity_upper_limit = humidity_upper_limit;
        //     anj_mw_free(modeStr_humidity_upper_limit);
        // }

        // char *modeStr_humidity_lower_limit = GetRequestParamValue(pDoc, (char *)"humidity_lower_limit");
        // if (modeStr_humidity_lower_limit != NULL)
        // {
        //     int humidity_lower_limit = atoi(modeStr_humidity_lower_limit);
        //     pAlm.humidity_lower_limit = humidity_lower_limit;
        //     anj_mw_free(modeStr_humidity_lower_limit);
        // }

        // char *modeStr_voc_enable = GetRequestParamValue(pDoc, (char *)"voc_enable");
        // if (modeStr_voc_enable != NULL)
        // {
        //     int voc_enable = atoi(modeStr_voc_enable);
        //     pAlm.voc_enable = voc_enable;
        //     anj_mw_free(modeStr_voc_enable);
        // }

        // char *modeStr_voc_threashhold_good = GetRequestParamValue(pDoc, (char *)"voc_threashhold_good");
        // if (modeStr_voc_threashhold_good != NULL)
        // {
        //     int voc_threashhold_good = atoi(modeStr_voc_threashhold_good);
        //     pAlm.voc_threashhold_good = voc_threashhold_good;
        //     anj_mw_free(modeStr_voc_threashhold_good);
        // }

        // char *modeStr_voc_threashhold_TracePollution = GetRequestParamValue(pDoc, (char *)"voc_threashhold_TracePollution");
        // if (modeStr_voc_threashhold_TracePollution != NULL)
        // {
        //     int voc_threashhold_TracePollution = atoi(modeStr_voc_threashhold_TracePollution);
        //     pAlm.voc_threashhold_TracePollution = voc_threashhold_TracePollution;
        //     anj_mw_free(modeStr_voc_threashhold_TracePollution);
        // }

        // char *modeStr_voc_threashhold_LightPollution = GetRequestParamValue(pDoc, (char *)"voc_threashhold_LightPollution");
        // if (modeStr_voc_threashhold_LightPollution != NULL)
        // {
        //     int voc_threashhold_LightPollution = atoi(modeStr_voc_threashhold_LightPollution);
        //     pAlm.voc_threashhold_LightPollution = voc_threashhold_LightPollution;
        //     anj_mw_free(modeStr_voc_threashhold_LightPollution);
        // }

        // char *modeStr_voc_threashhold_ModeratePollution = GetRequestParamValue(pDoc, (char *)"voc_threashhold_ModeratePollution");
        // if (modeStr_voc_threashhold_ModeratePollution != NULL)
        // {
        //     int voc_threashhold_ModeratePollution = atoi(modeStr_voc_threashhold_ModeratePollution);
        //     pAlm.voc_threashhold_ModeratePollution = voc_threashhold_ModeratePollution;
        //     anj_mw_free(modeStr_voc_threashhold_ModeratePollution);
        // }

        // char *modeStr_voc_threashhold_HeavyPollution = GetRequestParamValue(pDoc, (char *)"voc_threashhold_HeavyPollution");
        // if (modeStr_voc_threashhold_HeavyPollution != NULL)
        // {
        //     int voc_threashhold_HeavyPollution = atoi(modeStr_voc_threashhold_HeavyPollution);
        //     pAlm.voc_threashhold_HeavyPollution = voc_threashhold_HeavyPollution;
        //     anj_mw_free(modeStr_voc_threashhold_HeavyPollution);
        // }

        // todo
        // MsgSetTempHumidityAlarm(&pAlm);
    }
    break;
    case CMD_TEST_MCU:
    {
        // todo
        // P2pMsgTestMcu();
    }
    break;

    // todo
    case CMD_TEST_SMS:
    {
        //     SMSAlarm data;
        //     SMSAlarm *pAlm = &data;
        //     memset(&data, 0, sizeof(data));
        //     if (0 == Alarm_getSMSByXml(&data, cmdbuf))
        //     {
        //         __ERR("SMSAlarm enable=%d min_interval=%d playresult=%d dstnumber=%s content=%s",
        //               pAlm->enable, pAlm->min_interval, pAlm->playresult, pAlm->szSmsDstNum, pAlm->szSmsFixContent);

        //         MsgTestSMS(data);
        //     }
    }
    break;
    case CMD_TEST_PTZ_LED_IRCUT:
    {
        //     pthread_t pthid;
        //     int iRet = pthread_create(&pthid, NULL, test_ptz_led_ircut_thread, NULL);
        //     if (iRet < 0)
        //     {
        //         __ERR("pthread_create test_ptz_led_ircut_thread error!!");
        //     }
    }
    break;
    case CMD_PING_DOMAINS:
    {
        //     PingDomainsMsg data;
        //     memset(&data, 0, sizeof(data));

        //     char *pNetworkFlag = GetRequestParamValue(pDoc, (char *)"iftype");
        //     if (pNetworkFlag != NULL)
        //     {
        //         if (strcmp(pNetworkFlag, "4G") == 0)
        //             data.nRouter = PING_ROUTE_4G;

        //         anj_mw_free(pNetworkFlag);
        //     }

        //     char *pDomains = GetRequestParamValue(pDoc, (char *)"domains");
        //     if (pDomains != NULL)
        //     {
        //         strncpy(data.szDomains, pDomains, DOMAINS_STRING_MAX_LEN - 1);

        //         __ERR("CMD_PING_DOMAINS type %d, %s", data.nRouter, data.szDomains);
        //         MsgPingDomains(&data); // PING域名，逗号分割
        //         anj_mw_free(pDomains);
        //     }
    }
    break;
    case CMD_MANUAL_FOUCS:
    {
        //     IXML_NodeList *pNodelist = NULL;
        //     pNodelist = ixmlDocument_getElementsByTagName(pDoc, (char *)"MANUAL_FOCUS_CMD");
        //     if (pNodelist == NULL)
        //     {
        //         // bad msg header
        //         ixmlDocument_free(pDoc);
        //         return -2;
        //     }
        //     else
        //     {
        //         ManualFocusStruct data;
        //         memset(&data, 0, sizeof(data));

        //         if (0 == ParseManualFocusXML(pNodelist->nodeItem, data))
        //         {
        //             MediaSetManualFocus(&data);
        //         }
        //     }
    }
    break;
    default:
        __ERR("unknown cmd: %d\n", msg_code);
        iRet = -1;
    }

    *data = msg_body;
    return iRet;
}

int anj_service_cmd_convert_xml(char *cmdbuf, char *root_name)
{
    int iRet = 0;
    ANJ_CHK((cmdbuf != NULL) && (root_name != NULL), -1, "input Invalid\n");

    if (strstr(cmdbuf, XML_ROOT_NAME1))
    {
        strcpy(root_name, XML_ROOT_NAME1);
    }
    else if (strstr(cmdbuf, XML_ROOT_NAME2))
    {
        strcpy(root_name, XML_ROOT_NAME2);
    }
    else if (strstr(cmdbuf, XML_ROOT_NAME3))
    {
        strcpy(root_name, XML_ROOT_NAME3);
    }
    else
    {
        __ERR("No valid XML root found in buffer: %s\n", cmdbuf);
        iRet = -1;
        goto endFunc;
    }
endFunc:
    return iRet;
}

int anj_service_cmd_parse_xml(IXML_Document *pDoc, char *MsgRoot, char *MsgType, char *MsgCode, char *MsgFlag, int *channel)
{
    int iRet = 0;
    ANJ_CHK((pDoc != NULL) && (MsgRoot != NULL) && (MsgType != NULL) && (MsgCode != NULL) && (MsgFlag != NULL) && (channel != NULL),
            -1, "input Invalid");

    IXML_NodeList *pRootNode = ixmlDocument_getElementsByTagName(pDoc, MsgRoot);
    ANJ_CHK((pRootNode != NULL), -1, "bad msg header");
    ixmlNodeList_free(pRootNode);

    const char *tagName = "MESSAGE_HEADER";
    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDoc, tagName);
    ANJ_CHK((pNodelist != NULL), -1, "bad msg header");

    //  nodeType
    IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
    while (tmp != NULL)
    {
        if (strcmp(tmp->nodeName, "Msg_type") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                strcpy(MsgType, tmp->nodeValue);
            }
        }
        else if (strcmp(tmp->nodeName, "Msg_code") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                strcpy(MsgCode, tmp->nodeValue);
            }
        }
        else if (strcmp(tmp->nodeName, "Msg_flag") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                strcpy(MsgFlag, tmp->nodeValue);
            }
        }
        else if (strcmp(tmp->nodeName, "Msg_channel") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                *channel = atoi(tmp->nodeValue);
                if (*channel > ANJ_CAMERA_MAX_NUMS || *channel < 0)
                    *channel = 0;
            }
        }

        tmp = tmp->nextSibling;
    }

    ixmlNodeList_free(pNodelist);
endFunc:
    return iRet;
}

REGISTER_MODULE(anj_service, MODULE_PRIORITY_SERVICE);
