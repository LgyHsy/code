#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>    
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <string>

#include <map>
//using std::map;
using namespace std;

#include "anj_mw_thread.h"
#include "anj_mw_log.h"
#include "anj_mw_icmp.h"
#include "anj_mw_crypt.h"
#include "anj_mw_hwctrl.h"

#include "anj_config.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_sysctl.h"
#include "anj_ispctl.h"
#include "anj_net.h"
#include "anj_sysmng.h"
#include "anj_alarm.h"
#include "anj_service.h"
#include "anj_sdcard.h"
#include "anj_systime.h"
#include "anj_record.h"
#include "record_log.h"
#include "anj_ser.h"

#include "anj_comm.h"
#include "anj_module.h"
#include "rec_mov_def.h"
#include "eventhub.h"
#include "alarm_link.h"
#include "user_auth.h"

#include "http_handle.h"
#include "webpost_handle.h"
#include "cgi_handle.h"

#define SESSION_ID_VAILD_TIME   (60 * 1000)     // 60s
#define SID_LENGTH              16
#define USERNAME_PASSWORD_MAX   64
#define IPADDR_MAX              16
#define PORT_MAX                8

static const char *cgi_base64chr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

typedef enum IP_TYPE
{
    TYPE_IPADDR = 0,
    TYPE_NETMASK = 1,
    TYPE_GATEWAY = 2,
    TYPE_DNS = 3
}IP__TYPE;

/* 视频编码能力集结构体 */
typedef struct video_resolution
{
    char name[16];
    char framerate_range[16];
    char bitrate_range[16];
}v_resolution;

typedef struct video_encodeMdode
{
    char name[8];
    v_resolution resolution[10];
}v_encodeMdode;

typedef struct video_stream
{
    int num;
    v_encodeMdode encode_mode[3];
}v_stream;

typedef struct video_capabilities
{
    v_stream stream[3];
}v_capabilities;

/* 音频编码能力集结构体 */
typedef struct audio_samplerate
{
    int value;
    int bitrate;
}a_samplerate;

typedef struct audio_encode_type
{
    char name[16];
    a_samplerate samplerate[5];
}a_encode_type;

typedef struct audio_capabilities
{
    a_encode_type type[5];
}a_capabilities;

// 视频编码配置结构体
typedef struct video_encoder_config
{
    int id;
    char encode_mode[8];
    char resolution[32];
    char framerate[8];
    char govlength[8];
    char bitrate[8];
    char bitrate_control[8];
}v_encoder_config;


// cgi session id
static int s_SessionIdVaildTime = SESSION_ID_VAILD_TIME;

static map<string, unsigned long long> s_SessionIdMaps;
static pthread_mutex_t s_SessionIdMutex;
static anj_thread_s s_stSessionIdThread;

extern "C" {
extern char *xml_conver_record_query_info(record_query_result_s *pResult);
}


// 安全的字符串拷贝函数, 防止超过dst限制长度导致越界
int strnsafecpy(char *dst, const char *src, int len, int max_len)
{
    if (len < 0) 
    {
        len = 0;
    }

    int copy_len = (len < max_len) ? len : (max_len - 1);
    if (copy_len > 0)
    {
        strncpy(dst, src, copy_len);
    }

    dst[copy_len] = '\0';

    return (len < max_len) ? 0 : -1;
}

// 安全的字符串转整型函数, 有非法字符时返回-1
int safeatoi(const char *str)
{
    const char *pIndex = str;
    int len = strlen(str);

    while(len)
    {
        if(((*pIndex) < '0') || ((*pIndex) > '9'))
            return -1;
        pIndex++;
        len--;
    }

    return atoi(str);
}

static void session_id_time_check()
{
    unsigned long long uNow = GetCurrentTimeStampU64();
    map<string, void*> mapToErase;

    if(s_SessionIdVaildTime <= 0)
    {
        s_SessionIdVaildTime = SESSION_ID_VAILD_TIME;
    }

    anj_mutex_lock(&s_SessionIdMutex);

    map<string, unsigned long long>::iterator it = s_SessionIdMaps.begin();
    for(; it != s_SessionIdMaps.end(); ++it)
    {
        string uid = it->first;
        unsigned long long tLastUpdateTime = it->second;

        long long inverval = uNow - tLastUpdateTime;
        if(inverval > s_SessionIdVaildTime)
        {
            __INFO("session id timeout:%lld, clean id:%s", inverval, uid.c_str());
            mapToErase[uid] = NULL;
        }
    }

    map<string, void*>::iterator it2 = mapToErase.begin();
    for(; it2 != mapToErase.end(); ++it2)
    {
        string ID = it2->first;
        s_SessionIdMaps.erase(ID);
    }

    anj_mutex_unlock(&s_SessionIdMutex);    
}

static int session_id_check_thread(void *ctx, int *bStart)
{
    while (bStart && *bStart)
    {
        session_id_time_check();
        usleep(1000 * 1000);
    }

    return 0;
}

string cgi_session_id_get()
{
    anj_mutex_lock(&s_SessionIdMutex);
    char sidbuf[16] = {0};
    unsigned int uNow = GetCurrentTimeStamp();
    sprintf(sidbuf,"%X", uNow);

    s_SessionIdMaps[string(sidbuf)] = GetCurrentTimeStampU64();
    __DBG("session id:%s get\n", sidbuf);
    anj_mutex_unlock(&s_SessionIdMutex);

    return string(sidbuf);
}

void cgi_session_id_delete(string sid)
{
    anj_mutex_lock(&s_SessionIdMutex);
    s_SessionIdMaps.erase(sid);
    __DBG("session id:%s delete\n", sid.c_str());
    anj_mutex_unlock(&s_SessionIdMutex);
}

int cgi_session_id_refresh(string sid)
{
    anj_mutex_lock(&s_SessionIdMutex);
    map<string, unsigned long long>::iterator it = s_SessionIdMaps.find(sid);
    if(it == s_SessionIdMaps.end())
    {
        anj_mutex_unlock(&s_SessionIdMutex);
        __ERR("session id:%s not found\n", sid.c_str());
        return -1;
    }

    it->second = GetCurrentTimeStampU64();
    __DBG("session id:%s refresh ok\n", sid.c_str());
    anj_mutex_unlock(&s_SessionIdMutex);
    return 0;
}

int http_cgi_init_session_id()
{
    int iRet = -1;
    if (s_stSessionIdThread.start == 0)
    {
        memset(&s_stSessionIdThread, 0, sizeof(anj_thread_s));
        s_stSessionIdThread.bAutoDestroy = 0;
        strncpy(s_stSessionIdThread.iThreadName, "session_id_thread", sizeof(s_stSessionIdThread.iThreadName) - 1);
        s_stSessionIdThread.iThreadjob.ctx = &s_stSessionIdThread;
        s_stSessionIdThread.iThreadjob.func = session_id_check_thread;
        iRet = anj_thread_task_create(&s_stSessionIdThread);
    }

    return iRet;
}

void http_cgi_uninit_session_id()
{
    if (s_stSessionIdThread.start == 1)
    {
        anj_thread_task_destroy(&s_stSessionIdThread, -1);
    }
}

void split_path(const char *path, char (*url)[200])
{
    int ii, jj = 0;
    for(ii = 0; ii < 10; ii++)
    {
        for(jj = 0; jj < 200; jj++, path++)
        {
            if(*path == '?')
            {
                path++;
                break;
            }

            if(*path == '\0')
            {
                break;
            }

            url[ii][jj] = *path;
        }

        if(*path == '\0')
        {
            break;
        }
    }
}

int urldecode(char *str, int len)
{
    char *dest = str;
    char *data = str;

    int value;
    int c;

    while (len--)
    {
        if (*data == '+')
        {
            *dest = ' ';
        }
        else if (*data == '%' && 
                len >= 2 && 
                isxdigit((int) *(data + 1)) && 
                isxdigit((int) *(data + 2)))
        {

            c = ((unsigned char *)(data+1))[0];
            if (isupper(c))
                c = tolower(c);

            value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;
            c = ((unsigned char *)(data+1))[1];

            if (isupper(c))
                c = tolower(c);

            value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

            *dest = (char)value;
            data += 2;
            len -= 2;
        } 
        else
        {
            *dest = *data;
        }

        data++;
        dest++;
    }

    *dest = '\0';
    return dest - str;
}

int login_request(const char *urlpath, char (*url)[200], char *pResultBuff)
{
    if(strcmp(url[0], "/login") == 0)
    {
        int ii = {0};
        char username[50] = {0};
        char password[50] = {0};

        char *urlBuf = url[1];

        //提取用户名
        while(strncmp(urlBuf,"username", 8))
        {
            if(*urlBuf == '\0')
                break;
            urlBuf++;
        }

        if(*urlBuf != '\0')
            urlBuf += 9;

        for(ii = 0; ii < 50; ii++, urlBuf++)
        {
            if(*urlBuf == '&')
            {
                urlBuf++;
                break;
            }

            if(*urlBuf == '\0')
                break;

            username[ii]=*urlBuf;
        }

        urlBuf = url[1];
        //提取密码
        while(strncmp(urlBuf,"password", 8))
        {
            if(*urlBuf == '\0')
                break;
            urlBuf++;
        }

        if(*urlBuf != '\0')
            urlBuf += 9;

        for(ii = 0; ii < 50; ii++, urlBuf++)
        {
            if(*urlBuf == '&')
            {
                urlBuf++;
                break;
            }
            if(*urlBuf == '\0')
                break;

            password[ii]=*urlBuf;
        }
        __INFO("login username:%s password:%s\n", username, password);

        if(strlen(username) == 0 && strlen(password) == 0)
        {
            return -1;
        }

        char mypasswd[50] = {0};
        if(strlen(username) == 0)
        {
            strcpy(username,"admin");
        }

        if (UserAuthGetPassword(username, mypasswd) != 0)
        {
            __ERR("get username:%s password failed\n",username);
            return -1;
        }

        urldecode(password, sizeof(password));

        char md5Buf[64] = {0};        
        our_md5_encode(md5Buf, (const unsigned char *)mypasswd, strlen(mypasswd));

        if( strcasecmp(mypasswd, password) != 0 &&
            strcasecmp(md5Buf, password) != 0 )
        {
            strcat(pResultBuff, "{\n");
            strcat(pResultBuff, "\"failed\":0,\n");
            strcat(pResultBuff, "\"error_code\":\"invalid username or password\"\n");
            strcat(pResultBuff, "}\n"); 
        }
        else
        {
            string uid = cgi_session_id_get();

            strcat(pResultBuff, "{\n");
            strcat(pResultBuff, "\"success\":1,\n");
            strcat(pResultBuff, "\"g_uidMap\":\"");
            strcat(pResultBuff, uid.c_str());
            strcat(pResultBuff, "\"\n");
            strcat(pResultBuff, "}\n");
        }

        return 0;
    }

    return -1;
}

int check_userpassword(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    const char *p = urlpath;
    const char* pUsernameStr = "&username=";
    const char* pPasswordStr = "&password=";
    const char* pUsername = strstr(p, pUsernameStr);
    const char* pPassword = strstr(p, pPasswordStr);

    char szUsername[128] = {0};
    char szPassword[128] = {0};

    if(pUsername && pPassword)
    {
        const char *p1 = NULL;
        const char *p2 = NULL;
        char szTmp[128] = {0};        

        p1 = pUsername + strlen(pUsernameStr);
        p2 = pPassword;
        strncpy(szTmp, p1, p2-p1);
        snprintf(szUsername, sizeof(szUsername), "%s", szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pPassword + strlen(pPasswordStr);

        p2 = strstr(p1, "&");
        if( NULL == p2)
            p2 = strstr(p1, ";");
        if( NULL == p2)
            p2 = strstr(p1, "@");
        if( NULL == p2)
            p2 = p + strlen(p);

        strncpy(szTmp, p1, p2-p1);
        snprintf(szPassword, sizeof(szPassword), "%s", szTmp);
    }

    __ERR("username:%s;password:%s\n", szUsername, szPassword);

    if(strlen(szUsername) == 0 && strlen(szPassword) == 0 )
    {
        sprintf(pResultBuf, "error: username or password not found.");
        return -1;
    }

    char szCorrectPwd[ACCOUNT_PASSWORD_MAX_LEN] = {0};

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    UserConfig *pstUserCfg = &pstSystemCfg->userCfg;

    int bFound = 0;

    int i = 0;
    for(i = 0; i < pstUserCfg->count; i++)
    {
        if(strcmp(pstUserCfg->accounts[i].userName, szUsername) == 0)
        {
            if(strcasecmp("Enable", pstUserCfg->accounts[i].status) != 0 )
            {
                sprintf(pResultBuf, "error: user not enabled.");
                return -1;
            }

            if(strcasecmp("Administrator", pstUserCfg->accounts[i].group.groupName) != 0 &&
                strcasecmp("Operator", pstUserCfg->accounts[i].group.groupName) != 0 )  
            {
                sprintf(pResultBuf, "error: user have no permission.");
                return -1;
            }

            strcpy(szCorrectPwd, pstUserCfg->accounts[i].password);
            bFound = 1;
            break;
        }
    }

    if(0 == bFound)
    {
        sprintf(pResultBuf, "error: user not found.");
        return -1;
    }

    char md5Buf[64] = {0};        
    our_md5_encode(md5Buf, (const unsigned char *)szCorrectPwd, strlen(szCorrectPwd));

    if( strcasecmp(szCorrectPwd, szPassword) != 0 &&
        strcasecmp(md5Buf, szPassword) != 0 )
    {
        sprintf(pResultBuf, "error password %s", szPassword);
        return -1;
    }
    else
    {
        return 0;
    }

    return -1;
}


int slog_tcpopen_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    if(strcmp(urlpath, "/openslog") == 0)
    {
        if(check_userpassword(urlpath, url, pResultBuf) != 0)
        {
            return 0;
        }

        strcat(pResultBuf, "ok");
        //system("touch /tmp/flag.open.slog.tcp");
        return 0;
    }

    return -1;
}

int setting_audio_volumnplay_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    const char *audio_volumn_pre = "/settings/audio/VolumePlay=";
    const char *p = strstr(urlpath, audio_volumn_pre);

    if(p != NULL)
    {
        int nVolumePlay = atoi(p + strlen(audio_volumn_pre));
        if( nVolumePlay > 100 )
            nVolumePlay = 100;
        else if( nVolumePlay < 0 )
            nVolumePlay = 0;

        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        AudioCapture *pstAudioCaptureCfg = &pstMediaCfg->audioConfig.audioCapture;

        pstAudioCaptureCfg->volume_play = nVolumePlay;
        anj_config_audio_capture_set(pstAudioCaptureCfg);

        strcat(pResultBuf, "ok");
        return 0;
    }

    return -1;
}

int setting_audio_volumnmic_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    const char *audio_volumn_pre = "/settings/audio/VolumeMic=";
    const char *p = strstr(urlpath, audio_volumn_pre);
    if(p != NULL)
    {
        int nVolumePlay = atoi(p + strlen(audio_volumn_pre));
        if( nVolumePlay > 100 )
            nVolumePlay = 100;
        else if( nVolumePlay < 0 )
            nVolumePlay = 0;

        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        AudioCapture *pstAudioCaptureCfg = &pstMediaCfg->audioConfig.audioCapture;

        pstAudioCaptureCfg->volume_capture = nVolumePlay;
        anj_config_audio_capture_set(pstAudioCaptureCfg);

        strcat(pResultBuf, "ok");
        return 0;
    }

    return -1;
}

int setting_platform_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    const char *find_pre = "/settings/platform/";
    const char *p = strstr(urlpath, find_pre);
    if (p == NULL)
    {
        return -1;
    }

    const char* pEnableStr = "&enable=";
    const char* pServerStr = "&server=";
    const char* pPortStr = "&port=";
    const char* pUserStr = "&user=";
    const char* pPwdStr = "&password=";
    const char* pMacStr = "&mac=";

    const char* pEnable = strstr(p, pEnableStr);
    const char* pServer = strstr(p, pServerStr);
    const char* pPort = strstr(p, pPortStr);
    const char* pUser = strstr(p, pUserStr);
    const char* pPwd = strstr(p, pPwdStr);
    const char* pMac = strstr(p, pMacStr);

    if(pMac != NULL)
    {
        __INFO("update mac:%s\n", pMac);
        NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
        LANConfig *pstLanCfg = &pstNetworkCfg->lanCfg;

        memset(pstLanCfg->MACAddress, '\0', MAC_ADDRESS_LEN);
        snprintf((char*)pstLanCfg->MACAddress, MAC_ADDRESS_LEN - 1, pMac);
        anj_config_network_lan_set(pstLanCfg);
    }

    if(pEnable && pServer && pPort && pUser && pPwd)
    {
        PlatformConfig *pstPlatformCfg = (PlatformConfig *)getPlatformConfig();

        const char *p1 = NULL;
        const char *p2 = NULL;
        char szTmp[128] = {0};        

        p1 = pEnable + strlen(pEnableStr);
        p2 = pServer;
        strncpy(szTmp, p1, p2-p1);
        pstPlatformCfg->vmCfg.enable = atoi(szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pServer + strlen(pServerStr);
        p2 = pPort;
        strncpy(szTmp, p1, p2-p1);
        strcpy(pstPlatformCfg->vmCfg.server, szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pPort + strlen(pPortStr);
        p2 = pUser;
        strncpy(szTmp, p1, p2-p1);
        pstPlatformCfg->vmCfg.port = atoi(szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pUser + strlen(pUserStr);
        p2 = pPwd;
        strncpy(szTmp, p1, p2-p1);            
        strcpy(pstPlatformCfg->vmCfg.username, szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pPwd + strlen(pPwdStr);
        p2 = p + strlen(p);
        strncpy(szTmp, p1, p2-p1);            
        strcpy(pstPlatformCfg->vmCfg.password, szTmp);

        __DBG("vm platform enable=%d, server=%s, port=%d, username=%s, password=%s\n", 
            pstPlatformCfg->vmCfg.enable, pstPlatformCfg->vmCfg.server, 
            pstPlatformCfg->vmCfg.port, pstPlatformCfg->vmCfg.username, 
            pstPlatformCfg->vmCfg.password);

        anj_config_platform_set(pstPlatformCfg);
        strcat(pResultBuf, "ok");
    }
    else
    {
        strcat(pResultBuf,"error");
    }

    return 0;
}

int setting_ntp_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    const char *find_pre = "/settings/ntp/";
    const char *p = strstr(urlpath, find_pre);
    if (p == NULL)
    {
        return -1;
    }
    
    const char* pEnableStr = "&enable=";
    const char* pServerStr = "&server=";
    const char* pPortStr = "&port=";

    const char* pEnable = strstr(p, pEnableStr);
    const char* pServer = strstr(p, pServerStr);
    const char* pPort = strstr(p, pPortStr);

    if(pEnable && pServer && pPort)
    {
        SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
        TimeConfig *pstTimeCfg = &pstSystemCfg->timeCfg;

        const char *p1 = NULL;
        const char *p2 = NULL;
        char szTmp[128] = {0};        

        p1 = pEnable + strlen(pEnableStr);
        p2 = pServer;
        strncpy(szTmp, p1, p2 - p1);
        if(atoi(szTmp) > 0)
            sprintf(pstTimeCfg->timeMode.modeName, "NTP");
        else
            sprintf(pstTimeCfg->timeMode.modeName, "P2P");

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pServer + strlen(pServerStr);
        p2 = pPort;
        strncpy(szTmp, p1, p2-p1);
        strcpy(pstTimeCfg->ntpConfig.serverIP, szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pPort + strlen(pPortStr);
        p2 = p + strlen(p);
        strncpy(szTmp, p1, p2-p1);
        pstTimeCfg->ntpConfig.serverPort = atoi(szTmp);

        pstTimeCfg->ntpConfig.refreshInterval = 60;

        __ERR("ntp set time mode %s, ntp server=%s, port=%d\n", 
            pstTimeCfg->timeMode.modeName, pstTimeCfg->ntpConfig.serverIP, pstTimeCfg->ntpConfig.serverPort);

        anj_config_system_time_set(pstTimeCfg);
        strcat(pResultBuf, "ok");
    }
    else
    {
        strcat(pResultBuf, "error");
    }

    return 0;
}

int ko_door_get_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    //http:// x.x.x.x/&wallpadmac=00:06:00:01:9A:EE&doormac=00:06:00:01:9B:F8
    const char *find_pre = "&wallpadmac=";
    const char *p = strstr(urlpath, find_pre);
    if (p == NULL)
    {
        return -1;
    }

    const char* pPadMacStr = "&wallpadmac=";
    const char* pDoorMacStr = "&doormac=";

    const char* pPadMac = strstr(p, pPadMacStr);
    const char* pDoorMac = strstr(p, pDoorMacStr);

    if(pPadMac && pDoorMac)
    {
        char szPadMac[64] = {0};
        char szDoorMac[64] = {0};

        const char *p1 = NULL;
        const char *p2 = NULL;
        char szTmp[64] = {0};        

        p1 = pPadMac + strlen(pDoorMacStr);
        p2 = pDoorMac;
        strncpy(szTmp, p1, p2-p1);
        snprintf(szPadMac, sizeof(szPadMac), "%s", szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pDoorMac + strlen(pDoorMacStr);
        p2 = p + strlen(p);
        strncpy(szTmp, p1, p2-p1);
        snprintf(szDoorMac, sizeof(szDoorMac), "%s", szTmp);

        char  MACAddress[MAC_ADDRESS_LEN] = {0};    
        unsigned char macBuf[6] = {0};    
        net_get_hwaddr(WIRE_INTERFACE_NAME, macBuf);            

        snprintf(MACAddress, sizeof(MACAddress), "%02X:%02X:%02X:%02X:%02X:%02X", 
                macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);

        int syncstatus = 2;
        if(strcasecmp(MACAddress, szDoorMac) == 0)
        {
            syncstatus = 0;
        }

        int sipstatus = 408;
        PlatRegResult regResult;
        memset(&regResult, 0, sizeof(regResult));

        int fd = open(PLATFORM_REGISGER_RESULT_FILE, O_RDONLY);
        if(fd  > 0)
        {
            int readCnt = read(fd, &regResult, sizeof(regResult));
            if(readCnt == sizeof(regResult))
            {
                if(regResult.result == PLAT_REG_RESULT_REG_OK)
                    sipstatus = 200;
            }
            else
            {
                __ERR("read failed:%s.\n", PLATFORM_REGISGER_RESULT_FILE);
            }

            close(fd);
        }
        else
        {
            __ERR("open %s failed\n", PLATFORM_REGISGER_RESULT_FILE);
        }

        sprintf(pResultBuf,
            "{\n"
            "\"packetrev\" : \"1.0\",\n"
            "\"action\":\"ack_sync\",\n"
            "\"source\":\"door\",\n"
            "\"dest\":\"wallpad\",\n"
            "\"syncstat\":\"%d\",\n"
            "\"sipstat\":\"%d\"\n"
            "}",
            syncstatus, 
            sipstatus);
    }
    else
    {
        strcat(pResultBuf, "error");
    }

    return 0;
}

int ko_door_io_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    //http:// x.x.x.x/&wallpadmac=00:06:00:01:9A:EE&doormac=00:06:00:01:9B:F8
    const char *find_pre = "/door/&Door=";
    const char *p = strstr(urlpath, find_pre);

    if(p != NULL)
    {
        // char* pCryptStr=p + strlen("/door/&Door=");
        strcat(pResultBuf, "ok");
        return 0;
    }

    return -1;
}

int ko_door_ledlight_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    //http://x.x.x.x/&ledlight=0
    const char *find_pre = "/&ledlight=";
    const char *p = strstr(urlpath, find_pre);
    if(p != NULL)
    {
        const char* pData = p + strlen(find_pre);
        if (*pData == '0')
        {
            strcat(pResultBuf, "ok");
            //todo
            //anj_mw_hwctrl_gpio_write(5, 0);
        }
        else if (*pData == '1')
        {
            strcat(pResultBuf,"ok");
            //todo
            //anj_mw_hwctrl_gpio_write(5, 1);
        }
        else
        {
            strcat(pResultBuf, "error");
        }

        return 0;
    }

    return -1;
}

int ko_door_get_sipserver_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    //http:// x.x.x.x/&wallpadmac=00:06:00:01:9A:EE&doormac=00:06:00:01:9B:F8
    const char *find_pre = "/&queryPlatformSetup";
    const char *p = strstr(urlpath, find_pre);
    if (p == NULL)
    {
        return -1;
    }

    char MACAddress[MAC_ADDRESS_LEN] = {0};    
    unsigned char macBuf[6] = {0};

    net_get_hwaddr(WIRE_INTERFACE_NAME, macBuf);
    snprintf(MACAddress, sizeof(MACAddress), "%02X:%02X:%02X:%02X:%02X:%02X", 
        macBuf[0],macBuf[1],macBuf[2],macBuf[3],macBuf[4],macBuf[5]);

    PlatformConfig *pstPlatformCfg = (PlatformConfig *)getPlatformConfig();

    PlatRegResult regResult;
    memset(&regResult, 0, sizeof(regResult));

    int fd = open(PLATFORM_REGISGER_RESULT_FILE, O_RDONLY);
    if(fd  < 0)
    {
        __ERR("open %s failed\n", PLATFORM_REGISGER_RESULT_FILE);
    } 
    else
    {
        int readCnt = read(fd, &regResult, sizeof(regResult));
        if(readCnt == sizeof(regResult))
        {
        }
        else
        {
            __ERR("read failed: %s.\n", PLATFORM_REGISGER_RESULT_FILE);
        }

        close(fd);
    }

    char sipstat[128] = {0};
    if(strstr(regResult.reservedInfo, "status code ") != NULL)
        strcpy(sipstat, regResult.reservedInfo + strlen("status code "));
    else
        strcpy(sipstat, regResult.reservedInfo);

    sprintf(pResultBuf,
        "{\n"
        "\"packetrev\" : \"1.0\",\n"
        "\"action\":\"ack_sync\",\n"
        "\"source\":\"door\",\n"
        "\"dest\": \"wallpad\",\n"
        "\"Login Server\" : \"%s\",\n"
        "\"Server IP\" : \"%s\",\n"
        "\"Server Port\" : \"%u\",\n"
        "\"Server Account\" : \"%s\",\n"
        "\"Server Password\" : \"%s\",\n"
        "\"Mac\" : \"%s\",\n"
        "\"sipstat\":\"%s\",\n"
        "\"Reg\":\"%d\"\n"
        "}",
        pstPlatformCfg->vmCfg.enable > 0  ? "true":"false",
        pstPlatformCfg->vmCfg.server,
        pstPlatformCfg->vmCfg.port,
        pstPlatformCfg->vmCfg.username,
        pstPlatformCfg->vmCfg.password,
        MACAddress,
        sipstat,
        pstPlatformCfg->voipCfg.reserved);

    return 0;
}

int ko_door_set_broardcast_enable(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    //http:// x.x.x.x/&setdoorbroardcast=0@user=admin@password=e10adc3949ba59abbe56e057f20f883e
    const char *find_pre = "/&setdoorbroardcast=";
    const char *p = strstr(urlpath, find_pre);
    if(p != NULL)
    {
        int enable = atoi(p + strlen(find_pre));
        if(check_userpassword(urlpath, url, pResultBuf ) != 0 )
        {
            return 0;
        }

        PlatformConfig *pstPlatformCfg = (PlatformConfig *)getPlatformConfig();
        if (pstPlatformCfg->voipCfg.reserved != enable)
        {
            pstPlatformCfg->voipCfg.reserved = enable;
            anj_config_platform_set(pstPlatformCfg);
        }

        sprintf(pResultBuf, "ok");
        return 0;        
    }

    return -1;
}

int setting_motion_detect_request(const char *urlpath, char (*url)[200], char *pResultBuf)
 {
    //    __ERR("urlPath=%s\n", urlpath);
    const char *find_pre = "/settings/motiondetect";
    const char *p = strstr(urlpath, find_pre);
    if (p == NULL)
    {
        return -1;
    }
    
    const char* pEnableStr = "enable=";
    const char* pBlockCountStr =  "blockcount=";
    const char* pBlockConfigStr = "blockconfig=";

    const char* pEnablePtr = strstr(p, pEnableStr);
    const char* pBlockCountPtr = strstr(p, pBlockCountStr);
    const char* pBlockConfigPtr = strstr(p, pBlockConfigStr);

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm stMotionAlarm = {0};
    memcpy(&stMotionAlarm, &pstAlarmConfig->normalAlarm.motionDetectAlarm[iCameraIdx], sizeof(MotionDetectAlarm));

    if(pEnablePtr && pBlockCountPtr && pBlockConfigPtr)
    {
        char szTmp[256] = {0};
        const char *p1 = NULL;
        const char *p2 = NULL;

        p1 = pEnablePtr;
        p2 = strchr(p1 + strlen(pEnableStr), '&'); 
        if (p2)
        {
            memcpy(szTmp, p1 + strlen(pEnableStr), p2 - p1 - strlen(pEnableStr));     
        }
        else
        {
            strcpy(szTmp, p1 + strlen(pEnableStr));
        }
        stMotionAlarm.enable = atoi(szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pBlockCountPtr;
        p2 = strchr(p1 + strlen(pBlockCountStr), '&'); 
        if (p2)
        {
            memcpy(szTmp, p1 + strlen(pBlockCountStr), p2 - p1 - strlen(pBlockCountStr));     
        }
        else
        {
            strcpy(szTmp, p1 + strlen(pBlockCountStr));
        }
        stMotionAlarm.blockCount = atoi(szTmp);

        memset(szTmp, 0, sizeof(szTmp));
        p1 = pBlockConfigPtr;
        p2 = strchr(p1 + strlen(pBlockConfigStr), '&'); 
        if (p2)
        {
            memcpy(szTmp, p1 + strlen(pBlockConfigStr), p2 - p1 - strlen(pBlockConfigStr));     
        }
        else
        {
            strcpy(szTmp, p1 + strlen(pBlockConfigStr));
        }    

        strcpy(stMotionAlarm.blockCfg, szTmp);

        {
            MotionDetectAlarm stMotionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                memcpy(&stMotionAlarmArray[iCameraIdx], &stMotionAlarm, sizeof(MotionDetectAlarm));
            }
            anj_config_alarm_motion_set(stMotionAlarmArray);
        }

        strcat(pResultBuf, "ok");
    }
    else
    {
        strcat(pResultBuf, "error");
    }

    return 0;
}

