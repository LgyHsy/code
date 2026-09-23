/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2010-2014, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include "sys_inc.h"
#include "word_analyse.h"
#include "util.h"

/***************************************************************************************/
int get_if_nums()
{
#if __WIN32_OS__

	char ipt_buf[512];
	MIB_IPADDRTABLE *ipt = (MIB_IPADDRTABLE *)ipt_buf;
	ULONG ipt_len = sizeof(ipt_buf);
	DWORD fr = GetIpAddrTable(ipt,&ipt_len,FALSE);
	if (fr != NO_ERROR)
		return 0;
		
	return ipt->dwNumEntries;
	
#elif __LINUX_OS__

	int socket_fd;
	struct ifconf conf;
	char buff[BUFSIZ];
	int num;
	
	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socket_fd <= 0)
	{
		return 0;
	}
	
	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buff;
	
	ioctl(socket_fd, SIOCGIFCONF, &conf);
	
	num = conf.ifc_len / sizeof(struct ifreq);

	closesocket(socket_fd);
	
	return num;
	
#endif

	return 0;
}

unsigned int get_if_ip(int index)
{
#if __WIN32_OS__

	char ipt_buf[512];
	MIB_IPADDRTABLE *ipt = (MIB_IPADDRTABLE *)ipt_buf;
	ULONG ipt_len = sizeof(ipt_buf);
	DWORD fr = GetIpAddrTable(ipt,&ipt_len,FALSE);
	if (fr != NO_ERROR)
		return 0;

	for (DWORD i=0; i<ipt->dwNumEntries; i++)
	{
		if (i == index)
			return ipt->table[i].dwAddr;
	}
	
#elif __LINUX_OS__

	int socket_fd;
	struct ifreq *ifr;
	struct ifconf conf;
	char buff[BUFSIZ];
	int num;

	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buff;

	ioctl(socket_fd, SIOCGIFCONF, &conf);

	num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;

	unsigned int ip_addr = 0;

	for (int i=0; i<num; i++)
	{
		if (i == index)
		{
			struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);

			ioctl(socket_fd, SIOCGIFFLAGS, ifr);
			if ((ifr->ifr_flags & IFF_LOOPBACK) != 0)
			{
				ip_addr = 0;
			}
			else
			{
				ip_addr = sin->sin_addr.s_addr;
			}

			break;
		}
		
		ifr++;
	}

	closesocket(socket_fd);
	
	return ip_addr;
	
#endif

	return 0;
}

unsigned int get_route_if_ip(unsigned int dst_ip)
{
#if __WIN32_OS__

	DWORD dwIfIndex,fr;

	fr = GetBestInterface(dst_ip,&dwIfIndex);
	if (fr != NO_ERROR)
		return 0;

	char ipt_buf[512];
	MIB_IPADDRTABLE *ipt = (MIB_IPADDRTABLE *)ipt_buf;
	ULONG ipt_len = sizeof(ipt_buf);
	fr = GetIpAddrTable(ipt,&ipt_len,FALSE);
	if (fr != NO_ERROR)
		return 0;

	for (DWORD i=0; i<ipt->dwNumEntries; i++)
	{
		if (ipt->table[i].dwIndex == dwIfIndex)
			return ipt->table[i].dwAddr;
	}

#elif __VXWORKS_OS__

	char tmp_buf[24];
	char ifname[32];
	sprintf(ifname,"%s%d",hsip.ifname,0);
	STATUS ret = ifAddrGet(ifname,tmp_buf);
	if (ret == OK)
	{
		return inet_addr(tmp_buf);
	}
	
#elif __LINUX_OS__

	int socket_fd;
	struct ifreq *ifr;
	struct ifconf conf;
	char buff[BUFSIZ];
	int num;

	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buff;

	ioctl(socket_fd, SIOCGIFCONF, &conf);

	num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;

	for (int i=0; i<num; i++)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);

		ioctl(socket_fd, SIOCGIFFLAGS, ifr);
		if (((ifr->ifr_flags & IFF_LOOPBACK) == 0) && (ifr->ifr_flags & IFF_UP))
		{
			return sin->sin_addr.s_addr;
		}

		ifr++;
	}

#endif

	return 0;
}

unsigned int get_default_if_ip()
{
	return get_route_if_ip(0);
}

int get_default_if_mac(unsigned char * mac)
{
#if __WIN32_OS__

	IP_ADAPTER_INFO AdapterInfo[16];            // Allocate information for up to 16 NICs  
    DWORD dwBufLen = sizeof(AdapterInfo);       // Save the memory size of buffer  
  
    DWORD dwStatus = GetAdaptersInfo(           // Call GetAdapterInfo  
        AdapterInfo,                            // [out] buffer to receive data  
        &dwBufLen);                             // [in] size of receive data buffer  
  
    PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;// Contains pointer to current adapter info  
	if (pAdapterInfo)
	{
        memcpy(mac, pAdapterInfo->Address, 6);	
        return 0;
    }  
    
#elif __LINUX_OS__
	
	int socket_fd;
	struct ifreq *ifr;
	struct ifreq ifreq;
	struct ifconf conf;
	char buff[BUFSIZ];
	int num;

	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buff;

	ioctl(socket_fd, SIOCGIFCONF, &conf);

	num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;

	for (int i=0; i<num; i++)
	{
		if (ifr->ifr_addr.sa_family != AF_INET)
		{
			ifr++;
			continue;
		}
		
		ioctl(socket_fd, SIOCGIFFLAGS, ifr);
		if ((ifr->ifr_flags & IFF_LOOPBACK) != 0)
		{
			ifr++;
			continue;
		}

		strncpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
        if (ioctl(socket_fd, SIOCGIFHWADDR, &ifreq) < 0) 
        {
        	ifr++;
            continue;
        }

		memcpy(mac, &ifreq.ifr_hwaddr.sa_data, 6);		

		close(socket_fd);
		return 0;
	}

	close(socket_fd);
#endif	

	return -1;
}

