#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <netinet/ip_icmp.h>

#include "anj_mw_time.h"
#include "anj_mw_ping.h"
#include "anj_mw_comm.h"

#define ICMP_OK 0
#define ICMP_CMM_ERR -1
#define ICMP_WRONGPARAM_ERR -2
#define ICMP_SOCKET_ERR -3
#define ICMP_SENDTO_ERR -4
#define ICMP_RECVFROM_ERR -5
#define ICMP_SELECT_ERR -6

#define ARPING_DEFAULT_TIMEOUT (1500)
#define ARPING_MIN_TIMEOUT (20)
#define PING_RECVSIZE 1024
#define PING_BUFFERSIZE 72

#define PING_TIMETOLIVE 64
#define PING_RECVBUFFER 50 * 1024

// ARP消息包结构
typedef struct tagArpMsg
{
    struct ethhdr ethhdr;     /* Ethernet header */
    unsigned short htype;     /* hardware type (must be ARPHRD_ETHER) */
    unsigned short ptype;     /* protocol type (must be ETH_P_IP) */
    unsigned char hlen;       /* hardware address length (must be 6) */
    unsigned char plen;       /* protocol address length (must be 4) */
    unsigned short operation; /* ARP opcode */
    unsigned char sHaddr[6];  /* sender's hardware address */
    unsigned char sInaddr[4]; /* sender's IP address */
    unsigned char tHaddr[6];  /* target's hardware address */
    unsigned char tInaddr[4]; /* target's IP address */
    unsigned char pad[18];    /* pad for min. Ethernet payload (60 bytes) */
} ArpMsg;

static unsigned short icmp_cksum(unsigned char *data, int len)
{
    int sum = 0;
    int odd = len & 1;

    while (len & 0xfffe)
    {
        sum += *(unsigned short *)data;
        data += 2;
        len -= 2;
    }

    if (odd)
    {
        unsigned short tmp = ((*data) << 8) & 0xff00;
        sum += tmp;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);

    return ~sum;
}

static int icmp_pack(struct icmp *icmph, int pid, int length)
{
    unsigned char i = 0;

    if (NULL == icmph)
        return -1;

    memset(icmph, 0, sizeof(struct icmp));
    icmph->icmp_type = ICMP_ECHO;
    icmph->icmp_code = 0;
    icmph->icmp_cksum = 0;
    icmph->icmp_seq = 0;
    icmph->icmp_id = pid;

#if 1
    for (i = 0; i < length; i++)
    {
        icmph->icmp_data[i] = i;
    }
#endif
    icmph->icmp_cksum = icmp_cksum((unsigned char *)icmph, length);
    return length;
}

/*****************************************************************************
 函 数 名  : tv_sub
 功能描述  : 获取某个操作经历的时间长
 输入参数  : out 结束时间
             in  开始时间
 输出参数  : 无
 返 回 值  :
 调用函数  :
 被调函数  :
*****************************************************************************/
void tv_sub(struct timeval *out, struct timeval *in)
{
    out->tv_sec -= in->tv_sec;
    out->tv_usec -= in->tv_usec;
}

