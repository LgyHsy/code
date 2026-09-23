#include <stdio.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_time.h"
#include "anj_mw_smart.h"
#include "anj_comm.h"
#include "anj_module.h"
#include "anj_sysctl.h"
#include "anj_config.h"
#include "anj_config_ptz.h"
#include "anj_ser.h"
#include "anj_osd.h"
#include "anj_video.h"
#include "anj_ispctl.h"
#include "anj_smart.h"
#include "anj_zoom.h"
#include "anj_audio.h"

#include "eventhub.h"
#include "function_list.h"

#include "anj_ptz.h"
#include "anj_ptz_provider.h"

#define PTZ_NAME_MAX 64
#define PTZ_H_RESET_STEP(STEP) ((STEP) / 2)                                           /*步进电机水平最大步数*/
#define PTZ_V_RESET_STEP(STEP) (((STEP) * 7) / 10)                                    /*步进电机垂直最大步数*/
#define PTZ_THREAD_TIME (10 * 1000)                                                   /*线程循环间隔时间10ms*/
#define ZOOM_OSD_HIDE_DELAY_MS (1.5 * 1000)                                           /*停止变倍后延迟多少毫秒再隐藏OSD*/
#define PTZ_NOT_TRACK_CUMULATIVE_TIME (60 * 60 * 4 * (1000 * 1000 / PTZ_THREAD_TIME)) /*不追踪累计时间4h*/
#define PTZ_SAVE_STEP_TIEM (60 * 1000 * 1000)                                         /*1分钟保存一次位置步数*/
#define PTZ_OVER_TIME (60 * 1000 * 1000)                                              /*1分钟云台阻塞超时*/
#define PTZ_START_TRACK_ONE_STEP (1)                                                  /*云台追踪步数*/
#define PTZ_TRACK_DISPLAY_TIME (5000)                                                 /*5s没追踪坐标，就停止追踪*/
#define PTZ_TRACK_END_TIME (3000)                                                     /*结束追踪 3s 后才能再次开启追踪*/
#define PTZ_TRACK_START_TIMES (3)                                                     /*连续3次有坐标才开始追踪*/
#define PTZ_TRACK_MIN_H_PIXEL(w) ((w) / 10)                                           /*水平云台开始追踪的最小像素(相对显示画面)*/
#define PTZ_TRACK_MIN_V_PIXEL(h) ((h) / 7)                                            /*垂直云台开始追踪的最小像素(相对显示画面)*/
#define PTZ_TRACK_DUAL_MAX_H_DEBUG_FILE "/tmp/dual_track_max_h"
#define PTZ_TRACK_DUAL_MAX_V_DEBUG_FILE "/tmp/dual_track_max_v"
#define PTZ_TRACK_DUAL_MAX_H_DEFAULT (40)   /* 双开模式水平单次追踪最大步数 */
#define PTZ_TRACK_DUAL_MAX_V_DEFAULT (15)   /* 双开模式垂直单次追踪最大步数 */
#define PTZ_TRACK_ZOOM_SATURATE_EPS (0.02f) /* crop 贴边容差 */

static int s_iTrackDualMaxHStep = PTZ_TRACK_DUAL_MAX_H_DEFAULT;
static int s_iTrackDualMaxVStep = PTZ_TRACK_DUAL_MAX_V_DEFAULT;

static void anj_ptz_track_tune_load(void)
{
    char *str = NULL;

    s_iTrackDualMaxHStep = PTZ_TRACK_DUAL_MAX_H_DEFAULT;
    s_iTrackDualMaxVStep = PTZ_TRACK_DUAL_MAX_V_DEFAULT;

    str = anj_mw_read_file_buffer(PTZ_TRACK_DUAL_MAX_H_DEBUG_FILE);
    if (str)
    {
        if (atoi(str) > 0)
        {
            s_iTrackDualMaxHStep = atoi(str);
        }
        anj_mw_free(str);
        str = NULL;
    }

    str = anj_mw_read_file_buffer(PTZ_TRACK_DUAL_MAX_V_DEBUG_FILE);
    if (str)
    {
        if (atoi(str) > 0)
        {
            s_iTrackDualMaxVStep = atoi(str);
        }
        anj_mw_free(str);
    }

    anj_smart_track_tune_load();
    __INFO("ptz track dual max step h:%d v:%d (file:%s %s)\n", s_iTrackDualMaxHStep, s_iTrackDualMaxVStep,
           PTZ_TRACK_DUAL_MAX_H_DEBUG_FILE, PTZ_TRACK_DUAL_MAX_V_DEBUG_FILE);
}

#define PTZ_DEFAULT_V_SPEED (PTZ_SPEED_3)
#define PTZ_DEFAULT_H_SPEED (PTZ_SPEED_3)

#define PTZ_CMD_LENS_COVER_ON "LensCoverOn"
#define PTZ_CMD_LENS_COVER_OFF "LensCoverOff"

#define PTZ_LINE_SCAN_DWELL_TIME_US (5 * 1000 * 1000)
#define PTZ_LINE_SCAN_IDLE_START_MS (30 * 1000)
#define PTZ_CRUISE_DWELL_TIME_US (25 * 1000 * 1000)
#define PTZ_CRUISE_IDLE_START_MS (30 * 1000)
#define PTZ_CRUISE_MAX_DURATION_MS (15ULL * 24 * 60 * 60 * 1000)
typedef enum
{
    PTZ_SPECIAL_PRESET_LINE_SCAN_SET_LEFT = 47,
    PTZ_SPECIAL_PRESET_LINE_SCAN_START = 48,
    PTZ_SPECIAL_PRESET_SET_GUARD = 49,
    PTZ_SPECIAL_PRESET_ZOOM_OSD_SWITCH = 51,
    PTZ_SPECIAL_PRESET_CRUISE_START = 54,
    PTZ_SPECIAL_PRESET_LIGHT_DOUBLE = 66,
    PTZ_SPECIAL_PRESET_LIGHT_WHITE = 67,
    PTZ_SPECIAL_PRESET_LIGHT_IR = 68,
    PTZ_SPECIAL_PRESET_ZOOM_TRACK_TOGGLE = 69,
    PTZ_SPECIAL_PRESET_AUDIO_ALARM_SWITCH = 71,
    PTZ_SPECIAL_PRESET_ZOOM_TRACK_SWITCH = 72,
    PTZ_SPECIAL_PRESET_PTZ_RESET_A = 82,
    PTZ_SPECIAL_PRESET_PTZ_RESET_B = 84,
    PTZ_SPECIAL_PRESET_RESTORE_AND_REBOOT = 92,
    PTZ_SPECIAL_PRESET_STOP_SCAN_CRUISE = 171,
    PTZ_SPECIAL_PRESET_PTZ_TEST = 197,
} PTZ_SPECIAL_PRESET_E;

static pthread_mutex_t s_stPtzMutex = PTHREAD_MUTEX_INITIALIZER;
static anj_thread_s s_stPtzThread = {0};
static PTZ_PARAM s_stPtzParam = {0};
static int s_stPtzInit = 0;

/*
 * 双开：变倍 crop 已贴某侧，且目标中心仍偏该侧 → 该轴转云台。
 */
static void anj_ptz_track_need_turn_by_zoom_saturate(int Hdistance, int Hdirection, int Vdistance, int Vdirection,
                                                     int *pTurnH, int *pTurnV)
{
    DOUBLE_AREA_ENTRY curZoom = {0};
    double cropSpan = 1.0;
    int cropAtLeft = 0;
    int cropAtRight = 0;
    int cropAtTop = 0;
    int cropAtBottom = 0;
    int needH = (Hdistance >= PTZ_TRACK_MIN_H_PIXEL(SMART_PD_WIDTH));
    int needV = (Vdistance >= PTZ_TRACK_MIN_V_PIXEL(SMART_PD_HEIGHT));

    if (pTurnH)
    {
        *pTurnH = 0;
    }
    if (pTurnV)
    {
        *pTurnV = 0;
    }

    if (anj_zoom_run_get(0, &curZoom, NULL) != 0 || curZoom.width <= 0.0)
    {
        return;
    }

    cropSpan = 1.0 / curZoom.width;
    /* 接近 1 倍：crop 铺满全图，四边都“贴边”，有偏差即可转 */
    if (cropSpan >= (1.0 - PTZ_TRACK_ZOOM_SATURATE_EPS))
    {
        if (pTurnH)
        {
            *pTurnH = needH;
        }
        if (pTurnV)
        {
            *pTurnV = needV;
        }
        return;
    }

    cropAtLeft = (curZoom.xPos <= PTZ_TRACK_ZOOM_SATURATE_EPS);
    cropAtRight = (curZoom.xPos >= (1.0 - cropSpan - PTZ_TRACK_ZOOM_SATURATE_EPS));
    cropAtTop = (curZoom.yPos <= PTZ_TRACK_ZOOM_SATURATE_EPS);
    cropAtBottom = (curZoom.yPos >= (1.0 - cropSpan - PTZ_TRACK_ZOOM_SATURATE_EPS));

    if (pTurnH && needH)
    {
        /* Hdirection: 0=目标在左(需左转), 1=目标在右(需右转) */
        if ((Hdirection == 0 && cropAtLeft) || (Hdirection == 1 && cropAtRight))
        {
            *pTurnH = 1;
        }
    }
    if (pTurnV && needV)
    {
        /* Vdirection: 0=目标在上(需上转), 1=目标在下(需下转) */
        if ((Vdirection == 0 && cropAtTop) || (Vdirection == 1 && cropAtBottom))
        {
            *pTurnV = 1;
        }
    }
}

static void anj_ptz_osd_set(OverlayTextEnum osdType, int show)
{
    // 云台复位过程中不可被其他操作刷新osd
    if (s_stPtzParam.ptzWork == PTZ_RESET && osdType != OVERLAY_PTZ_RESET)
    {
        return;
    }

    if (s_stPtzParam.bLensCoverEnable)
    {
        return;
    }

    osd_custom_content_s osdPtzCustom = {0};
    osdPtzCustom.custom_show = show;
    osdPtzCustom.overlayText = osdType;
    osdPtzCustom.custom_x = 50;
    osdPtzCustom.custom_y = 99;
    osdPtzCustom.custom_location = POSITION_TYPE_BY_SCALE;
    anj_osd_debug_set(&osdPtzCustom);
    s_stPtzParam.ptzOsdTime = show ? anj_mw_get_cputime_ms(NULL) : 0;
}

static int anj_ptz_3d_step_get(int cfgStep, int defaultStep)
{
    return (cfgStep > 0) ? cfgStep : defaultStep;
}