unsigned int get_address_by_name(char *host_name)
{
	unsigned int addr = 0;

	if (is_ip_address(host_name))
		addr = inet_addr(host_name);
	else
	{
		struct hostent * remoteHost = gethostbyname(host_name);
		if (remoteHost)
			addr = *(unsigned long *)(remoteHost->h_addr);
	}

	return addr;
}

char * lowercase(char *str) 
{
	unsigned int i;
	
	for (i = 0; i < strlen(str); ++i)
		str[i] = tolower(str[i]);
	
	return str;
}

char * uppercase(char *str)
{
	unsigned int i;
	
	for (i = 0; i < strlen(str); ++i)
		str[i] = toupper(str[i]);
	
	return str;
}

#define MIN(a, b)  ((a) < (b) ? (a) : (b))

int unicode(char **dst, char *src) 
{
	char *ret;
	int l, i;
	
	if (!src) 
	{
		*dst = NULL;
		return 0;
	}
	
	l = MIN(64, strlen(src));
	ret = (char *)XMALLOC(2*l);
	for (i = 0; i < l; ++i)
	{
		ret[2*i] = src[i];
		ret[2*i+1] = '\0';
	}

	*dst = ret;
	return 2*l;
}

static char hextab[17] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 0};
static int hexindex[128] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

char * printmem(char *src, size_t len, int bitwidth) 
{
	char *tmp;
	unsigned int i;
	
	tmp = (char *)XMALLOC(2*len+1);
	for (i = 0; i < len; ++i) 
	{
		tmp[i*2] = hextab[((unsigned char)src[i] ^ (unsigned char)(7-bitwidth)) >> 4];
		tmp[i*2+1] = hextab[(src[i] ^ (unsigned char)(7-bitwidth)) & 0x0F];
	}
	
	return tmp;
}

char * scanmem(char *src, int bitwidth) 
{
	int h, l, i, bytes;
	char *tmp;
	
	if (strlen(src) % 2)
		return NULL;
	
	bytes = strlen(src)/2;
	tmp = (char *)XMALLOC(bytes+1);
	for (i = 0; i < bytes; ++i) 
	{
		h = hexindex[(int)src[i*2]];
		l = hexindex[(int)src[i*2+1]];
		if (h < 0 || l < 0) 
		{
		        XFREE(tmp);
		        return NULL;
		}
		tmp[i] = ((h << 4) + l) ^ (unsigned char)(7-bitwidth);
	}
	tmp[i] = 0;

	return tmp;
}


time_t get_time_by_string(char * p_time_str)
{
	struct tm st;
	memset(&st, 0, sizeof(struct tm));

	char * ptr_s = p_time_str;
	while(*ptr_s == ' ' || *ptr_s == '\t') ptr_s++;

	sscanf(ptr_s, "%04d-%02d-%02d %02d:%02d:%02d", &st.tm_year, &st.tm_mon, &st.tm_mday, &st.tm_hour, &st.tm_min, &st.tm_sec);

	st.tm_year -= 1900;
	st.tm_mon -= 1;

	time_t t = mktime(&st);
	return t;
}

int tcp_connect_timeout(unsigned int rip, int port, int timeout)
{
#if __WIN32_OS__
	int flag = 0;
#endif
	int cfd = socket(AF_INET, SOCK_STREAM, 0);
	if (cfd < 0)
	{
	    printf("tcp_connect_timeout socket failed\n");
		return -1;
    }
    
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = rip;
	addr.sin_port = htons((unsigned short)port);

#if __LINUX_OS__

	struct timeval tv;
	tv.tv_sec = timeout/1000;
	tv.tv_usec = (timeout%1000) * 1000;
	
	setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
	if (connect(cfd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
	{
	    return cfd;
	}
	else
	{
	    printf("connect:: %s\n", strerror(errno));
	    closesocket(cfd);
	    return -1;
	}
	
#elif __WIN32_OS__
    
    unsigned long ul = 1;
	ioctlsocket(cfd, FIONBIO, &ul);

	if (connect(cfd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		struct timeval tv;
		tv.tv_sec = timeout/1000;
		tv.tv_usec = (timeout%1000) * 1000;

		fd_set set;
		FD_ZERO(&set);
		FD_SET(cfd, &set);

		if (select(cfd+1, NULL, &set, NULL, &tv) > 0)
		{
			int err=0, len=sizeof(int);
			getsockopt(cfd, SOL_SOCKET, SO_ERROR, (char *)&err, (socklen_t*) &len);
			if (err == 0)
				flag = 1;
		}
	}
	else
	{
	    flag = 1;
	}

    ul = 0;
	ioctlsocket(cfd, FIONBIO, &ul);
		
	if (flag == 1)
	{
		return cfd;
	}
	else
	{
		closesocket(cfd);
		return -1;
	}

#endif	
}



