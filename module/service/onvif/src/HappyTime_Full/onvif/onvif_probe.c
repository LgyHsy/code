/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
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
#include "hxml.h"
#include "xml_node.h"
#include "onvif_probe.h"
#include "onvif_device.h"
#include "onvif.h"
#include "onvif_utils.h"
#include "arp_get_mac.h"
#include "anj_mw_net.h"
#include "anj_mw_comm.h"
#include "anj_mw_str.h"
#include "anj_config_network.h"
#include "para.h"
#include "record_log.h"

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern int g_allnet_allocated;				//全网通已被设置标准
extern MediaStreamConfig gStreamCfg;
extern int g_onvif_expand;

#define ScopeMatchByExact   "ScopeMatchByExact"
#define ScopeMatchByPrefix  "ScopeMatchByPrefix"
#define ScopeMatchByLdap    "ScopeMatchByLdap"
#define ScopeMatchByUuid    "ScopeMatchByUuid"
#define ScopeMatchByNone    "ScopeMatchByNone"

#define ONVIF_GRP_ADDR      "239.255.255.250"
#define ONVIF_GRP_PORT      3702
#define FILE_ONVIF_ALLNET_SAVEIP "/tmp/saveNVR_IP.cfg"

#if ONVIF_ALL_NET
extern char NVR_IP[10][16];
#endif 

/***************************************************************************************/
SOCKET onvif_probe_init()
{
	int opt = 1;
	int ttl = 64;
	int len = 65535;
	SOCKET fd;
	struct sockaddr_in addr;
	struct ip_mreq mcast;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	{
		log_print(HT_LOG_INFO, "socket SOCK_DGRAM error!\r\n");
		return 0;
	}

	addr.sin_family = AF_INET;
#if __WINDOWS_OS__
	addr.sin_addr.s_addr = get_default_if_ip();
#else
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	addr.sin_port = htons(ONVIF_GRP_PORT);

	/* reuse socket addr */
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)))
	{
		log_print(HT_LOG_INFO, "setsockopt SO_REUSEADDR error!\n");
	}

	if (g_onvif_expand)
	{
		// 允许接收广播数据
		int broadcast = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) 
		{
			log_print(HT_LOG_INFO, "setsockopt(SO_BROADCAST) failed");
			closesocket(fd);
			return 0;
		}
		else
			log_print(HT_LOG_INFO, "setsockopt(SO_BROADCAST) success\n");
	}
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		log_print(HT_LOG_INFO, "Bind udp socket fail,error = %s\r\n", sys_os_get_socket_error());
		closesocket(fd);
		return 0;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_INFO, "setsockopt SO_SNDBUF error!\n");
	}

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_INFO, "setsockopt SO_RCVBUF error!\n");
	}

	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));

	mcast.imr_multiaddr.s_addr = inet_addr(ONVIF_GRP_ADDR);
#if __WINDOWS_OS__
	mcast.imr_interface.s_addr = get_default_if_ip();
#else
	mcast.imr_interface.s_addr = htonl(INADDR_ANY);
#endif

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mcast, sizeof(mcast)) < 0)
	{
		log_print(HT_LOG_INFO, "setsockopt IP_ADD_MEMBERSHIP error!%s\n", sys_os_get_socket_error());
		closesocket(fd);
		return 0;
	}

	return fd;
}

int onvif_build_scopes_text(char * pbuf, int buflen)
{
	uint32 i;
	int offset = 0;

	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (g_onvif_cfg.scopes[i].ScopeItem[0] != '\0')
		{
			if (i == 0)
			{
				offset += snprintf(pbuf+offset, buflen-offset, "%s", g_onvif_cfg.scopes[i].ScopeItem);
			}
			else
			{
				offset += snprintf(pbuf+offset, buflen-offset, " %s", g_onvif_cfg.scopes[i].ScopeItem);
			}
		}
	}

#ifdef AUDIO_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/type/audio_encoder"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/type/audio_encoder");
	}
#endif

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)
	if (!onvif_is_scope_exist("onvif://www.onvif.org/type/video_encoder"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/type/video_encoder");
	}
#endif

#ifdef PTZ_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/type/ptz"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/type/ptz");
	}
#endif

#ifdef MEDIA_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/Profile/Streaming"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/Profile/Streaming");
	}
#endif

#ifdef MEDIA2_SUPPORT
	if (0)//!onvif_is_scope_exist("onvif://www.onvif.org/Profile/T"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/Profile/T");
	}
#endif

#ifdef PROFILE_G_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/Profile/G"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/Profile/G");
	}
#endif

#ifdef PROFILE_C_SUPPORT
	if (!onvif_is_scope_exist("onvif://www.onvif.org/Profile/C"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/Profile/C");
	}
#endif

#ifdef ACCESS_RULES
	if (0)//!onvif_is_scope_exist("onvif://www.onvif.org/Profile/A"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/Profile/A");
	}
#endif

#ifdef VIDEO_ANALYTICS
	if (0)//!onvif_is_scope_exist("onvif://www.onvif.org/Profile/M"))
	{
		offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/Profile/M");
	}
#endif

#ifdef PROFILE_Q_SUPPORT
	offset += snprintf(pbuf+offset, buflen-offset, " onvif://www.onvif.org/Profile/Q/");

	if (g_onvif_cfg.device_state)
	{
		offset += snprintf(pbuf+offset, buflen-offset, "Operational");
	}
	else
	{
		offset += snprintf(pbuf+offset, buflen-offset, "FactoryDefault");
	}
#endif

	return offset;
}

BOOL onvif_scope_match(const char * matchby, char * scope)
{
	uint32 i;

	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (g_onvif_cfg.scopes[i].ScopeItem[0] != '\0')
		{
			if (strcmp(matchby, ScopeMatchByExact) == 0)
			{
				if (strcmp(scope, g_onvif_cfg.scopes[i].ScopeItem) == 0)
				{
					return TRUE;
				}
			}
			else if (memcmp(scope, g_onvif_cfg.scopes[i].ScopeItem, strlen(scope)) == 0)
			{
				return TRUE;
			}
		}
	}

