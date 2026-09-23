#ifndef __ANJ_MW_NET_H__
#define __ANJ_MW_NET_H__

#include <netinet/in.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_IP_NAME_LEN 64

#define PING_BUF_SIZE 1500 /*发送缓存最大值*/
#define PING_DATA_LEN 56   /*ping消息数据默认大小*/
#define ICMP_HEADSIZE 8

#define WIFI_USE_80211N

#define DHCPC_EXEC "dhcpcd"

#define DHCPC_EXEC_PATH "/mnt/nfs/dhcpcd"
#define RESOLV_CONF "/etc/resolv.conf"
#define PROCNET_ROUTE_PATH "/proc/net/route"

#define WIRE_INTERFACE_NAME "eth0"
#define WIRE_INTERFACE_NAME1 "eth1"
#define WIFI_INTERFACE_NAME "wlan0"
#define BRIDGE_INTERFACE_NAME "br0"
#define PPPD_INTERFACE_NAME "ppp0"

#define DEFAULT_WIRE_MAC_ADDR "00:11:11:11:11:11"
#define DEFAULT_WIRELESS_MAC_ADDR "00:11:11:11:11:13"

struct NET_CONFIG
{
    unsigned char mac[6];
    in_addr_t ifaddr;
    in_addr_t netmask;
    in_addr_t gateway;
    in_addr_t dns;
};

typedef union __NET_IPV4
{
    unsigned long int32;
    char str[4];
} NET_IPV4;


int check_is_ipv4(const char *domain);

int ValidIpv4(const char *ip);

int is_network_interface_up(const char *ifname);

int is_network_connect(const char *ifname);

int is_network_connect(const char *ifname);

int is_network_device_exist(const char *ifname);

/**
 * @brief    Set a certain interface flag.
 * @param    "char *ifname" : interface name
 * @param    "short flag" : flag
 * @retval   0 : success ; -1 : fail
 */
int net_set_flag(const char *ifname, short flag);

/**
 * @brief    Clear a certain interface flag.
 * @param    "char *ifname" : interface name
 * @param    "short flag" : flag
 * @retval   0 : success ; -1 : fail
 */
int net_clr_flag(const char *ifname, short flag);

void net_set_up(const char *ifname);
void net_set_down(const char *ifname);

/**
 * @brief    Get an interface flag.
 * @param    "char *ifname" : interface name
 * @retval   ifr.ifr_flags
 * @retval   -1 : fail
 */
int net_get_flag(const char *ifname);

/**
 * @brief   get ip of an interface
 * @param   "int skfd" :
 * @param   "char *ifname" : interface name
 * @retval  ip
 */
struct in_addr net_get_ip(int skfd, const char *ifname);

void net_local_ip(const char *ifname, char *local_ip, int iplen);
void net_local_gw(const char *ifname, char *local_gw, int gwlen);
int net_del_ip(const char *ifname);

/**
 * @brief   set ip of an interface
 * @param   "char *ifname" : interface name
 * @param   "in_addr_t addr" : ip address
 * @retval  0 : success ; -1 : fail
 */
int net_set_ifaddr(const char *ifname, in_addr_t addr);

/**
 * @brief   get address of an interface
 * @param   "char *ifname" : interface name
 * @retval  net address
 */
in_addr_t net_get_ifaddr(const char *ifname);

/**
 * @brief   add a gateway
 * @param   "in_addr_t addr" : [IN]address of gateway
 * @retval  0 : success ; -1 : fail
 */
int net_add_gateway(in_addr_t addr);

/**
 * @brief   delete a gateway
 * @param   "in_addr_t addr" : [IN]address of gateway
 * @retval  0 : success ; -1 : fail
 */
int net_del_gateway(in_addr_t addr);

/**
 * @brief   get address of an interface
 * @param   "char *ifname" : interface name
 * @retval  address
 */
in_addr_t net_get_netmask(const char *ifname);

/**
 * @brief   get netmask of an interface
 * @param   "char *ifname" : [IN]interface name
 * @param   "in_addr_t addr" : [OUT]netmask
 * @retval  0 : success ; -1 : fail
 */
int net_set_netmask(const char *ifname, in_addr_t addr);

int get_netmask_prefix_len(in_addr_t netmask);

/**
 * @brief   get mac address of an interface
 * @param   "char *ifname" : [IN]interface name
 * @param   "unsigned char *mac" : [OUT]mac address
 * @retval  0 : success ; -1 : fail
 */
int net_get_hwaddr(const char *ifname, unsigned char *mac);

int format_mac_addr_from_digit_to_string(char *macaddr, int macLen, char *buf, int bufLen);

char *get_ip_str(in_addr_t addr, char *buf, int len);

/**
 * @brief   get net info
 * @param   "char *ifname" : [IN]interface name
 * @param   "struct NET_CONFIG *netcfg" : [OUT]net config
 * @return  0 : success ; -1 : fail
 */
int net_get_info(const char *ifname, struct NET_CONFIG *netcfg);

int set_mac_addr(const char *ifname, const char *macaddr, const char *defaultaddr);

int net_get_mtu(const char *ifname);
int net_set_mtu(const char *ifname, int mtu);
int net_set_all_mtu(int mtu);

/**
 * @brief   set domain name server.
 * @param   "char *dnsname" : [IN]dns name
 * @retval  0 : success ; -1 : fail
 */
int net_set_dns(const char *dnsname);

/**
 * @brief   get domain name server.
 * @param   none
 * @retval  dns address
 */
in_addr_t net_get_dns(void);

int net_set_two_dns(char *primaryDns, char *secondaryDns, const char *gwIp);
int net_get_two_dns(char *primaryBuf, int primaryBufLen, char *secondaryBuf, int secondaryBufLen);

void net_enable_dhcpcd(void);
void net_disable_dhcpcd(void);
int is_dhcp_running(char *ifname);

int net_search_gateway(char *buf, in_addr_t *gate_addr);
int net_search_gateway_by_ifname(const char *ifname, char *buf, in_addr_t *gate_addr);

int net_set_gateway(in_addr_t addr);
int net_clean_gateway(void);
in_addr_t net_get_gateway(void);
in_addr_t net_get_gateway_by_ifname(const char *ifname);

int net_makeSocketNonBlocking(int sock);
int net_makeSocketBlockingWithTimeout(int sock, unsigned int writeTimeoutInMilliseconds);

int arpping(unsigned int destIp, unsigned int sourceIp, const char *mac, int timeOutMs, char *ifname);
int try_ping(char *ips, int timeout, int cnt, const char *net_dev, const int *run_flag);

int net_add_broardcast_route(const char *pDev);
int net_del_broardcast_route(const char *pDev);

// 创建broardcast socket fd
int broadcastserver_ex(unsigned short localport, int nReuseAddress);
int broadcastserver(unsigned short localport);

int Check_Link_Status(const char *ifname);

int is_mac_addr_valid(char *macaddr);

int makeSocketBlockingWithTimeout(int sock, unsigned writeTimeoutInMilliseconds);

char *_inet_ntoa_r(in_addr_t addr, char *buf, int len);

const char *net_get_wireless_name();

unsigned int WS_getIpFromName(const char* szName);

char* get_ip_addr_str_from_int(int nIp);

void get_my_ifname(char *ifname);

in_addr_t get_my_ipaddr(void);
in_addr_t get_my_netmask(void);

int isValidIp4_on(char *str);
int decodeDNS(char *host, char *str, int len);
int getrel_hostname(char *hostname, char *rel_hostname, int hostname_len);

#ifdef __cplusplus
}
#endif

#endif
