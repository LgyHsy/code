#ifndef __ANJ_VIDEO_H__
#define __ANJ_VIDEO_H__

#include "anj_config_media.h"
#ifdef __cplusplus
extern "C"
{
#endif

int anj_video_adjust_gop(int iCameraIdex, int fps);
int anj_video_adjust_bitrate(int iCameraIdex, int fps);

int anj_video_set_bitrate(int VencChn, int iBitrate);
int anj_video_set_gop(int VencChn, int gop);
int anj_video_set_fps(int VencChn, int fps);

void anj_video_request_idr(int Chn, int VencId);
int anj_video_set_config(void *data);
int anj_video_encode_switch(void);
int anj_video_restart_is_busy(void);
int anj_video_scl_set(int enable);
int anj_video_bitrate_get(int iCameraIdex, int VencChn);
int anj_video_fps_get(int iCameraIdex, int VencChn);
/* 软抓拍取流分辨率：独立 SCL 用子码流，否则 smart */
void anj_video_snap_yuv_size_get(int iCameraIdex, int *width, int *height);

void anj_video_qos_notify(void);

#ifdef __cplusplus
}
#endif

#endif