static int anj_ptz_3d_point_conver_step(int posX, int posY, PtzStep *pTargetStep)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    int centerX = SMART_PD_WIDTH / 2;
    int centerY = SMART_PD_HEIGHT / 2;
    int leftStep = 0;
    int rightStep = 0;
    int upStep = 0;
    int downStep = 0;

    if (pTargetStep == NULL)
    {
        return -1;
    }

    *pTargetStep = s_stPtzParam.m_PtzStep;

    leftStep = anj_ptz_3d_step_get(pstIotPtzConfig->m_3dOrientStep.left_down_point.HStep,
                                   s_stPtzParam.MaxStep.HStep / 2);
    rightStep = anj_ptz_3d_step_get(pstIotPtzConfig->m_3dOrientStep.right_up_point.HStep,
                                    s_stPtzParam.MaxStep.HStep / 2);
    downStep = anj_ptz_3d_step_get(pstIotPtzConfig->m_3dOrientStep.left_down_point.VStep,
                                   s_stPtzParam.MaxStep.VStep / 2);
    upStep = anj_ptz_3d_step_get(pstIotPtzConfig->m_3dOrientStep.right_up_point.VStep,
                                 s_stPtzParam.MaxStep.VStep / 2);

    if (posX < centerX)
    {
        pTargetStep->HStep += leftStep * (centerX - posX) / centerX;
    }
    else if (posX > centerX)
    {
        pTargetStep->HStep -= rightStep * (posX - centerX) / centerX;
    }

    if (posY < centerY)
    {
        pTargetStep->VStep += upStep * (centerY - posY) / centerY;
    }
    else if (posY > centerY)
    {
        pTargetStep->VStep -= downStep * (posY - centerY) / centerY;
    }

    __INFO("3D position target point:(%d,%d) target step:(%d,%d) current:(%d,%d)\n",
           posX, posY,
           pTargetStep->HStep, pTargetStep->VStep,
           s_stPtzParam.m_PtzStep.HStep, s_stPtzParam.m_PtzStep.VStep);
    return 0;
}

static int anj_ptz_line_scan_wait_interrupt(int waitUs)
{
    int waitCnt = waitUs / (50 * 1000);
    if (waitCnt <= 0)
    {
        waitCnt = 1;
    }

    while (waitCnt-- > 0)
    {
        if (!s_stPtzInit || s_stPtzParam.bptzInterrupt ||
            s_stPtzParam.ptzWork != PTZ_LINE_SCAN)
        {
            return -1;
        }
        usleep(50 * 1000);
    }
    return 0;
}

static int anj_ptz_wait_interrupt(int waitUs, int ptzWork)
{
    int waitCnt = waitUs / (50 * 1000);
    if (waitCnt <= 0)
    {
        waitCnt = 1;
    }

    while (waitCnt-- > 0)
    {
        if (!s_stPtzInit || s_stPtzParam.bptzInterrupt ||
            s_stPtzParam.ptzWork != ptzWork)
        {
            return -1;
        }
        usleep(50 * 1000);
    }
    return 0;
}

static int anj_ptz_cruise_preset_list_get(int presetIds[], int maxCount)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    int count = 0;
    int i = 0;

    if (presetIds == NULL || maxCount <= 0)
    {
        return 0;
    }

    for (i = 0; i < 16 && count < maxCount; i++)
    {
        if (pstIotPtzConfig->m_ptzPreset[i].preset_id > 0)
        {
            presetIds[count++] = pstIotPtzConfig->m_ptzPreset[i].preset_id;
        }
    }

    return count;
}

static int anj_ptz_cruise_next_preset_get(int *pPresetId)
{
    int presetIds[16] = {0};
    int presetCount = 0;
    int i = 0;

    if (pPresetId == NULL)
    {
        return -1;
    }

    presetCount = anj_ptz_cruise_preset_list_get(presetIds, 16);
    if (presetCount <= 0)
    {
        return -1;
    }

    if (s_stPtzParam.ptzCruise.nextPresetId <= 0)
    {
        *pPresetId = presetIds[0];
        return 0;
    }

    for (i = 0; i < presetCount; i++)
    {
        if (presetIds[i] >= s_stPtzParam.ptzCruise.nextPresetId)
        {
            *pPresetId = presetIds[i];
            return 0;
        }
    }

    *pPresetId = presetIds[0];
    return 0;
}

static int anj_ptz_cruise_following_preset_get(int currentPresetId)
{
    int presetIds[16] = {0};
    int presetCount = 0;
    int i = 0;

    presetCount = anj_ptz_cruise_preset_list_get(presetIds, 16);
    if (presetCount <= 0)
    {
        return 0;
    }

    for (i = 0; i < presetCount; i++)
    {
        if (presetIds[i] == currentPresetId)
        {
            return presetIds[(i + 1) % presetCount];
        }
    }

    return presetIds[0];
}

static void anj_ptz_line_scan_toggle(int enable, int showOsd)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    IotPtzConfig stPtzConfig = *pstIotPtzConfig;

    stPtzConfig.m_ptzLineScan.enable = enable;
    s_stPtzParam.ptzLineScan.scanDir = 0;
    if (enable)
    {
        stPtzConfig.m_ptzCruiseEnable = 0;
        s_stPtzParam.ptzCruise.startTime = 0;
        s_stPtzParam.ptzCruise.nextPresetId = 0;
    }
    anj_ptz_config_save(&stPtzConfig);
    if (showOsd)
    {
        anj_ptz_osd_set(enable ? OVERLAY_PTZ_LINE_SCAN_ON : OVERLAY_PTZ_LINE_SCAN_OFF, 1);
    }
}

static void anj_ptz_cruise_toggle(int enable, int showOsd)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    IotPtzConfig stPtzConfig = *pstIotPtzConfig;

    stPtzConfig.m_ptzCruiseEnable = enable;
    if (enable)
    {
        stPtzConfig.m_ptzLineScan.enable = 0;
        s_stPtzParam.ptzLineScan.scanDir = 0;
    }
    else
    {
        s_stPtzParam.ptzCruise.startTime = 0;
        s_stPtzParam.ptzCruise.nextPresetId = 0;
    }
    anj_ptz_config_save(&stPtzConfig);
    if (showOsd)
    {
        anj_ptz_osd_set(enable ? OVERLAY_PTZ_CRUISE_ON : OVERLAY_PTZ_CRUISE_OFF, 1);
    }
}

static void anj_ptz_line_scan_boundary_save(int leftBoundary)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    IotPtzConfig stPtzConfig = *pstIotPtzConfig;
    DOUBLE_AREA_ENTRY stCurZoom = {0};
    double curMultiple = pstIotPtzConfig->m_zoom.cur_multiple;

    if (anj_zoom_run_get(0, &stCurZoom, NULL) == 0)
    {
        curMultiple = stCurZoom.width;
    }

    if (leftBoundary)
    {
        stPtzConfig.m_ptzLineScan.left_margin = s_stPtzParam.m_PtzStep;
        stPtzConfig.m_ptzLineScan.left_multiple = curMultiple;
        anj_ptz_osd_set(OVERLAY_PTZ_LINE_SCAN_SET_LEFT, 1);
    }
    else
    {
        stPtzConfig.m_ptzLineScan.right_margin = s_stPtzParam.m_PtzStep;
        stPtzConfig.m_ptzLineScan.right_multiple = curMultiple;
        anj_ptz_osd_set(OVERLAY_PTZ_LINE_SCAN_SET_RIGHT, 1);
    }
    anj_ptz_config_save(&stPtzConfig);
}

static void anj_ptz_work_set(int work)
{
    anj_mutex_lock(&s_stPtzMutex);
    // 镜头遮挡 不支持任何云台操作
    if (s_stPtzParam.bLensCoverEnable && work != PTZ_LENS_COVER_OFF)
    {
        anj_mutex_unlock(&s_stPtzMutex);
        return;
    }
    // 云台复位不可被其他操作打断
    if (s_stPtzParam.ptzWork != PTZ_RESET)
    {
        // 云台有其他任务 打断并更新任务
        if (s_stPtzParam.ptzWork != PTZ_NONE)
        {
            s_stPtzParam.bptzInterrupt = 1;
        }
        s_stPtzParam.ptzWork = work;
    }
    anj_mutex_unlock(&s_stPtzMutex);
}

static void anj_ptz_led_mode_set(LedMode ledMode, OverlayTextEnum osdType)
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        VideoCaptureCfg stVideoCapCfg = pstMediaConfig->videoConfig[i].videoCapture;
        stVideoCapCfg.led_mode = ledMode;
        anj_config_video_capture_set(&stVideoCapCfg, i);
    }
    anj_ptz_osd_set(osdType, 1);
}

static int anj_ptz_special_preset_handle(int presetId, int *pPtzWork)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    IotPtzConfig stPtzConfig = *pstIotPtzConfig;

    switch (presetId)
    {
    case PTZ_SPECIAL_PRESET_LIGHT_IR:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ) &&
            ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE_RED)
        {
            anj_ptz_led_mode_set(LED_PURE_INFRAED, OVERLAY_PTZ_SWITCH_TO_IR_MODE);
            return 1;
        }
        break;
    case PTZ_SPECIAL_PRESET_LIGHT_WHITE:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ) &&
            ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE_RED)
        {
            anj_ptz_led_mode_set(LED_PURE_WHITE, OVERLAY_PTZ_SWITCH_TO_WHITE_MODE);
            return 1;
        }
        break;
    case PTZ_SPECIAL_PRESET_LIGHT_DOUBLE:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ) &&
            ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE_RED)
        {
            anj_ptz_led_mode_set(LED_INFRAED_THEN_WHITE, OVERLAY_PTZ_SWITCH_TO_DOUBLE_MODE);
            return 1;
        }
        break;
    case PTZ_SPECIAL_PRESET_AUDIO_ALARM_SWITCH:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            // TODO: 音频告警开关（预置点71）后续补充具体实现。
        }
        return 1;
    case PTZ_SPECIAL_PRESET_ZOOM_TRACK_TOGGLE:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            if (anj_sysctl_capability_check(FUNCTION_ZOOM_TRACK))
            {
                anj_sysctl_capability_remove(FUNCTION_ZOOM_TRACK);
            }
            else
            {
                anj_sysctl_capability_add(FUNCTION_ZOOM_TRACK);
            }
        }
        return 1;
    case PTZ_SPECIAL_PRESET_LINE_SCAN_START:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            anj_ptz_line_scan_toggle(1, 1);
        }
        return 1;
    case PTZ_SPECIAL_PRESET_CRUISE_START:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            if (stPtzConfig.m_ptzLineScan.enable)
            {
                *pPtzWork = PTZ_STOP;
            }
            anj_ptz_cruise_toggle(1, 1);
        }
        return 1;
    case PTZ_SPECIAL_PRESET_STOP_SCAN_CRUISE:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            if (stPtzConfig.m_ptzCruiseEnable || stPtzConfig.m_ptzLineScan.enable)
            {
                *pPtzWork = PTZ_STOP;
            }
            if (stPtzConfig.m_ptzCruiseEnable)
            {
                anj_ptz_cruise_toggle(0, 1);
            }
            if (stPtzConfig.m_ptzLineScan.enable)
            {
                anj_ptz_line_scan_toggle(0, 1);
            }
        }
        return 1;
    case PTZ_SPECIAL_PRESET_RESTORE_AND_REBOOT:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            remove(CONFIG_PTZ_PATH);
            *pPtzWork = PTZ_RESET;
        }
        return 1;
    case PTZ_SPECIAL_PRESET_PTZ_TEST:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            *pPtzWork = PTZ_RESET;
        }
        return 1;
    case PTZ_SPECIAL_PRESET_SET_GUARD:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            int i = 0;
            for (i = 0; i < MAX_PTZ_PRESET; i++)
            {
                if (1 == stPtzConfig.m_ptzPreset[i].preset_id)
                {
                    stPtzConfig.watch_guard = 1;
                    stPtzConfig.watch_guard_time = 30;
                    anj_ptz_config_save(&stPtzConfig);
                    anj_ser_reponse(SER_RESPONSE_PTZ_PRESET, NULL);
                    break;
                }
            }
            if (i >= MAX_PTZ_PRESET)
            {
                anj_ptz_osd_set(OVERLAY_PTZ_WATCH_GUARD_SET_FAIL, 1);
            }
            return 1;
        }
    case PTZ_SPECIAL_PRESET_ZOOM_OSD_SWITCH:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            anj_zoom_osd_type_set();
        }
        return 1;
    case PTZ_SPECIAL_PRESET_ZOOM_TRACK_SWITCH:
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ))
        {
            if (anj_sysctl_capability_check(FUNCTION_ZOOM_TRACK))
            {
                anj_sysctl_capability_remove(FUNCTION_ZOOM_TRACK);
                anj_ptz_osd_set(OVERLAY_PTZ_ZOOM_TRACK_OFF, 1);
            }
            else
            {
                anj_sysctl_capability_add(FUNCTION_ZOOM_TRACK);
                anj_ptz_osd_set(OVERLAY_PTZ_ZOOM_TRACK_ON, 1);
            }
        }
        return 1;
    case PTZ_SPECIAL_PRESET_PTZ_RESET_A:
    case PTZ_SPECIAL_PRESET_PTZ_RESET_B:
        *pPtzWork = PTZ_RESET;
        return 1;
    default:
        break;
    }

    return 0;
}

