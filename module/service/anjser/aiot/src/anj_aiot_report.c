#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_systime.h"
#include "anj_net.h"
#include "anj_smart.h"
#include "anj_ser_api.h"
#include "anj_aiot_report.h"
#include "anj_aiot_cmd.h"

#include "protocol_queue.h"
#include "alarm_link.h"
#include "cJSON.h"

#include "gct_apiv4.h"
#include "gct_common.h"
#include "gct_dd_apiv4.h"

#include <time.h>

#define REPORT_ALARM_INTERVAL (120000)
#define PROP_CLOUD_CHN 0              /* 属性写云存通道号，单目固定 0 */
#define PROP_CLOUD_PERIOD_S 60        /* 属性/统计写云存周期，秒 */
#define PROP_CLOUD_RETRY_MS 10 * 1000 /* 增量上传失败后最短重试间隔，毫秒 */

typedef struct
{
    int alarm_code;             // AJ报警类型
    int alarm_sub_code;         // AJ报警子类型
    int report_event_type;      // 上报event类型
    const char *report_payload; // 上报event描述/名称
    int picture_enable;
    int uploadcloud_enable;   // 是否触发云存储
    unsigned int interval_ms; // 间隔时间
} AlarmReportDef_t;

typedef struct
{
    int event_type;
    unsigned long long report_time; // 事件上报时间
} AlarmReportInfo_t;

typedef struct
{
    int report_def_cnt;
    AlarmReportDef_t *alarm_report_def;
    AlarmReportInfo_t *alarm_report_info;
} AlarmReportManager_t;

/*
    移动侦测：推送图片、触发云存、推送间隔120s
    IO报警：不推送图片、触发云存、推送间隔3600s
    视频遮挡：不推送图片、不触发云存、推送间隔300s
    人形侦测：推送图片、触发云存、推送间隔120s
    车辆检测：推送图片、触发云存、推送间隔120s
    摩托车检测：推送图片、触发云存、推送间隔120s
    电单车：推送图片、触发云存、推送间隔120s
    自行车：推送图片、触发云存、推送间隔120s
    车牌检测：推送图片、触发云存、推送间隔120s
    越界检测：推送图片、触发云存、推送间隔120s
    烟火检测：推送图片、触发云存、推送间隔120s
    人脸识别   ：推送图片、触发云存、推送间隔120s
    区域侦测：推送图片、触发云存、推送间隔120s
    高空抛物检测：推送图片、触发云存、推送间隔120s
    跌倒：推送图片、触发云存、推送间隔120s
    一键呼叫：推送图片、触发云存、推送间隔0
*/
static AlarmReportDef_t s_stAlarmReportInfo [] =
{
    {ALARM_CODE_MOTION_DETECT, -1, GAT_MOTION_DETECT, "motiondetect", 1, 1, REPORT_ALARM_INTERVAL,},
    {ALARM_CODE_IO_ALARM, -1, GAT_IO_ALARM, "io alarm", 0, 1, 3600000,},
    {ALARM_CODE_VIDEO_COVERD, -1, GAT_VIDEO_SHEILD, "video covered", 0, 0, 300000,},

    {ALARM_CODE_VIDEO_AI, ALARM_AI_PD, GAT_HUMANOID_FRAME, NULL, 1, 1, REPORT_ALARM_INTERVAL,},
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VEHICLE_CAR, GAT_AI_CAR,	NULL, 1, 1, REPORT_ALARM_INTERVAL,},//车辆检测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VEHICLE_MOTO, GAT_AI_MOTO,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//摩托车
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VEHICLE_ELECTRICBICYCLE, GAT_AI_ELECTRICBICYCLE, NULL, 1, 1, REPORT_ALARM_INTERVAL,},//电单车
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VEHICLE_BICYCLE, GAT_AI_BICYCLE,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//自行车
    {ALARM_CODE_VIDEO_AI, ALARM_AI_LPR, GAT_AI_Lpr,	NULL, 1, 1, REPORT_ALARM_INTERVAL,},//车牌检测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_GATE, GAT_BORDER_DETECT, NULL, 1, 1, REPORT_ALARM_INTERVAL,},//越界检测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_FIRE, GAT_SMOKE_ALARM,	NULL, 1, 1, REPORT_ALARM_INTERVAL,},//烟火检测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_FACEDETECT, GAT_AI_FD,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//人脸识别
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_REGION_DETECT_ENTER, GAT_AI_REGION_DETECT_ENTER,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//区域侦测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_REGION_DETECT_LEAVE, GAT_AI_REGION_DETECT_LEAVE,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//区域侦测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_REGION_DETECT_STAY, GAT_AI_REGION_DETECT_STAY,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//区域侦测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_FALLINGOBJECT, GAT_AI_FALLINGOBJECT,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//高空抛物检测
    {ALARM_CODE_VIDEO_AI, -1, GAT_AI_FALLHUMAN,  NULL, 1, 1, REPORT_ALARM_INTERVAL,},//跌倒
    //{ALARM_CODE_VIDEO_AI, -1, GAT_HUMANOID_FRAME, NULL, 1, 1, REPORT_ALARM_INTERVAL,},//其他默认为人形 -> 影响计算去掉

    {ALARM_CODE_KEY_PRESS, -1, GAT_RINGING_ALARM, "call", 1, 1, 0,},
};