#ifdef PROFILE_Q_SUPPORT    
	if (g_onvif_cfg.device_state)
	{
		if (strcmp(matchby, ScopeMatchByExact) == 0)
		{
			if (strcmp(scope, "onvif://www.onvif.org/Profile/Q/Operational") == 0)
			{
				return TRUE;
			}
		}
		else if (memcmp(scope, "onvif://www.onvif.org/Profile/Q/Operational", strlen(scope)) == 0)
		{
			return TRUE;
		}
	}
	else
	{
		if (strcmp(matchby, ScopeMatchByExact) == 0)
		{
			if (strcmp(scope, "onvif://www.onvif.org/Profile/Q/FactoryDefault") == 0)
			{
				return TRUE;
			}
		}
		else if (memcmp(scope, "onvif://www.onvif.org/Profile/Q/FactoryDefault", strlen(scope)) == 0)
		{
			return TRUE;
		}
	}
#endif

	return FALSE;
}

BOOL onvif_scopes_match(const char * matchby, char * scopes)
{
	int i = 0;
	const char * p_buf = scopes;
	char scope[256] = {'\0'};

	// remove space
	while (*p_buf != '\0') 
	{
		if (*p_buf == ' ') p_buf++;
		else break;
	}
	while (*p_buf != '\0') 
	{
		if (*p_buf == ' ')
		{
			if (i > 0)
			{
				scope[i] = '\0';
				if (!onvif_scope_match(matchby, scope))
				{
					return FALSE;
				}
			}
			i = 0;
		}
		else if (i < 255)
		{
			scope[i++] = *p_buf;
		}
		p_buf++;
	}

	if (i > 0)
	{
		scope[i] = '\0';

		if (!onvif_scope_match(matchby, scope))
		{
			return FALSE;
		}
	}

	return TRUE;
}

void onvif_probe_err_rly(SOCKET vfd, uint32 rip, uint16 rport, char * p_msg_id, const char * code, const char * subcode, const char * reason)
{
	int rlen;
	int offset = 0;
	char buff[2048] = {'\0'};
	char uuid[100] = {'\0'};
	int mlen = sizeof(buff);
	struct sockaddr_in addr;

	offset = snprintf(buff, mlen, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:enc=\"http://www.w3.org/2003/05/soap-encoding\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
		"xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
		"xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
		"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">");

	offset += snprintf(buff+offset, mlen-offset, "<s:Header>"
		"<wsa:MessageID>uuid:%s</wsa:MessageID>"
		"<wsa:RelatesTo>%s</wsa:RelatesTo>"
		"<wsa:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>"
		"</s:Header>", onvif_uuid_create(uuid, sizeof(uuid)), p_msg_id);

	offset += snprintf(buff+offset, mlen-offset, "<s:Body>");

	offset += snprintf(buff+offset, mlen-offset, 
		"<s:Fault>"
			"<s:Code>"
				"<s:Value>%s</s:Value>"
				"<s:Subcode>"
					"<s:Value>%s</s:Value>"
				"</s:Subcode>"
			"</s:Code>"
			"<s:Reason>"
				"<s:Text xml:lang=\"en\">%s</s:Text>"
			"</s:Reason>"
		"</s:Fault>",
		code, subcode, reason);

	offset += snprintf(buff+offset, mlen-offset, "</s:Body></s:Envelope>");

	log_print(HT_LOG_DBG, "%s, buff = %s\r\n", __FUNCTION__, buff);

	// send to received addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = rip;
	addr.sin_port = rport;
	
	rlen = sendto(vfd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "%s, rlen=%d, slen=%d, ip=%s\r\n", __FUNCTION__, rlen, offset, get_ip_str_(rip));
	}
}

int onvif_probe_uniview_rly(char * p_msg_id, SOCKET vfd, uint32 rip, uint16 rport, const char * matchby, char * scopes)
{
	int rlen;
	int offset;
	int mlen;
	char uuid[100] = {'\0'};
	char buff[1024 * 10];
	char saddr[256] = {'\0'};
	struct sockaddr_in addr;

	if (DiscoveryMode_NonDiscoverable == g_onvif_cfg.network.DiscoveryMode)
	{
		return -1;
	}

	if (strlen(matchby) > 0 && 
		strcmp(matchby, ScopeMatchByExact) && 
		strcmp(matchby, ScopeMatchByPrefix) && 
		strcmp(matchby, ScopeMatchByLdap) && 
		strcmp(matchby, ScopeMatchByUuid) && 
		strcmp(matchby, ScopeMatchByNone))
	{
		onvif_probe_err_rly(vfd, rip, rport, p_msg_id, "s:Sender", "d:MatchingRuleNotSupported", "MatchingRuleNotSupported");
		return -1;
	}

	if (!onvif_scopes_match(matchby, scopes))
	{
		return -1;
	}
	
	mlen = sizeof(buff);
	
	offset = snprintf(buff, mlen, 
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:enc=\"http://www.w3.org/2003/05/soap-encoding\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
		"xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
		"xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
		"xmlns:tplt=\"http://www.onvif.org/ver10/plus/schema\" "
		"xmlns:tpl=\"http://www.onvif.org/ver10/plus/wsdl\" "
		"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
			);
	
	offset += snprintf(buff+offset, mlen-offset, "<s:Header>"
		"<wsa:MessageID>uuid:%s</wsa:MessageID>"
		"<wsa:RelatesTo>%s</wsa:RelatesTo>"
		"<wsa:To >urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/UniviewProbeMatches</wsa:Action>"
		"<tns:AppSequence InstanceId=\"1\" MessageNumber=\"10008\"></tns:AppSequence>"
		"</s:Header>", onvif_uuid_create(uuid, sizeof(uuid)), p_msg_id);

	offset += snprintf(buff+offset, mlen-offset, "<s:Body>"
		"<tns:UniviewProbeMatches><tns:ProbeMatch>"
		"<wsa:EndpointReference>"
		"<wsa:Address>urn:uuid:%s</wsa:Address>"
		"</wsa:EndpointReference>"
		"<tns:Types>dn:NetworkVideoTransmitter tds:Device</tns:Types>",
		g_onvif_cfg.EndpointReference);

	offset += snprintf(buff+offset, mlen-offset, "<tns:Scopes>");
	offset += onvif_build_scopes_text(buff+offset, mlen-offset);	
	offset += snprintf(buff+offset, mlen-offset, "</tns:Scopes>");
	
	offset += snprintf(buff+offset, mlen-offset, "<tns:XAddrs>%s</tns:XAddrs>"
		"<tns:MetadataVersion>1</tns:MetadataVersion>"
		"</tns:ProbeMatch></tns:UniviewProbeMatches></s:Body></s:Envelope>", 
		onvif_get_service_addr(CapabilityCategory_Device, 0, rip, saddr, sizeof(saddr)-1));

	log_print(HT_LOG_DBG, "%s, buff = %s\r\n", __FUNCTION__, buff);

	// send to received addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = rip;
	addr.sin_port = rport;
	
	rlen = sendto(vfd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "%s, rlen=%d, slen=%d, ip=%s\r\n", __FUNCTION__, rlen, offset, get_ip_str_(rip));
	}

	// send to multicast addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ONVIF_GRP_ADDR);
	addr.sin_port = htons(ONVIF_GRP_PORT);
	
	rlen = sendto(vfd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "%s, rlen=%d, slen=%d\r\n", __FUNCTION__, rlen, offset);
	}

	return rlen;
}

