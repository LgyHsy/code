#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <errno.h>
#include <pthread.h>
#include <fcntl.h>

#include "anj_mw_log.h"
#include "anj_mw_str.h"
#include "anj_mw_comm.h"
#include "anj_mw_net.h"

#if 0
static struct sockaddr_in sa = {
    sin_family: PF_INET,
    sin_port:   0
};
#endif

//// ----------------net_config start-----------

void init_sockaddrin(struct sockaddr_in *sa, in_addr_t addr)
{
    sa->sin_family = PF_INET;
    sa->sin_port = 0;
    sa->sin_addr.s_addr = addr;
}

int ValidIpv4(const char *ip)
{
    if (NULL == ip)
    {
        __ERR("ipaddr is NULL\n");
        return 0;
    }

    int len = strlen(ip);
    if (len == 0 || len > 16)
    {
        __ERR("ipaddr length %d error\n", len);
        return 0;
    }

    int validSegSize = 0; // 计算分了多少段
    int oneSeg = 0;       // 记录分段上的数值
    int i;
    for (i = 0; i < len; i++)
    {
        if (ip[i] >= '0' && ip[i] <= '9')
        {
            oneSeg = oneSeg * 10 + (ip[i] - '0'); // 分段上的数值
        }
        else if (ip[i] == '.') // 分段之间以点隔开
        {
            if (oneSeg <= 255 && oneSeg >= 0)
                validSegSize++;
            else
            {
                __ERR("ipaddr %d seg %d error\n", validSegSize, oneSeg);
                return 0;
            }

            oneSeg = 0; // 重置分段值
        }
        else
        {
            __ERR("ipaddr %s digit error\n", ip);
            return 0;
        }
    }
    if (oneSeg <= 255 && oneSeg >= 0) // 判断最后一个分段的合法性
        validSegSize++;
    else
    {
        __ERR("ipaddr latest seg %d error\n", oneSeg);
        return 0;
    }

    if (validSegSize == 4) // 判断是否一共有4个分段
        return 1;
    else
    {
        __ERR("ipaddr valid segsize %d error\n", validSegSize);
        return 0;
    }
}

int is_network_connect(const char *ifname)
{
    int flag;

    flag = net_get_flag(ifname);

    //	__ERR("flag=0x%x  IFF_UP=0x%x, IFF_RUNNING=0x%x\n", flag, IFF_UP, IFF_RUNNING);
    if (flag == -1)
    {
        return 0;
    }

    if ((flag & IFF_RUNNING) == 0)
    {
        return 0;
    }

    return 1;
}

int is_network_interface_up(const char *ifname)
{
    int flag;

    flag = net_get_flag(ifname);

    //	__INFO("%s: flag = %d  IFF_UP = %d\n", ifname, flag,IFF_UP);
    if (flag == -1)
    {
        return 0;
    }

    if ((flag & IFF_UP) == 0)
    {
        return 0;
    }

    return 1;
}

int is_network_device_exist(const char *ifname)
{
    struct ifreq ifr;
    int skfd;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("socket error\n");
        return 0;
    }

    StrCpy(ifr.ifr_name, IFNAMSIZ, ifname);
    if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0)
    {
        int errno_ret = errno;
        if (errno_ret == 19) // no such device
        {
            close(skfd);
            return 0;
        }
    }

    close(skfd);

    return 1;
}

int net_set_flag(const char *ifname, short flag)
{
    struct ifreq ifr;
    int skfd;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return -1;
    }

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFFLAGS\n", ifname);
        close(skfd);
        return (-1);
    }

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    ifr.ifr_flags |= flag;
    if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCSIFFLAGS\n", ifname);
        close(skfd);
        return -1;
    }

    close(skfd);
    return (0);
}

int net_clr_flag(const char *ifname, short flag)
{
    struct ifreq ifr;
    int skfd;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error", ifname);
        return -1;
    }

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFFLAGS", ifname);
        close(skfd);
        return -1;
    }

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    ifr.ifr_flags &= ~flag;
    if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCSIFFLAGS", ifname);
        close(skfd);
        return -1;
    }

    close(skfd);
    return (0);
}

void net_set_up(const char *ifname)
{
    short flag = IFF_UP | IFF_RUNNING;
    net_set_flag(ifname, flag);
}

void net_set_down(const char *ifname)
{
    short flag = IFF_UP;
    net_clr_flag(ifname, flag);
}

int net_get_flag(const char *ifname)
{
    struct ifreq ifr;
    int skfd;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s socket error\n", ifname);
        return -1;
    }

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        __ERR("%s ioctl SIOCGIFFLAGS, skfd %d\n", ifname, skfd);
        close(skfd);
        return -1;
    }

    close(skfd);
    return ifr.ifr_flags;
}

in_addr_t net_get_ifaddr(const char *ifname)
{
    struct ifreq ifr;
    int skfd;
    struct sockaddr_in *saddr;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return INADDR_NONE;
    }

    StrCpy(ifr.ifr_name, IFNAMSIZ, ifname);
    if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0)
    {
        //      __ERR("%s: ioctl SIOCGIFADDR\n", ifname);
        close(skfd);
        return INADDR_NONE;
    }
    close(skfd);

    saddr = (struct sockaddr_in *)&ifr.ifr_addr;
    return saddr->sin_addr.s_addr;
}

struct in_addr net_get_ip(int skfd, const char *ifname)
{
    struct ifreq ifr;
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);

    if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFADDR\n", ifname);
        return (struct in_addr){-1};
    }

    return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
}

