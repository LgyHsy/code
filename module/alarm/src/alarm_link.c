
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "anj_config.h"
#include "anj_mw_time.h"
#include "anj_mw_log.h"
#include "anj_mw_gpio.h"
#include "anj_mw_hwctrl.h"
#include "anj_mw_thread.h"
#include "anj_mw_file.h"
#include "anj_audio.h"
#include "anj_sysctl.h"
#include "anj_service.h"
#include "anj_ser.h"
#include "anj_ispctl.h"
#include "anj_record.h"
#include "function_list.h"
#include "eventhub.h"
#include "alarm_link.h"

#define ALARM_WLIGHT_FLICKER_PERIOD_TIME 250    // 250ms翻转一次
#define ALARM_WLIGHT_FLICKER_DURATION_TIME 2000 // 持续2s

#define ALARM_LED_DELAY_TIME 5000

#define ALARM_LINK_SEND_P2P_INTERVAL 3       // p2p消息发送间隔3s
#define ALARM_LINK_NOTIFY_RECORD_INTERVAL 10 // 发送录像消息间隔10s

typedef struct
{
    int enable;
    unsigned long long last_send_time; // 老架构几种报警共用一个发送时间
} AlarmLoopBackInfo_t;

static AlarmLoopBackInfo_t s_stAlarmLoopBackInfo = {0};
static anj_thread_s s_stAlarmLinkPtzThread; // ptz联动线程

int alarm_loopback_info_set(int enable)
{
    if (enable != s_stAlarmLoopBackInfo.enable)
    {
        __INFO("alarm loopback enable:% change to:%d\n", s_stAlarmLoopBackInfo.enable, enable);
        s_stAlarmLoopBackInfo.enable = enable;
        s_stAlarmLoopBackInfo.last_send_time = 0;
    }

    return 0;
}

int alarm_is_night_check()
{
    int day = anj_ispctl_day_night_get();
    return (day == 0) ? 1 : 0;
}

void alarm_get_arming_mp3(const char *pLanguage, const char *pFileName, char szFileFullName[FILE_PLAY_PATH_MAX_LEN])
{
    szFileFullName[0] = 0; // 先置空，用于不存在文件时快速返回

    // 先截掉路径
    const char *pName = strGetFilename(pFileName);
    if (NULL == pName)
        return;

    char szPath[FILE_PLAY_PATH_MAX_LEN] = {0};
    snprintf(szPath, sizeof(szPath), "%s/%s", "/mnt/nand", pName);
    if (access(szPath, F_OK) == 0)
    {
        snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
        return;
    }

    if (SUPPORT_NAND_FLASH)
    {
        snprintf(szPath, sizeof(szPath), "%s/%s", "/data/mp3", pName);
        if (access(szPath, F_OK) == 0)
        {
            snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
            return;
        }
    }

    snprintf(szPath, sizeof(szPath), "%s/%s", "/tmp/oem/mp3", pName);
    if (access(szPath, F_OK) == 0)
    {
        snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
        return;
    }

    snprintf(szPath, sizeof(szPath), "%s/%s", ANJ_MP3_ALARM_PATH, pName);
    if (access(szPath, F_OK) == 0)
    {
        snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
        return;
    }

    if ((pLanguage == NULL || 0 == strcasecmp(pLanguage, "zh_cn") || 0 == strcasecmp(pLanguage, "zh_tw")) && access("/opt/ch/flag.p2p.polishVoicePrompt", F_OK) != 0) // 波兰语强制优先使用en目录
    {
        snprintf(szPath, sizeof(szPath), "%s/%s/%s", ANJ_MP3_ALARM_PATH, "ch", pName);
        if (access(szPath, F_OK) == 0)
        {
            snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
            return;
        }

        snprintf(szPath, sizeof(szPath), "%s/%s/%s", ANJ_MP3_ALARM_PATH, "en", pName);
        if (access(szPath, F_OK) == 0)
        {
            snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
            return;
        }
    }
    else
    {
        snprintf(szPath, sizeof(szPath), "%s/%s/%s", ANJ_MP3_ALARM_PATH, "en", pName);
        if (access(szPath, F_OK) == 0)
        {
            snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
            return;
        }

        snprintf(szPath, sizeof(szPath), "%s/%s/%s", ANJ_MP3_ALARM_PATH, "ch", pName);
        if (access(szPath, F_OK) == 0)
        {
            snprintf(szFileFullName, FILE_PLAY_PATH_MAX_LEN, "%s", szPath);
            return;
        }
    }

    return;
}

int alarm_arming_with_muti_timespan_check(ArmingStruct *pstArmingSetting, int bNight)
{
    int enable = 0;
    switch (pstArmingSetting->enable_flag)
    {
    case ARMING_DISABLE:
        enable = 0;
        break;
    case ARMING_ALLDAY:
        enable = 1;
        break;
    case ARMING_DAYTIME:
        enable = (bNight == 0) ? 1 : 0;
        break;
    case ARMING_NIGHT:
        enable = (bNight == 0) ? 0 : 1;
        break;
    case ARMING_CUSTOM:
    {
        int in_span = 0;
        struct tm ptm = {0};
        SystemLocalTime(&ptm);

        int now_sec = ptm.tm_hour * 3600 + ptm.tm_min * 60 + ptm.tm_sec;

        int iIndex = 0;
        for (iIndex = 0; iIndex < DAY_TIMESPAN_MAX_NUM && iIndex < pstArmingSetting->timespan_num; iIndex++)
        {
            int start_sec = pstArmingSetting->timeSpans[iIndex].startTime.hour * 3600 + pstArmingSetting->timeSpans[iIndex].startTime.minute * 60 + pstArmingSetting->timeSpans[iIndex].startTime.sec;
            int end_sec = pstArmingSetting->timeSpans[iIndex].endTime.hour * 3600 + pstArmingSetting->timeSpans[iIndex].endTime.minute * 60 + pstArmingSetting->timeSpans[iIndex].endTime.sec;

            if (start_sec <= end_sec) // 跨天
            {
                in_span = (now_sec >= start_sec && now_sec <= end_sec);
            }
            else
            {
                in_span = (now_sec >= start_sec || now_sec <= end_sec);
            }

            if (in_span == 1)
            {
                break;
            }
        }

        if (!in_span)
        {
            __ERR("now_sec:%d not in timespans. timespan num:%d\n", now_sec, pstArmingSetting->timespan_num);
        }

        enable = in_span;
    }
    break;

    default:
        enable = 0;
        break;
    }

    return enable;
}

