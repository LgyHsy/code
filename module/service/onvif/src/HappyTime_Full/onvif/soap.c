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
#include "onvif.h"
#include "http.h"
#include "httpd.h"
#include "http_srv.h"
#include "http_parse.h"
#include "soap.h"
#include "onvif_device.h"
#include "onvif_pkt.h"
#include "soap_parser.h"
#include "onvif_event.h"
#include "sha1.h"
#include "onvif_ptz.h"
#include "onvif_err.h"
#include "onvif_image.h"
#include "http_auth.h"
#include "base64.h"
#include "onvif_utils.h"
#include "onvif_probe.h"
#include "onvif_srv.h"
#include "cgi_handle.h"
#include "anj_config.h"
#ifdef MEDIA2_SUPPORT
#include "onvif_media2.h"
#endif

#ifdef PROFILE_G_SUPPORT
#include "onvif_recording.h"
#endif

#ifdef CREDENTIAL_SUPPORT
#include "onvif_credential.h"
#endif

#ifdef ACCESS_RULES
#include "onvif_accessrules.h"
#endif

#ifdef HTTPS
#include "openssl/ssl.h"
#endif


/***************************************************************************************/

extern ONVIF_CFG    g_onvif_cfg;
extern ONVIF_CLS    g_onvif_cls;
extern int is_Y_Version;

/***************************************************************************************/
char xml_hdr[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";

char onvif_xmlns[] = 
	"<s:Envelope "
	"xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" \r\n"
	"xmlns:e=\"http://www.w3.org/2003/05/soap-encoding\" \r\n"
	"xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" \r\n"    
	"xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" \r\n"
	"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" \r\n"
	"xmlns:wsaw=\"http://www.w3.org/2006/05/addressing/wsdl\" \r\n"
	"xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" \r\n" 
	"xmlns:wstop=\"http://docs.oasis-open.org/wsn/t-1\" \r\n"     
	"xmlns:wsntw=\"http://docs.oasis-open.org/wsn/bw-2\" \r\n"
	"xmlns:wsrf-rw=\"http://docs.oasis-open.org/wsrf/rw-2\" \r\n"
	"xmlns:wsrf-r=\"http://docs.oasis-open.org/wsrf/r-2\" \r\n"
	"xmlns:wsrf-bf=\"http://docs.oasis-open.org/wsrf/bf-2\" \r\n" 
	"xmlns:wsdl=\"http://schemas.xmlsoap.org/wsdl\" \r\n"
	"xmlns:wsoap12=\"http://schemas.xmlsoap.org/wsdl/soap12\" \r\n"
	"xmlns:http=\"http://schemas.xmlsoap.org/wsdl/http\" \r\n" 
	"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" \r\n"
	"xmlns:wsadis=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" \r\n"
	"xmlns:tt=\"http://www.onvif.org/ver10/schema\" \r\n" 
	"xmlns:tns1=\"http://www.onvif.org/ver10/topics\" \r\n"
	"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" \r\n" 
	"xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" \r\n"
	"xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" \r\n"    
	"xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" \r\n"
	"xmlns:tst=\"http://www.onvif.org/ver10/storage/wsdl\" \r\n"
	"xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\" \r\n"
	"xmlns:pt=\"http://www.onvif.org/ver10/pacs\" \r\n"

#ifdef MEDIA2_SUPPORT
	"xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" \r\n"
#endif

#ifdef PTZ_SUPPORT    
	"xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" \r\n"   
#endif   

#ifdef VIDEO_ANALYTICS
	"xmlns:tan=\"http://www.onvif.org/ver20/analytics/wsdl\" \r\n"
	"xmlns:axt=\"http://www.onvif.org/ver20/analytics\" \r\n"
#endif

#ifdef PROFILE_G_SUPPORT    
	"xmlns:trp=\"http://www.onvif.org/ver10/replay/wsdl\" \r\n"
	"xmlns:tse=\"http://www.onvif.org/ver10/search/wsdl\" \r\n"
	"xmlns:trc=\"http://www.onvif.org/ver10/recording/wsdl\" \r\n"    
#endif

#ifdef PROFILE_C_SUPPORT
	"xmlns:tac=\"http://www.onvif.org/ver10/accesscontrol/wsdl\" \r\n"
	"xmlns:tdc=\"http://www.onvif.org/ver10/doorcontrol/wsdl\" \r\n"
#endif

#ifdef DEVICEIO_SUPPORT
	"xmlns:tmd=\"http://www.onvif.org/ver10/deviceIO/wsdl\" \r\n"
#endif

#ifdef THERMAL_SUPPORT
	"xmlns:tth=\"http://www.onvif.org/ver10/thermal/wsdl\" \r\n"
#endif

#ifdef CREDENTIAL_SUPPORT
	"xmlns:tcr=\"http://www.onvif.org/ver10/credential/wsdl\" \r\n"
#endif

#ifdef ACCESS_RULES
	"xmlns:tar=\"http://www.onvif.org/ver10/accessrules/wsdl\" \r\n"
#endif

#ifdef SCHEDULE_SUPPORT
	"xmlns:tsc=\"http://www.onvif.org/ver10/schedule/wsdl\" \r\n"
#endif

#ifdef RECEIVER_SUPPORT
	"xmlns:trv=\"http://www.onvif.org/ver10/receiver/wsdl\" \r\n"
#endif

#ifdef PROVISIONING_SUPPORT
	"xmlns:tpv=\"http://www.onvif.org/ver10/provisioning/wsdl\" \r\n"
#endif

	"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" \r\n"
	"xmlns:ter=\"http://www.onvif.org/ver10/error\" \r\n"
	"xmlns:ewsd=\"http://www.onvifext.com/onvif/ext/ver10/wsdl\" \r\n"
	"xmlns:exsd=\"http://www.onvifext.com/onvif/ext/ver10/schema\" \r\n"
	"xmlns:tnshik=\"http://www.hikvision.com/2011/event/topics\" \r\n"
	"xmlns:tplt=\"http://www.onvif.org/ver10/plus/schema\" \r\n"
	"xmlns:tpl=\"http://www.onvif.org/ver10/plus/wsdl\" \r\n"
	"xmlns:hikwsd=\"http://www.onvifext.com/onvif/ext/ver10/wsdl\" \r\n"
	"xmlns:hikxsd=\"http://www.onvifext.com/onvif/ext/ver10/schema\">\r\n";

char soap_head[] = 
	"<s:Header>\r\n"
		"<wsa:Action>%s</wsa:Action>\r\n"
	"</s:Header>\r\n";

char soap_body[] = 
	"<s:Body>\r\n";

char soap_tailer[] =
	"</s:Body>\r\n</s:Envelope>\r\n";


/***************************************************************************************/
int soap_http_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, const char * p_xml, int len)
{
	int slen;
	int offset;
	char bufs[2048] = {0};

	offset = snprintf(bufs, sizeof(bufs), 
				"HTTP/1.1 200 OK\r\n"
				"Server: Happytime onvif server %s\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: %d\r\n"
				"Connection: %s\r\n\r\n",
				ONVIF_VERSION_STRING, 
				http_get_headline(rx_msg, "Content-Type") ? http_get_headline(rx_msg, "Content-Type") : "application/soap+xml", 
				len,
				p_user->keep_alive ? "keep-alive" : "close");

	log_print(HT_LOG_DBG, "TX >> %s\r\n", bufs);

	slen = http_srv_cln_tx(p_user, bufs, offset);
	if (slen != offset)
	{
		return -1;
	}

	if (p_xml && len > 0)
	{
		log_print(HT_LOG_DBG, "%s\r\n", p_xml);

		slen = http_srv_cln_tx(p_user, p_xml, len);
		if (slen != len)
		{
			return -1;
		}
	}
	
	return len;
}

int soap_http_err_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, int err_code, const char * err_str, const char * p_xml, int len)
{
	int slen;
	int offset = 0;
	char bufs[2048] = {0};
	char auth[1024] = {'\0'};
	HD_AUTH_INFO * p_auth = &g_onvif_cls.onvif_auth;
	MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

	if (pStreamCfg && pStreamCfg->webConfig.onvif_auth)
	{
		if (p_auth->auth_nonce[0] == '\0')
		{
			sprintf(p_auth->auth_nonce, "%08X%08X", rand(), rand());
			strcpy(p_auth->auth_qop, "auth");
			strcpy(p_auth->auth_realm, "happytimesoft");
		}

		if (g_onvif_cfg.md5_hashing)
		{
			offset = snprintf(auth, sizeof(auth), "WWW-Authenticate: Digest algorithm=MD5,realm=\"%s\",qop=\"%s\",nonce=\"%s\"\r\n", 
				p_auth->auth_realm, p_auth->auth_qop, p_auth->auth_nonce);
		}

		if (g_onvif_cfg.sha256_hashing)
		{
			snprintf(auth+offset, sizeof(auth)-offset-1, "WWW-Authenticate: Digest algorithm=SHA-256,realm=\"%s\",qop=\"%s\",nonce=\"%s\"\r\n", 
				p_auth->auth_realm, p_auth->auth_qop, p_auth->auth_nonce);
		}
	}

	offset = snprintf(bufs,	sizeof(bufs), 
				"HTTP/1.1 %d %s\r\n"
				"Server: Happytime onvif server %s\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: %d\r\n"
				"%s"
				"Connection: %s\r\n\r\n",
				err_code, err_str, 
				ONVIF_VERSION_STRING, 
				http_get_headline(rx_msg, "Content-Type") ? http_get_headline(rx_msg, "Content-Type") : "application/soap+xml", 
				len, 
				auth,
				p_user->keep_alive ? "keep-alive" : "close");

	log_print(HT_LOG_DBG, "TX >> %s\r\n", bufs);

	slen = http_srv_cln_tx(p_user, bufs, offset);
	if (slen != offset)
	{
		return -1;
	}

	if (p_xml && len > 0)
	{
		log_print(HT_LOG_DBG, "%s\r\n", p_xml);

		slen = http_srv_cln_tx(p_user, p_xml, len);
		if (slen != len)
		{
			return -1;
		}
	}
	
	return len;
}

int soap_err_rly
(
HTTPCLN * p_user, 
HTTPMSG * rx_msg, 
const char * code, 
const char * subcode, 
const char * subcode_ex,
const char * reason,
const char * action,
int http_err_code, 
const char * http_err_str
)
{
	int ret = -1, mlen = 1024*16, xlen;
	char * p_xml;

	log_print(HT_LOG_INFO, "%s, reason : %s\r\n", __FUNCTION__, reason);
	
	p_xml = (char *)malloc(mlen);
	if (NULL == p_xml)
	{
		goto soap_rly_err;
	}
	
	xlen = build_err_rly_xml(p_xml, mlen, code, subcode, subcode_ex, reason, action);
	if (xlen < 0 || xlen >= mlen)
	{
		goto soap_rly_err;
	}
	
	ret = soap_http_err_rly(p_user, rx_msg, http_err_code, http_err_str, p_xml, xlen);
	
soap_rly_err:

	if (p_xml)
	{
		free(p_xml);
	}
	
	return ret;
}

int soap_err_def_rly(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	return soap_err_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, NULL, "Action Not Implemented", NULL, 400, "Bad Request");
}

int soap_err_def2_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, const char * code, const char * subcode, const char * subcode_ex, const char * reason)
{
	return soap_err_rly(p_user, rx_msg, code, subcode, subcode_ex, reason, NULL, 400, "Bad Request");
}

int soap_err_def3_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, const char * code, const char * subcode, const char * subcode_ex, const char * reason, const char * action)
{
	return soap_err_rly(p_user, rx_msg, code, subcode, subcode_ex, reason, action, 400, "Bad Request");
}

int soap_security_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, int errcode)
{
	return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_NOTAUTHORIZED, NULL, "Sender not Authorized", NULL, errcode, "Not Authorized");
}

int soap_build_err_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, ONVIF_RET err)
{
	int ret = 0;
	
	switch (err)
	{
	case ONVIF_ERR_InvalidIPv4Address:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidIPv4Address", "Invalid IPv4 Address");
		break;

	case ONVIF_ERR_InvalidIPv6Address:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidIPv6Address", "Invalid IPv6 Address");	
		break;

	case ONVIF_ERR_InvalidDnsName:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter::InvalidDnsName", "Invalid DNS Name");	
		break;	

	case ONVIF_ERR_ServiceNotSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ServiceNotSupported", "Service Not Supported");	
		break;

	case ONVIF_ERR_PortAlreadyInUse:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:PortAlreadyInUse", "Port Already In Use");	
		break;	

	case ONVIF_ERR_InvalidGatewayAddress:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidGatewayAddress", "Invalid Gateway Address");	
		break;	

	case ONVIF_ERR_InvalidHostname:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidHostname", "Invalid Hostname");	
		break;	

	case ONVIF_ERR_MissingAttribute:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");	
		break;	

	case ONVIF_ERR_InvalidDateTime:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidDateTime", "Invalid Datetime");	
		break;		

	case ONVIF_ERR_InvalidTimeZone:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidTimeZone", "Invalid Timezone");	
		break;	

	case ONVIF_ERR_ProfileExists:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ProfileExists", "Profile Exist");	
		break;	

	case ONVIF_ERR_MaxNVTProfiles:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxNVTProfiles", "Max Profiles");
		break;

	case ONVIF_ERR_NoProfile:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoProfile", "Profile Not Exist");
		break;

	case ONVIF_ERR_DeletionOfFixedProfile:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:DeletionOfFixedProfile", "Deleting Fixed Profile");
		break;

	case ONVIF_ERR_NoConfig:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoConfig", "Config Not Exist");
		break;

	case ONVIF_ERR_NoPTZProfile:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoPTZProfile", "PTZ Profile Not Exist");
		break;	

	case ONVIF_ERR_NoHomePosition:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:NoHomePosition", "No Home Position");
		break;	

	case ONVIF_ERR_NoToken:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:NoToken", "The requested token does not exist.");
		break;	

	case ONVIF_ERR_PresetExist:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:PresetExist", "The requested name already exist for another preset.");
		break;

	case ONVIF_ERR_TooManyPresets:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:TooManyPresets", "Maximum number of Presets reached.");
		break;	

	case ONVIF_ERR_MovingPTZ:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:MovingPTZ", "Preset cannot be set while PTZ unit is moving.");
		break;

	case ONVIF_ERR_NoEntity:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoEntity", "No such PTZ Node on the device");
		break;	

	case ONVIF_ERR_InvalidNetworkInterface:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidNetworkInterface", "The supplied network interface token does not exist");
		break;	

	case ONVIF_ERR_InvalidMtuValue:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidMtuValue", "The MTU value is invalid");
		break;	

	case ONVIF_ERR_ConfigModify:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ConfigModify", "The configuration parameters are not possible to set");
		break;

	case ONVIF_ERR_ConfigurationConflict:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ConfigurationConflict", "The new settings conflicts with other uses of the configuration");
		break;

	case ONVIF_ERR_InvalidPosition:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidPosition", "Invalid Postion");
		break;	

	case ONVIF_ERR_TooManyScopes:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:TooManyScopes", "The requested scope list exceeds the supported number of scopes");
		break;

	case ONVIF_ERR_FixedScope:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:FixedScope", "Trying to Remove fixed scope parameter, command rejected");
		break;

	case ONVIF_ERR_NoScope:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoScope", "Trying to Remove scope which does not exist");
		break;

	case ONVIF_ERR_ScopeOverwrite:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:ScopeOverwrite", "Scope Overwrite");
		break;

	case ONVIF_ERR_ResourceUnknownFault:
		ret = soap_err_def3_rly(p_user, rx_msg, ERR_RECEIVER, "wsrf-rw:ResourceUnknownFault", NULL, "ResourceUnknownFault", "http://www.w3.org/2005/08/addressing/soap/fault");
		break;
		
	case ONVIF_ERR_NoSource:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoSource", "The requested VideoSource does not exist");
		break;

	case ONVIF_ERR_CannotOverwriteHome:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:CannotOverwriteHome", "The home position is fixed and cannot be overwritten");
		break;

	case ONVIF_ERR_SettingsInvalid:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:SettingsInvalid", "The requested settings are incorrect");
		break;

	case ONVIF_ERR_NoImagingForSource:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NoImagingForSource", "The requested VideoSource does not support imaging settings");
		break;

	case ONVIF_ERR_UsernameClash:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:UsernameClash", "Username already exists");
		break;
		
	case ONVIF_ERR_PasswordTooLong:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:PasswordTooLong", "The password is too long");
		break;
		
	case ONVIF_ERR_UsernameTooLong:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:UsernameTooLong", "The username is too long");
		break;
		
	case ONVIF_ERR_Password:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:Password", "Too weak password");
		break;
		
	case ONVIF_ERR_TooManyUsers:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:TooManyUsers", "Maximum number of supported users exceeded");
		break;
		
	case ONVIF_ERR_AnonymousNotAllowed:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:AnonymousNotAllowed", "User level anonymous is not allowed");
		break;
		
	case ONVIF_ERR_UsernameTooShort:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:UsernameTooShort", "The username is too short");
		break;
		
	case ONVIF_ERR_UsernameMissing:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:UsernameMissing", "Username not recognized");
		break;
		
	case ONVIF_ERR_FixedUser:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:FixedUser", "Fixed user");
		break;

	case ONVIF_ERR_MaxOSDs:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxOSDs", "The maximum number of supported OSDs has been reached");
		break;

	case ONVIF_ERR_InvalidStreamSetup:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidStreamSetup", "Specification of Stream Type or Transport part in StreamSetup is not supported");
		break;

	case ONVIF_ERR_BadConfiguration:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:BadConfiguration", "The configuration is invalid");
		break;
		
	case ONVIF_ERR_MaxRecordings:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxRecordings", "Max recordings");
		break;
		
	case ONVIF_ERR_NoRecording:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoRecording", "The RecordingToken does not reference an existing recording");
		break;
		
	case ONVIF_ERR_CannotDelete:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:CannotDelete", "Can not delete");
		break;
		
	case ONVIF_ERR_MaxTracks:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxTracks", "Max tracks");
		break;
		
	case ONVIF_ERR_NoTrack:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoTrack", "The TrackToken does not reference an existing track of the recording");
		break;
		
	case ONVIF_ERR_MaxRecordingJobs:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxRecordingJobs", "The maximum number of recording jobs that the device can handle has been reached");
		break;
		
	case ONVIF_ERR_MaxReceivers:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxReceivers", "Max receivers");
		break;
		
	case ONVIF_ERR_NoRecordingJob:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoRecordingJob", "The JobToken does not reference an existing job");
		break;
		
	case ONVIF_ERR_BadMode:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:BadMode", "The Mode is invalid");
		break;
		
	case ONVIF_ERR_InvalidToken:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidToken", "The Token is not valid");
		break;

	case ONVIF_ERR_InvalidRule:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidRule", "The suggested rules configuration is not valid on the device");
		break;
		
	case ONVIF_ERR_RuleAlreadyExistent:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:RuleAlreadyExistent", "The same rule name exists already in the configuration");
		break;
		
	case ONVIF_ERR_TooManyRules:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:TooManyRules", "There is not enough space in the device to add the rules to the configuration");
	break;
		
	case ONVIF_ERR_RuleNotExistent:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:RuleNotExistent", "The rule name or names do not exist");
		break;
		
	case ONVIF_ERR_NameAlreadyExistent:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NameAlreadyExistent", "The same analytics module name exists already in the configuration");
		break;
		
	case ONVIF_ERR_TooManyModules:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:TooManyModules", "There is not enough space in the device to add the analytics modules to the configuration");
		break;

	case ONVIF_ERR_InvalidModule:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidModule", "The suggested module configuration is not valid on the device");
		break;

	case ONVIF_ERR_NameNotExistent:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NameNotExistent", "The analytics module with the requested name does not exist");
		break;

	case ONVIF_ERR_InvalidFilterFault:
		ret = soap_err_def3_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "wsntw:InvalidFilterFault", "InvalidFilterFault", "http://www.w3.org/2005/08/addressing/soap/fault");
		break;

	case ONVIF_ERR_InvalidTopicExpressionFault:
		ret = soap_err_def3_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "wsntw:InvalidTopicExpressionFault", "InvalidTopicExpressionFault", "http://www.w3.org/2005/08/addressing/soap/fault");
		break;

	case ONVIF_ERR_TopicNotSupportedFault:
		ret = soap_err_def3_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "wsntw:TopicNotSupportedFault", "TopicNotSupportedFault", "http://www.w3.org/2005/08/addressing/soap/fault");
		break;

	case ONVIF_ERR_InvalidMessageContentExpressionFault:
		ret = soap_err_def3_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "wsntw:InvalidMessageContentExpressionFault", "InvalidMessageContentExpressionFault", "http://www.w3.org/2005/08/addressing/soap/fault");
		break;

	case ONVIF_ERR_InvalidStartReference:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter::InvalidStartReference", "StartReference is invalid");
		break;

	case ONVIF_ERR_TooManyItems:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter::TooManyItems", "Too many items were requested, see MaxLimit capability");
		break;

	case ONVIF_ERR_NotFound:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NotFound", "Not found");
		break;

	case ONVIF_ERR_NotSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NotSupported", "The operation is not supported");
		break;

	case ONVIF_ERR_Failure:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:Failure", "Failed to go to Accessed state and unlock the door");
		break;

	case ONVIF_ERR_NoVideoOutput:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoVideoOutput", "The requested VideoOutput indicated with VideoOutputToken does not exist");
		break;

	case ONVIF_ERR_NoAudioOutput:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoAudioOutput", "The requested AudioOutput indicated with AudioOutputToken does not exist");
		break;

	case ONVIF_ERR_RelayToken:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:RelayToken", "Unknown relay token reference");
		break;

	case ONVIF_ERR_ModeError:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ModeError", "Monostable delay time not valid");
		break;

	case ONVIF_ERR_InvalidSerialPort:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidSerialPort", "The supplied serial port token does not exist");
		break;

	case ONVIF_ERR_DataLengthOver:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:DataLengthOver", "Number of available bytes exceeded");
		break;

	case ONVIF_ERR_DelimiterNotSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:DelimiterNotSupported", "Sequence of character (delimiter) is not supported");
		break;

	case ONVIF_ERR_InvalidDot11:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:InvalidDot11", "IEEE 802.11 configuration is not supported");
		break;

	case ONVIF_ERR_NotDot11:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NotDot11", "The interface is not an IEEE 802.11 interface");
		break;

	case ONVIF_ERR_NotConnectedDot11:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:NotConnectedDot11", "IEEE 802.11 network is not connected");
		break;

	case ONVIF_ERR_NotScanAvailable:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NotScanAvailable", "ScanAvailableDot11Networks is not supported");
		break;

	case ONVIF_ERR_NotRemoteUser:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NotRemoteUser", "Remote User handling is not supported");
		break;

	case ONVIF_ERR_NoVideoSource:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoVideoSource", "The requested video source does not exist");
		break;

	case ONVIF_ERR_NoVideoSourceMode:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoVideoSourceMode", "The requested video source mode does not exist");
		break;

	case ONVIF_ERR_NoThermalForSource:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NoThermalForSource", "The requested VideoSource does not support thermal configuration");
		break;

	case ONVIF_ERR_NoRadiometryForSource:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NoRadiometryForSource", "The requested VideoSource does not support radiometry config settings");
		break;

	case ONVIF_ERR_InvalidConfiguration:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidConfiguration", "The requested configuration is incorrect");
		break;

	case ONVIF_ERR_MaxAccessProfilesPerCredential:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:MaxAccessProfilesPerCredential", "There are too many access profiles per credential");
		break;

	case ONVIF_ERR_CredentialValiditySupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:CredentialValiditySupported", "Credential validity is not supported by device");
		break;

	case ONVIF_ERR_CredentialAccessProfileValiditySupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:CredentialAccessProfileValiditySupported", "Credential access profile validity is not supported by the device");
		break;

	case ONVIF_ERR_SupportedIdentifierType:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:SupportedIdentifierType", "Specified identifier type is not supported by device");
		break;

	case ONVIF_ERR_DuplicatedIdentifierType:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:DuplicatedIdentifierType", "The same identifier type was used more than once");
		break;

	case ONVIF_ERR_InvalidFormatType:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidFormatType", "Specified identifier format type is not supported by the device");
		break;

	case ONVIF_ERR_InvalidIdentifierValue:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidIdentifierValue", "Specified identifier value is not as per FormatType definition");
		break;

	case ONVIF_ERR_DuplicatedIdentifierValue:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:DuplicatedIdentifierValue", "The same combination of identifier type, format and value was used more than once");
		break;

	case ONVIF_ERR_ReferenceNotFound:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ReferenceNotFound", "A referred entity token is not found");
		break;

	case ONVIF_ERR_ExemptFromAuthenticationSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ExemptFromAuthenticationSupported", "Exempt from authentication is not supported by the device");
		break;

	case ONVIF_ERR_MaxCredentials:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:MaxCredentials", "There is not enough space to create a new credential");
		break;

	case ONVIF_ERR_ReferenceInUse:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ReferenceInUse", "Failed to delete, credential token is in use");
		break;

	case ONVIF_ERR_MinIdentifiersPerCredential:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CONSTRAINTVIOLATED, "ter:MinIdentifiersPerCredential", "At least one credential identifier is required");
		break;

	case ONVIF_ERR_InvalidArgs:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGS, NULL, "InvalidArgs");
		break;

	case ONVIF_ERR_MaxAccessProfiles:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:MaxAccessProfiles", "There is not enough space to add new AccessProfile, see the MaxAccessProfiles capability");
		break;

	case ONVIF_ERR_MaxAccessPoliciesPerAccessProfile:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:MaxAccessPoliciesPerAccessProfile", "There are too many AccessPolicies in anAccessProfile, see MaxAccessPoliciesPerAccessProfile capability");
		break;

	case ONVIF_ERR_MultipleSchedulesPerAccessPointSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:MultipleSchedulesPerAccessPointSupported", "Multiple AccessPoints are not supported for the same schedule, see MultipleSchedulesPerAccessPointSupported capability");
		break;

	case ONVIF_ERR_InvalidArgVal:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, NULL, "InvalidArgVal");
		break;

	case ONVIF_ERR_MaxSchedules:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:MaxSchedules", "There is not enough space to add new schedule, see MaxSchedules capability");
		break;

	case ONVIF_ERR_MaxSpecialDaysSchedules:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:MaxSpecialDaysSchedule", "There are too many SpecialDaysSchedule entities referred in this schedule, see MaxSpecialDaysSchedules capability");
		break;

	case ONVIF_ERR_MaxTimePeriodsPerDay: 
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:MaxTimePeriodsPerDay", "There are too many time periods in a day schedule, see MaxTimePeriodsPerDay capability");
		break;

	case ONVIF_ERR_MaxSpecialDayGroups:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:MaxSpecialDayGroups", "There is not enough space to add new SpecialDayGroup items, see the MaxSpecialDayGroups capabilit");
		break;

	case ONVIF_ERR_MaxDaysInSpecialDayGroup:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_CAPABILITYVIOLATED, "ter:MaxDaysInSpecialDayGroup", "There are too many special days in a SpecialDayGroup, see MaxDaysInSpecialDayGroup capability");
		break;

	case ONVIF_ERR_UnknownToken:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:UnknownToken", "The receiver indicated by ReceiverToken does not exist");
		break;

	case ONVIF_ERR_CannotDeleteReceiver:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:CannotDeleteReceiver", "It is not possible to delete the specified receiver");
		break;

	case ONVIF_ERR_MaxMasks:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxMasks", "The maximum number of supported masks by the specific VideoSourceConfiguration has been reached");
		break;

	case ONVIF_ERR_IPFilterListIsFull:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:IPFilterListIsFull", "It is not possible to add more IP filters since the IP filter list is full");
		break;

	case ONVIF_ERR_NoIPv4Address:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoIPv4Address", "The IPv4 address to be removed does not exist. ");
		break;

	case ONVIF_ERR_NoIPv6Address:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoIPv6Address", "Cannot set position automatically");
		break;

	case ONVIF_ERR_NoProvisioning:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTIONNOTSUPPORTED, "ter:NoProvisioning", "Provisioning is not supported for this operation on the given video source");
		break;

	case ONVIF_ERR_NoAutoMode:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTIONNOTSUPPORTED, "ter:NoAutoMode", "Cannot set position automatically");
		break;

	case ONVIF_ERR_TooManyPresetTours:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:TooManyPresetTours", "There is not enough space in the device to create the new preset tour to the profile");
		break;

	case ONVIF_ERR_InvalidPresetTour:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidPresetTour", "The requested PresetTour includes invalid parameter(s)");
		break;

	case ONVIF_ERR_SpaceNotSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:SpaceNotSupported", "A space is referenced in an argument which is not supported by the PTZ Node");
		break;

	case ONVIF_ERR_ActivationFailed:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_INVALIDARGVAL, "ter:ActivationFailed", "The requested preset tour cannot be activated while PTZ unit is moving or another preset tour is now activated");
		break;

	case ONVIF_ERR_MaxDoors:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:MaxDoors", "There is not enough space to add the new door");
		break;

	case ONVIF_ERR_ClientSuppliedTokenSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:ClientSuppliedTokenSupported", "The device does not support that the client supplies the token");
		break;

	case ONVIF_ERR_GeoMoveNotSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:GeoMoveNotSupported", "The device does not support geo move");
		break;

	case ONVIF_ERR_UnreachablePosition:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:UnreachablePosition", "The requested translation is out of bounds");
		break;

	case ONVIF_ERR_TimeoutNotSupported:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:TimeoutNotSupported", "The specified timeout argument is not within the supported timeout range");
		break;

	case ONVIF_ERR_GeoLocationUnknown:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:GeoLocationUnknown", "The unit is not able to perform GeoMove because its geolocation is not configured or available");
		break;

	case ONVIF_ERR_InvalidSpeed:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidSpeed", "The requested speed is out of bounds");
		break;

	case ONVIF_ERR_InvalidTranslation:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidTranslation", "The requested translation is out of bounds");
		break;

	case ONVIF_ERR_InvalidVelocity:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidVelocity", "The requested speed is out of bounds");
		break;

	case ONVIF_ERR_NoStatus:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:NoStatus", "Status is unavailable in the requested Media Profile");
		break;

	case ONVIF_ERR_AccesslogUnavailable:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGS, "ter:AccesslogUnavailable", "There is no access log information available");
		break;

	case ONVIF_ERR_SystemlogUnavailable:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGS, "ter:SystemlogUnavailable", "There is no system log information available");
		break;

	case ONVIF_ERR_NtpServerUndefined:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NtpServerUndefined", "Cannot switch DateTimeType to NTP because no NTP server is defined");
		break;

	case ONVIF_ERR_InvalidInterfaceSpeed:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidInterfaceSpeed", "The suggested speed is not supported");
		break;

	case ONVIF_ERR_InvalidInterfaceType:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidInterfaceType", "The suggested network interface type is not supported");
		break;

	case ONVIF_ERR_StreamConflict:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:StreamConflict", "Specification of StreamType or Transport part in causes conflict with other streams");
		break;

	case ONVIF_ERR_IncompleteConfiguration:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:IncompleteConfiguration", "The specified media profile does not have the minimum amount of configurations to have streams");
		break;

	case ONVIF_ERR_InvalidMulticastSettings:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidMulticastSettings", "No configuration is configured for multicast");
		break;

	case ONVIF_ERR_InvalidPolygon:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_INVALIDARGVAL, "ter:InvalidPolygon", "The provided polygon is not supported");
		break;

	case ONVIF_ERR_MaxAccessPoints:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:MaxAccessPoints", "There is not enough space to add new AccessPoint");
		break;

	case ONVIF_ERR_MaxAreas:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CAPABILITYVIOLATED, "ter:MaxAreas", "There is not enough space to add new Area");
		break;

	case ONVIF_ERR_TypeNotExistent:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:TypeNotExistent", "The module option type does not exist");
		break;

	case ONVIF_ERR_TooManyEntries:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:TooManyEntries", "The requested geo location list exceeds the supported number of entries");
		break;

	case ONVIF_ERR_NoAutoGeo:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoAutoGeo", "The device does not support automatic retrieval of geo information");
		break;

	case ONVIF_ERR_Fixed:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:Fixed", "Cannot delete a fixed entity");
		break;

	case ONVIF_ERR_GENERAL:
		ret = soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_CRITICALERROR, NULL, "The device has encountered an error");
		break;

	default:
		ret = soap_err_def_rly(p_user, rx_msg);
		break;
	}

	return ret;
}

int soap_build_header(char * p_xml, int mlen, const char * action, XMLN * p_header)
{
	int offset = 0;

	offset += snprintf(p_xml, mlen, "%s", xml_hdr);
	offset += snprintf(p_xml+offset, mlen-offset, "%s", onvif_xmlns);

	if (p_header)
	{
		XMLN * p_MessageID;
		XMLN * p_ReplyTo;

		offset += snprintf(p_xml+offset, mlen-offset, "<s:Header>\r\n");

		p_MessageID = xml_node_soap_get(p_header, "MessageID");
		if (p_MessageID && p_MessageID->data)
		{
			offset += snprintf(p_xml+offset, mlen-offset, "<wsa:MessageID>%s</wsa:MessageID>\r\n",p_MessageID->data);
		}

		p_ReplyTo = xml_node_soap_get(p_header, "ReplyTo");
		if (p_ReplyTo)
		{
			XMLN * p_Address;

			p_Address = xml_node_soap_get(p_ReplyTo, "Address");
			if (p_Address && p_Address->data)
			{
				offset += snprintf(p_xml+offset, mlen-offset, "<wsa:To>%s</wsa:To>\r\n",p_Address->data);
			}
		}

		if (action)
		{
			offset += snprintf(p_xml+offset, mlen-offset, "<wsa:Action>%s</wsa:Action>\r\n", action);
		}

		offset += snprintf(p_xml+offset, mlen-offset, "</s:Header>\r\n");
	}
	else if (action)
	{
		offset += snprintf(p_xml+offset, mlen-offset, soap_head, action);
	}

	return offset;
}