int onvif_probe_rly(char * p_msg_id, SOCKET vfd, uint32 rip, uint16 rport, const char * matchby, char * scopes)
{
	int rlen;
	int offset;
	int mlen;
	char uuid[100] = {'\0'};
	char buff[1024 * 10];
	char saddr[256] = {'\0'};
	struct sockaddr_in addr;

	if (DiscoveryMode_NonDiscoverable == g_onvif_cfg.network.DiscoveryMode)
	{
		return -1;
	}

	if (strlen(matchby) > 0 && 
		strcmp(matchby, ScopeMatchByExact) && 
		strcmp(matchby, ScopeMatchByPrefix) && 
		strcmp(matchby, ScopeMatchByLdap) && 
		strcmp(matchby, ScopeMatchByUuid) && 
		strcmp(matchby, ScopeMatchByNone))
	{
		onvif_probe_err_rly(vfd, rip, rport, p_msg_id, "s:Sender", "d:MatchingRuleNotSupported", "MatchingRuleNotSupported");
		return -1;
	}

	if (!onvif_scopes_match(matchby, scopes))
	{
		return -1;
	}
	
	mlen = sizeof(buff);
	
	offset = snprintf(buff, mlen, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:enc=\"http://www.w3.org/2003/05/soap-encoding\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
		"xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
		"xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
		"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">");
	
	offset += snprintf(buff+offset, mlen-offset, "<s:Header>"
		"<wsa:MessageID>uuid:%s</wsa:MessageID>"
		"<wsa:RelatesTo>%s</wsa:RelatesTo>"
		"<wsa:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>"
		"</s:Header>", onvif_uuid_create(uuid, sizeof(uuid)), p_msg_id);

	offset += snprintf(buff+offset, mlen-offset, "<s:Body>"
		"<d:ProbeMatches><d:ProbeMatch>"
		"<wsa:EndpointReference>"
		"<wsa:Address>urn:uuid:%s</wsa:Address>"
		"</wsa:EndpointReference>"
		"<d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>",
		g_onvif_cfg.EndpointReference);

	offset += snprintf(buff+offset, mlen-offset, "<d:Scopes>");
	offset += onvif_build_scopes_text(buff+offset, mlen-offset);	
	offset += snprintf(buff+offset, mlen-offset, "</d:Scopes>");
	
	offset += snprintf(buff+offset, mlen-offset, "<d:XAddrs>%s</d:XAddrs>"
		"<d:MetadataVersion>1</d:MetadataVersion>"
		"</d:ProbeMatch></d:ProbeMatches></s:Body></s:Envelope>", 
		onvif_get_service_addr(CapabilityCategory_Device, 0, rip, saddr, sizeof(saddr)-1));

	log_print(HT_LOG_DBG, "%s, buff = %s\r\n", __FUNCTION__, buff);

	// send to received addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = rip;
	addr.sin_port = rport;
	
	rlen = sendto(vfd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "%s, rlen=%d, slen=%d, ip=%s\r\n", __FUNCTION__, rlen, offset, get_ip_str_(rip));
	}

	// send to multicast addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ONVIF_GRP_ADDR);
	addr.sin_port = htons(ONVIF_GRP_PORT);
	
	rlen = sendto(vfd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "%s, rlen=%d, slen=%d\r\n", __FUNCTION__, rlen, offset);
	}

	return rlen;
}

