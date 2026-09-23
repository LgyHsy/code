#include "anj_mw_comm.h"
#include "anj_comm.h"
#include "anj_config.h"
#include <string.h>
#include "anj_ser.h"
#include "anj_audio.h"
#include "anj_net.h"
#include "anj_net_provider.h"
#include "anj_bind.h"

#include "eventhub.h"
#include "anj_module.h"

#define BIND_FILE_PATH DATA_BLOCK_MOUNT_PATH "/bind.xml"

#define BIND_CHECK_INTERVAL         (50 * 1000)
#define BIND_STATUS_CHECK_INTERVAL  (5 * 20)        // 绑定状态检查间隔5s
#define BIND_STATUS_CHECK_MAX_COUNT (40 * 20)       // 绑定状态检查最大次数

static anj_thread_s s_stBindThread = {0};
static pthread_mutex_t s_stBindMutex = PTHREAD_MUTEX_INITIALIZER;

static int s_stBindFlag = 0;
static unsigned long long s_stBindStartTime = 0;
static IOTBindConfig s_stIotBindCfg;
static int s_stBindInit = 0;

static void anj_bind_ble_p2p_ok(const char *devid)
{
    if (devid)
        anj_net_provider_ble_p2p_ok((char *)devid);
}

static void anj_bind_ble_recv_config(void)
{
    anj_net_provider_ble_recv_config();
}

char *anj_bind_conver_xml(IOTBindConfig *pstIotBindCfg)
{
    int maxSize = 2048;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<BindConfig\r\n");
    pb += snprintf(pb, pe - pb, "bindType=\"%d\"\r\n", pstIotBindCfg->bindType);
    pb += snprintf(pb, pe - pb, "router_ssid=\"%s\"\r\n", pstIotBindCfg->router_ssid);
    pb += snprintf(pb, pe - pb, "router_passwd=\"%s\"\r\n", pstIotBindCfg->router_passwd);
    pb += snprintf(pb, pe - pb, "match_code=\"%s\"\r\n", pstIotBindCfg->match_code);
    pb += snprintf(pb, pe - pb, "owner_user=\"%s\"\r\n", pstIotBindCfg->owner_user);
    pb += snprintf(pb, pe - pb, "client_code=\"%s\"\r\n", pstIotBindCfg->client_code);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

static int anj_bind_cfg_set(IOTBindConfig *pstIotBindCfg)
{
    int iRet = 0;
    ANJ_CHK((pstIotBindCfg != NULL), -1, "input Invalid");

    anj_mutex_lock(&s_stBindMutex);
    char *pDataXml = anj_bind_conver_xml(pstIotBindCfg);
    if (pDataXml)
    {
        anj_mw_write_file(BIND_FILE_PATH, 0, pDataXml, strlen(pDataXml));
        anj_mw_free(pDataXml);
    }
    anj_mutex_unlock(&s_stBindMutex);

endFunc:
    return iRet;
}

static int anj_bind_cfg_get()
{
    int iRet = 0;
    char *pCfgXml = anj_mw_read_file_buffer(BIND_FILE_PATH);
    ANJ_CHK(pCfgXml != NULL, -1, "read failed");

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "BindConfig");
    if (pNodelist)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        if (pNode)
        {
            IXML_Node *tmpAttr = pNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "bindType"))
                {
                    s_stIotBindCfg.bindType = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "router_ssid"))
                {
                    StrCpy(s_stIotBindCfg.router_ssid, sizeof(s_stIotBindCfg.router_ssid), tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "router_passwd"))
                {
                    StrCpy(s_stIotBindCfg.router_passwd, sizeof(s_stIotBindCfg.router_passwd), tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "match_code"))
                {
                    StrCpy(s_stIotBindCfg.match_code, sizeof(s_stIotBindCfg.match_code), tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "owner_user"))
                {
                    StrCpy(s_stIotBindCfg.owner_user, sizeof(s_stIotBindCfg.owner_user), tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "client_code"))
                {
                    StrCpy(s_stIotBindCfg.client_code, sizeof(s_stIotBindCfg.client_code), tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }
            ixmlDocument_free(pDocNode);
        }
        ixmlNodeList_free(pNodelist);
    }

endFunc:
    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
    return iRet;
}

