
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "anj_config.h"
#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_hwctrl.h"

#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_audio.h"
#include "anj_sdcard.h"
#include "anj_record.h"
#include "anj_net_provider.h"
#include "user_auth.h"
#include "anj_net.h"
#include "function_list.h"

#include "http_def.h"
#include "http_unv.h"
#include "unv_def.h"
#include "unv_get.h"
#include "hapi_handle.h"
#include "cgi_handle.h"
#include "webpost_handle.h"
#include "http_handle.h"
#include "anj_ftpemail.h"

#define HTTP_RESPONSE_BUF_SIZE      (100 * 1024)
#define HTTP_GET_RESULT_BUF_SIZE    (1000)


static cb_func_http_response g_ResponseFunc = NULL;
static cb_func_http_sendfile g_GetFileFunc = NULL;

static http_alarm_ability_t s_http_alarm_ability = {0};

static WIFI_AP_SCAN s_http_ap_list = {0};       // wifi ap列表

static char s_http_login_path[192] = {0};       // login 路径

static AudioFileList g_stAudioFileList = {0};   // 音频文件列表

static int s_http_query_record_flag = 0;        // http在查询record的标志

const char *pOemMp3Path = DATA_BLOCK_MOUNT_PATH;
const char *pOemAppPath = "/tmp/oem";
const char *pOemLogoPath = DATA_BLOCK_MOUNT_PATH;

const char *get_oem_mp3_path()
{
    return pOemMp3Path;
}

const char *get_oem_app_path()
{
    return pOemAppPath;
}

const char *get_oem_logo_path()
{
    return pOemLogoPath;
}


http_alarm_ability_t *http_alarm_ability_get()
{
    return &s_http_alarm_ability;
}

void http_query_record_set(int flag)
{
    s_http_query_record_flag = flag;    
}

int http_query_record_get()
{
    return s_http_query_record_flag; 
}

char* http_login_path_get(void)
{
    return s_http_login_path;
}

// 获取音频播放列表
AudioFileList *http_audio_file_list_get()
{
    return &g_stAudioFileList;
}

TestWebSiteStruct *get_test_website_info()
{
    static int first_get_flag = 1;
    static TestWebSiteStruct s_WebSiteInfo = {0};

    if (first_get_flag == 1)
    {
        first_get_flag = 0;
        anj_config_web_test_get(&s_WebSiteInfo);
    }

    return &s_WebSiteInfo;
}


int http_response_cb(void *pInst, const char *pResponse, int status)
{
    int iRet = -1;
    if (g_ResponseFunc != NULL)
    {
        iRet = g_ResponseFunc(pInst, pResponse, status);
    }

    return iRet;
}

int http_send_file_cb(void *pInst, void *pmsgt, const char *szFileName, const char *type)
{
    int iRet = -1;
    if( g_GetFileFunc != NULL)
    {
        iRet = g_GetFileFunc(pInst, pmsgt, szFileName, type);
    }
    return iRet;
}

void http_get_ip_addr_port(char *ip_addr, int ip_len, int *port)
{
    NETWORK_STATUS_DATA stNetworkStatus = {0};
    anj_net_info_get(&stNetworkStatus);
    StrCpy(ip_addr, ip_len, stNetworkStatus.ip);

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaConfig();
    *port = pstMediaStreamCfg->webConfig.webPort;    
}

static int http_file_tag_cmp(const char *s, const char *t)
{
    for (;;)
    {
        int c1 = *s;
        int c2 = *t;

        if (!c1 || c1 == '"')
            break;

        if (c2 != '-')
        {
            if (c1 != c2)
            {
                if (c1 >= 'A' && c1 <= 'Z')
                    c1 += 'a' - 'A';
                if (c2 >= 'A' && c2 <= 'Z')
                    c2 += 'a' - 'A';
            }

            if (c1 != c2)
            {
                if (c2 != '*')
                    return 1;

                c2 = *++t;
                if (!c2)
                    return 0;

                if (c2 >= 'A' && c2 <= 'Z')
                    c2 += 'a' - 'A';

                for (;;)
                {
                    c1 = *s;

                    if (!c1 || c1 == '"')
                        break;

                    if (c1 >= 'A' && c1 <= 'Z')
                        c1 += 'a' - 'A';

                    if (c1 == c2 && !http_file_tag_cmp(s + 1, t + 1))
                        return 0;

                    s++;
                }
                break;
            }
        }

        s++;
        t++;
    }

    if (*t == '*' && !t[1])
        return 0;

    return *t;
}


//return 0: 找到相应的CGI处理函数
static int http_cgi_bin(void *pInst, const char *http_url, const char* ipstr, const char* host)
{
    if(NULL == http_url || *http_url == 0 )
        return 0;

    if(0 == http_hapi_handle(pInst, "GET", http_url, NULL, ipstr, host))
        return 0;

    if(0 == http_cgi_find(http_url))
        return -1;

    char *response_buf = (char *)anj_mw_malloc(HTTP_RESPONSE_BUF_SIZE);
    if(response_buf == NULL)
    {
        __ERR("resultBuf malloc error!\n");
        return 0;
    }

    memset(response_buf, 0, HTTP_RESPONSE_BUF_SIZE);
    __DBG("ip:%s:http_url:%s\n", ipstr, http_url);

    int status = http_cgi_handle(http_url, response_buf, ipstr);
    __DBG("status:%d, response_buf:%s", status, response_buf);

    if( g_ResponseFunc!= NULL)
    {
        g_ResponseFunc(pInst, response_buf, status);
    }

    if (response_buf != NULL)
    {
        anj_mw_free(response_buf);
        response_buf = NULL;
    }

    return 0;
}