int onvif_resolve_rly(char * p_msg_id, SOCKET vfd, uint32 rip, uint16 rport, const char * ref)
{
	int rlen;
	int offset;
	int mlen;
	char uuid[100] = {'\0'};
	char buff[1024 * 10];
	char saddr[256] = {'\0'};
	struct sockaddr_in addr;

	if (!strstr(ref, g_onvif_cfg.EndpointReference))
	{
		return -1;
	}
	
	mlen = sizeof(buff);
	
	offset = snprintf(buff, mlen, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:enc=\"http://www.w3.org/2003/05/soap-encoding\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
		"xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
		"xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
		"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">");
	
	offset += snprintf(buff+offset, mlen-offset, "<s:Header>"
		"<wsa:MessageID>uuid:%s</wsa:MessageID>"
		"<wsa:RelatesTo>%s</wsa:RelatesTo>"
		"<wsa:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ResolveMatches</wsa:Action>"
		"</s:Header>", onvif_uuid_create(uuid, sizeof(uuid)), p_msg_id);

	offset += snprintf(buff+offset, mlen-offset, "<s:Body>"
		"<d:ResolveMatches><d:ResolveMatch>"
		"<wsa:EndpointReference>"
		"<wsa:Address>urn:uuid:%s</wsa:Address>"
		"</wsa:EndpointReference>"
		"<d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>",
		g_onvif_cfg.EndpointReference);
	
	offset += snprintf(buff+offset, mlen-offset, "<d:XAddrs>%s</d:XAddrs>"
		"<d:MetadataVersion>1</d:MetadataVersion>"
		"</d:ResolveMatch></d:ResolveMatches></s:Body></s:Envelope>", 
		onvif_get_service_addr(CapabilityCategory_Device, 0, rip, saddr, sizeof(saddr)-1));

	log_print(HT_LOG_DBG, "%s, buff = %s\r\n", __FUNCTION__, buff);

	// send to received addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = rip;
	addr.sin_port = rport;
	
	rlen = sendto(vfd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "%s, rlen=%d, slen=%d, ip=%s\r\n", __FUNCTION__, rlen, offset, get_ip_str_(rip));
	}

	return rlen;
}

BOOL onvif_type_match(const char * types)
{
	int i = 0;
	const char * p_buf = types;
	char type[256] = {'\0'};

	// remove space
	while (*p_buf != '\0') 
	{
		if (*p_buf == ' ')
			p_buf ++;
		else
			break;
	}

	while (*p_buf != '\0') 
	{
		if (*p_buf == ' ')
		{
			if (i > 0)
			{
				type[i] = '\0';
				if (soap_strcmp(type, "NetworkVideoTransmitter") == 0 || soap_strcmp(type, "Device") == 0)
				{
					return TRUE;
				}
			}
			i = 0;
		}
		else if (i < 255)
		{
			type[i++] = *p_buf;
		}
		p_buf++;
	}

	if (i > 0)
	{
		type[i] = '\0';
		if (soap_strcmp(type, "NetworkVideoTransmitter") == 0 || soap_strcmp(type, "Device") == 0)
		{
			return TRUE;
		}
	}

	return TRUE;
}

void onvif_probe_parse(SOCKET fd, char *rbuf, int rlen, uint32 sip, uint16 sport)
{
	char message_id[128] = {'\0'};
	XMLN * p_node;

	p_node = xxx_hxml_parse(rbuf, rlen);
	if (p_node == NULL)
	{
		log_print(HT_LOG_ERR, "%s, hxml parse err!!!\r\n", __FUNCTION__);
	}	
	else
	{
		XMLN * p_Header;
		XMLN * p_Body;
		p_Header = xml_node_soap_get(p_node, "Header"); 
		if (p_Header)
		{
			XMLN * p_MessageID = xml_node_soap_get(p_Header, "MessageID"); 
			if (p_MessageID && p_MessageID->data)
			{
				strncpy(message_id, p_MessageID->data, sizeof(message_id)-1);
			}
		}
		p_Body = xml_node_soap_get(p_node, "Body");
		if (p_Body)
		{
			if (g_onvif_expand)
			{
				XMLN * p_UniviewProbe = xml_node_soap_get(p_Body, "UniviewProbe");
				if (p_UniviewProbe)
				{
					XMLN * p_Scopes;
					XMLN * p_Types;
					char matchby[32] = {'\0'};
					char scopes[1024] = {'\0'};
					p_Scopes = xml_node_soap_get(p_UniviewProbe, "Scopes");
					if (p_Scopes)
					{
						const char * p_MatchBy;
						p_MatchBy = xml_attr_get(p_Scopes, "MatchBy");
						if (p_MatchBy)
						{
							strncpy(matchby, p_MatchBy, sizeof(matchby)-1);
						}
						if (p_Scopes->data)
						{
							strncpy(scopes, p_Scopes->data, sizeof(scopes)-1);
						}
					}
					p_Types = xml_node_soap_get(p_UniviewProbe, "Types");
					if (p_Types && p_Types->data)
					{
						if (onvif_type_match(p_Types->data))
						{
							log_print(HT_LOG_INFO, "%s, discovery NetworkVideoTransmitter\r\n", __FUNCTION__);
							onvif_probe_uniview_rly(message_id, fd, sip, sport, matchby, scopes);
						}
					}
					else
					{
						log_print(HT_LOG_INFO, "%s, discovery NetworkVideoTransmitter\r\n", __FUNCTION__);
						onvif_probe_uniview_rly(message_id, fd, sip, sport, matchby, scopes);
					}
				}
			}
			XMLN * p_Resolve = xml_node_soap_get(p_Body, "Resolve");
			XMLN * p_Probe = xml_node_soap_get(p_Body, "Probe");
			if (p_Probe)
			{
				XMLN * p_Scopes;
				XMLN * p_Types;
				char matchby[32] = {'\0'};
				char scopes[1024] = {'\0'};
				p_Scopes = xml_node_soap_get(p_Probe, "Scopes");
				if (p_Scopes)
				{
					const char * p_MatchBy;
					p_MatchBy = xml_attr_get(p_Scopes, "MatchBy");
					if (p_MatchBy)
					{
						strncpy(matchby, p_MatchBy, sizeof(matchby)-1);
					}
					if (p_Scopes->data)
					{
						strncpy(scopes, p_Scopes->data, sizeof(scopes)-1);
					}
				}
				p_Types = xml_node_soap_get(p_Probe, "Types");
				if (p_Types && p_Types->data)
				{
					if (onvif_type_match(p_Types->data))
					{
						log_print(HT_LOG_DBG, "%s, discovery NetworkVideoTransmitter\r\n", __FUNCTION__);
						onvif_probe_rly(message_id, fd, sip, sport, matchby, scopes);
					}
				}
				else
				{
					log_print(HT_LOG_INFO, "%s, discovery NetworkVideoTransmitter\r\n", __FUNCTION__);
					onvif_probe_rly(message_id, fd, sip, sport, matchby, scopes);
				}
			}
			else if (p_Resolve)
			{
				XMLN * p_EndpointReference;
				p_EndpointReference = xml_node_soap_get(p_Resolve, "EndpointReference");
				if (p_EndpointReference)
				{
					XMLN * p_Address;
					p_Address = xml_node_soap_get(p_EndpointReference, "Address");
					if (p_Address && p_Address->data)
					{
						onvif_resolve_rly(message_id, fd, sip, sport, p_Address->data);
					}
				}
			}
		}		
	}

	xml_node_del(p_node);
}

