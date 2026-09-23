#include "anj_mw_comm.h"
#include "zlib.h"
#include "anj_module.h"
#include "anj_config.h"
#include "anj_config_ptz.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_systime.h"
#include "anj_net.h"
#include "anj_ispctl.h"
#include "anj_smart.h"
#include "anj_sdcard.h"
#include "anj_service.h"
#include "anj_ser_api.h"
#include "anj_smart.h"
#include "anj_osd.h"
#include "anj_video.h"
#include "anj_record.h"
#include "anj_aiot_cmd.h"
#include "anj_aiot_trans.h"
#include "record_log.h"

#include "eventhub.h"
#include "ota_update.h"
#include "function_list.h"
#include "protocol_queue.h"
#include "cJSON.h"

#include "gct_apiv4.h"
#include "gct_common.h"
#include "dev_bind.h"

#define P2P_ID_SENDEVENT_FREQ "/mnt/nand/sendevent_freq" // 设备发送报警的频率, 按照秒计算

#define AIOT_OSS_LINK "http://download.icamra.com:8021/debug/i6c/oss_ali"
/*
后续添加功能，在这里加能力。
20200927: ipc_update_online  APP设置中显示系统信息项,可获取IPC版本信息,并远程升级IPC
20210205: monitor_support    布防撤防支持
*/
#define EXTRA_CAPABILITIES "+ipc_update_online+monitor_support+replay_speed"

#define AIOT_CMD_RECORD_MODE "StorageRecordMode"
#define AIOT_CMD_ALARM_SWITCH "AlarmSwitch"
#define AIOT_CMD_PTZ_SPEED "PtzStepInterval"
#define AIOT_CMD_PTZ_STATUS "PtzStatus"
#define AIOT_CMD_IMAGE_FLIP "ImageFlipState"
#define AIOT_CMD_LENS_COVER "LensCover"
#define AIOT_CMD_POWER_LIGHT "pilotPowerLight"
#define AIOT_CMD_MCU_VERSION "mcuversion"
#define AIOT_CMD_WEAK_LIGHT "WeakLightSwitch"
#define AIOT_CMD_4G_CARD "work4gcard"
#define AIOT_CMD_STREAM_MODE "StreamWorkMode"
#define AIOT_CMD_INTERCOM_TYPE "VoiceIntercomType"
#define AIOT_CMD_WDR_SWITCH "WDRswitch"
#define AIOT_CMD_DAYNIGHT_MODE "DayNightMode"
#define AIOT_CMD_MIC_SWITCH "MicSwitch"
#define AIOT_CMD_MIC_VOLUME "MicVolume"
#define AIOT_CMD_SPEAKER_VOLUME "SpeakerVolume"
#define AIOT_CMD_SHUTDOWN_SWITCH "ShutdownSwitch"
#define AIOT_CMD_ALARM_SOUND_LEVEL "AlarmSoundLevel"
#define AIOT_CMD_SHUTDOWN_PLAN "ShutdownPlan"
#define AIOT_CMD_ALARM_PLAN "AlarmNotifyPlan"
#define AIOT_CMD_FACE_SWITCH "FaceFrameSwitch"
#define AIOT_CMD_RECT_SWITCH "RectFrameSwitch"
#define AIOT_CMD_HUMANTRACK "TrackHumanSwitch"
#define AIOT_CMD_TWINKLE_RECT "RectTwinkleSwitch"
#define AIOT_CMD_GUNBALL_TRACK "gunball_track_mode"
#define AIOT_CMD_FACE_SENSITIVITY "FaceDetectSensitivity"
#define AIOT_CMD_MONITOR_MODE "MonitoringMode"
#define AIOT_CMD_DEVICE_POWER "DevicePower"
#define AIOT_CMD_TIME_ZONE "TimeZone"
#define AIOT_CMD_IPV4 "IpV4"
#define AIOT_CMD_NETWORK_TYPE "NetworkType"
#define AIOT_CMD_IRCUT_MODE "IcrWorkMode"
#define AIOT_CMD_LIGHT_CONFIG "LightConfig"
#define AIOT_CMD_IRCUT_NIGHTTIME "ircut_nighttime"
#define AIOT_CMD_OSD "osd"
#define AIOT_CMD_RECORD_STATUS "RecordStatus"
#define AIOT_CMD_PTZ_DIR "ptz_direction"
#define AIOT_CMD_AUDIO_CAPTURE "AudioCapture"
#define AIOT_CMD_PTZ_ADVANCE "PTZAdvancefunctions"
#define AIOT_CMD_PTZ_ADVANCE_STATE "AdvancePtzState"
#define AIOT_CMD_MOTION_DETECT "MotionDetect"
#define AIOT_CMD_ALARM_FREQ "AlarmFreq"
#define AIOT_CMD_STORAGE_STATUS "StorageStatus"
#define AIOT_CMD_WIFI_4G_PROPERTY "Wifi4gProperty"
#define AIOT_CMD_PRESET_LIST "PresetList"
#define AIOT_CMD_DEV_PROPERTY "DevProperty"
#define AIOT_CMD_SIM_INFO "SIMInfo2"
#define AIOT_CMD_G4_VER "G4Ver"
#define AIOT_CMD_EXTRA_CAPABILITY "ExtraCapabilities"
#define AIOT_CMD_UPLOAD_LOCATION "UploadLocation"
#define AIOT_CMD_AOV_WORKMODE "Aovworkmode"
#define AIOT_CMD_CHARGE_STA "ChargeSta"
#define AIOT_CMD_LOCATION_INFO "LocationInfo"
#define AIOT_CMD_AFCFG "Afcfg"

#define AIOT_UPLOAD_LOCATION_FLAG DATA_BLOCK_MOUNT_PATH "/upload_location.flag"
#define LOCATION_URL_CELL "/anjia/apiv2/UploadCellInfo"
#define LOCATION_URL_RESET "/anjia/apiv2/UploadResetInfo"

typedef enum
{
    AIOT_PTZ_LEFT = 0,
    AIOT_PTZ_RIGHT,
    AIOT_PTZ_UP,
    AIOT_PTZ_DOWN,
    AIOT_PTZ_UP_LEFT,
    AIOT_PTZ_UP_RIGHT,
    AIOT_PTZ_DOWN_LEFT,
    AIOT_PTZ_DOWN_RIGHT,
    AIOT_PTZ_SCALE_UP,
    AIOT_PTZ_SCALE_DOWN,

    AIOT_PTZ_STOP = 100,
    AIOT_PTZ_SET_PRESET,
    AIOT_PTZ_CALL_PRESET,

    AIOT_PTZ_FOCUS_ON = 105,
    AIOT_PTZ_FOCUS_OFF,
    AIOT_PTZ_CLEAR_PRESET,

    AIOT_PTZ_TRACK_ON = 110,
    AIOT_PTZ_TRACK_OFF,
    AIOT_PTZ_AUTO_SCAN_ON,
    AIOT_PTZ_AUTO_SCAN_OFF,
    AIOT_PTZ_AREA_SCAN_ON,
    AIOT_PTZ_AREA_SCAN_OFF,
    AIOT_PTZ_CRUISE_ON,
    AIOT_PTZ_CRUISE_OFF,
    AIOT_PTZ_GUARD_PRESET_ON,
    AIOT_PTZ_GUARD_PRESET_OFF,
    AIOT_PTZ_GUARD_TRACE_ON,
    AIOT_PTZ_GUARD_TRACE_OFF,
    AIOT_PTZ_GUARD_CRUISE_ON,
    AIOT_PTZ_GUARD_CRUISE_OFF,
    AIOT_PTZ_GUARD_AREA_SCAN_ON,
    AIOT_PTZ_GUARD_AREA_SCAN_OFF,
    AIOT_PTZ_LEFT_MARGIN,
    AIOT_PTZ_RIGHT_MARGIN,
    AIOT_PTZ_CLEAR_ALL_PRESET,
    AIOT_PTZ_STOP_GUARD,
    AIOT_PTZ_REBOOT,
    AIOT_PTZ_SCAN_SPEED_UP,
    AIOT_PTZ_SCAN_SPEED_DOWN,
    AIOT_PTZ_SET_GUARD_PRESET,
    AIOT_PTZ_CLEAR_GUARD_PRESET,
    AIOT_PTZ_SET_PRESET_NAME,
    AIOT_PTZ_NONE = 10000,
} AIOT_PTZ_CTRL;

typedef struct _PTZCtrlInfo
{
    AIOT_PTZ_CTRL ActionType; //-1:stop 0:左 1:右 2:上 3:下
    int Step;                 // 1 ~ 10
    int duration;             // 0-1000
    char name[256];
} PTZCtrlInfo;

typedef struct
{
    int CruisesSwitch; // 巡航是否开启
    int PdTracking;    // 人型跟踪
    int AreaScanning;  // 区域扫描
    int GuardPosition; // 看守位功能
} AdvancePtzState;

typedef struct
{
    int min_level; // 电量区间下限
    int threshold; // 变化阈值
} PowerReportThreshold;

typedef char *(*AIOT_CMD_HANDLER)();
typedef struct
{
    char *szReportID;
    AIOT_CMD_HANDLER handler;
    char *szData;
    int bChanged;
} CMD_AIOT_INFO;

static const PowerReportThreshold g_power_thresholds[] = {
    {0, 8},   // 0-20%: 变化≥8%上报
    {20, 15}, // 20-50%: 变化≥15%上报
    {50, 20}  // ≥50%: 变化≥20%上报
};

#define ALARM_PUSH_FREQ_DEFAULT (5 * 60) // 秒

static pthread_mutex_t s_stAiotCmdMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_stSendEventFreq = ALARM_PUSH_FREQ_DEFAULT; // 报警推送频率

static FRAME_BUFFER_MANAGER s_stAiotCmdMgr = {0};
static anj_thread_s s_stAiotCmdThread = {0};

static char *Report_Property_S32(const char *szPropertyName, int szPropertyValue)
{
    cJSON *pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
        return NULL;

    cJSON_AddNumberToObject(pRoot, szPropertyName, szPropertyValue);
    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *Report_Property_string(const char *szPropertyName, const char *szPropertyValue)
{
    cJSON *pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
        return NULL;

    cJSON_AddStringToObject(pRoot, szPropertyName, szPropertyValue);
    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anj_aiot_cmd_record_get()
{
    int recordmode = 0;
    RecordConfig *pstRecordConfig = (RecordConfig *)getRecordConfig();
    if (pstRecordConfig->motionRecordCfg.localStore)
    {
        recordmode = 1;
    }
    if (pstRecordConfig->scheduleRecordCfg.localStore)
    {
        recordmode = 2;
    }
    return Report_Property_S32("StorageRecordMode", recordmode);
}

static char *anj_aiot_cmd_alarm_get()
{
    int AlarmSwitch = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    if (pstAlarmConfig->aiAlarm.pdAlarm[0].enable || pstAlarmConfig->normalAlarm.motionDetectAlarm[0].enable)
    {
        AlarmSwitch = 1;
    }

    return Report_Property_S32("AlarmSwitch", AlarmSwitch);
}

static char *anj_aiot_cmd_ptz_speed_get()
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    return Report_Property_S32("PtzStepInterval", pstIotPtzConfig->m_ptzSpeed.HSpeed);
}

static char *anj_aiot_cmd_ptz_status_get()
{
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_MOVE_STATUS, &event_result, NULL);

    return Report_Property_S32("PtzStatus", event_result.ret);
}

static char *anj_aiot_cmd_ptz_preset_get()
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    cJSON *rootObj = cJSON_CreateObject();
    if (rootObj == NULL)
        return NULL;

    cJSON *preset_infos = cJSON_CreateArray();
    if (preset_infos == NULL)
    {
        cJSON_Delete(rootObj);
        return NULL;
    }

    for (int i = 0; i < MAX_PTZ_PRESET; i++)
    {
        PtzPreset *pstPtzPreset = &pstIotPtzConfig->m_ptzPreset[i];
        if (pstPtzPreset->preset_id > 0)
        {
            int bWatchGuard = (pstIotPtzConfig->watch_guard == pstPtzPreset->preset_id) ? 1 : 0;
            cJSON *presetObj = cJSON_CreateObject();

            cJSON_AddNumberToObject(presetObj, "PresetNum", pstPtzPreset->preset_id);
            cJSON_AddStringToObject(presetObj, "PresetName", pstPtzPreset->name);
            cJSON_AddNumberToObject(presetObj, "IsHome", bWatchGuard);
            cJSON_AddItemToArray(preset_infos, presetObj);
        }
    }

    cJSON_AddItemToObject(rootObj, "PresetList", preset_infos);

    char *szData = cJSON_PrintUnformatted(rootObj);
    cJSON_Delete(rootObj);

    __INFO("%s\n", szData);
    return szData;
}

static char *anj_aiot_cmd_flip_get()
{
    int ImageFlipState = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    if (pstVideoCapture->vflip == 0)
    {
        if (pstVideoCapture->hflip == 0)
            ImageFlipState = 0;
        else
            ImageFlipState = 2;
    }
    else
    {
        if (pstVideoCapture->hflip == 0)
            ImageFlipState = 3;
        else
            ImageFlipState = 1;
    }
    return Report_Property_S32("ImageFlipState", ImageFlipState);
}

static char *anj_aiot_cmd_lens_cover_get()
{
    return Report_Property_S32("LensCover", anj_osd_lens_cover_get());
}

static char *anj_aiot_cmd_power_light_get()
{
    int PowerLight = 1;
    if (anj_mw_file_exists("/mnt/nand/pilot_power_light_close"))
    {
        PowerLight = 1;
    }
    return Report_Property_S32("pilotPowerLight", PowerLight);
}

static char *anj_aiot_cmd_mcu_version_get()
{
    DevInfo *pstDevInfo = getDevInfo();
    return Report_Property_string("mcuversion", pstDevInfo->mcu_version);
}

static char *anj_aiot_cmd_weak_light_get()
{
    int weak_light_mode = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    if (pstVideoCapture->led_brightness_value != 100)
        weak_light_mode = 1;

    if (SUPPORT_WEAK_LIGHT)
    {
        weak_light_mode = 1;
    }
    return Report_Property_S32("WeakLightSwitch", weak_light_mode);
}

static char *anj_aiot_cmd_4g_card_get()
{
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
    if (event_result.result)
    {
        G4InfoStruct networkStatus = *(G4InfoStruct *)event_result.result;
        return Report_Property_S32("work4gcard", networkStatus.CurOperator);
    }
    else
    {
        return NULL;
    }
}

static char *anj_aiot_cmd_stream_work_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoEncode *pstVideoEncode = &pstMediaConfig->videoConfig[0].videoEncode;
    return Report_Property_S32("StreamWorkMode", pstVideoEncode->twoLensCfg.eTwoLensWorkMode);
}

static char *anj_aiot_cmd_intercom_type_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    return Report_Property_S32("VoiceIntercomType", pstAudioCapture->aec_enable);
}

static char *anj_aiot_cmd_wdr_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    return Report_Property_S32("WDRswitch", pstVideoCapture->wdr_mode);
}

static char *anj_aiot_cmd_datnight_mode_get()
{
    int DayNightMode = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    if (pstVideoCapture->ircut_mode == IRCUT_Mode_Manual)
        DayNightMode = 0; // 白天
    else
        DayNightMode = 2; // 自动

    return Report_Property_S32("DayNightMode", DayNightMode);
}

static char *anj_aiot_cmd_mic_get()
{
    int MicSwitch = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    if (pstAudioCapture->volume_capture > 10)
        MicSwitch = 1;
    else
        MicSwitch = 0;

    return Report_Property_S32("MicSwitch", MicSwitch);
}

static char *anj_aiot_cmd_mic_volume_get()
{
    return Report_Property_S32("MicVolume", 1);
}

static char *anj_aiot_cmd_speaker_volume_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    int SpeakerVolume = pstAudioCapture->volume_play / 10;
    return Report_Property_S32("SpeakerVolume", SpeakerVolume);
}

static char *anj_aiot_cmd_shutdown_get()
{
    SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
    MaintainConfig *pstMaintainConfig = &pstSystemConfig->maintainCfg;
    int ShutdownSwitch = pstMaintainConfig->enable;
    return Report_Property_S32("ShutdownSwitch", ShutdownSwitch);
}