int http_snapshot_request(void *pInst, void *pmsgt, const char* http_url, const char* ipRemote)
{
    __DBG("request: /snapshot.cgi\n");

    int iRet = 0;
    int Correct_display = 0;
    int quality = 50;
    int stream = 1;

    char filename[64] = {0};
    char localPath[64] = {0};
    char infomation[ANJ_FTPEMAIL_INFO_MAX_LEN] = {0};

    char buf[256] = {0};     //用于存储xml的内容
    char sn_str[32] = {0};

    anj_sysmng_load_sn(sn_str, sizeof(sn_str));

    strcpy(localPath, "/tmp");

    if (strstr(http_url, "stream=1"))
    {
        stream = 0;
    }
    else
    {
        stream = 1;
    }

    int upload_type = -1;   //初始化为一个异常的值
    if (strstr(http_url, "upload=0"))
    {
        upload_type = 0;   //上传到SD卡
    }
    else if(strstr(http_url, "upload=1"))
    {
        upload_type = 1;  //上传到FTP
    }
    else if(strstr(http_url, "upload=2"))
    {
        upload_type = 2;  //上传到Email
    }
    else if(strstr(http_url, "upload=3"))
    {
        upload_type = 3;  //上传到NFS服务器
    }
    else if(strstr(http_url, "upload="))
    {
        upload_type = 4;//上传的参数错误
    }
    else if(strstr(http_url, "upload"))
    {
        upload_type = 5;//上传的参数错误
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);      
    snprintf(filename, sizeof(filename), "snapshot_stream%d_%x_%x.jpg", stream, (unsigned int)tv.tv_sec, (unsigned int)tv.tv_usec/1000);

    char szCropSetting[64] = {0};
    if( cgi_get_value(http_url, "crop", szCropSetting, 64) > 0 )
    {
        int x, y, w, h;
        int ret = sscanf(szCropSetting, "%d,%d,%d,%d", &x, &y, &w, &h) ;
        __ERR("%s, sscanf=%d\n", szCropSetting, ret);
        if(ret == 4)
        {
            AreaStruct area = {x,y,w,h};
            anj_snap_jpg(0, stream, quality, localPath, filename, &area);
        }
        else
        {
            anj_snap_jpg(0, stream, quality, localPath, filename, NULL);
        }
    }
    else
    {
        __ERR("not found crop string\n");
        anj_snap_jpg(0, stream, quality, localPath, filename, NULL);
    }

    char filePath[256] = {0};
    snprintf(filePath, sizeof(filePath), "%s/%s", localPath, filename);

    //--------------------------------------------//

    char cmd[128] = {0};

    //分为4种方式上传
    if(upload_type >= 0)
    {
        int trycount = 300;
        while(trycount-- > 0)
        {
            if (anj_mw_file_exists(filePath) && is_jpeg_complete(filePath))
            {
                if (upload_type == 0)        //tf卡存储
                {
                    anj_sdcard_info *pstSdInfo = anj_sdcard_info_get();
                    if (pstSdInfo->eStatus != ANJ_SDCARD_STATUS_NORMAL)
                    {
                        __ERR("sdcard status:%d error!\n", pstSdInfo->eStatus);
                        snprintf(buf, sizeof(buf), XML_CGI_FAULT, "snapshot", "sd card not mounted");

                        goto __UPLOAD_FAIL; 
                    }

                    char mount_path[32] = {0};
                    snprintf(mount_path, sizeof(mount_path), SDCARD_MOUNT_PATH, anj_sdcard_mount_index_get());
                    snprintf(cmd, sizeof(cmd), "cp /tmp/%s %s", filename, mount_path);
                    anj_mw_system(cmd);

                    remove(filePath);
                    Correct_display = 1;

                    snprintf(buf, sizeof(buf), XML_HEAD"<upload>OK</upload>");
                    goto __UPLOAD_OK;
                }
                else if (upload_type == 1)       // ftp上传
                {
                    ServerConfig *pstServerCfg = (ServerConfig *)getServerConfig();
                    FtpServer *pstFtpCfg = &pstServerCfg->ftpServers[FTP_INDEX_FOR_ALARM_UPLOAD];
                    if (strlen(pstFtpCfg->serverIP) == 0)
                    {
                        snprintf(buf, sizeof(buf), XML_CGI_FAULT,"snapshot","Ftp not deploy");
                        goto __UPLOAD_FAIL;
                    }

                    iRet = anj_ftpemail_ftp_file(pstFtpCfg, localPath, filename,
                                                 pstFtpCfg->filePath, filename);
                    if (iRet != 0)
                    {
                        __ERR("ftp transfer file:%s failed!\n", filename);
                        snprintf(buf, sizeof(buf), XML_CGI_FAULT, "snapshot", "Ftp upload failed");
                        goto __UPLOAD_FAIL;
                    }

                    Correct_display = 1;
                    snprintf(buf, sizeof(buf), XML_HEAD"<upload>OK</upload>");
                    goto __UPLOAD_OK;
                }
                else if (upload_type == 2)       // email 
                {
                    ServerConfig *pstServerCfg = (ServerConfig *)getServerConfig();
                    SmtpServerList *pstSmtpCfg = &pstServerCfg->smtpServers;
                    if (strlen(pstSmtpCfg->smtpServers[SMTP_INDEX_FOR_ALARM_UPLOAD].toMail) == 0)
                    {
                        snprintf(buf,  sizeof(buf), XML_CGI_FAULT, "snapshot", "Email not deploy");
                        goto __UPLOAD_FAIL;
                    }

                    snprintf(infomation, sizeof(infomation),
                             "Snapshot upload from IP Camera.\r\nDevice SN: <%s>\r\n", sn_str);
                    iRet = anj_ftpemail_email_file(pstSmtpCfg, SMTP_INDEX_FOR_ALARM_UPLOAD,
                                                   localPath, filename, infomation);
                    if (iRet != 0)
                    {
                        __ERR("email transfer file:%s failed\n", filename);
                        snprintf(buf, sizeof(buf), XML_CGI_FAULT, "snapshot", "Email upload failed");
                        goto __UPLOAD_FAIL;
                    }

                    Correct_display = 1;
                    snprintf(buf, sizeof(buf), XML_HEAD"<upload>OK</upload>");
                    goto __UPLOAD_OK;
                }
                else if (upload_type == 3)      // nfs
                {
                    // todo
                    snprintf(buf, sizeof(buf), XML_CGI_FAULT, "snapshot", "NFS Server not mounted");
                    goto __UPLOAD_FAIL;
                }
                else if (upload_type == 4 || upload_type == 5)
                {
                    snprintf(buf, sizeof(buf), XML_CGI_FAULT, "snapshot", "Upload parameter is error");
                    goto __UPLOAD_FAIL;
                }
            }
            else
            {
                usleep(10 * 1000);
            }
        }    
    }            


    // 没有upload参数的话，使用这种方式
    if ((strstr(http_url, "upload") == NULL)  && (Correct_display == 0))
    {
        int trycount = 300;
        while(trycount-- > 0)
        {            
            if (anj_mw_file_exists(filePath) && is_jpeg_complete(filePath))
            {
                iRet =  http_send_file_cb(pInst, pmsgt, filePath, "image/jpeg");
                remove(filePath);        

                __DBG("snap jpeg and http trans OK: %s\n", filename);        
                return iRet;
            }
            else
            {
                usleep(10 * 1000);
            }
        }
    }

    __ERR("snapshot jpeg not found:%s\n", filename);
    return 407;

__UPLOAD_OK:
    if(g_ResponseFunc!= NULL)
    {
        g_ResponseFunc(pInst, buf, SOAP_FILE);
    }

    return HTTP_OK;


__UPLOAD_FAIL:
    if(g_ResponseFunc!= NULL)
    {
        g_ResponseFunc(pInst, buf, SOAP_FILE);
    }

    return SOAP_FAULT;
}

