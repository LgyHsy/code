#ifndef __MEDIA_UTIL_H__
#define __MEDIA_UTIL_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

typedef enum
{
    MEDIA_CODEC_VIDEO_H265_PLUS = 0,
    MEDIA_CODEC_VIDEO_H265,
    MEDIA_CODEC_VIDEO_H264,
    MEDIA_CODEC_VIDEO_JPG,
    MEDIA_CODEC_VIDEO_MJPG,

    MEDIA_CODEC_AUDIO_PCM = 10,
    MEDIA_CODEC_AUDIO_G711A = 11,
    MEDIA_CODEC_AUDIO_G711U = 12,
    MEDIA_CODEC_AUDIO_AAC = 13,
    MEDIA_CODEC_AUDIO_MP3 = 14,
    MEDIA_CODEC_NONE,
} media_codec_type_e;

typedef enum
{
    MEDIA_VFRAME_P = 0,
    MEDIA_VFRAME_I = 1,

    MEDIA_AFRAME_A = 10,
} media_frame_type_e;

typedef struct
{
    media_codec_type_e frameCodec;
    media_frame_type_e frameType;
    unsigned int aframeIndex;
    unsigned int vframeIndex;
    unsigned int frameKeyIndex;
    unsigned long long framePts;
    long frameTime;
    unsigned long long frameTimeMs;
    unsigned int frameLen;
} media_frame_param_t;

typedef struct
{
    media_frame_param_t frameParam;
    unsigned char *frameBuf;
} media_frame_info_t;

media_codec_type_e audio_encode_type_get(char *codec);
const char *audio_encode_type_str(media_codec_type_e audio_encode);
int video_encode_type_get(char *codec);

int stream_h264_pps_offset_get(unsigned char *buf, int len);
int stream_h265_pps_offset_get(unsigned char *buf, int len);
int h264_get_pframe_offset(char *buf, int len, int iKeepStartCode);
int h265_get_pframe_offset(char *buf, int len, int iKeepStartCode);
// buf sps+pps+idr
// Frame format: 00 00 00 01 68 AA AA AA AA AA AA 00 00 00 01 67 BB BB BB BB BB BB
//               00 00 00 01 06 CC CC CC CC CC CC 00 00 00 01 65 DD DD DD DD DD DD
int h264_get_sps_pps_sei(char *pcBuf, int iLen,
                              int iXpsKeepStartCode, int iIFrameKeepStartCode,
                              char *pcSps, int *pnSpsLen,
                              char *pcPps, int *pnPpsLen,
                              char *pcSei, int *pnSeiLen);
// buf vps+sps+pps+SEI+vcl
// Frame format: 00 00 00 01 40 01 AA AA AA AA AA AA 00 00 00 01 42 01 BB BB BB BB BB BB
//               00 00 00 01 44 01 CC CC CC CC CC CC 00 00 00 01 4E 01 DD DD DD DD DD DD
//               00 00 00 01 26 01 EE EE EE EE EE EE
int h265_get_vps_sps_pps_sei(char *pcBuf, int iLen,
                                  int iXpsKeepStartCode, int iIFrameKeepStartCode,
                                  char *pcVps, int *pnVpsLen,
                                  char *pcSps, int *pnSpsLen,
                                  char *pcPps, int *pnPpsLen,
                                  char *pcSei, int *pnSeiLen);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