static char *anj_aiot_cmd_shutdown_plan_get()
{
    char *szData = NULL;
    SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
    MaintainConfig *pstMaintainConfig = &pstSystemConfig->maintainCfg;
    int ShutdownSwitch = pstMaintainConfig->enable;
    if (ShutdownSwitch)
    {
        char daystrlist[7][8] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
        /* {"ShutdownPlan":[{"DayOfWeek":4,"EndTime":72000,"BeginTime":0,"Enable":1,"RepeatDays":"mon,thu,"}]} */
        cJSON *item = cJSON_CreateObject();
        if (item == NULL)
        {
            return szData;
        }

        int time = pstMaintainConfig->time.hour * 3600 + pstMaintainConfig->time.minute * 60 + pstMaintainConfig->time.sec;

        cJSON_AddNumberToObject(item, "DayOfWeek", 0);
        cJSON_AddNumberToObject(item, "EndTime", time + 1);
        cJSON_AddNumberToObject(item, "BeginTime", time);
        cJSON_AddNumberToObject(item, "Enable", 1);

        if (pstMaintainConfig->day != 7)
        {
            int day = 0;

            if (pstMaintainConfig->day > 7)
                day = 6;

            cJSON_AddStringToObject(item, "RepeatDays", daystrlist[day]);
        }
        else
        {
            int i = 0;
            char strRepeatDays[128] = {0};
            for (i = 0; i < 7; i++)
            {
                strcat(strRepeatDays, daystrlist[i]);
                strcat(strRepeatDays, ",");
            }

            cJSON_AddStringToObject(item, "RepeatDays", strRepeatDays);
        }

        cJSON *array = cJSON_CreateArray();
        if (array == NULL)
        {
            cJSON_Delete(item);
            return szData;
        }

        cJSON_AddItemToArray(array, item);

        cJSON *pRoot = cJSON_CreateObject();
        if (pRoot == NULL)
        {
            cJSON_Delete(array);
            return szData;
        }

        cJSON_AddItemToObject(pRoot, AIOT_CMD_SHUTDOWN_PLAN, array);

        szData = cJSON_PrintUnformatted(pRoot);
        cJSON_Delete(pRoot);
    }
    return szData;
}

static char *anj_aiot_cmd_face_get()
{
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAction *pstPdAction = &pstAlarmConfig->aiAlarm.pdAlarm[0].alarmAction;
    return Report_Property_S32("FaceFrameSwitch", pstPdAction->draw_human_enable);
}

static char *anj_aiot_cmd_rect_get()
{
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAction *pstPdAction = &pstAlarmConfig->aiAlarm.pdAlarm[0].alarmAction;
    return Report_Property_S32("RectFrameSwitch", pstPdAction->draw_rect_enable);
}

static char *anj_aiot_cmd_human_track_get()
{
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAction *pstPdAction = &pstAlarmConfig->aiAlarm.pdAlarm[0].alarmAction;
    return Report_Property_S32("TrackHumanSwitch", pstPdAction->track_human_enable);
}

static char *anj_aiot_cmd_twinkle_rect_get()
{
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAction *pstPdAction = &pstAlarmConfig->aiAlarm.pdAlarm[0].alarmAction;
    return Report_Property_S32("RectTwinkleSwitch", pstPdAction->rect_twinkle_enable);
}

static char *anj_aiot_cmd_gunball_track_get()
{
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAction *pstPdAction = &pstAlarmConfig->aiAlarm.pdAlarm[0].alarmAction;
    return Report_Property_S32("gunball_track_mode", pstPdAction->gunball_track_mode);
}

static char *anj_aiot_cmd_face_sensitivity_get()
{
    int Sensitivity = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmConfig->aiAlarm.pdAlarm[0];
    if (pstPdAlarm->sensitivity != 0 && (pstPdAlarm->enable))
        Sensitivity = pstPdAlarm->sensitivity / 20 + 1;
    else
        Sensitivity = 0;

    return Report_Property_S32("FaceDetectSensitivity", Sensitivity);
}

static char *anj_aiot_cmd_monitor_mode_get()
{
    int MonitorMode = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmConfig->aiAlarm.pdAlarm[0];
    MotionDetectAlarm *pstMdAlarm = &pstAlarmConfig->normalAlarm.motionDetectAlarm[0];
    smart_mask_e eSmartMask = anj_smart_mask_get();
    if (SMART_CHECK_MASK(eSmartMask, SMART_HUMAN_MASK))
    {
        MonitorMode = 1;
        if (pstPdAlarm->alarmAction.alarm_push.enable_flag != ARMING_DISABLE || 
            pstPdAlarm->alarmAction.audioAction.enable.enable_flag != ARMING_DISABLE ||
            (pstPdAlarm->alarmAction.alarm_led_enable.enable_flag != ARMING_DISABLE && anj_sysctl_capability_check(FUNCTION_ALARM_LED)) )
        {
            MonitorMode = 0;
        }
    }
    else
    {
        MonitorMode = pstMdAlarm->enable;
    }

    return Report_Property_S32("MonitoringMode", MonitorMode);
}

static char *anj_aiot_cmd_device_power_get()
{
    char *szData = NULL;
    if (ANJ_PROJECT_TYPE == PROJECT_TYPE_LP)
    {
        int cap = 0;
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_BATTERY_CAP_GET, &event_result, &cap);

        if (cap == -1)
        {
            __ERR("Failed to get power capacity\n");
            return szData;
        }
        static int stLastCap = 200;
        int absCap = abs(cap - stLastCap);
        if (stLastCap == 200 || absCap >= 5)
        {
            int should_report = 0;

            if (stLastCap == 200)
            {
                should_report = 1;
            }
            else
            {
                // 根据电量区间使用不同阈值
                // 查找适用的阈值配置
                for (int i = 0; i < (sizeof(g_power_thresholds) / sizeof(g_power_thresholds[0])); i++)
                {
                    if (cap >= g_power_thresholds[i].min_level)
                    {
                        if (absCap >= g_power_thresholds[i].threshold)
                        {
                            should_report = true;
                        }
                        break; // 找到对应区间即退出
                    }
                }
            }

            if (should_report)
            {
                szData = Report_Property_S32("DevicePower", cap);
                stLastCap = cap;
                __ERR("Reported DevicePower: %d\n", cap);
            }
        }
    }
    return szData;
}

static char *anj_aiot_cmd_timezone_get()
{
    int TimeZone = anj_systime_get_zone_by_system();
    return Report_Property_S32("TimeZone", TimeZone);
}

static char *anj_aiot_cmd_ipv4_get()
{
    NetworkConfigNew *pstNetWorkConfig = (NetworkConfigNew *)getNetWorkConfig();
    /* 设备局域网ip */
    return Report_Property_string("IpV4", pstNetWorkConfig->lanCfg.IPAddress);
}

static char *anj_aiot_cmd_network_type_get()
{
    int netstatus = 0;
    anj_net_status_e eNet = anj_net_status_check();
    if (eNet == ANJ_NET_STATUS_WIFI)
    {
        netstatus = 1;
    }
    else if (eNet == ANJ_NET_STATUS_4G)
    {
        netstatus = 2;
    }
    return Report_Property_S32("NetworkType", netstatus);
}

static char *anj_aiot_cmd_light_config_get()
{
    int data = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    data = pstVideoCapture->led_mode;
    if (data > 2)
        data = 2;
    return Report_Property_S32("LightConfig", data);
}

static char *anj_aiot_cmd_ircut_nighttime_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    cJSON *pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
    {
        return NULL;
    }

    cJSON *ircut_nighttime = cJSON_CreateObject();
    if (ircut_nighttime)
    {
        cJSON_AddNumberToObject(ircut_nighttime, "hour_start", pstVideoCapture->ircut_nighttime.startTime.hour);
        cJSON_AddNumberToObject(ircut_nighttime, "min_start", pstVideoCapture->ircut_nighttime.startTime.minute);
        cJSON_AddNumberToObject(ircut_nighttime, "sec_start", pstVideoCapture->ircut_nighttime.startTime.sec);
        cJSON_AddNumberToObject(ircut_nighttime, "hour_end", pstVideoCapture->ircut_nighttime.endTime.hour);
        cJSON_AddNumberToObject(ircut_nighttime, "min_end", pstVideoCapture->ircut_nighttime.endTime.minute);
        cJSON_AddNumberToObject(ircut_nighttime, "sec_end", pstVideoCapture->ircut_nighttime.endTime.sec);

        cJSON_AddItemToObject(pRoot, "ircut_nighttime", ircut_nighttime);
    }

    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anj_aiot_cmd_record_status_get()
{
    int RecordStatus = 0;
    RecordConfig *pstRecordCfg = (RecordConfig *)getRecordConfig();
    if (pstRecordCfg->commonCfg.localEnable &&
        (pstRecordCfg->inputAlarmRecordCfg.localStore ||
         pstRecordCfg->motionRecordCfg.localStore ||
         pstRecordCfg->scheduleRecordCfg.localStore))
    {
        RecordStatus = 1;
    }
    return Report_Property_S32("RecordStatus", RecordStatus);
}

static char *anj_aiot_cmd_ptz_advance_get()
{
    char szFunctionList[1024] = {0};

    // if (m_bHavePtzFunction)
    {
        SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
        PTZConfig *pCfg = &pstSystemConfig->ptzCfg;

        int iIndex = 0;
        for (iIndex = 0; iIndex < MAX_PTZFUCTION_COUNT && iIndex < pCfg->advanceCfg.functionCnt; iIndex++)
        {
            char *szFunction = pCfg->advanceCfg.functions[iIndex].functionName;
            if (strlen(szFunction) > 0)
            {
                // 检查缓冲区空间是否足够
                size_t current_len = strlen(szFunctionList);
                size_t add_len = strlen(szFunction) + 2; // +1 for comma, +1 for null terminator

                if (current_len + add_len < sizeof(szFunctionList))
                {
                    strcat(szFunctionList, szFunction);
                    strcat(szFunctionList, ",");
                }
                else
                {
                    __WARN("Function list buffer full\n");
                    break;
                }
            }
        }
    }

    return Report_Property_string("PTZAdvancefunctions", szFunctionList);
}

char *anj_aiot_cmd_ptz_advance_state_get(void *data)
{
    if (data == NULL)
        return NULL;

    int flag = *(int *)data;
    int CruisesSwitch = 0; // 巡航是否开启
    int PdTracking = 0;    // 人型跟踪
    int AreaScanning = 0;  // 区域扫描
    int GuardPosition = 0; // 看守位功能
    if (((flag & 0x08) >> 3) == 1)
    {
        PdTracking = 1;
    }
    if (((flag & 0x04) >> 2) == 1)
    {
        CruisesSwitch = 1;
    }
    if (((flag & 0x02) >> 1) == 1)
    {
        AreaScanning = 1;
    }
    if (((flag & 0x01) >> 0) == 1)
    {
        GuardPosition = 1;
    }

    cJSON *rootObj = cJSON_CreateObject();
    cJSON *m_AdvancePtzState = cJSON_CreateObject();
    if (m_AdvancePtzState)
    {
        cJSON_AddNumberToObject(m_AdvancePtzState, "PdTracking", PdTracking);
        cJSON_AddNumberToObject(m_AdvancePtzState, "CruisesSwitch", CruisesSwitch);
        cJSON_AddNumberToObject(m_AdvancePtzState, "AreaScanning", AreaScanning);
        cJSON_AddNumberToObject(m_AdvancePtzState, "GuardPosition", GuardPosition);

        cJSON_AddItemToObject(rootObj, "AdvancePtzState", m_AdvancePtzState);
    }
    char *szData = cJSON_PrintUnformatted(rootObj);
    cJSON_Delete(rootObj);

    __INFO("AdvancePtzState: %s \n", szData);
    return szData;
}

static char *anj_aiot_cmd_sdcard_status_get()
{
    cJSON *pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
    {
        return NULL;
    }
    anj_sdcard_info stSdInfo = {0};
    if (anj_sdcard_info_query(&stSdInfo) == 0)
    {
        int sdcard_stauts = Storage_NONE;
        if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_INSERT)
            sdcard_stauts = Storage_Mounting;
        else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_FORMAT)
            sdcard_stauts = Storage_FORMATING;
        else if ((stSdInfo.eStatus == ANJ_SDCARD_STATUS_MOUNT) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NOT_INIT))
            sdcard_stauts = Storage_UNINITED;
        else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NORMAL)
            sdcard_stauts = Storage_OK;
        else if ((stSdInfo.eStatus == ANJ_SDCARD_STATUS_RWERROR) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_RONLY))
            sdcard_stauts = Storage_EXEPTION;

        int space_free = stSdInfo.iRemainSize;
        int space_total = stSdInfo.iSize;

        cJSON_AddNumberToObject(pRoot, "StorageStatus", sdcard_stauts);
        cJSON_AddNumberToObject(pRoot, "StorageRemainCapacity", space_free);
        cJSON_AddNumberToObject(pRoot, "StorageTotalCapacity", space_total);
    }
    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anj_aiot_cmd_alarm_freq_get()
{
    cJSON *pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
    {
        return NULL;
    }

    /* send alram event freq */
    cJSON *frequency = cJSON_CreateObject();
    if (frequency)
    {
        cJSON_AddNumberToObject(frequency, "frequency", s_stSendEventFreq);
        cJSON_AddItemToObject(pRoot, "AlarmFreq", frequency);
    }

    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anj_aiot_cmd_wifi_4g_property_get()
{
    cJSON *pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
    {
        return NULL;
    }

    if (IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_WIFI ||
        IPC_NETWORK_TYPE == NET_DEV_TYPE_WIFI)
    {
        NETWORK_STATUS_DATA networkStatus = {0};
        anj_net_info_get(&networkStatus);

        if (networkStatus.signallevel < -100)
        {
            networkStatus.signallevel = -100;
        }

        int signalStrength = ~networkStatus.signallevel;
        __INFO("#######The Device support WIFI, networkStatus essid:%s, linkQuality:%d, signallevel:%d \n",
               networkStatus.essid, networkStatus.linkquality, signalStrength);

        cJSON_AddStringToObject(pRoot, "WifiSSID", networkStatus.essid);
        cJSON_AddNumberToObject(pRoot, "WifiQuality", networkStatus.signallevel);
    }
    else if (IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_4G ||
             IPC_NETWORK_TYPE == NET_DEV_TYPE_4G)
    {
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
        if (event_result.result)
        {
            G4InfoStruct get4gNetworkInfo = *(G4InfoStruct *)event_result.result;
            __INFO("#######The device support 4G, signal level %d\n", get4gNetworkInfo.nSignalLevel);
            cJSON_AddNumberToObject(pRoot, "WifiQuality", get4gNetworkInfo.nSignalLevel);
        }
    }
    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anj_aiot_cmd_dev_property_get(void)
{
    char szRealVersion[64] = {0};

    DevInfo *pstDevInfo = getDevInfo();
    cJSON *pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
    {
        return NULL;
    }
    sprintf(szRealVersion, "%s_V%s", pstDevInfo->devType, pstDevInfo->productVersion);

    cJSON_AddStringToObject(pRoot, "SN", pstDevInfo->sn);
    cJSON_AddStringToObject(pRoot, "DeviceType", "IPCamera");
    cJSON_AddNumberToObject(pRoot, "ChannelNumber", ANJ_CAMERA_MAX_NUMS);
    cJSON_AddStringToObject(pRoot, "Model", pstDevInfo->subDevType);
    cJSON_AddStringToObject(pRoot, "RealModel", szRealVersion);
    cJSON_AddStringToObject(pRoot, "IpcVersion", pstDevInfo->version_name);
    cJSON_AddStringToObject(pRoot, "release_date", pstDevInfo->release_date);
    char *szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anj_aiot_cmd_extra_capability_get(void)
{
    char capability_str[2560] = {0};
    char *cap = anj_sysctl_get_capability_string();
    if (cap && cap[0])
    {
        snprintf(capability_str, sizeof(capability_str), "%s%s", cap, EXTRA_CAPABILITIES);
    }
    else
    {
        snprintf(capability_str, sizeof(capability_str), "%s", EXTRA_CAPABILITIES);
    }
    return Report_Property_string(AIOT_CMD_EXTRA_CAPABILITY, capability_str);
}

static char *anj_aiot_cmd_upload_location_get(void)
{
    int enable = anj_mw_file_exists(AIOT_UPLOAD_LOCATION_FLAG) ? 1 : 0;
    return Report_Property_S32(AIOT_CMD_UPLOAD_LOCATION, enable);
}

static char *anj_aiot_cmd_aov_workmode_get(void)
{
    if (ANJ_PROJECT_TYPE != PROJECT_TYPE_AOV)
    {
        return NULL;
    }

    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    int aov_workmode = 0;
    int aov_fps = pstVideoCapture->aov_fps;

    if (pstVideoCapture->aov_mode == 1)
    {
        aov_workmode = 1;
    }
    else if (pstVideoCapture->aov_mode == 2)
    {
        aov_workmode = 2;
        if (aov_fps == 2)
        {
            aov_workmode = 3;
        }
        else if (aov_fps == 5)
        {
            aov_workmode = 4;
        }
    }
    return Report_Property_S32(AIOT_CMD_AOV_WORKMODE, aov_workmode);
}

static char *anj_aiot_cmd_charge_sta_get(void)
{
    if (ANJ_PROJECT_TYPE != PROJECT_TYPE_AOV)
    {
        return NULL;
    }

    // TODO: 获取充电状态
    return Report_Property_S32(AIOT_CMD_CHARGE_STA, -1);
}
static int anj_aiot_cmd_location_4g_get(G4InfoStruct *pst4g)
{
    EventResult event_result = {0};

    if (pst4g == NULL)
    {
        return -1;
    }
    eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
    if (event_result.result == NULL)
    {
        return -1;
    }
    *pst4g = *(G4InfoStruct *)event_result.result;
    return 0;
}

static int anj_aiot_cmd_location_upload_last(const G4InfoStruct *pst4g, const char *type, const char *user)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    char lac[32] = {0};
    char ci[32] = {0};
    char mnc[8] = {0};

    if (pst4g == NULL || pst4g->lastCellId == 0 || pst4g->lastLAC == 0)
    {
        return -1;
    }
    if (user == NULL || user[0] == '\0')
    {
        __ERR("not get bind user\n");
        return -1;
    }
    snprintf(lac, sizeof(lac), "%x", pst4g->lastLAC);
    snprintf(ci, sizeof(ci), "%x", pst4g->lastCellId);
    strncpy(mnc, "1", sizeof(mnc) - 1);
    if (strcasestr(pst4g->lastOper, "MOBILE") != NULL)
    {
        strncpy(mnc, "0", sizeof(mnc) - 1);
    }
    return dev_bind_location(LOCATION_URL_RESET, lac, ci, mnc, pst4g->IMEI,
                             pstSerInfo->stP2pLoginState.devid, user, type, NULL, NULL);
}

static void anj_aiot_cmd_location_poll(void)
{
    G4InfoStruct st4g = {0};
    G4LocationSet stSet = {0};
    EventResult event_result = {0};
    anj_ser_info *pstSerInfo = getSerInfo();
    char lac[32] = {0};
    char ci[32] = {0};
    char mnc[8] = {0};

    if (anj_aiot_cmd_location_4g_get(&st4g) != 0)
    {
        return;
    }
    if (st4g.locationHasReset)
    {
        if (anj_aiot_cmd_location_upload_last(&st4g, "1", st4g.locationResetUser) == 0)
        {
            stSet.action = G4_LOCATION_ACT_CLEAR_RESET;
            eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_LOCATION_SET, &event_result, &stSet);
        }
    }
    if (st4g.locationNeedUpload && st4g.cellId != 0 && st4g.LAC != 0)
    {
        snprintf(lac, sizeof(lac), "%x", st4g.LAC);
        snprintf(ci, sizeof(ci), "%x", st4g.cellId);
        snprintf(mnc, sizeof(mnc), "%d", st4g.MNC);
        if (dev_bind_location(LOCATION_URL_CELL, lac, ci, mnc, st4g.IMEI,
                              pstSerInfo->stP2pLoginState.devid, NULL, NULL, NULL, NULL) == 0)
        {
            memset(&stSet, 0, sizeof(stSet));
            stSet.action = G4_LOCATION_ACT_COMMIT;
            eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_LOCATION_SET, &event_result, &stSet);
        }
        else
        {
            __ERR("upload_cur location failed\n");
        }
    }
}