/*****************************************************************************
 函 数 名  : myPing
 功能描述  : 网络ping工具发接包
 输入参数  : ips 		IP地址/网关/域名
             timeout	超时等待时间
             isReceived	标识packet是否接受到
             responseTime	回应时间
 输出参数  : ping命令的输出，包括生存时间、回应时间
 返 回 值  :
 调用函数  :
 被调函数  :
*****************************************************************************/
char *myPing(const char *ips, int timeout, int *isReceived, unsigned long long *responseTime)
{
    struct timeval timeo;
    struct timeval tvrecv;
    struct timeval tvsend;
    int sockfd;
    struct sockaddr_in addr;
    struct sockaddr_in from;
    //	struct in_addr ipv4_addr;
    //	struct hostent *ipv4_host;

    struct ip *iph;
    struct icmp *icmp;
    struct iphdr *iph2;

    char sendpacket[PING_BUFFERSIZE];
    char recvpacket[2 * PING_RECVSIZE];

    int n;
    int ttl = PING_TIMETOLIVE;  // 生存时间
    int size = PING_RECVBUFFER; // 套接字接收缓存50K
    //	int iphdrlen = 0;
    //	int sendTime = 0;
    //	int recvTime = 0;
    unsigned long long rtt = 0;

    static char pingResult[500];
    pid_t pid;
    fd_set readfds;

    memset(pingResult, 0, sizeof(pingResult));

    // 设定Ip信息
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ips);

    // 解析域名
    /*
    error = inet_aton(ips, &ipv4_addr);
    if (!error)
    {
        ipv4_host = gethostbyname(ips);
        if (NULL == ipv4_host)
        {
            printf("connect: Invalid argument\n");
            *isReceived = ICMP_WRONGPARAM_ERR;
            return pingResult;
        }

        memcpy(&(addr.sin_addr), (struct in_addr*)ipv4_host->h_addr, sizeof(struct in_addr));
    }
    else
    {
        memcpy(&(addr.sin_addr), &(ipv4_addr.s_addr), sizeof(struct in_addr));
    }*/

    // 取得socket
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        *isReceived = ICMP_CMM_ERR;
        return pingResult;
    }

    // 设定TimeOut时间
    timeo.tv_sec = timeout / 1000;
    timeo.tv_usec = (timeout % 1000) * 1000;

    if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) ||
        -1 == setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) ||
        -1 == setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)))
    {
        close(sockfd);
        *isReceived = ICMP_CMM_ERR;
        return pingResult;
    }

    // 取得PID，作为Ping的Sequence ID
    pid = getpid();

    icmp_pack((struct icmp *)sendpacket, pid, 64);

    gettimeofday(&tvsend, NULL);

    // 发包
    n = sendto(sockfd, (char *)&sendpacket, 64, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (n < 1)
    {
        close(sockfd);
        *isReceived = ICMP_CMM_ERR;
        return pingResult;
    }

    // 接受
    // 由于可能接受到其他Ping的应答消息，所以这里要用循环
    // 设定TimeOut时间，这次才是真正起作用的
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        n = select(sockfd + 1, &readfds, NULL, NULL, &timeo);
        if (n <= 0)
        {
            close(sockfd);
            *isReceived = ICMP_CMM_ERR;
            return pingResult;
        }
        // 接受
        memset(recvpacket, 0, sizeof(recvpacket));
        unsigned int fromlen = sizeof(from);
        n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, &fromlen);
        if (n < 1)
        {
            close(sockfd);
            *isReceived = ICMP_CMM_ERR;
            return pingResult;
        }
        gettimeofday(&tvrecv, NULL);

        // 判断是否是自己Ping的回复
        char *from_ip = (char *)inet_ntoa(from.sin_addr);
        if (strcmp(from_ip, ips) != 0)
        {
            continue;
        }

        iph = (struct ip *)recvpacket;
        iph2 = (struct iphdr *)recvpacket;
        //		iphdrlen = iph2->ihl<<2;
        icmp = (struct icmp *)(recvpacket + (iph->ip_hl << 2));

        //  printf("ip:%s,port:%d,icmp->icmp_type:%d,icmp->icmp_id:%d\n",ips, from.sin_port, icmp->icmp_type,icmp->icmp_id);
        // 判断Ping回复包的状态
        if (ICMP_ECHOREPLY == icmp->icmp_type && icmp->icmp_id == pid)
        {
            tvrecv.tv_sec -= tvsend.tv_sec;
            tvrecv.tv_usec -= tvsend.tv_usec;
            rtt = tvrecv.tv_sec * 1000000 + tvrecv.tv_usec;
            snprintf(pingResult, sizeof(pingResult), "%d bytes from %s: ttl=%d rtt=%.3fms",
                     n, inet_ntoa(from.sin_addr), iph2->ttl, (float)(rtt) / 1000);
            *isReceived = ICMP_OK;
            *responseTime = rtt;
            break;
        }
        else
        {
            continue;
        }
    }

    // 关闭socket
    close(sockfd);
    return pingResult;
    // return 0;
}

