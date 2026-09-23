#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>

#include "gb28181.h"
#include "anj_mbuf.h"
#include "anj_config.h"
#include "alarm_link.h"
#include "anj_systime.h"
#include "anj_sysmng.h"
#include "anj_mw_time.h"
#include "anj_mw_file.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_config_ptz.h"
#include "audio_receiver.h"
#include "record_log.h"

/* GB28181 SIP/PU 消息常量与数据结构（NET_SERVER_*、SET/QUERY_PU_* 等） */
#include "gb28181/gb_pu_lib.h"
#include "gb28181/gb_pu_param.h"

typedef enum
{
    GB_RES_1080P = 0,
    GB_RES_720P,
    GB_RES_768P,
    GB_RES_QVGA,
    GB_RES_VGA,
    GB_RES_D1,
    GB_RES_CIF,
    GB_RES_BUIT,
} gb_res_e;

typedef struct
{
    unsigned long id;
    int active;
    int wait_iframe;
    int audio_type;
    int channel;
    int request_index;
    int request_type;
    int stream_mode;
    unsigned char frame_seq;
} gb_stream_session_t;

static int s_bInitService = 0;
static sip_net_info s_sip_info;
static net_info_t s_net_info;
static gb_stream_session_t s_live[MAX_CHANNEL_NUM];
static gb_stream_session_t s_playback[MAX_CHANNEL_NUM];
static gb_stream_session_t s_download[MAX_CHANNEL_NUM];
static GB28181_stream_CB s_stream_if;
static GB28181_ptz_CB s_ptz_if;
static HistoryQueryInterfaceMultiType s_history_query_if;
static HistoryChannelInterfaceMultiTypeCB s_history_channel_if;
static media_codec_type_e s_audio_type_live = MEDIA_CODEC_NONE;
static char s_osd_text[256] = {0};

static int gb_stop_session(unsigned long id, gb_stream_session_t *list, int mode);

static int gb_set_runtime_status(int channel, NET_RUNTIME_STATUS_TYPE type, int value)
{
    int ret = net_set_runtime_status(channel, type, value);
    if (ret != 0)
    {
        __ERR("gb28181_lib set runtime status failed: ch=%d type=%d value=%d ret=%d\n",
              channel, (int)type, value, ret);
    }
    return ret;
}

static int gb_set_media_config(int channel, NET_MEDIA_CONFIG *config)
{
    int ret = net_set_media_config(channel, config);
    if (ret != 0)
    {
        __ERR("gb28181_lib set media config failed: ch=%d ret=%d\n",
              channel, ret);
    }
    return ret;
}

static int gb_get_media_config(int channel, NET_MEDIA_CONFIG *config)
{
    int ret = net_get_media_config(channel, config);
    if (ret != 0)
    {
        __ERR("gb28181_lib get media config failed: ch=%d ret=%d\n",
              channel, ret);
    }
    return ret;
}

static int gb_send_packet(unsigned long stream_id,
                          int request_index,
                          int stream_mode,
                          unsigned long long timestamp,
                          const unsigned char *payload,
                          size_t payload_size,
                          int is_video,
                          unsigned char frame_seq)
{
    if (!is_video)
    {
        return net_send_stream(stream_id,
                               (char *)payload,
                               (unsigned long)payload_size,
                               timestamp,
                               request_index,
                               stream_mode,
                               0);
    }

    size_t packet_cap = sizeof(struct RtpHeader) + payload_size;
    unsigned char *packet = (unsigned char *)malloc(packet_cap);
    if (packet == NULL)
    {
        return -1;
    }

    struct RtpHeader header;
    memset(&header, 0, sizeof(header));
    header.pt = 1;
    header.ucFrameSeq = frame_seq;
    header.usPtLen = (unsigned short)payload_size;

    memcpy(packet, &header, sizeof(header));
    memcpy(packet + sizeof(header), payload, payload_size);

    int ret = net_send_stream(stream_id,
                              (char *)packet,
                              (unsigned long)(sizeof(header) + payload_size),
                              timestamp,
                              request_index,
                              stream_mode,
                              1);
    free(packet);
    return ret;
}

static gb_stream_session_t *gb_find_playback_session_by_handle(REC_HANDLE handle, gb_stream_session_t *list)
{
    rec_pb_poper *poper = (rec_pb_poper *)handle;
    int idx;

    if (poper == NULL)
    {
        return NULL;
    }

    for (idx = 0; idx < MAX_CHANNEL_NUM; ++idx)
    {
        if (list[idx].active && list[idx].id == (unsigned long)(unsigned int)poper->iPopId)
        {
            return &list[idx];
        }
    }
    return NULL;
}

static int gb_find_session(gb_stream_session_t *list, int size, unsigned long id)
{
    int i;
    for (i = 0; i < size; ++i)
    {
        if (list[i].active && list[i].id == id)
        {
            return i;
        }
    }
    return -1;
}

static int gb_find_free(gb_stream_session_t *list, int size)
{
    int i;
    for (i = 0; i < size; ++i)
    {
        if (!list[i].active)
        {
            return i;
        }
    }
    return -1;
}

static int gb_get_request_index(const NET_SERVER_START_MEDIA *media)
{
    int i;

    if (media != NULL && media->media.pu_id[0] != '\0')
    {
        for (i = 0; i < s_sip_info.channel_num && i < MAX_CHANNEL_NUM; ++i)
        {
            if (strcmp(media->media.pu_id, s_sip_info.channel_info[i].deviceid) == 0)
            {
                return i;
            }
        }
    }

    if (media != NULL && media->media.video_id > 0 && media->media.video_id <= MAX_CHANNEL_NUM)
    {
        return media->media.video_id - 1;
    }

    return -1;
}

static void gb_stop_session_by_request_index(gb_stream_session_t *list, int mode, int request_index)
{
    int i;
    if (request_index < 0 || request_index >= MAX_CHANNEL_NUM)
    {
        return;
    }
    for (i = 0; i < MAX_CHANNEL_NUM; ++i)
    {
        if (list[i].active && list[i].request_index == request_index)
        {
            gb_stop_session(list[i].id, list, mode);
            return;
        }
    }
}