int setting_get_specific_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    //http:// x.x.x.x/settings/getspecific
    const char *find_pre = "/settings/getspecific";
    const char *p = strstr(urlpath, find_pre);
    if (p == NULL)
    {
        return -1;
    }

    pResultBuf[0] = 0;
    sprintf(pResultBuf + strlen(pResultBuf), "{\n");

    AjOemStruct oemInfo = {0};
    anj_config_oem_get(&oemInfo);

    sprintf(pResultBuf + strlen(pResultBuf), 
        "\"specific\":\n"
        "{\n"
        "\"DEVICETYPE\" : \"%s\",\n"
        "\"OEMVERSION\":\"%s\",\n"
        "\"OEMBUILDTIME\":\"%s\",\n"
        "\"SN\":\"%s\",\n"
        "\"HW\":\"%s\",\n"
        "\"ETHMAC\":\"%s\",\n"
        "\"WIFIMAC\":\"%s\",\n"
        "\"MBL\":\"%s\",\n"
        "\"LANGUAGE\":\"%s\"\n"
        "}",
        oemInfo.szDeviceType,
        oemInfo.szVersion,
        oemInfo.szBuildtime,
        oemInfo.szOemSN,
        oemInfo.szOemHWVersion,
        oemInfo.szOemEthMac,
        oemInfo.szOemWifiMac,
        oemInfo.szOemMBL,
        oemInfo.szOemLanguage
    );

    char szP2pID[64] = {0};
    if(access(TMP_P2P_ID_FILE_NAME, F_OK) == 0)
    {
        if(read_file_to_string(TMP_P2P_ID_FILE_NAME, szP2pID, sizeof(szP2pID))  <= 0)
        {
            ;
        }
    }

    sprintf(pResultBuf + strlen(pResultBuf), ",\n");
    sprintf(pResultBuf + strlen(pResultBuf), "\"P2PID\" : \"%s\"", szP2pID);

    SYSTEM_VERSION_DATA stSystemVersion = {0};
    anj_sysmng_version_info_get(&stSystemVersion, 0);

    sprintf(pResultBuf + strlen(pResultBuf), ",\n");
    sprintf(pResultBuf + strlen(pResultBuf), "\"fsversion\" : \"%s\"", stSystemVersion.fsVersion);
    sprintf(pResultBuf + strlen(pResultBuf), ",\n");
    sprintf(pResultBuf + strlen(pResultBuf), 
        "\"network_status\":\n"
        "{\n");

    LANConfig stTmpLanCfg = {0};    
    int bHaveEth = 0;
    int bHaveWifi = 0;

    if (Check_Link_Status(WIRE_INTERFACE_NAME))
    {
        bHaveEth = 1;
        struct NET_CONFIG netcfg = {0};
        char macBuf[6] = {0};

        net_get_hwaddr(WIRE_INTERFACE_NAME, (unsigned char*)macBuf);                
        format_mac_addr_from_digit_to_string(macBuf, sizeof(macBuf), (char*)stTmpLanCfg.MACAddress, MAC_ADDRESS_LEN);

        net_get_info(WIRE_INTERFACE_NAME, &netcfg);
        get_ip_str(netcfg.ifaddr, stTmpLanCfg.IPAddress, MAX_IP_NAME_LEN);
        get_ip_str(netcfg.netmask, stTmpLanCfg.netMask, MAX_IP_NAME_LEN);
        get_ip_str(netcfg.gateway, stTmpLanCfg.gateWay, MAX_IP_NAME_LEN);

        sprintf(pResultBuf + strlen(pResultBuf), 
            "\"ethstatus\":\n"
            "{\n"
            "\"MAC\" : \"%s\",\n"
            "\"IPADDR\" : \"%s\",\n"
            "\"MASK\":\"%s\",\n"
            "\"GATEWAY\":\"%s\"\n"
            "}",
            stTmpLanCfg.MACAddress,
            stTmpLanCfg.IPAddress,
            stTmpLanCfg.netMask,
            stTmpLanCfg.gateWay);
    }

    char szWifiInterface[16] = {0};
    strncpy(szWifiInterface, net_get_wireless_name(), sizeof(szWifiInterface) -1);

    if(is_network_interface_up(szWifiInterface))
    {
        bHaveWifi = 1;
        struct NET_CONFIG netcfg = {0};            
        char macBuf[6] = {0};

        memset(&stTmpLanCfg, 0, sizeof(LANConfig));        

        net_get_hwaddr(szWifiInterface, (unsigned char*)macBuf);    
        format_mac_addr_from_digit_to_string(macBuf, 6, (char*)stTmpLanCfg.MACAddress, MAC_ADDRESS_LEN);

        net_get_info(szWifiInterface, &netcfg);            
        get_ip_str(netcfg.ifaddr, stTmpLanCfg.IPAddress, MAX_IP_NAME_LEN);
        get_ip_str(netcfg.netmask, stTmpLanCfg.netMask, MAX_IP_NAME_LEN);
        get_ip_str(netcfg.gateway, stTmpLanCfg.gateWay, MAX_IP_NAME_LEN);

        if( bHaveEth > 0)
            sprintf(pResultBuf + strlen(pResultBuf), ",\n");

        sprintf(pResultBuf + strlen(pResultBuf), 
            "\"wifistatus\":\n"
            "{\n"
            "\"MAC\" : \"%s\",\n"
            "\"IPADDR\" : \"%s\",\n"
            "\"MASK\":\"%s\",\n"
            "\"GATEWAY\":\"%s\"\n"
            "}",
            stTmpLanCfg.MACAddress,
            stTmpLanCfg.IPAddress,
            stTmpLanCfg.netMask,
            stTmpLanCfg.gateWay);
    }

    net_get_two_dns(stTmpLanCfg.DNS1, MAX_IP_NAME_LEN, stTmpLanCfg.DNS2, MAX_IP_NAME_LEN);    
    if( bHaveEth > 0 || bHaveWifi > 0 )
        sprintf(pResultBuf + strlen(pResultBuf), ",\n");

    sprintf(pResultBuf + strlen(pResultBuf), 
        "\"DNS1\":\"%s\",\n"
        "\"DNS2\":\"%s\"\n",
        stTmpLanCfg.DNS1,
        stTmpLanCfg.DNS2);

    sprintf(pResultBuf + strlen(pResultBuf), "\n}");
    sprintf(pResultBuf + strlen(pResultBuf), "\n}");

    return 0;
}

int cgi_settings_request(const char *urlpath, char (*url)[200], char *pResultBuf)
{
    //打开SLOG TCP收发
    if(slog_tcpopen_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //设置设备输出音量
    if(setting_audio_volumnplay_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //设置设备采集音量
    if(setting_audio_volumnmic_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //设置平台登录信息
    if(setting_platform_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //设置NTP
    if(setting_ntp_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //韩国客户要求HTTP取SIP状态
    if(ko_door_get_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //韩国客户要求HTTP开门
    if(ko_door_io_request(urlpath, url, pResultBuf) == 0)
        return 0;

    if(ko_door_ledlight_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //韩国客户要求HTTP取SIP SERVER配置与状态
    if(ko_door_get_sipserver_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //韩国客户设置广播自身信息开关
    if(ko_door_set_broardcast_enable(urlpath, url, pResultBuf) == 0)
        return 0;

    //韩国设置
    if (setting_motion_detect_request(urlpath, url, pResultBuf) == 0)
        return 0;

    //九洲要求HTTP取设备定制信息
    if(setting_get_specific_request(urlpath, url, pResultBuf) == 0)
        return 0;

    return -1;
}

// 获取cgi url中的参数的值, 获取失败时返回0
int cgi_get_value(const char *http_url, const char *name, char *buffer, int buflen)
{
    int iIndex = 0;
    const char *pIndex = strstr(http_url, name);
    if(pIndex)
    {
        pIndex += strlen(name);
        if(*pIndex == '=')
        {
            pIndex++;
            for(iIndex = 0; (iIndex < (buflen - 1)) && (*pIndex != '&') && (*pIndex != '\0'); iIndex++, pIndex++)
            {
                buffer[iIndex] = *pIndex;
            }
        }
    }

    if(iIndex > 0)
        buffer[iIndex] = '\0';

    return iIndex;
}

// 获取cgi url中的参数的值, 获取失败时返回0
int cgi_get_value_2(const char *http_url, const char *name, char *buffer, int buflen)
{
    int iIndex = 0;
    const char *pIndex = strstr(http_url, name);
    //__ERR("555pIndex = %s,http_url=%s,name=%s\n",pIndex,http_url,name);
    if(pIndex)
    {
        pIndex += strlen(name);
        if(*pIndex == '=')
        {
            pIndex++;
            for(iIndex = 0; (iIndex < (buflen-1)) && (*pIndex != '&') && (*pIndex != '\0'); iIndex++, pIndex++)
            {
                buffer[iIndex] = *pIndex;
            }
        }
    }

    if(pIndex == NULL)  //字符串2不是字符串1的子串
    {
        iIndex = -1;
    }

    if(iIndex > 0)
        buffer[iIndex] = '\0';

    //__ERR("iIndex = %d\n",iIndex);
    return iIndex;
}

// 获取cgi url中name后参数全部的值, 获取失败时返回0
int cgi_get_value_all(const char *http_url, const char *name, char *buffer, int buflen)
{
    int iIndex = 0;
    const char *pIndex = strstr(http_url, name);
    //__ERR("555pIndex = %s,http_url=%s,name=%s\n",pIndex,http_url,name);
    if(pIndex)
    {
        pIndex += strlen(name);
        if(*pIndex=='=')
        {
            pIndex++;
            for(iIndex = 0; (iIndex < (buflen-1)) && (*pIndex != '\0'); iIndex++, pIndex++)
            {
                buffer[iIndex]=*pIndex;
            }
        }
    }

    if(pIndex == NULL)  //字符串2不是字符串1的子串
    {
        iIndex = -1;
    }

    if(iIndex > 0)
        buffer[iIndex] = '\0';

    //__ERR("iIndex = %d\n",iIndex);
    return iIndex;
}

// 判断uid是否有效, 缺少或无效返回0, 有效返回1
int cgi_is_valid_uid(const char *http_url,char *response, const char *url)
{
    char uid[SID_LENGTH] = {0};
    if(!cgi_get_value(http_url, "uid", uid, SID_LENGTH))
    {
        sprintf(response, XML_CGI_FAULT, url ? url : "NULL", "error, parse uid");
        return 0;
    }

    if(cgi_session_id_refresh(string(uid)) != 0)
    {
        sprintf(response, XML_CGI_FAULT, url ? url : "NULL", "uid error");
        return 0;
    }

    return 1;
}

// 判断username,password是否有效, 缺少返回-1,密码错误返回0, 正确返回1
int cgi_is_valid_username_password(const char *http_url, char *response, const char *url)
{
    char username[USERNAME_PASSWORD_MAX] = {0};
    char password[USERNAME_PASSWORD_MAX] = {0};

    if(!cgi_get_value(http_url, "username", username, USERNAME_PASSWORD_MAX))
    {
        sprintf(response, XML_CGI_FAULT, url, "error, parse username");
        return -1;
    }
    else if(!cgi_get_value(http_url, "password", password, USERNAME_PASSWORD_MAX))
    {
        sprintf(response, XML_CGI_FAULT, url, "error, parse password");
        return -1;
    }

    char ipcpasswd[50] = {0};
    if (UserAuthGetPassword(username, ipcpasswd) != 0)
    {
        sprintf(response, XML_CGI_FAULT, url, "error username");
        return -1;
    }

    urldecode(password, sizeof(password));

    char md5Buf[64] = {0};        
    our_md5_encode(md5Buf, (const unsigned char *)ipcpasswd, strlen(ipcpasswd));

    if( strcasecmp(ipcpasswd, password) != 0 &&
        strcasecmp(md5Buf, password) != 0 )
    {
        sprintf(response, XML_CGI_FAULT, url, "password error");
        return 0;
    }

    return 1;
}


// 判断uid或username,password是否有效, 缺少或无效返回0, 有效返回1
int cgi_is_valid_uid_or_username(const char *http_url, char *response, const char *url)
{
    int status = 0;
    status = cgi_is_valid_username_password(http_url, response, url);
    __ERR("%s: check username/pwd status:%d\n", url ? url : "NULL", status);

    if(status >= 0)
        return status;
    else
    {
        status = cgi_is_valid_uid(http_url, response, url);
    }

    __ERR("%s: chech uid status:%d\n", url ? url : "NULL", status);
    return status;
}


//    时区数字转换成GMT字符串
int timezone_itoa(int zone_num, char *timezone, int max_len)
{
    switch(zone_num)
    {
        case 720:    StrCpy(timezone, max_len, "GMT+00:00");    break;
        case 780:    StrCpy(timezone, max_len, "GMT+01:00");    break;
        case 840:    StrCpy(timezone, max_len, "GMT+02:00");    break;
        case 900:    StrCpy(timezone, max_len, "GMT+03:00");    break;
        case 930:    StrCpy(timezone, max_len, "GMT+03:30");    break;
        
        case 960:    StrCpy(timezone, max_len, "GMT+04:00");    break;
        case 990:    StrCpy(timezone, max_len, "GMT+04:30");    break;
        case 1020:   StrCpy(timezone, max_len, "GMT+05:00");    break;
        case 1050:   StrCpy(timezone, max_len, "GMT+05:30");    break;
        case 1065:   StrCpy(timezone, max_len, "GMT+05:45");    break;

        case 1080:   StrCpy(timezone, max_len, "GMT+06:00");    break;
        case 1110:   StrCpy(timezone, max_len, "GMT+06:30");    break;
        case 1140:   StrCpy(timezone, max_len, "GMT+07:00");    break;
        case 1200:   StrCpy(timezone, max_len, "GMT+08:00");    break;
        case 1260:   StrCpy(timezone, max_len, "GMT+09:00");    break;

        case 1290:   StrCpy(timezone, max_len, "GMT+09:30");    break;
        case 1320:   StrCpy(timezone, max_len, "GMT+10:00");    break;
        case 1380:   StrCpy(timezone, max_len, "GMT+11:00");    break;
        case 1440:   StrCpy(timezone, max_len, "GMT+12:00");    break;
        case 1500:   StrCpy(timezone, max_len, "GMT+13:00");    break;

        case 660:    StrCpy(timezone, max_len, "GMT-01:00");    break;
        case 600:    StrCpy(timezone, max_len, "GMT-02:00");    break;
        case 540:    StrCpy(timezone, max_len, "GMT-03:00");    break;
        case 510:    StrCpy(timezone, max_len, "GMT-03:30");    break;
        case 480:    StrCpy(timezone, max_len, "GMT-04:00");    break;

        case 450:    StrCpy(timezone, max_len, "GMT-04:30");    break;
        case 420:    StrCpy(timezone, max_len, "GMT-05:00");    break;
        case 360:    StrCpy(timezone, max_len, "GMT-06:00");    break;
        case 300:    StrCpy(timezone, max_len, "GMT-07:00");    break;
        case 240:    StrCpy(timezone, max_len, "GMT-08:00");    break;

        case 180:    StrCpy(timezone, max_len, "GMT-09:00");    break;
        case 120:    StrCpy(timezone, max_len, "GMT-10:00");    break;
        case 60:     StrCpy(timezone, max_len, "GMT-11:00");    break;
        case 0:      StrCpy(timezone, max_len, "GMT-12:00");    break;

        default:     StrCpy(timezone, max_len, "GMT-12:00");    break;
    }
    return 0;
}


//    时区GMT字符串转换成数字 失败时返回-1
int timezone_atoi(char *timezone)
{
    if(!strcmp(timezone,"GMT+00:00"))    return 720;
    else if(!strcmp(timezone,"GMT+01:00"))    return 780;
    else if(!strcmp(timezone,"GMT+02:00"))    return 840;
    else if(!strcmp(timezone,"GMT+03:00"))    return 900;
    else if(!strcmp(timezone,"GMT+03:30"))    return 930;

    else if(!strcmp(timezone,"GMT+04:00"))    return 960;
    else if(!strcmp(timezone,"GMT+04:30"))    return 990;
    else if(!strcmp(timezone,"GMT+05:00"))    return 1020;
    else if(!strcmp(timezone,"GMT+05:30"))    return 1050;
    else if(!strcmp(timezone,"GMT+05:45"))    return 1065;

    else if(!strcmp(timezone,"GMT+06:00"))    return 1080;
    else if(!strcmp(timezone,"GMT+06:30"))    return 1110;
    else if(!strcmp(timezone,"GMT+07:00"))    return 1140;
    else if(!strcmp(timezone,"GMT+08:00"))    return 1200;
    else if(!strcmp(timezone,"GMT+09:00"))    return 1260;

    else if(!strcmp(timezone,"GMT+09:30"))    return 1290;
    else if(!strcmp(timezone,"GMT+10:00"))    return 1320;
    else if(!strcmp(timezone,"GMT+11:00"))    return 1380;
    else if(!strcmp(timezone,"GMT+12:00"))    return 1440;
    else if(!strcmp(timezone,"GMT+13:00"))    return 1500;

    else if(!strcmp(timezone,"GMT-01:00"))    return 660;
    else if(!strcmp(timezone,"GMT-02:00"))    return 600;
    else if(!strcmp(timezone,"GMT-03:00"))    return 540;
    else if(!strcmp(timezone,"GMT-03:30"))    return 510;
    else if(!strcmp(timezone,"GMT-04:00"))    return 480;

    else if(!strcmp(timezone,"GMT-04:30"))    return 450;
    else if(!strcmp(timezone,"GMT-05:00"))    return 420;
    else if(!strcmp(timezone,"GMT-06:00"))    return 360;
    else if(!strcmp(timezone,"GMT-07:00"))    return 300;
    else if(!strcmp(timezone,"GMT-08:00"))    return 240;

    else if(!strcmp(timezone,"GMT-09:00"))    return 180;
    else if(!strcmp(timezone,"GMT-10:00"))    return 120;
    else if(!strcmp(timezone,"GMT-11:00"))    return 60;
    else if(!strcmp(timezone,"GMT-12:00"))    return 0;
    
    return -1;
}

//    校验日期&时间格式 正确返回1 没有返回0 错误返回-1
int verify_date_time_format(char *year, char *month, char *day, 
                                    char *hour, char *min, char *sec,
                                    int hasYear, int hasMonth, int hasDay,
                                    int hasHour, int hasMin, int hasSec)
{
    if((!hasYear) && (!hasMonth) && (!hasDay) && (!hasHour) && (!hasMin) && (!hasSec))
        return 0;
    if((!hasYear) || (!hasMonth) || (!hasDay) || (!hasHour) || (!hasMin) || (!hasSec))
        return -1;

    int i_year = 0;
    int i_month = 0;
    int i_day = 0;
    int i_hour = 0;
    int i_min = 0;
    int i_sec = 0;

    i_year = safeatoi(year);
    i_month = safeatoi(month);
    i_day = safeatoi(day);
    i_hour = safeatoi(hour);
    i_min = safeatoi(min);
    i_sec = safeatoi(sec);
    if((i_year < 0) || (i_month < 0) || (i_day < 0) || (i_hour < 0) || (i_min < 0) || (i_sec < 0))
        return -1;

    sprintf(year, "%04d", i_year);    
    sprintf(month, "%02d", i_month);
    sprintf(day, "%02d", i_day);
    sprintf(hour, "%02d", i_hour);
    sprintf(min, "%02d", i_min);
    sprintf(sec, "%02d", i_sec);

    return 1;
}

//    判断encode_mode是否合法 合法返回1 非法返回0
int verify_encode_mode_format(int id, char *encode_mode)
{
    RESOLUTION_ENTRY *pVideo = NULL;
    int vcount = anj_sysmng_video_res_array_get(&pVideo);
    int iIndex = 0;
    for(iIndex = 0; iIndex < vcount; iIndex++)
    {
        if((id == (pVideo[iIndex].stream_type+1)) &&
            (!strcmp(pVideo[iIndex].codec_name, encode_mode)))
        {
            return 1;
        }
    }
    
    return 0;
}

//    判断分辨率是否合法 合法返回1 非法返回0
int verify_resolution_format(int id, char *encode_mode, char *resolution)
{
    RESOLUTION_ENTRY *pVideo = NULL;
    int vcount = anj_sysmng_video_res_array_get(&pVideo);
    int iIndex = 0;

    for(iIndex = 0; iIndex < vcount; iIndex++)
    {
        if((id == (pVideo[iIndex].stream_type + 1)) &&
            (!strcmp(pVideo[iIndex].codec_name, encode_mode))&&
            (!strcmp(resolution, pVideo[iIndex].res_name)))
        {
            return 1;
        }
    }
    
    return 0;
}

//    判断framerate是否合法 合法返回framerate 非法返回0
int verify_framerate_format(int id, char *encode_mode, char *resolution, char *framerate)
{
    int i_framerate = safeatoi(framerate);
    if(i_framerate < 0)
        return 0;

    RESOLUTION_ENTRY *pVideo = NULL;
    int vcount = anj_sysmng_video_res_array_get(&pVideo);
    int iIndex = 0;

    for(iIndex = 0; iIndex < vcount; iIndex++)
    {
        if((id == (pVideo[iIndex].stream_type + 1)) &&
            (!strcmp(pVideo[iIndex].codec_name, encode_mode)) &&
            (!strcmp(resolution, pVideo[iIndex].res_name)) &&
            (i_framerate >= pVideo[iIndex].min_framerate) &&
            (i_framerate <= pVideo[iIndex].max_display_framerate))
        {
            return i_framerate;
        }
    }

    return 0;
}

//    判断govlength是否合法 合法返回govlength 非法返回0
int verify_govlength_format(int framerate, char *govlength)
{
    int i_govlength = safeatoi(govlength);
    if(i_govlength < 0)
        return 0;

    if((i_govlength >= framerate) && (i_govlength <= (framerate * 4)))
        return i_govlength;
    
    return 0;
}

//    判断bitrate是否合法 合法返回bitrate 非法返回0
int verify_bitrate_format(int id, char *encode_mode, char *resolution, char *bitrate)
{
    int i_bitrate = safeatoi(bitrate);
    if(i_bitrate < 0)
        return 0;

    RESOLUTION_ENTRY *pVideo = NULL;
    int vcount = anj_sysmng_video_res_array_get(&pVideo);
    int iIndex = 0;
    for(iIndex = 0; iIndex < vcount; iIndex++)
    {
        if((id == (pVideo[iIndex].stream_type + 1)) &&
            (!strcmp(pVideo[iIndex].codec_name, encode_mode)) &&
            (!strcmp(resolution, pVideo[iIndex].res_name)) &&
            (i_bitrate >= pVideo[iIndex].min_bitrate) &&
            (i_bitrate <= pVideo[iIndex].max_bitrate))
        {
            return i_bitrate;
        }
    }
    
    return 0;
}
    
//    判断音频encode_type是否合法 合法返回1 非法返回0
int verify_a_encodetype_format(char *encode_type)
{
    AUDIO_CODEC_ENTRY *pAudio = NULL;
    int acount = anj_sysmng_audio_res_array_get(&pAudio);
    int iIndex = 0;

    for(iIndex = 0; iIndex < acount; iIndex++)
    {
        if(!strcmp(encode_type, pAudio[iIndex].codec_name))
        {
            return 1;
        }
    }
    
    return 0;
}

//    判断音频samplerate是否合法 合法返回samplerate 非法返回0
int verify_a_samplerate_format(char *encode_type, char *samplerate)
{
    int i_samplerate = safeatoi(samplerate);
    if(i_samplerate < 0)
        return 0;

    AUDIO_CODEC_ENTRY *pAudio = NULL;
    int acount = anj_sysmng_audio_res_array_get(&pAudio);
    int iIndex = 0;
    for(iIndex = 0; iIndex < acount; iIndex++)
    {
        if((!strcmp(encode_type,pAudio[iIndex].codec_name)) &&
            (i_samplerate==(pAudio[iIndex].samplerate*1000)))
        {
            return i_samplerate;
        }
    }
    
    return 0;
}

//    判断音频bitrate是否合法 合法返回bitrate 非法返回0
int verify_a_bitrate_format(char *encode_type, char *samplerate, char *bitrate)
{
    int i_samplerate = safeatoi(samplerate);
    if(i_samplerate < 0)
        return 0;

    int i_bitrate = safeatoi(bitrate);
    if(i_bitrate < 0)
        return 0;

    AUDIO_CODEC_ENTRY *pAudio = NULL;
    int acount = anj_sysmng_audio_res_array_get(&pAudio);
    int iIndex = 0;

    for(iIndex = 0; iIndex < acount; iIndex++)
    {
        if((!strcmp(encode_type, pAudio[iIndex].codec_name)) &&
            (i_samplerate == (pAudio[iIndex].samplerate*1000)) &&
            (i_bitrate == (pAudio[iIndex].bitrate*1000)))
        {
            return i_bitrate;
        }
    }
    
    return 0;
}

//    判断OSD时间格式 合法返回1 非法返回0
int verify_osd_time_format(char *time_format)
{
    int iRet = 0;
    if(!strcmp(time_format,"yyyy-mm-dd_hh:mm:ss"))          iRet = 1;
    else if(!strcmp(time_format,"yyyy/mm/dd_hh:mm:ss"))     iRet = 1;
    else if(!strcmp(time_format,"yy-mm-dd_hh:mm:ss"))       iRet = 1;
    else if(!strcmp(time_format,"yy/mm/dd_hh:mm:ss"))       iRet = 1;
    else if(!strcmp(time_format,"hh:mm:ss_dd/mm/yyyy"))     iRet = 1;
    else if(!strcmp(time_format,"hh:mm:ss_dd-mm-yyyy"))     iRet = 1;
    else if(!strcmp(time_format,"hh:mm:ss_mm/dd/yyyy"))     iRet = 1;
    else if(!strcmp(time_format,"hh:mm:ss_mm-dd-yyyy"))     iRet = 1;
    else if(!strcmp(time_format,"mm/dd/yyyy_hh:mm:ss"))     iRet = 1;
    else if(!strcmp(time_format,"mm-dd-yyyy_hh:mm:ss"))     iRet = 1;

    if(iRet == 1)
    {
        char *p = strchr(time_format, '_');
        if(p!=NULL)
        {
            *p = ' ';
        }
    }
    
    return iRet;
}

int verify_motion_blockcount_format(char *blockcout)
{
    int iRet = 0;
    
    if(!strcasecmp(blockcout, "1x1"))           iRet = 1;
    else if(!strcasecmp(blockcout, "2x2"))      iRet = 1;
    else if(!strcasecmp(blockcout, "3x2"))      iRet = 1;    
    else if(!strcasecmp(blockcout, "3x3"))      iRet = 1;
    else if(!strcasecmp(blockcout, "4x3"))      iRet = 1;    
    else if(!strcasecmp(blockcout, "4x4"))      iRet = 1;
    else if(!strcasecmp(blockcout, "8x8"))      iRet = 1;
    else if(!strcasecmp(blockcout, "16x16"))    iRet = 1;
    else if(!strcasecmp(blockcout, "22x18"))    iRet = 1;

    return iRet;
}

int transform_hex2dec(char hex)
{
    int dec = 0;
    switch(hex)
    {
        case 'a': dec=10; break;
        case 'A': dec=10; break;
        case 'b': dec=11; break;
        case 'B': dec=11; break;
        case 'c': dec=12; break;
        case 'C': dec=12; break;
        case 'd': dec=13; break;
        case 'D': dec=13; break;
        case 'e': dec=14; break;
        case 'E': dec=14; break;
        case 'f': dec=15; break;
        case 'F': dec=15; break;
        case '0': dec=0; break;
        case '1': dec=1; break;
        case '2': dec=2; break;
        case '3': dec=3; break;
        case '4': dec=4; break;
        case '5': dec=5; break;
        case '6': dec=6; break;
        case '7': dec=7; break;
        case '8': dec=8; break;
        case '9': dec=9; break;
        default : dec=0;break;
    }
    
    return dec;
}

int transform_osd_title_utf8(char *src, char *dst, int max_len)
{
    int iIndex = 0;
    int jIndex = 0;
    int src_len = 0;
    src_len = strlen(src);

    for(iIndex = 0; (iIndex < src_len) && (jIndex < max_len); iIndex++, jIndex++)
    {
        if((src[iIndex] == '%') && (iIndex < (src_len-2))&&
            (((src[iIndex+1] >= '0') && (src[iIndex+1] <= '9')) || ((src[iIndex+1] >= 'a')&&(src[iIndex+1] <= 'f'))||((src[iIndex+1] >= 'A')&&(src[iIndex+1] <= 'F'))) &&
            (((src[iIndex+2] >= '0') && (src[iIndex+2] <= '9')) || ((src[iIndex+2] >= 'a')&&(src[iIndex+2] <= 'f'))||((src[iIndex+2] >= 'A')&&(src[iIndex+2] <= 'F'))))
        {
            dst[jIndex] = transform_hex2dec(src[iIndex+1])*16 + transform_hex2dec(src[iIndex+2]);
            iIndex += 2;
        }
        else
        {
            dst[jIndex] = src[iIndex];
        }
    }

    dst[jIndex] = '\0';
    return jIndex;
}

int get_area_value(char *areabyte, char *areavalue, char *blockCfg, int rows, int cols, int enable, int Offset)
{
    int bits;
    int i, j;

    int nbyte = (cols * rows) / 8;
    if((cols * rows) % 8 != 0)
        nbyte += 1;
    
    int index = 0;
    const int offset = Offset;

    if(enable != 0)
    {
        for(i = 0; i < nbyte; i++)
        {
            for(j = 0; j < 8; j++)
            {
                index = (i * 8) + j;
                if(index >= (rows * cols))
                    break;

                if(blockCfg[index] == 0)
                    break;
                
                bits = blockCfg[index] != '0'?1:0;
                areabyte[i] |= (bits << j); 
            }
        }
    }

    for(i = 0; i < nbyte; i++)
    {
        sprintf(areavalue + offset * i, "%02x", areabyte[i]);
    }

    return 0;
}

void base64Encode1(unsigned char *buf, int buf_size, char *base64)
{
    unsigned char tmp[3];
    int i;
    for (i = 0; i < buf_size / 3; i++)
    {
        tmp[0] = buf[i * 3];
        tmp[1] = buf[i * 3 + 1];
        tmp[2] = buf[i * 3 + 2];
        base64[i * 4] = cgi_base64chr[tmp[0] >> 2];
        base64[i * 4 + 1] = cgi_base64chr[(tmp[0] << 4 | tmp[1] >> 4) & 0x3F];
        base64[i * 4 + 2] = cgi_base64chr[(tmp[1] << 2 | tmp[2] >> 6) & 0x3F];
        base64[i * 4 + 3] = cgi_base64chr[tmp[2] & 0x3F];

    }
    if (buf_size % 3 == 2)
    {
        tmp[0] = buf[i * 3];
        tmp[1] = buf[i * 3 + 1];
        base64[i * 4] = cgi_base64chr[tmp[0] >> 2];
        base64[i * 4 + 1] = cgi_base64chr[(tmp[0] << 4 | tmp[1] >> 4) & 0x3F];
        base64[i * 4 + 2] = cgi_base64chr[(tmp[1] << 2) & 0x3F];
        base64[i * 4 + 3] = '=';
    }
    else if (buf_size % 3 == 1)
    {
        tmp[0] = buf[i * 3];
        base64[i * 4] = cgi_base64chr[tmp[0] >> 2];
        base64[i * 4 + 1] = cgi_base64chr[(tmp[0] << 4) & 0x3F];
        base64[i * 4 + 2] = '=';
        base64[i * 4 + 3] = '=';
    }

}

char* Jpg_to_Base64(char *Pathjpg)
{
    if(NULL == Pathjpg)
        return NULL;

    char *base64 = NULL;
    unsigned long long len = 0;
    anj_mw_read_file_len(Pathjpg, &len);
    if (len == 0)
        return NULL;

    char *buf = anj_mw_read_file_buffer(Pathjpg);
    if (buf == NULL)
        return NULL;

#if 0
    //printf("共读取%d个字节.\n", len);
    //printf("16进制码:\n");
    for (int i = 0; i < len; i++)
    {
        if (i != 0 && i % 24 == 0) printf("\n");
        printf("%02x ", buf[i]);
    }
    printf("\n");
#endif // DEBUG

    int base64_len = len / 3 * 4 + (len % 3 != 0) * 4 + 1;
    base64 = (char*)anj_mw_malloc(base64_len);
    memset(base64, '\0', base64_len);

    base64Encode1((unsigned char *)buf, len, base64);

    anj_mw_free(buf);
    buf = NULL;

    return base64;
}


// 比较url 不一致返回0, 缺少参数返回400, 正确返回 200
int cgi_compare_url(const char *http_url, char *response, const char *url)
{
    __ERR("http_url:%s, url:%s\n", http_url, url);

    if(strncmp(http_url, url, strlen(url)))
    {
        return 0;
    }

    const char *pIndex = http_url + strlen(url);
    if(*pIndex != '?')
    {
        __ERR("*pIndex=%s\n", pIndex);
        sprintf(response, XML_CGI_FAULT, url, "error, parse parameters");
        return 400;
    }

    return 200;
}

//    判断地址是否合法 合法返回1 非法返回0
int cgi_is_legal_ipaddr(char *ipaddr, int ip_type)
{
    char *pIndex = NULL;
    char *pAddr = ipaddr;
    int iIndex = 0;
    int iRet = 0;
    unsigned char addr[4] = {0};
    char strAddr[4][4];

    memset(strAddr, 0, 4*4*sizeof(char));

    // 按'.'拆分地址
    for(iIndex = 0; iIndex < 3; iIndex++)
    {
        if(*pAddr == '\0')
            return 0;

        pIndex = strchr(pAddr, '.');
        if(pIndex==NULL)
            return 0;
        strnsafecpy(strAddr[iIndex], pAddr, pIndex-pAddr, 4);
        pAddr = pIndex + 1;
    }
    
    if(*pAddr == '\0')
        return 0;

    strnsafecpy(strAddr[3], pAddr, strlen(pAddr), 4);

    //转换成数字
    for(iIndex = 0; iIndex < 4; iIndex++)
    {
        iRet = safeatoi(strAddr[iIndex]);
        if((iRet < 0) || (iRet > 255))
            return 0;

        addr[iIndex] = iRet;
    }

    if(ip_type == TYPE_IPADDR)
    {
        if((addr[0]>=1) && (addr[0]<=223)&& 
            (addr[1]>=0) && (addr[1]<=255)&& 
            (addr[2]>=0) && (addr[2]<=255)&& 
            (addr[3]>=1) && (addr[3]<=254))
        {
            return 1;
        }
    }
    else if(ip_type == TYPE_NETMASK)
    {
        if((addr[0]>=0) && (addr[0]<=255)&& 
            (addr[1]>=0) && (addr[1]<=255)&& 
            (addr[2]>=0) && (addr[2]<=255)&& 
            (addr[3]>=0) && (addr[3]<=255))
        {
            return 1;
        }
    }
    else if(ip_type == TYPE_GATEWAY)
    {
        if((addr[0]>=1) && (addr[0]<=223)&& 
            (addr[1]>=0) && (addr[1]<=255)&& 
            (addr[2]>=0) && (addr[2]<=255)&& 
            (addr[3]>=1) && (addr[3]<=254))
        {
            return 1;
        }
    }
    else if(ip_type == TYPE_DNS)
    {
        if((addr[0]>=1) && (addr[0]<=223)&& 
            (addr[1]>=0) && (addr[1]<=255)&& 
            (addr[2]>=0) && (addr[2]<=255)&& 
            (addr[3]>=1) && (addr[3]<=254))
        {
            return 1;
        }
    }

    return 0;
}

/******************************************************************************\
 *
 *    HTTP CGI INTERFACE
 *
 \******************************************************************************/


/* 没有匹配的cgi url       */
int cgi_no_interface(const char *url_name, const char *clientip, const char *http_url,char *response)
{
    sprintf(response, XML_CGI_FAULT, http_url, "no such cgi interface");
    return HTTP_RES_STATUS_NOT_FOUND;
}

//    根据username和password分配uid
int cgi_getuid(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    char username[USERNAME_PASSWORD_MAX] = {0};
    char password[USERNAME_PASSWORD_MAX] = {0};
    if(cgi_get_value(http_url, "username", username, USERNAME_PASSWORD_MAX) == 0)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, parse username");
        return 400;
    }
    else if(cgi_get_value(http_url, "password", password, USERNAME_PASSWORD_MAX) == 0)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, parse password");
        return 400;
    }
    
    char ipcpasswd[50] = {0};
    if (UserAuthGetPassword(username, ipcpasswd) != 0)
    {
        __ERR("MsgUserGetPassword failed for %s\n",username);
        sprintf(response, XML_CGI_FAULT, url_name, "password error");
        return 400;
    }

    urldecode(password, sizeof(password));
    
    char md5Buf[64] = {0};        
    our_md5_encode(md5Buf, (const unsigned char *)ipcpasswd, strlen(ipcpasswd));

    if( strcasecmp(ipcpasswd, password) != 0 &&
        strcasecmp(md5Buf, password) != 0)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "password error");
        return 400;
    }

    string uid = cgi_session_id_get();
    sprintf(response, XML_HEAD"<uid>%s</uid>", uid.c_str());
    return 200;    
}

//    心跳,防止超时uid失效
int cgi_keep_alive(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url,response,url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    sprintf(response, XML_HEAD"<keep_alive>true</keep_alive>");
    return 200;
}

// 获取用户ip
int cgi_user_ip(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
        return status;

    sprintf(response, XML_HEAD"<user_ip>%s</user_ip>", clientip);
    return 200;
}

//设置uid存活时间
int cgi_set_uid_valid_time(const char *url_name, const char *clientip, const char *http_url,char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    if(!isValid)
    {
        return 400;
    }

    char validTime[8] = {0};
    if(cgi_get_value(http_url, "validTime", validTime, sizeof(validTime)))
    {
        int validtm = safeatoi(validTime);
        if((validtm <= 0))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "Time range: > 0");
            status = 400;
            goto __exit;
        }

        s_SessionIdVaildTime = validtm;
        __ERR("Cgi set uid valid time is %d sec.\n", s_SessionIdVaildTime);
        
        sprintf(response, XML_HEAD"<SetUidValidTime>Uid valid time is %s sec</SetUidValidTime>", validTime);
        status = 200;
    }

__exit:
    return status;    
}


//添加重新分区的CGI，解决上传音频失败
int cgi_flash_eraseall(const char *url_name, const char *clientip, const char *http_url,char *response)
{
    int status = cgi_compare_url(http_url,response,url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url,response,url_name) == 0)
    {
        return 400;
    }

    anj_sysmng_eraseall_mp3();
    
    sprintf(response, XML_HEAD"<flash_eraseall>true</flash_eraseall>");
    return 200;
}

//获取录像的文件列表
int cgi_get_record_query(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char *xmlBuf = NULL;
    char stream_index[10];
    char record_mode[10];
    char media_type[10];
    char record_year[10];
    char record_month[10];
    char record_day[10];
    char start_hour[10];
    char start_min[10];
    char start_sec[10];
    char end_hour[10];
    char end_min[10];
    char end_sec[10];
        
    // 初始化
    int i_stream_index = -1;
    int i_record_mode = -1;
    int i_media_type = 3;

    struct tm start_time = {0};
    struct tm end_time = {0};
    start_time.tm_hour = 0;
    start_time.tm_min = 0;
    start_time.tm_sec = 0;
    end_time.tm_hour = 23;
    end_time.tm_min = 59;
    end_time.tm_sec = 59;

    // 获取用户输入的参数
    if (cgi_get_value(http_url, "stream", stream_index, 10))
    {
        if ((strcmp(stream_index, "-1") == 0) || (strcmp(stream_index, "1") == 0) || (strcmp(stream_index, "2") == 0))
        {
            if (strcmp(stream_index, "-1") == 0)
            {
                i_stream_index = -1;
            }
            if (strcmp(stream_index, "1") == 0)
            {
                i_stream_index = 1;
            }
            if (strcmp(stream_index, "2") == 0)
            {
                i_stream_index = 2;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "stream: 1 or 2 or -1");
            return 400;
        }
    }

    if (cgi_get_value(http_url, "record_mode", record_mode, 10))
    {
        if ((strcmp(record_mode, "-1") == 0) || (strcmp(record_mode, "2") == 0) || 
            (strcmp(record_mode, "4") == 0) || (strcmp(record_mode, "8") == 0))
        {
            if (strcmp(record_mode, "-1") == 0)
            {
                i_record_mode = REC_EVENT_ALL;
            }
            if (strcmp(record_mode, "2") == 0)
            {
                i_record_mode = REC_EVENT_NONE;
            }
            if (strcmp(record_mode, "4") == 0)
            {
                i_record_mode = REC_EVENT_MOTION_ALARM_MASK;
            }
            if (strcmp(record_mode, "8") == 0)
            {
                i_record_mode = REC_EVENT_MOTION_ALARM_MASK;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "record_mode: error should be -1,2,4,8");
            return 400;
        }
    }

    if (cgi_get_value(http_url, "media_type", media_type, sizeof(media_type)))
    {
        if ((strcmp(media_type, "2") == 0) || (strcmp(media_type, "3") == 0) || (strcmp(media_type, "4") == 0))
        {
            if (strcmp(media_type, "2") == 0)
            {
                i_media_type = SERACH_FILE_MEDIA_TYPE_VIDEO;
            }
            if (strcmp(media_type, "3") == 0)
            {
                i_media_type = SERACH_FILE_MEDIA_TYPE_AUDIOVIDEO;
            }
            if (strcmp(media_type, "4") == 0)
            {
                i_media_type = SERACH_FILE_MEDIA_TYPE_IMAGE;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "media_type: error should be 2,3,4");
            return 400;
        }
    }

    if (cgi_get_value(http_url, "year", record_year, sizeof(record_year)))
    {
        start_time.tm_year = atoi(record_year) - 1900;
    }

    if (cgi_get_value(http_url, "month", record_month, sizeof(record_month)))
    {
        if ((atoi(record_month) >= 1) && (atoi(record_month) <= 12))
        {
            start_time.tm_mon = atoi(record_month) - 1;    // 里面月份需要-1
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "record_month should between 1-12");
            return 400;
        }
    }

    if (cgi_get_value(http_url, "day", record_day, sizeof(record_day)))
    {
        if ((atoi(record_day) >= 1) && (atoi(record_day) <= 31))
        {
            start_time.tm_mday = atoi(record_day);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "record_day should between 1-31");
            return 400;
        }
    }
    
    if (cgi_get_value(http_url, "start_hour", start_hour, sizeof(start_hour)))
    {
        if ((atoi(start_hour) >= 0) && (atoi(start_hour) <= 23))
        {
            start_time.tm_hour = atoi(start_hour);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "start_hour should between 0-23");
            return 400;
        }
    }
    
    if (cgi_get_value(http_url, "start_min", start_min, sizeof(start_min)))
    {
        if ((atoi(start_min) >= 0) && (atoi(start_min) <= 59))
        {
            start_time.tm_min = atoi(start_min);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "start_min should between 0-59");
            return 400;
        }
    }
    
    if (cgi_get_value(http_url, "start_sec", start_sec, sizeof(start_sec)))
    {
        if ((atoi(start_sec) >= 0) && (atoi(start_sec) <= 59))
        {
            start_time.tm_sec = atoi(start_sec);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "start_sec should between 0-59");
            return 400;
        }
    }
    
    if (cgi_get_value(http_url, "end_hour", end_hour, sizeof(end_hour)))
    {
        if ((atoi(end_hour) >= 0) && (atoi(end_hour) <= 23))
        {
            end_time.tm_hour = atoi(end_hour);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "end_hour should between 0-23");
            return 400;
        }
    }
    
    if (cgi_get_value(http_url, "end_min", end_min, sizeof(end_min)))
    {
        if ((atoi(end_min) >= 0) && (atoi(end_min) <= 59))
        {
            end_time.tm_min = atoi(end_min);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "end_min should between 0-59");
            return 400;
        }
    }
    
    if (cgi_get_value(http_url, "end_sec", end_sec, sizeof(end_sec)))
    {
        if ((atoi(end_sec) >= 0) && (atoi(end_sec) <= 59))
        {
            end_time.tm_sec = atoi(end_sec);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "end_sec should between 0-59");
            return 400;
        }
    }

    int s = start_time.tm_hour * 3600 + start_time.tm_min * 60 + start_time.tm_sec;
    int e = end_time.tm_hour * 3600 + end_time.tm_min * 60 + end_time.tm_sec;

    __DBG("cgi query record stream:%d, record mode:%d, media type:%d!\n", i_stream_index, i_record_mode, i_media_type);

    if (e <= s)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "End time needs to be later than start time");
        return 400;
    }

    // 这些是默认的参数
    // 默认只能一天一天的读取录像文件
    end_time.tm_year = start_time.tm_year;
    end_time.tm_mon = start_time.tm_mon;
    end_time.tm_mday = start_time.tm_mday;

    record_query_result_s *pstRecQueryResult = (record_query_result_s *)anj_mw_malloc(sizeof(record_query_result_s));
    if (NULL == pstRecQueryResult)
    {
        return 400;
    }

    memset(pstRecQueryResult, 0, sizeof(record_query_result_s));

