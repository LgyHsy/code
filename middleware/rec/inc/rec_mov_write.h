
#ifndef _REC_MOV_WRITE_H_
#define _REC_MOV_WRITE_H_

#include "rec_mov_index.h"
#include "rec_mov_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REC_MOV_PRE_MSEC (1000)
#define REC_MOV_PRE_SEC (90000)
#define REC_PTS_TO_MSEC(pts) (pts / 90)
#define REC_MSEC_TO_PTS(pts) ((uint64_t)pts * 90)

typedef enum
{
    MEDIA_VCODEC_RC_VBR = 0,
    MEDIA_VCODEC_RC_CBR = 1,
    MEDIA_VCODEC_RC_FIXQP = 2, // JPG 固定QP模式
} rec_media_vcodec_rc_mode_e;

typedef struct
{
    int height;
    int width;
    int framerate;
    int bitrate;
    int gop;
    rec_media_vcodec_rc_mode_e bitrate_ctl;
    media_codec_type_e vcodecType;
} rec_media_vcodec_param_t;

typedef struct
{
    int channels;
    int bitWidth;
    int sampleRate;
    media_codec_type_e acodecType;
} rec_media_acodec_param_t;

typedef struct
{
    rec_media_vcodec_param_t *pstVcodecParam;
    rec_media_acodec_param_t *pstAcodecParam;
    char iWriteFileName[128];
    rec_file_index_record *pstIndexRecord;
    rec_media_segment_index stIndexSegment[REC_MEDIA_INDEX_MAX_SEGMENT];

    unsigned int vtime_duration; // BASE 90000/sec
    unsigned int atime_duration; // BASE 90000/sec
    int atime_duration_delay;    // BASE 90000/sec
    unsigned int time_create;
    FILE *stMovfp;
    unsigned int stMediaFileOffset;
    rec_file_write_param stRecWriteParam;

    uint64_t stVFramePts; // 视频最新时间戳
    uint64_t stAFramePts; // 最新音频时间戳

    unsigned char *stDurationBuf_mvhd;
    unsigned char *stDurationBuf_vtrak_tkhd; // 1000 IPCAM_MSEC_PRE_SEC
    unsigned char *stDurationBuf_vtrak_elst; // 1000 IPCAM_MSEC_PRE_SEC
    unsigned char *stDurationBuf_vtrak_mdhd; // 90000 IPCAM_PTZ_PRE_SEC
    unsigned char *stMovVtrak_stsd_info;     // stsd info sps,vps pps等
    unsigned char stMovVtrak_stsd_infoLen;   // stsd info sps,vps pps等最大长度
    unsigned char bSaveStsdInfo;             // 是否回去成功

    unsigned char *stDurationBuf_atrak_tkhd;       // 1000 IPCAM_MSEC_PRE_SEC
    unsigned char *stDurationBuf_atrak_elst_delay; // 1000 IPCAM_MSEC_PRE_SEC
    unsigned char *stDurationBuf_atrak_elst;       // 1000 IPCAM_MSEC_PRE_SEC
    unsigned char *stDurationBuf_atrak_mdhd;       // 90000 IPCAM_PTZ_PRE_SEC

    unsigned char stMovVTrackBuf[MOV_BOX_MOOV_VTRACK_BUF_SIZE];      // mov与vtrack 数据 + stsd
    unsigned char stVTrackSttsBuf[MOV_BOX_MOOV_TRACK_STTS_BUF_SIZE]; // track STTS
    unsigned char stVTrackStssBuf[MOV_BOX_MOOV_TRACK_STSS_BUF_SIZE]; // track STSS
    unsigned char stVTrackStscBuf[MOV_BOX_MOOV_TRACK_STSC_BUF_SIZE]; // track STSC
    unsigned char stVTrackStszBuf[MOV_BOX_MOOV_TRACK_STSZ_BUF_SIZE]; // track STSZ
    unsigned char stVTrackStcoBuf[MOV_BOX_MOOV_TRACK_STCO_BUF_SIZE]; // track STCO
    unsigned int stVTrackFrameIndex;                                 // V起始帧序号
    unsigned int stVTrackSaveFrameIndex;                             // Vsave index
    unsigned int stVTrackKeyFrameIndex;                              // VKeyIndex
    unsigned int stVTrackKeySaveFrameIndex;                          // VSave KeyIndex
    unsigned int stLastVKeyFrameNo;                                  // 文件内最近I帧的视频帧号
    unsigned int stLastVKeyIndex;                                    // 文件内最近I帧的关键帧索引号
    uint64_t stLastVKeyPts;                                          // 最新I帧的pts
    unsigned int stLastVKeyTime;                                     // 最新I帧的秒时间戳
    unsigned int stLastAFrameNo;                                     // 文件内最近I帧对应的音频帧号

    unsigned char stATrackBuf[MOV_BOX_MOOV_ATRACK_BUF_SIZE];         // atrack 数据 除了sample stsd
    unsigned char stATrackSttsBuf[MOV_BOX_MOOV_TRACK_STTS_BUF_SIZE]; // track STTS
    unsigned char stATrackStscBuf[MOV_BOX_MOOV_TRACK_STSC_BUF_SIZE]; // track STSC
    unsigned char stATrackStszBuf[MOV_BOX_MOOV_TRACK_STSZ_BUF_SIZE]; // track STSZ
    unsigned char stATrackStcoBuf[MOV_BOX_MOOV_TRACK_STCO_BUF_SIZE]; // track STCO
    unsigned int stATrackFrameIndex;                                 // A起始帧序号
    unsigned int stATrackSaveFrameIndex;                             // Asave index
} rec_mov_info_t;

