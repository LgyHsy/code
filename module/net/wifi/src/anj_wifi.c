#include "anj_net.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_audio.h"
#include "anj_ser.h"
#include "anj_bind.h"
#include "eventhub.h"
#include "anj_comm.h"
#include "anj_mw_comm.h"
#include "anj_mw_net.h"

#include "function_list.h"
#include "anj_sysctl.h"
#include "anj_wifi.h"
#include "anj_net_provider.h"
#include "anj_audio.h"
#include "record_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define Freq5GToChannel(freq)                                           \
    ((freq) == 5180000000.000000                                  ? 36  \
     : (freq) == 5200000000.000000                                ? 40  \
     : (freq) == 5220000000.000000                                ? 44  \
     : (freq) == 5240000000.000000                                ? 48  \
     : (freq) == 5260000000.000000                                ? 52  \
     : (freq) == 5280000000.000000                                ? 56  \
     : (freq) == 5300000000.000000                                ? 60  \
     : (freq) == 5320000000.000000                                ? 64  \
     : ((freq) > 5340000000.000000 && (freq) < 5480000000.000000) ? 68  \
     : (freq) == 5500000000.000000                                ? 100 \
     : (freq) == 5520000000.000000                                ? 104 \
     : (freq) == 5540000000.000000                                ? 108 \
     : (freq) == 5560000000.000000                                ? 112 \
     : (freq) == 5580000000.000000                                ? 116 \
     : (freq) == 5600000000.000000                                ? 120 \
     : (freq) == 5620000000.000000                                ? 124 \
     : (freq) == 5640000000.000000                                ? 128 \
     : (freq) == 5660000000.000000                                ? 132 \
     : (freq) == 5680000000.000000                                ? 136 \
     : (freq) == 5700000000.000000                                ? 140 \
     : (freq) == 5745000000.000000                                ? 149 \
     : (freq) == 5765000000.000000                                ? 153 \
     : (freq) == 5785000000.000000                                ? 157 \
     : (freq) == 5805000000.000000                                ? 161 \
     : (freq) == 5825000000.000000                                ? 165 \
                                                                  : 165)

#define MAX_SIGNAL_LEVEL_DIFF (10)

#define MAX_WPA_NET_BUFF_SIZE (512)
#define MAX_WPA_BUFF_SIZE (1024)
#define WPA_NORMAL_OVERTIME (30)  // 普通连接超时时间
#define WPA_GBK_OVERTIME (60)     // GBK连接超时时间
#define WPA_NO_SCAN_OVERTIME (20) // 不连接隐藏WIFI的超时时间

#define WIFI_CHECK_BEST_SIGNAL_TIME (2 * 60) /*每隔2分钟判断是否最优WIFI*/
#define WIFI_CHECK_SIGNAL_TIME (30)          /*每隔30s检测一次WIFI信号强度*/
#define WIFI_SIGNAL_LEVEL3 (-56)             /*3格WIFI信号强度*/
#define WIFI_SIGNAL_LEVEL2 (-70)             /*2格WIFI信号强度*/
#define WIFI_SIGNAL_LEVEL1 (-85)             /*1格WIFI信号强度*/

#define WIFI_THREAD_SLEEP (1)
#define WIFI_WAIT_CONFIG_CNT (10)                          /*最多播放10次等待提示音*/
#define WIFI_WAIT_CONFIG_INTERVAL (35 / WIFI_THREAD_SLEEP) /*每35秒播放一次等待提示音*/

#define WIFI_MTU (1500)
#define AP_MODE_DEFAULT_IPx "192.168.1"

// ========== 基础配置元素 ==========
#define WPA_HEADER "ctrl_interface=/tmp/wpa_supplicant\n"
#define WPA_FOOTER "update_config=1\n"
#define BLOCK_START "network={\n"
#define BLOCK_END "}\n"

// 根据 WIFI_DEV_8188 定义 HT 能力参数
#if WIFI_DEV_8188
#define WIFI_CAPAB_CONFIG "ht_capab=[SHORT-GI-20][SHORT-GI-40][HT40-]\n"
#else
#define WIFI_CAPAB_CONFIG ""
#endif

static const char *AUTH_OPEN(int scan)
{
    return scan ? "scan_ssid=1\nkey_mgmt=NONE" : "key_mgmt=NONE";
}

static const char *AUTH_WEP(const char *key, int scan)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "%s\nwep_key0=%s\nwep_tx_keyidx=0", AUTH_OPEN(scan), key);
    return buf;
}

static const char *AUTH_WPA(const char *psk, int scan)
{
    static char buf[256];
    char psk_esc[128] = {0};
    const char *scan_part = scan ? "scan_ssid=1\n" : "";

    copy_with_quoted_escape(psk_esc, sizeof(psk_esc), psk);
    snprintf(buf, sizeof(buf), "%spairwise=CCMP TKIP\ngroup=CCMP TKIP\npsk=\"%s\"", scan_part, psk_esc);
    return buf;
}

// 定义公共的 hostapd 配置基础
#define HOSTAPD_COMMON_BASE         \
    "interface=wlan0\n"             \
    "driver=nl80211\n"              \
    "ctrl_interface=/tmp/hostapd\n" \
    "ssid=%s\n"                     \
    "channel=6\n"                   \
    "hw_mode=g\n"                   \
    "ieee80211n=1\n" WIFI_CAPAB_CONFIG

// 完整的 hostapd 配置模板
static const char *hostapd_open_config_template =
    HOSTAPD_COMMON_BASE;

static const char *hostapd_secure_config_template =
    HOSTAPD_COMMON_BASE
    "wpa=1\n"
    "wpa_passphrase=%s\n"
    "wpa_key_mgmt=WPA-PSK\n"
    "wpa_pairwise=TKIP CCMP\n";

typedef struct
{
    const char *ssid;       // 网络名称
    const char *credential; // 网络凭据（WEP密钥或WPA-PSK）
    int scan_ssid;          // 是否隐藏网络（1=是，0=否）
    const char *auth_type;  // 认证类型（OPEN/WEP/WPA）
} wpa_config;

typedef struct
{
    int bInit;
    int wifiMode;
    int wifiStatus; // WIFI状态
    int bAudioPlay;
    int audioWaitCnt;
    int audioWaitInterval;
    THREAD_RUN_STATUS thread_pause_flag;
} WIFI_PARAM;

typedef struct
{
    char *vendor_id;
    char *prod_id;
    int  dri_type;
}WIFI_MODULE_ENTRY;

static WIFI_MODULE_ENTRY s_stWifiModuleList[]=
{
    {"148f",    "760",  WIFI_TYPE_MT7601},
    {"148f",    "307",  WIFI_TYPE_RT3070},
    {"0e8d",    "7612", WIFI_TYPE_MT7612},
    {"007a",    "8888", WIFI_TYPE_ATBM603X},
    {"1b20",    "8888", WIFI_TYPE_SSW101X},
    {"8065",    "6000", WIFI_TYPE_SSW105X},
    {"a69c",    "88dc", WIFI_TYPE_AIC8800},
    {"a012",    "8000", WIFI_TYPE_TXW901},
    {NULL,      NULL,   0}
};

static int s_WifiModuleType = 0;
static WIFI_PARAM s_stWifiParam;
static anj_thread_s s_stWifiThread;
static anj_thread_s s_stWifiCheckThread;
static pthread_mutex_t s_stWifiMutex = PTHREAD_MUTEX_INITIALIZER;

static void anj_wifi_connect_mode_set_ops(int mode)
{
    anj_wifi_connect_mode_set(mode);
}

static int anj_wifi_generate_wpa_config_ops(void *pstWifiCfg, int wpaCfgType)
{
    return anj_wifi_generate_wpa_config((WIFIConfig *)pstWifiCfg, wpaCfgType);
}