static int anj_bind_data_get(const char *szInfo, int nLen, const char *szDataChar, char *szBindData, int bindBufSize)
{
    int iRet = 0;
    ANJ_CHK(((szInfo != NULL) && (nLen > 0)), -1, "input Invalid");
    ANJ_CHK(((szDataChar != NULL) && (szBindData != NULL) && (bindBufSize > 0)), -1, "input Invalid");

    int pattern_len = strlen(szDataChar);
    if (pattern_len <= 0 || pattern_len > nLen)
    {
        iRet = -1;
        goto endFunc;
    }

    const char *pStartData = NULL;
    for (int i = 0; i <= nLen - pattern_len; i++)
    {
        if (szInfo[i] == szDataChar[0])
        {
            if (memcmp(szInfo + i, szDataChar, pattern_len) == 0)
            {
                pStartData = szInfo + i;
                break;
            }
        }
    }
    if (pStartData == NULL)
    {
        iRet = -1;
        goto endFunc;
    }
    pStartData += pattern_len;

    /* 在剩余长度内查找结束符 ';' */
    int remain = (int)(szInfo + nLen - pStartData);
    if (remain <= 0)
    {
        iRet = -1;
        goto endFunc;
    }
    const char *pEndData = memchr(pStartData, BIND_DATA_END_CHAR[0], remain);
    if (pEndData)
    {
        int len = pEndData - pStartData;
        int copylen = (len < (bindBufSize - 1)) ? len : (bindBufSize - 1);
        memcpy(szBindData, pStartData, copylen);
        szBindData[copylen] = '\0';
    }
    else
    {
        int copylen = (remain < (bindBufSize - 1)) ? remain : (bindBufSize - 1);
        memcpy(szBindData, pStartData, copylen);
        szBindData[copylen] = '\0';
    }
endFunc:
    return iRet;
}

static int anj_bind_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    anj_ser_info *pstSerInfo = getSerInfo();
    __INFO("anj_bind_thread start\n");

    int unbind_status = 0;
    int bind_check_count = 0;

    while (bStart && *bStart)
    {
        if (pstSerInfo->stP2pLoginState.logined)
        {
            if (anj_mw_file_exists(P2P_DEVICEBIND_FLAG))
            {
            }
            else
            {
                anj_mutex_lock(&s_stBindMutex);
                if (s_stBindFlag)
                {
                    if (unbind_status == 0)
                    {
                        if (bind_check_count >= BIND_STATUS_CHECK_MAX_COUNT)
                        {
                            /* 40s 仍查不到 0：清残留后走 anj_ser_bind，不提前给 APP 发 p2p ok */
                            __ERR("bind status timeout, start anj_ser_bind\n");
                            anj_ser_remove_unbind_device();
                            unbind_status = 1;
                        }
                        else if ((bind_check_count % BIND_STATUS_CHECK_INTERVAL) == 0)
                        {
                            int bBind = anj_ser_bind_status_get();
                            __INFO("bind ing, status:%d\n", bBind);

                            if (bBind == 0)
                            {
                                anj_mutex_unlock(&s_stBindMutex);
                                anj_bind_success();
                                anj_mutex_lock(&s_stBindMutex);
                            }
                            else if (bBind == 1)
                            {
                                __ERR("ubind device!\n");
                                anj_ser_simple_unbind();
                            }
                        }

                        bind_check_count++;
                    }

                    if (unbind_status == 1)
                    {
                        __INFO("bind ing start!!\n");
                        bind_check_count = 0;
                        int i = 0;
                        int bind_ok = 0;
                        char owner_user[sizeof(s_stIotBindCfg.owner_user)] = {0};
                        char client_code[sizeof(s_stIotBindCfg.client_code)] = {0};

                        snprintf(owner_user, sizeof(owner_user), "%s", s_stIotBindCfg.owner_user);
                        snprintf(client_code, sizeof(client_code), "%s", s_stIotBindCfg.client_code);
                        anj_mutex_unlock(&s_stBindMutex);

                        for (i = 0; i < 3; i++)
                        {
                            iRet = anj_ser_bind(owner_user, client_code);
                            if (iRet == 0)
                            {
                                bind_ok = 1;
                                break;
                            }
                        }

                        if (bind_ok)
                        {
                            anj_bind_success();
                            anj_mutex_lock(&s_stBindMutex);
                        }
                        else
                        {
                            anj_mutex_lock(&s_stBindMutex);
                            s_stIotBindCfg.bindType = BIND_TYPE_NONE;
                            s_stBindFlag = 0;
                        }
                    }

                }
                anj_mutex_unlock(&s_stBindMutex);
            }
        }

        // 接收绑定的标志位 2分钟没有绑定成功 绑定标志位置0
        anj_mutex_lock(&s_stBindMutex);
        if (s_stBindStartTime > 0 && anj_mw_get_cputime_ms(NULL) - s_stBindStartTime > 2 * 60 * 1000)
        {
            __ERR("bind failed!!\n");
            anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_BIND_FAIL, 1);
            s_stBindStartTime = 0;
            s_stBindFlag = 0;

            unbind_status = 0;
            bind_check_count = 0;
        }
        anj_mutex_unlock(&s_stBindMutex);

        usleep(BIND_CHECK_INTERVAL);
    }
    return iRet;
}

