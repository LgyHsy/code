#ifndef __ANJ_RECORD_H__
#define __ANJ_RECORD_H__

#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "rec_mov_def.h"
#include "rec_mov_index.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REC_MAX_CH_NUM (2)
#define REC_MAX_PB_NUM (REC_MAX_CH_NUM * 3)

#define REC_SEGMENT_DURATION    (10 * 60)   // 10分钟一段录像
#define REC_SEGMENT_MAX_COUNT   (150)       // 1440 / 10 再加一点冗余

typedef enum
{
    REC_STATUS_ERROR = -1,
    REC_STATUS_UNINIT = 0,     // 未初始化
    REC_STATUS_NORMAL = 1,     // 正常
    REC_STATUS_FORMAT = 2,     // 格式化中
    REC_STATUS_BAD_RECOVER = 3 // 坏块恢复中
} rec_status_e;

enum
{
    PB_SPEED_0 = 0,
    PB_SPEED_1 = 1,
    PB_SPEED_2 = 2,
    PB_SPEED_4 = 4,
    PB_SPEED_8 = 8,
    PB_SPEED_16 = 16,
};

typedef enum {
    PB_CB_NONE = 0,
    PB_CB_START,
    PB_CB_FINISH,
    PB_CB_ERROR,
    PB_CB_MAX
}pb_cb_event_e;

typedef void* REC_HANDLE;

typedef struct {
    unsigned int tEvent;
    unsigned int year;
    unsigned int month;
    unsigned int day;
} rec_pb_date_s;
                
typedef struct __pb_segment{
    unsigned int tEvent;            // not used
    unsigned int begin_time_s;      // start timestamp in second of playback, unix timestampe
    unsigned int end_time_s;        // end timestamp in second of playback, unix timestampe
    struct __pb_segment *ptNext;
} rec_pb_segment_s;

typedef struct {
    unsigned int count;                  // file count of the day
    rec_pb_segment_s *pstSegment;   //片段
} rec_pb_list_s;

typedef struct
{
    int height;
    int width;
    int framerate;
    int bitrate;
    int gop;
    media_codec_type_e vcodecType;
    int channels;
    int bitWidth;
    int sampleRate;
    media_codec_type_e acodecType;
    char metaData[256];
    int metaLen;
} rec_pb_media_param_s;

typedef struct rec_pb_cache_segment
{
    unsigned int tEvent;
    unsigned int begin_time_s;
    unsigned int end_time_s;
    struct rec_pb_cache_segment *ptNext;
} rec_pb_cache_segment_s;

typedef struct rec_pb_cache_day
{
    unsigned int year;
    unsigned int month;
    unsigned int day;
    unsigned int count;
    rec_pb_cache_segment_s *pstSegment;
    rec_pb_cache_segment_s *pstTail;
    struct rec_pb_cache_day *ptNext;
} rec_pb_cache_day_s;

typedef struct
{
    rec_pb_cache_day_s *pstDay;
} rec_pb_cache_channel_s;

typedef struct
{
    anj_thread_s stPbCacheThread;
    pthread_mutex_t mutex;
    rec_pb_cache_channel_s stChannel[REC_MAX_CH_NUM];
} rec_pb_cache_param;

typedef int (*rec_pb_cb)(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e EventID);

typedef struct
{
    int bOpen;
    int bPopStop;
    int bPause;
    int bSeek;
    int bForceMedia;
    int iSpeed;
    int iDownLoad;
    anj_thread_s stThread;
    pthread_mutex_t iPopMutex; // 数据锁
    int iPopCh;                // 弹出通道
    int iPopId;
    unsigned int tEvent;
    int iPopKeyInterval;
    unsigned int tPlayTime; // 起始时间
    unsigned int tEndTime;  // 结束时间
    unsigned int tSeekTime; // 跳转时间,0不跳转 大于0跳转
    unsigned long long tLastPts;
    unsigned long long tSendTime;
    unsigned int iLastVFrameIndex;
    unsigned int tDayStartTime;
    rec_pb_media_param_s tPbMediaParam;
    rec_pb_cb pbCb;
} rec_pb_poper;

typedef struct
{
    unsigned long filesize;
    struct tm start_time;
    int record_mode;
    int media_type;
    int stream_index;
    char filepath[100];
}record_file_item;

// 用于各协议上报录像文件具体信息
typedef struct
{
    int count;
    record_file_item items[REC_SEGMENT_MAX_COUNT];
}record_query_result_s;


// 用于已有协议解析录像查询条件
typedef struct
{
	int record_mode;
	struct tm start_time;
	struct tm end_time;
	int media_type;
	int stream_index;
	//int min_size;
	//int max_size;
	unsigned long min_size;
	unsigned long max_size;
}record_query_condition_s;

int anj_record_init(const char *filePath, int iMaxPartition, int bRemountRecover);

int anj_record_uninit();

int anj_record_start(int iRecChannel);

int anj_record_stop(int iRecChannel);

int anj_record_check_valid();

int anj_record_fallocate(const char *filePath, int iMaxPartition);

rec_status_e anj_record_status_get();

int anj_record_start_event(int iRecChannel, rec_event_mask_e tEvent);

int anj_record_stop_event(int iRecChannel, rec_event_mask_e tEvent);

int anj_record_alarm_handle(int iRecChannel, int alarm_code, int alarm_level);

int anj_record_pb_is_valid(REC_HANDLE hPoperHandle);

int anj_record_pb_start_event(int iRecChannel, rec_event_mask_e tEvent);

int anj_record_pb_stop_event(int iRecChannel, rec_event_mask_e tEvent);

int anj_record_pb_query_mounth(int iRecChannel, rec_pb_date_s *pstPbDate);

int anj_record_pb_query_day_create(int iRecChannel, rec_pb_date_s *pstPbDate, rec_pb_list_s* pstPbList);

int anj_record_pb_query_day_release(int iRecChannel, rec_pb_list_s* pstPbList);

REC_HANDLE anj_record_pb_create(int iRecChannel, unsigned int tStartTime, unsigned int tEndTime, unsigned int iEvenType, int iPopId, rec_pb_cb pbCb);

int anj_record_pb_release(REC_HANDLE pHandle);

int anj_record_pb_pause_set(REC_HANDLE pHandle, int bPause);

int anj_record_pb_speed_set(REC_HANDLE pHandle, int iSpeed);

int anj_record_pb_download_set(REC_HANDLE pHandle, int iDownLoad);

int anj_record_pb_seek(REC_HANDLE pHandle, unsigned int tSeekTime);

int anj_record_pb_frame_type(REC_HANDLE pHandle, int bKeyFrame);

int anj_record_max_file_get(int *bFull, int *nextFileNo);
int anj_record_get_media_file_name(char *fileName, int fileNameLen, int iFileNo);
int anj_record_lastest_time(struct tm *tm_rec);

rec_file_index_record *anj_record_file_info_get(int iFileNo);

void anj_record_restart();

void anj_record_pb_download_mp4(int iRecChannel, char *filename, unsigned int tStartTime, unsigned int tEndTime, int timelapse);

#ifdef __cplusplus
}
#endif

#endif
