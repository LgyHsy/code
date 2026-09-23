#ifndef __HIK_NET_MEDIA_MAP_H__
#define __HIK_NET_MEDIA_MAP_H__

#include "hik_net_types.h"

#ifdef __cplusplus
extern "C" {
#endif

UINT8 hik_resolution_by_wh(int width, int height);
UINT32 hik_framerate_index(int fps);
UINT8 hik_video_enc_type(const char *encode_name);
UINT8 hik_audio_enc_type(const char *audio_name);

/* reverse helpers for SET_COMPRESSCFG_EX_V30 */
int hik_wh_by_resolution(UINT8 res_code, int *width, int *height);
UINT32 hik_framerate_from_index(UINT32 idx);
UINT32 hik_bitrate_kbps_from_index(UINT32 idx);
void hik_resolution_name_by_wh(int width, int height, char *name, int name_len);

#ifdef __cplusplus
}
#endif

#endif