static int anj_ptz_step_range(int step, int Mode)
{
    int iActualStep = 0;
    if (Mode == PTZ_UP)
    {
        iActualStep = (s_stPtzParam.m_PtzStep.VStep + step) < s_stPtzParam.MaxStep.VStep ? step : (s_stPtzParam.MaxStep.VStep - s_stPtzParam.m_PtzStep.VStep);
    }
    else if (Mode == PTZ_DOWN)
    {
        iActualStep = (s_stPtzParam.m_PtzStep.VStep - step) > 0 ? step : s_stPtzParam.m_PtzStep.VStep;
    }
    else if (Mode == PTZ_LEFT)
    {
        iActualStep = (s_stPtzParam.m_PtzStep.HStep + step) < s_stPtzParam.MaxStep.HStep ? step : (s_stPtzParam.MaxStep.HStep - s_stPtzParam.m_PtzStep.HStep);
    }
    else if (Mode == PTZ_RIGHT)
    {
        iActualStep = (s_stPtzParam.m_PtzStep.HStep - step) > 0 ? step : s_stPtzParam.m_PtzStep.HStep;
    }
    else
    {
        __INFO("invalid Mode:%d\n", Mode);
    }
    return iActualStep;
}

static void anj_ptz_step_update(int ptzLastWork, int iPtzRemainStep)
{
    if (ptzLastWork == PTZ_UP)
    {
        s_stPtzParam.m_PtzStep.VStep += iPtzRemainStep;
    }
    else if (ptzLastWork == PTZ_DOWN)
    {
        s_stPtzParam.m_PtzStep.VStep -= iPtzRemainStep;
    }
    else if (ptzLastWork == PTZ_LEFT)
    {
        s_stPtzParam.m_PtzStep.HStep += iPtzRemainStep;
    }
    else if (ptzLastWork == PTZ_RIGHT)
    {
        s_stPtzParam.m_PtzStep.HStep -= iPtzRemainStep;
    }
    __INFO("last ctl:%d iPtzRemainStep:%d\n", ptzLastWork, iPtzRemainStep);
    __INFO("Hstep:%d, Vstep:%d\n", s_stPtzParam.m_PtzStep.HStep, s_stPtzParam.m_PtzStep.VStep);
}

static int anj_ptz_operate(int mode, int arg, int speed)
{
    return anj_ptz_provider_operate(mode, arg, speed);
}

static int anj_ptz_wait_stop()
{
    int iPtzRemainStep = 0;
    int bOverTime = 0;
    while (1)
    {
        bOverTime++;
        iPtzRemainStep = anj_ptz_operate(PTZ_CTL_MOTOR_STOP, 0, 0);
        if (iPtzRemainStep == -1)
        {
            if (bOverTime >= PTZ_STOP_OVER_TIME)
            {
                __ERR("######PTZ Over Time!! %d\n", bOverTime);
                break;
            }
            usleep(PTZ_WAIT_TIME);
        }
        else
        {
            __INFO("######PTZ Reamin Step!! %d\n", iPtzRemainStep);
            break;
        }
    }
    return iPtzRemainStep;
}

static int anj_ptz_wait_rundone()
{
    int iPtzRemainStep = 0;
    int iPtzLastRemainStep = -1;
    int iPtzSameRemainCount = 0;
    int bOverTime = 0;
    while (!s_stPtzParam.bptzInterrupt && s_stPtzInit)
    {
        bOverTime++;
        if (s_stPtzParam.bptzInterrupt)
        {
            iPtzRemainStep = anj_ptz_wait_stop();
        }
        else
        {
            iPtzRemainStep = anj_ptz_operate(PTZ_CTL_REMAIN_STEP, 0, 0);
            if (iPtzRemainStep == 0)
            {
                break;
            }
            if (iPtzLastRemainStep == iPtzRemainStep)
            {
                iPtzSameRemainCount++;
                if (iPtzSameRemainCount >= 3)
                {
                    __ERR("remain step stalled:%d count:%d\n", iPtzRemainStep, iPtzSameRemainCount);
                    break;
                }
            }
            else
            {
                iPtzSameRemainCount = 0;
            }

            if (iPtzRemainStep < 0)
            {
                __ERR("######PTZ Get step failed:%d\n", iPtzRemainStep);
                break;
            }
            else
            {
                if (bOverTime >= PTZ_RUN_OVER_TIME)
                {
                    __ERR("######PTZ Over Time!! %d\n", bOverTime);
                    break;
                }
            }
            usleep(PTZ_WAIT_TIME);
            iPtzLastRemainStep = iPtzRemainStep;
        }
    }

    return iPtzRemainStep;
}

static int anj_ptz_ctl_reset(PtzStep *resetStep)
{
    int iRet = 0;
    anj_ptz_osd_set(OVERLAY_PTZ_RESET, 1);
    anj_ptz_wait_stop();
    if (!s_stPtzInit)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
        return -1;
    }
    __INFO("LEFT!\n");
    anj_ptz_operate(PTZ_CTL_MOTOR_LEFT, s_stPtzParam.MaxStep.HStep, PTZ_SPEED_5);
    iRet = anj_ptz_wait_rundone();
    if (0 != iRet)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
        return -1;
    }

    __INFO("RIGHT!\n");
    anj_ptz_operate(PTZ_CTL_MOTOR_RIGHT, s_stPtzParam.MaxStep.HStep, PTZ_SPEED_5);
    iRet = anj_ptz_wait_rundone();
    if (0 != iRet)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
        return -1;
    }

    __INFO("LEFT!\n");
    anj_ptz_operate(PTZ_CTL_MOTOR_LEFT, resetStep->HStep, PTZ_SPEED_5);
    iRet = anj_ptz_wait_rundone();
    if (0 != iRet)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
        return -1;
    }
    s_stPtzParam.m_PtzStep.HStep = resetStep->HStep;

    __INFO("UP!\n");
    anj_ptz_operate(PTZ_CTL_MOTOR_UP, s_stPtzParam.MaxStep.VStep, PTZ_SPEED_5);
    iRet = anj_ptz_wait_rundone();
    if (0 != iRet)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
        return -1;
    }

    __INFO("DOWN!\n");
    anj_ptz_operate(PTZ_CTL_MOTOR_DOWN, s_stPtzParam.MaxStep.VStep, PTZ_SPEED_5);
    iRet = anj_ptz_wait_rundone();
    if (0 != iRet)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
        return -1;
    }

    __INFO("UP!\n");
    anj_ptz_operate(PTZ_CTL_MOTOR_UP, resetStep->VStep, PTZ_SPEED_5);
    iRet = anj_ptz_wait_rundone();
    if (0 != iRet)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
        return -1;
    }
    s_stPtzParam.m_PtzStep.VStep = resetStep->VStep;
    anj_ptz_osd_set(OVERLAY_PTZ_RESET, 0);
    __INFO("PTZ RESET FINISH!\n");
    return 0;
}

static int anj_ptz_ctl_preset(PtzStep *presetStep, PtzSpeed *pSpeed, int *pPtzLastWork, int bTrack)
{
    int iRet = 0;
    int iPtzCtlStep = 0;
    anj_ptz_wait_stop();
    if (presetStep->HStep > s_stPtzParam.m_PtzStep.HStep)
    {
        iPtzCtlStep = (presetStep->HStep - s_stPtzParam.m_PtzStep.HStep);
        *pPtzLastWork = PTZ_LEFT;
        __INFO("LEFT!\n");
        iRet = anj_ptz_operate(PTZ_CTL_MOTOR_LEFT, iPtzCtlStep, pSpeed->HSpeed);
        if (iRet == 0)
        {
            iRet = anj_ptz_wait_rundone();
            if (iRet != 0)
            {
                return iRet;
            }
            s_stPtzParam.m_PtzStep.HStep = presetStep->HStep;
        }
    }
    else if (presetStep->HStep < s_stPtzParam.m_PtzStep.HStep)
    {
        iPtzCtlStep = (s_stPtzParam.m_PtzStep.HStep - presetStep->HStep);
        *pPtzLastWork = PTZ_RIGHT;
        __INFO("RIGHT!\n");
        iRet = anj_ptz_operate(PTZ_CTL_MOTOR_RIGHT, iPtzCtlStep, pSpeed->HSpeed);
        if (iRet == 0)
        {
            iRet = anj_ptz_wait_rundone();
            if (iRet != 0)
            {
                return iRet;
            }
            s_stPtzParam.m_PtzStep.HStep = presetStep->HStep;
        }
    }

    iRet = 0;
    if (presetStep->VStep > s_stPtzParam.m_PtzStep.VStep)
    {
        iPtzCtlStep = (presetStep->VStep - s_stPtzParam.m_PtzStep.VStep);
        *pPtzLastWork = PTZ_UP;
        __INFO("UP!\n");
        iRet = anj_ptz_operate(PTZ_CTL_MOTOR_UP, iPtzCtlStep, pSpeed->VSpeed);
        if (iRet == 0)
        {
            iRet = anj_ptz_wait_rundone();
            if (iRet != 0)
            {
                return iRet;
            }
            s_stPtzParam.m_PtzStep.VStep = presetStep->VStep;
        }
    }
    else if (presetStep->VStep < s_stPtzParam.m_PtzStep.VStep)
    {
        iPtzCtlStep = (s_stPtzParam.m_PtzStep.VStep - presetStep->VStep);
        *pPtzLastWork = PTZ_DOWN;
        __INFO("DOWN!\n");
        iRet = anj_ptz_operate(PTZ_CTL_MOTOR_DOWN, iPtzCtlStep, pSpeed->VSpeed);
        if (iRet == 0)
        {
            iRet = anj_ptz_wait_rundone();
            if (iRet != 0)
            {
                return iRet;
            }
            s_stPtzParam.m_PtzStep.VStep = presetStep->VStep;
        }
    }
    if (bTrack == 0)
    {
        anj_ptz_osd_set(OVERLAY_PTZ_GOTO_PRESET, 0);
    }

    __INFO("preset ok\n");
    return 0;
}

