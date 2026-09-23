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
#include "hxml.h"
#include "xml_node.h"
#include "onvif.h"
#include "http.h"
#include "onvif_device.h"
#include "onvif_pkt.h"
#include "soap_parser.h"
#include "onvif_event.h"
#include "sha1.h"
#include "onvif_ptz.h"
#include "onvif_err.h"
#include "onvif_image.h"


/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;


/***************************************************************************************/
int soap_http_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, const char * p_xml, int len)
{
	char * p_bufs = (char *)malloc(len + 1024);
	if (NULL == p_bufs)
	{
		return -1;
	}
	
	int tlen = sprintf(p_bufs,	"HTTP/1.1 200 OK\r\n"
								"Server: hsoap/2.8\r\n"
								"Content-Type: %s\r\n"
								"Content-Length: %d\r\n"
								"Connection: close\r\n\r\n",
								get_http_headline(rx_msg, "Content-Type"), len);

	memcpy(p_bufs+tlen, p_xml, len);
	tlen += len;

	send(p_user->cfd, p_bufs, tlen, 0);
	free(p_bufs);
	
	return tlen;
}

int soap_http_err_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, int err_code, const char * err_str, const char * p_xml, int len)
{
	char * p_bufs = (char *)malloc(1024 * 16);
	if (NULL == p_bufs)
	{
		return -1;
	}
	
	int tlen = sprintf(p_bufs,	"HTTP/1.1 %d %s\r\n"
								"Server: hsoap/2.8\r\n"
								"Content-Type: %s\r\n"
								"Content-Length: %d\r\n"
								"Connection: close\r\n\r\n",
								err_code, err_str,
								get_http_headline(rx_msg, "Content-Type"), len);

	memcpy(p_bufs+tlen, p_xml, len);
	tlen += len;

	send(p_user->cfd, p_bufs, tlen, 0);
	free(p_bufs);
	
	return tlen;
}

int soap_err_rly
(
HTTPCLN * p_user, 
HTTPMSG * rx_msg, 
const char * code = ERR_RECEIVER, 
const char * subcode = ERR_ACTIONNOTSUPPORTED, 
const char * subcode_ex = NULL,
const char * reason = "Action Not Implemented",
int http_err_code = 400, 
const char * http_err_str = "Bad Request"
)
{
	__INFO("soap_err_rly\r\n");
    
	int ret = -1, mlen = 1024*16, xlen;
	
	char * p_xml = (char *)malloc(mlen);
	if (NULL == p_xml)
	{
		goto soap_rly_err;
	}
	
	xlen = build_err_rly_xml(p_xml, mlen, code, subcode, subcode_ex, reason);
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

int soap_security_rly(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_security_rly.\r\n");

    return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_NOTAUTHORIZED, NULL, "Sender not Authorized", 400, "Not Authorized");
}

int soap_build_err_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, ONVIF_RET err)
{
	int ret = 0;
	
	switch (err)
	{
	case ONVIF_ERR_INVALID_IPV4_ADDR:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidIPv4Address", "Invalid IPv4 Address");
		break;

	case ONVIF_ERR_INVALID_IPV6_ADDR:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidIPv6Address", "Invalid IPv6 Address");	
		break;

	case ONVIF_ERR_INVALID_DNS_NAME:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter::InvalidDnsName", "Invalid DNS Name");	
		break;	

	case ONVIF_ERR_SERVICE_NOT_SUPPORT:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ServiceNotSupported", "Service Not Supported");	
		break;

	case ONVIF_ERR_PORT_ALREADY_INUSE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:PortAlreadyInUse", "Port Already In Use");	
		break;	

	case ONVIF_ERR_INVALID_GATEWAY_ADDR:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidGatewayAddress", "Invalid Gateway Address");	
		break;	

	case ONVIF_ERR_INVALID_HOSTNAME:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidHostname", "Invalid Hostname");	
		break;	

	case ONVIF_ERR_MISSINGATTR:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");	
		break;	

	case ONVIF_ERR_INVALID_DATETIME:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidDateTime", "Invalid Datetime");	
		break;		

	case ONVIF_ERR_INVALID_TIMEZONE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidTimeZone", "Invalid Timezone");	
		break;	

	case ONVIF_ERR_PROFILE_EXISTS:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ProfileExists", "Profile Exist");	
		break;	

	case ONVIF_ERR_MAX_NVT_PROFILES:
		ret = soap_err_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:MaxNVTProfiles", "Max Profiles");
		break;

	case ONVIF_ERR_NO_PROFILE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoProfile", "Profile Not Exist");
		break;

	case ONVIF_ERR_DEL_FIX_PROFILE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:DeletionOfFixedProfile", "Deleting Fixed Profile");
		break;

	case ONVIF_ERR_NO_CONFIG:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoConfig", "Config Not Exist");
		break;

	case ONVIF_ERR_NO_PTZ_PROFILE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoPTZProfile", "PTZ Profile Not Exist");
		break;	

	case ONVIF_ERR_NO_HOME_POSITION:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:NoHomePosition", "No Home Position");
		break;	

	case ONVIF_ERR_NO_TOKEN:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:NoToken", "The requested token does not exist.");
		break;	

	case ONVIF_ERR_PRESET_EXIST:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:PresetExist", "The requested name already exist for another preset.");
		break;

	case ONVIF_ERR_TOO_MANY_PRESETS:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:TooManyPresets", "Maximum number of Presets reached.");
		break;	

	case ONVIF_ERR_MOVING_PTZ:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_ACTION, "ter:MovingPTZ", "Preset cannot be set while PTZ unit is moving.");
		break;

	case ONVIF_ERR_NO_ENTITY:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoEntity", "No such PTZ Node on the device");
		break;	

	case ONVIF_ERR_INVALID_NETWORK_INTERFACE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidNetworkInterface", "The supplied network interface token does not exist.");
		break;	

	case ONVIF_ERR_INVALID_MTU_VALUE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidMtuValue", "The MTU value is invalid");
		break;	

	case ONVIF_ERR_CONFIG_MODIFY:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ConfigModify", "The configuration parameters are not possible to set.");
		break;

	case ONVIF_ERR_CONFIGURATION_CONFLICT:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:ConfigurationConflict", "The new settings conflicts with other uses of the configuration.");
		break;

	case ONVIF_ERR_INVALID_POSIION:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidPosition", "Invalid Postion");
		break;	

	case ONVIF_ERR_TOO_MANY_SCOPES:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:TooManyScopes", "The requested scope list exceeds the supported number of scopes.");
		break;

	case ONVIF_ERR_FIXED_SCOPE:
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:FixedScope", "Trying to Remove fixed scope parameter, command rejected.");
		break;

	case ONVIF_ERR_NO_SCOPE:
		soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoScope", "Trying to Remove scope which does not exist.");
		break;

	case ONVIF_ERR_SCOPE_OVERWRITE:
		soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_OPERATIONPROHIBITED, "ter:ScopeOverwrite", "Scope Overwrite");
		break;

	case ONVIF_ERR_NO_SOURCE:
		soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter::NoSource", "The requested VideoSource does not exist.");
		break;

	case ONVIF_ERR_CANNOT_OVERWRITE_HOME:
		soap_err_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTION, "ter:CannotOverwriteHome", "The home position is fixed and cannot be overwritten");
		break;

	case ONVIF_ERR_SETTINGS_INVALID:
		soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:SettingsInvalid", "The requested settings are incorrect");
		break;

	case ONVIF_ERR_NO_IMAGEING_FOR_SOURCE:
		soap_err_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NoImagingForSource", "The requested VideoSource does not support imaging settings");
		break;
		
	default:
		ret = soap_err_rly(p_user, rx_msg);
		break;
	}

	return ret;
}


typedef int (*soap_build_xml)(char * p_buf, int mlen, const char * argv);

int soap_build_send_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, soap_build_xml build_xml, const char * argv = NULL)
{
	int ret = -1, mlen = 1024*16, xlen;
	
	char * p_xml = (char *)malloc(mlen);
	if (NULL == p_xml)
	{
		return -1;
	}
	
	xlen = build_xml(p_xml, mlen, argv);
	if (ONVIF_ERR_NO_PROFILE == xlen)
	{
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoProfile", "Profile Not Exist");
	}
	else if (ONVIF_ERR_NO_CONFIG == xlen)
	{
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoConfig", "Config Not Exist");
	}
	else if (ONVIF_ERR_NO_PTZ_PROFILE == xlen)
	{
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoPTZProfile", "PTZ Profile Not Exist");
	}
	else if (ONVIF_ERR_NO_ENTITY == xlen)
	{
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoEntity", "No such PTZ Node on the device");
	}
	else if (ONVIF_ERR_NO_SOURCE == xlen)
	{
		ret = soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter::NoSource", "The requested VideoSource does not exist.");
	}
	else if (ONVIF_ERR_NO_IMAGEING_FOR_SOURCE == xlen)
	{
		ret = soap_err_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NoImagingForSource", "The requested VideoSource does not support imaging settings");
	}	
	else if (xlen < 0 || xlen >= mlen)
	{
		ret = soap_err_rly(p_user, rx_msg);
	}
	else
	{
		ret = soap_http_rly(p_user, rx_msg, p_xml, xlen);
	}
	
	free(p_xml);
	
	return ret;
}

