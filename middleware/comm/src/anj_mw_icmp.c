#include <stdio.h>    
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>      
#include <signal.h>           
#include <pthread.h>
#include <sys/prctl.h>  
#include <fcntl.h>

#include <resolv.h>
#include <netdb.h> 
#include <arpa/inet.h>  
#include <netinet/in.h> 
#include <netinet/ip_icmp.h>
#include <net/route.h>

#include "anj_mw_icmp.h"
#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"


typedef struct
{
    char *domain;
    unsigned int host;
    int completed;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} dns_query_t;


static pthread_mutex_t g_ping_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned short cal_chksum(unsigned short *addr, int len)
{
    if (NULL == addr)
    {
        return 0;
    }

    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while(nleft > 1)            //把ICMP报头二进制数据以2字节为单位累加起来
    {
        sum += *w++;
        nleft -= 2;
    }

    if( nleft == 1)             //若ICMP报头为奇数个字节,会剩下最后一字节.把最后一个字节视为一个2字节数据的高字节,这个2字节数据的低字节为0,继续累加
    {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

static void *thread_gethostbyname(void *arg)
{
    dns_query_t *query = (dns_query_t *)arg;
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;
    struct addrinfo *rp = NULL;
    struct sockaddr_in *sinp = NULL;
    char abuf[INET_ADDRSTRLEN] = {0};
    int ret = 0;

    pthread_detach(pthread_self());

    __ERR("start gethostbyname(%s)\n", query->domain);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    ret = getaddrinfo(query->domain, NULL, &hints, &result);
    
    pthread_mutex_lock(&query->mutex);
    
    if (ret == 0)
    {
        for (rp = result; rp != NULL; rp = rp->ai_next)
        {
            if (rp->ai_family == AF_INET)
            {
                sinp = (struct sockaddr_in *)rp->ai_addr;
                if (inet_ntop(AF_INET, &sinp->sin_addr, abuf, sizeof(abuf)) != NULL)
                {
                    query->host = sinp->sin_addr.s_addr;
                    __ERR("Resolved %s -> %s\n", query->domain, abuf);
                    break;
                }
            }
        }
        freeaddrinfo(result);
    }
    else
    {
        __ERR("getaddrinfo failed: %s\n", gai_strerror(ret));
        query->host = 0;
    }

    query->completed = 1;
    pthread_cond_signal(&query->cond);
    pthread_mutex_unlock(&query->mutex);

    __ERR("after gethostbyname(%s)\n", query->domain);
    return NULL;
}


static unsigned int gethostbyname_timeout(char *domain, int timeout_sec)
{
    pthread_t tid = 0;
    dns_query_t query = {0};
    struct timespec ts = {0};
    int iRet = 0;

    query.domain = domain;
    query.host = 0;
    query.completed = 0;

    pthread_mutex_init(&query.mutex, NULL);
    pthread_cond_init(&query.cond, NULL);

    iRet = pthread_create(&tid, NULL, thread_gethostbyname, &query);
    if (iRet != 0)
    {
        __ERR("pthread_create failed: %s\n", strerror(iRet));
        pthread_mutex_destroy(&query.mutex);
        pthread_cond_destroy(&query.cond);
        return 0;
    }

    pthread_mutex_lock(&query.mutex);
    
    if (timeout_sec > 0)
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_sec;
        
        while (!query.completed)
        {
            iRet = pthread_cond_timedwait(&query.cond, &query.mutex, &ts);
            if (iRet == ETIMEDOUT)
            {
                __ERR("DNS query timeout for %s\n", domain);
                pthread_cancel(tid);
                break;
            }
        }
    }
    else
    {
        while (!query.completed)
        {
            pthread_cond_wait(&query.cond, &query.mutex);
        }
    }

    pthread_mutex_unlock(&query.mutex);

    pthread_mutex_destroy(&query.mutex);
    pthread_cond_destroy(&query.cond);

    return query.host;
}



static int send_ping(char *sendbuf, int datalen, int sockfd, struct sockaddr_in *dest, pid_t pid)
{
#if 1
    int len = 0;
    struct icmp *icmp = NULL;

    icmp = (struct icmp *)sendbuf;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = 0;
    icmp->icmp_id = pid;
    
    len = ICMP_HEADSIZE + datalen;
    memset(icmp->icmp_data, 0xff, datalen);
    gettimeofday((struct timeval *)icmp->icmp_data, NULL);
    icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, len);

#else
    struct ip_hdr        *ip_hdr;   /*ip_hdr为IP头部结构体*/    
    struct icmp_hdr      *icmp_hdr;   /*icmp_hdr为ICMP头部结构体*/    
    int                 len;    
    int                 len1;    

    /*ip头部结构体变量初始化*/    
    ip_hdr=(struct ip_hdr *)sendbuf; /*字符串指针*/       
    ip_hdr->hlen=sizeof(struct ip_hdr)>>2;  /*头部长度*/    
    ip_hdr->ver=IPV4;   /*版本*/    
    ip_hdr->tos=0;   /*服务类型*/    
    ip_hdr->tot_len=IP_HDR_SIZE+ICMP_HDR_SIZE+datalen; /*报文头部加数据的总长度*/    
    ip_hdr->id=0;    /*初始化报文标识*/    
    ip_hdr->frag_off=0;  /*设置flag标记为0*/    
    ip_hdr->protocol=IPPROTO_ICMP;/*运用的协议为ICMP协议*/    
    ip_hdr->ttl=255; /*一个封包在网络上可以存活的时间*/    
    ip_hdr->daddr=dest->sin_addr.s_addr;  /*目的地址*/    
    len1=ip_hdr->hlen<<2;  /*ip数据长度*/    
    /*ICMP头部结构体变量初始化*/    
    icmp_hdr=(struct icmp_hdr *)(sendbuf+len1);  /*字符串指针*/    
    icmp_hdr->type=8;    /*初始化ICMP消息类型type*/    
    icmp_hdr->code=0;    /*初始化消息代码code*/    
    icmp_hdr->icmp_id=pid;   /*把进程标识码初始给icmp_id*/    
    icmp_hdr->icmp_seq=0;  /*发送的ICMP消息序号赋值给icmp序号*/        
    memset(icmp_hdr->data,0xff,datalen);  /*将datalen中前datalen个字节替换为0xff并返回icmp_hdr-dat*/      

    gettimeofday((struct timeval *)icmp_hdr->data,NULL); /* 获取当前时间*/    

    len=ip_hdr->tot_len; /*报文总长度赋值给len变量*/    
    icmp_hdr->checksum=0;    /*初始化*/    
    icmp_hdr->checksum=checksum((u8 *)icmp_hdr,len);  /*计算校验和*/    
#endif

    if (sendto(sockfd, sendbuf, len, 0, (struct sockaddr *)dest, sizeof(struct sockaddr)) < 0)
    {
        __ERR("error:ping sendto error\n");
        return -1;
    }

    return 0;
}

