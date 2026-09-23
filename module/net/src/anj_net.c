#include <pthread.h>
#include <net/if_arp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <net/if.h>
#include <linux/sockios.h>

#include "anj_comm.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mw_file.h"
#include "anj_mw_crypt.h"
#include "anj_module.h"
#include "anj_config.h"
#include "anj_net.h"
#include "anj_ser.h"
#include "anj_audio.h"
#include "anj_sysmng.h"
#include "eventhub.h"
#include "anj_net_provider.h"
#include "function_list.h"
#include "record_log.h"

#define DEFAULT_IP_ADDR "192.168.200.200"
#define DEFAULT_IP_NETMASK "255.255.255.0"
#define DEFAULT_IP_GATEWAY "192.168.200.1"
#define DEFAULT_WIRE_MAC_ADDR "00:11:11:11:11:11"
#define DEFAULT_WIRELESS_MAC_ADDR "00:11:11:11:11:13"

#define NET_THREAD_SLEEP_TIME (1)
#define CHECK_SAME_IP_TIMES (2 * 60) / NET_THREAD_SLEEP_TIME
#define CHECK_PING_IP_TIMES (1) / NET_THREAD_SLEEP_TIME
#define CHECK_ROUTE_TIMES (6) / NET_THREAD_SLEEP_TIME
#define PING_FAIL_TIMES (60) / CHECK_PING_IP_TIMES
#define NET_CHANGE_MIN_TIMES (3) / NET_THREAD_SLEEP_TIME /*网络切换间隔必须连续3秒状态切换*/
#define CHECK_WIRE_BOOT_TIMES     (120) / NET_THREAD_SLEEP_TIME
#define CHECK_WIRE_P2P_SKIP_TIMES (60) / NET_THREAD_SLEEP_TIME
#define CHECK_WIRE_TX_IDLE_TIMES  (120) / NET_THREAD_SLEEP_TIME
#define WIRE_TX_IDLE_PKT_LIMIT    (150)

#define ANJ_AP_SSID_PREFIX     "AC18-"
#define ANJ_AP_WPA_PSK         "88888888"
#define ANJ_AP_SN_SSID_HEX_OFF 8
#define ANJ_AP_SN_SSID_HEX_LEN 8

struct mii_data
{
    unsigned short phy_id;
    unsigned short reg_num;
    unsigned short val_in;
    unsigned short val_out;
};

typedef struct
{
    int last_p2p_online;
    int boot_cnt;
    int p2p_skip_cnt;
    int tx_cnt;
    int last_tx;
} anj_wire_normal_s;

static int mdio_write(const char *ifname, int phyid, int location, int value)
{
    int skfd = -1;
    struct ifreq ifr;
    struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;

    memset(&ifr, 0, sizeof(ifr));
    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("mdio_write socket failed\n");
        return -1;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0)
    {
        if (errno != ENODEV)
            __ERR("SIOCGMIIPHY on '%s' failed: %s\n", ifname, strerror(errno));
        close(skfd);
        return -1;
    }

    mii->phy_id = phyid;
    mii->reg_num = location;
    mii->val_in = value;
    if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0)
    {
        __ERR("SIOCSMIIREG on %s failed: %s\n", ifr.ifr_name, strerror(errno));
        close(skfd);
        return -1;
    }

    close(skfd);
    return 0;
}

static int get_eth0_tx_packets(int *tx_packets)
{
    FILE *fp = NULL;
    char line[256];
    unsigned long v[10];
    char *p = NULL;

    if (!tx_packets)
        return -1;

    fp = fopen("/proc/net/dev", "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp))
    {
        if (!strstr(line, "eth0:"))
            continue;
        p = strchr(line, ':');
        if (!p)
            break;
        if (sscanf(p + 1, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9]) >= 10)
        {
            *tx_packets = (int)v[9];
            fclose(fp);
            return 0;
        }
        break;
    }
    fclose(fp);
    return -1;
}

static anj_thread_s s_stNetThread;
static volatile int s_stFormatPercent = 0;
static anj_net_info s_stNetInfo;
static const anj_net_4g_ops *s_st4gOps = NULL;
static const anj_net_wifi_ops *s_stWifiOps = NULL;
static const anj_net_ble_ops *s_stBleOps = NULL;
static ADSLConfigNew s_stPppoeApplied;
static int s_iPppoeStarted;
static int s_iPppoeRetryWait;

#define ANJ_PPPOE_DIR "/tmp/ppp"
#define ANJ_PPPOE_OPTIONS ANJ_PPPOE_DIR "/options"
#define ANJ_PPPOE_PPPD "/usr/sbin/pppd"
#define ANJ_PPPOE_PLUGIN "/opt/ch/rp-pppoe.so"

int anj_net_4g_provider_register(const anj_net_4g_ops *ops)
{
    if (!ops)
        return -1;

    s_st4gOps = ops;
    return 0;
}

void anj_net_4g_provider_unregister(const anj_net_4g_ops *ops)
{
    if (s_st4gOps == ops)
        s_st4gOps = NULL;
}

int anj_net_wifi_provider_register(const anj_net_wifi_ops *ops)
{
    if (!ops)
        return -1;

    s_stWifiOps = ops;
    return 0;
}

void anj_net_wifi_provider_unregister(const anj_net_wifi_ops *ops)
{
    if (s_stWifiOps == ops)
        s_stWifiOps = NULL;
}

int anj_net_ble_provider_register(const anj_net_ble_ops *ops)
{
    if (!ops)
        return -1;

    s_stBleOps = ops;
    return 0;
}

void anj_net_ble_provider_unregister(const anj_net_ble_ops *ops)
{
    if (s_stBleOps == ops)
        s_stBleOps = NULL;
}

int anj_net_provider_4g_init(void)
{
    if (s_st4gOps && s_st4gOps->init)
        return s_st4gOps->init();
    return 0;
}

int anj_net_provider_4g_uninit(void)
{
    if (s_st4gOps && s_st4gOps->uninit)
        return s_st4gOps->uninit();
    return 0;
}

void anj_net_provider_4g_set_pause(void)
{
    if (s_st4gOps && s_st4gOps->set_pause)
        s_st4gOps->set_pause();
}

void anj_net_provider_4g_clear_pause(void)
{
    if (s_st4gOps && s_st4gOps->clear_pause)
        s_st4gOps->clear_pause();
}

int anj_net_provider_wifi_init(void)
{
    if (s_stWifiOps && s_stWifiOps->init)
        return s_stWifiOps->init();
    return 0;
}

int anj_net_provider_wifi_uninit(void)
{
    if (s_stWifiOps && s_stWifiOps->uninit)
        return s_stWifiOps->uninit();
    return 0;
}

int anj_net_provider_wifi_status_get(void)
{
    if (s_stWifiOps && s_stWifiOps->status_get)
        return s_stWifiOps->status_get();
    return WIFI_STATUS_NONE;
}

void anj_net_provider_wifi_thread_set(int status)
{
    if (s_stWifiOps && s_stWifiOps->thread_set)
        s_stWifiOps->thread_set(status);
}

int anj_net_provider_wifi_info_get(const char *ifname, void *info)
{
    if (s_stWifiOps && s_stWifiOps->info_get)
        return s_stWifiOps->info_get(ifname, info);
    return -1;
}

int anj_net_provider_wifi_ap_info_get(void *info)
{
    if (s_stWifiOps && s_stWifiOps->ap_info_get)
        return s_stWifiOps->ap_info_get(info);
    return -1;
}