int soap_GetDeviceInformation(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetDeviceInformation\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetDeviceInformation_rly_xml);
}

int soap_GetCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetCapabilities\r\n");

	XMLN * p_GetCapabilities = xml_node_soap_get(p_body, "GetCapabilities");
	assert(p_GetCapabilities);

	int ret = 0;

	XMLN * p_Category = xml_node_soap_get(p_GetCapabilities, "Category");
	if (p_Category && p_Category->data)
	{
		E_CAP_CATEGORY category = onvif_get_cap_category(p_Category->data);
		
		if (CAP_CATEGORY_INVALID == category || CAP_CATEGORY_ANALYTICS == category)
		{
			ret = soap_err_rly(p_user, rx_msg, ERR_RECEIVER, ERR_ACTIONNOTSUPPORTED, "ter:NoSuchService", "No Such Service");
		}
		else
		{
			ret = soap_build_send_rly(p_user, rx_msg, build_GetCapabilities_rly_xml, (char *)category);
		}
	}
	else
	{
		ret = soap_build_send_rly(p_user, rx_msg, build_GetCapabilities_rly_xml, (char *)CAP_CATEGORY_ALL);
	}
		
	return ret;
}

int soap_GetProfiles_2(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetProfiles.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetProfiles_2_rly_xml);
}

int soap_GetProfiles(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetProfiles.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetProfiles_rly_xml);
}


int soap_GetProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetProfile\r\n");

	XMLN * p_GetProfile = xml_node_soap_get(p_body, "GetProfile");
	assert(p_GetProfile);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetProfile, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetProfile_rly_xml, p_ProfileToken->data);
	}
	else
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_CreateProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_CreateProfile\r\n");

	XMLN * p_CreateProfile = xml_node_soap_get(p_body, "CreateProfile");
	assert(p_CreateProfile);

	CreateProfile_REQ CreateProfile_req;
	memset(&CreateProfile_req, 0, sizeof(CreateProfile_REQ));

	ONVIF_RET ret = parse_CreateProfile(p_CreateProfile, &CreateProfile_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_CreateProfile(&CreateProfile_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_CreateProfile_rly_xml, CreateProfile_req.token);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_DeleteProfile(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_DeleteProfile\r\n");

	XMLN * p_DeleteProfile = xml_node_soap_get(p_body, "DeleteProfile");
	assert(p_DeleteProfile);

	ONVIF_RET ret = ONVIF_ERR_MISSINGATTR;

	XMLN * p_ProfileToken = xml_node_soap_get(p_DeleteProfile, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_DeleteProfile(p_ProfileToken->data);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_DeleteProfile_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_AddVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_AddVideoSourceConfiguration\r\n");

	XMLN * p_AddVideoSourceConfiguration = xml_node_soap_get(p_body, "AddVideoSourceConfiguration");
	assert(p_AddVideoSourceConfiguration);

	AddVideoSourceConfiguration_REQ AddVideoSourceConfiguration_req;
	memset(&AddVideoSourceConfiguration_req, 0, sizeof(AddVideoSourceConfiguration_REQ));

	ONVIF_RET ret = parse_AddVideoSourceConfiguration(p_AddVideoSourceConfiguration, &AddVideoSourceConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_AddVideoSourceConfiguration(&AddVideoSourceConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_AddVideoSourceConfiguration_rly_xml);
		} 	
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_RemoveVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_RemoveVideoSourceConfiguration\r\n");
	
	XMLN * p_RemoveVideoSourceConfiguration = xml_node_soap_get(p_body, "RemoveVideoSourceConfiguration");
	assert(p_RemoveVideoSourceConfiguration);

	ONVIF_RET ret = ONVIF_ERR_MISSINGATTR;
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_RemoveVideoSourceConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_RemoveVideoSourceConfiguration(p_ProfileToken->data);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RemoveVideoSourceConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_AddAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_AddAudioSourceConfiguration\r\n");

	XMLN * p_AddAudioSourceConfiguration = xml_node_soap_get(p_body, "AddAudioSourceConfiguration");
	assert(p_AddAudioSourceConfiguration);

	AddAudioSourceConfiguration_REQ AddAudioSourceConfiguration_req;
	memset(&AddAudioSourceConfiguration_req, 0, sizeof(AddAudioSourceConfiguration_REQ));

	ONVIF_RET ret = parse_AddAudioSourceConfiguration(p_AddAudioSourceConfiguration, &AddAudioSourceConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_AddAudioSourceConfiguration(&AddAudioSourceConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_AddAudioSourceConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_RemoveAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_RemoveAudioSourceConfiguration\r\n");
	
	XMLN * p_RemoveAudioSourceConfiguration = xml_node_soap_get(p_body, "RemoveAudioSourceConfiguration");
	assert(p_RemoveAudioSourceConfiguration);

	ONVIF_RET ret = ONVIF_ERR_MISSINGATTR;
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_RemoveAudioSourceConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ONVIF_RET ret = onvif_RemoveAudioSourceConfiguration(p_ProfileToken->data);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RemoveAudioSourceConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}


int soap_AddVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_AddVideoEncoderConfiguration\r\n");

	XMLN * p_AddVideoEncoderConfiguration = xml_node_soap_get(p_body, "AddVideoEncoderConfiguration");
	assert(p_AddVideoEncoderConfiguration);

	AddVideoEncoderConfiguration_REQ AddVideoEncoderConfiguration_req;
	memset(&AddVideoEncoderConfiguration_req, 0, sizeof(AddVideoEncoderConfiguration_REQ));
	
	ONVIF_RET ret = parse_AddVideoEncoderConfiguration(p_AddVideoEncoderConfiguration, &AddVideoEncoderConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_AddVideoEncoderConfiguration(&AddVideoEncoderConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_AddVideoEncoderConfiguration_rly_xml);
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_RemoveVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{	
	__INFO("soap_RemoveVideoEncoderConfiguration\r\n");
	
	XMLN * p_RemoveVideoEncoderConfiguration = xml_node_soap_get(p_body, "RemoveVideoEncoderConfiguration");
	assert(p_RemoveVideoEncoderConfiguration);

	ONVIF_RET ret = ONVIF_ERR_MISSINGATTR;
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_RemoveVideoEncoderConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ONVIF_RET ret = onvif_RemoveVideoEncoderConfiguration(p_ProfileToken->data);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RemoveVideoEncoderConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_AddAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_AddAudioEncoderConfiguration\r\n");

	XMLN * p_AddAudioEncoderConfiguration = xml_node_soap_get(p_body, "AddAudioEncoderConfiguration");
	assert(p_AddAudioEncoderConfiguration);

	AddAudioEncoderConfiguration_REQ AddAudioEncoderConfiguration_req;
	memset(&AddAudioEncoderConfiguration_req, 0, sizeof(AddAudioEncoderConfiguration_REQ));
	
	ONVIF_RET ret = parse_AddAudioEncoderConfiguration(p_AddAudioEncoderConfiguration, &AddAudioEncoderConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_AddAudioEncoderConfiguration(&AddAudioEncoderConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_AddAudioEncoderConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_RemoveAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{	
	__INFO("soap_RemoveAudioEncoderConfiguration\r\n");
	
	XMLN * p_RemoveAudioEncoderConfiguration = xml_node_soap_get(p_body, "RemoveAudioEncoderConfiguration");
	assert(p_RemoveAudioEncoderConfiguration);

	ONVIF_RET ret = ONVIF_ERR_MISSINGATTR;
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_RemoveAudioEncoderConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ONVIF_RET ret = onvif_RemoveAudioEncoderConfiguration(p_ProfileToken->data);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RemoveAudioEncoderConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}


int soap_GetSystemDateAndTime(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetSystemDateAndTime\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetSystemDateAndTime_rly_xml);
}

int soap_GetStreamUri_2(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetStreamUri_2\r\n");

	XMLN * p_GetStreamUri = xml_node_soap_get(p_body, "GetStreamUri");
	assert(p_GetStreamUri);

	GetStreamUri_REQ GetStreamUri_req;
	memset(&GetStreamUri_req, 0, sizeof(GetStreamUri_REQ));

	ONVIF_RET ret = parse_GetStreamUri(p_GetStreamUri, &GetStreamUri_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetStreamUri_rly_xml_2, GetStreamUri_req.profile_token);
	}
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_GetStreamUri(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetStreamUri\r\n");

	XMLN * p_GetStreamUri = xml_node_soap_get(p_body, "GetStreamUri");
	assert(p_GetStreamUri);

	GetStreamUri_REQ GetStreamUri_req;
	memset(&GetStreamUri_req, 0, sizeof(GetStreamUri_REQ));

	ONVIF_RET ret = parse_GetStreamUri(p_GetStreamUri, &GetStreamUri_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetStreamUri_rly_xml, GetStreamUri_req.profile_token);
	}
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_GetSnapshotUri_2(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetSnapshotUri_2\r\n");

	XMLN * p_GetSnapshotUri = xml_node_soap_get(p_body, "GetSnapshotUri");
	assert(p_GetSnapshotUri);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetSnapshotUri, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
	    return soap_build_send_rly(p_user, rx_msg, build_GetSnapshotUri_rly_xml_2, p_ProfileToken->data);
	}
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_GetSnapshotUri(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetSnapshotUri\r\n");

	XMLN * p_GetSnapshotUri = xml_node_soap_get(p_body, "GetSnapshotUri");
	assert(p_GetSnapshotUri);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetSnapshotUri, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
	    return soap_build_send_rly(p_user, rx_msg, build_GetSnapshotUri_rly_xml, p_ProfileToken->data);
	}
	
	return soap_err_rly(p_user, rx_msg);
}


int soap_GetNetworkInterfaces(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetNetworkInterfaces\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetNetworkInterfaces_rly_xml);
}

int soap_SetNetworkInterfaces(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetNetworkInterfaces\r\n");

	XMLN * p_SetNetworkInterfaces = xml_node_soap_get(p_body, "SetNetworkInterfaces");
	assert(p_SetNetworkInterfaces);

	SetNetworkInterfaces_REQ SetNetworkInterfaces_req;
	memset(&SetNetworkInterfaces_req, 0, sizeof(SetNetworkInterfaces_REQ));
	
	ONVIF_RET ret = parse_SetNetworkInterfaces(p_SetNetworkInterfaces, &SetNetworkInterfaces_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetNetworkInterfaces(&SetNetworkInterfaces_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetNetworkInterfaces_rly_xml);
		}
	}
	return soap_build_err_rly(p_user, rx_msg, ret);
}


int soap_GetVideoSources(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetVideoSources\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetVideoSources_rly_xml);	
}

int soap_GetAudioSources(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetAudioSources\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetAudioSources_rly_xml);
}

int soap_GetVideoEncoderConfigurations_2(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetVideoEncoderConfigurations_2\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetVideoEncoderConfigurations_rly_xml_2);
}

int soap_GetVideoEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetVideoEncoderConfigurations\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetVideoEncoderConfigurations_rly_xml);
}

