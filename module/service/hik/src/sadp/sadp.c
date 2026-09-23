#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>

#include "sadp.h"
#include "MD5.h"
#include "packet.h"
#include <malloc.h>
#include <sys/prctl.h>
#include "anj_mw_log.h"
#include "anj_mw_net.h"
#include "anj_mw_file.h"
#include "anj_mw_time.h"

#ifndef FALSE
#define FALSE 	0
#endif

#ifndef TRUE
#define TRUE 	1
#endif

#ifndef BOOL
#define BOOL int
#endif


#define PACKET_SIZE	1024

struct SADP
{
	struct lib_cap descr;

	unsigned int seq;
	u_int32_t* packet;
	u_int32_t packet_s;

	char password[PWD_LEN];
	unsigned char serialno[DEVICE_SERIALNO_LEN];
	unsigned int  mask;
	unsigned int  src_ip;
	unsigned char src_mac[MAC_ADDR_LEN];

	unsigned int  dst_ip;
	unsigned char dst_mac[MAC_ADDR_LEN];

	pthread_mutex_t mutex;
	pthread_cond_t  cond;
	int login_result;
	unsigned int		dev_type;
	unsigned int		port;
	unsigned int		enc_cnt;
	unsigned int		hdisk_cnt;
	char				software_version[SOFTWARE_VERSION_LEN];
	char				dsp_software_version[SOFTWARE_VERSION_LEN];
	char				start_time[SOFTWARE_VERSION_LEN];

	unsigned int		reserved;
	unsigned int		gateway;
	int					auth_enable;

	NETCFGCALLBACK fn_net_config;
	PASSED reset_dflt_passwd;
	GETPASSWD get_passwd;
};
static dev_info	g_info;
static dev_info2	g_info2;
static struct SADP g_sadpInst;
char* g_filter = "ether proto 0x8033";
volatile int g_stopflag = 0;


/*init sadp
 *ip: net order
 *mask: net order
 */
void get_dev_info(dev_info * info);
void get_dev_info2(dev_info2 * info);

void* thr_sadp_capture(void*pv);
unsigned int sadp_getseq();
unsigned short sadp_check_sum(unsigned short* data, int len);

void str_to_mac(const char* addr_str, u_char* mac_addr);

BOOL verify_mac_addr(u_char* addr);
BOOL verify_password(const char* pw);
BOOL verify_sadp_packet(char* packet, int len);


int parse_sadp_packet(char* packet, int len);
void proc_op_AQR(struct sadp_header* header);
void proc_op_AIQ(struct sadp_header* header);
void proc_op_UIR(struct sadp_header* header);
void proc_op_passwd(struct sadp_header* header);


void make_sadp_packet(u_int8_t optype,u_int8_t opcode,char * content,int cont_len, unsigned int seq);
void send_sadp_packet(struct SADP* sadp_inst, const char* pDstMacAddr);

int modify_ip_config(struct sadp_header* header);

void update_net_param(void);

static unsigned int sadp_caculate_crc(void *c, int len)
{
	unsigned int crc = 0xFFFFFFFFu;
	int i = 0;
	const unsigned char *p = (const unsigned char *)c;

	if (p == NULL || len <= 0)
	{
		return 0;
	}
	for (i = 0; i < len; i++)
	{
		crc ^= p[i];
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
		crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
	}
	return crc ^ 0xFFFFFFFFu;
}