#if 0
    rec_pb_date_s stPbDate = {0};
    rec_pb_list_s stPbList = {0};
    //int rec_chn = (i_stream_index == 0) ? 0 : 1;

    stPbDate.tEvent = i_record_mode;
    stPbDate.year = start_time.tm_year + 1900;
    stPbDate.month = start_time.tm_mon + 1;
    stPbDate.day = start_time.tm_mday;
#endif

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
        //int tEvent = REC_EVENT_SET_MASK(pstIndexRecord->tMediaFileEvent, REC_EVENT_NULL_MASK);
        if (pstIndexRecord->iMediaFileStatus == REC_STATUS_NULL)
        {
            continue;
        }

        if (pstRecQueryResult->count < REC_SEGMENT_MAX_COUNT)
        {
            anj_record_get_media_file_name(pstRecQueryResult->items[pstRecQueryResult->count].filepath, sizeof(pstRecQueryResult->items[pstRecQueryResult->count].filepath), i);
            pstRecQueryResult->items[pstRecQueryResult->count].filesize = REC_MEDIA_FILE_SIZE;
            pstRecQueryResult->count++;
        }
    }

    http_query_record_set(0);

    xmlBuf = xml_conver_record_query_info(pstRecQueryResult);

    if (pstRecQueryResult)
    {
        anj_mw_free(pstRecQueryResult);
        pstRecQueryResult = NULL;
    }

    if (xmlBuf == NULL)
    {
        return 400;
    }

    sprintf(response, XML_HEAD "%s", xmlBuf);
    return 200;
}

//用户管理
int cgi_User_Management(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char myusername[100] = {0};
    char mypassword[100] = {0};
    char secureLogin[ACCOUNT_STATUS_MAX_LEN] = {0};

    int bsetUserManagementFlag = 0;
    int i = 0;
    int pos = 0;
    int iRet = 0;
    int bool_get_user = 0;
    int Passwd_Flag = 0;
    int add_Flag = 0;

    int privilege_Flag = 0;
    char mygroup[32] = {0};

    int enable_Flag = 0;
    char mystatus[10] = {0};

    int Name_Flag = 0;
    char action[10] = {0};

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    UserConfig *pstUserCfg = &pstSystemCfg->userCfg;

//判断登录用户的权限    ,或者使用uid，否则上报错误    
    if(cgi_get_value(http_url, "username", myusername, sizeof(myusername)) || 
        cgi_get_value(http_url, "uid", myusername, sizeof(myusername)))
    {
        for(i = 0; i < pstUserCfg->count && i < MAX_ACCOUNT_COUNT; i++)
        {
            if(strcasecmp(pstUserCfg->accounts[i].userName, myusername) == 0)
            {
                //如果登录用户权限不是管理员权限，则无法修改用户权限
                if(strcmp(pstUserCfg->accounts[i].group.groupName, "Administrator") != 0)
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "Non administrator cannot manage user");
                    status = 400;
                    goto __exit;                                
                }
            }
        }
    }
    else//登录用户不存在
    {
            sprintf(response, XML_CGI_FAULT, url_name, "username not exist to edit");
            status = 400;
            goto __exit;
    }

    if(cgi_get_value(http_url, "action", action, sizeof(action)))
    {
        if(strcmp(action, "add") == 0)
        {
            //不能重复添加相同的名字
            if(cgi_get_value(http_url, "name", myusername, sizeof(myusername)))
            {
                if(strcmp(myusername, "admin") == 0)
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "name: can not create name:admin");
                    status = 400;
                    goto __exit;
                }

                for(i = 0; i < pstUserCfg->count && i < MAX_ACCOUNT_COUNT; i++)
                {
                    if(strcasecmp(pstUserCfg->accounts[i].userName, myusername) == 0)
                    {
                        sprintf(response, XML_CGI_FAULT, url_name, "error username duplicated");
                        status = 400;
                        goto __exit;
                    }
                }

                Name_Flag = 0;
            }
            else
            {
                Name_Flag = 1;
            }

            if(cgi_get_value(http_url, "newpasswd", mypassword, sizeof(mypassword)) != 0)
            {
                if(strlen(mypassword) == 0)
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "error newpasswd: NULL");
                    status = 400;
                    goto __exit;
                }

                Passwd_Flag = 0;
            }
            else
            {
                Passwd_Flag = 1;
            }

            if(cgi_get_value(http_url, "privilege", mygroup, sizeof(mygroup)))
            {
                if((strcmp(mygroup, "User") == 0) || (strcmp(mygroup, "Administrator") == 0) || (strcmp(mygroup, "Operator") == 0))
                {
                    ;
                }
                else
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "privilege: User or Administrator or Operator");
                    status = 400;
                    goto __exit;
                }
                privilege_Flag = 0;
            }
            else
            {
                privilege_Flag = 1;
            }

            if(cgi_get_value(http_url, "enable", mystatus, sizeof(mystatus)) != 0)
            {
                if((strcmp(mystatus, "Enable") == 0) || (strcmp(mystatus, "Disable") == 0))
                {
                    ;
                }
                else
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "enable: Enable or Disable");
                    status = 400;
                    goto __exit;
                }
                enable_Flag = 0;
            }
            else
            {
                enable_Flag = 1;
            }
            
            bool_get_user = 0;
            if(enable_Flag || privilege_Flag || Passwd_Flag || Name_Flag)
            {
                //代表有一些参数丢失，上报错误
                add_Flag = 1;
            }
            else
            {
                add_Flag = 0;
            }

            if(add_Flag == 0)
            {
                UserAuthAddUser(myusername, mypassword, mygroup, mystatus);
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "The parameters should Correct include name,newpasswd,privilege,enable");
                status = 400;
                goto __exit;
            }
        }
        else if(strcmp(action, "delete") == 0)
        {
            //这边删除用户的时候，如果用户名不存在的话，需要报错提示无法删除
            if(cgi_get_value(http_url, "name", myusername, sizeof(myusername)))
            {
                if((strcmp(myusername, "admin") == 0))
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "name: can not delete name:admin");
                    status = 400;
                    goto __exit;
                }

                for(i = 0;i < pstUserCfg->count && i < MAX_ACCOUNT_COUNT; i++)
                {
                    //判断是否有这个用户，有的话删除
                    if(strcasecmp(pstUserCfg->accounts[i].userName, myusername) == 0)
                    {
                        bool_get_user = 0;
                        UserAuthDeleteUser(myusername);
                        bsetUserManagementFlag = 1;
                    }
                }

                //没有用户，输出错误
                if(bsetUserManagementFlag == 0)
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "error username");
                    status = 400;
                    goto __exit;
                }
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "user name is none");
                status = 400;
                goto __exit;      
            }
        }
        else if(strcmp(action, "edit") == 0)
        {
            //这里主要执行编辑用户的操作
            if(cgi_get_value(http_url, "name", myusername, sizeof(myusername)) != 0)
            {
                //如果是admin的用户的话，不能禁用admin用户,不能修改权限，只能修改密码
                if((strcmp(myusername, "admin") == 0))
                {
                    if(cgi_get_value(http_url, "enable", mystatus, sizeof(mystatus)) != 0)
                    {
                        if(strcmp(mystatus, "Disable") == 0)
                        {
                            sprintf(response, XML_CGI_FAULT, url_name, "edit: can not disable:admin");
                            status = 400;
                            goto __exit;
                        }
                    }

                    if(cgi_get_value(http_url, "privilege", mygroup, sizeof(mygroup)))
                    {
                        if(strcmp(mygroup, "Administrator") != 0)
                        {
                            sprintf(response, XML_CGI_FAULT, url_name, "Administrator's rights cannot be modified");
                            status = 400;
                            goto __exit;
                        }
                    }
                }
                
                //查找用户是否存在
                for(i = 0; i < pstUserCfg->count && i < MAX_ACCOUNT_COUNT; i++)
                {
                    //判断是否存在用户，如果存在才编辑
                    if(strcasecmp(pstUserCfg->accounts[i].userName, myusername) == 0)
                    {
                        bsetUserManagementFlag = 1;

                        //下面这些是可选的参数
                        int tmp = cgi_get_value_2(http_url, "newpasswd", mypassword, sizeof(mypassword));

                        //正常编辑密码
                        if(tmp > 0)
                        {
                            bsetUserManagementFlag = 1;
                        }//缺省参数newpasswd ,保留原来的配置
                        else if(tmp== -1)
                        {
                            strcpy(mypassword, pstUserCfg->accounts[i].password);
                            bsetUserManagementFlag = 1;
                        }//判断密码的参数，为空上报错误
                        else
                        {
                            if(strcmp(mypassword, "") == 0)
                            {
                                sprintf(response, XML_CGI_FAULT, url_name, "newpasswd: NULL");
                                status = 400;
                                goto __exit;
                            }                            
                        }
                        //--------------------------------------------------------//
                        tmp = 0;
                        tmp = cgi_get_value_2(http_url, "privilege", mygroup, sizeof(mygroup));
                        if(tmp>0)
                        {
                            if((strcmp(mygroup, "User") == 0) || (strcmp(mygroup, "Administrator") == 0) || (strcmp(mygroup, "Operator") == 0))
                            {
                                ;
                            }
                            else
                            {
                                sprintf(response, XML_CGI_FAULT, url_name, "privilege: User or Administrator or Operator");
                                status = 400;
                                goto __exit;
                            }
                            bsetUserManagementFlag = 1;
                        }
                        else if(tmp == -1)
                        {
                            strcpy(mygroup, pstUserCfg->accounts[i].group.groupName);
                            bsetUserManagementFlag = 1;
                        }
                        else
                        {
                            if(strcmp(mygroup,"") == 0)
                            {
                                sprintf(response, XML_CGI_FAULT, url_name, "privilege: NULL");
                                status = 400;
                                goto __exit;
                            }
                        }

                        //清空
                        tmp = 0;
                        tmp = cgi_get_value_2(http_url, "enable", mystatus, sizeof(mystatus));
                        if(tmp > 0)
                        {
                            if((strcmp(mystatus, "Enable") == 0)||(strcmp(mystatus, "Disable") == 0))
                            {
                                ;
                            }
                            else
                            {
                                sprintf(response, XML_CGI_FAULT, url_name, "enable: Enable or Disable");
                                status = 400;
                                goto __exit;
                            }
                            bsetUserManagementFlag = 1;
                        }
                        else if(tmp == -1)
                        {
                            strcpy(mystatus, pstUserCfg->accounts[i].status);
                            bsetUserManagementFlag = 1;                            
                        }
                        else
                        {
                            if(strcmp(mystatus, "") == 0)
                            {
                                sprintf(response, XML_CGI_FAULT, url_name, "enable: NULL");
                                status = 400;
                                goto __exit;
                            }
                        }
                        
                        __ERR("mangaer myusername=%s,mypassword=%s,mygroup=%s,mystatus=%s\n",
                            myusername,mypassword,mygroup,mystatus);
                        bool_get_user = 0;
                        if(bsetUserManagementFlag)
                        {
                            UserAuthEditUser(myusername, mypassword, mygroup, mystatus, secureLogin);
                        }                            
                    }
                }

                if(bsetUserManagementFlag == 0)
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "username not exist to edit");
                    status = 400;
                    goto __exit;
                }
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "user name is none");
                status = 400;
                goto __exit;
            }
        }
        else if(strcmp(action, "get_user") == 0)
        {
            //在此处进行获取最新的配置文件
            iRet = sprintf(response+pos,XML_HEAD"<UserConfig>\r\n");
            pos += iRet;

            for(i = 0; i < pstUserCfg->count && i < MAX_ACCOUNT_COUNT; i++)
            {
                iRet = sprintf(response+pos,"<Account ");
                pos += iRet;

                //用户名
                iRet = sprintf(response+pos,"Username=\"%s\" ", pstUserCfg->accounts[i].userName);
                pos += iRet;

                //密码
                //ret = sprintf(response+pos,"Password=\"%s\" ",pUserCfg->accounts[i].password);
                //pos += ret;
                
                //权限
                iRet = sprintf(response+pos,"Group=\"%s\" ", pstUserCfg->accounts[i].group.groupName);
                pos += iRet;

                //使能
                iRet = sprintf(response+pos,"Status=\"%s\" ", pstUserCfg->accounts[i].status);
                pos += iRet;

                iRet = sprintf(response+pos,"/>\r\n");
                pos += iRet;
                
            }
            iRet = sprintf(response + pos, "</UserConfig>");
            pos += iRet;

            bool_get_user = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "action: add or edit or delete or get_user");
            status = 400;
            goto __exit;
        }
    }

    if(bool_get_user == 0)
    {
        sprintf(response, XML_HEAD"<user_management>OK</user_management>");
    }
    status = 200;

__exit:
    return  status;
}

//获取设备的型号OEM
int cgi_get_oem(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    char *result = NULL;
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    SYSTEM_VERSION_DATA stSystemVersion = {0};
    anj_sysmng_version_info_get(&stSystemVersion, 0);

    result = strtok( stSystemVersion.fsVersion, "_");
    sprintf(response, XML_HEAD"<model>%s</model>", result);
    return 200;
}

//NFS卸载的指令
int cgi_nfs_remote_umount(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int retStatus = module_uninit_single("anj_nfs");
    __INFO("nfs module uninit status:%d\n", retStatus);

    sprintf(response, XML_HEAD"<umount_nfs>OK</umount_nfs>");
    return 200;
}

//获取当前模组的分辨率
int cgi_module_resolution(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int icameraidx = 0;
    int chn = 0;

    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();

    sprintf(response, XML_HEAD"<module_resolution>%s</module_resolution>",
        pstMediaCfg->videoConfig[icameraidx].videoEncode.encodeCfg[chn].resolution.name);
    
    return 200;
}

void getMemoryInfo(char * Memtotal,char * Memfree)
{
    FILE *fp = anj_mw_fopen("/proc/meminfo", "r");
    if(NULL == fp)
    {
        __ERR("failed to open meminfo\n");
        return;
    }

    char szTest[1000] = {0};
    char * myMemtotal = NULL;
    char * myMemfree = NULL;
    
    while(!feof(fp))
    {
        memset(szTest, 0, sizeof(szTest));
        fgets(szTest, sizeof(szTest) - 1, fp);

        if(strstr(szTest, "MemTotal:") != NULL)
        {
            myMemtotal = strstr(szTest, "MemTotal:");
            myMemtotal = myMemtotal +18;
            strcpy(Memtotal, myMemtotal);    
        }
        if(strstr(szTest, "MemFree:") != NULL)
        {
            myMemfree = strstr(szTest, "MemFree:");
            myMemfree = myMemfree +19;
            strcpy(Memfree, myMemfree);    
        }
    }

    anj_mw_fclose(fp);
}

//    获得系统的状态
int cgi_getDevState(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char SystemTime[128] = {0};
    char RunTime[128] = {0};

    char myMemtotal[64] ={0};
    char myMemfree[64] ={0};

    SYSTEM_TIME system_time;
    struct timeval tv;
    struct tm *ptm, tbuf;
    
    gettimeofday(&tv, NULL);    
    ptm = localtime_r(&tv.tv_sec, &tbuf);
    
    system_time.year = 1900 + ptm->tm_year;
    system_time.month = 1 + ptm->tm_mon;
    system_time.day = ptm->tm_mday;
    system_time.hour = ptm->tm_hour;
    system_time.minute = ptm->tm_min;
    system_time.second = ptm->tm_sec;

//------------获得系统运行时间----------//
    struct sysinfo info;    
    char run_time[128] = {0};    
    if (sysinfo(&info)) 
    { 
        __ERR("Failed to get sysinfo\n");
        return -1;    
    } 
    
    long timenum = info.uptime;
    int runday = timenum/86400;    
    int runhour = (timenum%86400)/3600;    
    int runmin = (timenum%3600)/60;    
    int runsec = timenum%60;

//每次获取完相应的信息后清空buffer
    memset(run_time, 0, sizeof(run_time));

    getMemoryInfo(myMemtotal, myMemfree);

//获取CPU的使用率    
//    __ERR("myMemtotal = %s,myMemfree = %s\n",myMemtotal,myMemfree);
//-----------------------------------------------//
//系统时间
    sprintf(SystemTime, "%d-%d-%d-%d-%d-%d", 
        system_time.year, system_time.month, system_time.day,
        system_time.hour, system_time.minute, system_time.second);

//运行时间
    sprintf(RunTime, "%d-%d-%d-%d", runday, runhour, runmin, runsec);

//SD Card
    anj_sdcard_info stSdInfo = {0};
    anj_sdcard_info_query(&stSdInfo);

    int storage_status = Storage_NONE;
    if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_INSERT)
        storage_status = Storage_Mounting;
    else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_FORMAT)
        storage_status = Storage_FORMATING;
    else if ((stSdInfo.eStatus == ANJ_SDCARD_STATUS_MOUNT) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NOT_INIT))
        storage_status = Storage_UNINITED;
    else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NORMAL)
        storage_status = Storage_OK;
    else if ((stSdInfo.eStatus == ANJ_SDCARD_STATUS_RWERROR) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_RONLY))
        storage_status = Storage_EXEPTION;

    __DBG("query sd storage_status:%d\n", storage_status);
    int bMounted = (stSdInfo.eStatus >= ANJ_SDCARD_STATUS_MOUNT) ? 1 : 0;

    sprintf(response, XML_HEAD"<DevState sd1=\"%d\" sd2=\"%d\" system_time=\"%s\" run_time=\"%s\" MemTotal=\"%s\" MemFree=\"%s\"/>",
        bMounted, 0, SystemTime, RunTime, myMemtotal, myMemfree);

    return 200;
}

//    获得系统的状态
int cgi_setAlarmState(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char position[8]={0};
    int  i_position = 0;
    char logicalstate[8]={0};
    int  i_logicalstate = 0;

    if(cgi_get_value(http_url, "position", position, 8))
    {
         i_position = safeatoi(position);
    
        if((i_position == 0) || (i_position == 1) || (i_position == 2) ||(i_position == 3) || 
            (i_position == 4) || (i_position == 5) ||(i_position == 6) )
        {
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "position: 0 ~ 6");
            status = 400;
            return status;
        }
    }

    if(cgi_get_value(http_url, "logicalstate", logicalstate, sizeof(logicalstate)))
    {
        i_logicalstate = safeatoi(logicalstate);
        if((i_logicalstate == 0) || (i_logicalstate == 1))
        {
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "logicalstate: 0 ~ 1");
            status = 400;
            return status;
        }
    }    

    int iRet = anj_mw_hwctrl_alarmout_chn_status_set(i_position, i_logicalstate);
     if(iRet)
    {
        __ERR("cgi set alarm out chn:%d status:%d failed !!!\n", i_position, i_logicalstate);
    }
    
    sprintf(response, XML_HEAD"<setAlarmState position=\"%s\" logicalstate=\"%s\"/>", position, logicalstate);
    return 200;
}

