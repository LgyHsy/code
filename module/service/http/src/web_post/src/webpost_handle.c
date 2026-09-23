#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>    /* need SIGPIPE */
#include <strings.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/wireless.h>

#include "anj_mw_comm.h"
#include "anj_mw_str.h"
#include "anj_mw_crypt.h"
#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "anj_mw_hwctrl.h"

#include "anj_record.h"
#include "anj_config.h"
#include "anj_audio.h"
#include "anj_net.h"
#include "anj_sysctl.h"
#include "function_list.h"
#include "anj_ser.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_sdcard.h"
#include "anj_systime.h"
#include "anj_sdcard.h"
#include "anj_record.h"
#include "record_log.h"
#include "anj_service.h"
#include "anj_service_provider.h"
#include "anj_search.h"
#include "anj_ispctl.h"
#include "anj_sysmng.h"
#include "anj_module.h"

#include "rec_mov_index.h"
#include "eventhub.h"
#include "user_auth.h"
#include "anj_net_provider.h"

#include "http_def.h"
#include "http_handle.h"
#include "http_upload.h"
#include "anj_ftpemail.h"

#include "des_ecb_crypt.h"


typedef struct 
{
    const char *pSoapMsg;           // msg
    const char *path;               // 路径
    char *pSoapBody;
    char *xmlBuf;
    unsigned int xmlbuf_len;
    char recvuserid[200];
    char recvpasswd[200];
    int bIsAdmin;                   // 是否是 admin账户
    int status;                     // handle status
    unsigned int Nowtime;
} WebPostContext;

typedef int (*WebPostHandler)(WebPostContext *ctx);

typedef struct 
{
    const char *path;
    WebPostHandler handler;
} WebPostHandlerEntry;

typedef struct
{
    int error_count;    // 错误次数
    unsigned int lock_start_time;   // 错误锁住时间
    unsigned int last_error_time;   // 上一次密码错误时间
}HttpPasswdErrInfo_t;
static HttpPasswdErrInfo_t s_stHttpPasswdErrInfo = {0};     // http密码错误


static FormDataBoundary s_FormFirmwareBoundary = {0};       // http升级表单信息


int s_mutex_record_query = 1;

char ThermalImagerDefault[] = {0x6e, 0x00, 0x00, 0x03, 0x00, 0x00, 0x86, 0xeb, 0x00, 0x00};
char ThermalImagerAdd[]     = {0x6e, 0x00, 0x00, 0x2f, 0x00, 0x02, 0x55, 0x0e, 0x00, 0x00, 0x00, 0x00};
char ThermalImagerSub[]     = {0x6e, 0x00, 0x00, 0x2f, 0x00, 0x02, 0x55, 0x0e, 0x00, 0x02, 0x20, 0x42};
char ThermalImagerMenuOk[]  = {0x6e, 0x00, 0x00, 0x2f, 0x00, 0x02, 0x55, 0x0e, 0x00, 0x01, 0x10, 0x21};
char ThermalImagerSave[]    = {0x6e, 0x00, 0x00, 0x01, 0x00, 0x00, 0xe8, 0x8b, 0x00, 0x00};
char ThermalImagerReboot[]  = {0x6e, 0x00, 0x00, 0x02, 0x00, 0x00, 0xb1, 0xdb, 0x00, 0x00};


FormDataBoundary *http_get_form_firmware_boundary()
{
    return &s_FormFirmwareBoundary;
}

int http_get_login_passwd_error_count()
{
    return s_stHttpPasswdErrInfo.error_count;
}

unsigned int http_get_login_passwd_error_locktime()
{
    return s_stHttpPasswdErrInfo.lock_start_time;
}


int xml_get_value_by_key(char *xml, char *key, char *value, int value_size)
{
    if ((xml == NULL) || (key == NULL) || (key == value))
        return -1;

    char *p = strstr(xml, key);
    if (p == NULL)
        return -1;

    p += strlen(key);
    int i = 0;
    for (i = 0; (i < value_size) && (p < (xml + strlen(xml))); p++)
    {
        if (*p == '\"')
            break;
        value[i++] = *p;
    }
    value[i] = '\0';
    value[value_size - 1] = '\0';

    return 0;
}

static int get_xml_value(char *mySOAPbody, char *value, char *mark)
{
    int mark_len = strlen(mark) + 2;

    char *mark_start = strstr(mySOAPbody, mark);
    if(mark_start != NULL)
    {
        char *mark_tmp = NULL;

        mark_start += mark_len;
        mark_tmp = mark_start;
        while(*mark_tmp != '"')
            mark_tmp++;


        size_t copy_len = strlen(mark_start) - strlen(mark_tmp);

        char *mark_buf = (char *)anj_mw_malloc(copy_len + 1);
        if(mark_buf)
        {
            memset(mark_buf, 0, copy_len + 1);

            snprintf(mark_buf, copy_len, "%s", mark_start);

            strcpy(value, mark_buf);

            anj_mw_free(mark_buf);
            mark_buf = NULL;
        }
        return 0;
    }

    return -1;
}

int delete_mount_path_file(char *filename)
{
    char cmd[100] = {0};
    snprintf(cmd, sizeof(cmd), "rm /mnt/nand/%s", filename);

    int iRet = anj_mw_system(cmd);
    return iRet;
}

int delete_log_file(char *filePath)
{
    if (filePath == NULL || strlen(filePath) == 0)
    {
        return -1;
    }

    char command[256] = {0};
    snprintf(command, sizeof(command), "rm -rf %s", filePath);
    
    if (system(command) != 0)
    {
        return -1;
    }

    if (strstr(filePath, ".mp4"))
    {
        snprintf(command, sizeof(command), "rm -rf %s.info", filePath);
        system(command);
    }

    return 0;
}

static int parse_userid_passwd_by_xml(const char *mySOAP, char *pRecvUserid, char *pRecvPasswd)
{
    int len = strlen(mySOAP);
    const char *p = mySOAP;

    int i = 0;
    int header_len = strlen("<soap:Header>");
    int param_len = 0;

    for(i = 0;i < len - header_len; i++, p++)
    {
        if (!strncmp(p, "<soap:Header>", header_len))
        {
            break;
        }
    }

    if(i == len - header_len)
    {
        return -1; /* HTTP not found */
    }

    p += header_len;
    int j = i;
    const char *p2 = p;

    header_len = strlen("</soap:Header>");

// <userid>
    param_len = strlen("<userid>");

    for(; i < len - param_len; i++, p++)
    {
        if(!strncmp(p, "</soap:Header>", header_len))
        {
            return -1; /* HTTP not found */
        }

        if (!strncmp(p, "<userid>", param_len))
        {
            break;
        }
    }

    if(i == len - param_len)
    {
        return -1; /* HTTP not found */
    }
    p += param_len;

// </userid>
    param_len = strlen("</userid>");
    for(;i < len - param_len; i++, p++, pRecvUserid++)
    {
        if (!strncmp(p, "</userid>", param_len))
        {
            break;
        }
        *pRecvUserid = *p;
    }

    if(i == len - param_len)
    {
        return -1; /* HTTP not found */
    }

    i = j;
    p = p2;

// <passwd>
    param_len = strlen("<passwd>");
    for(;i < len - param_len; i++, p++)
    {
        if(!strncmp(p, "</soap:Header>", header_len))
        {
            return -1; /* HTTP not found */
        }
        if (!strncmp(p, "<passwd>", param_len))
        {
            break;
        }
    }

    if(i == len - param_len)
    {
        return -1; /* HTTP not found */
    }
    p += param_len;

// </passwd>
    param_len = strlen("</passwd>");
    for(; i < len - param_len; i++, p++, pRecvPasswd++)
    {
        if (!strncmp(p, "</passwd>", param_len))
        {
            break;
        }
        *pRecvPasswd = *p;
    }

    if(i == len - param_len)
    {
        return -1; /* HTTP not found */
    }

    return 0;    
}

int parse_misc_config_language_by_xml(char *mySOAPbody, char *myLanguage)
{
    int len = strlen(mySOAPbody);
    char *p = mySOAPbody;
    int i;
    for (i = 0; i < len - 11; i++, p++)
    {
        if (!strncmp(p, "<MiscConfig", 11))
        {
            break;
        }

    }
    if (i == len - 11)
    {
        return -1; /* HTTP not found */
    }
    p += 11;

    for (; i < len - 10; i++, p++)
    {
        if (!strncmp(p, "Language=\"", 10))
        {
            break;
        }
    }
    if (i == len - 10)
    {
        return -1; /* HTTP not found */
    }
    p += 10;

    for (; i < len - 1; i++, p++, myLanguage++)
    {
        if (!strncmp(p, "\"", 1))
        {
            break;
        }
        *myLanguage = *p;
    }
    if (i == len - 1)
    {
        return -1; /* HTTP not found */
    }

    return 0;    
}

int parse_rtmp_config_by_xml(RtmpConfig *pRtmpConfig, char *mySOAPbody)
{
    if((pRtmpConfig == NULL) || (mySOAPbody == NULL))
        return -1;

    char str_enable[8] = {0};
    char str_streamno[8] = {0};
    char str_port[8] = {0};
    char str_type[8] = {0};

    xml_get_value_by_key(mySOAPbody, " Enable=\"", str_enable, sizeof(str_enable));
    xml_get_value_by_key(mySOAPbody, " Streamno=\"", str_streamno, sizeof(str_streamno));
    xml_get_value_by_key(mySOAPbody, " Port=\"", str_port, sizeof(str_port));
    xml_get_value_by_key(mySOAPbody, " Server=\"", pRtmpConfig->server, sizeof(pRtmpConfig->server));
    xml_get_value_by_key(mySOAPbody, " Appname=\"", pRtmpConfig->appname, sizeof(pRtmpConfig->appname));
    xml_get_value_by_key(mySOAPbody, " Streamid=\"", pRtmpConfig->streamid, sizeof(pRtmpConfig->streamid));
    xml_get_value_by_key(mySOAPbody, " Type=\"", str_type, sizeof(str_type));

    pRtmpConfig->enable = atoi(str_enable);
    pRtmpConfig->streamno = (short)atoi(str_streamno);
    pRtmpConfig->port = (short)atoi(str_port);
    pRtmpConfig->type = (short)atoi(str_type);

    if(pRtmpConfig->enable != 1)
        pRtmpConfig->enable = 0;
    if(pRtmpConfig->streamno < 0)
        pRtmpConfig->streamno = 1;
    if(pRtmpConfig->port <= 0)
        pRtmpConfig->port = 1935;

    return 0;    
}


int parse_record_query_condition_by_xml(char *soapBody, record_query_condition_s *pCondition, int *pSkipCount)
{
    char *p1 = NULL;
    char *p2 = NULL;
    char temp[32] = {0};

    p1 = strstr(soapBody, "record_mode=\"");
    if (p1 != NULL)
    {
        p1 += strlen("record_mode=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            pCondition->record_mode = atoi(temp);
        }
    }
    p1 = strstr(soapBody, "media_type=\"");
    if (p1 != NULL)
    {
        p1 += strlen("media_type=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            pCondition->media_type = atoi(temp);
        }
    }
    p1 = strstr(soapBody, "stream_index=\"");
    if (p1 != NULL)
    {
        p1 += strlen("stream_index=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            pCondition->stream_index = atoi(temp);
        }
    }
    p1 = strstr(soapBody, "min_size=\"");
    if (p1 != NULL)
    {
        p1 += strlen("min_size=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            pCondition->min_size = atoi(temp);
        }
    }
    p1 = strstr(soapBody, "max_size=\"");
    if (p1 != NULL)
    {
        p1 += strlen("max_size=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            pCondition->max_size = atoi(temp);
        }
    }
    p1 = strstr(soapBody, "skipCount=\"");
    if (p1 != NULL)
    {
        p1 += strlen("skipCount=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            *pSkipCount = atoi(temp);
        }
    }

    struct tm *start_time = &(pCondition->start_time);
    struct tm *end_time = &(pCondition->end_time);
    char timeBuf[8] = {0};

    p1 = strstr(soapBody, "start_time=\"");
    if (p1 != NULL)
    {
        p1 += strlen("start_time=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            p1 = temp;
            p2 = strchr(p1, '-');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            start_time->tm_year = atoi(timeBuf) - 1900;
            p1 = p2 + 1;
            p2 = strchr(p1, '-');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            start_time->tm_mon = atoi(timeBuf) - 1;
            p1 = p2 + 1;
            p2 = strchr(p1, ' ');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            start_time->tm_mday = atoi(timeBuf);
            p1 = p2 + 1;
            p2 = strchr(p1, ':');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            start_time->tm_hour = atoi(timeBuf);
            p1 = p2 + 1;
            p2 = strchr(p1, ':');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            start_time->tm_min = atoi(timeBuf);
            p1 = p2 + 1;
            memset(timeBuf, 0, sizeof(timeBuf));
            strcpy(timeBuf, p1);
            start_time->tm_sec = atoi(timeBuf);
        }
    }
    p1 = strstr(soapBody, "end_time=\"");
    if (p1 != NULL)
    {
        p1 += strlen("end_time=\"");
        p2 = strchr(p1, '"');
        if (p2 != NULL)
        {
            memset(temp, 0, sizeof(temp));
            strncpy(temp, p1, p2 - p1);
            p1 = temp;
            p2 = strchr(p1, '-');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            end_time->tm_year = atoi(timeBuf) - 1900;
            p1 = p2 + 1;
            p2 = strchr(p1, '-');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            end_time->tm_mon = atoi(timeBuf) - 1;
            p1 = p2 + 1;
            p2 = strchr(p1, ' ');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            end_time->tm_mday = atoi(timeBuf);
            p1 = p2 + 1;
            p2 = strchr(p1, ':');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            end_time->tm_hour = atoi(timeBuf);
            p1 = p2 + 1;
            p2 = strchr(p1, ':');
            memset(timeBuf, 0, sizeof(timeBuf));
            strncpy(timeBuf, p1, p2 - p1);
            end_time->tm_min = atoi(timeBuf);
            p1 = p2 + 1;
            memset(timeBuf, 0, sizeof(timeBuf));
            strcpy(timeBuf, p1);
            end_time->tm_sec = atoi(timeBuf);
        }
    }

    return 0;
}

int parse_account_info_by_xml(char *mySOAPbody, char *myusername, char *mypassword, char *mygroup, char *mystatus)
{
    int len = strlen(mySOAPbody);
    char *p = mySOAPbody;
    int i = 0;
    for (i = 0; i < len - 8; i++, p++)
    {
        if (!strncmp(p, "<Account", 8))
        {
            break;
        }

    }
    if (i == len - 8)
    {
        return -1; 
    }
    p += 8;
    int j = i;
    char *p2 = p;

// Username
    for (; i < len - 10; i++, p++)
    {
        if (!strncmp(p, "Username=\"", 10))
        {
            break;
        }
    }
    if (i == len - 10)
    {
        return -1;
    }
    p += 10;

    for (; i < len - 1; i++, p++, myusername++)
    {
        if (!strncmp(p, "\"", 1))
        {
            break;
        }
        *myusername = *p;
    }
    if (i == len - 1)
    {
        return -1;
    }

// Password
    i = j;
    p = p2;
    for (; i < len - 10; i++, p++)
    {
        if (!strncmp(p, "Password=\"", 10))
        {
            break;
        }
    }
    if (i == len - 10)
    {
        return -1;
    }
    p += 10;

    for (; i < len - 1; i++, p++, mypassword++)
    {
        if (!strncmp(p, "\"", 1))
        {
            break;
        }
        *mypassword = *p;
    }
    if (i == len - 1)
    {
        return -1; 
    }

// Group
    i = j;
    p = p2;
    for (; i < len - 7; i++, p++)
    {
        if (!strncmp(p, "Group=\"", 7))
        {
            break;
        }
    }
    if (i == len - 7)
    {
        return -1; 
    }
    p += 7;

    for (; i < len - 1; i++, p++, mygroup++)
    {
        if (!strncmp(p, "\"", 1))
        {
            break;
        }
        *mygroup = *p;
    }
    if (i == len - 1)
    {
        return -1;
    }

// Status
    i = j;
    p = p2;
    for (; i < len - 8; i++, p++)
    {
        if (!strncmp(p, "Status=\"", 8))
        {
            break;
        }
    }
    if (i == len - 8)
    {
        return -1; 
    }
    p += 8;

    for (; i < len - 1; i++, p++, mystatus++)
    {
        if (!strncmp(p, "\"", 1))
        {
            break;
        }
        *mystatus = *p;
    }

    if (i == len - 1)
    {
        return -1; 
    }

    return 0;    

}

int parse_system_maintain_by_xml(char *mySOAPbody, char *myOrder, char *myValue)
{
    int len = strlen(mySOAPbody);
    char *p = mySOAPbody;
    int i;
    for (i = 0; i < len - 16; i++, p++)
    {
        if (!strncmp(p, "<SystemMaintain>", 16))
        {
            break;
        }
    }

    if (i == len - 16)
    {
        return -1; 
    }
    p += 16;
    int j = i; 
    char *p2 = p;

// Order
    for (; i < len - 7; i++, p++)
    {
        if (!strncmp(p, "<Order>", 7))
        {
            break;
        }
    }
    if (i == len - 7)
    {
        return -1; 
    }
    p += 7;

    for (; i < len - 8; i++, p++, myOrder++)
    {
        if (!strncmp(p, "</Order>", 8))
        {
            break;
        }
        *myOrder = *p;
    }
    if (i == len - 8)
    {
        return -1; 
    }

// Value
    i = j;
    p = p2;
    for (; i < len - 7; i++, p++)
    {
        if (!strncmp(p, "<Value>", 7))
        {
            break;
        }
    }
    if (i == len - 7)
    {
        return -1; 
    }
    p += 7;

    for (; i < len - 8; i++, p++, myValue++)
    {
        if (!strncmp(p, "</Value>", 8))
        {
            break;
        }
        *myValue = *p;
    }
    if (i == len - 8)
    {
        return -1;
    }

    return 0;    
}

int parse_system_tm_by_xml(struct tm *time, char * mySOAPbody)
{
    int len = strlen(mySOAPbody);
    char *p = mySOAPbody;
    int i = 0;
    char sec[3] = {0};
    char min[3] = {0};
    char hour[3] = {0};
    char day[3] = {0};
    char mon[3] = {0};
    char year[5] = {0};

    for (i = 0; i < len - 11; i++, p++)
    {
        if (!strncmp(p, "<TimeConfig", 11))
        {
            break;
        }
    }

    if (i == len - 11)
    {
        return -1; 
    }
    p += 11;

    for (; i < len - 9; i++, p++)
    {
        if (!strncmp(p, "CurTime=\"", 9))
        {
            break;
        }
    }

    if (i == len - 9)
    {
        return -1; 
    }
    p += 9;

    if (!strncmp(p, "\"", 1))
    {
        return -1;
    }

    if (strlen(p) > 4)
    {
        strncpy(year, p, 4);
        p += 5;
    }
    if (strlen(p) > 2)
    {
        strncpy(mon, p, 2);
        p += 3;
    }
    if (strlen(p) > 2)
    {
        strncpy(day, p, 2);
        p += 3;
    }
    if (strlen(p) > 2)
    {
        strncpy(hour, p, 2);
        p += 3;
    }
    if (strlen(p) > 2)
    {
        strncpy(min, p, 2);
        p += 3;
    }
    if (strlen(p) > 2)
    {
        strncpy(sec, p, 2);
    }

    time->tm_sec = atoi(sec);
    time->tm_min = atoi(min);
    time->tm_hour = atoi(hour);
    time->tm_mday = atoi(day);
    time->tm_mon = atoi(mon) - 1; 
    time->tm_year = atoi(year) - 1900; 

    return 0;
}

int parse_coordinate_by_xml(char *mySOAPbody, int *StartX, int *StartY, int *EndX, int *EndY)
{
    char *tmp = NULL;

// StartX
    char *SX = strstr(mySOAPbody, "StartX");
    SX += 8;
    tmp = SX;
    while (*tmp != '"')
        tmp++;

    size_t buf_len = strlen(SX) - strlen(tmp);
    char *num1_buf = anj_mw_malloc(buf_len + 1);
    memset(num1_buf, 0, buf_len + 1);
    StrCpy(num1_buf, buf_len, SX);
    *StartX = atoi(num1_buf);

// StartY
    char *SY = strstr(mySOAPbody, "StartY");
    SY += 8;
    tmp = SY;
    while (*tmp != '"')
        tmp++;

    buf_len = strlen(SY) - strlen(tmp);
    char *num2_buf = anj_mw_malloc(buf_len + 1);
    memset(num2_buf, 0, buf_len + 1);
    StrCpy(num2_buf, buf_len, SY);
    *StartY = atoi(num2_buf);

// EndX
    char *EX = strstr(mySOAPbody, "EndX");
    EX += 6;
    tmp = EX;
    while (*tmp != '"')
        tmp++;

    buf_len = strlen(EX) - strlen(tmp);
    char *num3_buf = anj_mw_malloc(buf_len + 1);
    memset(num3_buf, 0, buf_len + 1);
    StrCpy(num3_buf, buf_len, EX);
    *EndX = atoi(num3_buf);

// EndY
    char *EY = strstr(mySOAPbody, "EndY");
    EY += 6;
    tmp = EY;
    while (*tmp != '"')
        tmp++;

    buf_len = strlen(EY) - strlen(tmp);
    char *num4_buf = anj_mw_malloc(buf_len + 1);
    memset(num4_buf, 0, buf_len + 1);
    StrCpy(num4_buf, buf_len, EY);
    *EndY = atoi(num4_buf);

    anj_mw_free(num1_buf);
    anj_mw_free(num2_buf);
    anj_mw_free(num3_buf);
    anj_mw_free(num4_buf);

    return 0;
}