static AlarmReportManager_t s_stAlarmReportManager = {0};

static FRAME_BUFFER_MANAGER s_stAiotReportMgr = {0};
static anj_thread_s s_stAiotReportThread = {0};
static anj_thread_s s_stPropCloudThread = {0};
static unsigned long long s_tLastPropUploadError = 0;

/* 云存字段名，顺序必须与 anj_aiot_stat_e 一致；本周期 take 后清零，由后台累加 */
static const char *s_stat_name[ANJ_AIOT_STAT_MAX] =
{
    "aidetect_cnt",
    "preview_cnt",
    "preview_duration",
    "playback_cnt",
    "playback_duration",
};
static unsigned long long s_stat_val[ANJ_AIOT_STAT_MAX] = {0};
static pthread_mutex_t s_stat_mutex = PTHREAD_MUTEX_INITIALIZER;

void anj_aiot_stat_add(anj_aiot_stat_e id, unsigned long long val)
{
    if (id < 0 || id >= ANJ_AIOT_STAT_MAX)
        return;

    anj_mutex_lock(&s_stat_mutex);
    s_stat_val[id] += val;
    anj_mutex_unlock(&s_stat_mutex);
}

static int anj_aiot_stat_dirty(void)
{
    int i = 0;
    int bDirty = 0;

    anj_mutex_lock(&s_stat_mutex);
    for (i = 0; i < ANJ_AIOT_STAT_MAX; i++)
    {
        if (s_stat_val[i] != 0)
        {
            bDirty = 1;
            break;
        }
    }
    anj_mutex_unlock(&s_stat_mutex);
    return bDirty;
}

/* 取出当前周期统计并清零；全 0 返回 NULL，不上报（连续空周期不刷脏） */
static cJSON *anj_aiot_stat_take(void)
{
    int i = 0;
    int bHas = 0;
    unsigned long long vals[ANJ_AIOT_STAT_MAX];
    cJSON *pStat = NULL;

    anj_mutex_lock(&s_stat_mutex);
    for (i = 0; i < ANJ_AIOT_STAT_MAX; i++)
    {
        vals[i] = s_stat_val[i];
        s_stat_val[i] = 0;
    }
    anj_mutex_unlock(&s_stat_mutex);

    pStat = cJSON_CreateObject();
    if (pStat == NULL)
        return NULL;

    for (i = 0; i < ANJ_AIOT_STAT_MAX; i++)
    {
        if (vals[i] == 0)
            continue;
        cJSON_AddNumberToObject(pStat, s_stat_name[i], (double)vals[i]);
        bHas = 1;
    }

    if (!bHas)
    {
        cJSON_Delete(pStat);
        return NULL;
    }
    return pStat;
}

