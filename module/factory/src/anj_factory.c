#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <net/if_arp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "anj_mw_comm.h"
#include "anj_mw_errcode.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_factory.h"
#include "anj_net.h"
#include "anj_net_provider.h"
#include "project_option.h"
#include "eventhub.h"
#include "anj_sdcard.h"
#include "anj_sysmng.h"
#include "anj_ispctl.h"
#include "anj_video.h"

#define FACTORY_WIFI_CONNECT_MAX_COUNT 20
#define FACTORY_WIFI_QUERY_MAX_CNT 20
#define FACTORY_IRCUT_LIGHT_TEST_MAX_CNT 4

typedef struct
{
    char ssid[128];
    char passwd[128];
    int m_BitrateCheck;   // 检测码率
    int m_WifiNewConnect; // wifi重新连接
    int m_WifiThreadFlag;
    int m_IrcutLightTest; // ircut和灯光测试
    int m_HardWareThreadFlag;

    int m_start_flag; // 开始flag
} FactoryInfo;

typedef struct
{
    WIFI_AP_INFO wifi_info;
    int wifi_check_completed;
} FactoryWifiInfo;

static anj_thread_s s_stFactoryTestThread;              // 产测主线程
static anj_thread_s s_stFactoryWifiTestThread;          // wifi产测线程
static anj_thread_s s_stFactoryHardwareTestThread;      // 硬件产测线程

static FactoryInfo s_stFactoryInfo = {0};
static pthread_mutex_t s_stFactoryMutex = PTHREAD_MUTEX_INITIALIZER;

int anj_factory_defcfg_get(const char *xmlBuf, FactoryDefaultCfg *pDefaultCfg)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    memset(pDefaultCfg, 0, sizeof(FactoryDefaultCfg));
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    pDefaultCfg->LedMode = pstMediaConfig->videoConfig[0].videoCapture.led_mode;
    pDefaultCfg->IrCutMode = pstMediaConfig->videoConfig[0].videoCapture.ircut_mode;

    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        iRet = -1;
        goto endFunc;
    }

    char *LedMode = GetRequestParamValueByName(pDocNode, "FactoryCfg", "LedMode");
    char *IrcutMode = GetRequestParamValueByName(pDocNode, "FactoryCfg", "IrcutMode");
    ixmlDocument_free(pDocNode);

    if (LedMode)
    {
        pDefaultCfg->LedMode = atoi(LedMode);
        anj_mw_free(LedMode);
    }

    if (IrcutMode)
    {
        pDefaultCfg->IrCutMode = atoi(IrcutMode);
        anj_mw_free(IrcutMode);
    }

endFunc:
    return iRet;
}

int anj_factory_defcfg_save(FactoryDefaultCfg *pDefaultCfg)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    ANJ_CHK((pDefaultCfg != NULL), -1, "input Invalid");
    char buf[1024] = {0};
    snprintf(buf, sizeof(buf),
             "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
             "<FactoryCfg IrcutMode=\"%d\" LedMode=\"%d\"/>",
             pDefaultCfg->IrCutMode, pDefaultCfg->LedMode);
    iRet = anj_mw_write_file(FACTORY_DEFAULT_CFG_PATH, 0, buf, strlen(buf));
    ANJ_CHK((iRet == strlen(buf)), -1, "fwrite failed!");

endFunc:
    return iRet;
}

int anj_factory_defcfg_load(FactoryDefaultCfg *pDefaultCfg)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    ANJ_CHK((pDefaultCfg != NULL), -1, "input Invalid");
    char buf[1024] = {0};
    iRet = anj_mw_read_file_limit_len(FACTORY_DEFAULT_CFG_PATH, buf, sizeof(buf));
    ANJ_CHK((iRet > 0), -1, "fread failed!");
    ANJ_CHK_FUNC(anj_factory_defcfg_get(buf, pDefaultCfg), 0, "defcfg get failed!\n");

endFunc:
    return iRet;
}

int anj_factory_image_flip_cfg_load(int *flip)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    if (flip == NULL)
    {
        iRet = -1;
        goto endFunc;
    }
    char buf[64] = {0};
    iRet = anj_mw_read_file_limit_len(FACTORY_IMAGE_FILP_CFG, buf, sizeof(buf));
    if (iRet <= 0)
    {
        __INFO("load factory image config failed\n");
        iRet = -1;
        goto endFunc;
    }

    int tmpvalue = 0;
    int cnt = sscanf(buf, "ImageFlip=(%d)", &tmpvalue);
    if (cnt == 1)
    {
        *flip = tmpvalue;
        iRet = 0;
        goto endFunc;
    }

    iRet = -1;
