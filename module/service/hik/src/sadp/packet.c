#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>  
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>  
#include <sys/ioctl.h>

#include <net/if.h>
#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>     /* the L2 protocols */
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#endif

#include <linux/filter.h>
#include <netinet/in.h>
#include "packet.h"
#include "anj_mw_net.h"
#include "anj_mw_log.h"


static int get_iface_index(int fd, const char *device);

const char *get_netcard_name()
{
	if (Check_Link_Status(WIRE_INTERFACE_NAME))
	{
		return WIRE_INTERFACE_NAME;
	}
	else
	{
		char szWifiInterface[16] = {0};
		strncpy(szWifiInterface, net_get_wireless_name(), 16-1);		
		if( is_network_device_exist(szWifiInterface) && is_network_interface_up(szWifiInterface) )
		{
			return net_get_wireless_name();
		}
		else
		{
			return WIRE_INTERFACE_NAME;
		}
	}
}

int init_packet_capture(struct lib_cap* p)
{
	int sock;
#ifdef FRANCE_Hymatom
	struct sockaddr_in sadp_local_addr;
	//set default value
	p->device = "eth0";
	p->ifindex = -1;
	p->fd = -1;
	p->buffer = NULL;
	p->buf_len= 0;

	if ( (sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_IP))<0) {
		return -1;
	}

	int on = 1;
	setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&on,sizeof(on));

	
	sadp_local_addr.sin_family = AF_INET;
	sadp_local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sadp_local_addr.sin_port = htons(49151);

	if(0!=bind(sock,(struct sockaddr*)&sadp_local_addr,sizeof(sadp_local_addr)))
	{
		close(sock);
		sock = -1;
		return -1;
	}

#else
	struct ifreq ethreq;
	struct sock_fprog Filter;

	// tcpdump -dd ether proto 0x8033
	struct sock_filter BPF_code[]= {
		{ 0x28, 0, 0, 0x0000000c },
		{ 0x15, 0, 1, 0x00008033 },
		{ 0x6, 0, 0, 0x00000060 },
		{ 0x6, 0, 0, 0x00000000 }
	};                            

	//init filter settings
	Filter.len = 4;
	Filter.filter = BPF_code;

	//set default value
	p->device = (char *)get_netcard_name();
	p->ifindex = -1;
	p->fd = -1;
	p->buffer = NULL;
	p->buf_len= 0;

	if ( (sock=socket(PF_PACKET, SOCK_RAW, htons(0x8033)))<0) {
		return -1;
	}

	/* Set the network card in promiscuos mode */
	bzero(&ethreq,sizeof(struct ifreq));
	snprintf(ethreq.ifr_name, IFNAMSIZ, "%s", p->device);
	if (ioctl(sock,SIOCGIFFLAGS,&ethreq)==-1) {
		close(sock);
		return -1;
	}

#if 0	
	ethreq.ifr_flags|=IFF_PROMISC;
	if (ioctl(sock,SIOCSIFFLAGS,&ethreq)==-1) {
		close(sock);
		return -1;
	}
#endif	
	/* onvif_expand wait removed for anjcam port */

	/* Attach the filter to the socket */
	if(setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &Filter, sizeof(Filter))<0){
		close(sock);
		return -1;
	}

	p->ifindex = get_iface_index(sock, (const char *)p->device);
	if ( p->ifindex == -1 ) {
		close(sock);
		return -1;
	}
#endif
	p->fd = sock;
	p->buffer = malloc(1024);
	p->buf_len= 1024;

	return 0;
}

int fini_packet_capture(struct lib_cap* p)
{
	if ( p->fd != -1 ) {
		close(p->fd);
		p->fd = -1;
	}

	if ( p->buffer ) {
		free( p->buffer );
		p->buffer = NULL;
		p->buf_len = 0;
	}

	return 0;
}

int get_capture_packet(struct lib_cap* p,u_int8_t** packet)
{
	int n;
	n = recvfrom(p->fd,p->buffer,p->buf_len,0,NULL,NULL);
	if ( n < 0 )
		*packet = NULL;
	else
		*packet = p->buffer;

	return n;
} 


int send_ether_packet(struct lib_cap* p, u_int8_t* packet,u_int32_t len)
{
#ifdef FRANCE_Hymatom
	int c;
	struct sockaddr_in	sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr("255.255.255.255");
	sa.sin_port = htons(49151);
	c = sendto(p->fd, packet, len, 0, (struct sockaddr *)&sa, sizeof(sa));
	return c;
#else
	int c;
	struct sockaddr_ll sa;

	memset(&sa, 0, sizeof(sa));
	sa.sll_family = AF_PACKET;
	sa.sll_ifindex = p->ifindex; 
	sa.sll_protocol = htons(ETH_P_ALL);

	c = sendto(p->fd, packet, len, 0, (struct sockaddr *)&sa, sizeof(sa));

	__ERR("sendto %d", c);	
	return c;
#endif
}


static int get_iface_index(int fd, const char *device)
{
    struct ifreq ifr;
 
    /* memset(&ifr, 0, sizeof(ifr)); */
    strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name) - 1);
    ifr.ifr_name[sizeof(ifr.ifr_name)-1] = '\0';
 
    if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
        return (-1);
    }
 
    return ifr.ifr_ifindex;
}