int alarm_arming_with_timespan_check(ArmingMode enable_flag, int bNight, const TimeSpanCfg *ptimeSpan)
{
    int enable = 0;

    switch (enable_flag)
    {
    case ARMING_DISABLE:
        enable = 0;
        break;
    case ARMING_ALLDAY:
        enable = 1;
        break;
    case ARMING_DAYTIME:
        enable = (bNight == 0) ? 1 : 0;
        break;
    case ARMING_NIGHT:
        enable = (bNight == 0) ? 0 : 1;
        break;
    case ARMING_CUSTOM:
    {
        if (0 == CheckNowIsInTimeSpan(ptimeSpan))
            enable = 0;
        else
            enable = 1;
    }
    break;
    default:
        enable = 0;
        break;
    }

    return enable;
}

int alarm_get_server_ip_and_port(char *url, char *ip, int iplen, char *port, int portlen, char *method, int methodlen)
{
    char urlBuf[256] = {0};
    char buf[256] = {0};
    int cpylen = 0;

    if (strstr(url, "http") != NULL)
    {
        char *pUrl = strstr(url, "//");
        if (pUrl != NULL)
        {
            strcpy(buf, pUrl + 2);
        }
    }
    else
    {
        strcpy(buf, url);
    }

    __ERR("alarm server url:%s\n", buf); // urlBuf:192.168.12.188:8089/protocol/userPrivacy

    if (strstr(url, "http") != NULL && strstr(buf, ":") != NULL) // http://192.168.12.188:8089/protocol/userPrivacy
    {
        char *p = strstr(url, "//");
        if (p != NULL)
        {
            strcpy(urlBuf, p + 2);
            __ERR("alarm server url:%s\n", urlBuf); // urlBuf:192.168.12.188:8089/protocol/userPrivacy
        }

        if (strstr(urlBuf, "/") != NULL) /* 192.168.12.188:8089/protocol/userPrivacy */
        {
            p = strstr(urlBuf, ":");
            if (p != NULL)
            {
                cpylen = (int)(strlen(urlBuf) - strlen(p));
                snprintf(ip, iplen, "%.*s", cpylen, urlBuf);

                __ERR("alarm-111 server ip:%s\n", ip);

                char tmpBuf[128] = {0};
                strcpy(tmpBuf, p + 1); // tmpBuf:8089/protocol/userPrivacy

                p = strstr(tmpBuf, "/");
                if (p != NULL)
                {
                    cpylen = (int)(strlen(tmpBuf) - strlen(p));
                    snprintf(port, portlen, "%.*s", cpylen, tmpBuf);
                    strcpy(method, p);

                    __ERR("alarm-111 server port:%s, method:%s\n", port, method);
                }
            }
        }
        else /* 192.168.12.188:8089 */
        {
            p = strstr(urlBuf, ":");
            if (p != NULL)
            {
                cpylen = (int)(strlen(urlBuf) - strlen(p));
                snprintf(ip, iplen, "%.*s", cpylen, urlBuf);
                strcpy(port, p + 1);

                // strcpy(method,"/");
                __ERR("alarm-222 server ip:%s, port:%s, method:%s\n", ip, port, method);
            }
        }
    }
    else if (strstr(url, "http") == NULL && strstr(buf, ":") != NULL) /* 192.168.12.188:8089/protocol/userPrivacy */
    {
        if (strstr(url, "/") != NULL) /* 192.168.12.188:8089/protocol/userPrivacy */
        {
            char *p = strstr(url, ":");
            if (p != NULL)
            {
                cpylen = (int)(strlen(url) - strlen(p));
                snprintf(ip, iplen, "%.*s", cpylen, url);
                __ERR("alarm-333 server ip:%s\n", ip);

                char tmpBuf[128] = {0};
                strcpy(tmpBuf, p + 1); // tmpBuf:8089/protocol/userPrivacy

                p = strstr(tmpBuf, "/");
                if (p != NULL)
                {
                    cpylen = (int)(strlen(tmpBuf) - strlen(p));
                    snprintf(port, portlen, "%.*s", cpylen, tmpBuf);
                    strcpy(method, p);

                    __ERR("alarm-333 server port:%s, method:%s\n", port, method);
                }
            }
        }
        else /* 192.168.12.188:8089 */
        {
            char *p = strstr(url, ":");
            if (p != NULL)
            {
                cpylen = (int)(strlen(url) - strlen(p));
                snprintf(ip, iplen, "%.*s", cpylen, url);
                strcpy(port, p + 1);
                // strcpy(method,"/");
                __ERR("alarm-444 server ip:%s, port:%s\n", ip, port);
            }
        }
    }
    else if (strstr(url, "http") != NULL && strstr(buf, ":") == NULL) /* http://192.168.12.188/protocol/userPrivacy */
    {
        char *p = strstr(url, "//");
        if (p != NULL)
        {
            strcpy(urlBuf, p + 2);
            __ERR("alarm server url:%s\n", urlBuf); // urlBuf:192.168.12.188/protocol/userPrivacy
        }

        if (strstr(urlBuf, "/") != NULL) /* 192.168.12.188/protocol/userPrivacy */
        {
            p = strstr(urlBuf, "/");
            if (p != NULL)
            {
                cpylen = (int)(strlen(urlBuf) - strlen(p));
                snprintf(ip, iplen, "%.*s", cpylen, urlBuf);
                __ERR("alarm-555 server ip:%s\n", ip);
                strcpy(port, "80");
                strcpy(method, p);
                __ERR("alarm-555 server port:%s, method:%s\n", port, method);
            }
        }
        else /* 192.168.12.188 */
        {
            cpylen = (int)strlen(urlBuf);
            snprintf(ip, iplen, "%.*s", cpylen, urlBuf);
            strcpy(port, "80");
            // strcpy(method,"/");
            __ERR("alarm-666 server ip:%s , port:%s\n", ip, port);
        }
    }
    else if (strstr(url, "http") == NULL && strstr(buf, ":") == NULL) /* 192.168.12.188/protocol/userPrivacy */
    {
        if (strstr(url, "/") != NULL) /* 192.168.12.188/protocol/userPrivacy */
        {
            char *p = strstr(url, "/");
            if (p != NULL)
            {
                cpylen = (int)(strlen(url) - strlen(p));
                snprintf(ip, iplen, "%.*s", cpylen, url);
                __ERR("alarm-777 server ip:%s\n", ip);
                strcpy(port, "80");
                strcpy(method, p);
                __ERR("alarm-777 server port:%s, method:%s\n", port, method);
            }
        }
        else /* 192.168.12.188 */
        {
            cpylen = (int)strlen(url);
            snprintf(ip, iplen, "%.*s", cpylen, url);
            strcpy(port, "80");
            // strcpy(method,"/");
            __ERR("alarm-888 server ip:%s, port:%s\n", ip, port);
        }
    }

    return 0;
}