endFunc:
    return iRet;
}

int anj_factory_image_flip_cfg_save(int flip)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "ImageFlip=(%d)", flip);
    iRet = write_buffer_to_file(FACTORY_IMAGE_FILP_CFG, buf, strlen(buf));
endFunc:
    return iRet;
}

int anj_factory_test(factory_test_mode_e mode)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    switch (mode)
    {
    case FACTORY_TEST_PTZ_LED_IRCUT:
        anj_factory_ircut_light_test_set();
        break;

    default:
        break;
    }
endFunc:
    return iRet;
}

void anj_factory_bitrate_check_set()
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    __INFO("factory set check bitrate! IPC_NETWORK_TYPE:%d\n", IPC_NETWORK_TYPE);
    // 非纯有线设备判断码率是否需要调整
    if (IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE)
    {
        __ERR("factory ipc type:%d don't need change dev bitrate!\n", IPC_NETWORK_TYPE);
        goto endFunc;
    }

    anj_mutex_lock(&s_stFactoryMutex);
    s_stFactoryInfo.m_BitrateCheck = 1;
    anj_mutex_unlock(&s_stFactoryMutex);
endFunc:
    (void)iRet;
    return;
}

int anj_factory_wifi_connect_set(char *ssid, char *passwd)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    if (NET_DEV_TYPE_WIRE_WIFI != IPC_NETWORK_TYPE && NET_DEV_TYPE_WIFI != IPC_NETWORK_TYPE)
    { 
        __ERR("factory ipc type:%d don't need connect wifi!\n", IPC_NETWORK_TYPE);
        iRet = -1;
        goto endFunc;
    }

    if (ssid == NULL || (strlen(ssid) == 0) || passwd == NULL)
    {
        __ERR("ssid or passwd is null\n");
        iRet = -1;
        goto endFunc;
    }

    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    WIFIConfig *pstWifiConfig = &pstNetworkConfig->wifiCfg;

    anj_mutex_lock(&s_stFactoryMutex);
    if (strcmp(pstWifiConfig->essid, ssid) != 0 || 
        (strlen(passwd) != strlen(pstWifiConfig->wirelessEncrypt.wpaEncrypt.keyValue)) || 
        (strlen(passwd) > 0 && strcmp(pstWifiConfig->wirelessEncrypt.wpaEncrypt.keyValue, passwd) != 0))
    {
        s_stFactoryInfo.m_WifiNewConnect = 1;

        pstWifiConfig->enable = 1;
        pstWifiConfig->dhcpEnable = 1;
        snprintf(pstWifiConfig->essid, sizeof(pstWifiConfig->essid), "%s", ssid);
        snprintf(pstWifiConfig->wirelessEncrypt.wpaEncrypt.keyValue,
                 sizeof(pstWifiConfig->wirelessEncrypt.wpaEncrypt.keyValue), "%s", passwd);
    }
    anj_mutex_unlock(&s_stFactoryMutex);

endFunc:
    return iRet;
}

void anj_factory_ircut_light_test_set()
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");
    anj_mutex_lock(&s_stFactoryMutex);
    s_stFactoryInfo.m_IrcutLightTest = 1;
    anj_mutex_unlock(&s_stFactoryMutex);
endFunc:
    (void)iRet;
    return;
}

static void anj_factory_adjust_bitrate()
{
    int encode_change_status = 0;
    int default_bitrate_wire = 2500;
    int default_bitrate_mobile = 800;

    int sd_status = 0;
    char sd_dev[16] = {0};
    snprintf(sd_dev, sizeof(sd_dev), SDCARD_DEV_NAME, 0);
    if (access(sd_dev, F_OK) == 0)
    {
        sd_status = 1;
    }

    int wire_status = 0;
    wire_status = Check_Link_Status(WIRE_INTERFACE_NAME);

    MediaConfig *pMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        VideoEncode *pVideoEnc = &pMediaConfig->videoConfig[cameraIndex].videoEncode;
        if (wire_status)
        {
            if (sd_status)
            {
                if (pVideoEnc->encodeCfg[0].bitRate > default_bitrate_mobile)
                {
                    pVideoEnc->encodeCfg[0].bitRate = default_bitrate_mobile;
                    encode_change_status = 1;
                }
            }
            else
            {
                if (pVideoEnc->encodeCfg[0].bitRate < default_bitrate_wire)
                {
                    pVideoEnc->encodeCfg[0].bitRate = default_bitrate_wire;
                    encode_change_status = 1;
                }
            }
        }
        else
        {
            if (pVideoEnc->encodeCfg[0].bitRate > default_bitrate_mobile)
            {
                pVideoEnc->encodeCfg[0].bitRate = default_bitrate_mobile;
                encode_change_status = 1;
            }
        }
    
        if (1 == encode_change_status)
        {
            __INFO("factory change dev bitrate! wire_status:%d, sd_status:%d\n", wire_status, sd_status);
            anj_config_video_encode_set(pVideoEnc, cameraIndex);
        }
    }
}

