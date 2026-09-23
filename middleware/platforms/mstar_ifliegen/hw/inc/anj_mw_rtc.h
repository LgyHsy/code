#ifndef _ANJ_MW_RTC_H_
#define _ANJ_MW_RTC_H_

#include <time.h>

#if defined(__cplusplus)
extern "C"
{
#endif

int anj_mw_rtc_read(struct tm *tm_time);

int anj_mw_rtc_write(struct tm tm_time);

#if defined(__cplusplus)
}
#endif

#endif