int alarm_connect_to_server_center(char *serverIp, char *port, int *alarmSockfd)
{
    struct sockaddr_in server_addr;
    int ret = -1;
    int fd = 0;

    __ERR("@@@@@@@@@alarm server center ip:%s \n", serverIp);

    /* 客户程序开始建立 sockfd描述符 */
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) /*建立SOCKET连接*/
    {
        __ERR("Socket Error:%s\a\n", strerror(errno));
        return -1;
    }
    __ERR("@@@@@@@@@socket creat success\n");

    /* 客户程序填充服务端的资料 */
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
#if 0 // 测试接口
	server_addr.sin_port = htons(80);	
	server_addr.sin_addr.s_addr = inet_addr("103.75.104.41");
#else // 正式接口
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = inet_addr(serverIp);
#endif

    /* 客户程序发起连接请求 */
    ret = connect(fd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr));
    if (ret < 0) /*连接网站*/
    {
        __ERR("Connect Error:%s\a\n", strerror(errno));
        close(fd);
        return -1;
    }

    *alarmSockfd = fd;
    __ERR("@@@@@@@@@@client connect success\n");

    return 0;
}

int alarm_send_to_server_center(char *alarmEvent, SYSTEM_TIME alarmTime,
                                char *serverIp, char *port, char *method, char *username, char *password)
{
    // 交给HAPI订阅去处理
    return 0;

    int ret = -1;
    char request[256] = {0};
    int nbytes = 0;
    int sendbyte = 0;
    int totalsend = 0;
    int alarmSockfd = 0;

    ret = alarm_connect_to_server_center(serverIp, port, &alarmSockfd);
    if (ret == 0) //
    {
        __ERR("send Alarm informmation to alarm Server Center, alarmSockfd:%d \n", alarmSockfd);
#if 0 // 测试接口:103.46.128.21
        sprintf(request,
        "GET /Home/Index/test_camera?sip=%s HTTP/1.1\r\n"
        "Host: web.jsjcloud.com\r\n"
        "Accept: */*\r\n"
        "\r\n",
        g_info.sip_net->lcId);

        __ERR("request:\n%s\n", request);
#else // 正式接口
        snprintf(request, sizeof(request),
                 "GET %s?username=%s&password=%s&event=%s&alarmTime=%d%d%d_%02d%02d%02d HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Accept: */*\r\n"
                 "\r\n",
                 method, username, password, alarmEvent,
                 alarmTime.year, alarmTime.month, alarmTime.day,
                 alarmTime.hour, alarmTime.minute, alarmTime.second,
                 serverIp);

        __ERR("request:\n%s\n", request);
#endif

        nbytes = strlen(request);
        while (totalsend < nbytes)
        {
            sendbyte = safe_send(alarmSockfd, request + totalsend, nbytes - totalsend, 0);
            if (sendbyte <= 0)
            {
                __ERR("send error!%s\n", strerror(errno));
                close(alarmSockfd);
                return -1;
            }

            totalsend += sendbyte;
            __ERR("%d bytes send OK!\n", totalsend);
        }

#if 0
        int recbyte = 0;
        char recBuff[1024];

        memset(recBuff,0,sizeof(recBuff));
        recbyte = recv(g_alarmSockfd,recBuff,1024,0);
        if(recbyte < 0)
        {
        __ERR("rec error!%s\n", strerror(errno));
        }	
        __ERR("recbyte: %d , recBuff: \n%s\n",recbyte,recBuff);
#endif
        close(alarmSockfd);
    }
    else
    {
        __ERR("Connect alarm server center failed! serverIp:%s port:%s\n",
              serverIp, port);
    }

    return 0;
}

int alarm_send_to_loopback(char *alarmEvent, SYSTEM_TIME alarmTime, char *serverIp, char *port)
{
    int ret = -1;
    char request[256] = {0};
    int nbytes = 0;
    int sendbyte = 0;
    int totalsend = 0;
    int alarmSockfd = 0;

    ret = alarm_connect_to_server_center(serverIp, port, &alarmSockfd);
    if (ret == 0)
    {
        snprintf(request, sizeof(request),
                 "AlarmInfo:alarmevent=%s,alarmTime=%d-%d-%d %02d:%02d:%02d",
                 alarmEvent,
                 alarmTime.year, alarmTime.month, alarmTime.day,
                 alarmTime.hour, alarmTime.minute, alarmTime.second);

        __ERR("alarm send informmation to loop back, alarmSockfd:%d, alarm msg:%s\n", alarmSockfd, request);
        nbytes = strlen(request);

        while (totalsend < nbytes)
        {
            sendbyte = safe_send(alarmSockfd, request + totalsend, nbytes - totalsend, 0);
            if (sendbyte <= 0)
            {
                __ERR("alarm send error!%s\n", strerror(errno));
                close(alarmSockfd);
                return -1;
            }

            totalsend += sendbyte;
            __ERR("alarm total send %d bytes OK, tatal len:%d !\n", totalsend, nbytes);
        }
    }
    else
    {
        __ERR("alarm connect server center failed! serverIp:%s port:%s\n", serverIp, port);
    }

    close(alarmSockfd);
    return 0;
}

int alarm_link_alarm_output(int portIndex, int is_night, OutPutAlarm *pOutputAlm)
{
    if (portIndex <= 0 || portIndex > ALARM_OUT_MAX_NUM)
    {
        return -1;
    }

    int iRet = 0;
    int index = portIndex - 1;
    OutputChannel *pOutputChannel = &pOutputAlm->outputChannels[index];

    iRet = alarm_arming_with_muti_timespan_check(&pOutputChannel->enable, is_night);
    if (iRet == 0)
    {
        return -1;
    }

    int TargetValue = 0;
    if (strcmp(pOutputChannel->triggerType.name, "HIGH") == 0)
    {
        TargetValue = 1;
    }
    else
    {
        TargetValue = 0;
    }

    // 报警输出gpio翻转
    anj_mw_hwctrl_alarmout_toggle_delay(portIndex, TargetValue, pOutputChannel->duration * 1000);
    return 0;
}

int alarm_link_sms_send(const char *pDstNumber, const char *pText, int min_interval, int voiceplayresult)
{
    // todo 比较久远的功能暂时没用了，先不加
    __ERR("alarm link sms send but don't handle!\n");
    return 0;
}

int alarm_push_enable_check(ArmingStruct alarm_push, int is_night)
{
    int enable = 0;
    enable = alarm_arming_with_muti_timespan_check(&alarm_push, is_night);
    return enable;
}