ONVIF_RET soap_build_send_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, ONVIF_RET ret, soap_build_xml build_xml, const char * argv, const char * action, XMLN * p_header)
{
	int offset = 0;
	int slen = -1, mlen = 1024*40, xlen;

	if (ONVIF_OK != ret)
	{
		slen = soap_build_err_rly(p_user, rx_msg, ret);
		if (slen <= 0)
		{
			return ONVIF_ERR_GENERAL;
		}
		else
		{
			return ONVIF_OK;
		}
	}

	// todo : The default maximum message buffer size is 40KB, 
	//  which supports a maximum of 10 profiles. 
	//  If you need to support more profiles, please modify mlen to a larger value

	char * p_xml = (char *)malloc(mlen);
	if (NULL == p_xml)
	{
		return ONVIF_ERR_GENERAL;
	}

	offset += soap_build_header(p_xml, mlen, action, p_header);

	offset += snprintf(p_xml+offset, mlen-offset, "%s", soap_body);

	xlen = build_xml(p_user, p_xml+offset, mlen-offset, argv);
	if (xlen < 0)
	{
		slen = soap_build_err_rly(p_user, rx_msg, (ONVIF_RET)xlen);
		if (slen <= 0)
		{
			slen = ONVIF_ERR_GENERAL;
		}
	}
	else
	{
		offset += xlen;
		offset += snprintf(p_xml+offset, mlen-offset, "%s", soap_tailer);

		slen = soap_http_rly(p_user, rx_msg, p_xml, offset);
		if (slen != offset)
		{
			slen = ONVIF_ERR_GENERAL;
		}
	}

	free(p_xml);
	
	return slen >= 0 ? ONVIF_OK : (ONVIF_RET)slen;
}

/***************************************************************************************/