int soap_GetCompatibleVideoEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetCompatibleVideoEncoderConfigurations\r\n");

	XMLN * p_GetCompatibleVideoEncoderConfigurations = xml_node_soap_get(p_body, "GetCompatibleVideoEncoderConfigurations");
	assert(p_GetCompatibleVideoEncoderConfigurations);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetCompatibleVideoEncoderConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetCompatibleVideoEncoderConfigurations_rly_xml, p_ProfileToken->data);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MISSINGATTR);
}

int soap_GetAudioEncoderConfigurations_2(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetAudioEncoderConfigurations_2\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetAudioEncoderConfigurations_rly_xml_2);
}

int soap_GetAudioEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetAudioEncoderConfigurations\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetAudioEncoderConfigurations_rly_xml);
}

int soap_GetCompatibleAudioEncoderConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetCompatibleAudioEncoderConfigurations\r\n");

	XMLN * p_GetCompatibleAudioEncoderConfigurations = xml_node_soap_get(p_body, "GetCompatibleAudioEncoderConfigurations");
	assert(p_GetCompatibleAudioEncoderConfigurations);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetCompatibleAudioEncoderConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetCompatibleAudioEncoderConfigurations_rly_xml, p_ProfileToken->data);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MISSINGATTR);
}

int soap_GetVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetVideoEncoderConfiguration\r\n");

	XMLN * p_GetVideoEncoderConfiguration = xml_node_soap_get(p_body, "GetVideoEncoderConfiguration");
	assert(p_GetVideoEncoderConfiguration);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_GetVideoEncoderConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetVideoEncoderConfiguration_rly_xml, p_ConfigurationToken->data);
	}
	else 
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_GetAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetAudioEncoderConfiguration\r\n");

	XMLN * p_GetAudioEncoderConfiguration = xml_node_soap_get(p_body, "GetAudioEncoderConfiguration");
	assert(p_GetAudioEncoderConfiguration);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_GetAudioEncoderConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetAudioEncoderConfiguration_rly_xml, p_ConfigurationToken->data);
	}
	else 
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_SetAudioEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetAudioEncoderConfiguration\r\n");

	XMLN * p_SetAudioEncoderConfiguration = xml_node_soap_get(p_body, "SetAudioEncoderConfiguration");
	assert(p_SetAudioEncoderConfiguration);

	SetAudioEncoderConfiguration_REQ SetAudioEncoderConfiguration_req;
	memset(&SetAudioEncoderConfiguration_req, 0, sizeof(SetAudioEncoderConfiguration_REQ));

	ONVIF_RET ret = parse_SetAudioEncoderConfiguration(p_SetAudioEncoderConfiguration, &SetAudioEncoderConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetAudioEncoderConfiguration(&SetAudioEncoderConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetAudioEncoderConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetVideoSourceConfigurations_2(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetVideoSourceConfigurations_2.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetVideoSourceConfigurations_rly_xml_2);
}

int soap_GetVideoSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetVideoSourceConfigurations.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetVideoSourceConfigurations_rly_xml);
}

int soap_GetVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetVideoSourceConfiguration\r\n");

	XMLN * p_GetVideoSourceConfiguration = xml_node_soap_get(p_body, "GetVideoSourceConfiguration");
	assert(p_GetVideoSourceConfiguration);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_GetVideoSourceConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetVideoSourceConfiguration_rly_xml, p_ConfigurationToken->data);
	}
	else 
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_SetVideoSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetVideoSourceConfiguration\r\n");

	XMLN * p_SetVideoSourceConfiguration = xml_node_soap_get(p_body, "SetVideoSourceConfiguration");
	assert(p_SetVideoSourceConfiguration);

	SetVideoSourceConfiguration_REQ SetVideoSourceConfiguration_req;
	memset(&SetVideoSourceConfiguration_req, 0, sizeof(SetVideoSourceConfiguration_REQ));

	ONVIF_RET ret = parse_SetVideoSourceConfiguration(p_SetVideoSourceConfiguration, &SetVideoSourceConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetVideoSourceConfiguration(&SetVideoSourceConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetVideoSourceConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetVideoSourceConfigurationOptions_2(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetVideoSourceConfigurationOptions_2\r\n");

	XMLN * p_GetVideoSourceConfigurationOptions = xml_node_soap_get(p_body, "GetVideoSourceConfigurationOptions");
	assert(p_GetVideoSourceConfigurationOptions);

	GetVideoSourceConfigurationOptions_REQ GetVideoSourceConfigurationOptions_req;
	memset(&GetVideoSourceConfigurationOptions_req, 0, sizeof(GetVideoSourceConfigurationOptions_REQ));
	
	ONVIF_RET ret = parse_GetVideoSourceConfigurationOptions(p_GetVideoSourceConfigurationOptions, &GetVideoSourceConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetVideoSourceConfigurationOptions_rly_xml_2, (char *)&GetVideoSourceConfigurationOptions_req);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetVideoSourceConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetVideoSourceConfigurationOptions\r\n");

	XMLN * p_GetVideoSourceConfigurationOptions = xml_node_soap_get(p_body, "GetVideoSourceConfigurationOptions");
	assert(p_GetVideoSourceConfigurationOptions);

	GetVideoSourceConfigurationOptions_REQ GetVideoSourceConfigurationOptions_req;
	memset(&GetVideoSourceConfigurationOptions_req, 0, sizeof(GetVideoSourceConfigurationOptions_REQ));
	
	ONVIF_RET ret = parse_GetVideoSourceConfigurationOptions(p_GetVideoSourceConfigurationOptions, &GetVideoSourceConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetVideoSourceConfigurationOptions_rly_xml, (char *)&GetVideoSourceConfigurationOptions_req);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetAudioSourceConfigurations_2(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetAudioSourceConfigurations_2\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetAudioSourceConfigurations_rly_xml_2);
}

int soap_GetAudioSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetAudioSourceConfigurations\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetAudioSourceConfigurations_rly_xml);
}