int anj_bind_init()
{
    if (s_stBindInit)
	{
	    __ERR("had been init!\n");
		return 0;
	}
    anj_bind_cfg_get();
    s_stBindThread.bAutoDestroy = 1;
    strncpy(s_stBindThread.iThreadName, "bind_thread", sizeof(s_stBindThread.iThreadName) - 1);
    s_stBindThread.iThreadjob.ctx = (void *)&s_stBindThread;
    s_stBindThread.iThreadjob.func = anj_bind_thread;
    anj_thread_task_create(&s_stBindThread);
    s_stBindInit = 1;
    return 0;
}

int anj_bind_uninit()
{
    if (s_stBindInit == 0)
	{
	    __ERR("not init!\n");
		return 0;
	}
    anj_thread_task_destroy(&s_stBindThread, -1);
    s_stBindInit = 0;
    return 0;
}

/*****************************************************************************
 函 数 名  : anj_bind_data_proc
 功能描述  : 解析数据绑定
 输入参数  : 无
 输出参数  : NULL
 返 回 值  : 小于0 失败，0成功
*****************************************************************************/
int anj_bind_data_proc(const char *szInfo, int nLen, BIND_TYPE BindType)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stBindInit), iRet, "not init");
    ANJ_CHK(((szInfo != NULL) && (nLen > 0)), -1, "input Invalid");
    char szBindData[64] = {0};

    anj_mutex_lock(&s_stBindMutex);
    if (s_stBindFlag == 0)
    {
        s_stIotBindCfg.bindType = BindType;
        if (0 == anj_bind_data_get(szInfo, nLen, BIND_DATA_TEST_CHAR, szBindData, sizeof(szBindData)))
        {
            // TEST
        }

        memset(szBindData, 0, sizeof(szBindData));
        if (0 == anj_bind_data_get(szInfo, nLen, BIND_DATA_WIFI_PASSWORD, szBindData, sizeof(szBindData)))
        {
            strncpy(s_stIotBindCfg.router_passwd, szBindData, sizeof(s_stIotBindCfg.router_passwd));
        }
        memset(szBindData, 0, sizeof(szBindData));
        if (0 == anj_bind_data_get(szInfo, nLen, BIND_DATA_WIFI_SSID, szBindData, sizeof(szBindData)))
        {
            strncpy(s_stIotBindCfg.router_ssid, szBindData, sizeof(s_stIotBindCfg.router_ssid));
        }
        memset(szBindData, 0, sizeof(szBindData));
        if (0 == anj_bind_data_get(szInfo, nLen, BIND_DATA_MATCH_CODE, szBindData, sizeof(szBindData)))
        {
            strncpy(s_stIotBindCfg.match_code, szBindData, sizeof(s_stIotBindCfg.match_code));
        }
        memset(szBindData, 0, sizeof(szBindData));
        if (0 == anj_bind_data_get(szInfo, nLen, BIND_DATA_USER_NAME, szBindData, sizeof(szBindData)))
        {
            strncpy(s_stIotBindCfg.owner_user, szBindData, sizeof(s_stIotBindCfg.owner_user));
        }
        memset(szBindData, 0, sizeof(szBindData));
        if (0 == anj_bind_data_get(szInfo, nLen, BIND_DATA_CLIENT_CODE, szBindData, sizeof(szBindData)))
        {
            strncpy(s_stIotBindCfg.client_code, szBindData, sizeof(s_stIotBindCfg.client_code));
        }
        if (ANJ_NET_STATUS_WIFI == anj_net_status_check())
        {
            if ((strlen(s_stIotBindCfg.router_ssid) > 0))
            {
                NetworkConfigNew *pstNetWorkConfig = (NetworkConfigNew *)getNetWorkConfig();
                WIFIConfig *pstWifiConfig = &pstNetWorkConfig->wifiCfg;

                // 接受绑定信息 先暂停wifi线程 配置好后恢复
                int wifi_thread_status = THREAD_STATUS_PAUSE;
                EventResult event_result = {0};
                eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_WIFI_CONNECT_SET, &event_result, (void *)&wifi_thread_status);

                anj_audio_prompt_play(ANJ_MP3_DEFAULT_PATH, ANJ_MP3_DI_DI, 1);
                sleep(1);
                anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_WIFI_CONFIG_RECEIVED, 1);

                pstWifiConfig->enable = 1;
                pstWifiConfig->dhcpEnable = 1;
                snprintf(pstWifiConfig->essid, sizeof(pstWifiConfig->essid), "%s", s_stIotBindCfg.router_ssid);
                snprintf(pstWifiConfig->wirelessEncrypt.wpaEncrypt.keyValue,
                         sizeof(pstWifiConfig->wirelessEncrypt.wpaEncrypt.keyValue), "%s", s_stIotBindCfg.router_passwd);

                anj_bind_ble_recv_config();

                wifi_thread_status = THREAD_STATUS_RUNNING;
                eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_WIFI_CONNECT_SET, &event_result, (void *)&wifi_thread_status);
                s_stBindFlag = 1;
                s_stBindStartTime = anj_mw_get_cputime_ms(NULL);
                __INFO("START BIND! NowTime:%llu!\n", s_stBindStartTime);
            }
            else
            {
                anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_CONFIG_INVALID, 1);
            }
        }
        if (ANJ_NET_STATUS_4G == anj_net_status_check())
        {
            if ((strlen(s_stIotBindCfg.client_code) > 0) &&
                (strlen(s_stIotBindCfg.owner_user) > 0))
            {
                s_stBindFlag = 1;
                s_stBindStartTime = anj_mw_get_cputime_ms(NULL);
                anj_audio_prompt_play(ANJ_MP3_DEFAULT_PATH, ANJ_MP3_DI_DI, 1);
                __INFO("START BIND! NowTime:%llu!\n", s_stBindStartTime);
            }
            else
            {
                anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_CONFIG_INVALID, 1);
            }
        }
    }
    anj_mutex_unlock(&s_stBindMutex);