static int handle_pkt(char *recvbuf, int datalen, struct sockaddr_in *from, struct timeval *recvtime, pid_t pid)
{
    struct ip *ip = NULL;
    int iphdrlen = 0;
    struct icmp *icmp = NULL;

    ip = (struct ip *)recvbuf;
    iphdrlen = ip->ip_hl << 2;                      // 求ip报头长度,即ip报头的长度标志乘4
    icmp = (struct icmp *)(recvbuf + iphdrlen);     // 越过ip报头,指向ICMP报头
    datalen -= iphdrlen;                            // ICMP报头及ICMP数据报的总长度

    if (datalen < 8)
    {
        return -1;
    }

    if ((icmp->icmp_type != ICMP_ECHOREPLY) || (icmp->icmp_id != pid))
    {
        return -1;
    }

    __DBG("recv icmp reply from %s, icmp_id=%d, icmp_seq=%d\n", 
          inet_ntoa(from->sin_addr), icmp->icmp_id, icmp->icmp_seq);
    return 0;
}

static int recv_reply(char *recvbuf, int sockfd, struct sockaddr_in *from, 
               struct timeval *recvtime, pid_t pid, int timeout_ms)
{
    int n = 0;
    socklen_t len = 0;
    int nrecv = 1;

    len = sizeof(struct sockaddr_in);   /*发送ping应答消息的主机IP*/  

    unsigned long long tNowTime = anj_mw_get_cputime_ms(NULL);
    unsigned long long tEndTime = tNowTime + timeout_ms;