int http_get_proc(void *pInst, void *pmsgt, const char* http_url, const char* ipRemote, const char* host)
{
    unsigned int nowTime = GetCurrentTimeStamp();

    __DBG("http get handle! ip:%s, host:%s, http url:%s!\n", (ipRemote != NULL) ? ipRemote : "", host, http_url);

    //新加http功能
    //以? 拆分soap->http_url
    int iRet1 = -1;
    int iRet2 = -1;

    static char url[10][200];
    memset(url, 0, 10 * 200);

    split_path(http_url, url);

    char *pResultBuf = NULL;
    pResultBuf = (char*)anj_mw_malloc(HTTP_GET_RESULT_BUF_SIZE);
    if(pResultBuf == NULL)
    {
        __ERR("resultBuf malloc error!\n");
        return -1;
    }
    memset(pResultBuf, 0, HTTP_GET_RESULT_BUF_SIZE);
    
    //判断是否是登录并处理
    iRet1 = login_request(http_url, url, pResultBuf);

    //查询/设置配置信息
    iRet2 = cgi_settings_request(http_url, url, pResultBuf);

    if(iRet1 == 0 || iRet2 == 0)
    {
        //响应结果
        if(strlen(pResultBuf) == 0)
        {
            strcpy(pResultBuf, "ok");
        }

        if(g_ResponseFunc != NULL)
        {
            g_ResponseFunc(pInst, pResultBuf, SOAP_FILE);
        }

        anj_mw_free(pResultBuf);
        pResultBuf = NULL;
        return HTTP_OK;
    }

    anj_mw_free(pResultBuf);
    pResultBuf = NULL;

    /************************** 
    *    新增截图扩展功能
    **************************/
    if (strstr(http_url, "snapshot.cgi"))
    {
        //YCX版本只有在开启ONVIF认证的时候才需要密码,非YCX版本任何时候都需要密码
        MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
        if (pstMediaStreamCfg->webConfig.onvif_auth)
        {
            char *pResBuf = (char *)anj_mw_malloc(1024);
            if (pResBuf == NULL)
            {
                __ERR("pResBuf malloc failed\n");
                return -1;
            }

            memset(pResBuf, 0, 1024);
            int cgi_ret = cgi_is_valid_uid_or_username(http_url, pResBuf, "/snapshot.cgi");
            if(cgi_ret == 0)
            {
                if(g_ResponseFunc!= NULL)
                {
                    g_ResponseFunc(pInst, pResBuf, HTTP_RES_STATUS_UNAUTH);
                }
            }
            
            anj_mw_free(pResBuf);
            pResBuf = NULL;

            if(cgi_ret == 0)
                return HTTP_RES_STATUS_UNAUTH;
        }

        return http_snapshot_request(pInst, pmsgt, http_url, ipRemote);
    }

    /************************** 
    *    ipc http cgi接口
    *    统一处理url为/cgi-bin/的cgi命令
    *    added on 2017-12-07
    **************************/
    if(0 == http_cgi_bin(pInst, http_url, ipRemote, host)) 
    {
        return HTTP_OK;
    }

    /************************** 
    *    处理宇视NVR请求
    **************************/
    int unv_enable = unv_enable_get();
    if(unv_enable)      //YCX版本 && (支持人形)
    {
        char* responsebuf = NULL;
        int status = unv_handle_get_request(http_url, &responsebuf);
        if(status > 0 && responsebuf != NULL)
        {
            __DBG("response:%s\n", responsebuf);
            if(g_ResponseFunc!= NULL)
            {
                g_ResponseFunc(pInst, responsebuf, SOAP_FILE);
            }
        }

        if(responsebuf)
        {
            anj_mw_free(responsebuf);
            responsebuf = NULL;
        }

        if(status != -1)
        {
            return HTTP_OK;
        }
    }

    /* gSOAP >=2.5 soap_response() will do this automatically for us, when sending SOAP_HTML or SOAP_FILE:
    if ((soap->omode & SOAP_IO) != SOAP_IO_CHUNK)
    soap_set_omode(soap, SOAP_IO_STORE); */ /* if not chunking we MUST buffer entire content when returning HTML pages to determine content length */

    /* Use http_url (from request URL) to determine request: */

    //    DebugLog( "%s: %s\n", soap->szAgentName, soap->endpoint);
    /* Note: http_url always starts with '/' */
    //  if (strchr(http_url + 1, '/') || strchr(http_url + 1, '\\'))    /* we don't like snooping in dirs */
    //   return 403; /* HTTP forbidden */

    //    if( strcmp(soap->szAgentName, "Digifort") == 0 )
    //        return 404; /* HTTP not found */
    //    printf("http_url:%s\n", http_url);

    int admin_login = UserAuthAdminLoginGet();
    if(admin_login) //added by XXX 20131223
    {
        // redirect /user to user.html
        // redirect /admin to admin.html

        __DBG("url:%s\n", http_url);
        if (strcmp(http_url, "/user")==0)
        {
            return http_send_file_cb(pInst, pmsgt, "user.html", "text/html");
        }
        else  if (strcmp(http_url, "/admin")==0)
        {
            return http_send_file_cb(pInst, pmsgt, "admin.html", "text/html");
        }
    }

    //eg:http://192.168.65.29/WEBlogin?username=admin&password=123456
    if (strcmp(http_url, "/") == 0 || strstr(http_url, "/WEBlogin?username") != NULL)
    {
        char file_ver[128] = "";
        read_file_to_string("/etc/filesys.ver", file_ver, sizeof(file_ver));
        if( strstr(file_ver, "_AEZJ") == NULL )
        {
            if(strstr(http_url, "/WEBlogin?username") != NULL)
            {
                strcpy(s_http_login_path, "SkipLogin=1");
                strcat(s_http_login_path, http_url);
            }
            else
            {
                strcpy(s_http_login_path, "SkipLogin=0");
            }

            __INFO("web login Path:%s\n", s_http_login_path);
            return http_send_file_cb(pInst, pmsgt, "index.html", "text/html");
        }
        else
        {
            return http_send_file_cb(pInst, pmsgt, "welcome.html", "text/html");
        }
    }
    else if(strstr(http_url, "/?userName") != NULL)
    {
        //  /?userName=admin&token=8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92&targetPage=/index/Preview
        __DBG("Dahua tmp token web login Path:%s\n", http_url);

        char *pToken = NULL;
        int iIndex = 0;
        char tmpUserName[64] = {0};
        char tmpWebLoginToken[128] = {0};

        char *http_tmp_url = (char *)anj_mw_malloc(strlen(http_url) + 1);
        if (http_tmp_url)
        {
            pToken = strtok(http_tmp_url, "&");
            while(pToken != NULL)
            {
                __DBG("tmpWebLoginToken iIndex:%d, ptoken:%s\n", iIndex, pToken);
                if(iIndex == 0)
                {
                    strncpy(tmpUserName, pToken, sizeof(tmpUserName) - 1);
                }
                else if (iIndex == 1)
                {
                    strncpy(tmpWebLoginToken, pToken, sizeof(tmpWebLoginToken) - 1);
                }

                iIndex++;
                pToken = strtok(NULL, "&");

                usleep(10 * 1000);
            }

            anj_mw_free(http_tmp_url);
            http_tmp_url = NULL;
        }

        __DBG("########tmpUserName:%s, tmpWebLoginToken:%s\n", tmpUserName, tmpWebLoginToken);

        char webLoginUserName[64] = {0};
        char webLoginToken[128] = {0};

        memcpy(webLoginUserName, tmpUserName + strlen("/?userName="), strlen(tmpUserName) - strlen("/?userName="));
        memcpy(webLoginToken, tmpWebLoginToken + strlen("token="), strlen(tmpWebLoginToken) - strlen("token="));
        __DBG("webLoginUserName:%s, webLoginToken:%s\n", webLoginUserName, webLoginToken);

        char devToken[128] = {0};
        char devicePwd[64] = {0};
        UserAuthGetPassword(webLoginUserName, devicePwd);

        if(anj_mw_file_exists("/tmp/ez_weblogin_token.txt"))
        {
            read_file_to_string("/tmp/ez_weblogin_token.txt", devToken, sizeof(devToken));
            __DBG("Get device current web login tmptoken:%s\n", devToken);
        }
        else
        {
            __DBG("Don't get device current web login tmptoken, return 404\n");
            return http_send_file_cb(pInst, pmsgt, "f404.html", "text/html");
        }

        if(strncmp(devToken, webLoginToken, strlen(webLoginToken)) == 0)
        {
            snprintf(s_http_login_path, sizeof(s_http_login_path), "SkipLogin=1/WEBlogin?username=%s&password=%s", webLoginUserName, devicePwd);
            //strcpy(g_loginPath,"SkipLogin=1/WEBlogin?username=admin&password=123456");
        }
        else
        {
            __DBG("Device login tmptoken != current token, return 404\n");
            __DBG("devToken:%s, webLoginToken:%s \n", devToken, webLoginToken);
            return http_send_file_cb(pInst, pmsgt,"f404.html", "text/html");
        }

        return http_send_file_cb(pInst, pmsgt, "index.html", "text/html");
    }

    if (strcmp(http_url, "/zjadmin") == 0)
    {
        return http_send_file_cb(pInst, pmsgt, "index.html", "text/html");
    }

    if (strstr(http_url, "/?cloudhost"))
    {
        return http_send_file_cb(pInst, pmsgt, "index.html", "text/html");
    }    

    char uripath[256] = {0};
    strcpy(uripath, http_url);
    char *p = strchr(uripath, '?');
    if (p)
    {
        *p = '\0';
    }

    if((strlen(uripath) > strlen("/playback/")) && (!strncmp(uripath, "/playback/", strlen("/playback/"))))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/octet-stream"); 
    }

    if (!http_file_tag_cmp(uripath, "*.css"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "text/css");
    }
    if (!http_file_tag_cmp(uripath, "*.js"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "text/javascript");
    }
    if (!http_file_tag_cmp(uripath, "*.map"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "text/javascript");
    }
    if (!http_file_tag_cmp(uripath, "*.swf"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/x-shockwave-flash");
    }
    if (!http_file_tag_cmp(uripath, "*.html"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "text/html");
    }
    if (strstr(http_url, "ConfigFile.ini"))
    {
        return http_send_file_cb(pInst, pmsgt, "ConfigFile.ini", "helloworld");
    }
    if (strstr(http_url, "config.xml"))
    {
        return http_send_file_cb(pInst, pmsgt, "config.xml", "application/octet-stream");
    }
    if (strstr(http_url, "snapshot.jpeg"))
    { 
        return http_send_file_cb(pInst, pmsgt, "snapshot.jpeg", "application/zip");
    }

    if (!http_file_tag_cmp(uripath, "*.xml") ||
        !http_file_tag_cmp(uripath, "*.xsd") ||
        !http_file_tag_cmp(uripath, "*.wsdl"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "text/xml");
    }
    if (!http_file_tag_cmp(uripath, "*.jpg"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "image/jpeg");
    }
    if (!http_file_tag_cmp(uripath, "*.gif"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "image/gif");
    }
    if (!http_file_tag_cmp(uripath, "*.png"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "image/png");
    }
    if (!http_file_tag_cmp(uripath, "*.ico"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "image/ico");
    }
    if (!http_file_tag_cmp(uripath, "*.mp3"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "audio/mp3");
    }
    if (!http_file_tag_cmp(uripath, "*.mp4"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "video/mp4");
    }

    if (!http_file_tag_cmp(uripath, "*.hevc"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "video/hevc");
    }
    if (!http_file_tag_cmp(uripath, "*.h265"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "video/h265");
    }
    if (!http_file_tag_cmp(uripath, "*.avi"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "video/avi");
    }
    if (!http_file_tag_cmp(uripath, "*.flv"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "video/x-flv");
    }
    if (!http_file_tag_cmp(uripath, "*.log"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/octet-stream");
    }
    if (!http_file_tag_cmp(uripath, "*.txt"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "text/plain");
    }
    if (!http_file_tag_cmp(uripath, "*.zip"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/zip");
    }
    if (!http_file_tag_cmp(uripath, "*.tgz"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/x-compressed");
    }
    if (!http_file_tag_cmp(uripath, "*.h264"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/octet-stream");
    }
    if (!http_file_tag_cmp(uripath, "*.webm"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/octet-stream");
    }
    if (!http_file_tag_cmp(uripath, "*.mem"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/octet-stream");
    }
    if (!http_file_tag_cmp(uripath, "*.wasm"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/octet-stream");
    }
    if (!http_file_tag_cmp(uripath, "*.exe"))
    {
        return http_send_file_cb(pInst, pmsgt, uripath + 1, "application/octet-stream");
    }
    __ERR("http_url:%s\n", http_url);


    int error_count = http_get_login_passwd_error_count();
    if(error_count > 5) //密码错误次数超过5次
    {
        unsigned int lockTime = 60 * 1000;
        unsigned int lock_start_time = http_get_login_passwd_error_locktime();

        if(nowTime - lock_start_time < lockTime)
        {
            __INFO("please wait %d s, password will unlock\n", lockTime / 1000);

            if(g_ResponseFunc!= NULL)
            {
                g_ResponseFunc(pInst, "The user is locked", SOAP_FILE);
            }

            return http_send_file_cb(pInst, pmsgt, "f404.html", "text/html");;
        }
    }

#if 0
    if (!http_file_tag_cmp(http_url, "*.css"))
    {
    return http_send_file_cb(pInst, http_url + 1, "text/css");
    }
    if (!http_file_tag_cmp(http_url, "*.html"))
    return http_send_file_cb(pInst, http_url + 1, "text/html");
    if (!http_file_tag_cmp(http_url, "*.xml")
    || !http_file_tag_cmp(http_url, "*.xsd")
    || !http_file_tag_cmp(http_url, "*.wsdl"))
    return http_send_file_cb(pInst, http_url + 1, "text/xml");
    if (!http_file_tag_cmp(http_url, "*.jpg"))
    return http_send_file_cb(pInst, http_url + 1, "image/jpeg");
    if (!http_file_tag_cmp(http_url, "*.gif"))
    return http_send_file_cb(pInst, http_url + 1, "image/gif");
    if (!http_file_tag_cmp(http_url, "*.png"))
    return http_send_file_cb(pInst, http_url + 1, "image/png");
    if (!http_file_tag_cmp(http_url, "*.ico"))
    return http_send_file_cb(pInst, http_url + 1, "image/ico");

#endif
    if (strstr(http_url, "httpPTZ/"))
    {
        char cmd[32] = {0};
        char ptzcmd[256] = {0};
        if (strstr(http_url, "stop"))
        {
            strcpy(cmd,"stop");
        }
        else if (strstr(http_url, "zoomwide"))
        {
            strcpy(cmd,"zoomwide");
        }
        else if (strstr(http_url, "zoomtele"))
        {
            strcpy(cmd,"zoomtele");
        }
        else if (strstr(http_url, "FocusFarAutoOff"))
        {
            strcpy(cmd,"FocusFarAutoOff");
        }
        else if (strstr(http_url, "FocusNearAutoOff"))
        {
            strcpy(cmd,"FocusNearAutoOff");
        }
        else if (strstr(http_url, "IrisOpenAutoOff"))
        {
            strcpy(cmd,"IrisOpenAutoOff");
        }
        else if (strstr(http_url, "IrisCloseAutoOff"))
        {
            strcpy(cmd,"IrisCloseAutoOff");
        }
        else if (strstr(http_url, "ptzCtrl="))
        {
            char *pIndex1 = strstr(http_url, "ptzCtrl=") + strlen("ptzCtrl=");
            if(pIndex1 == NULL)
                return 400;

            char *pIndex2 = strchr(pIndex1,'/');
            if(pIndex2 == NULL)
                return 400;

            char ptzCtrl[32] = {0};
            strncpy(ptzCtrl, pIndex1, pIndex2 - pIndex1);
            pIndex1 = strstr(http_url, "panspeed=") + strlen("panspeed=");
            if(pIndex1 == NULL)
                return 400;

            pIndex2 = strchr(pIndex1, '/');
            if(pIndex2 == NULL)
                return 400;

            char pSpeed[8] = {0};
            strncpy(pSpeed, pIndex1, pIndex2 - pIndex1);
            pIndex1 = strstr(http_url, "tiltspeed=") + strlen("tiltspeed=");
            if(pIndex1 == NULL)
                return 400;

            pIndex2 = strchr(pIndex1, '/');
            char tSpeed[8] = {0};
            if(pIndex2 == NULL)
                strcpy(tSpeed, pIndex1);
            else
                strncpy(tSpeed, pIndex1, pIndex2 - pIndex1);
            snprintf(ptzcmd, sizeof(ptzcmd), "<xml><cmd>%s</cmd><panspeed>%s</panspeed><tiltspeed>%s</tiltspeed></xml>", ptzCtrl, pSpeed, tSpeed);
        }
        else
        {
            strcpy(cmd, "stop");
        }

        if(strlen(ptzcmd) <= 0)
            snprintf(ptzcmd, sizeof(ptzcmd), "<xml>\n<cmd>%s</cmd>\n</xml>\n", cmd);

        //AuxMsgPTZCmd(ptzcmd);
        __ERR(">>-------setPTZCmd=%s<<---------\n",ptzcmd);    
        return 200;
    }

    if (strstr(http_url, "common.js"))
    {
        __DBG("request common.js file\n");
        return http_send_file_cb(pInst, pmsgt, "common.js", "text/javascript");
    }
    if (strstr(http_url, "login.html"))
    {
        __DBG("request login.html file\n");
        return http_send_file_cb(pInst, pmsgt, "login.html", "text/html");
    }

    if (strstr(http_url, "player.html"))
    {
        __DBG("request player.html file\n");
        return http_send_file_cb(pInst, pmsgt, "player.html", "text/html");
    }

    if (strstr(http_url, "WEBConfig.exe"))
    {
        __DBG("request WEBConfig.exe file\n");
        return http_send_file_cb(pInst, pmsgt, "WEBConfig.exe", "application/octet-stream");
    }

    if (strstr(http_url, "command_port.ini"))
    {
        __DBG("request command_port.ini file\n");
        return http_send_file_cb(pInst, pmsgt, "command_port.ini", "text/html");
    }

    //------------------------------------------------//
    if (strstr(http_url, "wireless_sta?"))
    {
        NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
        WIFIConfig *pstWifiCfg = &pstNetworkCfg->wifiCfg;

        int ret = 0;
        char params[256] = {0};
        char ssid[64] = {0};
        char securityMode[32] = {0};
        char passwd[64] = {0};
        char wirelessmode[32] = {0};

        char *p = strstr(http_url, "wireless_sta?");
        strcpy(params, p + strlen("wireless_sta?"));

        char *token = strtok(params,"&");
        while(token!=NULL)
        {
            if (strstr(token, "SSID="))
            {
                strcpy(ssid, token + strlen("SSID="));
            }
            else if(strstr(token, "SecurityMode="))
            {
                strcpy(securityMode, token + strlen("SecurityMode="));
            }
            else if(strstr(token, "passwd="))
            {
                strcpy(passwd, token + strlen("passwd="));
            }
            else if(strstr(token, "WirelessMode="))
            {
                strcpy(wirelessmode, token  + strlen("WirelessMode="));
            }

            token=strtok(NULL,"&");
        } 

        int encryptEnable = 1;
        char wpaEncryType[32] = {0};
        char wpaAuthMode[32] = {0};
        char wepKeyMode[32] = {0};
        char wifiEncryptType[32] = {0};  
        //char macMode[128];

        int iSecurityMode = atoi(securityMode);
        if (iSecurityMode == HUAWEI_WIFI_AUTH_NONE)
        {
            encryptEnable = 0;
        }

        strcpy(wpaEncryType, "TKIP");

        if (iSecurityMode == HUAWEI_WIFI_AUTH_WPA2_PSK_TKIP || 
            iSecurityMode == HUAWEI_WIFI_AUTH_WPA_PSK_TKIP)
        {
            strcpy(wpaEncryType, "TKIP");
        }
        else if(iSecurityMode == HUAWEI_WIFI_AUTH_WPA2_PSK_AES ||
                iSecurityMode == HUAWEI_WIFI_AUTH_WPA_PSK_AES)
        {
            strcpy(wpaEncryType, "AES");
        }

        strcpy(wpaAuthMode, "WPA2PSK");
        if (iSecurityMode == HUAWEI_WIFI_AUTH_WPA_PSK_TKIP || 
            iSecurityMode == HUAWEI_WIFI_AUTH_WPA_PSK_AES)
        {
            strcpy(wpaAuthMode, "WPAPSK");
        }
        else if(iSecurityMode == HUAWEI_WIFI_AUTH_WPA2_PSK_TKIP || 
                iSecurityMode == HUAWEI_WIFI_AUTH_WPA2_PSK_AES)
        {
            strcpy(wpaAuthMode, "WPA2PSK");
        }

        strcpy(wepKeyMode, "HEX");
        if (iSecurityMode == HUAWEI_WIFI_AUTH_WEP_SHARED && 
            (strlen(passwd) == 5 || strlen(passwd) == 13 || strlen(passwd) == 16))
        {
            strcpy(wepKeyMode, "ASCII");
        }

        strcpy(wifiEncryptType, "wpa");
        if (iSecurityMode ==  HUAWEI_WIFI_AUTH_WEP_SHARED
        || iSecurityMode ==  HUAWEI_WIFI_AUTH_WEP_NONE)
        {
        strcpy(wifiEncryptType, "wep");
        }
        else if(iSecurityMode ==  HUAWEI_WIFI_AUTH_WPA_PSK_TKIP
        || iSecurityMode ==  HUAWEI_WIFI_AUTH_WPA_PSK_AES
        || iSecurityMode ==  HUAWEI_WIFI_AUTH_WPA2_PSK_TKIP
        || iSecurityMode ==  HUAWEI_WIFI_AUTH_WPA2_PSK_AES)
        {
            strcpy(wifiEncryptType, "wpa");
        }
        else if(iSecurityMode == HUAWEI_WIFI_AUTH_NONE)
        {
            strcpy(wifiEncryptType, "NONE");
        }

        pstWifiCfg->enable = 1;
        pstWifiCfg->dhcpEnable = 1;
        strncpy(pstWifiCfg->operationMode, "managed", MAX_WIRELESS_OPERATIONMODE_NAME_LEN-1);
        strncpy(pstWifiCfg->essid, ssid, MAX_WIRELESS_ESSID_NAME_LEN);
        pstWifiCfg->channelNum = 1;
        strcpy(pstWifiCfg->region, "CN");
        strcpy(pstWifiCfg->bitRate, "33");
        strncpy(pstWifiCfg->macMode, wirelessmode, MAX_WIRELESS_MACMODE_NAME_LEN-1);
        pstWifiCfg->wirelessEncrypt.enable = encryptEnable;

        strcpy(pstWifiCfg->wirelessEncrypt.encryptType, wifiEncryptType);
        strcpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.authMode, "SHARED");
        strcpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.encryptType, "WEP64");
        pstWifiCfg->wirelessEncrypt.wepEncrypt.keyIndex = 1;                        
        strncpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.keyMode, wepKeyMode, MAX_WEPENCRYPT_KEYMODE_NAME_LEN-1);
        strncpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.keyValue, passwd, MAX_WEPENCRYPT_KEYVALUE_LEN);
        strncpy(pstWifiCfg->wirelessEncrypt.wpaEncrypt.encryptType, wpaEncryType, MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN-1);
        strncpy(pstWifiCfg->wirelessEncrypt.wpaEncrypt.authMode, wpaAuthMode, MAX_WPAENCRYPT_AUTHMODE_NAME_LEN-1);
        strncpy(pstWifiCfg->wirelessEncrypt.wpaEncrypt.keyValue,passwd, MAX_WPAENCRYPT_KEYVALUE_LEN);

        anj_config_network_wifi_set(pstWifiCfg);

        __ERR("set wifi config success\n");

        if(g_ResponseFunc!= NULL)
        {
            g_ResponseFunc(pInst, "", SOAP_FILE);
        }

        anj_net_reset();
        return ret;
    }

    if (strstr(http_url, "ipc/wireless"))
    {
        if (strstr(http_url, "cmd=search"))
        {
            __ERR("recv client search wifi req (http_url:%s)\n", http_url);
            memset(&s_http_ap_list, 0, sizeof(WIFI_AP_SCAN));
            iRet2 = anj_net_provider_wifi_ap_info_get((void *)&s_http_ap_list);
        __ERR("wifi ap count: %d\n", s_http_ap_list.apCnt);

            if(s_http_ap_list.apCnt > 0)
            {
                //int body_size=g_apList.apCnt*256 + 256;    
                char *pe = NULL;
                char *pb = NULL;

                char body[2048] = {0};
                pb = body;
                pe = pb + sizeof(body);

                int i = 0;
                for(i = 0; i < s_http_ap_list.apCnt; i++)
                {
                    pb += snprintf(pb, pe-pb, "ssid=%s\nkeyindex=%d\n", s_http_ap_list.apInfos[i].ssid, i+1);        
                }

                if(g_ResponseFunc!= NULL)
                {
                    g_ResponseFunc(pInst, body, SOAP_FILE);
                }

                return HTTP_OK;
            }
            else
            {
                if(g_ResponseFunc!= NULL)
                {
                    g_ResponseFunc(pInst, "", SOAP_FILE);
                }

                return HTTP_OK;
            }
        }
        else if (strstr(http_url, "cmd=get"))
        {
            __ERR("recv client get wifi req (http_url:%s)\n", http_url);
            const char *pos = strstr(http_url, "keyindex=");
            if (pos == NULL)
            {
                __ERR("invalid params\n");
                return HTTP_RES_STATUS_BAD_REQUEST;
            }

            int keyIndex = atoi(pos + strlen("keyindex="));
            if (keyIndex > s_http_ap_list.apCnt)
            {
                __ERR("invalid idx(idx:%d, ttalcnt:%d)\n", keyIndex, s_http_ap_list.apCnt);
                return HTTP_RES_STATUS_BAD_REQUEST;
            }

            WIFI_AP_INFO *pApInfo = &s_http_ap_list.apInfos[keyIndex - 1];
            char buf[256] = {0};
            int mode = 0;
            if (pApInfo->authMode == WIFI_AUTH_OPEN)
            {
                mode = HUAWEI_WIFI_AUTH_NONE;
            }
            else if (pApInfo->encryType == WIFI_ENCRYP_WEP)
            {
                mode = HUAWEI_WIFI_AUTH_WEP_SHARED;
            }
            else if(pApInfo->authMode == WIFI_AUTH_WPAPSK && pApInfo->encryType == WIFI_ENCRYP_TKIP)
            {
                mode = HUAWEI_WIFI_AUTH_WPA_PSK_TKIP;
            }
            else if(pApInfo->authMode == WIFI_AUTH_WPAPSK && pApInfo->encryType == WIFI_ENCRYP_AES)
            {
                mode = HUAWEI_WIFI_AUTH_WPA_PSK_AES;
            }        
            else if(pApInfo->authMode == WIFI_AUTH_WPA2PSK && pApInfo->encryType == WIFI_ENCRYP_TKIP)
            {
                mode = HUAWEI_WIFI_AUTH_WPA2_PSK_TKIP;
            }
            else if(pApInfo->authMode == WIFI_AUTH_WPA2PSK && pApInfo->encryType == WIFI_ENCRYP_AES)
            {
                mode = HUAWEI_WIFI_AUTH_WPA2_PSK_AES;
            }            

            snprintf(buf, sizeof(buf), "enable=1\nssid=%s\nauthentication=%d\nkeyindex=%d\nkey=*********\nstatus=0",
                pApInfo->ssid, mode, keyIndex);

            if(g_ResponseFunc!= NULL)
            {
                g_ResponseFunc(pInst, buf, SOAP_FILE);
            }
            return HTTP_OK;
        }
        else if (strstr(http_url, "cmd=current"))
        {
            __ERR("recv client get current wifi info(http_url:%s)\n", http_url);

            char buf[256] = {0};
            int status = 0;
            int authentication = HUAWEI_WIFI_AUTH_NONE;

            NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
            WIFIConfig *pstWifiCfg = &pstNetworkCfg->wifiCfg;

            NETWORK_STATUS_DATA stNetworkStatus = {0};
            anj_net_info_get(&stNetworkStatus);

            if (stNetworkStatus.signallevel != 0)
            {
                status = 1; 
            }

            if (pstWifiCfg->wirelessEncrypt.enable)
            {
                if (strcmp(pstWifiCfg->wirelessEncrypt.encryptType, "wep") == 0)
                {
                    authentication = HUAWEI_WIFI_AUTH_WEP_SHARED;
                }
                else if (strcmp(pstWifiCfg->wirelessEncrypt.encryptType, "wpa") == 0)
                {
                    if (strcmp(pstWifiCfg->wirelessEncrypt.wpaEncrypt.encryptType, "TKIP") == 0)
                    {
                        authentication = HUAWEI_WIFI_AUTH_WPA_PSK_TKIP;
                    }
                    else if(strcmp(pstWifiCfg->wirelessEncrypt.wpaEncrypt.encryptType, "AES") == 0)
                    {
                        authentication = HUAWEI_WIFI_AUTH_WPA_PSK_AES;
                    }
                }
            }

            snprintf(buf, sizeof(buf), "enable=%d\nssid=%s\nauthentication=%d\nkeyindex=1\nkey=*********\nstatus=%d",
                pstWifiCfg->enable, pstWifiCfg->essid, authentication, status);

            if(g_ResponseFunc!= NULL)
            {
                g_ResponseFunc(pInst, buf, SOAP_FILE);
            }

            return HTTP_OK;
        }
        else if (strstr(http_url, "cmd=set"))
        {
            NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
            WIFIConfig *pstWifiCfg = &pstNetworkCfg->wifiCfg;

            int encryptEnable = 0;
            char encryptType[32] = {0};
            strcpy(encryptType, "wep");
            char macMode[32] = {0};
            char passwd[128] = {0};
            int ret = 0;

            char wepKeyMode[32] = {0};
            strcpy(wepKeyMode, "HEX");

            char wpaEncryType[32] = {0};
            char wpaAuthMode[32] = {0};

            int ssidIdx = -1;
            char ssid[64] = {0};
            ssid[0] = '\0';

            char keyValue[64] = {0};
            keyValue[0] = '\0';

            int authentication = -1; 
            int enable = -1;

            __ERR("recv client set wifi req111111111111(http_url:%s)\n", http_url);

            char *params = strstr(http_url, "cmd=set&");
            if (params == NULL)
            {
                __ERR("invalid params(http_url:%s)\n", http_url);
                return HTTP_RES_STATUS_BAD_REQUEST;
            }

            char *token = strtok(params, "&");
            while(token != NULL)
            {
                char *p = NULL;
                if (NULL != (p = strstr(token, "enable=")))//J  20140120
                {
                    enable = atoi(p + strlen("enable="));
                }
                else if (NULL != (p = strstr(token, "ssid=")))
                {
                    strcpy(ssid, p + strlen("ssid="));
                }
                else if (NULL != (p = strstr(token, "authentication=")))
                {
                    authentication = atoi(p + strlen("authentication="));
                }
                else if (NULL != (p = strstr(token, "keyindex=")))
                {
                    ssidIdx = atoi(p + strlen("keyindex="));
                }
                else if (NULL != (p = strstr(token, "key=")))
                {
                    strcpy(keyValue, p + strlen("key="));
                }

                token = strtok(NULL, "&");
            }         


            if (authentication == -1 || 
                enable == -1 || 
                strlen(keyValue) == 0 || 
                ssidIdx == -1 ||
                strlen(ssid) == 0)
            {
                __ERR("invalid params\n");
                return HTTP_RES_STATUS_BAD_REQUEST;
            }

            strcpy(passwd, keyValue);
            __ERR("auth:%d, enable:%d, key:%s, ssidIdx:%d, ssid:%s\n",
                authentication, enable, keyValue, ssidIdx, ssid);

            if (ssidIdx > s_http_ap_list.apCnt || ssidIdx < 1)
            {
                __ERR("invalid ssid idx(%d, total:%d)\n", ssidIdx, s_http_ap_list.apCnt);
                return HTTP_RES_STATUS_BAD_REQUEST;
            }

            WIFI_AP_INFO *pApInfo = &s_http_ap_list.apInfos[ssidIdx - 1];
            if (strcmp(pApInfo->ssid, ssid) != 0)
            {
                __ERR("ssid not match(ssid_ipc:%s, ssid_req:%s)\n", pApInfo->ssid, ssid);
                return HTTP_RES_STATUS_BAD_REQUEST;
            }

            strcpy(macMode, pApInfo->wirelessMode);
            if (pApInfo->authMode != WIFI_AUTH_OPEN)
            {
                encryptEnable = 1;
            }
            if (pApInfo->encryType != WIFI_ENCRYP_WEP)
            {
                strcpy(encryptType, "wpa");
            }

            if (pApInfo->encryType == WIFI_ENCRYP_WEP && 
                (strlen(passwd) == 5 || strlen(passwd) == 13 || strlen(passwd) == 16))
            {
                strcpy(wepKeyMode, "ASCII");
            }

            strcpy(wpaEncryType, "TKIP");
            if (pApInfo->encryType == WIFI_ENCRYP_TKIP)
            {
                strcpy(wpaEncryType, "TKIP");
            }
            else if(pApInfo->encryType == WIFI_ENCRYP_AES)
            {
                strcpy(wpaEncryType, "AES");
            }

            strcpy(wpaAuthMode, "WPAPSK");
            if (pApInfo->authMode == WIFI_AUTH_WPAPSK)
            {
                strcpy(wpaAuthMode, "WPAPSK");
            }
            else if(pApInfo->authMode == WIFI_AUTH_WPA2PSK)
            {
                strcpy(wpaAuthMode, "WPA2PSK");
            }

            pstWifiCfg->enable = 1;
            pstWifiCfg->dhcpEnable = 1;
            strncpy(pstWifiCfg->operationMode, "managed", MAX_WIRELESS_OPERATIONMODE_NAME_LEN-1);
            strncpy(pstWifiCfg->essid, ssid, MAX_WIRELESS_ESSID_NAME_LEN);
            pstWifiCfg->channelNum = 1;
            strcpy(pstWifiCfg->region, "CN");
            strcpy(pstWifiCfg->bitRate, "33");
            strncpy(pstWifiCfg->macMode, macMode, MAX_WIRELESS_MACMODE_NAME_LEN-1);
            pstWifiCfg->wirelessEncrypt.enable = encryptEnable;

            strcpy(pstWifiCfg->wirelessEncrypt.encryptType, encryptType);
            strcpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.authMode, "SHARED");
            strcpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.encryptType, "WEP64");
            pstWifiCfg->wirelessEncrypt.wepEncrypt.keyIndex = 1;                        
            strncpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.keyMode, wepKeyMode, MAX_WEPENCRYPT_KEYMODE_NAME_LEN-1);
            strncpy(pstWifiCfg->wirelessEncrypt.wepEncrypt.keyValue, passwd, MAX_WEPENCRYPT_KEYVALUE_LEN-1);
            strncpy(pstWifiCfg->wirelessEncrypt.wpaEncrypt.encryptType, wpaEncryType, MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN-1);
            strncpy(pstWifiCfg->wirelessEncrypt.wpaEncrypt.authMode, wpaAuthMode, MAX_WPAENCRYPT_AUTHMODE_NAME_LEN-1);
            strncpy(pstWifiCfg->wirelessEncrypt.wpaEncrypt.keyValue,passwd, MAX_WPAENCRYPT_KEYVALUE_LEN-1);

            anj_config_network_wifi_set(pstWifiCfg);
            __ERR("set wifi config success\n");

            if(g_ResponseFunc!= NULL)
            {
                g_ResponseFunc(pInst, "", SOAP_FILE);
            }

            return ret;
        }
    }

    return http_send_file_cb(pInst, pmsgt, "f404.html", "text/html");
}