//增加隐私区域设置API
int cgi_setPrivacyZone(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int bsetFlag = 0;
    
//设置隐私遮挡
    char mainStreamMaskList_height[4][16] = {0};
    char mainStreamMaskList_width[4][16] = {0};
    char mainStreamMaskList_xPos[4][16] = {0};
    char mainStreamMaskList_yPos[4][16] = {0};

//初始化字符串数组
    char mainStreamMaskName_height[4][15] = 
    {
            "Mask1Height",
            "Mask2Height",
            "Mask3Height",
            "Mask4Height"
    };
    char mainStreamMaskName_width[4][15] = 
    {
            "Mask1width",
            "Mask2width",
            "Mask3width",
            "Mask4width"
    };
    char mainStreamMaskName_xPos[4][15] = 
    {
            "Mask1xPos",
            "Mask2xPos",
            "Mask3xPos",
            "Mask4xPos"
    };
    char mainStreamMaskName_yPos[4][15] = 
    {
            "Mask1yPos",
            "Mask2yPos",
            "Mask3yPos",
            "Mask4yPos"
    };

/*
    char subStreamMaskList_height[4][16] = {0};
    char subStreamMaskList_width[4][16] = {0};
    char subStreamMaskList_xPos[4][16] = {0};
    char subStreamMaskList_yPos[4][16] = {0};
*/

    memset(mainStreamMaskList_height, 0, sizeof(mainStreamMaskList_height));
    memset(mainStreamMaskList_width, 0, sizeof(mainStreamMaskList_width));
    memset(mainStreamMaskList_xPos, 0, sizeof(mainStreamMaskList_xPos));
    memset(mainStreamMaskList_yPos, 0, sizeof(mainStreamMaskList_yPos));

/*
    memset(subStreamMaskList_height,0,sizeof(subStreamMaskList_height));
    memset(subStreamMaskList_width,0,sizeof(subStreamMaskList_width));
    memset(subStreamMaskList_xPos,0,sizeof(subStreamMaskList_xPos));
    memset(subStreamMaskList_yPos,0,sizeof(subStreamMaskList_yPos));    
*/

//目前只支持主码流设置
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoMaskConfig *pstVideoMaskCfg = &pstMediaCfg->videoConfig[chn].videoMask;

    int i = 0;
    for(i = 0; i < 4; i++)
    {
        snprintf(mainStreamMaskList_height[i], sizeof(mainStreamMaskList_height[i])-1, "%d", pstVideoMaskCfg->mainStreamMaskList[i].height);
        snprintf(mainStreamMaskList_width[i], sizeof(mainStreamMaskList_width[i])-1, "%d", pstVideoMaskCfg->mainStreamMaskList[i].width); 
        snprintf(mainStreamMaskList_xPos[i], sizeof(mainStreamMaskList_xPos[i])-1, "%d", pstVideoMaskCfg->mainStreamMaskList[i].xPos);
        snprintf(mainStreamMaskList_yPos[i], sizeof(mainStreamMaskList_yPos[i])-1, "%d", pstVideoMaskCfg->mainStreamMaskList[i].yPos);

/*
        snprintf(subStreamMaskList_height[i], sizeof(subStreamMaskList_height[i])-1, "%d", pCfg->subStreamMaskList[i].height);
        snprintf(subStreamMaskList_width[i], sizeof(subStreamMaskList_width[i])-1, "%d", pCfg->subStreamMaskList[i].width); 
        snprintf(subStreamMaskList_xPos[i], sizeof(subStreamMaskList_xPos[i])-1, "%d", pCfg->subStreamMaskList[i].xPos);
        snprintf(subStreamMaskList_yPos[i], sizeof(subStreamMaskList_yPos[i])-1, "%d", pCfg->subStreamMaskList[i].yPos); 
*/
        __ERR("mainStreamMaskName_height = %s\n", mainStreamMaskName_height[i]);

        if(cgi_get_value(http_url, mainStreamMaskName_height[i], mainStreamMaskList_height[i], 16))
        {
            int my_mainStreamMask1Height = safeatoi(mainStreamMaskList_height[i]);    
        
            if(my_mainStreamMask1Height >= 0)
            {
                pstVideoMaskCfg->mainStreamMaskList[i].height = my_mainStreamMask1Height;
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "mainStreamMask_Height is none");
                status = 400;
                goto __exit;
            }
            bsetFlag = 1;
        }

        if(cgi_get_value(http_url, mainStreamMaskName_width[i], mainStreamMaskList_width[i], 16))
        {
            int  my_mainStreamMask1Width = safeatoi(mainStreamMaskList_width[i]);    
        
            if(my_mainStreamMask1Width >= 0)
            {
                pstVideoMaskCfg->mainStreamMaskList[i].width = my_mainStreamMask1Width;
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "mainStreamMask_Width is none");
                status = 400;
                goto __exit;
            }
            bsetFlag = 1;
        }

        if(cgi_get_value(http_url, mainStreamMaskName_xPos[i], mainStreamMaskList_xPos[i], 16))
        {
            int  my_mainStreamMask1xPos = safeatoi(mainStreamMaskList_xPos[i]);    
        
            if(my_mainStreamMask1xPos >= 0)
            {
                pstVideoMaskCfg->mainStreamMaskList[i].xPos= my_mainStreamMask1xPos;
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "mainStreamMask_xPos is none");
                status = 400;
                goto __exit;
            }
            bsetFlag = 1;
        }

        if(cgi_get_value(http_url, mainStreamMaskName_yPos[i], mainStreamMaskList_yPos[i], 16))
        {
            int  my_mainStreamMask1yPos = safeatoi(mainStreamMaskList_yPos[i]);    
        
            if(my_mainStreamMask1yPos >= 0)
            {
                pstVideoMaskCfg->mainStreamMaskList[i].yPos= my_mainStreamMask1yPos;
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "mainStreamMask_yPos is none");
                status = 400;
                goto __exit;
            }
            bsetFlag = 1;
        }
   }


    /* setmsg */
    if(bsetFlag == 1)
    {
        int cameraIndex = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_mask_set(pstVideoMaskCfg, cameraIndex);
            anj_ispctl_config_set();
        }
    }

    /* setxml  SNF=\"%s\"/*/
    sprintf(response, XML_HEAD"<Mask>OK</Mask>");
    status = 200;
                          
__exit:
    return status;
}

//增加服务端口开启关闭
int cgi_service_port(const char *url_name, const char *clientip, const char *http_url,char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int bsetFlag = 0;
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char enable_web[8] = {0};
    char enable_control[8] = {0};
    char enable_rtsp[8] = {0};
    char enable_hik[8] = {0};    

    char webport[8] = {0};
    char controlport[8] = {0};
    char rtspport[8] = {0};
    char hikport[8] = {0};

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

    snprintf(enable_web, sizeof(enable_web)-1, "%d", pstMediaStreamCfg->webConfig.enable_web);   //开启WEB 80
    snprintf(enable_control, sizeof(enable_control)-1, "%d", pstMediaStreamCfg->commConfig.enable); //开启控制端口8091

    snprintf(enable_rtsp, sizeof(enable_rtsp)-1, "%d", pstMediaStreamCfg->rtspConfig.enable_rtsp);   //开启RTSP 554
    snprintf(enable_hik, sizeof(enable_hik)-1, "%d", pstMediaStreamCfg->hikConfig.enable); //开启海康协议8000    

    snprintf(webport, sizeof(webport), "%d", pstMediaStreamCfg->webConfig.webPort);//web端口 
    snprintf(controlport, sizeof(controlport), "%d", pstMediaStreamCfg->commConfig.ptzPort);//控制端口 
    snprintf(rtspport, sizeof(rtspport), "%d", pstMediaStreamCfg->rtspConfig.videoPort);//rtsp端口
    snprintf(hikport, sizeof(hikport), "%d", pstMediaStreamCfg->hikConfig.port);//hik端口 

    /* setconfig */
    if(cgi_get_value(http_url, "enable_web", enable_web, sizeof(enable_web)))
    {
        int i_enable_web = safeatoi(enable_web);
    
        if((i_enable_web == 0) || (i_enable_web == 1))
        {
            pstMediaStreamCfg->webConfig.enable_web = i_enable_web;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable_web: 0 ~ 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "webport", webport, sizeof(webport)))
    {
        int i_webport = safeatoi(webport);
    
        if(i_webport > 0 && i_webport <= 65535)
        {
            pstMediaStreamCfg->webConfig.webPort = i_webport;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "webport: 1 ~ 65535");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "enable_control", enable_control, sizeof(enable_control)))
    {
        int i_enable_control = safeatoi(enable_control);
    
        if((i_enable_control ==0) || (i_enable_control == 1))
        {
            pstMediaStreamCfg->commConfig.enable = i_enable_control;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable_control: 0 ~ 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "controlport", controlport, sizeof(controlport)))
    {
        int  i_controlport = safeatoi(controlport);
    
        if(i_controlport > 0 && i_controlport <= 65535)
        {
            pstMediaStreamCfg->commConfig.ptzPort = i_controlport;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "controlport: 1 ~ 65535");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }    

    if(cgi_get_value(http_url, "enable_rtsp", enable_rtsp, sizeof(enable_rtsp)))
    {
        int i_enable_rtsp = safeatoi(enable_rtsp);
    
        if((i_enable_rtsp ==0) || (i_enable_rtsp == 1))
        {
            pstMediaStreamCfg->rtspConfig.enable_rtsp = i_enable_rtsp;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable_rtsp: 0 ~ 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "rtspport", rtspport, sizeof(rtspport)))
    {
        int i_rtspport = safeatoi(rtspport);
    
        if(i_rtspport > 0 && i_rtspport <= 65535)
        {
            pstMediaStreamCfg->rtspConfig.videoPort = i_rtspport;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "rtspport: 1 ~ 65535");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }    

    if(cgi_get_value(http_url, "enable_hik", enable_hik, sizeof(enable_hik)))
    {
        int i_enable_hik = safeatoi(enable_hik);
    
        if((i_enable_hik == 0) || (i_enable_hik == 1))
        {
            pstMediaStreamCfg->hikConfig.enable = i_enable_hik;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable_hik: 0 ~ 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "hikport", hikport, sizeof(hikport)))
    {
        int i_hikport = safeatoi(hikport);
    
        if(i_hikport > 0 && i_hikport <= 65535)
        {
            pstMediaStreamCfg->hikConfig.port = i_hikport;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "hikport: 1 ~ 65535");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }        

    if(bsetFlag == 1)
    {
        anj_config_stream_set(pstMediaStreamCfg);
    }

    sprintf(response, XML_HEAD"<MediaStreamConfig><StreamAccess enable_web=\"%s\" webport=\"%s\" enable_rtsp=\"%s\" rtspport=\"%s\" enable_comm=\"%s\" controlport=\"%s\" enable_hik=\"%s\" hikport=\"%s\"/></MediaStreamConfig>",
            enable_web, webport, enable_rtsp, rtspport, enable_control, controlport, enable_hik, hikport);

    status = 200;                        
__exit:
    return status;
}


//增加红外灯开关控制，亮度级别控制的API
int cgi_Infrared_lamp_brightness(const char *url_name, const char *clientip, const char *http_url, char *response)
{
//CGI的名字需要注意的地方，指令和功能不能相同，不然无法解析字符串
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int bsetFlag = 0;
    int cameraIndex = 0;

    char i_brightness[8] = {0};
    char brightness[8] = {0};
    char infrared_lamp[8] = {0};
    int lamp_ircut_ctrl = -1;

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[cameraIndex].videoCapture;

    snprintf(brightness, sizeof(i_brightness)-1, "%d", pstVideoCapCfg->led_brightness_value);
    snprintf(infrared_lamp, sizeof(infrared_lamp)-1, "%d", pstVideoCapCfg->led_mode);

    if(cgi_get_value(http_url, "setbrightness", brightness, sizeof(brightness)))
    {
        int my_brightness = safeatoi(brightness);    
        if((my_brightness >= 0) && (my_brightness <= 100))
        {
            pstVideoCapCfg->led_brightness_value = my_brightness;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "brightness: 0 ~ 100");
            status = 400;
            goto __exit;
        }

        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "infrared_lamp", infrared_lamp, sizeof(infrared_lamp)))
    {
        int  i_infrared_lamp = safeatoi(infrared_lamp);
    
        if((i_infrared_lamp >= 0) && (i_infrared_lamp <= 1))
        {
            if(i_infrared_lamp == 0)    // 关红外灯：ircut切换到白天
            {
                lamp_ircut_ctrl = 1;

            }
            else if(i_infrared_lamp == 1)     // 开红外灯：ircut切换到晚上
            {
                lamp_ircut_ctrl = 0;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "infrared_lamp: 0 ~ 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }    

    if(bsetFlag == 1)
    {
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
            anj_ispctl_config_set();

            if (lamp_ircut_ctrl != -1)
            {
                anj_ispctl_ircut_manual_ctrl(lamp_ircut_ctrl, cameraIndex);
            }
        }
    }

    sprintf(response, XML_HEAD"<Capture led_Brightness=\"%s\" infrared_lamp=\"%s\"></Capture>", brightness, infrared_lamp);
    status = 200;

__exit:
    return status;
}

int cgi_led_brightness(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int bsetFlag = 0;

    int getbrightness = 0;
    int getledmode = 0;

    int i_brightness = 0;
    int i_lampstatus = 0;
    int i_ledmode = 1;

    char brightness[8] = {0};
    char lampstatus[8] = {0};
    char ledmode[8] = {0};

    if(cgi_get_value(http_url, "brightness", brightness, sizeof(brightness)))
    {
        i_brightness = safeatoi(brightness);    
    
        if((i_brightness < 0) || (i_brightness > 100))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "brightness: 0 ~ 100");
            status = 400;
            goto __exit;
        }

        getbrightness = 1;
    }
    
    if(cgi_get_value(http_url, "ledmode", ledmode, sizeof(ledmode)) )
    {
        i_ledmode = safeatoi(ledmode);
    
        if(i_ledmode > 2 || i_ledmode < 1)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "ledmode: 1 or 2");
            status = 400;
            goto __exit;
        }
        
        getledmode = 1;
    }

    if(cgi_get_value(http_url, "ledstatus", lampstatus, sizeof(lampstatus)))
    {
        i_lampstatus = safeatoi(lampstatus);
    
        if(i_lampstatus > 1 || i_lampstatus < 0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "lampstatus: 0 or 1");
            status = 400;
            goto __exit;
        }

        if(i_lampstatus != 0)
        {
            if(getbrightness == 0)
            {
                sprintf(response, XML_CGI_FAULT, url_name, "not find param brightness");
                status = 400;
                goto __exit;
            }

            if(getledmode == 0)
            {
                sprintf(response, XML_CGI_FAULT, url_name, "not find param ledmode");
                status = 400;
                goto __exit;
            }
        }        
        
        bsetFlag = 1;
    }
    else
    {
        sprintf(response, XML_CGI_FAULT, url_name, "not find param lampstatus");
        status = 400;
        goto __exit;
    }

    /* setmsg */
    if(bsetFlag == 1)
    {
        if (i_lampstatus == 0)
        {
            i_brightness = 0;
        }
    
        if(i_lampstatus == 0 && getledmode == 0)
        {
            anj_ispctl_light_manual_ctrl(LIGHT_INDEX_WLED, 0);
            anj_ispctl_light_manual_ctrl(LIGHT_INDEX_RLED, 0);
        }
        else
        {

            if (i_ledmode == LIGHT_INDEX_WLED || i_ledmode == LIGHT_INDEX_RLED)
            {
                anj_ispctl_light_manual_ctrl(i_ledmode, i_brightness);
            }
            else if (i_ledmode == LIGHT_INDEX_RB_ALARM)
            {
                if (i_brightness > 0)
                    anj_mw_hwctrl_alarmled_open();
                else
                    anj_mw_hwctrl_alarmled_close();
            }
        }
    }

    sprintf(response, XML_HEAD"<Capture ledstatus=\"%s\" brightness=\"%s\" ledmode=\"%s\"></Capture>", lampstatus, brightness, ledmode);
    status = 200;

__exit:
    return status;
}


//增加编码码流开关API
int cgi_Codec_stream(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }
    
    if(cgi_is_valid_uid_or_username(http_url,response,url_name) == 0)
    {
        return 400;
    }

    int chn = 0;
    int bsetFlag = 0;
    char stream_enable0[8] = {0};
    char stream_enable1[8] = {0};

//获取视频编码设置
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoEncode *pstVideoEncode = &pstMediaCfg->videoConfig[chn].videoEncode;

    snprintf(stream_enable0, sizeof(stream_enable0) - 1, "%d", pstVideoEncode->encodeCfg[0].enable != 0 ? 1:0);
    snprintf(stream_enable1, sizeof(stream_enable1) - 1, "%d", pstVideoEncode->encodeCfg[1].enable != 0 ? 1:0);    

    /* setconfig */
    if(cgi_get_value(http_url, "stream0_enable", stream_enable0, 2))
    {
        if(stream_enable0[0] == '0' && stream_enable0[1] == 0)
        {
            pstVideoEncode->encodeCfg[0].enable = 0;
        }
        else if(stream_enable0[0] == '1' && stream_enable0[1] == 0)
        {
            pstVideoEncode->encodeCfg[0].enable = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "stream0_enable: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "stream1_enable", stream_enable1, 2))
    {
        if(stream_enable1[0] == '0' && stream_enable1[1] == 0)
        {
            pstVideoEncode->encodeCfg[1].enable = 0;
        }
        else if(stream_enable1[0] == '1' && stream_enable1[1] == 0)
        {
            pstVideoEncode->encodeCfg[1].enable = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "stream1_enable: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }    

    /* setmsg */
    if(bsetFlag == 1)
    {
        int cameraIndex = 0;
        int iNeedSwitch = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            iNeedSwitch |= anj_config_video_encode_set(pstVideoEncode, cameraIndex);
        }
        if (iNeedSwitch)
        {
            anj_video_encode_switch();
        }
    }

    /* setxml */
    sprintf(response, XML_HEAD"<Encode><EncodeConfig Stream=\"1\" Enable=\"%s\"/>\r\n<EncodeConfig Stream=\"2\" Enable=\"%s\"/></Encode>",
        stream_enable0, stream_enable1);

    status = 200;

__exit:
    return status;    
}

//2D/3D NR 开关和级别调整API
int cgi_noise_reduction(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int bsetFlag = 0;
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int cameraIndex = 0;
    char my_2D_value[8] = {0};
    char my_3D_value[8] = {0};

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[cameraIndex].videoCapture;

    snprintf(my_2D_value, sizeof(my_2D_value)-1, "%d", pstVideoCapCfg->tnf);   //2D
    snprintf(my_3D_value, sizeof(my_3D_value)-1, "%d", pstVideoCapCfg->snf); //3D    

    /* setconfig */
    if(cgi_get_value(http_url, "y_2D_value", my_2D_value, sizeof(my_2D_value)))
    {
        int i_2D_value = safeatoi(my_2D_value);
    
        if((i_2D_value >=0) && (i_2D_value <= 255))
        {
            pstVideoCapCfg->tnf = i_2D_value;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "2D_value: 0 ~ 255");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "y_3D_value", my_3D_value, sizeof(my_3D_value)))
    {
        int i_3D_value = safeatoi(my_3D_value);
        if((i_3D_value >= 0) && (i_3D_value <= 255))
        {
            pstVideoCapCfg->snf = i_3D_value;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "3D_value: 0 ~ 255");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }    

    /* setmsg */
    if(bsetFlag == 1)
    {
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
        }
    }

    sprintf(response, XML_HEAD"<Capture TNF=\"%s\" SNF=\"%s\"></Capture>", my_2D_value, my_3D_value);
    status = 200;

__exit:
    return status;    
}


//增加水平和垂直的翻转
int cgi_set_flip(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }
    __ERR("cgi_set_flip\n");

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int bsetFlag = 0;
    int chn = 0;
    char vertical_flip[8] = {0};
    char horizon_flip[8] = {0};

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    snprintf(vertical_flip, sizeof(vertical_flip)-1, "%d", pstVideoCapCfg->vflip != 0 ? 1 : 0);   //垂直翻转
    snprintf(horizon_flip, sizeof(horizon_flip)-1, "%d", pstVideoCapCfg->hflip != 0 ? 1 : 0);     //水平翻转

    /* setconfig */
    if(cgi_get_value(http_url, "vertical_flip", vertical_flip, 2))
    {
        if(vertical_flip[0] == '0' && vertical_flip[1] == 0)
        {
            pstVideoCapCfg->vflip = 0;
        }
        else if(vertical_flip[0] == '1' && vertical_flip[1] == 0)
        {
            pstVideoCapCfg->vflip = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "vertical_flip: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "horizon_flip", horizon_flip, sizeof(horizon_flip)))
    {
        if(horizon_flip[0] == '0' && horizon_flip[1] == 0)
        {
            pstVideoCapCfg->hflip = 0;
        }
        else if(horizon_flip[0] == '1' && horizon_flip[1] == 0)
        {
            pstVideoCapCfg->hflip = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "horizon_flip: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }    

    /* setmsg */
    if(bsetFlag == 1)
    {
        int cameraIndex = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
            anj_ispctl_config_set();
        }
    }

    /* setxml */
    sprintf(response, XML_HEAD"<Capture vertical_flip=\"%s\" horizon_flip=\"%s\"></Capture>", vertical_flip, horizon_flip);
    status = 200;                      
__exit:

    return status;    
}

//手动触发io输入告警
int cgi_set_manual_inputAlarm(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    __ERR("cgi set manual input alarm, uid or username isValid: %d\n",isValid);
    if(!isValid)
    {
        return 400;
    }

    anj_alarm_ioinput_manual_trigger();

    sprintf(response, XML_HEAD"<ManualInputAlarm>%s</ManualInputAlarm>", "successful");
    status = 200;
    return status;    
}


//设置onvif认证状态
int cgi_onvif(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    __ERR("cgi onvif uid or username isValid: %d\n",isValid);
    if(!isValid)
    {
        return 400;
    }

    char onvif_auth[8] = {0};
    int bsetFlag = 0;

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

    if(cgi_get_value(http_url, "onvif_auth", onvif_auth, 2))
    {    
        __ERR("cgi onvif auth:%s \n",onvif_auth);
        if(onvif_auth[0] == '0' && onvif_auth[1] == 0)
        {
            pstMediaStreamCfg->webConfig.onvif_auth = 0;
        }
        else if(onvif_auth[0] == '1' && onvif_auth[1] == 0)
        {
            pstMediaStreamCfg->webConfig.onvif_auth = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "onvif_auth: 0 or 1");
            status = 400;
            goto __exit;
        }        
        bsetFlag = 1;
    }

    if(bsetFlag == 1)
    {
        anj_config_stream_set(pstMediaStreamCfg);
    }

    /* setxml */
    sprintf(response, XML_HEAD"<OnvifConfig onvif_auth=\"%s\"/>",onvif_auth);
    status = 200;

__exit:
    return status;    
}

//设置rtsp认证状态
int cgi_rtsp(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    __ERR("cgi rtsp uid or username isValid:%d\n",isValid);
    if(!isValid)
    {
        return 400;
    }

    char rtsp_auth[8] = {0};
    int bsetFlag = 0;

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

    if(cgi_get_value(http_url, "rtsp_auth", rtsp_auth, 2))
    {    
        __ERR("cgi rtsp auth:%s\n",rtsp_auth);
        if(rtsp_auth[0] == '0' && rtsp_auth[1] == 0)
        {
            pstMediaStreamCfg->rtspConfig.rtsp_auth = 0;
        }
        else if(rtsp_auth[0] == '1' && rtsp_auth[1] == 0)
        {
            pstMediaStreamCfg->rtspConfig.rtsp_auth = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "rtsp_auth: 0 or 1");
            status = 400;
            goto __exit;
        }        
        bsetFlag = 1;
    }

    //如果检测到有参数的话，进入此函数
    if(bsetFlag == 1)
    {
        anj_config_stream_set(pstMediaStreamCfg);
    }

    /* setxml */
    sprintf(response, XML_HEAD"<RtspConfig rtsp_auth=\"%s\"/>", rtsp_auth);
    status = 200;

__exit:
    return status;    
}

// 获取用户名密码
int cgi_getCurrentUserAccount(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = 0;

    //回环端口获取账号密码不验证
    if(strcmp(clientip, "127.0.0.1") == 0 || strcmp(clientip, "127.0.0.1:80") == 0 || strstr(clientip, "127.0.0.1") != NULL)
    {
        __DBG("127.0.0.1 get device username and password\n");
    }
    else
    {    
        int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
        if(!isValid)
        {
            return 400;
        }
    }

    char username[40] = {0};
    char password[40] = {0};
    UserAuthGetUsernamePassword(username, sizeof(username), password, sizeof(password));
    __DBG("cgi get currnet account username:%s, password:%s\n", username, password);
#if 1
    if(strlen(username) > 0 && strlen(password) > 0)
    {
        sprintf(response, XML_HEAD"<UsernamePassword>username=%s, password=%s</UsernamePassword>", username, password);
        status = 200;
    }
    else
    {
        sprintf(response, XML_HEAD"<UsernamePassword>Get username password failed!!!</UsernamePassword>");
        status = 400;
    }
#else
    sprintf(response,XML_HEAD"<UsernamePassword>username=%s, password=%s</UsernamePassword>", username, password);
    status = 200;

#endif

    return status;    
}

// 开始发送告警消息到回环端口
int cgi_set_send_alarm_to_loopback(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    if(!isValid)
    {
        return 400;
    }
    
    char enable[8] = {0};
    int cmd = -1;
    int bsetFlag = 0;

    if(cgi_get_value(http_url, "enable", enable, 2))
    {
        __ERR("set alarm enable:%s \n",enable);

        if(enable[0] == '0' && enable[1] == 0)
        {
            cmd = 0;
        }
        else if(enable[0] == '1' && enable[1] == 0)
        {
            cmd = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "alarm enable: 0 or 1");
            status = 400;
            goto __exit;
        }        
        bsetFlag = 1;

    }

    //如果检测到有参数的话，进入此函数
    if(bsetFlag == 1)
    {
        alarm_loopback_info_set(cmd);
    }

    /* setxml */
    sprintf(response, XML_HEAD"<SendAlarm enable=\"%s\"/>", enable);
    status = 200;
    
__exit:
    return status;
}



// 获取继电器当前状态
int cgi_get_relay_status(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    if(!isValid)
    {
        return 400;
    }

    char relayStatus[16] = {0};
    status = anj_mw_hwctrl_alarmout_usable_chn_status_get();
    if (status >= 0)
    {
        snprintf(relayStatus, sizeof(relayStatus), "%d", status);
        sprintf(response, XML_HEAD"<RelayStatus>status=%s</RelayStatus>", relayStatus);
        status = 200;
    }
    else
    {
        sprintf(response, XML_HEAD"<RelayStatus>Get status(%s) fail</RelayStatus>", relayStatus);
        status = 400;
    }

    return status;    
}

int cgi_set_relay_status(const char *url_name, const char *clientip, const char *http_url,char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    if(!isValid)
    {
        return 400;
    }

    char relaystatus[8] = {0};
    int cmd = -1;

    if(cgi_get_value(http_url, "status", relaystatus, 2))
    {
        __ERR("cgi set relay status:%s\n", relaystatus);

        cmd = atoi(relaystatus);
        anj_mw_hwctrl_alarmout_usable_chn_status_set(cmd);
    }

    sprintf(response, XML_HEAD"<RelayStatus>set status=%s</RelayStatus>", relaystatus);
    status = 200;

    return status;
}

//修改快门值
int cgi_set_ShutterSpeed(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    if(!isValid)
    {
        return 400;
    }

    int chn = 0;
    int bsetFlag = 0;
    char dayShutterMode[8] = {0};
    char dayShutterSpeed[8] = {0};
    char nightShutterMode[8] = {0};
    char nightShutterSpeed[8] = {0};

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    if(cgi_get_value(http_url, "dayShutterMode", dayShutterMode, sizeof(dayShutterMode)))
    {
        __ERR("cgi set day shutter mode:%s\n", dayShutterMode);
        if(dayShutterMode[0] == '0' && dayShutterMode[1] == 0)
        {
            pstVideoCapCfg->shutterSetting.shutter_mode_day = 0;
        }
        else if(dayShutterMode[0] == '1' && dayShutterMode[1] == 0)
        {
            pstVideoCapCfg->shutterSetting.shutter_mode_day = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "dayShutterMode: 0 or 1");
            status = 400;
            goto __exit;
        }

        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "dayShutterSpeed", dayShutterSpeed, sizeof(dayShutterSpeed)))
    {    
        __ERR("cgi set day shutter speed:%s\n", dayShutterSpeed);
        int  dayShutterSpeedValue = safeatoi(dayShutterSpeed);

        if(dayShutterSpeedValue >= 10 && dayShutterSpeedValue <= 10000)
        {
            if(pstVideoCapCfg->shutterSetting.shutter_mode_day == 1) //手动
            {
                pstVideoCapCfg->shutterSetting.shutter_speed_day = dayShutterSpeedValue;
                bsetFlag = 1;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "dayShutterSpeed: 10 or 10000");
            status = 400;
            goto __exit;
        }

    }

    if(cgi_get_value(http_url, "nightShutterMode", nightShutterMode, sizeof(nightShutterMode)))
    {
        __ERR("cgi set night shutter mode:%s\n", nightShutterMode);
        if(nightShutterMode[0] == '0' && nightShutterMode[1] == 0)
        {
            pstVideoCapCfg->shutterSetting.shutter_mode_night = 0;
        }
        else if(nightShutterMode[0] == '1' && nightShutterMode[1] == 0)
        {
            pstVideoCapCfg->shutterSetting.shutter_mode_night = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "nightShutterMode: 0 or 1");
            status = 400;
            goto __exit;
        }

        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "nightShutterSpeed", nightShutterSpeed, sizeof(nightShutterSpeed)))
    {    
        __ERR("cgi set night shutter speed:%s \n",nightShutterSpeed);
        int  nightShutterSpeedValue = safeatoi(dayShutterSpeed);

        if(pstVideoCapCfg->shutterSetting.shutter_mode_night == 1) //手动
        {
            if(nightShutterSpeedValue >= 10 && nightShutterSpeedValue <= 10000)
            {
                pstVideoCapCfg->shutterSetting.shutter_speed_night = nightShutterSpeedValue;
                bsetFlag = 1;
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "nightShutterSpeed: 10 or 10000");
                status = 400;
                goto __exit;
            }
        }
    }

    /* setmsg */
    if(bsetFlag == 1)
    {
        int cameraIndex = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
            anj_ispctl_config_set();
        }

    }

    sprintf(response, XML_HEAD"<setShutterSpeed>Set dayShutterMode=%s, dayShutterSpeed=%s, nightShutterMode=%s, nightShuuterSpeed=%s</setShutterSpeed>",
            dayShutterMode, dayShutterSpeed, nightShutterMode, nightShutterSpeed);
    status = 200;

__exit:
    return status;
}

//修改设备OEM型号
int cgi_set_device_name(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int isValid = cgi_is_valid_uid_or_username(http_url, response, url_name);
    if(!isValid)
    {
        return 400;
    }

    char cust_path[32] = {0};
    snprintf(cust_path, sizeof(cust_path), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);

    if(access(cust_path, F_OK) != 0)
    {
        mysystem_with_param("mkdir %s", cust_path);
    }

    char deviceName[128] = {0};
    char xmlbuf[512] = {0};
    if(cgi_get_value(http_url, "deviceName", deviceName, sizeof(cust_path)))
    {
        __ERR("cgi set device name:%s\n", deviceName);
        snprintf(xmlbuf, sizeof(xmlbuf),
            "<?xml version=\"1.0\" encoding=\"gb2312\" ?>\r\n"
            "<OEM_CONFIG>\r\n"
            "    <SYSTEM DEVICETYPE=\"%s\" />\r\n"
            "</OEM_CONFIG>\r\n",
            deviceName);

        char szOemXMLFileName[128] = {0};
        snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_SPECIFIC_FILE_NAME);
        int wLen = write_buffer_to_file(szOemXMLFileName, xmlbuf, sizeof(xmlbuf));
        if (wLen <= 0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "set device name failed!");
            status = 400;
            goto __exit;
        }
    }

    sprintf(response, XML_HEAD"<setDeviceName>Set device name(%s) successful!</setDeviceName>",deviceName);
    status = 200;

    __WARN("set device name, reboot\n");
    __RECORD_LOG_INFO("set device name, reboot\n");
    anj_sysmng_delay_reboot(1);
__exit:
    return status;    
}