void anj_aiot_cmd_location_on_bind(const char *accountName)
{
    G4InfoStruct st4g = {0};

    if (!anj_mw_file_exists(AIOT_UPLOAD_LOCATION_FLAG))
    {
        return;
    }
    if (anj_aiot_cmd_location_4g_get(&st4g) != 0)
    {
        return;
    }
    if (anj_aiot_cmd_location_upload_last(&st4g, "2", accountName) != 0)
    {
        __ERR("upload last location on bind failed\n");
    }
}

void anj_aiot_cmd_location_on_unbind(void)
{
    G4InfoStruct st4g = {0};
    G4LocationSet stSet = {0};
    EventResult event_result = {0};
    char user[256] = {0};

    if (!anj_mw_file_exists(AIOT_UPLOAD_LOCATION_FLAG))
    {
        return;
    }
    gct_apiv4_get_binduser(user);
    if (anj_aiot_cmd_location_4g_get(&st4g) != 0)
    {
        return;
    }
    if (anj_aiot_cmd_location_upload_last(&st4g, "1", user) != 0)
    {
        stSet.action = G4_LOCATION_ACT_SAVE_RESET;
        snprintf(stSet.user, sizeof(stSet.user), "%s", user);
        eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_LOCATION_SET, &event_result, &stSet);
    }
}

static char *anj_aiot_cmd_location_info_get(void)
{
    anj_ser_info *pstSerInfo = getSerInfo();
    EventResult event_result = {0};
    G4InfoStruct st4g = {0};
    cJSON *pRoot = NULL;
    cJSON *pLocation = NULL;
    char *szData = NULL;
    eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
    if (event_result.result == NULL)
    {
        return NULL;
    }
    st4g = *(G4InfoStruct *)event_result.result;
    if (st4g.cellId == 0 || st4g.LAC == 0)
    {
        return NULL;
    }

    pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
    {
        return NULL;
    }
    pLocation = cJSON_CreateObject();
    if (pLocation == NULL)
    {
        cJSON_Delete(pRoot);
        return NULL;
    }

    cJSON_AddNumberToObject(pLocation, "CellId", st4g.cellId);
    cJSON_AddNumberToObject(pLocation, "LAC", st4g.LAC);
    cJSON_AddNumberToObject(pLocation, "MCC", st4g.MCC);
    cJSON_AddNumberToObject(pLocation, "MNC", st4g.MNC);
    cJSON_AddStringToObject(pLocation, "IMEI", st4g.IMEI);
    cJSON_AddNumberToObject(pLocation, "Signal", -60);
    cJSON_AddNumberToObject(pLocation, "Cage", 0);
    cJSON_AddNumberToObject(pLocation, "IsCDMA", 0);
    cJSON_AddStringToObject(pLocation, "DevNo", pstSerInfo->stP2pLoginState.devid);
    cJSON_AddStringToObject(pLocation, "Network", "GSM");
    cJSON_AddItemToObject(pRoot, AIOT_CMD_LOCATION_INFO, pLocation);

    szData = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return szData;
}

static char *anj_aiot_cmd_afcfg_get(void)
{
    // TODO: 暂无 AF 产品
    return NULL;
}

static CMD_AIOT_INFO s_cmd_info[] =
    {
        {AIOT_CMD_RECORD_MODE, anj_aiot_cmd_record_get, NULL},
        {AIOT_CMD_ALARM_SWITCH, anj_aiot_cmd_alarm_get, NULL},
        {AIOT_CMD_PTZ_SPEED, anj_aiot_cmd_ptz_speed_get, NULL},
        {AIOT_CMD_PTZ_STATUS, anj_aiot_cmd_ptz_status_get, NULL},
        {AIOT_CMD_PRESET_LIST, NULL, NULL},
        {AIOT_CMD_IMAGE_FLIP, anj_aiot_cmd_flip_get, NULL},
        {AIOT_CMD_LENS_COVER, anj_aiot_cmd_lens_cover_get, NULL},
        {AIOT_CMD_POWER_LIGHT, anj_aiot_cmd_power_light_get, NULL},
        {AIOT_CMD_MCU_VERSION, anj_aiot_cmd_mcu_version_get, NULL},
        {AIOT_CMD_WEAK_LIGHT, anj_aiot_cmd_weak_light_get, NULL},
        {AIOT_CMD_4G_CARD, anj_aiot_cmd_4g_card_get, NULL},
        {AIOT_CMD_STREAM_MODE, anj_aiot_cmd_stream_work_get, NULL},
        {AIOT_CMD_INTERCOM_TYPE, anj_aiot_cmd_intercom_type_get, NULL},
        {AIOT_CMD_WDR_SWITCH, anj_aiot_cmd_wdr_get, NULL},
        {AIOT_CMD_DAYNIGHT_MODE, anj_aiot_cmd_datnight_mode_get, NULL},
        {AIOT_CMD_MIC_SWITCH, anj_aiot_cmd_mic_get, NULL},
        {AIOT_CMD_MIC_VOLUME, anj_aiot_cmd_mic_volume_get, NULL},
        {AIOT_CMD_SPEAKER_VOLUME, anj_aiot_cmd_speaker_volume_get, NULL},
        {AIOT_CMD_SHUTDOWN_SWITCH, anj_aiot_cmd_shutdown_get, NULL},
        {AIOT_CMD_SHUTDOWN_PLAN, anj_aiot_cmd_shutdown_plan_get, NULL},
        {AIOT_CMD_FACE_SWITCH, anj_aiot_cmd_face_get, NULL},
        {AIOT_CMD_RECT_SWITCH, anj_aiot_cmd_rect_get, NULL},
        {AIOT_CMD_HUMANTRACK, anj_aiot_cmd_human_track_get, NULL},
        {AIOT_CMD_TWINKLE_RECT, anj_aiot_cmd_twinkle_rect_get, NULL},
        {AIOT_CMD_GUNBALL_TRACK, anj_aiot_cmd_gunball_track_get, NULL},
        {AIOT_CMD_FACE_SENSITIVITY, anj_aiot_cmd_face_sensitivity_get, NULL},
        {AIOT_CMD_MONITOR_MODE, anj_aiot_cmd_monitor_mode_get, NULL},
        {AIOT_CMD_DEVICE_POWER, anj_aiot_cmd_device_power_get, NULL},
        {AIOT_CMD_TIME_ZONE, anj_aiot_cmd_timezone_get, NULL},
        {AIOT_CMD_IPV4, anj_aiot_cmd_ipv4_get, NULL},
        {AIOT_CMD_NETWORK_TYPE, anj_aiot_cmd_network_type_get, NULL},
        {AIOT_CMD_IRCUT_MODE, anj_aiot_cmd_ircut_get, NULL},
        {AIOT_CMD_LIGHT_CONFIG, anj_aiot_cmd_light_config_get, NULL},
        {AIOT_CMD_IRCUT_NIGHTTIME, anj_aiot_cmd_ircut_nighttime_get, NULL},
        {AIOT_CMD_OSD, anj_aiot_cmd_osd_get, NULL},
        {AIOT_CMD_RECORD_STATUS, anj_aiot_cmd_record_status_get, NULL},
        {AIOT_CMD_PTZ_DIR, anj_aiot_cmd_ptz_dir_get, NULL},
        {AIOT_CMD_AUDIO_CAPTURE, anj_aiot_cmd_audio_capture_get, NULL},
        {AIOT_CMD_PTZ_ADVANCE, anj_aiot_cmd_ptz_advance_get, NULL},
        {AIOT_CMD_PTZ_ADVANCE_STATE, NULL, NULL},
        {AIOT_CMD_ALARM_FREQ, anj_aiot_cmd_alarm_freq_get, NULL},
        {AIOT_CMD_STORAGE_STATUS, anj_aiot_cmd_sdcard_status_get, NULL},
        {AIOT_CMD_WIFI_4G_PROPERTY, anj_aiot_cmd_wifi_4g_property_get, NULL},
        {AIOT_CMD_SIM_INFO, NULL, NULL},
        {AIOT_CMD_G4_VER, NULL, NULL},
        {AIOT_CMD_DEV_PROPERTY, anj_aiot_cmd_dev_property_get, NULL},
        {AIOT_CMD_EXTRA_CAPABILITY, anj_aiot_cmd_extra_capability_get, NULL},
        {AIOT_CMD_UPLOAD_LOCATION, anj_aiot_cmd_upload_location_get, NULL},
        {AIOT_CMD_AOV_WORKMODE, anj_aiot_cmd_aov_workmode_get, NULL},
        {AIOT_CMD_CHARGE_STA, anj_aiot_cmd_charge_sta_get, NULL},
        {AIOT_CMD_LOCATION_INFO, anj_aiot_cmd_location_info_get, NULL},
        {AIOT_CMD_AFCFG, anj_aiot_cmd_afcfg_get, NULL},
};

#define AIOT_CMD_INFO_CNT (sizeof(s_cmd_info) / sizeof(s_cmd_info[0]))

static void anj_aiot_cmd_store_property(CMD_AIOT_INFO *pstInfo, char *szNewData)
{
    if (pstInfo->szData != NULL && szNewData != NULL && strcmp(pstInfo->szData, szNewData) == 0)
    {
        anj_mw_free(szNewData);
        return;
    }

    if (szNewData != NULL)
    {
        if (pstInfo->bChanged == 0)
        {
            pstInfo->bChanged = 1;
            __INFO("%s: report new:%s\n", pstInfo->szReportID, szNewData);
        }
        else
        {
            __INFO("%s: Duplicate report new:%s\n", pstInfo->szReportID, szNewData);
        }
    }

    if (pstInfo->szData)
    {
        anj_mw_free(pstInfo->szData);
        pstInfo->szData = NULL;
    }
    pstInfo->szData = szNewData;
}

static void anj_aiot_cmd_prop_attach(cJSON *pProp, const char *szData, const char *szId)
{
    cJSON *pNode = NULL;
    cJSON *pChild = NULL;

    if (pProp == NULL || szData == NULL)
        return;

    pNode = cJSON_Parse(szData);
    if (pNode != NULL)
    {
        pChild = pNode->child;
        while (pChild != NULL)
        {
            cJSON *pAddNode = cJSON_Duplicate(pChild, 1);
            cJSON_AddItemToObject(pProp, pAddNode->string, pAddNode);
            pChild = pChild->next;
        }
        cJSON_Delete(pNode);
    }
    else if (szId != NULL)
    {
        cJSON_AddStringToObject(pProp, szId, szData);
    }
}