int http_put_proc(void *pInst, 
                    const char *szURLFullname,
                    const char *szMsgBuffer,
                    const char* szMsgBody, 
                    const char *clientip,
                    const char* host,
                    cb_func_http_response pCbResponse)
{
    __INFO("http put handle! clientip:%s, host:%s, http url:%s!\n", (clientip != NULL) ? clientip : "", host, szURLFullname);

    // unv处理
    int unv_enable = unv_enable_get();
    if (unv_enable)
    {
        if(strstr(szURLFullname, UNV_LAPI_CHANNELS) || strstr(szURLFullname, UNV_LAPI_SYSTEM))
        {
            return unv_handle_put_request((void *)pInst, szURLFullname, szMsgBody, szMsgBuffer, pCbResponse);
        }
    }

    // HAPI处理
    if(0 == http_hapi_handle(pInst, HTTP_PUT, szURLFullname, szMsgBody, clientip, host))
    {
        return 0;
    }

    return -1;
}

int http_post_proc(void *pInst, 
                        void *pmsgt,
                        const char *szURLFullname,
                        const char *szMsgBuffer, 
                        const char* szMsgBody, 
                        const char *clientip, 
                        const char* host, 
                        cb_func_http_response pCbResponse)
{
    __INFO("http post handle! clientip:%s, host:%s, http url:%s!\n", (clientip != NULL) ? clientip : "", host, szURLFullname);

    // unv处理
    int unv_enable = unv_enable_get();
    if (unv_enable && strncmp(szURLFullname, UNV_LAPI_SYSTEM_EVENT_SUB, strlen(UNV_LAPI_SYSTEM_EVENT_SUB)) == 0)
    {
        if(strncmp(szMsgBuffer, HTTP_POST, strlen(HTTP_POST)) != 0)
        {
            return 0;
        }

        return unv_handle_put_request((void *)pInst, szURLFullname, szMsgBody, szMsgBuffer, pCbResponse);
    }

    // HAPI处理
    if(0 == http_hapi_handle(pInst, HTTP_POST, szURLFullname, szMsgBody, clientip, host))
    {
        return 0;
    }

    // 以下为IPC WEB网页处理
    char uripath[256] = {0};
    if(szURLFullname[0] == '/')     // 跳过/
    {
        strncpy(uripath, szURLFullname + 1, sizeof(uripath) - 1);
    }
    else
    {
        strncpy(uripath, szURLFullname, sizeof(uripath)-1);
    }

    char *p = strchr(uripath, '?'); // 去掉path前面的 ?，保留后面的内容
    if (p)
    {
        *p = 0;
    }

    //-----响应结果-----
    /**
        解决原有HTTP POST响应方式可能会出现HTTP header中Content-Length与实际的body length不一致的问题
    */
    int status = 0;
    char *xmlBuf = web_post_handle(szMsgBody, uripath);
    if(NULL != xmlBuf)
    {
        if(pCbResponse != NULL)
        {
            status = pCbResponse(pInst, xmlBuf, SOAP_FILE);
        }
        anj_mw_free(xmlBuf);
    }
    else
    {
        if(pCbResponse != NULL)
        {
            status = pCbResponse(pInst, "", SOAP_FILE);
        }
    }        

    return status;
}