static int anj_wifi_start_wpa_ops(int iOverTime, void *pstWifiCfg)
{
    return anj_wifi_start_wpa(iOverTime, (WIFIConfig *)pstWifiCfg);
}

static const anj_net_wifi_ops s_stWifiOps = {
    .init = anj_wifi_init,
    .uninit = anj_wifi_uninit,
    .status_get = anj_wifi_status_get,
    .thread_set = anj_wifi_thread_set,
    .info_get = anj_wifi_info_get,
    .ap_info_get = anj_wifi_ap_info_get,
    .connect_mode_set = anj_wifi_connect_mode_set_ops,
    .generate_wpa_config = anj_wifi_generate_wpa_config_ops,
    .start_wpa = anj_wifi_start_wpa_ops,
};

ANJ_LINK_KEEP(anj_keep_net_wifi_provider);

__attribute__((constructor)) static void anj_wifi_provider_register(void)
{
    anj_net_wifi_provider_register(&s_stWifiOps);
}

__attribute__((destructor)) static void anj_wifi_provider_unregister(void)
{
    anj_net_wifi_provider_unregister(&s_stWifiOps);
}

void anj_wifi_thread_set(int status)
{
    if (!s_stWifiParam.bInit)
    {
        __ERR("Invalid Input\n");
        return;
    }

    anj_mutex_lock(&s_stWifiMutex);

    __INFO("thread_pause_flag = %d\n", status);
    s_stWifiParam.thread_pause_flag = status;

    anj_mutex_unlock(&s_stWifiMutex);
}

int anj_wifi_thread_flag_get()
{
    int flag = 0;
    anj_mutex_lock(&s_stWifiMutex);
    flag = s_stWifiParam.thread_pause_flag;
    anj_mutex_unlock(&s_stWifiMutex);

    return flag;
}

void anj_wifi_connect_mode_set(int mode)
{
    if (!s_stWifiParam.bInit)
    {
        __ERR("Invalid Input\n");
        return;
    }

    anj_mutex_lock(&s_stWifiMutex);

    if (s_stWifiParam.wifiMode != mode)
    {
        __INFO("wifiMode:%d change to:%d\n", s_stWifiParam.wifiMode, mode);
        s_stWifiParam.wifiMode = mode;
    }

    anj_mutex_unlock(&s_stWifiMutex);
}

static void anj_wifi_module_type_check()
{
    int iRet = 0;
    int vendor_index = 0;
    char vendor_str[32] = {0};
    const int max_index = sizeof(s_stWifiModuleList) / sizeof(s_stWifiModuleList[0]);

    for (vendor_index = 0; vendor_index < max_index; vendor_index++)
    {
        if (s_stWifiModuleList[vendor_index].vendor_id == NULL)
        {
            __ERR("wifi index:%d driver don't find!\n", vendor_index);
            s_WifiModuleType = WIFI_TYPE_AIC8800;
            break;
        }
        
        memset(vendor_str, 0, sizeof(vendor_str));
        snprintf(vendor_str, sizeof(vendor_str), "%s:%s", 
            s_stWifiModuleList[vendor_index].vendor_id, s_stWifiModuleList[vendor_index].prod_id);
        
        iRet = SearchStringInCmd("lsusb", vendor_str);
        if (iRet == 1)
        {
            s_WifiModuleType = s_stWifiModuleList[vendor_index].dri_type;
            break;
        }
    }

    __INFO("Device wifi module:%d\n", s_WifiModuleType);
}

static int anj_wifi_quality_get(WIFI_QUALITY *pstInfos)
{
    int skfd = -1;
    int ret = -1;
    struct iw_range range;
    iwstats stats;
    int has_range = 0;
    if (NULL == pstInfos)
    {
        __ERR("Invalid Input\n");
        return -1;
    }
    memset(pstInfos, 0, sizeof(WIFI_QUALITY));
    memset(&stats, 0, sizeof(stats));
    memset(&range, 0, sizeof(range));

    if ((skfd = iw_sockets_open()) < 0)
    {
        __ERR("iw_sockets_open fail\n");
        return -1;
    }

    if (iw_get_range_info(skfd, WIFI_INTERFACE_NAME, &range) >= 0)
    {
        pstInfos->maxquality = range.max_qual.qual;
        has_range = 1;
    }
    else
    {
        pstInfos->maxquality = 1;
        has_range = 0;
    }

    if (iw_get_stats(skfd, WIFI_INTERFACE_NAME, &stats, &range, has_range) >= 0)
    {
        if (has_range)
        {
            int quality = 0;
            int signal_level = 0;
            int noise_level = 0;

            iw_get_link_stats(&stats.qual, &range, has_range, &quality, &signal_level, &noise_level);
            pstInfos->quality = quality;
            pstInfos->signalLevel = signal_level;
            ret = 0;
        }
        else
        {
            pstInfos->quality = stats.qual.qual;
            pstInfos->signalLevel = (stats.qual.level / 2.0) - 110.0;
        }
    }
    else
    {
        ret = -2;
    }
    iw_sockets_close(skfd);
    __INFO("wifi stat ret:%d,range:%d,maxqua:%d,qual:%d,level:%d,update:%d\n", ret, has_range,
           pstInfos->maxquality, pstInfos->quality,
           pstInfos->signalLevel, stats.qual.updated);

    if (pstInfos->signalLevel > 0)
    {
        pstInfos->signalLevel = pstInfos->signalLevel - 100;
    }

    return ret;
}

/*****************************************************************************
 函 数 名  : anj_wifi_mac_get
 功能描述  : 获取连接wifi mac地址
 输入参数  : 无
 输出参数  : NULL
 返 回 值  : 0成功，其他失败
*****************************************************************************/
static int anj_wifi_mac_get(char *mac)
{
    int iRet = 0;
    int skfd = -1;
    ANJ_CHK(mac != NULL, -1, "input Invalid");

    skfd = iw_sockets_open();
    ANJ_CHK(skfd > 0, -1, "iw_sockets_open failed");

    iw_get_mac(skfd, WIFI_INTERFACE_NAME, mac);
    __INFO("wifi mac:%s\n", mac);

endFunc:
    if (skfd > 0)
    {
        iw_sockets_close(skfd);
    }
    return iRet;
}

static void anj_wifi_quality_event_get(EventResult *event_result, void *data)
{
    if (event_result == NULL || data == NULL)
    {
        return;
    }
    WIFI_QUALITY *pstInfos = (WIFI_QUALITY *)data;
    event_result->ret = anj_wifi_quality_get(pstInfos);
}

static void anj_wifi_thread_event_set(EventResult *event_result, void *data)
{
    if (data == NULL)
    {
        return;
    }
    int status = *(int *)data;
    anj_wifi_thread_set(status);
}