static int gb_load_sip_info(Service_GB28181_Cfg *cfg)
{
    if (cfg == NULL)
    {
        return -1;
    }

    memset(&s_sip_info, 0, sizeof(s_sip_info));
    s_sip_info.hcPort = cfg->hcPort;
    s_sip_info.lcPort = cfg->lcPort;
    s_sip_info.expires = cfg->expires;
    s_sip_info.keeplive = cfg->keepAlive;
    s_sip_info.keeplivenum = cfg->keepAliveNum;
    s_sip_info.channel_num = 1;
    s_sip_info.alarm_num = 1;
    s_sip_info.alarmout_num = 1;
    s_sip_info.alarm_timeinterval = 1; // 1 min

    snprintf(s_sip_info.hcIp, sizeof(s_sip_info.hcIp), "%.*s", (int)(sizeof(s_sip_info.hcIp) - 1), cfg->hcIP);
    snprintf(s_sip_info.hcName, sizeof(s_sip_info.hcName), "%s", cfg->hcName);
    snprintf(s_sip_info.hcPwd, sizeof(s_sip_info.hcPwd), "%s", cfg->Pwd);
    snprintf(s_sip_info.hcId, sizeof(s_sip_info.hcId), "%s", cfg->hcID);
    snprintf(s_sip_info.lcIp, sizeof(s_sip_info.lcIp), "%.*s", (int)(sizeof(s_sip_info.lcIp) - 1), cfg->lcIp);
    snprintf(s_sip_info.lcName, sizeof(s_sip_info.lcName), "%s", cfg->Username);
    snprintf(s_sip_info.lcPwd, sizeof(s_sip_info.lcPwd), "%s", cfg->Pwd);
    snprintf(s_sip_info.lcId, sizeof(s_sip_info.lcId), "%s", cfg->lcId);
    snprintf(s_sip_info.lcUsername, sizeof(s_sip_info.lcUsername), "%.*s", (int)(sizeof(s_sip_info.lcUsername) - 1), cfg->Username);

    for (int i = 0; i < s_sip_info.channel_num; i++)
    {
        snprintf(s_sip_info.channel_info[i].deviceid, sizeof(s_sip_info.channel_info[i].deviceid), "%s", cfg->lcId);
        snprintf(s_sip_info.channel_info[i].name, sizeof(s_sip_info.channel_info[i].name), "%s", cfg->lcId);
        strncpy(s_sip_info.channel_info[i].manufacturer, "ANJVision", sizeof(s_sip_info.channel_info[i].manufacturer) - 1);
        strncpy(s_sip_info.channel_info[i].model, "ANJVision", sizeof(s_sip_info.channel_info[i].model) - 1);
        strncpy(s_sip_info.channel_info[i].owner, "ANJVision", sizeof(s_sip_info.channel_info[i].owner) - 1);
        strncpy(s_sip_info.channel_info[i].civilcode, "China", sizeof(s_sip_info.channel_info[i].civilcode) - 1);
        strncpy(s_sip_info.channel_info[i].address, "China", sizeof(s_sip_info.channel_info[i].address) - 1);
        strncpy(s_sip_info.channel_info[i].status, "ON", sizeof(s_sip_info.channel_info[i].status) - 1);
        s_sip_info.channel_info[i].parental = 0;
        s_sip_info.channel_info[i].registerway = 1;
        s_sip_info.channel_info[i].secrecy = 0;
    }
    
    for (int i = 0; i < s_sip_info.alarm_num; i++   )
    {
        snprintf(s_sip_info.alarmstatus_info[i].dutystatus, sizeof(s_sip_info.alarmstatus_info[i].dutystatus), "OFFDUTY");
        snprintf(s_sip_info.alarmstatus_info[i].status, sizeof(s_sip_info.alarmstatus_info[i].status), "ON");
        snprintf(s_sip_info.alarmstatus_info[i].deviceid, sizeof(s_sip_info.alarmstatus_info[i].deviceid), cfg->AlarmId);
    }

    strncpy(s_sip_info.user_agent, "ANJVISION", sizeof(s_sip_info.user_agent) - 1);
    strncpy(s_sip_info.basic_device_info.DeviceType, "IPC", sizeof(s_sip_info.basic_device_info.DeviceType) - 1);
    strncpy(s_sip_info.basic_device_info.Manufacturer, "ANJVISION", sizeof(s_sip_info.basic_device_info.Manufacturer) - 1);
    anj_sysmng_dev_str_get(s_sip_info.basic_device_info.Model);
    strncpy(s_sip_info.basic_device_info.Firmware, "V1.0", sizeof(s_sip_info.basic_device_info.Firmware) - 1);
    s_sip_info.basic_device_info.MaxCamera = 1;
    s_sip_info.basic_device_info.MaxAlarm = 1;

    s_net_info.sip_net = &s_sip_info;
    return 0;
}

static int gb_tran_resol_tp2aebell(const char *res_name)
{
    if (strcmp(res_name, "5MP") == 0)
    {
        return GB_RES_1080P;
    }
    else if (strcmp(res_name, "3MP") == 0)
    {
        return GB_RES_1080P;
    }
    else if (strcmp(res_name, "1080P") == 0)
    {
        return GB_RES_1080P;
    }
    else if (strcmp(res_name, "1200P") == 0)
    {
        return GB_RES_1080P;
    }
    else if (strcmp(res_name, "1024P") == 0)
    {
        return GB_RES_1080P;
    }
    else if (strcmp(res_name, "960P") == 0)
    {
        return GB_RES_1080P;
    }
    else if (strcmp(res_name, "768P") == 0)
    {
        return GB_RES_768P;
    }
    else if (strcmp(res_name, "720P") == 0)
    {
        return GB_RES_720P;
    }
    else if (strcmp(res_name, "D1") == 0)
    {
        return GB_RES_D1;
    }
    else if (strcmp(res_name, "VGA") == 0)
    {
        return GB_RES_VGA;
    }
    else if (strcmp(res_name, "CIF") == 0)
    {
        return GB_RES_CIF;
    }
    else if (strcmp(res_name, "QVGA") == 0)
    {
        return GB_RES_QVGA;
    }
    else
    {
        return GB_RES_1080P;
    }
    return GB_RES_1080P;
}

static void gb_tran_resol_aebell2tp(int tw_resol, char *resol_buf, size_t len)
{
    const char *res = "720P";

    switch (tw_resol)
    {
    case GB_RES_1080P:
        res = "1080P";
        break;
    case GB_RES_720P:
        res = "720P";
        break;
    case GB_RES_768P:
        res = "768P";
        break;
    case GB_RES_QVGA:
        res = "QVGA";
        break;
    case GB_RES_VGA:
        res = "VGA";
        break;
    case GB_RES_D1:
        res = "D1";
        break;
    case GB_RES_CIF:
        res = "CIF";
        break;
    default:
        break;
    }
    snprintf(resol_buf, len, "%s", res);
}

static int gb_msg_ptz_ctrl(NET_SERVER_OPER_CTRL_PTZ ptz_ctrl)
{
    if (s_ptz_if.ifCtrlPtzCb == NULL)
    {
        __ERR("gb28181_lib ptz callback is not registered\n");
        return -1;
    }
    return s_ptz_if.ifCtrlPtzCb(ptz_ctrl.cmd, ptz_ctrl.speed, ptz_ctrl.param1);
}

static int gb_msg_snapshot(char *buf)
{
    NET_SERVER_SNAPSHOT_INFO *snap_info = (NET_SERVER_SNAPSHOT_INFO *)buf;
    const char *file_name = "pictest.jpg";
    char pathname[256] = {0};
    unsigned long long file_len = 0;

    if (snap_info == NULL)
    {
        return -1;
    }

    if (anj_snap_jpg(0, 1, 100, "/tmp", (char *)file_name, NULL) != 0)
    {
        __ERR("gb28181_lib snapshot trigger failed\n");
        return -1;
    }
    snprintf(pathname, sizeof(pathname), "/tmp/%s", file_name);
    if (anj_snap_wait_complete(pathname, 500) != 0)
    {
        __ERR("gb28181_lib wait jpeg failed\n");
        return -1;
    }

    if (anj_mw_read_file_len(pathname, &file_len) != 0)
    {
        __ERR("gb28181_lib read snapshot len failed\n");
        remove(pathname);
        return -1;
    }

    if (file_len > sizeof(snap_info->Picture))
    {
        file_len = sizeof(snap_info->Picture);
    }
    snap_info->PictureSize = (unsigned int)file_len;
    if (anj_mw_read_file_limit_len(pathname, snap_info->Picture, file_len) != 0)
    {
        __ERR("gb28181_lib read snapshot data failed\n");
        remove(pathname);
        return -1;
    }
    remove(pathname);
    return 0;
}

