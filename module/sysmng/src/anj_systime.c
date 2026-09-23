#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_file.h"
#include "anj_mw_crypt.h"
#include "anj_mw_time.h"
#include "anj_mw_rtc.h"
#include "anj_mw_thread.h"

#include "anj_config.h"
#include "anj_systime.h"
#include "anj_sysmng.h"
#include "ntp_client.h"

#include <stdio.h>
#include <errno.h>
#include <dirent.h>

#define TIME_ZONE_FILE "/mnt/nand/timezone"

typedef struct
{
	int zone;
	const char *localtimeLnk; // for date command
} TIME_ZONE_ITEM;

static const TIME_ZONE_ITEM tzItems[] =
	{
		{0, "/usr/share/zoneinfo/Etc/GMT+12"},
		{30, "/usr/share/zoneinfo/Etc/GMT+11.30"},
		{60, "/usr/share/zoneinfo/Etc/GMT+11"},
		{90, "/usr/share/zoneinfo/Etc/GMT+10.30"},
		{120, "/usr/share/zoneinfo/Etc/GMT+10"},
		{150, "/usr/share/zoneinfo/Etc/GMT+9.30"},
		{180, "/usr/share/zoneinfo/Etc/GMT+9"},
		{210, "/usr/share/zoneinfo/Etc/GMT+8.30"},
		{240, "/usr/share/zoneinfo/Etc/GMT+8"},
		{270, "/usr/share/zoneinfo/Etc/GMT+7.30"},
		{300, "/usr/share/zoneinfo/Etc/GMT+7"},
		{330, "/usr/share/zoneinfo/Etc/GMT+6.30"},
		{360, "/usr/share/zoneinfo/Etc/GMT+6"},
		{390, "/usr/share/zoneinfo/Etc/GMT+5.30"},
		{420, "/usr/share/zoneinfo/Etc/GMT+5"},
		{450, "/usr/share/zoneinfo/Etc/GMT+4.30"},
		{480, "/usr/share/zoneinfo/Etc/GMT+4"},
		{510, "/usr/share/zoneinfo/Etc/GMT+3.30"},
		{540, "/usr/share/zoneinfo/Etc/GMT+3"},
		{570, "/usr/share/zoneinfo/Etc/GMT+2.30"},
		{600, "/usr/share/zoneinfo/Etc/GMT+2"},
		{630, "/usr/share/zoneinfo/Etc/GMT+1.30"},
		{660, "/usr/share/zoneinfo/Etc/GMT+1"},
		{690, "/usr/share/zoneinfo/Etc/GMT+0.30"},
		{720, "/usr/share/zoneinfo/Etc/GMT"},
		{750, "/usr/share/zoneinfo/Etc/GMT-0.30"},
		{780, "/usr/share/zoneinfo/Etc/GMT-1"},
		{810, "/usr/share/zoneinfo/Etc/GMT-1.30"},
		{840, "/usr/share/zoneinfo/Etc/GMT-2"},
		{870, "/usr/share/zoneinfo/Etc/GMT-2.30"},
		{900, "/usr/share/zoneinfo/Etc/GMT-3"},
		{930, "/usr/share/zoneinfo/Etc/GMT-3.30"},
		{960, "/usr/share/zoneinfo/Etc/GMT-4"},
		{990, "/usr/share/zoneinfo/Etc/GMT-4.30"},
		{1020, "/usr/share/zoneinfo/Etc/GMT-5"},
		{1050, "/usr/share/zoneinfo/Etc/GMT-5.30"},
		{1080, "/usr/share/zoneinfo/Etc/GMT-6"},
		{1110, "/usr/share/zoneinfo/Etc/GMT-6.30"},
		{1140, "/usr/share/zoneinfo/Etc/GMT-7"},
		{1170, "/usr/share/zoneinfo/Etc/GMT-7.30"},
		{1200, "/usr/share/zoneinfo/Etc/GMT-8"},
		{1230, "/usr/share/zoneinfo/Etc/GMT-8.30"},
		{1260, "/usr/share/zoneinfo/Etc/GMT-9"},
		{1290, "/usr/share/zoneinfo/Etc/GMT-9.30"},
		{1320, "/usr/share/zoneinfo/Etc/GMT-10"},
		{1350, "/usr/share/zoneinfo/Etc/GMT-10.30"},
		{1380, "/usr/share/zoneinfo/Etc/GMT-11"},
		{1410, "/usr/share/zoneinfo/Etc/GMT-11.30"},
		{1440, "/usr/share/zoneinfo/Etc/GMT-12"},
		{1470, "/usr/share/zoneinfo/Etc/GMT-12.30"},
		{1485, "/usr/share/zoneinfo/Etc/GMT-12.45"},
		{1500, "/usr/share/zoneinfo/Etc/GMT-13"},
};