/******************************************************************************
@函数名称 : int anj_ptz_preset_list(char *list_str, int list_str_size, PtzConfig *pstPtzConfig)
@功能描述 : 获取预置点列表
@输入参数 : pstPtzConfig：云台配置参数
@输出参数 : list_str：预置点列表 list_str_size：预置点列表大小
@返回值： 成功返回0，失败返回-1
******************************************************************************/
static int anj_ptz_preset_list(char *list_str, int list_str_size, IotPtzConfig *pstPtzConfig)
{
    char str_buffer[1024] = {0};
    int str_size = sizeof(str_buffer) / sizeof(str_buffer[0]);
    int i = 0;
    int len = 0;
    int count = 0;

    for (i = 0; i < MAX_PTZ_PRESET; i++)
    {
        if (pstPtzConfig->m_ptzPreset[i].preset_id > 0)
        {
            len += snprintf(str_buffer + len, str_size - 1 - len, "%d^", pstPtzConfig->m_ptzPreset[i].preset_id);
            count++;
        }
    }

    len = snprintf(list_str, list_str_size, "%d^%s", count, str_buffer);

    return len;
}

static int anj_ptz_line_scan_start(int *pPtzLastWork)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    PtzLineScan *pstLineScan = &pstIotPtzConfig->m_ptzLineScan;
    PtzSpeed stSpeed = {0};
    int iRet = 0;
    int scanDir = 0;

    if (pPtzLastWork == NULL)
    {
        return -1;
    }
    if (!pstLineScan->enable)
    {
        __ERR("PTZ LINE SCAN disabled\n");
        return -1;
    }
    if (0 == memcmp(&pstLineScan->left_margin, &pstLineScan->right_margin, sizeof(PtzStep)))
    {
        __ERR("PTZ LINE SCAN margin invalid\n");
        return -1;
    }

    stSpeed.HSpeed = PTZ_DEFAULT_H_SPEED;
    stSpeed.VSpeed = PTZ_DEFAULT_V_SPEED;
    scanDir = s_stPtzParam.ptzLineScan.scanDir;

    __INFO("line scan start dir:%d\n", scanDir);
    while (s_stPtzParam.ptzWork == PTZ_LINE_SCAN && pstLineScan->enable)
    {
        PtzStep *pstTargetStep = scanDir ? &pstLineScan->right_margin : &pstLineScan->left_margin;
        double targetMultiple = scanDir ? pstLineScan->right_multiple : pstLineScan->left_multiple;
        anj_zoom_ctrl(0, targetMultiple);
        iRet = anj_ptz_ctl_preset(pstTargetStep, &stSpeed, pPtzLastWork, 0);
        if (iRet != 0)
        {
            __ERR("PTZ LINE SCAN ctl failed:%d\n", iRet);
            return iRet;
        }

        s_stPtzParam.ptzLineScan.scanDir = !scanDir;
        if (anj_ptz_line_scan_wait_interrupt(PTZ_LINE_SCAN_DWELL_TIME_US) != 0)
        {
            break;
        }
        scanDir = s_stPtzParam.ptzLineScan.scanDir;
    }

    __INFO("line scan end\n");
    *pPtzLastWork = PTZ_STOP;
    return 0;
}

static int anj_ptz_cruise_start(int *pPtzLastWork)
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    PtzSpeed stSpeed = {0};
    int presetId = 0;
    int iRet = 0;
    PtzStep *pstPresetStep = NULL;
    unsigned long long now = anj_mw_get_cputime_ms(NULL);

    if (pPtzLastWork == NULL)
    {
        return -1;
    }
    if (!pstIotPtzConfig->m_ptzCruiseEnable)
    {
        *pPtzLastWork = PTZ_STOP;
        return -1;
    }

    if (s_stPtzParam.ptzCruise.startTime == 0)
    {
        s_stPtzParam.ptzCruise.startTime = now;
    }
    else if ((now - s_stPtzParam.ptzCruise.startTime) >= PTZ_CRUISE_MAX_DURATION_MS)
    {
        anj_ptz_cruise_toggle(0, 1);
        __INFO("PTZ CRUISE duration reached limit\n");
        *pPtzLastWork = PTZ_STOP;
        return -1;
    }

    if (anj_ptz_cruise_next_preset_get(&presetId) != 0 || presetId <= 0 || presetId > 16)
    {
        __INFO("PTZ CRUISE no preset available\n");
        *pPtzLastWork = PTZ_STOP;
        return 0;
    }

    pstPresetStep = &pstIotPtzConfig->m_ptzPreset[presetId - 1].preset_step;
    stSpeed.HSpeed = PTZ_SPEED_5;
    stSpeed.VSpeed = PTZ_SPEED_5;

    anj_zoom_ctrl(0, pstIotPtzConfig->m_ptzPreset[presetId - 1].zoom_multiple);
    if (memcmp(&s_stPtzParam.m_PtzStep, pstPresetStep, sizeof(PtzStep)) != 0)
    {
        iRet = anj_ptz_ctl_preset(pstPresetStep, &stSpeed, pPtzLastWork, 0);
        if (iRet != 0)
        {
            __ERR("PTZ CRUISE ctl failed:%d preset:%d\n", iRet, presetId);
            return iRet;
        }
    }

    s_stPtzParam.ptzCruise.nextPresetId = anj_ptz_cruise_following_preset_get(presetId);
    if (anj_ptz_wait_interrupt(PTZ_CRUISE_DWELL_TIME_US, PTZ_CRUISE) != 0)
    {
        *pPtzLastWork = PTZ_STOP;
        return 0;
    }

    *pPtzLastWork = PTZ_STOP;
    return 0;
}

/*****************************************************************************
 函 数 名  : anj_ptz_track_adjust
 功能描述  : 云台追踪速度调节,由于追踪主要转动幅度最大的是水平云台
           所以根据水平距离来调节云台转动速度即可
 输入参数  : Hdistance:横坐标距离中心横坐标的距离
 输出参数  : 无
 返 回 值  : 成功返回0，失败返回-1
*****************************************************************************/
static int anj_ptz_track_adjust(PtzSpeed *pTrackSpeed, int Distance, int bVadjust, int bDualTrackMode)
{
    int iStep = PTZ_START_TRACK_ONE_STEP;

    pTrackSpeed->VSpeed = PTZ_SPEED_2;
    pTrackSpeed->HSpeed = PTZ_SPEED_2;

    if (bDualTrackMode)
    {
        if (bVadjust)
        {
            iStep = Distance * 36 / (SMART_PD_HEIGHT / 2);
            if (iStep < PTZ_START_TRACK_ONE_STEP)
            {
                iStep = PTZ_START_TRACK_ONE_STEP;
            }
            if (iStep > s_iTrackDualMaxVStep)
            {
                iStep = s_iTrackDualMaxVStep;
            }
        }
        else
        {
            iStep = Distance * 24 / (SMART_PD_WIDTH / 2);
            if (iStep < PTZ_START_TRACK_ONE_STEP)
            {
                iStep = PTZ_START_TRACK_ONE_STEP;
            }
            if (iStep > s_iTrackDualMaxHStep)
            {
                iStep = s_iTrackDualMaxHStep;
            }
        }
    }
    else
    {
        if (bVadjust)
        {
            iStep = Distance * 40 / (SMART_PD_HEIGHT / 2);
        }
        else
        {
            iStep = Distance * 16 / (SMART_PD_WIDTH / 2);
        }
        if (iStep < PTZ_START_TRACK_ONE_STEP)
        {
            iStep = PTZ_START_TRACK_ONE_STEP;
        }
    }

    __DBG("####dual:%d bVadjust:%d ChunkStep:%d Speed:[%d %d] Distance:%d\n", bDualTrackMode, bVadjust, iStep,
          pTrackSpeed->HSpeed, pTrackSpeed->VSpeed, Distance);
    return iStep;
}

static int anj_ptz_track_operate(int iRunStep, int *pPtzLastWork, PtzSpeed *pTrackSpeed)
{
    int iRet = 0;
    int iPtzCtlStep = (iRunStep > 0) ? anj_ptz_step_range(iRunStep, *pPtzLastWork) : 0;
    if (iPtzCtlStep > 0)
    {
        int iPtzCtlWork = PTZ_CTL_NULL;
        int iPtzCtlSpeed = PTZ_DEFAULT_H_SPEED;
        switch (*pPtzLastWork)
        {
        case PTZ_UP:
            iPtzCtlSpeed = pTrackSpeed->VSpeed;
            iPtzCtlWork = PTZ_CTL_MOTOR_UP;
            __INFO("PTZ_UP\n");
            break;
        case PTZ_DOWN:
            iPtzCtlSpeed = pTrackSpeed->VSpeed;
            iPtzCtlWork = PTZ_CTL_MOTOR_DOWN;
            __INFO("PTZ_DOWN\n");
            break;
        case PTZ_LEFT:
            iPtzCtlSpeed = pTrackSpeed->HSpeed;
            iPtzCtlWork = PTZ_CTL_MOTOR_LEFT;
            __INFO("PTZ_LEFT\n");
            break;
        case PTZ_RIGHT:
            iPtzCtlSpeed = pTrackSpeed->HSpeed;
            iPtzCtlWork = PTZ_CTL_MOTOR_RIGHT;
            __INFO("PTZ_RIGHT\n");
            break;
        default:
            break;
        }
        __INFO("iPtzCtlStep:%d\n", iPtzCtlStep);
        iRet = anj_ptz_operate(iPtzCtlWork, iPtzCtlStep, iPtzCtlSpeed);
        if (iRet != 0)
        {
            __ERR("PTZ TRACK ctl failed:%d\n", iRet);
            return 0;
        }

        iRet = anj_ptz_wait_rundone();
        if (*pPtzLastWork == PTZ_DOWN || *pPtzLastWork == PTZ_UP)
        {
            s_stPtzParam.ptzVStepAddup += iPtzCtlStep - iRet;
        }
        anj_ptz_step_update(*pPtzLastWork, iPtzCtlStep - iRet);
    }
    return iRet;
}