char *xml_conver_system_version(SYSTEM_VERSION_DATA *pSystemVersion, char *mySN)
{
    char *SystemVersionInfo = NULL;
    unsigned int buf_len = 1000;
    SystemVersionInfo = (char *)anj_mw_malloc(buf_len);
    if(SystemVersionInfo == NULL)
    {
        __ERR("SystemVersionInfo malloc error!\n");
        return NULL;
    }

    memset(SystemVersionInfo, 0, buf_len);

    strcat(SystemVersionInfo, "<SystemVersionInfo>\n");
    strcat(SystemVersionInfo, "<VersionInfo \n");
    strcat(SystemVersionInfo, "kernelVersion=\"");
    strcat(SystemVersionInfo, pSystemVersion->kernelVersion);
    strcat(SystemVersionInfo, "\"\n");
    strcat(SystemVersionInfo, "fsVersion=\"");
    strcat(SystemVersionInfo, pSystemVersion->fsVersion);
    strcat(SystemVersionInfo, "\"\n");
    strcat(SystemVersionInfo, "/>\n");
    strcat(SystemVersionInfo, "<SerialNumber \n");
    strcat(SystemVersionInfo, "serialNumber=\"");
    strcat(SystemVersionInfo, mySN);
    strcat(SystemVersionInfo, "\"\n");
    strcat(SystemVersionInfo, "/>\n");
    strcat(SystemVersionInfo, "</SystemVersionInfo>\n");

    return SystemVersionInfo;
}

char *xml_conver_record_query_info(record_query_result_s *pResult)
{
    int count = 0;
    char *pRecordQueryInfo = NULL;
    char *pbuf = NULL;

    int i = 0;

    if (pResult == NULL)
    {
        return NULL;
    }

    count = pResult->count;
    unsigned int infoSize = (count * 256 + 50) * sizeof(char);
    pRecordQueryInfo = (char *)anj_mw_malloc(infoSize);
    if (pRecordQueryInfo == NULL)
    {
        __ERR("RecordQueryInfo malloc error!\n");
        return NULL;
    }
    
    memset(pRecordQueryInfo, 0, infoSize);
    pbuf = pRecordQueryInfo;

    snprintf(pRecordQueryInfo, infoSize, "<RecordQueryInfo>\r\n");
    pRecordQueryInfo += strlen(pRecordQueryInfo);

    for (i = 0; (i < count) && ((int)strlen(pRecordQueryInfo) < (infoSize - 218 - 18)); i++)
    {
        struct tm *start_time = NULL;
        start_time = &(pResult->items[i].start_time);
        
        snprintf(pRecordQueryInfo, infoSize, 
            "<items filepath=\"%s\" filesize=\"%lu\" record_mode=\"%d\" media_type=\"%d\" stream_index=\"%d\" "
            "start_time=\"%04d-%02d-%02d %02d:%02d:%02d\" />\r\n",
            pResult->items[i].filepath, 
            pResult->items[i].filesize,
            pResult->items[i].record_mode, 
            pResult->items[i].media_type, 
            pResult->items[i].stream_index,
            start_time->tm_year + 1900, 
            start_time->tm_mon + 1, 
            start_time->tm_mday,
            start_time->tm_hour, 
            start_time->tm_min, 
            start_time->tm_sec);
        
        pRecordQueryInfo += strlen(pRecordQueryInfo);
    }

    snprintf(pRecordQueryInfo, infoSize, "</RecordQueryInfo>");
    return pbuf;
}

char *xml_conver_normal_file_list(file_query_result *pResult, int totalcount)
{
    if (pResult == NULL)
        return NULL;

    unsigned int buf_len = 1024;
    char *FilesList = (char *)anj_mw_malloc(buf_len);
    if (FilesList == NULL)
    {
        __ERR("FilesList malloc error!\n");
        return NULL;
    }
    memset(FilesList, 0, buf_len);

    strcat(FilesList, "<FilesList>");
    int pos = 0;
    for (pos = 0; pos < pResult->count; pos++)
    {
        strcat(FilesList, "<File path=\"");
        strcat(FilesList, pResult->file_info[pos].filepath);
        strcat(FilesList, "\" />");
    }
    strcat(FilesList, "</FilesList>");

    return FilesList;
}

char *xml_conver_wifi_network_status(NETWORK_STATUS_DATA *networkStatus)
{
    unsigned int buf_len = 512;
    char *XMLnetworkStatus = anj_mw_malloc(buf_len);
    if(XMLnetworkStatus == NULL)
    {
        __ERR("XMLnetworkStatus malloc error!\n");
        return NULL;
    }

    memset(XMLnetworkStatus, 0, buf_len);

    strcat(XMLnetworkStatus, "<WiFiNetworkStatus>\n");
    strcat(XMLnetworkStatus, "<WIFI_NETWORK \n");
    strcat(XMLnetworkStatus, "wirelessMacAddress=\"");
    strcat(XMLnetworkStatus, networkStatus->wirelessMac);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "wirelessGateway=\"");
    strcat(XMLnetworkStatus, networkStatus->wirelessGateway);
    strcat(XMLnetworkStatus, "\"\n");    
    strcat(XMLnetworkStatus, "wirelessNetmask=\"");
    strcat(XMLnetworkStatus, networkStatus->wirelessNetmask);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "wirelessIp=\"");
    strcat(XMLnetworkStatus, networkStatus->wirelessIp);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "essid=\"");
    strcat(XMLnetworkStatus, networkStatus->essid);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "freq=\"");
    strcat(XMLnetworkStatus, networkStatus->freq);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "bitRate=\"");
    strcat(XMLnetworkStatus, networkStatus->bitRate);
    strcat(XMLnetworkStatus, "\"\n");

    char str[10] = {0};
    strcat(XMLnetworkStatus, "linkquality=\"");
    snprintf(str, sizeof(str), "%d", networkStatus->linkquality);
    strcat(XMLnetworkStatus, str);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "signallevel=\"");

    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d", networkStatus->signallevel);
    strcat(XMLnetworkStatus, str);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "/>\n");
    strcat(XMLnetworkStatus, "</WiFiNetworkStatus>\n");

    return XMLnetworkStatus;
}

char *xml_conver_network_status(NETWORK_STATUS_DATA *networkStatus)
{
    unsigned int buf_len = 1000;
    char *XMLnetworkStatus = anj_mw_malloc(buf_len);
    if(XMLnetworkStatus == NULL)
    {
        __ERR("XMLnetworkStatus malloc error!\n");
        return NULL;
    }

    bzero(XMLnetworkStatus, buf_len);

    strcat(XMLnetworkStatus, "<NetworkStatus>\n");
    strcat(XMLnetworkStatus, "<WIRE_NETWORK \n");
    strcat(XMLnetworkStatus, "MacAddress=\"");
    strcat(XMLnetworkStatus, networkStatus->wireMac);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "IPType=\"");
    strcat(XMLnetworkStatus, networkStatus->ipType);
    strcat(XMLnetworkStatus, "\"\n");    
    strcat(XMLnetworkStatus, "IPAddress=\"");
    strcat(XMLnetworkStatus, networkStatus->ip);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "Netmask=\"");
    strcat(XMLnetworkStatus, networkStatus->netmask);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "Gateway=\"");
    strcat(XMLnetworkStatus, networkStatus->gateway);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "DNS1=\"");
    strcat(XMLnetworkStatus, networkStatus->dns1);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "DNS2=\"");
    strcat(XMLnetworkStatus, networkStatus->dns2);
    strcat(XMLnetworkStatus, "\"\n");    
    strcat(XMLnetworkStatus, "/>\n");
    strcat(XMLnetworkStatus, "<CLOUD \n");
    strcat(XMLnetworkStatus, "Enable=\"");

    char str[10] = {0};
    snprintf(str, sizeof(str), "%d", networkStatus->cloudEnable);
    strcat(XMLnetworkStatus, str);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "Type=\"");

    memset(str, 0, sizeof(str));
    snprintf(str, sizeof(str), "%d", networkStatus->cloudType);
    strcat(XMLnetworkStatus, str);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "LoginStatus=\"");

    snprintf(str, sizeof(str), "%d", networkStatus->cloudLogined);
    strcat(XMLnetworkStatus, str);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "ID=\"");
    strcat(XMLnetworkStatus, networkStatus->cloudId);
    strcat(XMLnetworkStatus, "\"\n");
    strcat(XMLnetworkStatus, "/>\n");
    strcat(XMLnetworkStatus, "</NetworkStatus>\n");

    return XMLnetworkStatus;
}

char *xml_conver_config_file(char *myfilename)
{
    unsigned int len = 20 * 1024;
    char *fileContent = (char *)anj_mw_malloc(len);
    if(fileContent == NULL)
    {
        __ERR("fileContent malloc error!\n");
        return NULL;
    }

    memset(fileContent, 0, len);

    char myConfigfile[100] = {0};
    strcat(myConfigfile, "/mnt/nand/");
    strcat(myConfigfile, myfilename);

    int fd = open(myConfigfile,O_RDONLY);
    int iRet = read(fd, fileContent, len);
    if(iRet <= 0)
    {
        __ERR("read config file:%s failed\n", myConfigfile);
        close(fd);
        return NULL;
    }
    __INFO("read config file:%s lenth:%d\n", myConfigfile, iRet);

    close(fd);
    return fileContent;
}

char *xml_cover_preset_list(char *retBuffer)
{
    unsigned int len = 1000;
    char *xmlBuf = (char *)anj_mw_malloc(len);
    if (xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc error!\n");
        return NULL;
    }

    memset(xmlBuf, 0, len);

    char buf[5] = {0};
    int i = 0;
    int j = 0;
    for (i = 0; i < strlen(retBuffer); i++, retBuffer++)
    {
        if (*retBuffer == '^')
        {
            retBuffer++;
            break;
        }
        buf[i] = *retBuffer;
    }

    int num = atoi(buf);
    strcat(xmlBuf, "<GetPresetList>");
    for (i = 0; i < num; i++)
    {
        bzero(buf, sizeof(buf));
        for (j = 0; j < strlen(retBuffer); j++, retBuffer++)
        {
            if (*retBuffer == '^')
            {
                retBuffer++;
                break;
            }
            buf[j] = *retBuffer;
        }

        strcat(xmlBuf, "<PresetList \n");
        strcat(xmlBuf, "Preset=\"");
        strcat(xmlBuf, buf);
        strcat(xmlBuf, "\"\n");
        strcat(xmlBuf, "/>\n");
    }
    strcat(xmlBuf, "</GetPresetList>");
    return xmlBuf;
}

char *xml_conver_log_file_list()
{
    FILE *stream = popen( "ls -hl /mnt/nand/", "r" );

    char *lsBuf = (char *)anj_mw_malloc(1024 * sizeof(char));
    if(lsBuf == NULL)
    {
        __ERR("lsBuf malloc error!\n");
        pclose(stream);
        return NULL;
    }

    memset(lsBuf, 0, 1024);

    char *logFileListBuf = (char *)anj_mw_malloc(1024* sizeof(char));
    if(logFileListBuf == NULL)
    {
        __ERR("logFileListBuf malloc error!\n");
        pclose(stream);
        return NULL;
    }

    memset(logFileListBuf, 0, 1024);

    anj_mw_fread(stream, lsBuf, 1024);

    char *p = lsBuf;
    int bufSize = strlen(lsBuf);

    strcat(logFileListBuf, "<LogFileList>\n");

    int i = 0;
    char filesize[20] = {0};
    char filename[100] = {0};

    while((p - lsBuf) <= (bufSize - 1))
    {
        if (!strncmp(p, ".log", 4))
        {
            for(i = 0; i < 4; i++)
            {
                while(*(p - 1) != ' ')
                {
                    p--;
                }
                while(*(p - 1) == ' ')
                {
                    p--;
                }
            }

            while(*(p - 1) != ' ')
            {
                p--;
            }

            memset(filesize, 0, sizeof(filesize));
            for(i = 0; i < 20; i++, p++)
            {
                if(*p == ' ')
                {
                    break;
                }
                filesize[i] = *p;
            }

            for(i = 0; i < 3; i++)
            {
                while(*(p) == ' ')
                {
                    p++;
                }
                while(*(p) != ' ')
                {
                    p++;
                }
            }

            while(*(p) == ' ')
            {
                p++;
            }

            memset(filename, 0, sizeof(filename));
            for(i = 0; i < 100; i++, p++)
            {
                if(*p == '\n')
                {
                    break;
                }
                filename[i] = *p;
            }

            strcat(logFileListBuf, "<LogFile\n");
            strcat(logFileListBuf, "FileName=\"");
            strcat(logFileListBuf, filename);
            strcat(logFileListBuf, "\"\n");
            strcat(logFileListBuf, "FileSize=\"");
            strcat(logFileListBuf, filesize);
            strcat(logFileListBuf, "\"\n");
            strcat(logFileListBuf, "/>\n");
        }
        else
        {
            p++;
        }
    }

    strcat(logFileListBuf, "</LogFileList>");
    anj_mw_free(lsBuf);
    lsBuf = NULL;

    pclose(stream);
    return logFileListBuf;
}

char *xml_cover_system_control_string(char *config_str, char *sessionid)
{
    unsigned int xmlbuf_len = 5000;
    char *xmlBuf = anj_mw_malloc(xmlbuf_len);
    if(xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc error!\n");
        return NULL;
    }
    memset(xmlBuf, 0, xmlbuf_len);

    char buf[50] = {0};
    int i = 0;
    int out = 0;
    char *p = config_str;

    strcat(xmlBuf, "<SystemControl> \n");
    strcat(xmlBuf, "<firmwareVersion\nfirmwareVersion=\"common\"\n/> \n");

    while(*p != '\0')
    {
        while(1)
        {
            if(*p == '\0')
            {
                out = 1;
                break;
            }
            else if(*p == '+')
            {
                break;
            }
            p++;
        }

        if(out == 1)
        {
            break;
        }
        p++;

        while(*p == ' ')
        {
            p++;
        }

        memset(buf, 0, sizeof(buf));
        for(i = 0; (*p != ' ') && (*p != '+') && (*p != '\0'); i++, p++)
        {
            buf[i] = *p;
        }
        strcat(xmlBuf, "<ControlString \n");
        strcat(xmlBuf, "ControlString=\"");
        strcat(xmlBuf, buf);
        strcat(xmlBuf, "\" \n");
        strcat(xmlBuf, "/> \n");
    }

    strcat(xmlBuf, "<sessionid \n");
    strcat(xmlBuf, "sessionid=\"");
    strcat(xmlBuf, sessionid);
    strcat(xmlBuf, "\" \n");
    strcat(xmlBuf, "/> \n");
    strcat(xmlBuf, "</SystemControl> \n");

    return xmlBuf;
}

char *xml_conver_wifi_connect(int wifistatus)
{
	int maxSize = 1000;
	char *pe = NULL;
	char *pb = NULL;
	char *buf = NULL;

	buf = (char*)anj_mw_malloc(maxSize);
    if(buf == NULL)
    {
        __ERR("xmlBuf malloc error!\n");
        return NULL;
    }

	pb = buf;
	pe = buf + maxSize -1;

	pb += snprintf(pb, pe-pb, "<WifiConnected\r\n");
	pb += snprintf(pb, pe-pb, "status=\"%d\"\r\n", wifistatus);
	pb += snprintf(pb, pe-pb, "/>\r\n");

	return buf;	
}

char *xml_cover_system_function_list(char *config_str, char *sessionid)
{
    unsigned int xmlbuf_len = 4 * 1024;
    char *xmlBuf = anj_mw_malloc(xmlbuf_len);
    if(xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc error!\n");
        return NULL;
    }
    memset(xmlBuf, 0, xmlbuf_len);

    char *p = config_str;
    int i = 0;
    int out = 0;
    char buf[50] = {0};
    strcat(xmlBuf, "<SystemFunction> \n");

    while(*p != '\0')
    {
        while(1)
        {
            if(*p == '\0')
            {
                out = 1;
                break;
            }
            else if(*p == '+')
            {
                break;
            }
            p++;
        }

        if(out == 1)
        {
            break;
        }

        p++;
        while(*p == ' ')
        {
            p++;
        }

        bzero(buf,50);
        for(i = 0; (*p != ' ') && (*p != '+') && (*p != '\0'); i++, p++)
        {
            buf[i] = *p;
        }
        strcat(xmlBuf, "<Fun ");
        strcat(xmlBuf, "Fun=\"");
        strcat(xmlBuf, buf);
        strcat(xmlBuf, "\"/> ");
    }
    strcat(xmlBuf, "<sessionid \n");
    strcat(xmlBuf, "sessionid=\"");
    strcat(xmlBuf, sessionid);
    strcat(xmlBuf, "\" \n");
    strcat(xmlBuf, "/> \n");
    strcat(xmlBuf, "</SystemFunction> \n");

    return xmlBuf;
}

int xml_conver_audio_code_list(char *xmlBuf, AUDIO_CODEC_ENTRY *audio_entry, int arescount)
{
    char *p = strstr(xmlBuf, "</Audio>");

    if(p == NULL)
        return -1;

    *p = '\0';
    int i = 0;
    char buf[100] = {0};

    for(i = 0;i < arescount; i++)
    {
        snprintf(buf, sizeof(buf), "<CodeList\nEncodeType=\"%s\"\n", audio_entry[i].codec_name);
        strcat(xmlBuf, buf);
        snprintf(buf, sizeof(buf), "Channels=\"%d\"\n", audio_entry[i].channels);
        strcat(xmlBuf, buf);
        snprintf(buf, sizeof(buf), "BitSpersample=\"%d\"\n", audio_entry[i].bitspersample);
        strcat(xmlBuf, buf);
        snprintf(buf, sizeof(buf), "SampleRate=\"%d\"\n", audio_entry[i].samplerate);
        strcat(xmlBuf, buf);
        snprintf(buf, sizeof(buf), "BitRate=\"%d\"\n", audio_entry[i].bitrate);
        strcat(xmlBuf, buf);
        snprintf(buf, sizeof(buf), "DefConfig=\"%d\"\n", audio_entry[i].def_config);
        strcat(xmlBuf, buf);
        strcat(xmlBuf, "/>\n");
    }

    strcat(xmlBuf, "</Audio>\n");
    return 0;
}

int xml_conver_video_code_list(char *xmlBuf, RESOLUTION_ENTRY *pVideoEntry, int vrescount)
{
    char *p = strstr(xmlBuf, "</Video>");
    if(p == NULL)
        return -1;

    *p = '\0';
    int i = 0;
    char buf[100] = {0};

    for(i = 0; i < vrescount; i++)
    {
        snprintf(buf, sizeof(buf), "<CodeList\nResolution=\"%s\"\n", pVideoEntry[i].res_name);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "EncodeFormat=\"%s\"\n", pVideoEntry[i].codec_name);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "Stream=\"%d\"\n", pVideoEntry[i].stream_type);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "BitRate=\"%d\"\n", pVideoEntry[i].def_bitrate);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "MinBitRate=\"%d\"\n", pVideoEntry[i].min_bitrate);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "MaxBitRate=\"%d\"\n", pVideoEntry[i].max_bitrate);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "FrameRate=\"%d\"\n", pVideoEntry[i].def_framerate);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "MinFrameRate=\"%d\"\n", pVideoEntry[i].min_framerate);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "MaxFrameRate=\"%d\"\n", pVideoEntry[i].max_display_framerate);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "DualStream=\"%d\"\n", pVideoEntry[i].dual_stream);
        strcat(xmlBuf, buf);

        snprintf(buf, sizeof(buf), "DefConfig=\"%d\"\n", pVideoEntry[i].def_config);
        strcat(xmlBuf, buf);
        strcat(xmlBuf, "/>\n");
    }

    strcat(xmlBuf, "</Video>\n");
    return 0;
}

int decode_des(char *str, int len)
{
    char *strBuf = (char *)anj_mw_malloc(len);
    if (strBuf == NULL)
    {
        __ERR("strBuf malloc failed\n");
        return -1;
    }

    memset(strBuf, 0, len);

    const char *pKey = "WebLogin";
    int outlen = len;
    int ret = des_ecb_decrypt(str, (const uint8_t *)pKey, (int)strlen(pKey),
                              (uint8_t *)strBuf, &outlen);
    if (ret != DES_OK)
    {
        __ERR("des_ecb_decrypt failed, ret:%d\n", ret);
        anj_mw_free(strBuf);
        return -1;
    }

    memset(str, 0, len);
    strncpy(str, strBuf, len - 1);

    anj_mw_free(strBuf);
    strBuf = NULL;
    return 0;
}