static int system_tz = -1;

static unsigned int s_bNtpUpdateOk = 0;
static anj_thread_s s_stNtpUpdateThread;

// 得到某年月第几周第几天的秒数
static int anj_systime_sec_from_weekly(int year, int month, int weekno, int weekday)
{

	struct tm tbuf;
	struct tm first_day; // 这个月第一天

	memset(&tbuf, 0, sizeof(struct tm));
	memset(&first_day, 0, sizeof(struct tm));

	first_day.tm_year = year - 1900;
	first_day.tm_mon = month - 1;
	first_day.tm_mday = 1;

	int timesecond_firstday = mktime(&first_day); // 算出来第一天的时间秒,用于计算第几周星期几的具体日期
	if (timesecond_firstday == -1)
	{
		__INFO("get firstday timesecond err!\n");
		return 0;
	}
	memcpy(&first_day, localtime_r((time_t *)&timesecond_firstday, &tbuf), sizeof(struct tm));
	__INFO("first_day.tm_year:%d\n", first_day.tm_year);
	int a = (7 - first_day.tm_wday) % 7; // 计算第一周如果不完整的话有几天
	// 第一天是周日,则是完整的一周。
	// 否则第一周只有a天，例如第一天是周六，则第一周只有1天
	// 第一周不完整，如果weekday小于1号的星期几，就需要对周数加1
	// weekday=0表示周日，其他表示周一-周六
	if (a > 0)
	{
		if (weekday < first_day.tm_wday)
		{
			weekno += 1;
			// CHAM 20211104改成第几个星期几，而不是第几周的星期几
			__INFO("%04d-%02d the first week from weekday %d, set weekday %d to weekno %d \n",
				   year, month, first_day.tm_wday, weekday, weekno);
			//			weekday = first_day.tm_wday;
		}
	}

	int tm_mday = 0;
	tm_mday = (weekno - 1) * 7 + (weekday - first_day.tm_wday + 1);
	int nMaxMday;
	switch (month)
	{
	case 1:
	case 3:
	case 5:
	case 7:
	case 8:
	case 10:
	case 12:
		nMaxMday = 31;
		break;

	case 4:
	case 6:
	case 9:
	case 11:
		nMaxMday = 30;
		break;

	case 2:
	{
		if (isleapyear(year))
			nMaxMday = 29;
		else
			nMaxMday = 28;
	}
	break;

	default:
		nMaxMday = 30;
	}

	if (tm_mday > nMaxMday)
	{
		tm_mday = nMaxMday;
	}

	first_day.tm_mday = tm_mday;

	return mktime(&first_day);
}