void net_local_ip(const char *ifname, char *local_ip, int iplen)
{
    if (local_ip == NULL || iplen <= 0)
    {
        return;
    }

    in_addr_t ip = net_get_ifaddr(ifname);
    get_ip_str(ip, local_ip, iplen);
}

void net_local_gw(const char *ifname, char *local_gw, int gwlen)
{
    if (local_gw == NULL || gwlen <= 0)
    {
        return;
    }

    in_addr_t gateway = net_get_gateway_by_ifname(ifname);
    get_ip_str(gateway, local_gw, gwlen);
}

int net_del_ip(const char *ifname)
{
    if (NULL != ifname)
    {
        char local_ip[MAX_IP_NAME_LEN] = "";
        net_local_ip(ifname, local_ip, MAX_IP_NAME_LEN);
        if (0 != strcmp(local_ip, "0.0.0.0") &&
            0 != strcmp(local_ip, "255.255.255.255"))
        {
            char cmd_str[128];
            snprintf(cmd_str, sizeof(cmd_str), "ip addr del %s dev %s", local_ip, ifname);
            anj_mw_system(cmd_str);
            __INFO("del_net_ip:%s\n", cmd_str);
            return 0;
        }
    }
    else
    {
        __ERR("Invalid Input %p\n", ifname);
    }
    return -1;
}

int net_set_ifaddr(const char *ifname, in_addr_t addr)
{
    struct ifreq ifr;
    int skfd = -1;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return -1;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    init_sockaddrin(&sa, addr);

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);

    memcpy((char *)&ifr.ifr_addr, (char *)&sa, sizeof(struct sockaddr));

    if (ioctl(skfd, SIOCSIFADDR, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCSIFADDR\n", ifname);
        close(skfd);
        return -1;
    }

    close(skfd);
    return 0;
}

in_addr_t net_get_netmask(const char *ifname)
{
    struct ifreq ifr;
    int skfd;
    struct sockaddr_in *saddr;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return -1;
    }

    StrCpy(ifr.ifr_name, IFNAMSIZ, ifname);
    if (ioctl(skfd, SIOCGIFNETMASK, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFNETMASK\n", ifname);
        close(skfd);
        return -1;
    }
    close(skfd);

    saddr = (struct sockaddr_in *)&ifr.ifr_addr;
    return saddr->sin_addr.s_addr;
}

int net_set_netmask(const char *ifname, in_addr_t addr)
{
    struct ifreq ifr;
    int skfd;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return -1;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    init_sockaddrin(&sa, addr);

    StrCpy(ifr.ifr_name, IFNAMSIZ, ifname);
    memcpy((char *)&ifr.ifr_addr, (char *)&sa, sizeof(struct sockaddr));
    if (ioctl(skfd, SIOCSIFNETMASK, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCSIFNETMASK\n", ifname);
        close(skfd);
        return -1;
    }
    close(skfd);
    return 0;
}

int get_netmask_prefix_len(in_addr_t netmask)
{
    int mask_host = ntohl(netmask);

    // __INFO("mask_host value:0x:%x\n", mask_host);

    unsigned int i;
    for (i = 0; i < 32; i++)
    {
        if (((mask_host >> i) & 0x00000001) != 0)
        {
            // __INFO("get netmask prefix len :%d\n", 32 - i);
            return (32 - i);
        }
    }

    return 0;
}

int net_set_hwaddr(const char *ifname, unsigned char *mac)
{
    int ret = 0;
    char cmd[256];
    net_set_down(ifname);
    usleep(1000);

    sprintf(cmd, "ifconfig %s hw ether %s", ifname, mac);

    __INFO("cmd: %s\n", cmd);

    if (anj_mw_system(cmd))
    {
        __ERR("call cmd (%s) fail\n", cmd);
        ret = -1;
    }

    usleep(1000);

    net_set_up(ifname);

    return ret;
}

/**
 * @brief   get mac address of an interface
 * @param   "char *ifname" : [IN]interface name
 * @param   "unsigned char *mac" : [OUT]mac address
 * @retval  0 : success ; -1 : fail
 */
int net_get_hwaddr(const char *ifname, unsigned char *mac)
{
    struct ifreq ifr;
    int skfd = -1;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return -1;
    }

    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFHWADDR. fd %d\n", ifname, skfd);
        close(skfd);
        return -1;
    }
    close(skfd);

    memcpy(mac, ifr.ifr_ifru.ifru_hwaddr.sa_data, IFHWADDRLEN);
    return 0;
}

int format_mac_addr_from_digit_to_string(char *macaddr, int macLen, char *buf, int bufLen)
{
    if (NULL == macaddr || NULL == buf)
    {
        return -1;
    }

    memset(buf, '\0', bufLen);
    if (macLen != 6 || bufLen < 17)
    {
        __ERR("param error\n");
        return -1;
    }

    // sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", macaddr[0],macaddr[1],macaddr[2],macaddr[3],macaddr[4],macaddr[5]);
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

    return 0;
}

char *get_ip_str(in_addr_t addr, char *buf, int len)
{
    if (buf == NULL || len <= 0)
    {
        return buf;
    }

    in_addr_t addr1 = addr;

    memset(buf, '\0', len);

    char *pb = buf;
    char *pe = buf + len - 1;

    unsigned char dotValue;

    int i;
    for (i = 0; i < 4; i++)
    {
        dotValue = (addr1 >> (i * 8)) & 0XFF;
        if (i == 3)
        {
            pb += snprintf(pb, pe - pb, "%d", dotValue);
        }
        else
        {
            pb += snprintf(pb, pe - pb, "%d.", dotValue);
        }
    }

    return buf;
}