int check_user_passwd(const char *username, const char *passwd, char *devUsergroup)
{
    if(NULL == passwd || strlen(passwd) == 0)
        return -1;

    unsigned int tNow = GetCurrentTimeStamp();
    unsigned int tLastGetTime = 0xffffffff;
    static UserConfig s_stUsrCfg = {0};

    if(0xffffffff == tLastGetTime || tNow - tLastGetTime > 10000)
    {
        SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
        memcpy(&s_stUsrCfg, &pstSystemCfg->userCfg, sizeof(UserConfig));
        tLastGetTime = tNow;
    }

    int iIndex = 0; 
    int bFound = 0;
    for( iIndex = 0; iIndex < MAX_ACCOUNT_COUNT && iIndex < s_stUsrCfg.count; iIndex++)
    {
        if( strcasecmp(s_stUsrCfg.accounts[iIndex].status, "Enable") != 0)
            continue;

        if( strcmp(username, s_stUsrCfg.accounts[iIndex].userName) != 0)
            continue;

        if( strcmp(passwd, s_stUsrCfg.accounts[iIndex].password) != 0)
            continue;

        strcpy(devUsergroup, s_stUsrCfg.accounts[iIndex].group.groupName);

        bFound = 1;
        break;
    }

    if(bFound == 0)
    {
        __ERR("username/passwd(%s/%s) verify failed.\n", username, passwd);
        return -1;
    }

    return 0;    
}


//调用者先获取长度，分配内存后，再次调用获取数据
int get_soap_body(const char *pSoapMsg, char *pBodyOutput, int max_size)
{
    if(NULL == pSoapMsg)
        return -1;

    const char *pIdentityBegin = "<soap:Body>";
    const char *pIdentityEnd = "</soap:Body>";
    const char *pBegin = NULL;
    const char *pEnd = NULL;    
    pBegin = strstr(pSoapMsg, pIdentityBegin);
    if( NULL == pBegin)
        return -1;

    pBegin += strlen(pIdentityBegin);
    pEnd = strstr(pSoapMsg, pIdentityEnd);
    if( NULL == pEnd)
        return -1;

    int len = pEnd - pBegin;
    if( NULL == pBodyOutput)
        return len;

    if(len <= 0 )
    {
        ;
    }
    else if( len < max_size )
    {
        memcpy(pBodyOutput, pBegin, len);
    }
    else
    {
        memcpy(pBodyOutput, pBegin, max_size);
    }

    return len;
}

int wifi_is_connected(const char *interface, const char *ssid)
{
    int sockfd = 0;
    int isConnected = 0;

    struct iwreq wreq;
    memset(&wreq, 0, sizeof(struct iwreq));
    sprintf(wreq.ifr_name, interface);

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
    {
        return 0;
    }

    char essid[64] = {0};
    wreq.u.essid.pointer = essid;
    wreq.u.essid.length = sizeof(essid);

    if (ioctl(sockfd, SIOCGIWESSID, &wreq) != -1) 
    {
        if( strlen(essid) > 0)
        {
            struct NET_CONFIG netcfg = {0};
            int iRet = net_get_info(net_get_wireless_name(), &netcfg);
            if(iRet == 0)
            {
                if ( netcfg.ifaddr != 0 )
                {
                    if( ssid == NULL || strlen(ssid) == 0)
                    {
                        isConnected = 1;
                        __INFO("Device had connect to %s\n", essid);
                    }
                    else
                    {
                        if( strcasecmp(essid, ssid) == 0)
                        {
                            isConnected = 1;
                            __INFO("Device had connect to %s\n", essid);
                        }
                    }
                }
            }

        }
        else
        {
            __ERR("ssid not match or not found\n");
        }
    }

    close(sockfd);

    return isConnected;
}


void network_lan_default_config_get(LANConfig *pLanCfg)
{
    int iRet = 0;
    GlobalConfig stGlobalCfg = {0};
    NetworkConfigNew *pOrgNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();

    char file[128] = {0};
    iRet = anj_config_get_default(file, sizeof(file), &stGlobalCfg);
    if (iRet == 0)
    {
        memcpy(pLanCfg, &stGlobalCfg.networkCfgNew.lanCfg, sizeof(LANConfig));
    }
    else
    {
        LANConfig stLanCfg = {0};
        memcpy(&stLanCfg, &pOrgNetworkCfg->lanCfg, sizeof(LANConfig));
        strcpy(stLanCfg.IPAddress, "192.168.0.123");

        memcpy(pLanCfg, &stLanCfg, sizeof(LANConfig));
    }
}

/*
 * 将全局能力串拷入 buf；若含 double_led_panel，则向 buf 追加
 * +white_led_panel / +ir_led_panel（已存在则跳过）。
 * 不修改 g_system_control_string。
 */
static void web_expand_led_panel_capability(char *buf, unsigned int buflen)
{
    const char *src = NULL;
    size_t used = 0;
    size_t need = 0;

    if (buf == NULL || buflen == 0)
    {
        return;
    }

    src = anj_sysctl_get_capability_string();
    if (src == NULL)
    {
        src = "";
    }
    __WARN("capability_string:%s\n", src);

    snprintf(buf, buflen, "%s", src);

    if (strstr(buf, FUNCTION_LEDPANEL_DOUBLE) == NULL)
    {
        return;
    }

    if (strstr(buf, FUNCTION_LEDPANEL_WHITE) == NULL)
    {
        used = strlen(buf);
        need = 1 + strlen(FUNCTION_LEDPANEL_WHITE) + 1;
        if (used + need <= buflen)
        {
            snprintf(buf + used, buflen - used, "+%s", FUNCTION_LEDPANEL_WHITE);
        }
        else
        {
            __WARN("web expand led capability: no space for white_led_panel\n");
        }
    }

    if (strstr(buf, FUNCTION_LEDPANEL_IR) == NULL)
    {
        used = strlen(buf);
        need = 1 + strlen(FUNCTION_LEDPANEL_IR) + 1;
        if (used + need <= buflen)
        {
            snprintf(buf + used, buflen - used, "+%s", FUNCTION_LEDPANEL_IR);
        }
        else
        {
            __WARN("web expand led capability: no space for ir_led_panel\n");
        }
    }
}

static int web_handle_login(WebPostContext *pstCtx)
{
    char sessionid[32] = {0};
    char userGroup[20] = {0};

    if (UserAuthLogin(pstCtx->recvuserid, pstCtx->recvpasswd, userGroup, sessionid) != 0)
    {
        __ERR("User auth login error!\n");
        pstCtx->status = 403;
        return -1;
    }

    __INFO("web login sessionid:%s, userGroup:%s\n", sessionid, userGroup);

    unsigned int capability_len = (MAX_SYSTEM_CONTROL_STRING_LEN + 256);
    char *capability_str = (char *)anj_mw_malloc(capability_len);
    if (capability_str == NULL)
    {
        __ERR("capability_str malloc failed\n");
        pstCtx->status = 403;
        return -1;
    }

    memset(capability_str, 0, capability_len);
    web_expand_led_panel_capability(capability_str, capability_len);
    char *xmlBuf = xml_cover_system_control_string(capability_str, sessionid);

    anj_mw_free(capability_str);
    capability_str = NULL;

    if (xmlBuf == NULL)
    {
        pstCtx->status = 403;
        return -1;
    }
    else
    {
        pstCtx->xmlBuf = xmlBuf;
    }

    return 0;
}

static int web_handle_ipc_login(WebPostContext *pstCtx)
{
    char sessionid[32] = {0};
    char userGroup[20] = {0};

    if (UserAuthLogin(pstCtx->recvuserid, pstCtx->recvpasswd, userGroup, sessionid) != 0)
    {
        __ERR("User auth login error!\n");
        pstCtx->status = 403;
        return -1;
    }

    __INFO("web ipc login sessionid:%s, userGroup:%s\n", sessionid, userGroup);

    unsigned int capability_len = (MAX_SYSTEM_CONTROL_STRING_LEN + 256);
    char *capability_str = (char *)anj_mw_malloc(capability_len);
    if (capability_str == NULL)
    {
        __ERR("capability_str malloc failed\n");
        pstCtx->status = 403;
        return -1;
    }

    memset(capability_str, 0, capability_len);
    web_expand_led_panel_capability(capability_str, capability_len);

    char *xmlBuf = xml_cover_system_function_list(capability_str, sessionid);

    anj_mw_free(capability_str);
    capability_str = NULL;

    if (xmlBuf == NULL)
    {
        pstCtx->status = 403;
        return -1;
    }
    else
    {
        pstCtx->xmlBuf = xmlBuf;
    }

    return 0;
}

static int web_handle_get_sys_boot_status(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 8;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);
    strcpy(pstCtx->xmlBuf, "ON");
    return 0;
}

static int web_handle_get_ipeye_cap(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 8;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);

    if (anj_mw_file_exists("/opt/ch/ZXST_plug"))
        strcpy(pstCtx->xmlBuf, "ON");
    else
        strcpy(pstCtx->xmlBuf, "OFF");
    return 0;
}

static int web_handle_get_ipeye_status(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 8;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);
    if (anj_mw_file_exists("/tmp/IPEYE_UP"))
        strcpy(pstCtx->xmlBuf, "ON");
    else
        strcpy(pstCtx->xmlBuf, "OFF");
    return 0;
}

static int web_handle_get_all_config(WebPostContext *pstCtx)
{
    if(pstCtx->bIsAdmin == 0)
    {
        __ERR("userid:%s is not admin!\n", pstCtx->recvuserid);
        pstCtx->status = 400;
        return -1;
    }

    //读取文件测试 /opt/ch/config.file
    char szDefaultConfigFileName[64] = {0};        
    snprintf(szDefaultConfigFileName, sizeof(szDefaultConfigFileName), CONFIG_FILE_PATH);
    
    if (access(szDefaultConfigFileName, F_OK) != 0)
    {
        memset(szDefaultConfigFileName, 0, sizeof(szDefaultConfigFileName));
        snprintf(szDefaultConfigFileName, sizeof(szDefaultConfigFileName),
            "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, "config.default.xml");
    }
    
    int file_size = get_file_size(szDefaultConfigFileName);

    pstCtx->xmlbuf_len = file_size + 1;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);

    if (pstCtx->xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc failed\n");
        pstCtx->status = HTTP_RES_STATUS_NOT_FOUND;
        return -1;
    }

    read_file_to_buffer(szDefaultConfigFileName, pstCtx->xmlBuf, file_size);
    return 0;
}

static int web_handle_get_mtu(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 32;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);

    const char *ifname = WIRE_INTERFACE_NAME;
    int mtu = net_get_mtu(ifname);

    char mtu_buf[32] = {0};
    snprintf(mtu_buf, sizeof(mtu_buf), "<Getmtu><Value=\"%d\" /></Getmtu>", mtu);
    strcpy(pstCtx->xmlBuf, mtu_buf);

    return 0;
}

static int web_handle_get_sys_control_string(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = (MAX_SYSTEM_CONTROL_STRING_LEN + 256);
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    if (pstCtx->xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc failed\n");
        pstCtx->status = 403;
        return -1;
    }

    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);
    web_expand_led_panel_capability(pstCtx->xmlBuf, pstCtx->xmlbuf_len);
    return 0;
}

static int web_handle_get_default_cfg(WebPostContext *pstCtx)
{
    if(pstCtx->bIsAdmin == 0)
    {
        __ERR("userid:%s is not admin!\n", pstCtx->recvuserid);
        pstCtx->status = 400;
        return -1;
    }

    char *default_file = anj_config_default_file_get();
    if (strlen(default_file) == 0)
    {
        __ERR("get default config file:%s failed!\n", default_file);
        pstCtx->status = 404;
        return -1;
    }
    
    pstCtx->xmlBuf = anj_mw_read_file_buffer(default_file);
    if(pstCtx->xmlBuf == NULL)
    {
        __ERR("get default config file:%s failed!\n", default_file);
        pstCtx->status = 404;
        return -1;
    }

    return 0;
}

static int web_handle_get_network_cfg(WebPostContext *pstCtx)
{
    NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
    pstCtx->xmlBuf = anj_config_network_conver_xml(pstNetworkCfg);

    return 0;
}

static int web_handle_get_app_type(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 32;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);

    if(access("/mnt/mtd/skyworth.txt", F_OK) == 0)
    {
        char appType[256] = {0};
        read_file_to_string("/mnt/mtd/skyworth.txt", appType, sizeof(appType));
    
        //ac18plus : {"ipc":{"pk":"a1JhXCOT4L6","dn":"1231913705421190099841","ds":"08e41fb300309e6fd62a4f84850b995a"}}
        char* pApp = strstr(appType, "a1JhXCOT4L6");
        if(pApp != NULL)
        {
            strcpy(pstCtx->xmlBuf, "AC18Plus");
        }
        else
        {
            strcpy(pstCtx->xmlBuf, "AC18Pro");
        }
    }
    else
    {
        strcpy(pstCtx->xmlBuf, "NOShowApp");
    }

    return 0;
}

static int web_handle_get_ircut_led_delay(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 128;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);

    if(0)
    {
        strcpy(pstCtx->xmlBuf, "0.001,0.005,0.01,0.05,0.08,0.10,0.20,0.30,0.40,0.50,0.60,0.70,0.80,0.90,1.0,1.1,1.2,1.3");
    }
    else
    {
        strcpy(pstCtx->xmlBuf, "0.01,0.05,0.08,0.10,0.20,0.30,0.40,0.50,0.60,0.70,0.80,0.90,1.0,1.1,1.2,1.3,1.4,1.5");
    }

    return 0;
}

static int web_handle_get_test_website(WebPostContext *pstCtx)
{
    TestWebSiteStruct *pstTestWebSite = get_test_website_info();

    pstCtx->xmlBuf = anj_config_oem_test_website_conver_msg_xml(pstTestWebSite);
    if(pstCtx->xmlBuf == NULL)
    {
        pstCtx->status = 404;
        return -1;
    }

    return 0;
}

static int web_handle_get_network_default_cfg(WebPostContext *pstCtx)
{
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)anj_mw_malloc(sizeof(NetworkConfigNew));
    if (pstNetworkConfig == NULL)
    {
        pstCtx->status = 404;
        return -1;
    }

    memset(pstNetworkConfig, 0, sizeof(NetworkConfigNew));
    network_lan_default_config_get(&pstNetworkConfig->lanCfg);
    
    pstCtx->xmlBuf = anj_config_network_conver_xml(pstNetworkConfig);

    anj_mw_free(pstNetworkConfig);
    pstNetworkConfig = NULL;

    return 0;
}

static int web_handle_get_wifi_cfg(WebPostContext *pstCtx)
{
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    WIFIConfig *pstWifiCfg = &pstNetworkConfig->wifiCfg;
    
    pstCtx->xmlBuf = anj_config_network_wifi_conver_xml(pstWifiCfg);
    return 0;
}

static int web_handle_get_wifi_ap_cfg(WebPostContext *pstCtx)
{
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    WIFIApConfig *pstWifiApCfg = &pstNetworkConfig->wifiApCfg;
    
    pstCtx->xmlBuf = anj_config_network_wifiap_conver_xml(pstWifiApCfg);
    return 0;
}

static int web_handle_get_wifi_ap_info(WebPostContext *pstCtx)
{
    int iRet = 0;
    static int s_last_wifi_ap_cnt = 0;
    static unsigned int s_last_search_wifi_ap_time = 0;

    __DBG("wifi ap info get last ap cnt:%d\n", s_last_wifi_ap_cnt);

    int goto_count = 0;
    WIFI_AP_SCAN apinfos = {0};

__GETWIFIAPLIST:            
    memset(&apinfos, 0, sizeof(WIFI_AP_SCAN));

    iRet = anj_net_provider_wifi_ap_info_get((void *)&apinfos);
    __DBG("### wifi ap count:%d, ret:%d\n", apinfos.apCnt, iRet);

    if(apinfos.apCnt > 0)
    {
        char *pe = NULL;
        char *pb = NULL;

        s_last_wifi_ap_cnt = apinfos.apCnt;
        int body_size = apinfos.apCnt * 256 + 256;    
        pstCtx->xmlBuf = (char *)anj_mw_malloc(body_size);
        memset(pstCtx->xmlBuf, 0, body_size);

        pb = pstCtx->xmlBuf;
        pe = pstCtx->xmlBuf + body_size - 1;
        pb += snprintf(pb, pe-pb, "<RESPONSE_PARAM ApCount=\"%d\">\n", apinfos.apCnt);

        int i = 0;
        int j = 0;

        // 如果SSID已经出现过，则跳过该条记录
        char ssidArray[50][128];
        int ssidCount = 0;

        for(i = 0; i < apinfos.apCnt; i++)
        {
            int ssidExists = 0;
            for(j = 0; j < ssidCount; j++)
            {
                if(strcmp(apinfos.apInfos[i].ssid, ssidArray[j]) == 0)
                {
                    ssidExists = 1;
                    __DBG("### ssid:%s already exists, skip it\n", apinfos.apInfos[i].ssid);
                    break;
                }
            }

            if(ssidExists)
            {
                continue;
            }

            strncpy(ssidArray[ssidCount], apinfos.apInfos[i].ssid, sizeof(apinfos.apInfos[i].ssid) -1);

            ssidArray[ssidCount][127] = '\0';
            ssidCount++;
            pb += snprintf(pb, pe-pb, "<WifiAp ");
            pb += snprintf(pb, pe-pb, "Ssid=\"%s\" ", apinfos.apInfos[i].ssid);
            pb += snprintf(pb, pe-pb, "Quality=\"%d\" ", apinfos.apInfos[i].quality);
            pb += snprintf(pb, pe-pb, "SignalLevel=\"%d\" ", apinfos.apInfos[i].signalLevel);
            pb += snprintf(pb, pe-pb, "WirelessMode=\"%s\" ", apinfos.apInfos[i].wirelessMode);
            pb += snprintf(pb, pe-pb, "AuthMode=\"%s\" ", anj_net_wifi_auth_str(apinfos.apInfos[i].authMode));
            pb += snprintf(pb, pe-pb, "EncryptType=\"%s\" ", anj_net_wifi_encrypt_str(apinfos.apInfos[i].encryType));
            pb += snprintf(pb, pe-pb, "NoiseLevel=\"%d\" ", apinfos.apInfos[i].noiseLevel);
            pb += snprintf(pb, pe-pb, "Channelnum=\"%d\"\n", apinfos.apInfos[i].channel);
            pb += snprintf(pb, pe-pb, "/>\n");
        }

        pb += snprintf(pb, pe-pb, "</RESPONSE_PARAM>"); 
        write_buffer_to_file("/mnt/nand/wifiApList.info", pstCtx->xmlBuf, body_size);
    }
    else
    {
        if(access("/mnt/nand/wifiApList.info", F_OK) == 0)
        {
            if(s_last_wifi_ap_cnt <= 0)
            {
                s_last_wifi_ap_cnt = 50;
            }

            int body_size = s_last_wifi_ap_cnt * 256 + 256;
            pstCtx->xmlBuf = (char *)anj_mw_malloc(body_size);

            if(pstCtx->xmlBuf != NULL)
            {
                memset(pstCtx->xmlBuf, 0, body_size);
                __DBG("### wifiap list info get last count:%d\n", s_last_wifi_ap_cnt);

                iRet = read_file_to_string("/mnt/nand/wifiApList.info", pstCtx->xmlBuf, body_size);
                if(iRet >= 0)
                {
                    ;
                }
                else
                {
                    strcpy(pstCtx->xmlBuf, "<RESPONSE_PARAM ApCount=\"0\">\n</RESPONSE_PARAM>"); 
                }
            }
        }
        else
        {
            if(goto_count == 0)
            {
                goto_count = 1;
                usleep(2 * 1000 * 1000);
                goto __GETWIFIAPLIST;
            }
            else
            {
                pstCtx->xmlbuf_len = 1024;
                pstCtx->xmlBuf=(char *)anj_mw_malloc(pstCtx->xmlbuf_len);
                strcpy(pstCtx->xmlBuf, "<RESPONSE_PARAM ApCount=\"0\">\n</RESPONSE_PARAM>"); 
            }
        }
    }

    __DBG("### wifi ap info goto count:%d, now - last_search_time:%d\n", goto_count, pstCtx->Nowtime - s_last_search_wifi_ap_time);
    s_last_search_wifi_ap_time = pstCtx->Nowtime;

    return 0;
}