void anj_net_provider_wifi_connect_mode_set(int mode)
{
    if (s_stWifiOps && s_stWifiOps->connect_mode_set)
        s_stWifiOps->connect_mode_set(mode);
}

int anj_net_provider_wifi_generate_wpa_config(void *pstWifiCfg, int wpaCfgType)
{
    if (s_stWifiOps && s_stWifiOps->generate_wpa_config)
        return s_stWifiOps->generate_wpa_config(pstWifiCfg, wpaCfgType);
    return -1;
}

int anj_net_provider_wifi_start_wpa(int iOverTime, void *pstWifiCfg)
{
    if (s_stWifiOps && s_stWifiOps->start_wpa)
        return s_stWifiOps->start_wpa(iOverTime, pstWifiCfg);
    return -1;
}

int anj_net_provider_ble_init(void)
{
    if (s_stBleOps && s_stBleOps->init)
        return s_stBleOps->init();
    return 0;
}

int anj_net_provider_ble_uninit(void)
{
    if (s_stBleOps && s_stBleOps->uninit)
        return s_stBleOps->uninit();
    return 0;
}

int anj_net_provider_ble_recv_config(void)
{
    if (s_stBleOps && s_stBleOps->recv_config)
        return s_stBleOps->recv_config();
    return 0;
}

int anj_net_provider_ble_config_invalid(void)
{
    if (s_stBleOps && s_stBleOps->config_invalid)
        return s_stBleOps->config_invalid();
    return 0;
}

int anj_net_provider_ble_config_pwd_err(void)
{
    if (s_stBleOps && s_stBleOps->config_pwd_err)
        return s_stBleOps->config_pwd_err();
    return 0;
}

int anj_net_provider_ble_connect_fail(void)
{
    if (s_stBleOps && s_stBleOps->connect_fail)
        return s_stBleOps->connect_fail();
    return 0;
}

int anj_net_provider_ble_p2p_ok(char *p2pidBuf)
{
    if (s_stBleOps && s_stBleOps->p2p_ok)
        return s_stBleOps->p2p_ok(p2pidBuf);
    return 0;
}

static void anj_net_rand_mac(unsigned char *mac_addr)
{
    struct timeval tv;
    SystemGetTimeofRun(&tv, NULL);
    int svalud = tv.tv_sec + tv.tv_usec;
    srand(svalud);

    mac_addr[0] = 0;
    mac_addr[1] = rand() % 256;
    mac_addr[2] = rand() % 256;
    mac_addr[3] = rand() % 256;
    mac_addr[4] = rand() % 256;
    mac_addr[5] = rand() % 256;

    mac_addr[5] += rand() % 256;
    mac_addr[4] += rand() % 256;
    mac_addr[3] += rand() % 256;
    mac_addr[2] += rand() % 256;
    mac_addr[1] += rand() % 256;

    __ERR("eth0 new mac is: %02x:%02x:%02x:%02x:%02x:%02x\n",
          mac_addr[0],
          mac_addr[1],
          mac_addr[2],
          mac_addr[3],
          mac_addr[4],
          mac_addr[5]);
}

static int anj_net_fix_mac(unsigned char *sn, unsigned char *mac_addr)
{
    if (sn[0] != 0xEF && sn[0] != 0xFF)
    {
        mac_addr[0] = 0; // fix as 0
        mac_addr[1] = sn[0];
        mac_addr[2] = sn[3];
        mac_addr[3] = sn[4];
        mac_addr[4] = sn[5]; // 0-3 is the serial index
        mac_addr[5] = sn[6];
    }
    else
    {
        mac_addr[0] = ANJ_ALIGN_UP(sn[0], 2) & 0XFF;
        mac_addr[1] = sn[3];
        mac_addr[2] = sn[4];
        mac_addr[3] = sn[5];
        mac_addr[4] = sn[6];
        mac_addr[5] = sn[7];
    }

    __INFO("valid SN, use fixed MAC, eth0 new mac is: %02x:%02x:%02x:%02x:%02x:%02x\n",
           mac_addr[0],
           mac_addr[1],
           mac_addr[2],
           mac_addr[3],
           mac_addr[4],
           mac_addr[5]);

    return 0;
}

static int anj_net_add_default_ip()
{
    struct in_addr ipaddr, netmask, gateway;
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    if (pstNetworkConfig->lanCfg.IPAddress[0] != '\0' &&
        pstNetworkConfig->lanCfg.netMask[0] != '\0' &&
        pstNetworkConfig->lanCfg.gateWay[0] != '\0')
    {
        inet_aton(pstNetworkConfig->lanCfg.IPAddress, &ipaddr);
        inet_aton(pstNetworkConfig->lanCfg.netMask, &netmask);
        inet_aton(pstNetworkConfig->lanCfg.gateWay, &gateway);
    }
    else
    {
        inet_aton(DEFAULT_IP_ADDR, &ipaddr);
        inet_aton(DEFAULT_IP_NETMASK, &netmask);
        inet_aton(DEFAULT_IP_GATEWAY, &gateway);
    }
    net_set_ifaddr(WIRE_INTERFACE_NAME, ipaddr.s_addr);
    net_set_netmask(WIRE_INTERFACE_NAME, netmask.s_addr);
    net_add_gateway(gateway.s_addr);
    return 0;
}

static int anj_net_mac_set(LANConfig *pLanCfg)
{
    int iRet = 0;
    unsigned char sn[256] = {0};
    char mac_str[MAC_ADDRESS_LEN] = {0};
    unsigned char mac_data[6] = {0};
    iRet = anj_sysmng_get_sn(sn, sizeof(sn));
    if (iRet == 0)
    {
        anj_net_fix_mac(sn, mac_data);
    }
    format_mac_addr_from_digit_to_string((char *)mac_data, 6, (char *)mac_str, MAC_ADDRESS_LEN);
    strncpy((char *)pLanCfg->MACAddress, mac_str, MAC_ADDRESS_LEN);
    anj_config_network_save(getNetWorkConfig());
    __INFO("set mac: %s\n", pLanCfg->MACAddress);

    soft_enc_uboot_anlyargs("ethaddr", (char *)pLanCfg->MACAddress);

    set_mac_addr(WIRE_INTERFACE_NAME, (const char *)pLanCfg->MACAddress, DEFAULT_WIRE_MAC_ADDR);

    if (pLanCfg->mtu > 0)
    {
        net_set_mtu(WIRE_INTERFACE_NAME, pLanCfg->mtu);
    }
    return 0;
}

static void anj_net_hostname_init(LANConfig *pLanCfg)
{
    if (strlen(pLanCfg->hostname) == 0)
    {
        unsigned char macBuf[6];
        char hostname[MAX_IP_NAME_LEN] = {0};
        net_get_hwaddr(WIRE_INTERFACE_NAME, (unsigned char *)macBuf);

        snprintf(hostname, sizeof(hostname),
                 "IPCAM-%02X%02X%02X-%s", macBuf[3], macBuf[4], macBuf[5], ANJ_PROJECT_NAME);
        anj_net_hostname_set(hostname);

        strncpy(pLanCfg->hostname, hostname, sizeof(pLanCfg->hostname));
    }
}