void http_cb_init(cb_func_http_response cbResponse, cb_func_http_sendfile cbGetFile)
{
    g_ResponseFunc = cbResponse;
    g_GetFileFunc = cbGetFile;
}

void http_alarm_cability_init()
{
    int unv_smart_support = unv_smart_support_get();

    if (anj_sysctl_capability_check(FUNCTION_ALARM_PD) ||
        anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_CAR) ||
        anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_MOTO) ||
        anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_ELECTRICBICYCLE) ||
        anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_BICYCLE))
    {
        s_http_alarm_ability.smart_motion = 1;
    }

    if (anj_sysctl_capability_check(FUNCTION_ALARM_PD))
    {
        s_http_alarm_ability.human_enable = 1;
    }

    if (anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_CAR))
    {
        s_http_alarm_ability.car_enable = 1;
    }

    if (anj_sysctl_capability_check(FUNCTION_ALARM_VIDEOGATE_BY_PD) && unv_smart_support)
    {
        s_http_alarm_ability.video_gate = 1;
    }

    if (anj_sysctl_capability_check(FUNCTION_ALARM_REGION_AI) && unv_smart_support)
    {
        s_http_alarm_ability.region_enable = 1;
    }

    if (anj_sysctl_capability_check(FUNCTION_FACE_FD) && unv_smart_support)
    {
        s_http_alarm_ability.face_detect = 1;
    }

    int input_cnt = anj_mw_hwctrl_alarmin_port_count_get();
    if (input_cnt > 0)
    {
        s_http_alarm_ability.io_alarm = 1;
    }

    if (anj_sysctl_capability_check(FUNCTION_ALARM_COVER))
    {
        s_http_alarm_ability.tamper_enable = 1;
    }

    if (anj_sysctl_capability_check(FUNCTION_PD_TRACK_HUMAN) && unv_smart_support)
    {
        s_http_alarm_ability.auto_track = 1;
    }

    __INFO("http alarm cability: smart:%d, human:%d, car:%d, vg:%d, region:%d, face:%d, tamper:%d, io:%d, track:%d\n",
        s_http_alarm_ability.smart_motion,
        s_http_alarm_ability.human_enable,
        s_http_alarm_ability.car_enable,
        s_http_alarm_ability.video_gate,
        s_http_alarm_ability.region_enable,
        s_http_alarm_ability.face_detect,
        s_http_alarm_ability.tamper_enable,
        s_http_alarm_ability.io_alarm,
        s_http_alarm_ability.auto_track);

    return;
}