/*****************************************************************************
 函 数 名  : anj_wifi_best_ssid_get
 功能描述  : 查找存在的同名WIFI，连接信号最好的那个
 输入参数  : ssid:WIFI名称 gbkssid:WIFI名称
 输出参数  : NULL
 返 回 值  : .0 不存在信号更好的WIFI  大于0 存在信号更好的WIFI
*****************************************************************************/
static int anj_wifi_best_ssid_get(char *ssid)
{
    int iRet = 0;
    ANJ_CHK(ssid != NULL, -1, "input Invalid");

    char mac[20] = {0};
    /*获取当前连接的WIFI信号强度*/
    WIFI_QUALITY stInfos = {0};
    ANJ_CHK_FUNC(anj_wifi_quality_get(&stInfos), 0, "wifi quality get error");

    /*获取当前连接的WIFI MAC*/
    char CurMac[20] = {0};
    ANJ_CHK_FUNC(anj_wifi_mac_get(CurMac), 0, "wifi mac get error");

    int signalLevel = 0;
    WIFI_AP_SCAN apinfos = {0};
    anj_wifi_ap_info_get((void *)&apinfos);

    for (int i = 0; i < apinfos.apCnt; i++)
    {
        if (strcmp(apinfos.apInfos[i].ssid, ssid) == 0)
        {
            __ERR("MAC:%s signalLevel:(%d, %d)\n", apinfos.apInfos[i].mac, stInfos.signalLevel, apinfos.apInfos[i].signalLevel);
            /*同名WIFI找信号最好的那个*/
            if (signalLevel == 0 || (apinfos.apInfos[i].signalLevel > signalLevel))
            {
                strncpy(mac, apinfos.apInfos[i].mac, sizeof(mac) - 1);
                mac[sizeof(mac) - 1] = '\0'; // 确保字符串终止
                signalLevel = apinfos.apInfos[i].signalLevel;
                iRet++;
            }
        }
    }

    if ((iRet > 0) && (signalLevel > (stInfos.signalLevel + MAX_SIGNAL_LEVEL_DIFF)) && (strcmp(mac, CurMac) != 0))
    {
        // char local_ip[16] = {0};
        // net_local_ip(WIFI_INTERFACE_NAME, local_ip, sizeof(local_ip));
        // __RECORD_LOG_INFO("SSID:%s SSID_GBK:%s MAC:%s IP:%s\n",
        //                   ssid, gbkssid, mac, local_ip);
    }
    else
    {
        iRet = 0;
    }

endFunc:
    return iRet;
}
// 生成单网络配置
static void anj_wifi_build_config(char *wpa_buff, int len, wpa_config *networks)
{
    if (wpa_buff == NULL || networks == NULL || networks->ssid == NULL || networks->auth_type == NULL)
    {
        __ERR("Invalid parameters for WPA config generation\n");
        return;
    }
    // 确定安全配置模板
    const char *auth_template = NULL;
    char ssid_esc[128] = {0};

    if (strcmp(networks->auth_type, "OPEN") == 0)
    {
        auth_template = AUTH_OPEN(networks->scan_ssid);
    }
    else if (strcmp(networks->auth_type, "WEP") == 0)
    {
        auth_template = AUTH_WEP(networks->credential, networks->scan_ssid);
    }
    else
    { // WPA/WPA2
        auth_template = AUTH_WPA(networks->credential, networks->scan_ssid);
    }

    copy_with_quoted_escape(ssid_esc, sizeof(ssid_esc), networks->ssid);
    snprintf(wpa_buff, len,
             WPA_HEADER BLOCK_START
             "ssid=\"%s\"\n"
             "%s\n" BLOCK_END
                 WPA_FOOTER,
             ssid_esc, auth_template);
}

static void anj_wifi_process_wpa_config(char *final_config, char *net_config1, char *net_config2)
{
    if (!final_config || !net_config1 || !net_config2)
    {
        __ERR("Invalid configuration pointers\n");
        return;
    }

    // 1. 提取第一个网络块
    char *block_start1 = strstr(net_config1, BLOCK_START);
    char *block_end1 = strstr(block_start1, BLOCK_END);
    if (!block_end1)
        block_end1 = block_start1 + strlen(block_start1);

    // 2. 提取第二个网络块
    char *block_start2 = strstr(net_config2, BLOCK_START);
    char *block_end2 = strstr(block_start2, BLOCK_END);
    if (!block_end2)
        block_end2 = block_start2 + strlen(block_start2);

    // 3. 构建最终配置
    snprintf(final_config, MAX_WPA_BUFF_SIZE,
             WPA_HEADER                                            // 全局头部
             "%.*s"                                                // 网络块1 (不带原始头部)
             "%.*s"                                                // 网络块2 (不带原始头部)
             WPA_FOOTER,                                           // 全局尾部
             (int)(block_end1 - block_start1 + strlen(BLOCK_END)), // 网络块1长度
             block_start1,
             (int)(block_end2 - block_start2 + strlen(BLOCK_END)), // 网络块2长度
             block_start2);
}

// ========== 具体配置生成函数 ==========
static void anj_wifi_build_opencfg(char *wpa_buff, int len, const char *ssid, int scan)
{
    wpa_config networks = {0};
    networks.ssid = ssid;
    networks.scan_ssid = scan;
    networks.auth_type = "OPEN";
    anj_wifi_build_config(wpa_buff, len, &networks);
}

static void anj_wifi_build_webcfg(char *wpa_buff, int len, const char *ssid, const char *key, int scan)
{
    wpa_config networks = {0};
    networks.ssid = ssid;
    networks.scan_ssid = scan;
    networks.auth_type = "WEP";
    networks.credential = key;
    anj_wifi_build_config(wpa_buff, len, &networks);
}

static void anj_wifi_build_wpacfg(char *wpa_buff, int len, const char *ssid, const char *key, int scan)
{
    wpa_config networks = {0};
    networks.ssid = ssid;
    networks.scan_ssid = scan;
    networks.auth_type = "WPA";
    networks.credential = key;
    anj_wifi_build_config(wpa_buff, len, &networks);
}

int anj_wifi_generate_wpa_config(WIFIConfig *pstWifiCfg, int wpaCfgType)
{
    int iRet = 0;
    if (access("/tmp/wifi", F_OK) == 0)
    {
        remove("/tmp/wifi");
        pstWifiCfg->wirelessEncrypt.enable = 1;
        strcpy(pstWifiCfg->wirelessEncrypt.encryptType, "WPA");
        strcpy(pstWifiCfg->wirelessEncrypt.wpaEncrypt.keyValue, "leo123456");
        strcpy(pstWifiCfg->essid, "leo");
    }
    ANJ_CHK((pstWifiCfg != NULL) && (strlen(pstWifiCfg->essid) > 0), -1, "input Invalid");

    char wpa_buff[MAX_WPA_BUFF_SIZE] = {0};
    char wpa_buff1[MAX_WPA_NET_BUFF_SIZE] = {0};
    char wpa_buff2[MAX_WPA_NET_BUFF_SIZE] = {0};
    int scan = (wpaCfgType == WPA_CONFIG_NO_SCAN) ? 0 : 1;

    if (pstWifiCfg->wirelessEncrypt.enable == 0)
    {
        anj_wifi_build_opencfg(wpa_buff1, sizeof(wpa_buff1), pstWifiCfg->essid, scan);
        if (wpaCfgType != WPA_CONFIG_NORMAL)
        {
            anj_wifi_build_opencfg(wpa_buff2, sizeof(wpa_buff2), pstWifiCfg->essid, scan);
            anj_wifi_process_wpa_config(wpa_buff, wpa_buff1, wpa_buff2);
        }
        else
        {
            snprintf(wpa_buff, sizeof(wpa_buff), "%s", wpa_buff1);
        }
    }
    else if (strstr(pstWifiCfg->wirelessEncrypt.encryptType, "WEP"))
    {
        anj_wifi_build_webcfg(wpa_buff1, sizeof(wpa_buff1), pstWifiCfg->essid, pstWifiCfg->wirelessEncrypt.wepEncrypt.keyValue, scan);
        if (wpaCfgType != WPA_CONFIG_NORMAL)
        {
            anj_wifi_build_webcfg(wpa_buff2, sizeof(wpa_buff2), pstWifiCfg->essid, pstWifiCfg->wirelessEncrypt.wepEncrypt.keyValue, scan);
            anj_wifi_process_wpa_config(wpa_buff, wpa_buff1, wpa_buff2);
        }
        else
        {
            snprintf(wpa_buff, sizeof(wpa_buff), "%s", wpa_buff1);
        }
    }
    else
    {
        anj_wifi_build_wpacfg(wpa_buff1, sizeof(wpa_buff1), pstWifiCfg->essid, pstWifiCfg->wirelessEncrypt.wpaEncrypt.keyValue, scan);
        if (wpaCfgType != WPA_CONFIG_NORMAL)
        {
            anj_wifi_build_wpacfg(wpa_buff2, sizeof(wpa_buff2), pstWifiCfg->essid, pstWifiCfg->wirelessEncrypt.wpaEncrypt.keyValue, scan);
            anj_wifi_process_wpa_config(wpa_buff, wpa_buff1, wpa_buff2);
        }
        else
        {
            snprintf(wpa_buff, sizeof(wpa_buff), "%s", wpa_buff1);
        }
    }

    ANJ_CHK((strlen(wpa_buff) > 0), -1, "wpa_buff is empty");

    iRet = anj_mw_write_file(WPA_SUPPLICANT_CONF_PATH, 0, wpa_buff, strlen(wpa_buff));
    if (iRet != strlen(wpa_buff))
    {
        __ERR("wifi config write error\n");
        iRet = -1;
        goto endFunc;
    }
    else
    {
        iRet = 0;
    }

endFunc:
    return iRet;
}