static int anj_systime_is_dst(time_t now, int timezone_cfg, const SummerTimeConfig *pSummer)
{
	if (pSummer->nEnable != 1)
		return 0;

	int bOffseted = 0;
	int timezone_system = anj_systime_get_zone_by_system();
	if (timezone_system == timezone_cfg)
	{
		__INFO("timezone_system=%d same with setting.\n", timezone_system);
	}
	else
	{
		int diff = timezone_system - timezone_cfg;
		if (diff == pSummer->nOffsetMin)
		{
			__INFO("timezone_system=%d already offset with setting %d.\n", timezone_system, pSummer->nOffsetMin);
			bOffseted = 1;
		}
		else
		{
			__INFO("timezone_system=%d, timezone_cfg=%d, offset %d error, should be setting %d.\n",
				   timezone_system, timezone_cfg, diff, pSummer->nOffsetMin);
		}
	}

	struct tm *ptm, tbuf;
	ptm = localtime_r(&now, &tbuf);

	int year = 1900 + ptm->tm_year;
	int month = 1 + ptm->tm_mon;
	int day = ptm->tm_mday;
	int hour = ptm->tm_hour;
	int minute = ptm->tm_min;
	int second = ptm->tm_sec;

	// 计算今年的起始和终止时
	int nFromSec = anj_systime_sec_from_weekly(year, pSummer->nStartMonth, pSummer->nStartWeek, pSummer->nStartWeekday);
	int nToSec = anj_systime_sec_from_weekly(year, pSummer->nToMonth, pSummer->nToWeek, pSummer->nToWeekday);
	if (nFromSec == 0 || nToSec == 0)
	{
		__INFO("nFromSec=%d nToSec=%d\n", nFromSec, nToSec);
		return 0;
	}

	__INFO("nFromSec %lld, %s\n", (long long)nFromSec);
	__INFO("nToSec %lld, %s\n", (long long)nToSec);
	nFromSec += pSummer->nStartHour * 3600;
	nToSec += pSummer->nToHour * 3600;

	if (bOffseted)
	{
		// 已经偏移的，根据夏令时配置(基于本地时间的年月日时)得到的起止秒，要比没偏移的时候要少偏移量
		// 也就是说，得到的这个起止秒，不管有没有将夏令时偏移时区，数字应该都应该一样
		nFromSec += pSummer->nOffsetMin * 60;
		nToSec += pSummer->nOffsetMin * 60;
		__INFO("bOffseted nFromSec %lld, %s\n", (long long)nFromSec);
		__INFO("bOffseted nToSec %lld, %s\n", (long long)nToSec);
	}

	int ret = 0;
	if (nFromSec < nToSec) // 起始配置早于终止配置，判断当前时间在这个区间中就为夏令时
	{
		if (now < nToSec && now > nFromSec)
		{
			__INFO("nowtime %lld, %04d-%02d-%02d %02d:%02d:%02d is in summer time between %d~%d",
				   (long long)now, year, month, day, hour, minute, second, nFromSec, nToSec);
			ret = 1;
		}
		else
		{
			__INFO("nowtime %lld, %04d-%02d-%02d %02d:%02d:%02d is NOT in summer time between %d~%d",
				   (long long)now, year, month, day, hour, minute, second, nFromSec, nToSec);
		}
	}
	else
	{
		if (now >= nToSec && now <= nFromSec)
		{
			__INFO("nowtime %lld, %04d-%02d-%02d %02d:%02d:%02d is in not winter time. (in %d~%d)",
				   (long long)now, year, month, day, hour, minute, second, nToSec, nFromSec);
			ret = 0;
		}
		else
		{
			__INFO("nowtime %lld, %04d-%02d-%02d %02d:%02d:%02d is in winter time (not in %d~%d)",
				   (long long)now, year, month, day, hour, minute, second, nToSec, nFromSec);
			ret = 1;
		}
	}

	return ret;
}

int anj_systime_set_zone_ex(int timezone_cfg, const SummerTimeConfig *pSummer)
{
	time_t now = time(NULL);
	int timezone = timezone_cfg;
	if (anj_systime_is_dst(now, timezone_cfg, pSummer))
	{
		timezone += pSummer->nOffsetMin;
		__INFO("now is in summer time, offset time %d seconds, set timezone=%d\n", pSummer->nOffsetMin, timezone);
	}
	else
	{
		__INFO("now is NOT in summer time, offset time %d seconds, set timezone=%d\n", pSummer->nOffsetMin, timezone);
	}
	anj_systime_set_zone(timezone);

	return 0;
}