static int alarm_link_ptz_thread(void *ctx, int *bStart)
{
    PTZAction *pAction = (PTZAction *)ctx;

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    if (strcmp(pAction->actionType.actionName, "PresetPosition") == 0)
    {
        strncpy(stPtzCmdParse.ptzCmd, "callpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = pAction->action.preset.postion.positionIndex;
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }
    else if (strcmp(pAction->actionType.actionName, "PositionLoop") == 0)
    {
        if (pAction->action.loop.positionCount != 2)
        {
            __ERR("alarm ptz action loop positionCount:%d error!\n", pAction->action.loop.positionCount);
            return -1;
        }

        memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
        memset(&event_result, 0, sizeof(event_result));

        // call preset 1
        strncpy(stPtzCmdParse.ptzCmd, "callpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = pAction->action.loop.ptzPositions[0].positionIndex;
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        sleep(pAction->action.loop.interval);

        // set scan begin
        memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
        strncpy(stPtzCmdParse.ptzCmd, "ScanBegin", sizeof(stPtzCmdParse.ptzCmd));
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        sleep(pAction->action.loop.interval);

        // call preset 2
        memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
        strncpy(stPtzCmdParse.ptzCmd, "callpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = pAction->action.loop.ptzPositions[1].positionIndex;
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        sleep(pAction->action.loop.interval);

        // set scan end
        memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
        strncpy(stPtzCmdParse.ptzCmd, "ScanEnd", sizeof(stPtzCmdParse.ptzCmd));
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        sleep(pAction->action.loop.interval);

        // scan on
        memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
        strncpy(stPtzCmdParse.ptzCmd, "ScanOn", sizeof(stPtzCmdParse.ptzCmd));
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        sleep(pAction->action.loop.interval);

        // scan off
        memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
        strncpy(stPtzCmdParse.ptzCmd, "ScanOff", sizeof(stPtzCmdParse.ptzCmd));
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        sleep(pAction->action.loop.interval);
    }
    else if (strcmp(pAction->actionType.actionName, "PositionWalk") == 0)
    {
        int i = 0;
        int j = 0;

        for (j = 0; j < pAction->action.walk.walkCount; j++)
        {
            for (i = 0; i < pAction->action.walk.positionCount; i++)
            {
                memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
                strncpy(stPtzCmdParse.ptzCmd, "callpreset", sizeof(stPtzCmdParse.ptzCmd));
                stPtzCmdParse.presetID = pAction->action.loop.ptzPositions[i].positionIndex;
                eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
                sleep(pAction->action.loop.interval);
            }
        }
    }

    return 0;
}

// 报警联动-output：翻转一次报警输出gpio口
int alarm_link_alarm_output_action(int event, int is_night, AlarmOutputAction *pAlmOutput, OutPutAlarm *pOutputAlarm)
{
    if (event < 0 || event >= ALARM_CODE_END)
    {
        return -1;
    }

    int chn = 0;
    OutputChannelAction *pOutputChannelAction = NULL;

    for (chn = 0; chn < MAX_OUTPUT_CHANENL_COUNT; chn++)
    {
        pOutputChannelAction = &(pAlmOutput->outputChnlActions[chn]);
        if (pOutputChannelAction->enable)
        {
            alarm_link_alarm_output(pOutputChannelAction->portIndex, is_night, pOutputAlarm);
        }
    }

    return 0;
}

// 报警联动-audio play：播放报警语音
int alarm_link_audio_play_action(unsigned long long *pLastPlayTime, unsigned long long now_time, char *filename, AudioPlayAction *pAction)
{
    if (now_time - *pLastPlayTime >= pAction->intervalsecnods * 1000)
    {
        if (filename == NULL)
        {
            const char *pName = strGetFilename(pAction->filename);
            if (pName && strcmp(pName, UPLOAD_MP3_FILE_NAME) == 0)
            {
                anj_audio_prompt_play(DATA_BLOCK_MOUNT_PATH, UPLOAD_MP3_FILE_NAME, pAction->times);
            }
            else
            {
                anj_audio_prompt_play(ANJ_MP3_ALARM_PATH, (char *)pName, pAction->times);
            }
        }
        else
        {
            anj_audio_prompt_play(ANJ_MP3_ALARM_PATH, filename, pAction->times);
        }

        *pLastPlayTime = now_time;
    }

    return 0;
}

// 报警联动-White Light：间隔250ms亮灭，持续2s
void alarm_link_wlight_action()
{
    return; // 后续定制产品再做逻辑
    anj_mw_hwctrl_wlight_period_flicker(ALARM_WLIGHT_FLICKER_PERIOD_TIME, ALARM_WLIGHT_FLICKER_DURATION_TIME);
}

// 报警联动-LED：先打开5s后关闭
void alarm_link_alarmled_action()
{
    anj_mw_hwctrl_alarmled_toggle_delay(ALARM_LED_DELAY_TIME);
}

// 报警联动-sms：4g短信发送，暂时没用了
int alarm_link_sms_action()
{
    int iRet = 0;
    AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();
    SMSAlarm *pSmsAlarm = &pAlarmCfg->normalAlarm.smsAlarm;

    if (pSmsAlarm->enable > 0 && strlen(pSmsAlarm->szSmsDstNum) > 0 && strlen(pSmsAlarm->szSmsFixContent) > 0)
    {
        iRet = alarm_link_sms_send(pSmsAlarm->szSmsDstNum, pSmsAlarm->szSmsFixContent, pSmsAlarm->min_interval, pSmsAlarm->playresult);
    }

    return iRet;
}

void alarm_link_send_server_center_action(AjAlarmCode code, SYSTEM_TIME alarmtime)
{
    char alarmtype[32] = {0};

    if (ALARM_CODE_VIDEO_AI == code)
    {
        snprintf(alarmtype, sizeof(alarmtype), "humanDetectAlarm");
    }
    else if (ALARM_CODE_MOTION_DETECT == code)
    {
        snprintf(alarmtype, sizeof(alarmtype), "motionDetionAlarm");
    }
    else if (ALARM_CODE_IO_ALARM == code)
    {
        snprintf(alarmtype, sizeof(alarmtype), "IOAlarm");
    }
    else
    {
        __ERR("alarm code:%d don't support send to center!\n", code);
        return;
    }

    NetworkConfigNew *networkcfg = (NetworkConfigNew *)getNetWorkConfig();
    if (strlen(networkcfg->alarmServerCfg.url) > 0)
    {
        char ip[32] = {0};
        char port[16] = {0};
        char method[32] = {0};
        alarm_get_server_ip_and_port(networkcfg->alarmServerCfg.url, ip, sizeof(ip), port, sizeof(port), method, sizeof(method));

        alarm_send_to_server_center(alarmtype, alarmtime, ip, port, method, networkcfg->alarmServerCfg.userName, networkcfg->alarmServerCfg.password);
    }

    return;
}

int alarm_link_send_loopback_action(AjAlarmCode code, unsigned long long now_time, SYSTEM_TIME alarmtime)
{
    char alarmtype[32] = {0};

    if (ALARM_CODE_VIDEO_AI == code)
    {
        snprintf(alarmtype, sizeof(alarmtype), "humanDetectAlarm");
    }
    else if (ALARM_CODE_MOTION_DETECT == code)
    {
        snprintf(alarmtype, sizeof(alarmtype), "motionDetionAlarm");
    }
    else if (ALARM_CODE_IO_ALARM == code)
    {
        snprintf(alarmtype, sizeof(alarmtype), "IOAlarm");
    }
    else
    {
        __ERR("alarm code:%d don't support send to loopback!\n", code);
        return 0;
    }

    if (s_stAlarmLoopBackInfo.enable)
    {
        if (now_time - s_stAlarmLoopBackInfo.last_send_time > 10 * 1000)
        {
            s_stAlarmLoopBackInfo.last_send_time = now_time;
            alarm_send_to_loopback(alarmtype, alarmtime, "127.0.0.1", "889");
        }
    }

    return 0;
}

// 报警联动-ptz：巡航或者调用预置点
void alarm_link_ptz_action(PTZAction *param)
{
    if (s_stAlarmLinkPtzThread.start > 0)
    {
        __ERR("alarm link ptz already start\n");
        return;
    }

    memset(&s_stAlarmLinkPtzThread, 0, sizeof(anj_thread_s));
    s_stAlarmLinkPtzThread.bAutoDestroy = 1;
    strncpy(s_stAlarmLinkPtzThread.iThreadName, "alarm_ptz_action_thr", sizeof(s_stAlarmLinkPtzThread.iThreadName) - 1);
    s_stAlarmLinkPtzThread.iThreadjob.ctx = (void *)param;
    s_stAlarmLinkPtzThread.iThreadjob.func = alarm_link_ptz_thread;
    anj_thread_task_create(&s_stAlarmLinkPtzThread);
}

void alarm_link_ptz_action_stop()
{
    if (s_stAlarmLinkPtzThread.start > 0)
    {
        anj_thread_task_destroy(&s_stAlarmLinkPtzThread, 0);
    }
}

int alarm_video_cover_link_process(unsigned long long now_time, int is_night, VideoCoverAlarm *pvideocoveralarm, OutPutAlarm *pOutputAlarm)
{
    static unsigned long long tLastPlayTime = 0;

    // output action
    alarm_link_alarm_output_action(ALARM_CODE_VIDEO_COVERD, is_night, &pvideocoveralarm->alarmAction.outputAction, pOutputAlarm);

    // audio play action
    int iRet = alarm_arming_with_muti_timespan_check(&pvideocoveralarm->alarmAction.audioAction.enable, is_night);
    if (iRet == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pvideocoveralarm->alarmAction.audioAction);
    }

    return 0;
}

int alarm_gpio3_high2low_link_process(unsigned long long now_time, int is_night, AlarmChannel *pAlarmConfig)
{
    int iRet = 0;
    static unsigned long long tLastPlayTime = 0;

    // 常开配置，开路了，就报警结束
    if (strcasecmp(pAlarmConfig->triggerType.name, "LOW-HIGH") == 0)
    {
        return 1;
    }

    // IO输出放到ALARM_CODE_IO_ALARM中去触发，用于常闭IO IN开路后，一直触发IO输出 //CHAM 20171222

    // ptz action
    if (pAlarmConfig->alarmAction.ptzAction.enable)
    {
        alarm_link_ptz_action(&pAlarmConfig->alarmAction.ptzAction);
    }

    iRet = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.audioAction.enable, is_night);
    if (iRet == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pAlarmConfig->alarmAction.audioAction);
    }

    iRet = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.notify_sms, is_night);
    if (iRet == 1)
    {
        alarm_link_sms_action();
    }

    return 0;
}