typedef struct
{
    unsigned int iFrameDuration;
    unsigned int iFrameSize;
    unsigned int iFrameOffset;
} rec_mov_frame_info_t;

typedef struct
{
    unsigned int iFrameStart;
    rec_media_vcodec_param_t stRecVcodecParam;
    rec_media_acodec_param_t stRecAcodecParam;
    rec_mov_frame_info_t iFrameInfo[REC_MEDIA_BUF_FPS];
} rec_mov_read_frame_info_t;

typedef struct
{
    unsigned int iKeyFrameStart;
    unsigned int iKeyIndex[REC_MEDIA_BUF_FPS];
} rec_mov_read_keyframe_info_t;

int rec_mov_write_box_moov_trak_update(rec_mov_info_t *pstMovInfo, int bSave);
int rec_mov_write_box_moov_vtrak_write_frame(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo);
int rec_mov_write_box_moov_atrak_write_frame(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo);
int rec_mov_write_box_write_update_index(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo, unsigned int tEvent, unsigned int segChangTime);
int rec_mov_write_box_write_file_check(rec_mov_info_t *pstMovInfo, media_frame_info_t *pFrameInfo);
rec_mov_info_t *rec_mov_write_box_create(const char *filename, rec_file_index_record *pstIndexRecord, rec_media_vcodec_param_t *pstVcodecParam, rec_media_acodec_param_t *pstAcodecParam);
int rec_mov_write_box_destroy(rec_mov_info_t *pstMovInfo, int err);

int rec_mov_read_box_free_segment(FILE *fp, rec_media_segment_index *pstSegMent, unsigned int count);

int rec_mov_read_frame_is_valid(rec_mov_frame_info_t *pstFrameinfo);
rec_mov_frame_info_t *rec_mov_read_vframe_info(FILE *fp, rec_mov_read_frame_info_t *pstVFrameinfo, unsigned int iFrameNo);
rec_mov_frame_info_t *rec_mov_read_aframe_info(FILE *fp, rec_mov_read_frame_info_t *pstVFrameinfo, unsigned int iFrameNo);
int rec_mov_read_keyframe_info(FILE *fp, rec_mov_read_keyframe_info_t *pstKeyFrameinfo, unsigned int iFrameNo);
int rec_mov_read_frame_data(FILE *fp, unsigned char *pData, unsigned int pDataLen, rec_mov_frame_info_t *pstFrameinfo, int *isKeyFlag, int bVideo);

rec_mov_info_t *rec_mov_create_mp4(char *filename, rec_media_vcodec_param_t *pstVcodecParam, rec_media_acodec_param_t *pstAcodecParam);
int rec_mov_write_mp4(media_frame_info_t *pFrameInfo, rec_mov_info_t *pstMovInfo);
int rec_mov_close_mp4(rec_mov_info_t *pstMovInfo);

#ifdef __cplusplus
}
#endif
#endif /* !_mov_writer_h_ */