static int gb_msg_set_ftp(char *buf)
{
    NET_SERVER_FTP_INFO *in_param = (NET_SERVER_FTP_INFO *)buf;
    ServerConfig *server_cfg = getServerConfig();
    ServerConfig cfg;

    if (in_param == NULL || server_cfg == NULL)
    {
        return -1;
    }

    cfg = *server_cfg;
    safe_strcpy(cfg.ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].serverIP, MAX_IP_NAME_LEN, in_param->Ftp);
    safe_strcpy(cfg.ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].userName, FTP_NAME_MAX_LEN, in_param->Ftpuser);
    safe_strcpy(cfg.ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].password, FTP_PASSWORD_MAX_LEN, in_param->Ftppassword);
    safe_strcpy(cfg.ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].filePath, FTP_PATH_MAX_LEN, in_param->Path);
    cfg.ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].serverPort = in_param->Port;
    return anj_config_server_set(&cfg);
}

static int gb_msg_get_ftp(char *buf)
{
    NET_SERVER_FTP_INFO *in_param = (NET_SERVER_FTP_INFO *)buf;
    ServerConfig *server_cfg = getServerConfig();

    if (in_param == NULL || server_cfg == NULL)
    {
        return -1;
    }

    safe_strcpy(in_param->Ftp, sizeof(in_param->Ftp), server_cfg->ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].serverIP);
    safe_strcpy(in_param->Ftpuser, sizeof(in_param->Ftpuser), server_cfg->ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].userName);
    safe_strcpy(in_param->Ftppassword, sizeof(in_param->Ftppassword), server_cfg->ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].password);
    safe_strcpy(in_param->Path, sizeof(in_param->Path), server_cfg->ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].filePath);
    in_param->Port = server_cfg->ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD].serverPort;
    return 0;
}

static int gb_msg_set_serial_port(char *buf)
{
    NET_SERVER_SERIAL_PORT_INFO *in_param = (NET_SERVER_SERIAL_PORT_INFO *)buf;
    SystemConfig *system_cfg = (SystemConfig *)getSystemConfig();
    PTZConfig ptz_cfg;

    if (in_param == NULL || system_cfg == NULL)
    {
        return -1;
    }

    ptz_cfg = system_cfg->ptzCfg;
    ptz_cfg.commonCfg.comPort = in_param->SerialPort;
    ptz_cfg.commonCfg.baudrate = in_param->BandRate;
    ptz_cfg.commonCfg.dataBits = in_param->DataBit;
    ptz_cfg.commonCfg.stopBits = in_param->StopBit * 10;
    if (in_param->FlowControl == 1)
    {
        snprintf(ptz_cfg.commonCfg.flowControl.flowControlName, sizeof(ptz_cfg.commonCfg.flowControl.flowControlName), "%s", "SOFTWARE");
    }
    else if (in_param->FlowControl == 2)
    {
        snprintf(ptz_cfg.commonCfg.flowControl.flowControlName, sizeof(ptz_cfg.commonCfg.flowControl.flowControlName), "%s", "HARDWARE");
    }
    else
    {
        snprintf(ptz_cfg.commonCfg.flowControl.flowControlName, sizeof(ptz_cfg.commonCfg.flowControl.flowControlName), "%s", "NONE");
    }
    return anj_config_system_ptz_set(&ptz_cfg);
}

static int gb_msg_get_serial_port(char *buf)
{
    NET_SERVER_SERIAL_PORT_INFO *in_param = (NET_SERVER_SERIAL_PORT_INFO *)buf;
    SystemConfig *system_cfg = (SystemConfig *)getSystemConfig();

    if (in_param == NULL || system_cfg == NULL)
    {
        return -1;
    }

    in_param->SerialPort = system_cfg->ptzCfg.commonCfg.comPort;
    in_param->BandRate = system_cfg->ptzCfg.commonCfg.baudrate;
    in_param->DataBit = system_cfg->ptzCfg.commonCfg.dataBits;
    in_param->StopBit = system_cfg->ptzCfg.commonCfg.stopBits;
    in_param->FlowControl = 0;
    if (!strcmp(system_cfg->ptzCfg.commonCfg.flowControl.flowControlName, "SOFTWARE"))
    {
        in_param->FlowControl = 1;
    }
    else if (!strcmp(system_cfg->ptzCfg.commonCfg.flowControl.flowControlName, "HARDWARE"))
    {
        in_param->FlowControl = 2;
    }
    snprintf(in_param->Mode, sizeof(in_param->Mode), "%s", "RS485");
    return 0;
}

static int gb_msg_get_time_cfg(char *buf)
{
    NET_SERVER_TIME_CONFIG_INFO *in_param = (NET_SERVER_TIME_CONFIG_INFO *)buf;
    SystemConfig *system_cfg = (SystemConfig *)getSystemConfig();

    if (in_param == NULL || system_cfg == NULL)
    {
        return -1;
    }

    snprintf(in_param->NTPServer, sizeof(in_param->NTPServer), "%s", system_cfg->timeCfg.ntpConfig.serverIP);
    in_param->NTPPort = system_cfg->timeCfg.ntpConfig.serverPort;
    in_param->NTPInterval = system_cfg->timeCfg.ntpConfig.refreshInterval;
    return 0;
}

static int gb_msg_set_pu_time(char *buf)
{
    LPNET_SERVER_DEV_TIME basic = (LPNET_SERVER_DEV_TIME)buf;
    
    __INFO("GB/T 28181 time set: %d-%02d-%02d %02d:%02d:%02d\n", 
        basic->year, basic->month, basic->date, 
        basic->hour, basic->minute, basic->second);

    struct tm newtime;
    memset(&newtime, 0, sizeof(struct tm));
    newtime.tm_sec = basic->second;
    newtime.tm_min = basic->minute;
    newtime.tm_hour = basic->hour;
    newtime.tm_mday = basic->date;
    newtime.tm_mon = basic->month - 1;
    newtime.tm_year = basic->year - 1900;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm *ptm = localtime(&tv.tv_sec);
    if (newtime.tm_mon > 12 || newtime.tm_mon < 1)
    {
        newtime.tm_mon = ptm->tm_mon;
    }

    if (newtime.tm_year < 1)
    {
        newtime.tm_year = ptm->tm_year;
    }

    time_t t = mktime(&newtime);
    if (abs(t - tv.tv_sec) > 10)
    {
        __ERR("set time OK! now time:%ld ,set time %ld \n",
              tv.tv_sec, t);

        SystemConfig *pSystemConfig = (SystemConfig *)getSystemConfig();
        TimeConfig *pTimeConfig = &pSystemConfig->timeCfg;
        anj_systime_set_only(newtime, pTimeConfig->timeZone, 1);
    }
    else
    {
        __ERR("diff time less than 10 seconds:now time:%ld ,set time %ld \n",
              tv.tv_sec, t);
    }

    return 0;
}