static void anj_net_ipaddr_init(LANConfig *pLanCfg)
{
    int dns_set = 0;
    if (pLanCfg->dhcpEnable == 0)
    {
        struct in_addr ipaddr, netmask, gateway;
        inet_aton(pLanCfg->IPAddress, &ipaddr);
        inet_aton(pLanCfg->netMask, &netmask);
        inet_aton(pLanCfg->gateWay, &gateway);
        net_set_ifaddr(WIRE_INTERFACE_NAME, ipaddr.s_addr);
        net_set_netmask(WIRE_INTERFACE_NAME, netmask.s_addr);
        net_add_gateway(gateway.s_addr);
        for (int k = 0; k < 10; k++)
        {
            sleep(1);
            __INFO("before wait if: %s up, try  = %d\n", WIRE_INTERFACE_NAME, k);
            if (is_network_interface_up(WIRE_INTERFACE_NAME))
            {
                __INFO("if: %s is up, try  = %d\n", WIRE_INTERFACE_NAME, k);
                break;
            }
        }

        dns_set = 1;
    }
    else
    {
        anj_net_dhcp_up(WIRE_INTERFACE_NAME);
        __INFO("before add_default_ip\n");
        in_addr_t ipaddr;
        ipaddr = net_get_ifaddr(WIRE_INTERFACE_NAME);
        if (ipaddr == -1)
        {
            anj_net_add_default_ip();
            __INFO("Eth0 does not have an IP address, eth0:0 add default ip\n");
        }
    }

    if (dns_set)
    {
        anj_net_gateway_load(pLanCfg->gateWay);
        __INFO("set dns (dns1:%s, dns2:%s)\n", pLanCfg->DNS1, pLanCfg->DNS2);
        net_set_two_dns(pLanCfg->DNS1, pLanCfg->DNS2, pLanCfg->gateWay);
    }
}

static int anj_net_check_config_vaild(LANConfig *pLanCfg)
{
    if (ValidIpv4(pLanCfg->IPAddress) != 1 ||
        ValidIpv4(pLanCfg->netMask) != 1)
    {
        return 0;
    }

    if (strlen(pLanCfg->gateWay) > 0 &&
        ValidIpv4(pLanCfg->gateWay) != 1)
    {
        return 0;
    }
    if (!strcmp(pLanCfg->IPAddress, "0.0.0.0") ||
        !strcmp(pLanCfg->IPAddress, "255.255.255.255") ||
        !strcmp(pLanCfg->netMask, "0.0.0.0") ||
        !strcmp(pLanCfg->gateWay, "255.255.255.255") ||
        !strcmp(pLanCfg->gateWay, "0.0.0.0"))
    {
        return 0;
    }

    in_addr_t addr_mask;
    addr_mask = inet_addr(pLanCfg->netMask);

    in_addr_t addr_ip;
    addr_ip = inet_addr(pLanCfg->IPAddress);

    if (((~addr_mask) & addr_ip) == 0)
    {
        __ERR("ip addr is 0\n");
        return 0;
    }

    if (strlen(pLanCfg->gateWay) > 0 && strcmp(pLanCfg->gateWay, "0.0.0.0") != 0)
    {
        in_addr_t addr_gw;
        addr_gw = inet_addr(pLanCfg->gateWay);
        if (((~addr_mask) & addr_gw) == 0 || ((~addr_mask) & addr_gw) == 0xff000000)
        {
            __ERR("gw addr is 0, or 255(gw ip=%s)\n", pLanCfg->gateWay);
            return 0;
        }
    }
    return 1;
}

static void anj_net_wire_normal_check(anj_wire_normal_s *pst, anj_ser_info *pstSerInfo)
{
    int tx = 0;

    if (!pst || !pstSerInfo)
        return;
    if (is_network_connect(WIRE_INTERFACE_NAME) == 0)
        return;

    if (pst->boot_cnt < CHECK_WIRE_BOOT_TIMES)
    {
        pst->boot_cnt++;
        return;
    }

    if (pst->p2p_skip_cnt > 0)
    {
        pst->p2p_skip_cnt--;
        return;
    }

    if (pstSerInfo->stP2pLoginState.logined)
    {
        pst->last_p2p_online = 1;
        return;
    }

    if (pst->last_p2p_online)
    {
        __ERR("p2p offline , reset eth phy\n");
        mdio_write(WIRE_INTERFACE_NAME, 0, 0, 0xb100);
        usleep(3000 * 1000);
        mdio_write(WIRE_INTERFACE_NAME, 0, 0, 0x3100);
    }
    pst->last_p2p_online = 0;

    pst->tx_cnt++;
    if (pst->tx_cnt < CHECK_WIRE_TX_IDLE_TIMES)
        return;
    pst->tx_cnt = 0;

    if (get_eth0_tx_packets(&tx) != 0)
        return;

    if (pst->last_tx < 0)
    {
        pst->last_tx = tx;
        return;
    }

    __ERR("tx_packets = %d, last_tx=%d\n", tx, pst->last_tx);
    if (tx - pst->last_tx <= WIRE_TX_IDLE_PKT_LIMIT)
    {
        __ERR("no data trans 2 mins, reset phy\n");
        mdio_write(WIRE_INTERFACE_NAME, 0, 0, 0xb100);
        usleep(3000 * 1000);
        mdio_write(WIRE_INTERFACE_NAME, 0, 0, 0x3100);
    }
    pst->last_tx = -1;
}

static int anj_net_pppoe_enable(void)
{
    NetworkConfigNew *cfg = (NetworkConfigNew *)getNetWorkConfig();

    if (cfg == NULL)
    {
        return 0;
    }
    return (cfg->adslCfg.enable != 0 && cfg->adslCfg.userName[0] != '\0');
}

static int anj_net_dhcp_enable(void)
{
    NetworkConfigNew *cfg = (NetworkConfigNew *)getNetWorkConfig();

    if (cfg == NULL || anj_net_pppoe_enable())
    {
        return 0;
    }
    return (cfg->lanCfg.dhcpEnable != 0);
}

const char *anj_net_wan_ifname(void)
{
    if (s_stNetInfo.eStatus == ANJ_NET_STATUS_WIRE && anj_net_pppoe_enable())
    {
        return PPPD_INTERFACE_NAME;
    }
    if (s_stNetInfo.eStatus == ANJ_NET_STATUS_WIFI)
    {
        return WIFI_INTERFACE_NAME;
    }
    if (s_stNetInfo.eStatus == ANJ_NET_STATUS_4G)
    {
        return WIRE_INTERFACE_NAME1;
    }
    if (is_network_device_exist(BRIDGE_INTERFACE_NAME))
    {
        return BRIDGE_INTERFACE_NAME;
    }
    return WIRE_INTERFACE_NAME;
}

static void anj_net_pppoe_stop(void)
{
    if (anj_sysmng_check_process("pppd") == 0)
    {
        anj_mw_system("killall pppd");
        usleep(200 * 1000);
    }
    s_iPppoeStarted = 0;
    memset(&s_stPppoeApplied, 0, sizeof(s_stPppoeApplied));
}