/* 把统计键写入 JSON 的 prop；pJson 为空则构造 {"ts":...,"prop":{统计}}，成功则释放原 pJson */
static char *anj_aiot_report_json_add_stat(char *pJson, cJSON *pStat)
{
    cJSON *pRoot = NULL;
    cJSON *pProp = NULL;
    cJSON *pChild = NULL;
    char *pOut = NULL;

    if (pStat == NULL || pStat->child == NULL)
        return pJson;

    if (pJson == NULL)
    {
        pRoot = cJSON_CreateObject();
        if (pRoot == NULL)
            return NULL;
        pProp = cJSON_CreateObject();
        if (pProp == NULL)
        {
            cJSON_Delete(pRoot);
            return NULL;
        }
        cJSON_AddNumberToObject(pRoot, "ts", (double)time(NULL));
        cJSON_AddItemToObject(pRoot, "prop", pProp);
    }
    else
    {
        pRoot = cJSON_Parse(pJson);
        if (pRoot == NULL)
            return pJson;
        free(pJson);
        pProp = cJSON_GetObjectItem(pRoot, "prop");
        if (pProp == NULL)
        {
            pProp = cJSON_CreateObject();
            if (pProp == NULL)
            {
                cJSON_Delete(pRoot);
                return NULL;
            }
            cJSON_AddItemToObject(pRoot, "prop", pProp);
        }
    }

    pChild = pStat->child;
    while (pChild != NULL)
    {
        cJSON *pAdd = cJSON_Duplicate(pChild, 1);
        if (pAdd != NULL && pAdd->string != NULL)
        {
            cJSON_DeleteItemFromObject(pProp, pAdd->string);
            cJSON_AddItemToObject(pProp, pAdd->string, pAdd);
        }
        else if (pAdd != NULL)
        {
            cJSON_Delete(pAdd);
        }
        pChild = pChild->next;
    }

    pOut = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return pOut;
}

static int anj_aiot_report_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    while (bStart && *bStart)
    {
        FRAME_ENTRY frame = {0};
        if (frame_mgr_pop(&s_stAiotReportMgr, &frame) > 0)
        {
            gct_dd_apiv4_push_alarm(frame.session, (GCT_ALARM_TYPE)frame.nFrameType, frame.nFlag, NULL, 0);
            if (frame.pFrame)
            {
                anj_mw_free(frame.pFrame);
            }
        }

        usleep(100 * 1000);
    }
    return iRet;
}

/* 无增量时上报仅含 ts 的心跳，维持云存属性在线 */
static void anj_aiot_report_property_heartbeat(void)
{
    cJSON *pRoot = NULL;
    char *pJsonText = NULL;
    int ret = 0;

    pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
        return;

    cJSON_AddNumberToObject(pRoot, "ts", (double)time(NULL));
    pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);

    if (pJsonText != NULL && strlen(pJsonText) > 0)
    {
        __INFO("property heartbeat: %s\n", pJsonText);
        ret = gct_apiv4_upload_properties_change(pJsonText, PROP_CLOUD_CHN);
        if (ret < 0)
        {
            __ERR("property heartbeat upload failed: %d\n", ret);
        }
        free(pJsonText);
    }
}