static int anj_wifi_generate_hostapd_config(WIFIApConfig *pstWifiApCfg)
{
    int iRet = 0;
    ANJ_CHK((pstWifiApCfg != NULL) && (strlen(pstWifiApCfg->essid) > 0), -1, "input Invalid");

    const char *template;
    // 确定使用哪种配置模板
    if (strlen(pstWifiApCfg->wirelessEncrypt.wpaEncrypt.keyValue) >= 8)
    {
        __INFO("(strlen(pstrPassword) >= 8)\n");
        template = hostapd_secure_config_template;
    }
    else
    {
        __INFO("(strlen(pstrPassword) < 8)\n");
        template = hostapd_open_config_template;
    }

    // 生成配置
    char hostapd_buff[512] = {0};
    snprintf(hostapd_buff, sizeof(hostapd_buff), template, pstWifiApCfg->essid, pstWifiApCfg->wirelessEncrypt.wpaEncrypt.keyValue);
    ANJ_CHK((strlen(hostapd_buff) > 0), -1, "hostapd_buff is empty");

    iRet = anj_mw_write_file(HOSTAPD_PATH, 0, hostapd_buff, strlen(hostapd_buff));
    if (iRet != strlen(hostapd_buff))
    {
        __ERR("hostapd config write error\n");
        iRet = -1;
        goto endFunc;
    }
    else
    {
        iRet = 0;
    }

endFunc:
    return iRet;
}

static int anj_wifi_ipaddr_set(WIFIConfig *pstWifiCfg)
{
    int iRet = 0;
    ANJ_CHK((pstWifiCfg != NULL), -1, "input Invalid");
    if (pstWifiCfg->dhcpEnable == 0)
    {
        char cmd[256] = {0};
        snprintf(cmd, sizeof(cmd), "killall %s", "udhcpc");
        anj_mw_system(cmd);

        if (strlen(pstWifiCfg->IPAddress) > 0)
        {
            struct in_addr ipaddr, netmask, gateway;
            inet_aton(pstWifiCfg->IPAddress, &ipaddr);
            inet_aton(pstWifiCfg->netMask, &netmask);
            inet_aton(pstWifiCfg->gateWay, &gateway);
            net_set_ifaddr(WIFI_INTERFACE_NAME, ipaddr.s_addr);
            net_set_netmask(WIFI_INTERFACE_NAME, netmask.s_addr);
            net_add_gateway(gateway.s_addr);

            for (int k = 0; k < 10; k++)
            {
                sleep(1);
                __ERR("before wait if: %s up, try  = %d\n", WIFI_INTERFACE_NAME, k);
                if (is_network_interface_up(WIFI_INTERFACE_NAME))
                {
                    __ERR("if: %s is up, try  = %d\n", WIFI_INTERFACE_NAME, k);
                    break;
                }
            }
        }
    }
    else
    {
        anj_net_dhcp_up(WIFI_INTERFACE_NAME);
    }

    anj_net_gateway_load(pstWifiCfg->gateWay);
    net_set_mtu(WIFI_INTERFACE_NAME, WIFI_MTU);

    /*双网卡切换，需要对默认网关进行一次切换，否则会出现搜索到IP无法出图的情况*/
    net_set_down(WIRE_INTERFACE_NAME);
    net_set_up(WIRE_INTERFACE_NAME);

endFunc:
    return iRet;
}

static void anj_wifi_start_hostapd(void)
{
    anj_wifi_destory();
    usleep(50);

    net_set_up(WIFI_INTERFACE_NAME);

    char cmd_str[256] = {0};
    sprintf(cmd_str, "/opt/ch/hostapd /mnt/nand/hostapd.conf &");
    anj_mw_system(cmd_str);

    char wireIp[64] = {0};
    net_local_ip(WIRE_INTERFACE_NAME, wireIp, sizeof(wireIp));
    if (strlen(wireIp) > 0)
    {
        if (strncmp(wireIp, AP_MODE_DEFAULT_IPx, strlen(AP_MODE_DEFAULT_IPx)) == 0)
        {
            memset(cmd_str, 0, sizeof(cmd_str));
            snprintf(cmd_str, sizeof(cmd_str), "route del -net %s.0 netmask 255.255.255.0 dev %s",
                     AP_MODE_DEFAULT_IPx, WIRE_INTERFACE_NAME);
            anj_mw_system(cmd_str);
        }
    }

    memset(cmd_str, 0, sizeof(cmd_str));
    snprintf(cmd_str, sizeof(cmd_str), "ifconfig %s %s.1", WIFI_INTERFACE_NAME, AP_MODE_DEFAULT_IPx);
    anj_mw_system(cmd_str);

    memset(cmd_str, 0, sizeof(cmd_str));
    snprintf(cmd_str, sizeof(cmd_str), "/opt/ch/udhcpd /etc/udhcpd.conf &");
    anj_mw_system(cmd_str);

    return;
}

static int anj_wifi_add_capability()
{
    anj_sysctl_capability_add(FUNCTION_WIRELESS_STATION);
    if (IPC_WIFI_SUPPORT_AP)
    {
        anj_sysctl_capability_add(FUNCTION_WIFI_AP_STATION_SAMETIME);
    }

    anj_sysctl_capability_add(FUNCTION_WIFI_AP);

    return 0;
}

int anj_wifi_start_wpa(int iOverTime, WIFIConfig *pstWifiCfg)
{
    int iRet = -1;
    char *wpa_log_buf = NULL;
    ANJ_CHK((pstWifiCfg != NULL), -1, "input Invalid");

    char cmd_str[256] = {0};
    int count = 0;

    anj_wifi_destory();
    usleep(50);

    net_set_up(WIFI_INTERFACE_NAME);

    snprintf(cmd_str, sizeof(cmd_str), AJ_APP_PATH "/wpa_supplicant -Dnl80211 -i%s -c %s > %s &",
             WIFI_INTERFACE_NAME, WPA_SUPPLICANT_CONF_PATH, WPA_LOG_PATH);
    anj_mw_system(cmd_str);

    /*允许设备有20s的连接AP时间*/
    while (count < iOverTime)
    {
        if (anj_mw_file_exists(WPA_LOG_PATH) == 0)
        {
            __ERR("%s is not exited\n", WPA_LOG_PATH);
        }
        else
        {
            wpa_log_buf = anj_mw_read_file_buffer(WPA_LOG_PATH);
            if (wpa_log_buf == NULL)
            {
                count++;
                sleep(1);
                continue;
            }

            if (strstr(wpa_log_buf, WPA_SUPPLICANT_CONNECT_SUCCESSFUL))
            {
                __INFO("WPA SUC!\n");
                iRet = 0;
                ANJ_CHK_FUNC(anj_wifi_ipaddr_set(pstWifiCfg), 0, "ipaddr set err");

                sleep(1);
                anj_wifi_status_set(WIFI_STATUS_CONNECTED);
                break;
            }
            else if (strstr(wpa_log_buf, WPA_SUPPLICANT_WRONG_PASSWD))
            {
                iRet = 0;
                __ERR("WPA password error, please check your configuration.\n");
                anj_wifi_status_set(WIFI_STATUS_PASSWORD_ERROR);
                break;
            }
            else
            {
                anj_wifi_status_set(WIFI_STATUS_CONNECTING);
            }

            anj_mw_free(wpa_log_buf);
            wpa_log_buf = NULL;
        }

        count++;
        sleep(1);
    }

endFunc:
    if (anj_mw_file_exists(WPA_LOG_PATH))
    {
        remove(WPA_LOG_PATH);
    }

    if (wpa_log_buf != NULL)
    {
        anj_mw_free(wpa_log_buf);
    }
    return iRet;
}