static char *anj_aiot_cmd_property_pack(int bChangeOnly)
{
    cJSON *pRoot = NULL;
    cJSON *pProp = NULL;
    char *pJsonText = NULL;
    int i = 0;

    pRoot = cJSON_CreateObject();
    if (pRoot == NULL)
        return NULL;

    pProp = cJSON_CreateObject();
    if (pProp == NULL)
    {
        cJSON_Delete(pRoot);
        return NULL;
    }

    for (i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
    {
        if (s_cmd_info[i].szData == NULL)
            continue;
        if (bChangeOnly && s_cmd_info[i].bChanged == 0)
            continue;
        anj_aiot_cmd_prop_attach(pProp, s_cmd_info[i].szData, s_cmd_info[i].szReportID);
    }

    cJSON_AddNumberToObject(pRoot, "ts", (double)time(NULL));
    cJSON_AddItemToObject(pRoot, "prop", pProp);
    pJsonText = cJSON_PrintUnformatted(pRoot);
    cJSON_Delete(pRoot);
    return pJsonText;
}

char *anj_aiot_cmd_property_change_json(void)
{
    char *pJsonText = NULL;
    int bHasChange = 0;
    int i = 0;

    anj_mutex_lock(&s_stAiotCmdMutex);
    for (i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
    {
        if (s_cmd_info[i].bChanged)
        {
            bHasChange = 1;
            break;
        }
    }
    if (bHasChange)
        pJsonText = anj_aiot_cmd_property_pack(1);
    anj_mutex_unlock(&s_stAiotCmdMutex);
    return pJsonText;
}

char *anj_aiot_cmd_property_full_json(void)
{
    char *pJsonText = NULL;

    anj_mutex_lock(&s_stAiotCmdMutex);
    pJsonText = anj_aiot_cmd_property_pack(0);
    anj_mutex_unlock(&s_stAiotCmdMutex);
    return pJsonText;
}

void anj_aiot_cmd_property_clear_change(void)
{
    int i = 0;

    anj_mutex_lock(&s_stAiotCmdMutex);
    for (i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
        s_cmd_info[i].bChanged = 0;
    anj_mutex_unlock(&s_stAiotCmdMutex);
}

/* 是否已采集过设备属性；空快照时不上报心跳 */
int anj_aiot_cmd_property_has_data(void)
{
    int i = 0;
    int bHas = 0;

    anj_mutex_lock(&s_stAiotCmdMutex);
    for (i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
    {
        if (s_cmd_info[i].szData != NULL)
        {
            bHas = 1;
            break;
        }
    }
    anj_mutex_unlock(&s_stAiotCmdMutex);
    return bHas;
}

static void anj_aiot_cmd_update(char *szReportID)
{
    anj_mutex_lock(&s_stAiotCmdMutex);
    for (int i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
    {
        if (strcmp(s_cmd_info[i].szReportID, szReportID) == 0)
        {
            if (s_cmd_info[i].handler)
                anj_aiot_cmd_store_property(&s_cmd_info[i], s_cmd_info[i].handler());
            break;
        }
    }
    anj_mutex_unlock(&s_stAiotCmdMutex);
}

static int anj_aiot_cmd_record_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    RecordConfig *pstRecordConfig = (RecordConfig *)getRecordConfig();
    RecordConfig stRecordConfigArray[ANJ_CAMERA_MAX_NUMS];
    memcpy(stRecordConfigArray, pstRecordConfig, sizeof(RecordConfig) * ANJ_CAMERA_MAX_NUMS);
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        RecordConfig stRecordConfig = stRecordConfigArray[cameraIndex];
        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];
        MotionDetectAlarm *pstMdAlarm = &stAlarmCfg.normalAlarm.motionDetectAlarm[cameraIndex];

        if (child->valueint == 1) // 告警录像
        {
            // 1. 开启移动侦测和移动侦测报警录像

            pstMdAlarm->enable = 1;

            pstPdAlarm->enable = 1;
            pstPdAlarm->arming_flag = ARMING_ALLDAY;
            stRecordConfig.motionRecordCfg.localStore = 1;
            stRecordConfig.motionRecordCfg.stream = 0;
            strcpy(stRecordConfig.motionRecordCfg.mediaType.typeName, "AV");
            stRecordConfig.motionRecordCfg.stopNoAlarm = 0;

            // 2. 关闭定时录像
            stRecordConfig.scheduleRecordCfg.localStore = 0;
        }
        else if (child->valueint == 2) // 定时录像
        {
            // 1. 关闭移动侦测和移动侦测报警录像
            stRecordConfig.motionRecordCfg.localStore = 0;

            // 2. 开启定时录像
            stRecordConfig.scheduleRecordCfg.localStore = 1;
            stRecordConfig.scheduleRecordCfg.stream = 0;
            strcpy(stRecordConfig.scheduleRecordCfg.mediaType.typeName, "AV");

            // 设置一下时间段
            SetAllTimeSpan(&stRecordConfig.scheduleRecordCfg.timeSpan);
        }
    }
    iRet |= anj_config_alarm_motion_set(stAlarmCfg.normalAlarm.motionDetectAlarm);
    iRet |= anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    iRet |= anj_config_record_set(stRecordConfigArray);

    return iRet;
}

static int anj_aiot_cmd_flip_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;
        if (child->valueint == 0) // 正常
        {
            stVideoCapture.vflip = 0;
            stVideoCapture.hflip = 0;
        }
        else if (child->valueint == 3) // 垂直翻转
        {
            stVideoCapture.vflip = 1;
            stVideoCapture.hflip = 0;
        }
        else if (child->valueint == 2) // 水平翻转
        {
            stVideoCapture.vflip = 0;
            stVideoCapture.hflip = 1;
        }
        else if (child->valueint == 1) // 对角翻转
        {
            stVideoCapture.vflip = 1;
            stVideoCapture.hflip = 1;
        }

        iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
    }

    return iRet;
}

static int anj_aiot_cmd_lens_cover_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    __INFO("recv LensCover cmd, value=%d\n", child->valueint);

    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo < ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
            {
                continue;
            }
        }

        VideoMaskConfig stVideoMask = pstMediaConfig->videoConfig[cameraIndex].videoMask;
        if (child->valueint != 0)
        {
            stVideoMask.mainStreamMaskList[0].xPos = 0;
            stVideoMask.mainStreamMaskList[0].yPos = 0;
            stVideoMask.mainStreamMaskList[0].width = OSD_IOT_COORDINATE_RATIO;
            stVideoMask.mainStreamMaskList[0].height = OSD_IOT_COORDINATE_RATIO;

            stVideoMask.subStreamMaskList[0].xPos = 0;
            stVideoMask.subStreamMaskList[0].yPos = 0;
            stVideoMask.subStreamMaskList[0].width = OSD_IOT_COORDINATE_RATIO;
            stVideoMask.subStreamMaskList[0].height = OSD_IOT_COORDINATE_RATIO;
        }
        else
        {
            memset(&stVideoMask.mainStreamMaskList[0], 0, sizeof(stVideoMask.mainStreamMaskList[0]));
            memset(&stVideoMask.subStreamMaskList[0], 0, sizeof(stVideoMask.subStreamMaskList[0]));
        }

        iRet |= anj_config_video_mask_set(&stVideoMask, cameraIndex);
    }

    return iRet;
}

static int anj_aiot_cmd_4g_card_set(cJSON *child)
{
    int iRet = 0;
    __INFO("recv work4gcard cmd, value=%d\n", child->valueint);
    if (child->valueint == 0)
    {
        // todo
        // write_buffer_to_file("/tmp/dualstandby_workmode", "1", 1);
    }
    else if (child->valueint == 1)
    {
        // write_buffer_to_file("/tmp/dualstandby_workmode", "2", 1);
    }

    return iRet;
}

static int anj_aiot_cmd_upload_location_set(cJSON *child)
{
    int iRet = 0;
    __INFO("recv UploadLocation cmd, value=%d\n", child->valueint);
    if (child->valueint == 0)
    {
        anj_mw_system_with_param("rm -f %s", AIOT_UPLOAD_LOCATION_FLAG);
    }
    else if (child->valueint == 1)
    {
        anj_mw_system_with_param("touch %s", AIOT_UPLOAD_LOCATION_FLAG);
    }
    return iRet;
}

static int anj_aiot_cmd_power_light_set(cJSON *child)
{
    int iRet = 0;
    __INFO("recv pilotPowerLight cmd, value=%d\n", child->valueint);
    // int power_light = 0;
    if (child->valueint != 0)
    {
        // power_light = 1;
        __INFO("recv pilotPowerLight cmd, child->valueint != 0\n");
    }
    else
    {
        __INFO("recv pilotPowerLight cmd, child->valueint = 0\n");
    }
    // todo
    // iRet = anj_ispctl_light_ctl(index, open, brightness)

    return iRet;
}

int anj_aiot_cmd_weak_light_set(cJSON *child)
{
    int iRet = 0;
    if (SUPPORT_WEAK_LIGHT)
    {
        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();

        for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;

            int brightness = 100;
            if (child->valueint != 0)
            {
                if (stVideoCapture.led_brightness_value == 100)
                {
                    brightness = 50;
                }
            }

            stVideoCapture.led_brightness_value = brightness;
            iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
        }
    }

    return iRet;
}

static int anj_aiot_cmd_stream_work_set(cJSON *child)
{
    __INFO("set Two lens WorkMode=%d\n", child->valueint);
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoEncode stVideoEncode = pstMediaConfig->videoConfig[0].videoEncode;

    int workMode = 0;
    workMode = child->valueint;
    stVideoEncode.twoLensCfg.eTwoLensWorkMode = (TwoLensWorkMode)workMode;
    if (anj_config_video_encode_set(&stVideoEncode, 0))
    {
        anj_video_encode_switch();
    }
    return 0;
}

static int anj_aiot_cmd_intercom_type_set(cJSON *child)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    AudioCapture stAudioCapture = *pstAudioCapture;

    if (child->valueint != 0)
    {
        stAudioCapture.aec_enable = 1;
    }
    else
    {
        stAudioCapture.aec_enable = 0;
    }
    iRet = anj_config_audio_capture_set(&stAudioCapture);
    return iRet;
}

static int anj_aiot_cmd_wdr_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;

        if (child->valueint != 0)
        {
            stVideoCapture.wdr_mode = 1;
            stVideoCapture.wdr_value = 128;
        }
        else
        {
            stVideoCapture.wdr_mode = 0;
        }

        iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
    }
    return iRet;
}

static int anj_aiot_cmd_ircut_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;
        int data = child->valueint;
        __INFO("%s: %d\n", child->string, data);
        if (data >= IRCUT_Mode_Active && data < IRCUT_Mode_MAX)
        {
            stVideoCapture.ircut_mode = (IRCutMode)data;
            iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
            anj_ispctl_config_set();
        }
    }

    return iRet;
}

static int anj_aiot_cmd_light_config_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;
        int data = child->valueint;
        __INFO("%s: %d\n", child->string, data);
        if (data >= LED_PURE_INFRAED && data <= LED_INFRAED_THEN_WHITE)
        {
            stVideoCapture.led_mode = (LedMode)data;
            iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
            anj_ispctl_config_set();
        }
    }

    return iRet;
}

static int anj_aiot_cmd_face_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];

        if (child->valueint != pstPdAlarm->alarmAction.draw_human_enable)
        {
            pstPdAlarm->alarmAction.draw_human_enable = child->valueint;
        }
    }
    iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

static int anj_aiot_cmd_rect_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];

        if (child->valueint != pstPdAlarm->alarmAction.draw_rect_enable)
        {
            pstPdAlarm->alarmAction.draw_rect_enable = child->valueint;
        }
    }
    iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

static int anj_aiot_cmd_human_track_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];

        if (child->valueint != pstPdAlarm->alarmAction.track_human_enable)
        {
            pstPdAlarm->alarmAction.track_human_enable = child->valueint;
        }
    }
    iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

static int anj_aiot_cmd_gunball_track_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];

        if (child->valueint != pstPdAlarm->alarmAction.gunball_track_mode)
        {
            pstPdAlarm->alarmAction.gunball_track_mode = child->valueint;
        }
    }
    iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

static int anj_aiot_cmd_twinkle_rect_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];

        if (child->valueint != pstPdAlarm->alarmAction.rect_twinkle_enable)
        {
            pstPdAlarm->alarmAction.rect_twinkle_enable = child->valueint;
        }
    }
    iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

static int anj_aiot_cmd_datnight_mode_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;
        if (child->valueint == 0) // 白天
        {
            stVideoCapture.ircut_mode = IRCUT_Mode_LIGHT_ALWAYS_OFF;
        }
        else if (child->valueint == 1) // 夜晚模式
        {
            stVideoCapture.ircut_mode = IRCUT_Mode_LIGHT_ALWAYS_ON;
        }
        else // 自动模式
        {
            stVideoCapture.ircut_mode = IRCUT_Mode_Active; // TODO or IRCUT_Mode_Active
        }

        iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
        anj_ispctl_config_set();
    }

    return iRet;
}

static int anj_aiot_cmd_mic_set(cJSON *child)
{
    int iRet = 0;

    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    AudioCapture stAudioCapture = *pstAudioCapture;

    if (child->valueint != 0)
        stAudioCapture.volume_capture = 80;
    else
        stAudioCapture.volume_capture = 0;

    iRet = anj_config_audio_capture_set(&stAudioCapture);

    return iRet;
}

static int anj_aiot_cmd_speaker_volume_set(cJSON *child)
{
    int iRet = 0;

    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    AudioCapture stAudioCapture = *pstAudioCapture;
    stAudioCapture.volume_play = child->valueint * 10;
    iRet = anj_config_audio_capture_set(&stAudioCapture);

    return iRet;
}

static int anj_aiot_cmd_shutdown_set(cJSON *child)
{
    int iRet = 0;

    SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
    SystemConfig stSystemConfig = *pstSystemConfig;

    if (child->valueint != 0)
        stSystemConfig.maintainCfg.enable = 1;
    else
        stSystemConfig.maintainCfg.enable = 0;
    iRet = anj_config_system_set(&stSystemConfig);

    return iRet;
}

static int anj_aiot_cmd_face_sensitivity_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }

        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];

        if (child->valueint != 0)
        {
            pstPdAlarm->enable = 1;
            pstPdAlarm->sensitivity = child->valueint * 20;
        }
        else
        {
            pstPdAlarm->enable = 0;
        }
    }
    iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

static int anj_aiot_cmd_monitor_mode_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    __INFO("set MonitoringMode:%d\n", child->valueint);
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }

        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];
        MotionDetectAlarm *pstMdAlarm = &stAlarmCfg.normalAlarm.motionDetectAlarm[cameraIndex];

        int alarmOpen = (child->valueint == 0 ? 1 : 0);

        smart_mask_e eSmartMask = anj_smart_mask_get();
        if (SMART_CHECK_MASK(eSmartMask, SMART_HUMAN_MASK))
        {
            pstPdAlarm->enable = 1;
            pstPdAlarm->alarmAction.alarm_push.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
            pstPdAlarm->alarmAction.alarm_led_enable.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
            pstPdAlarm->alarmAction.notify_alarmserver_enable.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
            pstPdAlarm->alarmAction.audioAction.enable.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
        }
        else // motion detect
        {
            pstMdAlarm->enable = alarmOpen;
            pstMdAlarm->alarmAction.alarm_led_enable.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
            pstMdAlarm->alarmAction.alarm_push.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
            pstMdAlarm->alarmAction.notify_alarmserver_enable.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
            pstMdAlarm->alarmAction.audioAction.enable.enable_flag = alarmOpen > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
        }

        // TODO 语音提示
        if (alarmOpen)
        {
        }
        else
        {
        }
    }
    iRet |= anj_config_alarm_motion_set(stAlarmCfg.normalAlarm.motionDetectAlarm);
    iRet |= anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

int anj_aiot_cmd_shutdown_plan_set(cJSON *child)
{
    int iRet = -1;
    SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
    SystemConfig stSystemConfig = *pstSystemConfig;

    cJSON *object = cJSON_GetArrayItem(child, 0);
    if (object)
    {
        cJSON *enable;

        enable = cJSON_GetObjectItem(object, "Enable");

        if (enable && enable->type == cJSON_Number)
        {
            if (enable->valueint != 0)
            {
                int daytotal = 0;
                cJSON *day = cJSON_GetObjectItem(object, "DayOfWeek");
                cJSON *sec = cJSON_GetObjectItem(object, "BeginTime");
                cJSON *RepeatDays = cJSON_GetObjectItem(object, "RepeatDays");

                if (day && sec && day->type == cJSON_Number && sec->type == cJSON_Number)
                {
                    stSystemConfig.maintainCfg.day = day->valueint;
                    stSystemConfig.maintainCfg.time.hour = sec->valueint / (3600);
                    stSystemConfig.maintainCfg.time.minute = (sec->valueint % 3600) / 60;
                    stSystemConfig.maintainCfg.time.sec = (sec->valueint % 3600) % 60;

                    if (RepeatDays && RepeatDays->type == cJSON_String && RepeatDays->valuestring)
                    {
                        /* sun,mon,tue,wed,thu,fri,sat */
                        int i = 0;
                        char *string = RepeatDays->valuestring;
                        char daystrlist[7][8] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
                        char *pos = NULL;

                        for (i = 0; i < 7; i++)
                        {
                            if ((pos = strstr(string, daystrlist[i])))
                            {
                                daytotal++;
                                if (pos == string)
                                    stSystemConfig.maintainCfg.day = i;
                            }
                        }
                    }

                    if (daytotal >= 7)
                        stSystemConfig.maintainCfg.day = 7;

                    iRet = anj_config_system_set(&stSystemConfig);
                }
            }
        }
    }

    return iRet;
}