int anj_systime_set_zone(int tz)
{
	int tzIndex = -1;
	const char *localtimeLnk;

	if (system_tz == tz)
	{
		return 0;
	}

	int i = 0;
	for (i = 0; i < (int)(sizeof(tzItems) / sizeof(tzItems[0])); i++)
	{
		if (tzItems[i].zone == tz)
		{
			tzIndex = i;
			break;
		}
	}

	if (tzIndex < 0)
	{
		anj_systime_set_zone(1200); // 默认设置为8时区
		return 0;
	}

	system_tz = tz;
	localtimeLnk = tzItems[tzIndex].localtimeLnk;

	__INFO("################## %d-->%s\n", tz, localtimeLnk);

	int bSetNeed = 1;
	if (access("/tmp/localtime", F_OK) == 0)
	{
		char szRealFile[256] = {0};
		readlink("/tmp/localtime", szRealFile, sizeof(szRealFile));
		if (strcmp(szRealFile, localtimeLnk) == 0)
		{
			bSetNeed = 0;
			__INFO("localtime %s need not change.\n", localtimeLnk);
		}
		else
		{
			__INFO("%s change to %s.\n", szRealFile, localtimeLnk);
		}
	}
	else
	{
		__INFO("localtime not exist\n");
	}

	if (bSetNeed)
	{
		// for date command
		// make sure that /etc/localtime is existed and link to /tmp/localtimeLnk
		remove("/tmp/localtime");
		symlink(localtimeLnk, "/tmp/localtime");
	}

	char szCST[32];
	int nZoneIn24 = tz / 60 - 12;
	int nRemainder = tz % 60;
	if (nZoneIn24 >= 0)
	{
		sprintf(szCST, "GMT-%02d:%02d", nZoneIn24, nRemainder);
	}
	else
	{
		if (nRemainder > 0)
		{
			nZoneIn24 = 0 - (nZoneIn24 + 1); // 690为GMT+0.30
		}
		else
		{
			nZoneIn24 = 0 - (nZoneIn24);
		}
		sprintf(szCST, "GMT+%02d:%02d", nZoneIn24, nRemainder);
	}

	setenv("TZ", szCST, 1);
	tzset();
	__INFO("TZ=%s\n", szCST);

	int timezone = -1;
	anj_mw_read_file_limit_len(TIME_ZONE_FILE, (char *)&timezone, sizeof(int));
	if (timezone != tz)
	{
		anj_mw_write_file(TIME_ZONE_FILE, 0, (char *)&tz, sizeof(int));
	}

	return 0;
}

int anj_systime_load_zone()
{
	int timezone = -1;
	if (0 == anj_mw_read_file_limit_len(TIME_ZONE_FILE, (char *)&timezone, sizeof(int)))
		return -1;

	if (timezone >= 0)
	{
		char szCST[32];
		int nZoneIn24 = timezone / 60 - 12;
		int nRemainder = timezone % 60;
		if (nZoneIn24 >= 0)
		{
			sprintf(szCST, "GMT-%02d:%02d", nZoneIn24, nRemainder);
		}
		else
		{
			nZoneIn24 = 0 - (nZoneIn24 + 1); // 690为GMT+0.30
			sprintf(szCST, "GMT+%02d:%02d", nZoneIn24, nRemainder);
		}

		setenv("TZ", szCST, 1);
		tzset();
		__INFO("TZ=%s", szCST);
	}

	return timezone;
}

// 将-12~+12时区换算到0-1440的偏移分钟
int anj_systime_get_zone_by_system()
{
	struct tm *ptm, tbuf;
	time_t now = time(NULL);
	ptm = gmtime_r(&now, &tbuf); // 得到0时区的tm结构，比localtime少偏移秒数

	time_t utc = mktime(ptm);

	time_t diff = now - utc;
	int timezone = (int)(12 * 60) + (int)(diff / 60);
	__INFO("%lld-%lld=%lld, timezone=%d\n", (long long)utc, (long long)now, (long long)diff, timezone);

	return timezone;
}