int soap_GetCompatibleAudioSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetCompatibleAudioSourceConfigurations\r\n");

	XMLN * p_GetCompatibleAudioSourceConfigurations = xml_node_soap_get(p_body, "GetCompatibleAudioSourceConfigurations");
	assert(p_GetCompatibleAudioSourceConfigurations);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetCompatibleAudioSourceConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
	    return soap_build_send_rly(p_user, rx_msg, build_GetCompatibleAudioSourceConfigurations_rly_xml, p_ProfileToken->data);
	}
	else 
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_GetAudioSourceConfigurationOptions_2(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetAudioSourceConfigurationOptions_2\r\n");

	XMLN * p_GetAudioSourceConfigurationOptions = xml_node_soap_get(p_body, "GetAudioSourceConfigurationOptions");
	assert(p_GetAudioSourceConfigurationOptions);

	GetAudioSourceConfigurationOptions_REQ GetAudioSourceConfigurationOptions_req;
	memset(&GetAudioSourceConfigurationOptions_req, 0, sizeof(GetAudioSourceConfigurationOptions_REQ));
	
	ONVIF_RET ret = parse_GetAudioSourceConfigurationOptions(p_GetAudioSourceConfigurationOptions, &GetAudioSourceConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetAudioSourceConfigurationOptions_rly_xml_2, (char *)&GetAudioSourceConfigurationOptions_req);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetAudioSourceConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetAudioSourceConfigurationOptions\r\n");

	XMLN * p_GetAudioSourceConfigurationOptions = xml_node_soap_get(p_body, "GetAudioSourceConfigurationOptions");
	assert(p_GetAudioSourceConfigurationOptions);

	GetAudioSourceConfigurationOptions_REQ GetAudioSourceConfigurationOptions_req;
	memset(&GetAudioSourceConfigurationOptions_req, 0, sizeof(GetAudioSourceConfigurationOptions_REQ));
	
	ONVIF_RET ret = parse_GetAudioSourceConfigurationOptions(p_GetAudioSourceConfigurationOptions, &GetAudioSourceConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetAudioSourceConfigurationOptions_rly_xml, (char *)&GetAudioSourceConfigurationOptions_req);
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetAudioSourceConfiguration\r\n");

	XMLN * p_GetAudioSourceConfiguration = xml_node_soap_get(p_body, "GetAudioSourceConfiguration");
	assert(p_GetAudioSourceConfiguration);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_GetAudioSourceConfiguration, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetAudioSourceConfiguration_rly_xml, p_ConfigurationToken->data);
	}
	else 
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_SetAudioSourceConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetAudioSourceConfiguration\r\n");

	XMLN * p_SetAudioSourceConfiguration = xml_node_soap_get(p_body, "SetAudioSourceConfiguration");
	assert(p_SetAudioSourceConfiguration);

	SetAudioSourceConfiguration_REQ SetAudioSourceConfiguration_req;
	memset(&SetAudioSourceConfiguration_req, 0, sizeof(SetAudioSourceConfiguration_REQ));

	ONVIF_RET ret = parse_SetAudioSourceConfiguration(p_SetAudioSourceConfiguration, &SetAudioSourceConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetAudioSourceConfiguration(&SetAudioSourceConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetAudioSourceConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetVideoEncoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetVideoEncoderConfigurationOptions\r\n");

	XMLN * p_GetVideoEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetVideoEncoderConfigurationOptions");
	assert(p_GetVideoEncoderConfigurationOptions);

	GetVideoEncoderConfigurationOptions_REQ GetVideoEncoderConfigurationOptions_req;
	memset(&GetVideoEncoderConfigurationOptions_req, 0, sizeof(GetVideoEncoderConfigurationOptions_REQ));

	ONVIF_RET ret = parse_GetVideoEncoderConfigurationOptions(p_GetVideoEncoderConfigurationOptions, &GetVideoEncoderConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetVideoEncoderConfigurationOptions_rly_xml, (char *)&GetVideoEncoderConfigurationOptions_req);	
	}

	return soap_build_err_rly(p_user, rx_msg, ret);    
}

int soap_GetVideoEncoderConfigurationOptions_2(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetVideoEncoderConfigurationOptions\r\n");

	XMLN * p_GetVideoEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetVideoEncoderConfigurationOptions");
	assert(p_GetVideoEncoderConfigurationOptions);

	GetVideoEncoderConfigurationOptions_REQ GetVideoEncoderConfigurationOptions_req;
	memset(&GetVideoEncoderConfigurationOptions_req, 0, sizeof(GetVideoEncoderConfigurationOptions_REQ));

	ONVIF_RET ret = parse_GetVideoEncoderConfigurationOptions(p_GetVideoEncoderConfigurationOptions, &GetVideoEncoderConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetVideoEncoderConfigurationOptions_rly_xml_2, (char *)&GetVideoEncoderConfigurationOptions_req);	
	}

	return soap_build_err_rly(p_user, rx_msg, ret);    
}

int soap_GetAudioEncoderConfigurationOptions_2(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetAudioEncoderConfigurationOptions_2\r\n");

	XMLN * p_GetAudioEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetAudioEncoderConfigurationOptions");
	assert(p_GetAudioEncoderConfigurationOptions);

	GetAudioEncoderConfigurationOptions_REQ GetAudioEncoderConfigurationOptions_req;
	memset(&GetAudioEncoderConfigurationOptions_req, 0, sizeof(GetAudioEncoderConfigurationOptions_REQ));

	ONVIF_RET ret = parse_GetAudioEncoderConfigurationOptions(p_GetAudioEncoderConfigurationOptions, &GetAudioEncoderConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetAudioEncoderConfigurationOptions_rly_xml_2, (char *)&GetAudioEncoderConfigurationOptions_req);	
	}

	return soap_build_err_rly(p_user, rx_msg, ret);  
}

int soap_GetAudioEncoderConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetAudioEncoderConfigurationOptions\r\n");

	XMLN * p_GetAudioEncoderConfigurationOptions = xml_node_soap_get(p_body, "GetAudioEncoderConfigurationOptions");
	assert(p_GetAudioEncoderConfigurationOptions);

	GetAudioEncoderConfigurationOptions_REQ GetAudioEncoderConfigurationOptions_req;
	memset(&GetAudioEncoderConfigurationOptions_req, 0, sizeof(GetAudioEncoderConfigurationOptions_REQ));

	ONVIF_RET ret = parse_GetAudioEncoderConfigurationOptions(p_GetAudioEncoderConfigurationOptions, &GetAudioEncoderConfigurationOptions_req);
	if (ONVIF_OK == ret)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetAudioEncoderConfigurationOptions_rly_xml, (char *)&GetAudioEncoderConfigurationOptions_req);	
	}

	return soap_build_err_rly(p_user, rx_msg, ret);  
}

int soap_GetCompatibleVideoSourceConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetCompatibleVideoSourceConfigurations\r\n");

	XMLN * p_GetCompatibleVideoSourceConfigurations = xml_node_soap_get(p_body, "GetCompatibleVideoSourceConfigurations");
	assert(p_GetCompatibleVideoSourceConfigurations);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetCompatibleVideoSourceConfigurations, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
	    return soap_build_send_rly(p_user, rx_msg, build_GetCompatibleVideoSourceConfigurations_rly_xml, p_ProfileToken->data);
	}
	else 
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_SetVideoEncoderConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetVideoEncoderConfiguration\r\n");

	XMLN * p_SetVideoEncoderConfiguration = xml_node_soap_get(p_body, "SetVideoEncoderConfiguration");
	assert(p_SetVideoEncoderConfiguration);

	SetVideoEncoderConfiguration_REQ SetVideoEncoderConfiguration_req;
	memset(&SetVideoEncoderConfiguration_req, 0, sizeof(SetVideoEncoderConfiguration_REQ));

	ONVIF_RET ret = parse_SetVideoEncoderConfiguration(p_SetVideoEncoderConfiguration, &SetVideoEncoderConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetVideoEncoderConfiguration(&SetVideoEncoderConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetVideoEncoderConfiguration_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}


int soap_SystemReboot(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_SystemReboot\r\n");

	int ret = soap_build_send_rly(p_user, rx_msg, build_SystemReboot_rly_xml);

	onvif_SystemReboot();

	return ret;
}

int soap_SetSystemFactoryDefault(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetSystemFactoryDefault\r\n");

	XMLN * p_SetSystemFactoryDefault = xml_node_soap_get(p_body, "SetSystemFactoryDefault");
	assert(p_SetSystemFactoryDefault);

	int type = 0;

	XMLN * p_FactoryDefault = xml_node_soap_get(p_SetSystemFactoryDefault, "FactoryDefault");
	if (p_FactoryDefault && p_FactoryDefault->data)
	{
		if (strcasecmp(p_FactoryDefault->data, "Hard") == 0)
		{
			type = 1;
		}
	}

	int ret = soap_build_send_rly(p_user, rx_msg, build_SetSystemFactoryDefault_rly_xml);

	onvif_SetSystemFactoryDefault(type);

	return ret;
}

int soap_GetSystemLog(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetSystemLog\r\n");

	XMLN * p_GetSystemLog = xml_node_soap_get(p_body, "GetSystemLog");
	assert(p_GetSystemLog);

	int type = 0;

	XMLN * p_LogType = xml_node_soap_get(p_GetSystemLog, "LogType");
	if (p_LogType && p_LogType->data)
	{
		if (strcasecmp(p_LogType->data, "Access") == 0)
		{
			type = 1;
		}
	}

	return soap_build_send_rly(p_user, rx_msg, build_GetSystemLog_rly_xml, (char *)type);
}

int soap_SetSystemDateAndTime(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetSystemDateAndTime\r\n");

	XMLN * p_SetSystemDateAndTime = xml_node_soap_get(p_body, "SetSystemDateAndTime");
	assert(p_SetSystemDateAndTime);

	SetSystemDateAndTime_REQ SetSystemDateAndTime_req;
	memset(&SetSystemDateAndTime_req, 0, sizeof(SetSystemDateAndTime_REQ));

	ONVIF_RET ret = parse_SetSystemDateAndTime(p_SetSystemDateAndTime, &SetSystemDateAndTime_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetSystemDateAndTime(&SetSystemDateAndTime_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetSystemDateAndTime_rly_xml); 
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}


int soap_GetServices(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetServices\r\n");

	XMLN * p_GetServices = xml_node_soap_get(p_body, "tds:GetServices");
	assert(p_GetServices);

	BOOL bflag = false;
	XMLN * p_IncludeCapability = xml_node_soap_get(p_GetServices, "IncludeCapability");
	if (p_IncludeCapability && p_IncludeCapability->data)
	{
		bflag = (strcasecmp(p_IncludeCapability->data, "true") ? FALSE : TRUE);
	}
		
	return soap_build_send_rly(p_user, rx_msg, build_GetServices_rly_xml, (char *)bflag);
}

int soap_Subscribe(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_Subscribe\r\n");

	XMLN * p_Subscribe = xml_node_soap_get(p_body, "Subscribe");
	assert(p_Subscribe);

	Subscribe_REQ Subscribe_req;
	memset(&Subscribe_req, 0, sizeof(Subscribe_REQ));
	
	ONVIF_RET ret = parse_Subscribe(p_Subscribe, &Subscribe_req);
	if (ONVIF_OK == ret)
	{
	    ret = onvif_Subscribe(&Subscribe_req);
	    if (ONVIF_OK == ret)
	    {
			return soap_build_send_rly(p_user, rx_msg, build_Subscribe_rly_xml, (char *)Subscribe_req.p_eua); 
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_Unsubscribe(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	__INFO("soap_Unsubscribe\r\n");

	XMLN * p_Unsubscribe = xml_node_soap_get(p_body, "Unsubscribe");
	assert(p_Unsubscribe);

	XMLN * p_To = xml_node_soap_get(p_header, "To");
	if (p_To && p_To->data)
	{
		ONVIF_RET ret = onvif_Unsubscribe(p_To->data);		
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_Unsubscribe_rly_xml);
		}
		else
		{
			return soap_build_err_rly(p_user, rx_msg, ret);
		}
	}
	
	return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attibute");
}

int soap_Renew(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body, XMLN * p_header)
{
	__INFO("soap_Renew.\r\n");

	XMLN * p_Renew = xml_node_soap_get(p_body, "Renew");
	assert(p_Renew);

	Renew_REQ Renew_req;
	memset(&Renew_req, 0, sizeof(Renew_REQ));

	XMLN * p_To = xml_node_soap_get(p_header, "To");
	if (p_To && p_To->data)
	{
		strncpy(Renew_req.refer_addr, p_To->data, sizeof(Renew_req.refer_addr)-1);
	}
	
	ONVIF_RET ret = parse_Renew(p_Renew, &Renew_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_Renew(&Renew_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_Renew_rly_xml);
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_SetSynchronizationPoint(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_SetSynchronizationPoint.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_SetSynchronizationPoint_rly_xml);
}

int soap_GetScopes(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetScopes\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetScopes_rly_xml); 
}

int soap_AddScopes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_AddScopes\r\n");

	XMLN * p_AddScopes = xml_node_soap_get(p_body, "AddScopes");
	assert(p_AddScopes);

	ONVIF_SCOPE scopes[MAX_SCOPE_NUMS];
	memset(scopes, 0, sizeof(ONVIF_SCOPE) * MAX_SCOPE_NUMS);

	ONVIF_RET ret = parse_AddScopes(p_AddScopes, scopes, MAX_SCOPE_NUMS);
	if (ONVIF_OK == ret)
	{
		ret = onvif_add_scopes(scopes, MAX_SCOPE_NUMS);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_AddScopes_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_SetScopes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetScopes\r\n");

	XMLN * p_SetScopes = xml_node_soap_get(p_body, "SetScopes");
	assert(p_SetScopes);

	ONVIF_SCOPE scopes[MAX_SCOPE_NUMS];
	memset(scopes, 0, sizeof(ONVIF_SCOPE) * MAX_SCOPE_NUMS);

	ONVIF_RET ret = parse_SetScopes(p_SetScopes, scopes, MAX_SCOPE_NUMS);
	if (ONVIF_OK == ret)
	{
		ret = onvif_set_scopes(scopes, MAX_SCOPE_NUMS);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetScopes_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_RemoveScopes(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_RemoveScopes\r\n");

	XMLN * p_RemoveScopes = xml_node_soap_get(p_body, "RemoveScopes");
	assert(p_RemoveScopes);

	ONVIF_SCOPE scopes[MAX_SCOPE_NUMS];
	memset(scopes, 0, sizeof(ONVIF_SCOPE) * MAX_SCOPE_NUMS);

	ONVIF_RET ret = parse_AddScopes(p_RemoveScopes, scopes, MAX_SCOPE_NUMS);
	if (ONVIF_OK == ret)
	{
		ret = onvif_remove_scopes(scopes, MAX_SCOPE_NUMS);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RemoveScopes_rly_xml, (char *)scopes);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetHostname(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetHostname\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetHostname_rly_xml); 
}

int soap_SetHostname(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetHostname\r\n");

	XMLN * p_SetHostname = xml_node_soap_get(p_body, "SetHostname");
	assert(p_SetHostname);

	XMLN * p_Name = xml_node_soap_get(p_SetHostname, "Name");
	if (p_Name && p_Name->data)
	{
		ONVIF_RET ret = onvif_SetHostname(p_Name->data, FALSE);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetHostname_rly_xml);
		}
		else if (ONVIF_ERR_INVALID_HOSTNAME == ret)
		{
			return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidHostname", "Invalid Hostname");
		}
	}
	
	return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:InvalidHostname", "Invalid ArgVal"); 
}

int soap_GetNetworkProtocols(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetNetworkProtocols\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetNetworkProtocols_rly_xml); 
}

int soap_SetNetworkProtocols(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetNetworkProtocols\r\n");

	XMLN * p_SetNetworkProtocols = xml_node_soap_get(p_body, "SetNetworkProtocols");
	assert(p_SetNetworkProtocols);

	SetNetworkProtocols_REQ SetNetworkProtocols_req;
	memset(&SetNetworkProtocols_req, 0, sizeof(SetNetworkProtocols_REQ));

	ONVIF_RET ret = parse_SetNetworkProtocols(p_SetNetworkProtocols, &SetNetworkProtocols_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetNetworkProtocols(&SetNetworkProtocols_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetNetworkProtocols_rly_xml); 
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetNetworkDefaultGateway(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetNetworkDefaultGateway\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetNetworkDefaultGateway_rly_xml); 
}

int soap_SetNetworkDefaultGateway(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetNetworkDefaultGateway\r\n");

	XMLN * p_SetNetworkDefaultGateway = xml_node_soap_get(p_body, "SetNetworkDefaultGateway");
	assert(p_SetNetworkDefaultGateway);

	SetNetworkDefaultGateway_REQ SetNetworkDefaultGateway_req;
	memset(&SetNetworkDefaultGateway_req, 0, sizeof(SetNetworkDefaultGateway_REQ));

	ONVIF_RET ret = parse_SetNetworkDefaultGateway(p_SetNetworkDefaultGateway, &SetNetworkDefaultGateway_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetNetworkDefaultGateway(&SetNetworkDefaultGateway_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetNetworkDefaultGateway_rly_xml); 
		}
	}
		
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetDiscoveryMode(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetDiscoveryMode.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetDiscoveryMode_rly_xml); 
}

int soap_SetDiscoveryMode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetDiscoveryMode\r\n");

	XMLN * p_SetDiscoveryMode = xml_node_soap_get(p_body, "SetDiscoveryMode");
	assert(p_SetDiscoveryMode);

	SetDiscoveryMode_REQ SetDiscoveryMode_req;
	memset(&SetDiscoveryMode_req, 0, sizeof(SetDiscoveryMode_REQ));

	ONVIF_RET ret = parse_SetDiscoveryMode(p_SetDiscoveryMode, &SetDiscoveryMode_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetDiscoveryMode(&SetDiscoveryMode_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetDiscoveryMode_rly_xml); 
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetDNS(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetDNS\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetDNS_rly_xml); 
}

int soap_SetDNS(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetDNS\r\n");

	XMLN * p_SetDNS = xml_node_soap_get(p_body, "SetDNS");
	assert(p_SetDNS);

	SetDNS_REQ SetDNS_req;
	memset(&SetDNS_req, 0, sizeof(SetDNS_REQ));

	ONVIF_RET ret = parse_SetDNS(p_SetDNS, &SetDNS_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetDNS(&SetDNS_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetDNS_rly_xml); 
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}


int soap_GetNTP(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetNTP\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetNTP_rly_xml); 
}

int soap_SetNTP(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetNTP\r\n");

	XMLN * p_SetNTP = xml_node_soap_get(p_body, "SetNTP");
	assert(p_SetNTP);

	SetNTP_REQ SetNTP_req;
	memset(&SetNTP_req, 0, sizeof(SetNTP_REQ));

	ONVIF_RET ret = parse_SetNTP(p_SetNTP, &SetNTP_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetNTP(&SetNTP_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetNTP_rly_xml); 
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}


int soap_GetServiceCapabilities(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetServiceCapabilities\r\n");

	XMLN * p_GetServiceCapabilities = xml_node_soap_get(p_body, "GetServiceCapabilities");
	assert(p_GetServiceCapabilities);

	E_CAP_CATEGORY category;
	char * post = rx_msg->first_line.value_string;
	if (NULL == post)
	{
		category = CAP_CATEGORY_DEVICE;
	}
	else if (strstr(post, "media"))
	{
		category = CAP_CATEGORY_MEDIA;
	}
	else if (strstr(post, "device"))
	{
		category = CAP_CATEGORY_DEVICE;
	}
	else if (strstr(post, "image"))
	{
		category = CAP_CATEGORY_IMAGE;
	}
	else if (strstr(post, "ptz"))
	{
		category = CAP_CATEGORY_PTZ;
	}
	else if (strstr(post, "event"))
	{
		category = CAP_CATEGORY_EVENTS;
	}
	else if (strstr(post, "analytics"))
	{
		category = CAP_CATEGORY_ANALYTICS;
	}
	else
	{
		category = CAP_CATEGORY_DEVICE;
	}
	
	return soap_build_send_rly(p_user, rx_msg, build_GetServiceCapabilities_rly_xml, (char *)category); 	
}

int soap_GetEventProperties(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetEventProperties.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetEventProperties_rly_xml);
}

int soap_GetWsdlUrl(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetWsdlUrl.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetWsdlUrl_rly_xml);
}

int soap_GetNodes(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetNodes.\r\n");

	return soap_build_send_rly(p_user, rx_msg, build_GetNodes_rly_xml);
}

int soap_GetNode(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetNode.\r\n");

	XMLN * p_GetNode = xml_node_soap_get(p_body, "GetNode");
	assert(p_GetNode);
	
	XMLN * p_NodeToken = xml_node_soap_get(p_GetNode, "tptz:NodeToken");
	if (p_NodeToken && p_NodeToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetNode_rly_xml, p_NodeToken->data);
	}
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_GetConfigurations(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	__INFO("soap_GetConfigurations.\r\n");
		
	return soap_build_send_rly(p_user, rx_msg, build_GetConfigurations_rly_xml);
}

int soap_GetConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetConfiguration.\r\n");

	XMLN * p_GetConfiguration = xml_node_soap_get(p_body, "GetConfiguration");
	assert(p_GetConfiguration);
	
	XMLN * p_PTZConfigurationToken = xml_node_soap_get(p_GetConfiguration, "tptz:PTZConfigurationToken");
	if (p_PTZConfigurationToken && p_PTZConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetConfiguration_rly_xml, p_PTZConfigurationToken->data);
	}
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_AddPTZConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_AddPTZConfiguration.\r\n");

	XMLN * p_AddPTZConfiguration = xml_node_soap_get(p_body, "AddPTZConfiguration");
	assert(p_AddPTZConfiguration);
	
	AddPTZConfiguration_REQ AddPTZConfiguration_req;
	memset(&AddPTZConfiguration_req, 0, sizeof(AddPTZConfiguration_REQ));

	ONVIF_RET ret = parse_AddPTZConfiguration(p_AddPTZConfiguration, &AddPTZConfiguration_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_AddPTZConfiguration(&AddPTZConfiguration_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_AddPTZConfiguration_rly_xml);
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_RemovePTZConfiguration(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_RemovePTZConfiguration.\r\n");

	XMLN * p_RemovePTZConfiguration = xml_node_soap_get(p_body, "RemovePTZConfiguration");
	assert(p_RemovePTZConfiguration);

	XMLN * p_ProfileToken = xml_node_soap_get(p_RemovePTZConfiguration, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ONVIF_RET ret = onvif_RemovePTZConfiguration(p_ProfileToken->data);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RemovePTZConfiguration_rly_xml);
		}
		else if (ONVIF_ERR_NO_PROFILE == ret)
		{
			return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_INVALIDARGVAL, "ter:NoProfile", "Profile Not Exist");
		}
	}
	else
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}

	return soap_err_rly(p_user, rx_msg);
}