int anj_aiot_cmd_alarm_plan_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        PdAlarm *pstPdAlarm = &stAlarmCfg.aiAlarm.pdAlarm[cameraIndex];

        int arraysize = cJSON_GetArraySize(child);
        cJSON *timepoint = NULL;

        for (int i = 0; i < arraysize; i++)
        {
            timepoint = cJSON_GetArrayItem(child, i);

            int EndTime = 0;
            int BeginTime = 0;
            char RepeatDays[128] = {0};

            cJSON *jsonBeginTime = cJSON_GetObjectItem(timepoint, "BeginTime");
            cJSON *jsonEndTime = cJSON_GetObjectItem(timepoint, "EndTime");
            cJSON *jsonRepeatDays = cJSON_GetObjectItem(timepoint, "RepeatDays");

            if (jsonBeginTime && jsonEndTime && jsonRepeatDays)
            {
                BeginTime = jsonBeginTime->valueint;
                EndTime = jsonEndTime->valueint;
                strcpy(RepeatDays, jsonRepeatDays->valuestring);

                TimeSpanList timeSpanList = {0};
                setTimeSpanList(&timeSpanList, BeginTime, EndTime, RepeatDays);
                TransTimeSpan2New(&timeSpanList, &pstPdAlarm->timeSpan);
            }
            else
                continue;
        }
    }
    iRet = anj_config_alarm_pd_set(stAlarmCfg.aiAlarm.pdAlarm);
    return iRet;
}

int anj_aiot_cmd_ptz_advance_state_set(cJSON *child)
{
    int iRet = 0;
    PtzCmdParse stPtzCmdParse = {0};
    cJSON *p = NULL;
    p = cJSON_GetObjectItem(child, "AreaScanning");
    if (NULL != p && p->type == cJSON_Number)
    {
        if (p->valueint == 1)
        {
            // 调用区域扫描开启
            strncpy(stPtzCmdParse.ptzCmd, "AreaScanOn", sizeof(stPtzCmdParse.ptzCmd));
        }
        else
        {
            // 调用区域扫描关闭预置点
            strncpy(stPtzCmdParse.ptzCmd, "AreaScanOff", sizeof(stPtzCmdParse.ptzCmd));
        }
    }
    p = cJSON_GetObjectItem(child, "GuardPosition");
    if (NULL != p && p->type == cJSON_Number)
    {
        if (p->valueint == 1)
            __INFO("GuardPosition open\n");
        // 调用看守位功能开启
        else if (p->valueint == 0)
            __INFO("GuardPosition close\n");
        // 调用看守位功能关闭预置点
    }
    p = cJSON_GetObjectItem(child, "PdTracking");
    if (NULL != p && p->type == cJSON_Number)
    {
        if (p->valueint == 1)
        {
            // 调用跟踪开启
            strncpy(stPtzCmdParse.ptzCmd, "TrackOn", sizeof(stPtzCmdParse.ptzCmd));
        }
        else
        {
            // 调用跟踪关闭预置点
            strncpy(stPtzCmdParse.ptzCmd, "TrackOff", sizeof(stPtzCmdParse.ptzCmd));
        }
    }
    p = cJSON_GetObjectItem(child, "CruisesSwitch");
    if (NULL != p && p->type == cJSON_Number)
    {
        if (p->valueint == 1)
        {
            // 调用巡航开启
            strncpy(stPtzCmdParse.ptzCmd, "CruiseOn", sizeof(stPtzCmdParse.ptzCmd));
        }
        else
        {
            // 调用巡航关闭预置点
            strncpy(stPtzCmdParse.ptzCmd, "CruiseOff", sizeof(stPtzCmdParse.ptzCmd));
        }
    }
    __INFO("ptzCmd:%s\n", stPtzCmdParse.ptzCmd);
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);

    return iRet;
}

static int anj_aiot_cmd_motion_detect_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    __INFO("set MonitoringMode:%d\n", child->valueint);
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    AlarmConfig stAlarmCfg = *pstAlarmConfig;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }

        MotionDetectAlarm *pstMdAlarm = &stAlarmCfg.normalAlarm.motionDetectAlarm[cameraIndex];

        memset(&(pstMdAlarm->timeSpan), 0, sizeof(TimeSpanCfg));

        /*
            {"MotionDetect":{"timestrategy":"0:-2130706434,1:-2147483648,2:-2130706434,3:-2147483648,4:-2147483648,5:-2147483648,6:-2147483648,","duration":0,
            "mbdesc":"0000000000000000000000,0000000000000000000000,0000000000000000000000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0001111111111111111000,0000000000000000000000,0000000000000000000000,0000000000000000000000,
            ","white_light":0,"alarm_type":0,"level":3,"enable":1,"indoor":0,"blink":0,"alarm_enable":0,"thresh":0}}
        */

        cJSON *timestrategy = NULL;
        timestrategy = cJSON_GetObjectItem(child, "timestrategy");

        TimeSpanList timeSpanList = {0};
        if (timestrategy)
        {
            setTimeSpanByStr(&timeSpanList, timestrategy->valuestring);
        }
        TransTimeSpan2New(&timeSpanList, &pstMdAlarm->timeSpan);

        cJSON *mbdesc = NULL;
        mbdesc = cJSON_GetObjectItem(child, "mbdesc");
        if (mbdesc != NULL && mbdesc->valuestring != NULL)
        {
            memset(pstMdAlarm->blockCfg, 0, sizeof(pstMdAlarm->blockCfg));
            char *copy = strdup(mbdesc->valuestring);
            if (!copy)
                return iRet;

            char *token = strtok(copy, ",");
            int pos = 0;

            while (token && pos < MAX_MOTIONDETECT_CONFIG_STRING - 1)
            {
                int len = strlen(token);
                if (pos + len >= MAX_MOTIONDETECT_CONFIG_STRING)
                    len = MAX_MOTIONDETECT_CONFIG_STRING - pos - 1;

                memcpy(pstMdAlarm->blockCfg + pos, token, len);
                pos += len;

                token = strtok(NULL, ",");
            }

            pstMdAlarm->blockCfg[pos] = '\0';
            anj_mw_free(copy);
        }

        cJSON *enable = NULL;
        enable = cJSON_GetObjectItem(child, "enable");
        if (enable && enable->type == cJSON_Number)
        {
            pstMdAlarm->enable = enable->valueint;
        }

        cJSON *level = NULL;
        level = cJSON_GetObjectItem(child, "level");
        if (level && level->type == cJSON_Number)
        {
            if (level->valueint == 0)
            {
                pstMdAlarm->sensitivity = 0;
            }
            else if (level->valueint == 1)
            {
                pstMdAlarm->sensitivity = 30;
            }
            else if (level->valueint == 2)
            {
                pstMdAlarm->sensitivity = 60;
            }
            else
            {
                pstMdAlarm->sensitivity = 90;
            }
        }

        cJSON *alarm_enable = NULL;
        alarm_enable = cJSON_GetObjectItem(child, "alarm_enable");
        if (alarm_enable && alarm_enable->type == cJSON_Number)
        {
            pstMdAlarm->alarmAction.audioAction.enable.enable_flag = alarm_enable->valueint > 0 ? ARMING_ALLDAY : ARMING_DISABLE;
            if (alarm_enable->valueint)
            {
                pstMdAlarm->alarmAction.audioAction.times = 1;
                pstMdAlarm->alarmAction.audioAction.intervalsecnods = 1;
            }

            cJSON *alarm_type = NULL;
            alarm_type = cJSON_GetObjectItem(child, "alarm_type");
            if (alarm_type && alarm_type->type == cJSON_Number)
            {
                // TODO
            }
        }

        cJSON *white_light = NULL;
        white_light = cJSON_GetObjectItem(child, "white_light");
        if (white_light && white_light->type == cJSON_Number)
        {
            if (white_light->valueint)
            {
                // TODO
            }
            else
            {
                // TODO
            }
        }
    }
    iRet = anj_config_alarm_motion_set(stAlarmCfg.normalAlarm.motionDetectAlarm);
    return iRet;
}

static int anj_aiot_cmd_alarm_freq_set(cJSON *child)
{
    int iRet = 0;
    cJSON *frequency = cJSON_GetObjectItem(child, "frequency");
    if (frequency->type == cJSON_Number)
    {
        s_stSendEventFreq = frequency->valueint;
        if (s_stSendEventFreq <= 0)
        {
            s_stSendEventFreq = ALARM_PUSH_FREQ_DEFAULT;
        }
        char freqBuffer[32] = {0};
        snprintf(freqBuffer, sizeof(freqBuffer), "%d", s_stSendEventFreq);
        anj_mw_write_file(P2P_ID_SENDEVENT_FREQ, 0, freqBuffer, strlen(freqBuffer));
    }
    return iRet;
}

static int anj_aiot_cmd_ircut_nighttime_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    DayTimeSpan data = {0};

    cJSON *p = NULL;
    p = cJSON_GetObjectItem(child, "hour_start");
    if (NULL != p && p->type == cJSON_Number)
    {
        data.startTime.hour = p->valueint;
    }
    p = cJSON_GetObjectItem(child, "min_start");
    if (NULL != p && p->type == cJSON_Number)
    {
        data.startTime.minute = p->valueint;
    }
    p = cJSON_GetObjectItem(child, "sec_start");
    if (NULL != p && p->type == cJSON_Number)
    {
        data.startTime.sec = p->valueint;
    }
    p = cJSON_GetObjectItem(child, "hour_end");
    if (NULL != p && p->type == cJSON_Number)
    {
        data.endTime.hour = p->valueint;
    }
    p = cJSON_GetObjectItem(child, "min_end");
    if (NULL != p && p->type == cJSON_Number)
    {
        data.endTime.minute = p->valueint;
    }
    p = cJSON_GetObjectItem(child, "sec_end");
    if (NULL != p && p->type == cJSON_Number)
    {
        data.endTime.sec = p->valueint;
    }

    __INFO("%s: %02d:%02d:%02d -> %02d:%02d:%02d\n", child->string,
           data.startTime.hour, data.startTime.minute, data.startTime.sec,
           data.endTime.hour, data.endTime.minute, data.endTime.sec);

    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        VideoCaptureCfg stVideoCapture = pstMediaConfig->videoConfig[cameraIndex].videoCapture;
        stVideoCapture.ircut_nighttime = data;
        iRet = anj_config_video_capture_set(&stVideoCapture, cameraIndex);
        anj_ispctl_config_set();
    }

    return iRet;
}

static int anj_aiot_cmd_osd_set(cJSON *child, int nChannelNo)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (nChannelNo >= 0 && nChannelNo <= ANJ_CAMERA_MAX_NUMS)
        {
            if (cameraIndex != nChannelNo)
                continue;
        }
        VideoOverlay stVideoOverlay = pstMediaConfig->videoConfig[cameraIndex].overlay;
        cJSON *p = NULL;
        p = cJSON_GetObjectItem(child, "enable");
        if (NULL != p && p->type == cJSON_Number)
        {
            stVideoOverlay.enable = p->valueint > 0 ? 1 : 0;
        }
        p = cJSON_GetObjectItem(child, "title");
        if (NULL != p && p->type == cJSON_String)
        {
            strncpy(stVideoOverlay.titleOverlay.title_utf8, p->valuestring, TITLE_MAX_LEN - 1);
        }
        else
            stVideoOverlay.titleOverlay.title_utf8[0] = 0;

        __INFO("%d: %s\n", stVideoOverlay.enable, stVideoOverlay.titleOverlay.title_utf8);

        iRet = anj_config_overlay_set(&stVideoOverlay, cameraIndex);
    }
    return iRet;
}

static int anj_aiot_cmd_ptz_dir_set(cJSON *child)
{
    int iRet = 0;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    IotPtzConfig stPtzConfig = *pstIotPtzConfig;

    cJSON *p = NULL;
    p = cJSON_GetObjectItem(child, "ptz_level_direction");
    if (NULL != p && p->type == cJSON_Number)
    {
        stPtzConfig.m_ptzDir.HDir = p->valueint > 0 ? 1 : 0;
    }
    p = cJSON_GetObjectItem(child, "ptz_vert_direction");
    if (NULL != p && p->type == cJSON_Number)
    {
        stPtzConfig.m_ptzDir.VDir = p->valueint > 0 ? 1 : 0;
    }

    __INFO("HDir=%d, VDir=%d\n", stPtzConfig.m_ptzDir.HDir, stPtzConfig.m_ptzDir.VDir);
    anj_ptz_config_save(&stPtzConfig);

    return iRet;
}

static int anj_aiot_cmd_audio_capture_set(cJSON *child)
{
    int iRet = 0;
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    AudioCapture stAudioCapture = *pstAudioCapture;

    cJSON *p = NULL;
    p = cJSON_GetObjectItem(child, "volume_capture");
    if (NULL != p && p->type == cJSON_Number)
    {
        stAudioCapture.volume_capture = p->valueint;
    }
    p = cJSON_GetObjectItem(child, "volume_play");
    if (NULL != p && p->type == cJSON_Number)
    {
        stAudioCapture.volume_play = p->valueint;
    }
    p = cJSON_GetObjectItem(child, "capture_amplify");
    if (NULL != p && p->type == cJSON_Number)
    {
        stAudioCapture.amplify = p->valueint > 0 ? 1 : 0;
    }
    p = cJSON_GetObjectItem(child, "aec_enable");
    if (NULL != p && p->type == cJSON_Number)
    {
        stAudioCapture.aec_enable = p->valueint > 0 ? 1 : 0;
    }
    p = cJSON_GetObjectItem(child, "mute_ptz_turn");
    if (NULL != p && p->type == cJSON_Number)
    {
        stAudioCapture.mute_ptz_turn = p->valueint > 0 ? 1 : 0;
    }

    iRet = anj_config_audio_capture_set(&stAudioCapture);
    return iRet;
}

static int anj_aiot_cmd_4g_info_get()
{
    EventResult event_result = {0};
    eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_STATUS_GET, &event_result, NULL);
    G4InfoStruct get4gNetworkInfo = {0};
    if (event_result.result)
        get4gNetworkInfo = *(G4InfoStruct *)event_result.result;
    else
        return 0;

    // 上报SIM卡信息
    cJSON *rootObj = cJSON_CreateObject();
    cJSON *Siminfo = cJSON_CreateObject();
    if (Siminfo)
    {
        cJSON_AddStringToObject(Siminfo, "Manufacturer", get4gNetworkInfo.Manufacturer);
        cJSON_AddStringToObject(Siminfo, "Model", get4gNetworkInfo.Model);
        cJSON_AddStringToObject(Siminfo, "ICCID", get4gNetworkInfo.ICCID);
        cJSON_AddStringToObject(Siminfo, "IMEI", get4gNetworkInfo.IMEI);
        cJSON_AddStringToObject(Siminfo, "IMSI", get4gNetworkInfo.IMSI);
        cJSON_AddStringToObject(Siminfo, "WorkMode", get4gNetworkInfo.WorkMode);
        cJSON_AddStringToObject(Siminfo, "Operator", get4gNetworkInfo.Operator);
        cJSON_AddNumberToObject(Siminfo, "nSvrStatus", get4gNetworkInfo.nSvrStatus);
        cJSON_AddNumberToObject(Siminfo, "nDialStatus", get4gNetworkInfo.nDialStatus);
        cJSON_AddNumberToObject(Siminfo, "nSimStatus", get4gNetworkInfo.nSimStatus);
        //	cJSON_AddNumberToObject(Siminfo, "nSignalLevel", get4gNetworkInfo.nSignalLevel);

        cJSON_AddItemToObject(rootObj, "SIMInfo2", Siminfo);
    }

    char *szData = cJSON_PrintUnformatted(rootObj);
    cJSON_Delete(rootObj);
    __INFO("%s\n", szData);

    char *szG4Ver = Report_Property_string(AIOT_CMD_G4_VER, get4gNetworkInfo.Revision);

    int i = 0;
    anj_mutex_lock(&s_stAiotCmdMutex);
    for (i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
    {
        if (strcmp(s_cmd_info[i].szReportID, AIOT_CMD_SIM_INFO) == 0)
        {
            anj_aiot_cmd_store_property(&s_cmd_info[i], szData);
            szData = NULL;
        }
        else if (strcmp(s_cmd_info[i].szReportID, AIOT_CMD_G4_VER) == 0)
        {
            anj_aiot_cmd_store_property(&s_cmd_info[i], szG4Ver);
            szG4Ver = NULL;
        }
    }
    anj_mutex_unlock(&s_stAiotCmdMutex);
    if (szData != NULL)
    {
        anj_mw_free(szData);
    }
    if (szG4Ver != NULL)
    {
        anj_mw_free(szG4Ver);
    }

    return 0;
}