static int web_handle_get_wifi_work_status(WebPostContext *pstCtx)
{
    NETWORK_STATUS_DATA *pstNetworkStatus = (NETWORK_STATUS_DATA *)anj_mw_malloc(sizeof(NETWORK_STATUS_DATA));
    if (pstNetworkStatus == NULL)
    {
        pstCtx->status = 403;
        return -1;
    }

    memset(pstNetworkStatus, 0, sizeof(NETWORK_STATUS_DATA));
    anj_net_info_get(pstNetworkStatus);

    pstCtx->xmlBuf = xml_conver_wifi_network_status(pstNetworkStatus);
    
    anj_mw_free(pstNetworkStatus);
    pstNetworkStatus = NULL;

    return 0;
}

static int web_handle_get_alarm_server_cfg(WebPostContext *pstCtx)
{
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    AlarmServerConfig *pstAlarmServerCfg = &pstNetworkConfig->alarmServerCfg;

    pstCtx->xmlBuf = anj_config_network_alarmserver_conver_xml(pstAlarmServerCfg, 0);
    return 0;
}

static int web_handle_get_network_status(WebPostContext *pstCtx)
{
    NETWORK_STATUS_DATA *pstNetworkStatus = (NETWORK_STATUS_DATA *)anj_mw_malloc(sizeof(NETWORK_STATUS_DATA));
    memset(pstNetworkStatus, 0, sizeof(NETWORK_STATUS_DATA));

    anj_net_info_get(pstNetworkStatus);
    pstCtx->xmlBuf = xml_conver_network_status(pstNetworkStatus);

    anj_mw_free(pstNetworkStatus);
    pstNetworkStatus = NULL;

    return 0;
}

static int web_handle_get_ftp_cfg(WebPostContext *pstCtx)
{
    ServerConfig *pstServerCfg = (ServerConfig *)getServerConfig();
    FtpServerList stftpServerList = {0};
    memcpy(&stftpServerList, pstServerCfg->ftpServers, sizeof(FtpServerList));

    pstCtx->xmlBuf = anj_config_server_ftp_conver_xml(&stftpServerList, 0);
    return 0;
}

static int web_handle_get_smtp_cfg(WebPostContext *pstCtx)
{
    ServerConfig *pstServerCfg = (ServerConfig *)getServerConfig();
    SmtpServerList *pstSmtpServerCfg = &pstServerCfg->smtpServers;

    pstCtx->xmlBuf = anj_config_server_smtp_conver_xml(pstSmtpServerCfg, 0);
    return 0;
}

static int web_handle_get_media_stream_cfg(WebPostContext *pstCtx)
{
    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    pstCtx->xmlBuf = anj_config_stream_conver_xml(pstMediaStreamCfg);

    return 0;
}

static int web_handle_get_platform_cfg(WebPostContext *pstCtx)
{
    PlatformConfig *pstPlatformCfg = (PlatformConfig *)getPlatformConfig();
    pstCtx->xmlBuf = anj_config_platform_conver_xml(pstPlatformCfg, 0);

    return 0;
}

static int web_handle_get_gb28181_cfg(WebPostContext *pstCtx)
{
    GB28181Config *pstGb28181Cfg = (GB28181Config *)getGb28181Config();
    pstCtx->xmlBuf = anj_config_gb28181_conver_xml(pstGb28181Cfg);

    return 0;
}

static int web_handle_get_gat1400_cfg(WebPostContext *pstCtx)
{
    GAT1400Config *pstGat1400Cfg = (GAT1400Config *)getGat1400Config();
    pstCtx->xmlBuf = anj_config_gat1400_conver_xml(pstGat1400Cfg);

    return 0;
}

static int web_handle_get_rtmp_cfg(WebPostContext *pstCtx)
{
    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    RtmpConfig *pstRtmpCfg = &pstMediaStreamCfg->rtmpConfig;
    
    pstCtx->xmlBuf = anj_config_stream_rtmp_conver_xml(pstRtmpCfg);
    return 0;
}

static int web_handle_get_video_size(WebPostContext *pstCtx)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoConfig *pstVideoCfg = &pstMediaCfg->videoConfig[chn];
    
    int iIndex = 0;
    int width[2] = {0};
    int height[2] = {0};
    
    for(iIndex = 0; iIndex < 2; iIndex++)
    {
        GetVideoSize(pstVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name,
            pstVideoCfg->videoCapture.tvsystem, 
            &(width[iIndex]), &(height[iIndex]));
    }
    
    pstCtx->xmlbuf_len = 128;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    if(pstCtx->xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc error!\n");
        pstCtx->status = 500;
        return -1;
    }
    
    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);
    snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len, 
            "<VideoSize><mainStream width=\"%d\" height=\"%d\" /><subStream width=\"%d\" height=\"%d\" /></VideoSize>",
            width[0], height[0], width[1], height[1]);

    return 0;
}

static int web_handle_get_media_video_cfg(WebPostContext *pstCtx)
{
    int iCameraIdx = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoConfig *pstVideoCfg = &pstMediaCfg->videoConfig[iCameraIdx];
    
    char *videoxml = anj_config_video_conver_xml(pstVideoCfg, iCameraIdx, 1);
    
    RESOLUTION_ENTRY *pVideoEntry = NULL;
    int vrescount = anj_sysmng_video_res_array_get(&pVideoEntry);
    
    int myXmlBuf_len = (strlen(videoxml) + vrescount * 256);
    char *myXmlBuf = anj_mw_malloc(myXmlBuf_len);
    if(myXmlBuf == NULL)
    {
        __ERR("myxmlBuf malloc error!\n");
        anj_mw_free(videoxml);
        pstCtx->status = 500;

        return -1;
    }
    
    memset(myXmlBuf, 0, myXmlBuf_len);
    memcpy(myXmlBuf, videoxml, strlen(videoxml));
    anj_mw_free(videoxml);
    videoxml = NULL;

    pstCtx->xmlBuf = myXmlBuf;
    xml_conver_video_code_list(pstCtx->xmlBuf, pVideoEntry, vrescount);

    return 0;
}

static int web_handle_get_media_jpeg_cfg(WebPostContext *pstCtx)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    JpegEncodeCfg *pstJpegCfg = &pstMediaCfg->videoConfig[chn].jpegCfg;

    pstCtx->xmlBuf = anj_config_jpeg_conver_xml(pstJpegCfg);
    return 0;
}

static int web_handle_get_video_default_cfg(WebPostContext *pstCtx)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoConfig *pstVideoCfg = &pstMediaCfg->videoConfig[chn];

    VideoConfig stVideoDefaultCfg = {0};
    memcpy(&stVideoDefaultCfg, pstVideoCfg, sizeof(VideoConfig));
    
    RESOLUTION_ENTRY *pVideoEntry = NULL;
    int vrescount = anj_sysmng_video_res_array_get(&pVideoEntry);
    int main_def = 0;
    int sub_def = 0;

    int i = 0;
    for(i = 0;i < vrescount; i++)
    {
        if((pVideoEntry[i].stream_type == 0) && (pVideoEntry[i].def_config != 0))
            main_def = i;
        if((pVideoEntry[i].stream_type == 1) && (pVideoEntry[i].def_config != 0))
            sub_def = i;
    }
    
    if((main_def != 0) || (sub_def != 0))
    {
        memset(stVideoDefaultCfg.videoEncode.encodeCfg[0].resolution.name, 0, sizeof(stVideoDefaultCfg.videoEncode.encodeCfg[0].resolution.name));
        strncpy(stVideoDefaultCfg.videoEncode.encodeCfg[0].resolution.name, pVideoEntry[main_def].res_name, RESOLUTION_NAME_MAX_LEN - 1);

        memset(stVideoDefaultCfg.videoEncode.encodeCfg[0].encodeFormat.name, 0, sizeof(stVideoDefaultCfg.videoEncode.encodeCfg[0].encodeFormat.name));
        strncpy(stVideoDefaultCfg.videoEncode.encodeCfg[0].encodeFormat.name, pVideoEntry[main_def].codec_name, VIDEO_ENCODE_FORAMT_MAX_LEN - 1);

        stVideoDefaultCfg.videoEncode.encodeCfg[0].initQuant = 100;
        stVideoDefaultCfg.videoEncode.encodeCfg[0].bitRate = pVideoEntry[main_def].def_bitrate;
        stVideoDefaultCfg.videoEncode.encodeCfg[0].bitRateQuality = VIDEO_QUALITY_CUSTOM;
        stVideoDefaultCfg.videoEncode.encodeCfg[0].frameRate = pVideoEntry[main_def].def_framerate;
        stVideoDefaultCfg.videoEncode.encodeCfg[0].display_frameRate = pVideoEntry[main_def].def_framerate;

        memset(stVideoDefaultCfg.videoEncode.encodeCfg[1].resolution.name, 0, sizeof(stVideoDefaultCfg.videoEncode.encodeCfg[1].resolution.name));
        strncpy(stVideoDefaultCfg.videoEncode.encodeCfg[1].resolution.name, pVideoEntry[sub_def].res_name, RESOLUTION_NAME_MAX_LEN - 1);

        memset(stVideoDefaultCfg.videoEncode.encodeCfg[1].encodeFormat.name, 0, sizeof(stVideoDefaultCfg.videoEncode.encodeCfg[1].encodeFormat.name));
        strncpy(stVideoDefaultCfg.videoEncode.encodeCfg[1].encodeFormat.name, pVideoEntry[sub_def].codec_name, VIDEO_ENCODE_FORAMT_MAX_LEN - 1);

        stVideoDefaultCfg.videoEncode.encodeCfg[1].initQuant = 100;
        stVideoDefaultCfg.videoEncode.encodeCfg[1].bitRate = pVideoEntry[sub_def].def_bitrate;
        stVideoDefaultCfg.videoEncode.encodeCfg[1].bitRateQuality = VIDEO_QUALITY_CUSTOM;
        stVideoDefaultCfg.videoEncode.encodeCfg[1].frameRate = pVideoEntry[sub_def].def_framerate;
        stVideoDefaultCfg.videoEncode.encodeCfg[1].display_frameRate = pVideoEntry[sub_def].def_framerate;

        //MsgSetVideoConfig(pVideoCfg);
    }

    char *videoxml = anj_config_video_conver_xml(&stVideoDefaultCfg, chn, 1);

    int myXmlBuf_len = (strlen(videoxml) + vrescount * 256);
    char *myXmlBuf = anj_mw_malloc(myXmlBuf_len);
    if(myXmlBuf == NULL)
    {
        __ERR("myxmlBuf malloc error!\n");
        anj_mw_free(videoxml);
        pstCtx->status = 500;

        return -1;
    }

    memset(myXmlBuf, 0, myXmlBuf_len);
    memcpy(myXmlBuf, videoxml, strlen(videoxml));
    anj_mw_free(videoxml);
    videoxml = NULL;

    pstCtx->xmlBuf = myXmlBuf;
    xml_conver_video_code_list(pstCtx->xmlBuf, pVideoEntry,vrescount);

    return 0;
}

static int web_handle_get_media_audio_cfg(WebPostContext *pstCtx)
{
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *pstAudioCfg = &pstMediaCfg->audioConfig;

    char *xmlBuf = anj_config_audio_conver_xml(pstAudioCfg);
    if (xmlBuf == NULL)
    {
        __ERR("anj_config_audio_conver_xml failed!\n");
        pstCtx->status = 500;
        return -1;
    }

    AUDIO_CODEC_ENTRY *audio_entry = NULL;
    int arescount = anj_sysmng_audio_res_array_get(&audio_entry);

    int myXmlBuf_len = (strlen(xmlBuf) + arescount * 130);
    char *myXmlBuf = anj_mw_malloc(myXmlBuf_len);
    if(myXmlBuf == NULL)
    {
        __ERR("myxmlBuf malloc error!\n");
        anj_mw_free(xmlBuf);
        pstCtx->status = 500;

        return -1;
        //服务器内部错误
    }
    
    memset(myXmlBuf, 0, myXmlBuf_len);
    memcpy(myXmlBuf, xmlBuf, strlen(xmlBuf));
    anj_mw_free(xmlBuf);
    xmlBuf = NULL;
    
    pstCtx->xmlBuf = myXmlBuf;
    xml_conver_audio_code_list(pstCtx->xmlBuf, audio_entry, arescount);

    return 0;
}

static int web_handle_get_audio_default_cfg(WebPostContext *pstCtx)
{
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *pstAudioCfg = &pstMediaCfg->audioConfig;

    AudioConfig stAudioDefaultCfg = {0};
    memcpy(&stAudioDefaultCfg, pstAudioCfg, sizeof(AudioConfig));
    memset(stAudioDefaultCfg.audioEncode.audioEncodeType.typeName, 0, sizeof(stAudioDefaultCfg.audioEncode.audioEncodeType.typeName));

    AUDIO_CODEC_ENTRY *audio_entry = NULL;
    int arescount = anj_sysmng_audio_res_array_get(&audio_entry);

    int i = 0;
    for(i = 0;i < arescount; i++)
    {
        if(audio_entry[i].def_config != 0)
            break;            
    }
    
    if(i >= arescount)
    {
        i=0;
    }

    strncpy(stAudioDefaultCfg.audioEncode.audioEncodeType.typeName, audio_entry[i].codec_name, AUDIO_ENCODE_TYPE_MAX_LEN-1);
    stAudioDefaultCfg.audioEncode.sampleRate = audio_entry[i].samplerate;
    stAudioDefaultCfg.audioEncode.bitRate = audio_entry[i].bitrate;
    stAudioDefaultCfg.audioEncode.enable = 1;
    //anj_config_audio_set(pstAudioDefCfg);

    char *audioxml = anj_config_audio_conver_xml(&stAudioDefaultCfg);

    int myXmlBuf_len = (strlen(audioxml) + arescount * 130);
    char *myXmlBuf = anj_mw_malloc(myXmlBuf_len);
    if(myXmlBuf == NULL)
    {
        __ERR("myxmlBuf malloc error!\n");
        anj_mw_free(audioxml);
        pstCtx->status = 500;

        return -1;
        //服务器内部错误
    }
    
    memset(myXmlBuf, 0, myXmlBuf_len);
    memcpy(myXmlBuf, audioxml, strlen(audioxml));
    anj_mw_free(audioxml);
    audioxml = NULL;
    
    pstCtx->xmlBuf = myXmlBuf;
    xml_conver_audio_code_list(pstCtx->xmlBuf, audio_entry, arescount);

    return 0;
}

static int web_handle_get_user_cfg(WebPostContext *pstCtx)
{
    if(pstCtx->bIsAdmin == 0)
    {
        __ERR("userid:%s is not admin", pstCtx->recvuserid);
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    UserConfig *pstUsrCfg = &pstSystemCfg->userCfg;

    pstCtx->xmlBuf = anj_config_system_user_conver_xml(pstUsrCfg, 0);
    return 0;
}

static int web_handle_get_user_pwd_entrypt(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    UserConfig *pstUsrCfg = &pstSystemCfg->userCfg;

    pstCtx->xmlBuf = anj_config_system_user_conver_xml(pstUsrCfg, 1);
    return 0;
}

static int web_handle_get_system_log_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    SyslogConfig *pstSyslogCfg = &pstSystemCfg->syslogCfg;

    pstCtx->xmlBuf = anj_config_system_syslog_conver_xml(pstSyslogCfg);
    return 0;
}

static int web_handle_get_system_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    pstCtx->xmlBuf = anj_config_system_conver_xml(pstSystemCfg);

    return 0;
}

static int web_handle_get_misc_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    MiscConfig *pstMisCfg = &pstSystemCfg->miscCfg;
    
    pstCtx->xmlBuf = anj_config_system_misc_conver_xml(pstMisCfg);

    return 0;
}

static int web_handle_get_record_cfg(WebPostContext *pstCtx)
{
    int iCameraIdx = 0;
    RecordConfig *pRecordConfig = (RecordConfig *)getRecordConfig();
    pstCtx->xmlBuf = anj_config_record_conver_xml(pRecordConfig, iCameraIdx, 1);

    return 0;
}

static int web_handle_get_time_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TimeConfig *pstTimeCfg = &pstSystemCfg->timeCfg;
    
    char *xmlBuf = anj_config_system_time_conver_xml(pstTimeCfg);
    if(xmlBuf != NULL)
    {
        size_t xmlLen = strlen(xmlBuf);
        char *xmlTemp = (char *)anj_mw_malloc(xmlLen + 128);
        if (xmlTemp == NULL)
        {
            anj_mw_free(xmlBuf);
            return -1;
        }
        memset(xmlTemp, 0, xmlLen + 128);
        strcpy(xmlTemp, xmlBuf);

        char *p = strstr(xmlTemp, "TimeZone");
        if(p != NULL)
        {
            char buf[1024] = {0};
            char strCurTime[128] = {0};
            snprintf(buf, sizeof(buf), "%s", p);
    
            struct tm stTime = {0};
            SystemLocalTime(&stTime);
            snprintf(strCurTime, sizeof(strCurTime), "CurTime=\"%04d-%02d-%02d %02d:%02d:%02d\" \n",
                (stTime.tm_year + 1900), (stTime.tm_mon + 1), stTime.tm_mday,
                stTime.tm_hour, stTime.tm_min, stTime.tm_sec);
    
            snprintf(p, xmlLen + 128 - (size_t)(p - xmlTemp), "%s%s", strCurTime, buf);
        }
    
        anj_mw_free(xmlBuf);
        xmlBuf = NULL;

        pstCtx->xmlBuf = xmlTemp;
    }

    return 0;
}


static int web_handle_get_motion_alarm_cfg(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pstMotionDetAlarmCfg = &pstAlarmCfg->normalAlarm.motionDetectAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_motion_conver_xml(pstMotionDetAlarmCfg, alarm_chn, 1);
    return 0;
}

static int web_handle_get_video_lost_alarm(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoLostAlarm *pstVideoLostAlarmCfg = &pstAlarmCfg->normalAlarm.videoLostAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_video_lost_conver_xml(pstVideoLostAlarmCfg, alarm_chn, 1);
    return 0;
}

static int web_handle_get_input_alarm(WebPostContext *pstCtx)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    InputAlarm *pstInputAlarm = &pstAlarmCfg->normalAlarm.inputAlarm;

    pstCtx->xmlBuf = anj_config_alarm_input_conver_xml(pstInputAlarm);
    return 0;
}

static int web_handle_get_output_alarm(WebPostContext *pstCtx)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    OutPutAlarm *pstOutputAlarmCfg = &pstAlarmCfg->normalAlarm.outputAlarm;

    pstCtx->xmlBuf = anj_config_alarm_output_conver_xml(pstOutputAlarmCfg);
    return 0;
}

static int web_handle_get_sms_alarm(WebPostContext *pstCtx)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    SMSAlarm *pstSmsAlarmCfg = &pstAlarmCfg->normalAlarm.smsAlarm;

    pstCtx->xmlBuf = anj_config_alarm_sms_conver_xml(pstSmsAlarmCfg);
    return 0;
}

static int web_handle_get_storage_alarm(WebPostContext *pstCtx)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    StorageFullAlarm *pstStorageFullAlarmCfg = &pstAlarmCfg->normalAlarm.storageFullAlarm;

    pstCtx->xmlBuf = anj_config_alarm_storage_full_conver_xml(pstStorageFullAlarmCfg);
    return 0;
}

static int web_handle_get_audio_alarm(WebPostContext *pstCtx)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    AudioAlarm *pstAudioAlarmCfg = &pstAlarmCfg->aiAlarm.audioAlarm;

    pstCtx->xmlBuf = anj_config_alarm_audio_conver_xml(pstAudioAlarmCfg);
    return 0;
}

static int web_handle_get_video_gate_alarm(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pVideoGateCfg = &pstAlarmCfg->aiAlarm.vgAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_video_gate_conver_xml(pVideoGateCfg, alarm_chn, 1);
    return 0;
}

static int web_handle_get_pd_alarm(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarmCfg = &pstAlarmCfg->aiAlarm.pdAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_pd_conver_xml(pstPdAlarmCfg, alarm_chn, 1);
    return 0;
}

static int web_handle_get_lpr_alarm(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    LprAlarm *pstLprAlarmCfg = &pstAlarmCfg->aiAlarm.lprAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_lpr_conver_xml(pstLprAlarmCfg);
    return 0;
}

static int web_handle_get_flame_alarm(WebPostContext *pstCtx)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FlameAndFlumesAlarm *pstFlameAlarmCfg = &pstAlarmCfg->aiAlarm.fireAlarm;

    pstCtx->xmlBuf = anj_config_alarm_fire_conver_xml(pstFlameAlarmCfg);
    return 0;
}

static int web_handle_get_video_cover_alarm(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoCoverAlarm *pstVideoCoverAlarmCfg = &pstAlarmCfg->normalAlarm.videoCoverAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_video_cover_conver_xml(pstVideoCoverAlarmCfg, alarm_chn, 1);
    return 0;
}

static int web_handle_get_fd_alarm(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm *pstFaceDetAlarmCfg = &pstAlarmCfg->aiAlarm.fdAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_fd_conver_xml(pstFaceDetAlarmCfg, alarm_chn, 1);
    return 0;
}