static int anj_wifi_connect_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    __INFO("anj_wifi_connect_thread start\n");
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    DevInfo *pstDevInfo = getDevInfo();

    while (bStart && *bStart)
    {
        if(pstDevInfo->bFactoryMode == 1)
        {
            sleep(1);
            continue;
        }
    
        if (s_stWifiParam.thread_pause_flag == THREAD_STATUS_PAUSE)
        {
            if (access("/tmp/wifi", F_OK) == 0)
            {
                s_stWifiParam.thread_pause_flag = THREAD_STATUS_RUNNING;
            }

            sleep(1);
            continue;
        }
        else if (s_stWifiParam.thread_pause_flag == THREAD_STATUS_WAIT)
        {
            anj_wifi_destory();
            s_stWifiParam.thread_pause_flag = THREAD_STATUS_PAUSE;
            sleep(1);
            continue;
        }
        else
        {
            if (s_stWifiParam.wifiMode == WIFI_MODE_WPA)
            {
                __INFO("Using WPA mode.\n");
                if (strlen(pstNetworkConfig->wifiCfg.essid) > 0)
                {
                    anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_WIFI_CONNECTING, 1);
                    /*进行3种WIFI环境的切换连接*/
                    /*连接顺序normal(30s) -> no_scan(30s) -> gbk(60s)*/
                    for (int i = WPA_CONFIG_NORMAL; i < WPA_CONFIG_NUM; i++)
                    {
                        iRet = anj_wifi_generate_wpa_config(&pstNetworkConfig->wifiCfg, i);
                        if (iRet != 0)
                        {
                            continue;
                        }
                        if (i == WPA_CONFIG_NORMAL)
                        {
                            iRet = anj_wifi_start_wpa(WPA_NORMAL_OVERTIME, &pstNetworkConfig->wifiCfg);
                        }
                        else if (i == WPA_CONFIG_GBK)
                        {
                            iRet = anj_wifi_start_wpa(WPA_GBK_OVERTIME, &pstNetworkConfig->wifiCfg);
                        }
                        else if (i == WPA_CONFIG_NO_SCAN)
                        {
                            iRet = anj_wifi_start_wpa(WPA_NO_SCAN_OVERTIME, &pstNetworkConfig->wifiCfg);
                        }
                        if (iRet == 0)
                        {
                            break;
                        }
                    }

                    if (s_stWifiParam.wifiStatus == WIFI_STATUS_CONNECTED)
                    {
                        s_stWifiParam.thread_pause_flag = THREAD_STATUS_PAUSE;

                        if (pstDevInfo->bFactoryMode != 1)
                        {
                            anj_config_network_save(pstNetworkConfig);
                        }
                    }
                    else if (s_stWifiParam.wifiStatus == WIFI_STATUS_PASSWORD_ERROR)
                    {
                        anj_bind_cfg_clean();
                        anj_wifi_destory();
                        anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_WIFI_PASSWORD_ERROR, 1);
                        anj_bind_set(0);
                        anj_net_provider_ble_config_pwd_err();
                    }
                    else if (s_stWifiParam.wifiStatus == WIFI_STATUS_NONE)
                    {
                        s_stWifiParam.audioWaitInterval--;
                        if (s_stWifiParam.audioWaitInterval <= 0 && s_stWifiParam.audioWaitCnt > 0)
                        {
                            s_stWifiParam.audioWaitInterval = WIFI_WAIT_CONFIG_INTERVAL;
                            anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_WAIT_CONFIG_NET, 1);
                            s_stWifiParam.audioWaitCnt--;
                        }
                    }
                    else
                    {
                        anj_bind_cfg_clean();
                        anj_wifi_destory();
                        anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_WIFI_CONNECT_FAIL, 1);
                        anj_bind_set(0);
                        anj_net_provider_ble_connect_fail();
                    }
                }
                else
                {
                    if (s_stWifiParam.wifiStatus == WIFI_STATUS_NONE)
                    {
                        s_stWifiParam.audioWaitInterval--;
                        if (s_stWifiParam.audioWaitInterval <= 0 && s_stWifiParam.audioWaitCnt > 0)
                        {
                            s_stWifiParam.audioWaitInterval = WIFI_WAIT_CONFIG_INTERVAL;
                            anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_WAIT_CONFIG_NET, 1);
                            s_stWifiParam.audioWaitCnt--;
                        }
                    }
                }
            }
            else if (s_stWifiParam.wifiMode == WIFI_MODE_AP && strlen(pstNetworkConfig->wifiApCfg.essid) > 0)
            {
                __INFO("Using AP mode.\n");
                if (anj_wifi_generate_hostapd_config(&pstNetworkConfig->wifiApCfg) == 0)
                {
                    anj_wifi_start_hostapd();
                }
                s_stWifiParam.thread_pause_flag = THREAD_STATUS_PAUSE;
            }
        }

        sleep(WIFI_THREAD_SLEEP);
    }

    anj_wifi_destory();
    __INFO("anj_wifi_connect_thread end\n");
    return 0;
}

