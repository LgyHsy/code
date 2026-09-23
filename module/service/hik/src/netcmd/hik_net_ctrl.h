#ifndef __HIK_NET_CTRL_H__
#define __HIK_NET_CTRL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Return: 0 ok+close, 1 keep-open, -1 error (caller closes). */
int hik_cmd_ptz(int fd, const char *recvbuf, int recvlen);
int hik_cmd_ptz_with_speed(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_jpeg(int fd, const char *recvbuf, int recvlen);
int hik_cmd_start_voicecom(int fd, const char *recvbuf, int recvlen);
int hik_cmd_alarmchan(int fd, const char *recvbuf, int recvlen);

int hik_net_voice_stop(void);

#ifdef __cplusplus
}
#endif

#endif