static int anj_net_pppoe_start(const NetworkConfigNew *cfg)
{
    char user[ADSL_NAME_MAX_LEN * 2] = {0};
    char pass[ADSL_PASSWORD_MAX_LEN * 2] = {0};
    char cmd[512] = {0};
    FILE *fp = NULL;

    if (cfg == NULL)
    {
        return -1;
    }

    anj_mw_system("mkdir -p " ANJ_PPPOE_DIR);
    anj_mw_system("killall udhcpc");
    if (!is_network_interface_up(WIRE_INTERFACE_NAME))
    {
        net_set_up(WIRE_INTERFACE_NAME);
    }

    copy_with_quoted_escape(user, sizeof(user), cfg->adslCfg.userName);
    copy_with_quoted_escape(pass, sizeof(pass), cfg->adslCfg.password);

    fp = fopen(ANJ_PPPOE_OPTIONS, "w");
    if (fp == NULL)
    {
        __ERR("open %s fail\n", ANJ_PPPOE_OPTIONS);
        return -1;
    }
    fprintf(fp, "user \"%s\"\n", user);
    fprintf(fp, "password \"%s\"\n", pass);
    fprintf(fp,
            "noipdefault\n"
            "defaultroute\n"
            "replacedefaultroute\n"
            "usepeerdns\n"
            "persist\n"
            "maxfail 0\n"
            "holdoff 10\n"
            "mtu 1492\n"
            "mru 1492\n"
            "lcp-echo-interval 20\n"
            "lcp-echo-failure 4\n"
            "noauth\n"
            "hide-password\n");
    fclose(fp);

    anj_net_pppoe_stop();
    snprintf(cmd, sizeof(cmd), "%s plugin %s nic-%s file %s &",
             ANJ_PPPOE_PPPD, ANJ_PPPOE_PLUGIN, WIRE_INTERFACE_NAME, ANJ_PPPOE_OPTIONS);
    __INFO("call cmd = %s\n", cmd);
    if (anj_mw_system(cmd))
    {
        __ERR("start pppd fail\n");
        return -1;
    }
    s_iPppoeStarted = 1;
    memcpy(&s_stPppoeApplied, &cfg->adslCfg, sizeof(s_stPppoeApplied));
    s_stNetInfo.reset = 1;
    return 0;
}

static void anj_net_pppoe_check(NetworkConfigNew *cfg, anj_net_status_e eNetStatus)
{
    int enable;

    if (cfg == NULL)
    {
        return;
    }
    enable = (eNetStatus == ANJ_NET_STATUS_WIRE) &&
             anj_net_pppoe_enable();
    if (!enable)
    {
        if (s_iPppoeStarted || anj_sysmng_check_process("pppd") == 0)
        {
            __INFO("pppoe stop\n");
            anj_net_pppoe_stop();
            if (eNetStatus == ANJ_NET_STATUS_WIRE)
            {
                anj_net_ipaddr_init(&cfg->lanCfg);
            }
        }
        s_iPppoeRetryWait = 0;
        return;
    }

    if (memcmp(&s_stPppoeApplied, &cfg->adslCfg, sizeof(s_stPppoeApplied)) != 0)
    {
        s_iPppoeRetryWait = 0;
        if (anj_net_pppoe_start(cfg) != 0)
        {
            s_iPppoeRetryWait = 10;
        }
        return;
    }
    if (anj_sysmng_check_process("pppd") != 0)
    {
        if (s_iPppoeRetryWait > 0)
        {
            s_iPppoeRetryWait--;
            return;
        }
        __WARN("pppd not running, redial\n");
        if (anj_net_pppoe_start(cfg) != 0)
        {
            s_iPppoeRetryWait = 10;
        }
    }
}

