#ifndef __ANJ_AIOT_H__
#define __ANJ_AIOT_H__

#include "gct_common.h"

int anj_aiot_init(void);
void anj_aiot_uninit(void);

void anj_aiot_media_info_get(int iIndex, gct_video_data_format *pstVideoFormat, gct_audio_data_format *pstAudioFormat);
int anj_aiot_bind(const char *product_key, const char *device_name, const char *accountName, const char *clientCode);
void anj_aiot_unbind();
void anj_aiot_reponse(int func, void *data);
void anj_aiot_reset_conn();

void anj_aiot_push_video(int camera_type, int streamtype, int iskey,
                            unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs);
void anj_aiot_push_audio(int camera_type, unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs);

void anj_aiot_manual_unbind();
void anj_aiot_remove_unbind_device();
int anj_aiot_bind_status_get();

#endif