static void anj_aiot_report_property_exec(void)
{
    char *pChangeJson = NULL;
    char *pFullJson = NULL;
    unsigned long long nowtime = GetCurrentTimeStampU64();
    int ret = 0;
    cJSON *pStat = NULL;
    int bStatDirty = 0;

    pChangeJson = anj_aiot_cmd_property_change_json();
    bStatDirty = anj_aiot_stat_dirty();
    /* 无设备属性增量且无统计：已有快照才发 {ts} 心跳，避免空属性写云 */
    if (pChangeJson == NULL && bStatDirty == 0)
    {
        if (anj_aiot_cmd_property_has_data())
            anj_aiot_report_property_heartbeat();
        return;
    }

    if (nowtime - s_tLastPropUploadError < PROP_CLOUD_RETRY_MS)
    {
        if (pChangeJson)
            free(pChangeJson);
        return;
    }

    pStat = anj_aiot_stat_take();
    pChangeJson = anj_aiot_report_json_add_stat(pChangeJson, pStat);
    pFullJson = anj_aiot_cmd_property_full_json();
    pFullJson = anj_aiot_report_json_add_stat(pFullJson, pStat);
    if (pStat)
        cJSON_Delete(pStat);

    if (pChangeJson != NULL)
        __INFO("property change: %s\n", pChangeJson);

    /* 先 full 再 change，时序由 SDK 双快照保证 */
    if (pFullJson != NULL)
    {
        gct_apiv4_upload_properties_full(pFullJson, PROP_CLOUD_CHN);
        free(pFullJson);
    }

    if (pChangeJson == NULL)
        return;

    ret = gct_apiv4_upload_properties_change(pChangeJson, PROP_CLOUD_CHN);
    if (ret >= 0)
    {
        s_tLastPropUploadError = 0;
        anj_aiot_cmd_property_clear_change();
    }
    else
    {
        s_tLastPropUploadError = nowtime;
        __ERR("property change upload failed: %d\n", ret);
    }
    free(pChangeJson);
}

static int anj_aiot_report_property_thread(void *ctx, int *bStart)
{
    int iWait = 0;

    (void)ctx;
    while (bStart && *bStart)
    {
        sleep(1);
        if (!(bStart && *bStart))
            break;
        iWait++;
        if (iWait < PROP_CLOUD_PERIOD_S)
            continue;
        iWait = 0;
        anj_aiot_report_property_exec();
    }
    return 0;
}

void anj_aiot_report_alarm_manger_init()
{
    s_stAlarmReportManager.report_def_cnt = sizeof(s_stAlarmReportInfo) / sizeof(s_stAlarmReportInfo[0]);
    s_stAlarmReportManager.alarm_report_def = s_stAlarmReportInfo;
    s_stAlarmReportManager.alarm_report_info = (AlarmReportInfo_t *)anj_mw_malloc(s_stAlarmReportManager.report_def_cnt * sizeof(AlarmReportInfo_t));
}

int anj_aiot_report_init()
{
    int iRet = 0;

    anj_aiot_report_alarm_manger_init();
    frame_mgr_init(&s_stAiotReportMgr, 20);

    s_stAiotReportThread.bAutoDestroy = 1;
    strncpy(s_stAiotReportThread.iThreadName, "aiot_report", sizeof(s_stAiotReportThread.iThreadName) - 1);
    s_stAiotReportThread.iThreadjob.ctx = &s_stAiotReportThread;
    s_stAiotReportThread.iThreadjob.func = anj_aiot_report_thread;
    iRet = anj_thread_task_create(&s_stAiotReportThread);

    gct_apiv4_upload_properties_set_enable(TRUE); /* SDK 默认关，需显式打开属性写云存 */
    s_stPropCloudThread.bAutoDestroy = 1;
    strncpy(s_stPropCloudThread.iThreadName, "prop_cloud", sizeof(s_stPropCloudThread.iThreadName) - 1);
    s_stPropCloudThread.iThreadjob.ctx = &s_stPropCloudThread;
    s_stPropCloudThread.iThreadjob.func = anj_aiot_report_property_thread;
    anj_thread_task_create(&s_stPropCloudThread);
    return iRet;
}

void anj_aiot_report_uninit()
{
    anj_thread_task_destroy(&s_stPropCloudThread, -1);
    anj_thread_task_destroy(&s_stAiotReportThread, -1);
    frame_mgr_release(&s_stAiotReportMgr);
}

void anj_aiot_report_push(int nChannelNo, int nAlarmType, int bUploadCloud, const char *event_payload)
{
    FRAME_ENTRY frame = {0};
    frame.pFrame = (char *)anj_mw_malloc(strlen(event_payload));
    memcpy(frame.pFrame, event_payload, strlen(event_payload));
    frame.nFrameLen = strlen(event_payload);
    frame.session = nChannelNo;
    frame.nFrameType = nAlarmType;
    frame.nFlag = bUploadCloud;
    frame_mgr_push(&s_stAiotReportMgr, &frame);
}