static int anj_net_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    int mac[6] = {0};
    int bWirePlayed = 0;
    char szMac[20] = {0};
    char localip[64] = {0};
    char localmac[32] = {0};
    int NetStatusChange = 0;
    anj_check_info mCheckInfo = {0};
    anj_wire_normal_s stWireNormal = {0};
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    anj_ser_info *pstSerInfo = getSerInfo();
    DevInfo *pstDevInfo = getDevInfo();

    stWireNormal.last_tx = -1;
    anj_audio_prompt_play(ANJ_MP3_DEVICE_PATH, ANJ_MP3_DEVICE_STARTUP, 1);

    while (bStart && *bStart)
    {
        if (pstDevInfo->bFactoryMode)
        {
            sleep(NET_THREAD_SLEEP_TIME);
            continue;
        }

        if (s_stNetInfo.reset)
        {
            s_stNetInfo.reset = 0;
            memset(&mCheckInfo, 0, sizeof(mCheckInfo));
        }
        anj_net_status_e eNetStatus = anj_net_status_check();
        anj_net_pppoe_check(pstNetworkConfig, eNetStatus);
        // __INFO("anj_net_thread: eNetStatus = %d %d\n", eNetStatus, s_stNetInfo.eStatus);

        if (eNetStatus == ANJ_NET_STATUS_WIRE && anj_mw_file_exists(P2P_DEVICEBIND_FLAG) == 0 && bWirePlayed == 0)
        {
            bWirePlayed = 1;
            if (IPC_NETWORK_TYPE != NET_DEV_TYPE_WIRE_4G || ANJ_CUSTOMER_TYPE != CUSTOMER_WTD)
            {
                anj_audio_prompt_play(ANJ_MP3_NETWORK_PATH, ANJ_MP3_CONNECTING_WIRE, 1);
            }
        }

        if (mCheckInfo.iCheckSameIpCnt >= CHECK_SAME_IP_TIMES)
        {
            mCheckInfo.iCheckSameIpCnt = 0;
            if (eNetStatus == ANJ_NET_STATUS_WIFI && (anj_sysmng_check_process("wpa_supplicant") == 0))
            {
                memset(localip, 0, sizeof(localip));
                net_local_ip(WIFI_INTERFACE_NAME, localip, sizeof(localip));

                memset(mac, 0, sizeof(mac));
                memset(szMac, 0, sizeof(szMac));
                memset(localmac, 0, sizeof(localmac));
                net_get_hwaddr(WIFI_INTERFACE_NAME, (unsigned char *)szMac);
                snprintf(szMac, sizeof(szMac), "%02x:%02x:%02x:%02x:%02x:%02x",
                         (szMac[0] & 0377), (szMac[1] & 0377), (szMac[2] & 0377),
                         (szMac[3] & 0377), (szMac[4] & 0377), (szMac[5] & 0377));

                sscanf(szMac, "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
                for (int i = 0; i < sizeof(mac) / sizeof(*mac); i++)
                {
                    localmac[i] = mac[i];
                }

                unsigned int destIp = inet_addr(localip);
                unsigned int sourceIp = inet_addr(localip);
                __INFO("start send arping...\n");
                iRet = arpping(destIp, sourceIp, localmac, 1500, "wlan0");
                if (iRet)
                {
                    __INFO("wlan0 same ip:%s\n", localip);
                    __RECORD_LOG_INFO("wlan0 same ip:%s\n", localip);
                    __INFO("Restart_wlan0_udhcpc\n");
                    anj_net_dhcp_up(WIFI_INTERFACE_NAME);
                }
            }
            else if (eNetStatus == ANJ_NET_STATUS_WIRE)
            {
                if (anj_net_dhcp_enable())
                {
                    memset(localip, 0, sizeof(localip));
                    net_local_ip("eth0", localip, sizeof(localip));

                    memset(mac, 0, sizeof(mac));
                    memset(szMac, 0, sizeof(szMac));
                    memset(localmac, 0, sizeof(localmac));
                    net_get_hwaddr(WIRE_INTERFACE_NAME, (unsigned char *)szMac);
                    snprintf(szMac, sizeof(szMac), "%02x:%02x:%02x:%02x:%02x:%02x",
                             (szMac[0] & 0377), (szMac[1] & 0377), (szMac[2] & 0377),
                             (szMac[3] & 0377), (szMac[4] & 0377), (szMac[5] & 0377));

                    sscanf(szMac, "%02x:%02x:%02x:%02x:%02x:%02x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
                    for (int i = 0; i < sizeof(mac) / sizeof(*mac); i++)
                    {
                        localmac[i] = mac[i];
                    }

                    unsigned int destIp = inet_addr(localip);
                    unsigned int sourceIp = inet_addr(localip);
                    __INFO("start send arping...\n");
                    iRet = arpping(destIp, sourceIp, localmac, 1500, WIRE_INTERFACE_NAME);
                    if (iRet)
                    {
                        __INFO("wire same ip:%s\n", localip);
                        __RECORD_LOG_INFO("wire same ip:%s\n", localip);
                        __INFO("Restart udhcpc\n");
                        anj_net_dhcp_up(WIRE_INTERFACE_NAME);
                    }
                }
            }
        }

        if (mCheckInfo.iCheckPingIpCnt >= CHECK_PING_IP_TIMES)
        {
            mCheckInfo.iCheckPingIpCnt = 0;
            if (eNetStatus == ANJ_NET_STATUS_WIFI && (anj_sysmng_check_process("wpa_supplicant") == 0))
            {
                /*ping 路由网关1分钟不通则重启udhcpc*/
                if (0 != anj_net_gw_ping(WIFI_INTERFACE_NAME))
                {
                    mCheckInfo.iPingFailTimes++;
                    if (mCheckInfo.iPingFailTimes >= PING_FAIL_TIMES)
                    {
                        __INFO("Restart_wlan0_udhcpc\n");
                        __RECORD_LOG_INFO("Restart_wlan0_udhcpc\n");
                        anj_net_dhcp_up(WIFI_INTERFACE_NAME);
                        mCheckInfo.iPingFailTimes = 0;
                    }
                }
                else
                {
                    mCheckInfo.iPingFailTimes = 0;
                }
            }
            else if (eNetStatus == ANJ_NET_STATUS_WIRE)
            {
                if (anj_net_pppoe_enable())
                {
                    if (0 != anj_net_gw_ping(anj_net_wan_ifname()))
                    {
                        mCheckInfo.iPingFailTimes++;
                        if (mCheckInfo.iPingFailTimes >= PING_FAIL_TIMES)
                        {
                            __INFO("pppoe gw ping fail, redial\n");
                            __RECORD_LOG_INFO("pppoe gw ping fail, redial\n");
                            anj_net_pppoe_stop();
                            s_iPppoeRetryWait = 0;
                            mCheckInfo.iPingFailTimes = 0;
                        }
                    }
                    else
                    {
                        mCheckInfo.iPingFailTimes = 0;
                        mCheckInfo.iWireFailAudioPlayed = 0;
                    }
                }
                else if (anj_net_dhcp_enable())
                {
                    /*ping 路由网关1分钟不通则重启udhcpc*/
                    if (0 != anj_net_gw_ping(WIRE_INTERFACE_NAME))
                    {
                        mCheckInfo.iPingFailTimes++;
                        if (mCheckInfo.iPingFailTimes >= PING_FAIL_TIMES)
                        {
                            if (mCheckInfo.iWireFailAudioPlayed == 0 && anj_mw_file_exists(P2P_DEVICEBIND_FLAG) == 0)
                            {
                                if (IPC_NETWORK_TYPE != NET_DEV_TYPE_WIRE_4G || ANJ_CUSTOMER_TYPE != CUSTOMER_WTD)
                                {
                                    anj_audio_prompt_play(ANJ_MP3_NETWORK_PATH, ANJ_MP3_CONNECT_WIRE_FAIL, 2);
                                }
                                mCheckInfo.iWireFailAudioPlayed = 1;
                            }
                            __INFO("Restart_eth0_udhcpc\n");
                            __RECORD_LOG_INFO("Restart_eth0_udhcpc\n");
                            anj_net_dhcp_up(WIRE_INTERFACE_NAME);
                            mCheckInfo.iPingFailTimes = 0;
                        }
                    }
                    else
                    {
                        mCheckInfo.iPingFailTimes = 0;
                        mCheckInfo.iWireFailAudioPlayed = 0;
                    }
                }
            }
            // 有线网络持续1分钟没连接服务器 播放语音提示
            if (eNetStatus == ANJ_NET_STATUS_WIRE)
            {
                mCheckInfo.iLoginFailTimes++;
                if (pstSerInfo->stP2pLoginState.enable && pstSerInfo->stP2pLoginState.logined == 0)
                {
                    if (mCheckInfo.iLoginFailTimes == PING_FAIL_TIMES &&
                        mCheckInfo.iWireFailAudioPlayed == 0 && anj_mw_file_exists(P2P_DEVICEBIND_FLAG) == 0)
                    {
                        if (IPC_NETWORK_TYPE != NET_DEV_TYPE_WIRE_4G || ANJ_CUSTOMER_TYPE != CUSTOMER_WTD)
                        {
                            anj_audio_prompt_play(ANJ_MP3_NETWORK_PATH, ANJ_MP3_CONNECT_WIRE_FAIL, 2);
                        }
                        mCheckInfo.iWireFailAudioPlayed = 1;
                    }
                }
                else
                {
                    mCheckInfo.iLoginFailTimes = 0;
                    if (mCheckInfo.iPingFailTimes == 0)
                    {
                        mCheckInfo.iWireFailAudioPlayed = 0;
                    }
                }
            }
        }

        if (mCheckInfo.iCheckRouteCnt > CHECK_ROUTE_TIMES)
        {
            mCheckInfo.iCheckRouteCnt = 0;
            anj_net_check_route(eNetStatus);
        }

        mCheckInfo.iCheckSameIpCnt++;
        mCheckInfo.iCheckPingIpCnt++;
        mCheckInfo.iCheckRouteCnt++;

        if (eNetStatus == ANJ_NET_STATUS_WIRE)
        {
            if (anj_net_dhcp_enable())
            {
                if ((is_dhcp_running(WIRE_INTERFACE_NAME) == 0) && (anj_sysmng_check_process("udhcpc") != 0))
                {
                    __ERR("start dhcp again.\n");
                    anj_net_dhcp_up(WIRE_INTERFACE_NAME);
                }
            }
        }
        else if (eNetStatus == ANJ_NET_STATUS_WIFI && pstNetworkConfig->wifiCfg.dhcpEnable)
        {
            if (WIFI_STATUS_CONNECTED == anj_net_provider_wifi_status_get())
            {
                if ((is_dhcp_running(WIFI_INTERFACE_NAME) == 0) && (anj_sysmng_check_process("udhcpc") != 0))
                {
                    __ERR("start dhcp again.\n");
                    anj_net_dhcp_up(WIFI_INTERFACE_NAME);
                }
            }
        }

        if (eNetStatus != s_stNetInfo.eStatus)
        {
            NetStatusChange++;
            // 网络切换需要稳定3秒才能切换，避免修改IP等抖动出现切换
            if (NetStatusChange > NET_CHANGE_MIN_TIMES)
            {
                stWireNormal.p2p_skip_cnt = CHECK_WIRE_P2P_SKIP_TIMES;
                anj_ser_reset_conn(eNetStatus);
                EventResult event_result = {0};
                eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_RTSP_RESTART, &event_result, NULL);
                eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_RTMP_RESTART, &event_result, NULL);
                s_stNetInfo.eStatus = eNetStatus;

                /*网卡切换状态更新*/
                memset(&mCheckInfo, 0, sizeof(mCheckInfo));

                if (eNetStatus == ANJ_NET_STATUS_WIRE)
                {
                    anj_net_provider_4g_set_pause();
                    anj_net_provider_wifi_thread_set(THREAD_STATUS_WAIT);

                    anj_net_set();
                    __INFO("wire insert!\n");
                    if (anj_mw_file_exists(P2P_DEVICEBIND_FLAG) == 0)
                    {
                        if (IPC_NETWORK_TYPE != NET_DEV_TYPE_WIRE_4G || ANJ_CUSTOMER_TYPE != CUSTOMER_WTD)
                        {
                            anj_audio_prompt_play(ANJ_MP3_NETWORK_PATH, ANJ_MP3_CONNECTING_WIRE, 1);
                        }
                        sleep(2); // Turn off wave and zxing after a 2-second delay
                        int start = 0;
                        EventResult event_result = {0};
                        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_WAVE_SET_STATUS, &event_result, (int *)&start);
                        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_ZXING_SET_STATUS, &event_result, (int *)&start);
                        anj_net_provider_ble_uninit();
                    }
                }
                else if (eNetStatus == ANJ_NET_STATUS_WIFI)
                {
                    anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_CONFIG_NET, 1);
                    __INFO("wire remove wifi start!\n");
                    anj_net_provider_wifi_thread_set(THREAD_STATUS_RUNNING);
                }
                else if (eNetStatus == ANJ_NET_STATUS_4G)
                {
                    anj_net_provider_4g_clear_pause();
                    __INFO("wire remove 4G start!\n");
                }
                if (eNetStatus == ANJ_NET_STATUS_WIFI || eNetStatus == ANJ_NET_STATUS_4G)
                {
                    if (anj_mw_file_exists(P2P_DEVICEBIND_FLAG) == 0)
                    {
                        sleep(5);
                        int start = 1;
                        EventResult event_result = {0};
                        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_WAVE_SET_STATUS, &event_result, (int *)&start);
                        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_ZXING_SET_STATUS, &event_result, (int *)&start);
                        anj_net_provider_ble_init();
                    }
                }
                NetStatusChange = 0;
            }
        }
        else
        {
            NetStatusChange = 0;
        }

        if (eNetStatus == ANJ_NET_STATUS_WIRE)
            anj_net_wire_normal_check(&stWireNormal, pstSerInfo);

        sleep(NET_THREAD_SLEEP_TIME);
    }
    return iRet;
}