static int anj_ptz_lens_cover_ctrl(int enable, int *pPtzLastWork)
{
    PtzStep stPtzStep = {0};
    PtzSpeed stSpeed = {0};

    stSpeed.HSpeed = PTZ_SPEED_5;
    stSpeed.VSpeed = PTZ_SPEED_5;

    anj_ptz_step_update(*pPtzLastWork, anj_ptz_wait_stop());
    if (enable)
    {
        anj_ptz_osd_set(OVERLAY_LENS_COVER, 1);
        if (s_stPtzParam.bLensCoverEnable == 0)
        {
            s_stPtzParam.lensCoverStep = s_stPtzParam.m_PtzStep;
            s_stPtzParam.bLensCoverStepValid = 1;
        }

        stPtzStep = s_stPtzParam.m_PtzStep;
        stPtzStep.VStep = 0;
        anj_ptz_ctl_preset(&stPtzStep, &stSpeed, pPtzLastWork, 0);
        s_stPtzParam.bLensCoverEnable = 1;
        s_stPtzParam.ptzWork = PTZ_NONE;
    }
    else
    {
        if (s_stPtzParam.bLensCoverStepValid)
        {
            stPtzStep = s_stPtzParam.lensCoverStep;
            anj_ptz_ctl_preset(&stPtzStep, &stSpeed, pPtzLastWork, 0);
        }
        s_stPtzParam.bLensCoverEnable = 0;
        s_stPtzParam.bLensCoverStepValid = 0;
        s_stPtzParam.ptzWork = PTZ_NONE;
        anj_ptz_osd_set(OVERLAY_LENS_COVER, 0);
    }

    return 0;
}

static int anj_ptz_track_start(int *pPtzLastWork)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    int iRet = 0;
    int iRunStep = 0;
    int Hdistance = 0;  // 水平差距
    int Hdirection = 0; // 水平方向
    int Vdistance = 0;  // 垂直差距
    int Vdirection = 0; // 垂直方向
    AJ_POINT_S center = {0};
    center.x = SMART_PD_WIDTH / 2;
    center.y = SMART_PD_HEIGHT / 2;

    PtzSpeed TrackSpeed = {0};                           /*电机速度*/
    PtzStep ptzBeforeTrackStep = s_stPtzParam.m_PtzStep; /*云台追踪之前的位置步数*/
    s_stPtzParam.ptzTrack.bTrackStart = 1;

    int errCnt = 0;

    anj_ptz_track_tune_load();
    __INFO("track start\n");

    while (s_stPtzParam.ptzWork == PTZ_TRACK)
    {
        PdAction *pstAlarmAction = &pstAlarmCfg->aiAlarm.pdAlarm[0].alarmAction;
        int bTrackEnable = pstAlarmAction->track_human_enable;
        int bZoomEnable = pstAlarmAction->auto_zoom_enable;
        int bTurnH = 0;
        int bTurnV = 0;
        int bDualTrackMode = (bTrackEnable && bZoomEnable);

        /* 如果没有新坐标，结束追踪 */
        if ((anj_mw_get_cputime_ms(NULL) - s_stPtzParam.ptzTrack.startTrackTime) > PTZ_TRACK_DISPLAY_TIME)
        {
            break;
        }

        /* 使用人形框中心点（y 取 height/3）作为目标 */
        PD_AREA_ENTRY stTrackCenterArea = s_stPtzParam.ptzTrack.ptzTrackArea;
        stTrackCenterArea.xPos = stTrackCenterArea.xPos + stTrackCenterArea.width / 2;
        stTrackCenterArea.yPos = stTrackCenterArea.yPos + stTrackCenterArea.height / 3;
        if (stTrackCenterArea.xPos >= center.x)
        {
            Hdistance = stTrackCenterArea.xPos - center.x;
            Hdirection = 1;
        }
        else
        {
            Hdistance = center.x - stTrackCenterArea.xPos;
            Hdirection = 0;
        }

        if (stTrackCenterArea.yPos >= center.y)
        {
            Vdistance = stTrackCenterArea.yPos - center.y;
            Vdirection = 1;
        }
        else
        {
            Vdistance = center.y - stTrackCenterArea.yPos;
            Vdirection = 0;
        }

        if (!bTrackEnable && !bZoomEnable)
        {
            break;
        }

        if (bZoomEnable)
        {
            /* 先做变倍追踪；双开时变倍贴边饱和后再转云台 */
            anj_zoom_track_start(0, &s_stPtzParam.ptzTrack.ptzTrackArea, 0);
        }

        if (bTrackEnable && bZoomEnable)
        {
            anj_ptz_track_need_turn_by_zoom_saturate(Hdistance, Hdirection, Vdistance, Vdirection, &bTurnH, &bTurnV);
        }
        else if (bTrackEnable)
        {
            bTurnH = (Hdistance >= PTZ_TRACK_MIN_H_PIXEL(SMART_PD_WIDTH));
            bTurnV = (Vdistance >= PTZ_TRACK_MIN_V_PIXEL(SMART_PD_HEIGHT));
        }

        if (bTurnH)
        {
            iRunStep = anj_ptz_track_adjust(&TrackSpeed, Hdistance, 0, bDualTrackMode);
            *pPtzLastWork = Hdirection ? PTZ_RIGHT : PTZ_LEFT;
            iRet = anj_ptz_track_operate(iRunStep, pPtzLastWork, &TrackSpeed);
            if (iRet < 0)
            {
                errCnt++;
                if (errCnt >= 3)
                {
                    __ERR("PTZ TRACK run interrupt:%d\n", iRet);
                    break;
                }
            }
            else
            {
                errCnt = 0;
            }
        }

        if (bTurnV)
        {
            iRunStep = anj_ptz_track_adjust(&TrackSpeed, Vdistance, 1, bDualTrackMode);
            *pPtzLastWork = Vdirection ? PTZ_DOWN : PTZ_UP;
            iRet = anj_ptz_track_operate(iRunStep, pPtzLastWork, &TrackSpeed);
            if (iRet < 0)
            {
                errCnt++;
                if (errCnt >= 3)
                {
                    __ERR("PTZ TRACK run interrupt:%d\n", iRet);
                    break;
                }
            }
            else
            {
                errCnt = 0;
            }
        }

        iRunStep = 0;
        anj_mw_rsleep(10 * 1000);
    }
    __INFO("track end\n");

    if (s_stPtzParam.ptzWork == PTZ_TRACK && iRet == 0)
    {
        if (s_stPtzParam.ptzVStepAddup >= PTZ_V_MAX_STEP * 3)
        {
            s_stPtzParam.ptzVStepAddup = 0;
            iRet = anj_ptz_ctl_reset(&ptzBeforeTrackStep);
        }
        else
        {
            PtzSpeed stSpeed = {0};
            stSpeed.HSpeed = PTZ_DEFAULT_H_SPEED;
            stSpeed.VSpeed = PTZ_DEFAULT_V_SPEED;
            anj_mutex_lock(&s_stPtzMutex);
            if (s_stPtzParam.ptzWork == PTZ_TRACK)
            {
                anj_zoom_track_start(0, NULL, 1);
            }
            anj_mutex_unlock(&s_stPtzMutex);
            iRet = anj_ptz_ctl_preset(&ptzBeforeTrackStep, &stSpeed, pPtzLastWork, 1);
        }
    }
    anj_zoom_track_stop(0);
    anj_smart_track_rect_reset(0);

    s_stPtzParam.ptzTrack.endTrackTime = anj_mw_get_cputime_ms(NULL);
    s_stPtzParam.ptzTrack.bTrackStart = 0;
    anj_mutex_lock(&s_stPtzMutex);
    if (s_stPtzParam.ptzWork == PTZ_TRACK)
    {
        s_stPtzParam.ptzWork = PTZ_NONE;
    }
    anj_mutex_unlock(&s_stPtzMutex);
    *pPtzLastWork = PTZ_STOP;
    return iRet;
}

static void anj_ptz_track_set(EventResult *event_result, void *data)
{
    event_rect_param_s *pstNowRectParam = (event_rect_param_s *)data;
    if (pstNowRectParam == NULL)
    {
        __ERR("input invalid!\n");
        return;
    }
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    int cameraIndex = pstNowRectParam->camera;
    if (pstAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.track_human_enable == 0 &&
        pstAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.auto_zoom_enable == 0)
    {
        return;
    }
    static int trackCnt = 0;
    if (pstNowRectParam->s32RectCnt > 0)
    {
        unsigned long long now = anj_mw_get_cputime_ms(NULL);
        int iPtzRemainStep = -1;
        anj_mutex_lock(&s_stPtzMutex);
        /* 开启追踪间隔至少 3s */
        if (s_stPtzParam.ptzTrack.endTrackTime != 0 &&
            (now - s_stPtzParam.ptzTrack.endTrackTime) < PTZ_TRACK_END_TIME)
        {
            anj_mutex_unlock(&s_stPtzMutex);
            return;
        }

        event_rect_s stTrackRec = {0};

        if (anj_smart_track_rect_update(cameraIndex, pstNowRectParam, &stTrackRec) == 0)
        {
            anj_mutex_unlock(&s_stPtzMutex);
            return;
        }

        // 将stTrackRec 坐标映射回原图坐标
        DOUBLE_AREA_ENTRY cur_area = {0};
        anj_zoom_run_get(cameraIndex, &cur_area, NULL);
        anj_mw_smart_rect_map_zoom_to_pd(&cur_area, &stTrackRec.pos_x, &stTrackRec.pos_y,
                                         &stTrackRec.width, &stTrackRec.height);
        s_stPtzParam.ptzTrack.ptzTrackArea.xPos = stTrackRec.pos_x;
        s_stPtzParam.ptzTrack.ptzTrackArea.yPos = stTrackRec.pos_y;
        s_stPtzParam.ptzTrack.ptzTrackArea.width = stTrackRec.width;
        s_stPtzParam.ptzTrack.ptzTrackArea.height = stTrackRec.height;
        s_stPtzParam.ptzTrack.bUpdate = 1;
        s_stPtzParam.ptzTrack.startTrackTime = now;
        if (s_stPtzParam.ptzWork == PTZ_NONE)
        {
            iPtzRemainStep = anj_ptz_operate(PTZ_CTL_REMAIN_STEP, 0, 0);
        }

        if (s_stPtzParam.ptzWork == PTZ_NONE &&
            iPtzRemainStep == 0 &&
            ((now - s_stPtzParam.ptzNoneTime) >= PTZ_TRACK_DISPLAY_TIME))
        {
            trackCnt++;
            if (trackCnt > PTZ_TRACK_START_TIMES)
            {
                trackCnt = 0;
                s_stPtzParam.ptzWork = PTZ_TRACK;
            }
        }
        else
        {
            trackCnt = 0;
        }
        anj_mutex_unlock(&s_stPtzMutex);
    }
    else
    {
        trackCnt = 0;
    }
}