//-------------------------------------------------------------
//
int init_sadp_lib(unsigned int ip, unsigned int mask, unsigned int gateway, unsigned char* mac,
		char*password,int pw_len, dev_info *info,  NETCFGCALLBACK netconfig,PASSED resetDfltPasswd, GETPASSWD getPasswd,
		int auth_enable)
{
	u_char enet_dst[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
//	u_char enet_dst[6] = {0xb8, 0x97, 0x5a, 0x85, 0x79, 0xda};

	bzero(&g_sadpInst,sizeof(struct SADP));
	
	if ( init_packet_capture(&g_sadpInst.descr) == -1 )
		return -1;

	g_sadpInst.src_ip = ip;
	g_sadpInst.mask = mask;
	g_sadpInst.gateway= gateway;	

	memcpy(g_sadpInst.src_mac,mac,6);
	memcpy(g_sadpInst.dst_mac,enet_dst,6);

	memcpy(g_sadpInst.password,password,pw_len);
	memcpy(g_sadpInst.serialno,info->serial_no,DEVICE_SERIALNO_LEN);
	memcpy(g_sadpInst.dsp_software_version,info->dsp_software_version,SOFTWARE_VERSION_LEN);
	memcpy(g_sadpInst.software_version,info->software_version,SOFTWARE_VERSION_LEN);
	memcpy(g_sadpInst.start_time,info->start_time,SOFTWARE_VERSION_LEN);
	g_sadpInst.dev_type = info->dev_type;
	g_sadpInst.port = info->port;
	g_sadpInst.enc_cnt = info->enc_cnt;
	g_sadpInst.hdisk_cnt = info->hdisk_cnt;
	g_sadpInst.auth_enable = auth_enable ? 1 : 0;

	g_sadpInst.packet = malloc(PACKET_SIZE);
	g_sadpInst.packet_s = PACKET_SIZE;

	g_sadpInst.fn_net_config = netconfig;

	g_sadpInst.reset_dflt_passwd = resetDfltPasswd;
	g_sadpInst.get_passwd = getPasswd;


#if 1
	g_sadpInst.reserved = 0x029c60df;	
#endif	

	pthread_mutex_init(&g_sadpInst.mutex,NULL);

	pthread_cond_init(&g_sadpInst.cond,NULL);

	g_stopflag = FALSE; 

	return 0;
}

int fini_sadp_lib()
{
	fini_packet_capture(&g_sadpInst.descr);

	pthread_mutex_destroy(&g_sadpInst.mutex);
	pthread_cond_destroy(&g_sadpInst.cond);

	return 0;
}

int start_sadp_cap()
{
	pthread_t cap_thread;

	pthread_create(&cap_thread,NULL,thr_sadp_capture,NULL);

	return 0;
}

int stop_sadp_cap()
{
	g_stopflag = TRUE;
	return 0;
}


void* thr_sadp_capture(void*pv)
{
	pthread_detach(pthread_self());
	prctl(PR_SET_NAME, __func__);  
	u_int8_t* pkt_data = NULL;
	struct ether_header* eth_header = NULL;
	int caplen = 0;
	char* sadp_data = NULL;
	char * pbuf	=	NULL;
	pbuf = (char *)memalign(16,MAX_PACKET_LEN);
	if(pbuf==NULL)
	{
		return 0;
	}

	while( !g_stopflag )
	{
		caplen = get_capture_packet(&g_sadpInst.descr,&pkt_data);
		if ( pkt_data == NULL )
			break;

		#ifdef FRANCE_Hymatom
		if ( caplen > (MAX_PACKET_LEN -14) || caplen < (MIN_PACKET_LEN -14) )
			continue;

			sadp_data = (char*)(pkt_data);
			
			// make arm happy. arm request addr alignment.
			memcpy(pbuf, sadp_data,caplen);

			// proc sadp packet
			parse_sadp_packet((char*)pbuf,caplen);
		#else
	                if ( caplen > MAX_PACKET_LEN || caplen < MIN_PACKET_LEN )
			      continue;	

	
			eth_header = (struct ether_header*)pkt_data;
			if ( eth_header->ether_type != ntohs(ETHERTYPE_SADP) )
				continue;

			if ( !verify_mac_addr(eth_header->des_addr) )
				continue;
			
			sadp_data = (char*)(pkt_data + MAC_HEADER_LEN);
			
			// make arm happy. arm request addr alignment.
			memcpy(pbuf, sadp_data,(caplen-MAC_HEADER_LEN));

			// proc sadp packet
			parse_sadp_packet((char*)pbuf,(caplen-MAC_HEADER_LEN));
		#endif
	}

	return 0;
}


unsigned int sadp_getseq()
{
	return g_sadpInst.seq++;
}

int sadp_login()
{
	int i;
	u_int32_t seq;
	u_int8_t code = SADP_ARQ_REQ_UAIP;
	struct timeval  now;
	struct timespec timeout;
	int retcode;

	for ( i = 0; i < 3; i++ ) {
		seq = sadp_getseq();
		get_dev_info(&g_info);
		get_dev_info2(&g_info2);
		make_sadp_packet(SADPOPTYPE_ARQ,code,(char *)&g_info,sizeof(dev_info),seq);
		send_sadp_packet(&g_sadpInst, NULL);

		pthread_mutex_lock(&g_sadpInst.mutex);
		gettimeofday(&now,NULL);
		timeout.tv_sec = now.tv_sec + 3;
		timeout.tv_nsec = now.tv_usec * 1000;
		retcode = pthread_cond_timedwait(&g_sadpInst.cond,&g_sadpInst.mutex,&timeout);
		pthread_mutex_unlock(&g_sadpInst.mutex);

		if ( retcode == ETIMEDOUT ){
			//printf( " wait response timeout.\n");
		}

		__ERR( "login result = %d \n", g_sadpInst.login_result);

		if ( g_sadpInst.login_result == SADP_AQR_ACK_RRF )
			return -1;

		if ( g_sadpInst.login_result == SADP_AQR_ACK_AIP || g_sadpInst.login_result == SADP_AQR_ACK_UAIP )
		       return 0;	
	}

	return -1;
}


unsigned short sadp_check_sum(unsigned short* data, int len)
{
	unsigned long sum = 0;
	while ( len > 1 )
	{
		sum += *data++;
		len -= sizeof(unsigned short);
	}

	if ( len )
		sum += *(unsigned char*)data;

	sum = (sum >> 16) + ( sum & 0xffff );
	sum += sum >> 16;

	return (unsigned short)(~sum);
}


void str_to_mac(const char* addr_str, u_char* mac_addr)
{
	int i,index;
	int value,temp;
	u_char c;
    
	index = 0;
	value = 0;
	temp  = 0;

	for(i=0; i<strlen(addr_str);i++)
	{
		c = addr_str[i];
		if ( (c >='0' && c<='9') || (c>='a' && c<='f') || (c >= 'A' && c <='F') )
		{
			if ( c >= '0' && c <= '9' )
				temp = c-'0';
			if ( c >= 'a' && c <= 'f' )
				temp = c-'a' + 0xa;
			if ( c >= 'A' && c <= 'F' )
				temp = c-'A' + 0xa;

			if ( (index %2) == 1 )
			{
				value = value * 0x10 + temp;
				mac_addr[index/2] = value;
			}
			else
			{
				value = temp;
			}
			index++;
		}

		if ( index == 12 )
			break;
	}	
}

BOOL verify_mac_addr(u_char* addr)
{
	const char* broadcast = "ff:ff:ff:ff:ff:ff";
	u_char bc_mac_addr[MAC_ADDR_LEN];

	str_to_mac(broadcast, bc_mac_addr);

	if ( memcmp(addr,bc_mac_addr,MAC_ADDR_LEN) !=0 && memcmp(addr,g_sadpInst.src_mac,MAC_ADDR_LEN) != 0 )
		return FALSE; 

	return TRUE;
}

BOOL verify_password(const char* pw)
{
	unsigned char pw_our[16];

	if (g_sadpInst.auth_enable == 0)
	{
		return TRUE;
	}

	sadp_MessageDigest((unsigned char *)g_sadpInst.password, (unsigned int)strlen(g_sadpInst.password), pw_our, 1 );
	if ( strncmp((const char *)pw_our, pw, PWD_LEN ) == 0 )
		return TRUE;

	return FALSE;
}

BOOL verify_resetpassword(const char* pw)
{
	if (g_sadpInst.auth_enable == 0)
	{
		return TRUE;
	}

	if ( strncmp( g_sadpInst.password, pw, PWD_LEN ) == 0 )
		return TRUE;

	__ERR("password check failed: %s", pw);
	return FALSE;
}


BOOL verify_sadp_packet(char* packet, int len)
{
	unsigned short check_sum;
	struct sadp_header* header = (struct sadp_header*)packet;

	if ( header->sadp_identifier != SADP_IDENTIFIER )
		return FALSE;

	if ( header->sadp_len != len )
		return FALSE;

	check_sum = ntohs(header->sadp_crc);
	header->sadp_crc = 0;

	if ( check_sum != sadp_check_sum( (unsigned short*)packet,len ) )
		return FALSE;

	return 0;
}

int parse_sadp_packet(char* packet, int len)
{
	struct sadp_header* header;

	if ( verify_sadp_packet(packet,len) == -1 )
		return -1;

	header = (struct sadp_header*)packet;
	
//	__ERR("type %d", header->sadp_optype );
//	deubug_show_data_hex((unsigned char*)(header), sizeof(header)+16, 1);	

	switch ( header->sadp_optype )
	{
		
		case SADPOPTYPE_AQR:
			proc_op_AQR(header);
			break;

		case SADPOPTYPE_AIQ:
			proc_op_AIQ(header);
			break;
				
		case SADPOPTYPE_UIR:
			proc_op_UIR(header);
			break;

		case SADPOPTYPE_DFLTPASSWD_ARQ:
			proc_op_passwd(header);
			break;
			
		default:
			break;
	}


	return 0;
}

void proc_op_AQR(struct sadp_header* header)
{
	switch(header->sadp_opcode)
	{
		case SADP_AQR_ACK_RRF:
			break;

		case SADP_AQR_ACK_AIP:
			modify_ip_config(header);
			break;

		case SADP_AQR_ACK_UAIP:
			break;

		default:
			break;
	}

	g_sadpInst.login_result = header->sadp_opcode;

	pthread_mutex_lock(&g_sadpInst.mutex);
	pthread_cond_broadcast(&g_sadpInst.cond);
	pthread_mutex_unlock(&g_sadpInst.mutex);
}

void proc_op_AIQ(struct sadp_header* header)
{
	unsigned int seq;
	u_int8_t code = 0;

	seq = ntohl(header->sadp_sequence);
	update_net_param();		//QinPan added for wifi 2009.04.13
	get_dev_info(&g_info);
	get_dev_info2(&g_info2);

	u_int8_t *p = header->sadp_src_hwaddr;
	__ERR("seq %u, src %02x:%02x:%02x:%02x:%02x:%02x", seq, p[0],p[1],p[2],p[3],p[4],p[5]);

//	__ERR("dev_info len %d\n", sizeof(dev_info));
	make_sadp_packet(SADPOPTYPE_IQR,code,(char *)&g_info,sizeof(dev_info),seq);
	//make_sadp_packet(SADPOPTYPE_IQR,code,g_sadpInst.serialno,DEVICE_SERIALNO_LEN,seq);

	send_sadp_packet(&g_sadpInst, NULL);
}

void proc_op_UIR(struct sadp_header* header)
{
	__ERR("");
	unsigned int seq;
	int ret = -1;
	u_int8_t code;
	unsigned char password[PWD_LEN];

	seq = ntohl(header->sadp_sequence);

	memcpy(password,header->sadp_data,PWD_LEN);

	if ( verify_password((const char *)password) )
		code = SADP_UIA_ACK_UIS;
	else
		code = SADP_UIA_ACK_UIF;
	
	if ( code == SADP_UIA_ACK_UIS )
	{
		ret = modify_ip_config(header);
		if(ret )
		{
			code = SADP_UIA_ACK_UIF;
		}		
	}
	
	u_int8_t *p = header->sadp_src_hwaddr;
	__ERR("seq %u, src %02x:%02x:%02x:%02x:%02x:%02x", seq, p[0],p[1],p[2],p[3],p[4],p[5]);

	get_dev_info(&g_info);
	get_dev_info2(&g_info2);
	make_sadp_packet(SADPOPTYPE_UIA,code,(char *)&g_info,sizeof(dev_info),seq);

	//make_sadp_packet(SADPOPTYPE_UIA,code,g_sadpInst.serialno,DEVICE_SERIALNO_LEN,seq);

	send_sadp_packet(&g_sadpInst, NULL);

}

void proc_op_passwd(struct sadp_header* header)
{
	__ERR("");
	unsigned int seq;
	u_int8_t code;
	unsigned char password[PWD_LEN];

	seq = ntohl(header->sadp_sequence);


	memcpy(password,header->sadp_data,PWD_LEN);

	if ( verify_resetpassword((const char *)password) )
		code = SADP_DFLTPASSWD_ACK_OK;
	else
		code = SADP_DFLTPASSWD_ACK_FAILURE;
	
	if ( code == SADP_DFLTPASSWD_ACK_OK )
	{
		if(g_sadpInst.reset_dflt_passwd!=NULL)
			code = g_sadpInst.reset_dflt_passwd((char *)password);
		if(code ==0)
		{
			char buffer[64];
			strcpy(buffer, "123456");
			if( g_sadpInst.get_passwd != NULL )
			{
				g_sadpInst.get_passwd(buffer, 64);
			}
		
			memcpy(g_sadpInst.password, buffer, PWD_LEN);
			code =SADP_DFLTPASSWD_ACK_OK;
		}
		else
			code =SADP_DFLTPASSWD_ACK_FAILURE;
	}	 
	
	u_int8_t *p = header->sadp_src_hwaddr;
	__ERR("seq %u, src %02x:%02x:%02x:%02x:%02x:%02x", seq, p[0],p[1],p[2],p[3],p[4],p[5]);

	get_dev_info(&g_info);
	get_dev_info2(&g_info2);
	make_sadp_packet(SADPOPTYPE_DFLTPASSWD_ACK,code,(char *)&g_info,sizeof(dev_info),seq);

	//make_sadp_packet(SADPOPTYPE_UIA,code,g_sadpInst.serialno,DEVICE_SERIALNO_LEN,seq);

	send_sadp_packet(&g_sadpInst, NULL);

}

void make_sadp_packet(u_int8_t optype,u_int8_t opcode,char * content,int cont_len, unsigned int seq)
{
	struct sadp_header* header;
	u_int8_t* packet = (u_int8_t*)g_sadpInst.packet;
	u_int16_t total_len;
	unsigned int temp;

	total_len = SADP_HEADER_LEN + cont_len;

	__ERR("cont_len %d. src %#x %02X:%02X:%02X:%02X:%02X:%02X, dst %#x %02X:%02X:%02X:%02X:%02X:%02X", 
		cont_len,
		g_sadpInst.src_ip,
		g_sadpInst.src_mac[0], g_sadpInst.src_mac[1], g_sadpInst.src_mac[2], 
		g_sadpInst.src_mac[3], g_sadpInst.src_mac[4], g_sadpInst.src_mac[5], 
		g_sadpInst.dst_ip,
		g_sadpInst.dst_mac[0], g_sadpInst.dst_mac[1], g_sadpInst.dst_mac[2], 
		g_sadpInst.dst_mac[3], g_sadpInst.dst_mac[4], g_sadpInst.dst_mac[5]
		);

	if ( total_len < MIN_PACKET )
		total_len = MIN_PACKET;

	bzero( packet,total_len );

	header = (struct sadp_header*)packet;

	header->sadp_identifier = SADP_IDENTIFIER;
	header->sadp_version = SADP_VERSION;
	header->sadp_series  = SADP_SERIES;
	header->sadp_len = total_len;
	header->sadp_sequence = htonl(seq);
	header->sadp_hwaddrlen = SADP_HW_ADDR_LEN;
	header->sadp_praddrlen = SADP_PR_ADDR_LEN;
	header->sadp_optype = optype;
	header->sadp_opcode = opcode;
	header->sadp_crc = 0;
	header->sadp_src_praddr = htonl(g_sadpInst.src_ip);
	memcpy(header->sadp_src_hwaddr,g_sadpInst.src_mac, 6);
	memcpy(header->sadp_dst_hwaddr,g_sadpInst.dst_mac, 6);
	temp = htonl(g_sadpInst.dst_ip);
	memcpy(header->sadp_dst_praddr,&temp, 4);
	temp = htonl(g_sadpInst.mask);
	memcpy(header->sadp_subnet_mask,&temp,4);

	memcpy(header->sadp_data,content, cont_len);

	header->sadp_crc = htons( sadp_check_sum((unsigned short*)packet,total_len) );

	g_sadpInst.packet_s = total_len;
}

void send_sadp_packet(struct SADP* sadp_inst, const char* pDstMacAddr)
{
	u_int8_t buf[PACKET_SIZE];
	u_int8_t* temp = buf;
#ifdef FRANCE_Hymatom
	u_int32_t len =  sadp_inst->packet_s;
#else
	u_int32_t len =  sadp_inst->packet_s + MAC_HEADER_LEN;
	u_int16_t type = htons(ETHERTYPE_SADP);

	if( pDstMacAddr == NULL)
		memcpy(temp,sadp_inst->dst_mac,MAC_ADDR_LEN);
	else
		memcpy(temp,pDstMacAddr,MAC_ADDR_LEN);
	
	u_int8_t *p = temp;
	__ERR("dst %02x:%02x:%02x:%02x:%02x:%02x", p[0],p[1],p[2],p[3],p[4],p[5]);
	
	temp += MAC_ADDR_LEN;

	memcpy(temp,sadp_inst->src_mac,MAC_ADDR_LEN);
	temp += MAC_ADDR_LEN;

	memcpy(temp,&type,sizeof(unsigned short));
	temp += sizeof(unsigned short);
#endif
	memcpy(temp, sadp_inst->packet, sadp_inst->packet_s);
#if 1
	memcpy(temp+sadp_inst->packet_s, &g_info2, sizeof(g_info2));

//	deubug_show_data_hex((unsigned char*)(buf), len, 0);	

	send_ether_packet(&sadp_inst->descr, buf,len+sizeof(g_info2) );
#else
	send_ether_packet(&sadp_inst->descr, buf,len);
#endif
}


int modify_ip_config(struct sadp_header* header)
{
	unsigned int  mask;
	unsigned int  ip;
	unsigned int port;
	int ret = -1;
	memcpy(&ip, header->sadp_dst_praddr,4);
	memcpy(&mask, header->sadp_subnet_mask,4);		
	memcpy(&port, header->sadp_data+PWD_LEN,4);

	int bDhcp = 0;

	#if 0
	if( 0x3A == *(header->sadp_data+ 31) )
		bDhcp = 1;
	if( 0x3B == *(header->sadp_data+ 31) )
		bDhcp = 0;
	#endif
	
	
	ip = 	ntohl(ip);
	mask = ntohl(mask);
	port = ntohl(port);
	
	__ERR("port =%x mask  =%x ip =%x, sadp_src_praddr=%x\n",port,mask,ip, header->sadp_src_praddr);
	if(g_sadpInst.fn_net_config)
	{
		ret = g_sadpInst.fn_net_config(ip ,mask ,0,port, bDhcp);
		if(ret == 0)
		{
		
			g_sadpInst.src_ip = ip;
			g_sadpInst.mask = mask;
			g_sadpInst.port = port;
		}
	}

	

	return ret;
}

void get_dev_info(dev_info * info){
	
	memcpy(info->serial_no,g_sadpInst.serialno,DEVICE_SERIALNO_LEN);
	memcpy(info->dsp_software_version,g_sadpInst.dsp_software_version,SOFTWARE_VERSION_LEN);
	memcpy(info->software_version,g_sadpInst.software_version,SOFTWARE_VERSION_LEN);

#if 1
	struct tm *ptm;
	unsigned int nRuntime = GetCurrentTimeStamp()/1000;
	time_t nNowTime = time(NULL);
	if( nNowTime > nRuntime )
		nNowTime -= nRuntime;
	ptm = localtime(&nNowTime);
	sprintf(info->start_time, "%04d-%02d-%02d %02d:%02d:%02d",
			(1900 + ptm->tm_year),
			(1 + ptm->tm_mon), 
			ptm->tm_mday,			
			ptm->tm_hour, 
			ptm->tm_min, 
			ptm->tm_sec);
#else	
	memcpy(info->start_time,g_sadpInst.start_time,SOFTWARE_VERSION_LEN);
#endif

	info->dev_type = htonl(g_sadpInst.dev_type);
	info->port = htonl(g_sadpInst.port);
	info->enc_cnt = htonl(g_sadpInst.enc_cnt);
	info->hdisk_cnt = htonl(g_sadpInst.hdisk_cnt); 
	return;
}

typedef struct 
{
	unsigned char nNetwork1;
	unsigned char nNetwork2;
	unsigned char magic2;
	unsigned char magic3;	
}HikSaspCheckSum;


#if 0
static HikSaspCheckSum g_checksumdata[]=
{//抓包发现其他字段不变的情况下，网关192.168.0.1 - 192.168.254.1 校验码第二位递减，递减小于0后第一位递减
	{168, 0x00, 0x72, 0xe9,},
	{168, 0x01, 0x72, 0xe8,},
	{168, 0x02, 0x72, 0xe7,},
	{168, 0x03, 0x72, 0xe6,},
	{168, 0x13, 0x72, 0xd6,},
	{168, 0xFA, 0x71, 0xef,},//250
	{168, 0xFB, 0x71, 0xee,},
	{168, 0xFC, 0x71, 0xed,},
	{168, 0xFD, 0x71, 0xec,},
	{168, 0xFE, 0x71, 0xeb,},//254
};
#endif

void get_dev_info2(dev_info2 * info){

	char customname[64] = {0};

	info->magic[0] = 0x02; 	
	info->magic[1] = 0x9c; 	
	info->magic[2] = 0x72; 	
	info->magic[3] = 0xe9;//192.168.0.1的校验码 		
	info->gateway= htonl(g_sadpInst.gateway); 	
	
	info->reservered_data[33] = 0x04;
	info->reservered_data[33+32] = 0x50;
	strcpy(info->model2,"VISIONFOCUS");

#if 0
	char szDeviceType[32];
	memset(szDeviceType, 0, 32);
	GetDeviceTypeStr(szDeviceType);	
	memcpy(info->model1,szDeviceType,AJ_MODEL_LEN);
	
	unsigned short sadp_crc = htons( sadp_check_sum(((unsigned char*)info)+4,sizeof(dev_info2)-4) );
	info->magic[2] = sadp_crc & 0xff; 	
	info->magic[3] = (sadp_crc>>8) & 0xff; 		
#else	

	read_file_to_string("/etc/flag.customize", customname, sizeof(customname));

	if(strstr(customname, "GENIV"))
	{
		strcpy(info->model1, "Onvif");
	}
	else if (strstr(customname, "_HX"))
	{
		strcpy(info->model1, "I5PT-390IPX10");
	}
	else
	{
		strcpy(info->model1, "HK-IPCAM-HI");
	}

	unsigned int data = g_sadpInst.gateway;
	unsigned char network0 = (data >> 24 ) & 0xff;
	unsigned char network1 = (data >> 16 ) & 0xff;
	unsigned char network2 = (data >> 8 ) & 0xff;
	unsigned char network3 = (data >> 0 ) & 0xff;
	if( data == 0 )
	{
		info->magic[2] = 0x1c;	
		info->magic[3] = 0xaa;	
	}
	else if( network0 == 192 && network1 == 168)
	{
		if (network3 == 1)//192.168.*.1的情况
		{
			if( network2 > 0xe9)
			{
				info->magic[2] = 0x71;	
				info->magic[3] = 0xff - (network2 - 0xe9 -1);	
			}
			else
			{
				info->magic[3] = 0xe9 - network2;	
			}
		}
		else //192.168.*.*的情况
		{
#if 0		
			unsigned char check1 = info->magic[2];
			unsigned char check2 = info->magic[3];
			
			//先得到192.168.*.1的校验码
			if( network2 > 0xe9)
			{
				check1 = 0x71;	
				check2 = 0xff - (network2 - 0xe9 -1);	
			}
			else
			{
				check2 = 0xe9 - network2;	
			}
#endif			

			//再计算192.168.*.*的校验码
		}
	}

#endif

//	printf("gateway=%#x\n", info->gateway);

	unsigned int crc = sadp_caculate_crc(&info->gateway,sizeof(dev_info2)-4);
	__ERR("gateway=%#x, crc=%#x\n", info->gateway, crc);

	
	return;
}

void update_net_param(void)
{
	struct NET_CONFIG netcfg;
	char ipAddr[64] = {0};
	char netMask[64] = {0};
	char szGateway[64] = {0};
	unsigned int ipaddr = 0;
	unsigned int netmask = 0;
	unsigned int gateway = 0;

	memset(&netcfg, 0, sizeof(netcfg));
	if (net_get_info(g_sadpInst.descr.device, &netcfg) == 0)
	{
		get_ip_str(netcfg.ifaddr, ipAddr, sizeof(ipAddr));
		get_ip_str(netcfg.netmask, netMask, sizeof(netMask));
		get_ip_str(netcfg.gateway, szGateway, sizeof(szGateway));
	}

	__ERR("ipAddr = %s, netMask = %s, gateway = %s\n", ipAddr, netMask, szGateway);
	ipaddr  = htonl(inet_addr(ipAddr));
	netmask = htonl(inet_addr(netMask));
	gateway = htonl(inet_addr(szGateway));

	g_sadpInst.src_ip = ipaddr;
	g_sadpInst.mask = netmask;
	g_sadpInst.gateway = gateway;

	if (ipaddr == 0xffffffff && netmask == 0xffffffff)
	{
		g_sadpInst.src_ip = 0;
		g_sadpInst.mask = htonl(inet_addr("255.255.255.0"));
		g_sadpInst.gateway = 0;
	}
}