int alarm_gpio3_low2high_link_process(unsigned long long now_time, int is_night, AlarmChannel *pAlarmConfig)
{
    int iRet = 0;
    static unsigned long long tLastPlayTime = 0;

    // 常开配置，开路了，就报警结束
    if (strcasecmp(pAlarmConfig->triggerType.name, "HIGH-LOW") == 0)
    {
        return 1;
    }

    // IO输出放到ALARM_CODE_IO_ALARM中去触发，用于常闭IO IN开路后，一直触发IO输出 //CHAM 20171222

    // ptz action
    if (pAlarmConfig->alarmAction.ptzAction.enable)
    {
        alarm_link_ptz_action(&pAlarmConfig->alarmAction.ptzAction);
    }

    iRet = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.audioAction.enable, is_night);
    if (iRet == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pAlarmConfig->alarmAction.audioAction);
    }

    iRet = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.notify_sms, is_night);
    if (iRet == 1)
    {
        alarm_link_sms_action();
    }

    return 0;
}

int alarm_motion_detect_link_process(unsigned long long now_time, int is_night, MotionDetectAlarm *pMdAlarm, OutPutAlarm *pOutputAlarm)
{
    int iRet = 0;
    int enable_action = 0;
    static unsigned long long tLastPlayTime = 0;

    // record action

    // alarm output action
    alarm_link_alarm_output_action(ALARM_CODE_MOTION_DETECT, is_night, &pMdAlarm->alarmAction.outputAction, pOutputAlarm);

    // audio action
    enable_action = alarm_arming_with_muti_timespan_check(&pMdAlarm->alarmAction.audioAction.enable, is_night);
    if (enable_action == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pMdAlarm->alarmAction.audioAction);
    }

    // light action
    iRet = anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION);
    if (iRet == 1)
    {
        enable_action = alarm_arming_with_muti_timespan_check(&pMdAlarm->alarmAction.light_twinkle_enable, is_night);
        if (enable_action == 1)
        {
            alarm_link_wlight_action();
        }
    }

    // alarmled action
    enable_action = alarm_arming_with_muti_timespan_check(&pMdAlarm->alarmAction.alarm_led_enable, is_night);
    if (enable_action == 1)
    {
        alarm_link_alarmled_action();
    }

    return 0;
}

int alarm_video_gate_link_process(unsigned long long now_time, int is_night, VideoGateAlarm *pAlarmConfig, OutPutAlarm *pOutputAlarm)
{
    int iRet = 0;
    int action_enable = 0;
    static unsigned long long tLastPlayTime = 0;

    // record action

    // output action
    alarm_link_alarm_output_action(ALARM_CODE_MOTION_DETECT, is_night, &pAlarmConfig->alarmAction.outputAction, pOutputAlarm);

    // audio play action
    action_enable = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.audioAction.enable, is_night);
    if (action_enable == 1)
    {
        alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pAlarmConfig->alarmAction.audioAction);
    }

    // light action
    iRet = anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION);
    if (iRet == 1)
    {
        action_enable = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.light_twinkle_enable, is_night);
        if (action_enable == 1)
        {
            alarm_link_wlight_action();
        }
    }

    action_enable = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.alarm_led_enable, is_night);
    if (action_enable == 1)
    {
        alarm_link_alarmled_action();
    }

    return 0;
}