int http_audio_alarm_file_init()
{
    file_query_result *pResult = (file_query_result *)anj_mw_malloc(sizeof(file_query_result));
    if (pResult == NULL)
    {
        __ERR("pResult malloc failed!\n");
        return -1;
    }
    else
    {
        memset(pResult, 0, sizeof(file_query_result));
    }

    int skipcount = 0;
    int pagesize = 15;

    int total_file_count = anj_audio_mp3_file_list_query(pResult, skipcount, pagesize);
    g_stAudioFileList.Num = 0;

    int index = 0;
    int id = 0;
    for (index = 0; index < pResult->count; index++)
    {
        const char *filename = strGetFilename(pResult->file_info[index].filepath);
        if (filename != NULL)
        {
            g_stAudioFileList.Item[id].ID = id;
            StrCpy(g_stAudioFileList.Item[id].file_pathname, sizeof(g_stAudioFileList.Item[id].file_pathname), pResult->file_info[index].filepath);
            StrCpy(g_stAudioFileList.Item[id].file_onlyname, sizeof(g_stAudioFileList.Item[id].file_onlyname), filename);
            id++;
        }
    }

    g_stAudioFileList.Num = id;
    __INFO("http alarm audio file total count:%d\n", total_file_count);

    anj_mw_free(pResult);
    pResult = NULL;

    return 0;
}

int http_handle_init(cb_func_http_response cbResponse, cb_func_http_sendfile cbGetFile)
{
    http_cb_init(cbResponse, cbGetFile);
    http_alarm_cability_init();
    http_audio_alarm_file_init();

    return 0;
}