int soap_tds_GetDeviceInformation(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetDeviceInformation_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetSystemUris(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	tds_GetSystemUris_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&res, 0, sizeof(res));

	ret = onvif_tds_GetSystemUris(p_user, &res);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_GetSystemUris_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tds_GetCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCapabilities;
	tds_GetCapabilities_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetCapabilities = xml_node_soap_get(p_body, "GetCapabilities");
	assert(p_GetCapabilities);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_GetCapabilities(p_GetCapabilities, &req);
	if (ONVIF_OK == ret)
	{
		if (CapabilityCategory_Invalid       == req.Category 
			|| (CapabilityCategory_Events    == req.Category && g_onvif_cfg.Capabilities.events.support == 0)
#ifdef PTZ_SUPPORT
			|| (CapabilityCategory_PTZ       == req.Category && g_onvif_cfg.Capabilities.ptz.support == 0)
#else
			|| (CapabilityCategory_PTZ       == req.Category) 
#endif
#ifdef MEDIA_SUPPORT
			|| (CapabilityCategory_Media     == req.Category && g_onvif_cfg.Capabilities.media.support == 0)
#else
			|| (CapabilityCategory_Media     == req.Category)
#endif
#ifdef IMAGE_SUPPORT
			|| (CapabilityCategory_Imaging   == req.Category && g_onvif_cfg.Capabilities.image.support == 0)
#else
			|| (CapabilityCategory_Imaging   == req.Category)
#endif
#ifdef VIDEO_ANALYTICS
			|| (CapabilityCategory_Analytics == req.Category && g_onvif_cfg.Capabilities.analytics.support == 0)
#else
			|| (CapabilityCategory_Analytics == req.Category)
#endif
			)
		{
			return soap_err_def2_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NoSuchService", "No Such Service");
		}
	}
	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_GetCapabilities_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tds_GetSystemDateAndTime(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetSystemDateAndTime_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetNetworkInterfaces(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetNetworkInterfaces_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetNetworkInterfaces(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetNetworkInterfaces;
	tds_SetNetworkInterfaces_REQ req;
	tds_SetNetworkInterfaces_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_SetNetworkInterfaces = xml_node_soap_get(p_body, "SetNetworkInterfaces");
	assert(p_SetNetworkInterfaces);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tds_SetNetworkInterfaces(p_SetNetworkInterfaces, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetNetworkInterfaces(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetNetworkInterfaces_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tds_SystemReboot(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	
	log_print(HT_LOG_DBG, "\n");

	ret = soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_SystemReboot_rly_xml, NULL, NULL, p_header);

	onvif_tds_SystemReboot();

	return ret;
}

int soap_tds_SetSystemFactoryDefault(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetSystemFactoryDefault;
	tds_SetSystemFactoryDefault_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetSystemFactoryDefault = xml_node_soap_get(p_body, "SetSystemFactoryDefault");
	assert(p_SetSystemFactoryDefault);	

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetSystemFactoryDefault(p_SetSystemFactoryDefault, &req);

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetSystemFactoryDefault_rly_xml, NULL, NULL, p_header);

	onvif_tds_SetSystemFactoryDefault(&req);

	return ret;
}

int soap_tds_GetSystemLog(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSystemLog;
	tds_GetSystemLog_REQ req;
	tds_GetSystemLog_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetSystemLog = xml_node_soap_get(p_body, "GetSystemLog");
	assert(p_GetSystemLog);    

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tds_GetSystemLog(p_GetSystemLog, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_GetSystemLog(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_GetSystemLog_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tds_SetSystemDateAndTime(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetSystemDateAndTime;
	tds_SetSystemDateAndTime_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetSystemDateAndTime = xml_node_soap_get(p_body, "SetSystemDateAndTime");
	assert(p_SetSystemDateAndTime);

	memset(&req, 0, sizeof(tds_SetSystemDateAndTime_REQ));

	ret = parse_tds_SetSystemDateAndTime(p_SetSystemDateAndTime, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetSystemDateAndTime(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetSystemDateAndTime_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetServices(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetServices;	
	tds_GetServices_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetServices = xml_node_soap_get(p_body, "GetServices");
	assert(p_GetServices);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_GetServices(p_GetServices, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_GetServices_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tds_GetScopes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetScopes_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_AddScopes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddScopes;
	tds_AddScopes_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_AddScopes = xml_node_soap_get(p_body, "AddScopes");
	assert(p_AddScopes);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_AddScopes(p_AddScopes, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_AddScopes(&req);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tds_AddScopes_rly_xml, NULL, NULL, p_header);
	if (ONVIF_OK == ret)
	{
		onvif_hello();
	}
		
	return ret;
}

int soap_tds_SetScopes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetScopes;
	tds_SetScopes_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetScopes = xml_node_soap_get(p_body, "SetScopes");
	assert(p_SetScopes);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetScopes(p_SetScopes, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetScopes(&req);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetScopes_rly_xml, NULL, NULL, p_header);
	if (ONVIF_OK == ret)
	{
		onvif_hello();
	}

	return ret;
}

int soap_tds_RemoveScopes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveScopes;
	tds_RemoveScopes_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_RemoveScopes = xml_node_soap_get(p_body, "RemoveScopes");
	assert(p_RemoveScopes);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_RemoveScopes(p_RemoveScopes, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_RemoveScopes(&req);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tds_RemoveScopes_rly_xml, (char *)&req, NULL, p_header);
	if (ONVIF_OK == ret)
	{
		onvif_hello();
	}

	return ret;
}

int soap_tds_GetHostname(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetHostname_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_SetHostname(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetHostname;
	tds_SetHostname_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetHostname = xml_node_soap_get(p_body, "SetHostname");
	assert(p_SetHostname);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetHostname(p_SetHostname, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetHostname(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetHostname_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetHostnameFromDHCP(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetHostnameFromDHCP;
	tds_SetHostnameFromDHCP_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetHostnameFromDHCP = xml_node_soap_get(p_body, "SetHostnameFromDHCP");
	assert(p_SetHostnameFromDHCP);

	ret = parse_tds_SetHostnameFromDHCP(p_SetHostnameFromDHCP, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetHostnameFromDHCP(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetHostnameFromDHCP_rly_xml, NULL, NULL, p_header);
}


int soap_tds_GetNetworkProtocols(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetNetworkProtocols_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_SetNetworkProtocols(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetNetworkProtocols;
	tds_SetNetworkProtocols_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetNetworkProtocols = xml_node_soap_get(p_body, "SetNetworkProtocols");
	assert(p_SetNetworkProtocols);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetNetworkProtocols(p_SetNetworkProtocols, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetNetworkProtocols(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetNetworkProtocols_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_GetNetworkDefaultGateway(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetNetworkDefaultGateway_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_SetNetworkDefaultGateway(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetNetworkDefaultGateway;
	tds_SetNetworkDefaultGateway_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetNetworkDefaultGateway = xml_node_soap_get(p_body, "SetNetworkDefaultGateway");
	assert(p_SetNetworkDefaultGateway);

	memset(&req, 0, sizeof(tds_SetNetworkDefaultGateway_REQ));

	ret = parse_tds_SetNetworkDefaultGateway(p_SetNetworkDefaultGateway, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetNetworkDefaultGateway(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetNetworkDefaultGateway_rly_xml, NULL, NULL, p_header);    	
}

int soap_tds_GetDiscoveryMode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetDiscoveryMode_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_SetDiscoveryMode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetDiscoveryMode;
	tds_SetDiscoveryMode_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetDiscoveryMode = xml_node_soap_get(p_body, "SetDiscoveryMode");
	assert(p_SetDiscoveryMode);
	
	memset(&req, 0, sizeof(tds_SetDiscoveryMode_REQ));

	ret = parse_tds_SetDiscoveryMode(p_SetDiscoveryMode, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetDiscoveryMode(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetDiscoveryMode_rly_xml, NULL, NULL, p_header); 	
}

int soap_tds_GetDNS(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetDNS_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_SetDNS(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetDNS;
	tds_SetDNS_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetDNS = xml_node_soap_get(p_body, "SetDNS");
	assert(p_SetDNS);

	memset(&req, 0, sizeof(tds_SetDNS_REQ));

	ret = parse_tds_SetDNS(p_SetDNS, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetDNS(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetDNS_rly_xml, NULL, NULL, p_header);    
}

int soap_tds_GetDynamicDNS(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetDynamicDNS_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetDynamicDNS(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetDynamicDNS;
	tds_SetDynamicDNS_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetDynamicDNS = xml_node_soap_get(p_body, "SetDynamicDNS");
	assert(p_SetDynamicDNS);

	memset(&req, 0, sizeof(tds_SetDynamicDNS_REQ));

	ret = parse_tds_SetDynamicDNS(p_SetDynamicDNS, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetDynamicDNS(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetDynamicDNS_rly_xml, NULL, NULL, p_header);     
}

int soap_tds_GetNTP(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetNTP_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_SetNTP(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetNTP;
	tds_SetNTP_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetNTP = xml_node_soap_get(p_body, "SetNTP");
	assert(p_SetNTP);

	memset(&req, 0, sizeof(tds_SetNTP_REQ));

	ret = parse_tds_SetNTP(p_SetNTP, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetNTP(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetNTP_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_GetZeroConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetZeroConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetZeroConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetZeroConfiguration;
	tds_SetZeroConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetZeroConfiguration = xml_node_soap_get(p_body, "SetZeroConfiguration");
	assert(p_SetZeroConfiguration);

	memset(&req, 0, sizeof(tds_SetZeroConfiguration_REQ));

	ret = parse_tds_SetZeroConfiguration(p_SetZeroConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetZeroConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetZeroConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Device;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header);
}

int soap_tds_GetWsdlUrl(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetWsdlUrl_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetEndpointReference(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetEndpointReference_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetUsers(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
	
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetUsers_rly_xml, NULL, NULL, p_header);
}

int soap_tds_CreateUsers(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateUsers;
	tds_CreateUsers_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateUsers = xml_node_soap_get(p_body, "CreateUsers");
	assert(p_CreateUsers);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_CreateUsers(p_CreateUsers, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_CreateUsers(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_CreateUsers_rly_xml, NULL, NULL, p_header);
}
	
int soap_tds_DeleteUsers(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteUsers;
	tds_DeleteUsers_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_DeleteUsers = xml_node_soap_get(p_body, "DeleteUsers");
	assert(p_DeleteUsers);
	
	memset(&req, 0, sizeof(req));

	ret = parse_tds_DeleteUsers(p_DeleteUsers, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_DeleteUsers(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_DeleteUsers_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetUser(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetUser;
	tds_SetUser_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetUser = xml_node_soap_get(p_body, "SetUser");
	assert(p_SetUser);
	
	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetUser(p_SetUser, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetUser(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetUser_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetRemoteUser(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	tds_GetRemoteUser_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&res, 0, sizeof(res));

	ret = onvif_tds_GetRemoteUser(&res);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_GetRemoteUser_rly_xml, (char *)&res, NULL, p_header);    	
}

int soap_tds_SetRemoteUser(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRemoteUser;
	tds_SetRemoteUser_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetRemoteUser = xml_node_soap_get(p_body, "SetRemoteUser");
	assert(p_SetRemoteUser);
	
	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetRemoteUser(p_SetRemoteUser, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetRemoteUser(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetRemoteUser_rly_xml, NULL, NULL, p_header);
}

int soap_tds_UpgradeSystemFirmware(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	(void)p_body;
	(void)p_header;
	return 0;
}

int soap_tds_StartFirmwareUpgrade(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	tds_StartFirmwareUpgrade_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	memset(&res, 0, sizeof(res));
	
	ret = onvif_tds_StartFirmwareUpgrade(p_user, &res);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_StartFirmwareUpgrade_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tds_StartSystemRestore(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	tds_StartSystemRestore_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&res, 0, sizeof(res));

	ret = onvif_tds_StartSystemRestore(p_user, &res);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_StartSystemRestore_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tds_SetHashingAlgorithm(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetHashingAlgorithm;
	tds_SetHashingAlgorithm_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetHashingAlgorithm = xml_node_soap_get(p_body, "SetHashingAlgorithm");

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetHashingAlgorithm(p_SetHashingAlgorithm, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetHashingAlgorithm(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetHashingAlgorithm_rly_xml, NULL, NULL, p_header);
}

#ifdef DEVICEIO_SUPPORT

int soap_tds_GetRelayOutputs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetRelayOutputs_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetRelayOutputSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRelayOutputSettings;
	tmd_SetRelayOutputSettings_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetRelayOutputSettings = xml_node_soap_get(p_body, "SetRelayOutputSettings");
	assert(p_SetRelayOutputSettings);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetRelayOutputSettings(p_SetRelayOutputSettings, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetRelayOutputSettings(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetRelayOutputSettings_rly_xml, NULL, NULL, p_header);	
}

int soap_tds_SetRelayOutputState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRelayOutputState;
	tmd_SetRelayOutputState_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetRelayOutputState = xml_node_soap_get(p_body, "SetRelayOutputState");
	assert(p_SetRelayOutputState);

	memset(&req, 0, sizeof(req));

	ret = parse_tmd_SetRelayOutputState(p_SetRelayOutputState, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetRelayOutputState(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetRelayOutputState_rly_xml, NULL, NULL, p_header);
}

#endif // DEVICEIO_SUPPORT

#ifdef IPFILTER_SUPPORT	

int soap_tds_GetIPAddressFilter(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
	
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetIPAddressFilter_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetIPAddressFilter(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetIPAddressFilter;
	tds_SetIPAddressFilter_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetIPAddressFilter = xml_node_soap_get(p_body, "SetIPAddressFilter");
	assert(p_SetIPAddressFilter);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetIPAddressFilter(p_SetIPAddressFilter, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetIPAddressFilter(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetIPAddressFilter_rly_xml, NULL, NULL, p_header);
}

int soap_tds_AddIPAddressFilter(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddIPAddressFilter;
	tds_AddIPAddressFilter_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_AddIPAddressFilter = xml_node_soap_get(p_body, "AddIPAddressFilter");
	assert(p_AddIPAddressFilter);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_AddIPAddressFilter(p_AddIPAddressFilter, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_AddIPAddressFilter(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_AddIPAddressFilter_rly_xml, NULL, NULL, p_header);
}

int soap_tds_RemoveIPAddressFilter(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveIPAddressFilter;
	tds_RemoveIPAddressFilter_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveIPAddressFilter = xml_node_soap_get(p_body, "RemoveIPAddressFilter");
	assert(p_RemoveIPAddressFilter);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_RemoveIPAddressFilter(p_RemoveIPAddressFilter, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_RemoveIPAddressFilter(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_RemoveIPAddressFilter_rly_xml, NULL, NULL, p_header);
}

#endif // end of IPFILTER_SUPPORT

#ifdef STORAGE_SUPPORT

int soap_tds_GetStorageConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetStorageConfigurations_rly_xml, NULL, NULL, p_header); 
}

int soap_tds_GetStorageConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetStorageConfiguration;	
	tds_GetStorageConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetStorageConfiguration = xml_node_soap_get(p_body, "GetStorageConfiguration");
	assert(p_GetStorageConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_GetStorageConfiguration(p_GetStorageConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_GetStorageConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tds_CreateStorageConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateStorageConfiguration;
	tds_CreateStorageConfiguration_REQ req;
	tds_CreateStorageConfiguration_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_CreateStorageConfiguration = xml_node_soap_get(p_body, "CreateStorageConfiguration");
	assert(p_CreateStorageConfiguration);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tds_CreateStorageConfiguration(p_CreateStorageConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_CreateStorageConfiguration(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_CreateStorageConfiguration_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tds_SetStorageConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetStorageConfiguration;
	tds_SetStorageConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetStorageConfiguration = xml_node_soap_get(p_body, "SetStorageConfiguration");
	assert(p_SetStorageConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetStorageConfiguration(p_SetStorageConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetStorageConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetStorageConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tds_DeleteStorageConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteStorageConfiguration;
	tds_DeleteStorageConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteStorageConfiguration = xml_node_soap_get(p_body, "DeleteStorageConfiguration");
	assert(p_DeleteStorageConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_DeleteStorageConfiguration(p_DeleteStorageConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_DeleteStorageConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_DeleteStorageConfiguration_rly_xml, NULL, NULL, p_header);
}

#endif // STORAGE_SUPPORT

#ifdef GEOLOCATION_SUPPORT

int soap_tds_GetGeoLocation(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetGeoLocation_rly_xml, NULL, NULL, p_header);
}

int soap_tds_SetGeoLocation(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetGeoLocation;
	tds_SetGeoLocation_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetGeoLocation = xml_node_soap_get(p_body, "SetGeoLocation");
	assert(p_SetGeoLocation);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_SetGeoLocation(p_SetGeoLocation, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_SetGeoLocation(&req);

		onvif_free_LocationEntitis(&req.Location);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_SetGeoLocation_rly_xml, NULL, NULL, p_header);
}

int soap_tds_DeleteGeoLocation(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteGeoLocation;
	tds_DeleteGeoLocation_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteGeoLocation = xml_node_soap_get(p_body, "DeleteGeoLocation");
	assert(p_DeleteGeoLocation);

	memset(&req, 0, sizeof(req));

	ret = parse_tds_DeleteGeoLocation(p_DeleteGeoLocation, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_DeleteGeoLocation(&req);

		onvif_free_LocationEntitis(&req.Location);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_DeleteGeoLocation_rly_xml, NULL, NULL, p_header);
}

#endif // GEOLOCATION_SUPPORT

#ifdef DOT11_SUPPORT

int soap_tds_GetDot11Capabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tds_GetDot11Capabilities_rly_xml, NULL, NULL, p_header);
}

int soap_tds_GetDot11Status(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetDot11Status;
	tds_GetDot11Status_REQ req;
	tds_GetDot11Status_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetDot11Status = xml_node_soap_get(p_body, "GetDot11Status");
	assert(p_GetDot11Status);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tds_GetDot11Status(p_GetDot11Status, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_GetDot11Status(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_GetDot11Status_rly_xml, (char *)&res, NULL, p_header); 
}

int soap_tds_ScanAvailableDot11Networks(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ScanAvailableDot11Networks;
	tds_ScanAvailableDot11Networks_REQ req;
	tds_ScanAvailableDot11Networks_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_ScanAvailableDot11Networks = xml_node_soap_get(p_body, "ScanAvailableDot11Networks");
	assert(p_ScanAvailableDot11Networks);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tds_ScanAvailableDot11Networks(p_ScanAvailableDot11Networks, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tds_ScanAvailableDot11Networks(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tds_ScanAvailableDot11Networks_rly_xml, (char *)&res, NULL, p_header);
}

#endif // DOT11_SUPPORT

void soap_FirmwareUpgrade(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	log_print(HT_LOG_INFO, "soap_FirmwareUpgrade\n");
	char * p_buff = http_get_ctt(rx_msg);
	
	if (onvif_tds_FirmwareUpgradeCheck(p_buff, rx_msg->ctt_len))
	{
		if (onvif_tds_FirmwareUpgrade(p_buff, rx_msg->ctt_len))
		{
			soap_http_rly(p_user, rx_msg, NULL, 0);

			onvif_tds_FirmwareUpgradePost();
		}
		else
		{
			soap_http_err_rly(p_user, rx_msg, 500, "Internal Server Error", NULL, 0);
		}
	}
	else
	{
		soap_http_err_rly(p_user, rx_msg, 415, "Unsupported Media Type", NULL, 0);
	}	
}

void soap_SystemRestore(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	char * p_buff = http_get_ctt(rx_msg);
	
	if (onvif_tds_SystemRestoreCheck(p_buff, rx_msg->ctt_len))
	{
		if (onvif_tds_SystemRestore(p_buff, rx_msg->ctt_len))
		{
			soap_http_rly(p_user, rx_msg, NULL, 0);

			onvif_tds_SystemRestorePost();
		}
		else
		{
			soap_http_err_rly(p_user, rx_msg, 500, "Internal Server Error", NULL, 0);
		}
	}
	else
	{
		soap_http_err_rly(p_user, rx_msg, 415, "Unsupported Media Type", NULL, 0);
	}	
}

void soap_GetSnapshot(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	log_print(HT_LOG_INFO, "soap_GetSnapshot\n");
#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

	int   rlen = 0;
	char  url[128] = {0};
	get_url_http(rx_msg->first_line.value_string, url);
	char  profile_token[ONVIF_TOKEN_LEN] = {'\0'};
	char *buff = NULL;
	char *post = url;
	char *p1 = strstr(post, "snapshot");
	if (p1)
	{
		char * p2 = strchr(p1 + 1, '=');
		if (p2)
		{
			int i = 0;
			p2 ++;
			while (p2 && *p2 != '&')
			{
				if (*p2 == ' ')
				{
					break;
				}
				if (i < ONVIF_TOKEN_LEN-1)
				{
					profile_token[i ++] = *p2;
				}
				p2 ++;
			}
			profile_token[i] = '\0';
		}
	}

	int cgi_ret = 1;
	char *response_buf = (char *)malloc(1024 * sizeof(char));
	if (response_buf == NULL)
	{
		soap_http_err_rly(p_user, rx_msg, 500, "Internal Server Error", NULL, 0);
		return;
	}
	memset(response_buf, 0, 1024 * sizeof(char));
	MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
	if ((pStreamCfg && pStreamCfg->webConfig.onvif_auth) || is_Y_Version)
	{
		cgi_ret = cgi_is_valid_uid_or_username(post, response_buf, "/snapshot.cgi");
		log_print(HT_LOG_INFO, "response_buf:%s\n", response_buf);
		log_print(HT_LOG_INFO, "cgi_ret:%d\n", cgi_ret);
	}
	free(response_buf);
	if (cgi_ret == 0)
	{
		int ret = httpd_auth_handler(g_onvif_cls.httpd, p_user, rx_msg, "GET");
		if (ret == FALSE)
		{
			httpd_send_authenticate(g_onvif_cls.httpd, p_user);
			return;
		}
		else
		{
			if (strstr(rx_msg->first_line.value_string, "stream=0"))
				strcpy(profile_token, "0");
			else
				strcpy(profile_token, "1");
		}
	}
	
	if (ONVIF_OK == onvif_trt_GetSnapshot(&buff, &rlen, profile_token))
	{
		int tlen;
		char tmp[1024] = {'\0'};
		
		tlen = sprintf(tmp,	"HTTP/1.1 200 OK\r\n"
							"Server: Happytime onvif server %s\r\n"
							"Content-Type: image/jpeg\r\n"
							"Content-Length: %d\r\n"
							"Connection: %s\r\n\r\n",
							ONVIF_VERSION_STRING, 
							rlen,
							p_user->keep_alive ? "keep-alive" : "close");

		http_srv_cln_tx(p_user, tmp, tlen);//����ͷ
		http_srv_cln_tx(p_user, buff, rlen);//����ͼƬ����
	}
	else
	{
		soap_http_err_rly(p_user, rx_msg, 500, "Internal Server Error", NULL, 0);
	}
	
	if (buff != NULL)
	{
		free(buff);
	}

	return;

#else 

	soap_http_err_rly(p_user, rx_msg, 500, "Internal Server Error", NULL, 0);

#endif //  defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)    
}

void soap_GetHttpSystemLog(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	int slen;
	int offset;
	int content_len;
	char buff[1024];
	char content[1024];

	// todo : just for test
	strcpy(content, "test system log");    
	content_len = (int)strlen(content);

	offset = sprintf(buff, "HTTP/1.1 200 OK\r\n"
							"Server: Happytime onvif server %s\r\n"
							"Content-Type: text/plain\r\n"
							"Content-Length: %d\r\n"
							"Connection: %s\r\n\r\n",
							ONVIF_VERSION_STRING, 
							content_len,
							p_user->keep_alive ? "keep-alive" : "close");

	slen = http_srv_cln_tx(p_user, buff, offset);
	if (slen != offset)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, offset=%d\r\n", __FUNCTION__, slen, offset);
	}

	slen = http_srv_cln_tx(p_user, content, content_len);
	if (slen != content_len)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, content_len=%d\r\n", __FUNCTION__, slen, content_len);
	}
}

void soap_GetHttpAccessLog(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	int slen;
	int offset;
	int content_len;
	char buff[1024];
	char content[1024];

	// todo : just for test
	strcpy(content, "test access log");    
	content_len = (int)strlen(content);

	offset = sprintf(buff, "HTTP/1.1 200 OK\r\n"
							"Server: Happytime onvif server %s\r\n"
							"Content-Type: text/plain\r\n"
							"Content-Length: %d\r\n"
							"Connection: %s\r\n\r\n",
							ONVIF_VERSION_STRING, 
							content_len,
							p_user->keep_alive ? "keep-alive" : "close");

	slen = http_srv_cln_tx(p_user, buff, offset);
	if (slen != offset)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, offset=%d\r\n", __FUNCTION__, slen, offset);
	}

	slen = http_srv_cln_tx(p_user, content, content_len);
	if (slen != content_len)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, content_len=%d\r\n", __FUNCTION__, slen, content_len);
	}
}

void soap_GetSupportInfo(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	int slen;
	int offset;
	int content_len;
	char buff[1024];
	char content[1024];

	// todo : just for test
	strcpy(content, "test support info");    
	content_len = (int)strlen(content);

	offset = sprintf(buff, "HTTP/1.1 200 OK\r\n"
							"Server: Happytime onvif server %s\r\n"
							"Content-Type: text/plain\r\n"
							"Content-Length: %d\r\n"
							"Connection: %s\r\n\r\n",
							ONVIF_VERSION_STRING, 
							content_len,
							p_user->keep_alive ? "keep-alive" : "close");

	slen = http_srv_cln_tx(p_user, buff, offset);
	if (slen != offset)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, offset=%d\r\n", __FUNCTION__, slen, offset);
	}

	slen = http_srv_cln_tx(p_user, content, content_len);
	if (slen != content_len)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, content_len=%d\r\n", __FUNCTION__, slen, content_len);
	}
}

void soap_GetSystemBackup(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	int slen;
	int offset;
	int content_len;
	char buff[1024];
	char content[1024];

	// todo : just for test
	strcpy(content, "test system backup");    
	content_len = (int)strlen(content);

	offset = sprintf(buff, "HTTP/1.1 200 OK\r\n"
							"Server: Happytime onvif server %s\r\n"
							"Content-Type: text/plain\r\n"
							"Content-Length: %d\r\n"
							"Connection: %s\r\n\r\n",
							ONVIF_VERSION_STRING, 
							content_len,
							p_user->keep_alive ? "keep-alive" : "close");

	slen = http_srv_cln_tx(p_user, buff, offset);
	if (slen != offset)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, offset=%d\r\n", __FUNCTION__, slen, offset);
	}

	slen = http_srv_cln_tx(p_user, content, content_len);
	if (slen != content_len)
	{
		log_print(HT_LOG_ERR, "%s, slen=%d, content_len=%d\r\n", __FUNCTION__, slen, content_len);
	}
}

int soap_tev_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");

	category = CapabilityCategory_Events;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, 
			"http://www.onvif.org/ver10/events/wsdl/EventPortType/GetServiceCapabilitiesResponse", p_header);
}

int soap_tev_GetEventProperties(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tev_GetEventProperties_rly_xml, NULL, 
			"http://www.onvif.org/ver10/events/wsdl/EventPortType/GetEventPropertiesResponse", p_header);
}

int soap_tev_Subscribe(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_Subscribe;
	tev_Subscribe_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_Subscribe = xml_node_soap_get(p_body, "Subscribe");
	assert(p_Subscribe);

	memset(&req, 0, sizeof(req));

	ret = parse_tev_Subscribe(p_Subscribe, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tev_Subscribe(p_user, &req);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tev_Subscribe_rly_xml, (char *)req.p_eua, 
				"http://docs.oasis-open.org/wsn/bw-2/NotificationProducer/SubscribeResponse", p_header); 
		
	return ret;
}

int soap_tev_Unsubscribe(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_To;
	uint32 eua_idx = 0;

	log_print(HT_LOG_DBG, "\n");

	p_To = xml_node_soap_get(p_header, "To");
	if (p_To && p_To->data)
	{
		char * p = strstr(p_To->data, "/event_service");
		if (p)
		{
			// get the event agent entity index
			sscanf(p, "/event_service/%u", &eua_idx);
		}
		else
		{
			// get the event agent entity index
			sscanf(rx_msg->first_line.value_string, "/event_service/%u", &eua_idx);
		}
	}
	else
	{
		// get the event agent entity index
		sscanf(rx_msg->first_line.value_string, "/event_service/%u", &eua_idx);
	}

	ret = onvif_tev_Unsubscribe(eua_idx);

    return soap_build_send_rly(p_user, rx_msg, ret, build_tev_Unsubscribe_rly_xml, NULL, 
				"http://docs.oasis-open.org/wsn/bw-2/SubscriptionManager/UnsubscribeResponse", p_header);
}

int soap_tev_Renew(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_Renew;
	XMLN * p_To;
	tev_Renew_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_Renew = xml_node_soap_get(p_body, "Renew");
	assert(p_Renew);
	
	memset(&req, 0, sizeof(req));

	p_To = xml_node_soap_get(p_header, "To");
	if (p_To && p_To->data)
	{
		char * p = strstr(p_To->data, "/event_service");
		if (p)
		{
			// get the event agent entity index
			sscanf(p, "/event_service/%u", &req.eua_idx);
		}
		else
		{
			// get the event agent entity index
			sscanf(rx_msg->first_line.value_string, "/event_service/%u", &req.eua_idx);
		}
	}
	else
	{
		// get the event agent entity index
		sscanf(rx_msg->first_line.value_string, "/event_service/%u", &req.eua_idx);
	}

    log_print(HT_LOG_INFO, "[%s, %5d] renew eua[%d]\n", __func__, __LINE__, req.eua_idx);

	ret = parse_tev_Renew(p_Renew, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tev_Renew(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tev_Renew_rly_xml, NULL, "http://docs.oasis-open.org/wsn/bw-2/SubscriptionManager/RenewResponse", p_header);	
}

int soap_tev_CreatePullPointSubscription(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreatePullPointSubscription;
	tev_CreatePullPointSubscription_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreatePullPointSubscription = xml_node_soap_get(p_body, "CreatePullPointSubscription");
	assert(p_CreatePullPointSubscription);

	memset(&req, 0, sizeof(req));

	ret = parse_tev_CreatePullPointSubscription(p_CreatePullPointSubscription, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tev_CreatePullPointSubscription(p_user, &req);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tev_CreatePullPointSubscription_rly_xml, (char *)req.p_eua, 
				"http://www.onvif.org/ver10/events/wsdl/EventPortType/CreatePullPointSubscriptionResponse", p_header);
	
	return ret;
}

int soap_tev_PullMessages(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_To;
	XMLN * p_PullMessages;
	tev_PullMessages_REQ req;
	int is_custom_ipc_web = 0;
	log_print(HT_LOG_DBG, "\n");

	p_PullMessages = xml_node_soap_get(p_body, "PullMessages");
	assert(p_PullMessages);

	memset(&req, 0, sizeof(req));

	p_To = xml_node_soap_get(p_header, "To");
	if (p_To && p_To->data)
	{
		char * p = strstr(p_To->data, "/event_service");
		if (p)
		{
			// get the event agent entity index
			sscanf(p, "/event_service/%u", &req.eua_idx);
		}
		else
		{
			// get the event agent entity index
			sscanf(rx_msg->first_line.value_string, "/event_service/%u", &req.eua_idx);
		}
	}
	else
	{
	    char * p = strstr(rx_msg->first_line.value_string, "/event_service");
	    if (p)
	    {
    	    // get the event agent entity index
    	    sscanf(rx_msg->first_line.value_string, "/event_service/%u", &req.eua_idx);
	    }
	    else
	    {
	        XMLN * p_any = NULL;
	        p_any = xml_node_soap_get(p_PullMessages, "any");
	        if (p_any && p_any->data && strstr(p_any->data, "ipc_web"))
	        {
                is_custom_ipc_web = 1;
	        }
	    }
	}

    log_print(HT_LOG_DBG, "[%s, %5d] is_custom_ipc_web:%d\r\n", is_custom_ipc_web);

	ret = parse_tev_PullMessages(p_PullMessages, &req);
	if (ONVIF_OK == ret)
	{
	    EUA * p_eua = NULL;
        if (1 == is_custom_ipc_web)
        {
            int custom_eua_index = -1;
            BOOL find_flag = FALSE;

            // 先找是否存在 该client相同IP创建的web eua节点
            find_flag = onvif_find_eua_by_client(p_user, &custom_eua_index, EUA_CLIENT_TYPE_WEB);        

            if (FALSE == find_flag)                             //没有找到时先创建针对该IP的web类型订阅节点
            {
                custom_eua_index = -1;
                ret = onvif_create_web_eua(p_user, &custom_eua_index);
            }

            if (ONVIF_OK == ret && custom_eua_index != -1)      // 设置eua节点参数
            {
                req.eua_idx = custom_eua_index;
                ret = onvif_tev_PullMessages(&req);
                if (ONVIF_OK == ret)
                {
                    p_eua = onvif_get_eua_by_index(req.eua_idx);
                }
            }

    		if (p_eua && p_eua->used)
    		{
    		    // eua上一个client如果没有被释放，在这里直接解绑，等check keep-alive时clean掉
                //HTTPCLN *p_cli_tmp = (HTTPCLN *)p_eua->pullmsg.http_cln;
                p_eua->pullmsg.http_cln = (void *)p_user;

    			return ONVIF_OK;
    		}
    		else
    		{
    			ret = ONVIF_ERR_ResourceUnknownFault;
    		}
        }
        else
        {
            ret = onvif_tev_PullMessages(&req);
            if (ONVIF_OK == ret)
            {
                p_eua = onvif_get_eua_by_index(req.eua_idx);
            }

    		if (p_eua && p_eua->used)
    		{
    			p_user->use_count++;
    			p_eua->pullmsg.http_cln = (void *)p_user;

    			return ONVIF_OK;
    		}
    		else
    		{
    			ret = ONVIF_ERR_ResourceUnknownFault;
    		}
        }
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tev_PullMessages_rly_xml, (char *)&req, 
				"http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesResponse", p_header);
}

int soap_tev_SetSynchronizationPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;

	log_print(HT_LOG_DBG, "\n");

	ret = onvif_tev_SetSynchronizationPoint();

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tev_SetSynchronizationPoint_rly_xml, NULL, 
		"http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/SetSynchronizationPointResponse", p_header);

	// todo : generate event, just for test

	if (g_onvif_cfg.evt_sim_flag)
	{
		NotificationMessageList * p_message = onvif_init_NotificationMessage(TRUE);
		if (p_message)
		{
			onvif_put_NotificationMessage(p_message);
		}

		p_message = onvif_init_NotificationMessage1(TRUE);
		if (p_message)
		{
			onvif_put_NotificationMessage(p_message);
		}
	}

	return ret;
}

#ifdef IMAGE_SUPPORT

int soap_img_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Imaging;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_img_GetImagingSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetImagingSettings;
	img_GetImagingSettings_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetImagingSettings = xml_node_soap_get(p_body, "GetImagingSettings");
	assert(p_GetImagingSettings);

	memset(&req, 0, sizeof(img_GetImagingSettings_REQ));

	ret = parse_img_GetImagingSettings(p_GetImagingSettings, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_GetImagingSettings_rly_xml, (char *)&req, NULL, p_header);
}

int soap_img_SetImagingSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetImagingSettings;
	img_SetImagingSettings_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetImagingSettings = xml_node_soap_get(p_body, "SetImagingSettings");
	assert(p_SetImagingSettings);

	memset(&req, 0, sizeof(img_SetImagingSettings_REQ));

	ret = parse_img_SetImagingSettings(p_SetImagingSettings, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_img_SetImagingSettings(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_SetImagingSettings_rly_xml, NULL, NULL, p_header);
}

int soap_img_GetOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetOptions;
	img_GetOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetOptions = xml_node_soap_get(p_body, "GetOptions");
	assert(p_GetOptions);

	memset(&req, 0, sizeof(img_GetOptions_REQ));

	ret = parse_img_GetOptions(p_GetOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_GetOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_img_GetMoveOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetMoveOptions;
	img_GetMoveOptions_REQ req;
	img_GetMoveOptions_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetMoveOptions = xml_node_soap_get(p_body, "GetMoveOptions");
	assert(p_GetMoveOptions);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_img_GetMoveOptions(p_GetMoveOptions, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_img_GetMoveOptions(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_GetMoveOptions_rly_xml, (char *)&res, NULL, p_header);
}

int soap_img_Move(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_Move;
	img_Move_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_Move = xml_node_soap_get(p_body, "Move");
	assert(p_Move);
	
	memset(&req, 0, sizeof(img_Move_REQ));

	ret = parse_img_Move(p_Move, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_img_Move(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_Move_rly_xml, NULL, NULL, p_header);	
}

int soap_img_GetStatus(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetStatus;
	img_GetStatus_REQ req;
	img_GetStatus_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetStatus = xml_node_soap_get(p_body, "GetStatus");
	assert(p_GetStatus);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_img_GetStatus(p_GetStatus, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_img_GetStatus(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_GetStatus_rly_xml, (char *)&res, NULL, p_header);
}

int soap_img_Stop(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_Stop;
	img_Stop_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_Stop = xml_node_soap_get(p_body, "Stop");
	assert(p_Stop);

	memset(&req, 0, sizeof(img_Stop_REQ));

	ret = parse_img_Stop(p_Stop, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_img_Stop(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_Stop_rly_xml, NULL, NULL, p_header);
}

int soap_img_GetPresets(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetPresets;
	img_GetPresets_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetPresets = xml_node_soap_get(p_body, "GetPresets");
	assert(p_GetPresets);

	memset(&req, 0, sizeof(img_GetPresets_REQ));

	ret = parse_img_GetPresets(p_GetPresets, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_GetPresets_rly_xml, (char *)&req, NULL, p_header);
}

int soap_img_GetCurrentPreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetPresets;
	img_GetCurrentPreset_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetPresets = xml_node_soap_get(p_body, "GetCurrentPreset");
	assert(p_GetPresets);

	ret = parse_img_GetCurrentPreset(p_GetPresets, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_GetCurrentPreset_rly_xml, (char *)&req, NULL, p_header);
}

int soap_img_SetCurrentPreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetPresets;
	img_SetCurrentPreset_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetPresets = xml_node_soap_get(p_body, "SetCurrentPreset");
	assert(p_GetPresets);

	ret = parse_img_SetCurrentPreset(p_GetPresets, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_img_SetCurrentPreset(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_img_SetCurrentPreset_rly_xml, NULL, NULL, p_header);	
}

#endif // IMAGE_SUPPORT

#ifdef MEDIA_SUPPORT

int soap_trt_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Media;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_trt_GetProfiles(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetProfiles_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetProfile;
	trt_GetProfile_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetProfile = xml_node_soap_get(p_body, "GetProfile");
	assert(p_GetProfile);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trt_GetProfile(p_GetProfile, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetProfile_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_CreateProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateProfile;
	trt_CreateProfile_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateProfile = xml_node_soap_get(p_body, "CreateProfile");
	assert(p_CreateProfile);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_CreateProfile(p_CreateProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_CreateProfile(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_CreateProfile_rly_xml, req.Token, NULL, p_header);
}

int soap_trt_DeleteProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteProfile;
	trt_DeleteProfile_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_DeleteProfile = xml_node_soap_get(p_body, "DeleteProfile");
	assert(p_DeleteProfile);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trt_DeleteProfile(p_DeleteProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_DeleteProfile(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_DeleteProfile_rly_xml, NULL, NULL, p_header);
}

int soap_trt_AddVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddVideoSourceConfiguration;
	trt_AddVideoSourceConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_AddVideoSourceConfiguration = xml_node_soap_get(p_body, "AddVideoSourceConfiguration");
	assert(p_AddVideoSourceConfiguration);
	
	memset(&req, 0, sizeof(req));
	
	ret = parse_trt_AddVideoSourceConfiguration(p_AddVideoSourceConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddVideoSourceConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddVideoSourceConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_RemoveVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveVideoSourceConfiguration;
	trt_RemoveVideoSourceConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveVideoSourceConfiguration = xml_node_soap_get(p_body, "RemoveVideoSourceConfiguration");
	assert(p_RemoveVideoSourceConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_RemoveVideoSourceConfiguration(p_RemoveVideoSourceConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_RemoveVideoSourceConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveVideoSourceConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_AddVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddVideoEncoderConfiguration;
	trt_AddVideoEncoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_AddVideoEncoderConfiguration = xml_node_soap_get(p_body, "AddVideoEncoderConfiguration");
	assert(p_AddVideoEncoderConfiguration);
	
	memset(&req, 0, sizeof(req));
	
	ret = parse_trt_AddVideoEncoderConfiguration(p_AddVideoEncoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddVideoEncoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddVideoEncoderConfiguration_rly_xml, NULL, NULL, p_header);	
}

int soap_trt_RemoveVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveVideoEncoderConfiguration;
	trt_RemoveVideoEncoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveVideoEncoderConfiguration = xml_node_soap_get(p_body, "RemoveVideoEncoderConfiguration");
	assert(p_RemoveVideoEncoderConfiguration);	

	memset(&req, 0, sizeof(req));

	ret = parse_trt_RemoveVideoEncoderConfiguration(p_RemoveVideoEncoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_RemoveVideoEncoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveVideoEncoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetVideoSources(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetVideoSources_rly_xml, NULL, NULL, p_header);	
}

int soap_trt_GetVideoEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetVideoEncoderConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetCompatibleVideoEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetCompatibleVideoEncoderConfigurations;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleVideoEncoderConfigurations = xml_node_soap_get(p_body, "GetCompatibleVideoEncoderConfigurations");
	assert(p_GetCompatibleVideoEncoderConfigurations);

	p_ProfileToken = xml_node_soap_get(p_GetCompatibleVideoEncoderConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetCompatibleVideoEncoderConfigurations_rly_xml, p_ProfileToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_GetVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetVideoEncoderConfiguration;
	XMLN * p_ConfigurationToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetVideoEncoderConfiguration = xml_node_soap_get(p_body, "GetVideoEncoderConfiguration");
	assert(p_GetVideoEncoderConfiguration);

	p_ConfigurationToken = xml_node_soap_get(p_GetVideoEncoderConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetVideoEncoderConfiguration_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_GetVideoSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetVideoSourceConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetVideoSourceConfiguration;
	XMLN * p_ConfigurationToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetVideoSourceConfiguration = xml_node_soap_get(p_body, "GetVideoSourceConfiguration");
	assert(p_GetVideoSourceConfiguration);

	p_ConfigurationToken = xml_node_soap_get(p_GetVideoSourceConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetVideoSourceConfiguration_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_SetVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoSourceConfiguration;
	trt_SetVideoSourceConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetVideoSourceConfiguration = xml_node_soap_get(p_body, "SetVideoSourceConfiguration");
	assert(p_SetVideoSourceConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetVideoSourceConfiguration(p_SetVideoSourceConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetVideoSourceConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetVideoSourceConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetVideoSourceConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoSourceConfigurationOptions;
	trt_GetVideoSourceConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetVideoSourceConfigurationOptions = xml_node_soap_get(p_body, "GetVideoSourceConfigurationOptions");
	assert(p_GetVideoSourceConfigurationOptions);
	
	memset(&req, 0, sizeof(req));
	
	ret = parse_trt_GetVideoSourceConfigurationOptions(p_GetVideoSourceConfigurationOptions, &req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetVideoSourceConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_trt_GetVideoEncoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoEncoderConfigurationOptions;
	trt_GetVideoEncoderConfigurationOptions_REQ req;
	trt_GetVideoEncoderConfigurationOptions_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetVideoEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetVideoEncoderConfigurationOptions");
	assert(p_GetVideoEncoderConfigurationOptions);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_trt_GetVideoEncoderConfigurationOptions(p_GetVideoEncoderConfigurationOptions, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_GetVideoEncoderConfigurationOptions(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetVideoEncoderConfigurationOptions_rly_xml, (char *)&res, NULL, p_header);
}

int soap_trt_GetCompatibleVideoSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetCompatibleVideoSourceConfigurations;
	XMLN * p_ProfileToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleVideoSourceConfigurations = xml_node_soap_get(p_body, "GetCompatibleVideoSourceConfigurations");
	assert(p_GetCompatibleVideoSourceConfigurations);

	p_ProfileToken = xml_node_soap_get(p_GetCompatibleVideoSourceConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetCompatibleVideoSourceConfigurations_rly_xml, p_ProfileToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_SetVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoEncoderConfiguration;
	trt_SetVideoEncoderConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetVideoEncoderConfiguration = xml_node_soap_get(p_body, "SetVideoEncoderConfiguration");
	assert(p_SetVideoEncoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetVideoEncoderConfiguration(p_SetVideoEncoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetVideoEncoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetVideoEncoderConfiguration_rly_xml, NULL, NULL, p_header);    
}

int soap_trt_SetSynchronizationPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetSynchronizationPoint;
	trt_SetSynchronizationPoint_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetSynchronizationPoint = xml_node_soap_get(p_body, "SetSynchronizationPoint");
	assert(p_SetSynchronizationPoint);

	ret = parse_trt_SetSynchronizationPoint(p_SetSynchronizationPoint, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetSynchronizationPoint(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetSynchronizationPoint_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_GetStreamUri(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetStreamUri;
	trt_GetStreamUri_REQ req;
	trt_GetStreamUri_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetStreamUri = xml_node_soap_get(p_body, "GetStreamUri");
	assert(p_GetStreamUri);
	
	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_trt_GetStreamUri(p_GetStreamUri, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_GetStreamUri(p_user, &req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetStreamUri_rly_xml, (char *)&res, NULL, p_header);
}

int soap_trt_GetSnapshotUri(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSnapshotUri;
	trt_GetSnapshotUri_REQ req;
	trt_GetSnapshotUri_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetSnapshotUri = xml_node_soap_get(p_body, "GetSnapshotUri");
	assert(p_GetSnapshotUri);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_trt_GetSnapshotUri(p_GetSnapshotUri, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_GetSnapshotUri(p_user, &req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetSnapshotUri_rly_xml, (char *)&res, NULL, p_header);
}

int soap_trt_GetOSDs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetOSDs;
	trt_GetOSDs_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetOSDs = xml_node_soap_get(p_body, "GetOSDs");
	assert(p_GetOSDs);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetOSDs(p_GetOSDs, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetOSDs_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_GetOSD(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetOSD;
	trt_GetOSD_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetOSD = xml_node_soap_get(p_body, "GetOSD");
	assert(p_GetOSD);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetOSD(p_GetOSD, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetOSD_rly_xml, (char *)&req, NULL, p_header);
} 

int soap_trt_SetOSD(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetOSD;
	trt_SetOSD_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetOSD = xml_node_soap_get(p_body, "SetOSD");
	assert(p_SetOSD);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetOSD(p_SetOSD, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetOSD(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetOSD_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetOSDOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetOSDOptions_rly_xml, NULL, NULL, p_header);	
}

int soap_trt_CreateOSD(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateOSD;
	trt_CreateOSD_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateOSD = xml_node_soap_get(p_body, "CreateOSD");
	assert(p_CreateOSD);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_CreateOSD(p_CreateOSD, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_CreateOSD(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_CreateOSD_rly_xml, req.OSD.token, NULL, p_header);
}

int soap_trt_DeleteOSD(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteOSD;
	trt_DeleteOSD_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_DeleteOSD = xml_node_soap_get(p_body, "DeleteOSD");
	assert(p_DeleteOSD);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_DeleteOSD(p_DeleteOSD, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_DeleteOSD(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_DeleteOSD_rly_xml, NULL, NULL, p_header);
}

int soap_trt_StartMulticastStreaming(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_StartMulticastStreaming;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_StartMulticastStreaming = xml_node_soap_get(p_body, "StartMulticastStreaming");
	assert(p_StartMulticastStreaming);	

	p_ProfileToken = xml_node_soap_get(p_StartMulticastStreaming, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_trt_StartMulticastStreaming(p_ProfileToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_StartMulticastStreaming_rly_xml, NULL, NULL, p_header);
}

int soap_trt_StopMulticastStreaming(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_StopMulticastStreaming;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_StopMulticastStreaming = xml_node_soap_get(p_body, "StopMulticastStreaming");
	assert(p_StopMulticastStreaming);	

	p_ProfileToken = xml_node_soap_get(p_StopMulticastStreaming, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_trt_StopMulticastStreaming(p_ProfileToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_StopMulticastStreaming_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetMetadataConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetMetadataConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetMetadataConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_GetMetadataConfiguration;
	XMLN * p_ConfigurationToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetMetadataConfiguration = xml_node_soap_get(p_body, "GetMetadataConfiguration");
	assert(p_GetMetadataConfiguration);	
	
	p_ConfigurationToken = xml_node_soap_get(p_GetMetadataConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetMetadataConfiguration_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_trt_GetCompatibleMetadataConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_GetCompatibleMetadataConfigurations;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleMetadataConfigurations = xml_node_soap_get(p_body, "GetCompatibleMetadataConfigurations");
	assert(p_GetCompatibleMetadataConfigurations);	
	
	p_ProfileToken = xml_node_soap_get(p_GetCompatibleMetadataConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetCompatibleMetadataConfigurations_rly_xml, p_ProfileToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_trt_GetMetadataConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetMetadataConfigurationOptions;
	trt_GetMetadataConfigurationOptions_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_GetMetadataConfigurationOptions = xml_node_soap_get(p_body, "GetMetadataConfigurationOptions");
	assert(p_GetMetadataConfigurationOptions);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetMetadataConfigurationOptions(p_GetMetadataConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetMetadataConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_SetMetadataConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetMetadataConfiguration;
	trt_SetMetadataConfiguration_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetMetadataConfiguration = xml_node_soap_get(p_body, "SetMetadataConfiguration");
	assert(p_SetMetadataConfiguration);
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetMetadataConfiguration(p_SetMetadataConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetMetadataConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetMetadataConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_AddMetadataConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddMetadataConfiguration;
	trt_AddMetadataConfiguration_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_AddMetadataConfiguration = xml_node_soap_get(p_body, "AddMetadataConfiguration");
	assert(p_AddMetadataConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_AddMetadataConfiguration(p_AddMetadataConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddMetadataConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddMetadataConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_RemoveMetadataConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_RemoveMetadataConfiguration;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveMetadataConfiguration = xml_node_soap_get(p_body, "RemoveMetadataConfiguration");
	assert(p_RemoveMetadataConfiguration);	
	
	p_ProfileToken = xml_node_soap_get(p_RemoveMetadataConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_trt_RemoveMetadataConfiguration(p_ProfileToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveMetadataConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetVideoSourceModes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoSourceModes;
	trt_GetVideoSourceModes_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetVideoSourceModes = xml_node_soap_get(p_body, "GetVideoSourceModes");
	assert(p_GetVideoSourceModes);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trt_GetVideoSourceModes(p_GetVideoSourceModes, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetVideoSourceModes_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_SetVideoSourceMode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoSourceMode;
	trt_SetVideoSourceMode_REQ req;
	trt_SetVideoSourceMode_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetVideoSourceMode = xml_node_soap_get(p_body, "SetVideoSourceMode");
	assert(p_SetVideoSourceMode);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_trt_SetVideoSourceMode(p_SetVideoSourceMode, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetVideoSourceMode(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetVideoSourceMode_rly_xml, (char*)&res, NULL, p_header);
}

int soap_trt_GetGuaranteedNumberOfVideoEncoderInstances(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetGuaranteedNumberOfVideoEncoderInstances;
	XMLN * p_ConfigurationToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetGuaranteedNumberOfVideoEncoderInstances = xml_node_soap_get(p_body, "GetGuaranteedNumberOfVideoEncoderInstances");
	assert(p_GetGuaranteedNumberOfVideoEncoderInstances);

	p_ConfigurationToken = xml_node_soap_get(p_GetGuaranteedNumberOfVideoEncoderInstances, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetGuaranteedNumberOfVideoEncoderInstances_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

#ifdef AUDIO_SUPPORT

int soap_trt_AddAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddAudioSourceConfiguration;
	trt_AddAudioSourceConfiguration_REQ req;;

	log_print(HT_LOG_DBG, "\n");

	p_AddAudioSourceConfiguration = xml_node_soap_get(p_body, "AddAudioSourceConfiguration");
	assert(p_AddAudioSourceConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_AddAudioSourceConfiguration(p_AddAudioSourceConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddAudioSourceConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddAudioSourceConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_RemoveAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_RemoveAudioSourceConfiguration;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveAudioSourceConfiguration = xml_node_soap_get(p_body, "RemoveAudioSourceConfiguration");
	assert(p_RemoveAudioSourceConfiguration);	

	p_ProfileToken = xml_node_soap_get(p_RemoveAudioSourceConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_trt_RemoveAudioSourceConfiguration(p_ProfileToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveAudioSourceConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_AddAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddAudioEncoderConfiguration;
	trt_AddAudioEncoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_AddAudioEncoderConfiguration = xml_node_soap_get(p_body, "AddAudioEncoderConfiguration");
	assert(p_AddAudioEncoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_AddAudioEncoderConfiguration(p_AddAudioEncoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddAudioEncoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddAudioEncoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_RemoveAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_RemoveAudioEncoderConfiguration;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveAudioEncoderConfiguration = xml_node_soap_get(p_body, "RemoveAudioEncoderConfiguration");
	assert(p_RemoveAudioEncoderConfiguration);

	p_ProfileToken = xml_node_soap_get(p_RemoveAudioEncoderConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_trt_RemoveAudioEncoderConfiguration(p_ProfileToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveAudioEncoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetAudioSources(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioSources_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetAudioEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioEncoderConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetCompatibleAudioEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetCompatibleAudioEncoderConfigurations;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleAudioEncoderConfigurations = xml_node_soap_get(p_body, "GetCompatibleAudioEncoderConfigurations");
	assert(p_GetCompatibleAudioEncoderConfigurations);

	p_ProfileToken = xml_node_soap_get(p_GetCompatibleAudioEncoderConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetCompatibleAudioEncoderConfigurations_rly_xml, p_ProfileToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_GetAudioSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioSourceConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetCompatibleAudioSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetCompatibleAudioSourceConfigurations;
	XMLN * p_ProfileToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleAudioSourceConfigurations = xml_node_soap_get(p_body, "GetCompatibleAudioSourceConfigurations");
	assert(p_GetCompatibleAudioSourceConfigurations);

	p_ProfileToken = xml_node_soap_get(p_GetCompatibleAudioSourceConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetCompatibleAudioSourceConfigurations_rly_xml, p_ProfileToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_GetAudioSourceConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioSourceConfigurationOptions;
	trt_GetAudioSourceConfigurationOptions_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAudioSourceConfigurationOptions = xml_node_soap_get(p_body, "GetAudioSourceConfigurationOptions");
	assert(p_GetAudioSourceConfigurationOptions);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetAudioSourceConfigurationOptions(p_GetAudioSourceConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetAudioSourceConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_GetAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetAudioSourceConfiguration;
	XMLN * p_ConfigurationToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioSourceConfiguration = xml_node_soap_get(p_body, "GetAudioSourceConfiguration");
	assert(p_GetAudioSourceConfiguration);

	p_ConfigurationToken = xml_node_soap_get(p_GetAudioSourceConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioSourceConfiguration_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_SetAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioSourceConfiguration;
	trt_SetAudioSourceConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetAudioSourceConfiguration = xml_node_soap_get(p_body, "SetAudioSourceConfiguration");
	assert(p_SetAudioSourceConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetAudioSourceConfiguration(p_SetAudioSourceConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetAudioSourceConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetAudioSourceConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetAudioEncoderConfiguration;
	XMLN * p_ConfigurationToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioEncoderConfiguration = xml_node_soap_get(p_body, "GetAudioEncoderConfiguration");
	assert(p_GetAudioEncoderConfiguration);

	p_ConfigurationToken = xml_node_soap_get(p_GetAudioEncoderConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioEncoderConfiguration_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_SetAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioEncoderConfiguration;
	trt_SetAudioEncoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAudioEncoderConfiguration = xml_node_soap_get(p_body, "SetAudioEncoderConfiguration");
	assert(p_SetAudioEncoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetAudioEncoderConfiguration(p_SetAudioEncoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetAudioEncoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetAudioEncoderConfiguration_rly_xml, NULL, NULL, p_header);    
}

int soap_trt_GetAudioEncoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioEncoderConfigurationOptions;
	trt_GetAudioEncoderConfigurationOptions_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAudioEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetAudioEncoderConfigurationOptions");
	assert(p_GetAudioEncoderConfigurationOptions);    

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetAudioEncoderConfigurationOptions(p_GetAudioEncoderConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetAudioEncoderConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_trt_AddAudioDecoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddAudioDecoderConfiguration;
	trt_AddAudioDecoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_AddAudioDecoderConfiguration = xml_node_soap_get(p_body, "AddAudioDecoderConfiguration");
	assert(p_AddAudioDecoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_AddAudioDecoderConfiguration(p_AddAudioDecoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddAudioDecoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddAudioDecoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetAudioDecoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioDecoderConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetAudioDecoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetAudioDecoderConfiguration;
	XMLN * p_ConfigurationToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioDecoderConfiguration = xml_node_soap_get(p_body, "GetAudioDecoderConfiguration");
	assert(p_GetAudioDecoderConfiguration);

	p_ConfigurationToken = xml_node_soap_get(p_GetAudioDecoderConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioDecoderConfiguration_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_trt_RemoveAudioDecoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveAudioDecoderConfiguration;
	trt_RemoveAudioDecoderConfiguration_REQ req;;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveAudioDecoderConfiguration = xml_node_soap_get(p_body, "RemoveAudioDecoderConfiguration");
	assert(p_RemoveAudioDecoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_RemoveAudioDecoderConfiguration(p_RemoveAudioDecoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_RemoveAudioDecoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveAudioDecoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_SetAudioDecoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioDecoderConfiguration;
	trt_SetAudioDecoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAudioDecoderConfiguration = xml_node_soap_get(p_body, "SetAudioDecoderConfiguration");
	assert(p_SetAudioDecoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetAudioDecoderConfiguration(p_SetAudioDecoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetAudioDecoderConfiguration(&req);
	}


	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetAudioDecoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetAudioDecoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioDecoderConfigurationOptions;
	trt_GetAudioDecoderConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioDecoderConfigurationOptions = xml_node_soap_get(p_body, "GetAudioDecoderConfigurationOptions");
	assert(p_GetAudioDecoderConfigurationOptions);    

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetAudioDecoderConfigurationOptions(p_GetAudioDecoderConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetAudioDecoderConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_trt_GetCompatibleAudioDecoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetCompatibleAudioDecoderConfigurations;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleAudioDecoderConfigurations = xml_node_soap_get(p_body, "GetCompatibleAudioDecoderConfigurations");
	assert(p_GetCompatibleAudioDecoderConfigurations);

	p_ProfileToken = xml_node_soap_get(p_GetCompatibleAudioDecoderConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetCompatibleAudioDecoderConfigurations_rly_xml, p_ProfileToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

#endif // end of AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT

int soap_trt_GetAudioOutputs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioOutputs_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetAudioOutputConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAudioOutputConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_SetAudioOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioOutputConfiguration;
	tmd_SetAudioOutputConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAudioOutputConfiguration = xml_node_soap_get(p_body, "SetAudioOutputConfiguration");
	assert(p_SetAudioOutputConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetAudioOutputConfiguration(p_SetAudioOutputConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetAudioOutputConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetAudioOutputConfiguration_rly_xml, NULL, NULL, p_header);	
}

int soap_trt_GetAudioOutputConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioOutputConfigurationOptions;
	trt_GetAudioOutputConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioOutputConfigurationOptions = xml_node_soap_get(p_body, "GetAudioOutputConfigurationOptions");
	assert(p_GetAudioOutputConfigurationOptions);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetAudioOutputConfigurationOptions(p_GetAudioOutputConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetAudioOutputConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_GetAudioOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioOutputConfiguration;
	trt_GetAudioOutputConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioOutputConfiguration = xml_node_soap_get(p_body, "GetAudioOutputConfiguration");
	assert(p_GetAudioOutputConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetAudioOutputConfiguration(p_GetAudioOutputConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetAudioOutputConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_AddAudioOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddAudioOutputConfiguration;
	trt_AddAudioOutputConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_AddAudioOutputConfiguration = xml_node_soap_get(p_body, "AddAudioOutputConfiguration");
	assert(p_AddAudioOutputConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_AddAudioOutputConfiguration(p_AddAudioOutputConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddAudioOutputConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddAudioOutputConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_RemoveAudioOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveAudioOutputConfiguration;
	trt_RemoveAudioOutputConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveAudioOutputConfiguration = xml_node_soap_get(p_body, "RemoveAudioOutputConfiguration");
	assert(p_RemoveAudioOutputConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_RemoveAudioOutputConfiguration(p_RemoveAudioOutputConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_RemoveAudioOutputConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveAudioOutputConfiguration_rly_xml, NULL, NULL, p_header);	
}

int soap_trt_GetCompatibleAudioOutputConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCompatibleAudioOutputConfigurations;
	trt_GetCompatibleAudioOutputConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleAudioOutputConfigurations = xml_node_soap_get(p_body, "GetCompatibleAudioOutputConfigurations");
	assert(p_GetCompatibleAudioOutputConfigurations);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetCompatibleAudioOutputConfigurations(p_GetCompatibleAudioOutputConfigurations, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetCompatibleAudioOutputConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

#endif // end of DEVICEIO_SUPPORT

#ifdef PTZ_SUPPORT

int soap_trt_AddPTZConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddPTZConfiguration;
	trt_AddPTZConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_AddPTZConfiguration = xml_node_soap_get(p_body, "AddPTZConfiguration");
	assert(p_AddPTZConfiguration);	
	
	memset(&req, 0, sizeof(req));

	ret = parse_trt_AddPTZConfiguration(p_AddPTZConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddPTZConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddPTZConfiguration_rly_xml, NULL, NULL, p_header);	
}

int soap_trt_RemovePTZConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_RemovePTZConfiguration;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_RemovePTZConfiguration = xml_node_soap_get(p_body, "RemovePTZConfiguration");
	assert(p_RemovePTZConfiguration);

	p_ProfileToken = xml_node_soap_get(p_RemovePTZConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_trt_RemovePTZConfiguration(p_ProfileToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemovePTZConfiguration_rly_xml, NULL, NULL, p_header);
}

#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

int soap_trt_GetAnalyticsConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetAnalyticsConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetVideoAnalyticsConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
	
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trt_GetVideoAnalyticsConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_trt_AddVideoAnalyticsConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddVideoAnalyticsConfiguration;
	trt_AddVideoAnalyticsConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_AddVideoAnalyticsConfiguration = xml_node_soap_get(p_body, "AddVideoAnalyticsConfiguration");
	assert(p_AddVideoAnalyticsConfiguration);	

	memset(&req, 0, sizeof(req));

	ret = parse_trt_AddVideoAnalyticsConfiguration(p_AddVideoAnalyticsConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_AddVideoAnalyticsConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_AddVideoAnalyticsConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetVideoAnalyticsConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoAnalyticsConfiguration;
	trt_GetVideoAnalyticsConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetVideoAnalyticsConfiguration = xml_node_soap_get(p_body, "GetVideoAnalyticsConfiguration");
	assert(p_GetVideoAnalyticsConfiguration);	

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetVideoAnalyticsConfiguration(p_GetVideoAnalyticsConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetVideoAnalyticsConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trt_RemoveVideoAnalyticsConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveVideoAnalyticsConfiguration;
	trt_RemoveVideoAnalyticsConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveVideoAnalyticsConfiguration = xml_node_soap_get(p_body, "RemoveVideoAnalyticsConfiguration");
	assert(p_RemoveVideoAnalyticsConfiguration);	

	memset(&req, 0, sizeof(req));

	ret = parse_trt_RemoveVideoAnalyticsConfiguration(p_RemoveVideoAnalyticsConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_RemoveVideoAnalyticsConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_RemoveVideoAnalyticsConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_SetVideoAnalyticsConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoAnalyticsConfiguration;
	trt_SetVideoAnalyticsConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetVideoAnalyticsConfiguration = xml_node_soap_get(p_body, "SetVideoAnalyticsConfiguration");
	assert(p_SetVideoAnalyticsConfiguration);	

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetVideoAnalyticsConfiguration(p_SetVideoAnalyticsConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetVideoAnalyticsConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_SetVideoAnalyticsConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_trt_GetCompatibleVideoAnalyticsConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCompatibleVideoAnalyticsConfigurations;
	trt_GetCompatibleVideoAnalyticsConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleVideoAnalyticsConfigurations = xml_node_soap_get(p_body, "GetCompatibleVideoAnalyticsConfigurations");
	assert(p_GetCompatibleVideoAnalyticsConfigurations);	

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetCompatibleVideoAnalyticsConfigurations(p_GetCompatibleVideoAnalyticsConfigurations, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trt_GetCompatibleVideoAnalyticsConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

#endif // VIDEO_ANALYTICS

#endif // MEDIA_SUPPORT

#ifdef PTZ_SUPPORT

int soap_ptz_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_PTZ;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_ptz_GetNodes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_ptz_GetNodes_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_GetNode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetNode;
	XMLN * p_NodeToken;
		
	log_print(HT_LOG_DBG, "\n");

	p_GetNode = xml_node_soap_get(p_body, "GetNode");
	assert(p_GetNode);
	
	p_NodeToken = xml_node_soap_get(p_GetNode, "NodeToken");
	if (p_NodeToken && p_NodeToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_ptz_GetNode_rly_xml, p_NodeToken->data, NULL, p_header);
	}
	
	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_ptz_GetConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
		
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_ptz_GetConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_GetCompatibleConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCompatibleConfigurations;
	ptz_GetCompatibleConfigurations_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_GetCompatibleConfigurations = xml_node_soap_get(p_body, "GetCompatibleConfigurations");
	assert(p_GetCompatibleConfigurations);
	
	memset(&req, 0, sizeof(req));

	ret = parse_ptz_GetCompatibleConfigurations(p_GetCompatibleConfigurations, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_GetCompatibleConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

int soap_ptz_GetConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetConfiguration;
	XMLN * p_PTZConfigurationToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetConfiguration = xml_node_soap_get(p_body, "GetConfiguration");
	assert(p_GetConfiguration);
	
	p_PTZConfigurationToken = xml_node_soap_get(p_GetConfiguration, "PTZConfigurationToken");
	if (p_PTZConfigurationToken && p_PTZConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_ptz_GetConfiguration_rly_xml, p_PTZConfigurationToken->data, NULL, p_header);
	}
	
	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_ptz_SetConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetConfiguration;
	ptz_SetConfiguration_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetConfiguration = xml_node_soap_get(p_body, "SetConfiguration");
	assert(p_SetConfiguration);
	
	memset(&req, 0, sizeof(req));

	ret = parse_ptz_SetConfiguration(p_SetConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_SetConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_SetConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_GetConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetConfigurationOptions;
	XMLN * p_ConfigurationToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetConfigurationOptions = xml_node_soap_get(p_body, "GetConfigurationOptions");
	assert(p_GetConfigurationOptions);
	
	p_ConfigurationToken = xml_node_soap_get(p_GetConfigurationOptions, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_ptz_GetConfigurationOptions_rly_xml, p_ConfigurationToken->data, NULL, p_header);
	}
	
	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_ptz_ContinuousMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ContinuousMove;
	ptz_ContinuousMove_REQ req;
	
	log_print(HT_LOG_DBG, "\n");
	
	p_ContinuousMove = xml_node_soap_get(p_body, "ContinuousMove");
	assert(p_ContinuousMove);
	
	memset(&req, 0, sizeof(ptz_ContinuousMove_REQ));

	ret = parse_ptz_ContinuousMove(p_ContinuousMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_ContinuousMove(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_ContinuousMove_rly_xml, NULL, NULL, p_header);	
}

int soap_ptz_AbsoluteMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AbsoluteMove;
	ptz_AbsoluteMove_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_AbsoluteMove = xml_node_soap_get(p_body, "AbsoluteMove");
	assert(p_AbsoluteMove);

	memset(&req, 0, sizeof(ptz_AbsoluteMove_REQ));

	ret = parse_ptz_AbsoluteMove(p_AbsoluteMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_AbsoluteMove(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_AbsoluteMove_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_RelativeMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RelativeMove;
	ptz_RelativeMove_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_RelativeMove = xml_node_soap_get(p_body, "RelativeMove");
	assert(p_RelativeMove);

	memset(&req, 0, sizeof(req));

	ret = parse_ptz_RelativeMove(p_RelativeMove, &req);    
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_RelativeMove(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_RelativeMove_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_SetPreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetPreset;
	ptz_SetPreset_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetPreset = xml_node_soap_get(p_body, "SetPreset");
	assert(p_SetPreset);
	
	memset(&req, 0, sizeof(ptz_SetPreset_REQ));

	ret = parse_ptz_SetPreset(p_SetPreset, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_SetPreset(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_SetPreset_rly_xml, req.PresetToken, NULL, p_header);
}

int soap_ptz_GetPresets(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetPresets;
	XMLN * p_ProfileToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetPresets = xml_node_soap_get(p_body, "GetPresets");
	assert(p_GetPresets);

	p_ProfileToken = xml_node_soap_get(p_GetPresets, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_ptz_GetPresets_rly_xml, p_ProfileToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_ptz_RemovePreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemovePreset;
	ptz_RemovePreset_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_RemovePreset = xml_node_soap_get(p_body, "RemovePreset");
	assert(p_RemovePreset);
	
	memset(&req, 0, sizeof(req));
	
	ret = parse_ptz_RemovePreset(p_RemovePreset, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_RemovePreset(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_RemovePreset_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_GotoPreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GotoPreset;
	ptz_GotoPreset_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GotoPreset = xml_node_soap_get(p_body, "GotoPreset");
	assert(p_GotoPreset);

	memset(&req, 0, sizeof(req));

	ret = parse_ptz_GotoPreset(p_GotoPreset, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_GotoPreset(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_GotoPreset_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_GotoHomePosition(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GotoHomePosition;
	ptz_GotoHomePosition_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GotoHomePosition = xml_node_soap_get(p_body, "GotoHomePosition");
	assert(p_GotoHomePosition);

	memset(&req, 0, sizeof(req));

	ret = parse_ptz_GotoHomePosition(p_GotoHomePosition, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_GotoHomePosition(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_GotoHomePosition_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_SetHomePosition(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;
	XMLN * p_SetHomePosition;
	XMLN * p_ProfileToken;

	log_print(HT_LOG_DBG, "\n");

	p_SetHomePosition = xml_node_soap_get(p_body, "SetHomePosition");
	assert(p_SetHomePosition);	

	p_ProfileToken = xml_node_soap_get(p_SetHomePosition, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_ptz_SetHomePosition(p_ProfileToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_SetHomePosition_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_GetPresetTours(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetPresetTours;
	ptz_GetPresetTours_REQ req;
	
	log_print(HT_LOG_DBG, "\n");
	
	p_GetPresetTours = xml_node_soap_get(p_body, "GetPresetTours");
	assert(p_GetPresetTours);
	
	memset(&req, 0, sizeof(ptz_GetPresetTours_REQ));

	ret = parse_ptz_GetPresetTours(p_GetPresetTours, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_GetPresetTours_new_rly_xml, (char *)&req, NULL, p_header);
}

int soap_ptz_GetPresetTour(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetPresetTour;
	ptz_GetPresetTour_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetPresetTour = xml_node_soap_get(p_body, "GetPresetTour");
	assert(p_GetPresetTour);

	memset(&req, 0, sizeof(ptz_GetPresetTour_REQ));

	ret = parse_ptz_GetPresetTour(p_GetPresetTour, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_GetPresetTour_rly_xml, (char *)&req, NULL, p_header);
}

int soap_ptz_GetPresetTourOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetPresetTourOptions;
	ptz_GetPresetTourOptions_REQ req;
	ptz_GetPresetTourOptions_RES res;
	
	log_print(HT_LOG_DBG, "\n");
	
	p_GetPresetTourOptions = xml_node_soap_get(p_body, "GetPresetTourOptions");
	assert(p_GetPresetTourOptions);
	
	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_ptz_GetPresetTourOptions(p_GetPresetTourOptions, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_GetPresetTourOptions(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_GetPresetTourOptions_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_ptz_CreatePresetTour(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreatePresetTour;
	ptz_CreatePresetTour_REQ req;
	ptz_CreatePresetTour_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_CreatePresetTour = xml_node_soap_get(p_body, "CreatePresetTour");
	assert(p_CreatePresetTour);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_ptz_CreatePresetTour(p_CreatePresetTour, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_CreatePresetTour(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_CreatePresetTour_rly_xml, (char*)&res, NULL, p_header);	
}

int soap_ptz_ModifyPresetTour(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyPresetTour;
	ptz_ModifyPresetTour_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_ModifyPresetTour = xml_node_soap_get(p_body, "ModifyPresetTour");
	assert(p_ModifyPresetTour);

	memset(&req, 0, sizeof(req));

	ret = parse_ptz_ModifyPresetTour(p_ModifyPresetTour, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_ModifyPresetTour(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_ModifyPresetTour_rly_xml, NULL, NULL, p_header);	
}

int soap_ptz_OperatePresetTour(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_OperatePresetTour;
	ptz_OperatePresetTour_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_OperatePresetTour = xml_node_soap_get(p_body, "OperatePresetTour");
	assert(p_OperatePresetTour);

	memset(&req, 0, sizeof(req));

	ret = parse_ptz_OperatePresetTour(p_OperatePresetTour, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_OperatePresetTour(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_OperatePresetTour_rly_xml, NULL, NULL, p_header);	
}

int soap_ptz_RemovePresetTour(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemovePresetTour;
	ptz_RemovePresetTour_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_RemovePresetTour = xml_node_soap_get(p_body, "RemovePresetTour");
	assert(p_RemovePresetTour);
	
	memset(&req, 0, sizeof(req));

	ret = parse_ptz_RemovePresetTour(p_RemovePresetTour, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_RemovePresetTour(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_RemovePresetTour_rly_xml, NULL, NULL, p_header);	
}

int soap_ptz_SendAuxiliaryCommand(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SendAuxiliaryCommand;
	ptz_SendAuxiliaryCommand_REQ req;
	ptz_SendAuxiliaryCommand_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_SendAuxiliaryCommand = xml_node_soap_get(p_body, "SendAuxiliaryCommand");
	assert(p_SendAuxiliaryCommand);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_ptz_SendAuxiliaryCommand(p_SendAuxiliaryCommand, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_SendAuxiliaryCommand(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_SendAuxiliaryCommand_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_ptz_GetStatus(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	XMLN * p_GetStatus;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetStatus = xml_node_soap_get(p_body, "GetStatus");
	assert(p_GetStatus);

	/*XMLN * p_ProfileToken = xml_node_soap_get(p_GetStatus, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
	}*/
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_ptz_GetStatus_rly_xml, NULL, NULL, p_header);
	
	
	//return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MissingAttribute);
}

int soap_ptz_Stop(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_Stop;
	ptz_Stop_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_Stop = xml_node_soap_get(p_body, "Stop");
	assert(p_Stop);

	memset(&req, 0, sizeof(req));

	ret = parse_ptz_Stop(p_Stop, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_Stop(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_Stop_rly_xml, NULL, NULL, p_header);
}

int soap_ptz_GeoMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GeoMove;
	ptz_GeoMove_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_GeoMove = xml_node_soap_get(p_body, "GeoMove");
	assert(p_GeoMove);

	memset(&req, 0, sizeof(req));

	ret = parse_ptz_GeoMove(p_GeoMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ptz_GeoMove(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_ptz_GeoMove_rly_xml, NULL, NULL, p_header);
}

#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

int soap_tan_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Analytics;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tan_GetSupportedRules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSupportedRules;
	tan_GetSupportedRules_REQ req;
	tan_GetSupportedRules_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetSupportedRules = xml_node_soap_get(p_body, "GetSupportedRules");
	assert(p_GetSupportedRules);	

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tan_GetSupportedRules(p_GetSupportedRules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_GetSupportedRules(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_GetSupportedRules_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tan_CreateRules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateRules;
	tan_CreateRules_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_CreateRules = xml_node_soap_get(p_body, "CreateRules");
	assert(p_CreateRules);	

	memset(&req, 0, sizeof(req));
	
	ret = parse_tan_CreateRules(p_CreateRules, &req);
	if (ONVIF_OK == ret)
	{		
		ret = onvif_tan_CreateRules(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_CreateRules_rly_xml, NULL, NULL, p_header);
}

int soap_tan_DeleteRules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteRules;
	tan_DeleteRules_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_DeleteRules = xml_node_soap_get(p_body, "DeleteRules");
	assert(p_DeleteRules);	

	memset(&req, 0, sizeof(req));
	
	ret = parse_tan_DeleteRules(p_DeleteRules, &req);
	if (ONVIF_OK == ret)
	{		
		ret = onvif_tan_DeleteRules(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_DeleteRules_rly_xml, NULL, NULL, p_header);
}

int soap_tan_GetRules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetRules;
	tan_GetRules_REQ req;
	tan_GetRules_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetRules = xml_node_soap_get(p_body, "GetRules");
	assert(p_GetRules);	

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tan_GetRules(p_GetRules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_GetRules(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_GetRules_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tan_ModifyRules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyRules;
	tan_ModifyRules_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_ModifyRules = xml_node_soap_get(p_body, "ModifyRules");
	assert(p_ModifyRules);	

	memset(&req, 0, sizeof(req));

	ret = parse_tan_ModifyRules(p_ModifyRules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_ModifyRules(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_ModifyRules_rly_xml, NULL, NULL, p_header);	
}

int soap_tan_CreateAnalyticsModules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateAnalyticsModules;
	tan_CreateAnalyticsModules_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_CreateAnalyticsModules = xml_node_soap_get(p_body, "CreateAnalyticsModules");
	assert(p_CreateAnalyticsModules);	

	memset(&req, 0, sizeof(req));

	ret = parse_tan_CreateAnalyticsModules(p_CreateAnalyticsModules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_CreateAnalyticsModules(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_CreateAnalyticsModules_rly_xml, NULL, NULL, p_header);
}

int soap_tan_DeleteAnalyticsModules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteAnalyticsModules;
	tan_DeleteAnalyticsModules_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteAnalyticsModules = xml_node_soap_get(p_body, "DeleteAnalyticsModules");
	assert(p_DeleteAnalyticsModules);	

	memset(&req, 0, sizeof(req));

	ret = parse_tan_DeleteAnalyticsModules(p_DeleteAnalyticsModules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_DeleteAnalyticsModules(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_DeleteAnalyticsModules_rly_xml, NULL, NULL, p_header);
}

int soap_tan_GetAnalyticsModules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAnalyticsModules;
	tan_GetAnalyticsModules_REQ req;
	tan_GetAnalyticsModules_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetAnalyticsModules = xml_node_soap_get(p_body, "GetAnalyticsModules");
	assert(p_GetAnalyticsModules);	

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tan_GetAnalyticsModules(p_GetAnalyticsModules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_GetAnalyticsModules(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_GetAnalyticsModules_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tan_ModifyAnalyticsModules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyAnalyticsModules;
	tan_ModifyAnalyticsModules_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_ModifyAnalyticsModules = xml_node_soap_get(p_body, "ModifyAnalyticsModules");
	assert(p_ModifyAnalyticsModules);	

	memset(&req, 0, sizeof(req));

	ret = parse_tan_ModifyAnalyticsModules(p_ModifyAnalyticsModules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_ModifyAnalyticsModules(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_ModifyAnalyticsModules_rly_xml, NULL, NULL, p_header);
}

int soap_tan_GetRuleOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetRuleOptions;
	tan_GetRuleOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetRuleOptions = xml_node_soap_get(p_body, "GetRuleOptions");
	assert(p_GetRuleOptions);	

	memset(&req, 0, sizeof(req));

	ret = parse_tan_GetRuleOptions(p_GetRuleOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_GetRuleOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tan_GetSupportedAnalyticsModules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSupportedAnalyticsModules;
	tan_GetSupportedAnalyticsModules_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetSupportedAnalyticsModules = xml_node_soap_get(p_body, "GetSupportedAnalyticsModules");
	assert(p_GetSupportedAnalyticsModules);	

	memset(&req, 0, sizeof(req));

	ret = parse_tan_GetSupportedAnalyticsModules(p_GetSupportedAnalyticsModules, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_GetSupportedAnalyticsModules_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tan_GetAnalyticsModuleOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAnalyticsModuleOptions;
	tan_GetAnalyticsModuleOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAnalyticsModuleOptions = xml_node_soap_get(p_body, "GetAnalyticsModuleOptions");
	assert(p_GetAnalyticsModuleOptions);	

	memset(&req, 0, sizeof(req));

	ret = parse_tan_GetAnalyticsModuleOptions(p_GetAnalyticsModuleOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tan_GetAnalyticsModuleOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tan_GetSupportedMetadata(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int i;
	ONVIF_RET ret;
	XMLN * p_GetSupportedMetadata;
	tan_GetSupportedMetadata_REQ req;
	tan_GetSupportedMetadata_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetSupportedMetadata = xml_node_soap_get(p_body, "GetSupportedMetadata");
	assert(p_GetSupportedMetadata);	

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tan_GetSupportedMetadata(p_GetSupportedMetadata, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tan_GetSupportedMetadata(&req, &res);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tan_GetSupportedMetadata_rly_xml, (char *)&res, NULL, p_header);

	for (i = 0; i < res.sizeAnalyticsModule; i++)
	{
		if (res.AnalyticsModule[i].Frame)
		{
			free(res.AnalyticsModule[i].Frame);
		}
	}

	return ret;
}

#endif // end of VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT

int soap_tse_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Search;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tse_GetRecordingSummary(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	tse_GetRecordingSummary_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	memset(&res, 0, sizeof(res));
	
	ret = onvif_tse_GetRecordingSummary(&res);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetRecordingSummary_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tse_GetRecordingInformation(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetRecordingInformation;
	tse_GetRecordingInformation_REQ req;
	tse_GetRecordingInformation_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetRecordingInformation = xml_node_soap_get(p_body, "GetRecordingInformation");
	assert(p_GetRecordingInformation);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tse_GetRecordingInformation(p_GetRecordingInformation, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_GetRecordingInformation(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetRecordingInformation_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tse_GetMediaAttributes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetMediaAttributes;
	tse_GetMediaAttributes_REQ req;
	tse_GetMediaAttributes_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetMediaAttributes = xml_node_soap_get(p_body, "GetMediaAttributes");
	assert(p_GetMediaAttributes);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tse_GetMediaAttributes(p_GetMediaAttributes, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_GetMediaAttributes(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetMediaAttributes_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tse_FindRecordings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_FindRecordings;
	tse_FindRecordings_REQ req;
	tse_FindRecordings_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_FindRecordings = xml_node_soap_get(p_body, "FindRecordings");
	assert(p_FindRecordings);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tse_FindRecordings(p_FindRecordings, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_FindRecordings(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_FindRecordings_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tse_GetRecordingSearchResults(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetRecordingSearchResults;
	tse_GetRecordingSearchResults_REQ req;
	tse_GetRecordingSearchResults_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetRecordingSearchResults = xml_node_soap_get(p_body, "GetRecordingSearchResults");
	assert(p_GetRecordingSearchResults);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tse_GetRecordingSearchResults(p_GetRecordingSearchResults, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_GetRecordingSearchResults(&req, &res);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetRecordingSearchResults_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_RecordingInformations(&res.ResultList.RecordInformation);

	return ret;	
}

int soap_tse_FindEvents(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_FindEvents;
	tse_FindEvents_REQ req;
	tse_FindEvents_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_FindEvents = xml_node_soap_get(p_body, "FindEvents");
	assert(p_FindEvents);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tse_FindEvents(p_FindEvents, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_FindEvents(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_FindEvents_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tse_GetEventSearchResults(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetEventSearchResults;
	tse_GetEventSearchResults_REQ req;
	tse_GetEventSearchResults_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetEventSearchResults = xml_node_soap_get(p_body, "GetEventSearchResults");
	assert(p_GetEventSearchResults);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tse_GetEventSearchResults(p_GetEventSearchResults, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_GetEventSearchResults(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetEventSearchResults_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_FindEventResults(&res.ResultList.Result);

	return ret;
}

int soap_tse_FindMetadata(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_FindMetadata;
	tse_FindMetadata_REQ req;
	tse_FindMetadata_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_FindMetadata = xml_node_soap_get(p_body, "FindMetadata");
	assert(p_FindMetadata);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tse_FindMetadata(p_FindMetadata, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_FindMetadata(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_FindMetadata_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tse_GetMetadataSearchResults(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetMetadataSearchResults;
	tse_GetMetadataSearchResults_REQ req;
	tse_GetMetadataSearchResults_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetMetadataSearchResults = xml_node_soap_get(p_body, "GetMetadataSearchResults");
	assert(p_GetMetadataSearchResults);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tse_GetMetadataSearchResults(p_GetMetadataSearchResults, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_GetMetadataSearchResults(&req, &res);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetMetadataSearchResults_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_FindMetadataResults(&res.ResultList.Result);

	return ret;	
}

int soap_tse_EndSearch(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_EndSearch;
	tse_EndSearch_REQ req;
	tse_EndSearch_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_EndSearch = xml_node_soap_get(p_body, "EndSearch");
	assert(p_EndSearch);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tse_EndSearch(p_EndSearch, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_EndSearch(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_EndSearch_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tse_GetSearchState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSearchState;
	tse_GetSearchState_REQ req;
	tse_GetSearchState_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetSearchState = xml_node_soap_get(p_body, "GetSearchState");
	assert(p_GetSearchState);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tse_GetSearchState(p_GetSearchState, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_GetSearchState(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetSearchState_rly_xml, (char *)&res, NULL, p_header);	
}

#ifdef PTZ_SUPPORT

int soap_tse_FindPTZPosition(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_FindPTZPosition;
	tse_FindPTZPosition_REQ req;
	tse_FindPTZPosition_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_FindPTZPosition = xml_node_soap_get(p_body, "FindPTZPosition");
	assert(p_FindPTZPosition);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tse_FindPTZPosition(p_FindPTZPosition, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_FindPTZPosition(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tse_FindPTZPosition_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tse_GetPTZPositionSearchResults(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetPTZPositionSearchResults;
	tse_GetPTZPositionSearchResults_REQ req;
	tse_GetPTZPositionSearchResults_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetPTZPositionSearchResults = xml_node_soap_get(p_body, "GetPTZPositionSearchResults");
	assert(p_GetPTZPositionSearchResults);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tse_GetPTZPositionSearchResults(p_GetPTZPositionSearchResults, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tse_GetPTZPositionSearchResults(&req, &res);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tse_GetPTZPositionSearchResults_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_FindPTZPositionResults(&res.ResultList.Result);

	return ret;
}

#endif // PTZ_SUPPORT

int soap_trc_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Recording;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_trc_CreateRecording(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateRecording;
	trc_CreateRecording_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateRecording = xml_node_soap_get(p_body, "CreateRecording");
	assert(p_CreateRecording);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_CreateRecording(p_CreateRecording, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_CreateRecording(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_CreateRecording_rly_xml, req.RecordingToken, NULL, p_header);	
}

int soap_trc_DeleteRecording(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_NoRecording;
	XMLN * p_DeleteRecording;
	XMLN * p_RecordingToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_DeleteRecording = xml_node_soap_get(p_body, "DeleteRecording");
	assert(p_DeleteRecording);

	p_RecordingToken = xml_node_soap_get(p_DeleteRecording, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		ret = onvif_trc_DeleteRecording(p_RecordingToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_DeleteRecording_rly_xml, NULL, NULL, p_header);
}

int soap_trc_GetRecordings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trc_GetRecordings_rly_xml, NULL, NULL, p_header);
}

int soap_trc_SetRecordingConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRecordingConfiguration;
	trc_SetRecordingConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetRecordingConfiguration = xml_node_soap_get(p_body, "SetRecordingConfiguration");
	assert(p_SetRecordingConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_SetRecordingConfiguration(p_SetRecordingConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_SetRecordingConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_SetRecordingConfiguration_rly_xml, NULL, NULL, p_header);	
}

int soap_trc_GetRecordingConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_NoRecording;
	XMLN * p_GetRecordingConfiguration;
	XMLN * p_RecordingToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetRecordingConfiguration = xml_node_soap_get(p_body, "GetRecordingConfiguration");
	assert(p_GetRecordingConfiguration);

	p_RecordingToken = xml_node_soap_get(p_GetRecordingConfiguration, "RecordingToken");
	if (p_RecordingToken)
	{
		if (p_RecordingToken->data)
		{
			return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trc_GetRecordingConfiguration_rly_xml, p_RecordingToken->data, NULL, p_header);
		}
		else
		{
			return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trc_GetRecordingConfiguration_rly_xml, "", NULL, p_header);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_trc_CreateTrack(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateTrack;
	trc_CreateTrack_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateTrack = xml_node_soap_get(p_body, "CreateTrack");
	assert(p_CreateTrack);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_CreateTrack(p_CreateTrack, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_CreateTrack(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_CreateTrack_rly_xml, req.TrackToken, NULL, p_header);	
}

int soap_trc_DeleteTrack(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteTrack;
	trc_DeleteTrack_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_DeleteTrack = xml_node_soap_get(p_body, "DeleteTrack");
	assert(p_DeleteTrack);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_DeleteTrack(p_DeleteTrack, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_DeleteTrack(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_DeleteTrack_rly_xml, req.TrackToken, NULL, p_header);	
}

int soap_trc_GetTrackConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetTrackConfiguration;
	trc_GetTrackConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetTrackConfiguration = xml_node_soap_get(p_body, "GetTrackConfiguration");
	assert(p_GetTrackConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_GetTrackConfiguration(p_GetTrackConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_GetTrackConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_trc_SetTrackConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetTrackConfiguration;
	trc_SetTrackConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetTrackConfiguration = xml_node_soap_get(p_body, "SetTrackConfiguration");
	assert(p_SetTrackConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_SetTrackConfiguration(p_SetTrackConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_SetTrackConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_SetTrackConfiguration_rly_xml, NULL, NULL, p_header);	
}

int soap_trc_CreateRecordingJob(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateRecordingJob;
	trc_CreateRecordingJob_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateRecordingJob = xml_node_soap_get(p_body, "CreateRecordingJob");
	assert(p_CreateRecordingJob);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_CreateRecordingJob(p_CreateRecordingJob, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_CreateRecordingJob(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_CreateRecordingJob_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_trc_DeleteRecordingJob(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_NoRecordingJob;
	XMLN * p_DeleteRecordingJob;
	XMLN * p_JobToken;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteRecordingJob = xml_node_soap_get(p_body, "DeleteRecordingJob");
	assert(p_DeleteRecordingJob);

	p_JobToken = xml_node_soap_get(p_DeleteRecordingJob, "JobToken");
	if (p_JobToken && p_JobToken->data)
	{	
		ret = onvif_trc_DeleteRecordingJob(p_JobToken->data);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_DeleteRecordingJob_rly_xml, NULL, NULL, p_header);
}

int soap_trc_GetRecordingJobs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trc_GetRecordingJobs_rly_xml, NULL, NULL, p_header);
}

int soap_trc_SetRecordingJobConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRecordingJobConfiguration;
	trc_SetRecordingJobConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetRecordingJobConfiguration = xml_node_soap_get(p_body, "SetRecordingJobConfiguration");
	assert(p_SetRecordingJobConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_SetRecordingJobConfiguration(p_SetRecordingJobConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_SetRecordingJobConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_SetRecordingJobConfiguration_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_trc_GetRecordingJobConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_NoRecordingJob;
	XMLN * p_GetRecordingJobConfiguration;
	XMLN * p_JobToken;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetRecordingJobConfiguration = xml_node_soap_get(p_body, "GetRecordingJobConfiguration");
	assert(p_GetRecordingJobConfiguration);

	p_JobToken = xml_node_soap_get(p_GetRecordingJobConfiguration, "JobToken");
	if (p_JobToken && p_JobToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_trc_GetRecordingJobConfiguration_rly_xml, p_JobToken->data, NULL, p_header);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_trc_SetRecordingJobMode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRecordingJobMode;
	trc_SetRecordingJobMode_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetRecordingJobMode = xml_node_soap_get(p_body, "SetRecordingJobMode");
	assert(p_SetRecordingJobMode);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trc_SetRecordingJobMode(p_SetRecordingJobMode, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_SetRecordingJobMode(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_SetRecordingJobMode_rly_xml, NULL, NULL, p_header);	
}

int soap_trc_GetRecordingJobState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_NoRecordingJob;
	XMLN * p_GetRecordingJobState;
	XMLN * p_JobToken;
	onvif_RecordingJobStateInformation state;

	log_print(HT_LOG_DBG, "\n");

	p_GetRecordingJobState = xml_node_soap_get(p_body, "GetRecordingJobState");
	assert(p_GetRecordingJobState);

	memset(&state, 0, sizeof(state));

	p_JobToken = xml_node_soap_get(p_GetRecordingJobState, "JobToken");
	if (p_JobToken && p_JobToken->data)
	{
		ret = onvif_trc_GetRecordingJobState(p_JobToken->data, &state);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_GetRecordingJobState_rly_xml, (char*)&state, NULL, p_header);
}

int soap_trc_GetRecordingOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_ERR_NoRecording;
	XMLN * p_GetRecordingOptions;
	XMLN * p_RecordingToken;
	onvif_RecordingOptions options;

	log_print(HT_LOG_DBG, "\n");

	p_GetRecordingOptions = xml_node_soap_get(p_body, "GetRecordingOptions");
	assert(p_GetRecordingOptions);

	memset(&options, 0, sizeof(options));

	p_RecordingToken = xml_node_soap_get(p_GetRecordingOptions, "RecordingToken");
	if (p_RecordingToken)
	{
		if (p_RecordingToken->data)
		{
			ret = onvif_trc_GetRecordingOptions(p_RecordingToken->data, &options);
		}
		else
		{
			ret = onvif_trc_GetRecordingOptions("", &options);
		}
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_GetRecordingOptions_rly_xml, (char *)&options, NULL, p_header);
}

int soap_trc_ExportRecordedData(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ExportRecordedData;
	trc_ExportRecordedData_REQ req;
	trc_ExportRecordedData_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_ExportRecordedData = xml_node_soap_get(p_body, "ExportRecordedData");
	assert(p_ExportRecordedData);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_trc_ExportRecordedData(p_ExportRecordedData, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_ExportRecordedData(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_ExportRecordedData_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_trc_StopExportRecordedData(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_StopExportRecordedData;
	trc_StopExportRecordedData_REQ req;
	trc_StopExportRecordedData_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_StopExportRecordedData = xml_node_soap_get(p_body, "StopExportRecordedData");
	assert(p_StopExportRecordedData);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_trc_StopExportRecordedData(p_StopExportRecordedData, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_StopExportRecordedData(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_StopExportRecordedData_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_trc_GetExportRecordedDataState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetExportRecordedDataState;
	trc_GetExportRecordedDataState_REQ req;
	trc_GetExportRecordedDataState_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetExportRecordedDataState = xml_node_soap_get(p_body, "GetExportRecordedDataState");
	assert(p_GetExportRecordedDataState);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_trc_GetExportRecordedDataState(p_GetExportRecordedDataState, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trc_GetExportRecordedDataState(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trc_GetExportRecordedDataState_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_trp_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Replay;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_trp_GetReplayUri(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetReplayUri;
	trp_GetReplayUri_REQ req;
	trp_GetReplayUri_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetReplayUri = xml_node_soap_get(p_body, "GetReplayUri");
	assert(p_GetReplayUri);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_trp_GetReplayUri(p_GetReplayUri, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trp_GetReplayUri(p_user, &req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trp_GetReplayUri_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_trp_GetReplayConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;	
	trp_GetReplayConfiguration_RES res;

	log_print(HT_LOG_DBG, "\n");
	
	memset(&res, 0, sizeof(res));

	ret = onvif_trp_GetReplayConfiguration(&res);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trp_GetReplayConfiguration_rly_xml, (char *)&res, NULL, p_header);
}

int soap_trp_SetReplayConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetReplayConfiguration;
	trp_SetReplayConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetReplayConfiguration = xml_node_soap_get(p_body, "SetReplayConfiguration");
	assert(p_SetReplayConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_trp_SetReplayConfiguration(p_SetReplayConfiguration, &req);
	if (ONVIF_OK == ret)
	{	
		ret = onvif_trp_SetReplayConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_trp_SetReplayConfiguration_rly_xml, NULL, NULL, p_header);	
}

#endif	// end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

int soap_tac_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_AccessControl;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tac_GetAccessPointList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessPointList;
	tac_GetAccessPointList_REQ req;
	tac_GetAccessPointList_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAccessPointList = xml_node_soap_get(p_body, "GetAccessPointList");
	assert(p_GetAccessPointList);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tac_GetAccessPointList(p_GetAccessPointList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_GetAccessPointList(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAccessPointList_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_AccessPoints(&res.AccessPoint);

	return ret;
}

int soap_tac_GetAccessPoints(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessPoints;
	tac_GetAccessPoints_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAccessPoints = xml_node_soap_get(p_body, "GetAccessPoints");
	assert(p_GetAccessPoints);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tac_GetAccessPoints(p_GetAccessPoints, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAccessPoints_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tac_CreateAccessPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateAccessPoint;
	tac_CreateAccessPoint_REQ req;
	tac_CreateAccessPoint_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateAccessPoint = xml_node_soap_get(p_body, "CreateAccessPoint");
	assert(p_CreateAccessPoint);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tac_CreateAccessPoint(p_CreateAccessPoint, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_CreateAccessPoint(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_CreateAccessPoint_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tac_SetAccessPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAccessPoint;
	tac_SetAccessPoint_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAccessPoint = xml_node_soap_get(p_body, "SetAccessPoint");
	assert(p_SetAccessPoint);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_SetAccessPoint(p_SetAccessPoint, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_SetAccessPoint(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_SetAccessPoint_rly_xml, NULL, NULL, p_header);	
}

int soap_tac_ModifyAccessPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyAccessPoint;
	tac_ModifyAccessPoint_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_ModifyAccessPoint = xml_node_soap_get(p_body, "ModifyAccessPoint");
	assert(p_ModifyAccessPoint);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_ModifyAccessPoint(p_ModifyAccessPoint, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_ModifyAccessPoint(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_ModifyAccessPoint_rly_xml, NULL, NULL, p_header);	
}

int soap_tac_DeleteAccessPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteAccessPoint;
	tac_DeleteAccessPoint_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteAccessPoint = xml_node_soap_get(p_body, "DeleteAccessPoint");
	assert(p_DeleteAccessPoint);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_DeleteAccessPoint(p_DeleteAccessPoint, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_DeleteAccessPoint(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_DeleteAccessPoint_rly_xml, NULL, NULL, p_header);	
}

int soap_tac_GetAccessPointInfoList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessPointInfoList;
	tac_GetAccessPointInfoList_REQ req;
	tac_GetAccessPointInfoList_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAccessPointInfoList = xml_node_soap_get(p_body, "GetAccessPointInfoList");
	assert(p_GetAccessPointInfoList);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tac_GetAccessPointInfoList(p_GetAccessPointInfoList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_GetAccessPointInfoList(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAccessPointInfoList_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_AccessPoints(&res.AccessPointInfo);
	
	return ret;
}

int soap_tac_GetAccessPointInfo(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessPointInfo;
	tac_GetAccessPointInfo_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAccessPointInfo = xml_node_soap_get(p_body, "GetAccessPointInfo");
	assert(p_GetAccessPointInfo);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tac_GetAccessPointInfo(p_GetAccessPointInfo, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAccessPointInfo_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tac_GetAreaList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAreaList;
	tac_GetAreaList_REQ req;
	tac_GetAreaList_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAreaList = xml_node_soap_get(p_body, "GetAreaList");
	assert(p_GetAreaList);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tac_GetAreaList(p_GetAreaList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_GetAreaList(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAreaList_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_Areas(&res.Area);
	
	return ret;
}

int soap_tac_GetAreas(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAreas;
	tac_GetAreas_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAreas = xml_node_soap_get(p_body, "GetAreas");
	assert(p_GetAreas);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_GetAreas(p_GetAreas, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAreas_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tac_CreateArea(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateArea;
	tac_CreateArea_REQ req;
	tac_CreateArea_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_CreateArea = xml_node_soap_get(p_body, "CreateArea");
	assert(p_CreateArea);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tac_CreateArea(p_CreateArea, &req);
	if (ONVIF_OK == ret)
	{
	    ret = onvif_tac_CreateArea(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_CreateArea_rly_xml, (char *)&res, NULL, p_header);	
}

int soap_tac_SetArea(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetArea;
	tac_SetArea_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetArea = xml_node_soap_get(p_body, "SetArea");
	assert(p_SetArea);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_SetArea(p_SetArea, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_SetArea(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_SetArea_rly_xml, NULL, NULL, p_header);	
}

int soap_tac_ModifyArea(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyArea;
	tac_ModifyArea_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_ModifyArea = xml_node_soap_get(p_body, "ModifyArea");
	assert(p_ModifyArea);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_ModifyArea(p_ModifyArea, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_ModifyArea(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_ModifyArea_rly_xml, NULL, NULL, p_header);	
}

int soap_tac_DeleteArea(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteArea;
	tac_DeleteArea_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteArea = xml_node_soap_get(p_body, "DeleteArea");
	assert(p_DeleteArea);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_DeleteArea(p_DeleteArea, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_DeleteArea(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_DeleteArea_rly_xml, NULL, NULL, p_header);	
}

int soap_tac_GetAreaInfoList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAreaInfoList;
	tac_GetAreaInfoList_REQ req;
	tac_GetAreaInfoList_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAreaInfoList = xml_node_soap_get(p_body, "GetAreaInfoList");
	assert(p_GetAreaInfoList);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tac_GetAreaInfoList(p_GetAreaInfoList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_GetAreaInfoList(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAreaInfoList_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_Areas(&res.AreaInfo);
	
	return ret;
}

int soap_tac_GetAreaInfo(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAreaInfo;
	tac_GetAreaInfo_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAreaInfo = xml_node_soap_get(p_body, "GetAreaInfo");
	assert(p_GetAreaInfo);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tac_GetAreaInfo(p_GetAreaInfo, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAreaInfo_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tac_GetAccessPointState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessPointState;
	tac_GetAccessPointState_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAccessPointState = xml_node_soap_get(p_body, "GetAccessPointState");
	assert(p_GetAccessPointState);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_GetAccessPointState(p_GetAccessPointState, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_GetAccessPointState_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tac_EnableAccessPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_EnableAccessPoint;
	tac_EnableAccessPoint_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_EnableAccessPoint = xml_node_soap_get(p_body, "EnableAccessPoint");
	assert(p_EnableAccessPoint);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tac_EnableAccessPoint(p_EnableAccessPoint, &req);
	if (ONVIF_OK == ret)
	{
	    ret = onvif_tac_EnableAccessPoint(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_EnableAccessPoint_rly_xml, NULL, NULL, p_header);
}

int soap_tac_DisableAccessPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DisableAccessPoint;
	tac_DisableAccessPoint_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DisableAccessPoint = xml_node_soap_get(p_body, "DisableAccessPoint");
	assert(p_DisableAccessPoint);

	memset(&req, 0, sizeof(req));

	ret = parse_tac_DisableAccessPoint(p_DisableAccessPoint, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tac_DisableAccessPoint(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tac_DisableAccessPoint_rly_xml, NULL, NULL, p_header);
}

int soap_tdc_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_DoorControl;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tdc_GetDoorList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetDoorList;
	tdc_GetDoorList_REQ req;
	tdc_GetDoorList_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetDoorList = xml_node_soap_get(p_body, "GetDoorList");
	assert(p_GetDoorList);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tdc_GetDoorList(p_GetDoorList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_GetDoorList(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tdc_GetDoorList_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_Doors(&res.Door);
	
	return ret;
}

int soap_tdc_GetDoors(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetDoors;
	tdc_GetDoors_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetDoors = xml_node_soap_get(p_body, "GetDoors");
	assert(p_GetDoors);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_GetDoors(p_GetDoors, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_GetDoors_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tdc_CreateDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateDoor;
	tdc_CreateDoor_REQ req;
	tdc_CreateDoor_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_CreateDoor = xml_node_soap_get(p_body, "CreateDoor");
	assert(p_CreateDoor);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tdc_CreateDoor(p_CreateDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_CreateDoor(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_CreateDoor_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tdc_SetDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetDoor;
	tdc_SetDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetDoor = xml_node_soap_get(p_body, "SetDoor");
	assert(p_SetDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_SetDoor(p_SetDoor, &req);
	if (ONVIF_OK == ret)
	{
	ret = onvif_tdc_SetDoor(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_SetDoor_rly_xml, NULL, NULL, p_header);
}

int soap_tdc_ModifyDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyDoor;
	tdc_ModifyDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_ModifyDoor = xml_node_soap_get(p_body, "ModifyDoor");
	assert(p_ModifyDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_ModifyDoor(p_ModifyDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_ModifyDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_ModifyDoor_rly_xml, NULL, NULL, p_header);	
}

int soap_tdc_DeleteDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteDoor;
	tdc_DeleteDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_DeleteDoor = xml_node_soap_get(p_body, "DeleteDoor");
	assert(p_DeleteDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_DeleteDoor(p_DeleteDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_DeleteDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_DeleteDoor_rly_xml, NULL, NULL, p_header);	
}

int soap_tdc_GetDoorInfoList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetDoorInfoList;
	tdc_GetDoorInfoList_REQ req;
	tdc_GetDoorInfoList_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetDoorInfoList = xml_node_soap_get(p_body, "GetDoorInfoList");
	assert(p_GetDoorInfoList);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));
	
	ret = parse_tdc_GetDoorInfoList(p_GetDoorInfoList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_GetDoorInfoList(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tdc_GetDoorInfoList_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_DoorInfos(&res.DoorInfo);

	return ret;
}

int soap_tdc_GetDoorInfo(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetDoorInfo;
	tdc_GetDoorInfo_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetDoorInfo = xml_node_soap_get(p_body, "GetDoorInfo");
	assert(p_GetDoorInfo);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_GetDoorInfo(p_GetDoorInfo, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_GetDoorInfo_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tdc_GetDoorState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetDoorState;
	tdc_GetDoorState_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetDoorState = xml_node_soap_get(p_body, "GetDoorState");
	assert(p_GetDoorState);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_GetDoorState(p_GetDoorState, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_GetDoorState_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tdc_AccessDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AccessDoor;
	tdc_AccessDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_AccessDoor = xml_node_soap_get(p_body, "AccessDoor");
	assert(p_AccessDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_AccessDoor(p_AccessDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_AccessDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_AccessDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_LockDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_LockDoor;
	tdc_LockDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_LockDoor = xml_node_soap_get(p_body, "LockDoor");
	assert(p_LockDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_LockDoor(p_LockDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_LockDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_LockDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_UnlockDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_UnlockDoor;
	tdc_UnlockDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_UnlockDoor = xml_node_soap_get(p_body, "UnlockDoor");
	assert(p_UnlockDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_UnlockDoor(p_UnlockDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_UnlockDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_UnlockDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_DoubleLockDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DoubleLockDoor;
	tdc_DoubleLockDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_DoubleLockDoor = xml_node_soap_get(p_body, "DoubleLockDoor");
	assert(p_DoubleLockDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_DoubleLockDoor(p_DoubleLockDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_DoubleLockDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_DoubleLockDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_BlockDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_BlockDoor;
	tdc_BlockDoor_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_BlockDoor = xml_node_soap_get(p_body, "BlockDoor");
	assert(p_BlockDoor);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tdc_BlockDoor(p_BlockDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_BlockDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_BlockDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_LockDownDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_LockDownDoor;
	tdc_LockDownDoor_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_LockDownDoor = xml_node_soap_get(p_body, "LockDownDoor");
	assert(p_LockDownDoor);

	memset(&req, 0, sizeof(req));

	ret = parse_tdc_LockDownDoor(p_LockDownDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_LockDownDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_LockDownDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_LockDownReleaseDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_LockDownReleaseDoor;
	tdc_LockDownReleaseDoor_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_LockDownReleaseDoor = xml_node_soap_get(p_body, "LockDownReleaseDoor");
	assert(p_LockDownReleaseDoor);

	memset(&req, 0, sizeof(req));

	ret = parse_tdc_LockDownReleaseDoor(p_LockDownReleaseDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_LockDownReleaseDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_LockDownReleaseDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_LockOpenDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_LockOpenDoor;
	tdc_LockOpenDoor_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_LockOpenDoor = xml_node_soap_get(p_body, "LockOpenDoor");
	assert(p_LockOpenDoor);

	memset(&req, 0, sizeof(req));

	ret = parse_tdc_LockOpenDoor(p_LockOpenDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_LockOpenDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_LockOpenDoor_rly_xml, (char *)&req, NULL, p_header);	
}

int soap_tdc_LockOpenReleaseDoor(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_LockOpenReleaseDoor;
	tdc_LockOpenReleaseDoor_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_LockOpenReleaseDoor = xml_node_soap_get(p_body, "LockOpenReleaseDoor");
	assert(p_LockOpenReleaseDoor);

	memset(&req, 0, sizeof(req));

	ret = parse_tdc_LockOpenReleaseDoor(p_LockOpenReleaseDoor, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tdc_LockOpenReleaseDoor(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tdc_LockOpenReleaseDoor_rly_xml, (char *)&req, NULL, p_header);	
}

#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT

int soap_tmd_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_DeviceIO;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tmd_GetVideoOutputs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
	
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tmd_GetVideoOutputs_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetVideoOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoOutputConfiguration;
	tmd_GetVideoOutputConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetVideoOutputConfiguration = xml_node_soap_get(p_body, "GetVideoOutputConfiguration");
	assert(p_GetVideoOutputConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_GetVideoOutputConfiguration(p_GetVideoOutputConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetVideoOutputConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_SetVideoOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoOutputConfiguration;
	tmd_SetVideoOutputConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetVideoOutputConfiguration = xml_node_soap_get(p_body, "SetVideoOutputConfiguration");
	assert(p_SetVideoOutputConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_SetVideoOutputConfiguration(p_SetVideoOutputConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetVideoOutputConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_SetVideoOutputConfiguration_rly_xml, NULL, NULL, p_header);	
}

int soap_tmd_GetVideoOutputConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoOutputConfigurationOptions;
	tmd_GetVideoOutputConfigurationOptions_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetVideoOutputConfigurationOptions = xml_node_soap_get(p_body, "GetVideoOutputConfigurationOptions");
	assert(p_GetVideoOutputConfigurationOptions);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_GetVideoOutputConfigurationOptions(p_GetVideoOutputConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetVideoOutputConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_GetAudioOutputs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tmd_GetAudioOutputs_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetAudioOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioOutputConfiguration;
	tmd_GetAudioOutputConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioOutputConfiguration = xml_node_soap_get(p_body, "GetAudioOutputConfiguration");
	assert(p_GetAudioOutputConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tmd_GetAudioOutputConfiguration(p_GetAudioOutputConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetAudioOutputConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_GetAudioOutputConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioOutputConfigurationOptions;
	tmd_GetAudioOutputConfigurationOptions_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetAudioOutputConfigurationOptions = xml_node_soap_get(p_body, "GetAudioOutputConfigurationOptions");
	assert(p_GetAudioOutputConfigurationOptions);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_GetAudioOutputConfigurationOptions(p_GetAudioOutputConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetAudioOutputConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_GetRelayOutputs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tmd_GetRelayOutputs_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetRelayOutputOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetRelayOutputOptions;
	tmd_GetRelayOutputOptions_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetRelayOutputOptions = xml_node_soap_get(p_body, "GetRelayOutputOptions");
	assert(p_GetRelayOutputOptions);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_GetRelayOutputOptions(p_GetRelayOutputOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetRelayOutputOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_SetRelayOutputSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRelayOutputSettings;
	tmd_SetRelayOutputSettings_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetRelayOutputSettings = xml_node_soap_get(p_body, "SetRelayOutputSettings");
	assert(p_SetRelayOutputSettings);

	memset(&req, 0, sizeof(req));

	ret = parse_tmd_SetRelayOutputSettings(p_SetRelayOutputSettings, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetRelayOutputSettings(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_SetRelayOutputSettings_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_SetRelayOutputState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRelayOutputState;
	tmd_SetRelayOutputState_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetRelayOutputState = xml_node_soap_get(p_body, "SetRelayOutputState");
	assert(p_SetRelayOutputState);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_SetRelayOutputState(p_SetRelayOutputState, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetRelayOutputState(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_SetRelayOutputState_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetDigitalInputs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tmd_GetDigitalInputs_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetDigitalInputConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetDigitalInputConfigurationOptions;
	tmd_GetDigitalInputConfigurationOptions_REQ req;
	
	
	log_print(HT_LOG_DBG, "\n");

	p_GetDigitalInputConfigurationOptions = xml_node_soap_get(p_body, "GetDigitalInputConfigurationOptions");
	assert(p_GetDigitalInputConfigurationOptions);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_GetDigitalInputConfigurationOptions(p_GetDigitalInputConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetDigitalInputConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_SetDigitalInputConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetDigitalInputConfigurations;
	tmd_SetDigitalInputConfigurations_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_SetDigitalInputConfigurations = xml_node_soap_get(p_body, "SetDigitalInputConfigurations");
	assert(p_SetDigitalInputConfigurations);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_SetDigitalInputConfigurations(p_SetDigitalInputConfigurations, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetDigitalInputConfigurations(&req);
	}

	onvif_free_DigitalInputs(&req.DigitalInputs);
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_SetDigitalInputConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetSerialPorts(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tmd_GetSerialPorts_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetSerialPortConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSerialPortConfiguration;
	tmd_GetSerialPortConfiguration_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetSerialPortConfiguration = xml_node_soap_get(p_body, "GetSerialPortConfiguration");
	assert(p_GetSerialPortConfiguration);

	memset(&req, 0, sizeof(req));
	
	ret = parse_tmd_GetSerialPortConfiguration(p_GetSerialPortConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetSerialPortConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_GetSerialPortConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSerialPortConfigurationOptions;
	tmd_GetSerialPortConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetSerialPortConfigurationOptions = xml_node_soap_get(p_body, "GetSerialPortConfigurationOptions");
	assert(p_GetSerialPortConfigurationOptions);

	memset(&req, 0, sizeof(req));

	ret = parse_tmd_GetSerialPortConfigurationOptions(p_GetSerialPortConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_GetSerialPortConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tmd_SetSerialPortConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetSerialPortConfiguration;
	tmd_SetSerialPortConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetSerialPortConfiguration = xml_node_soap_get(p_body, "SetSerialPortConfiguration");
	assert(p_SetSerialPortConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tmd_SetSerialPortConfiguration(p_SetSerialPortConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SetSerialPortConfiguration(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tmd_SetSerialPortConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_SendReceiveSerialCommand(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SendReceiveSerialCommand;
	tmd_SendReceiveSerialCommand_REQ req;
	tmd_SendReceiveSerialCommand_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_SendReceiveSerialCommand = xml_node_soap_get(p_body, "SendReceiveSerialCommand");
	assert(p_SendReceiveSerialCommand);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tmd_SendReceiveSerialCommand(p_SendReceiveSerialCommand, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tmd_SendReceiveSerialCommand(&req, &res);        
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tmd_SendReceiveSerialCommand_rly_xml, (char *)&res, NULL, p_header);

	if (req.Command.SerialDataFlag)
	{
		onvif_free_SerialData(&req.Command.SerialData);
	}

	if (res.SerialDataFlag)
	{
		onvif_free_SerialData(&res.SerialData);
	}

	return ret;	
}

int soap_tmd_GetAudioSources(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tmd_GetAudioSources_rly_xml, NULL, NULL, p_header);
}

int soap_tmd_GetVideoSources(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tmd_GetVideoSources_rly_xml, NULL, NULL, p_header);
}

#endif // end of DEVICEIO_SUPPORT

#ifdef MEDIA2_SUPPORT

int soap_tr2_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Media2;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tr2_GetProfiles(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetProfiles;
	tr2_GetProfiles_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	p_GetProfiles = xml_node_soap_get(p_body, "GetProfiles");
	assert(p_GetProfiles);
	
	memset(&req, 0, sizeof(req));

	ret = parse_tr2_GetProfiles(p_GetProfiles, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetProfiles_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_CreateProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateProfile;
	tr2_CreateProfile_REQ req;
	tr2_CreateProfile_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_CreateProfile = xml_node_soap_get(p_body, "CreateProfile");
	assert(p_CreateProfile);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tr2_CreateProfile(p_CreateProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_CreateProfile(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_CreateProfile_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tr2_DeleteProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteProfile;
	tr2_DeleteProfile_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteProfile = xml_node_soap_get(p_body, "DeleteProfile");
	assert(p_DeleteProfile);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_DeleteProfile(p_DeleteProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_DeleteProfile(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_DeleteProfile_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_AddConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddConfiguration;
	tr2_AddConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_AddConfiguration = xml_node_soap_get(p_body, "AddConfiguration");
	assert(p_AddConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_AddConfiguration(p_AddConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_AddConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_AddConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_RemoveConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveConfiguration;
	tr2_RemoveConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_RemoveConfiguration = xml_node_soap_get(p_body, "RemoveConfiguration");
	assert(p_RemoveConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_RemoveConfiguration(p_RemoveConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_RemoveConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_RemoveConfiguration_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetVideoEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoEncoderConfigurations;
	tr2_GetVideoEncoderConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetVideoEncoderConfigurations = xml_node_soap_get(p_body, "GetVideoEncoderConfigurations");
	assert(p_GetVideoEncoderConfigurations);

	ret = parse_tr2_GetConfiguration(p_GetVideoEncoderConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetVideoEncoderConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetVideoSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoSourceConfigurations;
	tr2_GetVideoSourceConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetVideoSourceConfigurations = xml_node_soap_get(p_body, "GetVideoSourceConfigurations");
	assert(p_GetVideoSourceConfigurations);

	ret = parse_tr2_GetConfiguration(p_GetVideoSourceConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetVideoSourceConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetMetadataConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetMetadataConfigurations;
	tr2_GetMetadataConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetMetadataConfigurations = xml_node_soap_get(p_body, "GetMetadataConfigurations");
	assert(p_GetMetadataConfigurations);

	ret = parse_tr2_GetConfiguration(p_GetMetadataConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetMetadataConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_SetVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoEncoderConfiguration;
	tr2_SetVideoEncoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetVideoEncoderConfiguration = xml_node_soap_get(p_body, "SetVideoEncoderConfiguration");
	assert(p_SetVideoEncoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetVideoEncoderConfiguration(p_SetVideoEncoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetVideoEncoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetVideoEncoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_SetVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoSourceConfiguration;
	tr2_SetVideoSourceConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetVideoSourceConfiguration = xml_node_soap_get(p_body, "SetVideoSourceConfiguration");
	assert(p_SetVideoSourceConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetVideoSourceConfiguration(p_SetVideoSourceConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetVideoSourceConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetVideoSourceConfiguration_rly_xml, NULL, NULL, p_header);    
}

int soap_tr2_SetMetadataConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetMetadataConfiguration;
	tr2_SetMetadataConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetMetadataConfiguration = xml_node_soap_get(p_body, "SetMetadataConfiguration");
	assert(p_SetMetadataConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetMetadataConfiguration(p_SetMetadataConfiguration, &req);
	if (ONVIF_OK == ret)
	{
	    ret = onvif_tr2_SetMetadataConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetMetadataConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_GetVideoSourceConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoSourceConfigurationOptions;
	tr2_GetVideoSourceConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetVideoSourceConfigurationOptions = xml_node_soap_get(p_body, "GetVideoSourceConfigurationOptions");
	assert(p_GetVideoSourceConfigurationOptions);

	ret = parse_tr2_GetConfiguration(p_GetVideoSourceConfigurationOptions, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetVideoSourceConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetVideoEncoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoEncoderConfigurationOptions;
	tr2_GetVideoEncoderConfigurationOptions_REQ req;
	tr2_GetVideoEncoderConfigurationOptions_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetVideoEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetVideoEncoderConfigurationOptions");
	assert(p_GetVideoEncoderConfigurationOptions);

	ret = parse_tr2_GetConfiguration(p_GetVideoEncoderConfigurationOptions, &req.GetConfiguration);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_GetVideoEncoderConfigurationOptions(&req, &res);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetVideoEncoderConfigurationOptions_rly_xml, (char *)&res, NULL, p_header);

	onvif_free_VideoEncoder2ConfigurationOptions(&res.Options);

	return ret;
}

int soap_tr2_GetMetadataConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetMetadataConfigurationOptions;
	tr2_GetMetadataConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetMetadataConfigurationOptions = xml_node_soap_get(p_body, "GetMetadataConfigurationOptions");
	assert(p_GetMetadataConfigurationOptions);

	ret = parse_tr2_GetConfiguration(p_GetMetadataConfigurationOptions, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetMetadataConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetVideoEncoderInstances(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoEncoderInstances;
	tr2_GetVideoEncoderInstances_REQ req;
	tr2_GetVideoEncoderInstances_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetVideoEncoderInstances = xml_node_soap_get(p_body, "GetVideoEncoderInstances");
	assert(p_GetVideoEncoderInstances);

	ret = parse_tr2_GetVideoEncoderInstances(p_GetVideoEncoderInstances, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_GetVideoEncoderInstances(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetVideoEncoderInstances_rly_xml,(char *)&res, NULL, p_header);
}

int soap_tr2_GetStreamUri(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetStreamUri;
	tr2_GetStreamUri_REQ req;
	tr2_GetStreamUri_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetStreamUri = xml_node_soap_get(p_body, "GetStreamUri");
	assert(p_GetStreamUri);

	ret = parse_tr2_GetStreamUri(p_GetStreamUri, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_GetStreamUri(p_user, &req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetStreamUri_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tr2_SetSynchronizationPoint(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetSynchronizationPoint;
	tr2_SetSynchronizationPoint_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetSynchronizationPoint = xml_node_soap_get(p_body, "SetSynchronizationPoint");
	assert(p_SetSynchronizationPoint);

	ret = parse_tr2_SetSynchronizationPoint(p_SetSynchronizationPoint, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetSynchronizationPoint(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetSynchronizationPoint_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetVideoSourceModes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetVideoSourceModes;
	tr2_GetVideoSourceModes_REQ req;

	log_print(HT_LOG_DBG, "\n");
	
	memset(&req, 0, sizeof(req));
	
	p_GetVideoSourceModes = xml_node_soap_get(p_body, "GetVideoSourceModes");
	assert(p_GetVideoSourceModes);

	ret = parse_tr2_GetVideoSourceModes(p_GetVideoSourceModes, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetVideoSourceModes_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_SetVideoSourceMode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetVideoSourceMode;
	tr2_SetVideoSourceMode_REQ req;
	tr2_SetVideoSourceMode_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_SetVideoSourceMode = xml_node_soap_get(p_body, "SetVideoSourceMode");
	assert(p_SetVideoSourceMode);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tr2_SetVideoSourceMode(p_SetVideoSourceMode, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetVideoSourceMode(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetVideoSourceMode_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tr2_GetSnapshotUri(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSnapshotUri;    
	tr2_GetSnapshotUri_REQ req;
	tr2_GetSnapshotUri_RES res;

	log_print(HT_LOG_DBG, "\n");

	p_GetSnapshotUri = xml_node_soap_get(p_body, "GetSnapshotUri");
	assert(p_GetSnapshotUri);

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	ret = parse_tr2_GetSnapshotUri(p_GetSnapshotUri, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_GetSnapshotUri(p_user, &req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetSnapshotUri_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tr2_SetOSD(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetOSD;
	trt_SetOSD_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetOSD = xml_node_soap_get(p_body, "SetOSD");
	assert(p_SetOSD);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_SetOSD(p_SetOSD, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_SetOSD(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetOSD_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_GetOSDOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tr2_GetOSDOptions_rly_xml, NULL, NULL, p_header);	
}

int soap_tr2_GetOSDs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetOSDs;
	tr2_GetOSDs_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetOSDs = xml_node_soap_get(p_body, "GetOSDs");
	assert(p_GetOSDs);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_GetOSDs(p_GetOSDs, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetOSDs_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_CreateOSD(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateOSD;
	trt_CreateOSD_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_CreateOSD = xml_node_soap_get(p_body, "CreateOSD");
	assert(p_CreateOSD);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_CreateOSD(p_CreateOSD, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_CreateOSD(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_CreateOSD_rly_xml, req.OSD.token, NULL, p_header);
}

int soap_tr2_DeleteOSD(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteOSD;
	trt_DeleteOSD_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteOSD = xml_node_soap_get(p_body, "DeleteOSD");
	assert(p_DeleteOSD);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_DeleteOSD(p_DeleteOSD, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trt_DeleteOSD(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_DeleteOSD_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_CreateMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateMask;
	tr2_CreateMask_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_CreateMask = xml_node_soap_get(p_body, "CreateMask");
	assert(p_CreateMask);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_CreateMask(p_CreateMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_CreateMask(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_CreateMask_rly_xml, req.Mask.token, NULL, p_header);
}

int soap_tr2_DeleteMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteMask;
	tr2_DeleteMask_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteMask = xml_node_soap_get(p_body, "DeleteMask");
	assert(p_DeleteMask);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_DeleteMask(p_DeleteMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_DeleteMask(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_DeleteMask_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_GetMasks(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GGetMasks;
	tr2_GetMasks_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GGetMasks = xml_node_soap_get(p_body, "GetMasks");
	assert(p_GGetMasks);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_GetMasks(p_GGetMasks, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetMasks_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_SetMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetMask;
	tr2_SetMask_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetMask = xml_node_soap_get(p_body, "SetMask");
	assert(p_SetMask);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetMask(p_SetMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetMask(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetMask_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_GetMaskOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tr2_GetMaskOptions_rly_xml, NULL, NULL, p_header);	
}

int soap_tr2_StartMulticastStreaming(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_StartMulticastStreaming;
	tr2_StartMulticastStreaming_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_StartMulticastStreaming = xml_node_soap_get(p_body, "StartMulticastStreaming");
	assert(p_StartMulticastStreaming);	

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_StartMulticastStreaming(p_StartMulticastStreaming, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_StartMulticastStreaming(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_StartMulticastStreaming_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_StopMulticastStreaming(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_StopMulticastStreaming;
	tr2_StopMulticastStreaming_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_StopMulticastStreaming = xml_node_soap_get(p_body, "StopMulticastStreaming");
	assert(p_StopMulticastStreaming);	

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_StopMulticastStreaming(p_StopMulticastStreaming, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_StopMulticastStreaming(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_StopMulticastStreaming_rly_xml, NULL, NULL, p_header);
}

#ifdef DEVICEIO_SUPPORT

int soap_tr2_GetAudioOutputConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioOutputConfigurationOptions;
	trt_GetAudioOutputConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioOutputConfigurationOptions = xml_node_soap_get(p_body, "GetAudioOutputConfigurationOptions");
	assert(p_GetAudioOutputConfigurationOptions);

	memset(&req, 0, sizeof(req));

	ret = parse_trt_GetAudioOutputConfigurationOptions(p_GetAudioOutputConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioOutputConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetAudioOutputConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioOutputConfigurations;
	tr2_GetAudioOutputConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioOutputConfigurations = xml_node_soap_get(p_body, "GetAudioOutputConfigurations");
	assert(p_GetAudioOutputConfigurations);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_GetConfiguration(p_GetAudioOutputConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioOutputConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_SetAudioOutputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioOutputConfiguration;
	tr2_SetAudioOutputConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAudioOutputConfiguration = xml_node_soap_get(p_body, "SetAudioOutputConfiguration");
	assert(p_SetAudioOutputConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetAudioOutputConfiguration(p_SetAudioOutputConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetAudioOutputConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetAudioOutputConfiguration_rly_xml, NULL, NULL, p_header);
}

#endif // end of DEVICEIO_SUPPORT

#ifdef AUDIO_SUPPORT

int soap_tr2_GetAudioEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioEncoderConfigurations;
	tr2_GetAudioEncoderConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetAudioEncoderConfigurations = xml_node_soap_get(p_body, "GetAudioEncoderConfigurations");
	assert(p_GetAudioEncoderConfigurations);

	ret = parse_tr2_GetConfiguration(p_GetAudioEncoderConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioEncoderConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetAudioSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioSourceConfigurations;
	tr2_GetAudioSourceConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetAudioSourceConfigurations = xml_node_soap_get(p_body, "GetAudioSourceConfigurations");
	assert(p_GetAudioSourceConfigurations);

	ret = parse_tr2_GetConfiguration(p_GetAudioSourceConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioSourceConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_SetAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioEncoderConfiguration;
	tr2_SetAudioEncoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAudioEncoderConfiguration = xml_node_soap_get(p_body, "SetAudioEncoderConfiguration");
	assert(p_SetAudioEncoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetAudioEncoderConfiguration(p_SetAudioEncoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetAudioEncoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetAudioEncoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_GetAudioSourceConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioSourceConfigurationOptions;
	tr2_GetAudioSourceConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetAudioSourceConfigurationOptions = xml_node_soap_get(p_body, "GetAudioSourceConfigurationOptions");
	assert(p_GetAudioSourceConfigurationOptions);

	ret = parse_tr2_GetConfiguration(p_GetAudioSourceConfigurationOptions, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioSourceConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_GetAudioEncoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioEncoderConfigurationOptions;
	tr2_GetAudioEncoderConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetAudioEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetAudioEncoderConfigurationOptions");
	assert(p_GetAudioEncoderConfigurationOptions);

	ret = parse_tr2_GetConfiguration(p_GetAudioEncoderConfigurationOptions, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioEncoderConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tr2_SetAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioSourceConfiguration;
	tr2_SetAudioSourceConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAudioSourceConfiguration = xml_node_soap_get(p_body, "SetAudioSourceConfiguration");
	assert(p_SetAudioSourceConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetAudioSourceConfiguration(p_SetAudioSourceConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetAudioSourceConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetAudioSourceConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_GetAudioDecoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioDecoderConfigurations;
	tr2_GetAudioDecoderConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioDecoderConfigurations = xml_node_soap_get(p_body, "GetAudioDecoderConfigurations");
	assert(p_GetAudioDecoderConfigurations);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_GetConfiguration(p_GetAudioDecoderConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioDecoderConfigurations_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tr2_SetAudioDecoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAudioDecoderConfiguration;
	tr2_SetAudioDecoderConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_SetAudioDecoderConfiguration = xml_node_soap_get(p_body, "SetAudioDecoderConfiguration");
	assert(p_SetAudioDecoderConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_SetAudioDecoderConfiguration(p_SetAudioDecoderConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tr2_SetAudioDecoderConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_SetAudioDecoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tr2_GetAudioDecoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAudioDecoderConfigurationOptions;
	tr2_GetAudioDecoderConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GetAudioDecoderConfigurationOptions = xml_node_soap_get(p_body, "GetAudioDecoderConfigurationOptions");
	assert(p_GetAudioDecoderConfigurationOptions);    

	memset(&req, 0, sizeof(req));

	ret = parse_tr2_GetConfiguration(p_GetAudioDecoderConfigurationOptions, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAudioDecoderConfigurationOptions_rly_xml, (char *)&req, NULL, p_header);	
}

#endif // end of AUDIO_SUPPORT

#ifdef VIDEO_ANALYTICS

int soap_tr2_GetAnalyticsConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAnalyticsConfigurations;
	tr2_GetAnalyticsConfigurations_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetAnalyticsConfigurations = xml_node_soap_get(p_body, "GetAnalyticsConfigurations");
	assert(p_GetAnalyticsConfigurations);

	ret = parse_tr2_GetConfiguration(p_GetAnalyticsConfigurations, &req.GetConfiguration);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tr2_GetAnalyticsConfigurations_rly_xml, (char *)&req, NULL, p_header);
}

#endif // end of VIDEO_ANALYTICS

#endif // end of MEDIA2_SUPPORT

#ifdef THERMAL_SUPPORT

int soap_tth_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Thermal;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tth_GetConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
		
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tth_GetConfigurations_rly_xml, NULL, NULL, p_header);
}

int soap_tth_GetConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetConfiguration;
	tth_GetConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetConfiguration = xml_node_soap_get(p_body, "GetConfiguration");
	assert(p_GetConfiguration);	

	ret = parse_tth_GetConfiguration(p_GetConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tth_GetConfiguration_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tth_SetConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetConfiguration;
	tth_SetConfiguration_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetConfiguration = xml_node_soap_get(p_body, "SetConfiguration");
	assert(p_SetConfiguration);
	
	memset(&req, 0, sizeof(req));

	ret = parse_tth_SetConfiguration(p_SetConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tth_SetConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tth_SetConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tth_GetConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetConfigurationOptions;
	tth_GetConfigurationOptions_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetConfigurationOptions = xml_node_soap_get(p_body, "GetConfigurationOptions");
	assert(p_GetConfigurationOptions);	

	ret = parse_tth_GetConfigurationOptions(p_GetConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tth_GetConfigurationOptions_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tth_GetRadiometryConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetRadiometryConfiguration;
	tth_GetRadiometryConfiguration_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetRadiometryConfiguration = xml_node_soap_get(p_body, "GetRadiometryConfiguration");
	assert(p_GetRadiometryConfiguration);	

	ret = parse_tth_GetRadiometryConfiguration(p_GetRadiometryConfiguration, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tth_GetRadiometryConfiguration_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tth_SetRadiometryConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetRadiometryConfiguration;
	tth_SetRadiometryConfiguration_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetRadiometryConfiguration = xml_node_soap_get(p_body, "SetRadiometryConfiguration");
	assert(p_SetRadiometryConfiguration);

	memset(&req, 0, sizeof(req));

	ret = parse_tth_SetRadiometryConfiguration(p_SetRadiometryConfiguration, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tth_SetRadiometryConfiguration(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tth_SetRadiometryConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tth_GetRadiometryConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetRadiometryConfigurationOptions;
	tth_GetRadiometryConfigurationOptions_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_GetRadiometryConfigurationOptions = xml_node_soap_get(p_body, "GetRadiometryConfigurationOptions");
	assert(p_GetRadiometryConfigurationOptions);	

	ret = parse_tth_GetRadiometryConfigurationOptions(p_GetRadiometryConfigurationOptions, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tth_GetRadiometryConfigurationOptions_rly_xml, (char*)&req, NULL, p_header);
}

#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT

int soap_tcr_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Credential;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tcr_GetCredentialInfo(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCredentialInfo;
	tcr_GetCredentialInfo_REQ req;
	tcr_GetCredentialInfo_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetCredentialInfo = xml_node_soap_get(p_body, "GetCredentialInfo");
	assert(p_GetCredentialInfo);	

	ret = parse_tcr_GetCredentialInfo(p_GetCredentialInfo, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetCredentialInfo(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetCredentialInfo_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_GetCredentialInfoList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCredentialInfoList;
	tcr_GetCredentialInfoList_REQ req;
	tcr_GetCredentialInfoList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetCredentialInfoList = xml_node_soap_get(p_body, "GetCredentialInfoList");
	assert(p_GetCredentialInfoList);	

	ret = parse_tcr_GetCredentialInfoList(p_GetCredentialInfoList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetCredentialInfoList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetCredentialInfoList_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_GetCredentials(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCredentials;
	tcr_GetCredentials_REQ req;
	tcr_GetCredentials_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetCredentials = xml_node_soap_get(p_body, "GetCredentials");
	assert(p_GetCredentials);	

	ret = parse_tcr_GetCredentials(p_GetCredentials, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetCredentials(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetCredentials_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_GetCredentialList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCredentialList;
	tcr_GetCredentialList_REQ req;
	tcr_GetCredentialList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetCredentialList = xml_node_soap_get(p_body, "GetCredentialList");
	assert(p_GetCredentialList);	

	ret = parse_tcr_GetCredentialList(p_GetCredentialList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetCredentialList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetCredentialList_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_CreateCredential(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateCredential;
	tcr_CreateCredential_REQ req;
	tcr_CreateCredential_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_CreateCredential = xml_node_soap_get(p_body, "CreateCredential");
	assert(p_CreateCredential);	

	ret = parse_tcr_CreateCredential(p_CreateCredential, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_CreateCredential(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_CreateCredential_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_ModifyCredential(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyCredential;
	tcr_ModifyCredential_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_ModifyCredential = xml_node_soap_get(p_body, "ModifyCredential");
	assert(p_ModifyCredential);	

	ret = parse_tcr_ModifyCredential(p_ModifyCredential, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_ModifyCredential(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_ModifyCredential_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tcr_DeleteCredential(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteCredential;
	tcr_DeleteCredential_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DeleteCredential = xml_node_soap_get(p_body, "DeleteCredential");
	assert(p_DeleteCredential);	

	ret = parse_tcr_DeleteCredential(p_DeleteCredential, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_DeleteCredential(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_DeleteCredential_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tcr_GetCredentialState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCredentialState;
	tcr_GetCredentialState_REQ req;
	tcr_GetCredentialState_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetCredentialState = xml_node_soap_get(p_body, "GetCredentialState");
	assert(p_GetCredentialState);	

	ret = parse_tcr_GetCredentialState(p_GetCredentialState, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetCredentialState(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetCredentialState_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_EnableCredential(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_EnableCredential;
	tcr_EnableCredential_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_EnableCredential = xml_node_soap_get(p_body, "EnableCredential");
	assert(p_EnableCredential);	

	ret = parse_tcr_EnableCredential(p_EnableCredential, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_EnableCredential(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_EnableCredential_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_DisableCredential(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DisableCredential;
	tcr_DisableCredential_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DisableCredential = xml_node_soap_get(p_body, "DisableCredential");
	assert(p_DisableCredential);	

	ret = parse_tcr_DisableCredential(p_DisableCredential, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_DisableCredential(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_DisableCredential_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_SetCredential(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetCredential;
	tcr_SetCredential_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetCredential = xml_node_soap_get(p_body, "SetCredential");
	assert(p_SetCredential);	

	ret = parse_tcr_SetCredential(p_SetCredential, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_SetCredential(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_SetCredential_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_ResetAntipassbackViolation(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ResetAntipassbackViolation;
	tcr_ResetAntipassbackViolation_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_ResetAntipassbackViolation = xml_node_soap_get(p_body, "ResetAntipassbackViolation");
	assert(p_ResetAntipassbackViolation);	

	ret = parse_tcr_ResetAntipassbackViolation(p_ResetAntipassbackViolation, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_ResetAntipassbackViolation(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_ResetAntipassbackViolation_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tcr_GetSupportedFormatTypes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSupportedFormatTypes;
	tcr_GetSupportedFormatTypes_REQ req;
	tcr_GetSupportedFormatTypes_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetSupportedFormatTypes = xml_node_soap_get(p_body, "GetSupportedFormatTypes");
	assert(p_GetSupportedFormatTypes);	

	ret = parse_tcr_GetSupportedFormatTypes(p_GetSupportedFormatTypes, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetSupportedFormatTypes(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetSupportedFormatTypes_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_GetCredentialIdentifiers(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCredentialIdentifiers;
	tcr_GetCredentialIdentifiers_REQ req;
	tcr_GetCredentialIdentifiers_RES res;
	
	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetCredentialIdentifiers = xml_node_soap_get(p_body, "GetCredentialIdentifiers");
	assert(p_GetCredentialIdentifiers);	

	ret = parse_tcr_GetCredentialIdentifiers(p_GetCredentialIdentifiers, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetCredentialIdentifiers(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetCredentialIdentifiers_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_SetCredentialIdentifier(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetCredentialIdentifier;
	tcr_SetCredentialIdentifier_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetCredentialIdentifier = xml_node_soap_get(p_body, "SetCredentialIdentifier");
	assert(p_SetCredentialIdentifier);	

	ret = parse_tcr_SetCredentialIdentifier(p_SetCredentialIdentifier, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_SetCredentialIdentifier(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_SetCredentialIdentifier_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tcr_DeleteCredentialIdentifier(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteCredentialIdentifier;
	tcr_DeleteCredentialIdentifier_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DeleteCredentialIdentifier = xml_node_soap_get(p_body, "DeleteCredentialIdentifier");
	assert(p_DeleteCredentialIdentifier);	

	ret = parse_tcr_DeleteCredentialIdentifier(p_DeleteCredentialIdentifier, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_DeleteCredentialIdentifier(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_DeleteCredentialIdentifier_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tcr_GetCredentialAccessProfiles(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetCredentialAccessProfiles;
	tcr_GetCredentialAccessProfiles_REQ req;
	tcr_GetCredentialAccessProfiles_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetCredentialAccessProfiles = xml_node_soap_get(p_body, "GetCredentialAccessProfiles");
	assert(p_GetCredentialAccessProfiles);	

	ret = parse_tcr_GetCredentialAccessProfiles(p_GetCredentialAccessProfiles, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetCredentialAccessProfiles(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetCredentialAccessProfiles_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tcr_SetCredentialAccessProfiles(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetCredentialAccessProfiles;
	tcr_SetCredentialAccessProfiles_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetCredentialAccessProfiles = xml_node_soap_get(p_body, "SetCredentialAccessProfiles");
	assert(p_SetCredentialAccessProfiles);	

	ret = parse_tcr_SetCredentialAccessProfiles(p_SetCredentialAccessProfiles, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_SetCredentialAccessProfiles(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_SetCredentialAccessProfiles_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tcr_DeleteCredentialAccessProfiles(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteCredentialAccessProfiles;
	tcr_DeleteCredentialAccessProfiles_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DeleteCredentialAccessProfiles = xml_node_soap_get(p_body, "DeleteCredentialAccessProfiles");
	assert(p_DeleteCredentialAccessProfiles);	

	ret = parse_tcr_DeleteCredentialAccessProfiles(p_DeleteCredentialAccessProfiles, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_DeleteCredentialAccessProfiles(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_DeleteCredentialAccessProfiles_rly_xml, (char*)&req, NULL, p_header);
}

int soap_tcr_GetWhitelist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetWhitelist;
	tcr_GetWhitelist_REQ req;
	tcr_GetWhitelist_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetWhitelist = xml_node_soap_get(p_body, "GetWhitelist");
	assert(p_GetWhitelist);	

	ret = parse_tcr_GetWhitelist(p_GetWhitelist, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetWhitelist(&req, &res);
	}

	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetWhitelist_rly_xml, (char*)&res, NULL, p_header);

	onvif_free_CredentialIdentifierItems(&res.Identifier);

	return ret;
}

int soap_tcr_AddToWhitelist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddToWhitelist;
	tcr_AddToWhitelist_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_AddToWhitelist = xml_node_soap_get(p_body, "AddToWhitelist");
	assert(p_AddToWhitelist);	

	ret = parse_tcr_AddToWhitelist(p_AddToWhitelist, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_AddToWhitelist(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_AddToWhitelist_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_RemoveFromWhitelist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveFromWhitelist;
	tcr_RemoveFromWhitelist_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_RemoveFromWhitelist = xml_node_soap_get(p_body, "RemoveFromWhitelist");
	assert(p_RemoveFromWhitelist);	

	ret = parse_tcr_RemoveFromWhitelist(p_RemoveFromWhitelist, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_RemoveFromWhitelist(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_RemoveFromWhitelist_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_DeleteWhitelist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteWhitelist;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteWhitelist = xml_node_soap_get(p_body, "DeleteWhitelist");
	assert(p_DeleteWhitelist);	

	ret = onvif_tcr_DeleteWhitelist();

	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_DeleteWhitelist_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_GetBlacklist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetBlacklist;
	tcr_GetBlacklist_REQ req;
	tcr_GetBlacklist_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetBlacklist = xml_node_soap_get(p_body, "GetBlacklist");
	assert(p_GetBlacklist);	

	ret = parse_tcr_GetBlacklist(p_GetBlacklist, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_GetBlacklist(&req, &res);
	}
	
	ret = soap_build_send_rly(p_user, rx_msg, ret, build_tcr_GetBlacklist_rly_xml, (char*)&res, NULL, p_header);

	onvif_free_CredentialIdentifierItems(&res.Identifier);

	return ret;
}

int soap_tcr_AddToBlacklist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_AddToBlacklist;
	tcr_AddToBlacklist_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_AddToBlacklist = xml_node_soap_get(p_body, "AddToBlacklist");
	assert(p_AddToBlacklist);	

	ret = parse_tcr_AddToBlacklist(p_AddToBlacklist, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_AddToBlacklist(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_AddToBlacklist_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_RemoveFromBlacklist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RemoveFromBlacklist;
	tcr_RemoveFromBlacklist_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_RemoveFromBlacklist = xml_node_soap_get(p_body, "RemoveFromBlacklist");
	assert(p_RemoveFromBlacklist);	

	ret = parse_tcr_RemoveFromBlacklist(p_RemoveFromBlacklist, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tcr_RemoveFromBlacklist(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_RemoveFromBlacklist_rly_xml, NULL, NULL, p_header);
}

int soap_tcr_DeleteBlacklist(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteBlacklist;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteBlacklist = xml_node_soap_get(p_body, "DeleteBlacklist");
	assert(p_DeleteBlacklist);	

	ret = onvif_tcr_DeleteBlacklist();

	return soap_build_send_rly(p_user, rx_msg, ret, build_tcr_DeleteBlacklist_rly_xml, NULL, NULL, p_header);
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

int soap_tar_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_AccessRules;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tar_GetAccessProfileInfo(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessProfileInfo;
	tar_GetAccessProfileInfo_REQ req;
	tar_GetAccessProfileInfo_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetAccessProfileInfo = xml_node_soap_get(p_body, "GetAccessProfileInfo");
	assert(p_GetAccessProfileInfo);	

	ret = parse_tar_GetAccessProfileInfo(p_GetAccessProfileInfo, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_GetAccessProfileInfo(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_GetAccessProfileInfo_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tar_GetAccessProfileInfoList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessProfileInfoList;
	tar_GetAccessProfileInfoList_REQ req;
	tar_GetAccessProfileInfoList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetAccessProfileInfoList = xml_node_soap_get(p_body, "GetAccessProfileInfoList");
	assert(p_GetAccessProfileInfoList);	

	ret = parse_tar_GetAccessProfileInfoList(p_GetAccessProfileInfoList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_GetAccessProfileInfoList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_GetAccessProfileInfoList_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tar_GetAccessProfiles(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessProfiles;
	tar_GetAccessProfiles_REQ req;
	tar_GetAccessProfiles_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetAccessProfiles = xml_node_soap_get(p_body, "GetAccessProfiles");
	assert(p_GetAccessProfiles);	

	ret = parse_tar_GetAccessProfiles(p_GetAccessProfiles, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_GetAccessProfiles(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_GetAccessProfiles_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tar_GetAccessProfileList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetAccessProfileList;
	tar_GetAccessProfileList_REQ req;
	tar_GetAccessProfileList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetAccessProfileList = xml_node_soap_get(p_body, "GetAccessProfileList");
	assert(p_GetAccessProfileList);	

	ret = parse_tar_GetAccessProfileList(p_GetAccessProfileList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_GetAccessProfileList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_GetAccessProfileList_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tar_CreateAccessProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateAccessProfile;
	tar_CreateAccessProfile_REQ req;
	tar_CreateAccessProfile_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_CreateAccessProfile = xml_node_soap_get(p_body, "CreateAccessProfile");
	assert(p_CreateAccessProfile);	

	ret = parse_tar_CreateAccessProfile(p_CreateAccessProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_CreateAccessProfile(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_CreateAccessProfile_rly_xml, (char*)&res, NULL, p_header);
}

int soap_tar_ModifyAccessProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifyAccessProfile;
	tar_ModifyAccessProfile_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_ModifyAccessProfile = xml_node_soap_get(p_body, "ModifyAccessProfile");
	assert(p_ModifyAccessProfile);	

	ret = parse_tar_ModifyAccessProfile(p_ModifyAccessProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_ModifyAccessProfile(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_ModifyAccessProfile_rly_xml, NULL, NULL, p_header);
}

int soap_tar_DeleteAccessProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteAccessProfile;
	tar_DeleteAccessProfile_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DeleteAccessProfile = xml_node_soap_get(p_body, "DeleteAccessProfile");
	assert(p_DeleteAccessProfile);	

	ret = parse_tar_DeleteAccessProfile(p_DeleteAccessProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_DeleteAccessProfile(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_DeleteAccessProfile_rly_xml, NULL, NULL, p_header);
}

int soap_tar_SetAccessProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetAccessProfile;
	tar_SetAccessProfile_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetAccessProfile = xml_node_soap_get(p_body, "SetAccessProfile");
	assert(p_SetAccessProfile);	

	ret = parse_tar_SetAccessProfile(p_SetAccessProfile, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tar_SetAccessProfile(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tar_SetAccessProfile_rly_xml, NULL, NULL, p_header);
}
#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

int soap_tsc_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Schedule;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tsc_GetScheduleInfo(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetScheduleInfo;
	tsc_GetScheduleInfo_REQ req;
	tsc_GetScheduleInfo_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetScheduleInfo = xml_node_soap_get(p_body, "GetScheduleInfo");
	assert(p_GetScheduleInfo);	

	ret = parse_tsc_GetScheduleInfo(p_GetScheduleInfo, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetScheduleInfo(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetScheduleInfo_rly_xml, (char*)&res, NULL, p_header);
}
  
int soap_tsc_GetScheduleInfoList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetScheduleInfoList;
	tsc_GetScheduleInfoList_REQ req;
	tsc_GetScheduleInfoList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetScheduleInfoList = xml_node_soap_get(p_body, "GetScheduleInfoList");
	assert(p_GetScheduleInfoList);	

	ret = parse_tsc_GetScheduleInfoList(p_GetScheduleInfoList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetScheduleInfoList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetScheduleInfoList_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_GetSchedules(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSchedules;
	tsc_GetSchedules_REQ req;
	tsc_GetSchedules_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetSchedules = xml_node_soap_get(p_body, "GetSchedules");
	assert(p_GetSchedules);	

	ret = parse_tsc_GetSchedules(p_GetSchedules, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetSchedules(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetSchedules_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_GetScheduleList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetScheduleList;
	tsc_GetScheduleList_REQ req;
	tsc_GetScheduleList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetScheduleList = xml_node_soap_get(p_body, "GetScheduleList");
	assert(p_GetScheduleList);	

	ret = parse_tsc_GetScheduleList(p_GetScheduleList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetScheduleList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetScheduleList_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_CreateSchedule(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateSchedule;
	tsc_CreateSchedule_REQ req;
	tsc_CreateSchedule_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_CreateSchedule = xml_node_soap_get(p_body, "CreateSchedule");
	assert(p_CreateSchedule);	

	ret = parse_tsc_CreateSchedule(p_CreateSchedule, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_CreateSchedule(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_CreateSchedule_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_ModifySchedule(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifySchedule;
	tsc_ModifySchedule_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_ModifySchedule = xml_node_soap_get(p_body, "ModifySchedule");
	assert(p_ModifySchedule);

	ret = parse_tsc_ModifySchedule(p_ModifySchedule, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_ModifySchedule(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_ModifySchedule_rly_xml, NULL, NULL, p_header);
}
 
int soap_tsc_DeleteSchedule(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteSchedule;
	tsc_DeleteSchedule_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DeleteSchedule = xml_node_soap_get(p_body, "DeleteSchedule");
	assert(p_DeleteSchedule);

	ret = parse_tsc_DeleteSchedule(p_DeleteSchedule, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_DeleteSchedule(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_DeleteSchedule_rly_xml, NULL, NULL, p_header);
}
 
int soap_tsc_GetSpecialDayGroupInfo(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSpecialDayGroupInfo;
	tsc_GetSpecialDayGroupInfo_REQ req;
	tsc_GetSpecialDayGroupInfo_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetSpecialDayGroupInfo = xml_node_soap_get(p_body, "GetSpecialDayGroupInfo");
	assert(p_GetSpecialDayGroupInfo);	

	ret = parse_tsc_GetSpecialDayGroupInfo(p_GetSpecialDayGroupInfo, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetSpecialDayGroupInfo(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetSpecialDayGroupInfo_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_GetSpecialDayGroupInfoList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSpecialDayGroupInfoList;
	tsc_GetSpecialDayGroupInfoList_REQ req;
	tsc_GetSpecialDayGroupInfoList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetSpecialDayGroupInfoList = xml_node_soap_get(p_body, "GetSpecialDayGroupInfoList");
	assert(p_GetSpecialDayGroupInfoList);	

	ret = parse_tsc_GetSpecialDayGroupInfoList(p_GetSpecialDayGroupInfoList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetSpecialDayGroupInfoList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetSpecialDayGroupInfoList_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_GetSpecialDayGroups(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSpecialDayGroups;
	tsc_GetSpecialDayGroups_REQ req;
	tsc_GetSpecialDayGroups_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetSpecialDayGroups = xml_node_soap_get(p_body, "GetSpecialDayGroups");
	assert(p_GetSpecialDayGroups);	

	ret = parse_tsc_GetSpecialDayGroups(p_GetSpecialDayGroups, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetSpecialDayGroups(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetSpecialDayGroups_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_GetSpecialDayGroupList(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetSpecialDayGroupList;
	tsc_GetSpecialDayGroupList_REQ req;
	tsc_GetSpecialDayGroupList_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetSpecialDayGroupList = xml_node_soap_get(p_body, "GetSpecialDayGroupList");
	assert(p_GetSpecialDayGroupList);	

	ret = parse_tsc_GetSpecialDayGroupList(p_GetSpecialDayGroupList, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetSpecialDayGroupList(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetSpecialDayGroupList_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_CreateSpecialDayGroup(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateSpecialDayGroup;
	tsc_CreateSpecialDayGroup_REQ req;
	tsc_CreateSpecialDayGroup_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_CreateSpecialDayGroup = xml_node_soap_get(p_body, "CreateSpecialDayGroup");
	assert(p_CreateSpecialDayGroup);	

	ret = parse_tsc_CreateSpecialDayGroup(p_CreateSpecialDayGroup, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_CreateSpecialDayGroup(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_CreateSpecialDayGroup_rly_xml, (char*)&res, NULL, p_header);
}
 
int soap_tsc_ModifySpecialDayGroup(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ModifySpecialDayGroup;
	tsc_ModifySpecialDayGroup_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_ModifySpecialDayGroup = xml_node_soap_get(p_body, "ModifySpecialDayGroup");
	assert(p_ModifySpecialDayGroup);	

	ret = parse_tsc_ModifySpecialDayGroup(p_ModifySpecialDayGroup, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_ModifySpecialDayGroup(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_ModifySpecialDayGroup_rly_xml, NULL, NULL, p_header);
}
 
int soap_tsc_DeleteSpecialDayGroup(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteSpecialDayGroup;
	tsc_DeleteSpecialDayGroup_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DeleteSpecialDayGroup = xml_node_soap_get(p_body, "DeleteSpecialDayGroup");
	assert(p_DeleteSpecialDayGroup);	

	ret = parse_tsc_DeleteSpecialDayGroup(p_DeleteSpecialDayGroup, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_DeleteSpecialDayGroup(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_DeleteSpecialDayGroup_rly_xml, NULL, NULL, p_header);
}
 
int soap_tsc_GetScheduleState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetScheduleState;
	tsc_GetScheduleState_REQ req;
	tsc_GetScheduleState_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetScheduleState = xml_node_soap_get(p_body, "GetScheduleState");
	assert(p_GetScheduleState);	

	ret = parse_tsc_GetScheduleState(p_GetScheduleState, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_GetScheduleState(&req, &res);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_GetScheduleState_rly_xml, (char*)&res, NULL, p_header);
} 

int soap_tsc_SetSchedule(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetSchedule;
	tsc_SetSchedule_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetSchedule = xml_node_soap_get(p_body, "SetSchedule");
	assert(p_SetSchedule);

	ret = parse_tsc_SetSchedule(p_SetSchedule, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_SetSchedule(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_SetSchedule_rly_xml, NULL, NULL, p_header);
}

int soap_tsc_SetSpecialDayGroup(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetSpecialDayGroup;
	tsc_SetSpecialDayGroup_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetSpecialDayGroup = xml_node_soap_get(p_body, "SetSpecialDayGroup");
	assert(p_SetSpecialDayGroup);

	ret = parse_tsc_SetSpecialDayGroup(p_SetSpecialDayGroup, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tsc_SetSpecialDayGroup(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tsc_SetSpecialDayGroup_rly_xml, NULL, NULL, p_header);
}
#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

int soap_trv_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Receiver;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_trv_GetReceivers(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	trv_GetReceivers_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&res, 0, sizeof(res));

	ret = onvif_trv_GetReceivers(&res);

	return soap_build_send_rly(p_user, rx_msg, ret, build_trv_GetReceivers_rly_xml, (char*)&res, NULL, p_header);
}

int soap_trv_GetReceiver(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetReceiver;
	trv_GetReceiver_REQ req;
	trv_GetReceiver_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetReceiver = xml_node_soap_get(p_body, "GetReceiver");
	assert(p_GetReceiver);	

	ret = parse_trv_GetReceiver(p_GetReceiver, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trv_GetReceiver(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_trv_GetReceiver_rly_xml, (char*)&res, NULL, p_header);
}

int soap_trv_CreateReceiver(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateReceiver;
	trv_CreateReceiver_REQ req;
	trv_CreateReceiver_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_CreateReceiver = xml_node_soap_get(p_body, "CreateReceiver");
	assert(p_CreateReceiver);	

	ret = parse_trv_CreateReceiver(p_CreateReceiver, &req);
	if (ONVIF_OK == ret)
	{
	    ret = onvif_trv_CreateReceiver(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_trv_CreateReceiver_rly_xml, (char*)&res, NULL, p_header);
}

int soap_trv_DeleteReceiver(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteReceiver;
	trv_DeleteReceiver_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_DeleteReceiver = xml_node_soap_get(p_body, "DeleteReceiver");
	assert(p_DeleteReceiver);	

	ret = parse_trv_DeleteReceiver(p_DeleteReceiver, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trv_DeleteReceiver(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_trv_DeleteReceiver_rly_xml, NULL, NULL, p_header);
}

int soap_trv_ConfigureReceiver(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ConfigureReceiver;
	trv_ConfigureReceiver_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_ConfigureReceiver = xml_node_soap_get(p_body, "ConfigureReceiver");
	assert(p_ConfigureReceiver);	

	ret = parse_trv_ConfigureReceiver(p_ConfigureReceiver, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trv_ConfigureReceiver(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_trv_ConfigureReceiver_rly_xml, NULL, NULL, p_header);
}

int soap_trv_SetReceiverMode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetReceiverMode;
	trv_SetReceiverMode_REQ req;
	
	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_SetReceiverMode = xml_node_soap_get(p_body, "SetReceiverMode");
	assert(p_SetReceiverMode);

	ret = parse_trv_SetReceiverMode(p_SetReceiverMode, &req);
	if (ONVIF_OK == ret)
	{	    
	    ret = onvif_trv_SetReceiverMode(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_trv_SetReceiverMode_rly_xml, NULL, NULL, p_header);
}

int soap_trv_GetReceiverState(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetReceiverState;
	trv_GetReceiverState_REQ req;
	trv_GetReceiverState_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetReceiverState = xml_node_soap_get(p_body, "GetReceiverState");
	assert(p_GetReceiverState);	

	ret = parse_trv_GetReceiverState(p_GetReceiverState, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_trv_GetReceiverState(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_trv_GetReceiverState_rly_xml, (char*)&res, NULL, p_header);
}

#endif // end of RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT

int soap_tpv_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_DBG, "\n");
	
	category = CapabilityCategory_Provisioning;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tpv_PanMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_PanMove;
	tpv_PanMove_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_PanMove = xml_node_soap_get(p_body, "PanMove");
	assert(p_PanMove);	

	ret = parse_tpv_PanMove(p_PanMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpv_PanMove(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpv_PanMove_rly_xml, NULL, NULL, p_header);
}

int soap_tpv_TiltMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_TiltMove;
	tpv_TiltMove_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_TiltMove = xml_node_soap_get(p_body, "TiltMove");
	assert(p_TiltMove);	

	ret = parse_tpv_TiltMove(p_TiltMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpv_TiltMove(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpv_TiltMove_rly_xml, NULL, NULL, p_header);
}

int soap_tpv_ZoomMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_ZoomMove;
	tpv_ZoomMove_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_ZoomMove = xml_node_soap_get(p_body, "ZoomMove");
	assert(p_ZoomMove);	

	ret = parse_tpv_ZoomMove(p_ZoomMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpv_ZoomMove(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpv_ZoomMove_rly_xml, NULL, NULL, p_header);
}

int soap_tpv_RollMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_RollMove;
	tpv_RollMove_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_RollMove = xml_node_soap_get(p_body, "RollMove");
	assert(p_RollMove);	

	ret = parse_tpv_RollMove(p_RollMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpv_RollMove(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpv_RollMove_rly_xml, NULL, NULL, p_header);
}

int soap_tpv_FocusMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_FocusMove;
	tpv_FocusMove_REQ req;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));

	p_FocusMove = xml_node_soap_get(p_body, "FocusMove");
	assert(p_FocusMove);	

	ret = parse_tpv_FocusMove(p_FocusMove, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpv_FocusMove(&req);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpv_FocusMove_rly_xml, NULL, NULL, p_header);
}

int soap_tpv_GetUsage(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GetUsage;
	tpv_GetUsage_REQ req;
	tpv_GetUsage_RES res;

	log_print(HT_LOG_DBG, "\n");

	memset(&req, 0, sizeof(req));
	memset(&res, 0, sizeof(res));

	p_GetUsage = xml_node_soap_get(p_body, "GetUsage");
	assert(p_GetUsage);	

	ret = parse_tpv_GetUsage(p_GetUsage, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpv_GetUsage(&req, &res);
	}
	
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpv_GetUsage_rly_xml, (char *)&res, NULL, p_header);
}

int soap_tpv_Stop(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_Stop;
	tpv_Stop_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_Stop = xml_node_soap_get(p_body, "Stop");
	assert(p_Stop);

	memset(&req, 0, sizeof(req));		

	ret = parse_tpv_Stop(p_Stop, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpv_Stop(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tpv_Stop_rly_xml, NULL, NULL, p_header);
}

#endif // end of PROVISIONING_SUPPORT

#if 1

int soap_hbgk_ext_GetPrivacyMaskOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
		
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_hbgk_ext_GetPrivacyMaskOptions_rly_xml, NULL, NULL, p_header);
}

int soap_hbgk_ext_GetPrivacyMasks(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GGetMasks;
	ewsd_GetMasks_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GGetMasks = xml_node_soap_get(p_body, "GetPrivacyMasks");
	assert(p_GGetMasks);

	memset(&req, 0, sizeof(req));

	ret = parse_hbgk_ext_GetPrivacyMasks(p_GGetMasks, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_hbgk_ext_GetPrivacyMasks_rly_xml, (char *)&req, NULL, p_header);
}

int soap_hbgk_ext_CreatePrivacyMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateMask;
	ewsd_CreateMask_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_CreateMask = xml_node_soap_get(p_body, "CreatePrivacyMask");
	assert(p_CreateMask);

	memset(&req, 0, sizeof(req));

	ret = parse_hbgk_ext_CreatePrivacyMask(p_CreateMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_hbgk_ext_CreatePrivacyMask(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_hbgk_ext_CreatePrivacyMask_rly_xml, req.PrivacyMask.token, NULL, p_header);
}

int soap_hbgk_ext_DeletePrivacyMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteMask;
	ewsd_DeleteMask_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteMask = xml_node_soap_get(p_body, "DeletePrivacyMask");
	assert(p_DeleteMask);

	memset(&req, 0, sizeof(req));

	ret = parse_hbgk_ext_DeletePrivacyMask(p_DeleteMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_hbgk_ext_DeletePrivacyMask(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_hbgk_ext_DeletePrivacyMask_rly_xml, NULL, NULL, p_header);
}

int soap_hbgk_ext_SetPrivacyMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetMask;
	ewsd_SetMask_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetMask = xml_node_soap_get(p_body, "SetPrivacyMask");
	assert(p_SetMask);

	memset(&req, 0, sizeof(req));

	ret = parse_hbgk_ext_SetPrivacyMask(p_SetMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_hbgk_ext_SetPrivacyMask(&req);
	}
	return soap_build_send_rly(p_user, rx_msg, ret, build_hbgk_ext_SetPrivacyMask_rly_xml, NULL, NULL, p_header);
}

#endif 


#if 1

int soap_tpl_GetPrivacyMaskOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	log_print(HT_LOG_DBG, "\n");
		
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tpl_GetPrivacyMaskOptions_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_GetPrivacyMasks(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_GGetMasks;
	tpl_GetMasks_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_GGetMasks = xml_node_soap_get(p_body, "GetPrivacyMasks");
	assert(p_GGetMasks);

	memset(&req, 0, sizeof(req));

	ret = parse_tpl_GetPrivacyMasks(p_GGetMasks, &req);

	return soap_build_send_rly(p_user, rx_msg, ret, build_tpl_GetPrivacyMasks_rly_xml, (char *)&req, NULL, p_header);
}

int soap_tpl_CreatePrivacyMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_CreateMask;
	tpl_CreateMask_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_CreateMask = xml_node_soap_get(p_body, "CreatePrivacyMask");
	assert(p_CreateMask);

	memset(&req, 0, sizeof(req));

	ret = parse_tpl_CreatePrivacyMask(p_CreateMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpl_CreatePrivacyMask(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tpl_CreatePrivacyMask_rly_xml, req.PrivacyMask.token, NULL, p_header);
}

int soap_tpl_DeletePrivacyMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_DeleteMask;
	tpl_DeleteMask_REQ req;

	log_print(HT_LOG_DBG, "\n");

	p_DeleteMask = xml_node_soap_get(p_body, "DeletePrivacyMask");
	assert(p_DeleteMask);

	memset(&req, 0, sizeof(req));

	ret = parse_tpl_DeletePrivacyMask(p_DeleteMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpl_DeletePrivacyMask(&req);
	}

	return soap_build_send_rly(p_user, rx_msg, ret, build_tpl_DeletePrivacyMask_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_SetPrivacyMask(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetMask;
	tpl_SetMask_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetMask = xml_node_soap_get(p_body, "SetPrivacyMask");
	assert(p_SetMask);

	memset(&req, 0, sizeof(req));

	ret = parse_tpl_SetPrivacyMask(p_SetMask, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpl_SetPrivacyMask(&req);
	}
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpl_SetPrivacyMask_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_SetImagingSettingDefault(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SetImagingSettingDefault;
	tpl_SetImagingSettingDefault_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SetImagingSettingDefault = xml_node_soap_get(p_body, "SetImagingSettingDefault");
	assert(p_SetImagingSettingDefault);

	memset(&req, 0, sizeof(req));

	ret = parse_tpl_SetImagingSettingDefault(p_SetImagingSettingDefault, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpl_SetImagingSettingDefault(&req);
	}
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpl_SetImagingSettingDefault_rly_xml, NULL, NULL, p_header);
	
}

int soap_tpl_SendCommand(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret;
	XMLN * p_SendCommand;
	tpl_SendCommand_REQ req;
		
	log_print(HT_LOG_DBG, "\n");

	p_SendCommand = xml_node_soap_get(p_body, "SendCommand");
	assert(p_SendCommand);

	memset(&req, 0, sizeof(req));

	ret = parse_tpl_SendCommand(p_SendCommand, &req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_tpl_SendCommand(&req);
	}
	return soap_build_send_rly(p_user, rx_msg, ret, build_tpl_SendCommand_rly_xml, NULL, NULL, p_header);
	
}

int soap_tpl_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	onvif_CapabilityCategory category;
	
	log_print(HT_LOG_INFO, "soap_tpl_GetServiceCapabilities:%s\r\n", __FUNCTION__);
	
	category = CapabilityCategory_Plus;

	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_GetServiceCapabilities_rly_xml, (char *)&category, NULL, p_header); 	
}

int soap_tpl_SetVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tpl_SetVideoEncoderConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_SetImagingSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tpl_SetImagingSettings_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_SetDigitalInputConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tpl_SetDigitalInputConfiguration_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_GetImagingSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tpl_GetImagingSettings_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_GetImagingOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tpl_GetImagingOptions_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_GetDigitalInputs(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	return soap_build_send_rly(p_user, rx_msg, ONVIF_OK, build_tpl_GetDigitalInputs_rly_xml, NULL, NULL, p_header);
}

int soap_tpl_GetVideoEncoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	ONVIF_RET ret = ONVIF_OK;

	return soap_build_send_rly(p_user, rx_msg, ret, build_tpl_GetVideoEncoderConfigurationOptions_rly_xml, NULL, NULL, p_header);
}

#endif

/*********************************************************/
void soap_calc_digest(const char *created, uint8 *nonce, int noncelen, const char *password, uint8 hash[20])
{
	sha1_context ctx;
	
	sha1_starts(&ctx);
	sha1_update(&ctx, (uint8 *)nonce, noncelen);
	sha1_update(&ctx, (uint8 *)created, (uint32)strlen(created));
	sha1_update(&ctx, (uint8 *)password, (uint32)strlen(password));
	sha1_finish(&ctx, (uint8 *)hash);
}

BOOL soap_auth_process(XMLN * p_Security, int level)
{
	int nonce_len;
	XMLN * p_UsernameToken;
	XMLN * p_Username;
	XMLN * p_Password;
	XMLN * p_Nonce;
	XMLN * p_Created;
	char HABase64[100];
	uint8 nonce[200];
	uint8 HA[20];
	const char * p_Type;
	onvif_User * p_user;

	p_UsernameToken = xml_node_soap_get(p_Security, "wsse:UsernameToken");
	if (NULL == p_UsernameToken)
	{
		return FALSE;
	}

	p_Username = xml_node_soap_get(p_UsernameToken, "wsse:Username");
	p_Password = xml_node_soap_get(p_UsernameToken, "wsse:Password");
	p_Nonce = xml_node_soap_get(p_UsernameToken, "wsse:Nonce");
	p_Created = xml_node_soap_get(p_UsernameToken, "wsse:Created");

	if (NULL == p_Username || NULL == p_Username->data || 
		NULL == p_Password || NULL == p_Password->data || 
		NULL == p_Nonce || NULL == p_Nonce->data ||
		NULL == p_Created || NULL == p_Created->data)
	{
		return FALSE;
	}

	p_Type = xml_attr_get(p_Password, "Type");
	if (NULL == p_Type)
	{
		return FALSE;
	}

	p_user = onvif_find_user(p_Username->data);
	if (NULL == p_user)	// user not exist
	{
		return FALSE;
	}

	if (p_user->UserLevel > level)
	{
		return FALSE;
	}
		
	nonce_len = base64_decode_bm(p_Nonce->data, strlen(p_Nonce->data), nonce, sizeof(nonce));	
	
	soap_calc_digest(p_Created->data, nonce, nonce_len, p_user->Password, HA);
	base64_encode_bm(HA, 20, HABase64, sizeof(HABase64));

	if (strcmp(HABase64, p_Password->data) == 0)
	{
		return TRUE;
	}
	
	return FALSE;
}

/**
 * if the request not need auth, return TRUE, or FALSE
 */
BOOL soap_is_req_not_need_auth(const char * request)
{
#ifdef PROFILE_Q_SUPPORT
	// A device shall provide full anonymous access to all ONVIF commands 
	//  while the device operates in Factory Default State

	if (0 == g_onvif_cfg.device_state)
	{
		return TRUE;
	}
#endif

	if (soap_strcmp(request, "GetCapabilities") == 0)
	{
		return TRUE;
	}
	else if (soap_strcmp(request, "GetServices") == 0)
	{
		return TRUE;
	}
	else if (soap_strcmp(request, "GetServiceCapabilities") == 0)
	{
		return TRUE;
	}
	else if (soap_strcmp(request, "GetSystemDateAndTime") == 0)
	{
		return TRUE;
	}
	else if (soap_strcmp(request, "GetEndpointReference") == 0)
	{
		return TRUE;
	}
	else if (soap_strcmp(request, "GetWsdlUrl") == 0)
	{
		return TRUE;
	}
	else if (soap_strcmp(request, "GetHostname") == 0)
	{
		return TRUE;
	}

	return FALSE;
}

onvif_UserLevel soap_get_user_level(const char * request)
{
	onvif_UserLevel level = UserLevel_User;

	if (soap_strcmp(request, "SetScopes") == 0)
	{
		level = UserLevel_Administrator;
	}
	else if (soap_strcmp(request, "SetDiscoveryMode") == 0)
	{
		level = UserLevel_Administrator;
	}
	else if (soap_strcmp(request, "GetAccessPolicy") == 0)
	{
		level = UserLevel_Administrator;
	}
	else if (soap_strcmp(request, "CreateUsers") == 0)
	{
		level = UserLevel_Administrator;
	}
	else if (soap_strcmp(request, "SetSystemDateAndTime") == 0)
	{
		level = UserLevel_Administrator;
	}

	return level;
}

BOOL soap_auth_handler(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_node, const char * reqest)
{
	onvif_refresh_User();
	int errcode = 401;
	BOOL auth = FALSE;	
	onvif_UserLevel level = UserLevel_Anonymous;

	level = soap_get_user_level(reqest);

	auth = soap_is_req_not_need_auth(reqest);
	if (!auth)
	{
		XMLN * p_header = xml_node_soap_get(p_node, "Header");
		if (p_header)
		{
			XMLN * p_Security = xml_node_soap_get(p_header, "Security");
			if (p_Security)
			{
				errcode = 400;
				auth = soap_auth_process(p_Security, level);
			}
		}
	}

	if (!auth)
	{
		HD_AUTH_INFO auth_info;
		memset(&auth_info, 0, sizeof(auth_info));

		// check http digest auth information
		if (http_get_auth_digest_info(rx_msg, &auth_info))
		{
			onvif_User * p_onvifuser = onvif_find_user(auth_info.auth_name);
			if (NULL == p_onvifuser)	// user not exist
			{
				auth = FALSE;
			}
			else if (p_onvifuser->UserLevel > level)
			{
				auth = FALSE;
			}
			else
			{
				auth = DigestAuthProcess(&auth_info, &g_onvif_cls.onvif_auth, "POST", p_onvifuser->Password);
			}
		}

		if (!auth)
		{
			soap_security_rly(p_user, rx_msg, errcode);
		}
	}

	return auth;
}

BOOL soap_match_namespace(XMLN * p_node, const char * ns, const char * prefix)
{
	XMLN * p_attr;

	p_attr = p_node->f_attrib;
	while (p_attr != NULL)
	{
		if (NTYPE_ATTRIB == p_attr->type)
		{
			if (NULL == prefix)
			{
				if (strcasecmp(p_attr->data, ns) == 0)
				{
					return TRUE;
				}
			}
			else
			{
				const char * ptr1;

				ptr1 = strchr(p_attr->name, ':');
				if (ptr1)
				{
					if (strcasecmp(ptr1+1, prefix) == 0 && strcasecmp(p_attr->data, ns) == 0)
					{
						return TRUE;
					}
				}
			}
		}

		p_attr = p_attr->next;
	}

	return FALSE;
}

BOOL soap_is_namespace(XMLN * p_body, const char * ns)
{
	const char * ptr1;
	XMLN * p_name = p_body->f_child;
	char prefix[32] = {'\0'};

	ptr1 = strchr(p_name->name, ':');
	if (ptr1)
	{
		memcpy(prefix, p_name->name, ptr1 - p_name->name);
	}

	if (prefix[0] == '\0')
	{
		return soap_match_namespace(p_name, ns, NULL);        
	}
	else
	{
		while (p_name)
		{
			if (soap_match_namespace(p_name, ns, prefix))
			{
				return TRUE;
			}

			p_name = p_name->parent;
		}
	}

	return FALSE;
}

int soap_process_device_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)/*1*/
	{
		soap_tds_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDeviceInformation") == 0)/*1*/
	{
		soap_tds_GetDeviceInformation(p_user, rx_msg, p_body, p_header);
	}
	/*else if (soap_strcmp(p_name, "GetSystemUris") == 0)
	{
		soap_tds_GetSystemUris(p_user, rx_msg, p_body, p_header);
	}*/
	else if (soap_strcmp(p_name, "GetCapabilities") == 0)/*1*/
	{
		soap_tds_GetCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSystemDateAndTime") == 0)/*1*/
	{
		soap_tds_GetSystemDateAndTime(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSystemDateAndTime") == 0)/*1*/
	{
		soap_tds_SetSystemDateAndTime(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetNetworkInterfaces") == 0)/*1*/
	{	
		soap_tds_GetNetworkInterfaces(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetNetworkInterfaces") == 0)/*1*/
	{
		soap_tds_SetNetworkInterfaces(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SystemReboot") == 0)/*1*/
	{
		soap_tds_SystemReboot(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSystemFactoryDefault") == 0)/*1*/
	{
		soap_tds_SetSystemFactoryDefault(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSystemLog") == 0)/*1*/
	{
		soap_tds_GetSystemLog(p_user, rx_msg, p_body, p_header);
	}	
	else if (soap_strcmp(p_name, "GetServices") == 0)/*1*/
	{
		soap_tds_GetServices(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetScopes") == 0)/*1*/
	{
		soap_tds_GetScopes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddScopes") == 0)/*1*/
	{
		soap_tds_AddScopes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetScopes") == 0)/*1*/
	{
		soap_tds_SetScopes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveScopes") == 0)/*1*/
	{
		soap_tds_RemoveScopes(p_user, rx_msg, p_body, p_header);
	}	
	else if (soap_strcmp(p_name, "GetHostname") == 0)/*1*/
	{
		soap_tds_GetHostname(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetHostname") == 0)/*1*/
	{
		soap_tds_SetHostname(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetHostnameFromDHCP") == 0)/*1*/
	{
		soap_tds_SetHostnameFromDHCP(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetNetworkProtocols") == 0)/*1*/
	{
		soap_tds_GetNetworkProtocols(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetNetworkProtocols") == 0)/*1*/
	{
		soap_tds_SetNetworkProtocols(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetNetworkDefaultGateway") == 0)/*1*/
	{
		soap_tds_GetNetworkDefaultGateway(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetNetworkDefaultGateway") == 0)/*1*/
	{
		soap_tds_SetNetworkDefaultGateway(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDiscoveryMode") == 0)/*1*/
	{
		soap_tds_GetDiscoveryMode(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetDiscoveryMode") == 0)/*1*/
	{
		soap_tds_SetDiscoveryMode(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDNS") == 0)/*1*/
	{
		soap_tds_GetDNS(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetDNS") == 0)/*1*/
	{
		soap_tds_SetDNS(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDynamicDNS") == 0)/*2*/
	{
		soap_tds_GetDynamicDNS(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetDynamicDNS") == 0)/*2*/
	{
		soap_tds_SetDynamicDNS(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetNTP") == 0)/*1*/
	{
		soap_tds_GetNTP(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetNTP") == 0)/*1*/
	{
		soap_tds_SetNTP(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetZeroConfiguration") == 0)/*1*/
	{
		soap_tds_GetZeroConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetZeroConfiguration") == 0)/*1*/
	{
		soap_tds_SetZeroConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetWsdlUrl") == 0)/*1*/
	{
		soap_tds_GetWsdlUrl(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetEndpointReference") == 0)/*1*/
	{
		soap_tds_GetEndpointReference(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetUsers") == 0)/*1*/
	{
		soap_tds_GetUsers(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateUsers") == 0)/*1*/
	{
		soap_tds_CreateUsers(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteUsers") == 0)/*1*/
	{
		soap_tds_DeleteUsers(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetUser") == 0)/*1*/
	{
		soap_tds_SetUser(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRemoteUser") == 0)/*2*/
	{
		soap_tds_GetRemoteUser(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRemoteUser") == 0)/*2*/
	{
		soap_tds_SetRemoteUser(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "UpgradeSystemFirmware") == 0)/*2*/
	{
		soap_tds_UpgradeSystemFirmware(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "StartFirmwareUpgrade") == 0)/*1*/
	{
		soap_tds_StartFirmwareUpgrade(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "StartSystemRestore") == 0)/*2*/
	{
		soap_tds_StartSystemRestore(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetHashingAlgorithm") == 0)/*2*/
	{
		soap_tds_SetHashingAlgorithm(p_user, rx_msg, p_body, p_header);
	}
#ifdef DEVICEIO_SUPPORT	
	else if (soap_strcmp(p_name, "GetRelayOutputs") == 0)/*1*/
	{
		soap_tds_GetRelayOutputs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRelayOutputSettings") == 0)/*1*/
	{
		soap_tds_SetRelayOutputSettings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRelayOutputState") == 0)/*1*/
	{
		soap_tds_SetRelayOutputState(p_user, rx_msg, p_body, p_header);
	}
#endif //  DEVICEIO_SUPPORT
#ifdef IPFILTER_SUPPORT	
	else if (soap_strcmp(p_name, "GetIPAddressFilter") == 0)/*2*/
	{
		soap_tds_GetIPAddressFilter(p_user, rx_msg, p_body, p_header);
	}	
	else if (soap_strcmp(p_name, "SetIPAddressFilter") == 0)/*2*/
	{
		soap_tds_SetIPAddressFilter(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddIPAddressFilter") == 0)/*2*/
	{
		soap_tds_AddIPAddressFilter(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveIPAddressFilter") == 0)/*2*/
	{
		soap_tds_RemoveIPAddressFilter(p_user, rx_msg, p_body, p_header);
	}
#endif // IPFILTER_SUPPORT
#ifdef STORAGE_SUPPORT
	else if (soap_strcmp(p_name, "GetStorageConfigurations") == 0)/*2*/
	{
		soap_tds_GetStorageConfigurations(p_user, rx_msg, p_body, p_header);
	}	
	else if (soap_strcmp(p_name, "GetStorageConfiguration") == 0)/*2*/
	{
		soap_tds_GetStorageConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateStorageConfiguration") == 0)/*2*/
	{
		soap_tds_CreateStorageConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetStorageConfiguration") == 0)/*2*/
	{
		soap_tds_SetStorageConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteStorageConfiguration") == 0)/*2*/
	{
		soap_tds_DeleteStorageConfiguration(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef GEOLOCATION_SUPPORT
	else if (soap_strcmp(p_name, "GetGeoLocation") == 0)/*2*/
	{
		soap_tds_GetGeoLocation(p_user, rx_msg, p_body, p_header);
	}	
	else if (soap_strcmp(p_name, "SetGeoLocation") == 0)/*2*/
	{
		soap_tds_SetGeoLocation(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteGeoLocation") == 0)/*2*/
	{
		soap_tds_DeleteGeoLocation(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef DOT11_SUPPORT
	else if (soap_strcmp(p_name, "GetDot11Capabilities") == 0)/*2*/
	{
		soap_tds_GetDot11Capabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDot11Status") == 0)/*2*/
	{
		soap_tds_GetDot11Status(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ScanAvailableDot11Networks") == 0)/*2*/
	{
		soap_tds_ScanAvailableDot11Networks(p_user, rx_msg, p_body, p_header);
	}
#endif
	else
	{
		ret = 0;
	}

	return ret;
}

int soap_process_hbgk_ext_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetPrivacyMaskOptions") == 0)											/*1*/
	{
		soap_hbgk_ext_GetPrivacyMaskOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPrivacyMasks") == 0)											/*1*/
	{
		soap_hbgk_ext_GetPrivacyMasks(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreatePrivacyMask") == 0)											/*1*/
	{
		soap_hbgk_ext_CreatePrivacyMask(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeletePrivacyMask") == 0)											/*1*/
	{
		soap_hbgk_ext_DeletePrivacyMask(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetPrivacyMask") == 0)											/*1*/
	{
		soap_hbgk_ext_SetPrivacyMask(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}
	
	return ret;
}

int soap_process_tpl_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)									/*1*/
	{
		soap_tpl_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPrivacyMaskOptions") == 0)											/*1*/
	{
		soap_tpl_GetPrivacyMaskOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPrivacyMasks") == 0)											/*1*/
	{
		soap_tpl_GetPrivacyMasks(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreatePrivacyMask") == 0)											/*1*/
	{
		soap_tpl_CreatePrivacyMask(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeletePrivacyMask") == 0)											/*1*/
	{
		soap_tpl_DeletePrivacyMask(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetPrivacyMask") == 0)											/*1*/
	{
		soap_tpl_SetPrivacyMask(p_user, rx_msg, p_body, p_header);
	}
	
	
	else if (soap_strcmp(p_name, "GetVideoEncoderConfigurationOptions") == 0)						/*1*/
	{
		soap_tpl_GetVideoEncoderConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetVideoEncoderConfiguration") == 0)								/*1*/
	{
		soap_tpl_SetVideoEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetImagingOptions") == 0)											/*1*/
	{
		soap_tpl_GetImagingOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDigitalInputs") == 0)											/*1*/
	{
		soap_tpl_GetDigitalInputs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetImagingSettings") == 0)										/*1*/
	{
		soap_tpl_SetImagingSettings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetImagingSettings") == 0)										/*1*/
	{
		soap_tpl_GetImagingSettings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetDigitalInputConfiguration") == 0)								/*1*/
	{
		soap_tpl_SetDigitalInputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetImagingSettingDefault") == 0)									/*1*/
	{
		soap_tpl_SetImagingSettingDefault(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SendCommand") == 0)												/*1*/
	{
		soap_tpl_SendCommand(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

int soap_process_event_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)/*1*/
	{
		soap_tev_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetEventProperties") == 0)/*1*/
	{
		soap_tev_GetEventProperties(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "Subscribe") == 0)/*1*/
	{
		soap_tev_Subscribe(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "Unsubscribe") == 0)/*1*/
	{
		soap_tev_Unsubscribe(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "Renew") == 0)/*1*/
	{
		soap_tev_Renew(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreatePullPointSubscription") == 0)/*1*/
	{
		soap_tev_CreatePullPointSubscription(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "PullMessages") == 0)/*1*/
	{
		soap_tev_PullMessages(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSynchronizationPoint") == 0)/*1*/
	{
		soap_tev_SetSynchronizationPoint(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}
	
	return ret;
}

#ifdef IMAGE_SUPPORT

int soap_process_image_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)/*1*/
	{
		soap_img_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetImagingSettings") == 0)/*1*/
	{
		soap_img_GetImagingSettings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetImagingSettings") == 0)/*1*/
	{
		soap_img_SetImagingSettings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetOptions") == 0)/*1*/
	{
		soap_img_GetOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMoveOptions") == 0)/*1*/
	{
		soap_img_GetMoveOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "Move") == 0)/*1*/
	{
		soap_img_Move(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetStatus") == 0)/*1*/
	{
		soap_img_GetStatus(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "Stop") == 0)/*1*/
	{
		soap_img_Stop(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPresets") == 0)/*2*/
	{
		soap_img_GetPresets(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCurrentPreset") == 0)/*2*/
	{
		soap_img_GetCurrentPreset(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetCurrentPreset") == 0)/*2*/
	{
		soap_img_SetCurrentPreset(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // IMAGE_SUPPORT

#ifdef MEDIA_SUPPORT

int soap_process_media_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)								/*1*/
	{
		soap_trt_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetProfiles") == 0)									/*1*/
	{
		soap_trt_GetProfiles(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetProfile") == 0)									/*1*/
	{
		soap_trt_GetProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateProfile") == 0)									/*1*/
	{
		soap_trt_CreateProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteProfile") == 0)									/*1*/
	{
		soap_trt_DeleteProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSourceModes") == 0)							/*1*/
	{
		soap_trt_GetVideoSourceModes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetVideoSourceMode") == 0)							/*1*/
	{
		soap_trt_SetVideoSourceMode(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddVideoSourceConfiguration") == 0)					/*1*/
	{
		soap_trt_AddVideoSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveVideoSourceConfiguration") == 0)				/*1*/
	{
		soap_trt_RemoveVideoSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddVideoEncoderConfiguration") == 0)					/*1*/
	{
		soap_trt_AddVideoEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveVideoEncoderConfiguration") == 0)				/*1*/
	{
		soap_trt_RemoveVideoEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSources") == 0)								/*1*/
	{
		soap_trt_GetVideoSources(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSourceConfiguration") == 0)					/*1*/
	{
		soap_trt_GetVideoSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSourceConfigurations") == 0)					/*1*/
	{
		soap_trt_GetVideoSourceConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSourceConfigurationOptions") == 0)			/*1*/
	{
		soap_trt_GetVideoSourceConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleVideoSourceConfigurations") == 0)		/*1*/
	{
		soap_trt_GetCompatibleVideoSourceConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoEncoderConfiguration") == 0)					/*1*/
	{
		soap_trt_GetVideoEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoEncoderConfigurations") == 0)					/*1*/
	{
		soap_trt_GetVideoEncoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoEncoderConfigurationOptions") == 0)			/*1*/
	{
		soap_trt_GetVideoEncoderConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleVideoEncoderConfigurations") == 0)		/*1*/
	{
		soap_trt_GetCompatibleVideoEncoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetVideoSourceConfiguration") == 0)					/*2*/
	{
		soap_trt_SetVideoSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetVideoEncoderConfiguration") == 0)					/*1*/
	{
		soap_trt_SetVideoEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetStreamUri") == 0)									/*1*/
	{
		soap_trt_GetStreamUri(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSnapshotUri") == 0)								/*1*/
	{
		soap_trt_GetSnapshotUri(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSynchronizationPoint") == 0)						/*1*/
	{
		soap_trt_SetSynchronizationPoint(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetGuaranteedNumberOfVideoEncoderInstances") == 0)	/*2*/
	{
		soap_trt_GetGuaranteedNumberOfVideoEncoderInstances(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetOSDs") == 0)										/*1*/
	{
		soap_trt_GetOSDs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetOSD") == 0)										/*1*/
	{
		soap_trt_GetOSD(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetOSD") == 0)										/*1*/
	{
		soap_trt_SetOSD(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetOSDOptions") == 0)									/*1*/
	{
		soap_trt_GetOSDOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateOSD") == 0)										/*2*/
	{
		soap_trt_CreateOSD(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteOSD") == 0)										/*2*/
	{
		soap_trt_DeleteOSD(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "StartMulticastStreaming") == 0)						/*2*/
	{
		soap_trt_StartMulticastStreaming(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "StopMulticastStreaming") == 0)						/*2*/
	{
		soap_trt_StopMulticastStreaming(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMetadataConfigurations") == 0)						/*2*/
	{
		soap_trt_GetMetadataConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMetadataConfiguration") == 0)						/*2*/
	{
		soap_trt_GetMetadataConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleMetadataConfigurations") == 0)			/*2*/
	{
		soap_trt_GetCompatibleMetadataConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMetadataConfigurationOptions") == 0)				/*2*/
	{
		soap_trt_GetMetadataConfigurationOptions(p_user, rx_msg, p_body, p_header);
	} 
	else if (soap_strcmp(p_name, "SetMetadataConfiguration") == 0)						/*2*/
	{
		soap_trt_SetMetadataConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddMetadataConfiguration") == 0)						/*2*/
	{
		soap_trt_AddMetadataConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveMetadataConfiguration") == 0)					/*2*/
	{
		soap_trt_RemoveMetadataConfiguration(p_user, rx_msg, p_body, p_header);
	}
#ifdef AUDIO_SUPPORT
	else if (soap_strcmp(p_name, "AddAudioSourceConfiguration") == 0)					/*2*/
	{
		soap_trt_AddAudioSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveAudioSourceConfiguration") == 0)				/*2*/
	{
		soap_trt_RemoveAudioSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddAudioEncoderConfiguration") == 0)					/*2*/
	{
		soap_trt_AddAudioEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveAudioEncoderConfiguration") == 0)				/*2*/
	{
		soap_trt_RemoveAudioEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioSources") == 0)								/*1*/
	{
		soap_trt_GetAudioSources(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioSourceConfigurations") == 0)					/*1*/
	{
		soap_trt_GetAudioSourceConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleAudioSourceConfigurations") == 0)		/*1*/
	{
		soap_trt_GetCompatibleAudioSourceConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioEncoderConfiguration") == 0)					/*1*/
	{
		soap_trt_GetAudioEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioEncoderConfigurations") == 0)					/*1*/
	{
		soap_trt_GetAudioEncoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleAudioEncoderConfigurations") == 0)		/*1*/
	{
		soap_trt_GetCompatibleAudioEncoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioSourceConfigurationOptions") == 0)			/*1*/
	{
		soap_trt_GetAudioSourceConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioSourceConfiguration") == 0)					/*1*/
	{
		soap_trt_GetAudioSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioSourceConfiguration") == 0)					/*2*/
	{
		soap_trt_SetAudioSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioEncoderConfiguration") == 0)					/*1*/
	{
		soap_trt_SetAudioEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioEncoderConfigurationOptions") == 0)			/*1*/
	{
		soap_trt_GetAudioEncoderConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddAudioDecoderConfiguration") == 0)					/*2*/
	{
		soap_trt_AddAudioDecoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioDecoderConfigurations") == 0)					/*2*/
	{
		soap_trt_GetAudioDecoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioDecoderConfiguration") == 0)					/*2*/
	{
		soap_trt_GetAudioDecoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveAudioDecoderConfiguration") == 0)				/*2*/
	{
		soap_trt_RemoveAudioDecoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioDecoderConfiguration") == 0)					/*2*/
	{
		soap_trt_SetAudioDecoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioDecoderConfigurationOptions") == 0)			/*2*/
	{
		soap_trt_GetAudioDecoderConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleAudioDecoderConfigurations") == 0)		/*2*/
	{
		soap_trt_GetCompatibleAudioDecoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
#ifdef DEVICEIO_SUPPORT
	else if (soap_strcmp(p_name, "GetAudioOutputs") == 0)								/*1*/
	{
		soap_trt_GetAudioOutputs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioOutputConfigurations") == 0)					/*2*/
	{
		soap_trt_GetAudioOutputConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioOutputConfiguration") == 0)					/*2*/
	{
		soap_trt_SetAudioOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioOutputConfigurationOptions") == 0)			/*2*/
	{
		soap_trt_GetAudioOutputConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddAudioOutputConfiguration") == 0)					/*2*/
	{
		soap_trt_AddAudioOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveAudioOutputConfiguration") == 0)				/*2*/
	{
		soap_trt_RemoveAudioOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleAudioOutputConfigurations") == 0)		/*2*/
	{
		soap_trt_GetCompatibleAudioOutputConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioOutputConfiguration") == 0)					/*2*/
	{
		soap_trt_GetAudioOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
#endif // end of DEVICEIO_SUPPORT
#endif // end of AUDIO_SUPPORT
#ifdef PTZ_SUPPORT
	else if (soap_strcmp(p_name, "AddPTZConfiguration") == 0)							/*1*/
	{
		soap_trt_AddPTZConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemovePTZConfiguration") == 0)						/*2*/
	{
		soap_trt_RemovePTZConfiguration(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef VIDEO_ANALYTICS	
	else if (soap_strcmp(p_name, "GetVideoAnalyticsConfiguration") == 0)				/**/
	{
		soap_trt_GetVideoAnalyticsConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoAnalyticsConfigurations") == 0)				/**/
	{
		soap_trt_GetVideoAnalyticsConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleVideoAnalyticsConfigurations") == 0)		/**/
	{
		soap_trt_GetCompatibleVideoAnalyticsConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetVideoAnalyticsConfiguration") == 0)				/**/
	{
		soap_trt_SetVideoAnalyticsConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddVideoAnalyticsConfiguration") == 0)				/*2*/
	{
		soap_trt_AddVideoAnalyticsConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveVideoAnalyticsConfiguration") == 0)				/*2*/
	{
		soap_trt_RemoveVideoAnalyticsConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAnalyticsConfigurations") == 0)					/**/
	{
		soap_trt_GetAnalyticsConfigurations(p_user, rx_msg, p_body, p_header);
	}
#endif	// endif of VIDEO_ANALYTICS	
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // MEDIA_SUPPORT

#ifdef MEDIA2_SUPPORT

int soap_process_media2_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)									/*1*/
	{
		soap_tr2_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetProfiles") == 0)										/*1*/
	{
		soap_tr2_GetProfiles(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateProfile") == 0)										/*2*/
	{
		soap_tr2_CreateProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteProfile") == 0)										/*2*/
	{
		soap_tr2_DeleteProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSourceModes") == 0)								/*1*/
	{
		soap_tr2_GetVideoSourceModes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetVideoSourceMode") == 0)								/*2*/
	{
		soap_tr2_SetVideoSourceMode(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetStreamUri") == 0)										/*1*/
	{
		soap_tr2_GetStreamUri(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoEncoderConfigurations") == 0)						/*1*/
	{
		soap_tr2_GetVideoEncoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSourceConfigurations") == 0)						/*1*/
	{
		soap_tr2_GetVideoSourceConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSourceConfigurationOptions") == 0)				/*1*/
	{
		soap_tr2_GetVideoSourceConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}	
	else if (soap_strcmp(p_name, "SetVideoSourceConfiguration") == 0)						/*2*/
	{
		soap_tr2_SetVideoSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}	
	else if (soap_strcmp(p_name, "SetVideoEncoderConfiguration") == 0)						/*1*/
	{
		soap_tr2_SetVideoEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoEncoderConfigurationOptions") == 0)				/*1*/
	{
		soap_tr2_GetVideoEncoderConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSnapshotUri") == 0)									/*1*/
	{
		soap_tr2_GetSnapshotUri(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSynchronizationPoint") == 0)							/*1*/
	{
		soap_tr2_SetSynchronizationPoint(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetOSDs") == 0)											/*1*/
	{
		soap_tr2_GetOSDs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetOSD") == 0)											/*1*/
	{
		soap_tr2_SetOSD(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetOSDOptions") == 0)										/*1*/
	{
		soap_tr2_GetOSDOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateOSD") == 0)											/*2*/
	{
		soap_tr2_CreateOSD(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteOSD") == 0)											/*2*/
	{
		soap_tr2_DeleteOSD(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMetadataConfigurations") == 0)							/*1*/
	{
		soap_tr2_GetMetadataConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMetadataConfigurationOptions") == 0)					/*2*/
	{
		soap_tr2_GetMetadataConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}	 
	else if (soap_strcmp(p_name, "SetMetadataConfiguration") == 0)							/*2*/
	{
		soap_tr2_SetMetadataConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddConfiguration") == 0)									/*2*/
	{
		soap_tr2_AddConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveConfiguration") == 0)								/*2*/
	{
		soap_tr2_RemoveConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoEncoderInstances") == 0)							/*1*/
	{
		soap_tr2_GetVideoEncoderInstances(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateMask") == 0)										/*1*/
	{
		soap_tr2_CreateMask(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteMask") == 0)										/*1*/
	{
		soap_tr2_DeleteMask(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMasks") == 0)											/*1*/
	{
		soap_tr2_GetMasks(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetMask") == 0)											/*1*/
	{
		soap_tr2_SetMask(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMaskOptions") == 0)									/*1*/
	{
		soap_tr2_GetMaskOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "StartMulticastStreaming") == 0)							/*2*/
	{
		soap_tr2_StartMulticastStreaming(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "StopMulticastStreaming") == 0)							/*2*/
	{
		soap_tr2_StopMulticastStreaming(p_user, rx_msg, p_body, p_header);
	}
#ifdef AUDIO_SUPPORT
	else if (soap_strcmp(p_name, "GetAudioSourceConfigurations") == 0)						/*1*/
	{
		soap_tr2_GetAudioSourceConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioSourceConfigurationOptions") == 0)				/*1*/
	{
		soap_tr2_GetAudioSourceConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioSourceConfiguration") == 0)						/*2*/
	{
		soap_tr2_SetAudioSourceConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioEncoderConfigurationOptions") == 0)				/*1*/
	{
		soap_tr2_GetAudioEncoderConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioEncoderConfigurations") == 0)						/*1*/
	{
		soap_tr2_GetAudioEncoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioEncoderConfiguration") == 0)						/*1*/
	{
		soap_tr2_SetAudioEncoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioDecoderConfigurationOptions") == 0)				/*1*/
	{
		soap_tr2_GetAudioDecoderConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioDecoderConfigurations") == 0)						/*1*/
	{
		soap_tr2_GetAudioDecoderConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioDecoderConfiguration") == 0)						/*2*/
	{
		soap_tr2_SetAudioDecoderConfiguration(p_user, rx_msg, p_body, p_header);
	}
#endif // end of AUDIO_SUPPORT
#ifdef DEVICEIO_SUPPORT
	else if (soap_strcmp(p_name, "GetAudioOutputConfigurations") == 0)						/*2*/
	{
		soap_tr2_GetAudioOutputConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAudioOutputConfiguration") == 0)						/*2*/
	{
		soap_tr2_SetAudioOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioOutputConfigurationOptions") == 0)				/*2*/
	{
		soap_tr2_GetAudioOutputConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
#endif // end of DEVICEIO_SUPPORT
#ifdef VIDEO_ANALYTICS	
	else if (soap_strcmp(p_name, "GetAnalyticsConfigurations") == 0)						/**/
	{
		soap_tr2_GetAnalyticsConfigurations(p_user, rx_msg, p_body, p_header);
	}
#endif	// endif of VIDEO_ANALYTICS
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // MEDIA2_SUPPORT

#ifdef DEVICEIO_SUPPORT

int soap_process_deviceio_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)									/*1*/
	{
		soap_tmd_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoSources") == 0)									/*1*/
	{
		soap_tmd_GetVideoSources(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoOutputs") == 0)									/*2*/
	{
		soap_tmd_GetVideoOutputs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoOutputConfiguration") == 0)						/*ԭ����0*/
	{
		soap_tmd_GetVideoOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetVideoOutputConfiguration") == 0)						/*ԭ����0*/
	{
		soap_tmd_SetVideoOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetVideoOutputConfigurationOptions") == 0)				/*2*/
	{
		soap_tmd_GetVideoOutputConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioOutputs") == 0)									/*1*/
	{
		soap_tmd_GetAudioOutputs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioOutputConfiguration") == 0)						/*2*/
	{
		soap_tmd_GetAudioOutputConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAudioOutputConfigurationOptions") == 0)				/*2*/
	{
		soap_tmd_GetAudioOutputConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRelayOutputs") == 0)									/*1*/
	{
		if (soap_is_namespace(p_body, "http://www.onvif.org/ver10/device/wsdl"))
		{
			soap_tds_GetRelayOutputs(p_user, rx_msg, p_body, p_header);
		}
		else
		{
			soap_tmd_GetRelayOutputs(p_user, rx_msg, p_body, p_header);
		}
	}
	else if (soap_strcmp(p_name, "GetRelayOutputOptions") == 0)								/*1*/
	{
		soap_tmd_GetRelayOutputOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRelayOutputSettings") == 0)							/*1*/
	{
		soap_tmd_SetRelayOutputSettings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRelayOutputState") == 0)								/*1*/
	{
		if (soap_is_namespace(p_body, "http://www.onvif.org/ver10/device/wsdl"))
		{
			soap_tds_SetRelayOutputState(p_user, rx_msg, p_body, p_header);
		}
		else
		{
			soap_tmd_SetRelayOutputState(p_user, rx_msg, p_body, p_header);
		}
	}
	else if (soap_strcmp(p_name, "GetDigitalInputs") == 0)									/*1*/
	{
		soap_tmd_GetDigitalInputs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDigitalInputConfigurationOptions") == 0)				/*2*/
	{
		soap_tmd_GetDigitalInputConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetDigitalInputConfigurations") == 0)						/*2*/
	{
		soap_tmd_SetDigitalInputConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSerialPorts") == 0)									/*2*/
	{
		soap_tmd_GetSerialPorts(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSerialPortConfiguration") == 0)						/*2*/
	{
		soap_tmd_GetSerialPortConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSerialPortConfigurationOptions") == 0)					/*2*/
	{
		soap_tmd_GetSerialPortConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSerialPortConfiguration") == 0)						/*2*/
	{
		soap_tmd_SetSerialPortConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SendReceiveSerialCommand") == 0)							/*2*/
	{
		soap_tmd_SendReceiveSerialCommand(p_user, rx_msg, p_body, p_header);
	}
#ifdef AUDIO_SUPPORT
	else if (soap_strcmp(p_name, "GetAudioSources") == 0)									/*1*/
	{
		soap_tmd_GetAudioSources(p_user, rx_msg, p_body, p_header);
	}
#endif // end of AUDIO_SUPPORT
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // DEVICEIO_SUPPORT

#ifdef PTZ_SUPPORT

int soap_process_ptz_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)					/*1*/
	{
		soap_ptz_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetStatus") == 0)							/*1*/
	{
		soap_ptz_GetStatus(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "Stop") == 0)								/*1*/
	{
		soap_ptz_Stop(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetConfigurations") == 0)					/*1*/
	{
		soap_ptz_GetConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetConfiguration") == 0)					/*1*/
	{
		soap_ptz_GetConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetConfiguration") == 0)					/*1*/
	{
		soap_ptz_SetConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetConfigurationOptions") == 0)			/*1*/
	{	
		soap_ptz_GetConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetNodes") == 0)							/*1*/
	{
		soap_ptz_GetNodes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetNode") == 0)							/*1*/
	{
		soap_ptz_GetNode(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCompatibleConfigurations") == 0)		/*1*/
	{
		soap_ptz_GetCompatibleConfigurations(p_user, rx_msg, p_body, p_header);
	}
	////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_name, "ContinuousMove") == 0)					/*1*/
	{
		soap_ptz_ContinuousMove(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AbsoluteMove") == 0)						/*1*/
	{
		soap_ptz_AbsoluteMove(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RelativeMove") == 0)						/*1*/
	{
		soap_ptz_RelativeMove(p_user, rx_msg, p_body, p_header);
	}
	////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_name, "SetPreset") == 0)							/*1*/
	{
		soap_ptz_SetPreset(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPresets") == 0)						/**/
	{
		soap_ptz_GetPresets(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemovePreset") == 0)						/*1*/
	{
		soap_ptz_RemovePreset(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GotoPreset") == 0)						/*1*/
	{
		soap_ptz_GotoPreset(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GotoHomePosition") == 0)					/*1*/
	{
		soap_ptz_GotoHomePosition(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetHomePosition") == 0)					/*1*/
	{
		soap_ptz_SetHomePosition(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPresetTours") == 0)					/*1*/
	{
		soap_ptz_GetPresetTours(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPresetTour") == 0)						/*1*/
	{
		soap_ptz_GetPresetTour(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPresetTourOptions") == 0)				/*2*/
	{
		soap_ptz_GetPresetTourOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreatePresetTour") == 0)					/*1*/
	{
		soap_ptz_CreatePresetTour(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyPresetTour") == 0)					/*1*/
	{
		soap_ptz_ModifyPresetTour(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "OperatePresetTour") == 0)					/*1*/
	{
		soap_ptz_OperatePresetTour(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemovePresetTour") == 0)					/*1*/
	{
		soap_ptz_RemovePresetTour(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SendAuxiliaryCommand") == 0)				/*1*/
	{
		soap_ptz_SendAuxiliaryCommand(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GeoMove") == 0)							/*2*/
	{
		soap_ptz_GeoMove(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // PTZ_SUPPORT

#ifdef THERMAL_SUPPORT

int soap_process_thermal_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)								/*2*/
	{
		soap_tth_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetConfigurations") == 0)								/*0*/
	{
		soap_tth_GetConfigurations(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetConfiguration") == 0)								/*0*/
	{
		soap_tth_GetConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetConfiguration") == 0)								/*0*/
	{
		soap_tth_SetConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetConfigurationOptions") == 0)						/*0*/
	{
		soap_tth_GetConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRadiometryConfiguration") == 0)					/*0*/
	{
		soap_tth_GetRadiometryConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRadiometryConfiguration") == 0)					/*0*/
	{
		soap_tth_SetRadiometryConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRadiometryConfigurationOptions") == 0)				/*0*/
	{
		soap_tth_GetRadiometryConfigurationOptions(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;

}

#endif // THERMAL_SUPPORT

#ifdef VIDEO_ANALYTICS

int soap_process_analytics_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)							/*2*/
	{
		soap_tan_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSupportedRules") == 0)							/*1*///��ȡ֧�ֵĹ�������Щ
	{
		soap_tan_GetSupportedRules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateRules") == 0)								/*2*/
	{
		soap_tan_CreateRules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteRules") == 0)								/*2*/
	{
		soap_tan_DeleteRules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRules") == 0)									/*1*///��ȡ�����µĵ�ǰ����
	{
		soap_tan_GetRules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyRules") == 0)								/*1*///���ò���
	{
		soap_tan_ModifyRules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateAnalyticsModules") == 0)					/*2*/
	{
		soap_tan_CreateAnalyticsModules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteAnalyticsModules") == 0)					/*2*/
	{
		soap_tan_DeleteAnalyticsModules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAnalyticsModules") == 0)						/*1*///��ȡ�����µĵ�ǰ����
	{
		soap_tan_GetAnalyticsModules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyAnalyticsModules") == 0)					/*1*///���ò���
	{
		soap_tan_ModifyAnalyticsModules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRuleOptions") == 0)							/*0*/
	{
		soap_tan_GetRuleOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSupportedAnalyticsModules") == 0)				/*1*///��ȡ֧�ֵĹ����µĲ���
	{
		soap_tan_GetSupportedAnalyticsModules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAnalyticsModuleOptions") == 0)					/*0*/
	{
		soap_tan_GetAnalyticsModuleOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSupportedMetadata") == 0)						/*2*/
	{
		soap_tan_GetSupportedMetadata(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT

int soap_process_recording_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)							/**/
	{
		soap_trc_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateRecording") == 0)							/*2*/
	{
		soap_trc_CreateRecording(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteRecording") == 0)							/*2*/
	{
		soap_trc_DeleteRecording(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordings") == 0)								/*2*/
	{
		soap_trc_GetRecordings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRecordingConfiguration") == 0)					/*2*/
	{
		soap_trc_SetRecordingConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingConfiguration") == 0)					/*2*/
	{
		soap_trc_GetRecordingConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateTrack") == 0)								/*2*/
	{
		soap_trc_CreateTrack(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteTrack") == 0)								/*2*/
	{
		soap_trc_DeleteTrack(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetTrackConfiguration") == 0)						/*2*/
	{
		soap_trc_GetTrackConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetTrackConfiguration") == 0)						/*2*/
	{
		soap_trc_SetTrackConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateRecordingJob") == 0)						/*2*/
	{
		soap_trc_CreateRecordingJob(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteRecordingJob") == 0)						/*2*/
	{
		soap_trc_DeleteRecordingJob(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingJobs") == 0)							/*2*/
	{
		soap_trc_GetRecordingJobs(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRecordingJobConfiguration") == 0)				/*2*/
	{
		soap_trc_SetRecordingJobConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingJobConfiguration") == 0)				/*2*/
	{
		soap_trc_GetRecordingJobConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetRecordingJobMode") == 0)						/*2*/
	{
		soap_trc_SetRecordingJobMode(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingJobState") == 0)						/*2*/
	{
		soap_trc_GetRecordingJobState(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingOptions") == 0)						/*2*/
	{
		soap_trc_GetRecordingOptions(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ExportRecordedData") == 0)						/*2*/
	{
		soap_trc_ExportRecordedData(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "StopExportRecordedData") == 0)					/*2*/
	{
		soap_trc_StopExportRecordedData(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetExportRecordedDataState") == 0)				/*2*/
	{
		soap_trc_GetExportRecordedDataState(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

int soap_process_search_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)						/*2*/
	{
		soap_tse_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingSummary") == 0)					/*2*/
	{
		soap_tse_GetRecordingSummary(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingInformation") == 0)				/*2*/
	{
		soap_tse_GetRecordingInformation(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMediaAttributes") == 0)					/*2*/
	{
		soap_tse_GetMediaAttributes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "FindRecordings") == 0)						/*2*/
	{
		soap_tse_FindRecordings(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetRecordingSearchResults") == 0)				/*2*/
	{
		soap_tse_GetRecordingSearchResults(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "FindEvents") == 0)							/*2*/
	{
		soap_tse_FindEvents(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetEventSearchResults") == 0)					/*2*/
	{
		soap_tse_GetEventSearchResults(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "FindMetadata") == 0)							/*2*/
	{
		soap_tse_FindMetadata(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetMetadataSearchResults") == 0)				/*2*/
	{
		soap_tse_GetMetadataSearchResults(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "EndSearch") == 0)								/*2*/
	{
		soap_tse_EndSearch(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSearchState") == 0)						/*2*/
	{
		soap_tse_GetSearchState(p_user, rx_msg, p_body, p_header);
	}
#ifdef PTZ_SUPPORT
	else if (soap_strcmp(p_name, "FindPTZPosition") == 0)						/*2*/
	{
		soap_tse_FindPTZPosition(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetPTZPositionSearchResults") == 0)			/*2*/
	{
		soap_tse_GetPTZPositionSearchResults(p_user, rx_msg, p_body, p_header);
	}
#endif
	else
	{
		ret = 0;
	}

	return ret;
}

int soap_process_replay_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)					/*2*/
	{
		soap_trp_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetReplayUri") == 0)						/*2*/
	{
		soap_trp_GetReplayUri(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetReplayConfiguration") == 0)			/*2*/
	{
		soap_trp_GetReplayConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetReplayConfiguration") == 0)			/*2*/
	{
		soap_trp_SetReplayConfiguration(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

int soap_process_accesscontrol_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)						/**/
	{
		soap_tac_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessPointList") == 0)					/**/
	{
		soap_tac_GetAccessPointList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateAccessPoint") == 0)						/**/
	{
		soap_tac_CreateAccessPoint(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAccessPoint") == 0)						/**/
	{
		soap_tac_SetAccessPoint(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyAccessPoint") == 0)						/**/
	{
		soap_tac_ModifyAccessPoint(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteAccessPoint") == 0)						/**/
	{
		soap_tac_DeleteAccessPoint(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessPoints") == 0)						/**/
	{
		soap_tac_GetAccessPoints(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessPointInfoList") == 0)				/**/
	{
		soap_tac_GetAccessPointInfoList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessPointInfo") == 0)					/**/
	{
		soap_tac_GetAccessPointInfo(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAreaList") == 0)							/**/
	{
		soap_tac_GetAreaList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateArea") == 0)							/**/
	{
		soap_tac_CreateArea(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetArea") == 0)								/**/
	{
		soap_tac_SetArea(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyArea") == 0)							/**/
	{
		soap_tac_ModifyArea(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteArea") == 0)							/**/
	{
		soap_tac_DeleteArea(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAreas") == 0)								/**/
	{
		soap_tac_GetAreas(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAreaInfoList") == 0)						/**/
	{
		soap_tac_GetAreaInfoList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAreaInfo") == 0)							/**/
	{
		soap_tac_GetAreaInfo(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessPointState") == 0)					/**/
	{
		soap_tac_GetAccessPointState(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "EnableAccessPoint") == 0)						/**/
	{
		soap_tac_EnableAccessPoint(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DisableAccessPoint") == 0)					/**/
	{
		soap_tac_DisableAccessPoint(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

int soap_process_doorcontrol_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)					/*0*/
	{
		soap_tdc_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDoorList") == 0)						/*0*/
	{
		soap_tdc_GetDoorList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDoors") == 0)							/*0*/
	{
		soap_tdc_GetDoors(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateDoor") == 0)						/*0*/
	{
		soap_tdc_CreateDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetDoor") == 0)							/*0*/
	{
		soap_tdc_SetDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyDoor") == 0)						/*0*/
	{
		soap_tdc_ModifyDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteDoor") == 0)						/*0*/
	{
		soap_tdc_DeleteDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDoorInfoList") == 0)					/*0*/
	{
		soap_tdc_GetDoorInfoList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDoorInfo") == 0)						/*0*/
	{
		soap_tdc_GetDoorInfo(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetDoorState") == 0)						/*0*/
	{
		soap_tdc_GetDoorState(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AccessDoor") == 0)						/*0*/
	{
		soap_tdc_AccessDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "LockDoor") == 0)							/*0*/
	{
		soap_tdc_LockDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "UnlockDoor") == 0)						/*0*/
	{
		soap_tdc_UnlockDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DoubleLockDoor") == 0)					/*0*/
	{
		soap_tdc_DoubleLockDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "BlockDoor") == 0)							/*0*/
	{
		soap_tdc_BlockDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "LockDownDoor") == 0)						/*0*/
	{
		soap_tdc_LockDownDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "LockDownReleaseDoor") == 0)				/*0*/
	{
		soap_tdc_LockDownReleaseDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "LockOpenDoor") == 0)						/*0*/
	{
		soap_tdc_LockOpenDoor(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "LockOpenReleaseDoor") == 0)				/*0*/
	{
		soap_tdc_LockOpenReleaseDoor(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // PROFILE_C_SUPPORT

#ifdef CREDENTIAL_SUPPORT

int soap_process_credential_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)							/*0*/
	{
		soap_tcr_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCredentialInfo") == 0)							/*0*/
	{
		soap_tcr_GetCredentialInfo(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCredentialInfoList") == 0)						/*0*/
	{
		soap_tcr_GetCredentialInfoList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCredentials") == 0)							/*0*/
	{
		soap_tcr_GetCredentials(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCredentialList") == 0)							/*0*/
	{
		soap_tcr_GetCredentialList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateCredential") == 0)							/*0*/
	{
		soap_tcr_CreateCredential(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyCredential") == 0)							/*0*/
	{
		soap_tcr_ModifyCredential(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteCredential") == 0)							/*0*/
	{
		soap_tcr_DeleteCredential(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCredentialState") == 0)						/*0*/
	{
		soap_tcr_GetCredentialState(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "EnableCredential") == 0)							/*0*/
	{
		soap_tcr_EnableCredential(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DisableCredential") == 0)							/*0*/
	{
		soap_tcr_DisableCredential(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetCredential") == 0)								/*0*/
	{
		soap_tcr_SetCredential(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ResetAntipassbackViolation") == 0)				/*0*/
	{
		soap_tcr_ResetAntipassbackViolation(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSupportedFormatTypes") == 0)					/*0*/
	{
		soap_tcr_GetSupportedFormatTypes(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCredentialIdentifiers") == 0)					/*0*/
	{
		soap_tcr_GetCredentialIdentifiers(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetCredentialIdentifier") == 0)					/*0*/
	{
		soap_tcr_SetCredentialIdentifier(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteCredentialIdentifier") == 0)				/*0*/
	{
		soap_tcr_DeleteCredentialIdentifier(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetCredentialAccessProfiles") == 0)				/*0*/
	{
		soap_tcr_GetCredentialAccessProfiles(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetCredentialAccessProfiles") == 0)				/*0*/
	{
		soap_tcr_SetCredentialAccessProfiles(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteCredentialAccessProfiles") == 0)			/*0*/
	{
		soap_tcr_DeleteCredentialAccessProfiles(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetWhitelist") == 0)								/*0*/
	{
		soap_tcr_GetWhitelist(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddToWhitelist") == 0)							/*0*/
	{
		soap_tcr_AddToWhitelist(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveFromWhitelist") == 0)						/*0*/
	{
		soap_tcr_RemoveFromWhitelist(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteWhitelist") == 0)							/*0*/
	{
		soap_tcr_DeleteWhitelist(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetBlacklist") == 0)								/*0*/
	{
		soap_tcr_GetBlacklist(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "AddToBlacklist") == 0)							/*0*/
	{
		soap_tcr_AddToBlacklist(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RemoveFromBlacklist") == 0)						/*0*/
	{
		soap_tcr_RemoveFromBlacklist(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteBlacklist") == 0)							/*0*/
	{
		soap_tcr_DeleteBlacklist(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}	

#endif // CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

int soap_process_accessrules_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)				/*0*/
	{
		soap_tar_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessProfileInfo") == 0)			/*0*/
	{
		soap_tar_GetAccessProfileInfo(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessProfileInfoList") == 0)		/*0*/
	{
		soap_tar_GetAccessProfileInfoList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessProfiles") == 0)				/*0*/
	{
		soap_tar_GetAccessProfiles(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetAccessProfileList") == 0)			/*0*/
	{
		soap_tar_GetAccessProfileList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateAccessProfile") == 0)			/*0*/
	{
		soap_tar_CreateAccessProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifyAccessProfile") == 0)			/*0*/
	{
		soap_tar_ModifyAccessProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteAccessProfile") == 0)			/*0*/
	{
		soap_tar_DeleteAccessProfile(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetAccessProfile") == 0)				/*0*/
	{
		soap_tar_SetAccessProfile(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}	

#endif // ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

int soap_process_schedule_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)						/*0*/
	{			
		soap_tsc_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetScheduleInfo") == 0)						/*0*/
	{
		soap_tsc_GetScheduleInfo(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetScheduleInfoList") == 0)					/*0*/
	{
		soap_tsc_GetScheduleInfoList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSchedules") == 0)							/*0*/
	{
		soap_tsc_GetSchedules(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetScheduleList") == 0)						/*0*/
	{
		soap_tsc_GetScheduleList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateSchedule") == 0)						/*0*/
	{
		soap_tsc_CreateSchedule(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifySchedule") == 0)						/*0*/
	{
		soap_tsc_ModifySchedule(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteSchedule") == 0)						/*0*/
	{
		soap_tsc_DeleteSchedule(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSpecialDayGroupInfo") == 0)				/*0*/
	{
		soap_tsc_GetSpecialDayGroupInfo(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSpecialDayGroupInfoList") == 0)			/*0*/
	{
		soap_tsc_GetSpecialDayGroupInfoList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSpecialDayGroups") == 0)					/*0*/
	{
		soap_tsc_GetSpecialDayGroups(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetSpecialDayGroupList") == 0)				/*0*/
	{
		soap_tsc_GetSpecialDayGroupList(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateSpecialDayGroup") == 0)					/*0*/
	{
		soap_tsc_CreateSpecialDayGroup(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ModifySpecialDayGroup") == 0)					/*0*/
	{
		soap_tsc_ModifySpecialDayGroup(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteSpecialDayGroup") == 0)					/*0*/
	{
		soap_tsc_DeleteSpecialDayGroup(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetScheduleState") == 0)						/*0*/
	{
		soap_tsc_GetScheduleState(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSchedule") == 0)							/*0*/
	{
		soap_tsc_SetSchedule(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetSpecialDayGroup") == 0)					/*0*/
	{
		soap_tsc_SetSpecialDayGroup(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}	

#endif // SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

int soap_process_receiver_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)					/*2*/
	{
		soap_trv_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetReceivers") == 0)						/*2*/
	{
		soap_trv_GetReceivers(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetReceiver") == 0)						/*2*/
	{
		soap_trv_GetReceiver(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "CreateReceiver") == 0)					/*2*/
	{
		soap_trv_CreateReceiver(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "DeleteReceiver") == 0)					/*2*/
	{
		soap_trv_DeleteReceiver(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ConfigureReceiver") == 0)					/*2*/
	{
		soap_trv_ConfigureReceiver(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "SetReceiverMode") == 0)					/*2*/
	{
		soap_trv_SetReceiverMode(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetReceiverState") == 0)					/*2*/
	{
		soap_trv_GetReceiverState(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

#endif // RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT

int soap_process_provisioning_request(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	int ret = 1;
	const char * p_name;

	p_name = p_body->f_child->name;

	if (soap_strcmp(p_name, "GetServiceCapabilities") == 0)					/*0*/
	{
		soap_tpv_GetServiceCapabilities(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "Stop") == 0)								/*0*/
	{
		soap_tpv_Stop(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "PanMove") == 0)							/*0*/
	{
		soap_tpv_PanMove(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "TiltMove") == 0)							/*0*/
	{
		soap_tpv_TiltMove(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "ZoomMove") == 0)							/*0*/
	{
		soap_tpv_ZoomMove(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "RollMove") == 0)							/*0*/
	{
		soap_tpv_RollMove(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "FocusMove") == 0)							/*0*/
	{
		soap_tpv_FocusMove(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_name, "GetUsage") == 0)							/*0*/
	{
		soap_tpv_GetUsage(p_user, rx_msg, p_body, p_header);
	}
	else
	{
		ret = 0;
	}

	return ret;
}

#endif

/*********************************************************
 *
 * process soap request
 *
 * p_user [in] --- http client
 * rx_msg [in] --- http message
 *
**********************************************************/ 
void soap_process_request(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	// log_print(HT_LOG_INFO, "soap_process_request(HTTPCLN * p_user, HTTPMSG * rx_msg)\n");
	int ret = 0;
	char * p_xml = NULL;
	char * p_post = NULL;
	XMLN * p_node;
	XMLN * p_header;
	XMLN * p_body;

	p_xml = http_get_ctt(rx_msg);
	if (NULL == p_xml)
	{
		log_print(HT_LOG_ERR, "%s, http_get_ctt ret null!!!\r\n", __FUNCTION__);
		return;
	}

	//log_print(HT_LOG_INFO, "soap_process::rx xml:\r\n%s\r\n", p_xml);

	p_node = xxx_hxml_parse(p_xml, (int)strlen(p_xml));
	if (NULL == p_node || NULL == p_node->name)
	{
		log_print(HT_LOG_ERR, "%s, xxx_hxml_parse ret null!!!\r\n", __FUNCTION__);
		return;
	}
	
	if (soap_strcmp(p_node->name, "Envelope") != 0)
	{
		log_print(HT_LOG_ERR, "%s, node name[%s] != [Envelope]!!!\r\n", __FUNCTION__, p_node->name);
		xml_node_del(p_node);
		return;
	}

	p_body = xml_node_soap_get(p_node, "Body");
	if (NULL == p_body)
	{
		log_print(HT_LOG_ERR, "%s, xml_node_soap_get[Body] ret null!!!\r\n", __FUNCTION__);
		xml_node_del(p_node);
		return;
	}

	if (NULL == p_body->f_child)
	{
		log_print(HT_LOG_ERR, "%s, body first child node is null!!!\r\n", __FUNCTION__);
		xml_node_del(p_node);
		return;
	}
	else if (NULL == p_body->f_child->name)
	{
		log_print(HT_LOG_ERR, "%s, body first child node name is null!!!\r\n", __FUNCTION__);
		xml_node_del(p_node);
		return;
	}
	else
	{
		log_print(HT_LOG_INFO, "%s, body first child node name[%s]\r\n", __FUNCTION__, p_body->f_child->name);
	}

	MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
	if (pStreamCfg && pStreamCfg->webConfig.onvif_auth && !strstr(rx_msg->first_line.value_string, "/html/onvif/Events/pullSubmanager") )
	{
		if (!soap_auth_handler(p_user, rx_msg, p_node, p_body->f_child->name))
		{
			xml_node_del(p_node);
			return;
		}
	}

	p_post = rx_msg->first_line.value_string;
	p_header = xml_node_soap_get(p_node, "Header");
	
	if (p_post != NULL)
	{
		log_print(HT_LOG_DBG, "p_post: --%s \n", p_post);
	}
	else
	{
		log_print(HT_LOG_ERR, "(p_post == NULL)\n");
	}

	if (strstr(p_post, "event") || strstr(p_post, "Event"))/*11*/
	{
		ret = soap_process_event_request(p_user, rx_msg, p_body, p_header);
	}
	else if (strstr(p_post, "tpl") || strstr(p_post, "hbgk_ext") || strstr(p_post, "Plus"))/*11*/
	{
		if (NULL != p_body->f_child->name)
		{
			if (strstr(p_body->f_child->name, "ewsd"))
				ret = soap_process_hbgk_ext_request(p_user, rx_msg, p_body, p_header);
			else
				ret = soap_process_tpl_request(p_user, rx_msg, p_body, p_header);
		}
		else
			ret = soap_process_tpl_request(p_user, rx_msg, p_body, p_header);
	}
#ifdef DEVICEIO_SUPPORT
	else if (strstr(p_post, "deviceio") || strstr(p_post, "Deviceio") || strstr(p_post, "DeviceIO"))/*11*/
	{
		ret = soap_process_deviceio_request(p_user, rx_msg, p_body, p_header);
	}
#endif
	else if (strstr(p_post, "device") || strstr(p_post, "Device"))/*11*/
	{
		ret = soap_process_device_request(p_user, rx_msg, p_body, p_header);
	}
#ifdef IMAGE_SUPPORT	
	else if (strstr(p_post, "image") || strstr(p_post, "Image"))/*11*/
	{
		ret = soap_process_image_request(p_user, rx_msg, p_body, p_header);
	}
#endif	
#ifdef MEDIA2_SUPPORT
	else if (strstr(p_post, "media2") || strstr(p_post, "Media2"))/*11*/
	{
		ret = soap_process_media2_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef MEDIA_SUPPORT
	else if (strstr(p_post, "media") || strstr(p_post, "Media"))/*11*/
	{
		ret = soap_process_media_request(p_user, rx_msg, p_body, p_header);
	}
#endif 
#ifdef PTZ_SUPPORT
	else if (strstr(p_post, "ptz") || strstr(p_post, "Ptz") || strstr(p_post, "PTZ"))/*00*/
	{
		ret = soap_process_ptz_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef THERMAL_SUPPORT
	else if (strstr(p_post, "thermal") || strstr(p_post, "Thermal"))/*00*/
	{
		ret = soap_process_thermal_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef VIDEO_ANALYTICS
	else if (strstr(p_post, "analytics") || strstr(p_post, "Analytics"))/*11*/
	{
		ret = soap_process_analytics_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef PROFILE_G_SUPPORT
	else if (strstr(p_post, "recording") || strstr(p_post, "Recording"))/*22*/
	{
		ret = soap_process_recording_request(p_user, rx_msg, p_body, p_header);
	}
	else if (strstr(p_post, "search") || strstr(p_post, "Search"))/*22*/
	{
		ret = soap_process_search_request(p_user, rx_msg, p_body, p_header);
	}
	else if (strstr(p_post, "replay") || strstr(p_post, "Replay"))/*22*/
	{
		ret = soap_process_replay_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef PROFILE_C_SUPPORT
	else if (strstr(p_post, "accesscontrol") || strstr(p_post, "Accesscontrol"))/*00*/
	{
		ret = soap_process_accesscontrol_request(p_user, rx_msg, p_body, p_header);
	}
	else if (strstr(p_post, "doorcontrol") || strstr(p_post, "Doorcontrol"))/*00*/
	{
		ret = soap_process_doorcontrol_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef CREDENTIAL_SUPPORT
	else if (strstr(p_post, "credential") || strstr(p_post, "Credential"))/*00*/
	{
		ret = soap_process_credential_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef ACCESS_RULES
	else if (strstr(p_post, "accessrules") || strstr(p_post, "Accessrules"))/*00*/
	{
		ret = soap_process_accessrules_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef SCHEDULE_SUPPORT
	else if (strstr(p_post, "schedule") || strstr(p_post, "Schedule"))/*00*/
	{
		ret = soap_process_schedule_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef RECEIVER_SUPPORT
	else if (strstr(p_post, "receiver") || strstr(p_post, "Receiver"))/*22*/
	{
		ret = soap_process_receiver_request(p_user, rx_msg, p_body, p_header);
	}
#endif
#ifdef PROVISIONING_SUPPORT
	else if (strstr(p_post, "provisioning") || strstr(p_post, "Provisioning"))/*00*/
	{
		ret = soap_process_provisioning_request(p_user, rx_msg, p_body, p_header);
	}
#endif

	if (!ret)
	{
		soap_err_def_rly(p_user, rx_msg);
	}
	
	xml_node_del(p_node);
}