int net_get_info(const char *ifname, struct NET_CONFIG *netcfg)
{
    struct ifreq ifr;
    int skfd = -1;
    struct sockaddr_in *saddr;

    char szIpAddr[MAX_IP_NAME_LEN] = {0};
    char log[256] = {0};

    sprintf(log + strlen(log), "%s ", ifname);

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return -1;
    }

    saddr = (struct sockaddr_in *)&ifr.ifr_addr;
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFADDR\n", ifname);
        close(skfd);
        return -1;
    }

    netcfg->ifaddr = saddr->sin_addr.s_addr;
    sprintf(log + strlen(log), "ifaddr=0x%x [%s] ", netcfg->ifaddr, get_ip_str(netcfg->ifaddr, szIpAddr, MAX_IP_NAME_LEN));

    if (ioctl(skfd, SIOCGIFNETMASK, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFNETMASK\n", ifname);
        close(skfd);
        return -1;
    }

    netcfg->netmask = saddr->sin_addr.s_addr;
    sprintf(log + strlen(log), "netmask=0x%x [%s] ", netcfg->netmask, get_ip_str(netcfg->netmask, szIpAddr, MAX_IP_NAME_LEN));

    if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
    {
        __ERR("%s: ioctl SIOCGIFHWADDR\n", ifname);
        close(skfd);
        return -1;
    }
    memcpy(netcfg->mac, ifr.ifr_ifru.ifru_hwaddr.sa_data, IFHWADDRLEN);
    sprintf(log + strlen(log), "hwaddr=%02x:%02x:%02x:%02x:%02x:%02x ", netcfg->mac[0], netcfg->mac[1],
            netcfg->mac[2], netcfg->mac[3], netcfg->mac[4], netcfg->mac[5]);

    close(skfd);

    netcfg->gateway = net_get_gateway_by_ifname(ifname);
    sprintf(log + strlen(log), "gateway=0x%x [%s] ", netcfg->gateway, get_ip_str(netcfg->gateway, szIpAddr, MAX_IP_NAME_LEN));
    netcfg->dns = net_get_dns();

    // __INFO("%s\n", log);

    return 0;
}
int is_mac_addr_valid(char *macaddr)
{
    if (macaddr == NULL || strlen(macaddr) != 17)
    {
        return 0;
    }

    char c_split = ':';

    if (macaddr[2] != c_split || macaddr[5] != c_split ||
        macaddr[8] != c_split || macaddr[11] != c_split ||
        macaddr[14] != c_split)
    {
        return 0;
    }

    if (macaddr[3] == '0' && macaddr[4] == '0' && macaddr[6] == '0' &&
        macaddr[7] == '0' && macaddr[9] == '0' && macaddr[10] == '0' &&
        macaddr[12] == '0' && macaddr[13] == '0' && macaddr[15] == '0' && macaddr[16] == '0')
    {
        return 0;
    }

    if (0 == strcasecmp(macaddr, "00:11:22:33:44:55") || 0 == strcasecmp(macaddr, "00:00:23:34:45:66") ||
        0 == strcasecmp(macaddr, "00:30:1b:ba:02:db") || 0 == strcasecmp(macaddr, "00:11:11:11:11:11") ||
        0 == strcasecmp(macaddr, "de:ad:be:ef:00:00") || 0 == strcasecmp(macaddr, "de:ad:be:af:00:00") ||
        0 == strcasecmp(macaddr, "00:AA:BB:CC:DD:EE") || 0 == strcasecmp(macaddr, "00:e0:0a:dd:00:00"))
    {
        return 0;
    }

    return 1;
}

int set_mac_addr(const char *ifname, const char *macaddr, const char *defaultaddr)
{
    char *mac_set = NULL;
    int set = 0;
    char macbuf[6];
    int ret;

    __INFO("ifname = %s, macaddr = %s, defaultaddr = %s\n",
          ifname, macaddr, defaultaddr);

    if (is_mac_addr_valid((char *)macaddr))
    {
        mac_set = (char *)macaddr;
        set = 1;
    }
    else
    {
        __WARN("invalid macadd = %s\n", macaddr);
        ret = net_get_hwaddr(ifname, (unsigned char *)macbuf);
        if (ret == -1)
        {
            __ERR("net_get_hwaddr error\n");
            return -1;
        }
        else
        {
            char mac_format[256];
            format_mac_addr_from_digit_to_string(macbuf, 6, mac_format, 256);

            if (is_mac_addr_valid(mac_format))
            {
                mac_set = mac_format;
                set = 1;
            }
            else
            {
                __WARN("invalid macadd = %s\n", mac_format);
                mac_set = (char *)defaultaddr;
                set = 1;
            }
        }
    }

    if (set == 1)
    {
        net_set_hwaddr(ifname, (unsigned char *)mac_set);
    }

    return 0;
}

int net_get_mtu(const char *ifname)
{
    int skfd = -1;
    struct ifreq ifr;

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("%s: socket error\n", ifname);
        return -1;
    }

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(skfd, SIOCGIFMTU, &ifr) != 0)
    {
        __ERR("ioctl fail %s\n", ifname);
        close(skfd);
        return -1;
    }

    close(skfd);
    return ifr.ifr_mtu;
}

