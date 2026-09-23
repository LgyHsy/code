#ifndef __ANJ_AIOT_REPORT_H__
#define __ANJ_AIOT_REPORT_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* 设备使用统计，仅随 60s 属性写云存；不是 Get/SetProperty 项 */
typedef enum
{
    ANJ_AIOT_STAT_AIDETECT_CNT = 0,
    ANJ_AIOT_STAT_PREVIEW_CNT,
    ANJ_AIOT_STAT_PREVIEW_DURATION,  /* 毫秒 */
    ANJ_AIOT_STAT_PLAYBACK_CNT,
    ANJ_AIOT_STAT_PLAYBACK_DURATION, /* 毫秒 */
    ANJ_AIOT_STAT_MAX
} anj_aiot_stat_e;

int anj_aiot_report_init();
void anj_aiot_report_uninit();
int anj_aiot_report_alarm_handle(int chn, int alarm_code, int alarm_level, char *alarm_data);
void anj_aiot_stat_add(anj_aiot_stat_e id, unsigned long long val);

#ifdef __cplusplus
}
#endif

#endif