AlarmReportDef_t *anj_aiot_report_alarm_handle_find(int alarm_code, int alarm_sub_code)
{
    int i = 0;

    // 先精确匹配
    for (i = 0; i < s_stAlarmReportManager.report_def_cnt; i++)
    {
        if (s_stAlarmReportManager.alarm_report_def[i].alarm_code == alarm_code &&
            s_stAlarmReportManager.alarm_report_def[i].alarm_sub_code == alarm_sub_code)
        {
            return &s_stAlarmReportManager.alarm_report_def[i];
        }
    }

    // 再模糊匹配报警码
    for (i = 0; i < s_stAlarmReportManager.report_def_cnt; i++)
    {
        if (s_stAlarmReportManager.alarm_report_def[i].alarm_code == alarm_code)
        {
            return &s_stAlarmReportManager.alarm_report_def[i];
        }
    }

    return NULL;
}

int anj_aiot_report_io_alarm_clear()
{
    int i = 0;
    for (i = 0; i < s_stAlarmReportManager.report_def_cnt; i++)
    {
        if (s_stAlarmReportManager.alarm_report_info[i].event_type == GAT_IO_ALARM)
        {
            s_stAlarmReportManager.alarm_report_info[i].report_time = 0;
            break;
        }
    }

    return 0;
}

int anj_aiot_report_event_interval_check(int event_type, int report_interval)
{
    int i = 0;
    int check_flag = 0;
    unsigned long long time_diff = 0;
    unsigned long long current_time = GetCurrentTimeStamp();

    for (i = 0; i < s_stAlarmReportManager.report_def_cnt; i++)
    {
        if (s_stAlarmReportManager.alarm_report_info[i].event_type == event_type)
        {
            time_diff = current_time - s_stAlarmReportManager.alarm_report_info[i].report_time;
            if (time_diff >= report_interval)
            {
                check_flag = 1;
                s_stAlarmReportManager.alarm_report_def[i].interval_ms = current_time;
            }

            break;
        }
    }

    return check_flag;
}

int anj_aiot_report_alarm_handle(int chn, int alarm_code, int alarm_level, char *alarm_data)
{
    AlarmReportDef_t *alarm_def_handle = NULL;

    /* 移动侦测 / 智能分析 / 车牌 / 越界 计入 AI 次数 */
    if (alarm_code == ALARM_CODE_MOTION_DETECT ||
        alarm_code == ALARM_CODE_VIDEO_AI ||
        alarm_code == ALARM_CODE_LPR ||
        alarm_code == ALARM_CODE_VIDEO_GATE)
    {
        anj_aiot_stat_add(ANJ_AIOT_STAT_AIDETECT_CNT, 1);
    }

    alarm_def_handle = anj_aiot_report_alarm_handle_find(alarm_code, alarm_level);
    if (alarm_def_handle == NULL)
    {
        __ERR("aiot report don't find alarm handle for:code:%d, level:%d\n", alarm_code, alarm_level);
        return -1;
    }

    // 特殊处理：IO报警结束
    if (alarm_code == ALARM_CODE_IO_ALARM_FINISH)
    {
        anj_aiot_report_io_alarm_clear();
    }

    // 特殊处理：响铃报警
    if (alarm_def_handle->report_event_type == GAT_RINGING_ALARM)
    {
        // todo
    }

/* 暂时屏蔽 不再判断2分钟的间隔而是判断是否连续报警 */
#if 0
    int report_interval = alarm_def_handle->interval_ms;

    // 检测是否满足p2p报警间隔
    int check_time = anj_aiot_report_event_interval_check(alarm_def_handle->report_event_type, report_interval);
    if (check_time == 0)
    {
        __ERR("aiot report check event type:%d interval:%d failed\n", alarm_def_handle->report_event_type, report_interval);
        return -2;
    }
#endif

    const char *event_payload = alarm_def_handle->report_payload;
    if (event_payload == NULL || *event_payload == 0)
    {
        event_payload = alarm_data;
    }

    anj_aiot_report_push(chn, alarm_def_handle->report_event_type, alarm_def_handle->uploadcloud_enable, event_payload);

    return 0;
}
