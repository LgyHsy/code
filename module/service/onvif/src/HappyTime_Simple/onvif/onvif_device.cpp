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

#include "onvif_device.h"
#include "sys_inc.h"
#include "onvif.h"
#include "xml_node.h"
#include "onvif_util.h"


/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;


/***************************************************************************************/

const char * onvif_GetAuthPass(const char * user)
{
	if (strcmp(user, g_onvif_cfg.auth_user) == 0)
		return g_onvif_cfg.auth_pass;

	return NULL;	
}

ONVIF_RET onvif_SetSystemDateAndTime(SetSystemDateAndTime_REQ * p_SetSystemDateAndTime_req)
{
	// check datetime
	if (p_SetSystemDateAndTime_req->type == 0)
	{
		if (p_SetSystemDateAndTime_req->month < 1 || p_SetSystemDateAndTime_req->month > 12 ||
			p_SetSystemDateAndTime_req->day < 1 || p_SetSystemDateAndTime_req->day > 31 ||
			p_SetSystemDateAndTime_req->hour < 0 || p_SetSystemDateAndTime_req->hour > 23 ||
			p_SetSystemDateAndTime_req->minute < 0 || p_SetSystemDateAndTime_req->minute > 59 ||
			p_SetSystemDateAndTime_req->second < 0 || p_SetSystemDateAndTime_req->second > 61)
		{
			return ONVIF_ERR_INVALID_DATETIME;
		}
	}

	// check timezone
	if (p_SetSystemDateAndTime_req->TZ[0] != '\0' && 
		onvif_is_valid_timezone(p_SetSystemDateAndTime_req->TZ) == FALSE)
	{
		return ONVIF_ERR_INVALID_TIMEZONE;
	}

	// set system date time ...

	g_onvif_cfg.datetime_ntp = p_SetSystemDateAndTime_req->type;
	g_onvif_cfg.daylightsavings = p_SetSystemDateAndTime_req->DaylightSavings;
	if (p_SetSystemDateAndTime_req->TZ[0] != '\0')
	{
		strcpy(g_onvif_cfg.timezone, p_SetSystemDateAndTime_req->TZ);
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_SystemReboot()
{
	// reboot system ...

	return ONVIF_OK;
}

ONVIF_RET onvif_SetSystemFactoryDefault(int type /* 0:soft, 1:hard */)
{
	// set system factory default
	
	return ONVIF_OK;
}

ONVIF_RET onvif_SetHostname(const char * name, BOOL fromdhcp)
{
    if (fromdhcp)
    {
    }
    else if (onvif_is_valid_hostname(name) == FALSE)
    {
    	return ONVIF_ERR_INVALID_HOSTNAME;
    }
    
    // set hostname ...

	if (name)
	{
    	strncpy(g_onvif_cfg.network.hostname, name, sizeof(g_onvif_cfg.network.hostname)-1);
    }
    g_onvif_cfg.network.hostname_fromdhcp = fromdhcp;

    return ONVIF_OK;
}

ONVIF_RET onvif_SetDNS(SetDNS_REQ * p_SetDNS_req)
{
	// set DNS ...

	g_onvif_cfg.network.dns_fromdhcp = p_SetDNS_req->fromdhcp;

	if (g_onvif_cfg.network.dns_fromdhcp == 0)
	{
		memcpy(g_onvif_cfg.network.dns_searchdomain, p_SetDNS_req->dns_searchdomain, sizeof(g_onvif_cfg.network.dns_searchdomain[0])*MAX_SEARCHDOMAIN);
		memcpy(g_onvif_cfg.network.dns_server, p_SetDNS_req->dns_server, sizeof(g_onvif_cfg.network.dns_server[0])*MAX_DNS_SERVER);
    }
	else
	{
		// get dns server from dhcp ...
		
	}
    
	return ONVIF_OK;
}

ONVIF_RET onvif_SetNTP(SetNTP_REQ * p_SetNTP_req)
{
	// set NTP ...

	g_onvif_cfg.network.ntp_fromdhcp = p_SetNTP_req->fromdhcp;

	if (g_onvif_cfg.network.ntp_fromdhcp == 0)
	{
		memcpy(g_onvif_cfg.network.ntp_server, p_SetNTP_req->ntp_server, sizeof(g_onvif_cfg.network.ntp_server[0])*MAX_NTP_SERVER);
    }
    else
    {
    	// get ntp server from dhcp ...
    	
    }
    
	return ONVIF_OK;
}


ONVIF_RET onvif_SetNetworkProtocols(SetNetworkProtocols_REQ * p_SetNetworkProtocols_req)
{
	if ((g_onvif_cfg.network.http_support == 0 && p_SetNetworkProtocols_req->http_flag) || 
		(g_onvif_cfg.network.https_support == 0 && p_SetNetworkProtocols_req->https_flag) || 
		(g_onvif_cfg.network.rtsp_support == 0 && p_SetNetworkProtocols_req->rtsp_flag))
	{
		return ONVIF_ERR_SERVICE_NOT_SUPPORT;
	}

	// set network protocols ...	


	if (p_SetNetworkProtocols_req->http_flag)
	{
		g_onvif_cfg.network.http_enable = p_SetNetworkProtocols_req->http_enable;
		memcpy(g_onvif_cfg.network.http_port, p_SetNetworkProtocols_req->http_port, sizeof(int)*MAX_SERVER_PORT);
	}

	if (p_SetNetworkProtocols_req->https_flag)
	{
		g_onvif_cfg.network.https_enable = p_SetNetworkProtocols_req->https_enable;
		memcpy(g_onvif_cfg.network.https_port, p_SetNetworkProtocols_req->https_port, sizeof(int)*MAX_SERVER_PORT);
	}

	if (p_SetNetworkProtocols_req->rtsp_flag)
	{
		g_onvif_cfg.network.rtsp_enable = p_SetNetworkProtocols_req->rtsp_enable;
		memcpy(g_onvif_cfg.network.rtsp_port, p_SetNetworkProtocols_req->rtsp_port, sizeof(int)*MAX_SERVER_PORT);
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_SetNetworkDefaultGateway(SetNetworkDefaultGateway_REQ * p_SetNetworkDefaultGateway_req)
{
	// set network default gateway ...

	memcpy(g_onvif_cfg.network.gateway, p_SetNetworkDefaultGateway_req->gateway, sizeof(g_onvif_cfg.network.gateway[0])*MAX_GATEWAY);

	return ONVIF_OK;
}

ONVIF_RET onvif_SetNetworkInterfaces(SetNetworkInterfaces_REQ * p_SetNetworkInterfaces_req)
{
	printf("onvif_SetNetworkInterfaces\n");
	
	struct in_addr ipaddr;
	char netmask[256];
	char gateway[256];
	LANConfig lanCfg;
	MsgGetNetworkLANConfig(&lanCfg);
	
	int local_dhcp_status = lanCfg.dhcpEnable;
	
	ONVIF_NET_INF * p_net_inf = onvif_find_net_inf(p_SetNetworkInterfaces_req->token);
	if (NULL == p_net_inf)
	{
		return ONVIF_ERR_INVALID_NETWORK_INTERFACE;
	}
	
	if (p_SetNetworkInterfaces_req->mtu < 0 || p_SetNetworkInterfaces_req->mtu > 1530)
	{
		return ONVIF_ERR_INVALID_MTU_VALUE;
	}
	
	if (p_SetNetworkInterfaces_req->enabled != FALSE)
	{
		if (p_SetNetworkInterfaces_req->ipv4_enabled != FALSE)
		{
			if(p_SetNetworkInterfaces_req->fromdhcp != FALSE && p_SetNetworkInterfaces_req->fromdhcp == 1 && local_dhcp_status == 0)
			{
				printf("p_SetNetworkInterfaces_req->fromdhcp:%d\n", p_SetNetworkInterfaces_req->fromdhcp);
				MsgEnableDhcp();
				MsgConfigNetwork();
				memset(g_onvif_cfg.serv_ip, 0, 32);
				strcpy(g_onvif_cfg.serv_ip, "0.0.0.0");
			}
			else if (is_ip_address(p_SetNetworkInterfaces_req->ipv4_addr) == TRUE  && p_SetNetworkInterfaces_req->prefix_len != 0)
			{
				ipv4_str_to_num(p_SetNetworkInterfaces_req->ipv4_addr, &ipaddr);
				
				printf("p_SetNetworkInterfaces_req->ipv4_addr:%s\n", p_SetNetworkInterfaces_req->ipv4_addr);
				in_addr_t mask = get_netmask_by_prefix_len(p_SetNetworkInterfaces_req->prefix_len);
				struct in_addr addr;
				addr.s_addr = mask;

				printf("NET_MASK :%s\n", inet_ntoa(addr));
				strcpy(netmask, inet_ntoa(addr));
				
				in_addr_t gatewayip = get_gateway_by_prefix_len( ntohl(ipaddr.s_addr), p_SetNetworkInterfaces_req->prefix_len);
				addr.s_addr = gatewayip;

				printf("IP %#x, GATEWAY :%s\n",ipaddr.s_addr,  inet_ntoa(addr));
				strcpy(gateway, inet_ntoa(addr));
				if (1)
				{
					p_net_inf->enabled = p_SetNetworkInterfaces_req->enabled;
					p_net_inf->fromdhcp = p_SetNetworkInterfaces_req->fromdhcp;
					strcpy(p_net_inf->ipv4_addr, p_SetNetworkInterfaces_req->ipv4_addr);
					p_net_inf->prefix_len = p_SetNetworkInterfaces_req->prefix_len;
				}
				MsgSetIp(p_SetNetworkInterfaces_req->ipv4_addr,  netmask, gateway);
				memset(g_onvif_cfg.serv_ip, 0, 32);
				strcpy(g_onvif_cfg.serv_ip, p_SetNetworkInterfaces_req->ipv4_addr);
			}
			
		}
	}
	return ONVIF_OK;
}

ONVIF_RET onvif_SetDiscoveryMode(SetDiscoveryMode_REQ * p_SetDiscoveryMode_req)
{
	g_onvif_cfg.discoverable = p_SetDiscoveryMode_req->discoverable;

	return ONVIF_OK;
}



