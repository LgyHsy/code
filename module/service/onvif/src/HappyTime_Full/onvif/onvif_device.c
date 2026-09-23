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

#include "onvif_device.h"
#include "sys_inc.h"
#include "onvif.h"
#include "xml_node.h"
#include "onvif_utils.h"
#include "onvif_cfg.h"
#include "onvif_probe.h"
#include "onvif_event.h"
#include "onvif_timer.h"
#include "onvif_srv.h"
#include "anj_config_network.h"
#include "user_auth.h"
#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_net.h"
#include "anj_module.h"
#include "anj_search.h"
#include "anj_audio.h"
#include "anj_systime.h"
#include "anj_sys.h"
#include "anj_service_provider.h"
#include "record_log.h"

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;

extern char *group[];
void http_upload_form_status_set(int status);

/***************************************************************************************/

/**
 * @brief
 *  When the device clock has been synchronized the last time either via an NTP
 *  message or via a SetSystemDateAndTime call.
 *
 **/
void onvif_LastClockSynchronizationNotify()
{
	char str[100] = {'\0'};
	NotificationMessageList * p_message;

	onvif_format_datetime_str(time(NULL), 1, "%Y-%m-%dT%H:%M:%SZ", str, sizeof(str));

	p_message = onvif_init_NotificationMessage3(
		"tns1:Monitoring/OperatingTime/LastClockSynchronization", 
		PropertyOperation_Changed, NULL, NULL, NULL, NULL, 
		"Status", str, NULL, NULL);
	if (p_message)
	{
		onvif_put_NotificationMessage(p_message);
	}
}

#ifdef PROFILE_Q_SUPPORT

void onvif_switchDeviceState(int state)
{
    // 0 - Factory Default state, 1 - Operational State

    if (state != g_onvif_cfg.device_state)
    {
        g_onvif_cfg.device_state = state;
    }
    
    // When switching from Factory Default State to Operational State, 
    //  the device may reboot if necessary.
    
}

#endif

/**
 * @brief
 *  Gets a system log from a device.
 *
 *  The exact format of the system logs is outside the scope of this standard.
 *
 *  The system log information is transmitted through MTOM [MTOM] or as a string.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_AccesslogUnavailable
 *  ONVIF_ERR_SystemlogUnavailable
 **/