static int web_handle_get_temp_humidity_alarm(WebPostContext *pstCtx)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    TempHumidityAlarm *pstTempHumidityCfg = &pstAlarmCfg->normalAlarm.temphumidityAlarm;

    pstCtx->xmlBuf = anj_config_alarm_temp_humidity_conver_xml(pstTempHumidityCfg);
    return 0;
}

static int web_handle_get_video_region_alarm(WebPostContext *pstCtx)
{
    int alarm_chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pstVideoRegionCfg = &pstAlarmCfg->aiAlarm.regionAiAlarm[alarm_chn];

    pstCtx->xmlBuf = anj_config_alarm_region_ai_conver_xml(pstVideoRegionCfg, alarm_chn, 1);

    return 0;
}

static int web_handle_get_pd_license(WebPostContext *pstCtx)
{
    int valid = 1;
    pstCtx->xmlbuf_len = 64;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);

    snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len, "<PdMadpLicense valid=\"%d\" />", valid);

    return 0;
}

static int web_handle_get_mstar_chip_uuid(WebPostContext *pstCtx)
{
    char uuid[64] = {0};
    int iRet = read_file_to_string("/tmp/mstar_chip_uuid", uuid, sizeof(uuid)); 
    if(iRet > 0 && iRet < 64)
    {
        uuid[iRet] = '\0';
    }
    else
    {
        __ERR("read mstar chip uuid file failed!\n");
        uuid[0] = 'A';
        uuid[1] = '\0';
    }
    pstCtx->xmlbuf_len = 100;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);

    snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len, "<uuid uuid=\"%s\" />", uuid);

    return 0;
}

static int web_handle_get_system_version(WebPostContext *pstCtx)
{
    DevInfo *pstDevInfo = getDevInfo();
    SYSTEM_VERSION_DATA *pstVersionInfo = &pstDevInfo->stVersionInfo;

    AjOemStruct stdevOemInfo = {0};
    anj_config_oem_get(&stdevOemInfo);

    char mySN[32] = {0};
    anj_sysmng_load_sn(mySN, sizeof(mySN));

    pstCtx->xmlBuf = xml_conver_system_version(pstVersionInfo, mySN);
    return 0;
}

static int web_handle_get_storage_dev_info(WebPostContext *pstCtx)
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

    pstCtx->xmlbuf_len = 1024;
    pstCtx->xmlBuf = anj_mw_malloc(pstCtx->xmlbuf_len);
    snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len,
            "<StorageDevInfo>"
            "<sd1 mounted=\"%d\" total=\"%d\" used=\"%d\" free=\"%d\" percent=\"%d\" />"
            "<sd2 mounted=\"%d\" total=\"%d\" used=\"%d\" free=\"%d\" percent=\"%d\" />"
            "<usb mounted=\"%d\" total=\"%d\" used=\"%d\" free=\"%d\" percent=\"%d\" />"
            "<network mounted=\"%d\" total=\"%d\" used=\"%d\" free=\"%d\" percent=\"%d\" />"
            "</StorageDevInfo>",
            bMounted, stSdInfo.iSize, iUseSize, stSdInfo.iRemainSize, stSdInfo.iFormatPercent,
            0, 0, 0, 0, 0,
            0, 0, 0, 0, 0,
            nfsMounted, nfsTotal, nfsUsed, nfsFree, nfsPercent);

    __INFO("sd status:%d\n", status);
    return 0;
}

static int web_handle_get_web_upload_status(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 64;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    if(pstCtx->xmlBuf != NULL)
    {
        memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);
        snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len, "<FormFileStatus status=\"%d\" />", http_upload_form_status_get());
    }

    return 0;
}

static int web_handle_get_oem_mount_status(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 64;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    if(pstCtx->xmlBuf != NULL)
    {
        memset(pstCtx->xmlBuf, 0, 64 * sizeof(char));
        const char *path = get_oem_mp3_path();
        
        if(path == NULL)
            strcpy(pstCtx->xmlBuf, "<OemDevMountedStatus Mp3Status=\"0\" />");
        else
            strcpy(pstCtx->xmlBuf, "<OemDevMountedStatus Mp3Status=\"1\" />");
    }

    return 0;
}

static int web_handle_get_audio_action_files(WebPostContext *pstCtx)
{
    int skipcount = 0;
    int pagesize = 15;
    int filcount = 0;

    file_query_result *pResult = (file_query_result *)anj_mw_malloc(sizeof(file_query_result));
    memset(pResult, 0, sizeof(file_query_result));

    filcount = anj_audio_mp3_file_list_query(pResult, skipcount, pagesize);
    pstCtx->xmlBuf = xml_conver_normal_file_list(pResult, filcount);

    anj_mw_free(pResult);
    pResult = NULL;

    return 0;
}

static int web_handle_get_https_files(WebPostContext *pstCtx)
{
    file_query_result *pResult = (file_query_result *)anj_mw_malloc(sizeof(file_query_result));
    memset(pResult, 0, sizeof(file_query_result));

    int skipcount = 0;
    int pagesize = 10;
    int totalcount = 0;
    int filecount = 0;

    filecount = query_normal_file_in_dir(pResult, skipcount, pagesize, ".crt", DATA_BLOCK_MOUNT_PATH);
    totalcount += filecount;

    filecount = query_normal_file_in_dir(pResult, skipcount, pagesize, ".key", DATA_BLOCK_MOUNT_PATH);
    totalcount += filecount;

    filecount = query_normal_file_in_dir(pResult, skipcount, pagesize, ".pwd", DATA_BLOCK_MOUNT_PATH);            
    totalcount += filecount;

    int i = 0;
    for(i = 0; i < totalcount; i++)
    {
        __ERR("query http files[%d]:%s!\n", i, pResult->file_info[i].filepath);
    }

    pstCtx->xmlBuf = xml_conver_normal_file_list(pResult, totalcount);
    anj_mw_free(pResult);
    pResult = NULL;

    return 0;
}

static int web_handle_get_odm_logo_files(WebPostContext *pstCtx)
{
    file_query_result *pResult = (file_query_result *)anj_mw_malloc(sizeof(file_query_result));
    memset(pResult, 0, sizeof(file_query_result));

    int skipcount = 0;
    int pagesize = 10;
    int totalcount = 0;
    int filecount = 0;

    filecount = query_normal_file_in_dir(pResult, skipcount, pagesize, ".bmp", DATA_BLOCK_MOUNT_PATH);
    __INFO("odm logo filecount:%d\n", filecount);

    pstCtx->xmlBuf = xml_conver_normal_file_list(pResult, totalcount);
    anj_mw_free(pResult);
    pResult = NULL;

    return 0;
}

static int web_handle_get_record_query_info(WebPostContext *pstCtx)
{
    if(pstCtx->pSoapBody == NULL) 
    {
        __ERR("soapBody error\n");
        pstCtx->status = 400;
        return -1;
    }

    record_query_condition_s stQueryCondition = {0};
    record_query_result_s stRecQueryResult = {0};

    int skipCount = 0;
    parse_record_query_condition_by_xml(pstCtx->pSoapBody, &stQueryCondition, &skipCount);

    int query_flag = 0;
    while (1)
    {
        query_flag = http_query_record_get();
        if(query_flag == 1)
        {
            usleep(10 * 1000);
        }
        else
        {
            break;
        }
    }

    http_query_record_set(1);

    int i = 0;
    int bFull = 0, nextFileNo = 0;
    int iMediaMaxFiles = anj_record_max_file_get(&bFull, &nextFileNo);

    for(i = 0; i < iMediaMaxFiles; i++)
    {
        rec_file_index_record *pstIndexRecord = anj_record_file_info_get(i);
        //REC_EVENT_SET_MASK(pstIndexRecord->tMediaFileEvent, REC_EVENT_NULL_MASK);
        if (pstIndexRecord->iMediaFileStatus == REC_STATUS_NULL)
        {
            continue;
        }

        if (stRecQueryResult.count < REC_SEGMENT_MAX_COUNT)
        {
            anj_record_get_media_file_name(stRecQueryResult.items[stRecQueryResult.count].filepath, sizeof(stRecQueryResult.items[stRecQueryResult.count].filepath), i);
            stRecQueryResult.items[stRecQueryResult.count].filesize = REC_MEDIA_FILE_SIZE;
            stRecQueryResult.count++;
        }
    }

    http_query_record_set(0);

    pstCtx->xmlBuf = xml_conver_record_query_info(&stRecQueryResult);
    if(pstCtx->xmlBuf == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    return 0;
}

static int web_handle_get_record_distribute_dist(WebPostContext *pstCtx)
{
    char *pY1 = NULL;
    char *pM1 = NULL;
    char *pD1 = NULL;
    const char *pY2 = NULL;
    const char *pM2 = NULL;
    const char *pD2 = NULL;

    char strY[5] = {0};
    char strM[3] = {0};
    char strD[3] = {0};
    int iyear = 0;
    int imonth = 0;
    int iday = 0;

    if (pstCtx == NULL || pstCtx->path == NULL)
    {
        __ERR("invalid record distribute request\n");
        if (pstCtx)
        {
            pstCtx->status = 400;
        }
        return -1;
    }

    pY1 = strstr(pstCtx->path, "&year=");
    pM1 = strstr(pstCtx->path, "&month=");
    pD1 = strstr(pstCtx->path, "&day=");
    if (pY1 && pM1 && pD1)
    {
        pY2 = pM1;
        pM2 = pD1;
        pD2 = pstCtx->path + strlen(pstCtx->path);

        pY1 += strlen("&year=");
        pM1 += strlen("&month=");
        pD1 += strlen("&day=");

        if (pY1 < pY2 && pM1 < pM2 && pD1 <= pD2)
        {
            strncpy(strY, pY1, (size_t)(pY2 - pY1));
            strncpy(strM, pM1, (size_t)(pM2 - pM1));
            strncpy(strD, pD1, (size_t)(pD2 - pD1));
            strY[4] = '\0';
            strM[2] = '\0';
            strD[2] = '\0';
            iyear = atoi(strY);
            imonth = atoi(strM);
            iday = atoi(strD);
        }
    }

    if((iyear > 0) && (imonth > 0) && (iday > 0))
    {    
        char *distribute = (char *)anj_mw_malloc((VS_RECORD_DISTRIBUTE_LEN + 4) * sizeof(char));
        if (distribute == NULL)
        {
            __ERR("malloc distribute failed\n");
            pstCtx->status = 500;
            return -1;
        }
        memset(distribute, 0, VS_RECORD_DISTRIBUTE_LEN + 4);
        memset(distribute, VS_REC_TYPE_NO_RECORDING, VS_RECORD_DISTRIBUTE_LEN);
        distribute[VS_RECORD_DISTRIBUTE_LEN] = '\0';

        int query_flag = 0;
        int wait_count = 0;
        while (wait_count < 500)
        {
            query_flag = http_query_record_get();
            if(query_flag == 1)
            {
                usleep(10 * 1000);
                wait_count++;
            }
            else
            {
                break;
            }
        }

        http_query_record_set(1);

        int chn = 0;
        rec_pb_date_s pbDate = {0};
        pbDate.tEvent = REC_EVENT_ALL;
        pbDate.year = iyear;
        pbDate.month = imonth;
        pbDate.day = iday;
        rec_pb_list_s stPbList = {0};

        if (anj_record_pb_query_day_create(chn, &pbDate, &stPbList) == 0)
        {
            rec_pb_segment_s *pHead = stPbList.pstSegment;
            while (pHead != NULL)
            {
                struct tm stSegTmBegin;
                struct tm stSegTmEnd;
                time_t tBegin = (time_t)pHead->begin_time_s;
                time_t tEnd = (time_t)pHead->end_time_s;
                int beginMin = 0;
                int endMin = 0;

                memset(&stSegTmBegin, 0, sizeof(stSegTmBegin));
                memset(&stSegTmEnd, 0, sizeof(stSegTmEnd));
                localtime_r(&tBegin, &stSegTmBegin);
                localtime_r(&tEnd, &stSegTmEnd);

                beginMin = stSegTmBegin.tm_hour * 60 + stSegTmBegin.tm_min;
                endMin = stSegTmEnd.tm_hour * 60 + stSegTmEnd.tm_min;
                if (beginMin < 0)
                {
                    beginMin = 0;
                }
                if (endMin >= VS_RECORD_DISTRIBUTE_LEN)
                {
                    endMin = VS_RECORD_DISTRIBUTE_LEN - 1;
                }
                if (beginMin >= VS_RECORD_DISTRIBUTE_LEN)
                {
                    pHead = pHead->ptNext;
                    continue;
                }

                for (int startMin = beginMin; startMin <= endMin && startMin < VS_RECORD_DISTRIBUTE_LEN; startMin++)
                {
                    if (REC_EVENT_CHECK_MASK(pHead->tEvent, REC_EVENT_MOTION_ALARM_MASK))
                    {
                        distribute[startMin] = VS_REC_TYPE_MD;
                    }
                    else if (REC_EVENT_CHECK_MASK(pHead->tEvent, REC_EVENT_HUMEN_ALARM_MASK))
                    {
                        distribute[startMin] = VS_REC_TYPE_PD;
                    }
                    else
                    {
                        if (distribute[startMin] == VS_REC_TYPE_NO_RECORDING)
                        {
                            distribute[startMin] = VS_REC_TYPE_UNCONDITIONAL;
                        }
                    }
                }

                pHead = pHead->ptNext;
            }
            anj_record_pb_query_day_release(chn, &stPbList);
        }

        http_query_record_set(0);

        pstCtx->xmlbuf_len = VS_RECORD_DISTRIBUTE_LEN + 64;
        pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
        if (pstCtx->xmlBuf == NULL)
        {
            __ERR("malloc xmlBuf failed\n");
            anj_mw_free(distribute);
            pstCtx->status = 500;
            return -1;
        }
        memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);
        snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len,
                 "<RecordDistribute> range=\"%.*s\" </RecordDistribute>",
                 VS_RECORD_DISTRIBUTE_LEN, distribute);

        anj_mw_free(distribute);
        distribute = NULL;
    }
    else
    {
        pstCtx->xmlbuf_len = 50;
        pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
        if (pstCtx->xmlBuf == NULL)
        {
            pstCtx->status = 500;
            return -1;
        }
        memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);
        snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len, "<RecordDistribute></RecordDistribute>");
    }                                   

    return 0;
}

static int web_handle_get_log_list(WebPostContext *pstCtx)
{
    pstCtx->xmlBuf = xml_conver_log_file_list();
    return 0;
}

static int web_handle_get_firmware_upload_progress(WebPostContext *pstCtx)
{
    pstCtx->xmlbuf_len = 128;
    pstCtx->xmlBuf = (char *)anj_mw_malloc(pstCtx->xmlbuf_len);
    if(pstCtx->xmlBuf == NULL)
    {
        pstCtx->status = 500;
        return -1;
    }

    memset(pstCtx->xmlBuf, 0, pstCtx->xmlbuf_len);

    snprintf(pstCtx->xmlBuf, pstCtx->xmlbuf_len, "<FirmwareUpload>"
    "<UploadProgress fileSize=\"%d\" recvSize=\"%d\" />"
    "</FirmwareUpload>", 
    s_FormFirmwareBoundary.fileSize, s_FormFirmwareBoundary.recvSize);

    return 0;
}

static int web_handle_get_ptz_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    PTZConfig *pstPtzCfg = &pstSystemCfg->ptzCfg;

    pstCtx->xmlBuf = anj_config_system_ptz_conver_xml(pstPtzCfg);
    return 0;
}

static int web_handle_get_ptz_dzoom_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    DZoomConfig *pstDzoomCfg = &pstSystemCfg->ptzCfg.dzoomCfg;

    pstCtx->xmlBuf = anj_config_system_ptz_dzoom_conver_xml(pstDzoomCfg);
    return 0;
}

static int web_handle_get_ptz_preset_list(WebPostContext *pstCtx)
{
    IXML_Document* ptz_conf = NULL;
    ptz_conf = ixmlLoadDocument(CONFIG_PTZ_PATH);

    pstCtx->xmlBuf = ixmlNodetoString(&ptz_conf->n);
    return 0;
}

static int web_handle_get_video_qos_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    VideoQoSConfig *pstVideoQosCfg = &pstSystemCfg->videoQosCfg;

    pstCtx->xmlBuf = anj_config_system_videoq_conver_xml(pstVideoQosCfg);
    return 0;
}

static int web_handle_get_ridge_server_cfg(WebPostContext *pstCtx)
{
    const char *file = "/data/xjcar_algo/config.xml";
    if (access(file, F_OK) != 0)
    {
        pstCtx->status = 500;
        return -1;
    }

    pstCtx->xmlBuf = anj_mw_read_file_buffer(file);

    return 0;
}


static int web_handle_get_audio_prompt_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    AudioPromptConfig *pstAudioPromtCfg = &pstSystemCfg->audioPromptCfg;

    pstCtx->xmlBuf = anj_config_system_audioprompt_conver_xml(pstAudioPromtCfg);
    return 0;
}

static int web_handle_get_tamper_proof_cfg(WebPostContext *pstCtx)
{
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TamperProofConfig *pstTamperProofCfg = &pstSystemCfg->tamperProofCfg;

    pstCtx->xmlBuf = anj_config_system_tamperproof_conver_xml(pstTamperProofCfg);
    return 0;
}

static int web_handle_get_wifi_conn(WebPostContext *pstCtx)
{
    int wifistatus = 0;
    const char *pIfname = net_get_wireless_name();
    if(pIfname != NULL)
    {
        wifistatus = wifi_is_connected(pIfname, NULL);
    }

    pstCtx->xmlBuf = xml_conver_wifi_connect(wifistatus);
    return 0;
}

static int web_handle_get_snapshot(WebPostContext *pstCtx)
{
    int quality = 30;
    int iCameraIdex = 0;
    char filename[128] = {0};
    const char *dir = "/tmp/www";

    snprintf(filename, sizeof(filename), "snapshot.jpg");
    anj_snap_jpg(iCameraIdex, 1, quality, (char *)dir, filename, NULL);
    
    char pathname[256] = {0};
    snprintf(pathname, sizeof(pathname), "%s/%s", dir, filename);
    if (0 == anj_snap_wait_complete(pathname, 1000))
    {
        anj_mw_system("cd /tmp/www/;tar -zcvf snapshot.tgz snapshot.jpg");
        usleep(10 * 1000);
        pstCtx->status = 200;
    }
    else
    {
        pstCtx->status = 0;
    }

    return 0;
}

static int web_handle_get_snapshot0(WebPostContext *pstCtx)
{
    int quality = 30;
    int iCameraIdex = 0;
    char filename[128] = {0};
    const char *dir = "/tmp/www";

    snprintf(filename, sizeof(filename), "snapshot0.jpg");
    anj_snap_jpg(iCameraIdex, 1, quality, (char *)dir, filename, NULL);
    
    char pathname[256] = {0};
    snprintf(pathname, sizeof(pathname), "%s/%s", dir, filename);
    if (0 == anj_snap_wait_complete(pathname, 1000))
    {
        pstCtx->status = 200;
    }
    else
    {
        pstCtx->status = 0;
    }

    return -1;
}

static int parse_ptz_speed_by_xml(const char *soapBody, int *pPanSpeed, int *pTiltSpeed)
{
    const char *pTag = NULL;

    if (soapBody == NULL || pPanSpeed == NULL || pTiltSpeed == NULL)
    {
        return -1;
    }

    pTag = strstr(soapBody, "<panspeed>");
    if (pTag == NULL)
    {
        return -1;
    }
    *pPanSpeed = atoi(pTag + strlen("<panspeed>"));

    pTag = strstr(soapBody, "<tiltspeed>");
    if (pTag == NULL)
    {
        return -1;
    }
    *pTiltSpeed = atoi(pTag + strlen("<tiltspeed>"));

    return 0;
}

static int web_handle_get_snap_data(WebPostContext *pstCtx)
{
    int cameraIndex = 1;
    int quality = 80;
    const char *dir = "/tmp";
    const char *filename = "mqtt_snap.jpg";
    char pathname[256] = {0};
    unsigned long long int fileLen = 0;
    unsigned char *jpgBuf = NULL;
    char *base64Buf = NULL;
    int base64Len = 0;

    anj_snap_jpg(cameraIndex, 1, quality, (char *)dir, (char *)filename, NULL);

    snprintf(pathname, sizeof(pathname), "%s/%s", dir, filename);
    if (anj_snap_wait_complete(pathname, 1000) != 0)
    {
        pstCtx->status = 500;
        return -1;
    }

    if (anj_mw_read_file_len(pathname, &fileLen) != 0 || fileLen == 0)
    {
        pstCtx->status = 500;
        return -1;
    }

    jpgBuf = (unsigned char *)anj_mw_malloc((size_t)fileLen);
    if (jpgBuf == NULL)
    {
        pstCtx->status = 500;
        return -1;
    }

    if (anj_mw_read_file(pathname, (char *)jpgBuf, &fileLen) != 0)
    {
        anj_mw_free(jpgBuf);
        pstCtx->status = 500;
        return -1;
    }

    base64Len = (int)fileLen / 3 * 4 + ((int)fileLen % 3 != 0) * 4 + 1;
    base64Buf = (char *)anj_mw_malloc(base64Len);
    if (base64Buf == NULL)
    {
        anj_mw_free(jpgBuf);
        pstCtx->status = 500;
        return -1;
    }

    memset(base64Buf, 0, base64Len);
    Base64Encode(jpgBuf, (int)fileLen, base64Buf);
    anj_mw_free(jpgBuf);

    pstCtx->xmlBuf = base64Buf;
    pstCtx->status = 200;
    return 0;
}