/******************************************************************************
@函数名称 : int ptz_handle(char *pPtzcmd, char *ret_buf, int ret_buf_size)

@功能描述 : 对云台命令进行操作

@输入参数 : pPtzcmd：云台命令

@输出参数 : ret_buf：返回的数据 ret_buf_size：数据长度

@返回值： 成功返回0，失败返回-1
******************************************************************************/
static void anj_ptz_handle(EventResult *event_result, void *data)
{
    int ptzWork = PTZ_NONE;
    PtzCmdParse *pstCmdParse = (PtzCmdParse *)data;
    if (pstCmdParse == NULL)
    {
        __ERR("input invalid!\n");
        return;
    }

    __INFO("CMD:%s\n", pstCmdParse->ptzCmd);
    if (strcmp(pstCmdParse->ptzCmd, "right") == 0)
    {
        ptzWork = PTZ_RIGHT;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "left") == 0)
    {
        ptzWork = PTZ_LEFT;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "up") == 0)
    {
        ptzWork = PTZ_UP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "down") == 0)
    {
        ptzWork = PTZ_DOWN;
    }
    // else if (strcmp(pstCmdParse->ptzCmd, "right_up") == 0)
    // {
    //     int ctrlLeft = pstVideoCapture->hflip ? 1 : 0;
    //     int ctrlDown = pstVideoCapture->vflip ? 1 : 0;
    // }
    // else if (strcmp(pstCmdParse->ptzCmd, "right_down") == 0)
    // {
    //     int ctrlLeft = pstVideoCapture->hflip ? 1 : 0;
    //     int ctrlDown = pstVideoCapture->vflip ? 0 : 1;
    // }
    // else if (strcmp(pstCmdParse->ptzCmd, "left_up") == 0)
    // {
    //     int ctrlLeft = pstVideoCapture->hflip ? 0 : 1;
    //     int ctrlDown = pstVideoCapture->vflip ? 1 : 0;
    // }
    // else if (strcmp(pstCmdParse->ptzCmd, "left_down") == 0)
    // {
    //     int ctrlLeft = pstVideoCapture->hflip ? 0 : 1;
    //     int ctrlDown = pstVideoCapture->vflip ? 0 : 1;
    // }
    else if (strcmp(pstCmdParse->ptzCmd, "stop") == 0)
    {
        anj_zoom_stop(0, NULL);
        ptzWork = PTZ_STOP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "callpreset") == 0) /*调用预置点*/
    {
        if (!anj_ptz_special_preset_handle(pstCmdParse->presetID, &ptzWork))
        {
            s_stPtzParam.preset_id = pstCmdParse->presetID;
            ptzWork = PTZ_PRESET;
        }
    }
    else if ((strcmp(pstCmdParse->ptzCmd, "PtzRestore") == 0) || (strcmp(pstCmdParse->ptzCmd, "PtzReboot") == 0)) /*复位*/
    {
        ptzWork = PTZ_RESET;
    }
    else if (strcmp(pstCmdParse->ptzCmd, PTZ_CMD_LENS_COVER_ON) == 0)
    {
        if (s_stPtzParam.bLensCoverEnable == 0)
            ptzWork = PTZ_LENS_COVER_ON;
    }
    else if (strcmp(pstCmdParse->ptzCmd, PTZ_CMD_LENS_COVER_OFF) == 0)
    {
        if (s_stPtzParam.bLensCoverEnable)
            ptzWork = PTZ_LENS_COVER_OFF;
    }
    else if ((strcmp(pstCmdParse->ptzCmd, "setpreset") == 0) ||
             (strcmp(pstCmdParse->ptzCmd, "SetPresetName") == 0)) /*设置预置点*/
    {
        if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ) && pstCmdParse->presetID == PTZ_SPECIAL_PRESET_LINE_SCAN_SET_LEFT)
        {
            anj_ptz_line_scan_boundary_save(1);
        }
        else if ((ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || SUPPORT_ADVANCE_PTZ) && pstCmdParse->presetID == PTZ_SPECIAL_PRESET_LINE_SCAN_START)
        {
            anj_ptz_line_scan_boundary_save(0);
        }
        else
        {
            IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
            IotPtzConfig stPtzConfig = *pstIotPtzConfig;
            PtzPreset *pstPtzPreset = &stPtzConfig.m_ptzPreset[pstCmdParse->presetID - 1];
            pstPtzPreset->preset_id = pstCmdParse->presetID;
            pstPtzPreset->preset_step = s_stPtzParam.m_PtzStep;
            strncpy(pstPtzPreset->name, pstCmdParse->presetName, sizeof(pstPtzPreset->name));
            anj_ptz_config_save(&stPtzConfig);
            anj_ptz_osd_set(OVERLAY_PTZ_SET_PRESET, 1);
        }
        anj_ser_reponse(SER_RESPONSE_PTZ_PRESET, NULL);
    }
    else if (strcmp(pstCmdParse->ptzCmd, "clearpreset") == 0) /*清除预置点*/
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        IotPtzConfig stPtzConfig = *pstIotPtzConfig;
        if (stPtzConfig.watch_guard == pstCmdParse->presetID)
        {
            stPtzConfig.watch_guard = 0;
            stPtzConfig.watch_guard_time = 0;
        }
        s_stPtzParam.preset_id = pstCmdParse->presetID;
        memset(&stPtzConfig.m_ptzPreset[pstCmdParse->presetID - 1], 0, sizeof(PtzPreset));
        anj_ptz_config_save(&stPtzConfig);
        anj_ptz_osd_set(OVERLAY_PTZ_CLEAR_PRESET, 1);
        anj_ser_reponse(SER_RESPONSE_PTZ_PRESET, NULL);
    }
    else if (strcmp(pstCmdParse->ptzCmd, "GetPresetList") == 0) /*获取预置点列表*/
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        event_data_s *pstEventData = (event_data_s *)event_result->result;
        anj_ptz_preset_list(pstEventData->data, pstEventData->len, pstIotPtzConfig);
    }
    else if (strcmp(pstCmdParse->ptzCmd, "SetGuardPreset") == 0) /*设置看守卫*/
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        IotPtzConfig stPtzConfig = *pstIotPtzConfig;
        int i = 0;
        for (i = 0; i < MAX_PTZ_PRESET; i++)
        {
            if (pstCmdParse->presetID == stPtzConfig.m_ptzPreset[i].preset_id)
            {
                stPtzConfig.watch_guard = pstCmdParse->presetID;
                stPtzConfig.watch_guard_time = pstCmdParse->watchGuardTime;
                anj_ptz_config_save(&stPtzConfig);
                anj_ptz_osd_set(OVERLAY_PTZ_WATCH_GUARD_SET_OK, 1);
                anj_ser_reponse(SER_RESPONSE_PTZ_PRESET, NULL);
                break;
            }
        }
        if (i >= MAX_PTZ_PRESET)
        {
            anj_ptz_osd_set(OVERLAY_PTZ_WATCH_GUARD_SET_FAIL, 1);
        }
    }
    else if (strcmp(pstCmdParse->ptzCmd, "ClearGuardPreset") == 0) /*删除看守卫*/
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        IotPtzConfig stPtzConfig = *pstIotPtzConfig;
        stPtzConfig.watch_guard = 0;
        stPtzConfig.watch_guard_time = 0;
        anj_ptz_config_save(&stPtzConfig);
        anj_ptz_osd_set(OVERLAY_PTZ_WATCH_GUARD_CLEAR, 1);
        anj_ser_reponse(SER_RESPONSE_PTZ_PRESET, NULL);
    }
    else if (strcmp(pstCmdParse->ptzCmd, "zoomtele") == 0)
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        s_stPtzParam.ptzZoomMultiple = pstIotPtzConfig->m_zoom.max_multiple;
        anj_zoom_interrupt(0, s_stPtzParam.ptzZoomMultiple);
        ptzWork = PTZ_ZOOM;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "zoomwide") == 0)
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        s_stPtzParam.ptzZoomMultiple = pstIotPtzConfig->m_zoom.min_multiple;
        anj_zoom_interrupt(0, s_stPtzParam.ptzZoomMultiple);
        ptzWork = PTZ_ZOOM;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "AreaScanOn") == 0)
    {
        anj_ptz_line_scan_toggle(1, 1);
        ptzWork = PTZ_STOP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "AreaScanOff") == 0)
    {
        anj_ptz_line_scan_toggle(0, 1);
        ptzWork = PTZ_STOP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "TrackOn") == 0)
    {
    }
    else if (strcmp(pstCmdParse->ptzCmd, "TrackOff") == 0)
    {
    }
    else if (strcmp(pstCmdParse->ptzCmd, "CruiseOn") == 0)
    {
        anj_ptz_cruise_toggle(1, 1);
        ptzWork = PTZ_STOP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "CruiseOff") == 0)
    {
        anj_ptz_cruise_toggle(0, 1);
        ptzWork = PTZ_STOP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "FocusFarAutoOff") == 0)
    {
    }
    else if (strcmp(pstCmdParse->ptzCmd, "FocusNearAutoOff") == 0)
    {
    }
    else if (strcmp(pstCmdParse->ptzCmd, "AutoScanOn") == 0)
    {
        anj_ptz_line_scan_toggle(1, 1);
        ptzWork = PTZ_STOP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "AutoScanOff") == 0)
    {
        anj_ptz_line_scan_toggle(0, 1);
        ptzWork = PTZ_STOP;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "LeftMargin") == 0)
    {
        anj_ptz_line_scan_boundary_save(1);
    }
    else if (strcmp(pstCmdParse->ptzCmd, "RightMargin") == 0)
    {
        anj_ptz_line_scan_boundary_save(0);
    }
    else if (strcmp(pstCmdParse->ptzCmd, "PtzReboot") == 0)
    {
    }
    else if (strcmp(pstCmdParse->ptzCmd, "ScanSpeedUp") == 0)
    {
    }
    else if (strcmp(pstCmdParse->ptzCmd, "ScanSpeedDown") == 0)
    {
    }
    else if (strcmp(pstCmdParse->ptzCmd, "move3DPoint") == 0)
    {
        anj_ptz_3d_point_conver_step(pstCmdParse->posX, pstCmdParse->posY, &s_stPtzParam.ptz3DStep);
        ptzWork = PTZ_3D_POSITION;
    }
    else if (strcmp(pstCmdParse->ptzCmd, "ptz3dlocate") == 0)
    {
        int posx1 = (pstCmdParse->posX >> 8) & 0xff;
        int posy1 = (pstCmdParse->posX) & 0xff;
        int posx2 = (pstCmdParse->posY >> 8) & 0xff;
        int posy2 = (pstCmdParse->posY) & 0xff;

        int centerx = (posx1 + posx2) >> 1;
        int centery = (posy1 + posy2) >> 1;

        anj_ptz_3d_point_conver_step(centerx, centery, &s_stPtzParam.ptz3DStep);
        ptzWork = PTZ_3D_POSITION;
    }

    anj_ptz_work_set(ptzWork);
}