int anj_factory_wifi_wpa_supplicant_conf_set()
{
    int iRet = 0;

    if (IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_WIFI || IPC_NETWORK_TYPE == NET_DEV_TYPE_WIFI)
    {
        FILE *pfile = NULL;
        char wpa_supplicant_conf_buf[512] = {0};
    
        pfile = anj_mw_fopen(WPA_SUPPLICANT_CONF_PATH, "w");
        if (pfile == NULL)
        {
            __ERR("open file:%s failed\n", WPA_SUPPLICANT_CONF_PATH);
            return -1;
        }
    
        if (strlen(s_stFactoryInfo.passwd) > 0)
        {
            snprintf(wpa_supplicant_conf_buf, sizeof(wpa_supplicant_conf_buf),
                     "ctrl_interface=/tmp/wpa_supplicant\n"
                     "network={\n"
                     "ssid=\"%s\"\n"
                     "pairwise=CCMP TKIP\n"
                     "group=CCMP TKIP\n"
                     "psk=\"%s\"\n"
                     "}\n"
                     "update_config=1",
                     s_stFactoryInfo.ssid, s_stFactoryInfo.passwd);
        }
        else
        {
            snprintf(wpa_supplicant_conf_buf, sizeof(wpa_supplicant_conf_buf),
                     "ctrl_interface=/tmp/wpa_supplicant\n"
                     "network={\n"
                     "ssid=\"%s\"\n"
                     "key_mgmt=NONE\n"
                     "}\n"
                     "update_config=1",
                     s_stFactoryInfo.ssid);
        }
    
        iRet = anj_mw_fwrite(pfile, wpa_supplicant_conf_buf, strlen(wpa_supplicant_conf_buf));
        if (iRet != strlen(wpa_supplicant_conf_buf))
        {
            __ERR("write file:%s wifi info failed\n", WPA_SUPPLICANT_CONF_PATH);
            anj_mw_fclose(pfile);
            return -1;
        }
        else
        {
            __INFO("write file:%s wifi info successful!\n", WPA_SUPPLICANT_CONF_PATH);
            anj_mw_fclose(pfile);
        }
    }

    return iRet;
}

int anj_factory_wifi_connect()
{
    char cmd[128] = {0};
    if (anj_sysmng_check_process("wpa_supplicant") == 0)
    {
        __INFO("wpa_supplicant is running, stopping it...\n");
        anj_mw_system("killall udhcpc");
        anj_mw_system("killall wpa_supplicant");
        usleep(100 * 1000);
    }

    if (anj_sysmng_check_process("hostapd") == 0)
    {
        __INFO("hostapd is running, stopping it...\n");
        anj_mw_system("killall hostapd");
        usleep(100 * 1000);
    }

    net_del_ip(WIFI_INTERFACE_NAME);
    usleep(50 * 1000);
    net_set_down(WIFI_INTERFACE_NAME);
    usleep(50 * 1000);

    net_set_up(WIFI_INTERFACE_NAME);

    snprintf(cmd, sizeof(cmd), AJ_APP_PATH "/wpa_supplicant -Dnl80211 -i%s -c %s > %s &",
             WIFI_INTERFACE_NAME, WPA_SUPPLICANT_CONF_PATH, WPA_LOG_PATH);
    anj_mw_system(cmd);

    return 0;
}