int alarm_ai_detect_link_process(unsigned long long now_time, int is_night, int level, PdAlarm *pdalarmcfg, OutPutAlarm *pOutputAlarm)
{
    int iRet = 0;
    int enable_action = 0;
    static unsigned long long tLastPlayTime = 0;

    // arming_flag check
    if (0 == alarm_arming_with_timespan_check(pdalarmcfg->arming_flag, is_night, &pdalarmcfg->timeSpan))
    {
        return 0;
    }

    // output action
    alarm_link_alarm_output_action(ALARM_CODE_MOTION_DETECT, is_night, &pdalarmcfg->alarmAction.outputAction, pOutputAlarm);

    // audio play action
    enable_action = alarm_arming_with_muti_timespan_check(&pdalarmcfg->alarmAction.audioAction.enable, is_night);
    if (enable_action == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pdalarmcfg->alarmAction.audioAction);
    }

    // light action
    iRet = anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION);
    if (iRet == 1)
    {
        enable_action = alarm_arming_with_muti_timespan_check(&pdalarmcfg->alarmAction.light_twinkle_enable, is_night);
        if (enable_action == 1)
        {
            alarm_link_wlight_action();
        }
    }

    // alarmled action
    enable_action = alarm_arming_with_muti_timespan_check(&pdalarmcfg->alarmAction.alarm_led_enable, is_night);
    if (enable_action == 1)
    {
        alarm_link_alarmled_action();
    }

    return 0;
}

int alarm_ai_fire_link_process(unsigned long long now_time, int is_night, FlameAndFlumesAlarm *pfirealarm, OutPutAlarm *pOutputAlarm, const char *data)
{
    int enable_action = 0;
    static unsigned long long tLastPlayTime = 0;

    __INFO("alarm ai fire link!\n");
    alarm_link_alarm_output_action(ALARM_CODE_VIDEO_AI, is_night, &pfirealarm->alarmAction.outputAction, pOutputAlarm);

    enable_action = alarm_arming_with_muti_timespan_check(&pfirealarm->alarmAction.audioAction.enable, is_night);
    if (enable_action == 1)
    {
        if (strstr(pfirealarm->alarmAction.audioAction.filename, "flame_alarm") != NULL && strstr(data, "flame") != NULL)
        {
            char *filename = "flame_alarm.mp3";
            alarm_link_audio_play_action(&tLastPlayTime, now_time, filename, &pfirealarm->alarmAction.audioAction);
        }
        else if (strstr(pfirealarm->alarmAction.audioAction.filename, "smog_alarm") != NULL && strstr(data, "smog") != NULL)
        {
            char *filename = "smog_alarm.mp3";
            alarm_link_audio_play_action(&tLastPlayTime, now_time, filename, &pfirealarm->alarmAction.audioAction);
        }
        else
        {
            alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pfirealarm->alarmAction.audioAction);
        }
    }

    return 0;
}

int alarm_ai_face_link_process(unsigned long long now_time, int is_night, FaceDetectAlarm *pfacealarm, OutPutAlarm *pOutputAlarm)
{
    int iRet = 0;
    int enable_action = 0;
    static unsigned long long tLastPlayTime = 0;

    alarm_link_alarm_output_action(ALARM_CODE_VIDEO_AI, is_night, &pfacealarm->alarmAction.outputAction, pOutputAlarm);

    // audio play action
    enable_action = alarm_arming_with_muti_timespan_check(&pfacealarm->alarmAction.audioAction.enable, is_night);
    if (enable_action == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pfacealarm->alarmAction.audioAction);
    }

    return iRet;
}

int alarm_ai_region_link_process(unsigned long long now_time, int is_night, VideoRegionAiAlarm *pvideoregionalarm, OutPutAlarm *pOutputAlarm)
{
    int iRet = 0;
    int enable_action = 0;
    static unsigned long long tLastPlayTime = 0;

    alarm_link_alarm_output_action(ALARM_CODE_VIDEO_AI, is_night, &pvideoregionalarm->alarmAction.outputAction, pOutputAlarm);

    enable_action = alarm_arming_with_muti_timespan_check(&pvideoregionalarm->alarmAction.audioAction.enable, is_night);
    if (enable_action == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pvideoregionalarm->alarmAction.audioAction);
    }

    // light action
    iRet = anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION);
    if (iRet == 1)
    {
        enable_action = alarm_arming_with_muti_timespan_check(&pvideoregionalarm->alarmAction.light_twinkle_enable, is_night);
        if (enable_action == 1)
        {
            alarm_link_wlight_action();
        }
    }

    enable_action = alarm_arming_with_muti_timespan_check(&pvideoregionalarm->alarmAction.alarm_led_enable, is_night);
    if (enable_action == 1)
    {
        alarm_link_alarmled_action();
    }

    return 0;
}

int alarm_alarm_in_out_link_process(unsigned long long now_time, int is_night, AlarmChannel *pAlarmConfig, OutPutAlarm *pOutputAlarm)
{
    int iRet = 0;
    int enable_action = 0;
    static unsigned long long tLastPlayTime = 0;

    // alarm output action
    alarm_link_alarm_output_action(ALARM_CODE_IO_ALARM, is_night, &pAlarmConfig->alarmAction.outputAction, pOutputAlarm);

    // audio play action
    enable_action = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.audioAction.enable, is_night);
    if (enable_action == 1)
    {
        iRet = alarm_link_audio_play_action(&tLastPlayTime, now_time, NULL, &pAlarmConfig->alarmAction.audioAction);
    }

    // light action
    iRet = anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION);
    if (iRet == 1)
    {
        enable_action = alarm_arming_with_muti_timespan_check(&pAlarmConfig->alarmAction.light_twinkle_enable, is_night);
        if (enable_action == 1)
        {
            alarm_link_wlight_action();
        }
    }

    return 0;
}

int alarm_link_notify_record_action(int chn, AjAlarmCode code, int level)
{
    if ((code == ALARM_CODE_IO_ALARM) || (code == ALARM_CODE_MOTION_DETECT)
        // added by XXX 20120904 to support linkdown record
        || (code == ALARM_CODE_LINKDOWN) || (code == ALARM_CODE_LINKUP) || (code == ALARM_CODE_VIDEO_AI) || (code == ALARM_CODE_VIDEO_GATE) || (code == ALARM_CODE_AUDIO_LSA) || (code == ALARM_CODE_AUDIO_BABYCRY))
    {
        event_alarm_s stAlarm = {0};
        EventResult event_result = {0};

        anj_record_alarm_handle(chn, code, level);

        stAlarm.chn = chn;
        stAlarm.code = code;
        stAlarm.level = level;
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_NFS_ALARM, &event_result, &stAlarm);
    }

    return 0;
}