int net_set_mtu(const char *ifname, int mtu)
{
    int skfd = -1;
    struct ifreq ifr;
    int iRet = -1;
    int status = is_network_interface_up(ifname);

    __INFO("set %s to mtu:%d\n", ifname, mtu);
    if (status)
    {
        net_set_down(ifname);
    }

    skfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (skfd < 0)
    {
        __ERR("%s: socket error\n", ifname);
    }
    else
    {
        strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
        ifr.ifr_mtu = mtu;
        if (ioctl(skfd, SIOCSIFMTU, &ifr) == 0)
        {
            iRet = 0;
        }
        else
        {
            __ERR("ioctl fail %s SIOCSIFMTU %d\n", ifname, mtu);
        }
        close(skfd);
    }

    if (status)
    {
        net_set_up(ifname);
    }
    return iRet;
}

int net_set_all_mtu(int mtu)
{
    struct ifaddrs *ifa = NULL, *ifList;

    if (getifaddrs(&ifList) < 0)
    {
        __ERR("getifaddrs failed \n");
        return -1;
    }

    for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next)
    {
        //      if(ifa->ifa_addr->sa_family == AF_INET)
        //      {

        //      }

        if (ifa->ifa_name != NULL)
        {
            if (strcmp(ifa->ifa_name, "lo") != 0)
            {
                net_set_mtu(ifa->ifa_name, mtu);
            }
        }
    }

    if (ifList != NULL)
    {
        freeifaddrs(ifList);
    }

    return 0;
}

/**
 * @brief   set domain name server.
 * @param   "char *dnsname" : [IN]dns name
 * @retval  0 : success ; -1 : fail
 */
int net_set_dns(const char *dnsname)
{
    FILE *fp = NULL;

    fp = fopen(RESOLV_CONF, "w");
    if (fp)
    {
        fprintf(fp, "nameserver %s\n", dnsname);
        fclose(fp);
        __INFO("dns=%s\n", dnsname);
        return 0;
    }

    __INFO("file \"%s\" opened for writing error!\n", RESOLV_CONF);
    return -1;
}

/**
 * @brief   get domain name server.
 * @param   none
 * @retval  dns address
 */
in_addr_t net_get_dns(void)
{
    char dnsname[80];
    char buf[128];
    memset(dnsname, 0, 80);
    memset(buf, 0, 128);

    int fd = -1;
    fd = open(RESOLV_CONF, O_RDONLY);
    if (fd == -1)
    {
        __ERR("open file(%s) error for(%s)\n", RESOLV_CONF, strerror(errno));
        return -1;
    }

    read(fd, buf, 128);

    close(fd);

    char *curPos = buf;
    char *tmp = NULL;
    int idx = 0;

    char *key = "nameserver";
    tmp = strstr(curPos, key);

    if (tmp == NULL)
    {
        return INADDR_ANY;
    }

    curPos = tmp + strlen(key);
    while ((*curPos == '\t' || *curPos == ' ') && *curPos != '\0')
    {
        curPos++;
    }

    idx = 0;
    while (*curPos != '\n' && *curPos != '\0' && idx < 80 - 1)
    {
        dnsname[idx++] = *(curPos);
        curPos++;
    }

    return inet_addr(dnsname);
}

int net_set_two_dns(char *primaryDns, char *secondaryDns, const char *gwIp)
{
    FILE *fp;

    fp = fopen(RESOLV_CONF, "w");

    char buf[526];
    memset(buf, '\0', 526);

    char *pb = buf;
    char *pe = buf + 525;

    struct in_addr in;

    if (primaryDns != NULL && inet_aton(primaryDns, &in) != 0)
    {
        if (strcmp(primaryDns, "255.255.255.255") != 0 &&
            strcmp(primaryDns, "0.0.0.0") != 0)
            pb += snprintf(pb, pe - pb, "nameserver %s\n", primaryDns);
    }

    if (secondaryDns != NULL && inet_aton(secondaryDns, &in) != 0)
    {
        if (strcmp(secondaryDns, "255.255.255.255") != 0 &&
            strcmp(secondaryDns, "0.0.0.0") != 0)
            pb += snprintf(pb, pe - pb, "nameserver %s\n", secondaryDns);
    }

    if (fp)
    {
        if (strlen(buf) != 0)
        {
            fprintf(fp, buf);
        }
        fclose(fp);
        __INFO("set dns (%s)\n", buf);
        return 0;
    }

    __INFO("file \"%s\" opened for writing error!\n", RESOLV_CONF);
    return -1;
}

int net_get_two_dns(char *primaryBuf, int primaryBufLen, char *secondaryBuf, int secondaryBufLen)
{
    memset(primaryBuf, '\0', primaryBufLen);
    memset(secondaryBuf, '\0', secondaryBufLen);

    int fileLen = 0;
    int fd = -1;
    fd = open(RESOLV_CONF, O_RDONLY);
    if (fd == -1)
    {
        __ERR("open file(%s) error for(%s)\n", RESOLV_CONF, strerror(errno));
        return -1;
    }

    fileLen = lseek(fd, 0, SEEK_END);
    if (0 == fileLen)
    {
        __DBG("lseek file:%s is 0!\n", RESOLV_CONF);
    }

    char buf[512];
    memset(buf, '\0', 512);

    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 512);

    //  __INFO("dns buf = %s\n", buf);

    close(fd);

    char *curPos = buf;
    char *tmp = NULL;
    int idx = 0;

    char dnslist[5][64];
    const char *key = "nameserver";
    memset(dnslist, 0, sizeof(dnslist));

    int iDnsIndex = 0;

    tmp = strstr(curPos, key);
    while (tmp != NULL && iDnsIndex < 5)
    {
        curPos = tmp + strlen(key);
        while ((*curPos == '\t' || *curPos == ' ') && *curPos != '\0')
        {
            curPos++;
        }

        idx = 0;
        while (*curPos != '\n' && *curPos != '\0' && idx < primaryBufLen)
        {
            dnslist[iDnsIndex][idx++] = *(curPos);
            curPos++;
        }

        tmp = strstr(curPos, key);
        if (tmp == NULL)
            break;

        iDnsIndex++;
    }

    iDnsIndex = 0;
    int dnscount = 0;
    for (iDnsIndex = 0; iDnsIndex < 5; iDnsIndex++)
    {
        if (strlen(dnslist[iDnsIndex]) > 0)
            dnscount++;
    }

    int fromindex = 0;
    if (strlen(dnslist[fromindex]) > 0)
    {
        strncpy(primaryBuf, dnslist[fromindex], primaryBufLen - 1);
    }

    if (strlen(dnslist[fromindex + 1]) > 0)
    {
        strncpy(secondaryBuf, dnslist[fromindex + 1], secondaryBufLen - 1);
    }

    return 0;
}

