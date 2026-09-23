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
#include "onvif.h"
#include "soap_parser.h"
#include "onvif_utils.h"
#include "util.h"


/***************************************************************************************/

BOOL parse_Bool(const char * pdata)
{
	if (strcasecmp(pdata, "true") == 0)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL parse_XSDDatetime(const char * s, time_t * p)
{
	if (s)
	{ 
		char zone[32];
		struct tm T;
		const char *t;
		
		*zone = '\0';
		memset(&T, 0, sizeof(T));
		
		if (strchr(s, '-'))
		{
			t = "%d-%d-%dT%d:%d:%d%31s";
		}	
		else if (strchr(s, ':'))
		{
			t = "%4d%2d%2dT%d:%d:%d%31s";
		}	
		else /* parse non-XSD-standard alternative ISO 8601 format */
		{
			t = "%4d%2d%2dT%2d%2d%2d%31s";
		}
		
		if (sscanf(s, t, &T.tm_year, &T.tm_mon, &T.tm_mday, &T.tm_hour, &T.tm_min, &T.tm_sec, zone) < 6)
		{
			return FALSE;
		}
		
		if (T.tm_year == 1)
		{
			T.tm_year = 70;
		}	
		else
		{
			T.tm_year -= 1900;
		}
		
		T.tm_mon--;
		
		if (*zone == '.')
		{ 
			for (s = zone + 1; *s; s++)
			{
				if (*s < '0' || *s > '9')
				{
					break;
				}	
			}	
		}
    	else
    	{
      		s = zone;
      	}
      	
		if (*s)
		{
			if (*s == '+' || *s == '-')
			{ 
				int h = 0, m = 0;
				if (s[3] == ':')
				{ 
					/* +hh:mm */
					sscanf(s, "%d:%d", &h, &m);
					if (h < 0)
						m = -m;
				}
				else /* +hhmm */
				{
					m = (int)strtol(s, NULL, 10);
					h = m / 100;
					m = m % 100;
				}
				
				T.tm_min -= m;
				T.tm_hour -= h;
				/* put hour and min in range */
				T.tm_hour += T.tm_min / 60;
				T.tm_min %= 60;
				
				if (T.tm_min < 0)
				{ 
					T.tm_min += 60;
					T.tm_hour--;
				}
				
				T.tm_mday += T.tm_hour / 24;
				T.tm_hour %= 24;
				
				if (T.tm_hour < 0)
				{
					T.tm_hour += 24;
					T.tm_mday--;
				}
				/* note: day of the month may be out of range, timegm() handles it */
			}

			*p = onvif_timegm(&T);
		}
		else /* no UTC or timezone, so assume we got a localtime */
		{ 
			T.tm_isdst = -1;
			*p = mktime(&T);
		}
	}
	
	return TRUE;
}

BOOL parse_XSDDuration(const char *s, int *a)
{ 
	int sign = 1, Y = 0, M = 0, D = 0, H = 0, N = 0, S = 0;
	float f = 0;
	*a = 0;
	if (s)
	{ 
		if (*s == '-')
		{ 
			sign = -1;
			s++;
		}
		if (*s++ != 'P')
			return FALSE;
			
		/* date part */
		while (s && *s)
		{ 
			int n;
			char k;
			if (*s == 'T')
			{ 
				s++;
				break;
			}
			
			if (sscanf(s, "%d%c", &n, &k) != 2)
				return FALSE;
				
			s = strchr(s, k);
			if (!s)
				return FALSE;
				
			switch (k)
			{ 
			case 'Y':
				Y = n;
				break;
				
			case 'M':
				M = n;
				break;
				
			case 'D':
				D = n;
				break;
				
			default:
				return FALSE;
			}
			
			s++;
		}
		
	    /* time part */
	    while (s && *s)
		{ 
			int n;
			char k;
			if (sscanf(s, "%d%c", &n, &k) != 2)
				return FALSE;
				
			s = strchr(s, k);
			if (!s)
				return FALSE;
				
			switch (k)
			{ 
			case 'H':
				H = n;
				break;
				
			case 'M':
				N = n;
				break;
				
			case '.':
				S = n;
				if (sscanf(s, "%g", &f) != 1)
					return FALSE;
				s = NULL;
				continue;
				
			case 'S':
				S = n;
				break;
				
			default:
				return FALSE;
			}
			
			s++;
		}
	    /* convert Y-M-D H:N:S.f to signed int */
	    *a = sign * ((((((((((Y * 12) + M) * 30) + D) * 24) + H) * 60) + N) * 60) + S);
	}

	return TRUE;
}


/***************************************************************************************/

BOOL parse_Vector(XMLN * p_node, onvif_Vector * p_req)
{
	const char * p_x;
	const char * p_y;
	const char * p_space;
	
	p_x = xml_attr_get(p_node, "x");
	if (p_x)
	{			
		p_req->x = (float)atof(p_x);
	}

	p_y = xml_attr_get(p_node, "y");
	if (p_y)
	{
		p_req->y = (float)atof(p_y);
	}

	p_space = xml_attr_get(p_node, "space");
	if (p_space)
	{
		p_req->spaceFlag = 1;
	    strncpy(p_req->space, p_space, sizeof(p_req->space)-1);
	}

	return TRUE;
}

BOOL parse_Vector1D(XMLN * p_node, onvif_Vector1D * p_req)
{
	const char * p_x;
	const char * p_space;
	
	p_x = xml_attr_get(p_node, "x");
	if (p_x)
	{			
		p_req->x = (float)atof(p_x);
	}

	p_space = xml_attr_get(p_node, "space");
	if (p_space)
	{
		p_req->spaceFlag = 1;
	    strncpy(p_req->space, p_space, sizeof(p_req->space)-1);
	}

	return TRUE;
}

BOOL parse_IntList(XMLN * p_node, onvif_IntList * p_req)
{
	XMLN * p_Items;

    p_req->sizeItems = 0;
    
	p_Items = xml_node_soap_get(p_node, "Items");
    while (p_Items && p_Items->data && soap_strcmp(p_Items->name, "Items") == 0)
    {
    	int idx = p_req->sizeItems;

    	p_req->Items[idx] = atoi(p_Items->data);

    	p_req->sizeItems++;
    	if (p_req->sizeItems >= ARRAY_SIZE(p_req->Items))
    	{
    		break;
    	}

		p_Items = p_Items->next;
    }

    return TRUE;
}

BOOL parse_FloatList(XMLN * p_node, onvif_FloatList * p_req)
{
	XMLN * p_Items;

    p_req->sizeItems = 0;
    
	p_Items = xml_node_soap_get(p_node, "Items");
    while (p_Items && p_Items->data && soap_strcmp(p_Items->name, "Items") == 0)
    {
    	int idx = p_req->sizeItems;

    	p_req->Items[idx] = (float)atof(p_Items->data);

    	p_req->sizeItems++;
    	if (p_req->sizeItems >= ARRAY_SIZE(p_req->Items))
    	{
    		break;
    	}

		p_Items = p_Items->next;
    }

    return TRUE;
}

BOOL parse_IntRange(XMLN * p_node, onvif_IntRange * p_req)
{
	XMLN * p_Min;
	XMLN * p_Max;

	p_Min = xml_node_soap_get(p_node, "Min");
	if (p_Min && p_Min->data)
	{
		p_req->Min = atoi(p_Min->data);
	}

	p_Max = xml_node_soap_get(p_node, "Max");
	if (p_Max && p_Max->data)
	{
		p_req->Max = atoi(p_Max->data);
	}

	return TRUE;
}

BOOL parse_FloatRange(XMLN * p_node, onvif_FloatRange * p_req)
{
	XMLN * p_Min;
	XMLN * p_Max;

	p_Min = xml_node_soap_get(p_node, "Min");
	if (p_Min && p_Min->data)
	{
		p_req->Min = (float)atof(p_Min->data);
	}
	
	p_Max = xml_node_soap_get(p_node, "Max");
	if (p_Max && p_Max->data)
	{
		p_req->Max = (float)atof(p_Max->data);
	}

	return TRUE;
}

BOOL parse_Resolution(XMLN * p_node, onvif_VideoResolution * p_req)
{
	XMLN * p_Width;
	XMLN * p_Height;

	p_Width = xml_node_soap_get(p_node, "Width");
    if (p_Width && p_Width->data)
    {
        p_req->Width = atoi(p_Width->data);
    }

    p_Height = xml_node_soap_get(p_node, "Height");
    if (p_Height && p_Height->data)
    {
        p_req->Height = atoi(p_Height->data);
    }

	return TRUE;
}

BOOL parse_SystemDateTime(XMLN * p_node, onvif_SystemDateTime * p_req)
{
    XMLN * p_DateTimeType;
	XMLN * p_DaylightSavings;
	XMLN * p_TimeZone;

	p_DateTimeType = xml_node_soap_get(p_node, "DateTimeType");
    if (p_DateTimeType && p_DateTimeType->data)
    {
        p_req->DateTimeType = onvif_StringToSetDateTimeType(p_DateTimeType->data);
    }    

    p_DaylightSavings = xml_node_soap_get(p_node, "DaylightSavings");
    if (p_DaylightSavings && p_DaylightSavings->data)
    {
        p_req->DaylightSavings = parse_Bool(p_DaylightSavings->data);
    }

    p_TimeZone = xml_node_soap_get(p_node, "TimeZone");
    if (p_TimeZone)
    {
    	XMLN * p_TZ;
		
    	p_req->TimeZoneFlag = 1;
    	
        p_TZ = xml_node_soap_get(p_TimeZone, "TZ");
		if (p_TZ && p_TZ->data)
		{
			strncpy(p_req->TimeZone.TZ, p_TZ->data, sizeof(p_req->TimeZone.TZ)-1);
		}
    }

    return TRUE;
}

ONVIF_RET prase_User(XMLN * p_node, onvif_User * p_req)
{
	XMLN * p_Username;
	XMLN * p_Password;
	XMLN * p_UserLevel;

	p_Username = xml_node_soap_get(p_node, "Username");
	if (p_Username && p_Username->data)
	{
		strncpy(p_req->Username, p_Username->data, sizeof(p_req->Username)-1);
	}

	p_Password = xml_node_soap_get(p_node, "Password");
	if (p_Password && p_Password->data)
	{
		if (strlen(p_Password->data) >= sizeof(p_req->Password))
		{
			return ONVIF_ERR_PasswordTooLong;
		}
		
		strncpy(p_req->Password, p_Password->data, sizeof(p_req->Password)-1);
	}

	p_UserLevel = xml_node_soap_get(p_node, "UserLevel");
	if (p_UserLevel && p_UserLevel->data)
	{
		p_req->UserLevel = onvif_StringToUserLevel(p_UserLevel->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_NetworkProtocols(XMLN * p_node, onvif_NetworkProtocol * p_req)
{
	char name[32];
	BOOL enable;
	int  port[MAX_SERVER_PORT];
	XMLN * p_NetworkProtocols;
	
	p_NetworkProtocols = xml_node_soap_get(p_node, "NetworkProtocols");
	while (p_NetworkProtocols && soap_strcmp(p_NetworkProtocols->name, "NetworkProtocols") == 0)
	{
		uint32 i = 0;
		XMLN * p_Name;
		XMLN * p_Enabled;
		XMLN * p_Port;
		
		enable = FALSE;
		memset(name, 0, sizeof(name));
		memset(port, 0, sizeof(int)*MAX_SERVER_PORT);
		
		p_Name = xml_node_soap_get(p_NetworkProtocols, "Name");
		if (p_Name && p_Name->data)
		{
			strncpy(name, p_Name->data, sizeof(name)-1);
		}

		p_Enabled = xml_node_soap_get(p_NetworkProtocols, "Enabled");
		if (p_Enabled && p_Enabled->data)
		{
			enable = parse_Bool(p_Enabled->data);
		}		
		
		p_Port = xml_node_soap_get(p_NetworkProtocols, "Port");
		while (p_Port && p_Port->data && soap_strcmp(p_Port->name, "Port") == 0)
		{
			if (i < ARRAY_SIZE(port))
			{
				port[i++] = atoi(p_Port->data);
			}
			
			p_Port = p_Port->next;
		}

		if (strcasecmp(name, "HTTP") == 0)
		{
			p_req->HTTPFlag = 1;
			p_req->HTTPEnabled = enable;
			memcpy(p_req->HTTPPort, port, sizeof(p_req->HTTPPort));
		}
		else if (strcasecmp(name, "HTTPS") == 0)
		{
			p_req->HTTPSFlag = 1;
			p_req->HTTPSEnabled = enable;
			memcpy(p_req->HTTPSPort, port, sizeof(p_req->HTTPSPort));
		}
		else if (strcasecmp(name, "RTSP") == 0)
		{
			p_req->RTSPFlag = 1;
			p_req->RTSPEnabled = enable;
			memcpy(p_req->RTSPPort, port, sizeof(p_req->RTSPPort));
		}
		else
		{
			return ONVIF_ERR_ServiceNotSupported;
		}

		p_NetworkProtocols = p_NetworkProtocols->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_DNSInformation(XMLN * p_node, onvif_DNSInformation * p_req)
{
	uint32 i = 0;
	XMLN * p_FromDHCP;
	XMLN * p_SearchDomain;
	XMLN * p_DNSManual;
	
	assert(p_node);

	p_FromDHCP = xml_node_soap_get(p_node, "FromDHCP");
	if (p_FromDHCP && p_FromDHCP->data)
	{
		p_req->FromDHCP = parse_Bool(p_FromDHCP->data);
	}	
	
	p_SearchDomain = xml_node_soap_get(p_node, "SearchDomain");
	while (p_SearchDomain && soap_strcmp(p_SearchDomain->name, "SearchDomain") == 0)
	{
		p_req->SearchDomainFlag = 1;
		
		if (p_SearchDomain->data && i < ARRAY_SIZE(p_req->SearchDomain))
		{
			strncpy(p_req->SearchDomain[i], p_SearchDomain->data, sizeof(p_req->SearchDomain[i]) - 1);
			++ i;
		}

		p_SearchDomain = p_SearchDomain->next;
	}

	i = 0;
	
	p_DNSManual = xml_node_soap_get(p_node, "DNSManual");
	while (p_DNSManual && soap_strcmp(p_DNSManual->name, "DNSManual") == 0)
	{
		XMLN * p_Type;
		XMLN * p_IPv4Address;
		
		p_Type = xml_node_soap_get(p_DNSManual, "Type");
		if (p_Type && p_Type->data)
		{
			if (strcasecmp(p_Type->data, "IPv4") != 0) // todo : now only support ipv4
			{
				p_DNSManual = p_DNSManual->next;
				continue;
			}
		}

		p_IPv4Address = xml_node_soap_get(p_DNSManual, "IPv4Address");
		if (p_IPv4Address && p_IPv4Address->data)
		{
			if (is_ip_address(p_IPv4Address->data) == FALSE)
			{
				return ONVIF_ERR_InvalidIPv4Address;
			}
			else if (i < ARRAY_SIZE(p_req->DNSServer))
			{
				strncpy(p_req->DNSServer[i], p_IPv4Address->data, sizeof(p_req->DNSServer[i])-1);
				++ i;
			}
		}
		
		p_DNSManual = p_DNSManual->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_DynamicDNSInformation(XMLN * p_node, onvif_DynamicDNSInformation * p_req)
{
    XMLN * p_Type;
    XMLN * p_Name;
    XMLN * p_TTL;

    p_Type = xml_node_soap_get(p_node, "Type");
	if (p_Type && p_Type->data)
	{
	    p_req->Type = onvif_StringToDynamicDNSType(p_Type->data);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
	    p_req->NameFlag = 1;
	    strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_TTL = xml_node_soap_get(p_node, "TTL");
	if (p_TTL && p_TTL->data)
	{
	    p_req->TTLFlag = 1;
	    p_req->TTL = atoi(p_TTL->data);
	}

    return ONVIF_OK;
}

ONVIF_RET parse_NTPInformation(XMLN * p_node, onvif_NTPInformation * p_req)
{
    uint32 i = 0;
	XMLN * p_FromDHCP;
	XMLN * p_NTPManual;

	p_FromDHCP = xml_node_soap_get(p_node, "FromDHCP");
	if (p_FromDHCP && p_FromDHCP->data)
	{
		p_req->FromDHCP = parse_Bool(p_FromDHCP->data);
	}	
	
	p_NTPManual = xml_node_soap_get(p_node, "NTPManual");
	while (p_NTPManual && soap_strcmp(p_NTPManual->name, "NTPManual") == 0)
	{
		XMLN * p_Type;
		XMLN * p_IPv4Address;
		XMLN * p_DNSname;
		
		p_Type = xml_node_soap_get(p_NTPManual, "Type");
		if (p_Type && p_Type->data)
		{
            if (strcasecmp(p_Type->data, "IPv6") == 0)
            {
                XMLN * p_IPV6Address = NULL;
                p_IPV6Address = xml_node_soap_get(p_NTPManual, "IPv6Address");
                if (p_IPV6Address && p_IPV6Address->data != NULL)
                {
      		        p_req->IsExistIPV6Address = TRUE;
                }
            }

			if (strcasecmp(p_Type->data, "IPv4") != 0 && strcasecmp(p_Type->data, "DNS") != 0) // todo : now only support ipv4
			{
				p_NTPManual = p_NTPManual->next;
				continue;
			}
		}

		p_IPv4Address = xml_node_soap_get(p_NTPManual, "IPv4Address");
		if (p_IPv4Address && p_IPv4Address->data)
		{
			if (is_ip_address(p_IPv4Address->data) == FALSE)
			{
				return ONVIF_ERR_InvalidIPv4Address;
			}
			else if (i < ARRAY_SIZE(p_req->NTPServer))
			{
				strncpy(p_req->NTPServer[i], p_IPv4Address->data, sizeof(p_req->NTPServer[i])-1);
				++i;
			}
		}

		p_DNSname = xml_node_soap_get(p_NTPManual, "DNSname");
		if (p_DNSname && p_DNSname->data)
		{
			if (i < ARRAY_SIZE(p_req->NTPServer))
			{
				strncpy(p_req->NTPServer[i], p_DNSname->data, sizeof(p_req->NTPServer[i])-1);
				++i;
			}
		}
		
		p_NTPManual = p_NTPManual->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_NetworkGateway(XMLN * p_node, onvif_NetworkGateway * p_req)
{
    uint32 i = 0;
	XMLN * p_IPv4Address;
	
	p_IPv4Address = xml_node_soap_get(p_node, "IPv4Address");
	while (p_IPv4Address && p_IPv4Address->data && soap_strcmp(p_IPv4Address->name, "IPv4Address") == 0)
	{
		if (is_ip_address(p_IPv4Address->data) == FALSE)
		{
			return ONVIF_ERR_InvalidIPv4Address;
		}

		if (i < ARRAY_SIZE(p_req->IPv4Address))
		{
			strncpy(p_req->IPv4Address[i++], p_IPv4Address->data, sizeof(p_req->IPv4Address[0])-1);
		}

		p_IPv4Address = p_IPv4Address->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_NetworkZeroConfiguration(XMLN * p_node, onvif_NetworkZeroConfiguration * p_req)
{
    XMLN * p_InterfaceToken;
    XMLN * p_Enabled;

    p_InterfaceToken = xml_node_soap_get(p_node, "InterfaceToken");
	if (p_InterfaceToken && p_InterfaceToken->data)
	{
	    strncpy(p_req->InterfaceToken, p_InterfaceToken->data, sizeof(p_req->InterfaceToken)-1);
	}

	p_Enabled = xml_node_soap_get(p_node, "Enabled");
	if (p_Enabled && p_Enabled->data)
	{
	    p_req->Enabled = parse_Bool(p_Enabled->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_StreamSetup(XMLN * p_node, onvif_StreamSetup * p_req)
{
	XMLN * p_Stream;
	XMLN * p_Transport;

	p_Stream = xml_node_soap_get(p_node, "Stream");
	if (p_Stream && p_Stream->data)
	{
		p_req->Stream = onvif_StringToStreamType(p_Stream->data);
		if (StreamType_Invalid == p_req->Stream)
		{
			return ONVIF_ERR_InvalidStreamSetup;
		}
	}

	p_Transport = xml_node_soap_get(p_node, "Transport");
	if (p_Transport)
	{
		XMLN * p_Protocol = xml_node_soap_get(p_Transport, "Protocol");
		if (p_Protocol && p_Protocol->data)
		{
			p_req->Transport.Protocol = onvif_StringToTransportProtocol(p_Protocol->data);
			if (TransportProtocol_Invalid == p_req->Transport.Protocol)
			{
				return ONVIF_ERR_InvalidStreamSetup;
			}
		}
	}

	return ONVIF_OK;
}

/***************************************************************************************/

ONVIF_RET parse_tds_GetCapabilities(XMLN * p_node, tds_GetCapabilities_REQ * p_req)
{
	XMLN * p_Category;

	p_Category = xml_node_soap_get(p_node, "Category");
	if (p_Category && p_Category->data)
	{
		p_req->Category = onvif_StringToCapabilityCategory(p_Category->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_GetServices(XMLN * p_node, tds_GetServices_REQ * p_req)
{
	XMLN * p_IncludeCapability;

	p_IncludeCapability = xml_node_soap_get(p_node, "IncludeCapability");
	if (p_IncludeCapability && p_IncludeCapability->data)
	{
		p_req->IncludeCapability = parse_Bool(p_IncludeCapability->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_GetSystemLog(XMLN * p_node, tds_GetSystemLog_REQ * p_req)
{
	XMLN * p_LogType;

	p_LogType = xml_node_soap_get(p_node, "LogType");
	if (p_LogType && p_LogType->data)
	{
		p_req->LogType = onvif_StringToSystemLogType(p_LogType->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetSystemDateAndTime(XMLN * p_node, tds_SetSystemDateAndTime_REQ * p_req)
{
	XMLN * p_DateTimeType;
	XMLN * p_DaylightSavings;
	XMLN * p_TimeZone;
	XMLN * p_UTCDateTime;

	p_DateTimeType = xml_node_soap_get(p_node, "DateTimeType");
	if (p_DateTimeType && p_DateTimeType->data)
	{
		p_req->SystemDateTime.DateTimeType = onvif_StringToSetDateTimeType(p_DateTimeType->data);
	}

	p_DaylightSavings = xml_node_soap_get(p_node, "DaylightSavings");
	if (p_DaylightSavings && p_DaylightSavings->data)
	{
		p_req->SystemDateTime.DaylightSavings = parse_Bool(p_DaylightSavings->data);
	}

	p_TimeZone = xml_node_soap_get(p_node, "TimeZone");
	if (p_TimeZone)
	{
		XMLN * p_TZ;
		
		p_req->SystemDateTime.TimeZoneFlag = 1;
		
		p_TZ = xml_node_soap_get(p_TimeZone, "TZ");
		if (p_TZ && p_TZ->data)
		{
			strncpy(p_req->SystemDateTime.TimeZone.TZ, p_TZ->data, sizeof(p_req->SystemDateTime.TimeZone.TZ)-1);
		}
	}

	p_UTCDateTime = xml_node_soap_get(p_node, "UTCDateTime");
	if (p_UTCDateTime)
	{
		XMLN * p_Time;
		XMLN * p_Hour;
		XMLN * p_Minute;
		XMLN * p_Second;
		XMLN * p_Date;
		XMLN * p_Year;
		XMLN * p_Month;
		XMLN * p_Day;
		
		p_req->UTCDateTimeFlag = 1;
		
		p_Time = xml_node_soap_get(p_UTCDateTime, "Time");
		if (!p_Time)
		{
			return ONVIF_ERR_MissingAttribute;
		}

		p_Hour = xml_node_soap_get(p_Time, "Hour");
		if (!p_Hour || !p_Hour->data)
		{
			return ONVIF_ERR_MissingAttribute;
		}
		p_req->UTCDateTime.Time.Hour = atoi(p_Hour->data);

		p_Minute = xml_node_soap_get(p_Time, "Minute");
		if (!p_Minute || !p_Minute->data)
		{
			return ONVIF_ERR_MissingAttribute;
		}
		p_req->UTCDateTime.Time.Minute = atoi(p_Minute->data);

		p_Second = xml_node_soap_get(p_Time, "Second");
		if (!p_Second || !p_Second->data)
		{
			return ONVIF_ERR_MissingAttribute;
		}
		p_req->UTCDateTime.Time.Second = atoi(p_Second->data);

		p_Date = xml_node_soap_get(p_UTCDateTime, "Date");
		if (!p_Date)
		{
			return ONVIF_ERR_MissingAttribute;
		}

		p_Year = xml_node_soap_get(p_Date, "Year");
		if (!p_Year || !p_Year->data)
		{
			return ONVIF_ERR_MissingAttribute;
		}
		p_req->UTCDateTime.Date.Year = atoi(p_Year->data);

		p_Month = xml_node_soap_get(p_Date, "Month");
		if (!p_Month || !p_Month->data)
		{
			return ONVIF_ERR_MissingAttribute;
		}
		p_req->UTCDateTime.Date.Month = atoi(p_Month->data);

		p_Day = xml_node_soap_get(p_Date, "Day");
		if (!p_Day || !p_Day->data)
		{
			return ONVIF_ERR_MissingAttribute;
		}
		p_req->UTCDateTime.Date.Day = atoi(p_Day->data);
	}    

	return ONVIF_OK;
}

ONVIF_RET parse_tds_AddScopes(XMLN * p_node, tds_AddScopes_REQ * p_req)
{
    uint32 i = 0;
	
	XMLN * p_ScopeItem = xml_node_soap_get(p_node, "ScopeItem");
	while (p_ScopeItem && p_ScopeItem->data && soap_strcmp(p_ScopeItem->name, "ScopeItem") == 0)
	{
		if (i < ARRAY_SIZE(p_req->ScopeItem))
		{
			strncpy(p_req->ScopeItem[i], p_ScopeItem->data, sizeof(p_req->ScopeItem[i])-1);

			++i;
		}
		else
		{
			return ONVIF_ERR_TooManyScopes;
		}
		
		p_ScopeItem = p_ScopeItem->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetScopes(XMLN * p_node, tds_SetScopes_REQ * p_req)
{
    uint32 i = 0;
	
	XMLN * p_Scopes = xml_node_soap_get(p_node, "Scopes");
	while (p_Scopes && p_Scopes->data && soap_strcmp(p_Scopes->name, "Scopes") == 0)
	{
		if (i < ARRAY_SIZE(p_req->Scopes))
		{
			strncpy(p_req->Scopes[i], p_Scopes->data, sizeof(p_req->Scopes[i])-1);

			++i;
		}
		else
		{
			return ONVIF_ERR_TooManyScopes;
		}
		
		p_Scopes = p_Scopes->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_RemoveScopes(XMLN * p_node, tds_RemoveScopes_REQ * p_req)
{
    uint32 i = 0;
	
	XMLN * p_ScopeItem = xml_node_soap_get(p_node, "ScopeItem");
	while (p_ScopeItem && p_ScopeItem->data && soap_strcmp(p_ScopeItem->name, "ScopeItem") == 0)
	{
		if (i < ARRAY_SIZE(p_req->ScopeItem))
		{
			strncpy(p_req->ScopeItem[i], p_ScopeItem->data, sizeof(p_req->ScopeItem[i])-1);

			++i;
		}
		else
		{
			return ONVIF_ERR_TooManyScopes;
		}
		
		p_ScopeItem = p_ScopeItem->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetHostname(XMLN * p_node, tds_SetHostname_REQ * p_req)
{
    XMLN * p_Name;

    p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);	
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetHostnameFromDHCP(XMLN * p_node, tds_SetHostnameFromDHCP_REQ * p_req)
{
    XMLN * p_FromDHCP;

    p_FromDHCP = xml_node_soap_get(p_node, "FromDHCP");
	if (p_FromDHCP && p_FromDHCP->data)
	{
	    p_req->FromDHCP = parse_Bool(p_FromDHCP->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetDiscoveryMode(XMLN * p_node, tds_SetDiscoveryMode_REQ * p_req)
{
	XMLN * p_DiscoveryMode = xml_node_soap_get(p_node, "DiscoveryMode");
	if (p_DiscoveryMode && p_DiscoveryMode->data)
	{
		p_req->DiscoveryMode = onvif_StringToDiscoveryMode(p_DiscoveryMode->data);	
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetDNS(XMLN * p_node, tds_SetDNS_REQ * p_req)
{
	return parse_DNSInformation(p_node, &p_req->DNSInformation);
}

ONVIF_RET parse_tds_SetDynamicDNS(XMLN * p_node, tds_SetDynamicDNS_REQ * p_req)
{
	return parse_DynamicDNSInformation(p_node, &p_req->DynamicDNSInformation);
}

ONVIF_RET parse_tds_SetNTP(XMLN * p_node, tds_SetNTP_REQ * p_req)
{
	return parse_NTPInformation(p_node, &p_req->NTPInformation);
}

ONVIF_RET parse_tds_SetZeroConfiguration(XMLN * p_node, tds_SetZeroConfiguration_REQ * p_req)
{
	XMLN * p_InterfaceToken;
	XMLN * p_Enabled;

	p_InterfaceToken = xml_node_soap_get(p_node, "InterfaceToken");
	if (p_InterfaceToken && p_InterfaceToken->data)
	{
		strncpy(p_req->InterfaceToken, p_InterfaceToken->data, sizeof(p_req->InterfaceToken)-1);
	}

	p_Enabled = xml_node_soap_get(p_node, "Enabled");
	if (p_Enabled && p_Enabled->data)
	{
		p_req->Enabled = parse_Bool(p_Enabled->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetNetworkProtocols(XMLN * p_node, tds_SetNetworkProtocols_REQ * p_req)
{
	return parse_NetworkProtocols(p_node, &p_req->NetworkProtocol);
}

ONVIF_RET parse_tds_SetNetworkDefaultGateway(XMLN * p_node, tds_SetNetworkDefaultGateway_REQ * p_req)
{
	ONVIF_RET ret;
	onvif_NetworkGateway req;
	memset(&req, 0, sizeof(req));

	ret = parse_NetworkGateway(p_node, &req);
	if (ONVIF_OK == ret)
	{
		memcpy(p_req->IPv4Address, req.IPv4Address, sizeof(req.IPv4Address));
	}

	return ret;
}

ONVIF_RET parse_tds_SetNetworkInterfaces(XMLN * p_node, tds_SetNetworkInterfaces_REQ * p_req)
{
    XMLN * p_InterfaceToken;
	XMLN * p_NetworkInterface;

	p_InterfaceToken = xml_node_soap_get(p_node, "InterfaceToken");
	if (p_InterfaceToken && p_InterfaceToken->data)
	{
	    strncpy(p_req->NetworkInterface.token, p_InterfaceToken->data, sizeof(p_req->NetworkInterface.token)-1);
	}
	else
	{
	    return ONVIF_ERR_MissingAttribute;
	}

	p_NetworkInterface = xml_node_soap_get(p_node, "NetworkInterface");
	if (p_NetworkInterface)
	{
		XMLN * p_Enabled;
		XMLN * p_MTU;
		XMLN * p_IPv4;
		XMLN * p_Extension;
		
	    p_req->NetworkInterface.Enabled = TRUE;
	    
	    p_Enabled = xml_node_soap_get(p_NetworkInterface, "Enabled");
	    if (p_Enabled && p_Enabled->data)
	    {
	        p_req->NetworkInterface.Enabled = parse_Bool(p_Enabled->data);
	    }

	    p_MTU = xml_node_soap_get(p_NetworkInterface, "MTU");
	    if (p_MTU && p_MTU->data)
	    {
	    	p_req->NetworkInterface.InfoFlag = 1;
	    	p_req->NetworkInterface.Info.MTUFlag = 1;
	        p_req->NetworkInterface.Info.MTU = atoi(p_MTU->data);
	    }

	    p_IPv4 = xml_node_soap_get(p_NetworkInterface, "IPv4");
	    if (p_IPv4)
	    {
	    	XMLN * p_Enabled;
			XMLN * p_DHCP;
			
	        p_req->NetworkInterface.IPv4Flag = 1;
	        
	        p_Enabled = xml_node_soap_get(p_IPv4, "Enabled");
	        if (p_Enabled && p_Enabled->data)
    	    {
    	        p_req->NetworkInterface.IPv4.Enabled = parse_Bool(p_Enabled->data);
    	    }

    	    p_DHCP = xml_node_soap_get(p_IPv4, "DHCP");
	        if (p_DHCP && p_DHCP->data)
	        {
	            p_req->NetworkInterface.IPv4.Config.DHCP = parse_Bool(p_DHCP->data);
	        }

	        if (p_req->NetworkInterface.IPv4.Config.DHCP == FALSE)
	        {
	            XMLN * p_Manual = xml_node_soap_get(p_IPv4, "Manual");
	            if (p_Manual)
	            {
	                XMLN * p_Address;
					XMLN * p_PrefixLength;

					p_Address = xml_node_soap_get(p_Manual, "Address");
	                if (p_Address && p_Address->data)
	                {
	                    strncpy(p_req->NetworkInterface.IPv4.Config.Address, p_Address->data, sizeof(p_req->NetworkInterface.IPv4.Config.Address)-1);
	                }

	                p_PrefixLength = xml_node_soap_get(p_Manual, "PrefixLength");
	                if (p_PrefixLength && p_PrefixLength->data)
	                {
	                    p_req->NetworkInterface.IPv4.Config.PrefixLength = atoi(p_PrefixLength->data);
	                }
	            }
	        }
	    }

	    p_Extension = xml_node_soap_get(p_NetworkInterface, "Extension");
	    if (p_Extension)
	    {
	        XMLN * p_InterfaceType;
#ifdef DOT11_SUPPORT	        
	        XMLN * p_Dot11;
#endif

	        p_req->NetworkInterface.ExtensionFlag = 1;

	        p_InterfaceType = xml_node_soap_get(p_Extension, "InterfaceType");
            if (p_InterfaceType && p_InterfaceType->data)
            {
                p_req->NetworkInterface.Extension.InterfaceType = atoi(p_InterfaceType->data);
            }

#ifdef DOT11_SUPPORT
	        p_Dot11 = xml_node_soap_get(p_Extension, "Dot11");
	        while (p_Dot11 && soap_strcmp(p_Dot11->name, "Dot11") == 0)
	        {
	            int idx = p_req->NetworkInterface.Extension.sizeDot11;

                parse_Dot11Configuration(p_Dot11, &p_req->NetworkInterface.Extension.Dot11[idx]);
                
	            p_req->NetworkInterface.Extension.sizeDot11++;
	            if (p_req->NetworkInterface.Extension.sizeDot11 >= ARRAY_SIZE(p_req->NetworkInterface.Extension.Dot11))
	            {
	                break;
	            }

	            p_Dot11 = p_Dot11->next;
	        }
#endif
	    }
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetSystemFactoryDefault(XMLN * p_node, tds_SetSystemFactoryDefault_REQ * p_req)
{
    XMLN * p_FactoryDefault;

    p_FactoryDefault = xml_node_soap_get(p_node, "FactoryDefault");
    if (p_FactoryDefault && p_FactoryDefault->data)
    {
    	p_req->FactoryDefault = onvif_StringToFactoryDefaultType(p_FactoryDefault->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tds_CreateUsers(XMLN * p_node, tds_CreateUsers_REQ * p_req)
{
	uint32 i = 0;
	ONVIF_RET ret;
	
	XMLN * p_User = xml_node_soap_get(p_node, "User");
	while (p_User)
	{
		if (i < ARRAY_SIZE(p_req->User))
		{
			ret = prase_User(p_User, &p_req->User[i]);
			if (ONVIF_OK != ret)
			{
				return ret;
			}
			++ i;
		}
		else
		{
			return ONVIF_ERR_TooManyUsers;
		}
		p_User = p_User->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_DeleteUsers(XMLN * p_node, tds_DeleteUsers_REQ * p_req)
{
    uint32 i = 0;
	
	XMLN * p_Username = xml_node_soap_get(p_node, "Username");
	while (p_Username)
	{
		if (i < ARRAY_SIZE(p_req->Username))
		{
			strncpy(p_req->Username[i], p_Username->data, sizeof(p_req->Username[i])-1);

			++i;
		}
		else
		{
			break;
		}
		
		p_Username = p_Username->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetUser(XMLN * p_node, tds_SetUser_REQ * p_req)
{
    uint32 i = 0;
	ONVIF_RET ret;
	
	XMLN * p_User = xml_node_soap_get(p_node, "User");
	while (p_User)
	{
		if (i < ARRAY_SIZE(p_req->User))
		{
		    ret = prase_User(p_User, &p_req->User[i]);
			if (ONVIF_OK != ret)
			{
			    return ret;
			}

			++i;
		}
		else
		{
			return ONVIF_ERR_TooManyUsers;
		}
		
		p_User = p_User->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetRemoteUser(XMLN * p_node, tds_SetRemoteUser_REQ * p_req)
{
	XMLN * p_RemoteUser = xml_node_soap_get(p_node, "RemoteUser");
	if (p_RemoteUser)
	{
		XMLN * p_Username;
		XMLN * p_Password;
		XMLN * p_UseDerivedPassword;

		p_req->RemoteUserFlag = 1;

		p_Username = xml_node_soap_get(p_RemoteUser, "Username");
    	if (p_Username && p_Username->data)
    	{
    	    strncpy(p_req->RemoteUser.Username, p_Username->data, sizeof(p_req->RemoteUser.Username)-1);
    	}

    	p_Password = xml_node_soap_get(p_RemoteUser, "Password");
    	if (p_Password && p_Password->data)
    	{
    	    p_req->RemoteUser.PasswordFlag = 1;
    	    strncpy(p_req->RemoteUser.Password, p_Password->data, sizeof(p_req->RemoteUser.Password)-1);
    	}

    	p_UseDerivedPassword = xml_node_soap_get(p_RemoteUser, "UseDerivedPassword");
    	if (p_UseDerivedPassword && p_UseDerivedPassword->data)
    	{
    	    p_req->RemoteUser.UseDerivedPassword = parse_Bool(p_UseDerivedPassword->data);
    	}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetHashingAlgorithm(XMLN * p_node, tds_SetHashingAlgorithm_REQ * p_req)
{
    XMLN * p_Algorithm;
    
    p_Algorithm = xml_node_soap_get(p_node, "Algorithm");
    if (p_Algorithm && p_Algorithm->data)
    {
        strncpy(p_req->Algorithm, p_Algorithm->data, sizeof(p_req->Algorithm)-1);
    }

    return ONVIF_OK;
}

#ifdef DEVICEIO_SUPPORT

ONVIF_RET parse_tds_SetRelayOutputSettings(XMLN * p_node, tmd_SetRelayOutputSettings_REQ * p_req)
{
    XMLN * p_RelayOutputToken;
    XMLN * p_Properties;

    p_RelayOutputToken = xml_node_soap_get(p_node, "RelayOutputToken");
    if (p_RelayOutputToken && p_RelayOutputToken->data)
    {
        strncpy(p_req->RelayOutput.token, p_RelayOutputToken->data, sizeof(p_req->RelayOutput.token)-1);
    }    

    p_Properties = xml_node_soap_get(p_node, "Properties");
    if (p_Properties)
    {    
        parse_RelayOutputSettings(p_Properties, &p_req->RelayOutput.Properties); 
    }

    return ONVIF_OK;
}

#endif // DEVICEIO_SUPPORT

#ifdef IPFILTER_SUPPORT

BOOL parse_PrefixedIPAddress(XMLN * p_node, onvif_PrefixedIPAddress * p_req)
{
    XMLN * p_Address;
    XMLN * p_PrefixLength;

    p_Address = xml_node_soap_get(p_node, "Address");
	if (p_Address && p_Address->data)
	{
	    strncpy(p_req->Address, p_Address->data, sizeof(p_req->Address)-1);
	}

	p_PrefixLength = xml_node_soap_get(p_node, "PrefixLength");
	if (p_PrefixLength && p_PrefixLength->data)
	{
	    p_req->PrefixLength = atoi(p_PrefixLength->data);
	}

	return TRUE;
}

ONVIF_RET parse_IPAddressFilter(XMLN * p_node, onvif_IPAddressFilter * p_req)
{
    int idx = 0;
    XMLN * p_Type;
    XMLN * p_IPv4Address;
    XMLN * p_IPv6Address;

    p_Type = xml_node_soap_get(p_node, "Type");
	if (p_Type && p_Type->data)
	{
	    p_req->Type = onvif_StringToIPAddressFilterType(p_Type->data);
	}

    idx = 0;
    
	p_IPv4Address = xml_node_soap_get(p_node, "IPv4Address");
	while (p_IPv4Address && soap_strcmp(p_IPv4Address->name, "IPv4Address") == 0)
	{
	    parse_PrefixedIPAddress(p_IPv4Address, &p_req->IPv4Address[idx]);

        if (++idx >= ARRAY_SIZE(p_req->IPv4Address))
        {
            break;
        }
        
	    p_IPv4Address = p_IPv4Address->next;
	}

    idx = 0;
    
	p_IPv6Address = xml_node_soap_get(p_node, "IPv6Address");
	while (p_IPv6Address && soap_strcmp(p_IPv6Address->name, "IPv6Address") == 0)
	{
	    parse_PrefixedIPAddress(p_IPv6Address, &p_req->IPv6Address[idx]);

        if (++idx >= ARRAY_SIZE(p_req->IPv6Address))
        {
            break;
        }
        
	    p_IPv6Address = p_IPv6Address->next;
	}

    return ONVIF_OK;
}

ONVIF_RET parse_tds_SetIPAddressFilter(XMLN * p_node, tds_SetIPAddressFilter_REQ * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_IPAddressFilter;

    p_IPAddressFilter = xml_node_soap_get(p_node, "IPAddressFilter");
	if (p_IPAddressFilter)
	{
	    ret = parse_IPAddressFilter(p_IPAddressFilter, &p_req->IPAddressFilter);
	}

	return ret;
}

ONVIF_RET parse_tds_AddIPAddressFilter(XMLN * p_node, tds_AddIPAddressFilter_REQ * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_IPAddressFilter;

    p_IPAddressFilter = xml_node_soap_get(p_node, "IPAddressFilter");
	if (p_IPAddressFilter)
	{
	    ret = parse_IPAddressFilter(p_IPAddressFilter, &p_req->IPAddressFilter);
	}

	return ret;
}

ONVIF_RET parse_tds_RemoveIPAddressFilter(XMLN * p_node, tds_RemoveIPAddressFilter_REQ * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_IPAddressFilter;

    p_IPAddressFilter = xml_node_soap_get(p_node, "IPAddressFilter");
	if (p_IPAddressFilter)
	{
	    ret = parse_IPAddressFilter(p_IPAddressFilter, &p_req->IPAddressFilter);
	}

	return ret;
}

#endif // end of #ifdef IPFILTER_SUPPORT

#ifdef STORAGE_SUPPORT

ONVIF_RET parse_UserCredential(XMLN * p_node, onvif_UserCredential * p_req)
{
    XMLN * p_UserName;
    XMLN * p_Password;

    p_UserName = xml_node_soap_get(p_node, "UserName");
    if (p_UserName && p_UserName->data)
    {
        strncpy(p_req->UserName, p_UserName->data, sizeof(p_req->UserName)-1);
    }

    p_Password = xml_node_soap_get(p_node, "Password");
    if (p_Password && p_Password->data)
    {
        p_req->PasswordFlag = 1;
        strncpy(p_req->Password, p_Password->data, sizeof(p_req->Password)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_StorageConfigurationData(XMLN * p_node, onvif_StorageConfigurationData * p_req)
{
    const char * p_type;
    XMLN * p_LocalPath;
    XMLN * p_StorageUri;
    XMLN * p_User;

    p_type = xml_attr_get(p_node, "type");
    if (p_type)
    {
        strncpy(p_req->type, p_type, sizeof(p_req->type)-1);
    }

    p_LocalPath = xml_node_soap_get(p_node, "LocalPath");
    if (p_LocalPath && p_LocalPath->data)
    {
        p_req->LocalPathFlag = 1;
        strncpy(p_req->LocalPath, p_LocalPath->data, sizeof(p_req->LocalPath)-1);
    }

    p_StorageUri = xml_node_soap_get(p_node, "StorageUri");
    if (p_StorageUri && p_StorageUri->data)
    {
        p_req->StorageUriFlag = 1;
        strncpy(p_req->StorageUri, p_StorageUri->data, sizeof(p_req->StorageUri)-1);
    }

    p_User = xml_node_soap_get(p_node, "User");
    if (p_User)
    {
        p_req->UserFlag = parse_UserCredential(p_User, &p_req->User);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_StorageConfiguration(XMLN * p_node, onvif_StorageConfiguration * p_req)
{
    const char * p_token;
    XMLN * p_Data;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Data = xml_node_soap_get(p_node, "Data");
    if (p_Data)
    {
        parse_StorageConfigurationData(p_Data, &p_req->Data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tds_GetStorageConfiguration(XMLN * p_node, tds_GetStorageConfiguration_REQ * p_req)
{
	XMLN * p_Token;

	p_Token = xml_node_soap_get(p_node, "Token");
	if (p_Token && p_Token->data)
	{
		strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_CreateStorageConfiguration(XMLN * p_node, tds_CreateStorageConfiguration_REQ * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_StorageConfiguration;

    p_StorageConfiguration = xml_node_soap_get(p_node, "StorageConfiguration");
	if (p_StorageConfiguration)
	{
	    ret = parse_StorageConfigurationData(p_StorageConfiguration, &p_req->StorageConfiguration);
	}

	return ret;
}

ONVIF_RET parse_tds_SetStorageConfiguration(XMLN * p_node, tds_SetStorageConfiguration_REQ * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_StorageConfiguration;

    p_StorageConfiguration = xml_node_soap_get(p_node, "StorageConfiguration");
	if (p_StorageConfiguration)
	{
	    ret = parse_StorageConfiguration(p_StorageConfiguration, &p_req->StorageConfiguration);
	}

	return ret;
}

ONVIF_RET parse_tds_DeleteStorageConfiguration(XMLN * p_node, tds_DeleteStorageConfiguration_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
	if (p_Token && p_Token->data)
	{
	    strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
	}

	return ONVIF_OK;
}

#endif // STORAGE_SUPPORT

#ifdef GEOLOCATION_SUPPORT

ONVIF_RET parse_LocationEntity(XMLN * p_node, onvif_LocationEntity * p_req)
{
    const char * p_Entity;
    const char * p_Token;
    const char * p_Fixed;
    const char * p_GeoSource;
    const char * p_AutoGeo;
    XMLN * p_GeoLocation;
    XMLN * p_GeoOrientation;
    XMLN * p_LocalLocation;
    XMLN * p_LocalOrientation;

    p_Entity = xml_attr_get(p_node, "Entity");
    if (p_Entity)
    {
        p_req->EntityFlag = 1;
        strncpy(p_req->Entity, p_Entity, sizeof(p_req->Entity)-1);
    }

    p_Token = xml_attr_get(p_node, "Token");
    if (p_Token)
    {
        p_req->TokenFlag = 1;
        strncpy(p_req->Token, p_Token, sizeof(p_req->Token)-1);
    }

    p_Fixed = xml_attr_get(p_node, "Fixed");
    if (p_Fixed)
    {
        p_req->FixedFlag = 1;
        p_req->Fixed = parse_Bool(p_Fixed);
    }

    p_GeoSource = xml_attr_get(p_node, "GeoSource");
    if (p_GeoSource)
    {
        p_req->GeoSourceFlag = 1;
        strncpy(p_req->GeoSource, p_GeoSource, sizeof(p_req->GeoSource)-1);
    }
    
    p_AutoGeo = xml_attr_get(p_node, "AutoGeo");
    if (p_AutoGeo)
    {
        p_req->AutoGeoFlag = 1;
        p_req->AutoGeo = parse_Bool(p_AutoGeo);
    }

    p_GeoLocation = xml_node_soap_get(p_node, "GeoLocation");
	if (p_GeoLocation)
	{
	    const char * p_lon;
        const char * p_lat;
        const char * p_elevation;

        p_req->GeoLocationFlag = 1;
        
        p_lon = xml_attr_get(p_GeoLocation, "lon");
        if (p_lon)
        {
            p_req->GeoLocation.lonFlag = 1;
            p_req->GeoLocation.lon = atof(p_lon);
        }

        p_lat = xml_attr_get(p_GeoLocation, "lat");
        if (p_lat)
        {
            p_req->GeoLocation.latFlag = 1;
            p_req->GeoLocation.lat = atof(p_lat);
        }

        p_elevation = xml_attr_get(p_GeoLocation, "elevation");
        if (p_elevation)
        {
            p_req->GeoLocation.elevationFlag = 1;
            p_req->GeoLocation.elevation = (float)atof(p_elevation);
        }
	}

	p_GeoOrientation = xml_node_soap_get(p_node, "GeoOrientation");
	if (p_GeoOrientation)
	{
	    const char * p_roll;
        const char * p_pitch;
        const char * p_yaw;

        p_req->GeoOrientationFlag = 1;
        
        p_roll = xml_attr_get(p_GeoOrientation, "roll");
        if (p_roll)
        {
            p_req->GeoOrientation.rollFlag = 1;
            p_req->GeoOrientation.roll = (float)atof(p_roll);
        }

        p_pitch = xml_attr_get(p_GeoOrientation, "pitch");
        if (p_pitch)
        {
            p_req->GeoOrientation.pitchFlag = 1;
            p_req->GeoOrientation.pitch = (float)atof(p_pitch);
        }

        p_yaw = xml_attr_get(p_GeoOrientation, "yaw");
        if (p_yaw)
        {
            p_req->GeoOrientation.yawFlag = 1;
            p_req->GeoOrientation.yaw = (float)atof(p_yaw);
        }
	}

	p_LocalLocation = xml_node_soap_get(p_node, "LocalLocation");
	if (p_LocalLocation)
	{
	    const char * p_x;
        const char * p_y;
        const char * p_z;

        p_req->LocalLocationFlag = 1;
        
        p_x = xml_attr_get(p_LocalLocation, "x");
        if (p_x)
        {
            p_req->LocalLocation.xFlag = 1;
            p_req->LocalLocation.x = (float)atof(p_x);
        }

        p_y = xml_attr_get(p_LocalLocation, "y");
        if (p_y)
        {
            p_req->LocalLocation.yFlag = 1;
            p_req->LocalLocation.y = (float)atof(p_y);
        }

        p_z = xml_attr_get(p_LocalLocation, "z");
        if (p_z)
        {
            p_req->LocalLocation.zFlag = 1;
            p_req->LocalLocation.z = (float)atof(p_z);
        }
	}

    p_LocalOrientation = xml_node_soap_get(p_node, "LocalOrientation");
	if (p_LocalOrientation)
	{
	    const char * p_pan;
        const char * p_tilt;
        const char * p_roll;

        p_req->LocalOrientationFlag = 1;
        
        p_pan = xml_attr_get(p_LocalOrientation, "pan");
        if (p_pan)
        {
            p_req->LocalOrientation.panFlag = 1;
            p_req->LocalOrientation.pan = (float)atof(p_pan);
        }

        p_tilt = xml_attr_get(p_LocalOrientation, "tilt");
        if (p_tilt)
        {
            p_req->LocalOrientation.tiltFlag = 1;
            p_req->LocalOrientation.tilt = (float)atof(p_tilt);
        }

        p_roll = xml_attr_get(p_LocalOrientation, "roll");
        if (p_roll)
        {
            p_req->LocalOrientation.rollFlag = 1;
            p_req->LocalOrientation.roll = (float)atof(p_roll);
        }
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tds_SetGeoLocation(XMLN * p_node, tds_SetGeoLocation_REQ * p_req)
{
    XMLN * p_Location;

    p_Location = xml_node_soap_get(p_node, "Location");
	while (p_Location && soap_strcmp(p_Location->name, "Location") == 0)
	{
        LocationEntityList * p_info = onvif_add_LocationEntity(&p_req->Location);
        if (p_info)
        {
            parse_LocationEntity(p_Location, &p_info->Location);
        }
	    
	    p_Location = p_Location->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_DeleteGeoLocation(XMLN * p_node, tds_DeleteGeoLocation_REQ * p_req)
{
    XMLN * p_Location;

    p_Location = xml_node_soap_get(p_node, "Location");
	while (p_Location && soap_strcmp(p_Location->name, "Location") == 0)
	{
        LocationEntityList * p_info = onvif_add_LocationEntity(&p_req->Location);
        if (p_info)
        {
            parse_LocationEntity(p_Location, &p_info->Location);
        }
	    
	    p_Location = p_Location->next;
	}

	return ONVIF_OK;
}

#endif // GEOLOCATION_SUPPORT

#ifdef DOT11_SUPPORT

BOOL parse_Dot11Configuration(XMLN * p_node, onvif_Dot11Configuration * p_req)
{
    XMLN * p_SSID;
    XMLN * p_Mode;
    XMLN * p_Alias;
    XMLN * p_Priority;
    XMLN * p_Security;

    p_SSID = xml_node_soap_get(p_node, "SSID");
	if (p_SSID && p_SSID->data)
	{
	    strncpy(p_req->SSID, p_SSID->data, sizeof(p_req->SSID)-1);
	}

	p_Mode = xml_node_soap_get(p_node, "Mode");
	if (p_Mode && p_Mode->data)
	{
	    p_req->Mode = onvif_StringToDot11StationMode(p_Mode->data);
	}

	p_Alias = xml_node_soap_get(p_node, "Alias");
	if (p_Alias && p_Alias->data)
	{
	    strncpy(p_req->Alias, p_Alias->data, sizeof(p_req->Alias)-1);
	}

	p_Priority = xml_node_soap_get(p_node, "Priority");
	if (p_Priority && p_Priority->data)
	{
	    p_req->Priority = atoi(p_Priority->data);
	}

	p_Security = xml_node_soap_get(p_node, "Security");
    if (p_Security)
    {
        XMLN * p_Mode;
        XMLN * p_Algorithm;
        XMLN * p_PSK;
        XMLN * p_Dot1X;

        p_Mode = xml_node_soap_get(p_Security, "Mode");
    	if (p_Mode && p_Mode->data)
    	{
    	    p_req->Security.Mode = onvif_StringToDot11SecurityMode(p_Mode->data);
    	}

    	p_Algorithm = xml_node_soap_get(p_Security, "Algorithm");
    	if (p_Algorithm && p_Algorithm->data)
    	{
    	    p_req->Security.AlgorithmFlag = 1;
    	    p_req->Security.Algorithm = onvif_StringToDot11Cipher(p_Algorithm->data);
    	}

    	p_PSK = xml_node_soap_get(p_Security, "PSK");
    	if (p_PSK)
    	{
    	    XMLN * p_Key;
            XMLN * p_Passphrase;

            p_req->Security.PSKFlag = 1;
            
            p_Key = xml_node_soap_get(p_PSK, "Key");
        	if (p_Key && p_Key->data)
        	{
        	    p_req->Security.PSK.KeyFlag = 1;
        	    strncpy(p_req->Security.PSK.Key, p_Key->data, sizeof(p_req->Security.PSK.Key)-1);
        	}

        	p_Passphrase = xml_node_soap_get(p_PSK, "Passphrase");
        	if (p_Passphrase && p_Passphrase->data)
        	{
        	    p_req->Security.PSK.PassphraseFlag = 1;
        	    strncpy(p_req->Security.PSK.Passphrase, p_Passphrase->data, sizeof(p_req->Security.PSK.Passphrase)-1);
        	}
    	}

    	p_Dot1X = xml_node_soap_get(p_Security, "Dot1X");
    	if (p_Dot1X && p_Dot1X->data)
    	{
    	    p_req->Security.Dot1XFlag = 1;
    	    strncpy(p_req->Security.Dot1X, p_Dot1X->data, sizeof(p_req->Security.Dot1X)-1);
    	}
    }

	return TRUE;
}

ONVIF_RET parse_tds_GetDot11Status(XMLN * p_node, tds_GetDot11Status_REQ * p_req)
{
    XMLN * p_InterfaceToken;

    p_InterfaceToken = xml_node_soap_get(p_node, "InterfaceToken");
	if (p_InterfaceToken && p_InterfaceToken->data)
	{
	    strncpy(p_req->InterfaceToken, p_InterfaceToken->data, sizeof(p_req->InterfaceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tds_ScanAvailableDot11Networks(XMLN * p_node, tds_ScanAvailableDot11Networks_REQ * p_req)
{
    XMLN * p_InterfaceToken;

    p_InterfaceToken = xml_node_soap_get(p_node, "InterfaceToken");
	if (p_InterfaceToken && p_InterfaceToken->data)
	{
	    strncpy(p_req->InterfaceToken, p_InterfaceToken->data, sizeof(p_req->InterfaceToken)-1);
	}

	return ONVIF_OK;
}

#endif // DOT11_SUPPORT

/***************************************************************************************/

ONVIF_RET parse_Filter(XMLN * p_node, ONVIF_FILTER * p_req)
{
    uint32 i = 0;
    XMLN * p_TopicExpression;
    XMLN * p_MessageContent;

    p_TopicExpression = xml_node_soap_get(p_node, "TopicExpression");
    while (p_TopicExpression && soap_strcmp(p_TopicExpression->name, "TopicExpression") == 0)
    {
        if (p_TopicExpression->data && i < ARRAY_SIZE(p_req->TopicExpression))
        {
            strncpy(p_req->TopicExpression[i], p_TopicExpression->data, sizeof(p_req->TopicExpression[i])-1);
            i++;
        }

        p_TopicExpression = p_TopicExpression->next;
    }

    i = 0;

    p_MessageContent = xml_node_soap_get(p_node, "MessageContent");
    while (p_MessageContent && soap_strcmp(p_MessageContent->name, "MessageContent") == 0)
    {
        if (p_MessageContent->data && i < ARRAY_SIZE(p_req->MessageContent))
        {
            strncpy(p_req->MessageContent[i], p_MessageContent->data, sizeof(p_req->MessageContent[i])-1);
            i++;
        }

        p_MessageContent = p_MessageContent->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tev_Subscribe(XMLN * p_node, tev_Subscribe_REQ * p_req)
{
	XMLN * p_ConsumerReference;
	XMLN * p_Address;
	XMLN * p_InitialTerminationTime;
	XMLN * p_Filter;
	
	p_ConsumerReference = xml_node_soap_get(p_node, "ConsumerReference");
	if (NULL == p_ConsumerReference)
	{
		return ONVIF_ERR_MissingAttribute;		
	}

	p_Address = xml_node_soap_get(p_ConsumerReference, "Address");
	if (p_Address && p_Address->data)
	{
		strncpy(p_req->ConsumerReference, p_Address->data, sizeof(p_req->ConsumerReference)-1);
	}	

	p_InitialTerminationTime = xml_node_soap_get(p_node, "InitialTerminationTime");
	if (p_InitialTerminationTime && p_InitialTerminationTime->data)
	{
		p_req->InitialTerminationTimeFlag = 1;
		parse_XSDDuration(p_InitialTerminationTime->data, &p_req->InitialTerminationTime);
	}

    p_Filter = xml_node_soap_get(p_node, "Filter");
	if (p_Filter)
	{
	    p_req->FiltersFlag = 1;
		parse_Filter(p_Filter, &p_req->Filters);
	}
    
	return ONVIF_OK;
}

ONVIF_RET parse_tev_Renew(XMLN * p_node, tev_Renew_REQ * p_req)
{
	XMLN * p_TerminationTime;
	
	p_TerminationTime = xml_node_soap_get(p_node, "TerminationTime");
	if (p_TerminationTime && p_TerminationTime->data)
	{
	    if (p_TerminationTime->data[0] == 'P' || 
	        (p_TerminationTime->data[0] == '-' && p_TerminationTime->data[1] == 'P'))
	    {
	        p_req->TerminationTimeType = 1;
	        
		    parse_XSDDuration(p_TerminationTime->data, (int *)&p_req->TerminationTime);
	    }
	    else
	    {
	        p_req->TerminationTimeType = 0;
	        
		    parse_XSDDatetime(p_TerminationTime->data, &p_req->TerminationTime);
		}    
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tev_CreatePullPointSubscription(XMLN * p_node, tev_CreatePullPointSubscription_REQ * p_req)
{
	XMLN * p_InitialTerminationTime;
	XMLN * p_Filter;

	p_InitialTerminationTime = xml_node_soap_get(p_node, "InitialTerminationTime");
	if (p_InitialTerminationTime && p_InitialTerminationTime->data)
	{
		p_req->InitialTerminationTimeFlag = 1;
		parse_XSDDuration(p_InitialTerminationTime->data, &p_req->InitialTerminationTime);
	}

    p_Filter = xml_node_soap_get(p_node, "Filter");
	if (p_Filter)
	{
	    p_req->FiltersFlag = 1;
		parse_Filter(p_Filter, &p_req->Filters);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tev_PullMessages(XMLN * p_node, tev_PullMessages_REQ * p_req)
{
	XMLN * p_Timeout;
	XMLN * p_MessageLimit;

	p_Timeout = xml_node_soap_get(p_node, "Timeout");
	if (p_Timeout && p_Timeout->data)
	{
		parse_XSDDuration(p_Timeout->data, &p_req->Timeout);
	}

	p_MessageLimit = xml_node_soap_get(p_node, "MessageLimit");
	if (p_MessageLimit && p_MessageLimit->data)
	{
		p_req->MessageLimit = atoi(p_MessageLimit->data);
	}

	return ONVIF_OK;
}

/***************************************************************************************/

#ifdef IMAGE_SUPPORT

ONVIF_RET parse_ImagingSettings(XMLN * p_node, onvif_ImagingSettings * p_req)
{
	XMLN * p_BacklightCompensation;
	XMLN * p_Brightness;
	XMLN * p_ColorSaturation;
	XMLN * p_Contrast;
	XMLN * p_Exposure;
	XMLN * p_Focus;
	XMLN * p_IrCutFilter;
	XMLN * p_Sharpness;
	XMLN * p_WideDynamicRange;
	XMLN * p_WhiteBalance;
	XMLN * p_Extension20;
	
	p_BacklightCompensation = xml_node_soap_get(p_node, "BacklightCompensation");
	if (p_BacklightCompensation)
	{
		XMLN * p_Mode;
		XMLN * p_Level;

		p_req->BacklightCompensationFlag = 1;
		
		p_Mode = xml_node_soap_get(p_BacklightCompensation, "Mode");
		if (p_Mode && p_Mode->data)
		{
			p_req->BacklightCompensation.Mode = onvif_StringToBacklightCompensationMode(p_Mode->data);
		}

		p_Level = xml_node_soap_get(p_BacklightCompensation, "Level");
		if (p_Level && p_Level->data)
		{
			p_req->BacklightCompensation.LevelFlag = 1;
			p_req->BacklightCompensation.Level = (float)atof(p_Level->data);
		}
	}

	p_Brightness = xml_node_soap_get(p_node, "Brightness");
	if (p_Brightness && p_Brightness->data)
	{
		p_req->BrightnessFlag = 1;
		p_req->Brightness = (float)atof(p_Brightness->data);
	}

	p_ColorSaturation = xml_node_soap_get(p_node, "ColorSaturation");
	if (p_ColorSaturation && p_ColorSaturation->data)
	{
		p_req->ColorSaturationFlag = 1;
		p_req->ColorSaturation = (float)atof(p_ColorSaturation->data);
	}

	p_Contrast = xml_node_soap_get(p_node, "Contrast");
	if (p_Contrast && p_Contrast->data)
	{
		p_req->ContrastFlag = 1;
		p_req->Contrast = (float)atof(p_Contrast->data);
	}

	p_Exposure = xml_node_soap_get(p_node, "Exposure");
	if (p_Exposure)
	{
		XMLN * p_Mode;
		XMLN * p_Priority;
		XMLN * p_Window;
		XMLN * p_MinExposureTime;
		XMLN * p_MaxExposureTime;
		XMLN * p_MinGain;
		XMLN * p_MaxGain;
		XMLN * p_MinIris;
		XMLN * p_MaxIris;
		XMLN * p_ExposureTime;
		XMLN * p_Gain;
		XMLN * p_Iris;

		p_req->ExposureFlag = 1;
		
		p_Mode = xml_node_soap_get(p_Exposure, "Mode");
		if (p_Mode && p_Mode->data)
		{
			p_req->Exposure.Mode = onvif_StringToExposureMode(p_Mode->data);
		}

		p_Priority = xml_node_soap_get(p_Exposure, "Priority");
		if (p_Priority && p_Priority->data)
		{
			p_req->Exposure.PriorityFlag = 1;
			p_req->Exposure.Priority = onvif_StringToExposurePriority(p_Priority->data);
		}

		p_Window = xml_node_soap_get(p_Exposure, "Window");
		if (p_Window)
		{
			const char * p_bottom;
			const char * p_top;
			const char * p_right;
			const char * p_left;

			p_bottom = xml_attr_get(p_Window, "bottom");
			if (p_bottom)
			{
				p_req->Exposure.Window.bottom = (float)atof(p_bottom);
			}

			p_top = xml_attr_get(p_Window, "top");
			if (p_top)
			{
				p_req->Exposure.Window.top = (float)atof(p_top);
			}

			p_right = xml_attr_get(p_Window, "right");
			if (p_right)
			{
				p_req->Exposure.Window.right = (float)atof(p_right);
			}

			p_left = xml_attr_get(p_Window, "left");
			if (p_left)
			{
				p_req->Exposure.Window.left = (float)atof(p_left);
			}
		}
		
		p_MinExposureTime = xml_node_soap_get(p_Exposure, "MinExposureTime");
		if (p_MinExposureTime && p_MinExposureTime->data)
		{
			p_req->Exposure.MinExposureTimeFlag = 1;
			p_req->Exposure.MinExposureTime = (float)atof(p_MinExposureTime->data);
		}

		p_MaxExposureTime = xml_node_soap_get(p_Exposure, "MaxExposureTime");
		if (p_MaxExposureTime && p_MaxExposureTime->data)
		{
			p_req->Exposure.MaxExposureTimeFlag = 1;
			p_req->Exposure.MaxExposureTime = (float)atof(p_MaxExposureTime->data);
		}

		p_MinGain = xml_node_soap_get(p_Exposure, "MinGain");
		if (p_MinGain && p_MinGain->data)
		{
			p_req->Exposure.MinGainFlag = 1;
			p_req->Exposure.MinGain = (float)atof(p_MinGain->data);
		}

		p_MaxGain = xml_node_soap_get(p_Exposure, "MaxGain");
		if (p_MaxGain && p_MaxGain->data)
		{
			p_req->Exposure.MaxGainFlag = 1;
			p_req->Exposure.MaxGain = (float)atof(p_MaxGain->data);
		}

		p_MinIris = xml_node_soap_get(p_Exposure, "MinIris");
		if (p_MinIris && p_MinIris->data)
		{
			p_req->Exposure.MinIrisFlag = 1;
			p_req->Exposure.MinIris = (float)atof(p_MinIris->data);
		}

		p_MaxIris = xml_node_soap_get(p_Exposure, "MaxIris");
		if (p_MaxIris && p_MaxIris->data)
		{
			p_req->Exposure.MaxIrisFlag = 1;
			p_req->Exposure.MaxIris = (float)atof(p_MaxIris->data);
		}

		p_ExposureTime = xml_node_soap_get(p_Exposure, "ExposureTime");
		if (p_ExposureTime && p_ExposureTime->data)
		{
			p_req->Exposure.ExposureTimeFlag = 1;
			p_req->Exposure.ExposureTime = (float)atof(p_ExposureTime->data);
		}

		p_Gain = xml_node_soap_get(p_Exposure, "Gain");
		if (p_Gain && p_Gain->data)
		{
			p_req->Exposure.GainFlag = 1;
			p_req->Exposure.Gain = (float)atof(p_Gain->data);
		}

		p_Iris = xml_node_soap_get(p_Exposure, "Iris");
		if (p_Iris && p_Iris->data)
		{
			p_req->Exposure.IrisFlag = 1;
			p_req->Exposure.Iris = (float)atof(p_Iris->data);
		}
	}

	p_Focus = xml_node_soap_get(p_node, "Focus");
	if (p_Focus)
	{
		XMLN * p_AutoFocusMode;
		XMLN * p_DefaultSpeed;
		XMLN * p_NearLimit;
		XMLN * p_FarLimit;

		p_req->FocusFlag = 1;
		
		p_AutoFocusMode = xml_node_soap_get(p_Focus, "AutoFocusMode");
		if (p_AutoFocusMode && p_AutoFocusMode->data)
		{
			p_req->Focus.AutoFocusMode = onvif_StringToAutoFocusMode(p_AutoFocusMode->data);
		}

		p_DefaultSpeed = xml_node_soap_get(p_Focus, "DefaultSpeed");
		if (p_DefaultSpeed && p_DefaultSpeed->data)
		{
			p_req->Focus.DefaultSpeedFlag = 1;
			p_req->Focus.DefaultSpeed = (float)atof(p_DefaultSpeed->data);
		}

		p_NearLimit = xml_node_soap_get(p_Focus, "NearLimit");
		if (p_NearLimit && p_NearLimit->data)
		{
			p_req->Focus.NearLimitFlag = 1;
			p_req->Focus.NearLimit = (float)atof(p_NearLimit->data);
		}

		p_FarLimit = xml_node_soap_get(p_Focus, "FarLimit");
		if (p_FarLimit && p_FarLimit->data)
		{
			p_req->Focus.FarLimitFlag = 1;
			p_req->Focus.FarLimit = (float)atof(p_FarLimit->data);
		}
	}
    
	p_IrCutFilter = xml_node_soap_get(p_node, "IrCutFilter");
	if (p_IrCutFilter && p_IrCutFilter->data)
	{
		p_req->IrCutFilterFlag = 1;
		p_req->IrCutFilter = onvif_StringToIrCutFilterMode(p_IrCutFilter->data);
	}

	p_Sharpness = xml_node_soap_get(p_node, "Sharpness");
	if (p_Sharpness && p_Sharpness->data)
	{
		p_req->SharpnessFlag = 1;
		p_req->Sharpness = (float)atof(p_Sharpness->data);
	}

	p_WideDynamicRange = xml_node_soap_get(p_node, "WideDynamicRange");
	if (p_WideDynamicRange)
	{
		XMLN * p_Mode;
		XMLN * p_Level;

		p_req->WideDynamicRangeFlag = 1;
		
		p_Mode = xml_node_soap_get(p_WideDynamicRange, "Mode");
		if (p_Mode && p_Mode->data)
		{
			p_req->WideDynamicRange.Mode = onvif_StringToWideDynamicMode(p_Mode->data);
		}

		p_Level = xml_node_soap_get(p_WideDynamicRange, "Level");
		if (p_Level && p_Level->data)
		{
			p_req->WideDynamicRange.LevelFlag = 1;
			p_req->WideDynamicRange.Level = (float)atof(p_Level->data);
		}
	}

	p_WhiteBalance = xml_node_soap_get(p_node, "WhiteBalance");
	if (p_WhiteBalance)
	{
		XMLN * p_Mode;
		XMLN * p_CrGain;
		XMLN * p_CbGain;

		p_req->WhiteBalanceFlag = 1;
		
		p_Mode = xml_node_soap_get(p_WhiteBalance, "Mode");
		if (p_Mode && p_Mode->data)
		{
			p_req->WhiteBalance.Mode = onvif_StringToWhiteBalanceMode(p_Mode->data);
		}

		p_CrGain = xml_node_soap_get(p_WhiteBalance, "CrGain");
		if (p_CrGain && p_CrGain->data)
		{
			p_req->WhiteBalance.CrGainFlag = 1;
			p_req->WhiteBalance.CrGain = (float)atof(p_CrGain->data);
		}

		p_CbGain = xml_node_soap_get(p_WhiteBalance, "CbGain");
		if (p_CbGain && p_CbGain->data)
		{
			p_req->WhiteBalance.CbGainFlag = 1;
			p_req->WhiteBalance.CbGain = (float)atof(p_CbGain->data);
		}
	}

	p_Extension20 = xml_node_soap_get(p_node, "Extension");
	if (p_Extension20)
	{
		XMLN * p_Extension202;
		
		p_req->ExtensionFlag = 1;

		p_Extension202 = xml_node_soap_get(p_Extension20, "Extension");
		if (p_Extension202)
		{
			XMLN * p_Extension203;
			
			p_req->Extension.Extension202Flag = 1;

			p_Extension203 = xml_node_soap_get(p_Extension202, "Extension");
			if (p_Extension203)
			{
				XMLN * p_Defogging;
				XMLN * p_NoiseReduction;
				
				p_req->Extension.Extension.Extension203Flag = 1;
				
				p_Defogging = xml_node_soap_get(p_Extension203, "Defogging");
				if (p_Defogging)
				{
					XMLN * p_Mode;
					XMLN * p_Level;
					
					p_req->Extension.Extension.Extension.DefoggingFlag = 1;
					
					p_Mode = xml_node_soap_get(p_Defogging, "Mode");
					if (p_Mode)
					{
						strcpy(p_req->Extension.Extension.Extension.Defogging.Mode, p_Mode->data);
					}
					
					p_Level = xml_node_soap_get(p_Defogging, "Level");
					if (p_Level)
					{
						p_req->Extension.Extension.Extension.Defogging.Level = atoi(p_Level->data);
					}
				}
				p_NoiseReduction = xml_node_soap_get(p_Extension203, "NoiseReduction");
				if (p_NoiseReduction)
				{
					XMLN * p_Level;
					
					p_req->Extension.Extension.Extension.NoiseReductionFlag = 1;
					
					p_Level = xml_node_soap_get(p_NoiseReduction, "Level");
					if (p_Level)
					{
						p_req->Extension.Extension.Extension.NoiseReduction.Level = atoi(p_Level->data);
					}
				}
			}
		}
	}
	log_print(HT_LOG_INFO, "parse_ImagingSettings BLC flag=%d mode=%s level=%.2f, "
		"Brightness flag=%d val=%.2f, ColorSat flag=%d val=%.2f, Contrast flag=%d val=%.2f, "
		"IrCut flag=%d mode=%s, Sharpness flag=%d val=%.2f\n",
		p_req->BacklightCompensationFlag,
		onvif_BacklightCompensationModeToString(p_req->BacklightCompensation.Mode),
		p_req->BacklightCompensation.Level,
		p_req->BrightnessFlag, p_req->Brightness,
		p_req->ColorSaturationFlag, p_req->ColorSaturation,
		p_req->ContrastFlag, p_req->Contrast,
		p_req->IrCutFilterFlag, onvif_IrCutFilterModeToString(p_req->IrCutFilter),
		p_req->SharpnessFlag, p_req->Sharpness);
	log_print(HT_LOG_INFO, "parse_ImagingSettings Exposure flag=%d mode=%s priority=%s "
		"win(l=%.2f t=%.2f r=%.2f b=%.2f) minET=%.2f maxET=%.2f minGain=%.2f maxGain=%.2f "
		"minIris=%.2f maxIris=%.2f ET=%.2f Gain=%.2f Iris=%.2f\n",
		p_req->ExposureFlag,
		onvif_ExposureModeToString(p_req->Exposure.Mode),
		onvif_ExposurePriorityToString(p_req->Exposure.Priority),
		p_req->Exposure.Window.left, p_req->Exposure.Window.top,
		p_req->Exposure.Window.right, p_req->Exposure.Window.bottom,
		p_req->Exposure.MinExposureTime, p_req->Exposure.MaxExposureTime,
		p_req->Exposure.MinGain, p_req->Exposure.MaxGain,
		p_req->Exposure.MinIris, p_req->Exposure.MaxIris,
		p_req->Exposure.ExposureTime, p_req->Exposure.Gain, p_req->Exposure.Iris);
	log_print(HT_LOG_INFO, "parse_ImagingSettings Focus flag=%d af=%s speed=%.2f near=%.2f far=%.2f, "
		"WDR flag=%d mode=%s level=%.2f, WB flag=%d mode=%s Cr=%.2f Cb=%.2f, "
		"Ext flag=%d defog=%s/%.2f nr=%.2f\n",
		p_req->FocusFlag,
		onvif_AutoFocusModeToString(p_req->Focus.AutoFocusMode),
		p_req->Focus.DefaultSpeed, p_req->Focus.NearLimit, p_req->Focus.FarLimit,
		p_req->WideDynamicRangeFlag,
		onvif_WideDynamicModeToString(p_req->WideDynamicRange.Mode),
		p_req->WideDynamicRange.Level,
		p_req->WhiteBalanceFlag,
		onvif_WhiteBalanceModeToString(p_req->WhiteBalance.Mode),
		p_req->WhiteBalance.CrGain, p_req->WhiteBalance.CbGain,
		p_req->ExtensionFlag,
		p_req->Extension.Extension.Extension.Defogging.Mode,
		p_req->Extension.Extension.Extension.Defogging.Level,
		p_req->Extension.Extension.Extension.NoiseReduction.Level);

	return ONVIF_OK;
}

ONVIF_RET parse_ImagingOptions(XMLN * p_node, onvif_ImagingOptions * p_req)
{
	XMLN * p_BacklightCompensation;
	XMLN * p_Brightness;
	XMLN * p_ColorSaturation;
	XMLN * p_Contrast;
	XMLN * p_Exposure;
	XMLN * p_Focus;
	XMLN * p_IrCutFilterModes;
	XMLN * p_Sharpness;
	XMLN * p_WideDynamicRange;
	XMLN * p_WhiteBalance;
	
	p_BacklightCompensation = xml_node_soap_get(p_node, "BacklightCompensation");
	if (p_BacklightCompensation)
	{
		XMLN * p_Mode;
		XMLN * p_Level;

		p_req->BacklightCompensationFlag = 1;
		
		p_Mode = xml_node_soap_get(p_BacklightCompensation, "Mode");
		while (p_Mode && p_Mode->data && soap_strcmp(p_Mode->name, "Mode") == 0)
		{
			if (strcasecmp(p_Mode->data, "OFF") == 0)
			{
				p_req->BacklightCompensation.Mode_OFF = 1;
			}
			else if (strcasecmp(p_Mode->data, "ON") == 0)
			{
				p_req->BacklightCompensation.Mode_ON = 1;
			}

			p_Mode = p_Mode->next;
		}

		p_Level = xml_node_soap_get(p_BacklightCompensation, "Level");
		if (p_Level)
		{
			p_req->BacklightCompensation.LevelFlag = 1;			
			parse_FloatRange(p_Level, &p_req->BacklightCompensation.Level);
		}
	}

	p_Brightness = xml_node_soap_get(p_node, "Brightness");
	if (p_Brightness)
	{
		p_req->BrightnessFlag = 1;
		parse_FloatRange(p_Brightness, &p_req->Brightness);
	}

	p_ColorSaturation = xml_node_soap_get(p_node, "ColorSaturation");
	if (p_ColorSaturation)
	{
		p_req->ColorSaturationFlag= 1;
		parse_FloatRange(p_ColorSaturation, &p_req->ColorSaturation);
	}

	p_Contrast = xml_node_soap_get(p_node, "Contrast");
	if (p_Contrast)
	{
		p_req->ContrastFlag = 1;
		parse_FloatRange(p_Contrast, &p_req->Contrast);
	}

	p_Exposure = xml_node_soap_get(p_node, "Exposure");
	if (p_Exposure)
	{
		XMLN * p_Mode;
		XMLN * p_Priority;
		XMLN * p_MinExposureTime;
		XMLN * p_MaxExposureTime;
		XMLN * p_MinGain;
		XMLN * p_MaxGain;
		XMLN * p_MinIris;
		XMLN * p_MaxIris;
		XMLN * p_ExposureTime;
		XMLN * p_Gain;
		XMLN * p_Iris;

		p_req->ExposureFlag = 1;
		
		p_Mode = xml_node_soap_get(p_Exposure, "Mode");
		while (p_Mode && p_Mode->data && soap_strcmp(p_Mode->name, "Mode") == 0)
		{
			if (strcasecmp(p_Mode->data, "Auto") == 0)
			{
				p_req->Exposure.Mode_AUTO = 1;
			}
			else if (strcasecmp(p_Mode->data, "Manual") == 0)
			{
				p_req->Exposure.Mode_MANUAL = 1;
			}

			p_Mode = p_Mode->next;
		}

		p_Priority = xml_node_soap_get(p_Exposure, "Priority");
		while (p_Priority && p_Priority->data && soap_strcmp(p_Priority->name, "Priority") == 0)
		{
			if (strcasecmp(p_Priority->data, "LowNoise") == 0)
			{
				p_req->Exposure.Priority_LowNoise = 1;
			}
			else if (strcasecmp(p_Priority->data, "FrameRate") == 0)
			{
				p_req->Exposure.Priority_FrameRate = 1;
			}

			p_Priority = p_Priority->next;
		}
		
		p_MinExposureTime = xml_node_soap_get(p_Exposure, "MinExposureTime");
		if (p_MinExposureTime)
		{
			p_req->Exposure.MinExposureTimeFlag = 1;
			parse_FloatRange(p_MinExposureTime, &p_req->Exposure.MinExposureTime);
		}

		p_MaxExposureTime = xml_node_soap_get(p_Exposure, "MaxExposureTime");
		if (p_MaxExposureTime)
		{
			p_req->Exposure.MaxExposureTimeFlag = 1;
			parse_FloatRange(p_MaxExposureTime, &p_req->Exposure.MaxExposureTime);
		}

		p_MinGain = xml_node_soap_get(p_Exposure, "MinGain");
		if (p_MinGain)
		{
			p_req->Exposure.MinGainFlag = 1;
			parse_FloatRange(p_MinGain, &p_req->Exposure.MinGain);
		}

		p_MaxGain = xml_node_soap_get(p_Exposure, "MaxGain");
		if (p_MaxGain)
		{
			p_req->Exposure.MaxGainFlag = 1;
			parse_FloatRange(p_MaxGain, &p_req->Exposure.MaxGain);
		}

		p_MinIris = xml_node_soap_get(p_Exposure, "MinIris");
		if (p_MinIris)
		{
			p_req->Exposure.MinIrisFlag = 1;
			parse_FloatRange(p_MinIris, &p_req->Exposure.MinIris);
		}

		p_MaxIris = xml_node_soap_get(p_Exposure, "MaxIris");
		if (p_MaxIris)
		{
			p_req->Exposure.MaxIrisFlag = 1;
			parse_FloatRange(p_MaxIris, &p_req->Exposure.MaxIris);
		}

		p_ExposureTime = xml_node_soap_get(p_Exposure, "ExposureTime");
		if (p_ExposureTime)
		{
			p_req->Exposure.ExposureTimeFlag = 1;
			parse_FloatRange(p_ExposureTime, &p_req->Exposure.ExposureTime);
		}

		p_Gain = xml_node_soap_get(p_Exposure, "Gain");
		if (p_Gain)
		{
			p_req->Exposure.GainFlag = 1;
			parse_FloatRange(p_Gain, &p_req->Exposure.Gain);
		}

		p_Iris = xml_node_soap_get(p_Exposure, "Iris");
		if (p_Iris)
		{
			p_req->Exposure.IrisFlag = 1;
			parse_FloatRange(p_Iris, &p_req->Exposure.Iris);
		}
	}

	p_Focus = xml_node_soap_get(p_node, "Focus");
	if (p_Focus)
	{
		XMLN * p_AutoFocusModes;
		XMLN * p_DefaultSpeed;
		XMLN * p_NearLimit;
		XMLN * p_FarLimit;
		
		p_req->FocusFlag = 1;
		
		p_AutoFocusModes = xml_node_soap_get(p_Focus, "AutoFocusModes");
		while (p_AutoFocusModes && p_AutoFocusModes->data && soap_strcmp(p_AutoFocusModes->name, "AutoFocusModes") == 0)
		{
			if (strcasecmp(p_AutoFocusModes->data, "Auto") == 0)
			{
				p_req->Focus.AutoFocusModes_AUTO = 1;
			}
			else if (strcasecmp(p_AutoFocusModes->data, "Manual") == 0)
			{
				p_req->Focus.AutoFocusModes_MANUAL = 1;
			}

			p_AutoFocusModes = p_AutoFocusModes->next;
		}

		p_DefaultSpeed = xml_node_soap_get(p_Focus, "DefaultSpeed");
		if (p_DefaultSpeed)
		{
			p_req->Focus.DefaultSpeedFlag = 1;
			parse_FloatRange(p_DefaultSpeed, &p_req->Focus.DefaultSpeed);
		}

		p_NearLimit = xml_node_soap_get(p_Focus, "NearLimit");
		if (p_NearLimit)
		{
			p_req->Focus.NearLimitFlag = 1;
			parse_FloatRange(p_NearLimit, &p_req->Focus.NearLimit);
		}

		p_FarLimit = xml_node_soap_get(p_Focus, "FarLimit");
		if (p_FarLimit)
		{
			p_req->Focus.FarLimitFlag = 1;
			parse_FloatRange(p_FarLimit, &p_req->Focus.FarLimit);
		}
	}
	
	p_IrCutFilterModes = xml_node_soap_get(p_node, "IrCutFilterModes");
	while (p_IrCutFilterModes && p_IrCutFilterModes->data && soap_strcmp(p_IrCutFilterModes->name, "IrCutFilterModes") == 0)
	{
		if (strcasecmp(p_IrCutFilterModes->data, "ON") == 0)
		{
			p_req->IrCutFilterMode_ON = 1;
		}
		else if (strcasecmp(p_IrCutFilterModes->data, "OFF") == 0)
		{
			p_req->IrCutFilterMode_OFF = 1;
		}
		else if (strcasecmp(p_IrCutFilterModes->data, "Auto") == 0)
		{
			p_req->IrCutFilterMode_AUTO = 1;
		}

		p_IrCutFilterModes = p_IrCutFilterModes->next;
	}

	p_Sharpness = xml_node_soap_get(p_node, "Sharpness");
	if (p_Sharpness)
	{
		p_req->SharpnessFlag = 1;
		parse_FloatRange(p_Sharpness, &p_req->Sharpness);
	}

	p_WideDynamicRange = xml_node_soap_get(p_node, "WideDynamicRange");
	if (p_WideDynamicRange)
	{
		XMLN * p_Mode;
		XMLN * p_Level;

		p_req->WideDynamicRangeFlag = 1;
		
		p_Mode = xml_node_soap_get(p_WideDynamicRange, "Mode");
		while (p_Mode && p_Mode->data && soap_strcmp(p_Mode->name, "Mode") == 0)
		{
			if (strcasecmp(p_Mode->data, "ON") == 0)
			{
				p_req->WideDynamicRange.Mode_ON = 1;
			}
			else if (strcasecmp(p_Mode->data, "OFF") == 0)
			{
				p_req->WideDynamicRange.Mode_OFF = 1;
			}

			p_Mode = p_Mode->next;
		}

		p_Level = xml_node_soap_get(p_WideDynamicRange, "Level");
		if (p_Level)
		{
			p_req->WideDynamicRange.LevelFlag = 1;
			parse_FloatRange(p_Level, &p_req->WideDynamicRange.Level);
		}
	}

	p_WhiteBalance = xml_node_soap_get(p_node, "WhiteBalance");
	if (p_WhiteBalance)
	{
		XMLN * p_Mode;
		XMLN * p_YrGain;
		XMLN * p_YbGain;

		p_req->WhiteBalanceFlag = 1;
		
		p_Mode = xml_node_soap_get(p_WhiteBalance, "Mode");
		while (p_Mode && p_Mode->data && soap_strcmp(p_Mode->name, "Mode") == 0)
		{
			if (strcasecmp(p_Mode->data, "Auto") == 0)
			{
				p_req->WhiteBalance.Mode_AUTO = 1;
			}
			else if (strcasecmp(p_Mode->data, "Manual") == 0)
			{
				p_req->WhiteBalance.Mode_MANUAL = 1;
			}

			p_Mode = p_Mode->next;
		}
		
		p_YrGain = xml_node_soap_get(p_WhiteBalance, "YrGain");
		if (p_YrGain)
		{
			p_req->WhiteBalance.YrGainFlag = 1;
			parse_FloatRange(p_YrGain, &p_req->WhiteBalance.YrGain);
		}

		p_YbGain = xml_node_soap_get(p_WhiteBalance, "YbGain");
		if (p_YbGain)
		{
			p_req->WhiteBalance.YbGainFlag = 1;
			parse_FloatRange(p_YrGain, &p_req->WhiteBalance.YbGain);
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_img_GetImagingSettings(XMLN * p_node, img_GetImagingSettings_REQ * p_req)
{
	XMLN * p_VideoSourceToken;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_img_SetImagingSettings(XMLN * p_node, img_SetImagingSettings_REQ * p_req)
{
	XMLN * p_VideoSourceToken;
	XMLN * p_ImagingSettings;
	XMLN * p_ForcePersistence;
	
	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}

	p_ImagingSettings = xml_node_soap_get(p_node, "ImagingSettings");
	if (p_ImagingSettings)
	{
		parse_ImagingSettings(p_ImagingSettings, &p_req->ImagingSettings);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}

	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistenceFlag = 1;
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}
    
	return ONVIF_OK;
}

ONVIF_RET parse_img_GetOptions(XMLN * p_node, img_GetOptions_REQ * p_req)
{
	XMLN * p_VideoSourceToken;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_img_GetMoveOptions(XMLN * p_node, img_GetMoveOptions_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    
    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
    }
    else
    {
    	return ONVIF_ERR_MissingAttribute;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_img_Move(XMLN * p_node, img_Move_REQ * p_req)
{
	XMLN * p_VideoSourceToken;
	XMLN * p_Focus;
	XMLN * p_Absolute;
	XMLN * p_Relative;
	XMLN * p_Continuous;
	
	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
    }

    p_Focus = xml_node_soap_get(p_node, "Focus");
    if (NULL == p_Focus)
    {
    	return ONVIF_ERR_MissingAttribute;
    }

    p_Absolute = xml_node_soap_get(p_Focus, "Absolute");
    if (p_Absolute)
    {
    	XMLN * p_Position;
		XMLN * p_Speed;
		
    	p_req->Focus.AbsoluteFlag = 1;
    	
    	p_Position = xml_node_soap_get(p_Absolute, "Position");
    	if (p_Position && p_Position->data)
    	{
    		p_req->Focus.Absolute.Position = (float)atof(p_Position->data);
    	}

    	p_Speed = xml_node_soap_get(p_Absolute, "Speed");
    	if (p_Speed && p_Speed->data)
    	{
    		p_req->Focus.Absolute.SpeedFlag = 1;
    		p_req->Focus.Absolute.Speed = (float)atof(p_Speed->data);
    	}
    }
    
	p_Relative = xml_node_soap_get(p_Focus, "Relative");
	if (p_Relative)
	{
		XMLN * p_Distance;
		XMLN * p_Speed;
		
		p_req->Focus.RelativeFlag = 1;
		
		p_Distance = xml_node_soap_get(p_Relative, "Distance");
		if (p_Distance && p_Distance->data)
		{
			p_req->Focus.Relative.Distance = (float)atof(p_Distance->data);
		}

		p_Speed = xml_node_soap_get(p_Relative, "Speed");
		if (p_Speed && p_Speed->data)
		{
			p_req->Focus.Relative.SpeedFlag = 1;			
			p_req->Focus.Relative.Speed = (float)atof(p_Speed->data);
		}
	}

	p_Continuous = xml_node_soap_get(p_Focus, "Continuous");
	if (p_Continuous)
	{
		XMLN * p_Speed;
		
		p_req->Focus.ContinuousFlag = 1;
		
		p_Speed = xml_node_soap_get(p_Continuous, "Speed");
		if (p_Speed && p_Speed->data)
		{
			p_req->Focus.Continuous.Speed = (float)atof(p_Speed->data);
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_img_GetStatus(XMLN * p_node, img_GetStatus_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    
    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
    }
    else
    {
    	return ONVIF_ERR_MissingAttribute;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_img_Stop(XMLN * p_node, img_Stop_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    
    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
    }
    else
    {
    	return ONVIF_ERR_MissingAttribute;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_img_GetPresets(XMLN * p_node, img_GetPresets_REQ * p_req)
{
	XMLN * p_VideoSourceToken;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_img_GetCurrentPreset(XMLN * p_node, img_GetCurrentPreset_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    
    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_img_SetCurrentPreset(XMLN * p_node, img_SetCurrentPreset_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    XMLN * p_PresetToken;
    
    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
    }

    p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
    if (p_PresetToken && p_PresetToken->data)
    {
        strncpy(p_req->PresetToken, p_PresetToken->data, sizeof(p_req->PresetToken)-1);
    }

    return ONVIF_OK;
}

#endif // IMAGE_SUPPORT

/***************************************************************************************/

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

ONVIF_RET parse_MulticastConfiguration(XMLN * p_node, onvif_MulticastConfiguration * p_req)
{
	XMLN * p_Multicast;
	XMLN * p_Address;
	XMLN * p_Port;
	XMLN * p_TTL;
	XMLN * p_AutoStart;
	
	p_Multicast = xml_node_soap_get(p_node, "Multicast");
    if (NULL == p_Multicast)
    {
        return ONVIF_ERR_MissingAttribute;
    }

    p_Address = xml_node_soap_get(p_Multicast, "Address");
    if (p_Address)
    {
    	XMLN * p_IPv4Address;
    	
        p_IPv4Address = xml_node_soap_get(p_Address, "IPv4Address");
	    if (p_IPv4Address && p_IPv4Address->data)
	    {
	        strncpy(p_req->IPv4Address, p_IPv4Address->data, sizeof(p_req->IPv4Address)-1);
	    }
    }

    p_Port = xml_node_soap_get(p_Multicast, "Port");
    if (p_Port && p_Port->data)
    {
    	p_req->Port = atoi(p_Port->data);
    }

    p_TTL = xml_node_soap_get(p_Multicast, "TTL");
    if (p_TTL && p_TTL->data)
    {
    	p_req->TTL = atoi(p_TTL->data);
    }

    p_AutoStart = xml_node_soap_get(p_Multicast, "AutoStart");
    if (p_AutoStart && p_AutoStart->data)
    {
    	p_req->AutoStart = parse_Bool(p_AutoStart->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_OSDColor(XMLN * p_node, onvif_OSDColor * p_req)
{
	XMLN * p_Color;
	const char * p_Transparent;

	p_Transparent = xml_attr_get(p_node, "Transparent");
	if (p_Transparent)
	{
		p_req->TransparentFlag = 1;
		p_req->Transparent = atoi(p_Transparent);
	}

	p_Color = xml_node_soap_get(p_node, "Color");
	if (p_Color)
	{
		const char * p_X;
		const char * p_Y;
		const char * p_Z;

		p_X = xml_attr_get(p_Color, "X");
		if (p_X)
		{
			p_req->X = (float) atof(p_X);
		}

		p_Y = xml_attr_get(p_Color, "Y");
		if (p_X)
		{
			p_req->Y = (float) atof(p_Y);
		}

		p_Z = xml_attr_get(p_Color, "Z");
		if (p_Z)
		{
			p_req->Z = (float) atof(p_Z);
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_OSDConfiguration(XMLN * p_node, onvif_OSDConfiguration * p_req)
{
	XMLN * p_VideoSourceConfigurationToken;
	XMLN * p_Type;
	XMLN * p_Position;
	XMLN * p_TextString;
	XMLN * p_Image;
	const char * p_token;
	
	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_VideoSourceConfigurationToken = xml_node_soap_get(p_node, "VideoSourceConfigurationToken");
	if (p_VideoSourceConfigurationToken && p_VideoSourceConfigurationToken->data)
	{
		strncpy(p_req->VideoSourceConfigurationToken, p_VideoSourceConfigurationToken->data, sizeof(p_req->VideoSourceConfigurationToken)-1);
	}

	p_Type = xml_node_soap_get(p_node, "Type");
	if (p_Type && p_Type->data)
	{
		p_req->Type = onvif_StringToOSDType(p_Type->data);
	}

	p_Position = xml_node_soap_get(p_node, "Position");
	if (p_Position)
	{
		XMLN * p_Type;
		XMLN * p_Pos;

		p_Type = xml_node_soap_get(p_Position, "Type");
		if (p_Type && p_Type->data)
		{
			p_req->Position.Type = onvif_StringToOSDPosType(p_Type->data);
		}

		p_Pos = xml_node_soap_get(p_Position, "Pos");
		if (p_Pos)
		{
			p_req->Position.PosFlag = parse_Vector(p_Pos, &p_req->Position.Pos);
		}
	}

	p_TextString = xml_node_soap_get(p_node, "TextString");
	if (p_TextString)
	{
		XMLN * p_Type;
		XMLN * p_DateFormat;
		XMLN * p_TimeFormat;
		XMLN * p_FontSize;
		XMLN * p_FontColor;
		XMLN * p_BackgroundColor;
		XMLN * p_PlainText;
		
		p_req->TextStringFlag = 1;
		
		p_Type = xml_node_soap_get(p_TextString, "Type");
		if (p_Type && p_Type->data)
		{
			p_req->TextString.Type = onvif_StringToOSDTextType(p_Type->data);
		}

		p_DateFormat = xml_node_soap_get(p_TextString, "DateFormat");
		if (p_DateFormat && p_DateFormat->data)
		{
			p_req->TextString.DateFormatFlag = 1;
			strncpy(p_req->TextString.DateFormat, p_DateFormat->data, sizeof(p_req->TextString.DateFormat)-1);
		}

		p_TimeFormat = xml_node_soap_get(p_TextString, "TimeFormat");
		if (p_TimeFormat && p_TimeFormat->data)
		{
			p_req->TextString.TimeFormatFlag = 1;
			strncpy(p_req->TextString.TimeFormat, p_TimeFormat->data, sizeof(p_req->TextString.TimeFormat)-1);
		}

		p_FontSize = xml_node_soap_get(p_TextString, "FontSize");
		if (p_FontSize && p_FontSize->data)
		{
			p_req->TextString.FontSizeFlag = 1;
			p_req->TextString.FontSize = atoi(p_FontSize->data);
		}

		p_FontColor = xml_node_soap_get(p_TextString, "FontColor");
		if (p_FontColor)
		{
			p_req->TextString.FontColorFlag = 1;
			
			parse_OSDColor(p_FontColor, &p_req->TextString.FontColor);
		}

		p_BackgroundColor = xml_node_soap_get(p_TextString, "BackgroundColor");
		if (p_BackgroundColor)
		{
			p_req->TextString.BackgroundColorFlag = 1;
			
			parse_OSDColor(p_BackgroundColor, &p_req->TextString.BackgroundColor);
		}

		p_PlainText = xml_node_soap_get(p_TextString, "PlainText");
		if (p_PlainText && p_PlainText->data)
		{
			p_req->TextString.PlainTextFlag = 1;
			strncpy(p_req->TextString.PlainText, p_PlainText->data, sizeof(p_req->TextString.PlainText)-1);
		}
	}

	p_Image = xml_node_soap_get(p_node, "Image");
	if (p_Image)
	{
		XMLN * p_ImgPath;
		
		p_req->ImageFlag = 1;
		
		p_ImgPath = xml_node_soap_get(p_Image, "ImgPath");
		if (p_ImgPath && p_ImgPath->data)
		{
			strncpy(p_req->Image.ImgPath, p_ImgPath->data, sizeof(p_req->Image.ImgPath)-1);
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_VideoSource(XMLN * p_node, onvif_VideoSource * p_req)
{
    XMLN * p_Framerate;
	XMLN * p_Resolution;
	const char * p_token;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_Framerate = xml_node_soap_get(p_node, "Framerate");
	if (p_Framerate && p_Framerate->data)
	{
	    p_req->Framerate = (float) atof(p_Framerate->data);
	}

	p_Resolution = xml_node_soap_get(p_node, "Resolution");
	if (p_Resolution)
	{
	    parse_Resolution(p_Resolution, &p_req->Resolution);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_VideoSourceMode(XMLN * p_node, onvif_VideoSourceMode * p_req)
{
	XMLN * p_MaxFramerate;
	XMLN * p_MaxResolution;
	XMLN * p_Encodings;
	XMLN * p_Reboot;
	XMLN * p_Description;
	const char * p_token;
	const char * p_Enabled;

	p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
    	strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}
	
	p_Enabled = xml_attr_get(p_node, "Enabled");
	if (p_Enabled)
	{
    	p_req->Enabled = parse_Bool(p_Enabled);
    }
    
	p_MaxFramerate = xml_node_soap_get(p_node, "MaxFramerate");
	if (p_MaxFramerate && p_MaxFramerate->data)
	{
		p_req->MaxFramerate = (float)atof(p_MaxFramerate->data);
	}

	p_MaxResolution = xml_node_soap_get(p_node, "MaxResolution");
	if (p_MaxResolution)
	{
		parse_Resolution(p_MaxResolution, &p_req->MaxResolution);
	}

	p_Encodings = xml_node_soap_get(p_node, "Encodings");
	if (p_Encodings && p_Encodings->data)
	{
		strncpy(p_req->Encodings, p_Encodings->data, sizeof(p_req->Encodings)-1);
	}

	p_Reboot = xml_node_soap_get(p_node, "Reboot");
	if (p_Reboot && p_Reboot->data)
	{
		p_req->Reboot = parse_Bool(p_Reboot->data);
	}

	p_Description = xml_node_soap_get(p_node, "Description");
	if (p_Description && p_Description->data)
	{
		p_req->DescriptionFlag = 1;
		strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description)-1);
	}

	return ONVIF_OK;    
}

ONVIF_RET parse_VideoSourceConfiguration(XMLN * p_node, onvif_VideoSourceConfiguration * p_req)
{
	XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_SourceToken;
	XMLN * p_Bounds;
	XMLN * p_Extension;
	const char * p_token;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_UseCount = xml_node_soap_get(p_node, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_req->UseCount = atoi(p_UseCount->data);
	}

	p_SourceToken = xml_node_soap_get(p_node, "SourceToken");
	if (p_SourceToken && p_SourceToken->data)
	{
		strncpy(p_req->SourceToken, p_SourceToken->data, sizeof(p_req->SourceToken)-1);
	}

	p_Bounds = xml_node_soap_get(p_node, "Bounds");
	if (p_Bounds)
	{
		const char * p_x;
		const char * p_y;
		const char * p_width;
		const char * p_height;

		p_x = xml_attr_get(p_Bounds, "x");
		if (p_x)
		{
			p_req->Bounds.x = atoi(p_x);
		}

		p_y = xml_attr_get(p_Bounds, "y");
		if (p_y)
		{
			p_req->Bounds.y = atoi(p_y);
		}

		p_width = xml_attr_get(p_Bounds, "width");
		if (p_width)
		{
			p_req->Bounds.width = atoi(p_width);
		}

		p_height = xml_attr_get(p_Bounds, "height");
		if (p_height)
		{
			p_req->Bounds.height = atoi(p_height);
		}
	}
	
	p_Extension = xml_node_soap_get(p_node, "Extension");
	if (p_Extension)
	{
		p_req->ExtensionFlag = 1;
		
		XMLN * p_Rotate = xml_node_soap_get(p_Extension, "Rotate");
		if (p_Rotate)
		{
			p_req->Extension.RotateFlag = 1;
			
			XMLN * p_Mode;
			XMLN * p_Degree;
			
			p_Mode = xml_node_soap_get(p_Rotate, "Mode");
			if (p_Mode && p_Mode->data)
			{
				p_req->Extension.Rotate.Mode = onvif_StringToRotateMode(p_Mode->data);
			}
			p_Degree= xml_node_soap_get(p_Rotate, "Degree");
			if (p_Degree && p_Degree->data)
			{
				p_req->Extension.Rotate.DegreeFlag = 1;
				p_req->Extension.Rotate.Degree = atoi(p_Degree->data);
			}
			
		}
	}
	return ONVIF_OK;
}

ONVIF_RET parse_VideoRateControl2(XMLN * p_node, onvif_VideoRateControl2 * p_req)
{
    XMLN * p_FrameRateLimit;
	XMLN * p_BitrateLimit;
	const char * p_ConstantBitRate;

	p_ConstantBitRate = xml_attr_get(p_node, "ConstantBitRate");
    if (p_ConstantBitRate)
    {
        p_req->ConstantBitRateFlag = 1;
        p_req->ConstantBitRate = parse_Bool(p_ConstantBitRate);
    }
    
    p_FrameRateLimit = xml_node_soap_get(p_node, "FrameRateLimit");
    if (p_FrameRateLimit && p_FrameRateLimit->data)
    {
        p_req->FrameRateLimit = (float)atof(p_FrameRateLimit->data);
    }

    p_BitrateLimit = xml_node_soap_get(p_node, "BitrateLimit");
    if (p_BitrateLimit && p_BitrateLimit->data)
    {
        p_req->BitrateLimit = atoi(p_BitrateLimit->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_VideoEncoder2Configuration(XMLN * p_node, onvif_VideoEncoder2Configuration * p_req)
{
	XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_Encoding;
	XMLN * p_Resolution;
	XMLN * p_Quality;
	XMLN * p_RateControl;
	XMLN * p_SessionTimeout;
	const char * p_token;
	const char * p_GovLength;
	const char * p_AnchorFrameDistance;
	const char * p_Profile;
	const char * p_GuaranteedFrameRate;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_GovLength = xml_attr_get(p_node, "GovLength");
	if (p_GovLength)
	{
		p_req->GovLengthFlag = 1;
		p_req->GovLength = atoi(p_GovLength);
	}

	p_AnchorFrameDistance = xml_attr_get(p_node, "AnchorFrameDistance");
	if (p_AnchorFrameDistance)
	{
		p_req->AnchorFrameDistanceFlag = 1;
		p_req->AnchorFrameDistance = atoi(p_AnchorFrameDistance);
	}

	p_Profile = xml_attr_get(p_node, "Profile");
	if (p_Profile)
	{
		p_req->ProfileFlag = 1;
		strncpy(p_req->Profile, p_Profile, sizeof(p_req->Profile)-1);
	}

	p_GuaranteedFrameRate = xml_attr_get(p_node, "GuaranteedFrameRate");
	if (p_GuaranteedFrameRate)
	{
		p_req->GuaranteedFrameRate = parse_Bool(p_GuaranteedFrameRate);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_UseCount = xml_node_soap_get(p_node, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_req->UseCount = atoi(p_UseCount->data);
	}

	p_Encoding = xml_node_soap_get(p_node, "Encoding");
	if (p_Encoding && p_Encoding->data)
	{
		strncpy(p_req->Encoding, p_Encoding->data, sizeof(p_req->Encoding)-1);
	}

	p_Resolution = xml_node_soap_get(p_node, "Resolution");
	if (p_Resolution)
	{
		parse_Resolution(p_Resolution, &p_req->Resolution);
	}

	p_RateControl = xml_node_soap_get(p_node, "RateControl");
	if (p_RateControl)
	{
		p_req->RateControlFlag = 1;
		parse_VideoRateControl2(p_RateControl, &p_req->RateControl);
	}

	if (ONVIF_OK == parse_MulticastConfiguration(p_node, &p_req->Multicast))
	{
		p_req->MulticastFlag = 1;
	}

	p_Quality = xml_node_soap_get(p_node, "Quality");
	if (p_Quality && p_Quality->data)
	{
		p_req->Quality = (float)atof(p_Quality->data);
	}

	p_SessionTimeout = xml_node_soap_get(p_node, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_PTZFilter(XMLN * p_node, onvif_PTZFilter * p_req)
{
    XMLN * p_Status;
	XMLN * p_Position;
	
	p_Status = xml_node_soap_get(p_node, "Status");
	if (p_Status && p_Status->data)
	{
		p_req->Status = parse_Bool(p_Status->data);
	}

	p_Position = xml_node_soap_get(p_node, "Position");
	if (p_Position && p_Position->data)
	{
		p_req->Position = parse_Bool(p_Position->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_EventSubscription(XMLN * p_node, onvif_EventSubscription * p_req)
{
	XMLN * p_Filter;

	p_Filter = xml_node_soap_get(p_node, "Filter");
	if (p_Filter)
	{
		XMLN * p_TopicExpression;

		p_TopicExpression = xml_node_soap_get(p_Filter, "TopicExpression");
		if (p_TopicExpression && p_TopicExpression->data)
		{
			const char * p_Dialect;

			p_Dialect = xml_attr_get(p_TopicExpression, "Dialect");
			if (p_Dialect)
			{
				strncpy(p_req->Dialect, p_Dialect, sizeof(p_req->Dialect)-1);
			}

			strncpy(p_req->TopicExpression, p_TopicExpression->data, sizeof(p_req->TopicExpression)-1);
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_MetadataConfiguration(XMLN * p_node, onvif_MetadataConfiguration * p_req)
{
	XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_PTZStatus;
	XMLN * p_Events;
	XMLN * p_Analytics;
	XMLN * p_SessionTimeout;
#ifdef VIDEO_ANALYTICS	
	XMLN * p_AnalyticsEngineConfiguration;
#endif	
	const char * p_token;
	const char * p_CompressionType;
	const char * p_GeoLocation;
	const char * p_ShapePolygon;
	
	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_CompressionType = xml_attr_get(p_node, "CompressionType");
	if (p_CompressionType)
	{
	    p_req->CompressionTypeFlag = 1;
		strncpy(p_req->CompressionType, p_CompressionType, sizeof(p_req->CompressionType)-1);
	}

	p_GeoLocation = xml_attr_get(p_node, "GeoLocation");
	if (p_GeoLocation)
	{
		p_req->GeoLocation = parse_Bool(p_GeoLocation);
	}

	p_ShapePolygon = xml_attr_get(p_node, "ShapePolygon");
	if (p_ShapePolygon)
	{
		p_req->ShapePolygon = parse_Bool(p_ShapePolygon);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_UseCount = xml_node_soap_get(p_node, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_req->UseCount = atoi(p_UseCount->data);
	}

	p_PTZStatus = xml_node_soap_get(p_node, "PTZStatus");
	if (p_PTZStatus)
	{
		p_req->PTZStatusFlag = 1;
		parse_PTZFilter(p_PTZStatus, &p_req->PTZStatus);
	}

	p_Events = xml_node_soap_get(p_node, "Events");
	if (p_Events)
	{
		p_req->EventsFlag = 1;
		parse_EventSubscription(p_Events, &p_req->Events);
	}

	p_Analytics = xml_node_soap_get(p_node, "Analytics");
	if (p_Analytics && p_Analytics->data)
	{
		p_req->AnalyticsFlag = 1;
		p_req->Analytics = parse_Bool(p_Analytics->data);
	}

	parse_MulticastConfiguration(p_node, &p_req->Multicast);

	p_SessionTimeout = xml_node_soap_get(p_node, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
	}

#ifdef VIDEO_ANALYTICS
	p_AnalyticsEngineConfiguration = xml_node_soap_get(p_node, "AnalyticsEngineConfiguration");
	if (p_AnalyticsEngineConfiguration)
	{
		p_req->AnalyticsEngineConfigurationFlag = 1;
		parse_AnalyticsEngineConfiguration(p_AnalyticsEngineConfiguration, &p_req->AnalyticsEngineConfiguration);
	}
#endif

	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetOSD(XMLN * p_node, trt_SetOSD_REQ * p_req)
{
    XMLN * p_OSD;

	p_OSD = xml_node_soap_get(p_node, "OSD");
	if (p_OSD)
	{
		parse_OSDConfiguration(p_OSD, &p_req->OSD);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_CreateOSD(XMLN * p_node, trt_CreateOSD_REQ * p_req)
{
	XMLN * p_OSD;

	p_OSD = xml_node_soap_get(p_node, "OSD");
	if (p_OSD)
	{
		parse_OSDConfiguration(p_OSD, &p_req->OSD);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_DeleteOSD(XMLN * p_node, trt_DeleteOSD_REQ * p_req)
{
	XMLN * p_OSDToken = xml_node_soap_get(p_node, "OSDToken");
	if (p_OSDToken && p_OSDToken->data)
	{
		strncpy(p_req->OSDToken, p_OSDToken->data, sizeof(p_req->OSDToken)-1);
	}

	return ONVIF_OK;
}

#ifdef AUDIO_SUPPORT

ONVIF_RET parse_AudioSourceConfiguration(XMLN * p_node, onvif_AudioSourceConfiguration * p_req)
{
	XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_SourceToken;
	const char * p_token;
	
	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_UseCount = xml_node_soap_get(p_node, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_req->UseCount = atoi(p_UseCount->data);
	}

	p_SourceToken = xml_node_soap_get(p_node, "SourceToken");
	if (p_SourceToken && p_SourceToken->data)
	{
		strncpy(p_req->SourceToken, p_SourceToken->data, sizeof(p_req->SourceToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_AudioDecoderConfiguration(XMLN * p_node, onvif_AudioDecoderConfiguration * p_req)
{
    XMLN * p_Name;
    XMLN * p_UseCount;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_UseCount = xml_node_soap_get(p_node, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_req->UseCount = atoi(p_UseCount->data);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_AudioEncoder2Configuration(XMLN * p_node, onvif_AudioEncoder2Configuration * p_req)
{
    XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_Encoding;
	XMLN * p_Bitrate;
	XMLN * p_SampleRate;
	XMLN * p_SessionTimeout;
	const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }
    
    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_UseCount = xml_node_soap_get(p_node, "UseCount");
    if (p_UseCount && p_UseCount->data)
    {
        p_req->UseCount = atoi(p_UseCount->data);
    }

    p_Encoding = xml_node_soap_get(p_node, "Encoding");
    if (p_Encoding && p_Encoding->data)
    {
		strncpy(p_req->Encoding, p_Encoding->data, sizeof(p_req->Encoding)-1);
    }

	if (ONVIF_OK == parse_MulticastConfiguration(p_node, &p_req->Multicast))
	{
	    p_req->MulticastFlag = 1;
	}
	
	p_Bitrate = xml_node_soap_get(p_node, "Bitrate");
    if (p_Bitrate && p_Bitrate->data)
    {
        p_req->Bitrate = atoi(p_Bitrate->data);
    }

    p_SampleRate = xml_node_soap_get(p_node, "SampleRate");
    if (p_SampleRate && p_SampleRate->data)
    {
        p_req->SampleRate = atoi(p_SampleRate->data);
    }

    p_SessionTimeout = xml_node_soap_get(p_node, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
	}

    return ONVIF_OK;
}

#endif // AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT

ONVIF_RET parse_AudioOutputConfiguration(XMLN * p_node, onvif_AudioOutputConfiguration * p_req)
{
    XMLN * p_Name;
    XMLN * p_UseCount;
    XMLN * p_OutputToken;
    XMLN * p_SendPrimacy;
    XMLN * p_OutputLevel;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_UseCount = xml_node_soap_get(p_node, "UseCount");
    if (p_UseCount && p_UseCount->data)
    {
        p_req->UseCount = atoi(p_UseCount->data);
    }

    p_OutputToken = xml_node_soap_get(p_node, "OutputToken");
    if (p_OutputToken && p_OutputToken->data)
    {
        strncpy(p_req->OutputToken, p_OutputToken->data, sizeof(p_req->OutputToken)-1);
    }

    p_SendPrimacy = xml_node_soap_get(p_node, "SendPrimacy");
    if (p_SendPrimacy && p_SendPrimacy->data)
    {
        p_req->SendPrimacyFlag = 1;
        strncpy(p_req->SendPrimacy, p_SendPrimacy->data, sizeof(p_req->SendPrimacy)-1);
    }

    p_OutputLevel = xml_node_soap_get(p_node, "OutputLevel");
    if (p_OutputLevel && p_OutputLevel->data)
    {
        p_req->OutputLevel = atoi(p_OutputLevel->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_trt_GetAudioOutputConfigurationOptions(XMLN * p_node, trt_GetAudioOutputConfigurationOptions_REQ * p_req)
{
    XMLN * p_ConfigurationToken;
    XMLN * p_ProfileToken;

    p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
    if (p_ConfigurationToken && p_ConfigurationToken->data)
    {
        p_req->ConfigurationTokenFlag = 1;
        strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
    }

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        p_req->ProfileTokenFlag = 1;
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }

    return ONVIF_OK;
}

#endif // DEVICEIO_SUPPORT

#ifdef VIDEO_ANALYTICS

ONVIF_RET parse_AnalyticsEngineConfiguration(XMLN * p_node, onvif_AnalyticsEngineConfiguration * p_req)
{
	XMLN * p_AnalyticsModule;
	ConfigList * p_config;

	p_AnalyticsModule = xml_node_soap_get(p_node, "AnalyticsModule");
	while (p_AnalyticsModule && soap_strcmp(p_AnalyticsModule->name, "AnalyticsModule") == 0)
	{
		p_config = onvif_add_Config(&p_req->AnalyticsModule);
		if (p_config)
		{
			parse_Config(p_AnalyticsModule, &p_config->Config);
		}
		
		p_AnalyticsModule = p_AnalyticsModule->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_RuleEngineConfiguration(XMLN * p_node, onvif_RuleEngineConfiguration * p_req)
{
	XMLN * p_Rule;
	ConfigList * p_config;

	p_Rule = xml_node_soap_get(p_node, "Rule");
	while (p_Rule && soap_strcmp(p_Rule->name, "Rule") == 0)
	{
		p_config = onvif_add_Config(&p_req->Rule);
		if (p_config)
		{
			parse_Config(p_Rule, &p_config->Config);
		}
		
		p_Rule = p_Rule->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_VideoAnalyticsConfiguration(XMLN * p_node, onvif_VideoAnalyticsConfiguration * p_req)
{
	XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_AnalyticsEngineConfiguration;
	XMLN * p_RuleEngineConfiguration;	
	const char * p_token;
	ONVIF_RET ret = ONVIF_OK;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}
	
	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_UseCount = xml_node_soap_get(p_node, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_req->UseCount = atoi(p_UseCount->data);
	}

	p_AnalyticsEngineConfiguration = xml_node_soap_get(p_node, "AnalyticsEngineConfiguration");
	if (p_AnalyticsEngineConfiguration)
	{
		ret = parse_AnalyticsEngineConfiguration(p_AnalyticsEngineConfiguration, &p_req->AnalyticsEngineConfiguration);
	}

	p_RuleEngineConfiguration = xml_node_soap_get(p_node, "RuleEngineConfiguration");
	if (p_RuleEngineConfiguration)
	{
		ret = parse_RuleEngineConfiguration(p_RuleEngineConfiguration, &p_req->RuleEngineConfiguration);
	}
	
	return ret;
}

#endif // VIDEO_ANALYTICS

#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef MEDIA_SUPPORT

BOOL parse_VideoRateControl(XMLN * p_node, onvif_VideoRateControl * p_req)
{
    XMLN * p_FrameRateLimit;
	XMLN * p_EncodingInterval;
	XMLN * p_BitrateLimit;
	const char * p_ConstantBitRate;

	p_ConstantBitRate = xml_attr_get(p_node, "ConstantBitRate");
	if (p_ConstantBitRate)
	{
	    p_req->ConstantBitRateFlag = 1;
	    p_req->ConstantBitRate = parse_Bool(p_ConstantBitRate);
	}
	
	p_FrameRateLimit = xml_node_soap_get(p_node, "FrameRateLimit");
    if (p_FrameRateLimit && p_FrameRateLimit->data)
    {
        p_req->FrameRateLimit = (int)atof(p_FrameRateLimit->data);
    }

    p_EncodingInterval = xml_node_soap_get(p_node, "EncodingInterval");
    if (p_EncodingInterval && p_EncodingInterval->data)
    {
        p_req->EncodingInterval = atoi(p_EncodingInterval->data);
    }

    p_BitrateLimit = xml_node_soap_get(p_node, "BitrateLimit");
    if (p_BitrateLimit && p_BitrateLimit->data)
    {
        p_req->BitrateLimit = atoi(p_BitrateLimit->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_VideoEncoderConfiguration(XMLN * p_node, onvif_VideoEncoderConfiguration * p_req)
{
    XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_Encoding;
	XMLN * p_Resolution;
	XMLN * p_Quality;
	XMLN * p_RateControl;
	XMLN * p_SessionTimeout;
	const char * token;

    token = xml_attr_get(p_node, "token");
    if (token)
    {
        strncpy(p_req->token, token, sizeof(p_req->token)-1);
    }    

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_UseCount = xml_node_soap_get(p_node, "UseCount");
    if (p_UseCount && p_UseCount->data)
    {
        p_req->UseCount = atoi(p_UseCount->data);
    }

    p_Encoding = xml_node_soap_get(p_node, "Encoding");
    if (p_Encoding && p_Encoding->data)
    {
		p_req->Encoding = onvif_StringToVideoEncoding(p_Encoding->data);
    }

    p_Resolution = xml_node_soap_get(p_node, "Resolution");
    if (p_Resolution)
    {
        parse_Resolution(p_Resolution, &p_req->Resolution);
    }

    p_Quality = xml_node_soap_get(p_node, "Quality");
    if (p_Quality && p_Quality->data)
    {
        p_req->Quality = atoi(p_Quality->data);
    }

    p_RateControl = xml_node_soap_get(p_node, "RateControl");
    if (p_RateControl)
    {
		p_req->RateControlFlag = 1;
		parse_VideoRateControl(p_RateControl, &p_req->RateControl);
    }
    
    if (p_req->Encoding == VideoEncoding_H264)
    {
    	XMLN * p_H264 = xml_node_soap_get(p_node, "H264");
    	if (p_H264)
    	{
    		XMLN * p_GovLength;
			XMLN * p_H264Profile;
			
    		p_req->H264Flag = 1;
    		
    		p_GovLength = xml_node_soap_get(p_H264, "GovLength");
		    if (p_GovLength && p_GovLength->data)
		    {
		        p_req->H264.GovLength = atoi(p_GovLength->data);
		    }

		    p_H264Profile = xml_node_soap_get(p_H264, "H264Profile");
		    if (p_H264Profile && p_H264Profile->data)
		    {
				p_req->H264.H264Profile = onvif_StringToH264Profile(p_H264Profile->data);
		    }
    	}
    }
    else if (p_req->Encoding == VideoEncoding_MPEG4)
    {
    	XMLN * p_MPEG4 = xml_node_soap_get(p_node, "MPEG4");
    	if (p_MPEG4)
    	{
    		XMLN * p_GovLength;
			XMLN * p_Mpeg4Profile;
			
    		p_req->MPEG4Flag = 1;
    		
    		p_GovLength = xml_node_soap_get(p_MPEG4, "GovLength");
		    if (p_GovLength && p_GovLength->data)
		    {
		        p_req->MPEG4.GovLength = atoi(p_GovLength->data);
		    }

		    p_Mpeg4Profile = xml_node_soap_get(p_MPEG4, "Mpeg4Profile");
		    if (p_Mpeg4Profile && p_Mpeg4Profile->data)
		    {
				p_req->MPEG4.Mpeg4Profile = onvif_StringToMpeg4Profile(p_Mpeg4Profile->data);
		    }
    	}
    }	

	parse_MulticastConfiguration(p_node, &p_req->Multicast);
	
	p_SessionTimeout = xml_node_soap_get(p_node, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_AudioEncoderConfiguration(XMLN * p_node, onvif_AudioEncoderConfiguration * p_req)
{
    XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_Encoding;
	XMLN * p_Bitrate;
	XMLN * p_SampleRate;
	XMLN * p_SessionTimeout;
	const char * token;
	
    token = xml_attr_get(p_node, "token");
    if (token)
    {
        strncpy(p_req->token, token, sizeof(p_req->token)-1);
    }    

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_UseCount = xml_node_soap_get(p_node, "UseCount");
    if (p_UseCount && p_UseCount->data)
    {
        p_req->UseCount = atoi(p_UseCount->data);
    }

    p_Encoding = xml_node_soap_get(p_node, "Encoding");
    if (p_Encoding && p_Encoding->data)
    {
		p_req->Encoding = onvif_StringToAudioEncoding(p_Encoding->data);
    }    

    p_Bitrate = xml_node_soap_get(p_node, "Bitrate");
    if (p_Bitrate && p_Bitrate->data)
    {
        p_req->Bitrate = atoi(p_Bitrate->data);
    }

    p_SampleRate = xml_node_soap_get(p_node, "SampleRate");
    if (p_SampleRate && p_SampleRate->data)
    {
        p_req->SampleRate = atoi(p_SampleRate->data);
    }    

	parse_MulticastConfiguration(p_node, &p_req->Multicast);
	
	p_SessionTimeout = xml_node_soap_get(p_node, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_AudioDecoderConfigurationOptions(XMLN * p_node, onvif_AudioDecoderConfigurationOptions * p_req)
{
    XMLN * p_AACDecOptions;
	XMLN * p_G711DecOptions;
	XMLN * p_G726DecOptions;

	p_AACDecOptions = xml_node_soap_get(p_node, "AACDecOptions");
	if (p_AACDecOptions)
	{
		XMLN * p_Bitrate;
		XMLN * p_SampleRateRange;

		p_req->AACDecOptionsFlag = 1;
		
		p_Bitrate = xml_node_soap_get(p_AACDecOptions, "Bitrate");
		if (p_Bitrate)	
		{
			parse_IntList(p_Bitrate, &p_req->AACDecOptions.Bitrate);
		}

		p_SampleRateRange = xml_node_soap_get(p_AACDecOptions, "SampleRateRange");
		if (p_SampleRateRange)	
		{
			parse_IntList(p_SampleRateRange, &p_req->AACDecOptions.SampleRateRange);
		}
	}

	p_G711DecOptions = xml_node_soap_get(p_node, "G711DecOptions");
	if (p_G711DecOptions)
	{
		XMLN * p_Bitrate;
		XMLN * p_SampleRateRange;

		p_req->G711DecOptionsFlag = 1;
		
		p_Bitrate = xml_node_soap_get(p_G711DecOptions, "Bitrate");
		if (p_Bitrate)	
		{
			parse_IntList(p_Bitrate, &p_req->G711DecOptions.Bitrate);
		}

		p_SampleRateRange = xml_node_soap_get(p_G711DecOptions, "SampleRateRange");
		if (p_SampleRateRange)	
		{
			parse_IntList(p_SampleRateRange, &p_req->G711DecOptions.SampleRateRange);
		}
	}

	p_G726DecOptions = xml_node_soap_get(p_node, "G726DecOptions");
	if (p_G726DecOptions)
	{
		XMLN * p_Bitrate;
		XMLN * p_SampleRateRange;

		p_req->G726DecOptionsFlag = 1;
		
		p_Bitrate = xml_node_soap_get(p_G726DecOptions, "Bitrate");
		if (p_Bitrate)	
		{
			parse_IntList(p_Bitrate, &p_req->G726DecOptions.Bitrate);
		}

		p_SampleRateRange = xml_node_soap_get(p_G726DecOptions, "SampleRateRange");
		if (p_SampleRateRange)	
		{
			parse_IntList(p_SampleRateRange, &p_req->G726DecOptions.SampleRateRange);
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetVideoEncoderConfiguration(XMLN * p_node, trt_SetVideoEncoderConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
	XMLN * p_ForcePersistence;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_VideoEncoderConfiguration(p_Configuration, &p_req->Configuration);
    }
    
	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}
    	
    return ONVIF_OK;
}

ONVIF_RET parse_trt_SetSynchronizationPoint(XMLN * p_node, trt_SetSynchronizationPoint_REQ * p_req)
{
	XMLN * p_ProfileToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
    
	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetProfile(XMLN * p_node, trt_GetProfile_REQ * p_req)
{
	XMLN * p_ProfileToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_CreateProfile(XMLN * p_node, trt_CreateProfile_REQ * p_req)
{
	XMLN * p_Name;
	XMLN * p_Token;
	
	assert(p_node);

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}
	
	p_Token = xml_node_soap_get(p_node, "Token");
	if (p_Token && p_Token->data)
	{
		p_req->TokenFlag = 1;
		strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_DeleteProfile(XMLN * p_node, trt_DeleteProfile_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_AddVideoSourceConfiguration(XMLN * p_node, trt_AddVideoSourceConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_RemoveVideoSourceConfiguration(XMLN * p_node, trt_RemoveVideoSourceConfiguration_REQ * p_req)
{
    XMLN * p_ProfileToken;
    
    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_AddVideoEncoderConfiguration(XMLN * p_node, trt_AddVideoEncoderConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_RemoveVideoEncoderConfiguration(XMLN * p_node, trt_RemoveVideoEncoderConfiguration_REQ * p_req)
{
    XMLN * p_ProfileToken;
    
    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetStreamUri(XMLN * p_node, trt_GetStreamUri_REQ * p_req)
{
	ONVIF_RET ret = ONVIF_OK;
	XMLN * p_StreamSetup;
	XMLN * p_ProfileToken;

	p_StreamSetup = xml_node_soap_get(p_node, "StreamSetup");
	if (p_StreamSetup)
	{
		ret = parse_StreamSetup(p_StreamSetup, &p_req->StreamSetup);		
	}
	
	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	return ret;
}

ONVIF_RET parse_trt_GetSnapshotUri(XMLN * p_node, trt_GetSnapshotUri_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
	    strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetVideoSourceConfigurationOptions(XMLN * p_node, trt_GetVideoSourceConfigurationOptions_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_ProfileToken;
	
	assert(p_node);

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetVideoSourceConfiguration(XMLN * p_node, trt_SetVideoSourceConfiguration_REQ * p_req)
{
	XMLN * p_Configuration;
	XMLN * p_ForcePersistence;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		parse_VideoSourceConfiguration(p_Configuration, &p_req->Configuration);
	}
    
	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}	
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetVideoEncoderConfigurationOptions(XMLN * p_node, trt_GetVideoEncoderConfigurationOptions_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_ProfileToken;
	
	assert(p_node);

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetOSDs(XMLN * p_node, trt_GetOSDs_REQ * p_req)
{
	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetOSD(XMLN * p_node, trt_GetOSD_REQ * p_req)
{
	XMLN * p_OSDToken = xml_node_soap_get(p_node, "OSDToken");
	if (p_OSDToken && p_OSDToken->data)
	{
		strncpy(p_req->OSDToken, p_OSDToken->data, sizeof(p_req->OSDToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetMetadataConfiguration(XMLN * p_node, trt_SetMetadataConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
	XMLN * p_ForcePersistence;

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_MetadataConfiguration(p_Configuration, &p_req->Configuration);
    }
    
	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_AddMetadataConfiguration(XMLN * p_node, trt_AddMetadataConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetMetadataConfigurationOptions(XMLN * p_node, trt_GetMetadataConfigurationOptions_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_ProfileToken;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetVideoSourceModes(XMLN * p_node, trt_GetVideoSourceModes_REQ * p_req)
{
    XMLN * p_VideoSourceToken;

    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetVideoSourceMode(XMLN * p_node, trt_SetVideoSourceMode_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    XMLN * p_VideoSourceModeToken;

    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	p_VideoSourceModeToken = xml_node_soap_get(p_node, "VideoSourceModeToken");
	if (p_VideoSourceModeToken && p_VideoSourceModeToken->data)
	{
		strncpy(p_req->VideoSourceModeToken, p_VideoSourceModeToken->data, sizeof(p_req->VideoSourceModeToken)-1);
	}

	return ONVIF_OK;
}

#ifdef AUDIO_SUPPORT

ONVIF_RET parse_trt_AddAudioSourceConfiguration(XMLN * p_node, trt_AddAudioSourceConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_AddAudioEncoderConfiguration(XMLN * p_node, trt_AddAudioEncoderConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetAudioSourceConfigurationOptions(XMLN * p_node, trt_GetAudioSourceConfigurationOptions_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_ProfileToken;
	
	assert(p_node);

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetAudioSourceConfiguration(XMLN * p_node, trt_SetAudioSourceConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
	XMLN * p_ForcePersistence;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_AudioSourceConfiguration(p_Configuration, &p_req->Configuration);
	}

	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}	
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetAudioEncoderConfigurationOptions(XMLN * p_node, trt_GetAudioEncoderConfigurationOptions_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_ProfileToken;
	
	assert(p_node);

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetAudioEncoderConfiguration(XMLN * p_node, trt_SetAudioEncoderConfiguration_REQ * p_req)
{
	XMLN * p_Configuration;
	XMLN * p_ForcePersistence;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_AudioEncoderConfiguration(p_Configuration, &p_req->Configuration);
    }

	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}
    	
    return ONVIF_OK;
}

ONVIF_RET parse_trt_AddAudioDecoderConfiguration(XMLN * p_node, trt_AddAudioDecoderConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_RemoveAudioDecoderConfiguration(XMLN * p_node, trt_RemoveAudioDecoderConfiguration_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetAudioDecoderConfiguration(XMLN * p_node, trt_SetAudioDecoderConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
	XMLN * p_ForcePersistence;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_AudioDecoderConfiguration(p_Configuration, &p_req->Configuration);
	}

	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
	    p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetAudioDecoderConfigurationOptions(XMLN * p_node, trt_GetAudioDecoderConfigurationOptions_REQ * p_req)
{
    XMLN * p_ConfigurationToken;
	XMLN * p_ProfileToken;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	
	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	return ONVIF_OK;
}

#endif // end of AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT

ONVIF_RET parse_trt_GetAudioOutputConfiguration(XMLN * p_node, trt_GetAudioOutputConfiguration_REQ * p_req)
{
    XMLN * p_ConfigurationToken;

    p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
    if (p_ConfigurationToken && p_ConfigurationToken->data)
    {
        strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_trt_SetAudioOutputConfiguration(XMLN * p_node, tmd_SetAudioOutputConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
    XMLN * p_ForcePersistence;

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_AudioOutputConfiguration(p_Configuration, &p_req->Configuration);
    }

    p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
    if (p_ForcePersistence && p_ForcePersistence->data)
    {
        p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_trt_GetCompatibleAudioOutputConfigurations(XMLN * p_node, trt_GetCompatibleAudioOutputConfigurations_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_trt_AddAudioOutputConfiguration(XMLN * p_node, trt_AddAudioOutputConfiguration_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_ConfigurationToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }

    p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
    if (p_ConfigurationToken && p_ConfigurationToken->data)
    {
        strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_trt_RemoveAudioOutputConfiguration(XMLN * p_node, trt_RemoveAudioOutputConfiguration_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }
    
    return ONVIF_OK;
}

#endif // DEVICEIO_SUPPORT

#ifdef PTZ_SUPPORT

ONVIF_RET parse_trt_AddPTZConfiguration(XMLN * p_node, trt_AddPTZConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}
	else
	{
		return ONVIF_ERR_MissingAttribute;
	}
	
	return ONVIF_OK;
}

#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

ONVIF_RET parse_trt_AddVideoAnalyticsConfiguration(XMLN * p_node, trt_AddVideoAnalyticsConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_ConfigurationToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_GetVideoAnalyticsConfiguration(XMLN * p_node, trt_GetVideoAnalyticsConfiguration_REQ * p_req)
{
	XMLN * p_ConfigurationToken;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_RemoveVideoAnalyticsConfiguration(XMLN * p_node, trt_RemoveVideoAnalyticsConfiguration_REQ * p_req)
{
	XMLN * p_ProfileToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trt_SetVideoAnalyticsConfiguration(XMLN * p_node, trt_SetVideoAnalyticsConfiguration_REQ * p_req)
{
	XMLN * p_Configuration;
	XMLN * p_ForcePersistence;
	ONVIF_RET ret = ONVIF_ERR_MissingAttribute;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		ret = parse_VideoAnalyticsConfiguration(p_Configuration, &p_req->Configuration);
	}

	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
	}
	
	return ret;
}

ONVIF_RET parse_trt_GetCompatibleVideoAnalyticsConfigurations(XMLN * p_node, trt_GetCompatibleVideoAnalyticsConfigurations_REQ * p_req)
{
    XMLN * p_ProfileToken;
	
	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

#endif // VIDEO_ANALYTICS

#endif // MEDIA_SUPPORT

#ifdef PTZ_SUPPORT

BOOL parse_Space2DDescription(XMLN * p_node, onvif_Space2DDescription * p_req)
{
	XMLN * p_XRange;
	XMLN * p_YRange;

	p_XRange = xml_node_soap_get(p_node, "XRange");
	if (p_XRange)
	{
		parse_FloatRange(p_XRange, &p_req->XRange);
	}

	p_YRange = xml_node_soap_get(p_node, "YRange");
	if (p_YRange)
	{
		parse_FloatRange(p_YRange, &p_req->YRange);
	}

	return TRUE;
}

BOOL parse_Space1DDescription(XMLN * p_node, onvif_Space1DDescription * p_req)
{
	XMLN * p_XRange;

	p_XRange = xml_node_soap_get(p_node, "XRange");
	if (p_XRange)
	{
		parse_FloatRange(p_XRange, &p_req->XRange);
	}

	return TRUE;
}

ONVIF_RET parse_PTZPresetTourSupported(XMLN * p_node, onvif_PTZPresetTourSupported * p_req)
{
    XMLN * p_MaximumNumberOfPresetTours;
    XMLN * p_PTZPresetTourOperation;

    p_MaximumNumberOfPresetTours = xml_node_soap_get(p_node, "MaximumNumberOfPresetTours");
    if (p_MaximumNumberOfPresetTours && p_MaximumNumberOfPresetTours->data)
    {
    	p_req->MaximumNumberOfPresetTours = atoi(p_MaximumNumberOfPresetTours->data);
    }

    p_PTZPresetTourOperation = xml_node_soap_get(p_node, "PTZPresetTourOperation");
    while (p_PTZPresetTourOperation && p_PTZPresetTourOperation->data && soap_strcmp(p_PTZPresetTourOperation->name, "PTZPresetTourOperation") == 0)
    {
    	if (strcasecmp(p_PTZPresetTourOperation->data, "Start") == 0)
    	{
    		p_req->PTZPresetTourOperation_Start = 1;
    	}
    	else if (strcasecmp(p_PTZPresetTourOperation->data, "Stop") == 0)
    	{
    		p_req->PTZPresetTourOperation_Stop = 1;
    	}
    	else if (strcasecmp(p_PTZPresetTourOperation->data, "Pause") == 0)
    	{
    		p_req->PTZPresetTourOperation_Pause = 1;
    	}
    	else if (strcasecmp(p_PTZPresetTourOperation->data, "Extended") == 0)
    	{
    		p_req->PTZPresetTourOperation_Extended = 1;
    	}

    	p_PTZPresetTourOperation = p_PTZPresetTourOperation->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_PTZNode(XMLN * p_node, onvif_PTZNode * p_req)
{
	XMLN * p_Name;
	XMLN * p_SupportedPTZSpaces;
	XMLN * p_MaximumNumberOfPresets;
	XMLN * p_HomeSupported;
	XMLN * p_AuxiliaryCommands;
	XMLN * p_Extension;
    const char * p_FixedHomePosition;
	const char * p_token;
	const char * p_GeoMove;

	p_FixedHomePosition = xml_attr_get(p_node, "FixedHomePosition");
	if (p_FixedHomePosition)
	{
		p_req->FixedHomePosition =  parse_Bool(p_FixedHomePosition);
	}

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
    	strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_GeoMove = xml_attr_get(p_node, "GeoMove");
	if (p_GeoMove)
	{
		p_req->GeoMove =  parse_Bool(p_GeoMove);
	}
        
	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		p_req->NameFlag = 1;
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_SupportedPTZSpaces = xml_node_soap_get(p_node, "SupportedPTZSpaces");
	if (p_SupportedPTZSpaces)
	{
		XMLN * p_AbsolutePanTiltPositionSpace;
		XMLN * p_AbsoluteZoomPositionSpace;
		XMLN * p_RelativePanTiltTranslationSpace;
		XMLN * p_RelativeZoomTranslationSpace;
		XMLN * p_ContinuousPanTiltVelocitySpace;
		XMLN * p_ContinuousZoomVelocitySpace;
		XMLN * p_PanTiltSpeedSpace;
		XMLN * p_ZoomSpeedSpace;

		p_AbsolutePanTiltPositionSpace = xml_node_soap_get(p_SupportedPTZSpaces, "AbsolutePanTiltPositionSpace");
		if (p_AbsolutePanTiltPositionSpace)
		{
			p_req->SupportedPTZSpaces.AbsolutePanTiltPositionSpaceFlag = 1;			
			parse_Space2DDescription(p_AbsolutePanTiltPositionSpace, &p_req->SupportedPTZSpaces.AbsolutePanTiltPositionSpace);
		}

		p_AbsoluteZoomPositionSpace = xml_node_soap_get(p_SupportedPTZSpaces, "AbsoluteZoomPositionSpace");
		if (p_AbsoluteZoomPositionSpace)
		{
			p_req->SupportedPTZSpaces.AbsoluteZoomPositionSpaceFlag = 1;			
			parse_Space1DDescription(p_AbsoluteZoomPositionSpace, &p_req->SupportedPTZSpaces.AbsoluteZoomPositionSpace);
		}

		p_RelativePanTiltTranslationSpace = xml_node_soap_get(p_SupportedPTZSpaces, "RelativePanTiltTranslationSpace");
		if (p_RelativePanTiltTranslationSpace)
		{
			p_req->SupportedPTZSpaces.RelativePanTiltTranslationSpaceFlag = 1;			
			parse_Space2DDescription(p_RelativePanTiltTranslationSpace, &p_req->SupportedPTZSpaces.RelativePanTiltTranslationSpace);
		}

		p_RelativeZoomTranslationSpace = xml_node_soap_get(p_SupportedPTZSpaces, "RelativeZoomTranslationSpace");
		if (p_RelativeZoomTranslationSpace)
		{
			p_req->SupportedPTZSpaces.RelativeZoomTranslationSpaceFlag = 1;			
			parse_Space1DDescription(p_RelativeZoomTranslationSpace, &p_req->SupportedPTZSpaces.RelativeZoomTranslationSpace);
		}

		p_ContinuousPanTiltVelocitySpace = xml_node_soap_get(p_SupportedPTZSpaces, "ContinuousPanTiltVelocitySpace");
		if (p_ContinuousPanTiltVelocitySpace)
		{
			p_req->SupportedPTZSpaces.ContinuousPanTiltVelocitySpaceFlag = 1;			
			parse_Space2DDescription(p_ContinuousPanTiltVelocitySpace, &p_req->SupportedPTZSpaces.ContinuousPanTiltVelocitySpace);
		}

		p_ContinuousZoomVelocitySpace = xml_node_soap_get(p_SupportedPTZSpaces, "ContinuousZoomVelocitySpace");
		if (p_ContinuousZoomVelocitySpace)
		{
			p_req->SupportedPTZSpaces.ContinuousZoomVelocitySpaceFlag = 1;			
			parse_Space1DDescription(p_ContinuousZoomVelocitySpace, &p_req->SupportedPTZSpaces.ContinuousZoomVelocitySpace);
		}

		p_PanTiltSpeedSpace = xml_node_soap_get(p_SupportedPTZSpaces, "PanTiltSpeedSpace");
		if (p_PanTiltSpeedSpace)
		{
			p_req->SupportedPTZSpaces.PanTiltSpeedSpaceFlag = 1;			
			parse_Space1DDescription(p_PanTiltSpeedSpace, &p_req->SupportedPTZSpaces.PanTiltSpeedSpace);
		}

		p_ZoomSpeedSpace = xml_node_soap_get(p_SupportedPTZSpaces, "ZoomSpeedSpace");
		if (p_ZoomSpeedSpace)
		{
			p_req->SupportedPTZSpaces.ZoomSpeedSpaceFlag = 1;			
			parse_Space1DDescription(p_ZoomSpeedSpace, &p_req->SupportedPTZSpaces.ZoomSpeedSpace);
		}
	}

	p_MaximumNumberOfPresets = xml_node_soap_get(p_node, "MaximumNumberOfPresets");
	if (p_MaximumNumberOfPresets && p_MaximumNumberOfPresets->data)
	{
		p_req->MaximumNumberOfPresets = atoi(p_MaximumNumberOfPresets->data);
	}
	
	p_HomeSupported = xml_node_soap_get(p_node, "HomeSupported");
	if (p_HomeSupported && p_HomeSupported->data)
	{
		p_req->HomeSupported = parse_Bool(p_HomeSupported->data);
	}

    p_req->sizeAuxiliaryCommands = 0;
    
    p_AuxiliaryCommands = xml_node_soap_get(p_node, "AuxiliaryCommands");
    while (p_AuxiliaryCommands && p_AuxiliaryCommands->data && soap_strcmp(p_AuxiliaryCommands->name, "AuxiliaryCommands") == 0)
    {
        int idx = p_req->sizeAuxiliaryCommands;

        strncpy(p_req->AuxiliaryCommands[idx], p_AuxiliaryCommands->data, sizeof(p_req->AuxiliaryCommands[idx])-1);

        p_req->sizeAuxiliaryCommands++;
        if (p_req->sizeAuxiliaryCommands >= ARRAY_SIZE(p_req->AuxiliaryCommands))
        {
            break;
        }

        p_AuxiliaryCommands = p_AuxiliaryCommands->next;
    }
    
	p_Extension = xml_node_soap_get(p_node, "Extension");
	if (p_Extension)
	{
		XMLN * p_SupportedPresetTour;
		
		p_req->ExtensionFlag = 1;
		
		p_SupportedPresetTour = xml_node_soap_get(p_Extension, "SupportedPresetTour");
		if (p_SupportedPresetTour)
		{
		    p_req->Extension.SupportedPresetTourFlag = 1;
			parse_PTZPresetTourSupported(p_SupportedPresetTour, &p_req->Extension.SupportedPresetTour);
		}
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_PTZConfiguration(XMLN * p_node, onvif_PTZConfiguration * p_req)
{
	XMLN * p_Name;
	XMLN * p_UseCount;
	XMLN * p_NodeToken;
	XMLN * p_DefaultPTZSpeed;
	XMLN * p_DefaultPTZTimeout;
	XMLN * p_PanTiltLimits;
	XMLN * p_ZoomLimits;
	XMLN * p_Extension;
	const char * p_token;
	const char * p_MoveRamp;
	const char * p_PresetRamp;
	const char * p_PresetTourRamp;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_MoveRamp = xml_attr_get(p_node, "MoveRamp");
    if (p_MoveRamp)
    {
        p_req->MoveRampFlag = 1;
        p_req->MoveRamp = atoi(p_MoveRamp);
    }
    
    p_PresetRamp = xml_attr_get(p_node, "PresetRamp");
    if (p_PresetRamp)
    {
        p_req->PresetRampFlag = 1;
        p_req->PresetRamp = atoi(p_PresetRamp);
    }
    
    p_PresetTourRamp = xml_attr_get(p_node, "PresetTourRamp");
    if (p_PresetTourRamp)
    {
        p_req->PresetTourRampFlag = 1;
        p_req->PresetTourRamp = atoi(p_PresetTourRamp);
    }
    
	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_UseCount = xml_node_soap_get(p_node, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_req->UseCount = atoi(p_UseCount->data);
	}

	p_NodeToken = xml_node_soap_get(p_node, "NodeToken");
	if (p_NodeToken && p_NodeToken->data)
	{
		strncpy(p_req->NodeToken, p_NodeToken->data, sizeof(p_req->NodeToken)-1);
	}

	p_DefaultPTZSpeed = xml_node_soap_get(p_node, "DefaultPTZSpeed");
	if (p_DefaultPTZSpeed)
	{
		XMLN * p_PanTilt;
		XMLN * p_Zoom;

		p_req->DefaultPTZSpeedFlag = 1;
		
		p_PanTilt = xml_node_soap_get(p_DefaultPTZSpeed, "PanTilt");
		if (p_PanTilt)
		{
			p_req->DefaultPTZSpeed.PanTiltFlag = parse_Vector(p_PanTilt, &p_req->DefaultPTZSpeed.PanTilt);
		}

		p_Zoom = xml_node_soap_get(p_DefaultPTZSpeed, "Zoom");
		if (p_Zoom)
		{
			p_req->DefaultPTZSpeed.ZoomFlag = parse_Vector1D(p_Zoom, &p_req->DefaultPTZSpeed.Zoom);
		}
	}

	p_DefaultPTZTimeout = xml_node_soap_get(p_node, "DefaultPTZTimeout");
	if (p_DefaultPTZTimeout && p_DefaultPTZTimeout->data)
	{
		p_req->DefaultPTZTimeoutFlag = parse_XSDDuration(p_DefaultPTZTimeout->data, &p_req->DefaultPTZTimeout);
	}

	p_PanTiltLimits = xml_node_soap_get(p_node, "PanTiltLimits");
	if (p_PanTiltLimits)
	{
		XMLN * p_Range;
		
		p_req->PanTiltLimitsFlag = 1;
		
		p_Range = xml_node_soap_get(p_PanTiltLimits, "Range");
		if (p_Range)
		{
			XMLN * p_XRange;
			XMLN * p_YRange;

			p_XRange = xml_node_soap_get(p_Range, "XRange");
			if (p_XRange)
			{
				parse_FloatRange(p_XRange, &p_req->PanTiltLimits.XRange);
			}	

			p_YRange = xml_node_soap_get(p_Range, "YRange");
			if (p_YRange)
			{
				parse_FloatRange(p_YRange, &p_req->PanTiltLimits.YRange);
			}
		}
	}

	p_ZoomLimits = xml_node_soap_get(p_node, "ZoomLimits");
	if (p_ZoomLimits)
	{
		XMLN * p_Range;
		
		p_req->ZoomLimitsFlag = 1;
		
		p_Range = xml_node_soap_get(p_ZoomLimits, "Range");
		if (p_Range)
		{
			XMLN * p_XRange;

			p_XRange = xml_node_soap_get(p_Range, "XRange");
			if (p_XRange)
			{
				parse_FloatRange(p_XRange, &p_req->ZoomLimits.XRange);
			}
		}
	}

	p_Extension = xml_node_soap_get(p_node, "Extension");
	if (p_Extension)
	{
		XMLN * p_PTControlDirection;
		XMLN * p_EFlip;
		XMLN * p_Reverse;
		XMLN * p_Mode;
		
		p_req->ExtensionFlag = 1;
		
		p_PTControlDirection = xml_node_soap_get(p_Extension, "PTControlDirection");
		if (p_PTControlDirection)
		{
			p_req->Extension.PTControlDirectionFlag = 1;
			
			p_EFlip = xml_node_soap_get(p_PTControlDirection, "EFlip");
			if (p_EFlip)
			{
				p_req->Extension.PTControlDirection.EFlipFlag = 1;

				p_Mode = xml_node_soap_get(p_EFlip, "Mode");
				if (p_Mode && p_Mode->data)
				{
					p_req->Extension.PTControlDirection.EFlip = onvif_StringToEFlipMode(p_Mode->data);
				}
			}

			p_Reverse = xml_node_soap_get(p_PTControlDirection, "Reverse");
			if (p_Reverse)
			{
				p_req->Extension.PTControlDirection.ReverseFlag = 1;

				p_Mode = xml_node_soap_get(p_Reverse, "Mode");
				if (p_Mode && p_Mode->data)
				{
					p_req->Extension.PTControlDirection.Reverse = onvif_StringToReverseMode(p_Mode->data);
				}
			}
		}
	}
	
	return ONVIF_OK;
}

BOOL parse_PTZSpeed(XMLN * p_node, onvif_PTZSpeed * p_req)
{
	XMLN * p_PanTilt;
	XMLN * p_Zoom;

	p_PanTilt = xml_node_soap_get(p_node, "PanTilt");
	if (p_PanTilt)
	{
		p_req->PanTiltFlag = parse_Vector(p_PanTilt, &p_req->PanTilt);
	}

	p_Zoom = xml_node_soap_get(p_node, "Zoom");
	if (p_Zoom)
	{
		p_req->ZoomFlag = parse_Vector1D(p_Zoom, &p_req->Zoom);
	}

	return TRUE;
}

BOOL parse_PTZVector(XMLN * p_node, onvif_PTZVector * p_req)
{
	XMLN * p_PanTilt;
	XMLN * p_Zoom;

	p_PanTilt = xml_node_soap_get(p_node, "PanTilt");
	if (p_PanTilt)
	{	
		p_req->PanTiltFlag = parse_Vector(p_PanTilt, &p_req->PanTilt);
	}

	p_Zoom = xml_node_soap_get(p_node, "Zoom");
	if (p_Zoom)
	{	
		p_req->ZoomFlag = parse_Vector1D(p_Zoom, &p_req->Zoom);
	}

	return TRUE;
}

ONVIF_RET parse_PTZPreset(XMLN * p_node, onvif_PTZPreset * p_req)
{
    XMLN * p_Name;
    XMLN * p_PTZPosition;
    const char * p_token;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

    p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
	    strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_PTZPosition = xml_node_soap_get(p_node, "PTZPosition");
	if (p_PTZPosition)
	{
	    p_req->PTZPositionFlag = 1;
	    parse_PTZVector(p_PTZPosition, &p_req->PTZPosition);
	}
    
    return ONVIF_OK;
}

BOOL parse_PTZPresetTourPresetDetail(XMLN * p_node, onvif_PTZPresetTourPresetDetail * p_req)
{
    XMLN * p_PresetToken;
    XMLN * p_Home;
    XMLN * p_PTZPosition;

    p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
	if (p_PresetToken && p_PresetToken->data)
	{
	    p_req->PresetTokenFlag = 1;
	    strncpy(p_req->PresetToken, p_PresetToken->data, sizeof(p_req->PresetToken)-1);
	}

	p_Home = xml_node_soap_get(p_node, "Home");
	if (p_Home && p_Home->data)
	{
	    p_req->HomeFlag = 1;
	    p_req->Home = parse_Bool(p_Home->data);
	}

	p_PTZPosition = xml_node_soap_get(p_node, "PTZPosition");
	if (p_PTZPosition)
    {
        p_req->PTZPositionFlag = parse_PTZVector(p_PTZPosition, &p_req->PTZPosition);
    }	

	return TRUE;
}

BOOL parse_PTZPresetTourSpot(XMLN * p_node, onvif_PTZPresetTourSpot * p_req)
{
    XMLN * p_PresetDetail;
    XMLN * p_Speed;
    XMLN * p_StayTime;

    p_PresetDetail = xml_node_soap_get(p_node, "PresetDetail");
	if (p_PresetDetail)
	{
	    parse_PTZPresetTourPresetDetail(p_PresetDetail, &p_req->PresetDetail);
	}

    p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{
	    p_req->SpeedFlag = parse_PTZSpeed(p_Speed, &p_req->Speed);
	}

	p_StayTime = xml_node_soap_get(p_node, "StayTime");
	if (p_StayTime && p_StayTime->data)
	{
	    p_req->StayTimeFlag = parse_XSDDuration(p_StayTime->data, &p_req->StayTime);	     
	}

	return TRUE;
}

BOOL parse_PTZPresetTourStatus(XMLN * p_node, onvif_PTZPresetTourStatus * p_req)
{
    XMLN * p_State;
    XMLN * p_CurrentTourSpot;
    
    p_State = xml_node_soap_get(p_node, "State");
	if (p_State && p_State->data)
	{
	    p_req->State = onvif_StringToPTZPresetTourState(p_State->data);
	}

	p_CurrentTourSpot = xml_node_soap_get(p_node, "CurrentTourSpot");
	if (p_CurrentTourSpot)
	{
	    p_req->CurrentTourSpotFlag = parse_PTZPresetTourSpot(p_CurrentTourSpot, &p_req->CurrentTourSpot);
	}

	return TRUE;
}

BOOL parse_PTZPresetTourStartingCondition(XMLN * p_node, onvif_PTZPresetTourStartingCondition * p_req)
{
    XMLN * p_RecurringTime;
    XMLN * p_RecurringDuration;
    XMLN * p_Direction;
    const char * p_RandomPresetOrder;

    p_RandomPresetOrder = xml_attr_get(p_node, "RandomPresetOrder");
	if (p_RandomPresetOrder)
	{
	    p_req->RandomPresetOrderFlag = 1;
	    p_req->RandomPresetOrder = parse_Bool(p_RandomPresetOrder);
	}

    p_RecurringTime = xml_node_soap_get(p_node, "RecurringTime");
	if (p_RecurringTime && p_RecurringTime->data)
	{
	    p_req->RecurringTimeFlag = 1;
	    p_req->RecurringTime = atoi(p_RecurringTime->data);
	}

	p_RecurringDuration = xml_node_soap_get(p_node, "RecurringDuration");
	if (p_RecurringDuration && p_RecurringDuration->data)
	{
		p_req->RecurringDurationFlag = parse_XSDDuration(p_RecurringDuration->data, &p_req->RecurringDuration);
	}

	p_Direction = xml_node_soap_get(p_node, "Direction");
	if (p_Direction && p_Direction->data)
	{
	    p_req->DirectionFlag = 1;
		p_req->Direction = onvif_StringToPTZPresetTourDirection(p_Direction->data);
	}

	return TRUE;
}

BOOL parse_PresetTour(XMLN * p_node, onvif_PresetTour * p_req)
{
    XMLN * p_Name;
    XMLN * p_Status;
    XMLN * p_AutoStart;
    XMLN * p_StartingCondition;
    XMLN * p_TourSpot;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	p_Status = xml_node_soap_get(p_node, "Status");
	if (p_Status)
	{
	    parse_PTZPresetTourStatus(p_Status, &p_req->Status);
	}

	p_AutoStart = xml_node_soap_get(p_node, "AutoStart");
	if (p_AutoStart && p_AutoStart->data)
	{
		p_req->AutoStart = parse_Bool(p_AutoStart->data);
	}

	p_StartingCondition = xml_node_soap_get(p_node, "StartingCondition");
	if (p_StartingCondition)
	{
	    parse_PTZPresetTourStartingCondition(p_StartingCondition, &p_req->StartingCondition);
	}

	p_TourSpot = xml_node_soap_get(p_node, "TourSpot");
	while (p_TourSpot && soap_strcmp(p_TourSpot->name, "TourSpot") == 0)
	{
        PTZPresetTourSpotList * p_spot = onvif_add_PTZPresetTourSpot(&p_req->TourSpot);
        if (p_spot)
        {
            memset(p_spot, 0, sizeof(PTZPresetTourSpotList));

            parse_PTZPresetTourSpot(p_TourSpot, &p_spot->PTZPresetTourSpot);
        }
	    
	    p_TourSpot = p_TourSpot->next;
	}

	return TRUE;
}

BOOL parse_GeoLocation(XMLN * p_node, onvif_GeoLocation * p_req)
{
    const char * p_lon;
    const char * p_lat;
    const char * p_elevation;

    p_lon = xml_attr_get(p_node, "lon");
	if (p_lon)
	{
		p_req->lonFlag = 1;
		p_req->lon = atof(p_lon);
	}

	p_lat = xml_attr_get(p_node, "lat");
	if (p_lat)
	{
		p_req->latFlag = 1;
		p_req->lat = atof(p_lat);
	}

	p_elevation = xml_attr_get(p_node, "elevation");
	if (p_elevation)
	{
		p_req->elevationFlag = 1;
		p_req->elevation = (float) atof(p_elevation);
	}

	return TRUE;
}

ONVIF_RET parse_ptz_GetCompatibleConfigurations(XMLN * p_node, ptz_GetCompatibleConfigurations_REQ * p_req)
{
	XMLN * p_ProfileToken;	    

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_SetConfiguration(XMLN * p_node, ptz_SetConfiguration_REQ * p_req)
{
	XMLN * p_PTZConfiguration;
	XMLN * p_ForcePersistence;	    

	p_PTZConfiguration = xml_node_soap_get(p_node, "PTZConfiguration");
	if (p_PTZConfiguration)
	{
		parse_PTZConfiguration(p_PTZConfiguration, &p_req->PTZConfiguration);
	}

	p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);		
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_ContinuousMove(XMLN * p_node, ptz_ContinuousMove_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_Velocity;
	XMLN * p_Timeout;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	p_Velocity = xml_node_soap_get(p_node, "Velocity");
	if (p_Velocity)
	{	
		parse_PTZSpeed(p_Velocity, &p_req->Velocity);
	}

	p_Timeout = xml_node_soap_get(p_node, "Timeout");
	if (p_Timeout && p_Timeout->data)
	{
		p_req->TimeoutFlag = parse_XSDDuration(p_Timeout->data, &p_req->Timeout);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_ptz_Stop(XMLN * p_node, ptz_Stop_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_PanTilt;
	XMLN * p_Zoom;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}
	
	p_PanTilt = xml_node_soap_get(p_node, "PanTilt");
	if (p_PanTilt && p_PanTilt->data)
	{
		p_req->PanTiltFlag = 1;
		p_req->PanTilt = parse_Bool(p_PanTilt->data);
	}

	p_Zoom = xml_node_soap_get(p_node, "Zoom");
	if (p_Zoom && p_Zoom->data)
	{
		p_req->ZoomFlag = 1;
		p_req->Zoom = parse_Bool(p_Zoom->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_AbsoluteMove(XMLN * p_node, ptz_AbsoluteMove_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_Position;
	XMLN * p_Speed;
	
    assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

    p_Position = xml_node_soap_get(p_node, "Position");
	if (p_Position)
	{	
	    parse_PTZVector(p_Position, &p_req->Position);
	}

	p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
		p_req->SpeedFlag = parse_PTZSpeed(p_Speed, &p_req->Speed);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_RelativeMove(XMLN * p_node, ptz_RelativeMove_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_Translation;
	XMLN * p_Speed;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_Translation = xml_node_soap_get(p_node, "Translation");
	if (p_Translation)
	{	
		parse_PTZVector(p_Translation, &p_req->Translation);
	}

	p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
		p_req->SpeedFlag = parse_PTZSpeed(p_Speed, &p_req->Speed);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_SetPreset(XMLN * p_node, ptz_SetPreset_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_PresetName;
	XMLN * p_PresetToken;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetName = xml_node_soap_get(p_node, "PresetName");
	if (p_PresetName && p_PresetName->data)
	{
		p_req->PresetNameFlag = 1;
		strncpy(p_req->PresetName, p_PresetName->data, sizeof(p_req->PresetName)-1);
	}

	p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
	if (p_PresetToken && p_PresetToken->data)
	{
		p_req->PresetTokenFlag = 1;
		strncpy(p_req->PresetToken, p_PresetToken->data, sizeof(p_req->PresetToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_RemovePreset(XMLN * p_node, ptz_RemovePreset_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_PresetToken;
	
	assert(p_node);

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
	if (p_PresetToken && p_PresetToken->data)
	{
		strncpy(p_req->PresetToken, p_PresetToken->data, sizeof(p_req->PresetToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_GotoPreset(XMLN * p_node, ptz_GotoPreset_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_PresetToken;
	XMLN * p_Speed;
	
	assert(p_node);

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
	if (p_PresetToken && p_PresetToken->data)
	{
		strncpy(p_req->PresetToken, p_PresetToken->data, sizeof(p_req->PresetToken)-1);
	}

	p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
		p_req->SpeedFlag = parse_PTZSpeed(p_Speed, &p_req->Speed);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_GotoHomePosition(XMLN * p_node, ptz_GotoHomePosition_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_Speed;
	
    assert(p_node);

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
		p_req->SpeedFlag = parse_PTZSpeed(p_Speed, &p_req->Speed);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_GetPresetTours(XMLN * p_node, ptz_GetPresetTours_REQ * p_req)
{
	XMLN * p_ProfileToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_GetPresetTour(XMLN * p_node, ptz_GetPresetTour_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_PresetTourToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetTourToken = xml_node_soap_get(p_node, "PresetTourToken");
	if (p_PresetTourToken && p_PresetTourToken->data)
	{
		strncpy(p_req->PresetTourToken, p_PresetTourToken->data, sizeof(p_req->PresetTourToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_GetPresetTourOptions(XMLN * p_node, ptz_GetPresetTourOptions_REQ * p_req)
{
	XMLN * p_ProfileToken;
	XMLN * p_PresetTourToken;

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetTourToken = xml_node_soap_get(p_node, "PresetTourToken");
	if (p_PresetTourToken && p_PresetTourToken->data)
	{
		p_req->PresetTourTokenFlag = 1;
		strncpy(p_req->PresetTourToken, p_PresetTourToken->data, sizeof(p_req->PresetTourToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_CreatePresetTour(XMLN * p_node, ptz_CreatePresetTour_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_ModifyPresetTour(XMLN * p_node, ptz_ModifyPresetTour_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_PresetTour;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetTour = xml_node_soap_get(p_node, "PresetTour");
	if (p_PresetTour)
	{
	    parse_PresetTour(p_PresetTour, &p_req->PresetTour);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_OperatePresetTour(XMLN * p_node, ptz_OperatePresetTour_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_PresetTourToken;
    XMLN * p_Operation;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetTourToken = xml_node_soap_get(p_node, "PresetTourToken");
	if (p_PresetTourToken && p_PresetTourToken->data)
	{
		strncpy(p_req->PresetTourToken, p_PresetTourToken->data, sizeof(p_req->PresetTourToken)-1);
	}

	p_Operation = xml_node_soap_get(p_node, "Operation");
	if (p_Operation && p_Operation->data)
	{
	    p_req->Operation = onvif_StringToPTZPresetTourOperation(p_Operation->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_RemovePresetTour(XMLN * p_node, ptz_RemovePresetTour_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_PresetTourToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_PresetTourToken = xml_node_soap_get(p_node, "PresetTourToken");
	if (p_PresetTourToken && p_PresetTourToken->data)
	{
		strncpy(p_req->PresetTourToken, p_PresetTourToken->data, sizeof(p_req->PresetTourToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_SendAuxiliaryCommand(XMLN * p_node, ptz_SendAuxiliaryCommand_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_AuxiliaryData;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_AuxiliaryData = xml_node_soap_get(p_node, "AuxiliaryData");
	if (p_AuxiliaryData && p_AuxiliaryData->data)
	{
		strncpy(p_req->AuxiliaryData, p_AuxiliaryData->data, sizeof(p_req->AuxiliaryData)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ptz_GeoMove(XMLN * p_node, ptz_GeoMove_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_Target;
    XMLN * p_Speed;
    XMLN * p_AreaHeight;
    XMLN * p_AreaWidth;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	p_Target = xml_node_soap_get(p_node, "Target");
	if (p_Target)
	{
	    parse_GeoLocation(p_Target, &p_req->Target);
	}

	p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{
	    p_req->SpeedFlag = parse_PTZSpeed(p_Speed, &p_req->Speed);
	}

	p_AreaHeight = xml_node_soap_get(p_node, "AreaHeight");
	if (p_AreaHeight && p_AreaHeight->data)
	{
	    p_req->AreaHeightFlag = 1;
		p_req->AreaHeight = (float) atof(p_AreaHeight->data);
	}

	p_AreaWidth = xml_node_soap_get(p_node, "AreaWidth");
	if (p_AreaWidth && p_AreaWidth->data)
	{
	    p_req->AreaWidthFlag = 1;
		p_req->AreaWidth = (float) atof(p_AreaWidth->data);
	}

	return ONVIF_OK;
}

#endif // PTZ_SUPPORT

#ifdef PROFILE_G_SUPPORT

ONVIF_RET parse_RecordingConfiguration(XMLN * p_node, onvif_RecordingConfiguration * p_req)
{
	XMLN * p_Source;
	XMLN * p_Content;
	XMLN * p_MaximumRetentionTime;

	p_Source = xml_node_soap_get(p_node, "Source");
	if (p_Source)
	{
		XMLN * p_SourceId;
		XMLN * p_Name;
		XMLN * p_Location;
		XMLN * p_Description;
		XMLN * p_Address;

		p_SourceId = xml_node_soap_get(p_Source, "SourceId");
		if (p_SourceId && p_SourceId->data)
		{
			strncpy(p_req->Source.SourceId, p_SourceId->data, sizeof(p_req->Source.SourceId)-1);
		}

		p_Name = xml_node_soap_get(p_Source, "Name");
		if (p_Name && p_Name->data)
		{
			strncpy(p_req->Source.Name, p_Name->data, sizeof(p_req->Source.Name)-1);
		}

		p_Location = xml_node_soap_get(p_Source, "Location");
		if (p_Location && p_Location->data)
		{
			strncpy(p_req->Source.Location, p_Location->data, sizeof(p_req->Source.Location)-1);
		}

		p_Description = xml_node_soap_get(p_Source, "Description");
		if (p_Description && p_Description->data)
		{
			strncpy(p_req->Source.Description, p_Description->data, sizeof(p_req->Source.Description)-1);
		}

		p_Address = xml_node_soap_get(p_Source, "Address");
		if (p_Address && p_Address->data)
		{
			strncpy(p_req->Source.Address, p_Address->data, sizeof(p_req->Source.Address)-1);
		}
	}

	p_Content = xml_node_soap_get(p_node, "Content");
	if (p_Content && p_Content->data)
	{
		strncpy(p_req->Content, p_Content->data, sizeof(p_req->Content)-1);
	}

	p_MaximumRetentionTime = xml_node_soap_get(p_node, "MaximumRetentionTime");
	if (p_MaximumRetentionTime && p_MaximumRetentionTime->data)
	{
		p_req->MaximumRetentionTimeFlag = parse_XSDDuration(p_MaximumRetentionTime->data, (int*)&p_req->MaximumRetentionTime);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_TrackConfiguration(XMLN * p_node, onvif_TrackConfiguration * p_req)
{
	XMLN * p_TrackType;
	XMLN * p_Description;

	p_TrackType = xml_node_soap_get(p_node, "TrackType");
	if (p_TrackType && p_TrackType->data)
	{
		p_req->TrackType = onvif_StringToTrackType(p_TrackType->data);
		if (TrackType_Invalid == p_req->TrackType)
		{
			return ONVIF_ERR_BadConfiguration;
		}
	}

	p_Description = xml_node_soap_get(p_node, "Description");
	if (p_Description && p_Description->data)
	{
		strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_Track(XMLN * p_node, onvif_Track * p_req)
{
    XMLN * p_TrackToken;
    XMLN * p_Configuration;

    p_TrackToken = xml_node_soap_get(p_node, "TrackToken");
    if (p_TrackToken && p_TrackToken->data)
    {
        strncpy(p_req->TrackToken, p_TrackToken->data, sizeof(p_req->TrackToken)-1);
    }

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_TrackConfiguration(p_Configuration, &p_req->Configuration);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_Recording(XMLN * p_node, onvif_Recording * p_req)
{
    XMLN * p_RecordingToken;
    XMLN * p_Configuration;
    XMLN * p_Tracks;

    p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
	    strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_RecordingConfiguration(p_Configuration, &p_req->Configuration);
	}

	p_Tracks = xml_node_soap_get(p_node, "Tracks");
	if (p_Tracks)
	{
	    XMLN * p_Track;

	    p_Track = xml_node_soap_get(p_Tracks, "Track");
	    while (p_Track && soap_strcmp(p_Track->name, "Track") == 0)
	    {
	        TrackList * p_item = onvif_add_Track(&p_req->Tracks);
	        if (p_item)
	        {
	            parse_Track(p_Track, &p_item->Track);
	        }

	        p_Track = p_Track->next;
	    }
	}

	return ONVIF_OK;
}

ONVIF_RET parse_JobConfiguration(XMLN * p_node, onvif_RecordingJobConfiguration * p_req)
{
	XMLN * p_RecordingToken;
	XMLN * p_Mode;
	XMLN * p_Priority;
	XMLN * p_Source;

	p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	p_Mode = xml_node_soap_get(p_node, "Mode");
	if (p_Mode && p_Mode->data)
	{
		strncpy(p_req->Mode, p_Mode->data, sizeof(p_req->Mode)-1);
	}

	p_Priority = xml_node_soap_get(p_node, "Priority");
	if (p_Priority && p_Priority->data)
	{
		p_req->Priority = atoi(p_Priority->data);
	}

	p_Source = xml_node_soap_get(p_node, "Source");
	while (p_Source && soap_strcmp(p_Source->name, "Source") == 0)
	{
		int i = p_req->sizeSource;
		XMLN * p_SourceToken;
		XMLN * p_AutoCreateReceiver;
		XMLN * p_Tracks;

		p_SourceToken = xml_node_soap_get(p_Source, "SourceToken");
		if (p_SourceToken)
		{
			const char * p_Type;
			XMLN * p_Token;

			p_req->Source[i].SourceTokenFlag = 1;
			
			p_Type = xml_attr_get(p_SourceToken, "Type");
			if (p_Type)
			{
				p_req->Source[i].SourceToken.TypeFlag = 1;
				strncpy(p_req->Source[i].SourceToken.Type, p_Type, sizeof(p_req->Source[i].SourceToken.Type)-1);
			}

			p_Token = xml_node_soap_get(p_SourceToken, "Token");
			if (p_Token && p_Token->data)
			{
				strncpy(p_req->Source[i].SourceToken.Token, p_Token->data, sizeof(p_req->Source[i].SourceToken.Token)-1);
			}
		}

		p_AutoCreateReceiver = xml_node_soap_get(p_Source, "AutoCreateReceiver");
		if (p_AutoCreateReceiver && p_AutoCreateReceiver->data)
		{
			p_req->Source[i].AutoCreateReceiverFlag = 1;
			p_req->Source[i].AutoCreateReceiver = parse_Bool(p_AutoCreateReceiver->data);
		}

		p_Tracks = xml_node_soap_get(p_Source, "Tracks");
		while (p_Tracks && soap_strcmp(p_Tracks->name, "Tracks") == 0)
		{
			int j = p_req->Source[i].sizeTracks;
			XMLN * p_SourceTag;
			XMLN * p_Destination;

			p_SourceTag = xml_node_soap_get(p_Tracks, "SourceTag");
			if (p_SourceTag && p_SourceTag->data)
			{
				strncpy(p_req->Source[i].Tracks[j].SourceTag, p_SourceTag->data, sizeof(p_req->Source[i].Tracks[j].SourceTag)-1);
			}

			p_Destination = xml_node_soap_get(p_Tracks, "Destination");
			if (p_Destination && p_Destination->data)
			{
				strncpy(p_req->Source[i].Tracks[j].Destination, p_Destination->data, sizeof(p_req->Source[i].Tracks[j].Destination)-1);
			}

			p_req->Source[i].sizeTracks++;
			if (p_req->Source[i].sizeTracks >= ARRAY_SIZE(p_req->Source[i].Tracks))
			{
				break;
			}

			p_Tracks = p_Tracks->next;
		}
		
		p_req->sizeSource++;
		if (p_req->sizeSource >= ARRAY_SIZE(p_req->Source))
		{
			break;
		}

		p_Source = p_Source->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_RecordingJob(XMLN * p_node, onvif_RecordingJob * p_req)
{
    XMLN * p_JobToken;
    XMLN * p_JobConfiguration;

    p_JobToken = xml_node_soap_get(p_node, "JobToken");
	if (p_JobToken && p_JobToken->data)
	{
		strncpy(p_req->JobToken, p_JobToken->data, sizeof(p_req->JobToken)-1);
	}

	p_JobConfiguration = xml_node_soap_get(p_node, "JobConfiguration");
	if (p_JobConfiguration)
	{
		parse_JobConfiguration(p_JobConfiguration, &p_req->JobConfiguration);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_SearchScope(XMLN * p_node, onvif_SearchScope * p_req)
{
	XMLN * p_IncludedSources;
	XMLN * p_IncludedRecordings;
	XMLN * p_RecordingInformationFilter;

	p_IncludedSources = xml_node_soap_get(p_node, "IncludedSources");
	while (p_IncludedSources && soap_strcmp(p_IncludedSources->name, "IncludedSources") == 0)
	{
		int idx = p_req->sizeIncludedSources;
		const char * p_Type;
		XMLN * p_Token;

		p_Type = xml_attr_get(p_IncludedSources, "Type");
		if (p_Type)
		{
			p_req->IncludedSources[idx].TypeFlag = 1;
			strncpy(p_req->IncludedSources[idx].Type, p_Type, sizeof(p_req->IncludedSources[idx].Type));			
		}

		p_Token = xml_node_soap_get(p_IncludedSources, "Token");
		if (p_Token && p_Token->data)
		{
			strncpy(p_req->IncludedSources[idx].Token, p_Token->data, sizeof(p_req->IncludedSources[idx].Token));
		}
		
		p_req->sizeIncludedSources++;
		if (p_req->sizeIncludedSources >= ARRAY_SIZE(p_req->IncludedSources))
		{
			break;
		}

		p_IncludedSources = p_IncludedSources->next;
	}

	p_IncludedRecordings = xml_node_soap_get(p_node, "IncludedRecordings");
	while (p_IncludedRecordings && p_IncludedRecordings->data && soap_strcmp(p_IncludedRecordings->name, "IncludedRecordings") == 0)
	{
		int idx = p_req->sizeIncludedRecordings;
		
		strncpy(p_req->IncludedRecordings[idx], p_IncludedRecordings->data, sizeof(p_req->IncludedRecordings[idx])-1);

		p_req->sizeIncludedRecordings++;
		if (p_req->sizeIncludedRecordings >= ARRAY_SIZE(p_req->IncludedRecordings))
		{
			break;
		}

		p_IncludedRecordings = p_IncludedRecordings->next;
	}

	p_RecordingInformationFilter = xml_node_soap_get(p_node, "RecordingInformationFilter");
	if (p_RecordingInformationFilter && p_RecordingInformationFilter->data)
	{
		strncpy(p_req->RecordingInformationFilter, p_RecordingInformationFilter->data, sizeof(p_req->RecordingInformationFilter));
	}

	return ONVIF_OK;
}

ONVIF_RET parse_StorageReferencePath(XMLN * p_node, onvif_StorageReferencePath * p_req)
{
    XMLN * p_StorageToken;
    XMLN * p_RelativePath;

    p_StorageToken = xml_node_soap_get(p_node, "StorageToken");
	if (p_StorageToken && p_StorageToken->data)
	{
		strncpy(p_req->StorageToken, p_StorageToken->data, sizeof(p_req->StorageToken)-1);
	}

	p_RelativePath = xml_node_soap_get(p_node, "RelativePath");
	if (p_RelativePath && p_RelativePath->data)
	{
		strncpy(p_req->RelativePath, p_RelativePath->data, sizeof(p_req->RelativePath)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trc_CreateRecording(XMLN * p_node, trc_CreateRecording_REQ * p_req)
{
	XMLN * p_RecordingConfiguration;

	p_RecordingConfiguration = xml_node_soap_get(p_node, "RecordingConfiguration");
	if (NULL == p_RecordingConfiguration)
	{
		return ONVIF_ERR_BadConfiguration;
	}
	else
	{
		return parse_RecordingConfiguration(p_RecordingConfiguration, &p_req->RecordingConfiguration);
	}
}

ONVIF_RET parse_trc_SetRecordingConfiguration(XMLN * p_node, trc_SetRecordingConfiguration_REQ * p_req)
{
	XMLN * p_RecordingToken;
	XMLN * p_RecordingConfiguration;

	p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}
	
	p_RecordingConfiguration = xml_node_soap_get(p_node, "RecordingConfiguration");
	if (p_RecordingConfiguration)
	{
		return parse_RecordingConfiguration(p_RecordingConfiguration, &p_req->RecordingConfiguration);
	}
	else
	{
		return ONVIF_ERR_BadConfiguration;
	}
}

ONVIF_RET parse_trc_CreateTrack(XMLN * p_node, trc_CreateTrack_REQ * p_req)
{
	XMLN * p_RecordingToken;
	XMLN * p_TrackConfiguration;

	p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	p_TrackConfiguration = xml_node_soap_get(p_node, "TrackConfiguration");
	if (p_TrackConfiguration)
	{
		return parse_TrackConfiguration(p_TrackConfiguration, &p_req->TrackConfiguration);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trc_DeleteTrack(XMLN * p_node, trc_DeleteTrack_REQ * p_req)
{
	XMLN * p_RecordingToken;
	XMLN * p_TrackToken;

	p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	p_TrackToken = xml_node_soap_get(p_node, "TrackToken");
	if (p_TrackToken && p_TrackToken->data)
	{
		strncpy(p_req->TrackToken, p_TrackToken->data, sizeof(p_req->TrackToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trc_GetTrackConfiguration(XMLN * p_node, trc_GetTrackConfiguration_REQ * p_req)
{
	XMLN * p_RecordingToken;
	XMLN * p_TrackToken;

	p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	p_TrackToken = xml_node_soap_get(p_node, "TrackToken");
	if (p_TrackToken && p_TrackToken->data)
	{
		strncpy(p_req->TrackToken, p_TrackToken->data, sizeof(p_req->TrackToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trc_SetTrackConfiguration(XMLN * p_node, trc_SetTrackConfiguration_REQ * p_req)
{
	XMLN * p_RecordingToken;
	XMLN * p_TrackToken;
	XMLN * p_TrackConfiguration;

	p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	p_TrackToken = xml_node_soap_get(p_node, "TrackToken");
	if (p_TrackToken && p_TrackToken->data)
	{
		strncpy(p_req->TrackToken, p_TrackToken->data, sizeof(p_req->TrackToken)-1);
	}

	p_TrackConfiguration = xml_node_soap_get(p_node, "TrackConfiguration");
	if (p_TrackConfiguration)
	{
		return parse_TrackConfiguration(p_TrackConfiguration, &p_req->TrackConfiguration);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_trc_CreateRecordingJob(XMLN * p_node, trc_CreateRecordingJob_REQ * p_req)
{
	XMLN * p_JobConfiguration;
	
	p_JobConfiguration = xml_node_soap_get(p_node, "JobConfiguration");
	if (p_JobConfiguration)
	{
		return parse_JobConfiguration(p_JobConfiguration, &p_req->JobConfiguration);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trc_SetRecordingJobConfiguration(XMLN * p_node, trc_SetRecordingJobConfiguration_REQ * p_req)
{
	XMLN * p_JobToken;
	XMLN * p_JobConfiguration;

	p_JobToken = xml_node_soap_get(p_node, "JobToken");
	if (p_JobToken && p_JobToken->data)
	{
		strncpy(p_req->JobToken, p_JobToken->data, sizeof(p_req->JobToken)-1);
	}
	
	p_JobConfiguration = xml_node_soap_get(p_node, "JobConfiguration");
	if (p_JobConfiguration)
	{
		return parse_JobConfiguration(p_JobConfiguration, &p_req->JobConfiguration);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trc_SetRecordingJobMode(XMLN * p_node, trc_SetRecordingJobMode_REQ * p_req)
{
	XMLN * p_JobToken;
	XMLN * p_Mode;

	p_JobToken = xml_node_soap_get(p_node, "JobToken");
	if (p_JobToken && p_JobToken->data)
	{
		strncpy(p_req->JobToken, p_JobToken->data, sizeof(p_req->JobToken)-1);
	}
	
	p_Mode = xml_node_soap_get(p_node, "Mode");
	if (p_Mode && p_Mode->data)
	{
		strncpy(p_req->Mode, p_Mode->data, sizeof(p_req->Mode));
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_trc_ExportRecordedData(XMLN * p_node, trc_ExportRecordedData_REQ * p_req)
{
    XMLN * p_StartPoint;
    XMLN * p_EndPoint;
    XMLN * p_SearchScope;
    XMLN * p_FileFormat;
    XMLN * p_StorageDestination;

    p_StartPoint = xml_node_soap_get(p_node, "StartPoint");
	if (p_StartPoint && p_StartPoint->data)
	{
		parse_XSDDatetime(p_StartPoint->data, &p_req->StartPoint);
	}

	p_EndPoint = xml_node_soap_get(p_node, "EndPoint");
	if (p_EndPoint && p_EndPoint->data)
	{
		parse_XSDDatetime(p_EndPoint->data, &p_req->EndPoint);
	}

	p_SearchScope = xml_node_soap_get(p_node, "SearchScope");
	if (p_SearchScope)
	{
	    parse_SearchScope(p_SearchScope, &p_req->SearchScope);
	}

	p_FileFormat = xml_node_soap_get(p_node, "FileFormat");
	if (p_FileFormat && p_FileFormat->data)
	{
		strncpy(p_req->FileFormat, p_FileFormat->data, sizeof(p_req->FileFormat)-1);
	}

	p_StorageDestination = xml_node_soap_get(p_node, "StorageDestination");
	if (p_StorageDestination)
	{
		parse_StorageReferencePath(p_StorageDestination, &p_req->StorageDestination);
	}
    
    return ONVIF_OK;
}

ONVIF_RET parse_trc_StopExportRecordedData(XMLN * p_node, trc_StopExportRecordedData_REQ * p_req)
{
    XMLN * p_OperationToken;

    p_OperationToken = xml_node_soap_get(p_node, "OperationToken");
	if (p_OperationToken && p_OperationToken->data)
	{
		strncpy(p_req->OperationToken, p_OperationToken->data, sizeof(p_req->OperationToken)-1);
	}
	
    return ONVIF_OK;
}

ONVIF_RET parse_trc_GetExportRecordedDataState(XMLN * p_node, trc_GetExportRecordedDataState_REQ * p_req)
{
    XMLN * p_OperationToken;

    p_OperationToken = xml_node_soap_get(p_node, "OperationToken");
	if (p_OperationToken && p_OperationToken->data)
	{
		strncpy(p_req->OperationToken, p_OperationToken->data, sizeof(p_req->OperationToken)-1);
	}
	
    return ONVIF_OK;
}

ONVIF_RET parse_tse_GetRecordingInformation(XMLN * p_node, tse_GetRecordingInformation_REQ * p_req)
{
    XMLN * p_RecordingToken;

    p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tse_GetMediaAttributes(XMLN * p_node, tse_GetMediaAttributes_REQ * p_req)
{
	int idx;
	XMLN * p_RecordingTokens;
	XMLN * p_Time;

	p_RecordingTokens = xml_node_soap_get(p_node, "RecordingTokens");
	while (p_RecordingTokens && p_RecordingTokens->data && soap_strcmp(p_RecordingTokens->name, "RecordingTokens") == 0)
	{
		idx = p_req->sizeRecordingTokens;
		strncpy(p_req->RecordingTokens[idx], p_RecordingTokens->data, sizeof(p_req->RecordingTokens[idx])-1);
		
		p_req->sizeRecordingTokens++;
		if (p_req->sizeRecordingTokens >= ARRAY_SIZE(p_req->RecordingTokens))
		{
			break;
		}

		p_RecordingTokens = p_RecordingTokens->next;
	}

	p_Time = xml_node_soap_get(p_node, "Time");
	if (p_Time && p_Time->data)
	{
		parse_XSDDatetime(p_Time->data, &p_req->Time);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tse_FindRecordings(XMLN * p_node, tse_FindRecordings_REQ * p_req)
{
	XMLN * p_Scope;
	XMLN * p_MaxMatches;
	XMLN * p_KeepAliveTime;

	p_Scope = xml_node_soap_get(p_node, "Scope");
	if (p_Scope)
	{
		parse_SearchScope(p_Scope, &p_req->Scope);
	}

	p_MaxMatches = xml_node_soap_get(p_node, "MaxMatches");
	if (p_MaxMatches && p_MaxMatches->data)
	{
		p_req->MaxMatchesFlag = 1;
		p_req->MaxMatches = atoi(p_MaxMatches->data);
	}

	p_KeepAliveTime = xml_node_soap_get(p_node, "KeepAliveTime");
	if (p_KeepAliveTime && p_KeepAliveTime->data)
	{
		parse_XSDDuration(p_KeepAliveTime->data, &p_req->KeepAliveTime);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tse_GetRecordingSearchResults(XMLN * p_node, tse_GetRecordingSearchResults_REQ * p_req)
{
	XMLN * p_SearchToken;
	XMLN * p_MinResults;
	XMLN * p_MaxResults;
	XMLN * p_WaitTime;

	p_SearchToken = xml_node_soap_get(p_node, "SearchToken");
	if (p_SearchToken && p_SearchToken->data)
	{
		strncpy(p_req->SearchToken, p_SearchToken->data, sizeof(p_req->SearchToken)-1);
	}

	p_MinResults = xml_node_soap_get(p_node, "MinResults");
	if (p_MinResults && p_MinResults->data)
	{
		p_req->MinResultsFlag = 1;
		p_req->MinResults = atoi(p_MinResults->data);
	}

	p_MaxResults = xml_node_soap_get(p_node, "MaxResults");
	if (p_MaxResults && p_MaxResults->data)
	{
		p_req->MaxResultsFlag = 1;
		p_req->MaxResults = atoi(p_MaxResults->data);
	}

	p_WaitTime = xml_node_soap_get(p_node, "WaitTime");
	if (p_WaitTime && p_WaitTime->data)
	{
		p_req->WaitTimeFlag = 1;
		parse_XSDDuration(p_WaitTime->data, &p_req->WaitTime);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tse_FindEvents(XMLN * p_node, tse_FindEvents_REQ * p_req)
{
	XMLN * p_StartPoint;
	XMLN * p_EndPoint;
	XMLN * p_Scope;
	XMLN * p_IncludeStartState;
	XMLN * p_MaxMatches;
	XMLN * p_KeepAliveTime;

	p_StartPoint = xml_node_soap_get(p_node, "StartPoint");
	if (p_StartPoint && p_StartPoint->data)
	{
		parse_XSDDatetime(p_StartPoint->data, &p_req->StartPoint);
	}

	p_EndPoint = xml_node_soap_get(p_node, "EndPoint");
	if (p_EndPoint && p_EndPoint->data)
	{
		p_req->EndPointFlag = 1;
		parse_XSDDatetime(p_EndPoint->data, &p_req->EndPoint);
	}

	p_Scope = xml_node_soap_get(p_node, "Scope");
	if (p_Scope)
	{
		parse_SearchScope(p_Scope, &p_req->Scope);
	}

	p_IncludeStartState = xml_node_soap_get(p_node, "IncludeStartState");
	if (p_IncludeStartState && p_IncludeStartState->data)
	{
		p_req->IncludeStartState = parse_Bool(p_IncludeStartState->data);
	}

	p_MaxMatches = xml_node_soap_get(p_node, "MaxMatches");
	if (p_MaxMatches && p_MaxMatches->data)
	{
		p_req->MaxMatchesFlag = 1;
		p_req->MaxMatches = atoi(p_MaxMatches->data);
	}

	p_KeepAliveTime = xml_node_soap_get(p_node, "KeepAliveTime");
	if (p_KeepAliveTime && p_KeepAliveTime->data)
	{
		p_req->KeepAliveTimeFlag = 1;
		parse_XSDDuration(p_KeepAliveTime->data, &p_req->KeepAliveTime);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tse_GetEventSearchResults(XMLN * p_node, tse_GetEventSearchResults_REQ * p_req)
{
	XMLN * p_SearchToken;
	XMLN * p_MinResults;
	XMLN * p_MaxResults;
	XMLN * p_WaitTime;

	p_SearchToken = xml_node_soap_get(p_node, "SearchToken");
	if (p_SearchToken && p_SearchToken->data)
	{
		strncpy(p_req->SearchToken, p_SearchToken->data, sizeof(p_req->SearchToken)-1);
	}

	p_MinResults = xml_node_soap_get(p_node, "MinResults");
	if (p_MinResults && p_MinResults->data)
	{
		p_req->MinResultsFlag = 1;
		p_req->MinResults = atoi(p_MinResults->data);
	}

	p_MaxResults = xml_node_soap_get(p_node, "MaxResults");
	if (p_MaxResults && p_MaxResults->data)
	{
		p_req->MaxResultsFlag = 1;
		p_req->MaxResults = atoi(p_MaxResults->data);
	}

	p_WaitTime = xml_node_soap_get(p_node, "WaitTime");
	if (p_WaitTime && p_WaitTime->data)
	{
		p_req->WaitTimeFlag = 1;
		parse_XSDDuration(p_WaitTime->data, &p_req->WaitTime);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tse_FindMetadata(XMLN * p_node, tse_FindMetadata_REQ * p_req)
{
    XMLN * p_StartPoint;
	XMLN * p_EndPoint;
	XMLN * p_Scope;
	XMLN * p_MetadataFilter;
	XMLN * p_MaxMatches;
	XMLN * p_KeepAliveTime;

	p_StartPoint = xml_node_soap_get(p_node, "StartPoint");
	if (p_StartPoint && p_StartPoint->data)
	{
		parse_XSDDatetime(p_StartPoint->data, &p_req->StartPoint);
	}

	p_EndPoint = xml_node_soap_get(p_node, "EndPoint");
	if (p_EndPoint && p_EndPoint->data)
	{
		p_req->EndPointFlag = 1;
		parse_XSDDatetime(p_EndPoint->data, &p_req->EndPoint);
	}

	p_Scope = xml_node_soap_get(p_node, "Scope");
	if (p_Scope)
	{
		parse_SearchScope(p_Scope, &p_req->Scope);
	}

	p_MetadataFilter = xml_node_soap_get(p_node, "MetadataFilter");
	if (p_MetadataFilter)
	{
	    XMLN * p_MetadataStreamFilter;

	    p_MetadataStreamFilter = xml_node_soap_get(p_MetadataFilter, "MetadataStreamFilter");
	    if (p_MetadataStreamFilter && p_MetadataStreamFilter->data)
	    {
	        strncpy(p_req->MetadataFilter.MetadataStreamFilter, p_MetadataStreamFilter->data, 
	            sizeof(p_req->MetadataFilter.MetadataStreamFilter)-1);
	    }
	}

	p_MaxMatches = xml_node_soap_get(p_node, "MaxMatches");
	if (p_MaxMatches && p_MaxMatches->data)
	{
		p_req->MaxMatchesFlag = 1;
		p_req->MaxMatches = atoi(p_MaxMatches->data);
	}

	p_KeepAliveTime = xml_node_soap_get(p_node, "KeepAliveTime");
	if (p_KeepAliveTime && p_KeepAliveTime->data)
	{
		p_req->KeepAliveTimeFlag = 1;
		parse_XSDDuration(p_KeepAliveTime->data, &p_req->KeepAliveTime);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tse_GetMetadataSearchResults(XMLN * p_node, tse_GetMetadataSearchResults_REQ * p_req)
{
    XMLN * p_SearchToken;
	XMLN * p_MinResults;
	XMLN * p_MaxResults;
	XMLN * p_WaitTime;

	p_SearchToken = xml_node_soap_get(p_node, "SearchToken");
	if (p_SearchToken && p_SearchToken->data)
	{
		strncpy(p_req->SearchToken, p_SearchToken->data, sizeof(p_req->SearchToken)-1);
	}

	p_MinResults = xml_node_soap_get(p_node, "MinResults");
	if (p_MinResults && p_MinResults->data)
	{
		p_req->MinResultsFlag = 1;
		p_req->MinResults = atoi(p_MinResults->data);
	}

	p_MaxResults = xml_node_soap_get(p_node, "MaxResults");
	if (p_MaxResults && p_MaxResults->data)
	{
		p_req->MaxResultsFlag = 1;
		p_req->MaxResults = atoi(p_MaxResults->data);
	}

	p_WaitTime = xml_node_soap_get(p_node, "WaitTime");
	if (p_WaitTime && p_WaitTime->data)
	{
		p_req->WaitTimeFlag = 1;
		parse_XSDDuration(p_WaitTime->data, &p_req->WaitTime);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tse_EndSearch(XMLN * p_node, tse_EndSearch_REQ * p_req)
{
	XMLN * p_SearchToken;

	p_SearchToken = xml_node_soap_get(p_node, "SearchToken");
	if (p_SearchToken && p_SearchToken->data)
	{
		strncpy(p_req->SearchToken, p_SearchToken->data, sizeof(p_req->SearchToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tse_GetSearchState(XMLN * p_node, tse_GetSearchState_REQ * p_req)
{
	XMLN * p_SearchToken;

	p_SearchToken = xml_node_soap_get(p_node, "SearchToken");
	if (p_SearchToken && p_SearchToken->data)
	{
		strncpy(p_req->SearchToken, p_SearchToken->data, sizeof(p_req->SearchToken)-1);
	}

	return ONVIF_OK;
}

#ifdef PTZ_SUPPORT

BOOL parse_PTZPositionFilter(XMLN * p_node, onvif_PTZPositionFilter * p_req)
{
    XMLN * p_MinPosition;
	XMLN * p_MaxPosition;
	XMLN * p_EnterOrExit;

	p_MinPosition = xml_node_soap_get(p_node, "MinPosition");
	if (p_MinPosition)
	{
		parse_PTZVector(p_MinPosition, &p_req->MinPosition);
	}

	p_MaxPosition = xml_node_soap_get(p_node, "MaxPosition");
	if (p_MaxPosition)
	{
		parse_PTZVector(p_MaxPosition, &p_req->MaxPosition);
	}

    p_EnterOrExit = xml_node_soap_get(p_node, "EnterOrExit");
	if (p_EnterOrExit && p_EnterOrExit->data)
	{
		p_req->EnterOrExit = parse_Bool(p_EnterOrExit->data);
	}
	
	return TRUE;
}

ONVIF_RET parse_tse_FindPTZPosition(XMLN * p_node, tse_FindPTZPosition_REQ * p_req)
{
    XMLN * p_StartPoint;
	XMLN * p_EndPoint;
	XMLN * p_Scope;
	XMLN * p_SearchFilter;
	XMLN * p_MaxMatches;
	XMLN * p_KeepAliveTime;

	p_StartPoint = xml_node_soap_get(p_node, "StartPoint");
	if (p_StartPoint && p_StartPoint->data)
	{
		parse_XSDDatetime(p_StartPoint->data, &p_req->StartPoint);
	}

	p_EndPoint = xml_node_soap_get(p_node, "EndPoint");
	if (p_EndPoint && p_EndPoint->data)
	{
		p_req->EndPointFlag = 1;
		parse_XSDDatetime(p_EndPoint->data, &p_req->EndPoint);
	}

	p_Scope = xml_node_soap_get(p_node, "Scope");
	if (p_Scope)
	{
		parse_SearchScope(p_Scope, &p_req->Scope);
	}

	p_SearchFilter = xml_node_soap_get(p_node, "SearchFilter");
	if (p_SearchFilter && p_SearchFilter->data)
	{
	    parse_PTZPositionFilter(p_SearchFilter, &p_req->SearchFilter);
	}

	p_MaxMatches = xml_node_soap_get(p_node, "MaxMatches");
	if (p_MaxMatches && p_MaxMatches->data)
	{
		p_req->MaxMatchesFlag = 1;
		p_req->MaxMatches = atoi(p_MaxMatches->data);
	}

	p_KeepAliveTime = xml_node_soap_get(p_node, "KeepAliveTime");
	if (p_KeepAliveTime && p_KeepAliveTime->data)
	{
		p_req->KeepAliveTimeFlag = 1;
		parse_XSDDuration(p_KeepAliveTime->data, &p_req->KeepAliveTime);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tse_GetPTZPositionSearchResults(XMLN * p_node, tse_GetPTZPositionSearchResults_REQ * p_req)
{
    XMLN * p_SearchToken;
	XMLN * p_MinResults;
	XMLN * p_MaxResults;
	XMLN * p_WaitTime;

	p_SearchToken = xml_node_soap_get(p_node, "SearchToken");
	if (p_SearchToken && p_SearchToken->data)
	{
		strncpy(p_req->SearchToken, p_SearchToken->data, sizeof(p_req->SearchToken)-1);
	}

	p_MinResults = xml_node_soap_get(p_node, "MinResults");
	if (p_MinResults && p_MinResults->data)
	{
		p_req->MinResultsFlag = 1;
		p_req->MinResults = atoi(p_MinResults->data);
	}

	p_MaxResults = xml_node_soap_get(p_node, "MaxResults");
	if (p_MaxResults && p_MaxResults->data)
	{
		p_req->MaxResultsFlag = 1;
		p_req->MaxResults = atoi(p_MaxResults->data);
	}

	p_WaitTime = xml_node_soap_get(p_node, "WaitTime");
	if (p_WaitTime && p_WaitTime->data)
	{
		p_req->WaitTimeFlag = 1;
		parse_XSDDuration(p_WaitTime->data, &p_req->WaitTime);
	}
	
	return ONVIF_OK;
}

#endif

ONVIF_RET parse_trp_GetReplayUri(XMLN * p_node, trp_GetReplayUri_REQ * p_req)
{
	ONVIF_RET ret = ONVIF_OK;
	XMLN * p_StreamSetup;
	XMLN * p_RecordingToken;

	p_StreamSetup = xml_node_soap_get(p_node, "StreamSetup");
	if (p_StreamSetup)
	{
		ret = parse_StreamSetup(p_StreamSetup, &p_req->StreamSetup);
	}

	p_RecordingToken = xml_node_soap_get(p_node, "RecordingToken");
	if (p_RecordingToken && p_RecordingToken->data)
	{
		strncpy(p_req->RecordingToken, p_RecordingToken->data, sizeof(p_req->RecordingToken)-1);
	}

	return ret;
}

ONVIF_RET parse_trp_SetReplayConfiguration(XMLN * p_node, trp_SetReplayConfiguration_REQ * p_req)
{
	XMLN * p_Configuration;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		XMLN * p_SessionTimeout;
		
		p_SessionTimeout = xml_node_soap_get(p_Configuration, "SessionTimeout");
		if (p_SessionTimeout && p_SessionTimeout->data)
		{
			parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
		}
	}

	return ONVIF_OK;
}

#endif	// end of PROFILE_G_SUPPORT

#ifdef VIDEO_ANALYTICS

ONVIF_RET parse_SimpleItem(XMLN * p_node, onvif_SimpleItem * p_req)
{
	const char * p_Name;
	const char * p_Value;

	p_Name = xml_attr_get(p_node, "Name");
	if (p_Name)
	{
		snprintf(p_req->Name, sizeof(p_req->Name), "%.*s",
		         (int)(sizeof(p_req->Name) - 1), p_Name);
	}

	p_Value = xml_attr_get(p_node, "Value");
	if (p_Value)
	{
		snprintf(p_req->Value, sizeof(p_req->Value), "%.*s",
		         (int)(sizeof(p_req->Value) - 1), p_Value);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ElementItem_old(XMLN * p_node, onvif_ElementItem * p_req)
{
	const char * p_Name;

	p_Name = xml_attr_get(p_node, "Name");
	if (p_Name)
	{
		snprintf(p_req->Name, sizeof(p_req->Name), "%.*s",
		         (int)(sizeof(p_req->Name) - 1), p_Name);

		if (p_node->f_child)
		{
			int len = xml_calc_buf_len(p_node->f_child);

			p_req->AnyFlag = 1;
			p_req->Any = (char *) malloc(len+8);
			if (p_req->Any)
			{
				memset(p_req->Any, 0, len + 8);
				xml_write_buf(p_node->f_child, p_req->Any, len);
			}
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ElementItem(XMLN * p_node, onvif_ElementItem * p_req)
{
	const char * p_Name;

	p_Name = xml_attr_get(p_node, "Name");
	if (p_Name)
	{
		snprintf(p_req->Name, sizeof(p_req->Name), "%.*s",
		         (int)(sizeof(p_req->Name) - 1), p_Name);
		int any_len = 0;
		int len = 0;
		struct XMLN * tmp = p_node->f_child;
		while (tmp != NULL) //���㳤��
		{
			len = xml_calc_buf_len(tmp);
			any_len += len;
			
			if (tmp->next != NULL)
				tmp = tmp->next;
			else
				break;
		}
		
		p_req->AnyFlag = 1;
		p_req->Any = (char *) malloc(any_len + 8);
		memset(p_req->Any, 0, any_len + 8);
		
		tmp = p_node->f_child;
		while (tmp != NULL)
		{
			len = xml_calc_buf_len(tmp);
			char *tmp_msg = malloc(len);
			memset(tmp_msg, 0, len);
			
			xml_write_buf(tmp, tmp_msg, len);
			strcat(p_req->Any, tmp_msg);
			
			if (tmp_msg != NULL)
				free(tmp_msg);
			
			if (tmp->next != NULL)
				tmp = tmp->next;
			else
				break;
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_Config(XMLN * p_node, onvif_Config * p_req)
{
	XMLN * p_Parameters;	
	const char * p_Name;
	const char * p_Type;
	ONVIF_RET ret;

	p_Name = xml_attr_get(p_node, "Name");
	if (p_Name)
	{
		snprintf(p_req->Name, sizeof(p_req->Name), "%.*s",
		         (int)(sizeof(p_req->Name) - 1), p_Name);
	}

	p_Type = xml_attr_get(p_node, "Type");
	if (p_Type)
	{
		if (p_node->l_attrib && soap_strcmp(p_node->l_attrib->name, "Type"))
		{
			snprintf(p_req->Type, sizeof(p_req->Type), "%.*s",
			         (int)(sizeof(p_req->Type) - 1), p_Type);

			p_req->attrFlag = 1;
			snprintf(p_req->attr, sizeof(p_req->attr)-1, "%s=\"%s\"", p_node->l_attrib->name, p_node->l_attrib->data);
		}
		else
		{
			snprintf(p_req->Type, sizeof(p_req->Type), "%.*s",
			         (int)(sizeof(p_req->Type) - 1), p_Type);
		}
	}

	p_Parameters = xml_node_soap_get(p_node, "Parameters");
	if (p_Parameters)
	{	
		XMLN * p_SimpleItem;
		XMLN * p_ElementItem;

		p_SimpleItem = xml_node_soap_get(p_Parameters, "SimpleItem");
		while (p_SimpleItem && soap_strcmp(p_SimpleItem->name, "SimpleItem") == 0)
		{
			SimpleItemList * p_simple_item = onvif_add_SimpleItem(&p_req->Parameters.SimpleItem);
			if (p_simple_item)
			{
				ret = parse_SimpleItem(p_SimpleItem, &p_simple_item->SimpleItem);
				if (ONVIF_OK != ret)
				{
					onvif_free_SimpleItems(&p_req->Parameters.SimpleItem);
					break;
				}
			}
			
			p_SimpleItem = p_SimpleItem->next;
		}

		p_ElementItem = xml_node_soap_get(p_Parameters, "ElementItem");
		while (p_ElementItem && soap_strcmp(p_ElementItem->name, "ElementItem") == 0)
		{
			ElementItemList * p_element_item = onvif_add_ElementItem(&p_req->Parameters.ElementItem);
			if (p_element_item)
			{
				ret = parse_ElementItem(p_ElementItem, &p_element_item->ElementItem);
				if (ONVIF_OK != ret)
				{
					onvif_free_ElementItems(&p_req->Parameters.ElementItem);
					break;
				}
			}
			
			p_ElementItem = p_ElementItem->next;
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tan_GetSupportedRules(XMLN * p_node, tan_GetSupportedRules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tan_CreateRules(XMLN * p_node, tan_CreateRules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_Rule;
	ONVIF_RET ret;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_Rule = xml_node_soap_get(p_node, "Rule");
	while (p_Rule && soap_strcmp(p_Rule->name, "Rule") == 0)
	{
		ConfigList * p_config = onvif_add_Config(&p_req->Rule);
		if (p_config)
		{
			ret = parse_Config(p_Rule, &p_config->Config);
			if (ONVIF_OK != ret)
			{
				onvif_free_Configs(&p_req->Rule);
				return ret;
			}
		}
		
		p_Rule = p_Rule->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tan_DeleteRules(XMLN * p_node, tan_DeleteRules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_RuleName;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_RuleName = xml_node_soap_get(p_node, "RuleName");
	while (p_RuleName && p_RuleName->data && soap_strcmp(p_RuleName->name, "RuleName") == 0)
	{	
		int idx = p_req->sizeRuleName;
		
		strncpy(p_req->RuleName[idx], p_RuleName->data, sizeof(p_req->RuleName[idx])-1);
		
		p_req->sizeRuleName++;
		if (p_req->sizeRuleName >= ARRAY_SIZE(p_req->RuleName))
		{
			break;
		}

		p_RuleName = p_RuleName->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tan_GetRules(XMLN * p_node, tan_GetRules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tan_ModifyRules(XMLN * p_node, tan_ModifyRules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_Rule;
	ONVIF_RET ret;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_Rule = xml_node_soap_get(p_node, "Rule");
	while (p_Rule && soap_strcmp(p_Rule->name, "Rule") == 0)
	{
		ConfigList * p_config = onvif_add_Config(&p_req->Rule);
		if (p_config)
		{
			ret = parse_Config(p_Rule, &p_config->Config);
			if (ONVIF_OK != ret)
			{
				onvif_free_Configs(&p_req->Rule);
				return ret;
			}
		}
		
		p_Rule = p_Rule->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tan_CreateAnalyticsModules(XMLN * p_node, tan_CreateAnalyticsModules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_AnalyticsModule;
	ONVIF_RET ret;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_AnalyticsModule = xml_node_soap_get(p_node, "AnalyticsModule");
	while (p_AnalyticsModule && soap_strcmp(p_AnalyticsModule->name, "AnalyticsModule") == 0)
	{
		ConfigList * p_config = onvif_add_Config(&p_req->AnalyticsModule);
		if (p_config)
		{
			ret = parse_Config(p_AnalyticsModule, &p_config->Config);
			if (ONVIF_OK != ret)
			{
				onvif_free_Configs(&p_req->AnalyticsModule);
				return ret;
			}
		}
		
		p_AnalyticsModule = p_AnalyticsModule->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tan_DeleteAnalyticsModules(XMLN * p_node, tan_DeleteAnalyticsModules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_AnalyticsModuleName;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_AnalyticsModuleName = xml_node_soap_get(p_node, "AnalyticsModuleName");
	while (p_AnalyticsModuleName && p_AnalyticsModuleName->data && soap_strcmp(p_AnalyticsModuleName->name, "AnalyticsModuleName") == 0)
	{	
		int idx = p_req->sizeAnalyticsModuleName;
		
		strncpy(p_req->AnalyticsModuleName[idx], p_AnalyticsModuleName->data, sizeof(p_req->AnalyticsModuleName[idx])-1);
		
		p_req->sizeAnalyticsModuleName++;
		if (p_req->sizeAnalyticsModuleName >= ARRAY_SIZE(p_req->AnalyticsModuleName))
		{
			break;
		}

		p_AnalyticsModuleName = p_AnalyticsModuleName->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tan_GetAnalyticsModules(XMLN * p_node, tan_GetAnalyticsModules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tan_ModifyAnalyticsModules(XMLN * p_node, tan_ModifyAnalyticsModules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_AnalyticsModule;
	ONVIF_RET ret;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_AnalyticsModule = xml_node_soap_get(p_node, "AnalyticsModule");
	while (p_AnalyticsModule && soap_strcmp(p_AnalyticsModule->name, "AnalyticsModule") == 0)
	{
		ConfigList * p_config = onvif_add_Config(&p_req->AnalyticsModule);
		if (p_config)
		{
			ret = parse_Config(p_AnalyticsModule, &p_config->Config);
			if (ONVIF_OK != ret)
			{
				onvif_free_Configs(&p_req->AnalyticsModule);
				return ret;
			}
		}
		
		p_AnalyticsModule = p_AnalyticsModule->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tan_GetRuleOptions(XMLN * p_node, tan_GetRuleOptions_REQ * p_req)
{
    XMLN * p_RuleType;
	XMLN * p_ConfigurationToken;

	p_RuleType = xml_node_soap_get(p_node, "RuleType");
	if (p_RuleType && p_RuleType->data)
	{
		snprintf(p_req->RuleType, sizeof(p_req->RuleType), "%.*s",
		         (int)(sizeof(p_req->RuleType) - 1), p_RuleType->data);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		snprintf(p_req->ConfigurationToken, sizeof(p_req->ConfigurationToken), "%.*s",
		         (int)(sizeof(p_req->ConfigurationToken) - 1), p_ConfigurationToken->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tan_GetSupportedAnalyticsModules(XMLN * p_node, tan_GetSupportedAnalyticsModules_REQ * p_req)
{
	XMLN * p_ConfigurationToken;
	
	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		snprintf(p_req->ConfigurationToken, sizeof(p_req->ConfigurationToken), "%.*s",
		         (int)(sizeof(p_req->ConfigurationToken) - 1), p_ConfigurationToken->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tan_GetAnalyticsModuleOptions(XMLN * p_node, tan_GetAnalyticsModuleOptions_REQ * p_req)
{
    XMLN * p_Type;
	XMLN * p_ConfigurationToken;

	p_Type = xml_node_soap_get(p_node, "Type");
	if (p_Type && p_Type->data)
	{
		snprintf(p_req->Type, sizeof(p_req->Type), "%.*s",
		         (int)(sizeof(p_req->Type) - 1), p_Type->data);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		snprintf(p_req->ConfigurationToken, sizeof(p_req->ConfigurationToken), "%.*s",
		         (int)(sizeof(p_req->ConfigurationToken) - 1), p_ConfigurationToken->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tan_GetSupportedMetadata(XMLN * p_node, tan_GetSupportedMetadata_REQ * p_req)
{
    XMLN * p_Type;

	p_Type = xml_node_soap_get(p_node, "Type");
	if (p_Type && p_Type->data)
	{
		snprintf(p_req->Type, sizeof(p_req->Type), "%.*s",
		         (int)(sizeof(p_req->Type) - 1), p_Type->data);
	}

	return ONVIF_OK;
}

#endif	// end of VIDEO_ANALYTICS

#ifdef PROFILE_C_SUPPORT

ONVIF_RET parse_DoorCapabilities(XMLN * p_node, onvif_DoorCapabilities * p_req)
{
    const char * p_Access;
    const char * p_AccessTimingOverride;
    const char * p_Lock;
    const char * p_Unlock;
    const char * p_Block;
    const char * p_DoubleLock;
    const char * p_LockDown;
    const char * p_LockOpen;
    const char * p_DoorMonitor;
    const char * p_LockMonitor;
    const char * p_DoubleLockMonitor;
    const char * p_Alarm;
    const char * p_Tamper;
    const char * p_Fault;

    p_Access = xml_attr_get(p_node, "Access");
    if (p_Access)
    {
        p_req->Access = parse_Bool(p_Access);
    }

    p_AccessTimingOverride = xml_attr_get(p_node, "AccessTimingOverride");
    if (p_AccessTimingOverride)
    {
        p_req->AccessTimingOverride = parse_Bool(p_AccessTimingOverride);
    }

    p_Lock = xml_attr_get(p_node, "Lock");
    if (p_Lock)
    {
        p_req->Lock = parse_Bool(p_Lock);
    }

    p_Unlock = xml_attr_get(p_node, "Unlock");
    if (p_Unlock)
    {
        p_req->Unlock = parse_Bool(p_Unlock);
    }

    p_Block = xml_attr_get(p_node, "Block");
    if (p_Block)
    {
        p_req->Block = parse_Bool(p_Block);
    }

    p_DoubleLock = xml_attr_get(p_node, "DoubleLock");
    if (p_DoubleLock)
    {
        p_req->DoubleLock = parse_Bool(p_DoubleLock);
    }

    p_LockDown = xml_attr_get(p_node, "LockDown");
    if (p_LockDown)
    {
        p_req->LockDown = parse_Bool(p_LockDown);
    }

    p_LockOpen = xml_attr_get(p_node, "LockOpen");
    if (p_LockOpen)
    {
        p_req->LockOpen = parse_Bool(p_LockOpen);
    }

    p_DoorMonitor = xml_attr_get(p_node, "DoorMonitor");
    if (p_DoorMonitor)
    {
        p_req->DoorMonitor = parse_Bool(p_DoorMonitor);
    }

    p_LockMonitor = xml_attr_get(p_node, "LockMonitor");
    if (p_LockMonitor)
    {
        p_req->LockMonitor = parse_Bool(p_LockMonitor);
    }

    p_DoubleLockMonitor = xml_attr_get(p_node, "DoubleLockMonitor");
    if (p_DoubleLockMonitor)
    {
        p_req->DoubleLockMonitor = parse_Bool(p_DoubleLockMonitor);
    }

    p_Alarm = xml_attr_get(p_node, "Alarm");
    if (p_Alarm)
    {
        p_req->Alarm = parse_Bool(p_Alarm);
    }

    p_Tamper = xml_attr_get(p_node, "Tamper");
    if (p_Tamper)
    {
        p_req->Tamper = parse_Bool(p_Tamper);
    }

    p_Fault = xml_attr_get(p_node, "Fault");
    if (p_Fault)
    {
        p_req->Fault = parse_Bool(p_Fault);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_AccessPointCapabilities(XMLN * p_node, onvif_AccessPointCapabilities * p_req)
{
    const char * p_DisableAccessPoint;
    const char * p_Duress;
    const char * p_AnonymousAccess;
    const char * p_AccessTaken;
    const char * p_ExternalAuthorization;
    const char * p_SupportedRecognitionTypes;
    const char * p_IdentifierAccess;
    const char * p_SupportedFeedbackTypes;

    p_DisableAccessPoint = xml_attr_get(p_node, "DisableAccessPoint");
    if (p_DisableAccessPoint)
    {
        p_req->DisableAccessPoint = parse_Bool(p_DisableAccessPoint);
    }

    p_Duress = xml_attr_get(p_node, "Duress");
    if (p_Duress)
    {
        p_req->Duress = parse_Bool(p_Duress);
    }

    p_AnonymousAccess = xml_attr_get(p_node, "AnonymousAccess");
    if (p_AnonymousAccess)
    {
        p_req->AnonymousAccess = parse_Bool(p_AnonymousAccess);
    }

    p_AccessTaken = xml_attr_get(p_node, "AccessTaken");
    if (p_AccessTaken)
    {
        p_req->AccessTaken = parse_Bool(p_AccessTaken);
    }

    p_ExternalAuthorization = xml_attr_get(p_node, "ExternalAuthorization");
    if (p_ExternalAuthorization)
    {
        p_req->ExternalAuthorization = parse_Bool(p_ExternalAuthorization);
    }

    p_SupportedRecognitionTypes = xml_attr_get(p_node, "SupportedRecognitionTypes");
    if (p_SupportedRecognitionTypes)
    {
        p_req->SupportedRecognitionTypesFlag = 1;
        strncpy(p_req->SupportedRecognitionTypes, p_SupportedRecognitionTypes, sizeof(p_req->SupportedRecognitionTypes)-1);
    }

    p_IdentifierAccess = xml_attr_get(p_node, "IdentifierAccess");
    if (p_IdentifierAccess)
    {
        p_req->IdentifierAccess = parse_Bool(p_IdentifierAccess);
    }

    p_SupportedFeedbackTypes = xml_attr_get(p_node, "SupportedFeedbackTypes");
    if (p_SupportedFeedbackTypes)
    {
        p_req->SupportedFeedbackTypesFlag = 1;
        strncpy(p_req->SupportedFeedbackTypes, p_SupportedFeedbackTypes, sizeof(p_req->SupportedFeedbackTypes)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_AccessPointInfo(XMLN * p_node, onvif_AccessPointInfo * p_req)
{
    const char * p_token;
    XMLN * p_Name;
    XMLN * p_Description;
    XMLN * p_AreaFrom;
    XMLN * p_AreaTo;
    XMLN * p_EntityType;
    XMLN * p_Entity;
    XMLN * p_Capabilities;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name) - 1);
	}

	p_Description = xml_node_soap_get(p_node, "Description");
	if (p_Description && p_Description->data)
	{
		p_req->DescriptionFlag = 1;
		strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description) - 1);
	}

	p_AreaFrom = xml_node_soap_get(p_node, "AreaFrom");
	if (p_AreaFrom && p_AreaFrom->data)
	{
	    p_req->AreaFromFlag = 1;
		strncpy(p_req->AreaFrom, p_AreaFrom->data, sizeof(p_req->AreaFrom) - 1);
	}

	p_AreaTo = xml_node_soap_get(p_node, "AreaTo");
	if (p_AreaTo && p_AreaTo->data)
	{
	    p_req->AreaToFlag = 1;
		strncpy(p_req->AreaTo, p_AreaTo->data, sizeof(p_req->AreaTo) - 1);
	}

	p_EntityType = xml_node_soap_get(p_node, "EntityType");
	if (p_EntityType && p_EntityType->data)
	{
	    p_req->EntityTypeFlag = 1;
		strncpy(p_req->EntityType, p_EntityType->data, sizeof(p_req->EntityType) - 1);

		if (p_EntityType->l_attrib && p_EntityType->l_attrib->data)
		{
		    p_req->EntityTypeAttrFlag = 1;
		    snprintf(p_req->EntityTypeAttr, sizeof(p_req->EntityTypeAttr) - 1, 
		        "%s=\"%s\"", p_EntityType->l_attrib->name, p_EntityType->l_attrib->data);
		}
	}

	p_Entity = xml_node_soap_get(p_node, "Entity");
	if (p_Entity && p_Entity->data)
	{
		strncpy(p_req->Entity, p_Entity->data, sizeof(p_req->Entity) - 1);
	}

	p_Capabilities = xml_node_soap_get(p_node, "Capabilities");
	if (p_Capabilities)
	{
	    parse_AccessPointCapabilities(p_Capabilities, &p_req->Capabilities);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_AreaInfo(XMLN * p_node, onvif_AreaInfo * p_req)
{
    const char * p_token;
    XMLN * p_Name;
    XMLN * p_Description;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name) - 1);
	}

	p_Description = xml_node_soap_get(p_node, "Description");
	if (p_Description && p_Description->data)
	{
		p_req->DescriptionFlag = 1;
		strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description) - 1);
	}

    return ONVIF_OK;
}

ONVIF_RET parse_DoorInfo(XMLN * p_node, onvif_DoorInfo * p_req)
{
	XMLN * p_Name;
	XMLN * p_Description;
	XMLN * p_Capabilities;
	const char * p_token;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token) - 1);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name) - 1);
	}

	p_Description = xml_node_soap_get(p_node, "Description");
	if (p_Description && p_Description->data)
	{
		p_req->DescriptionFlag = 1;
		strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description) - 1);
	}

	p_Capabilities = xml_node_soap_get(p_node, "Capabilities");
	if (p_Capabilities)
	{
		const char * p_Access;
		const char * p_AccessTimingOverride;
		const char * p_Lock;
		const char * p_Unlock;
		const char * p_Block;
		const char * p_DoubleLock;
		const char * p_LockDown;
		const char * p_LockOpen;
		const char * p_DoorMonitor;
		const char * p_LockMonitor;
		const char * p_DoubleLockMonitor;
		const char * p_Alarm;
		const char * p_Tamper;
		const char * p_Fault;

		p_Access = xml_attr_get(p_Capabilities, "Access");
		if (p_Access)
		{
			p_req->Capabilities.Access = parse_Bool(p_Access);
		}

		p_AccessTimingOverride = xml_attr_get(p_Capabilities, "AccessTimingOverride");
		if (p_AccessTimingOverride)
		{
			p_req->Capabilities.AccessTimingOverride = parse_Bool(p_AccessTimingOverride);
		}

		p_Lock = xml_attr_get(p_Capabilities, "Lock");
		if (p_Lock)
		{
			p_req->Capabilities.Lock = parse_Bool(p_Lock);
		}

		p_Unlock = xml_attr_get(p_Capabilities, "Unlock");
		if (p_Unlock)
		{
			p_req->Capabilities.Unlock = parse_Bool(p_Unlock);
		}

		p_Block = xml_attr_get(p_Capabilities, "Block");
		if (p_Block)
		{
			p_req->Capabilities.Block = parse_Bool(p_Block);
		}

		p_DoubleLock = xml_attr_get(p_Capabilities, "DoubleLock");
		if (p_DoubleLock)
		{
			p_req->Capabilities.DoubleLock = parse_Bool(p_DoubleLock);
		}

		p_LockDown = xml_attr_get(p_Capabilities, "LockDown");
		if (p_LockDown)
		{
			p_req->Capabilities.LockDown = parse_Bool(p_LockDown);
		}

		p_LockOpen = xml_attr_get(p_Capabilities, "LockOpen");
		if (p_LockOpen)
		{
			p_req->Capabilities.LockOpen = parse_Bool(p_LockOpen);
		}

		p_DoorMonitor = xml_attr_get(p_Capabilities, "DoorMonitor");
		if (p_DoorMonitor)
		{
			p_req->Capabilities.DoorMonitor = parse_Bool(p_DoorMonitor);
		}

		p_LockMonitor = xml_attr_get(p_Capabilities, "LockMonitor");
		if (p_LockMonitor)
		{
			p_req->Capabilities.LockMonitor = parse_Bool(p_LockMonitor);
		}

		p_DoubleLockMonitor = xml_attr_get(p_Capabilities, "DoubleLockMonitor");
		if (p_DoubleLockMonitor)
		{
			p_req->Capabilities.DoubleLockMonitor = parse_Bool(p_DoubleLockMonitor);
		}

		p_Alarm = xml_attr_get(p_Capabilities, "Alarm");
		if (p_Alarm)
		{
			p_req->Capabilities.Alarm = parse_Bool(p_Alarm);
		}

		p_Tamper = xml_attr_get(p_Capabilities, "Tamper");
		if (p_Tamper)
		{
			p_req->Capabilities.Tamper = parse_Bool(p_Tamper);
		}

		p_Fault = xml_attr_get(p_Capabilities, "Fault");
		if (p_Fault)
		{
			p_req->Capabilities.Fault = parse_Bool(p_Fault);
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_Timings(XMLN * p_node, onvif_Timings * p_req)
{
    XMLN * p_ReleaseTime;
    XMLN * p_OpenTime;
    XMLN * p_ExtendedReleaseTime;
    XMLN * p_DelayTimeBeforeRelock;
    XMLN * p_ExtendedOpenTime;
    XMLN * p_PreAlarmTime;

    p_ReleaseTime = xml_node_soap_get(p_node, "ReleaseTime");
    if (p_ReleaseTime && p_ReleaseTime->data)
    {
        parse_XSDDuration(p_ReleaseTime->data, (int*)&p_req->ReleaseTime);
    }

    p_OpenTime = xml_node_soap_get(p_node, "OpenTime");
    if (p_OpenTime && p_OpenTime->data)
    {
        parse_XSDDuration(p_OpenTime->data, (int *)&p_req->OpenTime);
    }

    p_ExtendedReleaseTime = xml_node_soap_get(p_node, "ExtendedReleaseTime");
    if (p_ExtendedReleaseTime && p_ExtendedReleaseTime->data)
    {
        p_req->ExtendedReleaseTimeFlag = 1;
        parse_XSDDuration(p_ExtendedReleaseTime->data, (int *)&p_req->ExtendedReleaseTime);
    }

    p_DelayTimeBeforeRelock = xml_node_soap_get(p_node, "DelayTimeBeforeRelock");
    if (p_DelayTimeBeforeRelock && p_DelayTimeBeforeRelock->data)
    {
        p_req->DelayTimeBeforeRelockFlag = 1;
        parse_XSDDuration(p_DelayTimeBeforeRelock->data, (int *)&p_req->DelayTimeBeforeRelock);
    }

    p_ExtendedOpenTime = xml_node_soap_get(p_node, "ExtendedOpenTime");
    if (p_ExtendedOpenTime && p_ExtendedOpenTime->data)
    {
        p_req->ExtendedOpenTimeFlag = 1;
        parse_XSDDuration(p_ExtendedOpenTime->data, (int *)&p_req->ExtendedOpenTime);
    }

    p_PreAlarmTime = xml_node_soap_get(p_node, "PreAlarmTime");
    if (p_PreAlarmTime && p_PreAlarmTime->data)
    {
        p_req->PreAlarmTimeFlag = 1;
        parse_XSDDuration(p_PreAlarmTime->data, (int *)&p_req->PreAlarmTime);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_Door(XMLN * p_node, onvif_Door * p_req)
{
    XMLN * p_DoorType;
    XMLN * p_Timings;
    
    parse_DoorInfo(p_node, &p_req->DoorInfo);

    p_DoorType = xml_node_soap_get(p_node, "DoorType");
    if (p_DoorType && p_DoorType->data)
    {
        strncpy(p_req->DoorType, p_DoorType->data, sizeof(p_req->DoorType)-1);
    }
    
    p_Timings = xml_node_soap_get(p_node, "Timings");
    if (p_Timings)
    {
        parse_Timings(p_Timings, &p_req->Timings);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_DoorState(XMLN * p_node, onvif_DoorState * p_req)
{
    XMLN * p_DoorPhysicalState;
    XMLN * p_LockPhysicalState;
    XMLN * p_DoubleLockPhysicalState;
    XMLN * p_Alarm;
    XMLN * p_Tamper;
    XMLN * p_Fault;
    XMLN * p_DoorMode;

    p_DoorPhysicalState = xml_node_soap_get(p_node, "DoorPhysicalState");
    if (p_DoorPhysicalState && p_DoorPhysicalState->data)
    {
        p_req->DoorPhysicalStateFlag = 1;
        p_req->DoorPhysicalState = onvif_StringToDoorPhysicalState(p_DoorPhysicalState->data);
    }

    p_LockPhysicalState = xml_node_soap_get(p_node, "LockPhysicalState");
    if (p_LockPhysicalState && p_LockPhysicalState->data)
    {
        p_req->LockPhysicalStateFlag = 1;
        p_req->LockPhysicalState = onvif_StringToLockPhysicalState(p_LockPhysicalState->data);
    }

    p_DoubleLockPhysicalState = xml_node_soap_get(p_node, "DoubleLockPhysicalState");
    if (p_DoubleLockPhysicalState && p_DoubleLockPhysicalState->data)
    {
        p_req->DoubleLockPhysicalStateFlag = 1;
        p_req->DoubleLockPhysicalState = onvif_StringToLockPhysicalState(p_DoubleLockPhysicalState->data);
    }

    p_Alarm = xml_node_soap_get(p_node, "Alarm");
    if (p_Alarm && p_Alarm->data)
    {
        p_req->AlarmFlag = 1;
        p_req->Alarm = onvif_StringToDoorAlarmState(p_Alarm->data);
    }

    p_Tamper = xml_node_soap_get(p_node, "Tamper");
    if (p_Tamper)
    {
        XMLN * p_Reason;
        XMLN * p_State;
    
        p_req->TamperFlag = 1;

        p_Reason = xml_node_soap_get(p_Tamper, "Reason");
        if (p_Reason && p_Reason->data)
        {
            p_req->Tamper.ReasonFlag = 1;
            strncpy(p_req->Tamper.Reason, p_Reason->data, sizeof(p_req->Tamper.Reason)-1);
        }

        p_State = xml_node_soap_get(p_Tamper, "State");
        if (p_State && p_State->data)
        {
            p_req->Tamper.State = onvif_StringToDoorTamperState(p_State->data);
        }
    }

    p_Fault = xml_node_soap_get(p_node, "Fault");
    if (p_Fault)
    {
        XMLN * p_Reason;
        XMLN * p_State;
    
        p_req->FaultFlag = 1;

        p_Reason = xml_node_soap_get(p_Fault, "Reason");
        if (p_Reason && p_Reason->data)
        {
            p_req->Fault.ReasonFlag = 1;
            strncpy(p_req->Fault.Reason, p_Reason->data, sizeof(p_req->Fault.Reason)-1);
        }

        p_State = xml_node_soap_get(p_Fault, "State");
        if (p_State && p_State->data)
        {
            p_req->Fault.State = onvif_StringToDoorFaultState(p_State->data);
        }
    }

    p_DoorMode = xml_node_soap_get(p_node, "DoorMode");
    if (p_DoorMode && p_DoorMode->data)
    {
        p_req->DoorMode = onvif_StringToDoorMode(p_DoorMode->data);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAccessPointList(XMLN * p_node, tac_GetAccessPointList_REQ * p_req)
{
    XMLN * p_Limit;
	XMLN * p_StartReference;

	p_Limit = xml_node_soap_get(p_node, "Limit");
	if (p_Limit && p_Limit->data)
	{
		p_req->LimitFlag = 1;
		p_req->Limit = atoi(p_Limit->data);
	}

	p_StartReference = xml_node_soap_get(p_node, "StartReference");
	if (p_StartReference && p_StartReference->data)
	{
		p_req->StartReferenceFlag = 1;
		strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAccessPoints(XMLN * p_node, tac_GetAccessPoints_REQ * p_req)
{
    int idx = 0;
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        strncpy(p_req->token[idx], p_Token->data, sizeof(p_req->token[idx])-1);

        if (++idx >= ARRAY_SIZE(p_req->token))
        {
            return ONVIF_ERR_TooManyItems;
        }
        
        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_CreateAccessPoint(XMLN * p_node, tac_CreateAccessPoint_REQ * p_req)
{
    XMLN * p_AccessPoint;
    XMLN * p_AuthenticationProfileToken;

    p_AccessPoint = xml_node_soap_get(p_node, "AccessPoint");
    if (p_AccessPoint)
    {
        parse_AccessPointInfo(p_AccessPoint, &p_req->AccessPoint);

        p_AuthenticationProfileToken = xml_node_soap_get(p_node, "AuthenticationProfileToken");
    	if (p_AuthenticationProfileToken && p_AuthenticationProfileToken->data)
    	{
    		strncpy(p_req->AuthenticationProfileToken, p_AuthenticationProfileToken->data, sizeof(p_req->AuthenticationProfileToken)-1);
    	}
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_SetAccessPoint(XMLN * p_node, tac_SetAccessPoint_REQ * p_req)
{
    XMLN * p_AccessPoint;
    XMLN * p_AuthenticationProfileToken;

    p_AccessPoint = xml_node_soap_get(p_node, "AccessPoint");
    if (p_AccessPoint)
    {
        parse_AccessPointInfo(p_AccessPoint, &p_req->AccessPoint);

        p_AuthenticationProfileToken = xml_node_soap_get(p_node, "AuthenticationProfileToken");
    	if (p_AuthenticationProfileToken && p_AuthenticationProfileToken->data)
    	{
    		strncpy(p_req->AuthenticationProfileToken, p_AuthenticationProfileToken->data, sizeof(p_req->AuthenticationProfileToken)-1);
    	}
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_ModifyAccessPoint(XMLN * p_node, tac_ModifyAccessPoint_REQ * p_req)
{
    XMLN * p_AccessPoint;
    XMLN * p_AuthenticationProfileToken;

    p_AccessPoint = xml_node_soap_get(p_node, "AccessPoint");
    if (p_AccessPoint)
    {
        parse_AccessPointInfo(p_AccessPoint, &p_req->AccessPoint);

        p_AuthenticationProfileToken = xml_node_soap_get(p_node, "AuthenticationProfileToken");
    	if (p_AuthenticationProfileToken && p_AuthenticationProfileToken->data)
    	{
    		strncpy(p_req->AuthenticationProfileToken, p_AuthenticationProfileToken->data, sizeof(p_req->AuthenticationProfileToken)-1);
    	}
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_DeleteAccessPoint(XMLN * p_node, tac_DeleteAccessPoint_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAccessPointInfoList(XMLN * p_node, tac_GetAccessPointInfoList_REQ * p_req)
{
    XMLN * p_Limit;
	XMLN * p_StartReference;

	p_Limit = xml_node_soap_get(p_node, "Limit");
	if (p_Limit && p_Limit->data)
	{
		p_req->LimitFlag = 1;
		p_req->Limit = atoi(p_Limit->data);
	}

	p_StartReference = xml_node_soap_get(p_node, "StartReference");
	if (p_StartReference && p_StartReference->data)
	{
		p_req->StartReferenceFlag = 1;
		strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAccessPointInfo(XMLN * p_node, tac_GetAccessPointInfo_REQ * p_req)
{
    int idx = 0;
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        strncpy(p_req->token[idx], p_Token->data, sizeof(p_req->token[idx])-1);

        if (++idx >= ARRAY_SIZE(p_req->token))
        {
            return ONVIF_ERR_TooManyItems;
        }
        
        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAreaList(XMLN * p_node, tac_GetAreaList_REQ * p_req)
{
    XMLN * p_Limit;
	XMLN * p_StartReference;

	p_Limit = xml_node_soap_get(p_node, "Limit");
	if (p_Limit && p_Limit->data)
	{
		p_req->LimitFlag = 1;
		p_req->Limit = atoi(p_Limit->data);
	}

	p_StartReference = xml_node_soap_get(p_node, "StartReference");
	if (p_StartReference && p_StartReference->data)
	{
		p_req->StartReferenceFlag = 1;
		strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAreas(XMLN * p_node, tac_GetAreas_REQ * p_req)
{
    int idx = 0;
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        strncpy(p_req->token[idx], p_Token->data, sizeof(p_req->token[idx])-1);

        if (++idx >= ARRAY_SIZE(p_req->token))
        {
            return ONVIF_ERR_TooManyItems;
        }
        
        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_CreateArea(XMLN * p_node, tac_CreateArea_REQ * p_req)
{
    XMLN * p_Area;

    p_Area = xml_node_soap_get(p_node, "Area");
    if (p_Area)
    {
        parse_AreaInfo(p_Area, &p_req->Area);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_SetArea(XMLN * p_node, tac_SetArea_REQ * p_req)
{
    XMLN * p_Area;

    p_Area = xml_node_soap_get(p_node, "Area");
    if (p_Area)
    {
        parse_AreaInfo(p_Area, &p_req->Area);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_ModifyArea(XMLN * p_node, tac_ModifyArea_REQ * p_req)
{
    XMLN * p_Area;

    p_Area = xml_node_soap_get(p_node, "Area");
    if (p_Area)
    {
        parse_AreaInfo(p_Area, &p_req->Area);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_DeleteArea(XMLN * p_node, tac_DeleteArea_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAreaInfoList(XMLN * p_node, tac_GetAreaInfoList_REQ * p_req)
{
    XMLN * p_Limit;
	XMLN * p_StartReference;

	p_Limit = xml_node_soap_get(p_node, "Limit");
	if (p_Limit && p_Limit->data)
	{
		p_req->LimitFlag = 1;
		p_req->Limit = atoi(p_Limit->data);
	}

	p_StartReference = xml_node_soap_get(p_node, "StartReference");
	if (p_StartReference && p_StartReference->data)
	{
		p_req->StartReferenceFlag = 1;
		strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAreaInfo(XMLN * p_node, tac_GetAreaInfo_REQ * p_req)
{
    int idx = 0;
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        strncpy(p_req->token[idx], p_Token->data, sizeof(p_req->token[idx])-1);

        if (++idx >= ARRAY_SIZE(p_req->token))
        {
            return ONVIF_ERR_TooManyItems;
        }
        
        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_GetAccessPointState(XMLN * p_node, tac_GetAccessPointState_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_EnableAccessPoint(XMLN * p_node, tac_EnableAccessPoint_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tac_DisableAccessPoint(XMLN * p_node, tac_DisableAccessPoint_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_GetDoorList(XMLN * p_node, tdc_GetDoorList_REQ * p_req)
{
    XMLN * p_Limit;
	XMLN * p_StartReference;

	p_Limit = xml_node_soap_get(p_node, "Limit");
	if (p_Limit && p_Limit->data)
	{
		p_req->LimitFlag = 1;
		p_req->Limit = atoi(p_Limit->data);
	}

	p_StartReference = xml_node_soap_get(p_node, "StartReference");
	if (p_StartReference && p_StartReference->data)
	{
		p_req->StartReferenceFlag = 1;
		strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tdc_GetDoors(XMLN * p_node, tdc_GetDoors_REQ * p_req)
{
    int idx = 0;
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        strncpy(p_req->token[idx], p_Token->data, sizeof(p_req->token[idx])-1);

        if (++idx >= ARRAY_SIZE(p_req->token))
        {
            return ONVIF_ERR_TooManyItems;
        }
        
        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_CreateDoor(XMLN * p_node, tdc_CreateDoor_REQ * p_req)
{
    XMLN * p_Door;

    p_Door = xml_node_soap_get(p_node, "Door");
    if (p_Door)
    {
        parse_Door(p_Door, &p_req->Door);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_SetDoor(XMLN * p_node, tdc_SetDoor_REQ * p_req)
{
    XMLN * p_Door;

    p_Door = xml_node_soap_get(p_node, "Door");
    if (p_Door)
    {
        parse_Door(p_Door, &p_req->Door);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_ModifyDoor(XMLN * p_node, tdc_ModifyDoor_REQ * p_req)
{
    XMLN * p_Door;

    p_Door = xml_node_soap_get(p_node, "Door");
    if (p_Door)
    {
        parse_Door(p_Door, &p_req->Door);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_DeleteDoor(XMLN * p_node, tdc_DeleteDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_GetDoorInfoList(XMLN * p_node, tdc_GetDoorInfoList_REQ * p_req)
{
    XMLN * p_Limit;
	XMLN * p_StartReference;

	p_Limit = xml_node_soap_get(p_node, "Limit");
	if (p_Limit && p_Limit->data)
	{
		p_req->LimitFlag = 1;
		p_req->Limit = atoi(p_Limit->data);
	}

	p_StartReference = xml_node_soap_get(p_node, "StartReference");
	if (p_StartReference && p_StartReference->data)
	{
		p_req->StartReferenceFlag = 1;
		strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tdc_GetDoorInfo(XMLN * p_node, tdc_GetDoorInfo_REQ * p_req)
{
    int idx = 0;
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        strncpy(p_req->token[idx], p_Token->data, sizeof(p_req->token[idx])-1);

        if (++idx >= ARRAY_SIZE(p_req->token))
        {
            return ONVIF_ERR_TooManyItems;
        }
        
        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_GetDoorState(XMLN * p_node, tdc_GetDoorState_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_AccessDoor(XMLN * p_node, tdc_AccessDoor_REQ * p_req)
{
    XMLN * p_Token;
    XMLN * p_UseExtendedTime;
    XMLN * p_AccessTime;
    XMLN * p_OpenTooLongTime;
    XMLN * p_PreAlarmTime;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    p_UseExtendedTime = xml_node_soap_get(p_node, "UseExtendedTime");
    if (p_UseExtendedTime && p_UseExtendedTime->data)
    {
        p_req->UseExtendedTimeFlag = 1;
        p_req->UseExtendedTime = parse_Bool(p_UseExtendedTime->data);
    }

    p_AccessTime = xml_node_soap_get(p_node, "AccessTime");
    if (p_AccessTime && p_AccessTime->data)
    {
        p_req->AccessTimeFlag = 1;
        parse_XSDDuration(p_AccessTime->data, &p_req->AccessTime);
    }

    p_OpenTooLongTime = xml_node_soap_get(p_node, "AccessTime");
    if (p_OpenTooLongTime && p_OpenTooLongTime->data)
    {
        p_req->OpenTooLongTimeFlag = 1;
        parse_XSDDuration(p_OpenTooLongTime->data, &p_req->OpenTooLongTime);
    }

    p_PreAlarmTime = xml_node_soap_get(p_node, "PreAlarmTime");
    if (p_PreAlarmTime && p_PreAlarmTime->data)
    {
        p_req->PreAlarmTimeFlag = 1;
        parse_XSDDuration(p_PreAlarmTime->data, &p_req->PreAlarmTime);
    }

    return ONVIF_OK;    
}

ONVIF_RET parse_tdc_LockDoor(XMLN * p_node, tdc_LockDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_UnlockDoor(XMLN * p_node, tdc_UnlockDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_DoubleLockDoor(XMLN * p_node, tdc_DoubleLockDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_BlockDoor(XMLN * p_node, tdc_BlockDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_LockDownDoor(XMLN * p_node, tdc_LockDownDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_LockDownReleaseDoor(XMLN * p_node, tdc_LockDownReleaseDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_LockOpenDoor(XMLN * p_node, tdc_LockOpenDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tdc_LockOpenReleaseDoor(XMLN * p_node, tdc_LockOpenReleaseDoor_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

#endif  // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT

ONVIF_RET parse_PaneLayout(XMLN * p_node, onvif_PaneLayout * p_req)
{
    XMLN * p_Pane;
    XMLN * p_Area;

    p_Pane = xml_node_soap_get(p_node, "Pane");
    if (p_Pane && p_Pane->data)
    {
        strncpy(p_req->Pane, p_Pane->data, sizeof(p_req->Pane)-1);
    }

    p_Area = xml_node_soap_get(p_node, "Area");
    if (p_Area)
    {
        const char * p_bottom;
        const char * p_top;
        const char * p_right;
        const char * p_left;

        p_bottom = xml_attr_get(p_Area, "bottom");
        if (p_bottom)
        {
            p_req->Area.bottom = (float)atof(p_bottom);
        }

        p_top = xml_attr_get(p_Area, "V");
        if (p_top)
        {
            p_req->Area.top = (float)atof(p_top);
        }

        p_right = xml_attr_get(p_Area, "right");
        if (p_right)
        {
            p_req->Area.right = (float)atof(p_right);
        }

        p_left = xml_attr_get(p_Area, "left");
        if (p_left)
        {
            p_req->Area.left = (float)atof(p_left);
        }
    }

    return ONVIF_OK;
}

ONVIF_RET parse_Layout(XMLN * p_node, onvif_Layout * p_req)
{
    XMLN * p_PaneLayout;

    p_PaneLayout = xml_node_soap_get(p_node, "PaneLayout");
    while (p_PaneLayout && soap_strcmp(p_PaneLayout->name, "PaneLayout") == 0)
    {
        PaneLayoutList * p_item = onvif_add_PaneLayout(&p_req->PaneLayout);
        if (p_item)
        {
            parse_PaneLayout(p_PaneLayout, &p_item->PaneLayout);
        }
        
        p_PaneLayout = p_PaneLayout->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_VideoOutput(XMLN * p_node, onvif_VideoOutput * p_req)
{
    XMLN * p_Layout;
    XMLN * p_Resolution;
    XMLN * p_RefreshRate;
    XMLN * p_AspectRatio;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }
    
    p_Layout = xml_node_soap_get(p_node, "Layout");
    if (p_Layout)
    {
        parse_Layout(p_Layout, &p_req->Layout);
    }

    p_Resolution = xml_node_soap_get(p_node, "Resolution");
    if (p_Resolution)
    {
        p_req->ResolutionFlag = 1;
        parse_Resolution(p_Resolution, &p_req->Resolution);
    }

    p_RefreshRate = xml_node_soap_get(p_node, "RefreshRate");
    if (p_RefreshRate && p_RefreshRate->data)
    {
        p_req->RefreshRateFlag = 1;
        p_req->RefreshRate = (float)atof(p_RefreshRate->data);
    }

    p_AspectRatio = xml_node_soap_get(p_node, "AspectRatio");
    if (p_AspectRatio && p_AspectRatio->data)
    {
        p_req->AspectRatioFlag = 1;
        p_req->AspectRatio = (float)atof(p_AspectRatio->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_VideoOutputConfiguration(XMLN * p_node, onvif_VideoOutputConfiguration * p_req)
{
    XMLN * p_Name;
    XMLN * p_UseCount;
    XMLN * p_OutputToken;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_UseCount = xml_node_soap_get(p_node, "UseCount");
    if (p_UseCount && p_UseCount->data)
    {
        p_req->UseCount = atoi(p_UseCount->data);
    }

    p_OutputToken = xml_node_soap_get(p_node, "OutputToken");
    if (p_OutputToken && p_OutputToken->data)
    {
        strncpy(p_req->OutputToken, p_OutputToken->data, sizeof(p_req->OutputToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_AudioOutput(XMLN * p_node, onvif_AudioOutput * p_req)
{
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_AudioOutputConfigurationOptions(XMLN * p_node, onvif_AudioOutputConfigurationOptions * p_req)
{
	XMLN * p_OutputTokensAvailable;
	XMLN * p_SendPrimacyOptions;
	XMLN * p_OutputLevelRange;

    p_req->sizeOutputTokensAvailable = 0;
    
	p_OutputTokensAvailable = xml_node_soap_get(p_node, "OutputTokensAvailable");
	while (p_OutputTokensAvailable && p_OutputTokensAvailable->data && soap_strcmp(p_OutputTokensAvailable->name, "OutputTokensAvailable") == 0)
	{
		int idx = p_req->sizeOutputTokensAvailable;

		strncpy(p_req->OutputTokensAvailable[idx], p_OutputTokensAvailable->data, sizeof(p_req->OutputTokensAvailable[idx]));

		p_req->sizeOutputTokensAvailable++;
		if (p_req->sizeOutputTokensAvailable >= ARRAY_SIZE(p_req->OutputTokensAvailable))
		{
			break;
		}
		
		p_OutputTokensAvailable = p_OutputTokensAvailable->next;
	}

    p_req->sizeSendPrimacyOptions = 0;
    
	p_SendPrimacyOptions = xml_node_soap_get(p_node, "SendPrimacyOptions");
	while (p_SendPrimacyOptions && p_SendPrimacyOptions->data && soap_strcmp(p_SendPrimacyOptions->name, "SendPrimacyOptions") == 0)
	{
		int idx = p_req->sizeSendPrimacyOptions;

		strncpy(p_req->SendPrimacyOptions[idx], p_SendPrimacyOptions->data, sizeof(p_req->SendPrimacyOptions[idx]));

		p_req->sizeSendPrimacyOptions++;
		if (p_req->sizeSendPrimacyOptions >= ARRAY_SIZE(p_req->SendPrimacyOptions))
		{
			break;
		}
		
		p_SendPrimacyOptions = p_SendPrimacyOptions->next;
	}
	
	p_OutputLevelRange = xml_node_soap_get(p_node, "OutputLevelRange");
	if (p_OutputLevelRange)
	{
		parse_IntRange(p_OutputLevelRange, &p_req->OutputLevelRange);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_RelayOutputSettings(XMLN * p_node, onvif_RelayOutputSettings * p_req)
{
    XMLN * p_Mode;
    XMLN * p_DelayTime;
    XMLN * p_IdleState;

    p_Mode = xml_node_soap_get(p_node, "Mode");
    if (p_Mode && p_Mode->data)
    {
        p_req->Mode = onvif_StringToRelayMode(p_Mode->data);
    }

    p_DelayTime = xml_node_soap_get(p_node, "DelayTime");
    if (p_DelayTime && p_DelayTime->data)
    {
        parse_XSDDuration(p_DelayTime->data, (int*)&p_req->DelayTime);
    }

    p_IdleState = xml_node_soap_get(p_node, "IdleState");
    if (p_IdleState && p_IdleState->data)
    {
        p_req->IdleState = onvif_StringToRelayIdleState(p_IdleState->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_RelayOutput(XMLN * p_node, onvif_RelayOutput * p_req)
{
    XMLN * p_Properties;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }
    
    p_Properties = xml_node_soap_get(p_node, "Properties");
    if (p_Properties)
    {
        parse_RelayOutputSettings(p_Properties, &p_req->Properties);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_RelayOutputOptions(XMLN * p_node, onvif_RelayOutputOptions * p_req)
{
    XMLN * p_Mode;
    XMLN * p_DelayTimes;
    XMLN * p_Discrete;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }
    
    p_Mode = xml_node_soap_get(p_node, "Mode");
    while (p_Mode && p_Mode->data && soap_strcmp(p_Mode->name, "Mode") == 0)
    {
        if (soap_strcmp(p_Mode->data, "Bistable") == 0)
        {
            p_req->RelayMode_BistableFlag = 1;
        }
        else if (soap_strcmp(p_Mode->data, "Monostable") == 0)
        {
            p_req->RelayMode_MonostableFlag = 1;
        }
        
        p_Mode = p_Mode->next;
    }

    p_DelayTimes = xml_node_soap_get(p_node, "DelayTimes");
    if (p_DelayTimes && p_DelayTimes->data)
    {
        p_req->DelayTimesFlag = 1;
        strncpy(p_req->DelayTimes, p_DelayTimes->data, sizeof(p_req->DelayTimes)-1);
    }

    p_Discrete = xml_node_soap_get(p_node, "Discrete");
    if (p_Discrete && p_Discrete->data)
    {
        p_req->DiscreteFlag = 1;
        p_req->Discrete = parse_Bool(p_Discrete->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_DigitalInput(XMLN * p_node, onvif_DigitalInput * p_req)
{
    const char * p_token;
    const char * p_IdleState;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_IdleState = xml_attr_get(p_node, "IdleState");
    if (p_IdleState)
    {
        p_req->IdleStateFlag = 1;
        p_req->IdleState = onvif_StringToDigitalIdleState(p_IdleState);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_DigitalInputConfigurationOptions(XMLN * p_node, onvif_DigitalInputConfigurationOptions * p_req)
{
    XMLN * p_IdleState;

    p_IdleState = xml_node_soap_get(p_node, "IdleState");
    while (p_IdleState && p_IdleState->data && soap_strcmp(p_IdleState->name, "IdleState") == 0)
    {
        if (soap_strcmp(p_IdleState->data, "closed") == 0)
        {
            p_req->DigitalIdleState_closedFlag = 1;
        }
        else if (soap_strcmp(p_IdleState->data, "open") == 0)
        {
            p_req->DigitalIdleState_openFlag = 1;
        }
        
        p_IdleState = p_IdleState->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_SerialPortConfiguration(XMLN * p_node, onvif_SerialPortConfiguration * p_req)
{
    XMLN * p_BaudRate;
    XMLN * p_ParityBit;
    XMLN * p_CharacterLength;
    XMLN * p_StopBit;
    const char * p_token;
    const char * p_type;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_type = xml_attr_get(p_node, "type");
    if (p_type)
    {
        p_req->type = onvif_StringToSerialPortType(p_type);
    }

    p_BaudRate = xml_node_soap_get(p_node, "BaudRate");
    if (p_BaudRate && p_BaudRate->data)
    {
        p_req->BaudRate = atoi(p_BaudRate->data);
    }

    p_ParityBit = xml_node_soap_get(p_node, "ParityBit");
    if (p_ParityBit && p_ParityBit->data)
    {
        p_req->ParityBit = onvif_StringToParityBit(p_ParityBit->data);
    }

    p_CharacterLength = xml_node_soap_get(p_node, "CharacterLength");
    if (p_CharacterLength && p_CharacterLength->data)
    {
        p_req->CharacterLength = atoi(p_CharacterLength->data);
    }

    p_StopBit = xml_node_soap_get(p_node, "StopBit");
    if (p_StopBit && p_StopBit->data)
    {
        p_req->StopBit = (float)atof(p_StopBit->data);
    }

    return ONVIF_OK;
}

BOOL parse_ParityBitList(XMLN * p_node, onvif_ParityBitList * p_req)
{
    XMLN * p_Items;

    p_req->sizeItems = 0;
    
	p_Items = xml_node_soap_get(p_node, "Items");
    while (p_Items && p_Items->data && soap_strcmp(p_Items->name, "Items") == 0)
    {
    	int idx = p_req->sizeItems;

    	p_req->Items[idx] = onvif_StringToParityBit(p_Items->data);

    	p_req->sizeItems++;
    	if (p_req->sizeItems >= ARRAY_SIZE(p_req->Items))
    	{
    		break;
    	}

		p_Items = p_Items->next;
    }

    return TRUE;
}

ONVIF_RET parse_SerialPortConfigurationOptions(XMLN * p_node, onvif_SerialPortConfigurationOptions * p_req)
{
    XMLN * p_BaudRateList;
    XMLN * p_ParityBitList;
    XMLN * p_CharacterLengthList;
    XMLN * p_StopBitList;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_BaudRateList = xml_node_soap_get(p_node, "BaudRateList");
    if (p_BaudRateList)
    {
        parse_IntList(p_BaudRateList, &p_req->BaudRateList);
    }

    p_ParityBitList = xml_node_soap_get(p_node, "ParityBitList");
    if (p_ParityBitList)
    {
        parse_ParityBitList(p_ParityBitList, &p_req->ParityBitList);
    }

    p_CharacterLengthList = xml_node_soap_get(p_node, "CharacterLengthList");
    if (p_CharacterLengthList)
    {
        parse_IntList(p_CharacterLengthList, &p_req->CharacterLengthList);
    }

    p_StopBitList = xml_node_soap_get(p_node, "StopBitList");
    if (p_StopBitList)
    {
        parse_FloatList(p_StopBitList, &p_req->StopBitList);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_SetRelayOutputState(XMLN * p_node, tmd_SetRelayOutputState_REQ * p_req)
{
    XMLN * p_RelayOutputToken;
    XMLN * p_LogicalState;

    p_RelayOutputToken = xml_node_soap_get(p_node, "RelayOutputToken");
    if (p_RelayOutputToken && p_RelayOutputToken->data)
    {
        strncpy(p_req->RelayOutputToken, p_RelayOutputToken->data, sizeof(p_req->RelayOutputToken)-1);
    }

    p_LogicalState = xml_node_soap_get(p_node, "LogicalState");
    if (p_LogicalState && p_LogicalState->data)
    {
        p_req->LogicalState = onvif_StringToRelayLogicalState(p_LogicalState->data);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_GetVideoOutputConfiguration(XMLN * p_node, tmd_GetVideoOutputConfiguration_REQ * p_req)
{
    XMLN * p_VideoOutputToken;

    p_VideoOutputToken = xml_node_soap_get(p_node, "VideoOutputToken");
    if (p_VideoOutputToken && p_VideoOutputToken->data)
    {
        strncpy(p_req->VideoOutputToken, p_VideoOutputToken->data, sizeof(p_req->VideoOutputToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_SetVideoOutputConfiguration(XMLN * p_node, tmd_SetVideoOutputConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
    XMLN * p_ForcePersistence;

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_VideoOutputConfiguration(p_Configuration, &p_req->Configuration);
    }

    p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
    if (p_ForcePersistence && p_ForcePersistence->data)
    {
        p_req->ForcePersistence = parse_Bool(p_ForcePersistence->data);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tmd_GetVideoOutputConfigurationOptions(XMLN * p_node, tmd_GetVideoOutputConfigurationOptions_REQ * p_req)
{
    XMLN * p_VideoOutputToken;

    p_VideoOutputToken = xml_node_soap_get(p_node, "VideoOutputToken");
    if (p_VideoOutputToken && p_VideoOutputToken->data)
    {
        strncpy(p_req->VideoOutputToken, p_VideoOutputToken->data, sizeof(p_req->VideoOutputToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_GetAudioOutputConfiguration(XMLN * p_node, tmd_GetAudioOutputConfiguration_REQ * p_req)
{
    XMLN * p_AudioOutputToken;

    p_AudioOutputToken = xml_node_soap_get(p_node, "AudioOutputToken");
    if (p_AudioOutputToken && p_AudioOutputToken->data)
    {
        strncpy(p_req->AudioOutputToken, p_AudioOutputToken->data, sizeof(p_req->AudioOutputToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_GetAudioOutputConfigurationOptions(XMLN * p_node, tmd_GetAudioOutputConfigurationOptions_REQ * p_req)
{
    XMLN * p_AudioOutputToken;

    p_AudioOutputToken = xml_node_soap_get(p_node, "AudioOutputToken");
    if (p_AudioOutputToken && p_AudioOutputToken->data)
    {
        strncpy(p_req->AudioOutputToken, p_AudioOutputToken->data, sizeof(p_req->AudioOutputToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_GetRelayOutputOptions(XMLN * p_node, tmd_GetRelayOutputOptions_REQ * p_req)
{
    XMLN * p_RelayOutputToken;

    p_RelayOutputToken = xml_node_soap_get(p_node, "RelayOutputToken");
    if (p_RelayOutputToken && p_RelayOutputToken->data)
    {
        p_req->RelayOutputTokenFlag = 1;
        strncpy(p_req->RelayOutputToken, p_RelayOutputToken->data, sizeof(p_req->RelayOutputToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_SetRelayOutputSettings(XMLN * p_node, tmd_SetRelayOutputSettings_REQ * p_req)
{
    XMLN * p_RelayOutput;
    
    p_RelayOutput = xml_node_soap_get(p_node, "RelayOutput");
    if (p_RelayOutput)
    {
        parse_RelayOutput(p_RelayOutput, &p_req->RelayOutput);
    }

    return ONVIF_OK;    
}

ONVIF_RET parse_tmd_GetDigitalInputConfigurationOptions(XMLN * p_node, tmd_GetDigitalInputConfigurationOptions_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        p_req->TokenFlag = 1;
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_SetDigitalInputConfigurations(XMLN * p_node, tmd_SetDigitalInputConfigurations_REQ * p_req)
{
    XMLN * p_DigitalInputs;

    p_DigitalInputs = xml_node_soap_get(p_node, "DigitalInputs");
    while (p_DigitalInputs && soap_strcmp(p_DigitalInputs->name, "DigitalInputs") == 0)
    {
        DigitalInputList * p_input = onvif_add_DigitalInput(&p_req->DigitalInputs);
        if (p_input)
        {
            parse_DigitalInput(p_DigitalInputs, &p_input->DigitalInput);
        }
        
        p_DigitalInputs = p_DigitalInputs->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_GetSerialPortConfiguration(XMLN * p_node, tmd_GetSerialPortConfiguration_REQ * p_req)
{
    XMLN * p_SerialPortToken;

    p_SerialPortToken = xml_node_soap_get(p_node, "SerialPortToken");
    if (p_SerialPortToken && p_SerialPortToken->data)
    {
        strncpy(p_req->SerialPortToken, p_SerialPortToken->data, sizeof(p_req->SerialPortToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_GetSerialPortConfigurationOptions(XMLN * p_node, tmd_GetSerialPortConfigurationOptions_REQ * p_req)
{
    XMLN * p_SerialPortToken;

    p_SerialPortToken = xml_node_soap_get(p_node, "SerialPortToken");
    if (p_SerialPortToken && p_SerialPortToken->data)
    {
        strncpy(p_req->SerialPortToken, p_SerialPortToken->data, sizeof(p_req->SerialPortToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tmd_SetSerialPortConfiguration(XMLN * p_node, tmd_SetSerialPortConfiguration_REQ * p_req)
{
    XMLN * p_SerialPortConfiguration;
    XMLN * p_ForcePersistance;

    p_SerialPortConfiguration = xml_node_soap_get(p_node, "SerialPortConfiguration");
    if (p_SerialPortConfiguration)
    {
        parse_SerialPortConfiguration(p_SerialPortConfiguration, &p_req->SerialPortConfiguration);
    }

    p_ForcePersistance = xml_node_soap_get(p_node, "ForcePersistance");
    if (p_ForcePersistance && p_ForcePersistance->data)
    {
        p_req->ForcePersistance = parse_Bool(p_ForcePersistance->data);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tmd_SendReceiveSerialCommand(XMLN * p_node, tmd_SendReceiveSerialCommand_REQ * p_req)
{
    XMLN * p_token;
    XMLN * p_SerialData;
    XMLN * p_TimeOut;
    XMLN * p_DataLength;
    XMLN * p_Delimiter;

    p_token = xml_node_soap_get(p_node, "token");
    if (p_token && p_token->data)
    {
        strncpy(p_req->token, p_token->data, sizeof(p_req->token)-1);
    }
    
    p_SerialData = xml_node_soap_get(p_node, "SerialData");
    if (p_SerialData)
    {
        XMLN * p_Binary;
        XMLN * p_String;

        p_Binary = xml_node_soap_get(p_SerialData, "Binary");
        if (p_Binary && p_Binary->data)
        {
            p_req->Command.SerialData._union_SerialData = 0;

            onvif_malloc_SerialData(&p_req->Command.SerialData, 0, (int)strlen(p_Binary->data)+1);
            strcpy(p_req->Command.SerialData.union_SerialData.Binary, p_Binary->data);            
        }

        p_String = xml_node_soap_get(p_SerialData, "String");
        if (p_String && p_String->data)
        {
            p_req->Command.SerialData._union_SerialData = 1;

            onvif_malloc_SerialData(&p_req->Command.SerialData, 1, (int)strlen(p_String->data)+1);
            strcpy(p_req->Command.SerialData.union_SerialData.String, p_String->data);                                    
        }
        
        p_req->Command.SerialDataFlag = 1;
    }

    p_TimeOut = xml_node_soap_get(p_node, "TimeOut");
    if (p_TimeOut && p_TimeOut->data)
    {
        p_req->Command.TimeOutFlag = 1;
        parse_XSDDuration(p_TimeOut->data, (int *)&p_req->Command.TimeOut);
    }

    p_DataLength = xml_node_soap_get(p_node, "DataLength");
    if (p_DataLength && p_DataLength->data)
    {
        p_req->Command.DataLengthFlag = 1;
        p_req->Command.DataLength = atoi(p_DataLength->data);
    }

    p_Delimiter = xml_node_soap_get(p_node, "Delimiter");
    if (p_Delimiter && p_Delimiter->data)
    {
        p_req->Command.DelimiterFlag = 1;
        strncpy(p_req->Command.Delimiter, p_Delimiter->data, sizeof(p_req->Command.Delimiter)-1);
    }
    
    return ONVIF_OK;
}

#endif // end of DEVICEIO_SUPPORT

#ifdef MEDIA2_SUPPORT

ONVIF_RET parse_ConfigurationRef(XMLN * p_node, onvif_ConfigurationRef * p_req)
{
    XMLN * p_Type;
    XMLN * p_Token;

    p_Type = xml_node_soap_get(p_node, "Type");
    if (p_Type && p_Type->data)
    {
        strncpy(p_req->Type, p_Type->data, sizeof(p_req->Type)-1);
    }

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        p_req->TokenFlag = 1;
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

BOOL parse_Polygon(XMLN * p_node, onvif_Polygon * p_req)
{
	XMLN * p_Point;

	p_Point = xml_node_soap_get(p_node, "Point");
	while (p_Point && soap_strcmp(p_Point->name, "Point") == 0)
	{
		int idx = p_req->sizePoint;

		parse_Vector(p_Point, &p_req->Point[idx]);

		p_req->sizePoint++;
		if (p_req->sizePoint >= ARRAY_SIZE(p_req->Point))
		{
			break;
		}
		p_Point = p_Point->next;
	}

	return TRUE;
}

BOOL parse_Color(XMLN * p_node, onvif_Color * p_req)
{
	const char * p_X;
	const char * p_Y;
	const char * p_Z;
	const char * p_Colorspace;

	p_X = xml_attr_get(p_node, "X");
	if (p_X)
	{
		p_req->X = (float)atof(p_X);
	}

	p_Y = xml_attr_get(p_node, "Y");
	if (p_Y)
	{
		p_req->Y = (float)atof(p_Y);
	}

	p_Z = xml_attr_get(p_node, "Z");
	if (p_Z)
	{
		p_req->Z = (float)atof(p_Z);
	}

	p_Colorspace = xml_attr_get(p_node, "Colorspace");
	if (p_Colorspace)
	{
		p_req->ColorspaceFlag = 1;
		strncpy(p_req->Colorspace, p_Colorspace, sizeof(p_req->Colorspace)-1);
	}

	return TRUE;
}

ONVIF_RET parse_Mask(XMLN * p_node, onvif_Mask * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_Polygon;
	XMLN * p_Type;
	XMLN * p_Color;
	XMLN * p_Enabled;
	const char * p_token;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_Polygon = xml_node_soap_get(p_node, "Polygon");
	if (p_Polygon)
	{
		parse_Polygon(p_Polygon, &p_req->Polygon);
	}

	p_Type = xml_node_soap_get(p_node, "Type");
	if (p_Type && p_Type->data)
	{
		strncpy(p_req->Type, p_Type->data, sizeof(p_req->Type)-1);
	}

	p_Color = xml_node_soap_get(p_node, "Color");
	if (p_Color)
	{
		p_req->ColorFlag = parse_Color(p_Color, &p_req->Color);
	}

	p_Enabled = xml_node_soap_get(p_node, "Enabled");
	if (p_Enabled && p_Enabled->data)
	{
		p_req->Enabled = parse_Bool(p_Enabled->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetConfiguration(XMLN * p_node, tr2_GetConfiguration * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_ProfileToken;

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetVideoEncoderConfiguration(XMLN * p_node, tr2_SetVideoEncoderConfiguration_REQ * p_req)
{
	XMLN * p_Configuration;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		parse_VideoEncoder2Configuration(p_Configuration, &p_req->Configuration);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_CreateProfile(XMLN * p_node, tr2_CreateProfile_REQ * p_req)
{
    XMLN * p_Name;
    XMLN * p_Configuration;

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    while (p_Configuration && soap_strcmp(p_Configuration->name, "Configuration") == 0)
    {
        int idx = p_req->sizeConfiguration;

        parse_ConfigurationRef(p_Configuration, &p_req->Configuration[idx]);

        p_req->sizeConfiguration++;
        if (p_req->sizeConfiguration >= ARRAY_SIZE(p_req->Configuration))
        {
            break;
        }

        p_Configuration = p_Configuration->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetProfiles(XMLN * p_node, tr2_GetProfiles_REQ * p_req)
{
    XMLN * p_Token;
    XMLN * p_Type;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        p_req->TokenFlag = 1;
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    p_Type = xml_node_soap_get(p_node, "Type");
    while (p_Type && p_Type->data && soap_strcmp(p_Type->name, "Type") == 0)
    {
        int idx = p_req->sizeType;
        
        strncpy(p_req->Type[idx], p_Type->data, sizeof(p_req->Type[idx])-1);        

        p_req->sizeType++;
        if (p_req->sizeType >= ARRAY_SIZE(p_req->Type))
        {
            break;
        }

        p_Type = p_Type->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tr2_DeleteProfile(XMLN * p_node, tr2_DeleteProfile_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tr2_AddConfiguration(XMLN * p_node, tr2_AddConfiguration_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_Name;
    XMLN * p_Configuration;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        p_req->NameFlag = 1;
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    while (p_Configuration && soap_strcmp(p_Configuration->name, "Configuration") == 0)
    {
        int idx = p_req->sizeConfiguration;

        parse_ConfigurationRef(p_Configuration, &p_req->Configuration[idx]);

        p_req->sizeConfiguration++;
        if (p_req->sizeConfiguration >= ARRAY_SIZE(p_req->Configuration))
        {
            break;
        }

        p_Configuration = p_Configuration->next;
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tr2_RemoveConfiguration(XMLN * p_node, tr2_RemoveConfiguration_REQ * p_req)
{
    XMLN * p_ProfileToken;
    XMLN * p_Configuration;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    while (p_Configuration && soap_strcmp(p_Configuration->name, "Configuration") == 0)
    {
        int idx = p_req->sizeConfiguration;

        parse_ConfigurationRef(p_Configuration, &p_req->Configuration[idx]);

        p_req->sizeConfiguration++;
        if (p_req->sizeConfiguration >= ARRAY_SIZE(p_req->Configuration))
        {
            break;
        }

        p_Configuration = p_Configuration->next;
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetVideoSourceConfiguration(XMLN * p_node, tr2_SetVideoSourceConfiguration_REQ * p_req)
{
	XMLN * p_Configuration;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		parse_VideoSourceConfiguration(p_Configuration, &p_req->Configuration);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetMetadataConfiguration(XMLN * p_node, tr2_SetMetadataConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
    
    p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_MetadataConfiguration(p_Configuration, &p_req->Configuration);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetVideoEncoderInstances(XMLN * p_node, tr2_GetVideoEncoderInstances_REQ * p_req)
{
    XMLN * p_ConfigurationToken;

    p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
    if (p_ConfigurationToken && p_ConfigurationToken->data)
    {
        strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetStreamUri(XMLN * p_node, tr2_GetStreamUri_REQ * p_req)
{
	XMLN * p_Protocol;
	XMLN * p_ProfileToken;

	p_Protocol = xml_node_soap_get(p_node, "Protocol");
	if (p_Protocol && p_Protocol->data)
	{
		strncpy(p_req->Protocol, p_Protocol->data, sizeof(p_req->Protocol)-1);
	}

	p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetSynchronizationPoint(XMLN * p_node, tr2_SetSynchronizationPoint_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetVideoSourceModes(XMLN * p_node, tr2_GetVideoSourceModes_REQ * p_req)
{
    XMLN * p_VideoSourceToken;

    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetVideoSourceMode(XMLN * p_node, tr2_SetVideoSourceMode_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    XMLN * p_VideoSourceModeToken;

    p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	p_VideoSourceModeToken = xml_node_soap_get(p_node, "VideoSourceModeToken");
	if (p_VideoSourceModeToken && p_VideoSourceModeToken->data)
	{
		strncpy(p_req->VideoSourceModeToken, p_VideoSourceModeToken->data, sizeof(p_req->VideoSourceModeToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetSnapshotUri(XMLN * p_node, tr2_GetSnapshotUri_REQ * p_req)
{
    XMLN * p_ProfileToken;

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
	    strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetOSDs(XMLN * p_node, tr2_GetOSDs_REQ * p_req)
{
	XMLN * p_OSDToken;
	XMLN * p_ConfigurationToken;

	p_OSDToken = xml_node_soap_get(p_node, "OSDToken");
	if (p_OSDToken && p_OSDToken->data)
	{
		p_req->OSDTokenFlag = 1;
		strncpy(p_req->OSDToken, p_OSDToken->data, sizeof(p_req->OSDToken)-1);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_CreateMask(XMLN * p_node, tr2_CreateMask_REQ * p_req)
{
    XMLN * p_Mask;

    p_Mask = xml_node_soap_get(p_node, "Mask");
    if (p_Mask)
    {
        parse_Mask(p_Mask, &p_req->Mask);
    }

    return ONVIF_OK;    
}

ONVIF_RET parse_tr2_DeleteMask(XMLN * p_node, tr2_DeleteMask_REQ * p_req)
{
    XMLN * p_Token;
    
    p_Token = xml_node_soap_get(p_node, "Token");
	if (p_Token && p_Token->data)
	{
	    strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_GetMasks(XMLN * p_node, tr2_GetMasks_REQ * p_req)
{
	XMLN * p_Token;
	XMLN * p_ConfigurationToken;

	p_Token = xml_node_soap_get(p_node, "Token");
	if (p_Token && p_Token->data)
	{
		p_req->TokenFlag = 1;
		strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetMask(XMLN * p_node, tr2_SetMask_REQ * p_req)
{
	XMLN * p_Mask;

	p_Mask = xml_node_soap_get(p_node, "Mask");
	if (p_Mask)
	{
		parse_Mask(p_Mask, &p_req->Mask);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tr2_StartMulticastStreaming(XMLN * p_node, tr2_StartMulticastStreaming_REQ * p_req)
{
    XMLN * p_ProfileToken;
    
    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tr2_StopMulticastStreaming(XMLN * p_node, tr2_StopMulticastStreaming_REQ * p_req)
{
    XMLN * p_ProfileToken;
    
    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        strncpy(p_req->ProfileToken, p_ProfileToken->data, sizeof(p_req->ProfileToken)-1);
    }
    
    return ONVIF_OK;
}

#ifdef DEVICEIO_SUPPORT

ONVIF_RET parse_tr2_GetAudioOutputConfigurations(XMLN * p_node, tr2_GetAudioOutputConfigurations_REQ * p_req)
{
    XMLN * p_ConfigurationToken;
    XMLN * p_ProfileToken;

    p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
    if (p_ConfigurationToken && p_ConfigurationToken->data)
    {
        p_req->GetConfiguration.ConfigurationTokenFlag = 1;
        strncpy(p_req->GetConfiguration.ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->GetConfiguration.ConfigurationToken)-1);
    }

    p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
    if (p_ProfileToken && p_ProfileToken->data)
    {
        p_req->GetConfiguration.ProfileTokenFlag = 1;
        strncpy(p_req->GetConfiguration.ProfileToken, p_ProfileToken->data, sizeof(p_req->GetConfiguration.ProfileToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetAudioOutputConfiguration(XMLN * p_node, tr2_SetAudioOutputConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_AudioOutputConfiguration(p_Configuration, &p_req->Configuration);
    }

    return ONVIF_OK;
}

#endif // end of DEVICEIO_SUPPORT

#ifdef AUDIO_SUPPORT

ONVIF_RET parse_AudioEncoder2ConfigurationOptions(XMLN * p_node, onvif_AudioEncoder2ConfigurationOptions * p_req)
{
	XMLN * p_Encoding;
	XMLN * p_BitrateList;
	XMLN * p_SampleRateList;

	p_Encoding = xml_node_soap_get(p_node, "Encoding");
    if (p_Encoding && p_Encoding->data)
    {
		strncpy(p_req->Encoding, p_Encoding->data, sizeof(p_req->Encoding)-1);
    }

    p_BitrateList = xml_node_soap_get(p_node, "BitrateList");
    if (p_BitrateList)
    {
    	parse_IntList(p_BitrateList, &p_req->BitrateList);
    }

	p_SampleRateList = xml_node_soap_get(p_node, "SampleRateList");
    if (p_SampleRateList)
    {
    	parse_IntList(p_SampleRateList, &p_req->SampleRateList);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetAudioSourceConfiguration(XMLN * p_node, tr2_SetAudioSourceConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;
    
    p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_AudioSourceConfiguration(p_Configuration, &p_req->Configuration);
	}
	
    return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetAudioEncoderConfiguration(XMLN * p_node, tr2_SetAudioEncoderConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_AudioEncoder2Configuration(p_Configuration, &p_req->Configuration);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tr2_SetAudioDecoderConfiguration(XMLN * p_node, tr2_SetAudioDecoderConfiguration_REQ * p_req)
{
    XMLN * p_Configuration;

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_AudioDecoderConfiguration(p_Configuration, &p_req->Configuration);
	}

	return ONVIF_OK;
}

#endif // end of AUDIO_SUPPORT

#endif // end of MEDIA2_SUPPORT

#ifdef THERMAL_SUPPORT

ONVIF_RET parse_ColorPalette(XMLN * p_node, onvif_ColorPalette * p_req)
{
    const char * p_token;
    const char * p_Type;
    XMLN * p_Name;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Type = xml_attr_get(p_node, "Type");
    if (p_Type)
    {
        strncpy(p_req->Type, p_Type, sizeof(p_req->Type)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
	    strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_NUCTable(XMLN * p_node, onvif_NUCTable * p_req)
{
    const char * p_token;
    const char * p_LowTemperature;
    const char * p_HighTemperature;
    XMLN * p_Name;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_LowTemperature = xml_attr_get(p_node, "LowTemperature");
    if (p_LowTemperature)
    {
        p_req->LowTemperatureFlag = 1;
        p_req->LowTemperature = (float) atof(p_LowTemperature);
    }

    p_HighTemperature = xml_attr_get(p_node, "HighTemperature");
    if (p_HighTemperature)
    {
        p_req->HighTemperatureFlag = 1;
        p_req->HighTemperature = (float) atof(p_HighTemperature);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
	    strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_Cooler(XMLN * p_node, onvif_Cooler * p_req)
{
    XMLN * p_Enabled;
    XMLN * p_RunTime;

    p_Enabled = xml_node_soap_get(p_node, "Enabled");
	if (p_Enabled && p_Enabled->data)
	{
	    p_req->Enabled = parse_Bool(p_Enabled->data);
	}

	p_RunTime = xml_node_soap_get(p_node, "RunTime");
	if (p_RunTime && p_RunTime->data)
	{
	    p_req->RunTime = (float) atof(p_RunTime->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_ThermalConfiguration(XMLN * p_node, onvif_ThermalConfiguration * p_req)
{
    XMLN * p_ColorPalette;
    XMLN * p_Polarity;
    XMLN * p_NUCTable;
    XMLN * p_Cooler;

    p_ColorPalette = xml_node_soap_get(p_node, "ColorPalette");
	if (p_ColorPalette)
	{
	    parse_ColorPalette(p_ColorPalette, &p_req->ColorPalette);
	}

	p_Polarity = xml_node_soap_get(p_node, "Polarity");
	if (p_Polarity && p_Polarity->data)
	{
	    p_req->Polarity = onvif_StringToPolarity(p_Polarity->data);
	}

	p_NUCTable = xml_node_soap_get(p_node, "NUCTable");
	if (p_NUCTable)
	{
	    p_req->NUCTableFlag = 1;
	    parse_NUCTable(p_NUCTable, &p_req->NUCTable);
	}

	p_Cooler = xml_node_soap_get(p_node, "Cooler");
	if (p_Cooler)
	{
	    p_req->CoolerFlag = 1;
	    parse_Cooler(p_Cooler, &p_req->Cooler);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_RadiometryGlobalParameters(XMLN * p_node, onvif_RadiometryGlobalParameters * p_req)
{
    XMLN * p_ReflectedAmbientTemperature;
    XMLN * p_Emissivity;
    XMLN * p_DistanceToObject;
    XMLN * p_RelativeHumidity;
    XMLN * p_AtmosphericTemperature;
    XMLN * p_AtmosphericTransmittance;
    XMLN * p_ExtOpticsTemperature;
    XMLN * p_ExtOpticsTransmittance;

    p_ReflectedAmbientTemperature = xml_node_soap_get(p_node, "ReflectedAmbientTemperature");
	if (p_ReflectedAmbientTemperature && p_ReflectedAmbientTemperature->data)
	{
	    p_req->ReflectedAmbientTemperature = (float) atof(p_ReflectedAmbientTemperature->data);
	}

	p_Emissivity = xml_node_soap_get(p_node, "Emissivity");
	if (p_Emissivity && p_Emissivity->data)
	{
	    p_req->Emissivity = (float) atof(p_Emissivity->data);
	}

	p_DistanceToObject = xml_node_soap_get(p_node, "DistanceToObject");
	if (p_DistanceToObject && p_DistanceToObject->data)
	{
	    p_req->DistanceToObject = (float) atof(p_DistanceToObject->data);
	}

	p_RelativeHumidity = xml_node_soap_get(p_node, "RelativeHumidity");
	if (p_RelativeHumidity && p_RelativeHumidity->data)
	{
	    p_req->RelativeHumidityFlag = 1;
	    p_req->RelativeHumidity = (float) atof(p_RelativeHumidity->data);
	}

    p_AtmosphericTemperature = xml_node_soap_get(p_node, "AtmosphericTemperature");
	if (p_AtmosphericTemperature && p_AtmosphericTemperature->data)
	{
	    p_req->AtmosphericTemperatureFlag = 1;
	    p_req->AtmosphericTemperature = (float) atof(p_AtmosphericTemperature->data);
	}

	p_AtmosphericTransmittance = xml_node_soap_get(p_node, "AtmosphericTransmittance");
	if (p_AtmosphericTransmittance && p_AtmosphericTransmittance->data)
	{
	    p_req->AtmosphericTransmittanceFlag = 1;
	    p_req->AtmosphericTransmittance = (float) atof(p_AtmosphericTransmittance->data);
	}

    p_ExtOpticsTemperature = xml_node_soap_get(p_node, "ExtOpticsTemperature");
	if (p_ExtOpticsTemperature && p_ExtOpticsTemperature->data)
	{
	    p_req->ExtOpticsTemperatureFlag = 1;
	    p_req->ExtOpticsTemperature = (float) atof(p_ExtOpticsTemperature->data);
	}

	p_ExtOpticsTransmittance = xml_node_soap_get(p_node, "ExtOpticsTransmittance");
	if (p_ExtOpticsTransmittance && p_ExtOpticsTransmittance->data)
	{
	    p_req->ExtOpticsTransmittanceFlag = 1;
	    p_req->ExtOpticsTransmittance = (float) atof(p_ExtOpticsTransmittance->data);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_RadiometryConfiguration(XMLN * p_node, onvif_RadiometryConfiguration * p_req)
{
    XMLN * p_RadiometryGlobalParameters;

    p_RadiometryGlobalParameters = xml_node_soap_get(p_node, "RadiometryGlobalParameters");
	if (p_RadiometryGlobalParameters)
	{
	    p_req->RadiometryGlobalParametersFlag = 1;

	    parse_RadiometryGlobalParameters(p_RadiometryGlobalParameters, &p_req->RadiometryGlobalParameters);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tth_GetConfiguration(XMLN * p_node, tth_GetConfiguration_REQ * p_req)
{
    XMLN * p_VideoSourceToken;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
	    strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tth_SetConfiguration(XMLN * p_node, tth_SetConfiguration_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    XMLN * p_Configuration;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
	    strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_ThermalConfiguration(p_Configuration, &p_req->Configuration);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tth_GetConfigurationOptions(XMLN * p_node, tth_GetConfigurationOptions_REQ * p_req)
{
    XMLN * p_VideoSourceToken;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
	    strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tth_GetRadiometryConfiguration(XMLN * p_node, tth_GetRadiometryConfiguration_REQ * p_req)
{
	XMLN * p_VideoSourceToken;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tth_SetRadiometryConfiguration(XMLN * p_node, tth_SetRadiometryConfiguration_REQ * p_req)
{
    XMLN * p_VideoSourceToken;
    XMLN * p_Configuration;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
	    strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
	    parse_RadiometryConfiguration(p_Configuration, &p_req->Configuration);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tth_GetRadiometryConfigurationOptions(XMLN * p_node, tth_GetRadiometryConfigurationOptions_REQ * p_req)
{
    XMLN * p_VideoSourceToken;

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
	    strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT

ONVIF_RET parse_CredentialIdentifierType(XMLN * p_node, onvif_CredentialIdentifierType * p_req)
{
    XMLN * p_Name;
    XMLN * p_FormatType;

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_FormatType = xml_node_soap_get(p_node, "FormatType");
    if (p_FormatType && p_FormatType->data)
    {
        strncpy(p_req->FormatType, p_FormatType->data, sizeof(p_req->FormatType)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_CredentialIdentifier(XMLN * p_node, onvif_CredentialIdentifier * p_req)
{
    XMLN * p_Type;
    XMLN * p_ExemptedFromAuthentication;
    XMLN * p_Value;

    p_Type = xml_node_soap_get(p_node, "Type");
    if (p_Type)
    {
        parse_CredentialIdentifierType(p_Type, &p_req->Type);
    }

    p_ExemptedFromAuthentication = xml_node_soap_get(p_node, "ExemptedFromAuthentication");
    if (p_ExemptedFromAuthentication && p_ExemptedFromAuthentication->data)
    {
        p_req->ExemptedFromAuthentication = parse_Bool(p_ExemptedFromAuthentication->data);
    }

    p_Value = xml_node_soap_get(p_node, "Value");
    if (p_Value && p_Value->data)
    {
        strncpy(p_req->Value, p_Value->data, sizeof(p_req->Value)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_CredentialIdentifierItem(XMLN * p_node, onvif_CredentialIdentifierItem * p_req)
{
    XMLN * p_Type;
    XMLN * p_Value;

    p_Type = xml_node_soap_get(p_node, "Type");
    if (p_Type)
    {
        parse_CredentialIdentifierType(p_Type, &p_req->Type);
    }

    p_Value = xml_node_soap_get(p_node, "Value");
    if (p_Value && p_Value->data)
    {
        strncpy(p_req->Value, p_Value->data, sizeof(p_req->Value)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_CredentialAccessProfile(XMLN * p_node, onvif_CredentialAccessProfile * p_req)
{
    XMLN * p_AccessProfileToken;
    XMLN * p_ValidFrom;
    XMLN * p_ValidTo;

    p_AccessProfileToken = xml_node_soap_get(p_node, "AccessProfileToken");
    if (p_AccessProfileToken && p_AccessProfileToken->data)
    {
        strncpy(p_req->AccessProfileToken, p_AccessProfileToken->data, sizeof(p_req->AccessProfileToken)-1);
    }

    p_ValidFrom = xml_node_soap_get(p_node, "ValidFrom");
    if (p_ValidFrom && p_ValidFrom->data)
    {
        p_req->ValidFromFlag = 1;
        strncpy(p_req->ValidFrom, p_ValidFrom->data, sizeof(p_req->ValidFrom)-1);
    }

    p_ValidTo = xml_node_soap_get(p_node, "ValidTo");
    if (p_ValidTo && p_ValidTo->data)
    {
        p_req->ValidToFlag = 1;
        strncpy(p_req->ValidTo, p_ValidTo->data, sizeof(p_req->ValidFrom)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_Attribute(XMLN * p_node, onvif_Attribute * p_req)
{
    const char * p_Name;
    const char * p_Value;

    p_Name = xml_attr_get(p_node, "Name");
    if (p_Name)
    {
        strncpy(p_req->Name, p_Name, sizeof(p_req->Name)-1);
    }

    p_Value = xml_attr_get(p_node, "Value");
    if (p_Value)
    {
        p_req->ValueFlag = 1;
        strncpy(p_req->Value, p_Value, sizeof(p_req->Value)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_Credential(XMLN * p_node, onvif_Credential * p_req)
{
    XMLN * p_Description;
    XMLN * p_CredentialHolderReference;
    XMLN * p_ValidFrom;
    XMLN * p_ValidTo;
    XMLN * p_CredentialIdentifier;
    XMLN * p_CredentialAccessProfile;
    XMLN * p_Attribute;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }
    
    p_Description = xml_node_soap_get(p_node, "Description");
    if (p_Description && p_Description->data)
    {
        p_req->DescriptionFlag = 1;
        strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description)-1);
    }

    p_CredentialHolderReference = xml_node_soap_get(p_node, "CredentialHolderReference");
    if (p_CredentialHolderReference && p_CredentialHolderReference->data)
    {
        strncpy(p_req->CredentialHolderReference, p_CredentialHolderReference->data, sizeof(p_req->CredentialHolderReference)-1);
    }

    p_ValidFrom = xml_node_soap_get(p_node, "ValidFrom");
    if (p_ValidFrom && p_ValidFrom->data)
    {
        p_req->ValidFromFlag = 1;
        strncpy(p_req->ValidFrom, p_ValidFrom->data, sizeof(p_req->ValidFrom)-1);
    }

    p_ValidTo = xml_node_soap_get(p_node, "ValidTo");
    if (p_ValidTo && p_ValidTo->data)
    {
        p_req->ValidToFlag = 1;
        strncpy(p_req->ValidTo, p_ValidTo->data, sizeof(p_req->ValidTo)-1);
    }

    p_CredentialIdentifier = xml_node_soap_get(p_node, "CredentialIdentifier");
    while (p_CredentialIdentifier && soap_strcmp(p_CredentialIdentifier->name, "CredentialIdentifier") == 0)
    {
        int idx = p_req->sizeCredentialIdentifier;

        parse_CredentialIdentifier(p_CredentialIdentifier, &p_req->CredentialIdentifier[idx]);

        p_req->CredentialIdentifier[idx].Used = TRUE;
        p_req->sizeCredentialIdentifier++;
        if (p_req->sizeCredentialIdentifier >= ARRAY_SIZE(p_req->CredentialIdentifier))
        {
            break;
        }

        p_CredentialIdentifier = p_CredentialIdentifier->next;
    }

    p_CredentialAccessProfile = xml_node_soap_get(p_node, "CredentialAccessProfile");
    while (p_CredentialAccessProfile && soap_strcmp(p_CredentialAccessProfile->name, "CredentialAccessProfile") == 0)
    {
        int idx = p_req->sizeCredentialAccessProfile;

        parse_CredentialAccessProfile(p_CredentialAccessProfile, &p_req->CredentialAccessProfile[idx]);

        p_req->CredentialAccessProfile[idx].Used = TRUE;
        p_req->sizeCredentialAccessProfile++;
        if (p_req->sizeCredentialAccessProfile >= ARRAY_SIZE(p_req->CredentialAccessProfile))
        {
            break;
        }

        p_CredentialAccessProfile = p_CredentialAccessProfile->next;
    }

    p_Attribute = xml_node_soap_get(p_node, "Attribute");
    while (p_Attribute && soap_strcmp(p_Attribute->name, "Attribute") == 0)
    {
        int idx = p_req->sizeAttribute;

        parse_Attribute(p_Attribute, &p_req->Attribute[idx]);

        p_req->sizeAttribute++;
        if (p_req->sizeAttribute >= ARRAY_SIZE(p_req->Attribute))
        {
            break;
        }

        p_Attribute = p_Attribute->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_CredentialState(XMLN * p_node, onvif_CredentialState * p_req)
{
    XMLN * p_Enabled;
    XMLN * p_Reason;
    XMLN * p_AntipassbackState;

    p_Enabled = xml_node_soap_get(p_node, "Enabled");
    if (p_Enabled && p_Enabled->data)
    {
        p_req->Enabled = parse_Bool(p_Enabled->data);
    }

    p_Reason = xml_node_soap_get(p_node, "Reason");
    if (p_Reason && p_Reason->data)
    {
        p_req->ReasonFlag = 1;
        strncpy(p_req->Reason, p_Reason->data, sizeof(p_req->Reason)-1);
    }

    p_AntipassbackState = xml_node_soap_get(p_node, "AntipassbackState");
    if (p_AntipassbackState)
    {
        XMLN * p_AntipassbackViolated;
        
        p_req->AntipassbackStateFlag = 1;

        p_AntipassbackViolated = xml_node_soap_get(p_AntipassbackState, "AntipassbackViolated");
        if (p_AntipassbackViolated && p_AntipassbackViolated->data)
        {
            p_req->AntipassbackState.AntipassbackViolated = parse_Bool(p_AntipassbackViolated->data);
        }
    }

    return ONVIF_OK;
}

ONVIF_RET parse_CredentialIdentifierItemList(XMLN * p_node, CredentialIdentifierItemList ** p_req)
{
    XMLN * p_Identifier;

    p_Identifier = xml_node_soap_get(p_node, "Identifier");
    while (p_Identifier && soap_strcmp(p_Identifier->name, "Identifier") == 0)
    {
        CredentialIdentifierItemList * p_item = onvif_add_CredentialIdentifierItem(p_req);
        if (p_item)
        {
            parse_CredentialIdentifierItem(p_Identifier, &p_item->Item);
        }
        
        p_Identifier = p_Identifier->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetCredentialInfo(XMLN * p_node, tcr_GetCredentialInfo_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetCredentialInfoList(XMLN * p_node, tcr_GetCredentialInfoList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetCredentials(XMLN * p_node, tcr_GetCredentials_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetCredentialList(XMLN * p_node, tcr_GetCredentialList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_CreateCredential(XMLN * p_node, tcr_CreateCredential_REQ * p_req)
{
    XMLN * p_Credential;
    XMLN * p_State;

    p_Credential = xml_node_soap_get(p_node, "Credential");
    if (p_Credential)
    {
        parse_Credential(p_Credential, &p_req->Credential);
    }

    p_State = xml_node_soap_get(p_node, "State");
    if (p_State)
    {
        parse_CredentialState(p_State, &p_req->State);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_ModifyCredential(XMLN * p_node, tcr_ModifyCredential_REQ * p_req)
{
    XMLN * p_Credential;

    p_Credential = xml_node_soap_get(p_node, "Credential");
    if (p_Credential)
    {
        parse_Credential(p_Credential, &p_req->Credential);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_DeleteCredential(XMLN * p_node, tcr_DeleteCredential_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetCredentialState(XMLN * p_node, tcr_GetCredentialState_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_EnableCredential(XMLN * p_node, tcr_EnableCredential_REQ * p_req)
{
    XMLN * p_Token;
    XMLN * p_Reason;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    p_Reason = xml_node_soap_get(p_node, "Reason");
    if (p_Reason && p_Reason->data)
    {
        p_req->ReasonFlag = 1;
        strncpy(p_req->Reason, p_Reason->data, sizeof(p_req->Reason)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_DisableCredential(XMLN * p_node, tcr_DisableCredential_REQ * p_req)
{
    XMLN * p_Token;
    XMLN * p_Reason;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    p_Reason = xml_node_soap_get(p_node, "Reason");
    if (p_Reason && p_Reason->data)
    {
        p_req->ReasonFlag = 1;
        strncpy(p_req->Reason, p_Reason->data, sizeof(p_req->Reason)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_SetCredential(XMLN * p_node, tcr_SetCredential_REQ * p_req)
{
    XMLN * p_CredentialData;

    p_CredentialData = xml_node_soap_get(p_node, "CredentialData");
    if (p_CredentialData)
    {
        XMLN * p_Credential;
        XMLN * p_CredentialState;

        p_Credential = xml_node_soap_get(p_CredentialData, "Credential");
        if (p_Credential)
        {
            parse_Credential(p_Credential, &p_req->CredentialData.Credential);
        }

        p_CredentialState = xml_node_soap_get(p_CredentialData, "CredentialState");
        if (p_CredentialState)
        {
            parse_CredentialState(p_CredentialState, &p_req->CredentialData.CredentialState);
        }
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_ResetAntipassbackViolation(XMLN * p_node, tcr_ResetAntipassbackViolation_REQ * p_req)
{
    XMLN * p_CredentialToken;

    p_CredentialToken = xml_node_soap_get(p_node, "CredentialToken");
    if (p_CredentialToken && p_CredentialToken->data)
    {
        strncpy(p_req->CredentialToken, p_CredentialToken->data, sizeof(p_req->CredentialToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetSupportedFormatTypes(XMLN * p_node, tcr_GetSupportedFormatTypes_REQ * p_req)
{
    XMLN * p_CredentialIdentifierTypeName;

    p_CredentialIdentifierTypeName = xml_node_soap_get(p_node, "CredentialIdentifierTypeName");
    if (p_CredentialIdentifierTypeName && p_CredentialIdentifierTypeName->data)
    {
        strncpy(p_req->CredentialIdentifierTypeName, p_CredentialIdentifierTypeName->data, sizeof(p_req->CredentialIdentifierTypeName)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetCredentialIdentifiers(XMLN * p_node, tcr_GetCredentialIdentifiers_REQ * p_req)
{
    XMLN * p_CredentialToken;

    p_CredentialToken = xml_node_soap_get(p_node, "CredentialToken");
    if (p_CredentialToken && p_CredentialToken->data)
    {
        strncpy(p_req->CredentialToken, p_CredentialToken->data, sizeof(p_req->CredentialToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_SetCredentialIdentifier(XMLN * p_node, tcr_SetCredentialIdentifier_REQ * p_req)
{
    XMLN * p_CredentialToken;
    XMLN * p_CredentialIdentifier;

    p_CredentialToken = xml_node_soap_get(p_node, "CredentialToken");
    if (p_CredentialToken && p_CredentialToken->data)
    {
        strncpy(p_req->CredentialToken, p_CredentialToken->data, sizeof(p_req->CredentialToken)-1);
    }

    p_CredentialIdentifier = xml_node_soap_get(p_node, "CredentialIdentifier");
    if (p_CredentialIdentifier)
    {
        parse_CredentialIdentifier(p_CredentialIdentifier, &p_req->CredentialIdentifier);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_tcr_DeleteCredentialIdentifier(XMLN * p_node, tcr_DeleteCredentialIdentifier_REQ * p_req)
{
    XMLN * p_CredentialToken;
    XMLN * p_CredentialIdentifierTypeName;

    p_CredentialToken = xml_node_soap_get(p_node, "CredentialToken");
    if (p_CredentialToken && p_CredentialToken->data)
    {
        strncpy(p_req->CredentialToken, p_CredentialToken->data, sizeof(p_req->CredentialToken)-1);
    }

    p_CredentialIdentifierTypeName = xml_node_soap_get(p_node, "CredentialIdentifierTypeName");
    if (p_CredentialIdentifierTypeName && p_CredentialIdentifierTypeName->data)
    {
        strncpy(p_req->CredentialIdentifierTypeName, p_CredentialIdentifierTypeName->data, sizeof(p_req->CredentialIdentifierTypeName)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetCredentialAccessProfiles(XMLN * p_node, tcr_GetCredentialAccessProfiles_REQ * p_req)
{
    XMLN * p_CredentialToken;

    p_CredentialToken = xml_node_soap_get(p_node, "CredentialToken");
    if (p_CredentialToken && p_CredentialToken->data)
    {
        strncpy(p_req->CredentialToken, p_CredentialToken->data, sizeof(p_req->CredentialToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_SetCredentialAccessProfiles(XMLN * p_node, tcr_SetCredentialAccessProfiles_REQ * p_req)
{
    XMLN * p_CredentialToken;
    XMLN * p_CredentialAccessProfile;

    p_CredentialToken = xml_node_soap_get(p_node, "CredentialToken");
    if (p_CredentialToken && p_CredentialToken->data)
    {
        strncpy(p_req->CredentialToken, p_CredentialToken->data, sizeof(p_req->CredentialToken)-1);
    }

    p_CredentialAccessProfile = xml_node_soap_get(p_node, "CredentialAccessProfile");
    while (p_CredentialAccessProfile && soap_strcmp(p_CredentialAccessProfile->name, "CredentialAccessProfile") == 0)
    {
        int idx = p_req->sizeCredentialAccessProfile;

        parse_CredentialAccessProfile(p_CredentialAccessProfile, &p_req->CredentialAccessProfile[idx]);

        p_req->sizeCredentialAccessProfile++;
        if (p_req->sizeCredentialAccessProfile >= ARRAY_SIZE(p_req->CredentialAccessProfile))
        {
            break;
        }

        p_CredentialAccessProfile = p_CredentialAccessProfile->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_DeleteCredentialAccessProfiles(XMLN * p_node, tcr_DeleteCredentialAccessProfiles_REQ * p_req)
{
    XMLN * p_CredentialToken;
    XMLN * p_AccessProfileToken;

    p_CredentialToken = xml_node_soap_get(p_node, "CredentialToken");
    if (p_CredentialToken && p_CredentialToken->data)
    {
        strncpy(p_req->CredentialToken, p_CredentialToken->data, sizeof(p_req->CredentialToken)-1);
    }

    p_AccessProfileToken = xml_node_soap_get(p_node, "AccessProfileToken");
    while (p_AccessProfileToken && p_AccessProfileToken->data && soap_strcmp(p_AccessProfileToken->name, "AccessProfileToken") == 0)
    {
        int idx = p_req->sizeAccessProfileToken;

        strncpy(p_req->AccessProfileToken[idx], p_AccessProfileToken->data, sizeof(p_req->AccessProfileToken[idx])-1);

        p_req->sizeAccessProfileToken++;
        if (p_req->sizeAccessProfileToken >= ARRAY_SIZE(p_req->AccessProfileToken))
        {
            break;
        }

        p_AccessProfileToken = p_AccessProfileToken->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_GetWhitelist(XMLN * p_node, tcr_GetWhitelist_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;
    XMLN * p_IdentifierType;
    XMLN * p_FormatType;
    XMLN * p_Value;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    p_IdentifierType = xml_node_soap_get(p_node, "IdentifierType");
    if (p_IdentifierType && p_IdentifierType->data)
    {
        p_req->IdentifierTypeFlag = 1;
        strncpy(p_req->IdentifierType, p_IdentifierType->data, sizeof(p_req->IdentifierType)-1);
    }

    p_FormatType = xml_node_soap_get(p_node, "FormatType");
    if (p_FormatType && p_FormatType->data)
    {
        p_req->FormatTypeFlag = 1;
        strncpy(p_req->FormatType, p_FormatType->data, sizeof(p_req->FormatType)-1);
    }

    p_Value = xml_node_soap_get(p_node, "Value");
    if (p_Value && p_Value->data)
    {
        p_req->ValueFlag = 1;
        strncpy(p_req->Value, p_Value->data, sizeof(p_req->Value)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_AddToWhitelist(XMLN * p_node, tcr_AddToWhitelist_REQ * p_req)
{
    return parse_CredentialIdentifierItemList(p_node, &p_req->Identifier);
}

ONVIF_RET parse_tcr_RemoveFromWhitelist(XMLN * p_node, tcr_RemoveFromWhitelist_REQ * p_req)
{
    return parse_CredentialIdentifierItemList(p_node, &p_req->Identifier);
}

ONVIF_RET parse_tcr_GetBlacklist(XMLN * p_node, tcr_GetBlacklist_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;
    XMLN * p_IdentifierType;
    XMLN * p_FormatType;
    XMLN * p_Value;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    p_IdentifierType = xml_node_soap_get(p_node, "IdentifierType");
    if (p_IdentifierType && p_IdentifierType->data)
    {
        p_req->IdentifierTypeFlag = 1;
        strncpy(p_req->IdentifierType, p_IdentifierType->data, sizeof(p_req->IdentifierType)-1);
    }

    p_FormatType = xml_node_soap_get(p_node, "FormatType");
    if (p_FormatType && p_FormatType->data)
    {
        p_req->FormatTypeFlag = 1;
        strncpy(p_req->FormatType, p_FormatType->data, sizeof(p_req->FormatType)-1);
    }

    p_Value = xml_node_soap_get(p_node, "Value");
    if (p_Value && p_Value->data)
    {
        p_req->ValueFlag = 1;
        strncpy(p_req->Value, p_Value->data, sizeof(p_req->Value)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tcr_AddToBlacklist(XMLN * p_node, tcr_AddToBlacklist_REQ * p_req)
{
    return parse_CredentialIdentifierItemList(p_node, &p_req->Identifier);
}

ONVIF_RET parse_tcr_RemoveFromBlacklist(XMLN * p_node, tcr_RemoveFromBlacklist_REQ * p_req)
{
    return parse_CredentialIdentifierItemList(p_node, &p_req->Identifier);
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

ONVIF_RET parse_AccessPolicy(XMLN * p_node, onvif_AccessPolicy * p_req)
{
    XMLN * p_ScheduleToken;
    XMLN * p_Entity;    
    XMLN * p_EntityType;

    p_ScheduleToken = xml_node_soap_get(p_node, "ScheduleToken");
    if (p_ScheduleToken && p_ScheduleToken->data)
    {
        strncpy(p_req->ScheduleToken, p_ScheduleToken->data, sizeof(p_req->ScheduleToken)-1);
    }

    p_Entity = xml_node_soap_get(p_node, "Entity");
    if (p_Entity && p_Entity->data)
    {
        strncpy(p_req->Entity, p_Entity->data, sizeof(p_req->Entity)-1);
    }

    p_EntityType = xml_node_soap_get(p_node, "EntityType");
    if (p_EntityType && p_EntityType->data)
    {
        p_req->EntityTypeFlag = 1;
        strncpy(p_req->EntityType, p_EntityType->data, sizeof(p_req->EntityType)-1);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_AccessProfile(XMLN * p_node, onvif_AccessProfile * p_req)
{
    XMLN * p_Name;
    XMLN * p_Description;
    XMLN * p_AccessPolicy;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }
        
    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_Description = xml_node_soap_get(p_node, "Description");
    if (p_Description && p_Description->data)
    {
        p_req->DescriptionFlag = 1;
        strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description)-1);
    }

    p_AccessPolicy = xml_node_soap_get(p_node, "AccessPolicy");
    while (p_AccessPolicy && soap_strcmp(p_AccessPolicy->name, "AccessPolicy") == 0)
    {
        int idx = p_req->sizeAccessPolicy;

        parse_AccessPolicy(p_AccessPolicy, &p_req->AccessPolicy[idx]);

        p_req->sizeAccessPolicy++;
        if (p_req->sizeAccessPolicy >= ARRAY_SIZE(p_req->AccessPolicy))
        {
            break;
        }

        p_AccessPolicy = p_AccessPolicy->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_GetAccessProfileInfo(XMLN * p_node, tar_GetAccessProfileInfo_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_GetAccessProfileInfoList(XMLN * p_node, tar_GetAccessProfileInfoList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_GetAccessProfiles(XMLN * p_node, tar_GetAccessProfiles_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_GetAccessProfileList(XMLN * p_node, tar_GetAccessProfileList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_CreateAccessProfile(XMLN * p_node, tar_CreateAccessProfile_REQ * p_req)
{
    XMLN * p_AccessProfile;

    p_AccessProfile = xml_node_soap_get(p_node, "AccessProfile");
    if (p_AccessProfile)
    {
        parse_AccessProfile(p_AccessProfile, &p_req->AccessProfile);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_ModifyAccessProfile(XMLN * p_node, tar_ModifyAccessProfile_REQ * p_req)
{
    XMLN * p_AccessProfile;

    p_AccessProfile = xml_node_soap_get(p_node, "AccessProfile");
    if (p_AccessProfile)
    {
        parse_AccessProfile(p_AccessProfile, &p_req->AccessProfile);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_DeleteAccessProfile(XMLN * p_node, tar_DeleteAccessProfile_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tar_SetAccessProfile(XMLN * p_node, tar_SetAccessProfile_REQ * p_req)
{
    XMLN * p_AccessProfile;

    p_AccessProfile = xml_node_soap_get(p_node, "AccessProfile");
    if (p_AccessProfile)
    {
        parse_AccessProfile(p_AccessProfile, &p_req->AccessProfile);
    }

    return ONVIF_OK;
}

#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

ONVIF_RET parse_TimePeriod(XMLN * p_node, onvif_TimePeriod * p_req)
{
    XMLN * p_From;
    XMLN * p_Until;

    p_From = xml_node_soap_get(p_node, "From");
    if (p_From && p_From->data)
    {
        strncpy(p_req->From, p_From->data, sizeof(p_req->From)-1);
    }

    p_Until = xml_node_soap_get(p_node, "Until");
    if (p_Until && p_Until->data)
    {
        p_req->UntilFlag = 1;
        strncpy(p_req->Until, p_Until->data, sizeof(p_req->Until)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_SpecialDaysSchedule(XMLN * p_node, onvif_SpecialDaysSchedule * p_req)
{
    XMLN * p_GroupToken;
    XMLN * p_TimeRange;

    p_GroupToken = xml_node_soap_get(p_node, "GroupToken");
    if (p_GroupToken && p_GroupToken->data)
    {
        strncpy(p_req->GroupToken, p_GroupToken->data, sizeof(p_req->GroupToken)-1);
    }

    p_TimeRange = xml_node_soap_get(p_node, "TimeRange");
    while (p_TimeRange && soap_strcmp(p_TimeRange->name, "TimeRange") == 0)
    {
        int idx = p_req->sizeTimeRange;

        parse_TimePeriod(p_TimeRange, &p_req->TimeRange[idx]);

        p_req->sizeTimeRange++;
        if (p_req->sizeTimeRange >= ARRAY_SIZE(p_req->TimeRange))
        {
            break;
        }
        
        p_TimeRange = p_TimeRange->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_Schedule(XMLN * p_node, onvif_Schedule * p_req)
{
    XMLN * p_Name;
    XMLN * p_Description;
    XMLN * p_Standard;
    XMLN * p_SpecialDays;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }

    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_Description = xml_node_soap_get(p_node, "Description");
    if (p_Description && p_Description->data)
    {
        p_req->DescriptionFlag = 1;
        strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description)-1);
    }

    p_Standard = xml_node_soap_get(p_node, "Standard");
    if (p_Standard && p_Standard->data)
    {
        strncpy(p_req->Standard, p_Standard->data, sizeof(p_req->Standard)-1);
    }

    p_SpecialDays = xml_node_soap_get(p_node, "SpecialDays");
    while (p_SpecialDays && soap_strcmp(p_SpecialDays->name, "SpecialDays") == 0)
    {
        int idx = p_req->sizeSpecialDays;

        parse_SpecialDaysSchedule(p_SpecialDays, &p_req->SpecialDays[idx]);

        p_req->sizeSpecialDays++;
        if (p_req->sizeSpecialDays >= ARRAY_SIZE(p_req->SpecialDays))
        {
            break;
        }
        
        p_SpecialDays = p_SpecialDays->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_SpecialDayGroup(XMLN * p_node, onvif_SpecialDayGroup * p_req)
{
    XMLN * p_Name;
    XMLN * p_Description;
    XMLN * p_Days;
    const char * p_token;

    p_token = xml_attr_get(p_node, "token");
    if (p_token)
    {
        strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
    }
        
    p_Name = xml_node_soap_get(p_node, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_req->Name, p_Name->data, sizeof(p_req->Name)-1);
    }

    p_Description = xml_node_soap_get(p_node, "Description");
    if (p_Description && p_Description->data)
    {
        p_req->DescriptionFlag = 1;
        strncpy(p_req->Description, p_Description->data, sizeof(p_req->Description)-1);
    }

    p_Days = xml_node_soap_get(p_node, "Days");
    if (p_Days && p_Days->data)
    {
        p_req->DaysFlag = 1;
        strncpy(p_req->Days, p_Days->data, sizeof(p_req->Days)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetScheduleInfo(XMLN * p_node, tsc_GetScheduleInfo_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetScheduleInfoList(XMLN * p_node, tsc_GetScheduleInfoList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetSchedules(XMLN * p_node, tsc_GetSchedules_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetScheduleList(XMLN * p_node, tsc_GetScheduleList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_CreateSchedule(XMLN * p_node, tsc_CreateSchedule_REQ * p_req)
{
    XMLN * p_Schedule;

    p_Schedule = xml_node_soap_get(p_node, "Schedule");
    if (p_Schedule)
    {
        parse_Schedule(p_Schedule, &p_req->Schedule);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_ModifySchedule(XMLN * p_node, tsc_ModifySchedule_REQ * p_req)
{
    XMLN * p_Schedule;

    p_Schedule = xml_node_soap_get(p_node, "Schedule");
    if (p_Schedule)
    {
        parse_Schedule(p_Schedule, &p_req->Schedule);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_DeleteSchedule(XMLN * p_node, tsc_DeleteSchedule_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetSpecialDayGroupInfo(XMLN * p_node, tsc_GetSpecialDayGroupInfo_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetSpecialDayGroupInfoList(XMLN * p_node, tsc_GetSpecialDayGroupInfoList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetSpecialDayGroups(XMLN * p_node, tsc_GetSpecialDayGroups_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    while (p_Token && p_Token->data && soap_strcmp(p_Token->name, "Token") == 0)
    {
        int idx = p_req->sizeToken;

        strncpy(p_req->Token[idx], p_Token->data, sizeof(p_req->Token[idx])-1);

        p_req->sizeToken++;
        if (p_req->sizeToken >= ARRAY_SIZE(p_req->Token))
        {
            break;
        }

        p_Token = p_Token->next;
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetSpecialDayGroupList(XMLN * p_node, tsc_GetSpecialDayGroupList_REQ * p_req)
{
    XMLN * p_Limit;
    XMLN * p_StartReference;

    p_Limit = xml_node_soap_get(p_node, "Limit");
    if (p_Limit && p_Limit->data)
    {
        p_req->LimitFlag = 1;
        p_req->Limit = atoi(p_Limit->data);
    }

    p_StartReference = xml_node_soap_get(p_node, "StartReference");
    if (p_StartReference && p_StartReference->data)
    {
        p_req->StartReferenceFlag = 1;
        strncpy(p_req->StartReference, p_StartReference->data, sizeof(p_req->StartReference)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_CreateSpecialDayGroup(XMLN * p_node, tsc_CreateSpecialDayGroup_REQ * p_req)
{
    XMLN * p_SpecialDayGroup;

    p_SpecialDayGroup = xml_node_soap_get(p_node, "SpecialDayGroup");
    if (p_SpecialDayGroup)
    {
        parse_SpecialDayGroup(p_SpecialDayGroup, &p_req->SpecialDayGroup);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_ModifySpecialDayGroup(XMLN * p_node, tsc_ModifySpecialDayGroup_REQ * p_req)
{
    XMLN * p_SpecialDayGroup;

    p_SpecialDayGroup = xml_node_soap_get(p_node, "SpecialDayGroup");
    if (p_SpecialDayGroup)
    {
        parse_SpecialDayGroup(p_SpecialDayGroup, &p_req->SpecialDayGroup);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_DeleteSpecialDayGroup(XMLN * p_node, tsc_DeleteSpecialDayGroup_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_GetScheduleState(XMLN * p_node, tsc_GetScheduleState_REQ * p_req)
{
    XMLN * p_Token;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_SetSchedule(XMLN * p_node, tsc_SetSchedule_REQ * p_req)
{
    XMLN * p_Schedule;

    p_Schedule = xml_node_soap_get(p_node, "Schedule");
    if (p_Schedule)
    {
        parse_Schedule(p_Schedule, &p_req->Schedule);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tsc_SetSpecialDayGroup(XMLN * p_node, tsc_SetSpecialDayGroup_REQ * p_req)
{
    XMLN * p_SpecialDayGroup;

    p_SpecialDayGroup = xml_node_soap_get(p_node, "SpecialDayGroup");
    if (p_SpecialDayGroup)
    {
        parse_SpecialDayGroup(p_SpecialDayGroup, &p_req->SpecialDayGroup);
    }

    return ONVIF_OK;
}

#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

ONVIF_RET parse_ReceiverConfiguration(XMLN * p_node, onvif_ReceiverConfiguration * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_Mode;
    XMLN * p_MediaUri;
    XMLN * p_StreamSetup;

    p_Mode = xml_node_soap_get(p_node, "Mode");
    if (p_Mode && p_Mode->data)
    {
        p_req->Mode = onvif_StringToReceiverMode(p_Mode->data);
    }

    p_MediaUri = xml_node_soap_get(p_node, "MediaUri");
    if (p_MediaUri && p_MediaUri->data)
    {
        strncpy(p_req->MediaUri, p_MediaUri->data, sizeof(p_req->MediaUri)-1);
    }

    p_StreamSetup = xml_node_soap_get(p_node, "StreamSetup");
    if (p_StreamSetup)
    {
        ret = parse_StreamSetup(p_StreamSetup, &p_req->StreamSetup);
    }

    return ret;
}

ONVIF_RET parse_Receiver(XMLN * p_node, onvif_Receiver * p_req)
{
    XMLN * p_Token;
    XMLN * p_Configuration;

    p_Token = xml_node_soap_get(p_node, "Token");
    if (p_Token && p_Token->data)
    {
        strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
    }

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        parse_ReceiverConfiguration(p_Configuration, &p_req->Configuration);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_trv_GetReceiver(XMLN * p_node, trv_GetReceiver_REQ * p_req)
{
    XMLN * p_ReceiverToken;

    p_ReceiverToken = xml_node_soap_get(p_node, "ReceiverToken");
    if (p_ReceiverToken && p_ReceiverToken->data)
    {
        strncpy(p_req->ReceiverToken, p_ReceiverToken->data, sizeof(p_req->ReceiverToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_trv_CreateReceiver(XMLN * p_node, trv_CreateReceiver_REQ * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_Configuration;

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        ret = parse_ReceiverConfiguration(p_Configuration, &p_req->Configuration);
    }

    return ret;
}

ONVIF_RET parse_trv_DeleteReceiver(XMLN * p_node, trv_DeleteReceiver_REQ * p_req)
{
    XMLN * p_ReceiverToken;

    p_ReceiverToken = xml_node_soap_get(p_node, "ReceiverToken");
    if (p_ReceiverToken && p_ReceiverToken->data)
    {
        strncpy(p_req->ReceiverToken, p_ReceiverToken->data, sizeof(p_req->ReceiverToken)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_trv_ConfigureReceiver(XMLN * p_node, trv_ConfigureReceiver_REQ * p_req)
{
    ONVIF_RET ret = ONVIF_OK;
    XMLN * p_ReceiverToken;
    XMLN * p_Configuration;

    p_ReceiverToken = xml_node_soap_get(p_node, "ReceiverToken");
    if (p_ReceiverToken && p_ReceiverToken->data)
    {
        strncpy(p_req->ReceiverToken, p_ReceiverToken->data, sizeof(p_req->ReceiverToken)-1);
    }

    p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (p_Configuration)
    {
        ret = parse_ReceiverConfiguration(p_Configuration, &p_req->Configuration);
    }
    
    return ret;
}

ONVIF_RET parse_trv_SetReceiverMode(XMLN * p_node, trv_SetReceiverMode_REQ * p_req)
{
    XMLN * p_ReceiverToken;
    XMLN * p_Mode;

    p_ReceiverToken = xml_node_soap_get(p_node, "ReceiverToken");
    if (p_ReceiverToken && p_ReceiverToken->data)
    {
        strncpy(p_req->ReceiverToken, p_ReceiverToken->data, sizeof(p_req->ReceiverToken)-1);
    }

    p_Mode = xml_node_soap_get(p_node, "Mode");
    if (p_Mode && p_Mode->data)
    {
        p_req->Mode = onvif_StringToReceiverMode(p_Mode->data);
    }
    
    return ONVIF_OK;
}

ONVIF_RET parse_trv_GetReceiverState(XMLN * p_node, trv_GetReceiverState_REQ * p_req)
{
    XMLN * p_ReceiverToken;

    p_ReceiverToken = xml_node_soap_get(p_node, "ReceiverToken");
    if (p_ReceiverToken && p_ReceiverToken->data)
    {
        strncpy(p_req->ReceiverToken, p_ReceiverToken->data, sizeof(p_req->ReceiverToken)-1);
    }

    return ONVIF_OK;
}

#endif // end of RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT

ONVIF_RET parse_tpv_PanMove(XMLN * p_node, tpv_PanMove_REQ * p_req)
{
    XMLN * p_VideoSource;
    XMLN * p_Direction;
    XMLN * p_Timeout;

    p_VideoSource = xml_node_soap_get(p_node, "VideoSource");
    if (p_VideoSource && p_VideoSource->data)
    {
        strncpy(p_req->VideoSource, p_VideoSource->data, sizeof(p_req->VideoSource)-1);
    }

    p_Direction = xml_node_soap_get(p_node, "Direction");
    if (p_Direction && p_Direction->data)
    {
        p_req->Direction = onvif_StringToPanDirection(p_Direction->data);
    }

    p_Timeout = xml_node_soap_get(p_node, "Timeout");
    if (p_Timeout && p_Timeout->data)
    {
        p_req->TimeoutFlag = parse_XSDDuration(p_Timeout->data, &p_req->Timeout);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tpv_TiltMove(XMLN * p_node, tpv_TiltMove_REQ * p_req)
{
    XMLN * p_VideoSource;
    XMLN * p_Direction;
    XMLN * p_Timeout;

    p_VideoSource = xml_node_soap_get(p_node, "VideoSource");
    if (p_VideoSource && p_VideoSource->data)
    {
        strncpy(p_req->VideoSource, p_VideoSource->data, sizeof(p_req->VideoSource)-1);
    }

    p_Direction = xml_node_soap_get(p_node, "Direction");
    if (p_Direction && p_Direction->data)
    {
        p_req->Direction = onvif_StringToTiltDirection(p_Direction->data);
    }

    p_Timeout = xml_node_soap_get(p_node, "Timeout");
    if (p_Timeout && p_Timeout->data)
    {
        p_req->TimeoutFlag = parse_XSDDuration(p_Timeout->data, &p_req->Timeout);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tpv_ZoomMove(XMLN * p_node, tpv_ZoomMove_REQ * p_req)
{
    XMLN * p_VideoSource;
    XMLN * p_Direction;
    XMLN * p_Timeout;

    p_VideoSource = xml_node_soap_get(p_node, "VideoSource");
    if (p_VideoSource && p_VideoSource->data)
    {
        strncpy(p_req->VideoSource, p_VideoSource->data, sizeof(p_req->VideoSource)-1);
    }

    p_Direction = xml_node_soap_get(p_node, "Direction");
    if (p_Direction && p_Direction->data)
    {
        p_req->Direction = onvif_StringToZoomDirection(p_Direction->data);
    }

    p_Timeout = xml_node_soap_get(p_node, "Timeout");
    if (p_Timeout && p_Timeout->data)
    {
        p_req->TimeoutFlag = parse_XSDDuration(p_Timeout->data, &p_req->Timeout);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tpv_RollMove(XMLN * p_node, tpv_RollMove_REQ * p_req)
{
    XMLN * p_VideoSource;
    XMLN * p_Direction;
    XMLN * p_Timeout;

    p_VideoSource = xml_node_soap_get(p_node, "VideoSource");
    if (p_VideoSource && p_VideoSource->data)
    {
        strncpy(p_req->VideoSource, p_VideoSource->data, sizeof(p_req->VideoSource)-1);
    }

    p_Direction = xml_node_soap_get(p_node, "Direction");
    if (p_Direction && p_Direction->data)
    {
        p_req->Direction = onvif_StringToRollDirection(p_Direction->data);
    }

    p_Timeout = xml_node_soap_get(p_node, "Timeout");
    if (p_Timeout && p_Timeout->data)
    {
        p_req->TimeoutFlag = parse_XSDDuration(p_Timeout->data, &p_req->Timeout);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tpv_FocusMove(XMLN * p_node, tpv_FocusMove_REQ * p_req)
{
    XMLN * p_VideoSource;
    XMLN * p_Direction;
    XMLN * p_Timeout;

    p_VideoSource = xml_node_soap_get(p_node, "VideoSource");
    if (p_VideoSource && p_VideoSource->data)
    {
        strncpy(p_req->VideoSource, p_VideoSource->data, sizeof(p_req->VideoSource)-1);
    }

    p_Direction = xml_node_soap_get(p_node, "Direction");
    if (p_Direction && p_Direction->data)
    {
        p_req->Direction = onvif_StringToFocusDirection(p_Direction->data);
    }

    p_Timeout = xml_node_soap_get(p_node, "Timeout");
    if (p_Timeout && p_Timeout->data)
    {
        p_req->TimeoutFlag = parse_XSDDuration(p_Timeout->data, &p_req->Timeout);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tpv_Stop(XMLN * p_node, tpv_Stop_REQ * p_req)
{
    XMLN * p_VideoSource;

    p_VideoSource = xml_node_soap_get(p_node, "VideoSource");
    if (p_VideoSource && p_VideoSource->data)
    {
        strncpy(p_req->VideoSource, p_VideoSource->data, sizeof(p_req->VideoSource)-1);
    }

    return ONVIF_OK;
}

ONVIF_RET parse_tpv_GetUsage(XMLN * p_node, tpv_GetUsage_REQ * p_req)
{
    XMLN * p_VideoSource;

    p_VideoSource = xml_node_soap_get(p_node, "VideoSource");
    if (p_VideoSource && p_VideoSource->data)
    {
        strncpy(p_req->VideoSource, p_VideoSource->data, sizeof(p_req->VideoSource)-1);
    }

    return ONVIF_OK;
}

#endif // end of PROVISIONING_SUPPORT

ONVIF_RET parse_PrivacyMask(XMLN * p_node, onvif_Mask * p_req)
{
	XMLN * p_ConfigurationToken;
	XMLN * p_Polygon;
	XMLN * p_Type;
	XMLN * p_Color;
	XMLN * p_Enabled;
	const char * p_token;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "VideoSourceConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	p_Polygon = xml_node_soap_get(p_node, "MaskArea");
	if (p_Polygon)
	{
		parse_Polygon(p_Polygon, &p_req->Polygon);
	}

	p_Type = xml_node_soap_get(p_node, "Type");
	if (p_Type && p_Type->data)
	{
		strncpy(p_req->Type, p_Type->data, sizeof(p_req->Type)-1);
	}

	p_Color = xml_node_soap_get(p_node, "Color");
	if (p_Color)
	{
		p_req->ColorFlag = parse_Color(p_Color, &p_req->Color);
	}

	p_Enabled = xml_node_soap_get(p_node, "Enabled");
	if (p_Enabled && p_Enabled->data)
	{
		p_req->Enabled = parse_Bool(p_Enabled->data);
	}

	return ONVIF_OK;
}

#if 1

ONVIF_RET parse_hbgk_ext_PrivacyMask(XMLN * p_node, ewsd_onvif_Mask * p_req)
{
	const char * p_token;
	XMLN * p_VideoSourceToken;
	XMLN * MaskArea;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceToken)-1);
	}

	MaskArea = xml_node_soap_get(p_node, "MaskArea");
	if (MaskArea)
	{
		parse_Polygon(MaskArea, &p_req->Polygon);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_hbgk_ext_CreatePrivacyMask(XMLN * p_node, ewsd_CreateMask_REQ * p_req)
{
	XMLN * p_PrivacyMaskEnable;
	XMLN * p_PrivacyMask;

	
	p_PrivacyMaskEnable = xml_node_soap_get(p_node, "PrivacyMaskEnable");
	if (p_PrivacyMaskEnable && p_PrivacyMaskEnable->data)
	{
		p_req->PrivacyMaskEnable = parse_Bool(p_PrivacyMaskEnable->data);
	}
	
	p_PrivacyMask = xml_node_soap_get(p_node, "PrivacyMask");
	if (p_PrivacyMask)
	{
		parse_hbgk_ext_PrivacyMask(p_PrivacyMask, &p_req->PrivacyMask);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_hbgk_ext_DeletePrivacyMask(XMLN * p_node, ewsd_DeleteMask_REQ * p_req)
{
	XMLN * p_Token;

	p_Token = xml_node_soap_get(p_node, "PrivacyMaskToken");
	if (p_Token && p_Token->data)
	{
		strncpy(p_req->PrivacyMaskToken, p_Token->data, sizeof(p_req->PrivacyMaskToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_hbgk_ext_GetPrivacyMasks(XMLN * p_node, ewsd_GetMasks_REQ * p_req)
{
	XMLN * p_Token;

	p_Token = xml_node_soap_get(p_node, "VideoSourceToken");
	if (p_Token && p_Token->data)
	{
		p_req->VideoSourceTokenFlag = 1;
		strncpy(p_req->VideoSourceToken, p_Token->data, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_hbgk_ext_SetPrivacyMask(XMLN * p_node, ewsd_SetMask_REQ * p_req)
{
	XMLN * p_PrivacyMaskEnable;
	XMLN * p_PrivacyMask;
	
	p_PrivacyMaskEnable = xml_node_soap_get(p_node, "PrivacyMaskEnable");
	if (p_PrivacyMaskEnable && p_PrivacyMaskEnable->data)
	{
		p_req->PrivacyMaskEnable = parse_Bool(p_PrivacyMaskEnable->data);
	}
	
	p_PrivacyMask = xml_node_soap_get(p_node, "PrivacyMask");
	if (p_PrivacyMask)
	{
		parse_hbgk_ext_PrivacyMask(p_PrivacyMask, &p_req->PrivacyMask);
	}
	
	return ONVIF_OK;
}

#endif

#if 1

ONVIF_RET parse_tpl_PrivacyMask(XMLN * p_node, tpl_onvif_Mask * p_req)
{
	const char * p_token;
	XMLN * p_VideoSourceToken;
	XMLN * MaskArea;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}

	p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceConfigurationToken");
	if (p_VideoSourceToken && p_VideoSourceToken->data)
	{
		strncpy(p_req->VideoSourceConfigurationToken, p_VideoSourceToken->data, sizeof(p_req->VideoSourceConfigurationToken)-1);
	}

	MaskArea = xml_node_soap_get(p_node, "MaskArea");
	if (MaskArea)
	{
		parse_Polygon(MaskArea, &p_req->Polygon);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tpl_CreatePrivacyMask(XMLN * p_node, tpl_CreateMask_REQ * p_req)
{
	XMLN * p_PrivacyMaskEnable;
	XMLN * p_PrivacyMask;

	
	p_PrivacyMaskEnable = xml_node_soap_get(p_node, "PrivacyMaskEnable");
	if (p_PrivacyMaskEnable && p_PrivacyMaskEnable->data)
	{
		p_req->PrivacyMaskEnable = parse_Bool(p_PrivacyMaskEnable->data);
	}
	
	p_PrivacyMask = xml_node_soap_get(p_node, "PrivacyMask");
	if (p_PrivacyMask)
	{
		parse_tpl_PrivacyMask(p_PrivacyMask, &p_req->PrivacyMask);
	}

	return ONVIF_OK;
}


ONVIF_RET parse_tpl_DeletePrivacyMask(XMLN * p_node, tpl_DeleteMask_REQ * p_req)
{
	XMLN * p_Token;

	p_Token = xml_node_soap_get(p_node, "PrivacyMaskToken");
	if (p_Token && p_Token->data)
	{
		strncpy(p_req->PrivacyMaskToken, p_Token->data, sizeof(p_req->PrivacyMaskToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tpl_GetPrivacyMasks(XMLN * p_node, tpl_GetMasks_REQ * p_req)
{
	XMLN * p_Token;
	XMLN * p_ConfigurationToken;

	p_Token = xml_node_soap_get(p_node, "Token");
	if (p_Token && p_Token->data)
	{
		p_req->TokenFlag = 1;
		strncpy(p_req->Token, p_Token->data, sizeof(p_req->Token)-1);
	}

	p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		p_req->ConfigurationTokenFlag = 1;
		strncpy(p_req->ConfigurationToken, p_ConfigurationToken->data, sizeof(p_req->ConfigurationToken)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_tpl_SetPrivacyMask(XMLN * p_node, tpl_SetMask_REQ * p_req)
{
	XMLN * p_PrivacyMaskEnable;
	XMLN * p_PrivacyMask;
	
	p_PrivacyMaskEnable = xml_node_soap_get(p_node, "PrivacyMaskEnable");
	if (p_PrivacyMaskEnable && p_PrivacyMaskEnable->data)
	{
		p_req->PrivacyMaskEnable = parse_Bool(p_PrivacyMaskEnable->data);
	}
	
	p_PrivacyMask = xml_node_soap_get(p_node, "PrivacyMask");
	if (p_PrivacyMask)
	{
		parse_tpl_PrivacyMask(p_PrivacyMask, &p_req->PrivacyMask);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tpl_Command(XMLN * p_node, tpl_onvif_Command * p_req)
{
	const char * p_ProfileToken;
	const char * p_Token;
	XMLN * p_CommandData;
	
	p_CommandData = xml_node_soap_get(p_node, "CommandData");
	if (p_CommandData && p_CommandData->data)
	{
		p_req->CommandDataFlag = 1;
		p_req->CommandData = parse_Bool(p_CommandData->data);
	}
	
	p_ProfileToken = xml_attr_get(p_node, "ProfileToken");
	if (p_ProfileToken)
	{
		p_req->ProfileTokenFlag = 1;
		strncpy(p_req->ProfileToken, p_ProfileToken, sizeof(p_req->ProfileToken)-1);
	}
	
	p_Token = xml_attr_get(p_node, "Token");
	if (p_Token)
	{
		p_req->TokenFlag = 1;
		strncpy(p_req->Token, p_Token, sizeof(p_req->Token)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tpl_SendCommand(XMLN * p_node, tpl_SendCommand_REQ * p_req)
{
	XMLN * p_SendCommand;
	
	p_SendCommand = xml_node_soap_get(p_node, "SendCommand");
	if (p_SendCommand)
	{
		parse_tpl_Command(p_SendCommand, &p_req->SendCommand);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_tpl_SetImagingSettingDefault(XMLN * p_node, tpl_SetImagingSettingDefault_REQ * p_req)
{
	const char * p_VideoSourceToken;
	
	p_VideoSourceToken = xml_attr_get(p_node, "VideoSourceToken");
	if (p_VideoSourceToken)
	{
		p_req->VideoSourceTokenFlag = 1;
		strncpy(p_req->VideoSourceToken, p_VideoSourceToken, sizeof(p_req->VideoSourceToken)-1);
	}

	return ONVIF_OK;
}

#endif