//    获得系统的状态
int cgi_set_wdr(const char *url_name, const char *clientip, const char *http_url,char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int bsetFlag = 0;
    
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int chn = 0;
    char enable[8] = {0};
    char wdr_value[8] = {0};

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    snprintf(enable, sizeof(enable)-1, "%d", pstVideoCapCfg->wdr_mode != 0 ? 1 : 0);
    snprintf(wdr_value, sizeof(wdr_value)-1, "%d", pstVideoCapCfg->wdr_value);    

    /* setconfig */
    if(cgi_get_value(http_url, "enable", enable, 2))
    {
        if(enable[0] == '0' && enable[1] == 0)
        {
            pstVideoCapCfg->wdr_mode = 0;
        }
        else if(enable[0] == '1' && enable[1] == 0)
        {
            pstVideoCapCfg->wdr_mode = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0 or 1");
            status = 400;
            goto __exit;
        }

        bsetFlag = 1;
    }

    if(cgi_get_value(http_url, "wdr_value", wdr_value, sizeof(wdr_value)))
    {
        int i_wdr_value = safeatoi(wdr_value);
    
        if((i_wdr_value >=0) && (i_wdr_value <= 255))
        {
            pstVideoCapCfg->wdr_value = i_wdr_value;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "wdr_value: 0 ~ 255");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }    

    /* setmsg */
    if(bsetFlag == 1)
    {
        int cameraIndex = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
            anj_ispctl_config_set();
        }
    }

    /* setxml */
    sprintf(response, XML_HEAD"<Capture WDRMode=\"%s\" WDRValue=\"%s\"></Capture>", enable, wdr_value);
    status = 200;
__exit:

    return status;    
}

int cgi_set_ircutmode(const char *url_name, const char *clientip, const char *http_url,char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int bsetFlag = 0;
    
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char time[8] = {0};
    char ircutmode[8] = {0};

    int chn = 0;
    int i_time = 0;
    int i_ircutmode = 0;

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    snprintf(ircutmode, sizeof(ircutmode), "%d", pstVideoCapCfg->ircut_mode);

    /* setconfig */
    if(cgi_get_value(http_url, "ircutmode", ircutmode, sizeof(ircutmode)))
    {
        i_ircutmode = safeatoi(ircutmode);

        if(i_ircutmode >= IRCUT_Mode_Active && i_ircutmode < IRCUT_Mode_MAX)
        {
            if(i_ircutmode == IRCUT_Mode_Manual)
            {
                if(cgi_get_value(http_url, "time", time, sizeof(time)))
                {
                    if(strcmp(time, "day") == 0)
                    {
                        i_time = 0;
                    }
                    else if(strcmp(time, "night") == 0)
                    {
                        i_time = 1;
                    }
                    else
                    {
                        sprintf(response, XML_CGI_FAULT, url_name, "error param time");
                        status = 400;
                        goto __exit;
                    }
                }
                else
                {
                    sprintf(response, XML_CGI_FAULT, url_name, "not find param time");
                    status = 400;
                    goto __exit;
                }
            }
            
            pstVideoCapCfg->ircut_mode = (IRCutMode)i_ircutmode;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "ircutmode: 0 ~ 5");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    /* setmsg */
    if(bsetFlag == 1)
    {
        int cameraIndex = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
            anj_ispctl_config_set();

            if(pstVideoCapCfg->ircut_mode == IRCUT_Mode_Manual)
            {
                anj_ispctl_ircut_manual_ctrl(i_time, cameraIndex);
            }
        }
    }

    /* setxml */
    sprintf(response, XML_HEAD"<Capture ircutmode=\"%s\"></Capture>", ircutmode);

    status = 200;
                                
__exit:

    return status;    
}

//    获得设备信息
int cgi_getinfo(const char *url_name, const char *clientip, const char *http_url, char *response)
{
//不带用户名和密码，后面必须带?，不然显示参数错误
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    int Certification = 0;

//如果输入的字符串有密码的话，那么证明输入用户名和密码
    if (strstr(http_url, "&password="))
    {
        Certification = 1;
    }
    else
    {
        Certification = 0;
    }

//如果没有密码的话，那么也可以输出OEM型号和MAC地址
    SYSTEM_VERSION_DATA stSystemVersion = {0};
    anj_sysmng_version_info_get(&stSystemVersion, 0);

    NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
    LANConfig *pstLanCfg = &pstNetworkCfg->lanCfg;

    char szDeviceType[64] = {0};    
    if( GetDeviceTypeStr(szDeviceType) < 0 )
    {
        __ERR("Unknown device type. set to HD\n");
        strcpy(szDeviceType, "HD");
    }

    AjOemStruct stOemInfo = {0};
    anj_config_oem_get(&stOemInfo);
    
    if(Certification == 1)
    {
        if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
        {
            return 400;
        }

        char sn[64] = {0};
        anj_sysmng_load_sn(sn, sizeof(sn));

        sprintf(response, XML_HEAD"<info kernel=\"%s\" filesystem=\"%s\" sn=\"%s\" model=\"%s\"",
            stSystemVersion.kernelVersion, stSystemVersion.fsVersion,sn, szDeviceType);

        if(strlen(stOemInfo.szDeviceType) != 0)
        {
            sprintf(response + strlen(response), " oem_model=\"%s\" ", stOemInfo.szDeviceType );
        }
        
        if(strlen(stOemInfo.szOemSN ) != 0 )
        {
            sprintf(response + strlen(response), " oem_sn=\"%s\" ", stOemInfo.szOemSN);
        }
        
        sprintf(response + strlen(response), " mac_address=\"%s\"/>", pstLanCfg->MACAddress);
    }
    else
    {
        sprintf(response, XML_HEAD"<info model=\"%s\" ", szDeviceType);
        if(strlen(stOemInfo.szDeviceType) != 0)
        {
            sprintf(response + strlen(response), " oem_model=\"%s\" ", stOemInfo.szDeviceType );
        }
        sprintf(response + strlen(response), " mac_address=\"%s\"/>", pstLanCfg->MACAddress);
    }

    return 200;
}

//    获取cgi的能力集
int cgi_getcapabilities(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int video = 1;
    int audio = 1;
    int ptz = 0;

    char *pabilityStr = anj_sysctl_get_capability_string();
    if(strstr(pabilityStr, "ptz_control"))
    {
        ptz=1;
    }
    else
    {
        ptz=0;
    }

    if(strstr(pabilityStr, "only_audio"))
    {
        video = 0;
        audio = 1;
    }
    else if(strstr(pabilityStr, "audio_support"))
    {
        video = 1;
        audio = 1;
    }
    else
    {
        video = 1;
        audio = 0;
    }

    sprintf(response, XML_HEAD"<capabilities video=\"%d\" audio=\"%d\" ptz=\"%d\"/>", video, audio, ptz);
    return 200;
}

// 重启
int cgi_reboot(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    __ERR("cgi reboot from:%s\n", clientip);

    sprintf(response, XML_HEAD"<reboot>true</reboot>");

    __WARN("cgi reboot service request from:%s\n", clientip);
    __RECORD_LOG_INFO("cgi reboot service request from:%s\n", clientip);
    anj_sysmng_delay_reboot(1);
    return 200;
}


// 恢复出厂设置扩展版本，兼容以前的设备
int cgi_factory_reset(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    char retain[20] = {0};

    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int tmp = 0;
    tmp = cgi_get_value_2(http_url, "retain_network", retain, sizeof(retain));
    if(tmp > 0)             //接收到参数的情况
    {
        //if((strcmp(retain, "1") == 0) || (strcmp(retain, "0") == 0))
        if((strcmp(retain, "1") == 0))
        {
            anj_sysmng_restore_netconfig_set(1);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "retain_network should be 1");
            status = 400;
            return status;
        }
    }
    else if(tmp == -1)
    {
            //就是说参数缺省的情况，直接默认重启
    }
    else
    {
        //__ERR("112233retain =%s,strcmp=%d\n",retain,strcmp(retain,""));
        if(strcmp(retain,"")==0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "retain_network: NULL");
            status = 400;
            return status;
        }
    }

    sprintf(response, XML_HEAD"<factory_reset>true</factory_reset>");
    anj_sysmng_delay_restore(1);
    return 200;
}


//    网络
int cgi_network(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char macaddr[20] = {0};
    char dhcp[2] = {0};
    char ipaddr[IPADDR_MAX] = {0};
    char netmask[IPADDR_MAX] = {0};
    char gateway[IPADDR_MAX] = {0};
    char dns1[IPADDR_MAX] = {0};
    char dns2[IPADDR_MAX] = {0};
    char port[PORT_MAX] = {0};
    char hasIpaddr = 0;
    char hasGateway = 0;
    char flag_change = 0;

    NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
    LANConfig *pstLanCfg = &pstNetworkCfg->lanCfg;

    StrCpy(macaddr, 20, (char *)pstLanCfg->MACAddress);
    sprintf(dhcp, "%d", (pstLanCfg->dhcpEnable != 0) ? 1 : 0);
    StrCpy(ipaddr, IPADDR_MAX, pstLanCfg->IPAddress);
    StrCpy(netmask, IPADDR_MAX, pstLanCfg->netMask);
    StrCpy(gateway, IPADDR_MAX, pstLanCfg->gateWay);
    StrCpy(dns1, IPADDR_MAX, pstLanCfg->DNS1);
    StrCpy(dns2, IPADDR_MAX, pstLanCfg->DNS2);

    int stream_change = 0;
    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    sprintf(port, "%d", pstMediaStreamCfg->webConfig.webPort);
    
    // 获取 dhcp
    if(cgi_get_value(http_url, "dhcp", dhcp, 2))
    {
        if(dhcp[0]=='0')
        {
            if (pstLanCfg->dhcpEnable != 0)
            {
                flag_change = 1;
                pstLanCfg->dhcpEnable = 0;
            }
        }
        else if(dhcp[0]=='1')
        {
            if (pstLanCfg->dhcpEnable == 0)
            {
                flag_change = 1;
                pstLanCfg->dhcpEnable = 1;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "dhcp: 0 or 1");
            status = 400;
            goto __exit;
        }
    }

    // 获取 port
    if(cgi_get_value(http_url, "port", port, PORT_MAX))
    {
        int i_port = atoi(port);
        if((i_port == 80) || ((i_port >= 1000) && (i_port <= 65535)))
        {
            if (pstMediaStreamCfg->webConfig.webPort != i_port)
            {
                stream_change = 1;
                pstMediaStreamCfg->webConfig.webPort = i_port;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "port range: 80,1000-65535");
            status = 400;
            goto __exit;
        }
    }

    // 获取 ipaddr
    if(cgi_get_value(http_url, "ipaddr", ipaddr, IPADDR_MAX))
    {
        if(!cgi_is_legal_ipaddr(ipaddr, TYPE_IPADDR))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "ipaddr error");
            status = 400;
            goto __exit;
        }
        hasIpaddr=1;
    }
    // 获取 netmask
    if(cgi_get_value(http_url,"netmask",netmask,IPADDR_MAX))
    {
        if(!cgi_is_legal_ipaddr(netmask,TYPE_NETMASK))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "netmask error");
            status = 400;
            goto __exit;
        }

        if (strcmp(pstLanCfg->netMask, netmask))
        {
            flag_change = 1;
            StrCpy(pstLanCfg->netMask, IPADDR_MAX, netmask);
        }
    }

    // 获取 gateway
    if(cgi_get_value(http_url,"gateway",gateway,IPADDR_MAX))
    {
        if(!cgi_is_legal_ipaddr(gateway, TYPE_GATEWAY))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "gateway error");
            status = 400;
            goto __exit;
        }    
        hasGateway=1;
    }
    // 获取 dns1
    if(cgi_get_value(http_url,"dns1",dns1,IPADDR_MAX))
    {
        if(!cgi_is_legal_ipaddr(dns1,TYPE_DNS))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "dns1 error");
            status = 400;
            goto __exit;
        }

        if (strcmp(pstLanCfg->DNS1, dns1))
        {
            flag_change = 1;
            StrCpy(pstLanCfg->DNS1, IPADDR_MAX, dns1);
        }
    }

    // 获取 dns2
    if(cgi_get_value(http_url,"dns2",dns2,IPADDR_MAX))
    {
        if(!cgi_is_legal_ipaddr(dns2,TYPE_DNS))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "dns2 error");
            status = 400;
            goto __exit;
        }

        if (strcmp(pstLanCfg->DNS2, dns1))
        {
            flag_change = 1;
            StrCpy(pstLanCfg->DNS2, IPADDR_MAX, dns2);
        }
    }

    if(hasIpaddr && !hasGateway)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, parse gateway");
        status = 400;
        goto __exit;
    }
    else if(!hasIpaddr && hasGateway)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, parse ipaddr");
        status = 400;
        goto __exit;
    }
    if(hasIpaddr && hasGateway)
    {
        if (strcmp(pstLanCfg->IPAddress, ipaddr))
        {
            flag_change = 1;
            StrCpy(pstLanCfg->IPAddress, IPADDR_MAX, ipaddr);
        }

        if (strcmp(pstLanCfg->gateWay, gateway))
        {
            flag_change = 1;
            StrCpy(pstLanCfg->gateWay, IPADDR_MAX, gateway);
        }
    }

    sprintf(response, XML_HEAD"<network macaddr=\"%s\" dhcp=\"%s\" "
                            "ipaddr=\"%s\" netmask=\"%s\" gateway=\"%s\" "
                            "dns1=\"%s\" dns2=\"%s\" port=\"%s\"/>",
                            macaddr, dhcp, ipaddr, netmask, gateway, 
                            dns1, dns2, port);

    if (stream_change)
    {
        anj_config_stream_set(pstMediaStreamCfg);
    }

    if (flag_change)
    {
        anj_config_network_lan_set(pstLanCfg);
    }

    status = 200;

__exit:
    return status;
}

// 时间
int cgi_time(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char timezone[16] = {0};
    char update_method[32] = {0};
    char year[8] = {0};
    char month[8] = {0};
    char day[8] = {0};
    char hour[8] = {0};
    char min[8] = {0};
    char sec[8] = {0};
    char ntpaddr[64] = {0};
    char port[PORT_MAX] = {0};
    char update_period[16] = {0};
    
    //int hasTimeZone = 0;
    int hasYear = 0;
    int hasMonth = 0;
    int hasDay = 0;
    int hasHour = 0;
    int hasMin = 0;
    int hasSec = 0;
    int iRet = 0;
    int flag_change = 0;

    struct tm stNowtime = {0};

    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    TimeConfig *pstTimeCfg = &pstSystemCfg->timeCfg;

    timezone_itoa(pstTimeCfg->timeZone, timezone, 16);
    StrCpy(update_method, sizeof(update_method), pstTimeCfg->timeMode.modeName);
    StrCpy(ntpaddr, sizeof(ntpaddr), pstTimeCfg->ntpConfig.serverIP);
    sprintf(port, "%d", pstTimeCfg->ntpConfig.serverPort);
    sprintf(update_period, "%u", pstTimeCfg->ntpConfig.refreshInterval);

    SystemLocalTime(&stNowtime);
    sprintf(year, "%04d", (stNowtime.tm_year & 0xff) + 1900);    
    sprintf(month, "%02d", (stNowtime.tm_mon & 0xff) + 1);
    sprintf(day, "%02d", stNowtime.tm_mday & 0xff);
    sprintf(hour, "%02d", stNowtime.tm_hour & 0xff);
    sprintf(min, "%02d", stNowtime.tm_min & 0xff);
    sprintf(sec, "%02d", stNowtime.tm_sec & 0xff);
    
    // 获取 timezone
    if(cgi_get_value(http_url, "timezone", timezone, sizeof(timezone)))
    {
        iRet = timezone_atoi(timezone);
        if(iRet < 0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "timezone error");
            status = 400;
            goto __exit;
        }

        pstTimeCfg->timeZone = iRet;
        //hasTimeZone = 1;
        flag_change = 1;
    }

    // 获取 update_method
    if(cgi_get_value(http_url, "update_method", update_method, sizeof(update_method)))
    {
        if(!strcmp(update_method, "MANUAL"))
        {
            StrCpy(pstTimeCfg->timeMode.modeName, sizeof(update_method), update_method);
        }
        else if(!strcmp(update_method,"NTP"))
        {
            StrCpy(pstTimeCfg->timeMode.modeName, sizeof(update_method), update_method);
        }
        else if(!strcmp(update_method,"P2P"))
        {
            StrCpy(pstTimeCfg->timeMode.modeName, sizeof(update_method), update_method);
        }
        else{
            sprintf(response, XML_CGI_FAULT, url_name, "update_method: MANUAL or NTP or P2P");
            status = 400;
            goto __exit;
        }

        flag_change=1;
    }

    // 获取 ntpaddr
    if(cgi_get_value(http_url, "ntpaddr", ntpaddr, sizeof(ntpaddr)))
    {
        StrCpy(pstTimeCfg->ntpConfig.serverIP, 32, ntpaddr);
        flag_change = 1;
    }

    // 获取 port
    if(cgi_get_value(http_url, "port", port, PORT_MAX))
    {
        unsigned int i_port = atoi(port);
        if((i_port >= 1) && (i_port <= 65535) )
        {
            pstTimeCfg->ntpConfig.serverPort = i_port;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "port range: 1-65535");
            status = 400;
            goto __exit;
        }

        flag_change=1;
    }
  
    // 获取 update_period
    if(cgi_get_value(http_url, "update_period", update_period, sizeof(update_period)))
    {
        unsigned int i_update_period = atoi(update_period);
        if((i_update_period >= 60) && (i_update_period <= 65535))
        {
            pstTimeCfg->ntpConfig.refreshInterval = i_update_period;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "update_period range: 1-65535");
            status = 400;
            goto __exit;
        }

        flag_change=1;
    }

    // 获取 year
    if(cgi_get_value(http_url, "year", year, 8))    hasYear=1;
    // 获取 month
    if(cgi_get_value(http_url, "month", month, 8))    hasMonth=1;
    // 获取 day
    if(cgi_get_value(http_url, "day", day, 8))    hasDay=1;
    // 获取 hour
    if(cgi_get_value(http_url, "hour", hour, 8))    hasHour=1;
    // 获取 min
    if(cgi_get_value(http_url, "min", min, 8))    hasMin=1;
    // 获取 sec
    if(cgi_get_value(http_url, "sec", sec, 8))    hasSec=1;

    status = verify_date_time_format(year, month, day, 
                                    hour, min, sec, 
                                    hasYear, hasMonth, hasDay,
                                    hasHour, hasMin, hasSec);
    if(status < 0)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, date or time format");
        status = 400;
        goto __exit;
    }

    //修改时间配置
    if(flag_change)
    {
        anj_config_system_time_set(pstTimeCfg);
    }

    if(status == 1)
    {
        //hasTimeZone = 1;
        anj_systime_set_time_and_zone(stNowtime, pstTimeCfg->timeZone, 1);
    }

    // 再次获取刷新后的时间
    SystemLocalTime(&stNowtime);
    sprintf(year,"%04d", (stNowtime.tm_year & 0xff) + 1900);    
    sprintf(month,"%02d", (stNowtime.tm_mon & 0xff) + 1);
    sprintf(day,"%02d", stNowtime.tm_mday & 0xff);
    sprintf(hour,"%02d", stNowtime.tm_hour & 0xff);
    sprintf(min,"%02d", stNowtime.tm_min & 0xff);
    sprintf(sec,"%02d", stNowtime.tm_sec & 0xff);

    sprintf(response,XML_HEAD"<time timezone=\"%s\" update_method=\"%s\" "
                            "ntpaddr=\"%s\" port=\"%s\" update_period=\"%s\" "
                            "current_time=\"%s-%s-%s %s:%s:%s\"/>",
                            timezone, update_method, ntpaddr, port, update_period,
                            year, month, day, hour, min, sec);
    status = 200;
__exit:
    return status;
}


int get_video_capabilities(v_capabilities *v_cap, RESOLUTION_ENTRY *pVideo, int vcount)
{
    int i = 0;
    int j = 0;
    int k = 0;
    int iIndex = 0;
    
    for (iIndex = 0; iIndex < vcount; iIndex++)
    {
        // v_stream i
        for (i = 0; i < 3; i++)
        {
            if (v_cap->stream[i].num == 0)
            {
                v_cap->stream[i].num = pVideo[iIndex].stream_type + 1;
                break;
            }
            else if (v_cap->stream[i].num == (pVideo[iIndex].stream_type + 1))
            {
                break;
            }
        }
        if (i >= 3)
        {
            continue;
        }
        
        // v_encodeMode j
        for (j = 0; j < 3; j++)
        {
            if (strlen(v_cap->stream[i].encode_mode[j].name) == 0)
            {
                StrCpy(v_cap->stream[i].encode_mode[j].name, 8, pVideo[iIndex].codec_name);
                break;
            }
            else if (!strcmp(pVideo[iIndex].codec_name, v_cap->stream[i].encode_mode[j].name))
            {
                break;
            }
        }
        if (j >= 3)
        {
            continue;
        }
        
        // v_resolution k
        for (k = 0; k < 10; k++)
        {
            if (strlen(v_cap->stream[i].encode_mode[j].resolution[k].name) == 0)
            {
                StrCpy(v_cap->stream[i].encode_mode[j].resolution[k].name, 16, pVideo[iIndex].res_name);
                break;
            }
            else if (!strcmp(v_cap->stream[i].encode_mode[j].resolution[k].name, pVideo[iIndex].res_name))
            {
                break;
            }
        }
        if (k >= 10)
        {
            continue;
        }
        
        // framerate_range
        sprintf(v_cap->stream[i].encode_mode[j].resolution[k].framerate_range,
                "%d-%d", pVideo[iIndex].min_framerate, pVideo[iIndex].max_display_framerate);
        
        // bitrate_range
        sprintf(v_cap->stream[i].encode_mode[j].resolution[k].bitrate_range,
                "%d-%d", pVideo[iIndex].min_bitrate, pVideo[iIndex].max_bitrate);
    }
    
    return 0;
}

//    视频编码能力集
int cgi_videoencoder_capabilities(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int i = 0;
    int j = 0;
    int k = 0;
    int pos = 0;
    int iRet = 0;

    RESOLUTION_ENTRY *pVideo = NULL;
    int vcount = anj_sysmng_video_res_array_get(&pVideo);

    v_capabilities *v_cap = (v_capabilities *)anj_mw_malloc(sizeof(v_capabilities));
    memset(v_cap, 0, sizeof(v_capabilities));

    get_video_capabilities(v_cap, pVideo, vcount);

    iRet = sprintf(response + pos, XML_HEAD "<video_capabilities>");
    pos += iRet;

    for (i = 0; i < 3; i++)
    {
        if (v_cap->stream[i].num > 0)
        {
            iRet = sprintf(response + pos, "<stream%d>", v_cap->stream[i].num);
            pos += iRet;

            for (j = 0; j < 3; j++)
            {
                if (strlen(v_cap->stream[i].encode_mode[j].name) > 0)
                {
                    iRet = sprintf(response + pos, "<encode_mode name=\"%s\">", v_cap->stream[i].encode_mode[j].name);
                    pos += iRet;

                    for (k = 0; k < 10; k++)
                    {
                        if (strlen(v_cap->stream[i].encode_mode[j].resolution[k].name) > 0)
                        {
                            iRet = sprintf(response + pos, "<resolution name=\"%s\">", v_cap->stream[i].encode_mode[j].resolution[k].name);
                            pos += iRet;
                            iRet = sprintf(response + pos, "<framerate_range>%s</framerate_range>", v_cap->stream[i].encode_mode[j].resolution[k].framerate_range);
                            pos += iRet;
                            iRet = sprintf(response + pos, "<bitrate_range>%s</bitrate_range>", v_cap->stream[i].encode_mode[j].resolution[k].bitrate_range);
                            pos += iRet;
                            iRet = sprintf(response + pos, "</resolution>");
                            pos += iRet;
                        }
                    }
                    
                    iRet = sprintf(response + pos, "</encode_mode>");
                    pos += iRet;
                }
            }
            
            iRet = sprintf(response + pos, "</stream%d>", v_cap->stream[i].num);
            pos += iRet;
        }
    }
    
    iRet = sprintf(response + pos, "</video_capabilities>");
    pos += iRet;
    
    anj_mw_free(v_cap);
    
    
    return 200;
}

//    视频编码配置
int cgi_videoencoder_handle(int setStreamId, int setProfile, const char* url_name, const char *http_url, char *response)
{
    int status = 0;

    __INFO("cgi video encode handle! setStreamId=%d, set_profile=%d\n", setStreamId, setProfile);

    int chn = 0;
    int iCameraIdx = 0;
    int iIndex = 0;
    int iRet = 0;
    int pos = 0;
    int flag_change = 0;

    int v_encode_size = 3;

    v_encoder_config *pstVEncodeCfg = (v_encoder_config *)anj_mw_malloc(v_encode_size * sizeof(v_encoder_config));
    memset(pstVEncodeCfg, 0, v_encode_size * sizeof(v_encoder_config));

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoEncode *pstVideoEncodeCfg = &pstMediaCfg->videoConfig[chn].videoEncode;

    for(iIndex = 0; iIndex < v_encode_size; iIndex++)
    {
        if(pstVideoEncodeCfg->encodeCfg[iIndex].streamID > 0)
        {
            // pVideo->encodeCfg[iIndex].enable
            pstVEncodeCfg[iIndex].id = pstVideoEncodeCfg->encodeCfg[iIndex].streamID;
            StrCpy(pstVEncodeCfg[iIndex].encode_mode, 8, pstVideoEncodeCfg->encodeCfg[iIndex].encodeFormat.name);
            StrCpy(pstVEncodeCfg[iIndex].resolution, 32, pstVideoEncodeCfg->encodeCfg[iIndex].resolution.name);
            StrCpy(pstVEncodeCfg[iIndex].bitrate_control, 8, pstVideoEncodeCfg->encodeCfg[iIndex].bitRateControl.name);
            sprintf(pstVEncodeCfg[iIndex].framerate, "%d", pstVideoEncodeCfg->encodeCfg[iIndex].frameRate);
            sprintf(pstVEncodeCfg[iIndex].govlength, "%d", pstVideoEncodeCfg->encodeCfg[iIndex].initQuant);
            sprintf(pstVEncodeCfg[iIndex].bitrate, "%d", pstVideoEncodeCfg->encodeCfg[iIndex].bitRate);
            // pVideo->encodeCfg[iIndex].display_frameRate
        }
    }

    char profile[8] = {0};
    if (setProfile)
    {
        if(cgi_get_value(http_url, "profile", profile, sizeof(profile)))
        {
            int i_profile = safeatoi(profile);
            if((i_profile < 0) || (i_profile > 2))
            {
                sprintf(response, XML_CGI_FAULT, url_name, "profile range: 0-2");
                status = 400;
                goto __exit;
            }        
            pstVideoEncodeCfg->encode_profile = i_profile;    
        }

        if (setStreamId == 0)
        {
            int iNeedSwitch = 0;
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                iNeedSwitch |= anj_config_video_encode_set(pstVideoEncodeCfg, iCameraIdx);
            }
            if (iNeedSwitch)
            {
                anj_video_encode_switch();
            }
        }
        
    }

    if(setStreamId > 0)
    {
        for(iIndex = 0; iIndex < v_encode_size; iIndex++)
        {
            if(pstVEncodeCfg[iIndex].id == setStreamId)
                break;
        }

        if(iIndex >= v_encode_size)
        {
            char error_smg[32] = {0};
            sprintf(error_smg, "error: stream id %d", setStreamId);
            sprintf(response, XML_CGI_FAULT, url_name, error_smg);
            status = 400;
            goto __exit;
        }

        // 获取 encode_mode
        if(cgi_get_value(http_url, "encode_mode", pstVEncodeCfg[iIndex].encode_mode, 8))
        {
            if(!verify_encode_mode_format(pstVEncodeCfg[iIndex].id, pstVEncodeCfg[iIndex].encode_mode))
            {
                sprintf(response,XML_CGI_FAULT,url_name,"encode_mode error");
                status = 400;
                goto __exit;
            }
            StrCpy(pstVideoEncodeCfg->encodeCfg[iIndex].encodeFormat.name, 8, pstVEncodeCfg[iIndex].encode_mode);
            flag_change = 1;
        }

        // 获取 resolution
        if(cgi_get_value(http_url, "resolution", pstVEncodeCfg[iIndex].resolution, 32))
        {
            if(!verify_resolution_format(pstVEncodeCfg[iIndex].id, pstVEncodeCfg[iIndex].encode_mode, pstVEncodeCfg[iIndex].resolution))
            {
                sprintf(response, XML_CGI_FAULT, url_name, "resolution error");
                status = 400;
                goto __exit;
            }
            StrCpy(pstVideoEncodeCfg->encodeCfg[iIndex].resolution.name, 32, pstVEncodeCfg[iIndex].resolution);
            flag_change = 1;
        }

        // 获取 bitrate_control
        if(cgi_get_value(http_url, "bitrate_control", pstVEncodeCfg[iIndex].bitrate_control, 8))
        {
            if(!strcmp(pstVEncodeCfg[iIndex].bitrate_control, "CBR"))
            {
                StrCpy(pstVideoEncodeCfg->encodeCfg[iIndex].bitRateControl.name, 8, pstVEncodeCfg[iIndex].bitrate_control);
            }
            else if(!strcmp(pstVEncodeCfg[iIndex].bitrate_control, "VBR"))
            {
                StrCpy(pstVideoEncodeCfg->encodeCfg[iIndex].bitRateControl.name, 8, pstVEncodeCfg[iIndex].bitrate_control);
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "bitrate_control: CBR or VBR");
                status = 400;
                goto __exit;
            }

            flag_change = 1;
        }

        // 获取 framerate
        if(cgi_get_value(http_url, "framerate", pstVEncodeCfg[iIndex].framerate, 8))
        {
            iRet = verify_framerate_format(pstVEncodeCfg[iIndex].id, pstVEncodeCfg[iIndex].encode_mode,
                                        pstVEncodeCfg[iIndex].resolution, pstVEncodeCfg[iIndex].framerate);
            if(!iRet)
            {
                sprintf(response, XML_CGI_FAULT, url_name, "framerate range error");
                status = 400;
                goto __exit;
            }
            pstVideoEncodeCfg->encodeCfg[iIndex].frameRate = iRet;
            pstVideoEncodeCfg->encodeCfg[iIndex].display_frameRate = iRet;
            flag_change = 1;
        }

        // 获取 govlength 
        if(cgi_get_value(http_url, "govlength", pstVEncodeCfg[iIndex].govlength, 8))
        {
            iRet = verify_govlength_format(pstVideoEncodeCfg->encodeCfg[iIndex].frameRate,
                                        pstVEncodeCfg[iIndex].govlength);
            if(!iRet)
            {
                char gov_msg[50] = {0};
                sprintf(gov_msg, "govlength range: %d-%d",
                                pstVideoEncodeCfg->encodeCfg[iIndex].frameRate,
                                pstVideoEncodeCfg->encodeCfg[iIndex].frameRate * 4);
                sprintf(response, XML_CGI_FAULT, url_name, gov_msg);
                status = 400;
                goto __exit;
            }
            pstVideoEncodeCfg->encodeCfg[iIndex].initQuant = iRet;
            flag_change = 1;
        }

        // 获取 bitrate
        if(cgi_get_value(http_url, "bitrate", pstVEncodeCfg[iIndex].bitrate, 8))
        {
            iRet = verify_bitrate_format(pstVEncodeCfg[iIndex].id, pstVEncodeCfg[iIndex].encode_mode,
                                        pstVEncodeCfg[iIndex].resolution, pstVEncodeCfg[iIndex].bitrate);
            if(!iRet)
            {
                sprintf(response, XML_CGI_FAULT, url_name, "bitrate range error");
                status = 400;
                goto __exit;
            }
            pstVideoEncodeCfg->encodeCfg[iIndex].bitRate = iRet;
            pstVideoEncodeCfg->encodeCfg[iIndex].bitRateQuality = VIDEO_QUALITY_CUSTOM;
            flag_change = 1;
        }
        
        if(flag_change)
        {
            int iNeedSwitch = 0;
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                iNeedSwitch |= anj_config_video_encode_set(pstVideoEncodeCfg, iCameraIdx);
            }
            if (iNeedSwitch)
            {
                anj_video_encode_switch();
            }
        }
    }


    iRet = sprintf(response + pos, XML_HEAD"<videoencoder>");
    pos += iRet;

    if (setProfile)
    {
        iRet = sprintf(response + pos, "<AdvanceEncodeConfig  EncodeProfile=\"%s\" />", profile);
        pos += iRet;
    }
    else
    {
        iRet = sprintf(response + pos, "<AdvanceEncodeConfig  EncodeProfile=\"%d\" />", pstVideoEncodeCfg->encode_profile);
        pos += iRet;
    }

    for(iIndex = 0; iIndex < 3; iIndex++)
    {
        if(pstVEncodeCfg[iIndex].id > 0)
        {
            iRet = sprintf(response + pos, "<stream%d "
                                    "encode_mode=\"%s\" "
                                    "resolution=\"%s\" "
                                    "framerate=\"%s\" "
                                    "govlength=\"%s\" "
                                    "bitrate=\"%s\" "
                                    "bitrate_control=\"%s\"/>",
                                    pstVEncodeCfg[iIndex].id,
                                    pstVEncodeCfg[iIndex].encode_mode, pstVEncodeCfg[iIndex].resolution, pstVEncodeCfg[iIndex].framerate,
                                    pstVEncodeCfg[iIndex].govlength, pstVEncodeCfg[iIndex].bitrate, pstVEncodeCfg[iIndex].bitrate_control);
            pos += iRet;
        }
    }

    iRet = sprintf(response + pos, "</videoencoder>");
    pos += iRet;

    status = 200;
    __ERR("cgi video encode handle response:%s\n", response);