int onvif_set_ip(const char* ifname, const char *dstIP, const char *dstGateway)
{
	in_addr_t nMyIpAddr = get_my_ipaddr(); //net_get_ifaddr(ETH_NAME);

	char ipaddCmd[128] = {0};
	//修改eth0的IP
	sprintf(ipaddCmd,"ifconfig eth0 %s",dstIP);

	log_print(HT_LOG_INFO, "%s\n",ipaddCmd); 			
	mysystem_with_param("%s", ipaddCmd);
	
	sprintf(ipaddCmd,"route add default gw %s",dstGateway);
	log_print(HT_LOG_INFO, "%s\n",ipaddCmd); 			
	mysystem_with_param("%s", ipaddCmd);

	usleep(2000);
	
	char szCurIpAddr[MAX_IP_NAME_LEN];
	get_ip_str(nMyIpAddr, szCurIpAddr, MAX_IP_NAME_LEN);

	//判断IP是否冲突
	const char *if_name = ifname; // 网卡名称 如: eth0
	const char *str_src_ip = szCurIpAddr; // 本设备IP
	const char *str_dst_ip = dstIP;
	unsigned char dst_mac[ARP_MAC_BYTE];
	int ret = arp_get_mac(if_name,str_src_ip,str_dst_ip,dst_mac, ARP_TIME_OUT_MS);
	if(ret == 1)
	{ 
		//该IP冲突，继续修改
		log_print(HT_LOG_INFO, "2>IP(%s) conflict\n",dstIP);
		return 0;
	}	

	//重启网卡
	LANConfig  mylanCfg;
	memcpy(&mylanCfg, getNetWorkConfig(), sizeof(mylanCfg));
	if( mylanCfg.dhcpEnable != 0 || strcmp(mylanCfg.IPAddress,dstIP) != 0 )
	{
		log_print(HT_LOG_INFO, "ready to set %s", dstIP);
		
		mylanCfg.dhcpEnable = 0;//全网通需要关闭DHCP
		strcpy(mylanCfg.IPAddress,dstIP);
		strcpy(mylanCfg.gateWay,dstGateway);

		string_trim_tail(mylanCfg.DNS1);
		string_trim_tail(mylanCfg.DNS2);

		if(strlen(mylanCfg.DNS1) == 0 && strlen(mylanCfg.DNS2) == 0)
		{
			strcpy(mylanCfg.DNS1,dstGateway);
			strcpy(mylanCfg.DNS2,"114.114.114.114");
		}
		
		anj_config_network_lan_set(&mylanCfg);
		
		usleep(1000000); //等待1s 等ipconfig配置生效后再判断ip是否冲突
	}
	g_allnet_allocated = 1;
	return 1;
}

void clear_saved_nvr_ip()
{
	memset(NVR_IP, 0, sizeof(NVR_IP));
	return ;
}

void write_saved_nvr_ip()
{
	FILE* fp = fopen(FILE_ONVIF_ALLNET_SAVEIP,"w");
	if(fp)
	{
		int j = 0;
		for(j = 0; j < 10; j ++)
		{
			if(strlen(NVR_IP[j]) > 0)
			{
				char xmlNVR_IP[sizeof("<NVR_IP></NVR_IP>") + sizeof(NVR_IP)] = {0};
				snprintf(xmlNVR_IP, sizeof(xmlNVR_IP), "<NVR_IP>%s</NVR_IP>", NVR_IP[j]);
				fwrite(xmlNVR_IP, sizeof(char), strlen(xmlNVR_IP), fp);
				log_print(HT_LOG_DBG, "save nvr ip: [%s] \n", NVR_IP[j]);
			}
		}
		fclose(fp);
	}
	return ;
}

//从文件读取到NVR IP历史数组
void read_saved_nvr_ip()
{
	memset(NVR_IP, 0, sizeof(NVR_IP));
	
	char NVR_IPbuf[512] = {0};
	FILE* fp = fopen(FILE_ONVIF_ALLNET_SAVEIP, "r");
	
	if(fp)
	{
		fread(NVR_IPbuf, sizeof(char), sizeof(NVR_IPbuf), fp);
		fclose(fp);

		char *pNVR_IPbuf = NVR_IPbuf;
		int j = 0;
		while(pNVR_IPbuf)
		{
			char *pIndex1 = strstr(pNVR_IPbuf, "<NVR_IP>");
			char *pIndex2 = strstr(pNVR_IPbuf, "</NVR_IP>");
			if((pIndex1 != NULL) && (pIndex2 != NULL))
			{
				strncpy(NVR_IP[j], pIndex1 + strlen("<NVR_IP>"), pIndex2 - pIndex1 - strlen("<NVR_IP>"));
				pNVR_IPbuf = pIndex2 + strlen("</NVR_IP>");
				j ++;
			}
			else
				break;
		}
	}
	log_print(HT_LOG_DBG, "read nvr ip: [%s] \n", NVR_IP);
	return ;
}

