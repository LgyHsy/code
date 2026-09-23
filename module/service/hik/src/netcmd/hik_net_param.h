#ifndef __HIK_NET_PARAM_H__
#define __HIK_NET_PARAM_H__

#include "hik_net_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int hik_cmd_get_netcfg(int fd);
int hik_cmd_set_netcfg(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_netappcfg(int fd);
int hik_cmd_get_piccfg(int fd, const char *recvbuf, int recvlen);
int hik_cmd_set_piccfg_ex(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_videoeffect(int fd, const char *recvbuf, int recvlen);
int hik_cmd_set_videoeffect(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_alarmincfg(int fd);
int hik_cmd_set_usercfg(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_usercfg_ex(int fd);
int hik_cmd_set_usercfg_ex(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_workstatus(int fd);
int hik_cmd_get_scalecfg(int fd);
int hik_cmd_set_scalecfg(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_ccdparamcfg(int fd);
int hik_cmd_set_ccdparamcfg(int fd, const char *recvbuf, int recvlen);
int hik_cmd_set_compress_v30(int fd, const char *recvbuf, int recvlen);
int hik_cmd_get_compress_aud(int fd);
int hik_cmd_get_compress_aud_current(int fd);

#ifdef __cplusplus
}
#endif

#endif