__exit:    
    return status;
}


//    视频编码配置
int cgi_videoencoder_stream1(const char* url_name, const char *clientip, const char *http_url, char *response)
{
    int setStreamId = 1;
    int setProfile = 0;

    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    return cgi_videoencoder_handle(setStreamId, setProfile, url_name, http_url, response);
}
int cgi_videoencoder_stream2(const char* url_name, const char *clientip, const char *http_url, char *response)
{
    int setStreamId = 2;
    int setProfile = 0;
    
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    return cgi_videoencoder_handle(setStreamId, setProfile, url_name, http_url, response);
}
int cgi_videoencoder_stream3(const char* url_name, const char *clientip, const char *http_url, char *response)
{
    int setStreamId = 3;
    int setProfile  = 0;
    
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    return cgi_videoencoder_handle(setStreamId, setProfile, url_name, http_url, response);
}
int cgi_videoencoder_adv(const char* url_name, const char *clientip, const char *http_url, char *response)
{
    int setStreamId = 0;
    int setProfile = 1;
    
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }
    return cgi_videoencoder_handle(setStreamId, setProfile, url_name, http_url, response);
}

int get_audio_capabilities(a_capabilities *a_cap, AUDIO_CODEC_ENTRY *pAudio, int acount)
{
    int i = 0, j = 0, iIndex = 0;
    for(iIndex = 0; iIndex < acount; iIndex++)
    {
        //    a_encode_type    i
        for(i = 0; i < 5; i++)
        {
            if(strlen(a_cap->type[i].name) == 0)
            {
                StrCpy(a_cap->type[i].name, 16, pAudio[iIndex].codec_name);
                break;
            }
            else if(!strcmp(a_cap->type[i].name, pAudio[iIndex].codec_name))
            {
                break;
            }
        }
        if(i >= 5)
            continue;

        //    a_samplerate    j
        for(j = 0; j < 5; j++)
        {
            if(a_cap->type[i].samplerate[j].value == 0)
            {
                a_cap->type[i].samplerate[j].value = pAudio[iIndex].samplerate;
                break;
            }
            else if(a_cap->type[i].samplerate[j].value == pAudio[iIndex].samplerate)
            {
                break;
            }
        }
        if(j >= 5) 
            continue;
        
        //    bitrate
        a_cap->type[i].samplerate[j].bitrate = pAudio[iIndex].bitrate;

    }
    return 0;
}


//    音频编码能力集
int cgi_audioencoder_capabilities(const char *url_name, const char *clientip, const char *http_url, char *response)
{    
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int i = 0;
    int j = 0;
    int pos = 0;
    int iRet = 0;

    AUDIO_CODEC_ENTRY *pAudio = NULL;
    int acount = anj_sysmng_audio_res_array_get(&pAudio);

    a_capabilities *a_cap = (a_capabilities *)anj_mw_malloc(sizeof(a_capabilities));
    memset(a_cap, 0, sizeof(a_capabilities));

    get_audio_capabilities(a_cap, pAudio, acount);

    iRet = sprintf(response + pos, XML_HEAD"<audio_capabilities>");
    pos += iRet;

    for(i = 0; i < 5; i++)
    {
        if(strlen(a_cap->type[i].name) > 0)
        {
            iRet = sprintf(response + pos, "<encode_type name=\"%s\">", a_cap->type[i].name);
            pos += iRet;

            for(j = 0; j < 5; j++)
            {
                if(a_cap->type[i].samplerate[j].value > 0)
                {
                    iRet = sprintf(response+pos, "<samplerate name=\"%d\">", a_cap->type[i].samplerate[j].value * 1000);
                    pos += iRet;
                    iRet = sprintf(response+pos, "<bitrate>%d</bitrate>", a_cap->type[i].samplerate[j].bitrate * 1000);
                    pos += iRet;
                    iRet = sprintf(response+pos, "</samplerate>");
                    pos += iRet;
                }
            }
            
            iRet = sprintf(response + pos, "</encode_type>");
            pos += iRet;
        }
    }
    
    iRet = sprintf(response + pos, "</audio_capabilities>");
    pos += iRet;
    
    anj_mw_free(a_cap);
    return 200;
}

//    音频编码配置
int cgi_audioencoder(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char enable[2] = {0};
    char encode_type[16] = {0};
    char samplerate[8] = {0};
    char bitrate[8] = {0};
    char input_volume[8] = {0};
    char output_volume[8] = {0};
    char amplify[2] = {0};
    
    int iRet = 0;
    int flag_changeEncode = 0;
    int flag_changeCapture = 0;

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioEncode *pstAudioEncodeCfg = &pstMediaCfg->audioConfig.audioEncode;
    AudioCapture *pstAudioCaptureCfg = &pstMediaCfg->audioConfig.audioCapture;

    sprintf(enable, "%d", (pstAudioEncodeCfg->enable != 0) ? 1:0);
    StrCpy(encode_type, sizeof(encode_type), pstAudioEncodeCfg->audioEncodeType.typeName);
    sprintf(samplerate, "%d", pstAudioEncodeCfg->sampleRate);
    sprintf(bitrate, "%d", pstAudioEncodeCfg->bitRate);
    sprintf(input_volume, "%d", pstAudioCaptureCfg->volume_capture);
    sprintf(output_volume, "%d", pstAudioCaptureCfg->volume_play);
    sprintf(amplify, "%d", (pstAudioCaptureCfg->amplify != 0) ? 1:0 );
    
    // 获取 enable
    if(cgi_get_value(http_url,"enable", enable, sizeof(enable)))
    {
        if(enable[0] == '0')
        {
            pstAudioEncodeCfg->enable = 0;
        }
        else if(enable[0] == '1')
        {
            pstAudioEncodeCfg->enable = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0 or 1");
            status = 400;
            goto __exit;
        }
        flag_changeEncode=1;
    }

    // 获取 encode_type
    if(cgi_get_value(http_url, "encode_type", encode_type, sizeof(encode_type)))
    {
        if(!verify_a_encodetype_format(encode_type))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "encode_type error");
            status = 400;
            goto __exit;
        }
        StrCpy(pstAudioEncodeCfg->audioEncodeType.typeName, 16, encode_type);
        flag_changeEncode=1;
    }

    // 获取 samplerate 
    if(cgi_get_value(http_url, "samplerate", samplerate, sizeof(samplerate)))
    {
        iRet = verify_a_samplerate_format(encode_type,samplerate);
        if(!iRet)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "samplerate error");
            status = 400;
            goto __exit;
        }

        pstAudioEncodeCfg->sampleRate = iRet;
        flag_changeEncode=1;
    }

    // 获取 bitrate
    if(cgi_get_value(http_url, "bitrate", bitrate, sizeof(bitrate)))
    {
        iRet = verify_a_bitrate_format(encode_type, samplerate, bitrate);
        if(!iRet)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "bitrate error");
            status = 400;
            goto __exit;
        }

        pstAudioEncodeCfg->bitRate = iRet;
        flag_changeEncode=1;
    }

    // 获取 input_volume
    if(cgi_get_value(http_url, "input_volume", input_volume, sizeof(input_volume)))
    {
        int i_input_volume = safeatoi(input_volume);
        if((i_input_volume < 1) || (i_input_volume > 100))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "input_volume range: 1-100");
            status = 400;
            goto __exit;
        }

        pstAudioCaptureCfg->volume_capture = i_input_volume;
        flag_changeCapture = 1;
    }

    // 获取 output_volume
    if(cgi_get_value(http_url, "output_volume", output_volume, sizeof(output_volume)))
    {
        int i_output_volume = safeatoi(output_volume);
        if((i_output_volume < 1) || (i_output_volume > 100))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "output_volume range: 1-100");
            status = 400;
            goto __exit;
        }

        pstAudioCaptureCfg->volume_play = i_output_volume;
        flag_changeCapture=1;
    }

    // 获取 amplify
    if(cgi_get_value(http_url, "amplify", amplify, sizeof(amplify)))
    {
        if(amplify[0] == '0')
        {
            pstAudioCaptureCfg->amplify = 0;
        }
        else if(amplify[0] == '1')
        {
            pstAudioCaptureCfg->amplify = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "amplify: 0 or 1");
            status = 400;
            goto __exit;
        }
        flag_changeCapture=1;
    }
    
    if(flag_changeEncode)
    {
        anj_config_audio_encode_set(pstAudioEncodeCfg);
    }

    if(flag_changeCapture)
    {
        anj_config_audio_capture_set(pstAudioCaptureCfg);
    }

    sprintf(response, XML_HEAD"<audioencoder enable=\"%s\" encode_type=\"%s\" "
                                "samplerate=\"%s\" bitrate=\"%s\" "
                                "input_volume=\"%s\" output_volume=\"%s\" amplify=\"%s\"/>",
                                enable, encode_type, 
                                samplerate, bitrate,
                                input_volume, output_volume, 
                                amplify);
    status = 200;

__exit:
    return status;
}

// OSD
int cgi_osd(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char enable[8] = {0};
    char color[8] = {0};
    char fontsize[8] = {0};
    char title_pos_type[8] = {0};
    char title_pos_x[8] = {0};
    char title_pos_y[8] = {0};
    char show_res_bit[8] = {0};
    char time_pos_type[8] = {0};
    char time_pos_x[8] = {0};
    char time_pos_y[8] = {0};
    char show_week[8] = {0};
    char time_format[32] = {0};
    char title_utf8[TITLE_MAX_LEN] = {0};

    int iRet = 0;
    int flag_change = 0;
    int chn = 0;

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoOverlay *pstOsdCfg = &pstMediaCfg->videoConfig[chn].overlay;

    // OSD 启用/禁用 
    sprintf(enable, "%d", (pstOsdCfg->enable != 0) ? 1 : 0);
    // OSD 颜色风格 0 白色背景黑色文字, 1 黑色背景白色文字, 2 透明背景白框黑字, 3 透明背景黑框白字
    sprintf(color, "%d", pstOsdCfg->style);
    // OSD 字体size 0 标准, 1 大, 2 超大
    sprintf(fontsize, "%d", pstOsdCfg->fontsize);
    // OSD显示标题位置 
    sprintf(title_pos_type, "%d", pstOsdCfg->titleOverlay.posType);
    sprintf(title_pos_x, "%d", pstOsdCfg->titleOverlay.posX);
    sprintf(title_pos_y, "%d", pstOsdCfg->titleOverlay.posY);
    // OSD显示分辨率和码率 0 不显示, 1 仅显示分辨率, 2 仅显示码率, 3 显示分辨率和码率
    sprintf(show_res_bit, "%d", pstOsdCfg->transparency);
    // OSD显示时间位置 
    sprintf(time_pos_type, "%d", pstOsdCfg->timeOverlay.posType);
    sprintf(time_pos_x, "%d", pstOsdCfg->timeOverlay.posX);
    sprintf(time_pos_y, "%d", pstOsdCfg->timeOverlay.posY);
    // OSD 显示星期
    sprintf(show_week, "%d", pstOsdCfg->bDsplayWeek);
    // OSD 时间格式
    StrCpy(time_format, 32, pstOsdCfg->timeOverlay.timeFormat.format);
    // OSD 标题内容
    StrCpy(title_utf8, TITLE_MAX_LEN, pstOsdCfg->titleOverlay.title_utf8);
    
    // 获取 enable
    if (cgi_get_value(http_url, "enable", enable, sizeof(enable)))
    {
        int i_enable = safeatoi(enable);
        if ((i_enable != 0) && (i_enable != 1)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0 or 1");
            status = 400;
            goto __exit;
        }

        pstOsdCfg->enable = i_enable;
        flag_change = 1;
    }

    // 获取 color
    if (cgi_get_value(http_url, "color", color, sizeof(color))) 
    {
        int i_color = safeatoi(color);
        if ((i_color < 0) || (i_color > 3)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "color range: 0-3");
            status = 400;
            goto __exit;
        }

        pstOsdCfg->style = i_color;
        flag_change = 1;
    }

    // 获取 fontsize
    if (cgi_get_value(http_url, "fontsize", fontsize, sizeof(fontsize)))
    {
        int i_fontsize = safeatoi(fontsize);
        if ((i_fontsize < 0) || (i_fontsize > 2)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "fontsize range: 0-2");
            status = 400;
            goto __exit;
        }

        pstOsdCfg->fontsize = i_fontsize;
        flag_change = 1;
    }

    // 获取 title_pos_type 
    if (cgi_get_value(http_url, "title_pos_type", title_pos_type, sizeof(title_pos_type))) 
    {
        int i_title_pos_type = safeatoi(title_pos_type);
        if ((i_title_pos_type != 0) && (i_title_pos_type != 1)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type: 0 or 1");
            status = 400;
            goto __exit;
        }

        pstOsdCfg->titleOverlay.posType = (Positiontype)i_title_pos_type;
        flag_change = 1;
    }

    // 获取 title_pos_x title_pos_y
    if (cgi_get_value(http_url, "title_pos_x", title_pos_x, sizeof(title_pos_x))) 
    {
        int i_title_pos_x = safeatoi(title_pos_x);
        if (pstOsdCfg->titleOverlay.posType) 
        {
            if ((i_title_pos_x < 0) || (i_title_pos_x > 100)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=1, title_pos_x range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else 
        {
            if ((i_title_pos_x < 0) || (i_title_pos_x > 2)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=0, title_pos_x range: 0-2");
                status = 400;
                goto __exit;
            }
        }

        pstOsdCfg->titleOverlay.posX = i_title_pos_x;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "title_pos_y", title_pos_y, sizeof(title_pos_y))) 
    {
        int i_title_pos_y = safeatoi(title_pos_y);
        if (pstOsdCfg->titleOverlay.posType) 
        {
            if ((i_title_pos_y < 0) || (i_title_pos_y > 100)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=1, title_pos_y range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else 
        {
            if ((i_title_pos_y < 0) || (i_title_pos_y > 2)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=0, title_pos_y range: 0-2");
                status = 400;
                goto __exit;
            }
        }

        pstOsdCfg->titleOverlay.posY = i_title_pos_y;
        flag_change = 1;
    }
    
    // 获取 show_res_bit
    if (cgi_get_value(http_url, "show_res_bit", show_res_bit, sizeof(show_res_bit))) 
    {
        int i_show_res_bit = safeatoi(show_res_bit);
        if ((i_show_res_bit < 0) || (i_show_res_bit > 3)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "show_res_bit range: 0-3");
            status = 400;
            goto __exit;
        }

        pstOsdCfg->transparency = (titleFormatEn)i_show_res_bit;
        flag_change = 1;
    }

    // 获取 time_pos_type 
    if (cgi_get_value(http_url, "time_pos_type", time_pos_type, sizeof(time_pos_type))) 
    {
        int i_time_pos_type = safeatoi(time_pos_type);
        if ((i_time_pos_type != 0) && (i_time_pos_type != 1)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "time_pos_type: 0 or 1");
            status = 400;
            goto __exit;
        }

        pstOsdCfg->timeOverlay.posType = (Positiontype)i_time_pos_type;
        flag_change = 1;
    }

    // 获取 time_pos_x time_pos_y
    if (cgi_get_value(http_url, "time_pos_x", time_pos_x, sizeof(time_pos_x))) 
    {
        int i_time_pos_x = safeatoi(time_pos_x);
        if (pstOsdCfg->timeOverlay.posType) 
        {
            if ((i_time_pos_x < 0) || (i_time_pos_x > 100)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "time_pos_type=1, time_pos_x range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else {
            if ((i_time_pos_x < 0) || (i_time_pos_x > 2)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "time_pos_type=0, time_pos_x range: 0-2");
                status = 400;
                goto __exit;
            }
        }

        pstOsdCfg->timeOverlay.posX = i_time_pos_x;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "time_pos_y", time_pos_y, sizeof(time_pos_y))) 
    {
        int i_time_pos_y = safeatoi(time_pos_y);
        if (pstOsdCfg->timeOverlay.posType) 
        {
            if ((i_time_pos_y < 0) || (i_time_pos_y > 100)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "time_pos_type=1, time_pos_y range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else {
            if ((i_time_pos_y < 0) || (i_time_pos_y > 2))
            {
                sprintf(response, XML_CGI_FAULT, url_name, "time_pos_type=0, time_pos_y range: 0-2");
                status = 400;
                goto __exit;
            }
        }

        pstOsdCfg->timeOverlay.posY = i_time_pos_y;
        flag_change = 1;
    }

    // 获取 show_week
    if (cgi_get_value(http_url, "show_week", show_week, sizeof(show_week)) )
    {
        int i_show_week = safeatoi(show_week);
        if ((i_show_week != 0) && (i_show_week != 1)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "show_week: 0 or 1");
            status = 400;
            goto __exit;
        }

        pstOsdCfg->bDsplayWeek = i_show_week;
        flag_change = 1;
    }

    // 获取 time_format
    if (cgi_get_value(http_url, "time_format", time_format, sizeof(time_format))) 
    {
        if (!verify_osd_time_format(time_format)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "time_format error");
            status = 400;
            goto __exit;
        }
        StrCpy(pstOsdCfg->timeOverlay.timeFormat.format, 32, time_format);
        flag_change = 1;
    }

    // 获取 title_utf8 title_utf8
    if (cgi_get_value(http_url, "title_utf8", title_utf8, TITLE_MAX_LEN)) 
    {
        if (strcmp(title_utf8, "NULL") == 0)
        {
            memset(pstOsdCfg->titleOverlay.title_utf8, 0, sizeof(pstOsdCfg->titleOverlay.title_utf8));
            memset(title_utf8, 0, sizeof(title_utf8));
        }
        else if (strcmp(title_utf8, "null") == 0)
        {
            memset(pstOsdCfg->titleOverlay.title_utf8, 0, sizeof(pstOsdCfg->titleOverlay.title_utf8));
            memset(title_utf8, 0, sizeof(title_utf8));
        }
        else
        {
            iRet = transform_osd_title_utf8(title_utf8, pstOsdCfg->titleOverlay.title_utf8, TITLE_MAX_LEN);
            memcpy(title_utf8, pstOsdCfg->titleOverlay.title_utf8, iRet);
            title_utf8[iRet] = '\0';
        }
        flag_change = 1;
    }
    
    if (!pstOsdCfg->titleOverlay.posType) 
    {
        if (pstOsdCfg->titleOverlay.posX > 2)
            pstOsdCfg->titleOverlay.posX = 0;
        if (pstOsdCfg->titleOverlay.posY > 2)
            pstOsdCfg->titleOverlay.posY = 0;
    }
    if (!pstOsdCfg->timeOverlay.posType) 
    {
        if (pstOsdCfg->timeOverlay.posX > 2)
            pstOsdCfg->timeOverlay.posX = 1;
        if (pstOsdCfg->timeOverlay.posY > 2)
            pstOsdCfg->timeOverlay.posY = 1;
    }

    if (flag_change)
    {
        int iCameraIdx = 0;
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            anj_config_overlay_set(pstOsdCfg, iCameraIdx);
        }
    }

    sprintf(response, XML_HEAD "<osd enable=\"%s\" color=\"%s\" fontsize=\"%s\" "
            "title_pos_type=\"%s\" title_pos_x=\"%s\" title_pos_y=\"%s\" show_res_bit=\"%s\" "
            "time_pos_type=\"%s\" time_pos_x=\"%s\" time_pos_y=\"%s\" show_week=\"%s\" "
            "time_format=\"%s\" title_utf8=\"%s\"/>",
            enable, color, fontsize, title_pos_type, title_pos_x, title_pos_y, show_res_bit,
            time_pos_type, time_pos_x, time_pos_y, show_week, time_format, title_utf8);

    status = 200;

__exit:
    return status;
}

int cgi_userosd(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char title_index[8] = {0};
    char enable[8] = {0};
    char color[16] = {0};
    char fontsize[8] = {0};
    char title_pos_type[8] = {0};
    char title_pos_x[8] = {0};
    char title_pos_y[8] = {0};
    char title_utf8[TITLE_MAX_LEN] = {0}; // TITLE_MAX_LEN 200
    
    int iRet = 0;
    int flag_change = 0;
    int index = 0;

    int chn = 0;

    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoUserOverlay *pstUsrOsdCfg = &pstMediaCfg->videoConfig[chn].useroverlay;

    /* 必须携带 title_index, 否则返回失败 */
    if (cgi_get_value(http_url, "title_index", title_index, sizeof(title_index)) == 0)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "title_index not find");
        status = 400;
        goto __exit;
    }
    else
    {
        index = safeatoi(title_index);
        if (index >= MAX_USER_OSD_NUM || index < 0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "title_index is incorrect");
            status = 400;
            goto __exit;
        }
    }

    // OSD 启用/禁用 
    snprintf(enable, sizeof(enable) - 1, "%d", (pstUsrOsdCfg->data[index].enable != 0) ? 1 : 0);
    // OSD 颜色风格 0 白色背景黑色文字, 1 黑色背景白色文字, 2 透明背景白框黑字, 3 透明背景黑框白字
    snprintf(color, sizeof(color) - 1, "0x%6x", pstUsrOsdCfg->data[index].color_front);
    // OSD 字体size 0 标准, 1 大, 2 超大
    snprintf(fontsize, sizeof(fontsize) - 1, "%d", pstUsrOsdCfg->data[index].fontsize);
    // OSD显示标题位置 
    snprintf(title_pos_type, sizeof(title_pos_type) - 1, "%d", pstUsrOsdCfg->data[index].posType);
    snprintf(title_pos_x, sizeof(title_pos_x) - 1, "%d", pstUsrOsdCfg->data[index].pos_xscale);
    snprintf(title_pos_y, sizeof(title_pos_y) - 1, "%d", pstUsrOsdCfg->data[index].pos_yscale);
    // OSD 标题内容
    StrCpy(title_utf8, TITLE_MAX_LEN, pstUsrOsdCfg->data[index].title_utf8);

    // 获取 enable
    if (cgi_get_value(http_url, "enable", enable, sizeof(enable))) 
    {
        int i_enable = safeatoi(enable);
        if ((i_enable != 0) && (i_enable != 1)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0 or 1");
            status = 400;
            goto __exit;
        }

        pstUsrOsdCfg->data[index].enable = i_enable;
        flag_change = 1;
    }

    // 获取 color
    if (cgi_get_value(http_url, "color", color, sizeof(color))) 
    {
        //int i_color = safeatoi(color);
        int i_color = strtol(color, NULL, 16);
        if ((i_color < 0) || (i_color > 0xffffff)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "color range: 0x00 - 0xffffff");
            status = 400;
            goto __exit;
        }

        pstUsrOsdCfg->data[index].color_front = i_color;
        flag_change = 1;
    }

    // 获取 fontsize
    if (cgi_get_value(http_url, "fontsize", fontsize, sizeof(fontsize))) 
    {
        int i_fontsize = safeatoi(fontsize);
        if ((i_fontsize < 0) || (i_fontsize > 2)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "fontsize range: 0 - 2");
            status = 400;
            goto __exit;
        }

        pstUsrOsdCfg->data[index].fontsize = i_fontsize;
        flag_change = 1;
    }

    // 获取 title_pos_type 
    if (cgi_get_value(http_url, "title_pos_type", title_pos_type, sizeof(title_pos_type))) 
    {
        int i_title_pos_type = safeatoi(title_pos_type);
        if ((i_title_pos_type != 0) && (i_title_pos_type != 1))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type: 0 or 1");
            status = 400;
            goto __exit;
        }

        pstUsrOsdCfg->data[index].posType = (Positiontype)i_title_pos_type;
        flag_change = 1;
    }

    // 获取 title_pos_x title_pos_y
    if (cgi_get_value(http_url, "title_pos_x", title_pos_x, sizeof(title_pos_x))) 
    {
        int i_title_pos_x = safeatoi(title_pos_x);
        if (pstUsrOsdCfg->data[index].posType) 
        {
            if ((i_title_pos_x < 0) || (i_title_pos_x > 100)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=1, title_pos_x range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else 
        {
            if ((i_title_pos_x < 0) || (i_title_pos_x > 2)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=0, title_pos_x range: 0-2");
                status = 400;
                goto __exit;
            }
        }

        pstUsrOsdCfg->data[index].pos_xscale = i_title_pos_x;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "title_pos_y", title_pos_y, sizeof(title_pos_y))) 
    {
        int i_title_pos_y = safeatoi(title_pos_y);
        if (pstUsrOsdCfg->data[index].posType) 
        {
            if ((i_title_pos_y < 0) || (i_title_pos_y > 100)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=1, title_pos_y range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else {
            if ((i_title_pos_y < 0) || (i_title_pos_y > 2)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=0, title_pos_y range: 0-2");
                status = 400;
                goto __exit;
            }
        }

        pstUsrOsdCfg->data[index].pos_yscale = i_title_pos_y;
        flag_change = 1;
    }

    // 获取 title_utf8
    if (cgi_get_value(http_url, "title_utf8", title_utf8, TITLE_MAX_LEN)) 
    {
        iRet = transform_osd_title_utf8(title_utf8, pstUsrOsdCfg->data[index].title_utf8, TITLE_MAX_LEN);
        memcpy(title_utf8, pstUsrOsdCfg->data[index].title_utf8, iRet);
        title_utf8[iRet] = '\0';
        flag_change = 1;
    }

    if (!pstUsrOsdCfg->data[index].posType) 
    {
        if (pstUsrOsdCfg->data[index].pos_xscale > 2)
            pstUsrOsdCfg->data[index].pos_xscale = 0;
        if (pstUsrOsdCfg->data[index].pos_yscale > 2)
            pstUsrOsdCfg->data[index].pos_yscale = 0;
    }

    if (flag_change)
    {
        int iCameraIdx = 0;
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            anj_config_user_overlay_set(pstUsrOsdCfg, iCameraIdx);
        }
    }

    sprintf(response, XML_HEAD "<osd enable=\"%s\" color=\"%s\" fontsize=\"%s\" "
            "title_pos_type=\"%s\" title_pos_x=\"%s\" title_pos_y=\"%s\" "
            "title_utf8=\"%s\"/>",
            enable, color, fontsize, title_pos_type, title_pos_x, title_pos_y, title_utf8);

    status = 200;

__exit:
    return status;
}

int cgi_setUserOsdToEncodeOnly(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }
#if 0
    char enable[8] = {0};
    char fontsize[8] = {0};
    char title_pos_type[8] = {0};
    char title_pos_x[8] = {0};
    char title_pos_y[8] = {0};
    char title_utf8[MAX_USER_OSD_TEXT_LEN] = {0}; // TITLE_MAX_LEN 200
    
    int ret = 0;
    int flag_change = 0;
    int index = 5;
    int i_title_pos_x = 0;
    int i_title_pos_y = 0;

    MSG_USR_OSD_DATA *pUserOsd = (MSG_USR_OSD_DATA *)malloc(sizeof(MSG_USR_OSD_DATA));
    memset(pUserOsd, 0, sizeof(MSG_USR_OSD_DATA));

    // 获取 enable
    if (cgi_get_value(http_url, "enable", enable, sizeof(enable)))
    {
        int i_enable = safeatoi(enable);
        if ((i_enable != 0) && (i_enable != 1))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0 or 1");
            status = 400;
            goto __exit;
        }

        pUserOsd->valid = i_enable;
        flag_change = 1;
    }

    // 获取 fontsize
    if (cgi_get_value(http_url, "fontsize", fontsize, sizeof(fontsize)))
    {
        int i_fontsize = safeatoi(fontsize);
        if ((i_fontsize < 0) || (i_fontsize > 2)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "fontsize range: 0 - 2");
            status = 400;
            goto __exit;
        }
        pUserOsd->fontsize = i_fontsize;
        flag_change = 1;
    }

    // 获取 title_pos_type 
    if (cgi_get_value(http_url, "title_pos_type", title_pos_type, sizeof(title_pos_type)))
    {
        int i_title_pos_type = safeatoi(title_pos_type);
        if ((i_title_pos_type != 0) && (i_title_pos_type != 1))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type: 0 or 1");
            status = 400;
            goto __exit;
        }

        pUserOsd->posision.type = (Positiontype)i_title_pos_type;
        flag_change = 1;
    }

    // 获取 title_pos_x title_pos_y
    if (cgi_get_value(http_url, "title_pos_x", title_pos_x, sizeof(title_pos_x)))
    {
        i_title_pos_x = safeatoi(title_pos_x);
        if (pUserOsd->posision.type)
        {
            if ((i_title_pos_x < 0) || (i_title_pos_x > 100))
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=1, title_pos_x range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else 
        {
            if ((i_title_pos_x < 0) || (i_title_pos_x > 2)) 
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=0, title_pos_x range: 0-2");
                status = 400;
                goto __exit;
            }
        }
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "title_pos_y", title_pos_y, sizeof(title_pos_y)))
    {
        i_title_pos_y = safeatoi(title_pos_y);
        if (pUserOsd->posision.type)
        {
            if ((i_title_pos_y < 0) || (i_title_pos_y > 100))
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=1, title_pos_y range: 0-100");
                status = 400;
                goto __exit;
            }
        }
        else 
        {
            if ((i_title_pos_y < 0) || (i_title_pos_y > 2))
            {
                sprintf(response, XML_CGI_FAULT, url_name, "title_pos_type=0, title_pos_y range: 0-2");
                status = 400;
                goto __exit;
            }
        }
        flag_change = 1;
    }

    // 获取 title_utf8
    if (cgi_get_value(http_url, "title_utf8", title_utf8, MAX_USER_OSD_TEXT_LEN))
    {
        ret = transform_osd_title_utf8(title_utf8, pUserOsd->text, MAX_USER_OSD_TEXT_LEN);
        memcpy(title_utf8, pUserOsd->text, ret);
        title_utf8[ret] = '\0';
        flag_change = 1;
    }

    if (flag_change)
    {
        SetUsrOsdToEncode(index, pUserOsd->valid, pUserOsd->posision.type,
              i_title_pos_x, // X方向位置, 基于整个画面的比例(0-100)
              i_title_pos_y, // Y方向位置, 基于整个画面的比例(0-100)
              pUserOsd->fontsize,
              title_utf8
              );
    }

    sprintf(response, XML_HEAD "<osd enable=\"%s\" fontsize=\"%s\" "
            "title_pos_type=\"%s\" title_pos_x=\"%s\" title_pos_y=\"%s\" "
            "title_utf8=\"%s\"/>",
            enable, fontsize, title_pos_type, title_pos_x, title_pos_y, title_utf8);

    status = 200;