ONVIF_RET onvif_tds_GetSystemLog(tds_GetSystemLog_REQ * p_req, tds_GetSystemLog_RES * p_res)
{
	if (SystemLogType_Access == p_req->LogType)
	{
		strcpy(p_res->String, "test access log");
	}
	else
	{
		strcpy(p_res->String, "test system log");
	}

	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the device system date and time.
 *  The device shall support the configuration of the daylight saving setting 
 *  and of the manual system date and time (if applicable) or indication of 
 *  NTP time (if applicable) through the SetSystemDateAndTime command. A device 
 *  shall consider a Timezone which is not formed according to the rules of 
 *  [IEEE 1003.1] section 8.3 as invalid.
 *
 *  The DayLightSavings flag should be set to true to activate any DST settings 
 *  of the TimeZone string. Clear the DayLightSavings flag if the DST portion of 
 *  the TimeZone settings should be ignored.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidTimeZone
 *  ONVIF_ERR_InvalidDateTime
 *  ONVIF_ERR_NtpServerUndefined
 **/
ONVIF_RET onvif_tds_SetSystemDateAndTime(tds_SetSystemDateAndTime_REQ * p_req)
{
	// check datetime
	if (p_req->SystemDateTime.DateTimeType == SetDateTimeType_Manual)
	{
		if (p_req->UTCDateTime.Date.Month < 1 || p_req->UTCDateTime.Date.Month > 12 ||
			p_req->UTCDateTime.Date.Day < 1 || p_req->UTCDateTime.Date.Day > 31 ||
			p_req->UTCDateTime.Time.Hour < 0 || p_req->UTCDateTime.Time.Hour > 23 ||
			p_req->UTCDateTime.Time.Minute < 0 || p_req->UTCDateTime.Time.Minute > 59 ||
			p_req->UTCDateTime.Time.Second < 0 || p_req->UTCDateTime.Time.Second > 61)
		{
			return ONVIF_ERR_InvalidDateTime;
		}
	}

	// check timezone
	if (p_req->SystemDateTime.TimeZoneFlag && 
		p_req->SystemDateTime.TimeZone.TZ[0] != '\0' && 
		onvif_is_valid_timezone(p_req->SystemDateTime.TimeZone.TZ) == FALSE)
	{
		return ONVIF_ERR_InvalidTimeZone;
	}

	// todo : here add handler code ...
#if 1
	TimeConfig timeCfg;
	SystemConfig *pSystemCfg = (SystemConfig *)getSystemConfig();
	if (!pSystemCfg)
	{	
		log_print(HT_LOG_INFO, "getTimeConfig failed!\n");
		return ONVIF_ERR_InvalidTimeZone;
	}
	else
	{
		memcpy(&timeCfg, &pSystemCfg->timeCfg, sizeof(timeCfg));
		// Some product variants should not modify time in NTP mode.
		// If /opt/ch/yen_set_time is absent, keep existing behavior guard.
		log_print(HT_LOG_INFO, "%s", timeCfg.timeMode.modeName);
	
		if( (strcmp(timeCfg.timeMode.modeName, TIME_MODE_NAME_MANUAL) != 0) && (access("/opt/ch/yen_set_time", F_OK) != F_OK))//NTP
		{
			char file_ver[64] = {0};
			read_file_to_string("/etc/filesys.ver", file_ver, 64);

			if(strstr(file_ver, "_YCX") != NULL || strstr(file_ver, "_Y_EN") != NULL  || strstr(file_ver, "_YCX_EN") != NULL)
			{
				log_print(HT_LOG_INFO, "time mode %s, not set time of version %s!\n", timeCfg.timeMode.modeName, file_ver);
				return 0;
			}
			else
			{
				log_print(HT_LOG_INFO, "%s", file_ver);
			}
		}
		else//MANUAL
		{
			log_print(HT_LOG_INFO, "%s", "manual time mode.");
		}
	}
#endif
	int ret;
	int _Year;
	int _Month;
	int _Day;
	int _Hour;
	int _Minute;
	int _Second;
	int _DaylightSavings;
	char *_TZ;

	_DaylightSavings = p_req->SystemDateTime.DaylightSavings;
	
	g_onvif_cfg.SystemDateTime.DateTimeType = p_req->SystemDateTime.DateTimeType;
	g_onvif_cfg.SystemDateTime.DaylightSavings = p_req->SystemDateTime.DaylightSavings;

	if (p_req->SystemDateTime.DateTimeType == SetDateTimeType_NTP)
	{
		SummerTimeConfig dst = {0};
			if (p_req->SystemDateTime.TimeZoneFlag)
			{
				TimeConfig *pTimeConfig = (TimeConfig *)malloc(sizeof(TimeConfig));
				memset(pTimeConfig, 0, sizeof(TimeConfig));
				memcpy(pTimeConfig, &pSystemCfg->timeCfg, sizeof(*pTimeConfig));
			
			log_print(HT_LOG_INFO, "tz: %s\n", p_req->SystemDateTime.TimeZone.TZ);
			_TZ  = p_req->SystemDateTime.TimeZone.TZ;
			/* Parse timezone and optional DST rule from ONVIF request. */
			int OffsetMin = 0;
			int retStatus = parse_timezone_DaylightSavings(_TZ, &ret, &OffsetMin, &dst);
			if(retStatus >= 0)
			{
				log_print(HT_LOG_INFO, "timezone: %s, ret = %d\n", _TZ, ret);
				if(retStatus > 0)
				{
					// Request carries explicit DST settings.
					if(_DaylightSavings > 0)
						dst.nEnable = 1;
					else 
						dst.nEnable = 0;
					memcpy(&(pTimeConfig->summerConfig), &dst, sizeof(SummerTimeConfig));

					if(ret == -1)
						ret = oset_timezone(_TZ, &OffsetMin); // fallback timezone set
					log_print(HT_LOG_INFO, "timezone: %s, ret = %d offset:%d\n", _TZ, ret, OffsetMin);
				}
				else
				{
					if(_DaylightSavings>0)
						pTimeConfig->summerConfig.nEnable = 1;
					else 
						pTimeConfig->summerConfig.nEnable = 0;
					
					log_print(HT_LOG_INFO, "timezone: %s, ret = %d offset:%d\n", _TZ, ret, OffsetMin);
				}
			}
			else if(ret == -1)
			{
				ret = oset_timezone(_TZ,&OffsetMin);
				log_print(HT_LOG_INFO, "timezone: %s, ret = %d offset:%d\n", _TZ, ret, OffsetMin);		
			}

			if(ret == 100)
			{
				free(pTimeConfig);
				return ONVIF_ERR_InvalidTimeZone;
			}

			/* Sync timezone/time mode to mainctrl in NTP mode. */
			int tz_cfg = OffsetMin + 12 * 60;
			
			if((retStatus >= 0)||(strcmp(pTimeConfig->timeMode.modeName, "NTP"))|| (pTimeConfig->timeZone != tz_cfg))
			{
				strcpy(pTimeConfig->timeMode.modeName, "NTP");
				if((tz_cfg % 60) == 0)
				{
					pTimeConfig->timeZone = tz_cfg;
				}
				else
				{
					if((tz_cfg == 450) || (tz_cfg == 510) ||
						(tz_cfg == 930) || (tz_cfg == 990) ||
						(tz_cfg == 1050) || (tz_cfg == 1065) ||
						(tz_cfg == 1110) || (tz_cfg == 1290) ||
						(tz_cfg == 1410) || (tz_cfg == 1485))
					{
						pTimeConfig->timeZone = tz_cfg;
					}
				}
				
				anj_config_system_time_set(pTimeConfig);
			}
			free(pTimeConfig);
			
			log_print(HT_LOG_INFO, "old timezone info (timezone = %ld, tzname[0] = %s)\n", timezone,  tzname[0]);

		}
	}
	else if(&p_req->UTCDateTime)/* manual datetime path */
	{
		log_print(HT_LOG_INFO, "set manual date/time\n");
		system("touch /tmp/330_onvif");
		_Year = p_req->UTCDateTime.Date.Year;
		_Month = p_req->UTCDateTime.Date.Month;
		_Day = p_req->UTCDateTime.Date.Day;
		_Hour = p_req->UTCDateTime.Time.Hour;
		_Minute = p_req->UTCDateTime.Time.Minute;
		_Second = p_req->UTCDateTime.Time.Second;

		log_print(HT_LOG_INFO, "recv time: %04d-%02d-%02d %02d:%02d:%02d\n", _Year, _Month, _Day, _Hour, _Minute, _Second);

		int retStatus = -1;
		int tz_cfg = 0;
		int OffsetMin = 0;
		struct tm recvtime;
		struct tm tbuf;
		struct tm newtime;
		memset(&recvtime, 0, sizeof(struct tm));
		memset(&tbuf, 0, sizeof(struct tm));
		memset(&newtime, 0, sizeof(struct tm));
		newtime.tm_year = recvtime.tm_year = _Year - 1900;
		newtime.tm_mon = recvtime.tm_mon 	= _Month - 1;
		newtime.tm_mday = recvtime.tm_mday = _Day;
		newtime.tm_hour = recvtime.tm_hour = _Hour;
		newtime.tm_min  = recvtime.tm_min  = _Minute;
		newtime.tm_sec  = recvtime.tm_sec  = _Second;

			TimeConfig *pTimeConfig = (TimeConfig *)malloc(sizeof(TimeConfig));
			memset(pTimeConfig, 0, sizeof(TimeConfig));
			memcpy(pTimeConfig, &pSystemCfg->timeCfg, sizeof(*pTimeConfig));
		SummerTimeConfig dst = {0};
		/* Time Zone */
		if(p_req->SystemDateTime.TimeZoneFlag)
		{
			_TZ  = p_req->SystemDateTime.TimeZone.TZ;
			/* Parse timezone and DST info if provided. */
			retStatus = parse_timezone_DaylightSavings(_TZ, &ret, &OffsetMin, &dst);
			log_print(HT_LOG_INFO, "retStatus:%d\n", retStatus);
			if(retStatus >= 0)
			{
				if(retStatus > 0)
				{
					// Request carries explicit DST settings.
					if(_DaylightSavings > 0)
						dst.nEnable = 1;
					else 
						dst.nEnable = 0;
					memcpy(&(pTimeConfig->summerConfig), &dst, sizeof(SummerTimeConfig));

					if(ret == -1)
						ret = oset_timezone(_TZ, &OffsetMin); // fallback timezone set
				}
				else
				{
					if(_DaylightSavings > 0)
						pTimeConfig->summerConfig.nEnable = 1;
					else 
						pTimeConfig->summerConfig.nEnable = 0;
				}
			}
			else
			{
				ret = oset_timezone(_TZ, &OffsetMin); // fallback timezone set
			}

			if(ret == 100)
			{
				free(pTimeConfig);
				return ONVIF_ERR_InvalidTimeZone;
			}

			tz_cfg = OffsetMin + 12 * 60;
			
			if((retStatus >= 0) || (strcmp(pTimeConfig->timeMode.modeName, "MANUAL")) || (pTimeConfig->timeZone != tz_cfg))
			{
				strcpy(pTimeConfig->timeMode.modeName, "MANUAL");
				if(((tz_cfg % 60) == 0))
				{
					pTimeConfig->timeZone = tz_cfg;
				}
				else
				{
					if((tz_cfg == 450) || (tz_cfg == 510) ||(tz_cfg == 930) || (tz_cfg == 990) ||(tz_cfg == 1050) || 
						(tz_cfg == 1065) ||(tz_cfg == 1110) || (tz_cfg == 1290) ||(tz_cfg == 1410) || (tz_cfg == 1485))
					{
						pTimeConfig->timeZone = tz_cfg;
					}
				}
				anj_config_system_time_set(pTimeConfig);
			}
		}
		else
		{ // if timezone is absent, reuse current configured offset
			OffsetMin = pTimeConfig->timeZone - 12 * 60;
			tz_cfg = pTimeConfig->timeZone;
			log_print(HT_LOG_INFO, "no timezone parameter!\n");
			ret = -1;
		}
		time_t time_seconds = mktime(&recvtime); // convert to epoch
		log_print(HT_LOG_INFO, "time_seconds:%ld\n", time_seconds);
		// Apply timezone offset minutes from request/config.
		time_seconds += (OffsetMin*60); // local wall clock candidate
		time_t timesecond = time_seconds;
		memcpy(&newtime, localtime_r(&timesecond, &tbuf), sizeof(struct tm));
		_Year = newtime.tm_year + 1900;
		_Month = newtime.tm_mon + 1;
		_Day = newtime.tm_mday;
		_Hour = newtime.tm_hour;
		_Minute = newtime.tm_min;
		_Second = newtime.tm_sec;
		log_print(HT_LOG_INFO, "NoDL_newtime: %04d-%02d-%02d %02d:%02d:%02d\n", _Year, _Month, _Day, _Hour, _Minute, _Second);

#if 1// Adjust with DST rules when DST is enabled.
		if(pTimeConfig->summerConfig.nEnable != 0 && (access("/opt/ch/daylight_time_close", F_OK) != 0))
		{
			log_print(HT_LOG_INFO, "apply DST adjustment for current UTC/local relation\n");
			/* Build DST start/end seconds for current year. */
			/* Then compare current second with the DST window. */
			time_t DLStart;
			time_t DLEnd;

			int Curyear = newtime.tm_year + 1900;

			DLStart = oset_GetSecFromMonthWeekNo(Curyear, pTimeConfig->summerConfig.nStartMonth, pTimeConfig->summerConfig.nStartWeek, pTimeConfig->summerConfig.nStartWeekday);
			DLEnd = oset_GetSecFromMonthWeekNo(Curyear, pTimeConfig->summerConfig.nToMonth, pTimeConfig->summerConfig.nToWeek, pTimeConfig->summerConfig.nToWeekday);
			
			DLStart += pTimeConfig->summerConfig.nStartHour*3600;
			DLEnd += pTimeConfig->summerConfig.nToHour*3600;

			log_print(HT_LOG_INFO, "timesecond:%ld StartSec:%ld EndSec:%ld \n", timesecond, DLStart, DLEnd);
			/* Add DST offset when current time is in DST interval. */
			if(DLStart < timesecond && timesecond < DLEnd)/* in DST interval */
			{
				timesecond += pTimeConfig->summerConfig.nOffsetMin*60;
			}
			else/* out of DST interval */
			{
				timesecond += 0;
			}
			
			/* Rebuild calendar time from adjusted seconds. */
			memcpy(&newtime, localtime_r(&timesecond, &tbuf), sizeof(struct tm)); 
			_Year = newtime.tm_year + 1900;
			_Month = newtime.tm_mon + 1;
			_Day = newtime.tm_mday;
			_Hour = newtime.tm_hour;
			_Minute = newtime.tm_min;
			_Second = newtime.tm_sec;
			
			log_print(HT_LOG_INFO, "DL_newtime: %04d-%02d-%02d %02d:%02d:%02d\n", _Year, _Month, _Day, _Hour, _Minute, _Second);
		}
#endif
		log_print(HT_LOG_INFO, "pTimeConfig->timeZone:%d\n", pTimeConfig->timeZone);
		/* TimingMode 1 = manual */
        anj_systime_set_time_and_zone(newtime, pTimeConfig->timeZone, 1);
	}
	
	if (p_req->SystemDateTime.TimeZoneFlag && 
		p_req->SystemDateTime.TimeZone.TZ[0] != '\0')
	{
		strcpy(g_onvif_cfg.SystemDateTime.TimeZone.TZ, p_req->SystemDateTime.TimeZone.TZ);
	}

	// send notify message ...
	onvif_LastClockSynchronizationNotify();

	return ONVIF_OK;
}

/**
 * @brief
 *  Reboots a device. 
 *  Before the device reboots the response message shall be sent.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SystemReboot()
{
	// todo : here add handler code ...
	log_print(HT_LOG_WARN, "onvif service reboot\n");
	__RECORD_LOG_INFO("onvif service reboot\n");
	anj_sysmng_reboot();
	// send onvif bye message    
	sleep(3);
	onvif_bye();

	// please comment the code below
	// send onvif hello message, just for test
	//sleep(3);
	//onvif_hello();

	return ONVIF_OK;
}

/**
 * @brief
 *  Reloads parameters of a device to their factory default values.
 *  The device shall support hard and soft factory default through the 
 *  SetSystemFactoryDefault command.
 *
 *  Hard All parameters are set to their factory default value.
 *
 *  Soft The meaning of soft factory default is device product-specific and 
 *       vendor-specific. The effect of a soft factory default operation is 
 *       not fully defined. However, it shall be guaranteed that after a soft 
 *       reset the device is reachable on the same IP address as used before 
 *       the reset. This means that basic network settings like IP address, 
 *       subnet and gateway or DHCP settings are kept unchanged by the soft reset.
 *  
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetSystemFactoryDefault(tds_SetSystemFactoryDefault_REQ * p_req)
{
	// todo : here add handler code ...
	if (access("/opt/ch/ignore_ofactory", F_OK) == F_OK)
	{
		return ONVIF_OK;
	}
	unsigned int reserved_bits = 0;
    __RECORD_LOG_INFO("restore config reserved_bits=%u\n", reserved_bits); 
	anj_sysmng_config_restore(reserved_bits);
#ifdef PROFILE_Q_SUPPORT
	onvif_switchDeviceState(0); // Devices conformant to Profile Q shall be in Factory Default State 
								// out-of-the-box and after hard factory reset
#endif

	// todo : please comment the code below, just for test
	// send onvif hello message
	sleep(3);
	onvif_hello();

	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the hostname on a device.
 *
 *  Attention: a call to SetDNS may result in overriding a previously set hostname.
 *  
 *  A device shall accept strings formated according to [RFC 1123] section 2.1 or 
 *  alternatively to [RFC 952], other string shall be considered as invalid strings.
 *
 *  A device shall try to retrieve the name via DHCP when the HostnameFromDHCP 
 *  capability is set and an empty name string is provided.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidHostname
 **/
ONVIF_RET onvif_tds_SetHostname(tds_SetHostname_REQ * p_req)
{
	if (p_req->Name[0] == '\0')
	{
		// A device shall try to retrieve the name via DHCP 
		//  when the HostnameFromDHCP capability is 
		//  set and an empty name string is provided

		if (g_onvif_cfg.Capabilities.device.HostnameFromDHCP)
		{
			// todo : retrieve the name via DHCP

			g_onvif_cfg.network.HostnameInformation.FromDHCP = TRUE;
		}
		else
		{
			return ONVIF_ERR_InvalidHostname;
		}
	}
	else if (onvif_is_valid_hostname(p_req->Name) == FALSE)
	{
		return ONVIF_ERR_InvalidHostname;
	}
	else
	{
		g_onvif_cfg.network.HostnameInformation.FromDHCP = FALSE;
	}

	// todo : here add handler code ...
	
	int ret;
	ret = checkhostname(p_req->Name);
	if(ret != 0)
	{
		return ONVIF_ERR_InvalidHostname;
	}
	
	sethostname(p_req->Name, strlen(p_req->Name));
	NetworkConfigNew *pTmpNetWorkConfig = (NetworkConfigNew *)getNetWorkConfig();
	snprintf(pTmpNetWorkConfig->lanCfg.hostname, sizeof(pTmpNetWorkConfig->lanCfg.hostname),
	         "%.*s", (int)(sizeof(pTmpNetWorkConfig->lanCfg.hostname) - 1), p_req->Name);
	snprintf(g_onvif_cfg.network.HostnameInformation.Name,
	         sizeof(g_onvif_cfg.network.HostnameInformation.Name),
	         "%.*s",
	         (int)(sizeof(g_onvif_cfg.network.HostnameInformation.Name) - 1),
	         p_req->Name);

	return ONVIF_OK;
}

/**
 * @brief
 *  Controls whether the hostname shall be retrieved from DHCP.
 *
 *  A device shall support this command if support is signalled via the 
 *  HostnameFromDHCP capability. Depending on the device implementation 
 *  the change may only become effective after a device reboot. A device 
 *  shall accept the command independent whether it is currently using 
 *  DHCP to retrieve its IPv4 address or not.Note that the device is not 
 *  required to retrieve its hostname via DHCP while the device is not 
 *  using DHCP for retrieving its IP address. In the latter case the device 
 *  may fall back to the statically
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetHostnameFromDHCP(tds_SetHostnameFromDHCP_REQ * p_req)
{
	// todo : here add handler code ...


	//g_onvif_cfg.network.HostnameInformation.FromDHCP = p_req->FromDHCP;

	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the DNS settings on a device.
 *
 *  It is valid to set the FromDHCP flag while the device is not using 
 *  DHCP to retrieve its IPv4 address.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 **/
 
extern char searchdomainname[MID_INFO_LENGTH];

ONVIF_RET onvif_tds_SetDNS(tds_SetDNS_REQ * p_req)
{
	int d = 0;
	char resolvConfig[256];
	char dnsServer[2][256];
	memset(resolvConfig, 0, 256);
	memset(dnsServer, 0, sizeof(dnsServer));

	// todo : here add handler code ...
	if (p_req->DNSInformation.FromDHCP == true)
	{
		return ONVIF_ERR_NotSupported;
	}

	if (ARRAY_SIZE(p_req->DNSInformation.SearchDomain) > 0)
	{
		strcat(resolvConfig, "search ");
		int i = 0;
		for (i = 0; i < ARRAY_SIZE(p_req->DNSInformation.SearchDomain); i++)
		{
			if ( i == 0)
			{
				strcpy(searchdomainname, p_req->DNSInformation.SearchDomain[i]);
			}
			strcat(resolvConfig, p_req->DNSInformation.SearchDomain[i]);
			strcat(resolvConfig, " ");
		}
		strcat(resolvConfig, "\n");
	}

	if (ARRAY_SIZE(p_req->DNSInformation.DNSServer) > 0)
	{
		int i = 0;
		for (i = 0; i < ARRAY_SIZE(p_req->DNSInformation.DNSServer); i++)
		{
			strcat(resolvConfig, "nameserver ");
			if (strlen(p_req->DNSInformation.DNSServer[i]) > 0 && isValidIp4(p_req->DNSInformation.DNSServer[i]) == 0)
			{
				return ONVIF_ERR_InvalidIPv4Address;
			}
			strcat(resolvConfig, p_req->DNSInformation.DNSServer[i]);
			strcat(resolvConfig, "\n");

			if (d <= 1)
			{
				strcpy(&dnsServer[d][0], p_req->DNSInformation.DNSServer[i]);
				d ++;
			}
		}
	}
	
	int fd = open("/etc/resolv.conf", O_TRUNC | O_RDWR);
	if (fd == -1)
	{
		return ONVIF_ERR_GENERAL;
	}
	write(fd, resolvConfig, strlen(resolvConfig));
	close(fd);

	char dnsList[256];
	memset(dnsList, 0, 256);

	if (d >= 1)
	{
		strcat(dnsList, &dnsServer[0][0]);
	}

	if (d >= 2)
	{
		strcat(dnsList, ";");
		strcat(dnsList, &dnsServer[1][0]);
	}

	{
		LANConfig lanCfg;
		memset(&lanCfg, 0, sizeof(lanCfg));
		memcpy(&lanCfg, getNetWorkConfig(), sizeof(lanCfg));
		char buf[128] = {0};
		char *sep;
		snprintf(buf, sizeof(buf), "%.*s", (int)(sizeof(buf) - 1), dnsList);
		sep = strchr(buf, ' ');
		if (!sep)
			sep = strchr(buf, ';');
		memset(lanCfg.DNS1, 0, sizeof(lanCfg.DNS1));
		memset(lanCfg.DNS2, 0, sizeof(lanCfg.DNS2));
		if (sep)
		{
			*sep = '\0';
			snprintf(lanCfg.DNS1, sizeof(lanCfg.DNS1), "%.*s", (int)(sizeof(lanCfg.DNS1) - 1), buf);
			snprintf(lanCfg.DNS2, sizeof(lanCfg.DNS2), "%.*s", (int)(sizeof(lanCfg.DNS2) - 1), sep + 1);
		}
		else
		{
			snprintf(lanCfg.DNS1, sizeof(lanCfg.DNS1), "%.*s", (int)(sizeof(lanCfg.DNS1) - 1), buf);
		}
		anj_config_network_lan_set(&lanCfg);
	}
	
	if(1)
	{
		g_onvif_cfg.network.DNSInformation.FromDHCP = p_req->DNSInformation.FromDHCP;
		g_onvif_cfg.network.DNSInformation.SearchDomainFlag = p_req->DNSInformation.SearchDomainFlag;

		if (g_onvif_cfg.network.DNSInformation.FromDHCP == FALSE)
		{
			if (p_req->DNSInformation.SearchDomainFlag)
			{
				memcpy(g_onvif_cfg.network.DNSInformation.SearchDomain, p_req->DNSInformation.SearchDomain, sizeof(g_onvif_cfg.network.DNSInformation.SearchDomain));
			}

			memcpy(g_onvif_cfg.network.DNSInformation.DNSServer, p_req->DNSInformation.DNSServer, sizeof(g_onvif_cfg.network.DNSInformation.DNSServer));
		}
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the dynamic DNS settings on a device.
 *
 * The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetDynamicDNS(tds_SetDynamicDNS_REQ * p_req)
{
	// todo : here add handler code ...

	memcpy(&g_onvif_cfg.network.DynamicDNSInformation, &p_req->DynamicDNSInformation, sizeof(onvif_DynamicDNSInformation));

	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the NTP settings on a device.
 *
 *  A device shall accept string formated according to [RFC 1123] section 2.1, 
 *  other string shall be considered as invalid strings. It is valid to set the 
 *  FromDHCP flag while the device is not using DHCP to retrieve its IPv4 address.
 *
 *  Changes to the NTP server list shall not affect the clock mode DateTimeType. 
 *  Use SetSystemDateAndTime to activate NTP operation.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 *  ONVIF_ERR_InvalidDnsName
 **/
ONVIF_RET onvif_tds_SetNTP(tds_SetNTP_REQ * p_req)
{
	// todo : here add handler code ...

	g_onvif_cfg.network.NTPInformation.FromDHCP = p_req->NTPInformation.FromDHCP;

	if (g_onvif_cfg.network.NTPInformation.FromDHCP == TRUE)
	{
		
		if (1)// refresh NTP server from DHCP helper script
		{
			system("rm "NTP_PATH);
			system("udhcpc -s /usr/share/udhcpc/ntp.script");
			usleep(1000*1500);
		}
		if (access(NTP_PATH, F_OK) == F_OK)
		{
			char ip_buf[64] = {0};
			char ntpmsg_buf[32] = {0};
			size_t ip_len = 0;
			memset(ntpmsg_buf, 0, 32);
			
			read_file_to_buffer(NTP_PATH, ntpmsg_buf, 32);
			ip_len = strcspn(ntpmsg_buf, "\r\n");
			if (ip_len >= sizeof(ip_buf))
				ip_len = sizeof(ip_buf) - 1;
			memcpy(ip_buf, ntpmsg_buf, ip_len);
			ip_buf[ip_len] = '\0';
			
			if (isValidIp4(ip_buf))
			{
				log_print(HT_LOG_INFO, "ntp server ip form dhcp is:%s\n", ip_buf);
				SetNtpConfig(ip_buf);
				return ONVIF_OK;
			}
			else if(isValidHostname(ip_buf))
			{
				log_print(HT_LOG_INFO, "ntp server domain form dhcp is:%s\n", ip_buf);
				SetNtpConfig(ip_buf);
				return ONVIF_OK;
			}
			else
			{
				return ONVIF_ERR_NotSupported;
			}
		}
		else
		{
			return ONVIF_ERR_NotSupported;
		}
	}
	else
	{
		if (TRUE == p_req->NTPInformation.IsExistIPV6Address)       // IPv6 NTP address is not supported here
        {
            return ONVIF_ERR_NotSupported;
        }
	
		SetNtpConfig((char *)p_req->NTPInformation.NTPServer);
		memcpy(g_onvif_cfg.network.NTPInformation.NTPServer, p_req->NTPInformation.NTPServer, sizeof(g_onvif_cfg.network.NTPInformation.NTPServer));
		return ONVIF_OK;
	}
}

/**
 * @brief
 *  Sets the zero-configuration on the device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 **/
ONVIF_RET onvif_tds_SetZeroConfiguration(tds_SetZeroConfiguration_REQ * p_req)
{
	if (strcmp(p_req->InterfaceToken, g_onvif_cfg.network.ZeroConfiguration.InterfaceToken))
	{
		return ONVIF_ERR_InvalidNetworkInterface;
	}
	
	// todo : here add handler code ...

	//g_onvif_cfg.network.ZeroConfiguration.Enabled = p_req->Enabled;

	//onvif_init_ZeroConfiguration();

	return ONVIF_OK;
}

/**
 * @brief
 *  Configures defined network protocols on a device.
 *
 *  This message configures one or more defined network protocols supported 
 *  by the device. There are currently three protocols defined, HTTP, HTTPS 
 *  and RTSP. For each protocol the parameters Port and Enable/Disable can 
 *  be configured.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_ServiceNotSupported
 *  ONVIF_ERR_PortAlreadyInUse
 **/
ONVIF_RET onvif_tds_SetNetworkProtocols(tds_SetNetworkProtocols_REQ * p_req)
{
#ifndef HTTPS
	if (p_req->NetworkProtocol.HTTPSFlag && p_req->NetworkProtocol.HTTPSEnabled)
	{
		return ONVIF_ERR_ServiceNotSupported;
	}
#endif

	// todo : here add handler code ...


	/*if (p_req->NetworkProtocol.HTTPFlag)
	{
		g_onvif_cfg.network.NetworkProtocol.HTTPEnabled = p_req->NetworkProtocol.HTTPEnabled;
		memcpy(g_onvif_cfg.network.NetworkProtocol.HTTPPort, p_req->NetworkProtocol.HTTPPort, sizeof(g_onvif_cfg.network.NetworkProtocol.HTTPPort));
	}

	if (p_req->NetworkProtocol.HTTPSFlag)
	{
		g_onvif_cfg.network.NetworkProtocol.HTTPSEnabled = p_req->NetworkProtocol.HTTPSEnabled;
		memcpy(g_onvif_cfg.network.NetworkProtocol.HTTPSPort, p_req->NetworkProtocol.HTTPSPort, sizeof(g_onvif_cfg.network.NetworkProtocol.HTTPSPort));
	}

	if (p_req->NetworkProtocol.RTSPFlag)
	{
		g_onvif_cfg.network.NetworkProtocol.RTSPEnabled = p_req->NetworkProtocol.RTSPEnabled;
		memcpy(g_onvif_cfg.network.NetworkProtocol.RTSPPort, p_req->NetworkProtocol.RTSPPort, sizeof(g_onvif_cfg.network.NetworkProtocol.RTSPPort));
	}*/
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the default gateway settings on a device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidGatewayAddress
 *  ONVIF_ERR_InvalidIPv4Address
 **/
ONVIF_RET onvif_tds_SetNetworkDefaultGateway(tds_SetNetworkDefaultGateway_REQ * p_req)
{
	// todo : here add handler code ...
	char _IPv4Address[LARGE_INFO_LENGTH];
	strcpy(_IPv4Address, p_req->IPv4Address[0]);
	
	if(isValidIp4(_IPv4Address) == 0) // Check IP address
	{
		return ONVIF_ERR_InvalidIPv4Address;
	}
	log_print(HT_LOG_INFO, "GateWayIp:%s\n", _IPv4Address);

	{
		LANConfig *pLan = (LANConfig *)getNetWorkConfig();
		if (pLan)
		{
			strncpy(pLan->gateWay, _IPv4Address, sizeof(pLan->gateWay) - 1);
			pLan->gateWay[sizeof(pLan->gateWay) - 1] = '\0';
			anj_config_network_lan_set(pLan);
		}
	}
	
	memcpy(g_onvif_cfg.network.NetworkGateway.IPv4Address, p_req->IPv4Address, sizeof(g_onvif_cfg.network.NetworkGateway.IPv4Address));

	return ONVIF_OK;
}

/**
 * @brief
 *  Retrieve URIs from which system information may be downloaded using HTTP. 
 *  URIs may be returned for the following system information:
 *
 *  System Logs. Multiple system logs may be returned, of different types. 
 *  The exact format of the system logs is outside the scope of this specification.
 *
 *  Support Information. This consists of arbitrary device diagnostics information 
 *  from a device. The exact format of the diagnostic information is outside the 
 *  scope of this specification.
 *
 *  System Backup. The received file is a backup file that can be used to restore 
 *  the current device configuration at a later date. The exact format of the backup 
 *  configuration file is outside the scope of this specification.
 *
 *  If the device allows retrieval of system logs, support information or system 
 *  backup data, it should make them available via HTTP GET.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_GetSystemUris(HTTPCLN * p_user, tds_GetSystemUris_RES * p_res)
{
	char sip[32];
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;

	onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);

	p_res->AccessLogUriFlag = 1;
	p_res->SystemLogUriFlag = 1;
	p_res->SupportInfoUriFlag = 1;
	p_res->SystemBackupUriFlag = 1;

	if (g_onvif_cfg.http_enable && !p_srv->https)
	{
		sprintf(p_res->SystemLogUri, "http://%s:%u/SystemLog", sip, g_onvif_cls.http_port);
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		sprintf(p_res->SystemLogUri, "https://%s:%u/SystemLog", sip, g_onvif_cls.https_port);
	}
#endif

	if (g_onvif_cfg.http_enable && !p_srv->https)
	{
		sprintf(p_res->AccessLogUri, "http://%s:%u/AccessLog", sip, g_onvif_cls.http_port);
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		sprintf(p_res->AccessLogUri, "https://%s:%u/AccessLog", sip, g_onvif_cls.https_port);
	}
#endif

	if (g_onvif_cfg.http_enable && !p_srv->https)
	{
		sprintf(p_res->SupportInfoUri, "http://%s:%u/SupportInfo", sip, g_onvif_cls.http_port);
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		sprintf(p_res->SupportInfoUri, "https://%s:%u/SupportInfo", sip, g_onvif_cls.https_port);
	}
#endif

	if (g_onvif_cfg.http_enable && !p_srv->https)
	{
		sprintf(p_res->SystemBackupUri, "http://%s:%u/SystemBackup", sip, g_onvif_cls.http_port);
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		sprintf(p_res->SystemBackupUri, "https://%s:%u/SystemBackup", sip, g_onvif_cls.https_port);
	}
#endif

	return ONVIF_OK;
}

/**
 * @brief
 *  This function is called when the network interface 
 *  is set without restarting the device
 **/
void onvif_tds_SetNetworkInterfacesToDevice(void * argv)
{
	NetworkInterfaceList * p_net_inf = (NetworkInterfaceList *)argv;
	LANConfig lan_cfg;

	if (p_net_inf == NULL)
	{
		return;
	}

	memset(&lan_cfg, 0, sizeof(lan_cfg));
	{
		LANConfig *pLan = (LANConfig *)getNetWorkConfig();
		if (!pLan)
			return;
		memcpy(&lan_cfg, pLan, sizeof(lan_cfg));
	}

	if (p_net_inf->NetworkInterface.Enabled)
	{
		if (p_net_inf->NetworkInterface.IPv4Flag && p_net_inf->NetworkInterface.IPv4.Enabled)
		{
			if (p_net_inf->NetworkInterface.IPv4.Config.DHCP)
			{
				lan_cfg.dhcpEnable = 1;
				anj_config_network_lan_set(&lan_cfg);
			}
			else
			{
				struct in_addr addr;
				char netmask[32] = {0};
				char gateway[32] = {0};
				in_addr_t mask;
				in_addr_t gateway_ip;

				mask = get_netmask_by_prefix_len(p_net_inf->NetworkInterface.IPv4.Config.PrefixLength);
				addr.s_addr = mask;
				snprintf(netmask, sizeof(netmask), "%s", inet_ntoa(addr));

				if (ipv4_str_to_num(p_net_inf->NetworkInterface.IPv4.Config.Address, &addr))
				{
					gateway_ip = get_gateway_by_prefix_len(ntohl(addr.s_addr), p_net_inf->NetworkInterface.IPv4.Config.PrefixLength);
					addr.s_addr = gateway_ip;
					snprintf(gateway, sizeof(gateway), "%s", inet_ntoa(addr));
				}

				lan_cfg.dhcpEnable = 0;
				{
					LANConfig *pLan = (LANConfig *)getNetWorkConfig();
					if (pLan)
					{
						strncpy(pLan->IPAddress, p_net_inf->NetworkInterface.IPv4.Config.Address, sizeof(pLan->IPAddress) - 1);
						pLan->IPAddress[sizeof(pLan->IPAddress) - 1] = '\0';
						strncpy(pLan->netMask, netmask, sizeof(pLan->netMask) - 1);
						pLan->netMask[sizeof(pLan->netMask) - 1] = '\0';
						strncpy(pLan->gateWay, gateway, sizeof(pLan->gateWay) - 1);
						pLan->gateWay[sizeof(pLan->gateWay) - 1] = '\0';
						anj_config_network_lan_set(pLan);
					}
				}
				anj_config_network_lan_set(&lan_cfg);
			}
		}
	}

	// The onvif discovery service needs to be restarted when the network interface changed
	onvif_stop_discovery();
	onvif_start_discovery();

	// Update onvif service IP address
	strcpy(g_onvif_cls.server_ip, get_local_ip());

	// Update device capability set and onvif service address
	onvif_init_capabilities();

	onvif_hello();
}

/**
 * @brief
 *  Sets the network interface configuration on a device.
 *
 *  If a device responds with RebootNeeded set to false, the device can be 
 *  reached via the new IP address without further action. A client should 
 *  be aware that a device may not be responsive for a short period of time 
 *  until it signals availability at the new address via the discovery Hello 
 *  messages
 *
 *  If a device responds with RebootNeeded set to true, it will be further 
 *  available under its previous IP address. The settings will only be activated 
 *  when the device is rebooted via the SystemReboot command.
 *
 *  For interoperability with a client unaware of the IEEE 802.11 extension 
 *  a device shall retain its IEEE 802.11 configuration if the IEEE 802.11 
 *  configuration element isn't present in the request.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 *  ONVIF_ERR_InvalidMtuValue
 *  ONVIF_ERR_InvalidInterfaceSpeed
 *  ONVIF_ERR_InvalidInterfaceType
 *  ONVIF_ERR_InvalidIPv4Address
 **/
ONVIF_RET onvif_tds_SetNetworkInterfaces(tds_SetNetworkInterfaces_REQ * p_req, tds_SetNetworkInterfaces_RES * p_res)
{
	NetworkInterfaceList * p_net_inf = onvif_find_NetworkInterface(g_onvif_cfg.network.interfaces, p_req->NetworkInterface.token);
	if (NULL == p_net_inf)
	{
		return ONVIF_ERR_InvalidNetworkInterface;
	}

	// Check network interface parameters

	if (p_req->NetworkInterface.InfoFlag && 
		p_req->NetworkInterface.Info.MTUFlag && 
		(p_req->NetworkInterface.Info.MTU < 0 || p_req->NetworkInterface.Info.MTU > 1530))
	{
		return ONVIF_ERR_InvalidMtuValue;
	}

	if (p_req->NetworkInterface.Enabled && 
		p_req->NetworkInterface.IPv4Flag && 
		p_req->NetworkInterface.IPv4.Enabled && 
		p_req->NetworkInterface.IPv4.Config.DHCP == FALSE)
	{
		if (is_ip_address(p_req->NetworkInterface.IPv4.Config.Address) == FALSE)
		{
			return ONVIF_ERR_InvalidIPv4Address;
		}
	}

	// Save network interface parameters

	p_net_inf->NetworkInterface.Enabled = p_req->NetworkInterface.Enabled;

	if (p_req->NetworkInterface.InfoFlag && p_req->NetworkInterface.Info.MTUFlag)
	{
		p_net_inf->NetworkInterface.Info.MTU = p_req->NetworkInterface.Info.MTU;
	}
		
	if (p_req->NetworkInterface.IPv4Flag)
	{
		p_net_inf->NetworkInterface.IPv4.Enabled = p_req->NetworkInterface.IPv4.Enabled;
		p_net_inf->NetworkInterface.IPv4.Config.DHCP = p_req->NetworkInterface.IPv4.Config.DHCP;
		
		if (p_net_inf->NetworkInterface.IPv4.Config.DHCP == FALSE)
		{
			strcpy(p_net_inf->NetworkInterface.IPv4.Config.Address, p_req->NetworkInterface.IPv4.Config.Address);
			p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = p_req->NetworkInterface.IPv4.Config.PrefixLength;
		}
	}
	if (p_req->NetworkInterface.Enabled && p_req->NetworkInterface.IPv4Flag && p_req->NetworkInterface.IPv4.Enabled)
	{
		LANConfig lan_cfg;
		memset(&lan_cfg, 0, sizeof(lan_cfg));
		{
			LANConfig *pLan = (LANConfig *)getNetWorkConfig();
			if (!pLan)
			{
				return ONVIF_ERR_NoToken;
			}
			memcpy(&lan_cfg, pLan, sizeof(lan_cfg));
		}

		if (p_req->NetworkInterface.IPv4.Config.DHCP == FALSE)
		{
			struct in_addr ipaddr;
			struct in_addr sys_ip;

			if (p_req->NetworkInterface.IPv4.Config.Address[0] == '\0' ||
				isValidIp4(p_req->NetworkInterface.IPv4.Config.Address) == 0 ||
				ipv4_str_to_num(p_req->NetworkInterface.IPv4.Config.Address, &ipaddr) == 0)
			{
				return ONVIF_ERR_InvalidIPv4Address;
			}

			if ((sys_ip.s_addr = net_get_ifaddr(p_net_inf->NetworkInterface.Info.Name)) == (in_addr_t)-1)
			{
				return ONVIF_ERR_NoToken;
			}

			if (sys_ip.s_addr != ipaddr.s_addr || lan_cfg.dhcpEnable)
			{
				struct in_addr addr;
				char netmask[64] = {0};
				char gateway[64] = {0};
				in_addr_t mask = get_netmask_by_prefix_len(p_req->NetworkInterface.IPv4.Config.PrefixLength);
				in_addr_t gatewayip = get_gateway_by_prefix_len(ntohl(ipaddr.s_addr), p_req->NetworkInterface.IPv4.Config.PrefixLength);

				addr.s_addr = mask;
				snprintf(netmask, sizeof(netmask), "%s", inet_ntoa(addr));

				addr.s_addr = gatewayip;
				snprintf(gateway, sizeof(gateway), "%s", inet_ntoa(addr));

				{
					LANConfig *pLan = (LANConfig *)getNetWorkConfig();
					if (pLan)
					{
						strncpy(pLan->IPAddress, p_req->NetworkInterface.IPv4.Config.Address, sizeof(pLan->IPAddress) - 1);
						pLan->IPAddress[sizeof(pLan->IPAddress) - 1] = '\0';
						strncpy(pLan->netMask, netmask, sizeof(pLan->netMask) - 1);
						pLan->netMask[sizeof(pLan->netMask) - 1] = '\0';
						strncpy(pLan->gateWay, gateway, sizeof(pLan->gateWay) - 1);
						pLan->gateWay[sizeof(pLan->gateWay) - 1] = '\0';
						anj_config_network_lan_set(pLan);
					}
				}
			}

			lan_cfg.dhcpEnable = 0;
			anj_config_network_lan_set(&lan_cfg);
		}
		else
		{
			lan_cfg.dhcpEnable = 1;
			anj_config_network_lan_set(&lan_cfg);
		}
	}
	p_res->RebootNeeded = FALSE;

	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the discovery mode operation of a device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_SetDiscoveryMode(tds_SetDiscoveryMode_REQ * p_req)
{
	g_onvif_cfg.network.DiscoveryMode = p_req->DiscoveryMode;

	return ONVIF_OK;
}

/**
 * @brief
 *  Creates new device users and corresponding credentials on a device 
 *  for authentication.
 *
 *  A device shall support this command unless support signalled via the 
 *  UserConfigNotSupported capability is 'True'.
 *
 *  Either all users are created successfully or a fault message shall 
 *  be returned without creating any user.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_UsernameClash
 *  ONVIF_ERR_PasswordTooLong
 *  ONVIF_ERR_UsernameTooLong
 *  ONVIF_ERR_Password
 *  ONVIF_ERR_TooManyUsers
 *  ONVIF_ERR_AnonymousNotAllowed
 *  ONVIF_ERR_UsernameTooShort
 **/
ONVIF_RET onvif_tds_CreateUsers(tds_CreateUsers_REQ * p_req)
{
	uint32 i;	
	for (i = 0; i < ARRAY_SIZE(p_req->User); i++)
	{
		uint32 len;
		onvif_User * p_idle_user;

		if (p_req->User[i].Username[0] == '\0')/* end of user list */
		{
			break;
		}

		len = strlen(p_req->User[i].Username);
		if (len <= 3)
		{
			return ONVIF_ERR_UsernameTooShort;
		}

		if (onvif_is_user_exist(p_req->User[i].Username))
		{
			return ONVIF_ERR_UsernameClash;
		}
		
		p_idle_user = onvif_get_idle_user();
		if (p_idle_user)
		{
			//add my code
			if (1)
			{
				add_user_t account;
				strcpy(account.user_id, p_req->User[i].Username);
				strcpy(account.password, p_req->User[i].Password);
				account.authority = p_req->User[i].UserLevel;
				UserAuthAddUser(account.user_id, account.password, group[account.authority], "Enable");
			}
			memcpy(p_idle_user, &p_req->User[i], sizeof(onvif_User));
#ifdef PROFILE_Q_SUPPORT
			if (UserLevel_Administrator == p_req->User[i].UserLevel && p_req->User[i].Password[0] != '\0')
			{
				onvif_switchDeviceState(1); // creates a new admin user, switch to Operational State
			}
#endif
		}
		else
		{
			return ONVIF_ERR_TooManyUsers;
		}
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Deletes users on a device.
 *
 *  A device shall support this command unless support signalled via
 *  the UserConfigNotSupported capability is 'True'.
 *
 *  A device may have one or more fixed users that cannot be deleted 
 *  to ensure access to the unit. Either all users are deleted 
 *  successfully or a fault message shall be returned and no users be 
 *  deleted.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_UsernameMissing
 *  ONVIF_ERR_FixedUser
 **/
ONVIF_RET onvif_tds_DeleteUsers(tds_DeleteUsers_REQ * p_req)
{
	uint32 i;
	onvif_User * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->Username); i++)
	{
		if (p_req->Username[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_user(p_req->Username[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_UsernameMissing;
		}
		else if (p_item->fixed)
		{
			return ONVIF_ERR_FixedUser;
		}
	}	

	for (i = 0; i < ARRAY_SIZE(p_req->Username); i++)
	{
		if (p_req->Username[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_user(p_req->Username[i]);
		if (NULL != p_item && p_item->fixed == FALSE)
		{
			//add my code
			UserAuthDeleteUser(p_req->Username[i]);
			
			memset(p_item, 0, sizeof(onvif_User));
		}
	}

	return ONVIF_OK;
}

/**
 * @brief
 *  Updates the settings for one or several users on a device for authentication.
 *
 *  A device shall support this command unless support signalled via the 
 *  UserConfigNotSupported capability is 'True'. Either all change requests are 
 *  processed successfully or a fault message shall be returned and no change 
 *  requests be processed.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_UsernameMissing
 *  ONVIF_ERR_FixedUser
 *  ONVIF_ERR_PasswordTooLong
 *  ONVIF_ERR_Password
 *  ONVIF_ERR_AnonymousNotAllowed
 **/
ONVIF_RET onvif_tds_SetUser(tds_SetUser_REQ * p_req)
{
	uint32 i;
	onvif_User * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->User); i++)
	{
		if (p_req->User[i].Username[0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_user(p_req->User[i].Username);
		if (NULL == p_item)
		{
			return ONVIF_ERR_UsernameMissing;
		}
	}
	
	for (i = 0; i < ARRAY_SIZE(p_req->User); i++)
	{
		if (p_req->User[i].Username[0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_user(p_req->User[i].Username);
		if (p_item && FALSE == p_item->fixed)
		{
			strcpy(p_item->Password, p_req->User[i].Password);
			p_item->UserLevel = p_req->User[i].UserLevel;
			//add my code
			if (1)
			{
				add_user_t account;
				strcpy(account.user_id, p_req->User[i].Username);
				strcpy(account.password, p_req->User[i].Password);
				account.authority = p_req->User[i].UserLevel;
				
				UserAuthEditUser(account.user_id, account.password, group[account.authority], "Enable", NULL);
			}
#ifdef PROFILE_Q_SUPPORT
			if (UserLevel_Administrator == p_item->UserLevel && p_item->Password[0] != '\0')
			{
				onvif_switchDeviceState(1); // modifies the password of an existing admin user, switch to Operational State
			}
#endif
		}
	}

	return ONVIF_OK;
}

/**
 * @brief
 *  Returns the configured remote user (if any). 
 *  A device that signals support for remote user handling via the Security 
 *  Capability RemoteUserHandling shall support this operation. The user is 
 *  only valid for the WSUserToken profile or as a HTTP / RTSP user.
 *
 *  Password derivation is outside of the scope of this specification.
 *  
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NotRemoteUser
 **/
ONVIF_RET onvif_tds_GetRemoteUser(tds_GetRemoteUser_RES * p_res)
{
	// todo : here add handler code ...


	// return ONVIF_ERR_NotRemoteUser;

	if (g_onvif_cfg.RemoteUser.Username[0] == '\0')
	{
		p_res->RemoteUserFlag = 0;
	}
	else
	{
		p_res->RemoteUserFlag = 1;
		strcpy(p_res->RemoteUser.Username, g_onvif_cfg.RemoteUser.Username);
	}

	p_res->RemoteUser.UseDerivedPassword = g_onvif_cfg.RemoteUser.UseDerivedPassword;

	return ONVIF_OK; 
}

/**
 * @brief
 *  Sets the remote user. 
 *  A device that signals support for remote user handling via the Security
 *  Capability RemoteUserHandling shall support this operation. Password 
 *  derivation is outside of the scope of this specification.
 *
 *  To remove the remote user SetRemoteUser should be called without the 
 *  RemoteUser parameter.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NotRemoteUser
 **/
ONVIF_RET onvif_tds_SetRemoteUser(tds_SetRemoteUser_REQ * p_req)
{
    // todo : here add handler code ...

    // To remove the remote user SetRemoteUser should be called without the RemoteUser parameter

    // return ONVIF_ERR_NotRemoteUser;
    
    if (p_req->RemoteUserFlag)
    {
        g_onvif_cfg.RemoteUser.UseDerivedPassword = p_req->RemoteUser.UseDerivedPassword;
        strcpy(g_onvif_cfg.RemoteUser.Username, p_req->RemoteUser.Username);

        if (p_req->RemoteUser.PasswordFlag)
        {
            strcpy(g_onvif_cfg.RemoteUser.Password, p_req->RemoteUser.Password);
        }
    }
    else
    {
        g_onvif_cfg.RemoteUser.UseDerivedPassword = FALSE;
        strcpy(g_onvif_cfg.RemoteUser.Username, "");
        strcpy(g_onvif_cfg.RemoteUser.Password, "");
    }
    
    return ONVIF_OK; 
}

/**
 * @brief
 *  Adds new configurable scope parameters to a device. 
 *  The scope parameters are used in the device discovery to match a probe message
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_TooManyScopes
 **/
ONVIF_RET onvif_tds_AddScopes(tds_AddScopes_REQ * p_req)
{
	uint32 i;
	for (i = 0; i < ARRAY_SIZE(p_req->ScopeItem); i++)
	{
		onvif_Scope * p_item;

		if (p_req->ScopeItem[i][0] == '\0')
		{
			break;
		}

		if (onvif_is_scope_exist(p_req->ScopeItem[i]))
		{
			continue;
		}
		
		p_item = onvif_get_idle_scope();
		if (p_item)
		{
			p_item->ScopeDef = ScopeDefinition_Configurable;
			strcpy(p_item->ScopeItem, p_req->ScopeItem[i]);
		}
		else
		{
			return ONVIF_ERR_TooManyScopes;
		}
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the scope parameters of a device. 
 *  The scope parameters are used in the device discovery to match a probe message
 *
 *  This operation replaces all existing configurable scope parameters 
 *  (not fixed parameters). If this shall be avoided, one should use the 
 *  scope add command instead.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_TooManyScopes
 *  ONVIF_ERR_ScopeOverwrite
 **/
ONVIF_RET onvif_tds_SetScopes(tds_SetScopes_REQ * p_req)
{
	uint32 i = 0;
    uint32 j = 0;
    int nRet = 0;
    int nFindScope = 0;

    int nTmpIndex = 0;
    signed int TmpScopeIndex[MAX_SCOPE_NUMS_LIMIT] = {0};

    onvif_ScopeParseCnt strScopeCnt = {0};
    onvif_Scope * p_item = NULL;

#if 1

    /* 
		Validate ONVIF scopes count limits:
		keep core scope categories (type/location/name/etc.) within limits.
    */

	// Initialize index cache.
    for (i = 0; i < MAX_SCOPE_NUMS_LIMIT; i++)
    {
        TmpScopeIndex[i] = -1;
    }

	// Count incoming scope categories and enforce limits.
    for (i = 0; i < ARRAY_SIZE(p_req->Scopes); i++)
    {
        if (p_req->Scopes[i][0] == '\0')
        {
            break;
        }

        if (strncmp(p_req->Scopes[i], "onvif://www.onvif.org/Hardware", strlen("onvif://www.onvif.org/Hardware")) == 0)
        {
            strScopeCnt.nHardWareCnt++;
        }
        else if (strncmp(p_req->Scopes[i], "onvif://www.onvif.org/Profile", strlen("onvif://www.onvif.org/Profile")) == 0)
        {
            strScopeCnt.nProfileCnt++;
        }
        else if (strncmp(p_req->Scopes[i], "onvif://www.onvif.org/manufacturer", strlen("onvif://www.onvif.org/manufacturer")) == 0)
        {
            strScopeCnt.nManufacCnt++;
        }
        else if (strncmp(p_req->Scopes[i], "onvif://www.onvif.org/location", strlen("onvif://www.onvif.org/location")) == 0)
        {
            strScopeCnt.nLocationCnt++;
        }
        else if (strncmp(p_req->Scopes[i], "onvif://www.onvif.org/type", strlen("onvif://www.onvif.org/type")) == 0)
        {
            strScopeCnt.nTypeCnt++;
        }
        else if (strncmp(p_req->Scopes[i], "onvif://www.onvif.org/name", strlen("onvif://www.onvif.org/name")) == 0)
        {
            strScopeCnt.nNameCnt++;
        }
        else
        {
            strScopeCnt.nUserCnt++;
        }

        if (strScopeCnt.nHardWareCnt > 1 || strScopeCnt.nProfileCnt > 1 || strScopeCnt.nManufacCnt > 1 ||
            strScopeCnt.nLocationCnt > 3 || strScopeCnt.nTypeCnt > 5 ||  strScopeCnt.nNameCnt > 1 || strScopeCnt.nUserCnt > 6)
        {
            log_print(HT_LOG_INFO, "Error Scopes[%d]:%s! ScopeCnt:%d, %d, %d, %d, %d, %d, %d\n",
                i, p_req->Scopes[i], strScopeCnt.nHardWareCnt, strScopeCnt.nProfileCnt,
                strScopeCnt.nManufacCnt, strScopeCnt.nLocationCnt, strScopeCnt.nTypeCnt,
                strScopeCnt.nNameCnt, strScopeCnt.nUserCnt);
            return ONVIF_ERR_TooManyScopes;
        }
    }

	// Find configurable scopes that are absent in the new set.
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
        nFindScope = 0;

        if(g_onvif_cfg.scopes[i].ScopeItem[0] == '\0') 
        {
            continue;
        }

        nFindScope = 0;
        p_item = &g_onvif_cfg.scopes[i];

        for (j = 0; j < ARRAY_SIZE(p_req->Scopes); j++)
        {

			if (p_req->Scopes[j][0] == '\0')                            // end of incoming scope list
            {
                break;
            }

			if (strlen(p_item->ScopeItem) != strlen(p_req->Scopes[j]) ) // quick length mismatch fast path
            {
                continue;
            }

            if (strncmp(p_item->ScopeItem, p_req->Scopes[j], strlen(p_req->Scopes[j])) == 0)
            {
                nFindScope = 1;
                break;
            }
        }

        if (0 == nFindScope)
        {
            nRet = onvif_compare_nofind_scope(p_item, strScopeCnt);

            if (0 == nRet)
            {
                TmpScopeIndex[nTmpIndex++] = i;
            }
            else if (-1 == nRet)
            {
                return ONVIF_ERR_ScopeOverwrite;
            }
            else if (-2 == nRet)
            {
                return ONVIF_ERR_FixedScope;
            }
        }
	}

	// Clear removed scope items by collected index.
    for(i = 0; i < MAX_SCOPE_NUMS_LIMIT; i++)
    {
        if (TmpScopeIndex[i] == -1)
        {
            break;
        }

        memset(g_onvif_cfg.scopes[TmpScopeIndex[i]].ScopeItem, 0, sizeof(g_onvif_cfg.scopes[i].ScopeItem));
    }

	// Apply incoming scopes into configurable scope slots.
    for (i = 0; i < ARRAY_SIZE(p_req->Scopes); i++)
    {
		if (p_req->Scopes[i][0] == '\0')
		{
			break;
		}

        p_item = onvif_find_scope(p_req->Scopes[i]);
        if(p_item && ScopeDefinition_Configurable == p_item->ScopeDef)
        {
			//strcpy(p_item->ScopeItem, p_req->Scopes[i]);  // keep existing item as-is
            ;
        }
        else
        {
            p_item = onvif_get_idle_scope();
            if (p_item)
            {
                p_item->ScopeDef = ScopeDefinition_Configurable;
                strcpy(p_item->ScopeItem, p_req->Scopes[i]);
            }
            else
            {
                return ONVIF_ERR_TooManyScopes;
            }
        }
    }

#else
	for (i = 0; i < ARRAY_SIZE(p_req->Scopes); i++)
	{
		if (p_req->Scopes[i][0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_scope(p_req->Scopes[i]);
		if (p_item && ScopeDefinition_Fixed == p_item->ScopeDef)
		{
			return ONVIF_ERR_ScopeOverwrite;
		}
	}
	
	for (i = 0; i < ARRAY_SIZE(p_req->Scopes); i++)
	{
		if (p_req->Scopes[i][0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_scope(p_req->Scopes[i]);
		if (p_item && ScopeDefinition_Configurable == p_item->ScopeDef)
		{
			strcpy(p_item->ScopeItem, p_req->Scopes[i]);
		}
		else
		{
		    p_item = onvif_get_idle_scope();
    		if (p_item)
    		{
    			p_item->ScopeDef = ScopeDefinition_Configurable;
    			strcpy(p_item->ScopeItem, p_req->Scopes[i]);
    		}
		}
	}
#endif

	return ONVIF_OK;
}

/**
 * @brief
 *  Deletes scope-configurable scope parameters from a device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_FixedScope
 *  ONVIF_ERR_NoScope
 **/
ONVIF_RET onvif_tds_RemoveScopes(tds_RemoveScopes_REQ * p_req)
{
    uint32 i;
	onvif_Scope * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->ScopeItem); i++)
	{
		if (p_req->ScopeItem[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_scope(p_req->ScopeItem[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_NoScope;
		}
		else if (ScopeDefinition_Fixed == p_item->ScopeDef)
		{
			return ONVIF_ERR_FixedScope;
		}
	}	

	for (i = 0; i < ARRAY_SIZE(p_req->ScopeItem); i++)
	{
		if (p_req->ScopeItem[i][0] == '\0')
		{
			break;
		}

		p_item = onvif_find_scope(p_req->ScopeItem[i]);
		if (NULL != p_item && ScopeDefinition_Configurable == p_item->ScopeDef)
		{
			memset(p_item, 0, sizeof(onvif_Scope));
		}
	}	

	return ONVIF_OK;
}

static int get_system_freemem(void)
{
	int getdata = 0;
	char szProcFile[64] = {0};
	sprintf(szProcFile, "/proc/meminfo");

	char buff[1024]="";
	int ret;
	FILE *fp=fopen(szProcFile,"rb");
	if(fp)
	{
		while( (ret = fread(buff, 1, 1023, fp)) > 0)
		{
			buff[ret]=0;			
			char *pSize = NULL;

			pSize = strstr(buff,"MemFree:");
			if(pSize)
			{
				pSize+=strlen("MemFree:");
				
				while(pSize)
				{
					if(!isspace(*pSize))
						break;
						
					pSize++;								
				}
				
				getdata=atoi(pSize);	
			}		
		}
	
		fclose(fp);
	}

	return getdata;
}

static void onvif_prepare_firmware_mem()
{
	static int uninit_module_flag = 1;
	if (uninit_module_flag)
	{
		anj_service_provider_uninit_single(2); // ANJ_SERVICE_PROVIDER_WEB
		anj_service_provider_uninit_single(1); // ANJ_SERVICE_PROVIDER_GB28181
		anj_service_provider_uninit_single(0); // ANJ_SERVICE_PROVIDER_RTSP
		anj_search_uninit();
		modules_uninit("anj_service", "anj_net");
		uninit_module_flag = 0;
	}
	anj_mw_system("echo 3 > /proc/sys/vm/drop_caches");
}

/***
 * @brief
 *  Initiates a firmware upgrade using the HTTP POST mechanism.
 *  The response to the command includes an HTTP URL to which the upgrade 
 *  file may be uploaded. The actual upgrade takes place as soon as the 
 *  HTTP POST operation has completed. The device should support firmware 
 *  upgrade through the StartFirmwareUpgrade command. The exact format of 
 *  the firmware data is outside the scope of this specification.
 *
 *  Firmware upgrade over HTTP may be achieved using the following steps:
 *  1. Client calls StartFirmwareUpgrade.
 *  2. Device service responds with upload URI and optional delay value.
 *  3. Client waits for delay duration if specified by server.
 *  4. Client transmits the firmware image to the upload URI using HTTP POST.
 *  5. Server reprograms itself using the uploaded image, then reboots
 *
 *  If the firmware upgrade fails because the upgrade file was invalid, 
 *  the HTTP POST response shall be "415 Unsupported Media Type". If the 
 *  firmware upgrade fails due to an error at the device, the HTTP POST 
 *  response shall be "500 Internal Server Error".
 *
 *  The value of the Content-Type header in the HTTP POST request shall be 
 *  "application/octet-stream".
 *
 *  After applying a firmware upgrade the device shall keep the basic network 
 *  configuration like IP address, subnet mask and gateway or DHCP settings 
 *  unchanged. Additionally a firmware upgrade shall not change user credentials.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *
 **/
ONVIF_RET onvif_tds_StartFirmwareUpgrade(HTTPCLN * p_user, tds_StartFirmwareUpgrade_RES * p_res)
{
	char sip[32];
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;
	anj_audio_prompt_play(ANJ_MP3_OTA_PATH, ANJ_MP3_DEVICE_START_UPDATE, 1);
	onvif_prepare_firmware_mem();
	onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);

	if (g_onvif_cfg.http_enable && !p_srv->https)
	{
		sprintf(p_res->UploadUri, "http://%s:%u/FirmwareUpgrade", sip, g_onvif_cls.http_port);
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		sprintf(p_res->UploadUri, "https://%s:%u/FirmwareUpgrade", sip, g_onvif_cls.https_port);
	}
#endif

	p_res->UploadDelay = 5;				// 5 seconds
	p_res->ExpectedDownTime = 3 * 60; 	// 5 minutes

	return ONVIF_OK;
}

/***
 * @brief
 *  Do some check before the upgrade.
 *
 * @param buff : pointer the upload content
 * @param len  : the upload content length
 *
 **/
BOOL onvif_tds_FirmwareUpgradeCheck(const char * buff, int len)
{
	log_print(HT_LOG_INFO, "onvif_tds_FirmwareUpgradeCheck\n");
	// Basic sanity checks to reject invalid payloads.
	if (buff == NULL || len < 1024 || len > (128 * 1024 * 1024))
	{
		return FALSE;
	}

	return TRUE;
}

static void *firmware_upgrade_thr(void *arg)
{
	pthread_detach(pthread_self());
    (void)arg;
    const char *fw_path = "/tmp/FirmwareUpgrade.bin";
    struct stat st;
    if (stat(fw_path, &st) != 0 || st.st_size <= 0)
    {
        log_print(HT_LOG_ERR, "firmware file %s not found or empty\n", fw_path);
        return NULL;
    }
    APPBIN_UPDATE_DATA updateData;
    memset(&updateData, 0, sizeof(updateData));
    snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", fw_path);
    updateData.nFileLen = (unsigned int)st.st_size;
    log_print(HT_LOG_INFO, "ONVIF firmware upgrade: %s size=%u\n", fw_path, updateData.nFileLen);

	anj_service_provider_uninit_single(3); // ANJ_SERVICE_PROVIDER_ONVIF
	anj_mw_system("echo 3 > /proc/sys/vm/drop_caches");
	int freemem = get_system_freemem();
	log_print(HT_LOG_INFO, "firmware mem check: free=%d kB, firmware size=%u kB\n", 
		freemem, updateData.nFileLen / 1024);

    anj_sysmng_app_update(&updateData);

	pthread_exit(NULL);	
    return NULL;
}

/***
 * @brief
 *  Begin firmware upgrade
 *
 * @param buff : pointer the upload content
 * @param len  : the upload content length
 **/
BOOL onvif_tds_FirmwareUpgrade(const char * buff, int len)
{
	log_print(HT_LOG_INFO, "onvif_tds_FirmwareUpgrade\n");

	if (buff == NULL || len <= 0)
	{
		log_print(HT_LOG_INFO, "invalid firmware upgrade payload\n");
		return FALSE;
	}

	log_print(HT_LOG_INFO, "firmware payload size is %d Byte\n", len);

	// todo : add the upgrade code ...
	char file_path[128] = {"/tmp/FirmwareUpgrade.bin"};
	if (access(file_path, F_OK) == 0)
	{
		remove(file_path);
	}
	int ret = write_buffer_to_file(file_path, buff, len);
	if (ret != len)
	{
		log_print(HT_LOG_ERR, "write firmware upgrade file failed \n");
		return FALSE;
	}
	else
	{
		/* Firmware upload payload is stored successfully. */
		log_print(HT_LOG_INFO, "recv (POST /OnvifFirmwareUpgrade) succeed\n");		
		// Trigger async upgrade sequence.
		log_print(HT_LOG_INFO, "start firmware upgrade thread\n");
		pthread_t msg_thread;
		if(pthread_create(&msg_thread, NULL, firmware_upgrade_thr, NULL) != 0)
		{
			log_print(HT_LOG_ERR, "Error: firmware_upgrade_thr thread create failed\n");
			return FALSE;
		}
	}
	
	return TRUE;
}

/***
 * @brief
 *  After the upgrade is complete do some works, such as reboot device ...
 *  
 **/
void onvif_tds_FirmwareUpgradePost()
{
	log_print(HT_LOG_INFO, "onvif_tds_FirmwareUpgradePost\n");
	
}

/***
 * @brief
 *  Initiates a system restore from backed up configuration data using 
 *  the HTTP POST mechanism.
 *
 *  The response to the command includes an HTTP URL to which the backup 
 *  file may be uploaded. The actual restore takes place as soon as the 
 *  HTTP POST operation has completed. The exact format of the backup 
 *  configuration data is outside the scope of this specification. 
 *  
 *  System restore over HTTP may be achieved using the following steps:
 *  1. Client calls StartSystemRestore.
 *  2. Device service responds with upload URI.
 *  3. Client transmits the configuration data to the upload URI using HTTP POST.
 *  4. Server applies the uploaded configuration, then reboots if necessary
 *
 *  If the system restore fails because the uploaded file was invalid, the HTTP POST 
 *  response shall be "415 Unsupported Media Type". If the system restore fails due 
 *  to an error at the device, the HTTP POST response shall be "500 Internal Server Error".
 *  The value of the Content-Type header in the HTTP POST request shall be 
 *  "Application/octet-stream".
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tds_StartSystemRestore(HTTPCLN * p_user, tds_StartSystemRestore_RES * p_res)
{
	char sip[32];
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;

	// todo : do some file upload prepare ...

	onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);

	if (g_onvif_cfg.http_enable && !p_srv->https)
	{
		sprintf(p_res->UploadUri, "http://%s:%u/SystemRestore", sip, g_onvif_cls.http_port);
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		sprintf(p_res->UploadUri, "https://%s:%u/SystemRestore", sip, g_onvif_cls.https_port);
	}
#endif

	p_res->ExpectedDownTime = 5 * 60; 	// 5 minutes

	return ONVIF_OK;
}

/***
 * @brief
 * Do some check before the restore.
 *
 * @param buff : pointer the upload content
 * @param len  : the upload content length
 *
 **/
BOOL onvif_tds_SystemRestoreCheck(const char * buff, int len)
{
	if (NULL == buff || len < 32 || len > (8 * 1024 * 1024))
	{
	    return FALSE;
	}
	
	return TRUE;
}

/***
 * @brief
 *  Begin system restore.
 *
 * @param buff : pointer the upload content
 * @param len  : the upload content length
 *
 **/
BOOL onvif_tds_SystemRestore(const char * buff, int len)
{
	if (!onvif_tds_SystemRestoreCheck(buff, len))
	{
		return FALSE;
	}

	if (write_buffer_to_file("/tmp/SystemRestore.bin", buff, len) != len)
	{
		return FALSE;
	}

    __RECORD_LOG_INFO("restore config reserved_bits=0\n"); 
	anj_sysmng_config_restore(0);

	return TRUE;
}

/***
 * @brief
 *  After the system restore is complete do some works, such as reboot device ...
 *  
 **/
void onvif_tds_SystemRestorePost()
{
    // todo : please comment the code below
    // send onvif hello message, just for test
    sleep(3);
    onvif_hello();
}

/**
 * @brief
 *  How to set HashingAlgorithm for an ONVIF Device/Client:
 *  ONVIF client should use SetHashingAlgorithm API to modify the current 
 *  hashing algorithm of a device.
 *  SetHashingAlgorithm API sets the hashing algorithm(s) to be used in 
 *  HTTP and RTSP Digest Authentication.
 *
 *  After changing the hashing algorithm of an ONVIF device, the device 
 *  should use the new hashing algorithm in the digest challenge for the 
 *  upcoming HTTP and RTSP request.
 *  
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidArgVal
 **/
ONVIF_RET onvif_tds_SetHashingAlgorithm(tds_SetHashingAlgorithm_REQ * p_req)
{
    if (strstr(p_req->Algorithm, "MD5"))
    {
        g_onvif_cfg.md5_hashing = 1;
    }
    else
    {
        g_onvif_cfg.md5_hashing = 0;
    }
    
    if (strstr(p_req->Algorithm, "SHA-256"))
    {
        g_onvif_cfg.sha256_hashing = 1;
    }
    else
    {
        g_onvif_cfg.sha256_hashing = 0;
    }

    if (!g_onvif_cfg.md5_hashing && !g_onvif_cfg.sha256_hashing)
    {
        g_onvif_cfg.md5_hashing = 1;
    }
    
    snprintf(g_onvif_cfg.Capabilities.device.HashingAlgorithms,
        sizeof(g_onvif_cfg.Capabilities.device.HashingAlgorithms),
        "%.*s",
        (int)(sizeof(g_onvif_cfg.Capabilities.device.HashingAlgorithms) - 1),
        p_req->Algorithm);
    
    return ONVIF_OK;
}

#ifdef IPFILTER_SUPPORT	

/**
 * @brief
 *  Sets the IP address filter settings on a device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 **/
ONVIF_RET onvif_tds_SetIPAddressFilter(tds_SetIPAddressFilter_REQ * p_req)
{
	memcpy(&g_onvif_cfg.ipaddr_filter, &p_req->IPAddressFilter, sizeof(onvif_IPAddressFilter));

	// todo : here add handler code ...
	
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Adds an IP filter address to a device.
 *
 *  The value of the Type field shall be ignored by the device. 
 *  Use SetIPAddressFilter to set the type.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 *  ONVIF_ERR_IPFilterListIsFull
 **/
ONVIF_RET onvif_tds_AddIPAddressFilter(tds_AddIPAddressFilter_REQ * p_req)
{
    uint32 i;
    onvif_PrefixedIPAddress * p_item;
    
	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv4Address); i++)
	{
		if (p_req->IPAddressFilter.IPv4Address[i].Address[0] == '\0')
		{
			break;
		}

		if (onvif_is_ipaddr_filter_exist(g_onvif_cfg.ipaddr_filter.IPv4Address, 
		        ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address), 
		        &p_req->IPAddressFilter.IPv4Address[i]))
		{
			continue;
		}
		
		p_item = onvif_get_idle_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv4Address, 
		            ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address));
		if (p_item)
		{
			p_item->PrefixLength = p_req->IPAddressFilter.IPv4Address[i].PrefixLength;
			strcpy(p_item->Address, p_req->IPAddressFilter.IPv4Address[i].Address);
		}
		else
		{
			return ONVIF_ERR_IPFilterListIsFull;
		}
	}

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv6Address); i++)
	{	    
		if (p_req->IPAddressFilter.IPv6Address[i].Address[0] == '\0')
		{
			break;
		}

		if (onvif_is_ipaddr_filter_exist(g_onvif_cfg.ipaddr_filter.IPv6Address, 
		        ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address), 
		        &p_req->IPAddressFilter.IPv6Address[i]))
		{
			continue;
		}
		
		p_item = onvif_get_idle_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv6Address, 
		            ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address));
		if (p_item)
		{
			p_item->PrefixLength = p_req->IPAddressFilter.IPv6Address[i].PrefixLength;
			strcpy(p_item->Address, p_req->IPAddressFilter.IPv6Address[i].Address);
		}
		else
		{
			return ONVIF_ERR_IPFilterListIsFull;
		}
	}

	// todo : here add handler code ...

	
	return ONVIF_OK;
}

/**
 * @brief
 *  Deletes an IP filter address from a device.
 *
 *  The value of the Type field shall be ignored by the device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidIPv4Address
 *  ONVIF_ERR_NoIPv4Address
 **/
ONVIF_RET onvif_tds_RemoveIPAddressFilter(tds_RemoveIPAddressFilter_REQ * p_req)
{
    uint32 i;
	onvif_PrefixedIPAddress * p_item = NULL;
	
	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv4Address); i++)
	{
		if (p_req->IPAddressFilter.IPv4Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv4Address, 
		            ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address), 
		            &p_req->IPAddressFilter.IPv4Address[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_NoIPv4Address;
		}
	}

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv6Address); i++)
	{
		if (p_req->IPAddressFilter.IPv6Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv6Address, 
		            ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address), 
		            &p_req->IPAddressFilter.IPv6Address[i]);
		if (NULL == p_item)
		{
			return ONVIF_ERR_NoIPv6Address;
		}
	}

	// todo : here add handler code ...
	

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv4Address); i++)
	{
		if (p_req->IPAddressFilter.IPv4Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv4Address, 
		            ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address), 
		            &p_req->IPAddressFilter.IPv4Address[i]);
		if (NULL != p_item)
		{
			memset(p_item, 0, sizeof(onvif_PrefixedIPAddress));
		}
	}

	for (i = 0; i < ARRAY_SIZE(p_req->IPAddressFilter.IPv6Address); i++)
	{
		if (p_req->IPAddressFilter.IPv6Address[i].Address[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_ipaddr_filter(g_onvif_cfg.ipaddr_filter.IPv6Address, 
		            ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv6Address), 
		            &p_req->IPAddressFilter.IPv6Address[i]);
		if (NULL != p_item)
		{
			memset(p_item, 0, sizeof(onvif_PrefixedIPAddress));
		}
	}
    
	return ONVIF_OK;
}

/**
 * @brief
 *  New HTTP connection callback
 *
 * @param p_srv http server pointer (HTTPSRV *)
 * @param addr the connection remote addr
 * @param port the connection remote port
 * @parma userdata user data
 *
 * @return
 *  TRUE, accept new connection
 *  FALSE, deny new connection
 *
 **/
BOOL onvif_http_conn_cb(void * p_srv, uint32 addr, int port, void * userdata)
{
	uint32 i;

	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.ipaddr_filter.IPv4Address); i++)
	{
		if (g_onvif_cfg.ipaddr_filter.IPv4Address[i].Address[0] == '\0')
		{
			continue;
		}

		uint32 ipaddr = get_address_by_name(g_onvif_cfg.ipaddr_filter.IPv4Address[i].Address);
		uint32 mask = inet_addr(get_mask_by_prefix_len(g_onvif_cfg.ipaddr_filter.IPv4Address[i].PrefixLength));

		if ((ipaddr & mask) == (addr & mask))
		{
			if (IPAddressFilterType_Deny == g_onvif_cfg.ipaddr_filter.Type)
			{
				return FALSE;
			}
			else
			{
				return TRUE;
			}
		}
	}

	return TRUE;
}