int soap_GetConfigurationOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetConfigurationOptions.\r\n");

	XMLN * p_GetConfigurationOptions = xml_node_soap_get(p_body, "GetConfigurationOptions");
	assert(p_GetConfigurationOptions);
	
	XMLN * p_ConfigurationToken = xml_node_soap_get(p_GetConfigurationOptions, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetConfigurationOptions_rly_xml, p_ConfigurationToken->data);
	}
	else
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetConfigurationOptions_rly_xml, "PTZ_000");
	}
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_GetStatus(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetStatus.\r\n");

	XMLN * p_GetStatus = xml_node_soap_get(p_body, "GetStatus");
	assert(p_GetStatus);

	char * post = rx_msg->first_line.value_string;
	if (strstr(post, "ptz"))
	{
		XMLN * p_ProfileToken = xml_node_soap_get(p_GetStatus, "ProfileToken");
		if (p_ProfileToken && p_ProfileToken->data)
		{
			return soap_build_send_rly(p_user, rx_msg, build_PTZ_GetStatus_rly_xml, p_ProfileToken->data);
		}
	}
	else if (strstr(post, "image"))
	{
		XMLN * p_VideoSourceToken = xml_node_soap_get(p_GetStatus, "VideoSourceToken");
		if (p_VideoSourceToken && p_VideoSourceToken->data)
		{
			return soap_build_send_rly(p_user, rx_msg, build_IMG_GetStatus_rly_xml, p_VideoSourceToken->data);
		}
	}	
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_ContinuousMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_ContinuousMove.\r\n");
	
	XMLN * p_ContinuousMove = xml_node_soap_get(p_body, "ContinuousMove");
	assert(p_ContinuousMove);

	ContinuousMove_REQ ContinuousMove_req;
	memset(&ContinuousMove_req, 0, sizeof(ContinuousMove_REQ));

	ONVIF_RET ret = parse_ContinuousMove(p_ContinuousMove, &ContinuousMove_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_ContinuousMove(&ContinuousMove_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_ContinuousMove_rly_xml);
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);		
}

int soap_Stop(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_Stop\r\n");

	XMLN * p_Stop = xml_node_soap_get(p_body, "Stop");
	assert(p_Stop);

	char * post = rx_msg->first_line.value_string;
	if (strstr(post, "ptz"))
	{
		PTZ_Stop_REQ Stop_req;
		memset(&Stop_req, 0, sizeof(PTZ_Stop_REQ));		

		ONVIF_RET ret = parse_PTZ_Stop(p_Stop, &Stop_req);
		if (ONVIF_OK == ret)
		{
			ret = onvif_PTZ_Stop(&Stop_req);
			if (ONVIF_OK == ret)
			{
				return soap_build_send_rly(p_user, rx_msg, build_PTZ_Stop_rly_xml);
			}
		}

		return soap_build_err_rly(p_user, rx_msg, ret);
	}
	else if (strstr(post, "image"))
	{
		XMLN * p_VideoSourceToken = xml_node_soap_get(p_Stop, "VideoSourceToken");
		if (p_VideoSourceToken && p_VideoSourceToken->data)
		{
			return soap_build_send_rly(p_user, rx_msg, build_IMG_Stop_rly_xml, p_VideoSourceToken->data);
		}
	}
	
	return soap_err_rly(p_user, rx_msg);
}

