#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "anj_config.h"
#include "anj_config_network.h"
#include "anj_config_oem.h"
#include "anj_sysmng.h"
#include "project_option.h"

#include "sadp.h"
#include "hik_sadp_glue.h"

static int s_sadp_inited = 0;

static int hik_sadp_netcfg_cb(unsigned int ip, unsigned int mask, unsigned int gateway,
                              unsigned int port, int bDhcp)
{
    NetworkConfigNew *pNetCfg = NULL;
    LANConfig lan;
    char ip_str[32] = {0};
    char mask_str[32] = {0};
    char gw_str[32] = {0};

    (void)port;

    pNetCfg = (NetworkConfigNew *)getNetWorkConfig();
    if (pNetCfg == NULL)
    {
        __ERR("getNetWorkConfig failed\n");
        return -1;
    }

    memcpy(&lan, &pNetCfg->lanCfg, sizeof(lan));
    lan.dhcpEnable = bDhcp ? 1 : 0;
    get_ip_str(htonl(ip), ip_str, sizeof(ip_str));
    get_ip_str(htonl(mask), mask_str, sizeof(mask_str));
    if (gateway != 0)
    {
        get_ip_str(htonl(gateway), gw_str, sizeof(gw_str));
        strncpy(lan.gateWay, gw_str, sizeof(lan.gateWay) - 1);
    }
    strncpy(lan.IPAddress, ip_str, sizeof(lan.IPAddress) - 1);
    strncpy(lan.netMask, mask_str, sizeof(lan.netMask) - 1);

    if (anj_config_network_lan_set(&lan) != 0)
    {
        __ERR("anj_config_network_lan_set failed\n");
        return -1;
    }

    __INFO("hik sadp netcfg ok ip=%s mask=%s dhcp=%d\n", lan.IPAddress, lan.netMask, lan.dhcpEnable);
    return 0;
}

static int hik_sadp_reset_passwd_cb(char *arg)
{
    (void)arg;
    __WARN("hik sadp reset default password not supported\n");
    return -1;
}

static int hik_sadp_get_passwd_cb(char *buffer, int len)
{
    SystemConfig *pSys = NULL;
    int i = 0;

    if (buffer == NULL || len <= 0)
    {
        return -1;
    }
    buffer[0] = '\0';

    pSys = (SystemConfig *)getSystemConfig();
    if (pSys == NULL)
    {
        return -1;
    }

    for (i = 0; i < MAX_ACCOUNT_COUNT; i++)
    {
        if (pSys->userCfg.accounts[i].userName[0] == '\0')
        {
            continue;
        }
        if (strcmp(pSys->userCfg.accounts[i].userName, "admin") == 0)
        {
            strncpy(buffer, pSys->userCfg.accounts[i].password, (size_t)len - 1);
            return 0;
        }
    }
    if (pSys->userCfg.accounts[0].userName[0] != '\0')
    {
        strncpy(buffer, pSys->userCfg.accounts[0].password, (size_t)len - 1);
        return 0;
    }
    return -1;
}