static int gb_msg_get_pu_time(char *buf)
{
    struct timeval tv;
    struct tm *ptm = NULL;
    LPNET_SERVER_DEV_TIME basic = (LPNET_SERVER_DEV_TIME)buf;

    if (basic == NULL)
    {
        return -1;
    }

    gettimeofday(&tv, NULL);
    ptm = localtime(&tv.tv_sec);
    if (ptm == NULL)
    {
        return -1;
    }

    basic->hour = ptm->tm_hour;
    basic->minute = ptm->tm_min;
    basic->second = ptm->tm_sec;
    basic->month = ptm->tm_mon + 1;
    basic->date = ptm->tm_mday;
    basic->year = ptm->tm_year + 1900;
    return 0;
}

static int gb_msg_set_network(char *buf)
{
    NET_SERVER_NETWORK_INFO *in_param = (NET_SERVER_NETWORK_INFO *)buf;
    NetworkConfigNew *network_cfg = (NetworkConfigNew *)getNetWorkConfig();
    NetworkConfigNew cfg;

    if (in_param == NULL || network_cfg == NULL)
    {
        return -1;
    }

    cfg = *network_cfg;
    cfg.lanCfg.dhcpEnable = in_param->DHCP;
    snprintf(cfg.lanCfg.IPAddress, sizeof(cfg.lanCfg.IPAddress), "%s", in_param->IPAddress);
    snprintf(cfg.lanCfg.netMask, sizeof(cfg.lanCfg.netMask), "%s", in_param->SubMask);
    snprintf(cfg.lanCfg.gateWay, sizeof(cfg.lanCfg.gateWay), "%s", in_param->DefaultGateway);
    snprintf(cfg.lanCfg.DNS1, sizeof(cfg.lanCfg.DNS1), "%s", in_param->DNSPrimary);
    snprintf(cfg.lanCfg.DNS2, sizeof(cfg.lanCfg.DNS2), "%s", in_param->DNSSecondary);
    snprintf(cfg.adslCfg.userName, sizeof(cfg.adslCfg.userName), "%s", in_param->PPPOEUsername);
    snprintf(cfg.adslCfg.password, sizeof(cfg.adslCfg.password), "%s", in_param->PPPOEPassword);
    anj_config_network_set(&cfg);
    return 0;
}

static int gb_msg_get_network(char *buf)
{
    NET_SERVER_NETWORK_INFO *in_param = (NET_SERVER_NETWORK_INFO *)buf;
    NetworkConfigNew *network_cfg = (NetworkConfigNew *)getNetWorkConfig();

    if (in_param == NULL || network_cfg == NULL)
    {
        return -1;
    }

    in_param->DHCP = network_cfg->lanCfg.dhcpEnable;
    safe_strcpy(in_param->IPAddress, sizeof(in_param->IPAddress), network_cfg->lanCfg.IPAddress);
    safe_strcpy(in_param->SubMask, sizeof(in_param->SubMask), network_cfg->lanCfg.netMask);
    safe_strcpy(in_param->DefaultGateway, sizeof(in_param->DefaultGateway), network_cfg->lanCfg.gateWay);
    safe_strcpy(in_param->DNSPrimary, sizeof(in_param->DNSPrimary), network_cfg->lanCfg.DNS1);
    safe_strcpy(in_param->DNSSecondary, sizeof(in_param->DNSSecondary), network_cfg->lanCfg.DNS2);
    safe_strcpy(in_param->PPPOEUsername, sizeof(in_param->PPPOEUsername), network_cfg->adslCfg.userName);
    safe_strcpy(in_param->PPPOEPassword, sizeof(in_param->PPPOEPassword), network_cfg->adslCfg.password);
    return 0;
}

static int gb_msg_set_display(char *buf)
{
    NET_SERVER_DISPLAY_INFO *in_param = (NET_SERVER_DISPLAY_INFO *)buf;
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    int camera_index;

    if (in_param == NULL || media_cfg == NULL)
    {
        return -1;
    }

    for (camera_index = 0; camera_index < ANJ_CAMERA_MAX_NUMS; ++camera_index)
    {
        VideoCaptureCfg video_capture = media_cfg->videoConfig[camera_index].videoCapture;
        video_capture.brightness = in_param->Bright;
        video_capture.contrast = in_param->Contrast;
        video_capture.saturation = in_param->Saturation;
        video_capture.sharpness = in_param->Hue;
        anj_config_video_capture_set(&video_capture, camera_index);
    }
    return 0;
}

static int gb_msg_get_display(char *buf)
{
    NET_SERVER_DISPLAY_INFO *in_param = (NET_SERVER_DISPLAY_INFO *)buf;
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *video_capture = NULL;

    if (in_param == NULL || media_cfg == NULL)
    {
        return -1;
    }

    video_capture = &media_cfg->videoConfig[0].videoCapture;
    in_param->Channel = 1;
    in_param->Bright = video_capture->brightness;
    in_param->Contrast = video_capture->contrast;
    in_param->Saturation = video_capture->saturation;
    in_param->Hue = video_capture->sharpness;
    return 0;
}

static int gb_msg_set_text(char *buf)
{
    NET_SERVER_TEXT_INFO *in_param = (NET_SERVER_TEXT_INFO *)buf;

    if (in_param == NULL)
    {
        return -1;
    }

    snprintf(s_osd_text, sizeof(s_osd_text), "%s", in_param->Text);
    return 0;
}

static int gb_msg_get_text(char *buf)
{
    NET_SERVER_TEXT_INFO *in_param = (NET_SERVER_TEXT_INFO *)buf;
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    VideoOverlay *video_overlay = NULL;

    if (in_param == NULL || media_cfg == NULL)
    {
        return -1;
    }

    video_overlay = &media_cfg->videoConfig[0].overlay;
    in_param->EnableTime = video_overlay->enable;
    in_param->EnableText = in_param->EnableTime;
    in_param->TimeX = video_overlay->timeOverlay.posX;
    in_param->TimeY = video_overlay->timeOverlay.posY;
    in_param->TextX = video_overlay->titleOverlay.posX;
    in_param->TextY = video_overlay->titleOverlay.posY;
    snprintf(in_param->Text, sizeof(in_param->Text), "%s", s_osd_text);
    return 0;
}

static int gb_msg_get_audio_enc(char *buf)
{
    NET_SERVER_AUDIO_ENCODER_INFO *in_param = (NET_SERVER_AUDIO_ENCODER_INFO *)buf;

    if (in_param == NULL)
    {
        return -1;
    }

    in_param->Channel = 1;
    in_param->EncodeMode = 0;
    return 0;
}

static int gb_msg_set_video_enc(char *buf)
{
    NET_SERVER_VIDEO_ENCODER_INFO *in_param = (NET_SERVER_VIDEO_ENCODER_INFO *)buf;
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    VideoConfig video_cfg_array[ANJ_CAMERA_MAX_NUMS];
    int camera_index;
    int need_switch = 0;

    if (in_param == NULL || media_cfg == NULL)
    {
        return -1;
    }
    if (in_param->StreamType > 1)
    {
        return 0;
    }

    memcpy(video_cfg_array, media_cfg->videoConfig, sizeof(video_cfg_array));
    for (camera_index = 0; camera_index < ANJ_CAMERA_MAX_NUMS; ++camera_index)
    {
        VideoEncode *video_encode = &video_cfg_array[camera_index].videoEncode;
        VideoEncodeCfg *encode_cfg = &video_encode->encodeCfg[in_param->StreamType];
        encode_cfg->bitRate = in_param->BitRate;
        encode_cfg->bitRateQuality = VIDEO_QUALITY_CUSTOM;
        encode_cfg->frameRate = in_param->FrameRate;
        gb_tran_resol_aebell2tp(in_param->ImageSize, encode_cfg->resolution.name, sizeof(encode_cfg->resolution.name));
        need_switch |= anj_config_video_encode_set(video_encode, camera_index);
    }
    if (need_switch)
    {
        anj_video_encode_switch();
    }
    return 0;
}

