#ifndef _ANJ_MW_MEDIA_VIDEO_H_
#define _ANJ_MW_MEDIA_VIDEO_H_

#include "sdk_option.h"
#include "media_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    ANJ_VIDEO_AVBR = 0,
    ANJ_VIDEO_VBR,
    ANJ_VIDEO_CBR,
    ANJ_VIDEO_FIXQP,
} AnjVideoRcMode_E;

typedef struct
{
    int bitrate;
    int frame_rate;
    int gop_sequence_number;
    int is_reset_gop;
    int second_seq_in_gop;
    unsigned long gop_second_byte[50];
    unsigned int gop_second_frame[50];
    unsigned long long last_frame_time_ms;
} STREAM_STATE;

typedef void (*anj_mw_media_scl_data)(int u32DevId, void *p_vir_addr, unsigned long long p_phy_addr, int len, void *param);

typedef void (*anj_mw_media_venc_data)(int VencChn, int iskey, char *data, int len, unsigned int u32Seq, int codec, unsigned long long int timestamp);

typedef struct
{
    int enable;
    int chn;
    int fps;
    int width;
    int height;
    int gop;
    int bitrate;
    int qpenable;
    int minqp;
    int maxqp;
    int maxIsize;
    int maxPsize;
    int qp_delta;
    int qfactor;
    int profile;
    int bufszie;
    STREAM_STATE stream_state;
    media_codec_type_e encodeType;
    AnjVideoRcMode_E rcMode;
    anj_mw_media_venc_data venc_data_cb;
} AnjVencConfig;

typedef struct
{
    anj_mw_media_scl_data yuv_data_cb;
    anj_mw_media_scl_data jpg_data_cb;
    int wdr_enable;
    int hflip;
    int vflip;
    AnjVencConfig stVencCfg[MAX_VENC_CHN];
} AnjVideoConfig;

int anj_mw_media_video_init(AnjVideoConfig *pstAnjVideoCfg);
int anj_mw_media_video_uninit(void);

int anj_mw_media_venc_bind_scl(int iCameraIdex, int iSclPortIdx);
int anj_mw_media_venc_unbind_scl(int iCameraIdex, int iSclPortIdx);
int anj_mw_media_isp_bind_scl(int iCameraIdex);
int anj_mw_media_isp_unbind_scl(int iCameraIdex);
int anj_mw_media_vi_bind_isp(int iCameraIdex);
int anj_mw_media_vi_unbind_isp(int iCameraIdex);
int anj_mw_media_video_bind(int iCameraIdex);
int anj_mw_media_video_unbind(int iCameraIdex);

int anj_mw_media_video_scl_crop(int iCameraIdex, double multiple);
int anj_mw_media_video_scl_pause(void);
int anj_mw_media_video_scl_recover(void);

int anj_mw_media_video_requeset_idr(int Chn, int VencId);
int anj_mw_media_video_gop_set(int Chn, int VencId, int gop);
int anj_mw_media_video_bitrate_set(int Chn, int VencId, int bitrate);
int anj_mw_media_video_fps_set(int Chn, int VencId, int fps);
void *anj_mw_media_video_attr_get(void);

int anj_mw_media_video_config_set(AnjVencConfig *pVenCfg, int iCameraIdx, int chn);

/* 应用分辨率/编码类型/RC 等：内部 video uninit + init 重启媒体流。
 * 调用前须已卸掉 OSD/smart 等对 VENC/SCL 的占用。 */
int anj_mw_media_video_encode_apply(AnjVideoConfig *pstAnjVideoCfg);

int anj_mw_media_video_jpg_start(int iCameraIdex, void *param);
int anj_mw_media_video_jpg_stop(int iCameraIdex);
int anj_mw_media_video_jpg_init(int iCameraIdex);
int anj_mw_media_video_jpg_uninit(int iCameraIdex);
int anj_mw_media_video_jpg_capture(int cam, int stream, int quality, const char *output_file);

#ifdef __cplusplus
}
#endif

#endif