int anj_systime_adjust_by_time_t(time_t t, long tv_usec)
{
	struct timeval tv;
	struct timezone tz1;
	struct timeval tv1;

	gettimeofday(&tv1, &tz1);
	if (tv1.tv_sec <= (t + 1) && tv1.tv_sec >= (t - 1))
	{
		__ERR("less than 3s, set: %lld, now: %lld, diff: %lld\n", (long long)t, (long long)tv1.tv_sec, (long long)(t - tv1.tv_sec));
		return -1;
	}
	else
	{
		__INFO("more than 3s, set: %lld, now: %lld, diff: %lld\n", (long long)t, (long long)tv1.tv_sec, (long long)(t - tv1.tv_sec));
	}

	tv.tv_sec = t;
	tv.tv_usec = tv_usec;
	__INFO("settime: %u:%lld\n", t, (long long)tv_usec);

	int ret = settimeofday(&tv, 0);
	if (ret == -1)
	{
		__ERR("settimeofday fail err:%d\n", errno);
	}

	gettimeofday(&tv1, &tz1);
	__INFO("gettimeofday return tv1.tv_sec:%lld timezone %d:%d\n", (long long)tv1.tv_sec, tz1.tz_minuteswest, tz1.tz_dsttime);

	struct tm tm_now;
	SystemLocalTime(&tm_now);
	__INFO("anj_systime_adjust is %04d-%02d-%02d %02d:%02d:%02d\n",
		  tm_now.tm_year + 1900,
		  tm_now.tm_mon + 1,
		  tm_now.tm_mday,
		  tm_now.tm_hour,
		  tm_now.tm_min,
		  tm_now.tm_sec);

	anj_mw_rtc_write(tm_now);

	return 0;
}

int anj_systime_adjust(struct tm tm_time, long tv_usec, int tz, int manual)
{
	time_t t;
	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();

	__INFO("set time: %04d-%02d-%02d %02d:%02d:%02d, tz=%d\n",
		  tm_time.tm_year,
		  tm_time.tm_mon,
		  tm_time.tm_mday,
		  tm_time.tm_hour,
		  tm_time.tm_min,
		  tm_time.tm_sec, tz);
	if (tz >= 0)
	{
		anj_systime_set_zone_ex(tz, &pstSystemConfig->timeCfg.summerConfig);
		if (pstSystemConfig->timeCfg.timeZone != tz)
		{
			pstSystemConfig->timeCfg.timeZone = tz;
			anj_config_system_save(pstSystemConfig);
		}

		if (tm_time.tm_year == 1900)
		{
			__ERR("Only set zone\n");
			pstSystemConfig->timeCfg.timeZone = tz;
			anj_config_system_save(pstSystemConfig);
			return 0;
		}
	}

	if (tz < 0)
		tz = anj_systime_get_zone_by_system();

	t = ch_mktime(tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
				  tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
				  (0 + 12) * 60); // 0时区时间戳
	t = t - (tz - (12 * 60)) * 60;
	__INFO("timestamp utc=%llu, tz=%d, anj_systime_get_zone_by_system=%d\n", (unsigned long long)t, tz, anj_systime_get_zone_by_system());

	// check if we need to set time, if difference is less than 3 seconds, skip it

	return anj_systime_adjust_by_time_t(t, tv_usec);
}

int anj_systime_set_time_and_zone(struct tm tm_time, int tz, int TimingMode)
{
	__INFO("set time:%04d%02d%02d %02d:%02d:%02d, tz=%d\n",
		  tm_time.tm_year + 1900,
		  tm_time.tm_mon + 1,
		  tm_time.tm_mday,
		  tm_time.tm_hour,
		  tm_time.tm_min,
		  tm_time.tm_sec, tz);

	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
	if (tz < 0)
		tz = pstSystemConfig->timeCfg.timeZone;

	anj_systime_adjust(tm_time, 0, tz, TimingMode);

	return 0;
}