//将NVR IP数组存入历史表
int add_saved_nvr_ip(const char* szNvrIp)
{
	int j = 0;
	for(j = 0; j < 10; j ++)
	{
		if(strlen(NVR_IP[j]) > 0)
		{
			if(strcmp(NVR_IP[j], szNvrIp) == 0)
			{
				log_print(HT_LOG_DBG, "Got NVR_IP %s on position %d\n", szNvrIp, j);
				return j;
			}
		}
	}

	for(j = 0; j < 10; j ++)
	{
		if(strlen(NVR_IP[j]) == 0)
		{
			strcpy(NVR_IP[j], szNvrIp);
			log_print(HT_LOG_DBG, "save NVR_IP %s on position %d\n", szNvrIp, j);
			return j;
		}
	}

	log_print(HT_LOG_INFO, "save NVR_IP %s: no position\n",szNvrIp);
	return -1;
}

void allnet_server(struct sockaddr_in *addr)
{
	int bCanSetIp = 0;								//可以设置ip
	int flag_samenetwork = 0;						//同网段
	int flag_eth0IPconflict = 0;					//同网段ip冲突
	//char addIPAddr[100] = {0};
	char ifname[256] = {0}; 						//网卡
	char macaddr[MACH_ADDR_LENGTH] = {0};			//随机数
	char szClientIp[MAX_IP_NAME_LEN] = {0}; 		//client的ip
	char szCurIpAddr[MAX_IP_NAME_LEN] = {0};		//设备当前ip
	char szMask[MAX_IP_NAME_LEN] = {0}; 			//掩码
	char szClientIpGateway[MAX_IP_NAME_LEN] = {0};	//client的ip网关

	
	get_my_macaddr(macaddr);
	strncpy(szClientIp, inet_ntoa(addr->sin_addr), MAX_IP_NAME_LEN - 1);
	
	struct NET_CONFIG netcfg;
	get_my_ifname(ifname);
	net_get_info(ifname, &netcfg);//获取当前network

	if( netcfg.netmask == 0 )
		netcfg.netmask = 0xffffff;//如果掩码为0，强制设成255.255.255.0

	unsigned int network_my = netcfg.ifaddr & netcfg.netmask;
	unsigned int network_client = addr->sin_addr.s_addr;
	unsigned int nClientIpNum = ntohl(network_client & 0xff000000);
	unsigned int gateway_client = htonl(ntohl(addr->sin_addr.s_addr & 0xffffff) + 1) ;
	
	get_ip_str(netcfg.ifaddr, szCurIpAddr, MAX_IP_NAME_LEN);
	get_ip_str(netcfg.netmask, szMask, MAX_IP_NAME_LEN);
	get_ip_str(gateway_client, szClientIpGateway, MAX_IP_NAME_LEN);

	log_print(HT_LOG_DBG, "%s: %#x(%s):%#x(%s), onvif client %#x(%s), client gateway %s, client num %u\n",
		ifname, netcfg.ifaddr, szCurIpAddr, netcfg.netmask, szMask,
		addr->sin_addr.s_addr, szClientIp, szClientIpGateway, nClientIpNum);
	
	if(strlen(NVR_IP[0]) <= 0)																					//没有NVR记录
	{
		//没有记录已访问的NVR IP,可以修改eth0的ip
		//log_print(HT_LOG_ERR, "No nvr history, can set ip now by nvr %s", szClientIp);
		bCanSetIp = 1;
	}
	else 
	{
		//有记录的NVR,判断eth0的网段的NVR是否在线 	
		int bHaveSavedNvrOnline = 0;																			//有NVR在线
		int j = 0;
		for(j = 0; j < 10; j ++)
		{
			if(strlen(NVR_IP[j]) > 0)
			{
				unsigned int nNvrIp = inet_addr(NVR_IP[j]);
				if((netcfg.ifaddr & 0xffffff) == (nNvrIp & 0xffffff))//同一网段
				{
					const char *if_name = ifname; // 网卡名称 如: eth0
					char *str_src_ip = szCurIpAddr; // 本设备IP
					char *str_dst_ip = NVR_IP[j];
					unsigned char dst_mac[ARP_MAC_BYTE];
					if(strcmp(str_src_ip, str_dst_ip) == 0)
					{
						log_print(HT_LOG_INFO, "NVR_IP[%d]:%s same with CurIpAddr: %s \n", j, str_src_ip, str_dst_ip);
						continue;
					}
					int ret = arp_get_mac(if_name, str_src_ip, str_dst_ip, dst_mac, ARP_TIME_OUT_MS);
					if(ret != 0)
					{ 
						//NVR还在线
						bHaveSavedNvrOnline = 1;
						log_print(HT_LOG_DBG, "ret=%d,NVR_IP[%d](%s) on line!\n", ret, j, NVR_IP[j]);
						break;
					}
					log_print(HT_LOG_ERR, "ret=%d,NVR_IP[%d](%s) NOT on line!\n", ret, j, NVR_IP[j]);
					memset(NVR_IP[j], 0, sizeof(NVR_IP[j]));
				}
				else
				{
					log_print(HT_LOG_INFO, "NVR_IP[%d](%s) NOT same network with my ip %s!\n", j, NVR_IP[j], szCurIpAddr);
					memset(NVR_IP[j], 0, sizeof(NVR_IP[j]));
				}
			}
		}
		if(0 == bHaveSavedNvrOnline)
		{
			//都已下线
			log_print(HT_LOG_INFO, "All history nvr are offline. can set ip now by nvr %s \n", szClientIp);
			bCanSetIp = 1;
			clear_saved_nvr_ip();
			write_saved_nvr_ip();
			read_saved_nvr_ip();
		}
	}

	if( add_saved_nvr_ip(szClientIp) >= 0 ) 																		//soap过来的对方ip
	{
		write_saved_nvr_ip();
	}
	
	network_client = network_client & netcfg.netmask;
	log_print(HT_LOG_DBG, "wsdd_event_Probe1, network_client:%d, network_my:%d\n", network_client, network_my);
	if( network_my == network_client)//同网段请求
	{
		flag_samenetwork = 1;
		char szNetwork[MAX_IP_NAME_LEN];
		get_ip_str(network_my, szNetwork, MAX_IP_NAME_LEN);
		log_print(HT_LOG_DBG, "network addr %#x(%s) same with onvif client.\n", network_my, szNetwork);

		char *if_name = ifname; 		// 网卡名称 如: eth0
		char *str_src_ip = szCurIpAddr; // 本设备IP
		char *str_dst_ip = szCurIpAddr;
		unsigned char dst_mac[ARP_MAC_BYTE];
		int ret = arp_get_mac(if_name, str_src_ip, str_dst_ip, dst_mac, ARP_TIME_OUT_MS);
		if(ret != 2)
		{
			//该IP冲突，修改ip
			flag_eth0IPconflict = 1;
			log_print(HT_LOG_INFO, "IP confilict, change IP\n");
		}
		else
		{
			//该IP仅自己使用，不冲突
			log_print(HT_LOG_DBG, "IP no conflict, no need to change IP\n");
			flag_eth0IPconflict = 0;
		}
	}
	log_print(HT_LOG_DBG, "flag_samenetwork:%d\n", flag_samenetwork);
	
	log_print(HT_LOG_DBG, "bCanSetIp:%d, flag_samenetwork:%d, flag_eth0IPconflict:%d\n", bCanSetIp, flag_samenetwork, flag_eth0IPconflict);
	if( (bCanSetIp && flag_samenetwork == 0) || (flag_eth0IPconflict == 1))  //跨网段请求or 同网段eth0的ip冲突
	{
		unsigned char fromIpNum=0;
		srand((unsigned)time(NULL) + macaddr[4]*256 + macaddr[5]);	//取随机数
		fromIpNum = rand() % 256;
		if((fromIpNum < 2) || (fromIpNum >= 255))
			fromIpNum = 2;

		unsigned int network_client = addr->sin_addr.s_addr;
		network_client = network_client & 0xffffff;

		char dstIP[MAX_IP_NAME_LEN] = {0};

		unsigned int nAllocIp;
		int iIndex = 0;
		for( iIndex = 0; iIndex < 254; iIndex ++)//以随机数为基准，向上遍历IP
		{
			//memset(addIPAddr,0,sizeof(addIPAddr));
			
			unsigned int ipNum = iIndex + fromIpNum;
			ipNum = ipNum % 255;
			if( ipNum == 0 || ipNum == 1 || ipNum == nClientIpNum)
				continue;

			nAllocIp = htonl(ntohl(network_client) + ipNum);
			get_ip_str(nAllocIp, dstIP, MAX_IP_NAME_LEN);
			log_print(HT_LOG_INFO, "nAllocIp:%d, dstIP:%s\n", nAllocIp, dstIP);
			char *if_name = ifname; // 网卡名称 如: eth0
			char *str_src_ip = szCurIpAddr; // 本设备IP
			char *str_dst_ip = dstIP;
			unsigned char dst_mac[ARP_MAC_BYTE];
			log_print(HT_LOG_INFO, "ready arp_get_mac str_src_ip:%s, str_dst_ip:%s\n", str_src_ip, str_dst_ip);
			int ret = arp_get_mac(if_name, str_src_ip, str_dst_ip, dst_mac, ARP_TIME_OUT_MS);
			if(ret == 1)
			{
				log_print(HT_LOG_INFO, "%s is used, try next ip\n", dstIP);
				//该IP被占用，继续尝试下一个
				continue;
			}

			if(0 == onvif_set_ip(ifname,  dstIP, szClientIpGateway))
			{
				log_print(HT_LOG_INFO, "%s set failed, try next ip\n", dstIP);
				continue;
			}
			else
			{
				/*关闭全网通重启服务*/
				NetworkConfigNew pNetworkCfg;
				memset(&pNetworkCfg, 0, sizeof(NetworkConfigNew));
				memcpy(&pNetworkCfg, (NetworkConfigNew *)getNetWorkConfig(), sizeof(pNetworkCfg));
				pNetworkCfg.lanCfg.onvifAllnetEnable = 0;
				anj_config_network_set(&pNetworkCfg);
				system("killall web_server");
				log_print(HT_LOG_INFO, "%s set ip cuccess\n", dstIP);
				__RECORD_LOG_INFO("ONVIF set ip from %s to %s \n", szCurIpAddr, dstIP);
			}
			//sprintf(addIPAddr,"%s",dstIP);
			break;
		}
	}
}