int ping(const char *ips, int timeout, const char *net_dev)
{
    struct timeval timeo;
    int sockfd;
    struct sockaddr_in addr;
    struct sockaddr_in from;

    struct ip *iph;
    struct icmp *icmp;

    char sendpacket[PING_BUFFERSIZE];
    char recvpacket[2 * PING_RECVSIZE];

    int n;
    pid_t pid;
    fd_set readfds;

    // 设定Ip信息
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ips);

    // 取得socket
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        //    perror("socket\n");
        return -1;
    }

    if (net_dev != NULL)
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", net_dev);
        if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr)) == -1)
        {
            printf("ping SO_BINDTODEVICE failed\n");
            close(sockfd);
            return -1;
        }
    }

    // 设定TimeOut时间
    timeo.tv_sec = timeout / 1000;
    timeo.tv_usec = (timeout % 1000) * 1000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo)) == -1)
    {
        //   perror("setsockopt");
        close(sockfd);
        return -1;
    }

    // 取得PID，作为Ping的Sequence ID
    pid = getpid();
    icmp_pack((struct icmp *)sendpacket, pid, 64);

    // 发包
    n = sendto(sockfd, (char *)&sendpacket, 64, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (n < 1)
    {
        //  perror("sendto");
        close(sockfd);
        return -1;
    }

    // 接受
    // 由于可能接受到其他Ping的应答消息，所以这里要用循环
    while (1)
    {
        // 设定TimeOut时间，这次才是真正起作用的
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        n = select(sockfd + 1, &readfds, NULL, NULL, &timeo);
        if (n <= 0)
        {
            //     printf("ip: %s, time out error\n", ips);
            close(sockfd);
            return -1;
        }

        // 接受
        memset(recvpacket, 0, sizeof(recvpacket));
        unsigned int fromlen = sizeof(from);
        n = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, &fromlen);
        if (n < 1)
        {
            close(sockfd);
            return -1;
        }

        // 判断是否是自己Ping的回复
        char *from_ip = (char *)inet_ntoa(from.sin_addr);
        //  printf("from ip: %s\n", from_ip);
        if (strcmp(from_ip, ips) != 0)
        {
            continue;
        }

        iph = (struct ip *)recvpacket;
        icmp = (struct icmp *)(recvpacket + (iph->ip_hl << 2));

        //  printf("ip:%s,port:%d,icmp->icmp_type:%d,icmp->icmp_id:%d\n",ips, from.sin_port, icmp->icmp_type,icmp->icmp_id);
        // 判断Ping回复包的状态
        if (icmp->icmp_type == ICMP_ECHOREPLY && icmp->icmp_id == pid)
        {
            break;
        }
        else
        {
            continue;
        }
    }

    // 关闭socket
    close(sockfd);
    return 0;
}

/*****************************************************************************
 函 数 名  : try_ping
 功能描述  : ping 指定ip，net_dev为空时不指定网卡
 输入参数  : ips ip地址，timeout 超时时间/毫秒，cnt 尝试次数，net_dev 网卡，run_flag运行标志，1运行，0退出
 输出参数  : NULL
 返 回 值  : 成功0，失败-1
*****************************************************************************/
int try_ping(char *ips, int timeout, int cnt, const char *net_dev, const int *run_flag)
{
    int ret = -1;
    if (ips == NULL || strlen(ips) == 0)
    {
        return ret;
    }

    for (int i = 0; i < cnt; i++)
    {
        if (run_flag)
        {
            if (*run_flag == 0)
            {
                return ret;
            }
        }
        ret = ping(ips, timeout, net_dev);
        if (ret == 0)
        {
            return ret;
        }
    }
    return ret;
}

int test_network(char *name)
{
#if 0
    int i;
    int ret;

    //Ping 3次 每次500ms
    for (i = 0; i < 3; i++)
    {
      ret =  ping(ip, 500);
      if (0 == ret)
          return 0;
    }
    return ret;
#endif
    if (NULL == name)
    {
        return 1;
    }

    char buf[50] = {0};
    sprintf(buf, "ping -c 1 -W 1 %s", name);

    FILE *fp = NULL;

    if (NULL == (fp = popen(buf, "r")))
    {
        printf("fopen error!");
        return 1;
    }

    char cResolveR[80] = {0};

    if (NULL == fgets(cResolveR, 80, fp))
    {
        if (feof(fp))
            printf("EOF!");
        else if (ferror(fp))
            printf("steam error!");

        pclose(fp);
        return 1;
    }

    printf("%s", cResolveR);
    char *pLparenthesis = strchr(cResolveR, '(');
    if (NULL != pLparenthesis)
    {
        char *pRparenthesis = ++pLparenthesis;
        pRparenthesis = strchr(pRparenthesis, ')');
        if (NULL != pRparenthesis)
        {
            *pRparenthesis = 0;
            printf("%s\n", pLparenthesis);
        }
    }

    memset(cResolveR, 0, 80);
    if (NULL == fgets(cResolveR, 80, fp))
    {
        if (feof(fp))
            printf("EOF!");
        else if (ferror(fp))
            printf("steam error!");

        pclose(fp);
        return 1;
    }
    printf("%s", cResolveR);
    if ((strcmp("\n", cResolveR) == 0) ||
        (strcmp("", cResolveR) == 0))
    {
        pclose(fp);
        return 1;
    }

    if (-1 == pclose(fp))
    {
        printf("pclose error!");
        return 1;
    }
    return 0;
}