static int anj_wifi_check_thread(void *ctx, int *bStart)
{
    int nCount = 0;                                /*根据信号强度每隔对应的时间判断是否切换最优WIFI*/
    int nSignalCount = WIFI_CHECK_SIGNAL_TIME;     /*每隔30s检测一下WIFI信号强度*/
    int nChangeCnt = 0;                            /*记录需要切换的次数*/
    int nChangeTime = WIFI_CHECK_BEST_SIGNAL_TIME; /*切换最优WIFI的时间*/

    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();

    // /*开机1分钟不做处理*/
    // sleep(60);
    const int ping_cnt = 3;          /* ping尝试次数 */
    const int ping_timeout_ms = 500; /* 每次ping超时时间/ms */
    int heart_beanj_wifi_at_cnt = 60;
    int heart_beanj_wifi_at_cur = 0;
    int internet_failed_cnt = 0;
    int first_ping_flag = 0;
    int play_net_connectd_flag = 0;
    anj_ser_info *pstSerInfo = getSerInfo();
    DevInfo *pstDevInfo = getDevInfo();

    while (bStart && *bStart)
    {
        if(pstDevInfo->bFactoryMode == 1)
        {
            sleep(1);
            continue;
        }

        if ((s_stWifiParam.thread_pause_flag != THREAD_STATUS_PAUSE) ||
            (anj_wifi_status_get() != WIFI_STATUS_CONNECTED))
        {
            nSignalCount = 0;
            nCount = 0;
            nChangeCnt = 0;
            heart_beanj_wifi_at_cnt = 60;
            heart_beanj_wifi_at_cur = 0;
            internet_failed_cnt = 0;
            first_ping_flag = 0;
            play_net_connectd_flag = 0;
            sleep(1);
            continue;
        }

        if (heart_beanj_wifi_at_cur % heart_beanj_wifi_at_cnt == 0)
        {
            heart_beanj_wifi_at_cur = 0;
            if (pstSerInfo->stP2pLoginState.logined == 0 || first_ping_flag == 0)
            {
                int ret = try_ping("8.8.8.8", ping_timeout_ms, ping_cnt, WIFI_INTERFACE_NAME, bStart);
                if (ret < 0)
                {
                    ret = try_ping("114.114.114.114", ping_timeout_ms, ping_cnt, WIFI_INTERFACE_NAME, bStart);
                }

                if (ret < 0)
                {
                    if(internet_failed_cnt == 0)        // net中的udhcpc状态准备好比较慢，首次ping失败不播放异常
                        heart_beanj_wifi_at_cnt = 5;
                    else
                        heart_beanj_wifi_at_cnt = 10;

                    if (internet_failed_cnt > 0 && play_net_connectd_flag != 1)
                    {
                        play_net_connectd_flag = 1;
                        anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_CONNECT_NET_FAIL, 1);
                    }

                    internet_failed_cnt++;
                    __ERR("ping internet failed %d\n", internet_failed_cnt);
                }
                else
                {
                    internet_failed_cnt = 0;
                    first_ping_flag = 1;
                    heart_beanj_wifi_at_cnt = 60;
                    if (play_net_connectd_flag != 2)
                    {
                        play_net_connectd_flag = 2;
                        anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_CONNECT_NET_SUCCESS, 1);
                    }

                    __INFO("###4G network heart beat success\n");
                }
            }
            else if (pstSerInfo->stP2pLoginState.logined)
            {
                if (play_net_connectd_flag != 2)
                {
                    play_net_connectd_flag = 2;
                    anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_CONNECT_NET_SUCCESS, 1);
                }
                internet_failed_cnt = 0;
            }
        }

        heart_beanj_wifi_at_cur++;

        // 信号切换逻辑
        WIFI_QUALITY stInfos = {0};
        if (anj_wifi_quality_get(&stInfos) == 0)
        {
            nSignalCount++;
            nCount++;
            if ((nSignalCount >= WIFI_CHECK_SIGNAL_TIME))
            {
                nSignalCount = 0;
                if (stInfos.signalLevel < WIFI_SIGNAL_LEVEL3 && stInfos.signalLevel >= WIFI_SIGNAL_LEVEL2)
                {
                    nChangeTime = WIFI_CHECK_BEST_SIGNAL_TIME;
                }
                else if (stInfos.signalLevel < WIFI_SIGNAL_LEVEL2 && stInfos.signalLevel >= WIFI_SIGNAL_LEVEL1)
                {
                    nChangeTime = WIFI_CHECK_BEST_SIGNAL_TIME / 2;
                }
                else if (stInfos.signalLevel < WIFI_SIGNAL_LEVEL1)
                {
                    nChangeTime = WIFI_CHECK_BEST_SIGNAL_TIME / 4;
                }
            }

            if (nCount >= nChangeTime)
            {
                nCount = 0;
                if (stInfos.signalLevel < WIFI_SIGNAL_LEVEL3)
                {
                    int ret = anj_wifi_best_ssid_get(pstNetworkConfig->wifiCfg.essid);
                    if (0 < ret)
                    {
                        /*连续两次信号都不是最好的情况下，进行WIFI切换处理*/
                        nChangeCnt++;
                        if (nChangeCnt >= 2)
                        {
                            nChangeCnt = 0;
                            __RECORD_LOG_INFO("wifi connect Restart,signal level:%d\n", stInfos.signalLevel);
                            anj_wifi_thread_set(THREAD_STATUS_RUNNING);
                        }
                    }
                    else
                    {
                        nChangeCnt = 0;
                    }
                }
                else
                {
                    nChangeCnt = 0;
                }
            }
        }
        else
        {
            nChangeCnt = 0;
            nSignalCount = 0;
            nCount = 0;
        }

        sleep(1);
    }

    return 0;
}

int anj_wifi_init(void)
{
    int iRet = 0;
    if (s_stWifiParam.bInit)
    {
        return iRet;
    }
    memset(&s_stWifiParam, 0, sizeof(s_stWifiParam));
    s_stWifiParam.thread_pause_flag = THREAD_STATUS_PAUSE;
    s_stWifiParam.audioWaitCnt = WIFI_WAIT_CONFIG_CNT;
    s_stWifiParam.audioWaitInterval = WIFI_WAIT_CONFIG_INTERVAL;
    
    NetworkConfigNew *networkCfg = (NetworkConfigNew *)getNetWorkConfig();
    WIFIApConfig *wifiApCfg = &networkCfg->wifiApCfg;
    s_stWifiParam.wifiMode = wifiApCfg->enable == 1 ? WIFI_MODE_AP : WIFI_MODE_WPA;

    memset(&s_stWifiThread, 0, sizeof(anj_thread_s));
    memset(&s_stWifiCheckThread, 0, sizeof(anj_thread_s));

    anj_wifi_add_capability();
    anj_wifi_module_type_check();

    s_stWifiThread.bAutoDestroy = 0;
    strncpy(s_stWifiThread.iThreadName, "anj_wifi_connect_thread", sizeof(s_stWifiThread.iThreadName) - 1);
    s_stWifiThread.iThreadjob.ctx = &s_stWifiThread;
    s_stWifiThread.iThreadjob.func = anj_wifi_connect_thread;
    iRet = anj_thread_task_create(&s_stWifiThread);
    if (iRet != 0)
    {
        __ERR("wifi connect create failed: %d\n", iRet);
        return iRet;
    }

    s_stWifiCheckThread.bAutoDestroy = 0;
    strncpy(s_stWifiCheckThread.iThreadName, "anj_wifi_check_thread", sizeof(s_stWifiCheckThread.iThreadName) - 1);
    s_stWifiCheckThread.iThreadjob.ctx = &s_stWifiCheckThread;
    s_stWifiCheckThread.iThreadjob.func = anj_wifi_check_thread;
    iRet = anj_thread_task_create(&s_stWifiCheckThread);
    if (iRet != 0)
    {
        __ERR("wifi check create failed: %d\n", iRet);
        return iRet;
    }

    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_WIFI_QUALITY_GET, anj_wifi_quality_event_get);
    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_WIFI_CONNECT_SET, anj_wifi_thread_event_set);

    s_stWifiParam.bInit = 1;
    return 0;
}

int anj_wifi_uninit(void)
{
    int iRet = 0;

    if (s_stWifiParam.bInit == 0)
    {
        return iRet;
    }

    iRet = anj_thread_task_destroy(&s_stWifiCheckThread, 0);
    if (iRet != 0)
    {
        __ERR("wifi check exit failed: %d\n", iRet);
    }

    iRet = anj_thread_task_destroy(&s_stWifiThread, 0);
    if (iRet != 0)
    {
        __ERR("wifi connect exit failed: %d\n", iRet);
    }
    memset(&s_stWifiParam, 0, sizeof(s_stWifiParam));

    __INFO("anj_wifi_uninit success\n");
    return iRet;
}

void anj_wifi_destory()
{
    __INFO("Restarting WiFi...\n");

    if (anj_sysmng_check_process("wpa_supplicant") == 0)
    {
        __INFO("wpa_supplicant is running, stopping it...\n");
        anj_mw_system("killall udhcpc");
        anj_mw_system("killall wpa_supplicant");
        usleep(100 * 1000); // 等待100毫秒
    }

    if (anj_sysmng_check_process("hostapd") == 0)
    {
        __INFO("hostapd is running, stopping it...\n");
        anj_mw_system("killall hostapd");
        usleep(100 * 1000); // 等待100毫秒
    }

    if (anj_sysmng_check_process("udhcpd") == 0)
    {
        __INFO("udhcpd is running, stopping it...\n");
        anj_mw_system("killall udhcpd");
        usleep(100 * 1000); // 等待100毫秒
    }

    net_del_ip(WIFI_INTERFACE_NAME);
    usleep(50 * 1000);
    net_set_down(WIFI_INTERFACE_NAME);
    usleep(50 * 1000);

    anj_wifi_status_set(WIFI_STATUS_NONE);
}