int onvif_probe_net_rx(SOCKET fd)
{
	int sret;
	int rlen;
	int addr_len;
	char rbuf[1024 * 10] = {'\0'};
	uint32 src_ip;
	uint16 src_port;
	fd_set fdr;
	struct timeval tv;
	struct sockaddr_in addr;

	FD_ZERO(&fdr);
	FD_SET(fd, &fdr);

	tv.tv_sec = 0;
	tv.tv_usec = 100 * 1000;

	sret = select((int)(fd + 1), &fdr, NULL, NULL, &tv);
	if (sret == 0)
	{
		return 0;
	}
	else if (sret < 0)
	{
		log_print(HT_LOG_ERR, "%s, select err[%s]\r\n", __FUNCTION__, sys_os_get_socket_error());
		return -1;
	}

	addr_len = sizeof(struct sockaddr_in);
	rlen = recvfrom(fd, rbuf, sizeof(rbuf), 0, (struct sockaddr *)&addr, (socklen_t*)&addr_len);
	if (rlen < 0)
	{
		log_print(HT_LOG_ERR, "%s, recvfrom err[%s]\r\n", __FUNCTION__, sys_os_get_socket_error());
		return -1;
	}

	src_ip = addr.sin_addr.s_addr;
	src_port = addr.sin_port;

	log_print(HT_LOG_DBG, "%s, rbuf = %s\r\n", __FUNCTION__, rbuf);

	if (1)
	{
		// log_print(HT_LOG_INFO, "%s, rbuf = %s\r\n", __FUNCTION__, rbuf);
		// log_print(HT_LOG_INFO, "\n\nReceived %zd bytes from %s:%d\n\n", rlen, inet_ntoa(addr.sin_addr), ntohs(src_port));
	}
	LANConfig *pLan = (LANConfig *)getNetWorkConfig();
	if (pLan && pLan->onvifAllnetEnable)
		allnet_server(&addr);
	
	onvif_probe_parse(fd, rbuf, rlen, src_ip, src_port);

	return 0;
}

