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
#include "onvif_util.h"
extern ONVIF_CFG g_onvif_cfg;
extern WIFIConfig   g_wifiCfg;
extern WIFIApConfig g_wifiapCfg;
extern int g_ExistWifi;
extern int g_product_type;
void onvif_get_time_str(char * buff, int len, int sec_off)
{
	time_t nowtime;
	struct tm *gtime;	

	time(&nowtime);
	nowtime += sec_off;
	gtime = gmtime(&nowtime);

	snprintf(buff, len, "%04d-%02d-%02dT%02d:%02d:%02dZ", 		 
		gtime->tm_year+1900, gtime->tm_mon+1, gtime->tm_mday,
		gtime->tm_hour, gtime->tm_min, gtime->tm_sec);		
}

BOOL onvif_is_valid_hostname(const char * name)
{
	// 0-9, a-z, A-Z, '-'

	const char * p = name;
	while (*p != '\0')
	{
		if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p == '-') || (*p == '.'))
		{
			++p;
			continue;
		}
		else
		{
			return FALSE;
		}
	}

	return TRUE;
}

void onvif_get_timezone(char * tz, int len)
{
	int local_hour, utc_hour;
	time_t time_utc;
	struct tm *tm_local;
	struct tm *tm_utc;
		
	// Get the UTC time
	time(&time_utc);

	// Get the local time
	tm_local = localtime(&time_utc);
	local_hour = tm_local->tm_hour;

	// Change it to GMT tm
	tm_utc = gmtime(&time_utc);
	utc_hour = tm_utc->tm_hour;

	int time_zone = local_hour - utc_hour;
	if (time_zone < -12)
	{
		time_zone += 24;
	}
	else if (time_zone > 12)
	{
		time_zone -= 24;
	}

	snprintf(tz, len, "PST%dPDT", time_zone);    
}

// stdoffset[dst[offset],[start[/time],end[/time]]]
BOOL onvif_is_valid_timezone(const char * tz)
{
	// not imcomplete

	const char * p = tz;
	while (*p != '\0')
	{
		if (*p >= '0' && *p <= '9')
		{
			return TRUE;
		}

		++p;
	}

	return FALSE;
}

const char * onvif_uuid_create()
{
	static char uuid[100];

	srand(time(NULL));
	sprintf(uuid, "%04x%04x-%04x-%04x-%04x-%04x%04x%04x", rand(), rand(), rand(), rand(), rand(), rand(), rand(), rand());

	return uuid;
}

const char * onvif_get_local_ip()
{
	struct in_addr addr;
	addr.s_addr = get_default_if_ip();

	if (addr.s_addr != 0)
	{
		return inet_ntoa(addr);
	}
	else
	{
		int nums = get_if_nums();
		for (int i = 0; i < nums; i++)
		{
			addr.s_addr = get_if_ip(i);
			if (addr.s_addr != 0)
			{
				return inet_ntoa(addr);
			}
		}
	}

	return 0;
}

void get_my_macaddr(char macaddr[MACH_ADDR_LENGTH])
{
	static char __my__mac[MACH_ADDR_LENGTH] = {0,0,0,0,0,0};
	if(memcmp(__my__mac, "\x0\x0\x0\x0\x0\x0", 6) != 0)
	{
		memcpy(macaddr, __my__mac, MACH_ADDR_LENGTH);
		return;
	}

	char ifname[256] = {0};
	get_my_ifname(ifname);
	net_get_hwaddr(ifname, (unsigned char *)macaddr);
	memcpy(__my__mac, macaddr, MACH_ADDR_LENGTH);
}

in_addr_t get_netmask_by_prefix_len(unsigned int prefix_len)
{
	unsigned int netmask = 0XFFFFFFFF;

	netmask = netmask << (32 - prefix_len);
	return htonl(netmask);
}


in_addr_t get_gateway_by_prefix_len(unsigned int IP, unsigned int prefix_len)
{
	unsigned int netmask = 0XFFFFFFFF;
	netmask = netmask << (32 - prefix_len);

	IP = (IP & netmask ) + 1;
	
	return htonl(IP);
}


