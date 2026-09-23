/**
根据指定IP地址获取MAC地址，可用于判断IP是否被占用
*/
  
#include <stdlib.h>
#include <sys/param.h>  
#include <sys/socket.h>
#include <linux/sockios.h> 
#include <sys/file.h>  
#include <sys/time.h>  
#include <sys/signal.h>  
#include <sys/ioctl.h>  
#include <linux/if.h>  
#include <linux/if_arp.h>  
#include <sys/uio.h>  
  
#include <netdb.h>  
#include <unistd.h>  
#include <stdio.h>  
#include <ctype.h>  
#include <errno.h>  
#include <string.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  

// #include <debug_util.h>
#include "arp_get_mac.h"
#ifndef HT_API
#define HT_API
#endif
#ifndef uint32
typedef unsigned int uint32;
#endif
#include "sys_log.h"
   
//struct timeval last; 
//int sent;  
//int brd_sent;
//int received, brd_recv, req_recv;
//int unicasting;     
  
#define MS_TDIFF(tv1,tv2) ( ((tv1).tv_sec-(tv2).tv_sec)*1000 + ((tv1).tv_usec-(tv2).tv_usec)/1000 )    

int send_pack(int fd, struct in_addr src, struct in_addr dst,  
              struct sockaddr_ll *ME, struct sockaddr_ll *HE)  
{
		int advert=0;
		
        int err;  
        struct timeval now;  
        unsigned char buf[256];  
        struct arphdr *ah = (struct arphdr*)buf;  
        unsigned char *p = (unsigned char *)(ah+1);  
         
        ah->ar_hrd = htons(ME->sll_hatype);        /* 硬件地址类型*/  
        if (ah->ar_hrd == htons(ARPHRD_FDDI))  
                ah->ar_hrd = htons(ARPHRD_ETHER);  
        ah->ar_pro = htons(ETH_P_IP);                /* 协议地址类型   */  
        ah->ar_hln = ME->sll_halen;                /* 硬件地址长度   */  
        ah->ar_pln = 4;                                /* 协议地址长度 */  
        ah->ar_op  = advert ? htons(ARPOP_REPLY) : htons(ARPOP_REQUEST);/* 操作类型*/  
  
        memcpy(p, &ME->sll_addr, ah->ar_hln);                       /* 发送者硬件地址*/  
        p+=ME->sll_halen;        /*以太网为6*/  
  
        memcpy(p, &src, 4);                /* 发送者IP */  
        p+=4;  
        /* 目的硬件地址*/  
        if (advert)  
                memcpy(p, &ME->sll_addr, ah->ar_hln);  
        else  
                memcpy(p, &HE->sll_addr, ah->ar_hln);  
        p+=ah->ar_hln;  
  
        memcpy(p, &dst, 4);                /* 目的IP地址*/  
        p+=4;  
  
        gettimeofday(&now, NULL);  
        err = sendto(fd, buf, p-buf, 0, (struct sockaddr*)HE, sizeof(*HE));  
#if 0		
        if (err == p-buf) {  
//                last = now;  
//                sent++;  
//                if (!unicasting)  
//                        brd_sent++;  
        }  
#endif		
        return err;  
}  


int is_time_out(struct timeval *start,int timeout_ms)
{
	struct timeval tv;  
    gettimeofday(&tv, NULL);  

    if ((start->tv_sec==0)&&(start->tv_usec==0)){  
        *start = tv;  
    }

    if (timeout_ms && MS_TDIFF(tv,*start) > timeout_ms)  
		return 1;
	else
		return 0; 
}


/*数据包分析主程序. 
把ARP 请求和答复的数据包格式 
 
                             |---------------28 bytes arp request/reply-----------------------------|        
|--------ethernet header----| 
_____________________________________________________________________________________________________ 
|ethernet | ethernet| frame|hardware|protocol|hardware|protocol|op|sender  |sender|target  |target| 
|dest addr|src addr | type| type   |type    | length |length  |  |eth addr| IP   |eth addr| IP    | 
-----------------------------------------------------------------------------------------------------   
  6 types    6        2      2        2         1        1      2     6       4       6        4 
*/  
int recv_pack(unsigned char *buf, int len, struct sockaddr_ll *FROM, struct sockaddr_ll *me,
			struct in_addr *src, struct in_addr *dst, unsigned char *dst_mac, int *recv_count)  
{  
    struct timeval tv;  
    struct arphdr *ah = (struct arphdr*)buf;  
    unsigned char *p = (unsigned char *)(ah+1);  
    struct in_addr src_ip, dst_ip;  