static int anj_factory_wifi_test_thread(void *ctx, int *bStart)
{
    int iRet = 0;

    // wifi连接查询
    int wifi_connect_success = 0;
    int wifi_status = WIFI_STATUS_NONE;
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    __INFO("device connect wifi ssid:%s in factory mode!\n", pstNetworkConfig->wifiCfg.essid);

    while (bStart && *bStart)
    {
        if (strlen(pstNetworkConfig->wifiCfg.essid) > 0 && wifi_connect_success == 0)
        {
            for (int i = WPA_CONFIG_NORMAL; i < WPA_CONFIG_NUM; i++)
            {
                iRet = anj_net_provider_wifi_generate_wpa_config(&pstNetworkConfig->wifiCfg, i);
                if (iRet != 0)
                {
                    continue;
                }
                if (i == WPA_CONFIG_NORMAL)
                {
                    iRet = anj_net_provider_wifi_start_wpa(FACTORY_WIFI_CONNECT_MAX_COUNT, &pstNetworkConfig->wifiCfg);
                }
                else if (i == WPA_CONFIG_GBK)
                {
                    iRet = anj_net_provider_wifi_start_wpa(FACTORY_WIFI_CONNECT_MAX_COUNT, &pstNetworkConfig->wifiCfg);
                }
                else if (i == WPA_CONFIG_NO_SCAN)
                {
                    iRet = anj_net_provider_wifi_start_wpa(FACTORY_WIFI_CONNECT_MAX_COUNT, &pstNetworkConfig->wifiCfg);
                }
                if (iRet == 0)
                {
                    break;
                }
            }
        }

        wifi_status = anj_net_provider_wifi_status_get();
        if (wifi_status == WIFI_STATUS_CONNECTED)
        {
            wifi_connect_success = 1;
            __INFO("factory wifi connect success\n");
            break;
        }
        else    // 超时后如果连接失败则继续
        {
            __INFO("factory wifi connect failed, status:%d! please check passwd!!\n", wifi_status);

            sleep(1);
            continue;
        }

// 产测wifi连接后，工具读取wifi信息从私有协议中net info中获取
    }

    anj_mutex_lock(&s_stFactoryMutex);
    s_stFactoryInfo.m_WifiThreadFlag = 0;
    anj_mutex_unlock(&s_stFactoryMutex);

    __INFO("factory wifi test thread exit!\n");
    return iRet;
}

static int anj_factory_hw_test_thread(void *ctx, int *bStart)
{
    __INFO("factory hardware test thread enter!\n");
#if 0
    int ircut_light_test = 0;
    int ircut_light_test_cnt = 0;
    int ircut_light_test_complete = 0;

    while (bStart && *bStart)
    {
        anj_mutex_lock(&s_stFactoryMutex);
        ircut_light_test = s_stFactoryInfo.m_IrcutLightTest;
        anj_mutex_unlock(&s_stFactoryMutex);
        
        if (1 == ircut_light_test && 0 == ircut_light_test_complete)
        {
            MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
            VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig.videoCapture;
            pVideoCapCfg->ircut_mode = IRCUT_Mode_Manual;
            anj_config_video_capture_set(pVideoCapCfg);

            // ircut切换
            while(ircut_light_test_cnt < FACTORY_IRCUT_LIGHT_TEST_MAX_CNT)
            {
                sleep(1);
                anj_ispctl_ircut_manual_ctrl(0);
                sleep(1);
                anj_ispctl_ircut_manual_ctrl(1);
                ircut_light_test_cnt++;
            }
            ircut_light_test_cnt = 0;

            // 白光灯切换
            while(ircut_light_test_cnt < FACTORY_IRCUT_LIGHT_TEST_MAX_CNT)
            {
                sleep(1);
                anj_ispctl_light_manual_ctrl(1, 100);
                sleep(1);
                anj_ispctl_light_manual_ctrl(1, 0);
                ircut_light_test_cnt++;
            }
            ircut_light_test_cnt = 0;

            // 红外灯切换
            while(ircut_light_test_cnt < FACTORY_IRCUT_LIGHT_TEST_MAX_CNT)
            {
                sleep(1);
                anj_ispctl_light_manual_ctrl(2, 100);
                sleep(1);
                anj_ispctl_light_manual_ctrl(2, 0);
                ircut_light_test_cnt++;
            }
            ircut_light_test_cnt = 0;

            ircut_light_test_complete = 1;
        }

        sleep(1);
    }
#endif
    __INFO("factory hardware test thread exit!\n");

    return 0;
}