/**
 * @brief   enable dhcp.
 * @param   none
 * @retval  none
 */
void net_enable_dhcpcd(void)
{
    anj_mw_system("killall -9 " DHCPC_EXEC);
    anj_mw_system(DHCPC_EXEC_PATH);
}

/**
 * @brief   disable dhcp.
 * @param   none
 * @retval  none
 */
void net_disable_dhcpcd(void)
{
    anj_mw_system("killall -9 " DHCPC_EXEC);
}

int is_dhcp_running(char *ifname)
{
    char result_file[128];
    snprintf(result_file, sizeof(result_file), "/tmp/dhcp.result.%s", ifname);
    if (anj_mw_file_exists(result_file) == 0)
    {
        // 结果文件不存在，说明DHCP信息不完整
        return 0;
    }

    char local_ip[MAX_IP_NAME_LEN] = "";
    net_local_ip(ifname, local_ip, MAX_IP_NAME_LEN);

    if (strlen(local_ip) == 0 || strcmp(local_ip, "0.0.0.0") == 0)
    {
        return 0;
    }

    // 从DHCP结果文件中读取分配的IP地址
    FILE *fp = anj_mw_fopen(result_file, "r");
    if (fp == NULL)
    {
        return 0;
    }

    char dhcp_ip[256] = {0};
    char line[256] = {0};
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "IPADDR=", 7) == 0)
        {
            char *ip_start = line + 7;
            char *ip_end = strchr(ip_start, '\n');
            if (ip_end)
                *ip_end = '\0';

            snprintf(dhcp_ip, sizeof(dhcp_ip), "%s", ip_start);
            break;
        }
    }
    anj_mw_fclose(fp);
    // 比较当前IP和DHCP分配的IP
    if (strcmp(local_ip, dhcp_ip) == 0)
    {
        return 1; // 是DHCP分配的IP
    }
    else
    {
        return 0; // 不是DHCP分配的IP
    }
}

/**
 * @brief   add a gateway
 * @param   "in_addr_t addr" : [IN]address of gateway
 * @retval  0 : success ; -1 : fail
 */
int net_add_gateway(in_addr_t addr)
{
    struct rtentry rt;
    int skfd = -1;

    /* Clean out the RTREQ structure. */
    memset((char *)&rt, 0, sizeof(struct rtentry));

    /* Fill in the other fields. */
    rt.rt_flags = (RTF_UP | RTF_GATEWAY);

    rt.rt_dst.sa_family = PF_INET;
    rt.rt_genmask.sa_family = PF_INET;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    init_sockaddrin(&sa, addr);

    memcpy((char *)&rt.rt_gateway, (char *)&sa, sizeof(struct sockaddr));

    /* Create a socket to the INET kernel. */
    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("socket error\n");
        return -1;
    }
    /* Tell the kernel to accept this route. */
    if (ioctl(skfd, SIOCADDRT, &rt) < 0)
    {
        __ERR("ioctl SIOCADDRT(%s)\n", strerror(errno));
        close(skfd);
        return -1;
    }
    /* Close the socket. */
    close(skfd);
    return (0);
}

/**
 * @brief   delete a gateway
 * @param   "in_addr_t addr" : [IN]address of gateway
 * @retval  0 : success ; -1 : fail
 */
int net_del_gateway(in_addr_t addr)
{
    struct rtentry rt;
    int skfd = -1;

    /* Clean out the RTREQ structure. */
    memset((char *)&rt, 0, sizeof(struct rtentry));

    /* Fill in the other fields. */
    rt.rt_flags = (RTF_UP | RTF_GATEWAY);

    rt.rt_dst.sa_family = PF_INET;
    rt.rt_genmask.sa_family = PF_INET;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    init_sockaddrin(&sa, addr);

    memcpy((char *)&rt.rt_gateway, (char *)&sa, sizeof(struct sockaddr));

    /* Create a socket to the INET kernel. */
    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        __ERR("socket error\n");
        return -1;
    }
    /* Tell the kernel to accept this route. */
    if (ioctl(skfd, SIOCDELRT, &rt) < 0)
    {
        __ERR("ioctl SIOCDELRT\n");
        close(skfd);
        return -1;
    }
    /* Close the socket. */
    close(skfd);
    return (0);
}

/**
 * @brief   search gateway
 * @param   "char *buf" : [IN]buffer
 * @param   "in_addr_t *gate_addr" : [OUT]gateway address
 * @return  0 : success ; -1 : fail
 */
