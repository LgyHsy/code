#ifndef __ANJ_SYSTIME_H__
#define __ANJ_SYSTIME_H__

#ifdef __cplusplus
extern "C"
{
#endif

int anj_systime_set_zone(int tz);
int anj_systime_load_zone();
// 将-12~+12时区换算到0-1440的偏移分钟
int anj_systime_get_zone_by_system();
int anj_systime_adjust_by_time_t(time_t t, long tv_usec);
int anj_systime_adjust(struct tm tm_time, long tv_usec, int tz, int manual);
int anj_systime_set_time_and_zone(struct tm tm_time, int tz, int TimingMode);
int anj_systime_rtc_update();
void anj_systime_dst_check();
int anj_systime_set_only(struct tm tm_time, int tz, int TimingMode);
int anj_systime_set_ex(struct timeval tv);

int anj_systime_set_zone_ex(int timezone_cfg, const SummerTimeConfig *pSummer);

void anj_systime_ntp_stop();
int anj_systime_ntp_update(const char *ntpserver, int port, int interval, int tz);

void anj_systime_init();
void anj_systime_uninit();


#if defined(__cplusplus)
}
#endif

#endif