// 分发报警到协议
int alarm_link_send_service_action(alarm_event_data alarm_event, unsigned long long now_time, int is_night, int is_new_alarm, AlarmConfig *pAlarmCfg)
{
    int p2p_enable = 0;

    int code = alarm_event.alarm_code;
    int level = alarm_event.alarm_level;

    if (code == ALARM_CODE_IO_ALARM_FINISH)
    {
        p2p_enable = 1;
    }
    else if (is_new_alarm == 1) // 要求发送到p2p先判断该报警类型是否在持续触发，如果在持续触发则不上报
    {
        p2p_enable = 1;

        ArmingStruct *pAlarmPush = NULL;
        if (ALARM_CODE_MOTION_DETECT == code || ALARM_CODE_MOTION_DETECT_DISAPPEAR == code)
        {
            pAlarmPush = &pAlarmCfg->normalAlarm.motionDetectAlarm[0].alarmAction.alarm_push;
            p2p_enable = alarm_arming_with_muti_timespan_check(pAlarmPush, is_night);
        }
        else if (ALARM_CODE_VIDEO_AI == code || ALARM_CODE_VIDEO_AI_FINISH == code)
        {
            if (ALARM_AI_FACEDETECT == level)
            {
                p2p_enable = 1;
            }
            else if (ALARM_AI_LPR == level)
            {
                p2p_enable = 1;
            }
            else if (ALARM_AI_VIDEO_REGION_DETECT_ENTER == level || ALARM_AI_VIDEO_REGION_DETECT_LEAVE == level || ALARM_AI_VIDEO_REGION_DETECT_STAY == level)
            {
                pAlarmPush = &pAlarmCfg->aiAlarm.regionAiAlarm[0].alarmAction.alarm_push;
                p2p_enable = alarm_arming_with_muti_timespan_check(pAlarmPush, is_night);
            }
            else if (ALARM_AI_VIDEO_GATE == level)
            {
                pAlarmPush = &pAlarmCfg->aiAlarm.vgAlarm[0].alarmAction.alarm_push;
                p2p_enable = alarm_arming_with_muti_timespan_check(pAlarmPush, is_night);
            }
            else if (ALARM_AI_FIRE == level)
            {
                pAlarmPush = &pAlarmCfg->aiAlarm.fireAlarm.alarmAction.alarm_push;
                p2p_enable = alarm_arming_with_muti_timespan_check(pAlarmPush, is_night);
            }
            else
            {
                pAlarmPush = &pAlarmCfg->aiAlarm.pdAlarm[0].alarmAction.alarm_push;
                p2p_enable = alarm_arming_with_muti_timespan_check(pAlarmPush, is_night);
            }
        }
        else if (ALARM_CODE_VIDEO_GATE == code || ALARM_CODE_VIDEO_GATE_FINISH == code)
        {
            pAlarmPush = &pAlarmCfg->aiAlarm.vgAlarm[0].alarmAction.alarm_push;
            p2p_enable = alarm_arming_with_muti_timespan_check(pAlarmPush, is_night);
        }
    }

    __INFO("alarm code:%d level:%d send p2p enable:%d\n", code, level, p2p_enable);
    if (p2p_enable)
    {
        anj_ser_alarm_handle(alarm_event.alarm_chn, alarm_event.alarm_code, alarm_event.alarm_level, alarm_event.alarm_payload);
    }

    anj_service_alarm_event_notify((void *)&alarm_event);

    return 0;
}