int hik_sadp_start(void)
{
    MediaStreamConfig *pStreamCfg = NULL;
    NetworkConfigNew *pNetCfg = NULL;
    DevInfo *pDevInfo = NULL;
    AjOemStruct oemInfo;
    dev_info info;
    unsigned char mac[6] = {0};
    char sn[DEVICE_SERIALNO_LEN] = {0};
    char password[64] = {0};
    char ip_str[32] = {0};
    char mask_str[32] = {0};
    char gw_str[32] = {0};
    const char *ifname = WIRE_INTERFACE_NAME;
    unsigned int ip = 0;
    unsigned int mask = 0;
    unsigned int gateway = 0;

    if (s_sadp_inited)
    {
        return 0;
    }

    pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    pNetCfg = (NetworkConfigNew *)getNetWorkConfig();
    pDevInfo = getDevInfo();
    if (pStreamCfg == NULL || pNetCfg == NULL)
    {
        __ERR("hik sadp config null\n");
        return -1;
    }

    memset(&info, 0, sizeof(info));
    memset(&oemInfo, 0, sizeof(oemInfo));

    anj_sysmng_load_sn(sn, sizeof(sn));
    snprintf(info.serial_no, sizeof(info.serial_no), "%.*s",
             (int)sizeof(info.serial_no) - 1, sn);

    if (pDevInfo != NULL && pDevInfo->stVersionInfo.fsVersion[0] != '\0')
    {
        snprintf(info.software_version, sizeof(info.software_version), "%.*s",
                 (int)sizeof(info.software_version) - 1, pDevInfo->stVersionInfo.fsVersion);
    }
    else
    {
        snprintf(info.software_version, sizeof(info.software_version), "%s", "anjcam");
    }

    if (anj_config_oem_get(&oemInfo) == 0 && oemInfo.szDeviceType[0] != '\0')
    {
        snprintf(info.dsp_software_version, sizeof(info.dsp_software_version), "%.*s",
                 (int)sizeof(info.dsp_software_version) - 1, oemInfo.szDeviceType);
    }
    else if (pDevInfo != NULL && pDevInfo->search_devicetype[0] != '\0')
    {
        snprintf(info.dsp_software_version, sizeof(info.dsp_software_version), "%.*s",
                 (int)sizeof(info.dsp_software_version) - 1, pDevInfo->search_devicetype);
    }
    else
    {
        anj_sysmng_dev_str_get(info.dsp_software_version);
    }

    info.dev_type = 0x1000 + 29;
    info.enc_cnt = ANJ_CAMERA_MAX_NUMS;
    info.hdisk_cnt = 0;
    info.port = pStreamCfg->hikConfig.port;

    if (Check_Link_Status(WIRE_INTERFACE_NAME))
    {
        ifname = WIRE_INTERFACE_NAME;
    }
    else if (is_network_device_exist(WIFI_INTERFACE_NAME) && is_network_interface_up(WIFI_INTERFACE_NAME))
    {
        ifname = WIFI_INTERFACE_NAME;
    }

    net_get_hwaddr(ifname, mac);
    {
        struct NET_CONFIG netcfg;
        memset(&netcfg, 0, sizeof(netcfg));
        if (net_get_info(ifname, &netcfg) == 0)
        {
            get_ip_str(netcfg.ifaddr, ip_str, sizeof(ip_str));
            get_ip_str(netcfg.netmask, mask_str, sizeof(mask_str));
            get_ip_str(netcfg.gateway, gw_str, sizeof(gw_str));
        }
        else
        {
            snprintf(ip_str, sizeof(ip_str), "%.*s", (int)sizeof(ip_str) - 1,
                     pNetCfg->lanCfg.IPAddress);
            snprintf(mask_str, sizeof(mask_str), "%.*s", (int)sizeof(mask_str) - 1,
                     pNetCfg->lanCfg.netMask);
            snprintf(gw_str, sizeof(gw_str), "%.*s", (int)sizeof(gw_str) - 1,
                     pNetCfg->lanCfg.gateWay);
        }
    }

    ip = ntohl(inet_addr(ip_str));
    mask = inet_addr(mask_str); /* keep same as legacy sadp glue */
    gateway = ntohl(inet_addr(gw_str));

    if (hik_sadp_get_passwd_cb(password, sizeof(password)) != 0)
    {
        strncpy(password, "123456", sizeof(password) - 1);
    }

    if (init_sadp_lib(ip, mask, gateway, mac, password, (int)strlen(password), &info,
                      hik_sadp_netcfg_cb, hik_sadp_reset_passwd_cb, hik_sadp_get_passwd_cb,
                      pStreamCfg->hikConfig.auth) != 0)
    {
        __ERR("init_sadp_lib failed\n");
        return -1;
    }

    start_sadp_cap();
    sadp_login();
    s_sadp_inited = 1;
    __INFO("hik sadp started if=%s ip=%s port=%u\n", ifname, ip_str, info.port);
    return 0;
}

int hik_sadp_stop(void)
{
    if (!s_sadp_inited)
    {
        return 0;
    }
    stop_sadp_cap();
    fini_sadp_lib();
    s_sadp_inited = 0;
    __INFO("hik sadp stopped\n");
    return 0;
}