static int anj_ptz_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    sleep(3);
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    int iSaveStepTime = 0;
    int ptzGuardTime = 0;           /*执行看守卫时间*/
    int ptzCurWork = PTZ_NONE;      /*记录云台当前电机状态(上下左右)*/
    int ptzLastWork = PTZ_NONE;     /*记录云台停止前电机状态(上下左右)*/
    int iLastInitCheckTimeDiff = 0; /*距离上次自检的时间*/
    int bAudioMute = 0;
    while (bStart && *bStart)
    {
        if (0 == access("/tmp/ptzdebug", F_OK))
        {
            remove("/tmp/ptzdebug");
            anj_ptz_provider_debug();
        }

        if (s_stPtzParam.ptzBurnInTime > 0)
        {
            anj_mutex_lock(&s_stPtzMutex);
            s_stPtzParam.ptzWork = PTZ_RESET;
            anj_mutex_unlock(&s_stPtzMutex);
            __INFO("Burn-IN ptz times(%d)\n", s_stPtzParam.ptzBurnInTime);
            sleep(s_stPtzParam.ptzBurnInTime);
        }

        if (s_stPtzParam.ptzOsdTime != 0 && anj_mw_get_cputime_ms(NULL) - s_stPtzParam.ptzOsdTime > ZOOM_OSD_HIDE_DELAY_MS)
        {
            anj_ptz_osd_set(OVERLAY_BUIT, 0);
        }

        // 云台转动静音
        if (pstMediaConfig->audioConfig.audioCapture.mute_ptz_turn)
        {
            int bPtzTurning = 0;
            int mute = 0;

            if ((s_stPtzParam.ptzWork != PTZ_NONE && s_stPtzParam.ptzWork != PTZ_ZOOM) ||
                ptzLastWork == PTZ_UP ||
                ptzLastWork == PTZ_DOWN ||
                ptzLastWork == PTZ_LEFT ||
                ptzLastWork == PTZ_RIGHT)
            {
                bPtzTurning = 1;
            }

            if (bPtzTurning)
            {
                mute = 1;
            }

            if (bAudioMute != mute)
            {
                bAudioMute = mute;
                anj_audio_ai_mute_set(bAudioMute);
            }
        }

        /*不存在看守卫，手动上下左右stop后停留1分钟，存一次resetStep；预置点/追踪等不更新*/
        if (pstIotPtzConfig->watch_guard == 0 && iSaveStepTime > 0 &&
            s_stPtzParam.ptzWork == PTZ_NONE && ptzLastWork == PTZ_STOP)
        {
            iSaveStepTime++;
            if (iSaveStepTime >= (PTZ_SAVE_STEP_TIEM / PTZ_THREAD_TIME) &&
                ((pstIotPtzConfig->m_resetStep.HStep != s_stPtzParam.m_PtzStep.HStep) ||
                 (pstIotPtzConfig->m_resetStep.VStep != s_stPtzParam.m_PtzStep.VStep)))
            {
                iSaveStepTime = 0;
                IotPtzConfig stPtzConfig = *pstIotPtzConfig;
                memcpy(&stPtzConfig.m_resetStep, &s_stPtzParam.m_PtzStep, sizeof(PtzStep));
                anj_ptz_config_save(&stPtzConfig);
            }
        }
        else
        {
            iSaveStepTime = 0;
        }

        if (s_stPtzParam.ptzWork == PTZ_NONE && ptzLastWork == PTZ_STOP)
        {
            /*转动电机结束后，若不在看守卫上达一定时间，电机回到看守卫*/
            if (pstIotPtzConfig->watch_guard > 0)
            {
                PtzStep *pstGuardStep = &pstIotPtzConfig->m_ptzPreset[pstIotPtzConfig->watch_guard - 1].preset_step;
                if (0 != memcmp(&s_stPtzParam.m_PtzStep, pstGuardStep, sizeof(PtzStep)))
                {
                    ptzGuardTime++;

                    if (ptzGuardTime >= pstIotPtzConfig->watch_guard_time * (1000 * 1000 / PTZ_THREAD_TIME))
                    {
                        anj_mutex_lock(&s_stPtzMutex);
                        if (iLastInitCheckTimeDiff >= PTZ_NOT_TRACK_CUMULATIVE_TIME)
                        {
                            iLastInitCheckTimeDiff = 0;
                            s_stPtzParam.ptzWork = PTZ_RESET;
                        }
                        else
                        {
                            s_stPtzParam.ptzWork = PTZ_GUARD;
                        }

                        ptzGuardTime = 0;
                        anj_mutex_unlock(&s_stPtzMutex);
                    }
                }
                else
                {
                    ptzGuardTime = 0;
                }
            }
            else
            {
                ptzGuardTime = 0;
            }
        }
        else
        {
            ptzGuardTime = 0;
        }

        if (s_stPtzParam.ptzWork == PTZ_NONE &&
            pstIotPtzConfig->m_ptzLineScan.enable &&
            pstIotPtzConfig->m_ptzCruiseEnable == 0 &&
            (anj_mw_get_cputime_ms(NULL) - s_stPtzParam.ptzNoneTime) >= PTZ_LINE_SCAN_IDLE_START_MS)
        {
            anj_mutex_lock(&s_stPtzMutex);
            if (s_stPtzParam.ptzWork == PTZ_NONE)
            {
                s_stPtzParam.ptzWork = PTZ_LINE_SCAN;
            }
            anj_mutex_unlock(&s_stPtzMutex);
        }
        if (s_stPtzParam.ptzWork == PTZ_NONE &&
            pstIotPtzConfig->m_ptzCruiseEnable &&
            pstIotPtzConfig->m_ptzLineScan.enable == 0 &&
            (anj_mw_get_cputime_ms(NULL) - s_stPtzParam.ptzNoneTime) >= PTZ_CRUISE_IDLE_START_MS)
        {
            if (s_stPtzParam.ptzCruise.startTime != 0 &&
                (anj_mw_get_cputime_ms(NULL) - s_stPtzParam.ptzCruise.startTime) >= PTZ_CRUISE_MAX_DURATION_MS)
            {
                anj_ptz_cruise_toggle(0, 1);
            }
            else
            {
                anj_mutex_lock(&s_stPtzMutex);
                if (s_stPtzParam.ptzWork == PTZ_NONE)
                {
                    s_stPtzParam.ptzWork = PTZ_CRUISE;
                }
                anj_mutex_unlock(&s_stPtzMutex);
            }
        }
        ptzCurWork = s_stPtzParam.ptzWork;

        PtzStep stPtzStep = {0};
        if (ptzCurWork == PTZ_PRESET)
        {
            PtzStep *pstPresetStep = &pstIotPtzConfig->m_ptzPreset[s_stPtzParam.preset_id - 1].preset_step;
            stPtzStep = *pstPresetStep;
            anj_zoom_ctrl(0, pstIotPtzConfig->m_ptzPreset[s_stPtzParam.preset_id - 1].zoom_multiple);
            __INFO("PRESET Hstep:%d Vstep:%d\n", stPtzStep.HStep, stPtzStep.VStep);
            anj_ptz_osd_set(OVERLAY_PTZ_GOTO_PRESET, 1);
        }
        else if (ptzCurWork == PTZ_3D_POSITION)
        {
            stPtzStep = s_stPtzParam.ptz3DStep;
            __INFO("3D POSITION Hstep:%d Vstep:%d\n", stPtzStep.HStep, stPtzStep.VStep);
        }
        else if (ptzCurWork == PTZ_LINE_SCAN)
        {
            PtzLineScan *pstLineScan = &pstIotPtzConfig->m_ptzLineScan;
            if (s_stPtzParam.ptzLineScan.scanDir)
            {
                stPtzStep = pstLineScan->right_margin;
            }
            else
            {
                stPtzStep = pstLineScan->left_margin;
            }
            __INFO("LINE SCAN Hstep:%d Vstep:%d dir:%d\n", stPtzStep.HStep, stPtzStep.VStep, s_stPtzParam.ptzLineScan.scanDir);
        }
        else if (ptzCurWork == PTZ_CRUISE)
        {
            __INFO("CRUISE next preset:%d\n", s_stPtzParam.ptzCruise.nextPresetId);
        }
        else if (ptzCurWork == PTZ_GUARD)
        {
            PtzStep *pstGuardStep = &pstIotPtzConfig->m_ptzPreset[pstIotPtzConfig->watch_guard - 1].preset_step;
            stPtzStep = *pstGuardStep;
            anj_zoom_ctrl(0, pstIotPtzConfig->m_ptzPreset[pstIotPtzConfig->watch_guard - 1].zoom_multiple);
            __INFO("Guard Hstep:%d Vstep:%d\n", stPtzStep.HStep, stPtzStep.VStep);
            anj_ptz_osd_set(OVERLAY_PTZ_GOTO_GARDPOS, 1);
        }
        else if (ptzCurWork == PTZ_RESET)
        {
            PtzStep *pstResetStep = &pstIotPtzConfig->m_resetStep;
            if (pstIotPtzConfig->watch_guard)
            {
                PtzStep *pstGuardStep = &pstIotPtzConfig->m_ptzPreset[pstIotPtzConfig->watch_guard - 1].preset_step;
                stPtzStep = *pstGuardStep;
            }
            else
            {
                if (pstResetStep->HStep == 0 && pstResetStep->VStep == 0)
                {
                    stPtzStep.HStep = PTZ_H_RESET_STEP(s_stPtzParam.MaxStep.HStep);
                    stPtzStep.VStep = PTZ_V_RESET_STEP(s_stPtzParam.MaxStep.VStep);
                }
                else
                {
                    stPtzStep = *pstResetStep;
                }
            }
            anj_zoom_ctrl(0, pstIotPtzConfig->m_zoom.cur_multiple);
            __INFO("RESET Hstep:%d Vstep:%d\n", stPtzStep.HStep, stPtzStep.VStep);
        }
        else if (ptzCurWork == PTZ_ZOOM)
        {
            __INFO("ZOOM target multiple:%f\n", s_stPtzParam.ptzZoomMultiple);
        }

        switch (ptzCurWork)
        {
        case PTZ_STOP:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            if (ptzLastWork == PTZ_UP || ptzLastWork == PTZ_DOWN ||
                ptzLastWork == PTZ_LEFT || ptzLastWork == PTZ_RIGHT)
            {
                iSaveStepTime = 1;
            }
            ptzLastWork = ptzCurWork;
            break;
        }
        case PTZ_UP:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            int iPtzCtlStep = (s_stPtzParam.MaxStep.VStep - s_stPtzParam.m_PtzStep.VStep);
            __INFO("UP, step:%d ctl:%d\n", s_stPtzParam.m_PtzStep.VStep, iPtzCtlStep);
            iRet = anj_ptz_operate(PTZ_CTL_MOTOR_UP, iPtzCtlStep, pstIotPtzConfig->m_ptzSpeed.VSpeed);
            if (iRet != 0)
            {
                __ERR("PTZ UP ctl failed:%d\n", iRet);
            }
            else
            {
                ptzLastWork = ptzCurWork;
            }
            break;
        }
        case PTZ_DOWN:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            int iPtzCtlStep = s_stPtzParam.m_PtzStep.VStep;
            __INFO("DOWN, step:%d ctl:%d\n", s_stPtzParam.m_PtzStep.VStep, iPtzCtlStep);
            iRet = anj_ptz_operate(PTZ_CTL_MOTOR_DOWN, iPtzCtlStep, pstIotPtzConfig->m_ptzSpeed.VSpeed);
            if (iRet != 0)
            {
                __ERR("PTZ DOWN ctl failed:%d\n", iRet);
            }
            else
            {
                ptzLastWork = ptzCurWork;
            }
            break;
        }
        case PTZ_LEFT:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            int iPtzCtlStep = (s_stPtzParam.MaxStep.HStep - s_stPtzParam.m_PtzStep.HStep);
            __INFO("LEFT, step:%d ctl:%d\n", s_stPtzParam.m_PtzStep.HStep, iPtzCtlStep);
            iRet = anj_ptz_operate(PTZ_CTL_MOTOR_LEFT, iPtzCtlStep, pstIotPtzConfig->m_ptzSpeed.HSpeed);
            if (iRet != 0)
            {
                __ERR("PTZ LEFT ctl failed:%d\n", iRet);
            }
            else
            {
                ptzLastWork = ptzCurWork;
            }
            break;
        }
        case PTZ_RIGHT:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            int iPtzCtlStep = s_stPtzParam.m_PtzStep.HStep;
            __INFO("RIGHT, step:%d ctl:%d\n", s_stPtzParam.m_PtzStep.HStep, iPtzCtlStep);
            iRet = anj_ptz_operate(PTZ_CTL_MOTOR_RIGHT, iPtzCtlStep, pstIotPtzConfig->m_ptzSpeed.HSpeed);
            if (iRet != 0)
            {
                __ERR("PTZ RIGHT ctl failed:%d\n", iRet);
            }
            else
            {
                ptzLastWork = ptzCurWork;
            }
            break;
        }
        case PTZ_RESET:
        {
            iRet = anj_ptz_ctl_reset(&stPtzStep);
            if (iRet != 0)
            {
                __ERR("PTZ RESET ctl failed:%d\n", iRet);
            }
            else
            {
                ptzLastWork = ptzCurWork;
            }
            break;
        }
        case PTZ_PRESET:
        case PTZ_3D_POSITION:
        case PTZ_GUARD:
        {
            PtzSpeed stSpeed = {0};
            stSpeed.HSpeed = PTZ_DEFAULT_H_SPEED;
            stSpeed.VSpeed = PTZ_DEFAULT_V_SPEED;
            if (ptzCurWork == PTZ_3D_POSITION)
            {
                stSpeed.HSpeed = PTZ_SPEED_6;
                stSpeed.VSpeed = PTZ_SPEED_6;
            }
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            iRet = anj_ptz_ctl_preset(&stPtzStep, &stSpeed, &ptzLastWork, 0);
            if (iRet != 0)
            {
                __ERR("PTZ PRESET ctl failed:%d\n", iRet);
            }
            else
            {
                ptzLastWork = ptzCurWork;
            }
            break;
        }
        case PTZ_TRACK:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            anj_ptz_track_start(&ptzLastWork);
            break;
        }
        case PTZ_LINE_SCAN:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            iRet = anj_ptz_line_scan_start(&ptzLastWork);
            if (iRet != 0)
            {
                __ERR("PTZ LINE SCAN run failed:%d\n", iRet);
            }
            break;
        }
        case PTZ_CRUISE:
        {
            anj_ptz_step_update(ptzLastWork, anj_ptz_wait_stop());
            iRet = anj_ptz_cruise_start(&ptzLastWork);
            if (iRet != 0)
            {
                __ERR("PTZ CRUISE run failed:%d\n", iRet);
            }
            break;
        }
        case PTZ_LENS_COVER_ON:
        {
            iRet = anj_ptz_lens_cover_ctrl(1, &ptzLastWork);
            if (iRet != 0)
            {
                __ERR("PTZ LENS COVER ON ctl failed:%d\n", iRet);
                s_stPtzParam.ptzWork = PTZ_NONE;
            }
            break;
        }
        case PTZ_LENS_COVER_OFF:
        {
            iRet = anj_ptz_lens_cover_ctrl(0, &ptzLastWork);
            if (iRet != 0)
            {
                __ERR("PTZ LENS COVER OFF ctl failed:%d\n", iRet);
            }
            break;
        }
        case PTZ_ZOOM:
        {
            anj_zoom_ctrl(0, s_stPtzParam.ptzZoomMultiple);
            ptzLastWork = ptzCurWork;
            break;
        }
        case PTZ_NONE:
        default:
        {
            break;
        }
        }
        anj_mutex_lock(&s_stPtzMutex);
        if (s_stPtzParam.ptzWork != PTZ_NONE)
        {
            s_stPtzParam.ptzNoneTime = anj_mw_get_cputime_ms(NULL);
        }
        else
        {
            if ((ptzCurWork != s_stPtzParam.ptzWork) && (ptzLastWork == PTZ_STOP))
            {
                s_stPtzParam.ptzNoneTime = anj_mw_get_cputime_ms(NULL);
            }
        }
        if (s_stPtzParam.bptzInterrupt)
        {
            s_stPtzParam.bptzInterrupt = 0;
        }
        else
        {
            /* PTZ_TRACK 由 anj_ptz_track_start 结束时自行清 work */
            if (s_stPtzParam.ptzWork == ptzLastWork && ptzCurWork != PTZ_TRACK)
            {
                s_stPtzParam.ptzWork = PTZ_NONE;
            }
        }
        anj_mutex_unlock(&s_stPtzMutex);
        usleep(PTZ_THREAD_TIME);
        iLastInitCheckTimeDiff++;
    }
    __INFO("PTZ_THREAD_EXIT!\n");
    return 0;
}