int anj_net_set()
{
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    if (strlen(pstNetworkConfig->lanCfg.hostname))
    {
        anj_net_hostname_set(pstNetworkConfig->lanCfg.hostname);
    }
    if (!anj_net_check_config_vaild(&pstNetworkConfig->lanCfg))
    {
        __ERR("network config invalid , use default config\n");

        NetworkConfigNew stNetworkConfig;
        char *pDefFile = anj_config_default_file_get();
        int iRet = -1;
        if (pDefFile != NULL && pDefFile[0] != 0)
        {
            iRet = anj_config_load("NetworkConfig", &stNetworkConfig, toLowerStr(pDefFile));
        }

        if (iRet == 0)
        {
            memcpy(&pstNetworkConfig->lanCfg, &stNetworkConfig.lanCfg, sizeof(pstNetworkConfig->lanCfg));
        }
        else
        {
            strcpy(pstNetworkConfig->lanCfg.IPAddress, "192.168.0.123");
            strcpy(pstNetworkConfig->lanCfg.netMask, "255.255.255.0");
            strcpy(pstNetworkConfig->lanCfg.gateWay, "192.168.0.1");
        }
        anj_config_network_save(pstNetworkConfig);
    }

    anj_net_reset();
    anj_net_mac_set(&pstNetworkConfig->lanCfg);
    anj_net_hostname_init(&pstNetworkConfig->lanCfg);
    anj_net_ipaddr_init(&pstNetworkConfig->lanCfg);
    return 0;
}

int anj_net_init(void)
{
    int iRet = 0;
    memset(&s_stNetThread, 0, sizeof(anj_thread_s));
    s_stNetInfo.eStatus = anj_net_status_check();
    // 无线启动 网卡状态由net线程更新
    if (s_stNetInfo.eStatus != ANJ_NET_STATUS_WIRE)
    {
        s_stNetInfo.eStatus = ANJ_NET_STATUS_NONE;
    }
    anj_net_set();

    if ((IPC_PROMPT_WIRE_USE_DETAIL && (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)) ||
        IPC_NETWORK_TYPE == NET_DEV_TYPE_4G || IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_4G)
    {
        anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_DEVICE_START, 1);
    }

    s_stNetThread.bAutoDestroy = 1;
    strncpy(s_stNetThread.iThreadName, "anj_net_thread", sizeof(s_stNetThread.iThreadName) - 1);
    s_stNetThread.iThreadjob.ctx = &s_stNetThread;
    s_stNetThread.iThreadjob.func = anj_net_thread;
    iRet = anj_thread_task_create(&s_stNetThread);
    anj_net_provider_4g_init();
    anj_net_provider_wifi_init();

    return iRet;
}

int anj_net_uninit(void)
{
    anj_net_provider_4g_uninit();
    anj_net_provider_wifi_uninit();
    anj_thread_task_destroy(&s_stNetThread, -1);
    return 0;
}

anj_net_status_e anj_net_status_check()
{
    anj_net_status_e eStatus = ANJ_NET_STATUS_WIRE;
    if (is_network_connect(WIRE_INTERFACE_NAME) == 0)
    {
        if (NET_DEV_TYPE_WIRE_4G == IPC_NETWORK_TYPE ||
            NET_DEV_TYPE_4G == IPC_NETWORK_TYPE)
        {
            eStatus = ANJ_NET_STATUS_4G;
        }
        else if (NET_DEV_TYPE_WIRE_WIFI == IPC_NETWORK_TYPE ||
                 NET_DEV_TYPE_WIFI == IPC_NETWORK_TYPE)
        {
            eStatus = ANJ_NET_STATUS_WIFI;
        }
    }

    return eStatus;
}

int anj_net_gw_ping(const char *ifname)
{
    if (NULL == ifname)
    {
        __ERR("PING ERROR!\n");
        return -1;
    }

    char gw[64] = {0};
    net_local_gw(ifname, gw, sizeof(gw));
    if (0 != try_ping(gw, 5000, 1, NULL, NULL))
    {
        __ERR("ping gw:%s failed!\n", gw);
        return -1;
    }

    return 0;
}

int anj_net_mac_create_by_sn(unsigned char *mac_addr)
{
    unsigned char sn[256] = {0};

    if (anj_sysmng_get_sn(sn, sizeof(sn)) < 0)
    {
        anj_net_rand_mac(mac_addr);
        return 1;
    }

    if (anj_sysmng_sn_validate((unsigned char *)sn) < 0)
    {
        anj_net_rand_mac(mac_addr);
    }
    else // new format serial number
    {
        anj_net_fix_mac(sn, mac_addr);
    }

    return 1;
}

void anj_net_hostname_set(const char *hostname)
{
    char cmd[128] = {0};
    snprintf(cmd, sizeof(cmd), "hostname %s", hostname);
    anj_mw_system(cmd);
}

int anj_net_gateway_load(char *gwip)
{
    if (gwip)
    {
        char cmd[256] = {0};

        sprintf(cmd, "route del default");
        __INFO("cmd: %s\n", cmd);

        if (anj_mw_system(cmd))
        {
            __ERR("route del default error.\n");
        }

        snprintf(cmd, sizeof(cmd), "route add default gw %s dev %s", gwip, WIRE_INTERFACE_NAME);
        __INFO("cmd: %s\n", cmd);

        if (anj_mw_system(cmd))
        {
            __ERR("add route error.\n");
        }

        __INFO("run route command to fresh route table\n");
        anj_mw_system("killall route");
        anj_mw_system("route &");
    }

    return 0;
}