static int gb_msg_get_video_enc(char *buf)
{
    NET_SERVER_VIDEO_ENCODER_INFO *in_param = (NET_SERVER_VIDEO_ENCODER_INFO *)buf;
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    VideoEncodeCfg *encode_cfg = NULL;

    if (in_param == NULL || media_cfg == NULL)
    {
        return -1;
    }

    encode_cfg = &media_cfg->videoConfig[0].videoEncode.encodeCfg[0];
    in_param->BitRate = encode_cfg->bitRate;
    in_param->FrameRate = encode_cfg->frameRate;
    in_param->ImageSize = gb_tran_resol_tp2aebell(encode_cfg->resolution.name);
    return 0;
}

static int gb_msg_query_preset_list(char *buf)
{
    NET_SERVER_PRESET_LIST *preset_list = (NET_SERVER_PRESET_LIST *)buf;
    IotPtzConfig *ptz_config = getIotPtzConfig();
    if (preset_list == NULL || ptz_config == NULL)
    {
        return -1;
    }

    int preset_num = 0;
    /* 先计算实际有多少个有效预置点 */
    for (int i = 0; i < MAX_PTZ_PRESET; ++i)
    {
        if (ptz_config->m_ptzPreset[i].preset_id != 0)
        {
            preset_num++;
        }
    }

    if (preset_num == 0)
    {
        preset_list->preset_list_cnt = 0;
        preset_list->preset_lists = NULL;
        __INFO("gb28181_lib query preset list, total preset num=0\n");
        return 0;
    }

    /* 一次性分配数组来存放所有预置点 */
    NET_SERVER_PRESET *presets = (NET_SERVER_PRESET *)malloc(preset_num * sizeof(NET_SERVER_PRESET));
    if (presets == NULL)
    {
        __ERR("gb28181_lib malloc preset array failed, size=%lu\n", preset_num * sizeof(NET_SERVER_PRESET));
        preset_list->preset_list_cnt = 0;
        preset_list->preset_lists = NULL;
        return -1;
    }
    memset(presets, 0, preset_num * sizeof(NET_SERVER_PRESET));

    /* 填充预置点数据到数组 */
    int idx = 0;
    for (int i = 0; i < MAX_PTZ_PRESET && idx < preset_num; ++i)
    {
        if (ptz_config->m_ptzPreset[i].preset_id != 0)
        {
            presets[idx].preset_index = ptz_config->m_ptzPreset[i].preset_id;
            idx++;
        }
    }

    preset_list->preset_lists = presets;
    preset_list->preset_list_cnt = preset_num;
    __INFO("gb28181_lib query preset list, total preset num=%d\n", preset_num);

    return 0;
}

static void gb_update_media_config(int request_index)
{
    NET_MEDIA_CONFIG config;
    media_codec_type_e codec_type;
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    gb_get_media_config(request_index, &config);
    codec_type = video_encode_type_get(media_cfg->videoConfig[request_index].videoEncode.encodeCfg[0].encodeFormat.name);
    if (codec_type == MEDIA_CODEC_VIDEO_H265)
    {
        config.video_codec = NET_VIDEO_CODEC_H265;
    }
    else
    {
        config.video_codec = NET_VIDEO_CODEC_H264;
    }
    codec_type = audio_encode_type_get(media_cfg->audioConfig.audioEncode.audioEncodeType.typeName);
    if (codec_type == MEDIA_CODEC_AUDIO_AAC)
    {
        config.audio_codec = NET_AUDIO_CODEC_AAC;
    }
    else if (codec_type == MEDIA_CODEC_AUDIO_G711A)
    {
        config.audio_codec = NET_AUDIO_CODEC_G711A;
    }
    else
    {
        config.audio_codec = NET_AUDIO_CODEC_G711U;
    }
    gb_set_media_config(request_index, &config);
}

static int gb_start_session(unsigned long id, NET_SERVER_START_MEDIA *m, gb_stream_session_t *list, int mode)
{
    int ret = 0;
    int idx = gb_find_free(list, MAX_CHANNEL_NUM);
    int request_index;
    if (idx < 0)
    {
        return -1;
    }
    request_index = gb_get_request_index(m);
    if (request_index < 0 || request_index >= MAX_CHANNEL_NUM)
    {
        __ERR("gb28181_lib invalid request index pu_id=%s video_id=%d\n",
              (m != NULL) ? m->media.pu_id : "",
              (m != NULL) ? m->media.video_id : -1);
        return -1;
    }

    /* One stream per request_index, align with demo and avoid seek races. */
    if (mode == 1 || mode == 2)
    {
        gb_stop_session_by_request_index(list, mode, request_index);
    }

    list[idx].id = id;
    list[idx].active = 1;
    list[idx].wait_iframe = 1;
    list[idx].audio_type = s_audio_type_live;
    list[idx].channel = m->media.video_id;
    list[idx].request_index = request_index;
    list[idx].request_type = m->request_type;
    list[idx].stream_mode = (mode == 0) ? 0 : 1;
    list[idx].frame_seq = 0;

    /* Update media config */
    gb_update_media_config(request_index);

    if (mode == 0 && s_stream_if.ifStartStreamCb)
    {
        int h265 = 0;
        /* Pass session index (NOT pointer) to avoid 64-bit truncation. */
        s_stream_if.ifStartStreamCb(idx, &h265);

        /* Align with SDK demo: these flags gate internal stream packing/sending. */
        if (list[idx].request_index >= 0 && list[idx].request_index < MAX_CHANNEL_NUM)
        {
            gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_SENDVIDEO_LIVE, 1);
            gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_BREAK_LIVE, 1);
            gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_ACTIVE_LIVE, 1);
            __INFO("gb28181_lib start audio stream type idx=%d video_id=%d req=%d audio_stream_type=%d\n",
                   idx, list[idx].channel, list[idx].request_index, s_audio_type_live);
        }
    }
    if ((mode == 1) && s_history_channel_if.ifHistoryChannelStartCapture)
    {
        HistoryChannelPositionInfo pos;
        memset(&pos, 0, sizeof(pos));
        pos.cSeekTime.wYear = m->start_time.year;
        pos.cSeekTime.wMonth = m->start_time.month;
        pos.cSeekTime.wDay = m->start_time.date;
        pos.cSeekTime.wHour = m->start_time.hour;
        pos.cSeekTime.wMinute = m->start_time.minute;
        pos.cSeekTime.wSecond = m->start_time.second;
        __INFO("start_time=%d-%d-%d %d:%d:%d\n", 
            m->start_time.year, m->start_time.month, m->start_time.date, 
            m->start_time.hour, m->start_time.minute, m->start_time.second);
        if (list[idx].request_index >= 0 && list[idx].request_index < MAX_CHANNEL_NUM)
        {
            gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_BREAK_PLAYBACK, 1);
            gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_ACTIVE_PLAYBACK, 1);
            gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_SENDVIDEO_PLAYBACK, 1);
            __INFO("gb28181_lib start playback status idx=%d video_id=%d req=%d stream_id=%d\n",
                   idx, list[idx].channel, list[idx].request_index, id);
        }
        ret = s_history_channel_if.ifHistoryChannelStartCapture(id, (char *)&pos);
        if (ret != 0)
        {
            __ERR("gb28181_lib start playback failed: idx=%d video_id=%d req=%d stream_id=%d ret=%d\n",
                   idx, list[idx].channel, list[idx].request_index, id, ret);
            return ret;
        }
    }
    else if ((mode == 2) && s_history_channel_if.ifHistoryChannelStartCapture)
    {
        HistoryChannelPositionInfo pos;
        memset(&pos, 0, sizeof(pos));
        pos.cSeekTime.wYear   = m->start_time.year;
        pos.cSeekTime.wMonth  = m->start_time.month;
        pos.cSeekTime.wDay    = m->start_time.date;
        pos.cSeekTime.wHour   = m->start_time.hour;
        pos.cSeekTime.wMinute = m->start_time.minute;
        pos.cSeekTime.wSecond = m->start_time.second;
        pos.cEndTime.wYear    = m->stop_time.year;
        pos.cEndTime.wMonth   = m->stop_time.month;
        pos.cEndTime.wDay     = m->stop_time.date;
        pos.cEndTime.wHour    = m->stop_time.hour;
        pos.cEndTime.wMinute  = m->stop_time.minute;
        pos.cEndTime.wSecond  = m->stop_time.second;
        pos.bDownload = 1; // download mode enable
        gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_BREAK_PLAYBACK, 1);
        gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_ACTIVE_PLAYBACK, 1);
        gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_BREAK_DOWNLOAD, 1);
        gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_ACTIVE_DOWNLOAD, 1);
        gb_set_runtime_status(list[idx].request_index, NET_RUNTIME_STATUS_SENDVIDEO_DOWNLOAD, 1);
        ret = s_history_channel_if.ifHistoryChannelStartCapture(id, (char *)&pos);
        if (ret != 0)
        {
            __ERR("gb28181_lib start download failed: idx=%d video_id=%d req=%d stream_id=%d ret=%d\n",
                   idx, list[idx].channel, list[idx].request_index, id, ret);
            return ret;
        }
    }
    return 0;
}