static void anj_ptz_dir_set(EventResult *event_result, void *data)
{
    if (event_result)
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        anj_ptz_provider_dir_set(&pstIotPtzConfig->m_ptzDir);
        event_result->ret = 0;
    }
}

static void anj_ptz_speed_set(EventResult *event_result, void *data)
{
    if (event_result)
    {
        IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
        anj_ptz_provider_speed_set(&pstIotPtzConfig->m_ptzSpeed);

        event_result->ret = 0;
    }
}

static void anj_ptz_move_status_get(EventResult *event_result, void *data)
{
    if (event_result)
    {
        anj_mutex_lock(&s_stPtzMutex);
        event_result->ret = (s_stPtzParam.ptzWork == PTZ_NONE) ? 0 : 1;
        anj_mutex_unlock(&s_stPtzMutex);
    }
}

int anj_ptz_init(void)
{
    int iRet = -1;
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    ANJ_CHK((0 == s_stPtzInit), iRet, "had been init");
    anj_config_ptz_load();
    if (anj_ptz_provider_available())
    {
        s_stPtzParam.ptzWork = PTZ_RESET;
    }
    else
    {
        s_stPtzParam.ptzWork = PTZ_NONE;
    }
    s_stPtzParam.MaxStep = pstIotPtzConfig->m_MaxStep;
    if (s_stPtzParam.MaxStep.HStep == 0 && s_stPtzParam.MaxStep.VStep == 0)
    {
        s_stPtzParam.MaxStep.HStep = PTZ_H_MAX_STEP;
        s_stPtzParam.MaxStep.VStep = PTZ_V_MAX_STEP;
    }
    if (pstIotPtzConfig->m_ptzSpeed.HSpeed == 0 && pstIotPtzConfig->m_ptzSpeed.VSpeed == 0)
    {
        pstIotPtzConfig->m_ptzSpeed.HSpeed = PTZ_DEFAULT_H_SPEED;
        pstIotPtzConfig->m_ptzSpeed.VSpeed = PTZ_DEFAULT_V_SPEED;
    }
    iRet = anj_ptz_provider_init();
    anj_ptz_track_tune_load();

    anj_sysctl_capability_add(FUNCTION_PTZ_CONTROL);
    anj_sysctl_capability_add(FUNCTION_PTZ_ALL_CTRL);
    anj_sysctl_capability_add(FUNCTION_PTZ_4_DIRECTION);
    if (CUSTOMER_WTD == ANJ_CUSTOMER_TYPE || SUPPORT_ADVANCE_PTZ)
    {
        anj_sysctl_capability_add(FUNCTION_PTZ_CRUISE);
        anj_sysctl_capability_add(FUUNCTION_PTZ_AB_SCAN);
        anj_sysctl_capability_add(FUNCTION_AF_PROTOCOL_5);
        if (CUSTOMER_WTD == ANJ_CUSTOMER_TYPE)
        {
            anj_sysctl_capability_add(FUNCTION_AF_PROTOCOL_4);
        }
        anj_sysctl_capability_add(FUNCTION_PT_3D);
    }

    s_stPtzThread.bAutoDestroy = 1;
    strncpy(s_stPtzThread.iThreadName, "ptz_thread", sizeof(s_stPtzThread.iThreadName) - 1);
    s_stPtzThread.iThreadjob.ctx = &s_stPtzThread;
    s_stPtzThread.iThreadjob.func = anj_ptz_thread;
    iRet = anj_thread_task_create(&s_stPtzThread);

    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_TRACK, anj_ptz_track_set);
    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, anj_ptz_handle);
    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_DIRECTION, anj_ptz_dir_set);
    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_CHANGE_SPEED, anj_ptz_speed_set);
    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_MOVE_STATUS, anj_ptz_move_status_get);

    s_stPtzInit = 1;
endFunc:
    return iRet;
}

int anj_ptz_uninit(void)
{
    int iRet = -1;

    ANJ_CHK((1 == s_stPtzInit), iRet, "not init");
    s_stPtzInit = 0;
    iRet = anj_ptz_provider_uninit();

    anj_thread_task_destroy(&s_stPtzThread, -1);
endFunc:
    return iRet;
}

REGISTER_MODULE(anj_ptz, MODULE_PRIORITY_PTZ);