void anj_wifi_status_set(int status)
{
    anj_mutex_lock(&s_stWifiMutex);
    s_stWifiParam.wifiStatus = status;
    s_stWifiParam.audioWaitInterval = WIFI_WAIT_CONFIG_INTERVAL;
    s_stWifiParam.audioWaitCnt = WIFI_WAIT_CONFIG_CNT;
    anj_mutex_unlock(&s_stWifiMutex);
}

int anj_wifi_status_get()
{
    int status = WIFI_STATUS_NONE;
    anj_mutex_lock(&s_stWifiMutex);
    status = s_stWifiParam.wifiStatus;
    anj_mutex_unlock(&s_stWifiMutex);
    return status;
}

int anj_wifi_info_get(const char *ifname, void *arg)
{
    int skfd;
    char buf[256] = {0};
    NETWORK_STATUS_DATA *networkStatus = (NETWORK_STATUS_DATA *)arg;
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("socket error");
        return -1;
    }

    struct iwreq wrq;
    wireless_info info = {0};

    /* Get basic information */
    if (iw_get_basic_config(skfd, ifname, &(info.b)) < 0)
    {
        /* If no wireless name : no wireless extensions */
        /* But let's check if the interface exists at all */
        struct ifreq ifr;
        snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
        if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
        {
            close(skfd);
            return (-ENODEV);
        }
        else
        {
            close(skfd);
            return (-ENOTSUP);
        }
    }

    /* Get ranges */
    if (iw_get_range_info(skfd, ifname, &(info.range)) >= 0)
        info.has_range = 1;

    /* Get AP address */
    if (iw_get_ext(skfd, ifname, SIOCGIWAP, &wrq) >= 0)
    {
        info.has_ap_addr = 1;
        memcpy(&(info.ap_addr), &(wrq.u.ap_addr), sizeof(sockaddr));
    }

    /* Get bit rate */
    if (iw_get_ext(skfd, ifname, SIOCGIWRATE, &wrq) >= 0)
    {
        info.has_bitrate = 1;
        memcpy(&(info.bitrate), &(wrq.u.bitrate), sizeof(iwparam));
    }

    /* Get RTS threshold */
    if (iw_get_ext(skfd, ifname, SIOCGIWRTS, &wrq) >= 0)
    {
        info.has_rts = 1;
        memcpy(&(info.rts), &(wrq.u.rts), sizeof(iwparam));
    }

    /* Get fragmentation threshold */
    if (iw_get_ext(skfd, ifname, SIOCGIWFRAG, &wrq) >= 0)
    {
        info.has_frag = 1;
        memcpy(&(info.frag), &(wrq.u.frag), sizeof(iwparam));
    }

    /* Get Power Management settings */
    wrq.u.power.flags = 0;
    if (iw_get_ext(skfd, ifname, SIOCGIWPOWER, &wrq) >= 0)
    {
        info.has_power = 1;
        memcpy(&(info.power), &(wrq.u.power), sizeof(iwparam));
    }

    //	__ERR("has_range = %d,  range->we_version_compiled = %d\n",
    //		info.has_range,
    //		info.range.we_version_compiled);

    if ((info.has_range) && (info.range.we_version_compiled > 9))
    {
        /* Get Transmit Power */
        if (iw_get_ext(skfd, ifname, SIOCGIWTXPOW, &wrq) >= 0)
        {
            info.has_txpower = 1;
            memcpy(&(info.txpower), &(wrq.u.txpower), sizeof(iwparam));
        }
    }

    if ((info.has_range) && (info.range.we_version_compiled > 10))
    {
        /* Get retry limit/lifetime */
        if (iw_get_ext(skfd, ifname, SIOCGIWRETRY, &wrq) >= 0)
        {
            info.has_retry = 1;
            memcpy(&(info.retry), &(wrq.u.retry), sizeof(iwparam));
        }
    }

    /* Get stats */
    //	__ERR("call iw_get_stats!!!\n");

    if (iw_get_stats(skfd, ifname, &(info.stats),
                     &info.range, info.has_range) >= 0)
    {
        //		__ERR("call iw_get_stats OK!!!\n");

        info.has_stats = 1;
    }

    if ((info.has_range) && (info.range.we_version_compiled > 17))
    {
        wrq.u.param.flags = IW_AUTH_KEY_MGMT;
        if (iw_get_ext(skfd, ifname, SIOCGIWAUTH, &wrq) >= 0)
        {
            info.has_auth_key_mgmt = 1;
            info.auth_key_mgmt = wrq.u.param.value;
        }

        wrq.u.param.flags = IW_AUTH_CIPHER_PAIRWISE;
        if (iw_get_ext(skfd, ifname, SIOCGIWAUTH, &wrq) >= 0)
        {
            info.has_auth_cipher_pairwise = 1;
            info.auth_cipher_pairwise = wrq.u.param.value;
        }

        wrq.u.param.flags = IW_AUTH_CIPHER_GROUP;
        if (iw_get_ext(skfd, ifname, SIOCGIWAUTH, &wrq) >= 0)
        {
            info.has_auth_cipher_group = 1;
            info.auth_cipher_group = wrq.u.param.value;
        }
    }
    close(skfd);

    memset(networkStatus->essid, '\0', sizeof(networkStatus->essid));
    if (info.b.has_essid)
    {
        strcpy(networkStatus->essid, info.b.essid);
    }

    memset(networkStatus->operationMode, '\0', sizeof(networkStatus->operationMode));
    __ERR("info.b.has_mode:%d\n", info.b.has_mode);
    if (info.b.has_mode)
    {

        __ERR("info.b.mode:%d\n", info.b.mode);
        if (strcmp(iw_operation_mode[info.b.mode], "Master") == 0)
        {
            strcpy(networkStatus->operationMode, "Access Point");
        }
        else
        {
            strcpy(networkStatus->operationMode, "Station");

            // strcpy(networkStatus->operationMode, iw_operation_mode[info.b.mode]);
        }
    }

    memset(networkStatus->bitRate, '\0', sizeof(networkStatus->bitRate));
    if (info.has_bitrate)
    {
        if (info.bitrate.value == 0)
            strcpy(networkStatus->bitRate, "AUTO");
        else
        {
            iw_print_bitrate(buf, 256, info.bitrate.value);
            strcpy(networkStatus->bitRate, buf);
        }
    }

    memset(networkStatus->freq, '\0', 256);
    if (info.b.has_freq)
    {
        double freq = info.b.freq;
        iw_print_freq(buf, sizeof(buf), freq, -1, info.b.freq_flags);

        strcpy(networkStatus->freq, buf + strlen("Frequency="));
    }

    memset(networkStatus->accessPoint, '\0', 256);
    if (info.has_ap_addr)
    {
        iw_sawap_ntop(&info.ap_addr, buf);
        strcpy(networkStatus->accessPoint, buf);
    }

    // encrypt type
    strcpy(networkStatus->encryptType, "none");

    __ERR("info.b.key_flags = 0x%08x\n", info.b.key_flags);

    if (pstNetworkConfig->wifiCfg.wirelessEncrypt.enable)
    {
        if (strcmp(pstNetworkConfig->wifiCfg.wirelessEncrypt.encryptType, "wpa") == 0)
        {
            strcpy(networkStatus->encryptType, "WPA");
        }
        else if (strcmp(pstNetworkConfig->wifiCfg.wirelessEncrypt.encryptType, "wep") == 0)
        {
            strcpy(networkStatus->encryptType, "WEP");
        }
    }

    __ERR("info.has_stats = %d\n", info.has_stats);

    // link quality,signal level, noise
    if (info.has_stats)
    {
        __ERR("info link = %d, signal = %d, level = %d\n",
              info.stats.qual.qual,
              info.stats.qual.level,
              info.stats.qual.noise);

        networkStatus->linkquality = info.stats.qual.qual;

        if (info.stats.qual.level & 0x80)
            networkStatus->signallevel = info.stats.qual.level - 256;
        else
            networkStatus->signallevel = info.stats.qual.level & 0x7F;

        if (info.stats.qual.noise & 0x80)
            networkStatus->noise = info.stats.qual.noise - 256;
        else
            networkStatus->noise = info.stats.qual.noise & 0x7F;

        if (WIFI_TYPE_AIC8800 == s_WifiModuleType)
        {
            networkStatus->linkquality = 100 * (networkStatus->signallevel - (-120)) / ((-30) - (-120));
        }

        __ERR("networkStatus link = %d, signal = %d, level = %d\n",
              networkStatus->linkquality,
              networkStatus->signallevel,
              networkStatus->noise);
    }
    else
    {
        networkStatus->linkquality = 0;
        networkStatus->signallevel = 0;
        networkStatus->noise = 0;
    }

    return 0;
}