int anj_net_check_gateway_exist(const char *ifrname, char *gateway)
{
    int iRet = 0;
    char szStr[256] = {0};
    char current_gateway[256] = {0};
    char cmd[320] = {0};
    FILE *fp = NULL;
    memset(&szStr, 0, sizeof(szStr));
    snprintf(szStr, (sizeof(szStr) - 1), "route  -n | grep %s | grep U | awk '$1 == \"0.0.0.0\"{print $2}'", ifrname);

    fp = popen(szStr, "r");
    if (fp == NULL)
    {
        __INFO("popen failed\n");
    }
    else
    {
        memset(&szStr, 0, sizeof(szStr));
        if (NULL != fgets(szStr, 20, fp))
        {
            /*因为szStr获取出来的网关字符串末尾有\n*/
            snprintf(current_gateway, sizeof(current_gateway), "%s", szStr);
            if (0 != strcmp(gateway, current_gateway))
            {
                // __INFO("gateway:%s, current_gateway:%s, net:%s\n", gateway, current_gateway, ifrname);
                snprintf(cmd, sizeof(cmd), "route del default gw %s", current_gateway);
                anj_mw_system(cmd);
                usleep(50 * 1000);
                snprintf(cmd, sizeof(cmd), "route add default gw %s dev %s", gateway, ifrname);
                anj_mw_system(cmd);
                iRet = 1;
            }
        }
        else
        {
            __INFO("current no default gateway\n");
            snprintf(cmd, sizeof(cmd), "route add default gw %s dev %s", gateway, ifrname);
            anj_mw_system(cmd);
            iRet = 1;
        }
        pclose(fp);
    }

    return iRet;
}

int anj_net_add_route(const char *ifname)
{
    int iRet = 0;

    if (ifname == NULL)
    {
        return -1;
    }

    if (net_get_ifaddr(ifname) == -1)
    {
        __ERR("get %s ip failed!\n", ifname);
        return -1;
    }

    char gw[64] = {0};
    net_local_gw(ifname, gw, sizeof(gw));
    if ((!is_dhcp_running(WIFI_INTERFACE_NAME)) && (strcmp(ifname, WIFI_INTERFACE_NAME) == 0))
    {
        NetworkConfigNew stNetworkConfig;
        memcpy(&stNetworkConfig, (NetworkConfigNew *)getNetWorkConfig(), sizeof(NetworkConfigNew));
        memset(gw, 0, sizeof(gw));
        memcpy(gw, stNetworkConfig.wifiCfg.gateWay, sizeof(stNetworkConfig.wifiCfg.gateWay));
        iRet = 0;
    }
    if (iRet == 0 && strlen(gw) > 0)
    {
        anj_net_check_gateway_exist(ifname, gw);
    }
    else
    {
        __ERR("get %s dhcp gw failed!\n", ifname);
    }

    return 0;
}

int anj_net_dhcp_up(char *ifname)
{
    int iRet = 0;
    char cmd[256] = {0};

    s_stNetInfo.reset = 1;
    anj_mw_system("killall udhcpc");

    if (!is_network_interface_up(ifname))
    {
        net_set_up(ifname);
    }
    unsigned char macBuf[6];
    char hostname[MAX_IP_NAME_LEN] = {0};
    net_get_hwaddr(WIRE_INTERFACE_NAME, (unsigned char *)macBuf);

    snprintf(hostname, sizeof(hostname),
             "IPCAM-%02X%02X%02X-%s", macBuf[3], macBuf[4], macBuf[5], ANJ_PROJECT_NAME);

    sprintf(cmd, "udhcpc -i %s -x hostname:%s &", ifname, hostname);
    __INFO("call cmd = %s\n", cmd);
    if (anj_mw_system(cmd))
    {
        __ERR("call cmd(%s) fail\n", cmd);
        iRet = -1;
    }

    return iRet;
}

int anj_net_check_route(int netWorkStatus)
{
    if (ANJ_NET_STATUS_WIRE == netWorkStatus)
    {
        if (!anj_net_pppoe_enable())
        {
            in_addr_t ipaddr;
            ipaddr = net_get_ifaddr(WIRE_INTERFACE_NAME);
            if (ipaddr == INADDR_NONE)
            {
                anj_net_add_default_ip();
                __WARN("Eth0 does not have an IP address, eth0:0 add default ip\n");
            }
        }
    }
    else if (ANJ_NET_STATUS_WIFI == netWorkStatus || ANJ_NET_STATUS_4G == netWorkStatus)
    {
        net_del_ip(WIRE_INTERFACE_NAME);
    }
    else
    {
        return 0;
    }

    anj_net_add_route(anj_net_wan_ifname());
    return 0;
}

int anj_net_mac_set_by_sn()
{
    char szInitMac[32] = {0};
    unsigned char macaddr[6] = {0};
    anj_net_mac_create_by_sn(macaddr);
    format_mac_addr_from_digit_to_string((char *)macaddr, 6, (char *)szInitMac, MAC_ADDRESS_LEN);

    __ERR("Set init mac: %s", (char *)szInitMac);
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();
    strcpy((char *)pstNetworkConfig->lanCfg.MACAddress, (char *)szInitMac);
    anj_config_network_save(pstNetworkConfig);

    __WARN("set init mac, reboot\n");
    __RECORD_LOG_INFO("set init mac, reboot\n");
    anj_sysmng_delay_reboot(5);
    return 0;
}