int anj_systime_rtc_update()
{
	struct tm tm_rtc;
	struct timeval tv;
	time_t t;

	memset(&tm_rtc, 0, sizeof(struct tm));

	if (anj_mw_rtc_read(&tm_rtc) < 0)
	{
		__ERR("anj_mw_rtc_read faild\n");
		return -1;
	}

	__INFO("get time from rtc is %d:%d:%d %d:%d:%d, time_zone:%d anj_systime_get_zone_by_system=%d\n",
		  tm_rtc.tm_year + 1900,
		  tm_rtc.tm_mon + 1,
		  tm_rtc.tm_mday,
		  tm_rtc.tm_hour,
		  tm_rtc.tm_min,
		  tm_rtc.tm_sec);

	t = mktime(&tm_rtc);

	tv.tv_sec = t;
	tv.tv_usec = 0;

	if (settimeofday(&tv, 0) < 0)
	{
		__ERR("settimeofday faild, errno=%d, errString=%s\n", errno, strerror(errno));
		return -1;
	}

	return 0;
}

void anj_systime_dst_check()
{
	time_t now = time(NULL);
	SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();

	int timezone_cfg = pstSystemConfig->timeCfg.timeZone;
	const SummerTimeConfig *pSummer = &pstSystemConfig->timeCfg.summerConfig;

	// 用配置时区的方式来执行夏令时更改
	static int bLastStatus = -1;
	int bFlag = anj_systime_is_dst(now, timezone_cfg, pSummer);
	if (bLastStatus != bFlag)
	{
		bLastStatus = bFlag;
		__INFO("IsInSummerTime=%d, ready to set timezone\n", bFlag);
		anj_systime_set_zone_ex(timezone_cfg, pSummer);
	}
}

int anj_systime_set_only(struct tm tm_time, int tz, int TimingMode)
{
	// 设置指定时区的年月日时分秒
	__INFO("got time:%04d-%02d-%02d %02d:%02d:%02d, tz=%d\n",
		   tm_time.tm_year + 1900,
		   tm_time.tm_mon + 1,
		   tm_time.tm_mday,
		   tm_time.tm_hour,
		   tm_time.tm_min,
		   tm_time.tm_sec, tz);

	if ((tm_time.tm_year < 100) || (tm_time.tm_year > 300) ||
		(tm_time.tm_mon > 11) || (tm_time.tm_mon < 0) ||
		(tm_time.tm_mday > 31) || (tm_time.tm_mday < 1) ||
		(tm_time.tm_hour > 23) || (tm_time.tm_hour < 0) ||
		(tm_time.tm_min > 59) || (tm_time.tm_min < 0) ||
		(tm_time.tm_sec > 60) || (tm_time.tm_sec < 0))
	{
		__ERR("invalid time , discard it..\n");
		return 0;
	}

	if (tz < 0)
		tz = anj_systime_get_zone_by_system();

	time_t t_settime = ch_mktime(
		tm_time.tm_year + 1900,
		tm_time.tm_mon + 1,
		tm_time.tm_mday,
		tm_time.tm_hour,
		tm_time.tm_min,
		tm_time.tm_sec,
		(0 + 12) * 60); // 指定年月日时分秒的0时区时间戳

	// 将时区偏移秒数减掉，才是相应时区指定时间点的time_t时间秒
	t_settime = t_settime - (tz - 12 * 60) * 60; // 减掉设置者的时区

	struct tm tm, *ptm;
	ptm = localtime_r(&t_settime, &tm);
	__INFO("utc %d, tz %d, localtime: %04d-%02d-%02d %02d:%02d:%02d\n",
		   t_settime, tz,
		   ptm->tm_year + 1900,
		   ptm->tm_mon + 1,
		   ptm->tm_mday,
		   ptm->tm_hour,
		   ptm->tm_min,
		   ptm->tm_sec);

	anj_systime_adjust_by_time_t(t_settime, 0);

	return 0;
}

int anj_systime_set_ex(struct timeval tv)
{
	struct tm tm, *ptm;
	ptm = localtime_r(&tv.tv_sec, &tm);
	__INFO("got from: %lld:%ld, localtime: %04d-%02d-%02d %02d:%02d:%02d\n",
		  (long long)tv.tv_sec, (long)tv.tv_usec,
		  ptm->tm_year + 1900,
		  ptm->tm_mon + 1,
		  ptm->tm_mday,
		  ptm->tm_hour,
		  ptm->tm_min,
		  ptm->tm_sec);

	anj_systime_adjust_by_time_t(tv.tv_sec, tv.tv_usec);
	return 0;
}