    gettimeofday(&tv, NULL);  

    /* Filter out wild packets */  
    if (FROM->sll_pkttype != PACKET_HOST &&  
        FROM->sll_pkttype != PACKET_BROADCAST &&  
        FROM->sll_pkttype != PACKET_MULTICAST){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    }
    /*到这里pkttype为HOST||BROADCAST||MULTICAST*/  
     
    /* Only these types are recognised */  
    /*只要ARP request and reply*/  
    if (ah->ar_op != htons(ARPOP_REQUEST) &&  
        ah->ar_op != htons(ARPOP_REPLY)){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    }
		
    /* ARPHRD check and this darned FDDI hack here :-( */  
    if (ah->ar_hrd != htons(FROM->sll_hatype) &&  
        (FROM->sll_hatype != ARPHRD_FDDI || ah->ar_hrd != htons(ARPHRD_ETHER))){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    }  

    /* Protocol must be IP. */  
    if (ah->ar_pro != htons(ETH_P_IP)){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    }  
    if (ah->ar_pln != 4){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    }  
    if (ah->ar_hln != me->sll_halen){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    }  
    if (len < sizeof(*ah) + 2*(4 + ah->ar_hln)){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    }  
    /*src_ip:对方的IP 
      det_ip:我的IP*/  
    memcpy(&src_ip, p+ah->ar_hln, 4);  
    memcpy(&dst_ip, p+ah->ar_hln+4+ah->ar_hln, 4);  
	(*recv_count)++;

    log_print(HT_LOG_DBG, "[%s:%d] res_src=%s, res_dst=%s\n", __FUNCTION__, __LINE__, inet_ntoa(src_ip), inet_ntoa(dst_ip));
	
	
    /* DAD packet was: 
       src_ip = 0 (or some src) 
       src_hw = ME 
       dst_ip = tested address 
       dst_hw = <unspec>; 

       We fail, if receive request/reply with: 
       src_ip = tested_address 
       src_hw != ME 
       if src_ip in request was not zero, check 
       also that it matches to dst_ip, otherwise 
       dst_ip/dst_hw do not matter. 
     */  
     /*dst.s_addr是我们发送请求是置的对方的IP,当然要等于对方发来的包的src_ip啦*/  
    if (src_ip.s_addr != dst->s_addr){ 
        log_print(HT_LOG_DBG, "[%s:%d] res_src=%s, req_dst=%s\n", __FUNCTION__, __LINE__, inet_ntoa(src_ip), inet_ntoa(*dst));
        return 0;  
    }  
    if (memcmp(p, &me->sll_addr, me->sll_halen) == 0){ 
//        log_print(HT_LOG_ERR, "[%s:%d] debug\n",__FUNCTION__,__LINE__);
        return 0;  
    } 

	/*同理,src.s_addr是我们发包是置的自己的IP,要等于对方回复包的目的地址*/  
        if (src->s_addr && src->s_addr != dst_ip.s_addr){ 
            log_print(HT_LOG_INFO, "[%s:%d] req_src=%s, res_dst=%s\n", __FUNCTION__, __LINE__, inet_ntoa(*src), inet_ntoa(dst_ip));
        return 0;  
    }
      

	int i=0;
	for(i=0;(i<ARP_MAC_BYTE)&&(i<ah->ar_hln);i++){
		dst_mac[i] = p[i];
	}

//    received++;  
//    if (FROM->sll_pkttype != PACKET_HOST)  
//        brd_recv++;  
//    if (ah->ar_op == htons(ARPOP_REQUEST))  
//        req_recv++;   
  
    return 1;  
}  