#else
    sprintf(response, XML_CGI_FAULT, url_name, "don't support");
    status = 400;

#endif

//__exit:
    return status;
}

// 图像
int cgi_image(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    //        亮度            对比度            饱和度                锐度
    char brightness[8] = {0};
    char contrast[8] = {0};
    char saturation[8] = {0};
    char sharpness[8] = {0};
    char tvsystem[8] = {0};
    char antificker[8] = {0};
    int flag_change = 0;
    char hlc[8] = {0};
    char blc[8] = {0};
    char dnr_2d[8] = {0};
    char dnr_3d[8] = {0};

    __INFO("cgi image set\n");
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    sprintf(brightness, "%d", pstVideoCapCfg->brightness);
    sprintf(contrast, "%d", pstVideoCapCfg->contrast);
    sprintf(saturation, "%d", pstVideoCapCfg->saturation);
    sprintf(sharpness, "%d", pstVideoCapCfg->sharpness);
    sprintf(hlc, "%d", pstVideoCapCfg->HLC);
    sprintf(blc, "%d", pstVideoCapCfg->backlight);
    sprintf(dnr_2d, "%d", pstVideoCapCfg->tnf);
    sprintf(dnr_3d, "%d", pstVideoCapCfg->snf);

    if (pstVideoCapCfg->tvsystem == 0) // 60hz
        sprintf(tvsystem, "%d", 60);
    else // 50hz
        sprintf(tvsystem, "%d", 50);

    if (pstVideoCapCfg->forct_antiflicker == 0)
        sprintf(antificker, "%d", 0);
    else
        sprintf(antificker, "%d", 1);

    // 获取 brightness
    if (cgi_get_value(http_url, "brightness", brightness, sizeof(brightness))) 
    {
        int i_brightness = safeatoi(brightness);
        if ((i_brightness < 0) || (i_brightness > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "brightness range: 0-255");
            status = 400;
            goto __exit;
        }

        pstVideoCapCfg->brightness = i_brightness;
        flag_change = 1;
    }

    // 获取 contrast
    if (cgi_get_value(http_url, "contrast", contrast, sizeof(contrast))) 
    {
        int i_contrast = safeatoi(contrast);
        if ((i_contrast < 0) || (i_contrast > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "contrast range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->contrast = i_contrast;
        flag_change = 1;
    }

    // 获取 saturation
    if (cgi_get_value(http_url, "saturation", saturation, sizeof(saturation))) 
    {
        int i_saturation = safeatoi(saturation);
        if ((i_saturation < 0) || (i_saturation > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "saturation range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->saturation = i_saturation;
        flag_change = 1;
    }
    // 获取 sharpness
    if (cgi_get_value(http_url, "sharpness", sharpness, sizeof(sharpness))) 
    {
        int i_sharpness = safeatoi(sharpness);
        if ((i_sharpness < 0) || (i_sharpness > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "sharpness range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->sharpness = i_sharpness;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "sharpness", sharpness, sizeof(sharpness))) 
    {
        int i_sharpness = safeatoi(sharpness);
        if ((i_sharpness < 0) || (i_sharpness > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "sharpness range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->sharpness = i_sharpness;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "hlc", hlc, sizeof(hlc)))
    {
        int i_hlc = safeatoi(hlc);
        if ((i_hlc < 0) || (i_hlc > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "hlc range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->HLC = i_hlc;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "blc", blc, sizeof(blc))) 
    {
        int i_blc = safeatoi(blc);
        if ((i_blc < 0) || (i_blc > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "blc range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->backlight = i_blc;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "nr2d", dnr_2d, sizeof(dnr_2d))) 
    {
        int i_dnr2d = safeatoi(dnr_2d);
        if ((i_dnr2d < 0) || (i_dnr2d > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "nr2d range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->tnf = i_dnr2d;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "nr3d", dnr_3d, sizeof(dnr_3d))) 
    {
        int i_dnr3d = safeatoi(dnr_3d);
        if ((i_dnr3d < 0) || (i_dnr3d > 255)) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "nr3d range: 0-255");
            status = 400;
            goto __exit;
        }
        pstVideoCapCfg->snf = i_dnr3d;
        flag_change = 1;
    }

    if (cgi_get_value(http_url, "tvsystem", tvsystem, sizeof(tvsystem))) 
    {
        int i_tvsystem = safeatoi(tvsystem);
        if (i_tvsystem != 50 && i_tvsystem != 60) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "tvsystem should be 60hz or 50hz");
            status = 400;
            goto __exit;
        }

        if (i_tvsystem == 60)
            pstVideoCapCfg->tvsystem = 0;
        else
            pstVideoCapCfg->tvsystem = 1;

        flag_change = 1;
    }

    if (cgi_get_value(http_url, "antificker", antificker, sizeof(antificker))) 
    {
        int i_antificker = safeatoi(antificker);
        if (i_antificker < 0 || i_antificker > 1) 
        {
            sprintf(response, XML_CGI_FAULT, url_name, "antificker should be 0 or 1");
            status = 400;
            goto __exit;
        }

        if (i_antificker)
            pstVideoCapCfg->forct_antiflicker = 1;
        else
            pstVideoCapCfg->forct_antiflicker = 0;

        flag_change = 1;
    }
    
    if (flag_change)
    {
        int cameraIndex = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pstVideoCapCfg, cameraIndex);
            anj_ispctl_config_set();
        }
    }

    sprintf(response, XML_HEAD "<image brightness=\"%s\" contrast=\"%s\" "
            "saturation=\"%s\" sharpness=\"%s\" hlc=\"%s\" blc=\"%s\" nr2d=\"%s\" nr3d=\"%s\" tvsystem=\"%s\" antificker=\"%s\" "
            "brightness_range=\"0-255\" contrast_range=\"0-255\" saturation_range=\"0-255\" sharpness_range=\"0-255\" hlc_range=\"0-255\" "
            "blc_range=\"0-255\" nr2d_range=\"0-255\" nr3d_range=\"0-255\" tvsystem_range=\"50,60\" antificker_range=\"0,1\" />",
            brightness, contrast, saturation, sharpness, hlc, blc, dnr_2d, dnr_3d, tvsystem, antificker);

    status = 200;
    
__exit:
    return status;
}


// 云台控制
int cgi_ptz_ctrl(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    char stop[8] = {0};
    char move_x[8] = {0};
    char move_y[8] = {0};
    char speed[8] = {0};
    char zoom[8] = {0};
    char focus[8] = {0};
    char iris[8] = {0};
    char preset[8] = {0};
    int flag_change = 0;
    int hasMoveX = 0;
    int hasMoveY = 0;
    int i_speed = 5;
    int i_move_x = 0;
    int i_move_y = 0;
    int i_preset = 0;
    char multiple[8] = {0};

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    // 获取 move_x
    if (cgi_get_value(http_url, "move_x", move_x, sizeof(move_x)))
    {
        if (!strcmp(move_x, "-1"))
        {
            i_move_x = -1;
        }
        else if (!strcmp(move_x, "0"))
        {
            i_move_x = 0;
        }
        else if (!strcmp(move_x, "1"))
        {
            i_move_x = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "move_x range: -1,0,1");
            status = 400;
            goto __exit;
        }
        hasMoveX = 1;
        flag_change = 1;
    }

    // 获取 move_y
    if (cgi_get_value(http_url, "move_y", move_y, sizeof(move_y)))
    {
        if (!strcmp(move_y, "-1"))
        {
            i_move_y = -1;
        }
        else if (!strcmp(move_y, "0"))
        {
            i_move_y = 0;
        }
        else if (!strcmp(move_y, "1"))
        {
            i_move_y = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "move_y range: -1,0,1");
            status = 400;
            goto __exit;
        }
        hasMoveY = 1;
    }

    // 获取 speed
    if (cgi_get_value(http_url, "speed", speed, sizeof(speed)))
    {
        i_speed = safeatoi(speed);
        if ((i_speed < 1) || (i_speed > 10))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "speed range: 1-10");
            status = 400;
            goto __exit;
        }
    }

    // 控制云台方向移动
    if (hasMoveX || hasMoveY)
    {
        if ((i_move_x == -1) && (i_move_y == 0))
        {
            strncpy(stPtzCmdParse.ptzCmd, "left", sizeof(stPtzCmdParse.ptzCmd));
        }
        else if ((i_move_x == 1) && (i_move_y == 0))
        {
            strncpy(stPtzCmdParse.ptzCmd, "right", sizeof(stPtzCmdParse.ptzCmd));
        }
        else if ((i_move_x == 0) && (i_move_y == 1))
        {
            strncpy(stPtzCmdParse.ptzCmd, "up", sizeof(stPtzCmdParse.ptzCmd));
        }
        else if ((i_move_x == 0) && (i_move_y == -1))
        {
            strncpy(stPtzCmdParse.ptzCmd, "down", sizeof(stPtzCmdParse.ptzCmd));
        }
        else if ((i_move_x == -1) && (i_move_y == 1))
        {
            strncpy(stPtzCmdParse.ptzCmd, "left_up", sizeof(stPtzCmdParse.ptzCmd));
        }
        else if ((i_move_x == -1) && (i_move_y == -1))
        {
            strncpy(stPtzCmdParse.ptzCmd, "left_down", sizeof(stPtzCmdParse.ptzCmd));
        }
        else if ((i_move_x == 1) && (i_move_y == 1))
        {
            strncpy(stPtzCmdParse.ptzCmd, "right_up", sizeof(stPtzCmdParse.ptzCmd));
        }
        else if ((i_move_x == 1) && (i_move_y == -1))
        {
            strncpy(stPtzCmdParse.ptzCmd, "right_down", sizeof(stPtzCmdParse.ptzCmd));
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "Unknown direction");
            status = 400;
            goto __exit;
        }

        stPtzCmdParse.panSpeed = i_speed;
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 获取 zoom
    if (cgi_get_value(http_url, "zoom", zoom, sizeof(zoom)))
    {
        if (!strcmp(zoom, "1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "zoomtele", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else if (!strcmp(zoom, "-1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "zoomwide", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "zoom: -1 or 1");
            status = 400;
            goto __exit;
        }
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 获取 focus
    if (cgi_get_value(http_url, "focus", focus, sizeof(focus)))
    {
        if (!strcmp(focus, "1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "FocusNearAutoOff", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else if (!strcmp(focus, "-1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "FocusFarAutoOff", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "focus: -1 or 1");
            status = 400;
            goto __exit;
        }
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 获取 iris
    if (cgi_get_value(http_url, "iris", iris, sizeof(iris)))
    {
        if (!strcmp(iris, "1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "IrisOpenAutoOff", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else if (!strcmp(iris, "-1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "IrisCloseAutoOff", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "iris: -1 or 1");
            status = 400;
            goto __exit;
        }
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 获取 call_preset 
    if (cgi_get_value(http_url, "call_preset", preset, sizeof(preset)))
    {
        i_preset = safeatoi(preset);
        if ((i_preset < 1) || (i_preset > 255))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "preset range: 1-255");
            status = 400;
            goto __exit;
        }

        strncpy(stPtzCmdParse.ptzCmd, "callpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = i_preset;
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 获取 add_preset 
    if (cgi_get_value(http_url, "add_preset", preset, sizeof(preset)))
    {
        i_preset = safeatoi(preset);
        if ((i_preset < 1) || (i_preset > 255))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "preset range: 1-255");
            status = 400;
            goto __exit;
        }

        strncpy(stPtzCmdParse.ptzCmd, "add_preset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = i_preset;
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 获取 delete_preset 
    if (cgi_get_value(http_url, "delete_preset", preset, sizeof(preset)))
    {
        i_preset = safeatoi(preset);
        if ((i_preset < 1) || (i_preset > 255))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "preset range: 1-255");
            status = 400;
            goto __exit;
        }

        strncpy(stPtzCmdParse.ptzCmd, "clearpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = i_preset;
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 设置倍数
    if (cgi_get_value(http_url, "set_multiple", multiple, sizeof(multiple)))
    {
        __DBG("cgi_get_value set_multiple\n");
        if (multiple != NULL)
        {
            char intBuf[8] = {0};
            char decBuf[8] = {0};
            if (strstr(multiple, ".") != NULL)
            {
                const char s[4] = ".";
                char *token = NULL;

                int iIndex = 0;
                token = strtok(multiple, s);
                while (token != NULL)
                {
                    if (iIndex == 0)
                    {
                        strncpy(intBuf, token, sizeof(intBuf) - 1);
                    }
                    else if (iIndex == 1)
                    {
                        strncpy(decBuf, token, sizeof(decBuf) - 1);
                    }

                    __ERR("Cgi get multiple iIndex:%s \n", token);
                    
                    iIndex++;
                    token = strtok(NULL, s);
                }
            }
            else
            {
                snprintf(intBuf, sizeof(intBuf), "%s", multiple);
            }

            int highOrder = atoi(intBuf);
            int lowOrder = atoi(decBuf);

            __DBG("set_multiple value:%s, intBuf:%s, decBuf:%s, highOrder:%d, lowOrder:%d\n",
                multiple, intBuf, decBuf, highOrder, lowOrder);
            
            // 获取倍数发送到串口
            PtzTransCmd stPelcod_D = {0};
            stPelcod_D.datalen = 7;
            memset(stPelcod_D.buffer, 0, sizeof(stPelcod_D.buffer));
            stPelcod_D.buffer[0] = 0xff;
            stPelcod_D.buffer[1] = 0x01;
            stPelcod_D.buffer[2] = 0x00;
            stPelcod_D.buffer[3] = 0x54;
            stPelcod_D.buffer[4] = highOrder;
            stPelcod_D.buffer[5] = lowOrder;
            stPelcod_D.buffer[6] = stPelcod_D.buffer[1] + stPelcod_D.buffer[2] +
                                  stPelcod_D.buffer[3] + stPelcod_D.buffer[4] + 
                                  stPelcod_D.buffer[5];
            stPelcod_D.buffer[7] = '\0';

            __DBG("d cmd: %02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
                stPelcod_D.buffer[0],
                stPelcod_D.buffer[1],
                stPelcod_D.buffer[2],
                stPelcod_D.buffer[3],
                stPelcod_D.buffer[4],
                stPelcod_D.buffer[5],
                stPelcod_D.buffer[6]);

            // 写入到/tmp/pelcod_cood
            //todo
        }
        __DBG("zoom done\n");
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    // 获取 stop
    if (cgi_get_value(http_url, "stop", stop, sizeof(stop)))
    {
        if (strcmp(stop, "1"))
        {
            sprintf(response, XML_CGI_FAULT, url_name, "stop should be 1");
            status = 400;
            goto __exit;
        }

        strncpy(stPtzCmdParse.ptzCmd, "stop", sizeof(stPtzCmdParse.ptzCmd));
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        flag_change = 1;
    }

    if (!flag_change)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, parse parameters");
        status = 400;
        goto __exit;
    }
    
    sprintf(response, XML_HEAD "<ptz_ctrl>true</ptz_ctrl>");
    status = 200;
    
__exit:
    return status;
}

int cgi_focus_slight_ctrl(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }
    
    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    char focus[8] = {0};
    int flag_change = 0;

    PtzCmdParse stPtzCmdParse = {0};
    EventResult event_result = {0};

    // 获取 focus
    if (cgi_get_value(http_url, "setfocus", focus, 8))
    {
        if (!strcmp(focus, "1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "FocusNearAutoOff", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else if (!strcmp(focus, "-1"))
        {
            strncpy(stPtzCmdParse.ptzCmd, "FocusFarAutoOff", sizeof(stPtzCmdParse.ptzCmd));
            eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "focus: -1 or 1");
            status = 400;
            goto __exit;
        }
        flag_change = 1;
    }

    memset(&stPtzCmdParse, 0, sizeof(stPtzCmdParse));
    if (flag_change == 1)
    {
        usleep(200 * 1000);

        strncpy(stPtzCmdParse.ptzCmd, "stop", sizeof(stPtzCmdParse.ptzCmd));
        eventhub_publish(EVENTHUB_CLASS_CTRL, (char *)EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }

    if (!flag_change)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, parse parameters");
        status = 400;
        goto __exit;
    }

    sprintf(response, XML_HEAD "<ptz_ctrl>true</ptz_ctrl>");
    status = 200;

__exit:
    return status;
}



//  AF状态
int cgi_getAfStatus(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    PtzTransCmd stPelcod_D = {0};

    stPelcod_D.datalen = 7;
    memset(stPelcod_D.buffer, 0, sizeof(MAX_INNNER_ENCODE_CMD));
    stPelcod_D.buffer[0] = 0xff;
    stPelcod_D.buffer[1] = 0x01;
    stPelcod_D.buffer[2] = 0x00;
    stPelcod_D.buffer[3] = 0x58;
    stPelcod_D.buffer[4] = 0x00;
    stPelcod_D.buffer[5] = 0x00;
    stPelcod_D.buffer[6] = 0x59;
    stPelcod_D.buffer[7] = '\0';

    // 写入到/tmp/pelcod_cood
    //AuxMsgPTZCmdTransData(Pelcod_D);
    // todo
    __ERR("Pelcod_D send ok!\n");

    usleep(200 * 1000);
    if (access("/tmp/cgi_AfStatus_doing", F_OK) == F_OK)
    {
        sprintf(response, XML_HEAD "<AfStatus>status=0</AfStatus>");
    }
    else if (access("/tmp/cgi_AfStatus_done", F_OK) == F_OK)
    {
        sprintf(response, XML_HEAD "<AfStatus>status=1</AfStatus>");
    }
    else
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, parse parameters");
    }

    status = 200;
    return status;
}

int cgi_motion(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    #define AREABYTELEN (22 * 18 / 8 + 1)

    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    char enable[8] = {0};
    char sensitivity[8] = {0};
    char alarmthreshold[8] = {0};
    char enable_nighttime[8] = {0};
    char night_sensitivity[8] = {0};
    char night_alarmthreshold[8] = {0};
    char blockcount[16] = {0};
    
    char areavalue[AREABYTELEN * 4] = {0};
    char areabyte[AREABYTELEN] = {0};
    
    int cols = -1;
    int rows = -1;
    int i = 0;
    int j = 0;
    int nbyte = 0;
    int index = 0;
    const int offset = 2;

    int bsetFlag = 0;
    int iCameraIdx = 0;

    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pstTmpMotionAlarmConfig = (MotionDetectAlarm *) anj_mw_malloc (sizeof(MotionDetectAlarm));
    if (pstTmpMotionAlarmConfig == NULL)
    {
        __ERR("tmp config malloc failed!\n");
        status = 400;
        goto __exit;
    }
    memcpy(pstTmpMotionAlarmConfig, &pstAlarmConfig->normalAlarm.motionDetectAlarm[iCameraIdx], sizeof(MotionDetectAlarm));

    /* getconfig */
    snprintf(enable, sizeof(enable) - 1, "%d", pstTmpMotionAlarmConfig->enable != 0 ? 1 : 0);
    snprintf(sensitivity, sizeof(sensitivity) - 1, "%d", pstTmpMotionAlarmConfig->sensitivity);
    snprintf(alarmthreshold, sizeof(alarmthreshold) - 1, "%d", pstTmpMotionAlarmConfig->alarmThreshold);
    snprintf(enable_nighttime, sizeof(enable_nighttime) - 1, "%d", pstTmpMotionAlarmConfig->dayNightSwitch != 0 ? 1 : 0);
    snprintf(night_sensitivity, sizeof(night_sensitivity) - 1, "%d", pstTmpMotionAlarmConfig->nightSensitivity);
    snprintf(night_alarmthreshold, sizeof(night_alarmthreshold) - 1, "%d", pstTmpMotionAlarmConfig->nightAlarmThreshold);

    cols = pstTmpMotionAlarmConfig->blockCount & 0xffff;
    rows = (pstTmpMotionAlarmConfig->blockCount >> 16) & 0xffff;
    snprintf(blockcount, sizeof(blockcount) - 1, "%dx%d", rows, cols);

    nbyte = (cols * rows) / 8;
    if ((cols * rows) % 8 != 0)
    {
        nbyte += 1;
    }

    get_area_value(areabyte, areavalue, pstTmpMotionAlarmConfig->blockCfg, rows, cols, pstTmpMotionAlarmConfig->enable, offset);

    /* setconfig */
    if (cgi_get_value(http_url, "enable", enable, 2))
    {
        if (enable[0] == '0' && enable[1] == 0)
        {
            pstTmpMotionAlarmConfig->enable = 0;
        }
        else if (enable[0] == '1' && enable[1] == 0)
        {
            pstTmpMotionAlarmConfig->enable = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0 or 1");
            status = 400;
            goto __exit;
        }

        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "sensitivity", sensitivity, sizeof(sensitivity)))
    {
        int isensitivity = atoi(sensitivity);
        if (0 <= isensitivity && isensitivity <= 100)
        {
            pstTmpMotionAlarmConfig->sensitivity = isensitivity;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "sensitivity range: 0-100");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "alarmthreshold", alarmthreshold, sizeof(alarmthreshold)))
    {
        int ialarmthreshold = atoi(alarmthreshold);
        if (0 <= ialarmthreshold && ialarmthreshold <= 100)
        {
            pstTmpMotionAlarmConfig->alarmThreshold = ialarmthreshold;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "alarmthreshold range: 0-100");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "enable_nighttime", enable_nighttime, 2))
    {
        if (enable_nighttime[0] == '0' && enable_nighttime[1] == 0)
        {
            pstTmpMotionAlarmConfig->dayNightSwitch = 0;
        }
        else if (enable_nighttime[0] == '1' && enable_nighttime[1] == 0)
        {
            pstTmpMotionAlarmConfig->dayNightSwitch = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable_nighttime: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "night_sensitivity", night_sensitivity, sizeof(night_sensitivity)))
    {
        int inight_sensitivity = atoi(night_sensitivity);
        if (0 <= inight_sensitivity && inight_sensitivity <= 100)
        {
            pstTmpMotionAlarmConfig->nightSensitivity = inight_sensitivity;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "night_sensitivity range: 0-100");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "night_alarmthreshold", night_alarmthreshold, sizeof(night_alarmthreshold)))
    {
        int inight_alarmthreshold = atoi(night_alarmthreshold);
        if (0 <= inight_alarmthreshold && inight_alarmthreshold <= 100)
        {
            pstTmpMotionAlarmConfig->nightAlarmThreshold = inight_alarmthreshold;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "night_alarmthreshold range: 0-100");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "blockcount", blockcount, sizeof(blockcount)))
    {
        int comparenum = -1;
        int ret = verify_motion_blockcount_format(blockcount);
        if (ret == 1)
        {
            comparenum = sscanf(blockcount, "%dx%d", &rows, &cols);
            if (comparenum != 2)
            {
                sscanf(blockcount, "%dX%d", &rows, &cols);
            }

            pstTmpMotionAlarmConfig->blockCount = (rows << 16) | (cols & 0xffff);
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "blockcount [rows]x[cols]");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "areavalue", areavalue, AREABYTELEN * 4))
    {
        unsigned int bytes;
        char strbyte[8];

        index = 0;
        nbyte = (cols * rows) / 8;
        if ((cols * rows) % 8 != 0)
        {
            nbyte += 1;
        }

        memset(areabyte, 0, sizeof(areabyte));
        int len = strlen(areavalue) / 2;
        for (i = 0; i < nbyte; i++)
        {
            if (len < i)
            {
                break;
            }
            
            memcpy(strbyte, areavalue + i * offset, offset);
            strbyte[3] = 0;
            sscanf(strbyte, "%x", &bytes);

            areabyte[i] = (char)bytes;
        }
        
        for (i = 0; i < nbyte; i++)
        {
            for (j = 0; j < 8; j++)
            {
                index = i * 8 + j;
                if (index >= (rows * cols))
                {
                    break;
                }
                
                pstTmpMotionAlarmConfig->blockCfg[index] = ((areabyte[i] >> j) & 0x01) != 0 ? '1' : '0';
            }
        }

        pstTmpMotionAlarmConfig->blockCfg[rows * cols] = 0;

        memset(areabyte, 0, sizeof(areabyte));
        memset(areavalue, 0, sizeof(areavalue));
        get_area_value(areabyte, areavalue, pstTmpMotionAlarmConfig->blockCfg, rows, cols, pstTmpMotionAlarmConfig->enable, offset);

        bsetFlag = 1;
    }

    if (bsetFlag == 1)
    {
        MotionDetectAlarm stMotionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stMotionAlarmArray[iCameraIdx], pstTmpMotionAlarmConfig, sizeof(MotionDetectAlarm));
        }

        anj_config_alarm_motion_set(stMotionAlarmArray);
    }

    sprintf(response, XML_HEAD "<motion enable=\"%s\" sensitivity=\"%s\" alarmthreshold=\"%s\" enable_nighttime=\"%s\" "
            "night_sensitivity=\"%s\" night_alarmthreshold=\"%s\" blockcount=\"%dx%d\" areavalue=\"%s\"/>",
            enable, sensitivity, alarmthreshold, enable_nighttime, night_sensitivity, night_alarmthreshold, 
            rows, cols, areavalue);

    status = 200;

__exit:
    return status;
}

//-----------获取抓图base64数据-----//
int cgi_getMotion(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }
    
    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    char imgRes[8] = {0};

    if (cgi_get_value(http_url, "imgRes", imgRes, sizeof(imgRes)))
    {
        int imgRes_flag = atoi(imgRes);
        if (imgRes_flag == 1) // 需要抓图 g_getMotion_flag
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);

            int stream = 1;
            int quality = 30;
            int iCameraIdex = 0;
            char filename[128] = {0};
            const char *path = "/tmp";
            
            snprintf(filename, sizeof(filename), "snapshot_stream%d_%x_%x.jpg", stream, (unsigned int)tv.tv_sec, (unsigned int)tv.tv_usec / 1000);
            anj_snap_jpg(iCameraIdex, stream, quality, (char *)path, filename, NULL);

            char pathname[256] = {0};
            snprintf(pathname, sizeof(pathname), "/%s/%s", path, filename);

            if (0 == anj_snap_wait_complete(pathname, 5000))
            {
                char *picbuf = Jpg_to_Base64(pathname);
                if (picbuf != NULL)
                {
                    sprintf(response, XML_HEAD "<data> <Res>true</Res> <people>false</people> <img>%s</img> <errCode>0</errCode> </data>", picbuf);
                }
                else
                {
                    __ERR("jpg to base64 failed for %s", pathname);
                }

                remove(pathname);
            }
            else
            {
                __ERR("snap failed.\n");
                sprintf(response, XML_HEAD "<data> <Res>true</Res> <people>false</people> <img>null</img> <errCode>0</errCode> </data>");
            }
        }
        else
        {
            sprintf(response, XML_HEAD "<data> <Res>true</Res> <people>false</people> <img>null</img> <errCode>0</errCode> </data>");
        }
    }
    else
    {
        sprintf(response, XML_HEAD "<data> <Res>true</Res> <people>false</people> <img>null</img> <errCode>0</errCode> </data>");
    }
    
    status = 200;
    return status;
}


//-----------设置存储录像的配置-----//
int cgi_ScheduleStorage(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }
    
    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    char localEnable[8] = {0};
    char remoteEnable[8] = {0};

    char mountParam[256] = {0};
    char recordFileSize[8] = {0};

    char nfs_server[64] = {0};
    char nfs_server_path[160] = {0};

    // 到底是主码流，还是子码流，还是主子码流同时录像
    char stream[16] = {0};
    char JpegInterval[16] = {0};
    int Enable = 0;
    char Enable_1[16] = {0};

    int bsetRecordConfigFlag = 0;
    int nfs_server_1 = 0;
    int nfs_server_path_1 = 0;

    int iCameraIdx = 0;

    /* getmsg */
    RecordConfig *pstRecordConfig = (RecordConfig *)getRecordConfig();
    RecordConfig *pstTmpRecordConfig = (RecordConfig *)anj_mw_malloc(sizeof(RecordConfig));
    if (pstTmpRecordConfig == NULL)
    {
        __ERR("tmp config malloc failed!\n");
        status = 400;
        goto __exit;
    }

    memcpy(pstTmpRecordConfig, &pstRecordConfig[iCameraIdx], sizeof(RecordConfig));

    /* getconfig */
    // 判断是本地存储还是远程存储
    snprintf(localEnable, sizeof(localEnable), "%d", pstTmpRecordConfig->commonCfg.localEnable != 0 ? 1 : 0);
    snprintf(remoteEnable, sizeof(remoteEnable), "%d", pstTmpRecordConfig->commonCfg.remoteEnable);

    // 如果是NFS的方式，需要NFS服务器的地址和路径
    snprintf(mountParam, sizeof(mountParam), "%s", pstTmpRecordConfig->commonCfg.mountParam);
    snprintf(recordFileSize, sizeof(recordFileSize), "%d", pstTmpRecordConfig->commonCfg.recordFileSize);

    snprintf(stream, sizeof(stream), "%d", pstTmpRecordConfig->scheduleRecordCfg.stream);
    snprintf(JpegInterval, sizeof(JpegInterval), "%d", pstTmpRecordConfig->scheduleRecordCfg.jpgInterval);

    // 如果ScheduleRecord  LocalStore & RemoteStore 都为0 的话，证明Enable开关没有打开
    if ((pstTmpRecordConfig->scheduleRecordCfg.localStore == 0) && (pstTmpRecordConfig->scheduleRecordCfg.remoteStore == 0))
    {
        Enable = 0;
    }
    else
    {
        Enable = 1;
    }
    __ERR("cgi schedule storage enable:%d\n", Enable);

    /* setconfig */
    if (cgi_get_value(http_url, "localEnable", localEnable, 2))
    {
        if (localEnable[0] == '0' && localEnable[1] == 0)
        {
            pstTmpRecordConfig->commonCfg.localEnable = 0;
        }
        else if (localEnable[0] == '1' && localEnable[1] == 0)
        {
            pstTmpRecordConfig->commonCfg.localEnable = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "localEnable: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
    }

    if (cgi_get_value(http_url, "remoteEnable", remoteEnable, 2))
    {
        if (remoteEnable[0] == '0' && remoteEnable[1] == 0)
        {
            pstTmpRecordConfig->commonCfg.remoteEnable = NETWORK_STORAGE_DISABLE;
        }
        else if (remoteEnable[0] == '1' && remoteEnable[1] == 0)
        {
            pstTmpRecordConfig->commonCfg.remoteEnable = NETWORK_STORAGE_TYPE_NFS;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "remoteEnable: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
    }

    if (cgi_get_value(http_url, "recordFileSize", recordFileSize, sizeof(recordFileSize)))
    {
        int i_recordFileSize = safeatoi(recordFileSize);
        
        if ((i_recordFileSize == 2) || (i_recordFileSize == 5) || 
            (i_recordFileSize == 10) || (i_recordFileSize == 20) || (i_recordFileSize == 30))
        {
            pstTmpRecordConfig->commonCfg.recordFileSize = i_recordFileSize;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "recordFileSize: 2,5,10,20,30");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
    }

    if (cgi_get_value(http_url, "remote_nfs_server", nfs_server, sizeof(nfs_server)))
    {
        __ERR("setScheduleStorage nfs_server=%s\n", nfs_server);
    
        if (nfs_server == NULL)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "remote_nfs_server: error\n");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
        nfs_server_1 = 1;
    }

    if (cgi_get_value(http_url, "nfs_server_path", nfs_server_path, sizeof(nfs_server_path)))
    {
        __ERR("setScheduleStorage nfs_server_path=%s\n", nfs_server_path);
    
        if (nfs_server_path == NULL)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "nfs_server_path: error\n");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
        nfs_server_path_1 = 1;
    }

    if (cgi_get_value(http_url, "stream", stream, sizeof(stream)))
    {
        int i_stream = safeatoi(stream);
    
        if (i_stream == 0 || i_stream == 1 || i_stream == 2)
        {
            pstTmpRecordConfig->scheduleRecordCfg.stream = i_stream;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "stream: 0,1,2\n");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
    }

    if (cgi_get_value(http_url, "JpegInterval", JpegInterval, sizeof(JpegInterval)))
    {
        int i_JpegInterval = safeatoi(JpegInterval);
    
        if (i_JpegInterval == 0 || i_JpegInterval == 1)
        {
            pstTmpRecordConfig->scheduleRecordCfg.jpgInterval = i_JpegInterval;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "JpegInterval: 0,1\n");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
    }

    if (cgi_get_value(http_url, "enable", Enable_1, sizeof(Enable_1)))
    {
        int i_enable = safeatoi(Enable_1);
    
        if (i_enable == 0 || i_enable == 1)
        {
            if (i_enable == 0)
            {
                pstTmpRecordConfig->scheduleRecordCfg.localStore = 0;
                pstTmpRecordConfig->scheduleRecordCfg.remoteStore = 0;
            }
            if (i_enable == 1)
            {
                if (pstTmpRecordConfig->commonCfg.localEnable)  // 前面设置的是本地录像
                {
                    pstTmpRecordConfig->scheduleRecordCfg.localStore = 1;
                }
                if (pstTmpRecordConfig->commonCfg.remoteEnable) // 前面设置的是远程录像
                {
                    pstTmpRecordConfig->scheduleRecordCfg.remoteStore = 1;
                }
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0,1\n");
            status = 400;
            goto __exit;
        }
        bsetRecordConfigFlag = 1;
    }

    // 把NFS远程服务器只有两个部分都设置成功之后，才生效，不然还是原来的配置
    if (nfs_server_1 == 1 && nfs_server_path_1 == 1)
    {
        snprintf(mountParam, sizeof(mountParam), "-t nfs -o nolock %s:%s", nfs_server, nfs_server_path);
        memcpy(pstTmpRecordConfig->commonCfg.mountParam, mountParam, sizeof(mountParam));
    }

    if (bsetRecordConfigFlag == 1)
    {
        RecordConfig stRecordConfigArray[ANJ_CAMERA_MAX_NUMS] = {0};
        memcpy(stRecordConfigArray, pstTmpRecordConfig, sizeof(RecordConfig) * ANJ_CAMERA_MAX_NUMS);

        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stRecordConfigArray[iCameraIdx], pstTmpRecordConfig, sizeof(RecordConfig));
        }
        anj_config_record_set(stRecordConfigArray);
    }

    sprintf(response, XML_HEAD "<RecordConfig><Common LocalEnable=\"%s\" RemoteEnable=\"%s\" MountParam=\"%s\" RecordFileSize=\"%s\"/>"
            "<ScheduleRecord Stream=\"%s\" LocalStore=\"%d\" RemoteStore=\"%d\" JpegInterval=\"%s\"/></RecordConfig>",
            localEnable, remoteEnable, mountParam, recordFileSize, stream, 
            pstTmpRecordConfig->scheduleRecordCfg.localStore,
            pstTmpRecordConfig->scheduleRecordCfg.remoteStore, JpegInterval);

    status = 200;

