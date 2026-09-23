#ifndef __ANJ_EVENTHUB_H__
#define __ANJ_EVENTHUB_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

#define EVENTHUB_LOAD_ISPBIN "LOAD_ISPBIN"

#define EVENTHUB_SMART_MD_PROCESS "SMART_MD_PROCESS"
#define EVENTHUB_SMART_PD_PROCESS "SMART_PD_PROCESS"

#define EVENTHUB_PTZ_TRACK "PTZ_TRACK"
#define EVENTHUB_PTZ_HANDLE "PTZ_HANDLE"
#define EVENTHUB_PTZ_DIRECTION "PTZ_DIRECTION"
#define EVENTHUB_PTZ_CHANGE_SPEED "PTZ_CHANGE_SPEED"
#define EVENTHUB_PTZ_MOVE_STATUS "PTZ_MOVE_STATUS"

#define EVENTHUB_AOV_NOTIFY_ENCODE      "AOV_NOTIFY_ENCODE"
#define EVENTHUB_AOV_NOTIFY_ALGO        "AOV_NOTIFY_ALGO"
#define EVENTHUB_AOV_TRIGGER_HUMAN      "AOV_TRIGGER_HUMAN"

#define EVENTHUB_ALARM_SEND_MOTORCOORDI "ALARM_SEND_MOTORCOORDI"
#define EVENTHUB_NFS_ALARM "NFS_ALARM"
#define EVENTHUB_NFS_RESTART "NFS_RESTART"

typedef struct
{
    int chn;
    int code;
    int level;
} event_alarm_s;

#define EVENTHUB_4G_OSD_SET "4G_OSD_SET"
#define EVENTHUB_4G_SIM_SET "4G_SIM_SET"
#define EVENTHUB_4G_STATUS_GET "4G_STATUS_GET"
#define EVENTHUB_4G_LOCATION_SET "4G_LOCATION_SET"

#define EVENTHUB_WIFI_QUALITY_GET "WIFI_QUALITY_GET"
#define EVENTHUB_WIFI_CONNECT_SET "WIFI_CONNECT_SET"

#define EVENTHUB_ZXING_SET_IMAGE "ZXING_SET_IMAGE"
#define EVENTHUB_ZXING_SET_STATUS "ZXING_SET_STATUS"

typedef struct
{
    void *data; /* Y 平面 */
    int width;
    int height;
} event_yuv_s;

#define EVENTHUB_WAVE_SEND_DATA "WAVE_SEND_DATA"
#define EVENTHUB_WAVE_SET_STATUS "WAVE_SET_STATUS"
#define EVENTHUB_WAVE_GET_STATUS "WAVE_GET_STATUS"

#define EVENTHUB_RTSP_RESTART "RTSP_RESTART"
#define EVENTHUB_RTMP_RESTART "RTMP_RESTART"
#define EVENTHUB_H5_RESTART "H5_RESTART"
#define EVENTHUB_ONVIF_RESTART "ONVIF_RESTART"
#define EVENTHUB_WEB_RESTART "WEB_RESTART"
#define EVENTHUB_HIK_RESTART "HIK_RESTART"
#define EVENTHUB_UNV_RESTART "UNV_RESTART"

#define EVENTHUB_GB28181_UPDATE "GB28181_UPDATE"

#define EVENTHUB_HTTP_UNV_GET_PROBE_STATUS  "HTTP_UNV_GET_PROBE_STATUS"

#define EVENTHUB_BATTERY_CAP_GET    "BATTERY_CAP_GET"
#define EVENTHUB_BATTERY_CHARGE_GET "BATTERY_CHANGE_GET"
#define EVENTHUB_GYRO_DATA_GET      "GYRO_DATA_GET"
#define EVENTHUB_NFS_STORAGE_GET    "NFS_STORAGE_GET"

typedef struct
{
    int mounted;
    int total;   /* MB */
    int used;
    int free;
    int percent;
} event_nfs_storage_s;

#define MAX_EVENT_RECT_NUM (20)
#define MAX_EVENT_LINE_NUM (10)

#define EVENT_ALARM_TYPE_MD                 (1)
#define EVENT_ALARM_TYPE_AI_VG              (2)
#define EVENT_ALARM_TYPE_AI_PD              (3)
#define EVENT_ALARM_TYPE_VEDIA_COVER        (4)
#define EVENT_ALARM_TYPE_AUDIO_LSA          (5)
#define EVENT_ALARM_TYPE_AUDIO_CRY          (6)