static int web_handle_set_logout(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web logout sessionid:%s\n", pstCtx->pSoapBody);

    int iRet = -1;
    iRet = UserAuthLogout(pstCtx->pSoapBody);
    if(iRet != 0)
    {
        __ERR("web logout error, ret:%d\n", iRet);
        pstCtx->status = 404;
        return -1;
    }

    return 0;
}

static int web_handle_set_network_lan_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    LANConfig stLanNewCfg = {0};
    anj_config_network_lan_get_by_xml(&stLanNewCfg, pstCtx->pSoapBody, 1);
    anj_config_network_lan_set(&stLanNewCfg);

    return 0;
}

static int web_handle_set_wifi_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    WIFIConfig stWifiCfg = {0};
    anj_config_network_wifi_get_by_xml(&stWifiCfg, pstCtx->pSoapBody);

    anj_config_network_wifi_set(&stWifiCfg);

    anj_net_reset();
    return 0;
}

static int web_handle_set_wifiap_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    WIFIApConfig stWifiApCfg = {0};
    anj_config_network_wifiap_get_by_xml(&stWifiApCfg, pstCtx->pSoapBody);

    anj_config_network_wifiap_set(&stWifiApCfg);

    anj_net_reset();
    return 0;
}

static int web_handle_set_network_p2p_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    P2PConfig stOldP2pCfg = pstNetworkConfig->p2pCfg;
    P2PConfig stP2pCfg = stOldP2pCfg;
    if (anj_config_network_p2p_get_by_xml(&stP2pCfg, pstCtx->pSoapBody) != 0)
    {
        pstCtx->status = 400;
        return -1;
    }

    if (anj_config_network_p2p_set(&stP2pCfg) == 0 &&
        memcmp(&stOldP2pCfg, &stP2pCfg, sizeof(P2PConfig)) != 0)
    {
        __INFO("p2p config changed, restart anj_ser\n");
        module_uninit_single("anj_ser");
        usleep(10 * 1000);
        module_init_single("anj_ser");
    }
    return 0;
}

static int web_handle_set_network_alarmserver_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmServerConfig stAlarmServerCfg = {0};
    anj_config_network_alarmserver_get_by_xml(&stAlarmServerCfg, pstCtx->pSoapBody);

    anj_config_network_alarmserver_set(&stAlarmServerCfg);
    return 0;
}

static int web_handle_set_ftp_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    ServerConfig *pstServerCfg = (ServerConfig *)anj_mw_malloc(sizeof(ServerConfig));
    if (pstServerCfg == NULL)
    {
        pstCtx->status = 500;
        return -1;
    }

    if (anj_config_server_ftp_get_by_xml(pstServerCfg, pstCtx->pSoapBody) != 0)
    {
        pstCtx->status = 400;
        anj_mw_free(pstServerCfg);
        pstServerCfg = NULL;
        return -1;
    }

    FtpServerList stFtpList = {0};
    memcpy(stFtpList.ftpServers, pstServerCfg->ftpServers, sizeof(stFtpList.ftpServers));
    anj_config_server_ftp_set(&stFtpList);
    anj_mw_free(pstServerCfg);
    pstServerCfg = NULL;

    return 0;
}

static int web_handle_set_smtp_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }
    ServerConfig *pstServerCfg = (ServerConfig *)anj_mw_malloc(sizeof(ServerConfig));
    if (pstServerCfg == NULL)
    {
        pstCtx->status = 500;
        return -1;
    }

    anj_config_server_smtp_get_by_xml(pstServerCfg, pstCtx->pSoapBody);

    anj_config_server_smtp_list_set(&pstServerCfg->smtpServers);

    anj_mw_free(pstServerCfg);
    pstServerCfg = NULL;

    return 0;
}

static int web_handle_set_test_smtp_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    ServerConfig *pstServerCfg = (ServerConfig *)anj_mw_malloc(sizeof(ServerConfig));
    if (pstServerCfg == NULL)
    {
        pstCtx->status = 500;
        return -1;
    }

    anj_config_server_smtp_get_by_xml(pstServerCfg, pstCtx->pSoapBody);

    if (anj_ftpemail_smtp_test(&pstServerCfg->smtpServers, SMTP_INDEX_FOR_ALARM_UPLOAD, NULL) != 0)
    {
        pstCtx->status = 500;
        anj_mw_free(pstServerCfg);
        pstServerCfg = NULL;
        return -1;
    }

    anj_mw_free(pstServerCfg);
    pstServerCfg = NULL;

    return 0;
}

static int web_handle_set_test_ftp_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    ServerConfig *pstServerCfg = (ServerConfig *)anj_mw_malloc(sizeof(ServerConfig));
    if (pstServerCfg == NULL)
    {
        pstCtx->status = 500;
        return -1;
    }

    anj_config_server_ftp_get_by_xml(pstServerCfg, pstCtx->pSoapBody);

    FtpServer *pstFtpCfg = anj_config_server_ftp_get_by_id(pstServerCfg, FTP_INDEX_FOR_ALARM_UPLOAD);
    if (pstFtpCfg == NULL)
    {
        pstCtx->status = 404;
        return -1;
    }

    char IPAddress[MAX_IP_NAME_LEN] = {0};
    struct NET_CONFIG netcfg = {0};
    if (Check_Link_Status(WIRE_INTERFACE_NAME))
    {
        net_get_info(WIRE_INTERFACE_NAME, &netcfg);
        get_ip_str(netcfg.ifaddr, IPAddress, MAX_IP_NAME_LEN);
    }
    else
    {
        char szWifiInterface[16] = {0};
        StrCpy(szWifiInterface, sizeof(szWifiInterface), WIFI_INTERFACE_NAME);

        if( is_network_device_exist(szWifiInterface) && is_network_interface_up(szWifiInterface) )
        {
            net_get_info(szWifiInterface, &netcfg);         
            get_ip_str(netcfg.ifaddr, IPAddress, MAX_IP_NAME_LEN);
        }
        else
        {
            net_get_info(WIRE_INTERFACE_NAME, &netcfg);
            get_ip_str(netcfg.ifaddr, IPAddress, MAX_IP_NAME_LEN);
        }
    }

    __INFO("ftp test: server ip:%s, port:%d, username:%s, password:%s, ipaddress:%s\n",
        pstFtpCfg->serverIP, pstFtpCfg->serverPort,
        pstFtpCfg->userName, pstFtpCfg->password,
        IPAddress);

    if (anj_ftpemail_ftp_test(pstFtpCfg, IPAddress) != 0)
    {
        pstCtx->status = 500;
        anj_mw_free(pstServerCfg);
        pstServerCfg = NULL;
        return -1;
    }

    anj_mw_free(pstServerCfg);
    pstServerCfg = NULL;

    return 0;
}

static int web_handle_set_media_stream_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    MediaStreamConfig *pstMediaStreamOldCfg = (MediaStreamConfig *)getMediaStreamConfig();

    anj_config_stream_get_by_xml(pstMediaStreamOldCfg, pstCtx->pSoapBody, 1);
    anj_config_stream_set(pstMediaStreamOldCfg);

    return 0;
}

static int web_handle_set_platform_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    PlatformConfig *pstPlatformCfg = (PlatformConfig *)getPlatformConfig();
    anj_config_platform_get_by_xml(pstPlatformCfg, pstCtx->pSoapBody);
    anj_config_platform_set(pstPlatformCfg);

    return 0;
}

static int web_handle_set_gb28181_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    GB28181Config *pstGb28181Cfg = (GB28181Config *)getGb28181Config();
    anj_config_gb28181_get_by_xml(pstGb28181Cfg, pstCtx->pSoapBody);
    anj_config_gb28181_set(pstGb28181Cfg);

    return 0;
}

static int web_handle_set_gat1400_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    GAT1400Config *pstGAT1400Config = (GAT1400Config *)getGat1400Config();
    anj_config_gat1400_get_by_xml(pstGAT1400Config, pstCtx->pSoapBody);
    anj_config_gat1400_set(pstGAT1400Config);

    return 0;
}

static int web_handle_set_rtmp_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig;
    parse_rtmp_config_by_xml(&pstMediaStreamCfg->rtmpConfig, pstCtx->pSoapBody);

    anj_config_stream_set(pstMediaStreamCfg);
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_RTMP_RESTART, &event_result, NULL);
    return 0;
}

static int web_handle_set_video_capture_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    VideoCaptureCfg *pstVideoCapCfg = (VideoCaptureCfg *)anj_mw_malloc(sizeof(VideoCaptureCfg));
    memset(pstVideoCapCfg, 0, sizeof(VideoCaptureCfg));

    anj_config_video_capture_get_by_xml(pstVideoCapCfg, pstCtx->pSoapBody, 0);

    int iCameraIdx = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_video_capture_set(pstVideoCapCfg, iCameraIdx);
    }

    anj_mw_free(pstVideoCapCfg);
    pstVideoCapCfg = NULL;

    return 0;
}

static int web_handle_set_video_picture_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    JpegEncodeCfg *pstJpegEncodeCfg = (JpegEncodeCfg *)anj_mw_malloc(sizeof(JpegEncodeCfg));
    memset(pstJpegEncodeCfg, 0, sizeof(JpegEncodeCfg));

    anj_config_jpeg_encode_get_by_xml(pstJpegEncodeCfg, pstCtx->pSoapBody);

    int iCameraIdx = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_jpeg_encode_set(pstJpegEncodeCfg, iCameraIdx);
    }

    anj_mw_free(pstJpegEncodeCfg);
    pstJpegEncodeCfg = NULL;

    return 0;
}

static int web_handle_set_led_ctrl(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if(strlen(pstCtx->pSoapBody) <= 0)
    {
        pstCtx->status = 403;
        return -1;
    }

    if(!strcmp(pstCtx->pSoapBody, "KeepOn"))
    {
        anj_ispctl_light_manual_ctrl(LIGHT_INDEX_WLED, 100);
        anj_ispctl_light_manual_ctrl(LIGHT_INDEX_RLED, 100);
    }
    else if(!strcmp(pstCtx->pSoapBody, "KeepOff"))
    {
        anj_ispctl_light_manual_ctrl(LIGHT_INDEX_WLED, 0);
        anj_ispctl_light_manual_ctrl(LIGHT_INDEX_RLED, 0);
    }

    return 0;
}

static int web_handle_set_ipeye_ctrl(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if(strlen(pstCtx->pSoapBody) <= 0)
    {
        pstCtx->status = 403;
        return -1;
    }

    if(!strcmp(pstCtx->pSoapBody, "KeepOn"))
    {
        anj_mw_system("touch /tmp/IPEYE_UP");
        __DBG("ipeye keep on\n");
    }
    else if(!strcmp(pstCtx->pSoapBody, "KeepOff"))
    {
        anj_mw_system("rm /tmp/IPEYE_UP");
        __DBG("ipeye keep off\n");
    }

    return 0;
}

static int web_handle_set_manual_iruct_ctrl(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if(strlen(pstCtx->pSoapBody) <= 0)
    {
        pstCtx->status = 403;
        return -1;
    }

    int cameraIndex = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoConfig stVideoConfigArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stVideoConfigArray, pstMediaConfig->videoConfig, sizeof(VideoConfig) * ANJ_CAMERA_MAX_NUMS);

    if(stVideoConfigArray[cameraIndex].videoCapture.ircut_mode == IRCUT_Mode_Manual)
    {
        if(!strcmp(pstCtx->pSoapBody, "DayMode"))
        {
            anj_ispctl_ircut_manual_ctrl(1, 0);
        }
        else if(!strcmp(pstCtx->pSoapBody, "NightMode"))
        {
            anj_ispctl_ircut_manual_ctrl(0, 0);
        }
    }
    else
    {

        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            stVideoConfigArray[cameraIndex].videoCapture.ircut_mode = IRCUT_Mode_Manual;
        }
        anj_config_video_set(stVideoConfigArray);

        pstCtx->status = 403;
        return -1;
    }

    return 0;
}

static int web_handle_set_video_encode_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    VideoEncode *pstVideoEncode = (VideoEncode *)anj_mw_malloc(sizeof(VideoEncode));
    memset(pstVideoEncode, 0, sizeof(VideoEncode));

    anj_config_video_encode_get_by_xml(pstVideoEncode, pstCtx->pSoapBody);

    int iCameraIdx = 0;
    int iNeedSwitch = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        iNeedSwitch |= anj_config_video_encode_set(pstVideoEncode, iCameraIdx);
    }
    if (iNeedSwitch)
    {
        anj_video_encode_switch();
    }

    anj_mw_free(pstVideoEncode);
    pstVideoEncode = NULL;

    return 0;
}

static int web_handle_set_video_mask_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    VideoMaskConfig *pstVideoMaskCfg = (VideoMaskConfig *)anj_mw_malloc(sizeof(VideoMaskConfig));
    memset(pstVideoMaskCfg, 0, sizeof(VideoMaskConfig));

    anj_config_video_mask_get_by_xml(pstVideoMaskCfg, pstCtx->pSoapBody);

    int iCameraIdx = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_video_mask_set(pstVideoMaskCfg, iCameraIdx);
    }

    anj_mw_free(pstVideoMaskCfg);
    pstVideoMaskCfg = NULL;

    return 0;
}

static int web_handle_set_video_roi_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    VideoROI *pstVideoRoi = (VideoROI *)anj_mw_malloc(sizeof(VideoROI));
    memset(pstVideoRoi, 0, sizeof(VideoROI));

    anj_config_video_roi_get_by_xml(pstVideoRoi, pstCtx->pSoapBody);

    int iCameraIdx = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_video_roi_set(pstVideoRoi, iCameraIdx);
    }

    anj_mw_free(pstVideoRoi);
    pstVideoRoi = NULL;

    return 0;
}

static int web_handle_set_video_yuv_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    YuvEncodeCfg *pstYuvEncodeCfg = (YuvEncodeCfg *)anj_mw_malloc(sizeof(YuvEncodeCfg));
    memset(pstYuvEncodeCfg, 0, sizeof(YuvEncodeCfg));

    anj_config_video_yuv_get_by_xml(pstYuvEncodeCfg, pstCtx->pSoapBody);

    int iCameraIdx = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_video_yuv_set(pstYuvEncodeCfg, iCameraIdx);
    }

    anj_mw_free(pstYuvEncodeCfg);
    pstYuvEncodeCfg = NULL;

    return 0;
}

static int web_handle_set_video_user_overlay(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    VideoUserOverlay *pstVideoUserOverlay = (VideoUserOverlay *)anj_mw_malloc(sizeof(VideoUserOverlay));
    memset(pstVideoUserOverlay, 0, sizeof(VideoUserOverlay));

    anj_config_user_overlay_get_by_xml(pstVideoUserOverlay, pstCtx->pSoapBody);

    int iCameraIdx = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_user_overlay_set(pstVideoUserOverlay, iCameraIdx);
    }

    anj_mw_free(pstVideoUserOverlay);
    pstVideoUserOverlay = NULL;

    return 0;
}


static int web_handle_set_video_overlay_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iCameraIdx = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoOverlay *pstVideoOverlay = &pstMediaCfg->videoConfig[iCameraIdx].overlay;

    anj_config_overlay_get_by_xml(pstVideoOverlay, pstCtx->pSoapBody);
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_overlay_set(pstVideoOverlay, iCameraIdx);
    }

    return 0;
}

static int web_handle_set_audio_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *pstAuidoCfg = &pstMediaCfg->audioConfig;

    anj_config_audio_get_by_xml(pstAuidoCfg, pstCtx->pSoapBody);
    anj_config_audio_set(pstAuidoCfg);

    return 0;
}

static int web_handle_set_system_user_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    UserConfig *pstUserCfg = (UserConfig *)anj_mw_malloc(sizeof(UserConfig));
    memset(pstUserCfg, 0, sizeof(UserConfig));

    anj_config_system_user_get_by_xml(pstUserCfg, pstCtx->pSoapBody);
    anj_config_system_user_set(pstUserCfg);

    anj_mw_free(pstUserCfg);
    pstUserCfg = NULL;

    return 0;
}

static int web_handle_set_system_log_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SyslogConfig *pstSyslogCfg = (SyslogConfig *)anj_mw_malloc(sizeof(SyslogConfig));
    memset(pstSyslogCfg, 0, sizeof(SyslogConfig));

    anj_config_system_syslog_get_by_xml(pstSyslogCfg, pstCtx->pSoapBody);
    anj_config_system_syslog_set(pstSyslogCfg);

    anj_mw_free(pstSyslogCfg);
    pstSyslogCfg = NULL;

    return 0;
}

static int web_handle_set_timer_maintain_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    MaintainConfig *pstMaintainCfg = &pstSystemCfg->maintainCfg;

    MaintainConfig stMaintainNewCfg = {0};
    anj_config_system_maintain_get_by_xml(&stMaintainNewCfg, pstCtx->pSoapBody);

    pstMaintainCfg->enable      = stMaintainNewCfg.enable;
    pstMaintainCfg->day         = stMaintainNewCfg.day;
    pstMaintainCfg->time.hour   = stMaintainNewCfg.time.hour;
    pstMaintainCfg->time.minute = stMaintainNewCfg.time.minute;
    pstMaintainCfg->time.sec    = stMaintainNewCfg.time.sec;
    anj_config_system_maintain_set(pstMaintainCfg);

    return 0;
}

static int web_handle_set_misc_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    MiscConfig *pstMisCfg = &pstSystemCfg->miscCfg;

    char language[MAX_LANGUAGE_LEN] = {0};
    if(parse_misc_config_language_by_xml(pstCtx->pSoapBody,language) == -1)
    {
        pstCtx->status = 400;
        return -1;
    }

    memset(pstMisCfg->language, 0, MAX_LANGUAGE_LEN);
    StrCpy(pstMisCfg->language, MAX_LANGUAGE_LEN, language);
    anj_config_system_misc_set(pstMisCfg);

    return 0;
}

static int web_handle_set_record_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int cameraIndex = 0;

    RecordConfig *pstRecordConfig = (RecordConfig *)getRecordConfig();
    RecordConfig stRecordConfigArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stRecordConfigArray, pstRecordConfig, sizeof(RecordConfig) * ANJ_CAMERA_MAX_NUMS);
    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        iRet |= anj_config_record_get_by_xml(&stRecordConfigArray[cameraIndex], pstCtx->pSoapBody, cameraIndex);
    }
    if (iRet == 0)
    {
        iRet = anj_config_record_set(stRecordConfigArray);
    }

    return 0;
}

static int web_handle_set_motion_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int iCameraIdx = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm stMotionDetectAlarmArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stMotionDetectAlarmArray, pstAlarmCfg->normalAlarm.motionDetectAlarm,
           sizeof(MotionDetectAlarm) * ANJ_CAMERA_MAX_NUMS);

    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        iRet = anj_config_alarm_motion_get_by_xml(&stMotionDetectAlarmArray[iCameraIdx], pstCtx->pSoapBody, iCameraIdx);
    }
    if (iRet == 0)
    {
        iRet = anj_config_alarm_motion_set(stMotionDetectAlarmArray);
    }

    return 0;
}

static int web_handle_set_video_lost_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    VideoLostAlarm stVideoLostAlarmArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stVideoLostAlarmArray, pstAlarmConfig->normalAlarm.videoLostAlarm,
           sizeof(VideoLostAlarm) * ANJ_CAMERA_MAX_NUMS);

    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        iRet = anj_config_alarm_video_lost_get_by_xml(&stVideoLostAlarmArray[cameraIndex], pstCtx->pSoapBody, cameraIndex);
    }
    if (iRet == 0)
    {
        iRet = anj_config_alarm_video_lost_set(stVideoLostAlarmArray);
    }

    return 0;
}

static int web_handle_set_input_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    InputAlarm *pstInputAlarm = &pstAlarmCfg->normalAlarm.inputAlarm;

    anj_config_alarm_input_get_by_xml(pstInputAlarm, pstCtx->pSoapBody);
    anj_config_alarm_input_set(pstInputAlarm);

    return 0;
}

static int web_handle_set_manual_input(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    return 0;
}

static int web_handle_set_output(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    OutPutAlarm *pstOutputAlarm = &pstAlarmCfg->normalAlarm.outputAlarm;

    anj_config_alarm_output_get_by_xml(pstOutputAlarm, pstCtx->pSoapBody);
    anj_config_alarm_output_set(pstOutputAlarm);

    return 0;
}