    while (nrecv > 0)
    {
        tNowTime = anj_mw_get_cputime_ms(NULL);
        
        if (tNowTime > tEndTime)
        {
            __ERR("ping time out after %d ms\n", timeout_ms);
            return -1;
        }

        /*经socket接收数据,如果正确接收返回接收到的字节数，失败返回0.*/
        n = recvfrom(sockfd, recvbuf, PING_BUF_SIZE, 0, (struct sockaddr *)from, &len);
        if (n <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                usleep(1000);
                continue;
            }
            else
            {
                __ERR("recvfrom error: %s\n", strerror(errno));
                return -1;
            }
        }

        gettimeofday(recvtime, NULL);                           /*记录收到应答的时间*/  

        if (handle_pkt(recvbuf, n, from, recvtime, pid) < 0)    /*接收到错误的ICMP应答信息*/ 
        {
            continue;
        }

        nrecv--;
        break;
    }

    return 0;
}

int icmp_ping_url(char *addr_url, int timeout_ms)
{
    unsigned int host = 0;
    int status = 0;
    struct timeval recvtime = {0};
    int datalen = 0;
    pid_t pid = 0;

    if ((addr_url == NULL) || (strlen(addr_url) == 0))
    {
        __ERR("icmp ping invalid addr_url(%s)\n", addr_url);
        return 0;
    }

    pthread_mutex_lock(&g_ping_mutex);

    int nTry = 0;
    while (nTry++ < 2)
    {
        host = gethostbyname_timeout(addr_url, 5);
        
        if (host != 0)
        {
            break;
        }

        if (nTry < 2)
        {
            __ERR("icmp ping dns retry:%d for url:%s\n", nTry, addr_url);
            usleep(50 * 1000);
        }
    }

    if (host == 0)
    {
        __ERR("icmp ping failed to resolve:%s\n", addr_url);
        pthread_mutex_unlock(&g_ping_mutex);
        return 0;
    }

    int sockfd = 0;                     /*发送和接收原始套接字*/
    struct sockaddr_in dest = {0};      /*被ping的主机IP*/
    struct sockaddr_in from = {0};      /*发送ping应答消息的主机IP*/

    memset(&dest, 0, sizeof(dest));     /*将dest中前sizeof(dest)个字节替换为0并返回s,此处为初始化,给最大内存清零*/  
    dest.sin_family = PF_INET;          /*PF_INET为IPV4，internet协议，在<netinet/in.h>中，地址族*/    
    dest.sin_port = ntohs(0);           /*端口号,ntohs()返回一个以主机字节顺序表达的数。*/  
    dest.sin_addr.s_addr = host;

    /*PF_INEI套接字协议族，SOCK_RAW套接字类型，IPPROTO_ICMP使用协议， 
    调用socket函数来创建一个能够进行网络通信的套接字。这里判断是否创建成功*/
    sockfd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        __ERR("icmp ping raw socket created error!\n");
        pthread_mutex_unlock(&g_ping_mutex);
        return 0;
    }

    //不阻塞
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

#if 0  
    /*设置当前套接字选项特定属性值，sockfd套接字，IPPROTO_IP协议层为IP层， 
    IP_HDRINCL套接字选项条目，套接字接收缓冲区指针，sizeof(on)缓冲区长度的长度*/  
    int on = 1;
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));
#endif

    pid = getpid();
    datalen = PING_DATA_LEN;

    char *sendbuf = (char *)anj_mw_malloc(PING_BUF_SIZE);
    char *recvbuf = (char *)anj_mw_malloc(PING_BUF_SIZE);
    
    if (sendbuf == NULL || recvbuf == NULL)
    {
        __ERR("malloc failed\n");
        status = 0;
        goto __cleanup;
    }

    memset(sendbuf, 0, PING_BUF_SIZE);
    memset(recvbuf, 0, PING_BUF_SIZE);

    if (send_ping(sendbuf, datalen, sockfd, &dest, pid) < 0)
    {
        status = 0;
        goto __cleanup;
    }

    if (recv_reply(recvbuf, sockfd, &from, &recvtime, pid, timeout_ms) < 0)
    {
        status = 0;
    }
    else
    {
        status = 1;
    }

__cleanup:
    if (sockfd > 0)
    {
        close(sockfd);
    }

    if (sendbuf != NULL)
    {
        anj_mw_free(sendbuf);
        sendbuf = NULL;
    }

    if (recvbuf != NULL)
    {
        anj_mw_free(recvbuf);
        recvbuf = NULL;
    }

    pthread_mutex_unlock(&g_ping_mutex);

    return status;
}