typedef struct
{
    int Chn;
    int VencId;
} request_idr_info;

typedef struct
{
    int pos_x;
    int pos_y;
    int width;
    int height;
    int u32Color;
} event_rect_s;

typedef struct
{
    event_rect_s event_rect[MAX_EVENT_RECT_NUM];
    int type;
    int s32RectCnt;
    int camera;
    int move;
    int bShow;
} event_rect_param_s;

enum AUDIO_PLAY_ACTION
{
    AUDIO_PLAY_ACTION_NONE = 0,
    AUDIO_PLAY_ACTION_WAIT_PREV,        // 等待上个语音播放完成
    AUDIO_PLAY_ACTION_SKIP_IF_PLAY      // 如果正在播放则放弃
};

typedef struct
{
    char *data;
    int len;
} event_data_s;

typedef struct
{
    int quality;     // 质量
    int maxquality;  // 最大质量
    int signalLevel; // 信号强度
} WIFI_QUALITY;

typedef enum
{
    AOV_NOTIFY_ENC_START = 0,
    AOV_NOTIFY_ENC_DONE,
    AOV_NOTIFY_ENC_CHANGE
} aov_notify_enc_e;

typedef enum
{
    AOV_NOTIFY_ALGO_START = 0,
    AOV_NOTIFY_ALGO_DONE,
    AOV_NOTIFY_ALGO_CHANGE
} aov_notify_algo_e;

typedef struct
{
    aov_notify_enc_e event;
    int status;
}aov_notify_enc_t;

typedef struct
{
    aov_notify_algo_e event;
    int status;
}aov_notify_algo_t;

#define MIN_TRANS_PTZ_COMMAND 4

#define MAX_TRANSPARENT_CMD 128
#define MAX_INNNER_ENCODE_CMD 64
typedef struct
{
    int datalen;
    char buffer[MAX_TRANSPARENT_CMD];
} PtzTransCmd;

typedef struct ptz_cmd_parse
{
    char ptzCmd[32];
    char presetName[64];
    unsigned char panSpeed;
    unsigned char tiltSpeed;
    unsigned int watchGuardTime;
    unsigned int manualLocateStatus; /*1：放大拉框，2：缩小拉框*/
    int presetID;
    int flag;
    int posX;
    int posY;
    int rectLeftTopX;
    int rectLeftTopY;
    int rectRightDownX;
    int rectRightDownY;
    int bTrachEnable;
    int trackTime;
    PtzTransCmd trans;
} PtzCmdParse;

typedef struct
{
    int ret;
    void *result;
} EventResult;

typedef struct
{
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float accel_x;
    float accel_y;
    float accel_z;
    float temp;
} event_gyro_data_t;

typedef void (*EventHandler)(EventResult *event_result, void *data);

typedef struct EventSubscription
{
    const char *event_name;
    EventHandler handler;
    struct EventSubscription *next;
} EventSubscription;

/* 事件总线结构体 */
typedef struct EventHub
{
    EventSubscription *subscriptions;
    int event_count;
} EventHub;

/* 总线分类 */
typedef enum
{
    EVENTHUB_CLASS_CTRL = 0,    /* 实时控制总线：PTZ、AOV、SMART、ALARM 等 */
    EVENTHUB_CLASS_STATUS,      /* 状态查询总线：4G、WiFi、Battery 等 */
    EVENTHUB_CLASS_MEDIA,       /* 媒体/配置总线：RTSP、GB28181、Wave、ZXing 等 */

    EVENTHUB_CLASS_MAX
} EventHubClass;

void eventhub_init(void);
void eventhub_uninit(void);

void eventhub_ctrl_init(void);
void eventhub_status_init(void);
void eventhub_media_init(void);

void eventhub_ctrl_uninit(void);
void eventhub_status_uninit(void);
void eventhub_media_uninit(void);

void eventhub_subscribe(EventHubClass cls, const char *event_name, EventHandler handler);
void eventhub_publish(EventHubClass cls, const char *event_name, EventResult *event_result, void *data);
bool eventhub_unsubscribe(EventHubClass cls, const char *event_name, EventHandler handler);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