void onvif_hello()
{
	int rlen;
	int offset = 0;
	int mlen;
	char uuid[100] = {'\0'};
	char buff[1024 * 10];
	char saddr[256] = {'\0'};
	struct sockaddr_in addr;

	if (DiscoveryMode_NonDiscoverable == g_onvif_cfg.network.DiscoveryMode)
	{
		return;
	}

	mlen = sizeof(buff);

	offset += snprintf(buff + offset, mlen - offset,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:enc=\"http://www.w3.org/2003/05/soap-encoding\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
		"xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
		"xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
		"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">");

	offset += snprintf(buff + offset, mlen - offset, 
		"<s:Header>"
		"<wsa:MessageID>uuid:%s</wsa:MessageID>"
		"<wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Hello</wsa:Action>"
		"</s:Header>", 
		onvif_uuid_create(uuid, sizeof(uuid)));

	offset += snprintf(buff + offset, mlen - offset, 
		"<s:Body><d:Hello>"
		"<wsa:EndpointReference>"
			"<wsa:Address>urn:uuid:%s</wsa:Address>"
		"</wsa:EndpointReference>"
		"<d:Types>dn:NetworkVideoTransmitter tds:Device</d:Types>",
		g_onvif_cfg.EndpointReference);

	offset += snprintf(buff + offset, mlen - offset, "<d:Scopes>"); 
	offset += onvif_build_scopes_text(buff + offset, mlen - offset);
	offset += snprintf(buff + offset, mlen - offset, "</d:Scopes>");

	offset += snprintf(buff + offset, mlen - offset, 
		"<d:XAddrs>%s</d:XAddrs>"
		"<d:MetadataVersion>1</d:MetadataVersion></d:Hello></s:Body></s:Envelope>",
		onvif_get_service_addr(CapabilityCategory_Device, 0, 0, saddr, sizeof(saddr) - 1));

	log_print(HT_LOG_DBG, "%s, buff = %s\r\n", __FUNCTION__, buff);
	
	// send to multicast addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ONVIF_GRP_ADDR);
	addr.sin_port = htons(ONVIF_GRP_PORT);
	
	rlen = sendto(g_onvif_cls.discovery_fd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "onvif_hello::rlen = %d, slen = %d\r\n", rlen, offset);
	}
}

void onvif_bye()
{
	int rlen;
	int offset = 0;
	int mlen;
	char uuid[100] = {'\0'};
	char buff[1024 * 10];
	struct sockaddr_in addr;

	if (DiscoveryMode_NonDiscoverable == g_onvif_cfg.network.DiscoveryMode)
	{
		return;
	}

	mlen = sizeof(buff);

	offset += snprintf(buff + offset, mlen - offset,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:enc=\"http://www.w3.org/2003/05/soap-encoding\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:wsa5=\"http://www.w3.org/2005/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
		"xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" "
		"xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
		"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">");

	offset += snprintf(buff + offset, mlen - offset, "<s:Header>"
		"<wsa:MessageID>uuid:%s</wsa:MessageID>"
		"<wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To>"
		"<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Bye</wsa:Action>"
		"</s:Header>", onvif_uuid_create(uuid, sizeof(uuid)));

	offset += snprintf(buff + offset, mlen - offset, "<s:Body><d:Bye>"
		"<wsa:EndpointReference>"
			"<wsa:Address>urn:uuid:%s</wsa:Address>"
		"</wsa:EndpointReference>"
		"</d:Bye></s:Body></s:Envelope>",
		g_onvif_cfg.EndpointReference);

	log_print(HT_LOG_DBG, "%s, p_buf = %s\r\n", __FUNCTION__, buff);

	// send to multicast addr
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ONVIF_GRP_ADDR);
	addr.sin_port = htons(ONVIF_GRP_PORT);

	rlen = sendto(g_onvif_cls.discovery_fd, buff, offset, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rlen != offset)
	{
		log_print(HT_LOG_INFO, "onvif_hello::rlen = %d, slen = %d\r\n", rlen, offset);
	}
	return ;
}

void * onvif_discovery_thread(void * argv)
{
	onvif_hello();
	
	while (g_onvif_cls.discovery_flag)
	{
		onvif_probe_net_rx(g_onvif_cls.discovery_fd);

		usleep(20*1000);
	}

	g_onvif_cls.discovery_tid = 0;

	return NULL;
}

void onvif_start_discovery()
{
    if (gStreamCfg.webConfig.enable_onvif == 0)
    {
        log_print(HT_LOG_INFO, "onvif disable so device stop discovery!\n");
        return;
    }

	g_onvif_cls.discovery_fd = onvif_probe_init();
	if (g_onvif_cls.discovery_fd <= 0)
	{
		log_print(HT_LOG_INFO, "onvif_probe_init fd failed\r\n");
		return;
	}

	g_onvif_cls.discovery_flag = 1;
	g_onvif_cls.discovery_tid = sys_os_create_thread((void *)onvif_discovery_thread, NULL);
	return ;
}

void onvif_stop_discovery()
{
	g_onvif_cls.discovery_flag = 0;
	while (g_onvif_cls.discovery_tid != 0)
	{
		usleep(10*1000);
	}

	if (g_onvif_cls.discovery_fd > 0)
	{
		closesocket(g_onvif_cls.discovery_fd);
		g_onvif_cls.discovery_fd = 0;
	}
	return ;
}