int soap_AbsoluteMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_AbsoluteMove\r\n");

	XMLN * p_AbsoluteMove = xml_node_soap_get(p_body, "AbsoluteMove");
	assert(p_AbsoluteMove);

	AbsoluteMove_REQ AbsoluteMove_req;
	memset(&AbsoluteMove_req, 0, sizeof(AbsoluteMove_REQ));

	ONVIF_RET ret = parse_AbsoluteMove(p_AbsoluteMove, &AbsoluteMove_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_AbsoluteMove(&AbsoluteMove_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_AbsoluteMove_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_RelativeMove(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_RelativeMove\r\n");

	XMLN * p_RelativeMove = xml_node_soap_get(p_body, "RelativeMove");
	assert(p_RelativeMove);

	RelativeMove_REQ RelativeMove_req;
	memset(&RelativeMove_req, 0, sizeof(RelativeMove_REQ));

	ONVIF_RET ret = parse_RelativeMove(p_RelativeMove, &RelativeMove_req);    
	if (ONVIF_OK == ret)
	{
		ret = onvif_RelativeMove(&RelativeMove_req);		
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RelativeMove_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_SetPreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetPreset\r\n");

	XMLN * p_SetPreset = xml_node_soap_get(p_body, "SetPreset");
	assert(p_SetPreset);

	SetPreset_REQ SetPreset_req;
	memset(&SetPreset_req, 0, sizeof(SetPreset_REQ));

	ONVIF_RET ret = parse_SetPreset(p_SetPreset, &SetPreset_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetPreset(&SetPreset_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetPreset_rly_xml, SetPreset_req.preset_token);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetPresets(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetPresets\r\n");

	XMLN * p_GetPresets = xml_node_soap_get(p_body, "GetPresets");
	assert(p_GetPresets);

	XMLN * p_ProfileToken = xml_node_soap_get(p_GetPresets, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetPresets_rly_xml, p_ProfileToken->data);
	}
	else 
	{
		return soap_err_rly(p_user, rx_msg, ERR_SENDER, ERR_MISSINGATTR, NULL, "Missing Attribute");
	}
}

int soap_RemovePreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_RemovePreset\r\n");

	XMLN * p_RemovePreset = xml_node_soap_get(p_body, "RemovePreset");
	assert(p_RemovePreset);

	RemovePreset_REQ RemovePreset_req;
	memset(&RemovePreset_req, 0, sizeof(RemovePreset_REQ));
	
	ONVIF_RET ret = parse_RemovePreset(p_RemovePreset, &RemovePreset_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_RemovePreset(&RemovePreset_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_RemovePreset_rly_xml);
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GotoPreset(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GotoPreset\r\n");

	XMLN * p_GotoPreset = xml_node_soap_get(p_body, "GotoPreset");
	assert(p_GotoPreset);

	GotoPreset_REQ GotoPreset_req;
	memset(&GotoPreset_req, 0, sizeof(GotoPreset_REQ));

	ONVIF_RET ret = parse_GotoPreset(p_GotoPreset, &GotoPreset_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_GotoPreset(&GotoPreset_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_GotoPreset_rly_xml);
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GotoHomePosition(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GotoHomePosition\r\n");

	XMLN * p_GotoHomePosition = xml_node_soap_get(p_body, "GotoHomePosition");
	assert(p_GotoHomePosition);

	GotoHomePosition_REQ GotoHomePosition_req;
	memset(&GotoHomePosition_req, 0, sizeof(GotoHomePosition_REQ));

	ONVIF_RET ret = parse_GotoHomePosition(p_GotoHomePosition, &GotoHomePosition_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_GotoHomePosition(&GotoHomePosition_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_GotoHomePosition_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_SetHomePosition(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetHomePosition\r\n");

    XMLN * p_SetHomePosition = xml_node_soap_get(p_body, "SetHomePosition");
	assert(p_SetHomePosition);

	ONVIF_RET ret = ONVIF_ERR_MISSINGATTR;
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_SetHomePosition, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		ret = onvif_SetHomePosition(p_ProfileToken->data);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetHomePosition_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetGuaranteedNumberOfVideoEncoderInstances(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetGuaranteedNumberOfVideoEncoderInstances\r\n");

	XMLN * p_GetGuaranteedNumberOfVideoEncoderInstances = xml_node_soap_get(p_body, "GetGuaranteedNumberOfVideoEncoderInstances");
	assert(p_GetGuaranteedNumberOfVideoEncoderInstances);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_GetGuaranteedNumberOfVideoEncoderInstances, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetGuaranteedNumberOfVideoEncoderInstances_rly_xml, p_ConfigurationToken->data);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MISSINGATTR);
}

int soap_GetImagingSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetImagingSettings\r\n");

	XMLN * p_GetImagingSettings = xml_node_soap_get(p_body, "GetImagingSettings");
	assert(p_GetImagingSettings);

	XMLN * p_VideoSourceToken = xml_node_soap_get(p_GetImagingSettings, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetImagingSettings_rly_xml, p_VideoSourceToken->data);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MISSINGATTR);
}

int soap_SetImagingSettings(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_SetImagingSettings\r\n");

	XMLN * p_SetImagingSettings = xml_node_soap_get(p_body, "SetImagingSettings");
	assert(p_SetImagingSettings);

	SetImagingSettings_REQ SetImagingSettings_req;
	memset(&SetImagingSettings_req, 0, sizeof(SetImagingSettings_REQ));
	memcpy(&SetImagingSettings_req.img_cfg, &g_onvif_cfg.img_cfg, sizeof(IMAGE_CFG));

	ONVIF_RET ret = parse_SetImagingSettings(p_SetImagingSettings, &SetImagingSettings_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_SetImagingSettings(&SetImagingSettings_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_SetImagingSettings_rly_xml);
		}
	}

	return soap_build_err_rly(p_user, rx_msg, ret);
}

int soap_GetOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetOptions\r\n");

	XMLN * p_GetOptions = xml_node_soap_get(p_body, "GetOptions");
	assert(p_GetOptions);

	XMLN * p_VideoSourceToken = xml_node_soap_get(p_GetOptions, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetOptions_rly_xml, p_VideoSourceToken->data);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MISSINGATTR);
}

int soap_GetMoveOptions(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_GetMoveOptions\r\n");

	XMLN * p_GetMoveOptions = xml_node_soap_get(p_body, "GetMoveOptions");
	assert(p_GetMoveOptions);

	XMLN * p_VideoSourceToken = xml_node_soap_get(p_GetMoveOptions, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		return soap_build_send_rly(p_user, rx_msg, build_GetMoveOptions_rly_xml, p_VideoSourceToken->data);
	}

	return soap_build_err_rly(p_user, rx_msg, ONVIF_ERR_MISSINGATTR);
}

int soap_Move(HTTPCLN * p_user, HTTPMSG * rx_msg, XMLN * p_body)
{
	__INFO("soap_Move\r\n");

	XMLN * p_Move = xml_node_soap_get(p_body, "Move");
	assert(p_Move);

	Move_REQ Move_req;
	memset(&Move_req, 0, sizeof(Move_REQ));

	ONVIF_RET ret = parse_Move(p_Move, &Move_req);
	if (ONVIF_OK == ret)
	{
		ret = onvif_Move(&Move_req);
		if (ONVIF_OK == ret)
		{
			return soap_build_send_rly(p_user, rx_msg, build_Move_rly_xml);
		}
	}
	
	return soap_build_err_rly(p_user, rx_msg, ret);
}



void soap_calc_digest(const char *created, unsigned char *nonce, int noncelen, const char *password, unsigned char hash[20])
{
	sha1_context ctx;
	
	sha1_starts(&ctx);
	sha1_update(&ctx, (unsigned char *)nonce, noncelen);
	sha1_update(&ctx, (unsigned char *)created, strlen(created));
	sha1_update(&ctx, (unsigned char *)password, strlen(password));
	sha1_finish(&ctx, (unsigned char *)hash);
}

bool soap_auth_process(XMLN * p_Security)
{
	XMLN * p_UsernameToken = xml_node_soap_get(p_Security, "wsse:UsernameToken");
	if (NULL == p_UsernameToken)
	{
		return false;
	}

	XMLN * p_Username = xml_node_soap_get(p_UsernameToken, "wsse:Username");
	XMLN * p_Password = xml_node_soap_get(p_UsernameToken, "wsse:Password");
	XMLN * p_Nonce = xml_node_soap_get(p_UsernameToken, "wsse:Nonce");
	XMLN * p_Created = xml_node_soap_get(p_UsernameToken, "wsse:Created");

	if (NULL == p_Username || NULL == p_Username->data || 
		NULL == p_Password || NULL == p_Password->data || 
		NULL == p_Nonce || NULL == p_Nonce->data ||
		NULL == p_Created || NULL == p_Created->data)
	{
		return false;
	}

	const char * auth_pass = onvif_GetAuthPass(p_Username->data);
	if (NULL == auth_pass)	// user not exist
	{
		return false;
	}

	unsigned char nonce[200];	
	int nonce_len = base64_decode(p_Nonce->data, nonce, sizeof(nonce));
	unsigned char HA[20];
	char HABase64[100];
	
	soap_calc_digest(p_Created->data, nonce, nonce_len, auth_pass, HA);
	base64_encode(HA, 20, HABase64, sizeof(HABase64));

	if (strcmp(HABase64, p_Password->data) == 0)
	{
		return true;
	}
	
	return false;
}


/*********************************************************
 *
 * process soap request
 *
 * p_user [in] 	 --- http client
 * rx_msg [in] --- http message
 *
**********************************************************/ 
void soap_process_request(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	bool auth = false;
	
	char * p_xml = get_http_ctt(rx_msg);
	if (NULL == p_xml)
	{
		__INFO("soap_process::get_http_ctt ret null!!!\r\n");
		return;
	}

	//__INFO("soap_process::rx xml:\r\n%s\r\n", p_xml);

	XMLN * p_node = xxx_hxml_parse(p_xml, strlen(p_xml));
	if (NULL == p_node || NULL == p_node->name)
	{
		__INFO("soap_process::xxx_hxml_parse ret null!!!\r\n");
		return;
	}
	
	if (soap_strcmp(p_node->name, "Envelope") != 0)
	{
		__INFO("soap_process::node name[%s] != [s:Envelope]!!!\r\n", p_node->name);
		xml_node_del(p_node);
		return;
	}

	XMLN * p_header = xml_node_soap_get(p_node, "Header");
	if (p_header)
	{
		XMLN * p_Security = xml_node_soap_get(p_header, "Security");
		if (p_Security)
		{
			auth = soap_auth_process(p_Security);
		}
	}

	if (g_onvif_cfg.need_auth && !auth)
	{
		soap_security_rly(p_user, rx_msg);
		xml_node_del(p_node);
		return;
	}

	XMLN * p_body = xml_node_soap_get(p_node, "Body");
	if (NULL == p_body)
	{
		__INFO("soap_process::xml_node_soap_get[s:Body] ret null!!!\r\n");
		xml_node_del(p_node);
		return;
	}

	if (NULL == p_body->f_child)
	{
		__INFO("soap_process::body first child node is null!!!\r\n");
	}	
	else if (NULL == p_body->f_child->name)
	{
		__INFO("soap_process::body first child node name is null!!!\r\n");
	}	
	else
	{
		__INFO("soap_process::body first child node name[%s].\r\n", p_body->f_child->name);
	}

	if (soap_strcmp(p_body->f_child->name, "GetDeviceInformation") == 0)
	{
		soap_GetDeviceInformation(p_user, rx_msg);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "GetCapabilities") == 0)
	{
		soap_GetCapabilities(p_user, rx_msg, p_body);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "GetProfiles") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetProfiles") == 0)
			soap_GetProfiles_2(p_user, rx_msg);
		else
			soap_GetProfiles(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetProfile") == 0)
	{
		soap_GetProfile(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "CreateProfile") == 0)
	{
		soap_CreateProfile(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "DeleteProfile") == 0)
	{
		soap_DeleteProfile(p_user, rx_msg, p_body);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "AddVideoSourceConfiguration") == 0)
	{
		soap_AddVideoSourceConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RemoveVideoSourceConfiguration") == 0)
	{
		soap_RemoveVideoSourceConfiguration(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "AddAudioSourceConfiguration") == 0)
	{
		soap_AddAudioSourceConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RemoveAudioSourceConfiguration") == 0)
	{
		soap_RemoveAudioSourceConfiguration(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "AddVideoEncoderConfiguration") == 0)
	{
		soap_AddVideoEncoderConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RemoveVideoEncoderConfiguration") == 0)
	{
		soap_RemoveVideoEncoderConfiguration(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "AddAudioEncoderConfiguration") == 0)
	{
		soap_AddAudioEncoderConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RemoveAudioEncoderConfiguration") == 0)
	{
		soap_RemoveAudioEncoderConfiguration(p_user, rx_msg, p_body);
	}	
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "GetSystemDateAndTime") == 0)
	{
		soap_GetSystemDateAndTime(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetSystemDateAndTime") == 0)
	{
		soap_SetSystemDateAndTime(p_user, rx_msg, p_body);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "GetStreamUri") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetStreamUri") == 0)
			soap_GetStreamUri_2(p_user, rx_msg, p_body);
		else
			soap_GetStreamUri(p_user, rx_msg, p_body);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "GetNetworkInterfaces") == 0)
	{	
		soap_GetNetworkInterfaces(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetNetworkInterfaces") == 0)
	{
		soap_SetNetworkInterfaces(p_user, rx_msg, p_body);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "GetVideoSources") == 0)
	{
		soap_GetVideoSources(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetAudioSources") == 0)
	{
		soap_GetAudioSources(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetVideoEncoderConfigurations") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetVideoEncoderConfigurations") == 0)
			soap_GetVideoEncoderConfigurations_2(p_user, rx_msg);
		else
			soap_GetVideoEncoderConfigurations(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetCompatibleVideoEncoderConfigurations") == 0)
	{
		soap_GetCompatibleVideoEncoderConfigurations(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "GetAudioEncoderConfigurations") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetAudioEncoderConfigurations") == 0)
			soap_GetAudioEncoderConfigurations_2(p_user, rx_msg);
		else
			soap_GetAudioEncoderConfigurations(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetCompatibleAudioEncoderConfigurations") == 0)
	{
		soap_GetCompatibleAudioEncoderConfigurations(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "GetVideoSourceConfigurations") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetVideoSourceConfigurations") == 0)
			soap_GetVideoSourceConfigurations_2(p_user, rx_msg);
		else
			soap_GetVideoSourceConfigurations(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetVideoSourceConfiguration") == 0)
	{
		soap_GetVideoSourceConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetVideoSourceConfigurationOptions") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetVideoSourceConfigurationOptions") == 0)
			soap_GetVideoSourceConfigurationOptions_2(p_user, rx_msg, p_body);
		else
			soap_GetVideoSourceConfigurationOptions(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetVideoSourceConfiguration") == 0)
	{
		soap_SetVideoSourceConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetAudioSourceConfigurations") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetAudioSourceConfigurations") == 0)
			soap_GetAudioSourceConfigurations_2(p_user, rx_msg);
		else
			soap_GetAudioSourceConfigurations(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetCompatibleAudioSourceConfigurations") == 0)
	{
		soap_GetCompatibleAudioSourceConfigurations(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetAudioSourceConfigurationOptions") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetAudioSourceConfigurationOptions") == 0)
			soap_GetAudioSourceConfigurationOptions_2(p_user, rx_msg, p_body);
		else
			soap_GetAudioSourceConfigurationOptions(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetAudioSourceConfiguration") == 0)
	{
		soap_GetAudioSourceConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetAudioSourceConfiguration") == 0)
	{
		soap_SetAudioSourceConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetVideoEncoderConfiguration") == 0)
	{
		soap_GetVideoEncoderConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetVideoEncoderConfiguration") == 0)
	{
		soap_SetVideoEncoderConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetAudioEncoderConfiguration") == 0)
	{
		soap_GetAudioEncoderConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetAudioEncoderConfiguration") == 0)
	{
		soap_SetAudioEncoderConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetVideoEncoderConfigurationOptions") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetVideoEncoderConfigurationOptions") == 0)
			soap_GetVideoEncoderConfigurationOptions_2(p_user, rx_msg, p_body);
		else
			soap_GetVideoEncoderConfigurationOptions(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetAudioEncoderConfigurationOptions") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetAudioEncoderConfigurationOptions") == 0)
			soap_GetAudioEncoderConfigurationOptions_2(p_user, rx_msg, p_body);
		else
			soap_GetAudioEncoderConfigurationOptions(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetCompatibleVideoSourceConfigurations") == 0)
	{
		soap_GetCompatibleVideoSourceConfigurations(p_user, rx_msg, p_body);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "SystemReboot") == 0)
	{
		soap_SystemReboot(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetSystemFactoryDefault") == 0)
	{
		soap_SetSystemFactoryDefault(p_user, rx_msg, p_body);
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (soap_strcmp(p_body->f_child->name, "GetSystemLog") == 0)
	{
		soap_GetSystemLog(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "GetServices") == 0)
	{
		soap_GetServices(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetSnapshotUri") == 0)
	{
		if (strcmp(p_body->f_child->name, "tr2:GetSnapshotUri") == 0)
			soap_GetSnapshotUri_2(p_user, rx_msg, p_body);
		else
			soap_GetSnapshotUri(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetScopes") == 0)
	{
		soap_GetScopes(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "AddScopes") == 0)
	{
		soap_AddScopes(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetScopes") == 0)
	{
		soap_SetScopes(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RemoveScopes") == 0)
	{
		soap_RemoveScopes(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "GetHostname") == 0)
	{
		soap_GetHostname(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetHostname") == 0)
	{
		soap_SetHostname(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetNetworkProtocols") == 0)
	{
		soap_GetNetworkProtocols(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetNetworkProtocols") == 0)
	{
		soap_SetNetworkProtocols(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetNetworkDefaultGateway") == 0)
	{
		soap_GetNetworkDefaultGateway(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetNetworkDefaultGateway") == 0)
	{
		soap_SetNetworkDefaultGateway(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetDiscoveryMode") == 0)
	{
		soap_GetDiscoveryMode(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetDiscoveryMode") == 0)
	{
		soap_SetDiscoveryMode(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetDNS") == 0)
	{
		soap_GetDNS(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetDNS") == 0)
	{
		soap_SetDNS(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetNTP") == 0)
	{
		soap_GetNTP(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetNTP") == 0)
	{
		soap_SetNTP(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetServiceCapabilities") == 0)
	{			
		soap_GetServiceCapabilities(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetEventProperties") == 0)
	{
		soap_GetEventProperties(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "Subscribe") == 0)
	{
	    soap_Subscribe(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "Unsubscribe") == 0)
	{
		soap_Unsubscribe(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_body->f_child->name, "Renew") == 0)
	{
		soap_Renew(p_user, rx_msg, p_body, p_header);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetSynchronizationPoint") == 0)
	{
		soap_SetSynchronizationPoint(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetWsdlUrl") == 0)
	{
		soap_GetWsdlUrl(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetNodes") == 0)
	{
		soap_GetNodes(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetNode") == 0)
	{
		soap_GetNode(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetConfigurations") == 0)
	{
		soap_GetConfigurations(p_user, rx_msg);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetConfiguration") == 0)
	{
		soap_GetConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "AddPTZConfiguration") == 0)
	{
		soap_AddPTZConfiguration(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RemovePTZConfiguration") == 0)
	{
		soap_RemovePTZConfiguration(p_user, rx_msg, p_body);
	}	
	else if (soap_strcmp(p_body->f_child->name, "GetConfigurationOptions") == 0)
	{	
		soap_GetConfigurationOptions(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetStatus") == 0)
	{
		soap_GetStatus(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "ContinuousMove") == 0)
	{
		soap_ContinuousMove(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "Stop") == 0)
	{
		soap_Stop(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "AbsoluteMove") == 0)
	{
		soap_AbsoluteMove(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RelativeMove") == 0)
	{
		soap_RelativeMove(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetPreset") == 0)
	{
		soap_SetPreset(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetPresets") == 0)
	{
		soap_GetPresets(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "RemovePreset") == 0)
	{
		soap_RemovePreset(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GotoPreset") == 0)
	{
		soap_GotoPreset(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GotoHomePosition") == 0)
	{
		soap_GotoHomePosition(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetHomePosition") == 0)
	{
		soap_SetHomePosition(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetGuaranteedNumberOfVideoEncoderInstances") == 0)
	{
		soap_GetGuaranteedNumberOfVideoEncoderInstances(p_user, rx_msg, p_body);		
	}
	else if (soap_strcmp(p_body->f_child->name, "GetImagingSettings") == 0)
	{
		soap_GetImagingSettings(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "SetImagingSettings") == 0)
	{
		soap_SetImagingSettings(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetOptions") == 0)
	{
		soap_GetOptions(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "GetMoveOptions") == 0)
	{
		soap_GetMoveOptions(p_user, rx_msg, p_body);
	}
	else if (soap_strcmp(p_body->f_child->name, "Move") == 0)
	{
		soap_Move(p_user, rx_msg, p_body);
	}
	else
	{
		soap_err_rly(p_user, rx_msg);
	}
	
	xml_node_del(p_node);
}



