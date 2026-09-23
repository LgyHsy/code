#ifndef _ANJ_CONFIG_RECORD_H_
#define _ANJ_CONFIG_RECORD_H_

#include "ixml.h"
#include "anj_mw_time.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RECORD_FILEFORMAT_MAX_LEN 32

typedef struct
{
    char formatName[RECORD_FILEFORMAT_MAX_LEN];
} RecordFileFormat;

#define RECORD_MEDIA_TYPE_MAX_LEN 32
typedef struct
{
    char typeName[RECORD_MEDIA_TYPE_MAX_LEN];
} RecordMediaType;

#define RECORD_STORAGE_POLICY_MAX_LEN 32
typedef struct
{
    char policyName[RECORD_STORAGE_POLICY_MAX_LEN];
} RecordStoragePolicy;

#define MAX_STORAGE_SEQUENCE_NAME_LEN 256
#define MAX_REMOTE_MOUNT_PARAM_LEN 256

typedef enum
{
    NETWORK_STORAGE_DISABLE = 0,
    NETWORK_STORAGE_TYPE_NFS = 1,
} NetworkStorageType;

typedef struct
{
    int timelapseEnable;   // 缩时摄影启用与否
    int timelapseSec;      // 间隔多少秒录制一帧
    int timelapseFps;      // 播放帧率
    int timelapseFileSize; // 缩时摄影文件大小
} RecordTimelapseConfig;

typedef struct
{
    int localEnable;
    NetworkStorageType remoteEnable;
    char storageSequence[MAX_STORAGE_SEQUENCE_NAME_LEN];
    char mountParam[MAX_REMOTE_MOUNT_PARAM_LEN];
    RecordStoragePolicy storePolicy;
    int recordFileSize;
    int recordFileKeeyDays; // 录像保留最大天数

    RecordTimelapseConfig timelapseCfg; // 缩时摄影配置
} RecordCommConfig;

typedef enum
{
    RECORD_SCHED_VIDEO_MAIN = 1,     // 主码流录像
    RECORD_SCHED_VIDEO_AUX = 2,      // 子码流录像
    RECORD_SCHED_JPG_MAIN = 3,       // 主码流抓图
    RECORD_SCHED_JPG_AUX = 4,        // 子码流抓图
    RECORD_SCHED_VIDEO_DUAL = 5,     // 双码流录像
    RECORD_SCHED_VIDEO_JPG_MAIN = 6, // 主码流录像抓图
    RECORD_SCHED_VIDEO_JPG_AUX = 7,  // 子码流抓图+抓图
    RECORD_SCHED_VIDEO_JPG_DUAL = 8  // 双码流录像+抓图
} RecordSchedStreamType;

typedef struct
{
    int stream; // RecordSchedStreamType
    RecordFileFormat fileFormat;
    RecordMediaType mediaType;
    int localStore;
    int remoteStore;
    TimeSpanCfg timeSpan;

    int jpgInterval;
    int ftpUpload;
    int emailUpload;
} ScheduleRecordConfig;

typedef enum
{
    RECORD_ALARM_BIT_MOTION_DETECT = 0,   // 移动侦测
    RECORD_ALARM_BIT_HUMAN = 1,           // 人形
    RECORD_ALARM_BIT_CAR = 2,             // 汽车
    RECORD_ALARM_BIT_MOTO = 3,            // 摩托车
    RECORD_ALARM_BIT_ELECTRICBICYCLE = 4, // 电单车
    RECORD_ALARM_BIT_BICYCLE = 5,         // 自行车
    RECORD_ALARM_BIT_FACE = 6,            // 人脸
    RECORD_ALARM_BIT_NONMOTO_VEHICLE = 7, // 非机动车
    RECORD_ALARM_BIT_FIRE = 8,            // 火焰
    RECORD_ALARM_BIT_FALLINGOBJECT = 9,   // 高空抛物
    RECORD_ALARM_BIT_LPR = 10,            // 车牌识别
    RECORD_ALARM_BIT_VIDEO_COVERD = 11,
    RECORD_ALARM_BIT_VIDEO_GATE = 12,    // 电子围栏
    RECORD_ALARM_BIT_AUDIO_BABYCRY = 13, // 婴儿啼哭
    RECORD_ALARM_BIT_AUDIO_LSA = 14,     // 高分贝声音
    RECORD_ALARM_BIT_IO_ALARM = 15,      // IO输入报警
    RECORD_ALARM_BIT_EMERGENCY_CALL = 16,
    RECORD_ALARM_BIT_TEMP_HUMID_ALARM = 17, // 温湿度告警
    RECORD_ALARM_BIT_SENSOR = 18,           // 传感器报警，使用AlarmSensor作为子类型
    RECORD_ALARM_BIT_LINKDOWN = 19,

    RECORD_ALARM_BIT_MAX,
} AlarmRecordBits;

typedef struct
{
    int stream; // 0:主码流 1:子码流 2:主子码流同时录像
    RecordFileFormat fileFormat;
    RecordMediaType mediaType;
    int precordTime;
    int recordTime;
    int localStore;
    int remoteStore;
    int ftpUpload;
    int emailUpload;
    int stopNoAlarm;        // if noalarmstop=1, recordTime means the record time after alarm disappear
    unsigned int alarmbits; // 报警类型,每一种报警占用1个bit，参见AlarmRecordBits
} AlarmRecordConfig;

typedef struct
{
    short preTakeTime;
    short sendoutInterval; // FTP/EMAIL发送频率,秒.0标识不控制
    int totalTakeTime;
    int localStore;
    int remoteStore;
    int ftpUpload;
    int emailUpload;
    int stopNoAlarm;        // if noalarmstop=1, recordTime means the record time after alarm disappear
    int stream;             // 抓图使用的码流
    unsigned int alarmbits; // 报警类型,每一种报警占用1个bit，参见AlarmRecordBits
} AlarmCaptureConfig;

typedef struct
{
    RecordCommConfig commonCfg;
    ScheduleRecordConfig scheduleRecordCfg;
    AlarmRecordConfig motionRecordCfg;
    AlarmCaptureConfig motionCaptureCfg;
    AlarmRecordConfig inputAlarmRecordCfg;
    AlarmCaptureConfig inputAlarmCaptureCfg;
} RecordConfig;

char *anj_config_record_conver_xml(RecordConfig *pRecordCfgArray, int camera_index, int bMsg);

int anj_config_record_get(IXML_Node *pNode, RecordConfig *pRecordCfgArray);

int anj_config_record_load(RecordConfig *pRecordCfgArray);

int anj_config_record_save(RecordConfig *pRecordCfgArray);

int anj_config_record_set(RecordConfig *pRecordCfgArray);

int anj_config_record_get_by_xml(RecordConfig *pRecordCfg, char *xmlBuf, int channel);

#ifdef __cplusplus
}
#endif

#endif