int netsplit( char *pAddress, void *ip )
{
	unsigned int ret;
	NET_IPV4 *ipaddr = (NET_IPV4 *)ip;

	if ((ret = atoi(pAddress + 9)) > 255)
		return FALSE;
	ipaddr->str[3] = ret;

	*( pAddress + 9 ) = '\x0';
	if ((ret = atoi(pAddress + 6)) > 255)
		return FALSE;
	ipaddr->str[2] = ret;

	*( pAddress + 6 ) = '\x0';
	if ((ret = atoi(pAddress + 3)) > 255)
		return FALSE;
	ipaddr->str[1] = ret;

	*( pAddress + 3 ) = '\x0';
	if ((ret = atoi(pAddress + 0)) > 255)
		return FALSE;
	ipaddr->str[0] = ret;

	return TRUE;
}

int ipv4_str_to_num(char *data, struct in_addr *ipaddr)
{
	if ( strchr(data, '.') == NULL )
		return netsplit(data, ipaddr);
	return inet_aton(data, ipaddr);
}

void OnvifGetVideoSize(char *resName, int tvsystem, int *width, int *height)
{
	if(tvsystem != 0)
		tvsystem = 1;
	else 
		tvsystem = 0;

	GetVideoSize(resName, tvsystem, width, height);

	if(g_product_type == PRODUCT_TYPE_AC400L && strcmp(resName, "5MP") == 0 )
	{
		*width = 2592;
		*height = 1944;
	}
	
	if((g_product_type == PRODUCT_TYPE_MCL15 
		|| g_product_type == PRODUCT_TYPE_MCF46
		|| g_product_type == PRODUCT_TYPE_MCL16
		)
		&& strcmp(resName, "5MP") == 0 )
	{
		*width = 2592;
		*height = 1944;
	}
	
	if((g_product_type == PRODUCT_TYPE_MC400L2 
		|| g_product_type == PRODUCT_TYPE_MS400L2 
		|| g_product_type == PRODUCT_TYPE_MHC31J4 )
		&& strcmp(resName, "5MP") == 0 )
	{
		*width = 2592;
		*height = 1944;
	}
	//以下为兼容海康NVR无法识别分辨率、不显示的问题
	else if( strcmp(resName, "2592X1520") == 0 )
	{
		*width = 2688;
		*height = 1520;
	}
	else if( strcmp(resName, "2592X1512") == 0 )
	{
		*width = 2688;
		*height = 1520;
	}
	else if( strcmp(resName, "2048X1520") == 0 )
	{
		*width = 2048;
		*height = 1536;
	}
}

RESOLUTION_ENTRY *  GetResolution(char *encode,int width, int height, int tvsystem, int streamno)
{
	RESOLUTION_ENTRY *pEntry = NULL;
	RESOLUTION_ENTRY *pEntryArray = NULL;
	int nrescount = GetVideoResArray(&pEntryArray);
	if( nrescount == 0 || pEntryArray == NULL )
	{
		return pEntry;
	}


	int iIndex = 0;
	for( iIndex = 0; iIndex < nrescount; iIndex++)
	{
		int w, h;

		OnvifGetVideoSize(pEntryArray[iIndex].res_name, tvsystem, &w, &h);
		if(!strcmp(encode,pEntryArray[iIndex].codec_name)){
			if( width == w && height == h && pEntryArray[iIndex].stream_type == streamno )
			{
				pEntry = &pEntryArray[iIndex];
				return pEntry;
			}
		}
	}

	return NULL;
}

int get_rtsp_port(void)
{
	int video_port = g_onvif_cfg.network.rtsp_port[0];
	if(video_port <= 0  || video_port >= 65535)
	{
		__ERR("gStreamCfg.rtspConfig.videoPort invalid = %d!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",
				video_port);

		short port;
		FILE *video_port_fp = fopen("/tmp/rtsp_port","rb");
		if(video_port_fp)
		{
			fread(&port, 1, sizeof(short), video_port_fp);
			fclose(video_port_fp);

			__ERR("read video port from /tmp/rtsp_port OK, port = %d\n", port);
			return port;
		}
		else
		{
			__ERR("read video port from /tmp/rtsp_port failed, use default port = %d\n", 554);
			return 554;
		}
	}
	else
		return video_port;
}