__exit:
    return status;
}


//-----------设置移动侦测的存储-----//
int cgi_EventStorage(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }

    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    char trigger_recording[8] = {0};
    char stream[8] = {0};
    char record_ftp_upload[8] = {0};
    char record_email_upload[8] = {0};
    char capture_ftp_upload[8] = {0};
    char capture_email_upload[8] = {0};
    char capture_motionDetectAlarm[8] = {0};

    // 到底是主码流，还是子码流，还是主子码流同时录像
    char trigger_capture[8] = {0};
    int bsetFlag = 0;
    int bsetAlarmFlag = 0;

    int iCameraIdx = 0;
    RecordConfig *pstRecordConfig = (RecordConfig *)getRecordConfig();
    RecordConfig *pstTmpRecordConfig = (RecordConfig *)anj_mw_malloc(sizeof(RecordConfig));

    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pstTmpMotionAlarmConfig = (MotionDetectAlarm *) anj_mw_malloc (sizeof(MotionDetectAlarm));

    if (pstTmpRecordConfig == NULL || pstTmpMotionAlarmConfig == NULL)
    {
        __ERR("tmp config malloc failed!\n");
        status = 400;
        goto __exit;
    }

    memcpy(pstTmpRecordConfig, &pstRecordConfig[iCameraIdx], sizeof(RecordConfig));
    memcpy(pstTmpMotionAlarmConfig, &pstAlarmConfig->normalAlarm.motionDetectAlarm[iCameraIdx], sizeof(MotionDetectAlarm));

    /* getconfig */
    // 判断是本地存储还是远程存储
    snprintf(trigger_recording, sizeof(trigger_recording) - 1, "%d", 
             (pstTmpRecordConfig->motionRecordCfg.remoteStore != 0 || pstTmpRecordConfig->motionRecordCfg.localStore != 0) ? 1 : 0);
    snprintf(trigger_capture, sizeof(trigger_capture) - 1, "%d", 
             (pstTmpRecordConfig->motionCaptureCfg.remoteStore != 0 || pstTmpRecordConfig->motionCaptureCfg.localStore != 0) != 0 ? 1 : 0);
    
    snprintf(stream, sizeof(stream) - 1, "%d", pstTmpRecordConfig->motionRecordCfg.stream != 0 ? 1 : 0);

    snprintf(record_ftp_upload, sizeof(record_ftp_upload) - 1, "%d", pstTmpRecordConfig->motionRecordCfg.ftpUpload != 0 ? 1 : 0);
    snprintf(record_email_upload, sizeof(record_email_upload) - 1, "%d", pstTmpRecordConfig->motionRecordCfg.emailUpload != 0 ? 1 : 0);

    snprintf(capture_ftp_upload, sizeof(capture_ftp_upload) - 1, "%d", pstTmpRecordConfig->motionCaptureCfg.ftpUpload != 0 ? 1 : 0);
    snprintf(capture_email_upload, sizeof(capture_email_upload) - 1, "%d", pstTmpRecordConfig->motionCaptureCfg.emailUpload != 0 ? 1 : 0);

    snprintf(capture_motionDetectAlarm, sizeof(capture_motionDetectAlarm) - 1, "%d", 
             pstTmpMotionAlarmConfig->alarmAction.outputAction.outputChnlActions[0].enable != 0 ? 1 : 0);


    if (cgi_get_value(http_url, "trigger_recording", trigger_recording, 2))
    {
        if (trigger_recording[0] == '0' && trigger_recording[1] == 0)
        {
            // 只要移动侦测的启动录像不启用的时候，就全部设置为0
            pstTmpRecordConfig->motionRecordCfg.localStore = 0;
            pstTmpRecordConfig->motionRecordCfg.remoteStore = 0;
        }
        else if (trigger_recording[0] == '1' && trigger_recording[1] == 0)
        {
            if (pstTmpRecordConfig->commonCfg.localEnable == 1)
            {
                pstTmpRecordConfig->motionRecordCfg.localStore = 1;
            }
            if (pstTmpRecordConfig->commonCfg.localEnable == 1)
            {
                pstTmpRecordConfig->motionRecordCfg.remoteStore = 1;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "trigger_recording: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "stream", stream, sizeof(stream)))
    {
        int i_stream = safeatoi(stream);
        // 主码流为1，子码流为2
        if (i_stream == 1 || i_stream == 2)
        {
            pstTmpRecordConfig->motionRecordCfg.stream = i_stream;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "stream: 1,2\n");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }
    
    // 子码流才能进行配置上传到FTP还是Email
    if (pstTmpRecordConfig->motionRecordCfg.stream == 2)
    {
        if (cgi_get_value(http_url, "record_ftp_upload", record_ftp_upload, sizeof(record_ftp_upload)))
        {
            int i_record_ftp_upload = safeatoi(record_ftp_upload);
            // 主码流为1，子码流为2
            if (i_record_ftp_upload == 0 || i_record_ftp_upload == 1)
            {
                pstTmpRecordConfig->motionRecordCfg.ftpUpload = i_record_ftp_upload;
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "record_ftp_upload: 0,1\n");
                status = 400;
                goto __exit;
            }
            bsetFlag = 1;
        }

        if (cgi_get_value(http_url, "record_email_upload", record_email_upload, sizeof(record_email_upload)))
        {
            int i_record_email_upload = safeatoi(record_email_upload);
            // 主码流为1，子码流为2
            if (i_record_email_upload == 0 || i_record_email_upload == 1)
            {
                pstTmpRecordConfig->motionRecordCfg.emailUpload = i_record_email_upload;
            }
            else
            {
                sprintf(response, XML_CGI_FAULT, url_name, "record_email_upload: 0,1\n");
                status = 400;
                goto __exit;
            }
            bsetFlag = 1;
        }
    }

    if (cgi_get_value(http_url, "trigger_capture", trigger_capture, 2))
    {
        if (trigger_capture[0] == '0' && trigger_capture[1] == 0)
        {
            // 只要移动侦测的启动录像不启用的时候，就全部设置为0
            pstTmpRecordConfig->motionCaptureCfg.localStore = 0;
            pstTmpRecordConfig->motionCaptureCfg.remoteStore = 0;
        }
        else if (trigger_capture[0] == '1' && trigger_capture[1] == 0)
        {
            if (pstTmpRecordConfig->commonCfg.localEnable == 1)
            {
                pstTmpRecordConfig->motionCaptureCfg.localStore = 1;
            }
            if (pstTmpRecordConfig->commonCfg.localEnable == 1)
            {
                pstTmpRecordConfig->motionCaptureCfg.remoteStore = 1;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "trigger_capture: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "capture_ftp_upload", capture_ftp_upload, sizeof(capture_ftp_upload)))
    {
        int i_capture_ftp_upload = safeatoi(capture_ftp_upload);
        // 主码流为1，子码流为2
        if (i_capture_ftp_upload == 0 || i_capture_ftp_upload == 1)
        {
            pstTmpRecordConfig->motionCaptureCfg.ftpUpload = i_capture_ftp_upload;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "capture_ftp_upload: 0,1\n");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "capture_email_upload", capture_email_upload, sizeof(capture_email_upload)))
    {
        int i_capture_email_upload = safeatoi(capture_email_upload);
        // 主码流为1，子码流为2
        if (i_capture_email_upload == 0 || i_capture_email_upload == 1)
        {
            pstTmpRecordConfig->motionCaptureCfg.emailUpload = i_capture_email_upload;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "capture_email_upload: 0,1\n");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "capture_motionDetectAlarm", capture_motionDetectAlarm, sizeof(capture_motionDetectAlarm)))
    {
        int i_capture_motionDetectAlarm = safeatoi(capture_motionDetectAlarm);
        // 主码流为1，子码流为2
        if (i_capture_motionDetectAlarm == 0 || i_capture_motionDetectAlarm == 1)
        {
            if (i_capture_motionDetectAlarm != pstTmpMotionAlarmConfig->alarmAction.outputAction.outputChnlActions[0].enable)
            {
                bsetAlarmFlag = 1;
                pstTmpMotionAlarmConfig->alarmAction.outputAction.outputChnlActions[0].enable = i_capture_motionDetectAlarm;
            }
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "capture_motionDetectAlarm: 0,1\n");
            status = 400;
            goto __exit;
        }
    }

    if (bsetFlag == 1)
    {
        RecordConfig stRecordConfigArray[ANJ_CAMERA_MAX_NUMS] = {0};
        memcpy(stRecordConfigArray, pstTmpRecordConfig, sizeof(RecordConfig) * ANJ_CAMERA_MAX_NUMS);
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stRecordConfigArray[iCameraIdx], pstTmpRecordConfig, sizeof(RecordConfig));
        }
        anj_config_record_set(stRecordConfigArray);
    }

    if (bsetAlarmFlag == 1)
    {
        MotionDetectAlarm stMotionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stMotionAlarmArray[iCameraIdx], pstTmpMotionAlarmConfig, sizeof(MotionDetectAlarm));
        }

        anj_config_alarm_motion_set(stMotionAlarmArray);
    }

    sprintf(response, XML_HEAD "<RecordConfig><MotionDetectRecord Stream=\"%s\" LocalStore=\"%d\" RemoteStore=\"%d\" FtpUpload=\"%s\" EmailUpload=\"%s\"/>"
            "<MotionDetectCapture LocalStore=\"%d\" RemoteStore=\"%d\" FtpUpload=\"%s\" EmailUpload=\"%s\" OutputChannelAction_enable=\"%s\"/></RecordConfig>",
            stream, 
            pstTmpRecordConfig->motionRecordCfg.localStore, pstTmpRecordConfig->motionRecordCfg.remoteStore, 
            record_ftp_upload, record_email_upload,
            pstTmpRecordConfig->motionCaptureCfg.localStore, pstTmpRecordConfig->motionCaptureCfg.remoteStore, 
            capture_ftp_upload, capture_email_upload,
            capture_motionDetectAlarm);

    status = 200;

__exit:
    return status;
}



//-----------获取RTMP的配置-------------//
int cgi_rtmp(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if (status != 200)
    {
        return status;
    }
    
    if (!cgi_is_valid_uid_or_username(http_url, response, url_name))
    {
        return 400;
    }

    char enable[8] = {0};
    char server[MAX_IP_NAME_LEN] = {0};
    char port[16] = {0};
    char streamno[8] = {0};   // 0: main 1: sub 2: third
    char appname[MAX_RTMP_APP_NAME_LEN] = {0};
    char streamid[MAX_RTMP_STREAMID_LEN] = {0};
    char type[8] = {0};

    // ----判断是否为空的临时数组------//
    char server_set[MAX_IP_NAME_LEN] = {0};
    char appname_set[MAX_RTMP_APP_NAME_LEN] = {0};
    char streamid_set[MAX_RTMP_STREAMID_LEN] = {0};
    // ---------------------------------------------//

    int bsetFlag = 0;
    int tmp = 0;

    /* get  rtmp  msg */
    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

    /* getconfig */
    snprintf(enable, sizeof(enable) - 1, "%d", pstMediaStreamCfg->rtmpConfig.enable != 0 ? 1 : 0);
    snprintf(port, sizeof(port) - 1, "%d", pstMediaStreamCfg->rtmpConfig.port);
    snprintf(streamno, sizeof(streamno) - 1, "%d", pstMediaStreamCfg->rtmpConfig.streamno);
    snprintf(type, sizeof(type) - 1, "%d", pstMediaStreamCfg->rtmpConfig.type);
    memcpy(server, pstMediaStreamCfg->rtmpConfig.server, sizeof(server));
    memcpy(appname, pstMediaStreamCfg->rtmpConfig.appname, sizeof(appname));
    memcpy(streamid, pstMediaStreamCfg->rtmpConfig.streamid, sizeof(streamid));

    // 因为传递进来的都是字符串，所以上面的方式是不可行的
    // __ERR("cgi_rtmp enable=%s,server = %s,port = %s,streamno = %s,appname = %s,streamid = %s,type = %s\n",enable,server,port,
    //     streamno,appname,streamid,type);

    /* setconfig */
    if (cgi_get_value(http_url, "enable", enable, 2))
    {
        if (enable[0] == '0' && enable[1] == 0)
        {
            pstMediaStreamCfg->rtmpConfig.enable = 0;
        }
        else if (enable[0] == '1' && enable[1] == 0)
        {
            pstMediaStreamCfg->rtmpConfig.enable = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "enable: 0 or 1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    tmp = cgi_get_value_2(http_url, "server", server_set, MAX_IP_NAME_LEN - 1);
    if (tmp > 0) // 接收到参数的情况
    {
        memcpy(pstMediaStreamCfg->rtmpConfig.server, server_set, sizeof(server_set));
        bsetFlag = 1;
    }
    else if (tmp == -1)
    {
        // 参数不存在，不做处理
    }
    else
    {
        if (strcmp(server_set, "") == 0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "server is NULL");
            status = 400;
            goto __exit;
        }
    }
    
    tmp = cgi_get_value_2(http_url, "appname", appname_set, MAX_RTMP_APP_NAME_LEN - 1);
    if (tmp > 0) // 接收到参数的情况
    {
        memcpy(pstMediaStreamCfg->rtmpConfig.appname, appname_set, sizeof(appname_set));
        bsetFlag = 1;
    }
    else if (tmp == -1)
    {
        // 参数不存在，不做处理
    }
    else
    {
        __ERR("appname = %s\n", appname_set);
        if (strcmp(appname_set, "") == 0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "appname is NULL");
            status = 400;
            goto __exit;
        }
    }
    
    tmp = cgi_get_value_all(http_url, "streamid", streamid_set, MAX_RTMP_STREAMID_LEN - 1);
    if (tmp > 0) // 接收到参数的情况
    {
        memcpy(pstMediaStreamCfg->rtmpConfig.streamid, streamid_set, sizeof(streamid_set));
        bsetFlag = 1;
    }
    else if (tmp == -1)
    {
        // 参数不存在，不做处理
    }
    else
    {
        __ERR("streamid = %s\n", streamid_set);
        if (strcmp(streamid_set, "") == 0)
        {
            sprintf(response, XML_CGI_FAULT, url_name, "streamid is NULL");
            status = 400;
            goto __exit;
        }
    }

    if (cgi_get_value(http_url, "port", port, sizeof(port)))
    {
        int port1 = atoi(port);
        if (0 <= port1 && port1 <= 65535)
        {
            pstMediaStreamCfg->rtmpConfig.port = port1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "port range: 0-65535");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "streamno", streamno, 2))
    {
        if (streamno[0] == '0' && streamno[1] == 0)
        {
            pstMediaStreamCfg->rtmpConfig.streamno = 0;
        }
        else if (streamno[0] == '1' && streamno[1] == 0)
        {
            pstMediaStreamCfg->rtmpConfig.streamno = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "streamno range: 0-1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }

    if (cgi_get_value(http_url, "type", type, 2))
    {
        if (type[0] == '0' && type[1] == 0)
        {
            pstMediaStreamCfg->rtmpConfig.type = 0;
        }
        else if (type[0] == '1' && type[1] == 0)
        {
            pstMediaStreamCfg->rtmpConfig.type = 1;
        }
        else
        {
            sprintf(response, XML_CGI_FAULT, url_name, "type range: 0-1");
            status = 400;
            goto __exit;
        }
        bsetFlag = 1;
    }
    
    /* setmsg */
    // 如果检测到有参数的话，进入此函数
    if (bsetFlag == 1)
    {
        anj_config_stream_set(pstMediaStreamCfg);
    }

    /* setxml */
    sprintf(response, XML_HEAD "<RtmpConfig Enable=\"%s\" Streamno=\"%s\" Server=\"%s\" Port=\"%s\" "
            "Appname=\"%s\" Streamid=\"%s\" Type=\"%s\"/>",
            enable, streamno, pstMediaStreamCfg->rtmpConfig.server, port, 
            pstMediaStreamCfg->rtmpConfig.appname, pstMediaStreamCfg->rtmpConfig.streamid, type);

    status = 200;                        
__exit:
    return status;
}


//--------------------------------------------//

int cgi_connect_check(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }
    
    //if(!cgi_is_valid_uid_or_username(http_url,response,url_name))    return 400;

    int value = 0;
    char addr_url[64] = {0};
    if(cgi_get_value(http_url, "addr", addr_url, sizeof(addr_url)) == 0)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, invalid addr");
        status = 400;
        goto __exit;
    }
    __ERR("connect_check addr_url=%s\n", addr_url);


    value = icmp_ping_url(addr_url, 1000);
    
    __ERR("connect_check status=%d\n", value);
    sprintf(response, XML_HEAD"<connect_check status=\"%d\" />", value);
    status = 200;

__exit:    
    return status;    
}

int cgi_open_slogtcp(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    int status = cgi_compare_url(http_url, response, url_name);
    if(status != 200)
    {
        return status;
    }

    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    int value = 0;
    char buffer[64];
    if(cgi_get_value(http_url, "enable", buffer, sizeof(buffer)) == 0)
    {
        sprintf(response, XML_CGI_FAULT, url_name, "error, invalid param");
        status = 400;
        goto __exit;
    }
    else
    {
        sprintf(response, XML_HEAD"<ok />");
        status = 200;
        value = atoi(buffer);
        if(value)
        {
            anj_mw_system("touch /tmp/flag.open.slog.tcp");
        }
        else
        {
            anj_mw_system("rm -f /tmp/flag.open.slog.tcp");    
        }
    }
__exit:    
    return status;    
}

int cgi_reboot_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    __DBG("Got reboot cmd from %s\n", clientip);
    sprintf(response, XML_HEAD"<reboot>true</reboot>");

    __WARN("cgi reboot service request from:%s\n", clientip);
    __RECORD_LOG_INFO("cgi reboot service request from:%s\n", clientip);
    anj_sysmng_delay_reboot(1);
    return 200;    
}

int cgi_factory_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    __DBG("Got factory cmd from %s\n", clientip);
    sprintf(response, XML_HEAD"<factory>true</factory>");

    unsigned int reserved_bits = 0;        
    __RECORD_LOG_INFO("restore config reserved_bits=%u\n", reserved_bits); 
    anj_sysmng_config_restore(reserved_bits);
    
    return 200;    
}

int cgi_getptzport_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    int port = pstMediaStreamCfg->commConfig.ptzPort;
    sprintf(response, "%d", port);

    return 200;    
}

int cgi_getmacaddr_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
    sprintf(response, "%s", pstNetworkCfg->lanCfg.MACAddress);

    return 200;    
}

int cgi_kernelVersion_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url,response,url_name) == 0)
    {
        return 400;
    }

    SYSTEM_VERSION_DATA stSystemVersion = {0};
    anj_sysmng_version_info_get(&stSystemVersion, 0);

    strcat(response, "{\n");
    strcat(response, "\"directory\":\"");
    strcat(response, "system/version_info/");
    strcat(response, "\",\n");
    strcat(response, "\"name\":\"");
    strcat(response, "kernelVersion");
    strcat(response, "\",\n");
    strcat(response, "\"type\":\"");
    strcat(response, "string");
    strcat(response, "\",\n");
    strcat(response, "\"value\":\"");
    strcat(response, stSystemVersion.kernelVersion);
    strcat(response, "\"\n");
    strcat(response, "}\n");
    __INFO("kernelVersion----->>\n%s<<-----\n",response);

    return 200;
}

int cgi_fsVersion_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url,response,url_name) == 0)
    {
        return 400;
    }

    SYSTEM_VERSION_DATA stSystemVersion = {0};
    anj_sysmng_version_info_get(&stSystemVersion, 0);

    strcat(response, "{\n");
    strcat(response, "\"directory\":\"");
    strcat(response, "system/version_info/");
    strcat(response, "\",\n");
    strcat(response, "\"name\":\"");
    strcat(response, "fsVersion");
    strcat(response, "\",\n");
    strcat(response, "\"type\":\"");
    strcat(response, "string");
    strcat(response, "\",\n");
    strcat(response, "\"value\":\"");
    strcat(response, stSystemVersion.fsVersion);
    strcat(response, "\"\n");
    strcat(response, "}\n");
    __INFO("fsVersion----->>\n%s<<-----\n", response);

    return 200;
}

int cgi_serialNumber_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char mySN[32] = {0};
    anj_sysmng_load_sn(mySN, sizeof(mySN));
    
    strcat(response, "{\n");
    strcat(response, "\"directory\":\"");
    strcat(response, "system/version_info/");
    strcat(response, "\",\n");
    strcat(response, "\"name\":\"");
    strcat(response, "serialNumber");
    strcat(response, "\",\n");
    strcat(response, "\"type\":\"");
    strcat(response, "string");
    strcat(response, "\",\n");
    strcat(response, "\"value\":\"");
    strcat(response, mySN);
    strcat(response, "\"\n");
    strcat(response, "}\n");
    __INFO("serialNumber----->>\n%s<<-----\n", response);

    return 200;
}

int cgi_device_info_request(const char *url_name, const char *clientip, const char *http_url, char *response)
{
    if(cgi_is_valid_uid_or_username(http_url, response, url_name) == 0)
    {
        return 400;
    }

    char szDeviceType[20] = {0};
    GetDeviceTypeStr(szDeviceType);
    
    strcat(response, "{\n");
    strcat(response, "\"directory\":\"");
    strcat(response, "system/device_info/");
    strcat(response, "\",\n");
    strcat(response, "\"name\":\"");
    strcat(response, "device_type");
    strcat(response, "\",\n");
    strcat(response, "\"type\":\"");
    strcat(response, "string");
    strcat(response, "\",\n");
    strcat(response, "\"value\":\"");
    strcat(response, szDeviceType);
    strcat(response, "\"\n");
    strcat(response, "}\n");
    __INFO("deviceType----->>\n%s<<-----\n", response);

    return 200;
}

typedef int (*callback_cgi_handle)(const char *clientip, const char* szUrlPrefix, const char *http_url, char *response);

typedef struct
{
    const char* szUrlPrefix;
    callback_cgi_handle cb;
}CgiStaticTableStruct;

CgiStaticTableStruct g_CgiTable[] = 
{
    { "/cgi-bin/console.cgi", cgi_open_slogtcp },
    { "/cgi-bin/rebootipc", cgi_reboot_request },
    { "/cgi-bin/factoryipc", cgi_factory_request },
    { "/cgi-bin/getptzport", cgi_getptzport_request },
    { "/cgi-bin/getmacaddr_eth0.cgi", cgi_getmacaddr_request },
    { "/cgi-bin/settings/system/version_info/kernelVersion", cgi_kernelVersion_request },
    { "/cgi-bin/settings/system/version_info/fsVersion", cgi_fsVersion_request },
    { "/cgi-bin/settings/system/version_info/serialNumber", cgi_serialNumber_request },
    { "/cgi-bin/settings/system/device_info/device_type", cgi_device_info_request },
    
    { "/cgi-bin/getuid", cgi_getuid },
    { "/cgi-bin/keep_alive", cgi_keep_alive },
    { "/cgi-bin/user_ip", cgi_user_ip },
    { "/cgi-bin/flash_eraseall", cgi_flash_eraseall },
    { "/cgi-bin/getmodel", cgi_get_oem },
    { "/cgi-bin/umount_nfs", cgi_nfs_remote_umount },
    { "/cgi-bin/get_record_query", cgi_get_record_query },
    { "/cgi-bin/user_management", cgi_User_Management },
    { "/cgi-bin/getinfo", cgi_getinfo },
    { "/cgi-bin/set_wdr", cgi_set_wdr },
    { "/cgi-bin/set_flip", cgi_set_flip },
    { "/cgi-bin/setManualInputAlarm", cgi_set_manual_inputAlarm },
    { "/cgi-bin/setUidValidTime", cgi_set_uid_valid_time },
    { "/cgi-bin/onvif", cgi_onvif },
    { "/cgi-bin/rtsp", cgi_rtsp },
    { "/cgi-bin/getCurrentUserAccount", cgi_getCurrentUserAccount },
    { "/cgi-bin/setSendAlarmMsgEnable", cgi_set_send_alarm_to_loopback },
    { "/cgi-bin/getRelayStatus", cgi_get_relay_status },
    { "/cgi-bin/setRelayStatus", cgi_set_relay_status },
    { "/cgi-bin/setShutterSpeed", cgi_set_ShutterSpeed },
    { "/cgi-bin/setDeviceName", cgi_set_device_name },
    { "/cgi-bin/set_ircut", cgi_set_ircutmode },
    { "/cgi-bin/set_noise_reduction", cgi_noise_reduction },
    { "/cgi-bin/getDevState", cgi_getDevState },
    { "/cgi-bin/setAlarmState", cgi_setAlarmState },
    { "/cgi-bin/set_privacy_zone", cgi_setPrivacyZone },
    { "/cgi-bin/set_service_port", cgi_service_port },
    { "/cgi-bin/set_Infrared_led", cgi_Infrared_lamp_brightness },
    { "/cgi-bin/set_led", cgi_led_brightness },
    { "/cgi-bin/set_start_codec", cgi_Codec_stream },
    { "/cgi-bin/module_resolution", cgi_module_resolution },
    { "/cgi-bin/getcapabilities", cgi_getcapabilities },
    { "/cgi-bin/reboot", cgi_reboot },
    { "/cgi-bin/factory_reset", cgi_factory_reset },
    { "/cgi-bin/network", cgi_network },
    { "/cgi-bin/time", cgi_time },
    { "/cgi-bin/videoencoder/getcapabilities", cgi_videoencoder_capabilities },
    { "/cgi-bin/videoencoder/stream1", cgi_videoencoder_stream1 },
    { "/cgi-bin/videoencoder/stream2", cgi_videoencoder_stream2 },
    { "/cgi-bin/videoencoder/stream3", cgi_videoencoder_stream3 },
    { "/cgi-bin/videoencoder/AdvanceEncodeConfig", cgi_videoencoder_adv },
    { "/cgi-bin/audioencoder/getcapabilities", cgi_audioencoder_capabilities },
    { "/cgi-bin/audioencoder", cgi_audioencoder },
    { "/cgi-bin/osd", cgi_osd },
    { "/cgi-bin/userosd", cgi_userosd },
    { "/cgi-bin/setUserOsdOnly", cgi_setUserOsdToEncodeOnly },
    { "/cgi-bin/image", cgi_image },
    { "/cgi-bin/ptz_ctrl", cgi_ptz_ctrl },
    { "/cgi-bin/focus_slight_ctrl", cgi_focus_slight_ctrl },
    { "/cgi-bin/getAfStatus", cgi_getAfStatus },
    { "/cgi-bin/motion", cgi_motion },
    { "/cgi-bin/getMotion", cgi_getMotion },
    { "/cgi-bin/setScheduleStorage", cgi_ScheduleStorage },
    { "/cgi-bin/setEventStorage", cgi_EventStorage },
    { "/cgi-bin/rtmp", cgi_rtmp },
    { "/cgi-bin/connect_check", cgi_connect_check },

    
    { "/cgi-bin", cgi_no_interface },    
};


int http_cgi_find(const char *http_url)
{
    int bFind = 0;
    unsigned int iIndex = 0;
    for(iIndex = 0; iIndex < sizeof(g_CgiTable)/sizeof(g_CgiTable[0]); iIndex++)
    {
        CgiStaticTableStruct *p = &g_CgiTable[iIndex];
        if(strlen(p->szUrlPrefix) == 0 )
        {
            continue;
        }

        if(strncmp(p->szUrlPrefix, http_url, strlen(p->szUrlPrefix))   == 0)
        {
            bFind = 1;
        }
    }

    return bFind;
}

int http_cgi_handle(const char *http_url,char *response, const char *ipstr)
{
    int status = 0;
    unsigned int iIndex = 0;
    for(iIndex = 0; iIndex < sizeof(g_CgiTable)/sizeof(g_CgiTable[0]); iIndex++)
    {
        CgiStaticTableStruct *p = &g_CgiTable[iIndex];
        if(strlen(p->szUrlPrefix) == 0)
        {
            continue;
        }

        if(strncmp(p->szUrlPrefix, http_url, strlen(p->szUrlPrefix))  == 0)
        {
            __ERR("found url prefix:%s\n",p->szUrlPrefix);
            if(p->cb != NULL)
            {
                status = p->cb(p->szUrlPrefix, ipstr, http_url, response);
                break;
            }
        }
    }

    return status;
}