endFunc:
    return iRet;
}

void anj_bind_set(int status)
{
    if (s_stBindInit == 0)
	{
		return ;
	}
    anj_mutex_lock(&s_stBindMutex);
    s_stBindFlag = status;
    anj_mutex_unlock(&s_stBindMutex);
}

int anj_bind_get()
{
    int bBind = 0;
    if (s_stBindInit == 0)
	{
		return bBind;
	}
    anj_mutex_lock(&s_stBindMutex);
    if (s_stBindFlag || anj_mw_file_exists(P2P_DEVICEBIND_FLAG))
    {
        bBind = 1;
    }
    anj_mutex_unlock(&s_stBindMutex);
    return bBind;
}

void anj_bind_success()
{
    if (s_stBindInit == 0)
	{
		return ;
	}
    if (anj_mw_file_exists(P2P_DEVICEBIND_FLAG))
    {
        return;
    }
    __INFO("bind suc!!\n");
    anj_mutex_lock(&s_stBindMutex);
    s_stBindStartTime = 0;
    s_stBindFlag = 0;
    anj_mutex_unlock(&s_stBindMutex);

    anj_bind_cfg_set(&s_stIotBindCfg);

    // audio play
    anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_BIND_SUCCESS, 1);

    anj_mw_create_file(P2P_DEVICEBIND_FLAG, NULL);

    /*等待音频播放完成,才销毁音频通道重建*/
    sleep(2);
    anj_ser_info *pstSerInfo = getSerInfo();
    anj_bind_ble_p2p_ok(pstSerInfo->stP2pLoginState.devid);
    int start = 0;
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_WAVE_SET_STATUS, &event_result, (int *)&start);
    eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_ZXING_SET_STATUS, &event_result, (int *)&start);
}

void anj_bind_cfg_clean()
{
    if (s_stBindInit == 0)
	{
		return ;
	}
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    anj_mutex_lock(&s_stBindMutex);
    memset(&s_stIotBindCfg, 0, sizeof(s_stIotBindCfg));
    memset(pstNetworkConfig->wifiCfg.essid, 0, sizeof(pstNetworkConfig->wifiCfg.essid));
    anj_mutex_unlock(&s_stBindMutex);
}

IOTBindConfig *getBindInfo()
{
    return &s_stIotBindCfg;
}

REGISTER_MODULE(anj_bind, MODULE_PRIORITY_BIND);