int net_search_gateway(char *buf, in_addr_t *gate_addr)
{
    char iface[16] = {0};
    unsigned long dest = 0, gate = 0;
    int iflags = 0;

    sscanf(buf, "%s\t%08lX\t%08lX\t%8X\t", iface, &dest, &gate, &iflags);
    //  __INFO("%s, %lX, %lX, %X\n", iface, dest, gate, iflags);
    if ((iflags & (RTF_UP | RTF_GATEWAY)) == (RTF_UP | RTF_GATEWAY))
    {
        *gate_addr = gate;
        return 0;
    }

    return -1;
}

/**
 * @brief   search gateway
 * @param   "char *buf" : [IN]buffer
 * @param   "in_addr_t *gate_addr" : [OUT]gateway address
 * @return  0 : success ; -1 : fail
 */
int net_search_gateway_by_ifname(const char *ifname, char *buf, in_addr_t *gate_addr)
{
    char iface[16] = {0};
    unsigned long dest = 0, gate = 0;
    int iflags = 0;

    sscanf(buf, "%s\t%08lX\t%08lX\t%8X\t", iface, &dest, &gate, &iflags);
    if (NULL != ifname && strlen(ifname) > 0)
    {
        if (strcmp(ifname, iface) != 0)
        {
            return -1;
        }
    }

    //  __INFO("%s, %lX, %lX, %X\n", iface, dest, gate, iflags);
    if ((iflags & (RTF_UP | RTF_GATEWAY)) == (RTF_UP | RTF_GATEWAY))
    {
        *gate_addr = gate;
        return 0;
    }

    return -1;
}

/**
 * @brief   set gateway
 * @param   "in_addr_t addr" : [IN]gateway address
 * @return  0 : success ; -1 : fail
 */
int net_set_gateway(in_addr_t addr)
{
    in_addr_t gate_addr;
    char buff[132] = {0};
    FILE *fp = fopen(PROCNET_ROUTE_PATH, "r");
    if (!fp)
    {
        __INFO("INET (IPv4) not configured in this anj_mw_system.\n");
        return -1;
    }

    fgets(buff, 130, fp);

    int ntimes = 0;
    while (fgets(buff, 130, fp) != NULL && ntimes++ < 100)
    {
        if (net_search_gateway(buff, &gate_addr) == 0)
        {
            net_del_gateway(gate_addr);
        }
    }
    fclose(fp);

    return net_add_gateway(addr);
}

/**
 * @brief   clean gateway
 * @param   none
 * @return  0 : success ; -1 : fail
 */
int net_clean_gateway(void)
{
    in_addr_t gate_addr;
    char buff[132] = {0};
    FILE *fp = fopen(PROCNET_ROUTE_PATH, "r");
    if (!fp)
    {
        __INFO("INET (IPv4) not configured in this anj_mw_system.\n");
        return -1;
    }

    int ntimes = 0;
    fgets(buff, 130, fp);
    while (fgets(buff, 130, fp) != NULL && ntimes++ < 100)
    {
        if (net_search_gateway(buff, &gate_addr) == 0)
        {
            net_del_gateway(gate_addr);
        }
    }
    fclose(fp);

    return 0;
}

/**
 * @brief   get gateway
 * @param   none
 * @return  gatewat address
 */
in_addr_t net_get_gateway(void)
{
    in_addr_t gate_addr;
    char buff[132] = {0};
    FILE *fp = fopen(PROCNET_ROUTE_PATH, "r");
    if (!fp)
    {
        __INFO("INET (IPv4) not configured in this anj_mw_system.\n");
        return (INADDR_ANY);
    }

    int ntimes = 0;
    fgets(buff, 130, fp);
    while (fgets(buff, 130, fp) != NULL && ntimes++ < 100)
    {
        if (net_search_gateway(buff, &gate_addr) == 0)
        {
            fclose(fp);
            return gate_addr;
        }
    }

    fclose(fp);
    return (INADDR_ANY);
}

/**
 * @brief   get gateway
 * @param   none
 * @return  gatewat address
 */
in_addr_t net_get_gateway_by_ifname(const char *ifname)
{
    in_addr_t gate_addr;
    char buff[132] = {0};
    FILE *fp = fopen(PROCNET_ROUTE_PATH, "r");
    if (!fp)
    {
        __INFO("INET (IPv4) not configured in this anj_mw_system.\n");
        return (INADDR_ANY);
    }

    int ntimes = 0;
    fgets(buff, 130, fp);
    while (fgets(buff, 130, fp) != NULL && ntimes++ < 100)
    {
        if (net_search_gateway_by_ifname(ifname, buff, &gate_addr) == 0)
        {
            fclose(fp);
            return gate_addr;
        }
    }

    fclose(fp);
    return (INADDR_ANY);
}

/*
return code
-1 : invalid param  0:domain   1 :ipv4
*/
int check_is_ipv4(const char *domain)
{
    struct in_addr s;
    char IPdotdec[20] = {0};

    if ((strlen(domain) == 0) || (strlen(domain) > MAX_IP_NAME_LEN))
    {
        printf("invalid domain length!\n");
        return -1;
    }

    if (inet_pton(AF_INET, domain, (void *)&s) == 1)
    {
        inet_ntop(AF_INET, (void *)&s, IPdotdec, 16);
        return 1;
    }
    else
    {
        return -1;
    }
}

int net_makeSocketNonBlocking(int sock)
{
    int curFlags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, curFlags | O_NONBLOCK);
    return 1;
}