static int web_handle_set_sms_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    SMSAlarm *pstSmsAlarm = &pstAlarmCfg->normalAlarm.smsAlarm;

    anj_config_alarm_sms_get_by_xml(pstSmsAlarm, pstCtx->pSoapBody);
    anj_config_alarm_sms_set(pstSmsAlarm);

    return 0;
}

static int web_handle_set_storage_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    StorageFullAlarm *pstStorageFullAlarm = &pstAlarmCfg->normalAlarm.storageFullAlarm;

    anj_config_alarm_storage_full_get_by_xml(pstStorageFullAlarm, pstCtx->pSoapBody);
    anj_config_alarm_storage_full_set(pstStorageFullAlarm);

    return 0;
}

static int web_handle_set_audio_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    AudioAlarm *pstAudioAlarm = &pstAlarmCfg->aiAlarm.audioAlarm;

    anj_config_alarm_audio_get_by_xml(pstAudioAlarm, pstCtx->pSoapBody);
    anj_config_alarm_audio_set(pstAudioAlarm);

    return 0;
}

static int web_handle_set_video_gate_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stVideoGateAlarmArray, pstAlarmConfig->aiAlarm.vgAlarm, sizeof(VideoGateAlarm) * ANJ_CAMERA_MAX_NUMS);
    
    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        iRet = anj_config_alarm_video_gate_get_by_xml(&stVideoGateAlarmArray[cameraIndex], pstCtx->pSoapBody, cameraIndex);
    }
    if (iRet == 0)
    {
        iRet = anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
    }

    return 0;
}

static int web_handle_set_pd_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stPdAlarmArray, pstAlarmConfig->aiAlarm.pdAlarm, sizeof(PdAlarm) * ANJ_CAMERA_MAX_NUMS);
    
    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        iRet = anj_config_alarm_pd_get_by_xml(&stPdAlarmArray[cameraIndex], pstCtx->pSoapBody, cameraIndex);
    }
    if (iRet == 0)
    {
        iRet = anj_config_alarm_pd_set(stPdAlarmArray);
    }

    return 0;
}

static int web_handle_set_video_cover_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    VideoCoverAlarm stVideoCoverAlarmArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stVideoCoverAlarmArray, pstAlarmConfig->normalAlarm.videoCoverAlarm, sizeof(VideoCoverAlarm) * ANJ_CAMERA_MAX_NUMS);
    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        iRet = anj_config_alarm_video_cover_get_by_xml(&stVideoCoverAlarmArray[cameraIndex], pstCtx->pSoapBody, cameraIndex);
    }

    if (iRet == 0)
    {
        iRet = anj_config_alarm_video_cover_set(stVideoCoverAlarmArray);
    }

    return 0;
}

static int web_handle_set_lpr_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    LprAlarm stLprAlarmArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stLprAlarmArray, pstAlarmConfig->aiAlarm.lprAlarm, sizeof(LprAlarm) * ANJ_CAMERA_MAX_NUMS);

    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        iRet = anj_config_alarm_lpr_get_by_xml(&stLprAlarmArray[cameraIndex], pstCtx->pSoapBody, cameraIndex);
    }

    if (iRet == 0)
    {
        iRet = anj_config_alarm_lpr_set(stLprAlarmArray);
    }

    return 0;
}

static int web_handle_set_flame_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FlameAndFlumesAlarm *pstFlameAlarm = &pstAlarmCfg->aiAlarm.fireAlarm;

    anj_config_alarm_fire_get_by_xml(pstFlameAlarm, pstCtx->pSoapBody);
    anj_config_alarm_fire_set(pstFlameAlarm);

    return 0;
}

static int web_handle_set_fd_alarm(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = 0;
    int cameraIndex = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm stFdAlarmArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stFdAlarmArray, pstAlarmConfig->aiAlarm.fdAlarm, sizeof(FaceDetectAlarm) * ANJ_CAMERA_MAX_NUMS);

    for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        iRet = anj_config_alarm_fd_get_by_xml(&stFdAlarmArray[cameraIndex], pstCtx->pSoapBody, cameraIndex);
    }

    if (iRet == 0)
    {
        iRet = anj_config_alarm_fd_set(stFdAlarmArray);
    }

    return 0;
}

static int web_handle_set_temp_humidity(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    TempHumidityAlarm *pstTemphumidity = &pstAlarmCfg->normalAlarm.temphumidityAlarm;

    anj_config_alarm_temp_humidity_get_by_xml(pstTemphumidity, pstCtx->pSoapBody);
    anj_config_alarm_temphumidity_set(pstTemphumidity);

    return 0;
}

static int web_handle_set_video_region(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pstRegionAlarm = &pstAlarmCfg->aiAlarm.regionAiAlarm[iCameraIdx];

    anj_config_alarm_region_get_by_xml(pstRegionAlarm, pstCtx->pSoapBody, iCameraIdx);
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_config_alarm_region_set(pstRegionAlarm);
    }

    return 0;
}

static int web_handle_set_time_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TimeConfig *pstTimeCfg = &pstSystemCfg->timeCfg;

    anj_config_system_time_get_by_xml(pstTimeCfg, pstCtx->pSoapBody);
    __INFO("web set time config mode:%s, timezone:%s, ntp serverip:%s, port:%d!n",
        pstTimeCfg->timeMode.modeName, pstTimeCfg->timeZone,
        pstTimeCfg->ntpConfig.serverIP, pstTimeCfg->ntpConfig.serverPort);

    if(strncmp(pstTimeCfg->timeMode.modeName, "MANUAL",strlen("MANUAL")) == 0 )
    {
        struct tm time;
        if(parse_system_tm_by_xml(&time, pstCtx->pSoapBody) == 0)
        {
            anj_systime_set_time_and_zone(time, pstTimeCfg->timeZone, 1);
        }
    }

    return 0;
}

static int web_handle_set_add_user_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    char myusername[100] = {0};
    char mypassword[100] = {0};
    char mygroup[20] = {0};
    char mystatus[10] = {0};
    //char mysecureLogin[10] = {0};

    if(parse_account_info_by_xml(pstCtx->pSoapBody, myusername, mypassword, mygroup, mystatus) == -1)
    {
        pstCtx->status = 400;
        return -1;
    }

    UserAuthAddUser(myusername, mypassword, mygroup, mystatus);
    return 0;
}

static int web_handle_set_edit_user_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    char myusername[100] = {0};
    char mypassword[100] = {0};
    char mygroup[20] = {0};
    char mystatus[10] = {0};
    char mysecureLogin[10] = {0};

    if(parse_account_info_by_xml(pstCtx->pSoapBody, myusername, mypassword, mygroup, mystatus) == -1)
    {
        pstCtx->status = 400;
        return -1;
    }

    UserAuthEditUser(myusername, mypassword, mygroup, mystatus,mysecureLogin);
    return 0;
}

static int web_handle_set_delete_user_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    char myusername[100] = {0};
    char mypassword[100] = {0};
    char mygroup[20] = {0};
    char mystatus[10] = {0};
    //char mysecureLogin[10] = {0};

    if(parse_account_info_by_xml(pstCtx->pSoapBody, myusername, mypassword, mygroup, mystatus) == -1)
    {
        pstCtx->status = 400;
        return -1;
    }

    UserAuthDeleteUser(myusername);
    return 0;
}

static int web_handle_set_ptz_cmd(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if (strlen(pstCtx->pSoapBody) == 0)
    {
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web ptz ctrl cmd:%s\n", pstCtx->pSoapBody);
    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    StrCpy(stPtzCmdParse.ptzCmd, sizeof(stPtzCmdParse.ptzCmd), pstCtx->pSoapBody);
    eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);

    return 0;
}

static int web_handle_set_ptz_3d_locate(WebPostContext *pstCtx)
{
    int panSpeed = 0;
    int tiltSpeed = 0;

    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if (parse_ptz_speed_by_xml(pstCtx->pSoapBody, &panSpeed, &tiltSpeed) != 0)
    {
        __ERR("parse ptz3dlocate body failed: %s\n", pstCtx->pSoapBody);
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web ptz3dlocate panspeed:%d tiltspeed:%d\n", panSpeed, tiltSpeed);

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    strncpy(stPtzCmdParse.ptzCmd, "ptz3dlocate", sizeof(stPtzCmdParse.ptzCmd) - 1);
    stPtzCmdParse.posX = tiltSpeed;
    stPtzCmdParse.posY = panSpeed;
    eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);

    return 0;
}

static int web_handle_set_storage_device_format(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if (strlen(pstCtx->pSoapBody) == 0)
    {
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web storage device format path:%s\n", pstCtx->pSoapBody);
    anj_sdcard_format(0);

    return 0;
}

static int web_handle_set_storage_device_mount(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if (strlen(pstCtx->pSoapBody) == 0)
    {
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web storage device mount path:%s\n", pstCtx->pSoapBody);
    int iRet = anj_sdcard_mount();
    if (iRet != 0)
    {
        __ERR("mount failed:%d!\n", iRet);
        pstCtx->status = 404;
        return -1;
    }

    return 0;
}

static int web_handle_set_storage_device_unmount(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    if (strlen(pstCtx->pSoapBody) == 0)
    {
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web storage device mount path:%s\n", pstCtx->pSoapBody);
    int iRet = anj_sdcard_umount();
    if (iRet != 0)
    {
        __ERR("unmount failed:%d!\n", iRet);
        pstCtx->status = 404;
        return -1;
    }

    return 0;
}


static int web_handle_set_log_file_delete(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = delete_log_file(pstCtx->pSoapBody);
    if (iRet == -1)
    {
        pstCtx->status = 404;
        return -1;
    }

    return 0;
}

static int web_handle_set_file_delete(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int iRet = delete_mount_path_file(pstCtx->pSoapBody);
    if (iRet == -1)
    {
        pstCtx->status = 404;
        return -1;
    }

    return 0;
}

static int web_handle_set_ptz_common_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    PTZCommonConfig *pstPtzCommonCfg = &pstSystemCfg->ptzCfg.commonCfg;

    anj_config_system_ptz_common_get_by_xml(pstPtzCommonCfg, pstCtx->pSoapBody);
    anj_config_system_ptz_comm_set(pstPtzCommonCfg);

    return 0;
}

static int web_handle_set_ptz_af_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    AfConfig *pstAfCfg = &pstSystemCfg->ptzCfg.afCfg;

    anj_config_system_ptz_af_get_by_xml(pstAfCfg, pstCtx->pSoapBody);
    anj_config_system_ptz_af_set(pstAfCfg);

    return 0;
}

static int web_handle_set_ptz_dzoom_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    DZoomConfig *pstDzoomCfg = &pstSystemCfg->ptzCfg.dzoomCfg;

    anj_config_system_ptz_dzoom_get_by_xml(pstDzoomCfg, pstCtx->pSoapBody);
    anj_config_system_ptz_dzoom_set(pstDzoomCfg);

    return 0;
}

static int web_handle_set_ptz_advance_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    PTZAdvanceConfig *pstPtzAdvanceCfg = &pstSystemCfg->ptzCfg.advanceCfg;

    anj_config_system_ptz_advance_get_by_xml(pstPtzAdvanceCfg, pstCtx->pSoapBody);
    anj_config_system_ptz_advance_set(pstPtzAdvanceCfg);

    return 0;
}

static int web_handle_set_video_qos_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    VideoQoSConfig *pstVideoQosCfg = &pstSystemCfg->videoQosCfg;

    anj_config_system_videoq_get_by_xml(pstVideoQosCfg, pstCtx->pSoapBody);
    anj_config_system_videoq_set(pstVideoQosCfg);

    return 0;
}

static int web_handle_set_audio_prompt_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    AudioPromptConfig *pstAudioPromptCfg = &pstSystemCfg->audioPromptCfg;

    anj_config_system_audioprompt_get_by_xml(pstAudioPromptCfg, pstCtx->pSoapBody);
    anj_config_system_audioprompt_set(pstAudioPromptCfg);

    return 0;
}

static int web_handle_set_tamper_proof_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TamperProofConfig *pstCfg = &pstSystemCfg->tamperProofCfg;

    anj_config_system_tamperproof_get_by_xml(pstCfg, pstCtx->pSoapBody);
    anj_config_system_tamperproof_set(pstCfg);

    return 0;
}

static int web_handle_set_mtu_value(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    char mtu[32] = {0};
    get_xml_value(pstCtx->pSoapBody, mtu, "Value");

    int mtu_value = -1;
    mtu_value = atoi(mtu);
    if(mtu_value != -1)
    {
        const char *ifname  = WIRE_INTERFACE_NAME;
        int iRet = net_set_mtu(ifname, mtu_value);
        __INFO("web set mtu value:%d %s!\n", mtu_value, (iRet == 0) ? "success" : "failed");
    }

    return 0;
}

static int web_handle_set_ridge_server_cfg(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    write_buffer_to_file("/data/xjcar_algo/config.xml", pstCtx->pSoapBody, strlen(pstCtx->pSoapBody) + 1);
    return 0;
}


static int web_handle_set_ptz_3d_coordinate(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int StartX = 0;
    int StartY = 0;
    int EndX = 0;
    int EndY = 0;

    parse_coordinate_by_xml(pstCtx->pSoapBody, &StartX, &StartY, &EndX, &EndY);
    // todo

    return 0;
}

int web_upgrade_thread(void *ctx, int *bStart)
{
    const char *web_upgrade_file = "/tmp/FirmwareUpgrade.bin";

    APPBIN_UPDATE_DATA updateData = {0};
    snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", web_upgrade_file);
    updateData.nPhyAddr = 0;
    updateData.nFileLen = 0;

    anj_sysmng_app_update(&updateData);

    return 0;
}

static int web_handle_set_firmware_upgrade(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    static anj_thread_s stWebUpgradeThread;

    if (stWebUpgradeThread.start == 1)
    {
        __ERR("web upgradeing...\n");
        return -1;
    }

    memset(&stWebUpgradeThread, 0, sizeof(anj_thread_s));
    stWebUpgradeThread.bAutoDestroy = 0;
    strncpy(stWebUpgradeThread.iThreadName, "web_upgrade_thread", sizeof(stWebUpgradeThread.iThreadName) - 1);
    stWebUpgradeThread.iThreadjob.ctx = &stWebUpgradeThread;
    stWebUpgradeThread.iThreadjob.func = web_upgrade_thread;
    int iRet = anj_thread_task_create(&stWebUpgradeThread);
    if (iRet)
    {
        __ERR("web_upgrade_thread create failed\n");
    }

    return iRet;
}

static int web_handle_set_firmware_check_memory(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    s_FormFirmwareBoundary.fileSize = atoi(pstCtx->pSoapBody);

    int filesize = atoi(pstCtx->pSoapBody) / 1024 + 1024;       // 比文件大小多预留1M
    int freemem = anj_sysmng_get_freemem();
    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    int keep_onvif = (pstMediaStreamCfg != NULL && pstMediaStreamCfg->webConfig.enable_onvif != 0);

    __INFO("web upgrade filezie:%d KB, freemem:%d Kb!\n", filesize, freemem);

    /* 对齐 anjpri/onvif_prepare：卸非 HTTP 腾内存；enable_onvif 留 ONVIF，否则留 WEB */
    anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_RTSP);
    anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_GB28181);
    anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_H5LIVE);
    anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_RTMP);
    anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_FTP_EMAIL);
    anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_HIK);
    if (!keep_onvif)
    {
        anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_ONVIF);
        __INFO("skip uninit web: web served by civetweb\n");
    }
    else
    {
        anj_service_provider_uninit_single(ANJ_SERVICE_PROVIDER_WEB);
        __INFO("skip uninit onvif: web served by onvif http\n");
    }
    anj_search_uninit();
    modules_uninit("anj_service", "anj_net");
    anj_mw_system("echo 3 > /proc/sys/vm/drop_caches");
    freemem = anj_sysmng_get_freemem();
    usleep(1 * 1000 * 1000);
    __INFO("now freemem is:%d KB\n", freemem);

    return 0;
}

static int web_handle_set_force_idr(WebPostContext *pstCtx)
{
    if (pstCtx->bIsAdmin == 0 || pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    int streamno = atoi(pstCtx->pSoapBody);
    if((streamno < 0) || (streamno > 1))
    {
        streamno = 0;
    }

    __INFO("web force idr stream:%d!\n", streamno);
    int iCameraIdx = 0;
    for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        anj_video_request_idr(iCameraIdx, streamno);
    }

    return 0;
}

int web_restore_thread(void *ctx, int *bStart)
{
    sleep(1);
    unsigned int reserved_bits = 0;
    __RECORD_LOG_INFO("restore config reserved_bits=%u\n", reserved_bits); 
    anj_sysmng_config_restore(reserved_bits);

    return 0;
}

static int web_handle_system_maintain(WebPostContext *pstCtx)
{
    int iRet = 0;
    char myOrder[100] = {0};

    int value_len = 20 * 1024;
    char *myValue = (char *)anj_mw_malloc(value_len);
    if(myValue == NULL)
    {
        __ERR("myValue malloc error!\n");
        pstCtx->status = 500;
        return -1;
    }
    memset(myValue, 0, value_len);

    pstCtx->status = 200;

    if(parse_system_maintain_by_xml(pstCtx->pSoapBody, myOrder, myValue) == -1)
    {
        __ERR("parseSystemMaintainByXML error\n");
        anj_mw_free(myValue);
        pstCtx->status = 400;
        return -1;
    }

    if(strcmp(myOrder, "ConfigfileDownload") == 0)
    {
        __INFO("-----ConfigfileDownload:%s-----\n", myValue);
        pstCtx->xmlBuf = xml_conver_config_file(myValue);
    }
    else if(strcmp(myOrder, "ConfigRestore") == 0)
    {
        static anj_thread_s stWebRetoreThread;

        if (stWebRetoreThread.start == 1)
        {
            __ERR("web restoreing...\n");
            return -1;
        }

        memset(&stWebRetoreThread, 0, sizeof(anj_thread_s));
        stWebRetoreThread.bAutoDestroy = 0;
        strncpy(stWebRetoreThread.iThreadName, "web_restore_thread", sizeof(stWebRetoreThread.iThreadName) - 1);
        stWebRetoreThread.iThreadjob.ctx = &stWebRetoreThread;
        stWebRetoreThread.iThreadjob.func = web_restore_thread;
        iRet = anj_thread_task_create(&stWebRetoreThread);
        if (iRet)
        {
            __ERR("web_restore_thread create failed\n");
        }
    }
    else if(strcmp(myOrder, "SystemReboot") == 0)
    {
        __WARN("web system reboot service request\n");
        __RECORD_LOG_INFO("web system reboot service request\n");
        anj_sysmng_delay_reboot(1);
    }
    else
    {
        __ERR("post request:%s action SystemMaintain error\n", myOrder);
        pstCtx->status = 400;
    }

    anj_mw_free(myValue);
    myValue = NULL;

    return 0;
}

static int web_handle_query_log_file(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    pstCtx->xmlBuf = xml_conver_config_file(pstCtx->pSoapBody);
    return 0;
}

static int web_handle_download_log_file(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    pstCtx->xmlBuf = xml_conver_config_file(pstCtx->pSoapBody);
    return 0;
}

static int web_handle_p2p_unbind_device(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web unbind p2p!\n");
    anj_ser_unbind();
    return 0;
}

static int web_handle_preset_list(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    //todo
    return 0;
}

static int web_handle_thermal_image_cmd(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    // todo
#if 0
    char *cmd = NULL;
    int cmdLen = 0;
    if(!strcmp(pstCtx->pSoapBody, "default"))
    {
        cmd = ThermalImagerDefault;
        cmdLen = sizeof(ThermalImagerDefault);
    }
    else if(!strcmp(pstCtx->pSoapBody, "add"))
    {
        cmd = ThermalImagerAdd;
        cmdLen = sizeof(ThermalImagerAdd);
    }
    else if(!strcmp(pstCtx->pSoapBody, "sub"))
    {
        cmd = ThermalImagerSub;
        cmdLen = sizeof(ThermalImagerSub);
    }
    else if(!strcmp(pstCtx->pSoapBody, "menu_ok"))
    {
        cmd = ThermalImagerMenuOk;
        cmdLen = sizeof(ThermalImagerMenuOk);
    }
    else if(!strcmp(pstCtx->pSoapBody, "save"))
    {
        cmd = ThermalImagerSave;
        cmdLen = sizeof(ThermalImagerSave);
    }
    else if(!strcmp(pstCtx->pSoapBody, "reboot"))
    {
        cmd = ThermalImagerReboot;
        cmdLen = sizeof(ThermalImagerReboot);
    }
#endif

    return 0;
}

static int web_handle_clear_query_replay_cache(WebPostContext *pstCtx)
{
    //todo
    return 0;
}

static int web_handle_web_upload_configuration(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    //todo
    return 0;
}

static int web_handle_play_audio_action_file(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    __INFO("web play audio:%s\n", pstCtx->pSoapBody);

    const char *pName = strGetFilename(pstCtx->pSoapBody);
    anj_audio_prompt_play(ANJ_MP3_ALARM_PATH, (char *)pName, 1);

    return 0;
}