int anj_net_info_get(NETWORK_STATUS_DATA *networkStatus)
{
    int iRet;
    NetworkConfigNew *pstNetworkConfig = (NetworkConfigNew *)getNetWorkConfig();

    memset(networkStatus, 0, sizeof(NETWORK_STATUS_DATA));

    net_get_two_dns(networkStatus->dns1, sizeof(networkStatus->dns1), networkStatus->dns2, sizeof(networkStatus->dns2));
    if (strlen(networkStatus->dns1) != 0)
    {
        networkStatus->dns1Exsit = 1;
    }

    if (strlen(networkStatus->dns2) != 0)
    {
        networkStatus->dns2Exsit = 1;
    }

    networkStatus->doBridge = is_network_device_exist(BRIDGE_INTERFACE_NAME);
    networkStatus->isWirelessUp = is_network_interface_up(WIFI_INTERFACE_NAME);

    struct NET_CONFIG netcfg;
    memset(&netcfg, 0, sizeof(struct NET_CONFIG));
    const char *ifname = anj_net_wan_ifname();

    __INFO("IFNAME = %s\n", ifname);
    iRet = net_get_info(ifname, &netcfg);
    if (iRet == 0)
    {
        _inet_ntoa_r(netcfg.ifaddr, networkStatus->ip, 256);
        _inet_ntoa_r(netcfg.netmask, networkStatus->netmask, 256);
        _inet_ntoa_r(netcfg.gateway, networkStatus->gateway, 256);
    }

    char ipType[32] = "STATIC";
    if (anj_net_pppoe_enable())
    {
        strncpy(ipType, "PPPOE", sizeof(ipType));
    }
    else if (is_dhcp_running("eth0") || is_dhcp_running("wlan0"))
    {
        strncpy(ipType, "DHCP", sizeof(ipType));
    }

    strncpy(networkStatus->ipType, ipType, sizeof(networkStatus->ipType));

    char macBuf[6] = {0};
    net_get_hwaddr(ifname, (unsigned char *)macBuf);

    format_mac_addr_from_digit_to_string(macBuf, sizeof(macBuf), networkStatus->wireMac, sizeof(networkStatus->wireMac));

    if (networkStatus->isWirelessUp)
    {
        memset(macBuf, 0, 6);
        net_get_hwaddr(WIFI_INTERFACE_NAME, (unsigned char *)macBuf);
        format_mac_addr_from_digit_to_string(macBuf, 6, networkStatus->wirelessMac, 256);
        anj_net_provider_wifi_info_get(WIFI_INTERFACE_NAME, (void *)networkStatus);
        iRet = net_get_info(WIFI_INTERFACE_NAME, &netcfg);
        if (iRet == 0)
        {
            get_ip_str(netcfg.ifaddr, networkStatus->wirelessIp, 256);
            get_ip_str(netcfg.netmask, networkStatus->wirelessNetmask, 256);
            get_ip_str(netcfg.gateway, networkStatus->wirelessGateway, 256);
        }
    }

    networkStatus->cloudLogined = 0;
    strcpy(networkStatus->cloudId, "");
    networkStatus->cloudEnable = 0;
    networkStatus->cloudType = P2P_TYPE_NOTDEFINED;
    if (pstNetworkConfig->p2pCfg.enable > 0 && iRet == 0)
    {
        anj_ser_info *pstSerInfo = getSerInfo();
        networkStatus->cloudLogined = pstSerInfo->stP2pLoginState.logined;
        strcpy(networkStatus->cloudId, pstSerInfo->stP2pLoginState.devid);
        networkStatus->cloudEnable = pstSerInfo->stP2pLoginState.enable;
        networkStatus->cloudType = pstSerInfo->stP2pLoginState.p2ptype;
    }

    if (strcmp(pstNetworkConfig->wifiCfg.operationMode, WIRELESS_OPERATIONMODE_MASTER_NAME) == 0)
    {
        networkStatus->linkquality = 0;
        networkStatus->signallevel = 0;
        networkStatus->noise = 0;

        strcpy(networkStatus->operationMode, "Access Point");
        strcpy(networkStatus->wirelessGateway, "--");
        strcpy(networkStatus->bitRate, "--");
        strcpy(networkStatus->freq, "--");
        strcpy(networkStatus->encryptType, "wpa");
    }
    return 0;
}

int anj_net_wifi_ap_info_get(void *wifiApScan)
{
    if (wifiApScan == NULL)
    {
        return -1;
    }
    return anj_net_provider_wifi_ap_info_get(wifiApScan);
}

char *anj_net_wifi_auth_str(int authType)
{
    switch (authType)
    {
    case WIFI_AUTH_OPEN:
        return (char *)"OPEN";
    case WIFI_AUTH_SHARED:
        return (char *)"SHARED";
    case WIFI_AUTH_WPAPSK:
        return (char *)"WPAPSK";
    case WIFI_AUTH_WPA2PSK:
        return (char *)"WPA2PSK";
    case WIFI_AUTH_UNSPPORT:
        return (char *)"UNSPPORT";
    default:
        return (char *)"UNSPPORT";
    }
}

char *anj_net_wifi_encrypt_str(int encryptType)
{
    switch (encryptType)
    {
    case WIFI_ENCRYP_NONE:
        return (char *)"NONE";
    case WIFI_ENCRYP_WEP:
        return (char *)"WEP";
    case WIFI_ENCRYP_TKIP:
        return (char *)"TKIP";
    case WIFI_ENCRYP_AES:
        return (char *)"AES";
    case WIFI_ENCRYP_UNSPPORT:
        return (char *)"UNSPPORT";
    default:
        return (char *)"UNSPPORT";
    }
}

int anj_net_hotspot_enable(void)
{
    if (!IPC_WIFI_SUPPORT_AP)
    {
        __INFO("wifi hotspot: AP not supported on this product\n");
        return -1;
    }
    if (IPC_NETWORK_TYPE != NET_DEV_TYPE_WIFI && IPC_NETWORK_TYPE != NET_DEV_TYPE_WIRE_WIFI)
    {
        __INFO("wifi hotspot: network type is not wifi\n");
        return -1;
    }
    if (if_nametoindex(WIFI_INTERFACE_NAME) == 0)
    {
        __ERR("wifi hotspot: interface %s not present\n", WIFI_INTERFACE_NAME);
        return -1;
    }

    DevInfo *pDevInfo = getDevInfo();
    NetworkConfigNew *pNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
    WIFIConfig wifiCfg = pNetworkCfg->wifiCfg;
    WIFIApConfig wifiApCfg = pNetworkCfg->wifiApCfg;

    // set operation mode to master
    snprintf(wifiCfg.operationMode, sizeof(wifiCfg.operationMode), "%s", WIRELESS_OPERATIONMODE_MASTER_NAME);
    anj_config_network_wifi_set(&wifiCfg);

    // set essid to AP SSID
    snprintf(wifiApCfg.essid, sizeof(wifiApCfg.essid), "%s%.*s",
             ANJ_AP_SSID_PREFIX, (int)ANJ_AP_SN_SSID_HEX_LEN, pDevInfo->sn + ANJ_AP_SN_SSID_HEX_OFF);
    wifiApCfg.enable = 1;
    anj_config_network_wifiap_set(&wifiApCfg);

    anj_net_provider_wifi_connect_mode_set(WIFI_MODE_AP);
    anj_net_provider_wifi_thread_set(THREAD_STATUS_RUNNING);
    anj_net_reset();
    return 0;
}

int anj_net_reset()
{
    __INFO("Reset network...\n");
    char cmd[256] = {0};
    s_stNetInfo.reset = 1;

    if (s_stNetInfo.eStatus == ANJ_NET_STATUS_WIRE)
    {
        if (is_network_device_exist(WIFI_INTERFACE_NAME))
        {
            net_set_down(WIFI_INTERFACE_NAME);
            anj_mw_system("killall wpa_supplicant");
        }

        if (is_network_device_exist(WIRE_INTERFACE_NAME1))
        {
            net_set_down(WIRE_INTERFACE_NAME1);
        }
    }

    snprintf(cmd, sizeof(cmd), "killall %s", "udhcpc");
    anj_mw_system(cmd);
    return 0;
}

int anj_net_wire_and_wireless_ip_ready_check()
{
    in_addr_t gateway_addr;
    in_addr_t ip_addr;

    char ifname[32] = {0};
    char gateway_addr_str[MAX_IP_NAME_LEN] = {0};
    char ip_addr_str[MAX_IP_NAME_LEN] = {0};

    snprintf(ifname, sizeof(ifname), "%s", anj_net_wan_ifname());

    ip_addr = net_get_ifaddr(ifname);
    gateway_addr = net_get_gateway();

    if (ip_addr != INADDR_ANY && ip_addr != 0xFFFFFFFF &&
        gateway_addr != INADDR_ANY && gateway_addr != 0xFFFFFFFF)
    {
        get_ip_str(ip_addr, ip_addr_str, sizeof(ip_addr_str));
        get_ip_str(gateway_addr, gateway_addr_str, sizeof(gateway_addr_str));

        __INFO("Net status:%d ifname:%s ip addr:%s gateway:%s ready!\n", s_stNetInfo.eStatus, ifname, ip_addr_str, gateway_addr_str);
        return 1;
    }

    return 0;
}

void anj_net_ip_get(char *localip, int ipLen)
{
    if (localip == NULL || ipLen <= 0)
    {
        return;
    }

    net_local_ip(anj_net_wan_ifname(), localip, ipLen);
}

REGISTER_MODULE(anj_net, MODULE_PRIORITY_NET);
