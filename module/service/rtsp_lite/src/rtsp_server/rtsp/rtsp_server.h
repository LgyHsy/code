#ifndef __RTSP_SERVER_H__
#define __RTSP_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

int rtsp_server_init(void);
int rtsp_server_uninit(void);
int rtsp_server_restart(void);

#ifdef __cplusplus
}
#endif

#endif /* __RTSP_SERVER_H__ */