static int gb_stop_session(unsigned long id, gb_stream_session_t *list, int mode)
{
    int idx = gb_find_session(list, MAX_CHANNEL_NUM, id);
    if (idx < 0)
    {
        return -1;
    }
    if (mode == 0 && s_stream_if.ifStopStreamCb)
    {
        /* Clear SDK flags for this channel (best-effort). */
        if (list[idx].request_index >= 0 && list[idx].request_index < MAX_CHANNEL_NUM)
        {
            int request_index = list[idx].request_index;
            gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_BREAK_LIVE, 0);
            gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_ACTIVE_LIVE, 0);
            gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_SENDVIDEO_LIVE, 0);
            __INFO("gb28181_lib stop status idx=%d video_id=%d req=%d\n", idx, list[idx].channel, request_index);
        }

        s_stream_if.ifStopStreamCb(idx);
    }
    if ((mode == 1) && s_history_channel_if.ifHistoryChannelStopCapture)
    {
        if (list[idx].request_index >= 0 && list[idx].request_index < MAX_CHANNEL_NUM)
        {
            int request_index = list[idx].request_index;
            gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_BREAK_PLAYBACK, 0);
            gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_ACTIVE_PLAYBACK, 0);
            gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_SENDVIDEO_PLAYBACK, 0);
        }
        s_history_channel_if.ifHistoryChannelStopCapture(id);
    }
    else if (mode == 2)
    {
        int request_index = list[idx].request_index;
        gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_BREAK_DOWNLOAD, 0);
        gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_ACTIVE_DOWNLOAD, 0);
        gb_set_runtime_status(request_index, NET_RUNTIME_STATUS_SENDVIDEO_DOWNLOAD, 0);
    }
    memset(&list[idx], 0, sizeof(list[idx]));
    return 0;
}

static int gb_on_login(int type, char *ip, unsigned short port, unsigned long error, unsigned long times, unsigned long param)
{
    (void)type;
    (void)ip;
    (void)port;
    (void)error;
    (void)times;
    (void)param;
    return 0;
}

static int gb_on_msg(const char *cmd, char *buf, unsigned long *size, unsigned long param)
{
    int ret = 0;
    (void)param;

    if (cmd == NULL || buf == NULL || size == NULL)
    {
        return -1;
    }

    __INFO("gb_on_msg cmd=%s\n", cmd);

    if (strcmp(cmd, OPER_PU_QUERYPULOCALSTORAGEFILES) == 0)
    {
        unsigned int buflen = (unsigned int)*size;
        if (buflen < sizeof(NET_SERVER_QUERY_STORE_FILE))
        {
            __ERR("####WORN,Invaild len %u<%u\n", buflen, sizeof(NET_SERVER_QUERY_STORE_FILE));
            return -1;
        }
        if (s_history_query_if.ifHistoryQuery == NULL)
        {
            __ERR("gb28181_lib history query callback not registered, cmd=%s\n", cmd);
            return -1;
        }
        return s_history_query_if.ifHistoryQuery(buf);
    }
    else if (strcmp(cmd, OPER_PU_CONTROLPTZ) == 0)
    {
        __INFO("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@buf:%s\n", buf);
        NET_SERVER_OPER_CTRL_PTZ LPptz_ctrl;
        memset(&LPptz_ctrl, 0, sizeof(LPptz_ctrl));
        memcpy(&LPptz_ctrl, buf, sizeof(LPptz_ctrl));
        unsigned int buflen = (unsigned int)*size;
        if (buflen < sizeof(NET_SERVER_OPER_CTRL_PTZ))
        {
            __ERR("####WORN,Invaild len %u<%u\n", buflen, sizeof(NET_SERVER_OPER_CTRL_PTZ));
            return -1;
        }
        ret = gb_msg_ptz_ctrl(LPptz_ctrl);
    }
    else if (strcmp(cmd, OPER_PU_SNAPSHOT) == 0)
    {
        ret = gb_msg_snapshot(buf);
    }
    else if (strcmp(cmd, SET_PU_FTP) == 0)
    {
        ret = gb_msg_set_ftp(buf);
    }
    else if (strcmp(cmd, QUERY_PU_FTP_INFO) == 0)
    {
        ret = gb_msg_get_ftp(buf);
    }
    else if (strcmp(cmd, SET_PU_SERIAL_PORT) == 0)
    {
        ret = gb_msg_set_serial_port(buf);
    }
    else if (strcmp(cmd, QUERY_PU_SERIAL_PORT_INFO) == 0)
    {
        ret = gb_msg_get_serial_port(buf);
    }
    else if (strcmp(cmd, QUERY_PU_TIME_CONFIG_INFO) == 0)
    {
        ret = gb_msg_get_time_cfg(buf);
    }
    else if (strcmp(cmd, SET_PU_TIME) == 0)
    {
        ret = gb_msg_set_pu_time(buf);
    }
    else if (strcmp(cmd, GET_PU_TIME) == 0)
    {
        ret = gb_msg_get_pu_time(buf);
    }
    else if (strcmp(cmd, SET_PU_NETWORK) == 0)
    {
        ret = gb_msg_set_network(buf);
    }
    else if (strcmp(cmd, QUERY_PU_NETWORK_INFO) == 0)
    {
        ret = gb_msg_get_network(buf);
    }
    else if (strcmp(cmd, SET_PU_DISPLAY) == 0)
    {
        ret = gb_msg_set_display(buf);
    }
    else if (strcmp(cmd, QUERY_PU_DISPLAY_INFO) == 0)
    {
        ret = gb_msg_get_display(buf);
    }
    else if (strcmp(cmd, SET_PU_TEXT) == 0)
    {
        ret = gb_msg_set_text(buf);
    }
    else if (strcmp(cmd, QUERY_PU_TEXT_INFO) == 0)
    {
        ret = gb_msg_get_text(buf);
    }
    else if (strcmp(cmd, QUERY_PU_AUDIO_ENCODER_INFO) == 0)
    {
        ret = gb_msg_get_audio_enc(buf);
    }
    else if (strcmp(cmd, SET_PU_VIDEO_ENCODER) == 0)
    {
        ret = gb_msg_set_video_enc(buf);
    }
    else if (strcmp(cmd, QUERY_PU_VIDEO_ENCODER_INFO) == 0)
    {
        ret = gb_msg_get_video_enc(buf);
    }
    else if (strcmp(cmd, OPER_PU_CONTROLPU) == 0) // reboot
    {
        __WARN("reboot service request from gb28181\n");
        __RECORD_LOG_INFO("Reboot service request from gb28181\n");
        anj_sysmng_delay_reboot(1);
    }
    else if (strcmp(cmd, QUERY_PU_PRESET_LIST) == 0)
    {
        ret = gb_msg_query_preset_list(buf);
    }
    else
    {
        gb28181_log("####WORN,Undefine type %s\n", cmd);
        return -2;
    }

    return ret;
}

static int gb_on_stream(unsigned long id, const char *cmd, char *buf, unsigned long size, unsigned long param)
{
    NET_SERVER_START_MEDIA m;
    NET_SERVER_STOP_MEDIA stop;
    (void)param;
    if (cmd == NULL || buf == NULL)
    {
        return -1;
    }

    __INFO("gb_on_stream cmd=%s \n", cmd);

    if (strcmp(cmd, "Start_MEDIA") == 0)
    {
        if (size < sizeof(m))
        {
            return -1;
        }
        memset(&m, 0, sizeof(m));
        memcpy(&m, buf, sizeof(m));
        if (m.request_type == REQUEST_REAL_STREAM)
        {
            return gb_start_session(id, &m, s_live, 0);
        }
        if (m.request_type == REQUEST_FILE_STREAM)
        {
            return gb_start_session(id, &m, s_playback, 1);
        }
        if (m.request_type == 2)
        {
            return gb_start_session(id, &m, s_download, 2);
        }
        return 0;
    }
    if (strcmp(cmd, "Stop_MEDIA") == 0)
    {
        if (size < sizeof(stop))
        {
            return -1;
        }
        memset(&stop, 0, sizeof(stop));
        memcpy(&stop, buf, sizeof(stop));
        gb_stop_session(id, s_live, 0);
        gb_stop_session(id, s_playback, 1);
        gb_stop_session(id, s_download, 2);
        return 0;
    }
    if (size < sizeof(m))
    {
        return -1;
    }
    memset(&m, 0, sizeof(m));
    memcpy(&m, buf, sizeof(m));

    if ((strcmp(cmd, "Control_MEDIA") == 0 || strcmp(cmd, "Seek_MEDIA") == 0) &&
        (m.request_type == REQUEST_FILE_STREAM || m.request_type == 2))
    {
        if (s_history_channel_if.ifHistoryChannelPosition)
        {
            HistoryChannelPositionInfo pos;
            memset(&pos, 0, sizeof(pos));
            pos.cSeekTime.wYear = m.start_time.year;
            pos.cSeekTime.wMonth = m.start_time.month;
            pos.cSeekTime.wDay = m.start_time.date;
            pos.cSeekTime.wHour = m.start_time.hour;
            pos.cSeekTime.wMinute = m.start_time.minute;
            pos.cSeekTime.wSecond = m.start_time.second;
            __INFO("gb28181_lib %s stream_id=%lu start_time=%u-%u-%u %u:%u:%u (local)\n",
                   cmd, id,
                   (unsigned)pos.cSeekTime.wYear, (unsigned)pos.cSeekTime.wMonth, (unsigned)pos.cSeekTime.wDay,
                   (unsigned)pos.cSeekTime.wHour, (unsigned)pos.cSeekTime.wMinute, (unsigned)pos.cSeekTime.wSecond);
            return s_history_channel_if.ifHistoryChannelPosition(id, (char *)&pos);
        }
    }
    if (strcmp(cmd, "Pause_MEDIA") == 0 &&
        (m.request_type == REQUEST_FILE_STREAM || m.request_type == 2) &&
        s_history_channel_if.ifHistoryChannelPauseCapture)
    {
        return s_history_channel_if.ifHistoryChannelPauseCapture(id, 1);
    }
    if (strcmp(cmd, "Play_MEDIA") == 0 &&
        (m.request_type == REQUEST_FILE_STREAM || m.request_type == 2))
    {
        if (s_history_channel_if.ifHistoryChannelPauseCapture)
        {
            s_history_channel_if.ifHistoryChannelPauseCapture(id, 0);
        }
        if (s_history_channel_if.ifHistoryChannelSetSpeed && m.scale > 0.0f)
        {
            unsigned long speed = (unsigned long)(m.scale + 0.5f);
            if (speed == 0)
            {
                speed = 1;
            }
            s_history_channel_if.ifHistoryChannelSetSpeed(id, speed);
        }
        return 0;
    }
    return 0;
}

static int gb_on_audio(unsigned long id, char *data, unsigned long size)
{
    (void)id;
    media_codec_type_e talk_codec_type = MEDIA_CODEC_AUDIO_G711U; // 库内部固定输出G711U
    int talk_samplerate = 8000; // 库内部固定输出8000Hz
    int talk_bitrate = 16000; // 库内部固定输出16000bps

    if (data == NULL || size == 0 || size > (unsigned long)INT_MAX)
    {
        return -1;
    }
    audio_talk_status_set(1);
    return audio_talk_feed_audio(data, (int)size, talk_codec_type, talk_samplerate, talk_bitrate);
}

int Service_GB2818_HistoryQueryMultiTypeIf(const void *pInterface)
{
    if (pInterface == NULL)
    {
        return -1;
    }
    memcpy(&s_history_query_if, pInterface, sizeof(s_history_query_if));
    return 0;
}

int Service_GB2818_HistoryChannelMultiTypeIf(const void *pInterface)
{
    if (pInterface == NULL)
    {
        return -1;
    }
    memcpy(&s_history_channel_if, pInterface, sizeof(s_history_channel_if));
    return 0;
}

int Service_GB2818_SetStreamIf(LPGB28181_stream_CB pInterface)
{
    if (pInterface == NULL)
    {
        return -1;
    }
    memcpy(&s_stream_if, pInterface, sizeof(s_stream_if));
    return 0;
}