/**

返回值 :		-1	错误
			0	不存在
			1	存在
			2	请求IP是本设备IP,且IP可用
*/
int arp_get_mac(const char *if_name,const char *str_src_ip, const char *str_dst_ip,unsigned char *dst_mac,int timeout_ms)  
{
    log_print(HT_LOG_DBG, "arp_get_mac: if_name(%s) src_ip(%s) dst_ip(%s) timeout_ms(%d)\n",if_name,str_src_ip,str_dst_ip,timeout_ms);
	if((if_name==NULL)||(str_src_ip==NULL)||(str_dst_ip==NULL)||(dst_mac==NULL)||(timeout_ms==0)){
        log_print(HT_LOG_ERR, "invalid input parameter\n");
		return -1;
	}
	
	int i=0;
	for(i=0;i<ARP_MAC_BYTE;i++){
		dst_mac[i] = 0;
	}

	struct in_addr src;
	struct in_addr dst;
	struct sockaddr_ll me;
	struct sockaddr_ll he;
	memset(&src,0,sizeof(struct in_addr));
	memset(&dst,0,sizeof(struct in_addr));
	memset(&me,0,sizeof(struct sockaddr_ll));
	memset(&he,0,sizeof(struct sockaddr_ll));
	
	int recv_count=0;
	int is_same_ip = (!strcmp(str_src_ip,str_dst_ip))?1:0;
    int socket_errno=0;  
    uid_t uid = getuid();  
	setuid(uid);
    /*取得一个packet socket. 
    int packet_sock=socket(PF_PACKET,int sock_type,int protocol); 
    其中sock_type有两种: 
        1.SOCK_RAW,使用类型的套接字,那么当你向设备写数据时,要提供physical layer 
    header.当从设备读数据时,得到的数据是含有physical layer header的 
        2.SOCK_DGRAM.这种类型的套接字使用在相对高层.当数据传送给用户之前,physical layer 
        header已经去掉了*/  
    int fd = socket(PF_PACKET, SOCK_DGRAM, 0);  
    socket_errno = errno;  
	if (fd < 0) {  
        errno = socket_errno;  
 //       log_print(HT_LOG_ERR, "arping: socket: %s\r\n", strerror(errno));  
		log_print(HT_LOG_ERR, "arping: socket errno %d: %s", errno, strerror(errno));
        return -1;  
    }

	 
    struct ifreq ifr;  
    memset(&ifr, 0, sizeof(ifr));  
    strncpy(ifr.ifr_name, if_name, IFNAMSIZ-1);  // src 网卡 eth0
    // 判断网卡是否存在
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {  
        log_print(HT_LOG_ERR, "arping: unknown iface %s\n", if_name);  
		close(fd);

		log_print(HT_LOG_ERR, "arping: unknown iface errno %d: %s", errno, strerror(errno));

        return -1;  
    }  
	
    int ifindex = ifr.ifr_ifindex;  

    if (ioctl(fd, SIOCGIFFLAGS, (char*)&ifr)) {  
        log_print(HT_LOG_ERR, "ioctl(SIOCGIFFLAGS): %s\r\n", strerror(errno));  
        close(fd);
		log_print(HT_LOG_ERR, "arping: ioctl(SIOCGIFFLAGS) errno %d: %s", errno, strerror(errno));
        return -1;  
    }  
	
    /*设备当然是要up的想要bring up eth0 可以/etc/sysconfig/network-scripts/ifup eth0*/  
	//网卡被禁用
    if (!(ifr.ifr_flags&IFF_UP)) {   
        log_print(HT_LOG_ERR, "Interface \"%s\" is down\n", if_name);  
        close(fd);
        return -1;   
    }  
	//网卡禁用ARP功能
    if (ifr.ifr_flags&(IFF_NOARP|IFF_LOOPBACK)) {   
        log_print(HT_LOG_ERR, "Interface \"%s\" is not ARPable\n", if_name);  
        close(fd);
        return -1;   
    }  

	// 设置超时  
    struct timeval timeout;  
    timeout.tv_sec = timeout_ms/1000;//秒  
    timeout.tv_usec = (timeout_ms%1000)*1000;//微秒  
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {  
        log_print(HT_LOG_ERR, "setsockopt failed:: %s\r\n", strerror(errno)); 
		close(fd);
		log_print(HT_LOG_ERR, "arping: setsockopt failed: errno %d: %s", errno, strerror(errno));
        return -1;
    } 


    if (inet_aton(str_src_ip, &src) != 1) {  
        struct hostent *hp;  
        hp = gethostbyname2(str_src_ip, AF_INET);  
        if (!hp) {  
            close(fd);
			log_print(HT_LOG_ERR, "arping: unknown host %s", str_src_ip);
    		return -1;   
        }  
        memcpy(&src, hp->h_addr, 4);  
    }  


	//转换得到 dst ip
    if (inet_aton(str_dst_ip, &dst) != 1) {  
        struct hostent *hp;  
        hp = gethostbyname2(str_dst_ip, AF_INET);  
        if (!hp) {  
            close(fd);
			log_print(HT_LOG_ERR, "arping: unknown host %s", str_dst_ip);
    		return -1;   
        }  
        memcpy(&dst, hp->h_addr, 4);  
    }  

    me.sll_family = AF_PACKET;  
    me.sll_ifindex = ifindex;  
    me.sll_protocol = htons(ETH_P_ARP);  
    /* 只想要由me指定的接口收到的数据包*/  
    if (bind(fd, (struct sockaddr*)&me, sizeof(me)) == -1) {  
//        log_print(HT_LOG_ERR, "bind: %s\r\n", strerror(errno));  
        close(fd);
		log_print(HT_LOG_ERR, "arping: bind failed: errno %d: %s", errno, strerror(errno));
        return -1;  
    }  

     
    int alen = sizeof(me);  
    /*get link layer information   是下面这些.因为sll_family sll_ifindex sll_protocol已知 
      unsigned short  sll_hatype;            Header type 
      unsigned char   sll_pkttype;           Packet type 
      unsigned char   sll_halen;             Length of address 
      unsigned char   sll_addr[8];           Physical layer address */  
    if (getsockname(fd, (struct sockaddr*)&me, (socklen_t *)&alen) == -1) {  
        log_print(HT_LOG_ERR, "getsockname: %s\r\n", strerror(errno));  
        close(fd);
		log_print(HT_LOG_ERR, "arping: getsockname failed: errno %d: %s", errno, strerror(errno));
		return -1;   
    }  
     
    if (me.sll_halen == 0) {   
        log_print(HT_LOG_ERR, "Interface \"%s\" is not ARPable (no ll address)\n", if_name);  
        close(fd);
        return -1;   
    }  

    he = me;  
    /*把他的地址设为ff:ff:ff:ff:ff:ff  即广播地址,当然假设是以太网*/  
    memset(he.sll_addr, -1, he.sll_halen);  

  
    log_print(HT_LOG_DBG, "ARPING %s from %s %s\n", inet_ntoa(dst), inet_ntoa(src), if_name ? : "");  
       

	send_pack(fd, src, dst, &me, &he);

	struct timeval start;
	memset(&start,0,sizeof(struct timeval));
	int recv_status = 0;
    while(1) {  
		if(is_time_out(&start, timeout_ms))
			break;
		
        char packet[4096];  
        struct sockaddr_ll from;  
		memset(&from,0,sizeof(struct sockaddr_ll));
        int alen = sizeof(from);  
        int cc=0;  
        /*注意s的类型是SOCK_DGRAM,所以收到的数据包里没有link layer info,这些信息被记录在from里*/  
        if ((cc = recvfrom(fd, packet, sizeof(packet), 0,  
                           (struct sockaddr *)&from, (socklen_t *)&alen)) < 0) {  
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                log_print(HT_LOG_ERR, "arping: recvfrom %s failed: errno %d: %s", str_dst_ip, errno, strerror(errno));
            }
            continue;  
        }  
 
        recv_status = recv_pack((unsigned char *)packet, cc, &from, &me, &src, &dst, dst_mac, &recv_count);  
		if(is_same_ip && (!recv_status))
		{
			if(recv_count>0)
			{
				recv_status=2;
				break;
			}
		}
		if(!recv_status)
			continue;
 
		break;
    } 

	if(is_same_ip && (recv_status!=1)){
		recv_status=2;
	}
	
	if(recv_status>0){
        log_print(HT_LOG_DBG, "recv_status=%d,dst_mac=[%02X:%02X:%02X:%02X:%02X:%02X]\n",
            recv_status,dst_mac[0],dst_mac[1],dst_mac[2],dst_mac[3],dst_mac[4],dst_mac[5]);
	}
	close(fd);

	return recv_status;
}

#if 0
int main(int argc, char **argv) 
{
	if(argc!=2)
		return 0;
	
	char *if_name = "eth0";
	char *str_src_ip = "192.168.4.158";
	char *str_dst_ip = argv[1];
	unsigned char dst_mac[ARP_MAC_BYTE]={0};

	int ret = arp_get_mac(if_name,str_src_ip,str_dst_ip,dst_mac, 1000);
	log_print(HT_LOG_INFO, "ret=%d,dst_mac=[%02X:%02X:%02X:%02X:%02X:%02X]\n",ret,dst_mac[0],dst_mac[1],dst_mac[2],dst_mac[3],dst_mac[4],dst_mac[5]);

	return 0;
}
#endif