int anj_wifi_ap_info_get(void *info)
{
    int iRet = 0;
    int skfd = -1;
    ANJ_CHK(info != NULL, -1, "input Invalid");

    WIFI_AP_SCAN *apinfos = (WIFI_AP_SCAN *)info;
    skfd = iw_sockets_open();
    ANJ_CHK(skfd > 0, -1, "iw_sockets_open failed");

    wireless_scan_head scanList;
    wireless_scan *apscan;

    __ERR("try iw_scan\n");
    ANJ_CHK_FUNC(iw_scan(skfd, WIFI_INTERFACE_NAME, 22, &scanList), 0, "iw_scan error");
    __ERR("end iw_scan\n");

    apscan = scanList.result;
    while (apscan)
    {
        if (apinfos->apCnt > MAX_WIFI_AP_CNT - 1)
            break;

        WIFI_AP_INFO *pApRet = &(apinfos->apInfos[apinfos->apCnt]);
        memset(pApRet, 0, sizeof(WIFI_AP_INFO));

        if (apscan->b.essid_on)
        {
            //__ERR("essid len :%d, strlen:%d, haskey:%d\n", apscan->b.essid_len,
            // strlen(apscan->b.essid), apscan->b.has_key);

            if (apscan->b.essid_len == 0)
            {
                //__ERR("invalid ssid(essid_len = 0) , skip\n");
                apscan = apscan->next;
                continue;
            }

            strncpy(pApRet->ssid, apscan->b.essid, sizeof(pApRet->ssid) - 1);
            if (str_is_utf8(apscan->b.essid, apscan->b.essid_len) == 0)
            {
                gb2312_to_utf8(apscan->b.essid, pApRet->ssid);
            }
        }
        else
        {
            //__ERR("essid is not on, skip\n");
            apscan = apscan->next;
            continue;
        }

        if (apscan->b.has_mode)
        {
            strncpy(pApRet->workMode, iw_operation_mode[apscan->b.mode], sizeof(pApRet->workMode) - 1);
            //__ERR("@@@@@@@@@@@ workmode:%d, des:%s\n", apscan->b.mode, iw_operation_mode[apscan->b.mode]);
        }

        if (apscan->b.has_freq)
        {
            pApRet->channel = apscan->b.channel;
        }

        if (pApRet->channel < 0)
        {
            //__ERR("ssid(%s) @@@@@@@[%lf]@@@@@@@@@@@\n", pApRet->ssid, apscan->b.freq);
            // 5g default channel
            pApRet->channel = Freq5GToChannel(apscan->b.freq);
        }

        pApRet->quality = apscan->quality;
        pApRet->signalLevel = apscan->signal_level;
        pApRet->noiseLevel = apscan->noise_level;
        pApRet->maxquality = apscan->maxquality;

        strncpy(pApRet->wirelessMode, apscan->b.name, sizeof(pApRet->wirelessMode) - 1);
        snprintf(pApRet->mac, sizeof(apscan->mac), "%s", apscan->mac);

        if (!apscan->b.has_key)
        {
            pApRet->authMode = WIFI_AUTH_OPEN;
            pApRet->encryType = WIFI_ENCRYP_NONE;

            apinfos->apCnt++;
            apscan = apscan->next;

            continue;
        }

        pApRet->authMode = WIFI_AUTH_SHARED;
        pApRet->encryType = WIFI_ENCRYP_WEP;

        if (apscan->auth_cnt > 0)
        {
            wireless_auth_info *pauthinfo = &(apscan->auth_info[0]);

            if (pauthinfo->auth_type == 1)
            {
                pApRet->authMode = WIFI_AUTH_WPAPSK;
                pApRet->encryType = WIFI_ENCRYP_TKIP;
            }
            else if (pauthinfo->auth_type == 2)
            {
                pApRet->authMode = WIFI_AUTH_WPA2PSK;
                pApRet->encryType = WIFI_ENCRYP_AES;
            }
            else
            {
                pApRet->authMode = WIFI_AUTH_SHARED;
                pApRet->encryType = WIFI_ENCRYP_WEP;

                apinfos->apCnt++;
                apscan = apscan->next;

                continue;
            }

            if (pauthinfo->pairwise_cnt > 0)
            {
                pApRet->encryType = WIFI_ENCRYP_UNSPPORT;

                int p;
                for (p = 0; p < pauthinfo->pairwise_cnt; p++)
                {
                    if (pauthinfo->pairwise_cipher[p] == IW_IE_CIPHER_TKIP)
                    {
                        pApRet->encryType = WIFI_ENCRYP_TKIP;
                        break;
                    }
                    else if (pauthinfo->pairwise_cipher[p] == IW_IE_CIPHER_WEP40)
                    {
                        pApRet->encryType = WIFI_ENCRYP_WEP;
                        break;
                    }
                    else if (pauthinfo->pairwise_cipher[p] == IW_IE_CIPHER_CCMP)
                    {
                        pApRet->encryType = WIFI_ENCRYP_AES;
                        break;
                    }
                    else if (pauthinfo->pairwise_cipher[p] == IW_IE_CIPHER_WEP104)
                    {
                        pApRet->encryType = WIFI_ENCRYP_WEP;
                        break;
                    }
                }
            }
            else
            {
                pApRet->encryType = WIFI_ENCRYP_UNSPPORT;
            }

            if (pauthinfo->key_type_cnt > 0)
            {
                pApRet->authMode = WIFI_AUTH_UNSPPORT;

                int key_type = pauthinfo->key_type[0];

                if (key_type == IW_IE_KEY_MGMT_PSK)
                {
                    if (pauthinfo->auth_type == 1)
                    {
                        pApRet->authMode = WIFI_AUTH_WPAPSK;
                    }
                    else if (pauthinfo->auth_type == 2)
                    {
                        pApRet->authMode = WIFI_AUTH_WPA2PSK;
                    }
                }
                else
                {
                    pApRet->authMode = WIFI_AUTH_UNSPPORT;
                }
            }
            else
            {
                pApRet->authMode = WIFI_AUTH_UNSPPORT;
            }
        }
        apinfos->apCnt++;
        apscan = apscan->next;

        __INFO("%s:|mac:%s|signal:%d|noise:%d|quality:%d/%d|\n",
               pApRet->ssid, pApRet->mac, pApRet->signalLevel, pApRet->noiseLevel, pApRet->quality, pApRet->maxquality);
    }

    apscan = scanList.result;
    wireless_scan *apscanNext;

    while (apscan)
    {
        apscanNext = apscan->next;

        free(apscan);
        apscan = apscanNext;
    }

    __ERR("scan %d ap devices\n", apinfos->apCnt);

endFunc:
    if (skfd > 0)
    {
        iw_sockets_close(skfd);
    }
    return iRet;
}
