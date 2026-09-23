#ifndef __ANJ_NET_H__
#define __ANJ_NET_H__

#include "anj_mw_comm.h"
#include "anj_mw_net.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define HOSTAPD_PATH    "/mnt/nand/hostapd.conf"
#define WPA_SUPPLICANT_CONF_PATH    "/mnt/nand/wpa_supplicant.conf"
#define WPA_LOG_PATH    "/tmp/wpa_log"
#define WPA_SUPPLICANT_CONNECT_SUCCESSFUL "CTRL-EVENT-CONNECTED"
#define WPA_SUPPLICANT_WRONG_PASSWD "pre-shared key may be incorrect"

#define WIFI_AUTH_OPEN      (0)
#define WIFI_AUTH_SHARED    (1)
#define WIFI_AUTH_WPAPSK    (2)
#define WIFI_AUTH_WPA2PSK   (3)
#define WIFI_AUTH_UNSPPORT  (4)

#define WIFI_ENCRYP_NONE    (0)
#define WIFI_ENCRYP_WEP     (1)
#define WIFI_ENCRYP_TKIP    (2)
#define WIFI_ENCRYP_AES     (3)
#define WIFI_ENCRYP_UNSPPORT (4)

enum
{
    WIFI_STATUS_NONE = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_CONNECT_FAILED,
    WIFI_STATUS_PASSWORD_ERROR,
};

enum
{
    WPA_CONFIG_NORMAL = 0,
    WPA_CONFIG_NO_SCAN,
    WPA_CONFIG_GBK,
    WPA_CONFIG_NUM,
};

enum
{
    WIFI_MODE_WPA = 0,
    WIFI_MODE_AP,
    WIFI_MODE_MAX,
};

enum
{
    NET_DEV_TYPE_WIRE_WIFI = 0, /* 有线+ wifi 设备 */
    NET_DEV_TYPE_WIRE_4G = 1,   /* 有线+ 4G 设备 */
    NET_DEV_TYPE_WIFI = 2,      /* wifi 设备 */
    NET_DEV_TYPE_4G = 3,        /* 4g 设备 */
    NET_DEV_TYPE_WIRE = 4,      /* 纯有线 设备 */
    NET_DEV_TYPE_MAX            /* MAX NUM */
};

typedef enum
{
    ANJ_NET_STATUS_NONE,
    ANJ_NET_STATUS_WIRE,
    ANJ_NET_STATUS_WIFI,
    ANJ_NET_STATUS_4G,
} anj_net_status_e;

typedef struct
{
    int iLoginFailTimes;
    int iPingFailTimes;
    int iCheckSameIpCnt;
    int iCheckPingIpCnt;
    int iCheckRouteCnt;
    int iWireFailAudioPlayed;
} anj_check_info;

typedef struct
{
    anj_net_status_e eStatus;
    int session;
    int reset;
} anj_net_info;

typedef struct
{
    int doBridge;

    char wireMac[256];

    char ipType[256];
    char ip[256];
    char gateway[256];
    char netmask[256];
    char dns1[256];
    int dns1Exsit;
    char dns2[256];
    int dns2Exsit;

    int isWirelessUp;
    char wirelessIp[256];
    char wirelessGateway[256];
    char wirelessNetmask[256];
    char wirelessMac[256];
    char essid[256];
    char operationMode[256];
    char bitRate[256];
    char freq[256];
    char accessPoint[256];
    char encryptType[256];

    unsigned char linkquality;
    // unsigned char signallevel;
    // unsigned char noise;
    int signallevel;
    int noise;

    int cloudLogined;
    char cloudId[128];
    int cloudEnable;
    int cloudType;
} NETWORK_STATUS_DATA;

anj_net_status_e anj_net_status_check();

int anj_net_set();

int anj_net_gw_ping(const char *ifname);

int anj_net_mac_create_by_sn(unsigned char *mac_addr);

void anj_net_hostname_set(const char *hostname);

int anj_net_gateway_load(char *gwip);

int anj_net_check_gateway_exist(const char *ifrname, char *gateway);

int anj_net_add_route(const char *ifname);

int anj_net_dhcp_up(char *ifname);

int anj_net_check_route(int netWorkStatus);

int anj_net_mac_set_by_sn();

int anj_net_info_get(NETWORK_STATUS_DATA *networkStatus);

int anj_net_wifi_ap_info_get(void *wifiApScan);

char *anj_net_wifi_auth_str(int authType);
char *anj_net_wifi_encrypt_str(int encryptType);

int anj_net_hotspot_enable(void);

int anj_net_reset();

int anj_net_wire_and_wireless_ip_ready_check();

void anj_net_ip_get(char *localip, int ipLen);

/* 当前上网口：有线且开 PPPoE 则 ppp0，否则 wifi/4g/br0/eth0 */
const char *anj_net_wan_ifname(void);

#ifdef __cplusplus
}
#endif

#endif
