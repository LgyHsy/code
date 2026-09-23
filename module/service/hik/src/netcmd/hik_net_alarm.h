#ifndef __HIK_NET_ALARM_H__
#define __HIK_NET_ALARM_H__

#ifdef __cplusplus
extern "C" {
#endif

int hik_net_alarm_start(void);
int hik_net_alarm_stop(void);

/* Take ownership of fd after QUALIFIED. Returns 0 on success. */
int hik_net_alarm_add_fd(int fd);

/* Push alarm to registered upload channels (matches old addAlarm). */
int hik_net_alarm_notify(int alarm_code, int alarm_flag);

#ifdef __cplusplus
}
#endif

#endif