static int web_handle_stop_audio_action_file(WebPostContext *pstCtx)
{
    if (pstCtx->pSoapBody == NULL)
    {
        pstCtx->status = 400;
        return -1;
    }

    anj_audio_play_file_stop();
    return 0;
}

// 上传 web form 文件前准备
static int web_handle_ready_web_upload_formfile(WebPostContext *pstCtx)
{
    http_upload_form_status_set(0);
    return 0;
}

int web_handle_web_login(WebPostContext *pstWebPostCtx)
{
    pstWebPostCtx->xmlBuf = (char *)anj_mw_malloc(pstWebPostCtx->xmlbuf_len);
    if (pstWebPostCtx->xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc xmlbuf_len:%d failed\n", pstWebPostCtx->xmlbuf_len);
        return -1;
    }

    strncpy(pstWebPostCtx->xmlBuf, http_login_path_get(), pstWebPostCtx->xmlbuf_len);
    return 0;
}

int web_handle_skipidpwd_login(WebPostContext *pstWebPostCtx)
{
    int kcSkipLogin = 0;
    char devUsername[64] = {0};
    char devPassword[128] = {0};

    pstWebPostCtx->xmlBuf = (char *)anj_mw_malloc(pstWebPostCtx->xmlbuf_len);
    if (pstWebPostCtx->xmlBuf == NULL)
    {
        __ERR("xmlBuf malloc xmlbuf_len:%d failed\n", pstWebPostCtx->xmlbuf_len);
        return -1;
    }
    
    UserAuthGetUsernamePassword(devUsername, sizeof(devUsername), devPassword, sizeof(devPassword));
    __DBG("kcSkipIdPwdLogin devUsername:%s, devPassword:%s\n", devUsername, devPassword);

    if(strcmp(devPassword, "123456") == 0)
    {
        kcSkipLogin = 1;
    }

    snprintf(pstWebPostCtx->xmlBuf, pstWebPostCtx->xmlbuf_len, "kcSkipLogin:%d,username:%s,password:%s", kcSkipLogin, devUsername, devPassword);
    return 0;
}

static WebPostHandlerEntry g_handlerTable[] = 
{
    /******************************************************************
     * 标准登录处理
     ******************************************************************/
    {"login",           web_handle_login},
    {"ipcLogin",        web_handle_ipc_login},

    /******************************************************************
     * GET
     ******************************************************************/
    {"getSysBootStatus",        web_handle_get_sys_boot_status},
    {"getIPEYECapability",      web_handle_get_ipeye_cap},
    {"getIPEYEStatus",          web_handle_get_ipeye_status},
    {"getCurrentAllConfig",     web_handle_get_all_config},
    {"getMtuValue",             web_handle_get_mtu},
    {"getSystemControlString",  web_handle_get_sys_control_string},
    {"getDefaultConfigMsg",     web_handle_get_default_cfg},
    {"getTestWebSiteConfigMsg", web_handle_get_test_website},
    {"getNetworkConfig",        web_handle_get_network_cfg},
    {"getAppType",              web_handle_get_app_type},
    {"getIrcutOpenLedDelayOption",  web_handle_get_ircut_led_delay},
    {"getNetworkDefaultConfig", web_handle_get_network_default_cfg},
    {"getSystemVersionInfo",    web_handle_get_system_version},
    {"getWebUploadFormStatus",  web_handle_get_web_upload_status},
    {"getOemDevMountedStatus",  web_handle_get_oem_mount_status},
    {"get_mstar_chip_uuid",     web_handle_get_mstar_chip_uuid},

    {"getWifiConfig",           web_handle_get_wifi_cfg},
    {"getWifiAPConfig",         web_handle_get_wifi_ap_cfg},
    {"getWifiAPInfo",           web_handle_get_wifi_ap_info},
    {"getWiFiNetworkStatus",    web_handle_get_wifi_work_status},
    {"getAlarmServerConfig",    web_handle_get_alarm_server_cfg},
    {"getNetworkStatus",        web_handle_get_network_status},
    {"getFTPConfig",            web_handle_get_ftp_cfg},
    {"getSMTPConfig",           web_handle_get_smtp_cfg},
    {"getWifiConnected",        web_handle_get_wifi_conn},

    {"getMediaStreamConfig",    web_handle_get_media_stream_cfg},
    {"getPlatformConfig",       web_handle_get_platform_cfg},
    {"getGB28181Config",        web_handle_get_gb28181_cfg},
    {"getGAT1400Config",        web_handle_get_gat1400_cfg},
    {"getRtmpConfig",           web_handle_get_rtmp_cfg},
    {"getVideoSize",            web_handle_get_video_size},
    {"getMediaVideoConfig",     web_handle_get_media_video_cfg},
    {"getMediaVideoJpegEncodeConfig", web_handle_get_media_jpeg_cfg},
    {"getVideoDefaultConfig",   web_handle_get_video_default_cfg},
    {"getMediaAudioConfig",     web_handle_get_media_audio_cfg},
    {"getAudioDefaultConfig",   web_handle_get_audio_default_cfg},

    {"getUserConfig",           web_handle_get_user_cfg},
    {"getUserConfigPwdEntrypt", web_handle_get_user_pwd_entrypt},
    {"getSystemLogConfig",      web_handle_get_system_log_cfg},
    {"getSystemConfig",         web_handle_get_system_cfg},
    {"getMiscConfig",           web_handle_get_misc_cfg},
    {"getTimeConfig",           web_handle_get_time_cfg},
    {"getPtzConfig",            web_handle_get_ptz_cfg},
    {"getPtzDzoomConfig",       web_handle_get_ptz_dzoom_cfg},
    {"getVideoQoSConfig",       web_handle_get_video_qos_cfg},
    {"getRidgeOSServerConfig",  web_handle_get_ridge_server_cfg},
    {"getAudioPromptConfig",    web_handle_get_audio_prompt_cfg},
    {"getTamperProofConfig",    web_handle_get_tamper_proof_cfg},
    {"getPresetList",           web_handle_get_ptz_preset_list},

    {"getRecordConfig",         web_handle_get_record_cfg},
    {"getMotionDetectAlarm",    web_handle_get_motion_alarm_cfg},
    {"getVideoLostAlarm",       web_handle_get_video_lost_alarm},
    {"getInputAlarm",           web_handle_get_input_alarm},
    {"getOutput",               web_handle_get_output_alarm},
    {"getSMSAlarm",             web_handle_get_sms_alarm},
    {"getStorageFullAlarm",     web_handle_get_storage_alarm},
    {"getAudioAlarm",           web_handle_get_audio_alarm},
    {"getVideoGateAlarm",       web_handle_get_video_gate_alarm},
    {"getPdAlarm",              web_handle_get_pd_alarm},
    {"getLprAlarm",             web_handle_get_lpr_alarm},
    {"getFlameAndFlumesAlarm",  web_handle_get_flame_alarm},
    {"getVideoCoverAlarm",      web_handle_get_video_cover_alarm},
    {"getFdAlarm",              web_handle_get_fd_alarm},
    {"getTempHumidityInfo",     web_handle_get_temp_humidity_alarm},
    {"getVideoRegionAi",        web_handle_get_video_region_alarm},
    {"getIsPdMadpLicenseValid", web_handle_get_pd_license},

    {"getStorageDevInfo",       web_handle_get_storage_dev_info},
    {"getAudioActionFiles",     web_handle_get_audio_action_files},
    {"getHttpsFiles",           web_handle_get_https_files},
    {"getOdmLogoFiles",         web_handle_get_odm_logo_files},
    {"getRecordQueryInfo",      web_handle_get_record_query_info},
    {"getRecordDistribute",     web_handle_get_record_distribute_dist},
    {"getRecordDistribute&",    web_handle_get_record_distribute_dist},
    {"getLogFileList",          web_handle_get_log_list},
    {"getFirmwareUploadProgress",   web_handle_get_firmware_upload_progress},
    {"getSnapshot",             web_handle_get_snapshot},
    {"getSnapshot0",            web_handle_get_snapshot0},
    {"getSnapData",             web_handle_get_snap_data},
    
    /******************************************************************
     * SET
     ******************************************************************/
    {"setLogout",               web_handle_set_logout},
    {"setNetworkLANConfig",     web_handle_set_network_lan_cfg},

    {"setWifiConfig",           web_handle_set_wifi_cfg},
    {"setWifiAPConfig",         web_handle_set_wifiap_cfg},
    {"setNetworkP2PConfig",     web_handle_set_network_p2p_cfg},
    {"setNetworkAlarmServerConfig", web_handle_set_network_alarmserver_cfg},
    {"setFTPConfig",            web_handle_set_ftp_cfg},
    {"setSMTPConfig",           web_handle_set_smtp_cfg},
    {"setTestSMTP",             web_handle_set_test_smtp_cfg},
    {"setTestFTP",              web_handle_set_test_ftp_cfg},

    {"setMediaStreamConfig",    web_handle_set_media_stream_cfg},
    {"setPlatformConfig",       web_handle_set_platform_cfg},
    {"setGB28181Config",        web_handle_set_gb28181_cfg},
    {"setGAT1400Config",        web_handle_set_gat1400_cfg},
    {"setRtmpConfig",           web_handle_set_rtmp_cfg},
    {"setMediaVideoCaptureConfig",  web_handle_set_video_capture_cfg},
    {"setMediaVideoPictureConfig",  web_handle_set_video_picture_cfg},
    {"setLed_KeepOn_Off",       web_handle_set_led_ctrl},
    {"setIPEYE_KeepOn_Off",     web_handle_set_ipeye_ctrl},
    {"setIRCutManual_DayNight", web_handle_set_manual_iruct_ctrl},
    {"setMediaVideoEncodeConfig",   web_handle_set_video_encode_cfg},
    {"setMediaVideoMaskConfig", web_handle_set_video_mask_cfg},
    {"setMediaVideoROIConfig",  web_handle_set_video_roi_cfg},
    {"setMediaVideoYuvConfig",  web_handle_set_video_yuv_cfg},
    {"setMediaVideoUserOverlayConfig",  web_handle_set_video_user_overlay},
    {"setMediaVideoOverlayConfig",  web_handle_set_video_overlay_cfg},
    {"setMediaAudioConfig",     web_handle_set_audio_cfg},

    {"setUserConfig",           web_handle_set_system_user_cfg},
    {"setSystemLogConfig",      web_handle_set_system_log_cfg},
    {"setTimerMaintainConfig",  web_handle_set_timer_maintain_cfg},
    {"setMiscConfig",           web_handle_set_misc_cfg},
    {"setTimeConfig",           web_handle_set_time_cfg},
    {"setPtzCommomConfig",      web_handle_set_ptz_common_cfg},
    {"setPtzAfConfig",          web_handle_set_ptz_af_cfg},
    {"setPtzDzoomConfig",       web_handle_set_ptz_dzoom_cfg},
    {"setPtzAdvanceConfig",     web_handle_set_ptz_advance_cfg},
    {"setVideoQoSConfig",       web_handle_set_video_qos_cfg},
    {"setAudioPromptConfig",    web_handle_set_audio_prompt_cfg},
    {"setTamperProofConfig",    web_handle_set_tamper_proof_cfg},
    {"setMtuValue",             web_handle_set_mtu_value},
    {"setRidgeOSServerConfig",  web_handle_set_ridge_server_cfg},

    {"setRecordConfig",         web_handle_set_record_cfg},
    {"setMotionDetectAlarm",    web_handle_set_motion_alarm},
    {"setVideoLostAlarm",       web_handle_set_video_lost_alarm},
    {"setInputAlarm",           web_handle_set_input_alarm},
    {"setManualInputAlarm",     web_handle_set_manual_input},
    {"setOutput",               web_handle_set_output},
    {"setSMSAlarm",             web_handle_set_sms_alarm},
    {"setStorageFullAlarm",     web_handle_set_storage_alarm},
    {"setAudioAlarm",           web_handle_set_audio_alarm},
    {"setVideoGateAlarm",       web_handle_set_video_gate_alarm},
    {"setPdAlarm",              web_handle_set_pd_alarm},
    {"setVideoCoverAlarm",      web_handle_set_video_cover_alarm},
    {"setLprAlarm",             web_handle_set_lpr_alarm},
    {"setFlameAndFlumesAlarm",  web_handle_set_flame_alarm},
    {"setFdAlarm",              web_handle_set_fd_alarm},
    {"setTempHumidityInfo",     web_handle_set_temp_humidity},
    {"setVideoRegionAi",        web_handle_set_video_region},
    {"setAddUserConfig",        web_handle_set_add_user_cfg},
    {"setEditUserConfig",       web_handle_set_edit_user_cfg},
    {"setDelUserConfig",        web_handle_set_delete_user_cfg},

    {"setPTZCmd",               web_handle_set_ptz_cmd},
    {"set3DYuntaiCoordinate",   web_handle_set_ptz_3d_coordinate},
    {"setPtz3dlocate",          web_handle_set_ptz_3d_locate},
    {"setForceIdr",             web_handle_set_force_idr},

    {"setStorageDevFormat",     web_handle_set_storage_device_format},
    {"setStorageDevMount",      web_handle_set_storage_device_mount},
    {"setStorageDevUnmount",    web_handle_set_storage_device_unmount},
    {"setLogfileDelete",        web_handle_set_log_file_delete},
    {"setDeleteFile",           web_handle_set_file_delete},
    
    {"setFirmwareUpgrade",      web_handle_set_firmware_upgrade},
    {"setFirmwareUpgradeFreeMemory",    web_handle_set_firmware_check_memory},

    /******************************************************************
     * 特殊功能请求
     ******************************************************************/
    {"SystemMaintain",          web_handle_system_maintain},
    {"LogfileQuery",            web_handle_query_log_file},
    {"LogfileDownload",         web_handle_download_log_file},
    {"P2p_unbindDevice",        web_handle_p2p_unbind_device},
    {"PresetList",              web_handle_preset_list},
    {"ThermalImagerCmd",        web_handle_thermal_image_cmd},
    {"replayQueryCacheClear",   web_handle_clear_query_replay_cache},
    {"WebUploadconfiguration",  web_handle_web_upload_configuration},
    {"playAudioActionFile",     web_handle_play_audio_action_file},
    {"stopAudioActionFile",     web_handle_stop_audio_action_file},
    {"beforeWebUploadFormFile", web_handle_ready_web_upload_formfile},
    
    //{"getAlarmData", web_handle_generic_get},
    {NULL, NULL}  // 结束标记
};


static WebPostHandler web_find_handle(const char *path)
{
    int i = 0;
    const char *sep = NULL;
    size_t cmd_len = 0;

    if (path == NULL)
    {
        return NULL;
    }

    for (i = 0; g_handlerTable[i].path != NULL; i++)
    {
        if (strcmp(g_handlerTable[i].path, path) == 0)
        {
            return g_handlerTable[i].handler;
        }
    }

    sep = strchr(path, '&');
    if (sep == NULL)
    {
        sep = strchr(path, '?');
    }
    if (sep != NULL && sep > path)
    {
        cmd_len = (size_t)(sep - path);
        for (i = 0; g_handlerTable[i].path != NULL; i++)
        {
            if (strlen(g_handlerTable[i].path) == cmd_len &&
                strncmp(g_handlerTable[i].path, path, cmd_len) == 0)
            {
                return g_handlerTable[i].handler;
            }
        }
    }

    return NULL;
}

char* web_post_handle(const char* pSoapMsg, const char *path)
{
    if (pSoapMsg == NULL || path == NULL)
    {
        __ERR("pSoapMsg or path is NULL\n");
        return NULL;
    }

    int iRet = 0;
    WebPostContext stWebPostCtx = {0};
    stWebPostCtx.pSoapMsg = pSoapMsg;
    stWebPostCtx.path = path;

    if(!strcmp(path, "WEBLogin"))
    {
        stWebPostCtx.xmlbuf_len = 256;
        iRet = web_handle_web_login(&stWebPostCtx);
        if (iRet)
        {
            stWebPostCtx.status = 500;
            goto __quickexit;
        }

        __DBG("--- web login path:%s, buf:%s ---\n", path, stWebPostCtx.xmlBuf);
        return stWebPostCtx.xmlBuf;
    }
    else if(!strcmp(path, "kcSkipIdPwdLogin"))
    {
        stWebPostCtx.xmlbuf_len = 128;
        iRet = web_handle_skipidpwd_login(&stWebPostCtx);
        if (iRet)
        {
            stWebPostCtx.status = 500;
            goto __quickexit;
        }

        __DBG("--- web login path:%s, buf:%s ---\n", path, stWebPostCtx.xmlBuf);
        return stWebPostCtx.xmlBuf;
    }

    unsigned int nowTime = GetCurrentTimeStamp();
    stWebPostCtx.Nowtime = nowTime;

    if(s_stHttpPasswdErrInfo.error_count > 5)       // 密码错误次数超过5次
    {
        unsigned int lockTime = 60 * 1000;
        if(nowTime - s_stHttpPasswdErrInfo.lock_start_time < lockTime)
        {
            __DBG("web handle please wait:%ds, password will unlock\n", lockTime / 1000);

            stWebPostCtx.xmlbuf_len = 128;
            stWebPostCtx.xmlBuf = (char *)anj_mw_malloc(stWebPostCtx.xmlbuf_len);
            strcpy(stWebPostCtx.xmlBuf, "passwordLock");
            stWebPostCtx.status = 403; 
            goto __quickexit;
        }
    }

    /* 获取用户名，密码 */
    if(parse_userid_passwd_by_xml(pSoapMsg, stWebPostCtx.recvuserid, stWebPostCtx.recvpasswd) == -1)
    {
        __ERR("ParseUseridAndPasswdByXML error\n");
        stWebPostCtx.status = 400;
        goto __quickexit;
    }

    /* 解密DES，校验用户名和密码*/
    decode_des(stWebPostCtx.recvuserid, sizeof(stWebPostCtx.recvuserid));
    decode_des(stWebPostCtx.recvpasswd, sizeof(stWebPostCtx.recvpasswd));

    char devUsergroup[64] = {0};
    iRet = check_user_passwd(stWebPostCtx.recvuserid, stWebPostCtx.recvpasswd, devUsergroup);
    if(iRet != 0)    
    {
        __ERR("check passwd ret:%d, recv useid:%s, passwd:%s!\n", iRet, stWebPostCtx.recvuserid, stWebPostCtx.recvpasswd);

        if(nowTime - s_stHttpPasswdErrInfo.last_error_time > 1000)
        {
            s_stHttpPasswdErrInfo.last_error_time = nowTime;
            s_stHttpPasswdErrInfo.error_count++;

            if(s_stHttpPasswdErrInfo.error_count > 5)
            {
                __ERR("http login password user lock now!\n");
                s_stHttpPasswdErrInfo.lock_start_time = nowTime;
            }
        }

        stWebPostCtx.xmlbuf_len = 128;
        stWebPostCtx.xmlBuf = (char *)anj_mw_malloc(stWebPostCtx.xmlbuf_len);
        strcpy(stWebPostCtx.xmlBuf, "passwordError");

        stWebPostCtx.status = 403;
        goto __quickexit;
    }
    else
    {
        s_stHttpPasswdErrInfo.last_error_time = 0;
        s_stHttpPasswdErrInfo.error_count = 0;
    }

    if(strcasecmp(devUsergroup, "Administrator") == 0)
    {
        stWebPostCtx.bIsAdmin = 1;
    }

    int bodylen = get_soap_body(pSoapMsg, stWebPostCtx.pSoapBody, 0);
    if(bodylen >= 0)
    {
        stWebPostCtx.pSoapBody = (char*)anj_mw_malloc(bodylen + 4);
        if(NULL == stWebPostCtx.pSoapBody)
        {
            __ERR("pSoapBody malloc %d failed\n", bodylen + 4);
            goto __quickexit;
        }        

        int len = get_soap_body(pSoapMsg, stWebPostCtx.pSoapBody, bodylen);
        if(len < 0)
        {
            __ERR("pSoapBody get body failed\n");
            anj_mw_free(stWebPostCtx.pSoapBody);
            stWebPostCtx.pSoapBody = NULL;
            goto __quickexit;
        }

        *(stWebPostCtx.pSoapBody + bodylen) = 0;
    }

    stWebPostCtx.status = 200;
    WebPostHandler pstWebPostHandler = web_find_handle(path);
    if (pstWebPostHandler != NULL)
    {
        iRet = pstWebPostHandler(&stWebPostCtx);
        if (iRet)
        {
            goto __quickexit;
        }

        return stWebPostCtx.xmlBuf;
    }
    else
    {
        __ERR("web post path don't find:%s\n", path);

        stWebPostCtx.status = HTTP_RES_STATUS_NOT_FOUND;
        goto __quickexit;
    }

__quickexit:

    if (stWebPostCtx.pSoapBody != NULL)
    {
        anj_mw_free(stWebPostCtx.pSoapBody);
        stWebPostCtx.pSoapBody = NULL;
    }

    return stWebPostCtx.xmlBuf;
}

