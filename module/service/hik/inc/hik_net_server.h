#ifndef __HIK_NET_SERVER_H__
#define __HIK_NET_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

int hik_net_server_start(unsigned short port);
int hik_net_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif
