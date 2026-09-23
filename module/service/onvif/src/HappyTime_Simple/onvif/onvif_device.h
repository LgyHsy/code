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

#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "sys_inc.h"
#include "onvif.h"

/***************************************************************************************/
typedef struct 
{
	bool fromdhcp;
	char dns_searchdomain[MAX_SEARCHDOMAIN][64];
	char dns_server[MAX_DNS_SERVER][32];
} SetDNS_REQ;

typedef struct 
{
	bool fromdhcp;
	char ntp_server[MAX_NTP_SERVER][32];
} SetNTP_REQ;

typedef struct
{
	bool http_flag;
	bool http_enable;
	bool https_flag;
	bool https_enable;
	bool rtsp_flag;
	bool rtsp_enable;

	int  http_port[MAX_SERVER_PORT];
	int  https_port[MAX_SERVER_PORT];
	int  rtsp_port[MAX_SERVER_PORT];
} SetNetworkProtocols_REQ;

typedef struct
{
	char gateway[MAX_GATEWAY][32];
} SetNetworkDefaultGateway_REQ;

typedef struct
{
    int    type;   /* 0 : Manual, 1 : NTP*/
    
    char   TZ[32];

    /*UTC Time*/
    short  year;
    short  month;
    short  day;
    short  hour;
    short  minute;
    short  second;

    bool   DaylightSavings;
} SetSystemDateAndTime_REQ;

typedef struct
{
	char    token[ONVIF_TOKEN_LEN];
	BOOL    enabled;
	char    hwaddr[32];
	int	    mtu;
	BOOL    ipv4_enabled;
	BOOL    fromdhcp;
	char    ipv4_addr[32];
	int	    prefix_len;
} SetNetworkInterfaces_REQ;

typedef struct
{
	BOOL 	discoverable;
} SetDiscoveryMode_REQ;


#ifdef __cplusplus
extern "C" {
#endif

const char * onvif_GetAuthPass(const char * user);

ONVIF_RET onvif_SetSystemDateAndTime(SetSystemDateAndTime_REQ * p_SetSystemDateAndTime_req);
ONVIF_RET onvif_SetHostname(const char * name, BOOL fromdhcp);
ONVIF_RET onvif_SetDNS(SetDNS_REQ * p_SetDNS_req);
ONVIF_RET onvif_SetNTP(SetNTP_REQ * p_SetNTP_req);
ONVIF_RET onvif_SetNetworkProtocols(SetNetworkProtocols_REQ * p_SetNetworkProtocols_req);
ONVIF_RET onvif_SetNetworkDefaultGateway(SetNetworkDefaultGateway_REQ * p_SetNetworkDefaultGateway_req);
ONVIF_RET onvif_SystemReboot();
ONVIF_RET onvif_SetSystemFactoryDefault(int type /* 0:soft, 1:hard */);
ONVIF_RET onvif_SetNetworkInterfaces(SetNetworkInterfaces_REQ * p_SetNetworkInterfaces_req);
ONVIF_RET onvif_SetDiscoveryMode(SetDiscoveryMode_REQ * p_SetDiscoveryMode_req);


#ifdef __cplusplus
}
#endif

#endif // _DEVICE_H_

