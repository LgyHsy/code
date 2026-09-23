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
#include "onvif_utils.h"
#include "onvif.h"

/**
 * @brief Get UTC Standard Time String
 * 
 * @param buff output buffer, cannot be null
 * @param len buff lenght
 * @param sec_off second offset, add a specified second to the current time
 *
 * @return return buff param
 */
char * onvif_get_time_str(char * buff, int len, int sec_off)
{
	time_t t;
	struct tm *gtime;	

	time(&t);
	t += sec_off;           // plus second offset
	gtime = gmtime(&t);     // UTC

	snprintf(buff, len, "%04d-%02d-%02dT%02d:%02d:%02dZ", 		 
		gtime->tm_year+1900, gtime->tm_mon+1, gtime->tm_mday,
		gtime->tm_hour, gtime->tm_min, gtime->tm_sec);		

    return buff;
}

/**
 * @brief Get the UTC standard string for the specified time_t time
 * 
 * @param buff output buffer, cannot be null
 * @param len buff lenght
 * @param t time_t time
 * @param sec_off second offset, add a specified second to the time_t time
 *
 * @return return buff param
 */
char * onvif_get_time_str_s(char * buff, int len, time_t t, int sec_off)
{
	struct tm *gtime;	

	t += sec_off;           // plus second offset
	gtime = gmtime(&t);     // UTC

	snprintf(buff, len, "%04d-%02d-%02dT%02d:%02d:%02dZ", 		 
		gtime->tm_year+1900, gtime->tm_mon+1, gtime->tm_mday,
		gtime->tm_hour, gtime->tm_min, gtime->tm_sec);		

    return buff;
}