static void anj_system_ntp_update_callback(int sec, int usec)
{
    anj_systime_adjust_by_time_t(sec, usec);
    s_bNtpUpdateOk = 1;
}

int anj_systime_ntp_update_time(void *ctx, int *bStart)
{
    s_bNtpUpdateOk = 0;

    NTPConfig *pstNtpConfig = (NTPConfig *)ctx;

    int iRet = 0;
    int loopcnt = 0;
    int update_interval = 0;

    while(bStart && *bStart)
    {
        if (s_bNtpUpdateOk == 0)
        {
            update_interval = 10 * 10;
        }
        else
        {
            update_interval = pstNtpConfig->refreshInterval * 10;
        }

        if ((loopcnt % update_interval) == 0)
        {
            if (loopcnt > 0)
            {
                loopcnt = 0;
            }

            ntp_stop_get_time();
            iRet = ntp_start_get_time(pstNtpConfig->serverIP, pstNtpConfig->refreshInterval, 1, 0);
            __INFO("NTP updating time server:%s and interval:%d finished:%d. result:%d!!!\n", 
                pstNtpConfig->serverIP, pstNtpConfig->refreshInterval, iRet, s_bNtpUpdateOk);
        }

        loopcnt++;
        usleep(100 * 1000);
    }

    __INFO("ntp update time end!\n");
    anj_mw_free(pstNtpConfig);
    pstNtpConfig = NULL;

    return 0;
}

void anj_systime_ntp_stop()
{
    if (s_stNtpUpdateThread.start > 0)
    {
        anj_thread_task_destroy(&s_stNtpUpdateThread, 0);
        __INFO("ntp update stop!\n");
    }

    ntp_unregister_time_callback();
}

int anj_systime_ntp_update(const char *ntpserver, int port, int interval, int tz)
{
    int iRet = 0;
    anj_systime_ntp_stop();

    if (s_stNtpUpdateThread.start == 0)
    {
        __INFO("ntp update time server:%s, port:%d, interval:%d, tz:%d!\n", ntpserver, port, interval, tz);

        ntp_register_time_callback(anj_system_ntp_update_callback);

        SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
        anj_systime_set_zone_ex(tz, &pstSystemConfig->timeCfg.summerConfig);

        NTPConfig *pstNtpConfig = (NTPConfig *)anj_mw_malloc(sizeof(NTPConfig));
        if (pstNtpConfig == NULL)
        {
            __ERR("NTPConfig malloc failed!\n");
            return -1;
        }

        memset(pstNtpConfig, 0, sizeof(NTPConfig));
        StrCpy(pstNtpConfig->serverIP, sizeof(pstNtpConfig->serverIP), ntpserver);
        pstNtpConfig->serverPort = port;
        pstNtpConfig->refreshInterval = interval;

        s_stNtpUpdateThread.bAutoDestroy = 1;
        strncpy(s_stNtpUpdateThread.iThreadName, "anj_ntp_update", sizeof(s_stNtpUpdateThread.iThreadName) - 1);
        s_stNtpUpdateThread.iThreadjob.ctx = (void *)pstNtpConfig;
        s_stNtpUpdateThread.iThreadjob.func = anj_systime_ntp_update_time;
        iRet = anj_thread_task_create(&s_stNtpUpdateThread);
        if (iRet)
        {
            __ERR("update time thread create failed\n");

            anj_mw_free(pstNtpConfig);
            pstNtpConfig = NULL;
            return -1;
        }
    }

    return 0;
}

void anj_systime_init()
{
    SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
    TimeConfig *pstTimeConfig = &pstSystemConfig->timeCfg;
    if (strcmp(pstTimeConfig->timeMode.modeName, TIME_MODE_NAME_NTP) == 0)
    {
        anj_systime_ntp_update(pstTimeConfig->ntpConfig.serverIP, 
                            pstTimeConfig->ntpConfig.serverPort,
                            pstTimeConfig->ntpConfig.refreshInterval,
                            pstTimeConfig->timeZone);
    }
}

void anj_systime_uninit()
{
    anj_systime_ntp_stop();    
}