int Service_GB2818_SetPtzIf(LPGB28181_ptz_CB pInterface)
{
    if (pInterface == NULL)
    {
        return -1;
    }
    memcpy(&s_ptz_if, pInterface, sizeof(s_ptz_if));
    return 0;
}

void Service_GB28181_UpdateAudioType()
{
    MediaConfig *m = (MediaConfig *)getMediaConfig();
    AudioEncode *a = &m->audioConfig.audioEncode;
    s_audio_type_live = audio_encode_type_get(a->audioEncodeType.typeName);
}

int Service_GB28181_AlarmEventDeal(int alarmCode, SYSTEM_TIME *pAlarmtime)
{
    int iRet = -1;
    int iChannel = 0;

    if (!s_bInitService || pAlarmtime == NULL)
    {
        __ERR("Invalid Input alarmtime\n");
        return -1;
    }

    __INFO("now gb28181 alarm code:%d, time:%d:%d:%d!!!\n",
           alarmCode, pAlarmtime->hour, pAlarmtime->minute, pAlarmtime->second);

    int alarmtype = -1;
    switch (alarmCode)
    {
        case ALARM_CODE_MOTION_DETECT:
            alarmtype = CAM_MOTIONDETECT;
            break;

        case ALARM_CODE_VIDEO_AI:
            /* 这里没有携带子类型(AlarmLevel)；先按“人形”上报。 */
            alarmtype = VCA_PD;
            break;

        default:
            return iRet;
    }

    iRet = net_send_alarm(iChannel, alarmtype);
    return iRet;
}

int Service_GB2818_Open(Service_GB28181_Cfg *sip_info)
{
    int ret = 0;
    if (s_bInitService || gb_load_sip_info(sip_info) != 0)
    {
        return -1;
    }

    memset(s_live, 0, sizeof(s_live));
    memset(s_playback, 0, sizeof(s_playback));
    memset(s_download, 0, sizeof(s_download));
    Service_GB28181_UpdateAudioType();
    ret = net_set_platform(GB_PLATFORM_GENERIC);
    __INFO("gb28181_lib set platform ret=%d\n", ret);
    if (net_initlib(s_net_info, gb_on_login, gb_on_msg, gb_on_stream, 
                    gb_on_audio, 0, LINK_MODE_UDP) != 0)
    {
        return -1;
    }

    s_bInitService = 1;
    return 0;
}

int Service_GB2818_Close(void)
{
    if (!s_bInitService)
    {
        return -1;
    }
    s_bInitService = 0;
    memset(s_live, 0, sizeof(s_live));
    memset(s_playback, 0, sizeof(s_playback));
    memset(s_download, 0, sizeof(s_download));
    return net_fililib();
}

int Service_GB2818_SendVStream(void *pHandle, media_frame_info_t *pFrameInfo)
{
    if (!s_bInitService || pHandle == NULL || pFrameInfo == NULL)
    {
        __ERR("Invalid parameters. \n");
        return -1;
    }

    ANJ_MBUF_POPER *pPoper = (ANJ_MBUF_POPER *)pHandle;
    int sess_idx = (int)pPoper->iPopId;
    if (sess_idx < 0 || sess_idx >= MAX_CHANNEL_NUM)
    {
        return -1;
    }
    gb_stream_session_t *sess = &s_live[sess_idx];
    if (sess == NULL)
    {
        return 0;
    }
    if (!sess->active)
    {
        return 0;
    }
    if (sess->wait_iframe && pFrameInfo->frameParam.frameType != MEDIA_VFRAME_I)
    {
        return 0;
    }
    sess->wait_iframe = 0;

    int request_index = (sess->request_index >= 0) ? sess->request_index : 0;
    if (pFrameInfo->frameParam.frameType == MEDIA_AFRAME_A)
    {
        return gb_send_packet(sess->id, request_index, 0,
                              pFrameInfo->frameParam.framePts,
                              (const unsigned char *)pFrameInfo->frameBuf,
                              (size_t)pFrameInfo->frameParam.frameLen,
                              0, 0);
    }
    else
    {
        return gb_send_packet(sess->id, request_index, 0,
                            pFrameInfo->frameParam.framePts,
                            (const unsigned char *)pFrameInfo->frameBuf,
                            (size_t)pFrameInfo->frameParam.frameLen,
                            1, sess->frame_seq++);
    }
}

int Service_GB2818_PbSendVStream(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e EventID)
{
    gb_stream_session_t *sess = NULL;
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;

    if (EventID != PB_CB_START || pFrameInfo == NULL || pHandle == NULL)
    {
        return 0;
    }

    sess = gb_find_playback_session_by_handle(pHandle, s_playback);
    if (sess == NULL)
    {
        sess = gb_find_playback_session_by_handle(pHandle, s_download);
    }
    if (sess == NULL || !sess->active)
    {
        return 0;
    }

    if (anj_record_pb_is_valid(pHandle) && pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A && !pPoper->iDownLoad)
    {
        if ((pPoper->tLastPts > 0) && (pPoper->tSendTime > 0) && (pFrameInfo->frameParam.framePts > pPoper->tLastPts))
        {
            unsigned int diff_ms = (unsigned int)((pFrameInfo->frameParam.framePts - pPoper->tLastPts) / 90);
            unsigned long long elapsed_ms = anj_mw_get_cputime_ms(NULL) - pPoper->tSendTime;
            if (elapsed_ms < diff_ms)
            {
                diff_ms -= (unsigned int)elapsed_ms;
                if (pPoper->iSpeed > PB_SPEED_0)
                {
                    diff_ms /= pPoper->iSpeed;
                    if (pPoper->iSpeed > PB_SPEED_2)
                    {
                        diff_ms /= (pPoper->iSpeed / PB_SPEED_4);
                    }
                }
                if (diff_ms >= 10 && diff_ms <= 1000)
                {
                    usleep((diff_ms - 1) * 1000);
                }
                else if (diff_ms > 0 && diff_ms < 10)
                {
                    usleep(10 * 1000);
                }
            }
        }
        else
        {
            usleep(10 * 1000);
        }
        pPoper->tLastPts = pFrameInfo->frameParam.framePts;
        pPoper->tSendTime = anj_mw_get_cputime_ms(NULL);
    }

    if (sess->wait_iframe)
    {
        if (pFrameInfo->frameParam.frameType != MEDIA_VFRAME_I)
        {
            return 0;
        }
        sess->wait_iframe = 0;
    }

    /* Match demo semantics: i_flag is resolved from pu_id/device mapping. */
    int request_index = (sess->request_index >= 0) ? sess->request_index : 0;
    if (pFrameInfo->frameParam.frameType == MEDIA_AFRAME_A)
    {
        return gb_send_packet(sess->id, request_index, 1,
                              pFrameInfo->frameParam.frameTimeMs,
                              (const unsigned char *)pFrameInfo->frameBuf,
                              (size_t)pFrameInfo->frameParam.frameLen,
                              0, 0);
    }
    else
    {
        return gb_send_packet(sess->id, request_index, 1,
                            pFrameInfo->frameParam.frameTimeMs,
                            (const unsigned char *)pFrameInfo->frameBuf,
                            (size_t)pFrameInfo->frameParam.frameLen,
                            1, sess->frame_seq++);
    }
}