#endif // end of IPFILTER_SUPPORT

#ifdef STORAGE_SUPPORT

/**
 * @brief
 *  Creates a new storage configuration. The configuration data shall be created
 *  in the device and shall be persistent (remains after a device reboots). 
 *  A device indicating storage configuration capability shall support the 
 *  creation of storage configurations as long as the number of existing storage
 *  configurations does not exceed the value of MaxStorageConfigurations capability.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_MaxStorageConfigurations
 **/
ONVIF_RET onvif_tds_CreateStorageConfiguration(tds_CreateStorageConfiguration_REQ * p_req, tds_CreateStorageConfiguration_RES * p_res)
{
    StorageConfigurationList * p_storage = onvif_add_StorageConfiguration(&g_onvif_cfg.storage);
    if (p_storage)
    {
        memcpy(&p_storage->Configuration.Data, &p_req->StorageConfiguration, sizeof(onvif_StorageConfigurationData));

        strcpy(p_res->Token, p_storage->Configuration.token);
    }

    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies an existing storage configuration.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_ConfigModify
 **/
ONVIF_RET onvif_tds_SetStorageConfiguration(tds_SetStorageConfiguration_REQ * p_req)
{
    StorageConfigurationList * p_storage = onvif_find_StorageConfiguration(g_onvif_cfg.storage, p_req->StorageConfiguration.token);
    if (NULL == p_storage)
    {
        return ONVIF_ERR_NoConfig;
    }

    memcpy(&p_storage->Configuration.Data, &p_req->StorageConfiguration.Data, sizeof(onvif_StorageConfigurationData));

    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes a storage configuration.
 *  
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoConfig
 **/
ONVIF_RET onvif_tds_DeleteStorageConfiguration(tds_DeleteStorageConfiguration_REQ * p_req)
{
    StorageConfigurationList * p_storage = onvif_find_StorageConfiguration(g_onvif_cfg.storage, p_req->Token);
    if (NULL == p_storage)
    {
        return ONVIF_ERR_NoConfig;
    }

    // todo : here add handler code ...
    
    onvif_free_StorageConfiguration(&g_onvif_cfg.storage, p_storage);

    return ONVIF_OK;
}

#endif // STORAGE_SUPPORT

#ifdef GEOLOCATION_SUPPORT

/**
 * @brief
 *  Modify one or more geo location entries.
 *  A device that signals support for GeoLocation via the GeoLocationEntities 
 *  capabiliy shall support modifying geo location information via this command.
 *
 *  The method allows to update one or more entries at once. The method shall 
 *  modify only those entries that are referenced by the request arguments. 
 *  A device shall create a new entry in case the combination of type
 *  and token does not yet exist. A device shall remove any of the location and 
 *  orientations components in case they are not passed in the request.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_TooManyEntries
 *  ONVIF_ERR_NoAutoGeo
 **/
ONVIF_RET onvif_tds_SetGeoLocation(tds_SetGeoLocation_REQ * p_req)
{
    int count = 0;
    
    count = onvif_get_LocationEntity_nums(p_req->Location);
    if (count > g_onvif_cfg.Capabilities.device.GeoLocationEntries)
    {
        return ONVIF_ERR_TooManyEntries;
    }
    
    onvif_free_LocationEntitis(&g_onvif_cfg.location);

    g_onvif_cfg.location = p_req->Location;

    p_req->Location = NULL;

    return ONVIF_OK;
}

/**
 * @brief
 *  Remove one or more geo location entries.
 *
 *  A device shall delete an entity based on the passed fields type and token.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_Fixed
 **/
ONVIF_RET onvif_tds_DeleteGeoLocation(tds_DeleteGeoLocation_REQ * p_req)
{
    LocationEntityList * p_location;
    LocationEntityList * p_item = p_req->Location;

    while (p_item)
    {
        p_location = onvif_find_LocationEntity(g_onvif_cfg.location, p_item->Location.Entity, p_item->Location.Token);
        if (p_location)
        {
            if (p_location->Location.Fixed)
            {
                return ONVIF_ERR_Fixed;
            }
        }
        else
        {
            return ONVIF_ERR_NoConfig;
        }
        
        p_item = p_item->next;
    }

    p_item = p_req->Location;
    
    while (p_item)
    {
        p_location = onvif_find_LocationEntity(g_onvif_cfg.location, p_item->Location.Entity, p_item->Location.Token);
        if (p_location)
        {
            onvif_free_LocationEntity(&g_onvif_cfg.location, p_location);
        }
        
        p_item = p_item->next;
    }
    
    return ONVIF_OK;
}

#endif // GEOLOCATION_SUPPORT

#ifdef DOT11_SUPPORT

/**
 * @brief
 *  Returns the status of a wireless network interface.
 *
 *  The following status can be returned:
 *  SSID (shall)
 *  BSSID (should)
 *  Pair cipher (should)
 *  Group cipher (should)
 *  Signal strength (should)
 *  Alias of active wireless configuration (shall)
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 *  ONVIF_ERR_InvalidDot11
 *  ONVIF_ERR_NotDot11
 *  ONVIF_ERR_NotConnectedDot11
 **/
ONVIF_RET onvif_tds_GetDot11Status(tds_GetDot11Status_REQ * p_req, tds_GetDot11Status_RES * p_res)
{
	NetworkInterfaceList * p_net_inf = onvif_find_NetworkInterface(g_onvif_cfg.network.interfaces, p_req->InterfaceToken);
	if (NULL == p_net_inf)
	{
		return ONVIF_ERR_InvalidNetworkInterface;
	}

	// todo : here add handler code ...


	return ONVIF_OK;
}

/**
 * @brief
 *  Returns a lists of the wireless networks in range of the device.
 *
 *  The following status can be returned for each network:
 *  SSID (shall)
 *  BSSID (should)
 *  Authentication and key management suite(s) (should)
 *  Pair cipher(s) (should)
 *  Group cipher(s) (should)
 *  Signal strength (should)
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidNetworkInterface
 *  ONVIF_ERR_InvalidDot11
 *  ONVIF_ERR_NotDot11
 *  ONVIF_ERR_NotScanAvailable
 **/
ONVIF_RET onvif_tds_ScanAvailableDot11Networks(tds_ScanAvailableDot11Networks_REQ * p_req, tds_ScanAvailableDot11Networks_RES * p_res)
{
    NetworkInterfaceList * p_net_inf = onvif_find_NetworkInterface(g_onvif_cfg.network.interfaces, p_req->InterfaceToken);
    if (NULL == p_net_inf)
    {
        return ONVIF_ERR_InvalidNetworkInterface;
    }

    if (g_onvif_cfg.Capabilities.dot11.ScanAvailableNetworks == 0)
    {
        return ONVIF_ERR_NotScanAvailable;
    }

    // todo : here add handler code ...

    
    return ONVIF_OK;
}

#endif // DOT11_SUPPORT