static char *anj_aiot_cmd_data_get(int nChannelNo, char *szReportID)
{
    char *szData = NULL;
    anj_mutex_lock(&s_stAiotCmdMutex);
    if (szReportID)
    {
        cJSON *pRoot = cJSON_CreateObject();
        if (pRoot == NULL)
        {
            __ERR("cjson create obj failed!\n");
            anj_mutex_unlock(&s_stAiotCmdMutex);
            return szData;
        }

        // 取出所有需要上报的属性，组成一个json
        for (int i = 0; i < (sizeof(s_cmd_info) / sizeof(s_cmd_info[0])); i++)
        {
            if (s_cmd_info[i].szData)
            {
                cJSON *pNode = cJSON_Parse(s_cmd_info[i].szData);
                if (pNode != NULL)
                {
                    cJSON *pChild = pNode->child;
                    while (pChild != NULL)
                    {
                        cJSON *pAddNode = cJSON_Duplicate(pChild, 1);
                        cJSON_AddItemToObject(pRoot, pAddNode->string, pAddNode);
                        pChild = pChild->next;
                    }
                    cJSON_Delete(pNode);
                }
            }
        }

        szData = cJSON_PrintUnformatted(pRoot);
        cJSON_Delete(pRoot);
    }
    else
    {
        // 取出所有需要上报的属性，组成一个json
        for (int i = 0; i < (sizeof(s_cmd_info) / sizeof(s_cmd_info[0])); i++)
        {
            if (strcmp(s_cmd_info[i].szReportID, szReportID) == 0)
            {
                szData = s_cmd_info[i].szData;
                break;
            }
        }
    }
    anj_mutex_unlock(&s_stAiotCmdMutex);
    return szData;
}

static int anj_aiot_cmd_data_set(int nChannelNo, const char *request, const int request_len)
{
    int iRet = -1;
    cJSON *root = NULL;
    ANJ_CHK(((request != NULL) && (request_len > 0)), -1, "input Invalid");

    root = cJSON_Parse(request);
    ANJ_CHK((root != NULL), -1, "cJSON_Parse failed");
    cJSON *child = root->child;

    for (; child != NULL; child = child->next)
    {
        char szSettingName[128] = {0};
        strncpy(szSettingName, child->string, sizeof(szSettingName) - 1);
        string_trim_head(szSettingName);
        string_trim_tail(szSettingName);

        __INFO("chile type=%d, string=%s\n", child->type, szSettingName);

        if (child->type == cJSON_Number)
        {
            /* 媒体设置 */
            if (0 == strcmp(szSettingName, AIOT_CMD_RECORD_MODE)) // StorageRecordMode 本地录像设置
                iRet = anj_aiot_cmd_record_set(child, nChannelNo);
            /* 图像设置 */
            else if (0 == strcmp(szSettingName, AIOT_CMD_IMAGE_FLIP)) // ImageFlipState 吊装模式
                iRet = anj_aiot_cmd_flip_set(child, nChannelNo);
            else if (0 == strcmp(szSettingName, AIOT_CMD_LENS_COVER)) // 视频遮挡
                iRet = anj_aiot_cmd_lens_cover_set(child, nChannelNo);
            else if (0 == strcmp(szSettingName, AIOT_CMD_4G_CARD))
                iRet = anj_aiot_cmd_4g_card_set(child);
            else if (0 == strcmp(szSettingName, AIOT_CMD_POWER_LIGHT))
            {
                iRet = anj_aiot_cmd_power_light_set(child);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_UPLOAD_LOCATION))
            {
                iRet = anj_aiot_cmd_upload_location_set(child);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_PTZ_SPEED)) // 云台手动控制速度
            {
                IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
                IotPtzConfig stPtzConfig = *pstIotPtzConfig;
                stPtzConfig.m_ptzSpeed.HSpeed = child->valueint;
                anj_ptz_config_save(&stPtzConfig);
                iRet = 0;
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_WEAK_LIGHT)) // 省电模式
                iRet = anj_aiot_cmd_weak_light_set(child);
            else if (0 == strcmp(szSettingName, AIOT_CMD_STREAM_MODE)) // 拼接工作模式设置
            {
                iRet = anj_aiot_cmd_stream_work_set(child);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_INTERCOM_TYPE)) // 回音消除启用，禁用
            {
                iRet = anj_aiot_cmd_intercom_type_set(child);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_WDR_SWITCH)) // WDRswitch 逆光模式
            {
                iRet = anj_aiot_cmd_wdr_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_IRCUT_MODE)) // IRCUT工作模式
            {
                iRet = anj_aiot_cmd_ircut_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_LIGHT_CONFIG)) // 灯光控制模式
            {
                iRet = anj_aiot_cmd_light_config_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_FACE_SWITCH)) // FaceFrameSwitch 人形标识
            {
                iRet = anj_aiot_cmd_face_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_RECT_SWITCH)) // FaceFrameSwitch 人形标识
            {
                iRet = anj_aiot_cmd_rect_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_HUMANTRACK)) // FaceFrameSwitch 人形标识
            {
                iRet = anj_aiot_cmd_human_track_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_GUNBALL_TRACK)) // gunball_track_mode枪球跟踪模式
            {
                iRet = anj_aiot_cmd_gunball_track_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_TWINKLE_RECT)) // FaceFrameSwitch 人形标识
            {
                iRet = anj_aiot_cmd_twinkle_rect_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_DAYNIGHT_MODE)) // DayNightMode 夜视功能
            {
                iRet = anj_aiot_cmd_datnight_mode_set(child, nChannelNo);
            }
            /* 高级设置 */
            /* 高级设置->音频设置 */
            else if (0 == strcmp(szSettingName, AIOT_CMD_MIC_SWITCH)) // MicSwitch 麦克风
            {
                iRet = anj_aiot_cmd_mic_set(child);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_SPEAKER_VOLUME)) // SpeakerVolume 音量
            {
                iRet = anj_aiot_cmd_speaker_volume_set(child);
            }
            /* 高级设置->维护 */
            else if (0 == strcmp(szSettingName, AIOT_CMD_SHUTDOWN_SWITCH)) // ShutdownSwitch 定时关机
            {
                iRet = anj_aiot_cmd_shutdown_set(child);
            }
            /* 报警设置 */
            else if (0 == strcmp(szSettingName, AIOT_CMD_ALARM_SOUND_LEVEL))
            {
                // TODO
            }
            /* 人形检测 */
            else if (0 == strcmp(szSettingName, AIOT_CMD_FACE_SENSITIVITY)) // PeopleDetectSensitivity 人形检测
            {
                iRet = anj_aiot_cmd_face_sensitivity_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_MONITOR_MODE)) // 0:离家(布防) 1:在家（撤防）
            {
                iRet = anj_aiot_cmd_monitor_mode_set(child, nChannelNo);
            }
            else // other
            {
            }
        }
        else if (child->type == cJSON_String && child->valuestring)
        {
        }
        else if (child->type == cJSON_Array && child->child)
        {
            /* 高级设置 */
            /* 高级设置->维护 */
            if (0 == strcmp(szSettingName, AIOT_CMD_SHUTDOWN_PLAN)) // ShutdownPlan 定时关机时间设置
                iRet = anj_aiot_cmd_shutdown_plan_set(child);
            else if (0 == strcmp(szSettingName, AIOT_CMD_ALARM_PLAN))
                iRet = anj_aiot_cmd_alarm_plan_set(child, nChannelNo);
        }
        else if (child->type == cJSON_Object)
        {
            if (0 == strcmp(szSettingName, AIOT_CMD_PTZ_ADVANCE_STATE)) // PTZ状态
                iRet = anj_aiot_cmd_ptz_advance_state_set(child);
            else if (0 == strcmp(szSettingName, AIOT_CMD_MOTION_DETECT)) // 移动侦测
            {
                iRet = anj_aiot_cmd_motion_detect_set(child, nChannelNo);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_ALARM_FREQ))
            {
                iRet = anj_aiot_cmd_alarm_freq_set(child);
            }
            else if (0 == strcmp(szSettingName, AIOT_CMD_IRCUT_NIGHTTIME))
                iRet = anj_aiot_cmd_ircut_nighttime_set(child, nChannelNo);
            else if (0 == strcmp(szSettingName, AIOT_CMD_OSD))
                iRet = anj_aiot_cmd_osd_set(child, nChannelNo);
            else if (0 == strcmp(szSettingName, AIOT_CMD_PTZ_DIR))
                iRet = anj_aiot_cmd_ptz_dir_set(child);
            else if (0 == strcmp(szSettingName, AIOT_CMD_AUDIO_CAPTURE))
                iRet = anj_aiot_cmd_audio_capture_set(child);
            else if (0 == strcmp(szSettingName, AIOT_CMD_AFCFG))
            {
                // TODO: 暂无 AF 产品
            }
        }
        if (iRet == 0)
        {
            anj_aiot_cmd_update(szSettingName);
        }
    }

endFunc:
    if (root)
        cJSON_Delete(root);
    return iRet;
}

static int anj_aiot_cmd_debug(const char *request)
{
    int iRet = 0;
    cJSON *root = NULL;
    ANJ_CHK((request != NULL), -1, "input Invalid");

    root = cJSON_Parse(request);
    ANJ_CHK((root != NULL), -1, "cJSON_Parse failed");

    char szCmd[512] = {0};

    cJSON *child = cJSON_GetObjectItem(root, "cmd");
    if (child != NULL && child->valuestring)
    {
        snprintf(szCmd, sizeof(szCmd), "%s", child->valuestring);
        __INFO("%s\n", szCmd);
        anj_mw_system(szCmd);
        anj_mw_log_cmd_proc(szCmd, strlen(szCmd));
    }
    else
    {
        child = cJSON_GetObjectItem(root, "cloudstorageupdate");
        if (child != NULL)
        {
            gct_api_forceupdate_cloud();
        }
    }

    cJSON_Delete(root);
    root = NULL;
endFunc:
    return iRet;
}

static int anj_aiot_cmd_upload(const char *request)
{
    int iRet = 0;
    cJSON *root = NULL;
    ANJ_CHK((request != NULL), -1, "input Invalid");

    root = cJSON_Parse(request);
    ANJ_CHK((root != NULL), -1, "cJSON_Parse failed");

    char szFileName[512] = {0};
    cJSON *child = cJSON_GetObjectItem(root, "filename");
    if (child != NULL)
    {
        snprintf(szFileName, sizeof(szFileName), "%s", child->valuestring);
    }

    cJSON_Delete(root);
    string_trim_tail(szFileName);
    __INFO("%s\n", szFileName);

    if (!anj_mw_file_exists(szFileName))
    {
        __ERR("not exist %s", szFileName);
        goto endFunc;
    }

    DevInfo *pstDevInfo = getDevInfo();
    anj_ser_info *pstSerInfo = getSerInfo();

    char szCmd[64] = {0};
    sprintf(szCmd, "/opt/ch/oss_ali");
    if (!anj_mw_file_exists(szCmd))
    {
        sprintf(szCmd, "/tmp/oss_ali");
        if (!anj_mw_file_exists(szCmd))
        {
            anj_mw_system_with_param("wget %s -P /tmp", AIOT_OSS_LINK);
        }
        if (anj_mw_file_exists(szCmd))
        {
            anj_mw_system("chmod +x /tmp/oss_ali");
        }
    }

    if (anj_mw_file_exists(szCmd))
    {
        char cmd[1024] = {0};
        snprintf(cmd, sizeof(cmd), "%s --upload --pk %s --dn %s --sn %s --localfile %s &",
                 szCmd, pstSerInfo->uid, pstDevInfo->sn, pstDevInfo->sn, szFileName);
        __INFO("%s\n", cmd);
        anj_mw_system(cmd);
    }
    else
    {
        __ERR("download oss_ali failed");
    }
endFunc:
    return iRet;
}

static int anj_aiot_cmd_ota_download(const char *request)
{
    int iRet = 0;
    cJSON *root = NULL;
    ANJ_CHK((request != NULL), -1, "input Invalid");
    root = cJSON_Parse(request);
    ANJ_CHK((root != NULL), -1, "cJSON_Parse failed");

    char *url = NULL;
    char szFileName[64] = {0};
    cJSON *child = cJSON_GetObjectItem(root, "filename");
    if (child != NULL)
    {
        url = child->valuestring;
        if (!url)
        {
            goto endFunc;
        }

        const char *lastSlash = strrchr(url, '/');
        if (lastSlash)
        {
            const char *filenameStart = lastSlash + 1;
            if (*filenameStart)
            {
                snprintf(szFileName, sizeof(szFileName), "%s", filenameStart);
            }
        }
    }

    string_trim_tail(szFileName);
    __INFO("url: %s filename:%s\n", url, szFileName);
    if (strlen(szFileName) > 0)
    {
        modules_uninit("anj_ser", NULL);
        anj_mw_system_with_param("wget -c -T 60 %s -P /tmp", url);

        char szOtaFile[128] = {0};
        sprintf(szOtaFile, "/tmp/%s", szFileName);
        if (anj_mw_file_exists(szOtaFile))
        {
            unsigned long long FileLen = 0;
            anj_mw_read_file_len(szOtaFile, &FileLen);
            APPBIN_UPDATE_DATA updateData = {0};
            snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", szOtaFile);
            updateData.nPhyAddr = 0;
            updateData.nFileLen = (unsigned int)FileLen;
            anj_sysmng_app_update(&updateData);
        }
        else
        {
            __WARN("ota file:%s not found, reboot \n", szOtaFile);
            __RECORD_LOG_INFO("ota file:%s not found, reboot \n", szOtaFile);
            anj_sysmng_reboot();
        }
    }
endFunc:
    if (root)
    {
        cJSON_Delete(root);
    }
    return iRet;
}

static int anj_aiot_cmd_scare_off(const char *request)
{
    int iRet = 0;
    cJSON *root = NULL;
    ANJ_CHK((request != NULL), -1, "input Invalid");
    root = cJSON_Parse(request);
    ANJ_CHK((root != NULL), -1, "cJSON_Parse failed");

    int play_sound = 1;
    int play_times = 1;
    int led_on = 0;

    cJSON *child = cJSON_GetObjectItem(root, "play_sound");
    if (child != NULL)
    {
        play_sound = child->valueint;
    }

    child = cJSON_GetObjectItem(root, "playtimes");
    if (child != NULL)
    {
        play_times = child->valueint;
    }

    child = cJSON_GetObjectItem(root, "led_on");
    if (child != NULL)
    {
        led_on = child->valueint;
    }

    cJSON_Delete(root);

    __INFO("play_sound=%d, play_times=%d, led_on=%d\n", play_sound, play_times, led_on);
    // todo
    // MsgScareOff(play_sound, led_on, play_times);
endFunc:
    if (root)
    {
        cJSON_Delete(root);
    }
    return iRet;
}

