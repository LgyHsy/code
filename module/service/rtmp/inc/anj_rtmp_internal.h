#ifndef __ANJ_RTMP_INTERNAL_H__
#define __ANJ_RTMP_INTERNAL_H__

#include <pthread.h>
#include <stdint.h>

#include "librtmp/rtmp-client.h"
#include "anj_config_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ANJ_RTMP_PORT_DEF 1935
#define ANJ_RTMP_MAIN_STREAM 0
#define ANJ_RTMP_SUB_STREAM 1

typedef struct
{
    int started;
    int stream_no;
    int rtmp_type;
    int use_system_ts;
    int audio_enable;
    int video_codec_id;
    int audio_codec_id;
    int send_sps_pps;
    uint32_t start_ts;
    int mutex_inited;
    rtmp_client_t *client;
    int socket_fd;
    pthread_mutex_t push_mutex;
} anj_rtmp_session_t;

int anj_rtmp_session_start(const RtmpConfig *cfg, anj_rtmp_session_t *session);
void anj_rtmp_session_stop(anj_rtmp_session_t *session);

int anj_rtmp_media_start(anj_rtmp_session_t *session);
void anj_rtmp_media_stop(void);

int anj_rtmp_mux_publish_script(anj_rtmp_session_t *session, int width, int height, int framerate,
                                int audiodatarate, int audiosamplerate);
int anj_rtmp_mux_publish_aac_header(anj_rtmp_session_t *session);
int anj_rtmp_mux_on_video(anj_rtmp_session_t *session, unsigned char *data, unsigned int len,
                          int is_key, uint32_t ts);
int anj_rtmp_mux_on_audio(anj_rtmp_session_t *session, unsigned char *data, unsigned int len,
                          uint32_t ts);
void anj_rtmp_mux_reset_state(anj_rtmp_session_t *session);
void anj_rtmp_mux_uninit(void);
int anj_rtmp_mux_fill_codec_info(anj_rtmp_session_t *session);
int anj_rtmp_mux_get_av_param(anj_rtmp_session_t *session, int *width, int *height, int *framerate,
                              int *audiodatarate, int *audiosamplerate);

int anj_rtmp_ctrl_start(void);
void anj_rtmp_ctrl_stop(void);
void anj_rtmp_ctrl_restart(void);

#ifdef __cplusplus
}
#endif

#endif
