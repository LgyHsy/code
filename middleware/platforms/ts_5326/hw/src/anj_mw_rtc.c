#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#include "anj_mw_comm.h"
#include "anj_mw_rtc.h"

typedef struct
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
} rtc_time_t;

#define RTC_RD_TIME _IOR('p', 0x09, rtc_time_t)  /* Read RTC time   */
#define RTC_SET_TIME _IOW('p', 0x0a, rtc_time_t) /* Set RTC time    */

#define RTC_DEVICE "/dev/rtc0"

int anj_mw_rtc_read(struct tm *tm_time)
{
    int fd_t = -1;
    rtc_time_t rtcTime;
    time_t lt = time(NULL);
    struct tm *ptr = localtime(&lt);

    fd_t = open(RTC_DEVICE, O_RDWR);
    if (fd_t < 0)
    {
        __ERR("open rtc is error \n");
        return -1;
    }

    if (ioctl(fd_t, RTC_RD_TIME, &rtcTime) < 0)
    {
        __ERR("=ioctl read time error\n");
        close(fd_t);
        return -1;
    }

    memcpy(tm_time, ptr, sizeof(struct tm));
    tm_time->tm_year = rtcTime.tm_year;
    tm_time->tm_mon = rtcTime.tm_mon;
    tm_time->tm_mday = rtcTime.tm_mday;
    tm_time->tm_hour = rtcTime.tm_hour;
    tm_time->tm_min = rtcTime.tm_min;
    tm_time->tm_sec = rtcTime.tm_sec;
    tm_time->tm_wday = rtcTime.tm_wday;

    __ERR("get time from rtc is %d:%d:%d %d:%d:%d \n",
          tm_time->tm_year + 1900,
          tm_time->tm_mon + 1,
          tm_time->tm_mday,
          tm_time->tm_hour,
          tm_time->tm_min,
          tm_time->tm_sec);

    close(fd_t);

    __ERR("read Rtc Time is ok \n");

    return 0;
}

int anj_mw_rtc_write(struct tm tm_time)
{
    int fd_t = -1;
    rtc_time_t rtcTime = {0};

    fd_t = open(RTC_DEVICE, O_RDWR);
    if (fd_t < 0)
    {
        __ERR("open rtc is error \n");
        return -1;
    }

    rtcTime.tm_year = tm_time.tm_year;
    rtcTime.tm_mon = tm_time.tm_mon;
    rtcTime.tm_mday = tm_time.tm_mday;
    rtcTime.tm_wday = tm_time.tm_wday;
    rtcTime.tm_hour = tm_time.tm_hour;
    rtcTime.tm_min = tm_time.tm_min;
    rtcTime.tm_sec = tm_time.tm_sec;

    __ERR("set rtc time --> %d:%d:%d %d:%d:%d \n",
          rtcTime.tm_year + 1900, rtcTime.tm_mon + 1, rtcTime.tm_mday, rtcTime.tm_hour, rtcTime.tm_min, rtcTime.tm_sec);

    if (ioctl(fd_t, RTC_SET_TIME, &rtcTime) < 0)
    {
        __ERR("ioctl set time error\n");
        close(fd_t);
        return -1;
    }

    close(fd_t);

    __ERR("set Rtc Time is ok \n");

    return 0;
}