int net_makeSocketBlockingWithTimeout(int sock, unsigned int writeTimeoutInMilliseconds)
{
    int curFlags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, curFlags & (~O_NONBLOCK));

    if (writeTimeoutInMilliseconds > 0)
    {
        struct timeval tv;
        tv.tv_sec = writeTimeoutInMilliseconds / 1000;
        tv.tv_usec = (writeTimeoutInMilliseconds % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof tv);
    }
    return 1;
}

int net_get_route(in_addr_t dst_addr, in_addr_t netmask, const char *pDev)
{
    int ret = 0;
    char buff[132] = {0};

    FILE *fp = fopen(PROCNET_ROUTE_PATH, "r");

    if (!fp)
    {
        __ERR("fopen %s failed\n", PROCNET_ROUTE_PATH);
        __INFO("INET (IPv4) not configured in this system.\n");
        return ret;
    }

    fgets(buff, 130, fp);
    int ntimes = 0;
    char iface[16] = {0};
    unsigned long dest, gate, mask;
    int iflags, RefCnt, Use, Metric;
    while (fgets(buff, 130, fp) != NULL && ntimes++ < 100)
    {

        sscanf(buff, "%s\t%08lX\t%08lX\t%8X\t%d\t%d\t%d\t%08lX", iface, &dest, &gate, &iflags, &RefCnt, &Use, &Metric, &mask);
        //__INFO("%s: dst %lX, gate %lX, iflags %X, mask %lX\n", iface, dest, gate, iflags, mask);

        if (strcmp(pDev, iface) == 0 &&
            dst_addr == dest &&
            netmask == mask)
        {
            ret = 1;
            break;
        }
    }
    fclose(fp);

    return ret;
}

int net_add_broardcast_route(const char *pDev)
{
    struct in_addr addr_net, addr_mask;
    addr_net.s_addr = 0xffffffff;
    addr_mask.s_addr = 0;

    char cmd[64] = {0};

    if (net_get_route(addr_net.s_addr, addr_mask.s_addr, pDev) == 0)
    {
        snprintf(cmd, sizeof(cmd), "route add -host 255.255.255.255 dev %s", pDev);
        anj_mw_system(cmd);
    }

    return 0;
}

int net_del_broardcast_route(const char *pDev)
{
    struct in_addr addr_net, addr_mask;
    addr_net.s_addr = 0xffffffff;
    addr_mask.s_addr = 0;

    char cmd[64] = {0};

    while (net_get_route(addr_net.s_addr, addr_mask.s_addr, pDev) != 0)
    {
        snprintf(cmd, sizeof(cmd), "route del -host 255.255.255.255 dev %s", pDev);
        anj_mw_system(cmd);
    }

    return 0;
}

// 创建broardcast socket fd
int broadcastserver_ex(unsigned short localport, int nReuseAddress)
{
    int sockfd = -1;
    struct sockaddr_in local;
    int nRet = 0;
    int so_broadcast = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd <= 0)
    {
        __ERR("socket() failed, errno=%d\n", errno);
        return -1;
        ;
    }
    else
    {
        __INFO("socket() ok, sockfd=%d\n", sockfd);
    }

    nRet = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char *)&so_broadcast, sizeof(so_broadcast));
    if (nRet != 0)
    {
        __ERR("setsockopt() failed, errno=%d\n", errno);

        close(sockfd);
        return -1;
    }
    else
    {
        __INFO("setsockopt() ok!\n");
    }

    nReuseAddress = nReuseAddress > 0 ? 1 : 0;
    nRet = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&nReuseAddress, sizeof(nReuseAddress));
    if (nRet != 0)
    {
        __ERR("set socket reuse address option failed, errno=%d\n", errno);
        close(sockfd);
        return -1;
    }
    else
    {
        __INFO("set socket reuse address option ok!\n");
    }

    memset((void *)&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(localport);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    nRet = bind(sockfd, (struct sockaddr *)&local, sizeof(local));
    if (nRet != 0)
    {
        __ERR("Can't bind socket to local port!, errno=%d\n", errno);
        close(sockfd);
        return -1;
    }
    else
    {
        __INFO("bind() ok!\n");
    }

    return sockfd;
}

// 创建broardcast socket fd
int broadcastserver(unsigned short localport)
{
    return broadcastserver_ex(localport, 1);
}

// ibsoftsn.a需要
int Check_Link_Status(const char *ifname)
{
    return is_network_connect(ifname);
}

int makeSocketBlockingWithTimeout(int sock, unsigned writeTimeoutInMilliseconds)
{
    int curFlags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, curFlags & (~O_NONBLOCK));

    if (writeTimeoutInMilliseconds > 0)
    {
        struct timeval tv;
        tv.tv_sec = writeTimeoutInMilliseconds / 1000;
        tv.tv_usec = (writeTimeoutInMilliseconds % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof tv);
    }
    return 1;
}

char *_inet_ntoa_r(in_addr_t addr, char *buf, int len)
{
    in_addr_t addr1 = addr;

    memset(buf, 0x0, len);

    int clen = 0;
    int i;
    for (i = 0; i < 4; i++)
    {
        unsigned char dotValue = (addr1 >> (i * 8)) & 0XFF;
        if (i == 3)
        {
            clen += snprintf(buf + clen, len - clen - 1, "%d", dotValue);
        }
        else
        {
            clen += snprintf(buf + clen, len - clen - 1, "%d.", dotValue);
        }
    }

    return buf;
}

const char *net_get_wireless_name()
{
    return WIFI_INTERFACE_NAME;
}

unsigned int WS_getIpFromName(const char *szName)
{
    unsigned int nSvrIp = 0;
    if (szName == NULL || szName[0] == 0)
        return nSvrIp;

    struct addrinfo *res = NULL, *pt = NULL;
    struct sockaddr_in *sinp;
    char abuf[INET_ADDRSTRLEN];
    int succ = 0, i = 0;

    succ = getaddrinfo(szName, NULL, NULL, &res);
    if (succ == 0)
    {
        for (pt = res, i = 0; pt != NULL; pt = pt->ai_next, i++)
        {
            sinp = (struct sockaddr_in *)pt->ai_addr;
            const char *addr = (const char *)inet_ntop(AF_INET, &sinp->sin_addr, abuf, INET_ADDRSTRLEN);
            if (nSvrIp == 0 && addr != NULL)
            {
                nSvrIp = sinp->sin_addr.s_addr;
            }
            __ERR("%2d. ai_protocol=%u, IP=%u(%s)\n", i, pt->ai_protocol, sinp->sin_addr.s_addr, addr ? addr : "NULL");
        }

        freeaddrinfo(res);
    }
    else
    {
        __ERR("getaddrinfo failed with %d. errorno: %d (%s)\n", succ, errno, strerror(errno));
    }

    return nSvrIp;
}

char *get_ip_addr_str_from_int(int nIp)
{
    struct in_addr addr;
    memcpy(&addr.s_addr, &nIp, sizeof(int));
    return inet_ntoa(addr);
}

void get_my_ifname(char *ifname)
{
    if (ifname == NULL)
    {
        return;
    }
    if ((is_network_interface_up(WIRE_INTERFACE_NAME) == 1) && net_get_ifaddr(WIRE_INTERFACE_NAME) != INADDR_NONE)
    {
        strcpy(ifname, WIRE_INTERFACE_NAME);
    }
    else if (is_network_interface_up(WIFI_INTERFACE_NAME) > 0 && net_get_ifaddr(WIFI_INTERFACE_NAME) != INADDR_NONE)
    {
        strcpy(ifname, WIFI_INTERFACE_NAME);
    }
    else
    {
        strcpy(ifname, WIRE_INTERFACE_NAME);
    }
}

in_addr_t get_my_ipaddr(void)
{
    static in_addr_t __my__ip = 0;

    char ifname[64] = {0};
    get_my_ifname(ifname);

    __my__ip = net_get_ifaddr(ifname);
    return __my__ip;
}

in_addr_t get_my_netmask(void)
{
    static in_addr_t __my__mask = 0;
    char ifname[64] = {0};
    get_my_ifname(ifname);

    __my__mask = net_get_netmask(ifname);
    return __my__mask;
}

int isValidIp4_on(char *str)
{
    int segs = 0;  /* Segment count. */
    int chcnt = 0; /* Character count within segment. */
    int accum = 0; /* Accumulator for segment. */
    /* Catch NULL pointer. */
    if (str == NULL)
        return 0;
    /* Process every character in string. */
    while (*str != '\0')
    {
        /* Segment changeover. */
        if (*str == '.')
        {
            /* Must have some digits in segment. */
            if (chcnt == 0)
                return 0;
            /* Limit number of segments. */
            if (++segs == 4)
                return 0;
            /* Reset segment values and restart loop. */
            chcnt = accum = 0;
            str++;
            continue;
        }

        /* Check numeric. */
        if ((*str < '0') || (*str > '9'))
            return 0;
        /* Accumulate and check segment. */
        if ((accum = accum * 10 + *str - '0') > 255)
            return 0;
        /* Advance other segment specific stuff and continue loop. */
        chcnt++;
        str++;
    }
    /* Check enough segments and enough characters in last segment. */
    if (segs != 3)
        return 0;
    if (chcnt == 0)
        return 0;
    /* Address okay. */
    return 1;
}

int decodeDNS(char *host, char *str, int len)
{
    char **pptr;
    struct hostent *hptr;
    // char *str = malloc(sizeof(char)*100);
    if ((hptr = gethostbyname(host)) == NULL)
    {
        fprintf(stderr, "Can't get IP\n");
        return -1;
    }
    printf("HostName :%s\n", hptr->h_name);

    for (pptr = hptr->h_aliases; *pptr != NULL; pptr++)
        printf("Alias: %s \n", *pptr);

    switch (hptr->h_addrtype)
    {
    case AF_INET:
        pptr = hptr->h_addr_list;

        for (; *pptr != NULL; pptr++)
            printf("Address: %s \n", inet_ntop(hptr->h_addrtype, *pptr, str, len));
        break;

    default:
        printf("Unknown addres type");
        break;
    }
    return 0;
}

int getrel_hostname(char *hostname, char *rel_hostname, int hostname_len)
{
    char *p_start = NULL;
    p_start = strstr(hostname, "//");
    if (p_start == NULL)
    {
        return -1;
    }
    p_start += 2;
    printf("p_start:%s\n", p_start);

    char *p_tmp = p_start;
    p_tmp += strlen(p_start) - 1;

    char *p_end = p_tmp;
    while (*p_end == '/')
    {
        p_end--;
    }
    printf("p_end:%s\n", p_end);

    int len = strlen(p_start) - strlen(p_end) + 1;
    if (len >= hostname_len)
    {
        len = hostname_len - 1;
    }

    strncpy(rel_hostname, p_start, len);
    rel_hostname[len] = '\0';

    return 0;
}