BOOL onvif_is_valid_hostname(const char * name)
{
	// 0-9, a-z, A-Z, '-'

	const char * p = name;
	while (*p != '\0')
	{
		if ((*p >= '0' && *p <= '9') || 
		    (*p >= 'a' && *p <= 'z') || 
		    (*p >= 'A' && *p <= 'Z') || 
		    (*p == '-') || 
		    (*p == '.'))
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

void onvif_get_timezone(char * tz, int len, BOOL * daylight)
{
	int time_hour, time_min;
	time_t time_utc;
	struct tm tm_local;
	struct tm tm_utc;
		
	time(&time_utc);

#if __WINDOWS_OS__
	localtime_s(&tm_local, &time_utc);
	gmtime_s(&tm_utc, &time_utc);
#else
	localtime_r(&time_utc, &tm_local);
	gmtime_r(&time_utc, &tm_utc);
#endif

	time_hour = tm_local.tm_hour - tm_utc.tm_hour;
	time_min = tm_local.tm_min - tm_utc.tm_min;

	if (difftime(mktime(&tm_local), mktime(&tm_utc)) < 0)
	{
		if (time_hour > 0)
		{
			time_hour -= 24;
		}

		if (time_min < 0)
		{
			time_min += 60;
		}
	}
	else if (time_min < 0)
	{
		time_min += 60;
		time_hour -= 1;
	}

	if (tm_utc.tm_isdst > 0)
	{
		time_hour -= 1;
	}

	if (time_hour > 0)
	{
		snprintf(tz, len, "&lt;UTC+%02d:%02d&gt;%d", time_hour, time_min, -time_hour);
	}
	else
	{
		snprintf(tz, len, "&lt;UTC%02d:%02d&gt;%d", time_hour, time_min, -time_hour);
	}

	if (daylight)
	{
		*daylight = tm_utc.tm_isdst > 0 ? TRUE : FALSE;
	}
}

/**
 * stdoffset[dst[offset],[start[/time],end[/time]]]
 */
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

char * onvif_uuid_create(char * uuid, int len)
{
	static char suuid[100];
	static int seed = 1;

	srand((uint32)time(NULL) + seed++);
	
	if (NULL == uuid || 0 == len)
	{
		uuid = suuid;
		len = sizeof(suuid);
	}
	
	snprintf(uuid, len, "%04x%04x-%04x-%04x-%04x-%04x%04x%04x", 
		rand()%0xFFFF, rand()%0xFFFF, rand()%0xFFFF, rand()%0xFFFF, 
		rand()%0xFFFF, rand()%0xFFFF, rand()%0xFFFF, rand()%0xFFFF);

	return uuid;
}

time_t onvif_timegm(struct tm *T)
{
	time_t t, g, z;
	struct tm tm;
	
	t = mktime(T);	
	if (t == (time_t)-1)
	{
		return (time_t)-1;
	}
	
	tm = *gmtime(&t);

	tm.tm_isdst = 0;
	g = mktime(&tm);
	if (g == (time_t)-1)
	{
		return (time_t)-1;
	}
	
	z = g - t;
	return t - z;
}

/**
 * http://192.168.5.235/onvif/device_service
 */
int onvif_parse_xaddr_(const char * pdata, char * host, int hostsize, char * url, int urlsize, int * port, int * https)
{
    int len = (int)strlen(pdata);
    int prefixlen = 0;

    *port = 80;
    
    if (len > 7) // skip "http://"
    {
    	if (strncmp(pdata, "https://", 8) == 0)
    	{
    		prefixlen = 8;
    		*https = 1;
    	}
    	else
    	{
    		prefixlen = 7;
    		*https = 0;
    	}

		char const* from = &pdata[prefixlen];
		
        // Next, parse <server-address-or-name>
		char* to = host;
		int i;
		for (i = 0; i < hostsize; ++i) 
		{
			if (*from == '\0' || *from == ':' || *from == '/') 
			{
				// We've completed parsing the address
				*to = '\0';
				break;
			}
			*to++ = *from++;
		}
		if (i == hostsize) 
		{
			log_print(HT_LOG_ERR, "%s, URL is too long\r\n", __FUNCTION__);
			return 0;
		}

		char next = *from;
		if (next == ':') 
		{
			int portNumInt;
			if (sscanf(++from, "%d", &portNumInt) != 1) 
			{
				log_print(HT_LOG_ERR, "%s, No port number follows ':'\r\n", __FUNCTION__);
				return 0;
			}
			
			if (portNumInt < 1 || portNumInt > 65535) 
			{
				log_print(HT_LOG_ERR, "%s, Bad port number\r\n", __FUNCTION__);
				return 0;
			}
			
			*port = portNumInt;
			while (*from >= '0' && *from <= '9') ++from; // skip over port number
		}

		// The remainder of the URL is the suffix:
		strncpy(url, from, urlsize-1);
    }

    return 1;
}

/**
 * http://192.168.5.235/onvif/device_service http://[fe80::c256:e3ff:fea2:e019]/onvif/device_service 
 */
int onvif_parse_xaddr(const char * pdata, char * host, int hostsize, char * url, int urlsize, int * port, int * https)
{
    int ret;
	uint32 size;
    const char *p1, *p2;
    char xaddr[256];

    p2 = pdata;
    p1 = strchr(pdata, ' ');
    while (p1)
    {
    	size = p1 - p2;
    	size = size >= sizeof(xaddr) ? sizeof(xaddr)-1 : size;
        strncpy(xaddr, p2, size);
        xaddr[size] = '\0';

        ret = onvif_parse_xaddr_(xaddr, host, hostsize, url, urlsize, port, https);
        if (ret && is_ip_address(host) && strncmp(host, "169.", 4)) // if ipv4 address
        {
            return 1;
        }

        p2 = p1+1;
        p1 = strchr(p2, ' ');
    }

	if (p2)
	{
		return onvif_parse_xaddr_(p2, host, hostsize, url, urlsize, port, https);	
	}

    return 1;
}

/***************************************************************************************/

time_t onvif_datetime_to_time_t(onvif_DateTime * p_datetime)
{
    struct tm t1;

    t1.tm_year = p_datetime->Date.Year - 1900;
    t1.tm_mon = p_datetime->Date.Month - 1;
    t1.tm_mday = p_datetime->Date.Day;
    t1.tm_hour = p_datetime->Time.Hour;
    t1.tm_min = p_datetime->Time.Minute;
    t1.tm_sec = p_datetime->Time.Second;

    return mktime(&t1);
}

void onvif_time_t_to_datetime(time_t n, onvif_DateTime * p_datetime)
{
    struct tm *t1 = gmtime(&n);

	if (t1)
	{
	    p_datetime->Date.Year = t1->tm_year + 1900;
	    p_datetime->Date.Month = t1->tm_mon + 1;
	    p_datetime->Date.Day = t1->tm_mday;
	    p_datetime->Time.Hour = t1->tm_hour;
	    p_datetime->Time.Minute = t1->tm_min;
	    p_datetime->Time.Second = t1->tm_sec;
    }
}

/**
 * @brief Format time_t time in the specified format
 *
 * @param n time_t time
 * @param flag 0-local,1-UTC
 * @param format format string, such as "%Y-%m-%dT%H:%M:%SZ"
 * @param buff Receive formatted string buffer, if NULL, use static buffer within the function
 * @param buflen buffer length
 *
 * @return If the parameter buff is not NULL, return buff; 
 *         if buff is NULL, return static buffer within the function
 *
 */
char * onvif_format_datetime_str(time_t n, int flag, const char * format, char * buff, int buflen)
{
    struct tm *t1;
  	static char sbuff[100];

	if (NULL == buff || 0 == buflen)
	{
		buff = sbuff;
		buflen = sizeof(sbuff);
		
		memset(sbuff, 0, buflen);
	}

    if (flag == 1)
    {
  	    t1 = gmtime(&n);
  	}
  	else
  	{
  	    t1 = localtime(&n);
  	}

  	if (t1)
  	{
		strftime(buff, buflen, format, t1);
	}
	
	return buff;
}

int onvif_parse_uri(const char * p_in, char * p_out, int outlen)
{
    int inlen = (int)strlen(p_in);
    char *p;

    if (inlen < 8 || outlen < inlen)
    {
        return -1;
    }

    if (p_out != p_in) // prevent copy to self
    {
	    strcpy(p_out, p_in);
	}

    // replace "&amp;" with "&"
	p = strstr(p_out, "&amp;");
	while (p)
	{
		memmove(p+1, p+5, strlen(p+5));
		p = strstr(p+5, "&amp;");
		p_out[strlen(p_out)-4] = '\0';
	}  

	return 0;
}