/*
    报警联动前已经判断过是否允许联动（enable && timespan），这里不再判断
    chn:报警通道 AI AO报警时是chn1~chn4，其他侦测类型是sensor通道
    code:报警类型
    flag:事件状态
    level:报警子类型
    newalarm:新的报警 要求如果报警一直持续，则不再推送新的p2p消息（哪怕超过2分钟）
    data:报警描述
    snapfile:抓拍路径
*/
int anj_alarm_event_handle(int chn, 
                                AjAlarmCode code, 
                                int flag, 
                                int level, 
                                int newalarm,
                                const char *data, 
                                const char *snapfile)
{
    if (code < 0 || code >= ALARM_CODE_END)
    {
        return -1;
    }

    int iRet = 0;
    int is_night = alarm_is_night_check();

    SYSTEM_TIME sys_time = {0};
    unsigned long long now_time = 0;
    SystemGetNowTime(&sys_time);
    now_time = anj_mw_get_cputime_ms(NULL);

    AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();
    OutPutAlarm *pOutputAlarm = &pAlarmCfg->normalAlarm.outputAlarm;

    __INFO("alarm link handle chn:%d code:%d, flag:%d, level:%d, is_new:%d, is_night:%d, data:%s\n", chn, code, flag, level, newalarm, is_night, data);

    switch (code)
    {
    case ALARM_CODE_USB_FREESPACE_LOW:
    case ALARM_CODE_SD0_FREESPACE_LOW:
    case ALARM_CODE_SD1_FREESPACE_LOW:
    {
        AlarmOutputAction *pAction = &pAlarmCfg->normalAlarm.storageFullAlarm.alarmAction.outputAction;
        alarm_link_alarm_output_action(code, is_night, pAction, pOutputAlarm);
    }
    break;
    case ALARM_CODE_VIDEO_LOST:
    {
        AlarmOutputAction *pAction = &pAlarmCfg->normalAlarm.videoLostAlarm[0].alarmAction.outputAction;
        alarm_link_alarm_output_action(code, is_night, pAction, pOutputAlarm);
    }
    break;
    case ALARM_CODE_VIDEO_COVERD:
    {
        VideoCoverAlarm *pVcAalrm = &pAlarmCfg->normalAlarm.videoCoverAlarm[0];
        alarm_video_cover_link_process(now_time, is_night, pVcAalrm, pOutputAlarm);
    }
    break;
    case ALARM_CODE_VIDEO_GATE:
    {
        VideoGateAlarm *pVgAlarm = &pAlarmCfg->aiAlarm.vgAlarm[0];
        alarm_video_gate_link_process(now_time, is_night, pVgAlarm, pOutputAlarm);
    }
    break;
    case ALARM_CODE_GPIO3_HIGH2LOW:
    {
        AlarmChannel *pChnCfg = &pAlarmCfg->normalAlarm.inputAlarm.alarmChannels[chn - 1];
        iRet = alarm_gpio3_high2low_link_process(now_time, is_night, pChnCfg);
        if (iRet == 1)
        {
            code = ALARM_CODE_IO_ALARM_FINISH;
        }
    }
    break;
    case ALARM_CODE_GPIO3_LOW2HIGH:
    {
        AlarmChannel *pChnCfg = &pAlarmCfg->normalAlarm.inputAlarm.alarmChannels[chn - 1];
        iRet = alarm_gpio3_low2high_link_process(now_time, is_night, pChnCfg);
        if (iRet == 1)
        {
            code = ALARM_CODE_IO_ALARM_FINISH;
        }
    }
    break;
    case ALARM_CODE_MOTION_DETECT:
    {
        MotionDetectAlarm *pMdAlarmCfg = &pAlarmCfg->normalAlarm.motionDetectAlarm[0];
        alarm_motion_detect_link_process(now_time, is_night, pMdAlarmCfg, pOutputAlarm);

        alarm_link_send_server_center_action(ALARM_CODE_MOTION_DETECT, sys_time);
        alarm_link_send_loopback_action(ALARM_CODE_MOTION_DETECT, now_time, sys_time);
    }
    break;

    case ALARM_CODE_VIDEO_AI:
    {
        switch (level)
        {
        case ALARM_AI_PD:                      // 1, //人形
        case ALARM_AI_VEHICLE_CAR:             // 2, //车形
        case ALARM_AI_VEHICLE_MOTO:            // 3, //摩托
        case ALARM_AI_VEHICLE_ELECTRICBICYCLE: // 4, //电单车
        case ALARM_AI_VEHICLE_BICYCLE:         // 5, //自行车
        case ALARM_AI_GASTANK:
        {
            alarm_link_send_server_center_action(ALARM_CODE_VIDEO_AI, sys_time);
            alarm_link_send_loopback_action(ALARM_CODE_VIDEO_AI, now_time, sys_time);

            PdAlarm *pAlarmConfig = &pAlarmCfg->aiAlarm.pdAlarm[0];
            alarm_ai_detect_link_process(now_time, is_night, level, pAlarmConfig, pOutputAlarm);
        }
        break;
        case ALARM_AI_LPR: // 6, //车牌
            break;
        case ALARM_AI_VIDEO_GATE: // 7,//越界(拌线)
        {
            VideoGateAlarm *pVgAlarm = &pAlarmCfg->aiAlarm.vgAlarm[0];
            alarm_video_gate_link_process(now_time, is_night, pVgAlarm, pOutputAlarm);
        }
        break;
        case ALARM_AI_FIRE: // 8,   //火焰
        {
            FlameAndFlumesAlarm *pFireAlarm = &pAlarmCfg->aiAlarm.fireAlarm;
            alarm_ai_fire_link_process(now_time, is_night, pFireAlarm, pOutputAlarm, data);
        }
        break;
        case ALARM_AI_FACEDETECT: // 9,//FACE DETECT
        {
            FaceDetectAlarm *pFaceAlarm = &pAlarmCfg->aiAlarm.fdAlarm[0];
            alarm_ai_face_link_process(now_time, is_night, pFaceAlarm, pOutputAlarm);
        }
        break;
        case ALARM_AI_VIDEO_REGION_DETECT_ENTER:
        case ALARM_AI_VIDEO_REGION_DETECT_LEAVE:
        case ALARM_AI_VIDEO_REGION_DETECT_STAY:
        {
            VideoRegionAiAlarm *pRegionAlarm = &pAlarmCfg->aiAlarm.regionAiAlarm[0];
            alarm_ai_region_link_process(now_time, is_night, pRegionAlarm, pOutputAlarm);
        }
        break;
        default:
            break;
        }
    }
    break;

    case ALARM_CODE_VIDEO_AI_FINISH:
    {
        switch (level)
        {
        case ALARM_AI_PD:                      // 1, //人形
        case ALARM_AI_VEHICLE_CAR:             // 2, //车形
        case ALARM_AI_VEHICLE_MOTO:            // 3, //摩托
        case ALARM_AI_VEHICLE_ELECTRICBICYCLE: // 4, //电单车
        case ALARM_AI_VEHICLE_BICYCLE:         // 5, //自行车
        {
            // 普通版本不需要做处理，定制处理等后续具体需求
            break;
        }

        default:
            break;
        }
    }
    break;

    case ALARM_CODE_IO_ALARM:
    {
        AlarmChannel *pChnAlarm = &pAlarmCfg->normalAlarm.inputAlarm.alarmChannels[chn - 1];
        alarm_alarm_in_out_link_process(now_time, is_night, pChnAlarm, pOutputAlarm);
        alarm_link_send_server_center_action(ALARM_CODE_IO_ALARM, sys_time);
    }
    break;

    case ALARM_CODE_LPR:
    {
        // 普通版本暂不支持LPR报警，定制处理等后续具体需求
    }

    break;
    case ALARM_CODE_BEGIN_REBOOT:
        // 普通版本不需要处理
        break;
    case ALARM_CODE_SENSOR:
    {
        // 暂时不需要处理sensor报警
        switch (level)
        {
        case ALARM_SENSOR_OFF:          // 无报警/报警消除
        case ALARM_SENSOR_ON:           // 常规报警
        case ALARM_SENSOR_POW_CUT:      // 停电报警
        case ALARM_SENSOR_OUT_OF_SCOPE: // 传感器超出范围报警
        case ALARM_SENSOR_FAULT:        // 设备报警
            break;
        default:
            break;
        }
    }
    break;
    case ALARM_CODE_PTZSTATUS:
    {
        // 暂时不需要处理，老架构在威特迪 af线程中调用
    }
    break;
    default:
        __ERR("alarm vaild code:%d\n", code);
        break;
    }

    alarm_link_notify_record_action(chn, code, level);

    alarm_event_data alarm_event = {0};
    alarm_event.year = sys_time.year;
    alarm_event.month = sys_time.month;
    alarm_event.day = sys_time.day;
    alarm_event.hour = sys_time.hour;
    alarm_event.minute = sys_time.minute;
    alarm_event.second = sys_time.second;
    alarm_event.alarm_code = code;
    alarm_event.alarm_flag = flag;
    alarm_event.alarm_level = level;
    alarm_event.alarm_chn = chn;

    if (data != NULL)
    {
        snprintf(alarm_event.alarm_payload, sizeof(alarm_event.alarm_payload), "%s", data);
    }
    if (snapfile != NULL)
    {
        snprintf(alarm_event.snap_path, sizeof(alarm_event.snap_path), "%s", snapfile);
    }

    alarm_link_send_service_action(alarm_event, now_time, is_night, newalarm, pAlarmCfg);

    return 0;
}
