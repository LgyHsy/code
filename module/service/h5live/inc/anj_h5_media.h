#ifndef __ANJ_H5_MEDIA_H__
#define __ANJ_H5_MEDIA_H__

#ifdef __cplusplus
extern "C" {
#endif

int anj_h5_media_streams_start(void);
int anj_h5_media_streams_stop(void);
int anj_h5_media_monitor_thread(void *ctx, int *bStart);
void anj_h5_media_monitor_start(void);
void anj_h5_media_monitor_stop(void);
void anj_h5_request_idr(int stream_id);
void anj_h5_audio_config_sync(void);
void anj_h5_live_audio_reader_restart(void);

#ifdef __cplusplus
}
#endif

#endif