int arpping(unsigned int destIp, unsigned int sourceIp, const char *mac, int timeOutMs, char *ifname)
{
    int nRet = 0; /* return value */
    int nSpendTime = 0;
    int optval = 1;
    int sockFd = -1;      /* socket */
    struct sockaddr addr; /* for interface name */
    unsigned int iSenderIp;
    ArpMsg arp;
    fd_set fdset;
    struct timeval tmBlock;
    struct timeval curTime;
    struct timeval prevTime;
    if (NULL == mac)
    {
        __ERR("Invalid input mac.\n");
        return -1;
    }

    timeOutMs = (timeOutMs < ARPING_MIN_TIMEOUT) ? ARPING_DEFAULT_TIMEOUT : timeOutMs;

    /*socket发送一个arp包*/
    if ((sockFd = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP))) == -1)
    {
        __ERR("Could not open raw socket.\n");
        return -1;
    }

    /*设置套接口类型为广播，把这个arp包是广播到这个局域网*/
    if (setsockopt(sockFd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) == -1)
    {
        __ERR("Could not setsocketopt on raw socket.\n");
        close(sockFd);
        return -1;
    }

    /* 对arp设置，这里按照arp包的封装格式赋值即可，详见http://blog.csdn.net/wanxiao009/archive/2010/05/21/5613581.aspx */
    memset(&arp, 0, sizeof(arp));
    memset(arp.ethhdr.h_dest, 0xff, 6);    /* MAC DA */
    memcpy(arp.ethhdr.h_source, mac, 6);   /* MAC SA */
    arp.ethhdr.h_proto = htons(ETH_P_ARP); /* protocol type (Ethernet) */
    arp.htype = htons(ARPHRD_ETHER);       /* hardware type */
    arp.ptype = htons(ETH_P_IP);           /* protocol type (ARP message) */
    arp.hlen = 6;                          /* hardware address length */
    arp.plen = 4;                          /* protocol address length */
    arp.operation = htons(ARPOP_REQUEST);  /* ARP op code */
    if (destIp == sourceIp)
    {
        if (destIp != htonl(0xa9fe010a))
        {
            iSenderIp = htonl(0xa9fe010a); /* Use Reserve IP for Sender */
        }
        else
        {
            iSenderIp = htonl(0xa9fe010b); /* Use Reserve IP for Sender */
        }
    }
    else
    {
        iSenderIp = sourceIp;
    }
    *((u_int *)arp.sInaddr) = iSenderIp; /* Sender IP address */
    memcpy(arp.sHaddr, mac, 6);          /* Sender hardware address */
    memset(arp.tHaddr, 0xff, 6);         /* target hardware address */
    *((u_int *)arp.tInaddr) = destIp;    /* target IP address */

    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sa_data, ifname);
    /*发送arp请求*/
    if (sendto(sockFd, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0)
    {
        __ERR("arpping sendto failed.\n");
        close(sockFd);
        return -1;
    }

    /* 利用select函数进行多路等待*/
    SystemGetTimeofRun(&prevTime, NULL);
    while (timeOutMs > 10)
    {
        FD_ZERO(&fdset);
        FD_SET(sockFd, &fdset);
        tmBlock.tv_sec = timeOutMs / 1000;
        tmBlock.tv_usec = (timeOutMs % 1000) * 1000;
        if (select(sockFd + 1, &fdset, (fd_set *)NULL, (fd_set *)NULL, &tmBlock) < 0)
        {
            if (errno != EINTR)
            {
                nRet = -1;
                break;
            }
        }
        else if (FD_ISSET(sockFd, &fdset))
        {
            if (recv(sockFd, &arp, sizeof(arp), 0) < 0)
            {
                nRet = 0;
            }

            if (arp.operation == htons(ARPOP_REPLY))
            {
                /*ARP应答有效,说明这个地址是已经存在的*/
                if ((memcmp(arp.sHaddr, mac, 6) != 0) && (memcmp(arp.tHaddr, mac, 6) == 0) && (*((u_int *)arp.sInaddr) == destIp))
                {
                    nRet = 1;
                    break;
                }
            }
        }
        SystemGetTimeofRun(&curTime, NULL);
        nSpendTime = (curTime.tv_sec - prevTime.tv_sec) * 1000 + curTime.tv_usec / 1000 - prevTime.tv_usec / 1000;
        if (nSpendTime >= 0)
        {
            timeOutMs = timeOutMs - nSpendTime;
        }
        else
        {
            timeOutMs = timeOutMs - 200; // 耗时计算出错，固定减200ms
        }
        prevTime.tv_sec = curTime.tv_sec;
        prevTime.tv_usec = curTime.tv_usec;
    }
    close(sockFd);

    return nRet;
}