static int anj_factory_test_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    __INFO("factory test thread enter\n");

    while (bStart && *bStart)
    {
        anj_mutex_lock(&s_stFactoryMutex);

        if (1 == s_stFactoryInfo.m_BitrateCheck)
        {
            __INFO("factory adjust bitrate\n");
            anj_factory_adjust_bitrate();
            s_stFactoryInfo.m_BitrateCheck = 0;
        }

        // 收到wifi连接时起wifi测试线程
        if (1 == s_stFactoryInfo.m_WifiNewConnect)
        {
            s_stFactoryInfo.m_WifiNewConnect = 0;
            if (0 == s_stFactoryInfo.m_WifiThreadFlag && s_stFactoryWifiTestThread.start == 0)
            {
                iRet = anj_thread_task_create(&s_stFactoryWifiTestThread);
                if (iRet != 0)
                {
                    __ERR("create thread failed\n");
                }

                s_stFactoryInfo.m_WifiThreadFlag = 1;
            }
        }

        // 收到硬件测试时创建硬件测试线程
        if (1 == s_stFactoryInfo.m_IrcutLightTest)
        {
            s_stFactoryInfo.m_IrcutLightTest = 0;
            if (0 == s_stFactoryInfo.m_HardWareThreadFlag && s_stFactoryHardwareTestThread.start == 0)
            {
                iRet = anj_thread_task_create(&s_stFactoryHardwareTestThread);
                if (iRet != 0)
                {
                    __ERR("create thread failed\n");
                }

                s_stFactoryInfo.m_HardWareThreadFlag = 1;
            }

        }

        anj_mutex_unlock(&s_stFactoryMutex);

        usleep(1000 * 1000);
    }

    return 0;
}

int anj_factory_parse_sd_file()
{
    return 0;
}

int anj_factory_init()
{
    int iRet = 0;
    ANJ_CHK((0 == s_stFactoryInfo.m_start_flag), ANJ_FAILURE, "had been init");

    __INFO("factory init\n");

    memset(&s_stFactoryInfo, 0, sizeof(s_stFactoryInfo));
    s_stFactoryInfo.m_start_flag = 1;

    memset(&s_stFactoryTestThread, 0, sizeof(s_stFactoryTestThread));
    memset(&s_stFactoryWifiTestThread, 0, sizeof(s_stFactoryWifiTestThread));
    memset(&s_stFactoryHardwareTestThread, 0, sizeof(s_stFactoryHardwareTestThread));

    anj_factory_parse_sd_file();

    s_stFactoryTestThread.bAutoDestroy = 1;
    strncpy(s_stFactoryTestThread.iThreadName, "anj_factory_test_thread", sizeof(s_stFactoryTestThread.iThreadName));
    s_stFactoryTestThread.iThreadjob.ctx = &s_stFactoryTestThread;
    s_stFactoryTestThread.iThreadjob.func = anj_factory_test_thread;
    iRet = anj_thread_task_create(&s_stFactoryTestThread);
    ANJ_CHK((0 == iRet), iRet, "factory test thread create failed!\n");

    anj_factory_bitrate_check_set();

    if (IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_WIFI || IPC_NETWORK_TYPE == NET_DEV_TYPE_WIFI)
    {
        s_stFactoryWifiTestThread.bAutoDestroy = 1;
        strncpy(s_stFactoryWifiTestThread.iThreadName, "anj_factory_wifi_test_thread", sizeof(s_stFactoryWifiTestThread.iThreadName));
        s_stFactoryWifiTestThread.iThreadjob.ctx = &s_stFactoryWifiTestThread;
        s_stFactoryWifiTestThread.iThreadjob.func = anj_factory_wifi_test_thread;
    }

    s_stFactoryHardwareTestThread.bAutoDestroy = 1;
    strncpy(s_stFactoryHardwareTestThread.iThreadName, "anj_factory_hw_test_thread", sizeof(s_stFactoryHardwareTestThread.iThreadName));
    s_stFactoryHardwareTestThread.iThreadjob.ctx = &s_stFactoryHardwareTestThread;
    s_stFactoryHardwareTestThread.iThreadjob.func = anj_factory_hw_test_thread;

endFunc:
    return iRet;
}

int anj_factory_uninit()
{
    int iRet = 0;
    ANJ_CHK((0 != s_stFactoryInfo.m_start_flag), ANJ_ERR_NOT_INIT, "not init");

    iRet = anj_thread_task_destroy(&s_stFactoryHardwareTestThread, 0);
    if (iRet != 0)
    {
        __ERR("factory hw test exit failed: %d\n", iRet);
    }

    iRet = anj_thread_task_destroy(&s_stFactoryWifiTestThread, 0);
    if (iRet != 0)
    {
        __ERR("actory wifi test exit failed: %d\n", iRet);
    }

    iRet = anj_thread_task_destroy(&s_stFactoryTestThread, 0);
    if (iRet != 0)
    {
        __ERR("actory wifi test exit failed: %d\n", iRet);
    }

    s_stFactoryInfo.m_start_flag = 0;

endFunc:
    return iRet;
}