static int anj_aiot_cmd_ptz_action(const char *serviceid, const char *request)
{
    int iRet = 0;
    cJSON *root = NULL;
    ANJ_CHK((request != NULL), -1, "input Invalid");

    PTZCtrlInfo ptzctrlInfo = {0};

    if (strstr(serviceid, "Stop"))
    {
        ptzctrlInfo.ActionType = -1;
    }
    else
    {
        root = cJSON_Parse(request);
        ANJ_CHK((root != NULL), -1, "cJSON_Parse failed");

        cJSON *child = cJSON_GetObjectItem(root, "ActionType");
        ANJ_CHK((child != NULL), -1, "cJSON_GetObjectItem ActionType failed");

        ptzctrlInfo.ActionType = child->valueint;

        child = cJSON_GetObjectItem(root, "Step");
        ANJ_CHK((child != NULL), -1, "cJSON_GetObjectItem Step failed");

        ptzctrlInfo.Step = child->valueint;

        child = cJSON_GetObjectItem(root, "duration");
        if (child != NULL)
        {
            ptzctrlInfo.duration = child->valueint;
        }

        child = cJSON_GetObjectItem(root, "Name");
        if (child != NULL)
        {
            if (child->valuestring)
            {
                strcpy(ptzctrlInfo.name, child->valuestring);
            }
        }
    }

    EventResult event_result = {0};
    PtzCmdParse stPtzCmdParse = {0};
    int ptz_h = -1;
    int ptz_v = -1;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    if (pstIotPtzConfig->m_ptzSpeed.HSpeed > 0)
    {
        ptz_h = pstIotPtzConfig->m_ptzSpeed.HSpeed;
    }

    if (pstIotPtzConfig->m_ptzSpeed.VSpeed > 0)
    {
        ptz_v = pstIotPtzConfig->m_ptzSpeed.VSpeed;
    }

    __INFO("ptzctrlInfo.ActionType:%d\n", ptzctrlInfo.ActionType);

    switch (ptzctrlInfo.ActionType)
    {
    case AIOT_PTZ_STOP: // stop
        strncpy(stPtzCmdParse.ptzCmd, "stop", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_LEFT: // 左
        strncpy(stPtzCmdParse.ptzCmd, "left", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        if (ptz_h != -1)
            stPtzCmdParse.panSpeed = ptz_h;

        if (ptz_v != -1)
            stPtzCmdParse.tiltSpeed = ptz_v;
        break;

    case AIOT_PTZ_RIGHT: // 右
        strncpy(stPtzCmdParse.ptzCmd, "right", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        if (ptz_h != -1)
            stPtzCmdParse.panSpeed = ptz_h;

        if (ptz_v != -1)
            stPtzCmdParse.tiltSpeed = ptz_v;
        break;

    case AIOT_PTZ_UP: // 上
        strncpy(stPtzCmdParse.ptzCmd, "up", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        if (ptz_h != -1)
            stPtzCmdParse.panSpeed = ptz_h;

        if (ptz_v != -1)
            stPtzCmdParse.tiltSpeed = ptz_v;

        break;

    case AIOT_PTZ_DOWN: // 下
        strncpy(stPtzCmdParse.ptzCmd, "down", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        if (ptz_h != -1)
            stPtzCmdParse.panSpeed = ptz_h;

        if (ptz_v != -1)
            stPtzCmdParse.tiltSpeed = ptz_v;
        break;

    // 4-7：上左、上右、下左、下右
    case AIOT_PTZ_UP_LEFT:
        strncpy(stPtzCmdParse.ptzCmd, "left_up", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        break;

    case AIOT_PTZ_UP_RIGHT:
        strncpy(stPtzCmdParse.ptzCmd, "right_up", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        break;

    case AIOT_PTZ_DOWN_LEFT:
        strncpy(stPtzCmdParse.ptzCmd, "left_down", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        break;

    case AIOT_PTZ_DOWN_RIGHT:
        strncpy(stPtzCmdParse.ptzCmd, "right_down", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        break;

    // 8 ~ 9:放大 缩小
    case AIOT_PTZ_SCALE_UP:
        strncpy(stPtzCmdParse.ptzCmd, "zoomtele", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.panSpeed = ptzctrlInfo.Step;
        stPtzCmdParse.tiltSpeed = ptzctrlInfo.Step;
        break;

    case AIOT_PTZ_SCALE_DOWN:
        strncpy(stPtzCmdParse.ptzCmd, "zoomwide", sizeof(stPtzCmdParse.ptzCmd));
        break;

    // 101、102：设置调用预置点
    case AIOT_PTZ_SET_PRESET:
    {
        char escapeBuf[2048] = {0};
        strncpy(stPtzCmdParse.ptzCmd, "setpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = ptzctrlInfo.Step;
        stPtzCmdParse.flag = 1;
        strncpy(stPtzCmdParse.presetName, copy_with_escape(escapeBuf, ptzctrlInfo.name), sizeof(stPtzCmdParse.presetName));
        stPtzCmdParse.presetName[sizeof(stPtzCmdParse.presetName) - 1] = '\0';
    }
    break;

    case AIOT_PTZ_CALL_PRESET:
        strncpy(stPtzCmdParse.ptzCmd, "callpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = ptzctrlInfo.Step;
        break;

    case AIOT_PTZ_CLEAR_PRESET:
        strncpy(stPtzCmdParse.ptzCmd, "clearpreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = ptzctrlInfo.Step;
        break;

    case AIOT_PTZ_FOCUS_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "FocusFarAutoOff", sizeof(stPtzCmdParse.ptzCmd));
        break;
    case AIOT_PTZ_FOCUS_ON:
        strncpy(stPtzCmdParse.ptzCmd, "FocusNearAutoOff", sizeof(stPtzCmdParse.ptzCmd));
        break;
    // followings are advanced PTZ function
    case AIOT_PTZ_TRACK_ON:
        strncpy(stPtzCmdParse.ptzCmd, "TrackOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_TRACK_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "TrackOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_AUTO_SCAN_ON:
        strncpy(stPtzCmdParse.ptzCmd, "AutoScanOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_AUTO_SCAN_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "AutoScanOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_AREA_SCAN_ON:
        strncpy(stPtzCmdParse.ptzCmd, "AreaScanOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_AREA_SCAN_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "AreaScanOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_CRUISE_ON:
        strncpy(stPtzCmdParse.ptzCmd, "CruiseOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_CRUISE_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "CruiseOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_PRESET_ON:
        strncpy(stPtzCmdParse.ptzCmd, "GuardPresetOneOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_PRESET_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "GuardPresetOneOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_TRACE_ON:
        strncpy(stPtzCmdParse.ptzCmd, "GuardTraceOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_TRACE_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "GuardTraceOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_CRUISE_ON:
        strncpy(stPtzCmdParse.ptzCmd, "GuardCuriseOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_CRUISE_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "GuardCuriseOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_AREA_SCAN_ON:
        strncpy(stPtzCmdParse.ptzCmd, "GuardAreaScanOn", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_GUARD_AREA_SCAN_OFF:
        strncpy(stPtzCmdParse.ptzCmd, "GuardAreaScanOff", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_LEFT_MARGIN:
        strncpy(stPtzCmdParse.ptzCmd, "LeftMargin", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_RIGHT_MARGIN:
        strncpy(stPtzCmdParse.ptzCmd, "RightMargin", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_CLEAR_ALL_PRESET:
        strncpy(stPtzCmdParse.ptzCmd, "ClearAllPreset", sizeof(stPtzCmdParse.ptzCmd));
        break;

    case AIOT_PTZ_STOP_GUARD:
        strncpy(stPtzCmdParse.ptzCmd, "StopGuard", sizeof(stPtzCmdParse.ptzCmd));
        break;
    case AIOT_PTZ_REBOOT:
        strncpy(stPtzCmdParse.ptzCmd, "PtzReboot", sizeof(stPtzCmdParse.ptzCmd));
        break;
    case AIOT_PTZ_SCAN_SPEED_UP:
        strncpy(stPtzCmdParse.ptzCmd, "ScanSpeedUp", sizeof(stPtzCmdParse.ptzCmd));
        break;
    case AIOT_PTZ_SCAN_SPEED_DOWN:
        strncpy(stPtzCmdParse.ptzCmd, "ScanSpeedDown", sizeof(stPtzCmdParse.ptzCmd));
        break;
    case AIOT_PTZ_SET_GUARD_PRESET:
        strncpy(stPtzCmdParse.ptzCmd, "SetGuardPreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = ptzctrlInfo.Step;
        stPtzCmdParse.watchGuardTime = ptzctrlInfo.duration;
        break;
    case AIOT_PTZ_CLEAR_GUARD_PRESET:
        strncpy(stPtzCmdParse.ptzCmd, "ClearGuardPreset", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = ptzctrlInfo.Step;
        stPtzCmdParse.watchGuardTime = ptzctrlInfo.duration;
        break;
    case AIOT_PTZ_SET_PRESET_NAME:
    {
        strncpy(stPtzCmdParse.ptzCmd, "SetPresetName", sizeof(stPtzCmdParse.ptzCmd));
        stPtzCmdParse.presetID = ptzctrlInfo.Step;
        stPtzCmdParse.flag = 1;
        char escapeBuf[256];
        strncpy(stPtzCmdParse.presetName, copy_with_escape(escapeBuf, ptzctrlInfo.name), sizeof(stPtzCmdParse.presetName));
        stPtzCmdParse.presetName[sizeof(stPtzCmdParse.presetName) - 1] = '\0';
    }
    break;

    case AIOT_PTZ_NONE:
    {
        string_trim_tail(ptzctrlInfo.name);
        if (strlen(ptzctrlInfo.name) > 0)
        {
            strncpy(stPtzCmdParse.ptzCmd, ptzctrlInfo.name, sizeof(stPtzCmdParse.ptzCmd));
            stPtzCmdParse.ptzCmd[sizeof(stPtzCmdParse.ptzCmd) - 1] = '\0';
        }
    }
    break;

    default:
        __ERR("ptzCtrl no support !!\n");
    }

    if (strlen(stPtzCmdParse.ptzCmd))
    {
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
    }
endFunc:
    if (root)
    {
        cJSON_Delete(root);
    }
    return iRet;
}

int anj_aiot_cmd_dotask(int nChannelNo, const char *serviceid, const char *request)
{
    int iRet = 0;
    /* 重启 */
    if (0 == strcasecmp(serviceid, "Reboot"))
    {
        __WARN("Reboot service\n");
        __RECORD_LOG_INFO("Reboot service\n");
        anj_sysmng_reboot();
    } /* PTZ控制 */
    if (0 == strcasecmp(serviceid, "debugcmd"))
    {
        iRet = anj_aiot_cmd_debug(request);
    }
    else if (0 == strcasecmp(serviceid, "upload"))
    {
        iRet = anj_aiot_cmd_upload(request);
    }
    else if (0 == strcasecmp(serviceid, "otadownload"))
    {
        iRet = anj_aiot_cmd_ota_download(request);
    }
    else if (strcasecmp(serviceid, "otaauto") == 0)
    {
        ota_check_version(1);
    }
    else if (0 == strcasecmp(serviceid, "ScareOff"))
    {
        iRet = anj_aiot_cmd_scare_off(request);
    }
    else if (strstr(serviceid, "PTZAction")) // PTZActionControl
    {
        iRet = anj_aiot_cmd_ptz_action(serviceid, request);
    }
    /* SD卡格式化 */
    else if (0 == strcasecmp(serviceid, "FormatStorageMedium")) // FormatStorageMedium
    {
        anj_sdcard_format(0);
    }
    else if (0 == strcasecmp(serviceid, "remountsdcard")) // 重新挂载
    {
        iRet = anj_sdcard_umount();
        iRet |= anj_sdcard_mount();
    }
    else if (0 == strcasecmp(serviceid, "repartionsdcard")) // 重新分区
    {
        iRet = anj_sdcard_umount();
    }
    else
    {
        __ERR("Cannot found services [%s]", serviceid);
        iRet = -1;
    }
    return iRet;
}

static char *anj_aiot_cmd_json(const char *pDecBuffer, unsigned int cmdlen)
{
    int iRet = 0;
    char *szRspMsg = NULL;
    int bGetProperty = 0;
    int bSetProperty = 0;
    int bServices = 0;
    int nChannelNo = -1;
    char szData[64] = {0};
    cJSON *pDataNode = NULL;

    cJSON *pNode = cJSON_Parse(pDecBuffer);
    if (pNode != NULL)
    {
        cJSON *pChild = pNode->child;
        while (pChild != NULL)
        {
            __INFO("request json name:%s\n", pChild->string);
            if (strcasecmp("GetProperty", pChild->string) == 0)
            {
                if (pChild->type == cJSON_String)
                {
                    if (pChild->valuestring)
                    {
                        __INFO("request json value:%s\n", pChild->valuestring);
                        bGetProperty = 1;
                        snprintf(szData, sizeof(szData), "%s", pChild->valuestring);
                    }
                }
            }
            else if (strcasecmp("SetProperty", pChild->string) == 0)
            {
                if (pChild->type == cJSON_Object)
                {
                    bSetProperty = 1;
                    pDataNode = cJSON_Duplicate(pChild, 1);
                    char *pJsonText = cJSON_PrintUnformatted(pDataNode);
                    if (pJsonText != NULL)
                    {
                        __INFO("json object:%s\n", pJsonText);
                        snprintf(szData, sizeof(szData), "%s", pJsonText);
                        anj_mw_free(pJsonText);
                        pJsonText = NULL;
                    }
                }
            }
            else if (strcasecmp("services", pChild->string) == 0)
            {
                if (pChild->type == cJSON_Object)
                {
                    bServices = 1;
                    pDataNode = cJSON_Duplicate(pChild, 1);
                    char *pJsonText = cJSON_PrintUnformatted(pDataNode);
                    if (pJsonText != NULL)
                    {
                        __INFO("json services:%s\n", pJsonText);
                        snprintf(szData, sizeof(szData), "%s", pJsonText);
                        anj_mw_free(pJsonText);
                        pJsonText = NULL;
                    }
                }
            }
            else if (strcasecmp("Channel", pChild->string) == 0)
            {
                if (pChild->type == cJSON_Number)
                {
                    __INFO("request json value:%d\n", pChild->valueint);
                    nChannelNo = pChild->valueint;
                }
            }

            pChild = pChild->next;
        }

        cJSON_Delete(pNode);
    }

    if (bGetProperty > 0)
    {
        szRspMsg = anj_aiot_cmd_data_get(nChannelNo, szData);
    }
    else if (bSetProperty > 0)
    {
        iRet = anj_aiot_cmd_data_set(nChannelNo, szData, strlen(szData));

        { // 封装回应消息
            cJSON *rootObj = cJSON_CreateObject();
            if (rootObj)
            {
                cJSON_AddNumberToObject(rootObj, "Channel", nChannelNo);
                cJSON *pOrigDataNode = cJSON_Duplicate(pDataNode, 1);
                cJSON_AddItemToObject(rootObj, "SetProperty", pOrigDataNode);
                cJSON_AddNumberToObject(rootObj, "result", iRet);
                szRspMsg = cJSON_PrintUnformatted(rootObj);
                cJSON_Delete(rootObj);
            }
        }
    }
    else if (bServices > 0)
    {
        cJSON *pRootServices = pDataNode;
        if (pRootServices)
        {
            cJSON *pChild = pRootServices->child;

            for (; pChild != NULL; pChild = pChild->next) // 找到services的servicename，以及json数据字符串
            {
                char szServiceName[128];
                strncpy(szServiceName, pChild->string, sizeof(szServiceName) - 1);
                string_trim_head(szServiceName);
                string_trim_tail(szServiceName);

                cJSON *pservicesNode = cJSON_Duplicate(pChild, 1);
                char *pJsonText = cJSON_PrintUnformatted(pservicesNode);
                cJSON_Delete(pservicesNode);

                if (pJsonText != NULL)
                {
                    __INFO("services %s: %s\n", szServiceName, pJsonText);
                    iRet = anj_aiot_cmd_dotask(nChannelNo, szServiceName, pJsonText);
                    anj_mw_free(pJsonText);
                }

                {
                    // 封装回应消息
                    cJSON *rootObj = cJSON_CreateObject();
                    if (rootObj)
                    {
                        cJSON_AddNumberToObject(rootObj, "Channel", nChannelNo);
                        cJSON *pOrigDataNode = cJSON_Duplicate(pDataNode, 1);
                        cJSON_AddItemToObject(rootObj, "services", pOrigDataNode);
                        cJSON_AddNumberToObject(rootObj, "result", iRet);
                        szRspMsg = cJSON_PrintUnformatted(rootObj);
                        cJSON_Delete(rootObj);
                    }
                }

                break;
            }
        }
    }

    if (NULL != pDataNode)
    {
        cJSON_Delete(pDataNode);
        pDataNode = NULL;
    }

    return szRspMsg;
}

static int anj_aiot_cmd_decompress(const char *pSrcBuffer, unsigned int nSrcBuflen, char **pDstBuffer)
{
    int iRet = -1;
    ANJ_CHK(((pSrcBuffer != NULL) && (nSrcBuflen > 0) && (pDstBuffer != NULL)), -1, "input Invalid");

    const AiotTransHeader *pHeader = (const AiotTransHeader *)pSrcBuffer;
    if (FLAG_MAGIC_AIOT_TRANSDATA != pHeader->magic)
    {
        __ERR("magic %llx error.", pHeader->magic);
        goto endFunc;
    }

    __DBG("input length=%u, nDecompressedLength=%u, nCompressedLength=%u, nCompressType=%d\n",
          nSrcBuflen, pHeader->nDecompressedLength, pHeader->nCompressedLength, pHeader->nCompressType);
    switch (pHeader->nCompressType)
    {
    case COMPRESS_NONE:
    {
        if (pHeader->nCompressedLength != pHeader->nDecompressedLength)
        {
            __ERR("datalen %u != %u,  error.\n", pHeader->nCompressedLength, pHeader->nDecompressedLength);
            return iRet;
        }

        if (pHeader->nCompressedLength + sizeof(AiotTransHeader) > nSrcBuflen)
        {
            __ERR("datalen %u + %u > %u,  error.\n", pHeader->nCompressedLength, sizeof(AiotTransHeader), nSrcBuflen);
            return iRet;
        }

        __DBG("datalen=%u, input buflen=%u.\n", pHeader->nCompressedLength + sizeof(AiotTransHeader), nSrcBuflen);

        unsigned int nDecodeBufLen = pHeader->nDecompressedLength;
        char *p_szDecoderBuf = (char *)anj_mw_malloc(nDecodeBufLen);
        if (NULL == p_szDecoderBuf)
        {
            __ERR("anj_mw_malloc failed for %u.\n", nDecodeBufLen);
            goto endFunc;
        }
        p_szDecoderBuf[nDecodeBufLen] = 0;

        iRet = nDecodeBufLen;
        memcpy(p_szDecoderBuf, (void *)(pSrcBuffer + sizeof(AiotTransHeader)), iRet);
        *pDstBuffer = p_szDecoderBuf;
    }
    break;
    case COMPRESS_ZLIB:
    {
        if (pHeader->nCompressedLength + sizeof(AiotTransHeader) > nSrcBuflen)
        {
            __ERR("datalen %u + %u > %u,  error.\n", pHeader->nCompressedLength, sizeof(AiotTransHeader), nSrcBuflen);
            goto endFunc;
        }

        __DBG("datalen=%u, input buflen=%u.\n", pHeader->nCompressedLength + sizeof(AiotTransHeader), nSrcBuflen);

        unsigned int nDecodeBufLen = pHeader->nDecompressedLength + 128;
        char *p_szDecoderBuf = (char *)anj_mw_malloc(nDecodeBufLen);
        if (NULL == p_szDecoderBuf)
        {
            __ERR("anj_mw_malloc failed for %u\n", nDecodeBufLen);
            goto endFunc;
        }

        const Bytef *pData = (const Bytef *)(pSrcBuffer + sizeof(AiotTransHeader));
        uLong nDataLen = pHeader->nCompressedLength;
        iRet = (int)nDecodeBufLen;
        if (uncompress((Bytef *)p_szDecoderBuf, (uLongf *)&iRet, (const Bytef *)pData, (uLong)nDataLen) == 0)
        {
            p_szDecoderBuf[iRet] = 0;
            __DBG("data length %u, uncompress to %d\n", nDataLen, iRet);
            __DBG("%s\n", p_szDecoderBuf);

            if (iRet != pHeader->nDecompressedLength)
            {
                __ERR("decoded length=%d, should be %u\n", iRet, pHeader->nDecompressedLength);
                anj_mw_free(p_szDecoderBuf);

                iRet = -1;
                goto endFunc;
            }

            *pDstBuffer = p_szDecoderBuf;
        }
        else
        {
            __ERR("decoded failed(%d) for srclen %u, dstbuflen %u\n", iRet, nDataLen, nDecodeBufLen);
            anj_mw_free(p_szDecoderBuf);
            p_szDecoderBuf = NULL;
            iRet = -1;
        }
    }
    break;
    default:
    {
        __ERR("nCompressType %d error.\n", pHeader->nCompressType);
    }
    }
endFunc:
    return iRet;
}

static int anj_aiot_cmd_compress(const char *pSrcBuffer, unsigned int nSrcBuflen, char **pDstBuffer, AiotTransDataType nDataType)
{
    int nEncodeLen = -1;

    if (pDstBuffer == NULL)
    {
        __ERR("dst buffer NULL.\n");
        return nEncodeLen;
    }

    char *p_szTransData = NULL;
    char *p_szCompData = NULL;
    unsigned int nDstBuflen = nSrcBuflen + sizeof(AiotTransHeader);
    if (nDstBuflen < 1024)
        nDstBuflen = 1024; // 字符少的时候，压缩后的大小比压缩前还大，所以这里安全起见分多一点
    int nCompOutLen = nDstBuflen;
    GCT_UINT32 nDataLen = nSrcBuflen;

    p_szTransData = (char *)anj_mw_malloc(nDstBuflen);
    if (NULL == p_szTransData)
    {
        __ERR("anj_mw_malloc failed for %u.\n", nDstBuflen);
        return nEncodeLen;
    }

    p_szCompData = p_szTransData + sizeof(AiotTransHeader);
    AiotTransHeader *pHeader = (AiotTransHeader *)p_szTransData;
    pHeader->magic = FLAG_MAGIC_AIOT_TRANSDATA;
    pHeader->nCompressType = COMPRESS_ZLIB;
    pHeader->nDecompressedLength = nSrcBuflen;
    pHeader->nDataType = nDataType;

    *pDstBuffer = p_szTransData;
    int iRet = compress((Bytef *)p_szCompData, (uLongf *)&nCompOutLen, (Bytef *)(pSrcBuffer), (uLongf)nDataLen);
    if (iRet != 0)
    { // 压缩失败，原样填充
        __ERR("compress failed(%d) for srclen %u, dstbuflen %u\n", iRet, nDataLen, nDstBuflen);
        memcpy(p_szCompData, pSrcBuffer, nDataLen);
        pHeader->nCompressType = COMPRESS_NONE;
        pHeader->nCompressedLength = nDataLen;

        nEncodeLen = nDataLen + sizeof(AiotTransHeader);
        return nEncodeLen;
    }
    else
    {
        __DBG("compress data length %u->%u\n", nDataLen, nCompOutLen);
        pHeader->nCompressedLength = nCompOutLen;
        nEncodeLen = nCompOutLen + sizeof(AiotTransHeader);
        return nEncodeLen;
    }
}

static int anj_aiot_cmd_proc(char *pBuf, int nBufLen, int sid)
{
    int iRet = -1;
    char *szRspMsg = NULL;
    char *pDecBuffer = NULL;
    ANJ_CHK(((pBuf != NULL) && (nBufLen > 0)), -1, "input Invalid");
    __DBG("nBufLen = %d,sid=%u,pBuf = %s\n", nBufLen, sid, pBuf);

    int nDeCompressLen = anj_aiot_cmd_decompress(pBuf, nBufLen, &pDecBuffer);
    if (nDeCompressLen <= 0)
    {
        __ERR("decompress length %u error\n", nBufLen);
        goto endFunc;
    }
    ANJ_CHK((pDecBuffer != NULL), -1, "pDecBuffer is null");

    __DBG("decompress length %u->%u OK\n", nBufLen, nDeCompressLen);

    const AiotTransHeader *pHeader = (const AiotTransHeader *)pBuf;
    if (TRANS_DATA_ACP_JSON == pHeader->nDataType)
    {
        __INFO("\nreceived:\n%s\n", pDecBuffer);
        szRspMsg = anj_aiot_cmd_json(pDecBuffer, nDeCompressLen);
        ANJ_CHK((szRspMsg != NULL), -1, "anj_aiot_cmd_json failed");
        __INFO("\nsend:\n%s\n", szRspMsg);
    }
    else
    {
        __INFO("\nreceived:\n%s\n", pDecBuffer);
        szRspMsg = anj_aiot_trans_proc(pDecBuffer, nDeCompressLen);
        ANJ_CHK((szRspMsg != NULL), -1, "anj_aiot_trans_proc failed");
        __INFO("\nsend:\n%s\n", szRspMsg);
    }

    int dstLen = strlen(szRspMsg);
    if (dstLen > 0)
    {
        char *pSendBuffer = NULL;
        int nCompressLen = anj_aiot_cmd_compress(szRspMsg, dstLen, &pSendBuffer, pHeader->nDataType);
        if (nCompressLen <= 0)
        {
            __ERR("compress length %u error\n", dstLen);
        }
        else
        {
            __DBG("compress length %u->%u OK\n", dstLen, nCompressLen);
            gct_apiv4_trans_channel_write(sid, pSendBuffer, nCompressLen);
        }

        if (pSendBuffer)
        {
            anj_mw_free(pSendBuffer);
            pSendBuffer = NULL;
        }
    }

endFunc:
    if (szRspMsg)
    {
        anj_mw_free(szRspMsg);
    }
    if (pDecBuffer)
    {
        anj_mw_free(pDecBuffer);
    }
    return iRet;
}

static int anj_aiot_cmd_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    while (bStart && *bStart)
    {
        FRAME_ENTRY frame = {0};
        if (frame_mgr_pop(&s_stAiotCmdMgr, &frame) > 0)
        {
            anj_aiot_cmd_proc(frame.pFrame, frame.nFrameLen, frame.session);
            if (frame.pFrame)
            {
                anj_mw_free(frame.pFrame);
            }
        }
        usleep(10 * 1000);
    }
    return iRet;
}

int anj_aiot_cmd_data_init()
{
    unsigned long long nowCheckTime = anj_mw_get_cputime_ms(NULL);
    static unsigned long long stCheckTime = 0;
    if (0 == stCheckTime || nowCheckTime - stCheckTime > 60000)
    {
        stCheckTime = nowCheckTime;
        anj_mutex_lock(&s_stAiotCmdMutex);
        for (int i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
        {
            if (s_cmd_info[i].handler)
                anj_aiot_cmd_store_property(&s_cmd_info[i], s_cmd_info[i].handler());
        }
        anj_mutex_unlock(&s_stAiotCmdMutex);
    }

    if (IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_4G ||
        IPC_NETWORK_TYPE == NET_DEV_TYPE_4G)
    {
        static int bFirstReport4GInfo = 1;
        static unsigned long long stCheck4GTime = 0;
        int nMSeconds;
        if (bFirstReport4GInfo)
        {
            nMSeconds = 5000; // 没有上报过则间隔5秒检查4G信息
        }
        else
        {
            nMSeconds = 30000; // 已经上报过则间隔30秒检查4G信息
        }

        if (nowCheckTime - stCheck4GTime > nMSeconds)
        {
            __INFO("ready to check 4g status\n");
            stCheck4GTime = nowCheckTime;
            anj_aiot_cmd_4g_info_get();
            anj_aiot_cmd_location_poll();
            bFirstReport4GInfo = 1;
        }
    }

    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
    {
        static unsigned long long stCheckPresetTime = 0;
        if (nowCheckTime - stCheckPresetTime > 30 * 1000)
        {
            stCheckPresetTime = nowCheckTime;
            anj_aiot_cmd_ptz_preset_reponse();
        }
    }
    return 0;
}

int anj_aiot_cmd_init()
{
    int iRet = 0;
    frame_mgr_init(&s_stAiotCmdMgr, 200);

    if (anj_mw_file_exists(P2P_ID_SENDEVENT_FREQ))
    {
        char buffer[128] = {0};

        anj_mw_read_file_limit_len(P2P_ID_SENDEVENT_FREQ, buffer, sizeof(buffer));

        s_stSendEventFreq = atoi(buffer);
        if (s_stSendEventFreq <= 0)
        {
            s_stSendEventFreq = ALARM_PUSH_FREQ_DEFAULT;
        }
    }
    char freqBuffer[32] = {0};
    snprintf(freqBuffer, sizeof(freqBuffer), "%d", s_stSendEventFreq);
    anj_mw_write_file(P2P_ID_SENDEVENT_FREQ, 0, freqBuffer, strlen(freqBuffer));

    s_stAiotCmdThread.bAutoDestroy = 1;
    strncpy(s_stAiotCmdThread.iThreadName, "aiot_cmd", sizeof(s_stAiotCmdThread.iThreadName) - 1);
    s_stAiotCmdThread.iThreadjob.ctx = &s_stAiotCmdThread;
    s_stAiotCmdThread.iThreadjob.func = anj_aiot_cmd_thread;
    iRet = anj_thread_task_create(&s_stAiotCmdThread);
    return iRet;
}

void anj_aiot_cmd_uninit()
{
    anj_thread_task_destroy(&s_stAiotCmdThread, -1);
    frame_mgr_release(&s_stAiotCmdMgr);
}

void anj_aiot_cmd_push(char *pBuf, unsigned int nBufLen, unsigned int sid)
{
    FRAME_ENTRY frame = {0};
    frame.pFrame = anj_mw_malloc(nBufLen + 1);
    if (frame.pFrame)
    {
        memcpy(frame.pFrame, pBuf, nBufLen);
        frame.pFrame[nBufLen] = '\0';
        frame.nFrameLen = nBufLen;
        frame.session = sid;
        frame_mgr_push(&s_stAiotCmdMgr, &frame);
    }
}

void anj_aiot_cmd_sd_format_reponse()
{
    anj_aiot_cmd_update(AIOT_CMD_STORAGE_STATUS);
}

void anj_aiot_cmd_ptz_preset_reponse()
{
    int i = 0;
    anj_mutex_lock(&s_stAiotCmdMutex);
    for (i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
    {
        if (strcmp(s_cmd_info[i].szReportID, AIOT_CMD_PRESET_LIST) == 0)
        {
            anj_aiot_cmd_store_property(&s_cmd_info[i], anj_aiot_cmd_ptz_preset_get());
            break;
        }
    }
    anj_mutex_unlock(&s_stAiotCmdMutex);
}

void anj_aiot_cmd_ptz_advance_state_reponse(void *data)
{
    int i = 0;
    anj_mutex_lock(&s_stAiotCmdMutex);
    for (i = 0; i < (int)AIOT_CMD_INFO_CNT; i++)
    {
        if (strcmp(s_cmd_info[i].szReportID, AIOT_CMD_PTZ_ADVANCE_STATE) == 0)
        {
            anj_aiot_cmd_store_property(&s_cmd_info[i], anj_aiot_cmd_ptz_advance_state_get(data));
            break;
        }
    }
    anj_mutex_unlock(&s_stAiotCmdMutex);
}

char *anj_aiot_cmd_ptz_dir_get()
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    cJSON *pJsonRoot = cJSON_CreateObject();
    if (pJsonRoot)
    {
        cJSON *pJsonSub = cJSON_CreateObject();
        if (pJsonSub)
        {
            cJSON_AddNumberToObject(pJsonSub, "ptz_level_direction", pstIotPtzConfig->m_ptzDir.HDir);
            cJSON_AddNumberToObject(pJsonSub, "ptz_vert_direction", pstIotPtzConfig->m_ptzDir.VDir);
            cJSON_AddItemToObject(pJsonRoot, "ptz_direction", pJsonSub);
        }
    }

    char *szData = cJSON_PrintUnformatted(pJsonRoot);
    cJSON_Delete(pJsonRoot);
    return szData;
}

char *anj_aiot_cmd_audio_capture_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapture = &pstMediaConfig->audioConfig.audioCapture;
    cJSON *pJsonRoot = cJSON_CreateObject();
    if (pJsonRoot)
    {
        cJSON *pJsonSub = cJSON_CreateObject();
        if (pJsonSub)
        {
            cJSON_AddNumberToObject(pJsonSub, "volume_capture", pstAudioCapture->volume_capture);
            cJSON_AddNumberToObject(pJsonSub, "volume_play", pstAudioCapture->volume_play);
            cJSON_AddNumberToObject(pJsonSub, "capture_amplify", pstAudioCapture->amplify);
            cJSON_AddNumberToObject(pJsonSub, "aec_enable", pstAudioCapture->aec_enable);
            cJSON_AddNumberToObject(pJsonSub, "mute_ptz_turn", pstAudioCapture->mute_ptz_turn);

            cJSON_AddItemToObject(pJsonRoot, "AudioCapture", pJsonSub);
        }
    }

    char *szData = cJSON_PrintUnformatted(pJsonRoot);
    cJSON_Delete(pJsonRoot);
    return szData;
}

char *anj_aiot_cmd_ircut_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapture = &pstMediaConfig->videoConfig[0].videoCapture;
    return Report_Property_S32("IcrWorkMode", pstVideoCapture->ircut_mode);
}

char *anj_aiot_cmd_osd_get()
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoOverlay *pstVideoOverlay = &pstMediaConfig->videoConfig[0].overlay;
    cJSON *pJsonRoot = cJSON_CreateObject();
    if (NULL == pJsonRoot)
        return NULL;

    cJSON *pJsonSub = cJSON_CreateObject();
    if (pJsonSub)
    {
        cJSON_AddNumberToObject(pJsonSub, "enable", pstVideoOverlay->enable);
        cJSON_AddStringToObject(pJsonSub, "title", pstVideoOverlay->titleOverlay.title_utf8);

        cJSON_AddItemToObject(pJsonRoot, "osd", pJsonSub);
    }

    char *szData = cJSON_PrintUnformatted(pJsonRoot);
    cJSON_Delete(pJsonRoot);
    //	__INFO("%s", szData);
    return szData;
}