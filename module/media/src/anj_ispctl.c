#include "anj_mw_comm.h"
#include "anj_module.h"
#include "eventhub.h"
#include "anj_config.h"
#include "anj_mw_thread.h"
#include "anj_comm.h"
#include "anj_mw_time.h"
#include "anj_mw_mutex.h"
#include "anj_mw_media_isp.h"
#include "anj_mw_hwctrl.h"
#include "anj_ispctl.h"
#include "anj_video.h"
#include "anj_sysmng.h"
#include "anj_smart.h"
#include "anj_net.h"
#include "anj_ser.h"

#define ISPCTL_THREAD_USLEEP (100 * 1000)
#define ISPCTL_THREAD_SECOND (1000 * 1000) / ISPCTL_THREAD_USLEEP
#define DELAY_TIME_LIGHT_ON (3 * ISPCTL_THREAD_SECOND)      // 到达开灯阈值后，等多少s才开灯
#define DELAY_TIME_LIGHT_OFF (15 * ISPCTL_THREAD_SECOND)    // 到达关灯阈值后，等多少s才关灯
#define DELAY_TIME_LOW_BV_ON (3 * ISPCTL_THREAD_SECOND)     // 到达开灯阈值后，等多少s进入低照度模式
#define DELAY_TIME_AIISP_SWITCH (10 * ISPCTL_THREAD_SECOND) // 10S才能切一次AIISP
#define AETARGET_SET_TIME (2.5 * ISPCTL_THREAD_SECOND)      // 间隔2.5s 设置一次ae target

// PWM控制步长时的DUTTY的分割线，大于则使用LIGHT_PWM_DUTY_SET_HIGH,小于则使用LIGHT_PWM_DUTY_SET_LOW，用于避免开关灯来回切
#define LIGHT_PWM_DUTY_CRITIAL (2500) // 在该值以上500一档，以下20一档，从最亮降到该值需要9.5秒，再往下就看关灯延时次数
#define LIGHT_PWM_DUTY_SET_HIGH (500) // PWM大值时的灯光设置步长
#define LIGHT_PWM_DUTY_SET_LOW (20)   // PWM大值时的灯光设置步长

#define IRCUT_SWITCH_TIME (350 * 1000)        // 关灯切IRCUT后再黑白切彩色的延时时间
#define ARGING_SWITCH_TIME (500 * 1000)       // 老化开光灯延时时间

#define ISP_DEFAUT_START_TIMES (3)   /*连续3次有坐标才开始防检测过曝*/

/* gain 告警相关阈值 */
#define ISPCTL_LOWBV_GAIN_TH (128.0f)
#define ISPCTL_AIISP_PD_SENSITIVITY_GAIN_TH (256.0f)
#define ISPCTL_AIISP_PD_SENSITIVITY_SCALE (10.0f)
#define ISPCTL_AIISP_PD_SENSITIVITY_HIGH_GAIN_OFFSET (0.05f)

/* 告警白光资格窗口：1s 内有人检认为当前告警仍有效；2s 无新检测清资格。 */
#define ISPCTL_PD_ALARM_HIT_WINDOW_MS (1000)
#define ISPCTL_PD_ALARM_RESET_WINDOW_MS (2 * 1000)

/* 单次连续亮白光超过 120s 后进入强制关闭逻辑。 */
#define ISPCTL_WHITE_FORCE_CLOSE_MAX_OPEN_MS (120 * 1000)

/* 最近有人移动时间窗口：
 * 30s 用于区分“当前有人但已静止较久”；
 * 5s 用于“无人后重新检测到人”的重新放行等待。
 */
#define ISPCTL_HUMAN_MOVE_RECENT_MS (30 * 1000)
#define ISPCTL_HUMAN_REDETECT_HOLD_MS (5 * 1000)

/* 人形检测触发的暗光增强条件：
 * 这里统一给红外补光、告警白光和 AIISP 回切共用。
 */
#define ISPCTL_PD_DIM_LIGHT_BOOST_WINDOW_MS (8 * 1000)
#define ISPCTL_PD_DIM_LIGHT_BOOST_BV_TH (5000)

/* 低照度告警：10s 后再灭到 0；环境已开灯时不用此路径。 */
#define ISPCTL_WHITE_CLOSE_WINDOW_MS (10 * 1000)
/* 环境开灯后告警收光步长（对齐 old light_open==1 每次 -500）。 */
#define ISPCTL_ALARM_ENV_DIM_STEP (500)

/* 灯控/IRCUT/PQ 由 camera 0 驱动，与线程主循环一致。 */
#define ISPCTL_CONTROL_CAMERA_INDEX (0)

/*
 * 单通道灯控状态机。
 * 将原 process_single_light_channel 拆分为：状态判定 → 状态处理 → 输出转换 三阶段。
 */
typedef enum
{
    ISPCTL_LIGHT_STATE_DARK_OPEN,       /* 黑暗：需要开灯 */
    ISPCTL_LIGHT_STATE_DARK_KEEP,       /* 黑暗：已开灯，保持/渐增 */
    ISPCTL_LIGHT_STATE_MID_BRIGHT,      /* 中间亮度：适度补光 */
    ISPCTL_LIGHT_STATE_CLOSE_COUNTDOWN, /* 亮：亮度低，进入关灯倒计时 */
    ISPCTL_LIGHT_STATE_CLOSE_DIM,       /* 亮：亮度高，渐暗 */
    ISPCTL_LIGHT_STATE_IDLE,            /* 无操作 */
} IspCtlLightState;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
    int target_day;
    int target_night;
} IspDetectRectCfg;

/*
 * 开灯 delay 表：{idx, trigger_value, reserved_value}
 * trigger_value：白光开灯阈值（或 BV 项目共用开灯阈值）
 * reserved_value：GAIN 类型时为红外开灯阈值；BV 类型忽略
 */
typedef struct
{
    int delay_idx;
    int trigger_value;
    int reserved_value;
} IspLightDelayEntry;

static IspLightDelayEntry g_light_delay_table[] = {ISP_LIGHT_DELAY_ENTRY};
static IspDetectRectCfg g_detect_rect_cfgs[] = {ISP_DETECT_RECT_CFG};
static AnjIspCtlInfo s_stAnjIspCtlInfo = {0};
static anj_thread_s s_stIspCtlThread;
static pthread_mutex_t s_AnjIspCtrlInfoMutex = PTHREAD_MUTEX_INITIALIZER;

static int s_stIspctlInit = 0;
static inline IspCtlControlRuntime *anj_ispctl_ctrl_runtime(void)
{
    return &s_stAnjIspCtlInfo.control_runtime;
}

static inline IspCtlCameraRuntime *anj_ispctl_camera_runtime(int cameraIndex)
{
    return &s_stAnjIspCtlInfo.camera_runtime[cameraIndex];
}

static inline IspDetectRuntime *anj_ispctl_detect_runtime(void)
{
    return &s_stAnjIspCtlInfo.detect_runtime;
}

static inline IspCtlWhiteAlarmState *anj_ispctl_white_alarm(void)
{
    return &anj_ispctl_ctrl_runtime()->white_alarm;
}

static inline void anj_ispctl_white_alarm_reset(void)
{
    memset(anj_ispctl_white_alarm(), 0, sizeof(IspCtlWhiteAlarmState));
}

static inline int anj_ispctl_white_alarm_session_active(void)
{
    const IspCtlWhiteAlarmState *wa = anj_ispctl_white_alarm();
    return (wa->session_open_ms > 0 || wa->fade_close_start_ms > 0) ? 1 : 0;
}

static inline void anj_ispctl_white_alarm_force_set(IspCtlWhiteForceState state, unsigned long long sinceMs)
{
    IspCtlWhiteAlarmState *wa = anj_ispctl_white_alarm();
    wa->force_recover = state;
    wa->force_recover_since_ms = sinceMs;
}

static inline int anj_ispctl_check_close_condition(int sensitiveValue, int closeTh, int aeStable)
{
    if (aeStable || ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
    {
        return anj_mw_media_isp_is_brighter(sensitiveValue, closeTh);
    }
    return 0;
}

static inline int anj_ispctl_check_mid_bright_condition(int sensitiveValue, int closeTh, int aeStable)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int midMargin = ISP_LIGHT_MID_MARGIN;
    int midTh;

    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
    {
        midMargin *= 2;
    }

    /* 向“更暗”方向偏移 close 阈值，形成中间补光带 */
    midTh = (control->sensitive.type == ANJ_ISP_SENSITIVE_BV) ? (closeTh - midMargin) : (closeTh + midMargin);
    if (aeStable)
    {
        return anj_mw_media_isp_is_darker(sensitiveValue, midTh);
    }
    return 0;
}

static int anj_ispctl_pd_detect_recent_ms(int cameraIndex, unsigned long long windowMs)
{
    int detected = 0;
    unsigned long long detectTimeMs = 0;
    IspDetectRuntime *detectRuntime = anj_ispctl_detect_runtime();

    if (detectRuntime->camera_index == cameraIndex)
    {
        detectTimeMs = detectRuntime->detect_start_time_ms;
    }

    if (detectTimeMs > 0)
    {
        unsigned long long nowMs = anj_mw_get_cputime_ms(NULL);
        if ((nowMs >= detectTimeMs) && ((nowMs - detectTimeMs) <= windowMs))
        {
            detected = 1;
        }
    }

    return detected;
}

// 将开灯、关灯的等待倒计时恢复为默认值
static void anj_ispctl_delay_time_default()
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    control->light_on_hold_ticks = DELAY_TIME_LIGHT_ON;
    control->light_off_hold_ticks = DELAY_TIME_LIGHT_OFF;
}

// 将开灯、关灯的等待倒计时清零，用于更改配置后马上切灯
void anj_ispctl_delay_time_memset()
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    control->light_on_hold_ticks = 0;
    control->light_off_hold_ticks = 0;
}

static int anj_ispctl_human_move_recent_ms(unsigned long long windowMs)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    unsigned long long nowMs = anj_mw_get_cputime_ms(NULL);
    if (control->last_human_move_time_ms == 0)
    {
        return 0;
    }

    return ((nowMs - control->last_human_move_time_ms) <= windowMs) ? 1 : 0;
}

static void anj_ispctl_low_bv_status_update(float curGain, ISPCTL_PQBIN_MODE curMode, int currentPwm)
{
    IspCtlLowBvState *lowBv = &anj_ispctl_ctrl_runtime()->low_bv_state;
    int gainHigh = DOUBLE_GREATER(curGain, ISPCTL_LOWBV_GAIN_TH) ? 1 : 0;

    // RGB模式下才能支持进入低照模式
    if (curMode == PQBIN_MODE_RGB && lowBv->low_bv == 0)
    {
        if (gainHigh)
        {
            lowBv->low_bv_enter_ticks--;
            if (lowBv->low_bv_enter_ticks <= 0)
            {
                lowBv->low_bv_enter_ticks = DELAY_TIME_LOW_BV_ON;
                lowBv->low_bv = 1;
            }
        }
        else
        {
            lowBv->low_bv_enter_ticks = DELAY_TIME_LOW_BV_ON;
        }

        return;
    }

    if (!gainHigh && (currentPwm == 0))
    {
        lowBv->low_bv_enter_ticks--;
        if (lowBv->low_bv_enter_ticks <= 0)
        {
            lowBv->low_bv_enter_ticks = DELAY_TIME_LOW_BV_ON;
            lowBv->low_bv = 0;
        }
    }
    else
    {
        lowBv->low_bv_enter_ticks = DELAY_TIME_LOW_BV_ON;
    }
}

// 120s关灯后。 
//状态1: 若有人且一直静止大于30s，再动一下 就立马开灯
//状态2: 人离开了，再次检测到人 需判断开关灯间隔大于5s
static IspCtlWhiteAlarmPermission anj_ispctl_white_alarm_force_recover_update(int alarmTrip)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    IspCtlWhiteAlarmState *wa = anj_ispctl_white_alarm();
    unsigned long long nowMs = anj_mw_get_cputime_ms(NULL);
    int staticHuman = alarmTrip && !anj_ispctl_human_move_recent_ms(ISPCTL_HUMAN_MOVE_RECENT_MS);

    if (wa->force_recover == ISPCTL_WHITE_FORCE_STATE_NONE)
    {
        if (staticHuman)
        {
            anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_WAIT_MOTION, nowMs);
        }
        else if (!alarmTrip)
        {
            anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_WAIT_REDETECT, 0);
        }
        else
        {
            wa->session_open_ms = 0;
        }

        return ISPCTL_WHITE_ALARM_BLOCKED;
    }

    if (wa->force_recover == ISPCTL_WHITE_FORCE_STATE_WAIT_MOTION)
    {
        if (!alarmTrip)
        {
            anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_WAIT_REDETECT, 0);
            return ISPCTL_WHITE_ALARM_BLOCKED;
        }

        if ((control->last_human_move_time_ms > 0) &&
            (control->last_human_move_time_ms > wa->force_recover_since_ms))
        {
            wa->session_open_ms = 0;
            anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_NONE, 0);
            return ISPCTL_WHITE_ALARM_ALLOWED;
        }

        return ISPCTL_WHITE_ALARM_BLOCKED;
    }

    if (!alarmTrip)
    {
        wa->force_recover_since_ms = 0;
        return ISPCTL_WHITE_ALARM_BLOCKED;
    }

    if (wa->force_recover_since_ms == 0)
    {
        wa->force_recover_since_ms = nowMs;
        return ISPCTL_WHITE_ALARM_BLOCKED;
    }

    if ((nowMs - wa->force_recover_since_ms) >= ISPCTL_HUMAN_REDETECT_HOLD_MS)
    {
        wa->session_open_ms = 0;
        anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_NONE, 0);
        return ISPCTL_WHITE_ALARM_ALLOWED;
    }
    return ISPCTL_WHITE_ALARM_BLOCKED;
}

static int anj_ispctl_alarm_white_pd_pwm(int led_brightness_mode)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();

    if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV || ANJ_PROJECT_TYPE == PROJECT_TYPE_LP || led_brightness_mode == LED_BRIGHTNESS_MODE_AUTO)
    {
        return control->pwm_max;
    }

    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
    {
        return LIGHT_PWM_MAX_VALUE * 6 / 10;
    }

    return LIGHT_PWM_MAX_VALUE;
}

/*
 * 每帧更新：低照度标记 + 1s 人形资格 + 120s 会话 + 强制恢复子状态机。
 * dualNightAssist：双光夜态/会话中，不依赖白光的 RGB low_bv 也可进入告警资格。
 * 返回本帧是否允许拉高告警白光。
 */
static IspCtlWhiteAlarmPermission anj_ispctl_white_alarm_permission_update(int BV, int baseWhitePwm,
                                                                           int dualNightAssist)
{
    IspCtlLowBvState *lowBv = &anj_ispctl_ctrl_runtime()->low_bv_state;
    IspCtlWhiteAlarmState *wa = anj_ispctl_white_alarm();
    unsigned long long nowMs = anj_mw_get_cputime_ms(NULL);
    int alarmTrip = anj_ispctl_pd_detect_recent_ms(ISPCTL_CONTROL_CAMERA_INDEX, ISPCTL_PD_ALARM_HIT_WINDOW_MS);
    int allowWhiteAlarm = 0;
    int allowGate = lowBv->low_bv || (baseWhitePwm > 0) || dualNightAssist;
    int brightBlock = lowBv->low_bv || dualNightAssist;

    /* 低照、base 已开白光、或双光夜态/会话：进入人形告警资格判断 */
    if (allowGate)
    {
        if (alarmTrip)
        {
            wa->pd_hit_ms = nowMs;
            allowWhiteAlarm = 1;
        }
        else if (wa->pd_hit_ms > 0 && (nowMs - wa->pd_hit_ms) >= ISPCTL_PD_ALARM_RESET_WINDOW_MS)
        {
            wa->pd_hit_ms = 0;
            wa->session_open_ms = 0;
            // 2s无人告警灯维持10s才关
            wa->fade_close_start_ms = nowMs;
            anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_NONE, 0);
        }

        if (allowWhiteAlarm && brightBlock && (BV > ISPCTL_PD_DIM_LIGHT_BOOST_BV_TH))
        {
            if (wa->session_open_ms > 0)
            {
                wa->pd_hit_ms = 0;
                wa->session_open_ms = 0;
                // 2s无人告警灯维持10s才关
                wa->fade_close_start_ms = nowMs;
                anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_NONE, 0);
            }
            allowWhiteAlarm = 0;
        }

        if (wa->session_open_ms > 0 && (nowMs - wa->session_open_ms) > ISPCTL_WHITE_FORCE_CLOSE_MAX_OPEN_MS)
        {
            return anj_ispctl_white_alarm_force_recover_update(alarmTrip);
        }

        if (allowWhiteAlarm)
        {
            if (wa->session_open_ms == 0)
            {
                wa->session_open_ms = nowMs;
            }
            wa->fade_close_start_ms = 0;
        }
    }

    return allowWhiteAlarm ? ISPCTL_WHITE_ALARM_ALLOWED : ISPCTL_WHITE_ALARM_BLOCKED;
}

static void anj_ispctl_get_open_ths(int delay, int *whiteOpenTh, int *redOpenTh)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int delay_idx = delay;
    int white_open;
    int red_open;
    IspLightDelayEntry *delay_entry;

    if (delay_idx < 0 || delay_idx > 17)
    {
        delay_idx = 0;
    }

    delay_entry = &(g_light_delay_table[delay_idx]);
    white_open = delay_entry->trigger_value;
    if (control->sensitive.type == ANJ_ISP_SENSITIVE_BV)
    {
        /* BV 可叠加 debug open_offset；白/红外共用开灯阈值 */
        white_open += control->light_threshold.open_offset;
        red_open = white_open;
    }
    else
    {
        /* GAIN：reserved 为红外开灯档 */
        red_open = delay_entry->reserved_value;
    }

    if (whiteOpenTh)
    {
        *whiteOpenTh = white_open;
    }
    if (redOpenTh)
    {
        *redOpenTh = red_open;
    }
}

static int anj_ispctl_calc_close_th(int openTh, int light_off_sensitivity, int closeDiffMax)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    float diff = (float)ISP_LIGHT_CLOSE_MIN_DIFF +
                 (100 - light_off_sensitivity) / 100.0f * (float)closeDiffMax;
    float tmpvalue;

    if (control->sensitive.type == ANJ_ISP_SENSITIVE_BV)
    {
        tmpvalue = (float)openTh + diff;
        if ((ISP_LIGHT_CLOSE_MAX_LIMIT > 0) && (tmpvalue > (float)ISP_LIGHT_CLOSE_MAX_LIMIT))
        {
            __ERR("Close th %d will not turn off light. so set it %d\n", (int)tmpvalue,
                  ISP_LIGHT_CLOSE_MAX_LIMIT);
            tmpvalue = (float)ISP_LIGHT_CLOSE_MAX_LIMIT;
        }
    }
    else
    {
        tmpvalue = (float)openTh - diff;
        if (tmpvalue < (float)ISP_LIGHT_CLOSE_VALUE_MIN)
        {
            tmpvalue = (float)ISP_LIGHT_CLOSE_VALUE_MIN;
        }
    }

    return (int)(tmpvalue + (tmpvalue >= 0 ? 0.5f : -0.5f));
}

/* is_white: 1=白光, 0=红外 */
static int anj_ispctl_get_close_th(int is_white, int light_off_sensitivity)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int openTh = is_white ? control->light_threshold.white_open_th : control->light_threshold.red_open_th;
    int closeDiffMax = is_white ? control->light_threshold.white_close_diff_max
                                : control->light_threshold.red_close_diff_max;

    return anj_ispctl_calc_close_th(openTh, light_off_sensitivity, closeDiffMax);
}

/* BV 类型 debug：按 open+回差 解析关灯阈值；GAIN 类型直接写绝对值，不走此函数 */
static void anj_ispctl_debug_apply_close_th(int nCloseValue, int openTh, int minCloseGap, int sens,
                                           int *closeTh, int *diffMax)
{
    int diff;

    if ((nCloseValue - openTh) < minCloseGap)
    {
        *closeTh = openTh + minCloseGap;
        return;
    }

    diff = nCloseValue - openTh;
    *diffMax = (sens < 100) ? (((diff - minCloseGap) * 100) / (100 - sens)) : (diff - minCloseGap);
    *closeTh = nCloseValue;
}

static char *anj_ispctl_pqbin_get(ISPCTL_PQBIN_MODE pqbin_mode)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig[0].videoCapture;

    if (control->aiisp_enabled && (strlen(ISP_AI_BIN_PATH) > 0))
    {
        if ((pqbin_mode == PQBIN_MODE_MONO) && (strlen(ISP_AI_IR_BIN_PATH) > 0))
        {
            return ISP_AI_IR_BIN_PATH;
        }
        else
        {
            return ISP_AI_BIN_PATH;
        }
    }
    else if (pVideoCapCfg->wdr_mode && strlen(ISP_WDR_BIN_PATH) > 0)
    {
        return ISP_WDR_BIN_PATH;
    }
    else
    {
        if (pqbin_mode == PQBIN_MODE_MONO)
        {
            return ISP_NIGHT_BIN_PATH;
        }
        else
        {
            return ISP_DAY_BIN_PATH;
        }
    }
}

static void anj_ispctl_load_debug(void)
{
    const char *debug_paths[] = {
        "/tmp/light.debug.xml",
        "/mnt/nand/light.debug.xml",
        "/opt/ch/light.debug.xml",
    };
    const char *debug_file = NULL;
    char *xml_buf = NULL;
    IXML_Document *pDocNode = NULL;
    IXML_NodeList *pNodelist = NULL;
    int n_open = -1;
    int n_close_red = -1;
    int n_close_white = -1;

    for (unsigned int i = 0; i < sizeof(debug_paths) / sizeof(debug_paths[0]); i++)
    {
        if (anj_mw_file_exists(debug_paths[i]))
        {
            debug_file = debug_paths[i];
            break;
        }
    }

    if (debug_file == NULL)
    {
        return;
    }

    xml_buf = anj_mw_read_file_buffer(debug_file);
    if (xml_buf == NULL)
    {
        __ERR("read failed: %s\n", debug_file);
        return;
    }

    pDocNode = ixmlParseBuffer(xml_buf);
    if (pDocNode == NULL)
    {
        __ERR("ixmlParseBuffer error: %s\n", debug_file);
        anj_mw_free(xml_buf);
        return;
    }

    pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "light");
    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        IXML_Node *tmpAttr = NULL;
        if (pNode == NULL)
        {
            __ERR("light node is null: %s\n", debug_file);
            ixmlNodeList_free(pNodelist);
            ixmlDocument_free(pDocNode);
            anj_mw_free(xml_buf);
            return;
        }

        tmpAttr = pNode->firstAttr;

        while (tmpAttr != NULL)
        {
            if (tmpAttr->nodeValue == NULL)
            {
                tmpAttr = tmpAttr->nextSibling;
                continue;
            }

            if (strcmp(tmpAttr->nodeName, "open") == 0)
            {
                n_open = atoi(tmpAttr->nodeValue);
            }
            else if (strcmp(tmpAttr->nodeName, "close") == 0)
            {
                n_close_red = atoi(tmpAttr->nodeValue);
            }
            else if (strcmp(tmpAttr->nodeName, "closewhite") == 0)
            {
                n_close_white = atoi(tmpAttr->nodeValue);
            }

            tmpAttr = tmpAttr->nextSibling;
        }
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(light) return NULL: %s\n", debug_file);
        ixmlDocument_free(pDocNode);
        anj_mw_free(xml_buf);
        return;
    }

    ixmlNodeList_free(pNodelist);
    ixmlDocument_free(pDocNode);
    anj_mw_free(xml_buf);

    if (n_open == -1 || n_close_red == -1)
    {
        __ERR("Get light debug cfg open/close error: %s\n", debug_file);
        return;
    }

    if (n_close_white == -1)
    {
        n_close_white = n_close_red;
    }

    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pDefaultCapture = &pMediacfg->videoConfig[ISPCTL_CONTROL_CAMERA_INDEX].videoCapture;
    int minCloseGap = ISP_LIGHT_CLOSE_MIN_DIFF;
    int sens = pDefaultCapture->light_off_sensitivity;

    control->light_threshold.white_open_th = n_open;
    control->light_threshold.red_open_th = n_open;

    if (control->sensitive.type != ANJ_ISP_SENSITIVE_BV)
    {
        control->light_threshold.open_offset = 0;
        control->light_threshold.red_close_th = n_close_red;
        control->light_threshold.white_close_th = n_close_white;
        __INFO("debug light open trigger=%d (gain), close red=%d white=%d\n",
               n_open, n_close_red, n_close_white);
        return;
    }

    control->light_threshold.open_offset =
        n_open - g_light_delay_table[pDefaultCapture->ircut_openled_delay].trigger_value;
    __INFO("debug light open trigger=%d, delay_idx=%d, offset=%d\n",
           n_open, pDefaultCapture->ircut_openled_delay, control->light_threshold.open_offset);

    anj_ispctl_debug_apply_close_th(n_close_red, control->light_threshold.red_open_th, minCloseGap, sens,
                                    &control->light_threshold.red_close_th,
                                    &control->light_threshold.red_close_diff_max);
    if (n_close_red != n_close_white)
    {
        anj_ispctl_debug_apply_close_th(n_close_white, control->light_threshold.white_open_th, minCloseGap, sens,
                                        &control->light_threshold.white_close_th,
                                        &control->light_threshold.white_close_diff_max);
    }
    else
    {
        control->light_threshold.white_close_diff_max = control->light_threshold.red_close_diff_max;
        control->light_threshold.white_close_th = control->light_threshold.red_close_th;
    }
}

static void anj_ispctl_light_thr_init(VideoCaptureCfg *pVideoCapCfg)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    AnjIspAeInfo aeInfo = {0};

    anj_mw_media_isp_aeinfo_get(ISPCTL_CONTROL_CAMERA_INDEX, &aeInfo);
    anj_mw_media_isp_sensitive_get(&control->sensitive, &aeInfo);

    control->light_threshold.white_close_diff_max = ISP_LIGHT_CLOSE_MAX_DIFF_WHITE;
    control->light_threshold.red_close_diff_max = ISP_LIGHT_CLOSE_MAX_DIFF_RED;
    control->light_threshold.open_offset = 0;

    anj_ispctl_load_debug();

    anj_ispctl_get_open_ths(pVideoCapCfg->ircut_openled_delay,
                            &control->light_threshold.white_open_th,
                            &control->light_threshold.red_open_th);
    control->light_threshold.red_close_th = anj_ispctl_get_close_th(0, pVideoCapCfg->light_off_sensitivity);
    control->light_threshold.white_close_th = anj_ispctl_get_close_th(1, pVideoCapCfg->light_off_sensitivity);

    __INFO("Open white=%d red=%d, delay_idx=%d, light_off_sensitivity=%d\n",
           control->light_threshold.white_open_th, control->light_threshold.red_open_th,
           pVideoCapCfg->ircut_openled_delay, pVideoCapCfg->light_off_sensitivity);
    __INFO("Close red=%d (diff %d), white=%d (diff %d)\n",
           control->light_threshold.red_close_th,
           control->light_threshold.red_close_th - control->light_threshold.red_open_th,
           control->light_threshold.white_close_th,
           control->light_threshold.white_close_th - control->light_threshold.white_open_th);
}

static void anj_ispctl_maxshutter_set(int iCamerIndex, int FrameRate, ISPCTL_PQBIN_MODE pqbin_mode, VideoCaptureCfg *pVideoCapCfg)
{
#if _SUPPORT_IQTOOL_
    return;
#endif

    int MaxShutterUS = 0;
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    VideoEncodeCfg *pEncodeCfg = &pMediacfg->videoConfig[iCamerIndex].videoEncode.encodeCfg[0];
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();

    if (VIDEO_WDR_MODE_HDR == pVideoCapCfg->wdr_mode)
        return;

    if ((FrameRate == 0) || (pVideoCapCfg->videoEncodeMode == VIDEO_ISP_MODE_FORCE_FRAMERATE))
    {
        FrameRate = pEncodeCfg->frameRate;
    }

    if ((LED_BRIGHTNESS_MODE_EXT_3 == pVideoCapCfg->led_brightness_mode) &&
        (pVideoCapCfg->videoEncodeMode == VIDEO_ISP_MODE_LOW_POWER) &&
        (ANJ_PROJECT_TYPE == PROJECT_TYPE_NORMAL))
    {
        if (FrameRate > 25)
        {
            FrameRate = pEncodeCfg->frameRate;
        }

        if (0 == anj_mw_media_isp_fps_set(iCamerIndex, FrameRate))
        {
            anj_video_adjust_gop(iCamerIndex, FrameRate);
        }
    }
    if (control->aiisp_enabled == 0)
    {
        // 快门值实际上是分母，值越大快门越小
        int nMinShutterValue = FrameRate; // 快门分母最小值

        if (pqbin_mode == PQBIN_MODE_RGB && pVideoCapCfg->shutterSetting.shutter_mode_day == 1)
        {
            nMinShutterValue = pVideoCapCfg->shutterSetting.shutter_speed_day;
        }
        else if (pqbin_mode == PQBIN_MODE_MONO && pVideoCapCfg->shutterSetting.shutter_mode_night == 1)
        {
            nMinShutterValue = pVideoCapCfg->shutterSetting.shutter_speed_night;
        }

        MaxShutterUS = 1000000 / nMinShutterValue;
        anj_mw_media_isp_shutterus_set(iCamerIndex, 0, MaxShutterUS);
    }
}

static int anj_ispctl_aetarget_set(int iCamerIndex)
{
#if _SUPPORT_IQTOOL_
    return 0 ;
#endif
    IspCtlCameraRuntime *cameraRuntime = anj_ispctl_camera_runtime(iCamerIndex);
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int iRet = 0;
    int bSet = 0;
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig[iCamerIndex].videoCapture;
    unsigned int curTargetY[16] = {0};

    anj_mw_media_isp_aetarget_get(iCamerIndex, curTargetY);
    if (control->aiisp_enabled && ISP_DEFAUT_AE_TARGET != 0)
    {
        for (int i = 0; i < sizeof(curTargetY) / sizeof(curTargetY[0]); i++)
        {
            curTargetY[i] = ISP_DEFAUT_AE_TARGET;
        }
        bSet = 1;
    }
    else if (pVideoCapCfg->ispadvmode == LED_IMAGE_FACE_EXPOSURE_PREVENTION)
    {
        memcpy(curTargetY, cameraRuntime->ae_target_y, sizeof(curTargetY));
        bSet = 1;
    }
    if (bSet)
        iRet = anj_mw_media_isp_aetarget_set(iCamerIndex, curTargetY);
    return iRet;
}

static void anj_ispctl_load_pq(ISPCTL_PQBIN_MODE pqbin_mode)
{
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        IspCtlCameraRuntime *cameraRuntime = anj_ispctl_camera_runtime(i);
        VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig[i].videoCapture;
        anj_mw_media_isp_load(i, anj_ispctl_pqbin_get(pqbin_mode), &s_stIspCtlThread.start);
        anj_mw_media_isp_aetarget_get(i, cameraRuntime->ae_target_y);
        anj_mw_media_isp_saturation_get(i, cameraRuntime->saturation_cfg);
        anj_mw_media_isp_sharpness_get(i, cameraRuntime->sharpness_ud0_cfg,
                                       cameraRuntime->sharpness_ud1_cfg, cameraRuntime->sharpness_ud2_cfg);
        anj_mw_media_isp_backlight_get(i, cameraRuntime->blc_cfg);
        anj_mw_media_isp_wdr_value_get(i, cameraRuntime->wdr_cfg);
        anj_mw_media_isp_2dnr_get(i, cameraRuntime->nr_2d_cfg);
        anj_mw_media_isp_3dnr_get(i, cameraRuntime->nr_3d_cfg);
        anj_ispctl_maxshutter_set(i, 2, pqbin_mode, pVideoCapCfg);
        anj_ispctl_aetarget_set(i);
    }
}

/*
 * 自动亮度模式下使用同一套 PWM 步进策略：
 * 高亮区快速收放光，低亮区细步进，避免阈值附近闪烁。
 */
static int anj_ispctl_step_pwm_duty(int add, int current_duty, int pwm_max)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int pwm_duty = current_duty;
    if (add)
    {
        if (current_duty > LIGHT_PWM_DUTY_CRITIAL)
        {
            pwm_duty = pwm_duty + LIGHT_PWM_DUTY_SET_HIGH;
        }
        else
        {
            pwm_duty = pwm_duty + LIGHT_PWM_DUTY_SET_LOW;
        }
        if (pwm_duty > pwm_max)
        {
            pwm_duty = pwm_max;
        }
    }
    else
    {
        if (current_duty > LIGHT_PWM_DUTY_CRITIAL)
        {
            pwm_duty = pwm_duty - LIGHT_PWM_DUTY_SET_HIGH;
        }
        else
        {
            pwm_duty = pwm_duty - LIGHT_PWM_DUTY_SET_LOW;
        }

        if (pwm_duty < control->pwm_min)
        {
            pwm_duty = control->pwm_min;
        }
    }

    __DBG("anj_ispctl_step_pwm_duty pwm_duty=%d, add=%d, pwm_min=%d, pwm_max=%d,current_duty=%d\n",
          pwm_duty, add, control->pwm_min, pwm_max, current_duty);
    return pwm_duty;
}

void anj_ispctl_pqmode_manual_ctrl(ISPCTL_PQBIN_MODE Mode, int cameraIndex)
{
    if (Mode == PQBIN_MODE_MONO)
    {
        anj_mw_media_isp_load(cameraIndex, anj_ispctl_pqbin_get(Mode), &s_stIspCtlThread.start);
        usleep(IRCUT_SWITCH_TIME);
        anj_mw_hwctrl_ircut_set_night();
    }
    else
    {
        anj_mw_hwctrl_ircut_set_day();
        usleep(IRCUT_SWITCH_TIME);
        anj_mw_media_isp_load(cameraIndex, anj_ispctl_pqbin_get(Mode), &s_stIspCtlThread.start);
    }
}

void anj_ispctl_light_init(int period, int duty_cycle)
{
    int i = 0;
    for (i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        anj_mw_hwctrl_pwm_light_init(i, period, duty_cycle);
    }
}

void anj_ispctl_light_uninit()
{
    int i = 0;
    for (i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        anj_mw_hwctrl_pwm_light_uninit(i);
    }
}

void anj_ispctl_wlight_set(int value)
{
    int i = 0;
    // value = value * 666 / 500;
    for (i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        anj_mw_hwctrl_pwm_wlight_set(i, value);
    }
}

void anj_ispctl_rlight_set(int value)
{
    int i = 0;
    // value = value * 666 / 500;
    for (i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        anj_mw_hwctrl_pwm_rlight_set(i, value);
    }
}

static void anj_ispctl_set_pwm_layer(IspCtlPwmLayerState *layer, int whitePwmTarget, int redPwmTarget)
{
    layer->white_pwm = whitePwmTarget;
    layer->red_pwm = redPwmTarget;
}

static void anj_ispctl_clear_pwm_layer(IspCtlPwmLayerState *layer)
{
    anj_ispctl_set_pwm_layer(layer, 0, 0);
}

static void anj_ispctl_set_pwm_targets(int whitePwmTarget, int redPwmTarget)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    control->white_pwm_target = whitePwmTarget;
    control->red_pwm_target = redPwmTarget;
}

static void anj_ispctl_pwm_layer_init_from_target(IspCtlPwmLayerState *layer)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();

    layer->white_pwm = control->white_pwm_target;
    layer->red_pwm = control->red_pwm_target;
    layer->pq_mode = PQBIN_MODE_NONE;
}

static void anj_ispctl_alarm_layer_reset(IspCtlPwmLayerState *alarm)
{
    alarm->white_pwm = 0;
    alarm->red_pwm = 0;
    alarm->pq_mode = PQBIN_MODE_NONE;
}

static int anj_ispctl_alarm_white_default_pwm(int led_brightness_mode)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int defaultPwm = control->pwm_max;

    if (led_brightness_mode == LED_BRIGHTNESS_MODE_AUTO)
    {
        if (defaultPwm > LIGHT_PWM_MAX_VALUE)
        {
            defaultPwm = LIGHT_PWM_MAX_VALUE;
        }
    }

    if (defaultPwm < control->pwm_min)
    {
        defaultPwm = control->pwm_min;
    }

    return defaultPwm;
}

/*
 * 判断是否需要强制全亮（红外已开 / 手动模式 / 无会话冷启动）。
 * base 已开灯时的暗光提亮见 env_follow（需 aeStable）。
 */
static int anj_ispctl_alarm_white_need_force_full(int currentWhitePwm, int currentRedPwm, int led_brightness_mode)
{
    if (currentRedPwm > 0 || currentWhitePwm == 0 ||
        led_brightness_mode == LED_BRIGHTNESS_MODE_MANUAL ||
        anj_ispctl_white_alarm_session_active() == 0)
    {
        return 1;
    }
    return 0;
}

/*
 * 环境灯已开时的跟光/收光计算（暗光增强路径）。
 */
static int anj_ispctl_alarm_white_env_follow(int BV, int aeStable, int currentWhitePwm,
                                             int defaultPwm, int pdPwm)
{
    int nextPwm = currentWhitePwm;

    if (BV < 0)
    {
        if (currentWhitePwm <= defaultPwm)
        {
            if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || aeStable)
                nextPwm = pdPwm;
        }
    }
    else if (currentWhitePwm > 0)
    {
        if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD || aeStable)
            nextPwm = anj_ispctl_step_pwm_duty(0, currentWhitePwm, defaultPwm);
    }
    return nextPwm;
}

/*
 * 环境灯已开且当前亮度高于默认值时的收光计算。
 */
static int anj_ispctl_alarm_white_env_dim(int sensitiveValue, int aeStable, int currentWhitePwm, int defaultPwm,
                                          int whiteCloseTh, IspCtlWhiteAlarmState *wa)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int nextPwm = currentWhitePwm;

    if (currentWhitePwm > (defaultPwm + ISPCTL_ALARM_ENV_DIM_STEP))
    {
        nextPwm = currentWhitePwm - ISPCTL_ALARM_ENV_DIM_STEP;
    }
    else if (anj_ispctl_check_close_condition(sensitiveValue, whiteCloseTh, aeStable))
    {
        nextPwm = anj_ispctl_step_pwm_duty(0, currentWhitePwm, defaultPwm);
    }
    else
    {
        nextPwm = defaultPwm;
    }

    if (nextPwm <= defaultPwm)
    {
        wa->session_open_ms = 0;
        wa->fade_close_start_ms = 0;
    }

    if (nextPwm < control->pwm_min)
    {
        nextPwm = control->pwm_min;
    }
    return nextPwm;
}

/*
 * 普通路径：无环境灯跟光时的步进收光/补光。
 */
static int anj_ispctl_alarm_white_normal_step(int sensitiveValue, int aeStable, int currentWhitePwm, int defaultPwm,
                                               int whiteCloseTh)
{
    int nextPwm = currentWhitePwm;

    if (currentWhitePwm > (defaultPwm + LIGHT_PWM_DUTY_SET_HIGH))
    {
        /* 当前亮度已经明显高于常态亮度时，优先往默认亮度回落，而不是继续提亮。 */
        nextPwm = anj_ispctl_step_pwm_duty(0, currentWhitePwm, defaultPwm);
    }
    else
    {
        if (anj_ispctl_check_close_condition(sensitiveValue, whiteCloseTh, aeStable))
        {
            nextPwm = anj_ispctl_step_pwm_duty(0, currentWhitePwm, defaultPwm);
        }
        else if (anj_ispctl_check_mid_bright_condition(sensitiveValue, whiteCloseTh, aeStable))
        {
            nextPwm = anj_ispctl_step_pwm_duty(1, currentWhitePwm, defaultPwm);
        }
    }

    return nextPwm;
}

/*
 * 有资格时计算告警白光 PWM（含低照度全亮、强制全亮、环境跟光/收光）。
 */
static int anj_ispctl_alarm_white_pwm_calc(int sensitiveValue, int BV, int aeStable, int currentWhitePwm,
                                           int currentRedPwm, int led_brightness_mode, int baseWhitePwm, int ircut_mode)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    IspCtlWhiteAlarmState *wa = anj_ispctl_white_alarm();
    int defaultPwm = anj_ispctl_alarm_white_default_pwm(led_brightness_mode);
    int pdPwm = anj_ispctl_alarm_white_pd_pwm(led_brightness_mode);
    int nextPwm = currentWhitePwm;

    if (control->pwm_max == 0)
    {
        return 0;
    }

    if (anj_ispctl_alarm_white_need_force_full(currentWhitePwm, currentRedPwm, led_brightness_mode))
    {
        return pdPwm;
    }

    // 软光敏的告警灯才收光
    if (ircut_mode != IRCUT_Mode_Active)
    {
        return pdPwm;
    }

    if (led_brightness_mode == LED_BRIGHTNESS_MODE_MANUAL)
    {
        return nextPwm;
    }

    /* base 已开灯：按 BV 跟人形窗口跟光/收光（与是否 low_bv 无关）。 */
    if ((baseWhitePwm > 0) &&
        anj_ispctl_pd_detect_recent_ms(ISPCTL_CONTROL_CAMERA_INDEX, ISPCTL_PD_DIM_LIGHT_BOOST_WINDOW_MS))
    {
        return anj_ispctl_alarm_white_env_follow(BV, aeStable, currentWhitePwm, defaultPwm, pdPwm);
    }

    if ((baseWhitePwm > 0) && (currentWhitePwm > defaultPwm))
    {
        return anj_ispctl_alarm_white_env_dim(sensitiveValue, aeStable, currentWhitePwm, defaultPwm,
                                              control->light_threshold.white_close_th, wa);
    }

    return anj_ispctl_alarm_white_normal_step(sensitiveValue, aeStable, currentWhitePwm, defaultPwm,
                                              control->light_threshold.white_close_th);
}

static void anj_ispctl_alarm_apply_white_pwm(IspCtlPwmLayerState *alarm, int sensitiveValue, int BV, int aeStable,
                                             int hwWhitePwm, int hwRedPwm, int led_brightness_mode, int baseWhitePwm,
                                             int ircut_mode)
{
    int calcWhitePwm = anj_ispctl_alarm_white_pwm_calc(sensitiveValue, BV, aeStable, hwWhitePwm, hwRedPwm,
                                                       led_brightness_mode, baseWhitePwm, ircut_mode);

    if (calcWhitePwm <= 0)
    {
        alarm->white_pwm = 0;
        return;
    }

    if (baseWhitePwm > 0 && calcWhitePwm <= baseWhitePwm)
    {
        alarm->white_pwm = 0;
        return;
    }

    alarm->white_pwm = calcWhitePwm;
}

/*
 * 被动模式、定时模式、常亮/常灭这类“固定夜态”流程统一走这里，
 * 只根据目标日夜态和灯模式决定本轮的目标白光/红外 PWM。
 */
static void anj_ispctl_apply_fixed_mode_targets_on(IspCtlPwmLayerState *layer, ISPCTL_PQBIN_MODE pqMode, LedMode led_mode)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();

    if (pqMode != PQBIN_MODE_MONO)
    {
        anj_ispctl_clear_pwm_layer(layer);
        return;
    }

    if (led_mode == LED_PURE_WHITE)
    {
        /* 单白光：夜态开白光，保持彩色由 pqmode_process 保证。 */
        anj_ispctl_set_pwm_layer(layer, control->pwm_max, 0);
    }
    else
    {
        /* 单红外 / 双光：夜态开红外；双光有人切白光由 alarm 层覆盖。 */
        anj_ispctl_set_pwm_layer(layer, 0, control->pwm_max);
    }
}

void anj_ispctl_pqmode_process(ISPCTL_PQBIN_MODE curMode, LedMode led_mode)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    // __INFO("PWM_W %d  PWM_R %d\n", control->white_pwm_target, control->red_pwm_target);
    if (curMode == PQBIN_MODE_MONO && led_mode != LED_PURE_WHITE)
    {
        anj_ispctl_load_pq(curMode);
        usleep(IRCUT_SWITCH_TIME);
        anj_ispctl_wlight_set(control->white_pwm_target);
        anj_ispctl_rlight_set(control->red_pwm_target);
        anj_mw_hwctrl_ircut_set_night();
        control->mLoopDecision.apply_pq_mode = PQBIN_MODE_MONO;
        control->pqbin_mode = PQBIN_MODE_MONO;
        __INFO("PQBIN_MODE_MONO\n");
    }
    else if (curMode == PQBIN_MODE_RGB)
    {
        anj_ispctl_wlight_set(control->white_pwm_target);
        anj_ispctl_rlight_set(control->red_pwm_target);
        anj_mw_hwctrl_ircut_set_day();
        usleep(IRCUT_SWITCH_TIME);
        anj_ispctl_load_pq(curMode);
        control->mLoopDecision.apply_pq_mode = PQBIN_MODE_RGB;
        control->pqbin_mode = PQBIN_MODE_RGB;
        __INFO("PQBIN_MODE_RGB\n");
    }
    else
    {
        if (anj_mw_hwctrl_pwm_rlight_get(ISPCTL_CONTROL_CAMERA_INDEX) != control->red_pwm_target)
        {
            anj_ispctl_rlight_set(control->red_pwm_target);
        }
        if (anj_mw_hwctrl_pwm_wlight_get(ISPCTL_CONTROL_CAMERA_INDEX) != control->white_pwm_target)
        {
            anj_ispctl_wlight_set(control->white_pwm_target);
        }
    }
}

static ISPCTL_PQBIN_MODE anj_ispctl_time_process(DayTimeSpan *ircut_nighttime)
{
    struct tm ptm;
    SystemLocalTime(&ptm);

    char now_time_str[32] = {0};
    char start_time_str[32] = {0};
    char end_time_str[32] = {0};

    snprintf(now_time_str, sizeof(now_time_str), "%02d:%02d:%02d",
             ptm.tm_hour,
             ptm.tm_min,
             ptm.tm_sec);

    snprintf(start_time_str, sizeof(start_time_str), "%02d:%02d:%02d",
             ircut_nighttime->startTime.hour,
             ircut_nighttime->startTime.minute,
             ircut_nighttime->startTime.sec);

    snprintf(end_time_str, sizeof(end_time_str), "%02d:%02d:%02d",
             ircut_nighttime->endTime.hour,
             ircut_nighttime->endTime.minute,
             ircut_nighttime->endTime.sec);

    ISPCTL_PQBIN_MODE nighttime = PQBIN_MODE_RGB;
    if (strcmp(start_time_str, end_time_str) > 0)
    {
        if (strcmp(now_time_str, start_time_str) > 0 || strcmp(now_time_str, end_time_str) < 0)
            nighttime = PQBIN_MODE_MONO;
    }
    else if (strcmp(start_time_str, end_time_str) == 0)
    {
        nighttime = PQBIN_MODE_MONO;
    }
    else
    {
        if (strcmp(now_time_str, start_time_str) > 0 && strcmp(now_time_str, end_time_str) < 0)
            nighttime = PQBIN_MODE_MONO;
    }
    return nighttime;
}

static ISPCTL_PQBIN_MODE anj_ispctl_passive_process()
{
    return anj_mw_hwctrl_photo_sensor_get() ? PQBIN_MODE_RGB : PQBIN_MODE_MONO;
}

static ISPCTL_PQBIN_MODE anj_ispctl_manual_always_process(int light_on)
{
    return light_on ? PQBIN_MODE_MONO : PQBIN_MODE_RGB;
}

static ISPCTL_PQBIN_MODE anj_ispctl_aging_process(ISPCTL_PQBIN_MODE curMode)
{
    ISPCTL_PQBIN_MODE pqbin_mode = PQBIN_MODE_NONE;
    if (curMode != PQBIN_MODE_RGB)
    {
        pqbin_mode = PQBIN_MODE_RGB;
    }
    else
    {
        pqbin_mode = PQBIN_MODE_MONO;
    }
    usleep(ARGING_SWITCH_TIME);
    return pqbin_mode;
}

/* 状态判定：根据光敏度量与开/关阈值确定当前所处状态 */
static IspCtlLightState anj_ispctl_light_state_judge(int sensitiveValue, int openTh, int currentPwm, int closeTh,
                                                     int aeStable)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    if (anj_mw_media_isp_is_darker(sensitiveValue, openTh))
    {
        return (currentPwm == 0) ? ISPCTL_LIGHT_STATE_DARK_OPEN : ISPCTL_LIGHT_STATE_DARK_KEEP;
    }
    else if (anj_ispctl_check_mid_bright_condition(sensitiveValue, closeTh, aeStable))
    {
        return ISPCTL_LIGHT_STATE_MID_BRIGHT;
    }
    else if (anj_ispctl_check_close_condition(sensitiveValue, closeTh, aeStable))
    {
        if (currentPwm > 0 && currentPwm <= control->pwm_min)
            return ISPCTL_LIGHT_STATE_CLOSE_COUNTDOWN;
        else if (currentPwm > control->pwm_min)
            return ISPCTL_LIGHT_STATE_CLOSE_DIM;
        return ISPCTL_LIGHT_STATE_IDLE;
    }
    else
    {
        return ISPCTL_LIGHT_STATE_IDLE;
    }
}

/* 处理开灯倒计时逻辑 */
static void anj_ispctl_light_state_handle_dark_open(IspCtlControlRuntime *control, int *targetPwm)
{
    control->light_on_hold_ticks--;
    if (control->light_on_hold_ticks <= 0)
    {
        *targetPwm = control->pwm_max;
        anj_ispctl_delay_time_default();
    }
}

/* 处理已开灯保持/渐增逻辑 */
static void anj_ispctl_light_state_handle_dark_keep(IspCtlControlRuntime *control, int currentPwm,
                                                    int led_brightness_mode, int *targetPwm)
{
    anj_ispctl_delay_time_default();
    if (led_brightness_mode == LED_BRIGHTNESS_MODE_MANUAL)
    {
        *targetPwm = control->pwm_max;
    }
    else
    {
        *targetPwm = anj_ispctl_step_pwm_duty(1, currentPwm, control->pwm_max);
    }
}

/* 处理中间亮度补光逻辑 */
static void anj_ispctl_light_state_handle_mid_bright(IspCtlControlRuntime *control, int currentPwm,
                                                     int led_brightness_mode, int *targetPwm)
{
    if (currentPwm > 0 && led_brightness_mode != LED_BRIGHTNESS_MODE_MANUAL)
    {
        anj_ispctl_delay_time_default();
        *targetPwm = anj_ispctl_step_pwm_duty(1, currentPwm, control->pwm_max);
    }
}

/* 处理关灯倒计时逻辑 */
static void anj_ispctl_light_state_handle_close_countdown(IspCtlControlRuntime *control, int *targetPwm)
{
    control->light_off_hold_ticks--;
    if (control->light_off_hold_ticks <= 0)
    {
        *targetPwm = 0;
        anj_ispctl_delay_time_default();
    }
}

/* 处理渐暗收光逻辑 */
static void anj_ispctl_light_state_handle_close_dim(IspCtlControlRuntime *control, int led_brightness_mode,
                                                    int *targetPwm)
{
    if (led_brightness_mode == LED_BRIGHTNESS_MODE_MANUAL)
    {
        control->light_off_hold_ticks--;
        if (control->light_off_hold_ticks <= 0)
        {
            *targetPwm = 0;
            anj_ispctl_delay_time_default();
        }
    }
    else
    {
        anj_ispctl_delay_time_default();
        *targetPwm = anj_ispctl_step_pwm_duty(0, *targetPwm, control->pwm_max);
    }
}

/*
 * 主动灯控基础策略（重构后）：
 * 1. 灯亮度上限为 0 时，只切 PQ/IRCUT，不驱动实际灯光。
 * 2. 到达开灯条件后，先走开灯保持倒计时，再把灯拉到目标亮度。
 * 3. 在开灯阈值和关灯阈值之间晃动时，自动亮度模式会适度补光。
 * 4. 超过关灯阈值后，先收光再关灯；手动亮度模式偏向延时后直接关。
 */
static ISPCTL_PQBIN_MODE anj_ispctl_process_single_light_channel(int sensitiveValue, int openTh, int led_brightness_mode,
                                                                 int currentPwm, int closeTh, int aeStable,
                                                                 int *targetPwm)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    ISPCTL_PQBIN_MODE pqbin_mode = PQBIN_MODE_NONE;

    if (0 == control->pwm_max)
    {
        *targetPwm = 0;
        if (anj_mw_media_isp_is_darker(sensitiveValue, openTh))
            pqbin_mode = PQBIN_MODE_MONO;
        else if (anj_ispctl_check_close_condition(sensitiveValue, closeTh, aeStable))
            pqbin_mode = PQBIN_MODE_RGB;
        return pqbin_mode;
    }

    /* 状态判定 */
    IspCtlLightState state = anj_ispctl_light_state_judge(sensitiveValue, openTh, currentPwm, closeTh, aeStable);

    /* 状态处理 */
    switch (state)
    {
    case ISPCTL_LIGHT_STATE_DARK_OPEN:
        anj_ispctl_light_state_handle_dark_open(control, targetPwm);
        break;
    case ISPCTL_LIGHT_STATE_DARK_KEEP:
        anj_ispctl_light_state_handle_dark_keep(control, currentPwm, led_brightness_mode, targetPwm);
        break;
    case ISPCTL_LIGHT_STATE_MID_BRIGHT:
        anj_ispctl_light_state_handle_mid_bright(control, currentPwm, led_brightness_mode, targetPwm);
        break;
    case ISPCTL_LIGHT_STATE_CLOSE_COUNTDOWN:
        anj_ispctl_light_state_handle_close_countdown(control, targetPwm);
        break;
    case ISPCTL_LIGHT_STATE_CLOSE_DIM:
        anj_ispctl_light_state_handle_close_dim(control, led_brightness_mode, targetPwm);
        break;
    default:
        break;
    }

    if (state != ISPCTL_LIGHT_STATE_DARK_OPEN)
        control->light_on_hold_ticks = DELAY_TIME_LIGHT_ON;
    if (state != ISPCTL_LIGHT_STATE_CLOSE_COUNTDOWN)
        control->light_off_hold_ticks = DELAY_TIME_LIGHT_OFF;

    return (*targetPwm > 0) ? PQBIN_MODE_MONO : PQBIN_MODE_RGB;
}

static ISPCTL_PQBIN_MODE anj_ispctl_soft_photosensor_rlight_process(int sensitiveValue, int aeStable,
                                                                    int led_brightness_mode, LedMode led_mode,
                                                                    int nightAlarm, int bvTarget,
                                                                    IspCtlPwmLayerState *layer)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int currentRedPwm = anj_mw_hwctrl_pwm_rlight_get(ISPCTL_CONTROL_CAMERA_INDEX);
    int isDouble = (led_mode == LED_INFRAED_THEN_WHITE) || (led_mode == LED_WHITE_THEN_INFRAED);
    int openTh = control->light_threshold.red_open_th;
    int closeTh = control->light_threshold.red_close_th;

    layer->white_pwm = 0;
    /*
     * 检测窗口内有人形时 BV < 5000，红外优先拉到当前配置亮度上限。
     * 该旁路仍用 AE BV（非 softlight 主度量）。
     */
    if ((led_mode == LED_PURE_INFRAED) &&
        anj_ispctl_pd_detect_recent_ms(ISPCTL_CONTROL_CAMERA_INDEX, ISPCTL_PD_DIM_LIGHT_BOOST_WINDOW_MS) &&
        (bvTarget < ISPCTL_PD_DIM_LIGHT_BOOST_BV_TH) &&
        (control->pwm_max > 0))
    {
        layer->red_pwm = control->pwm_max;
        return PQBIN_MODE_MONO;
    }

    /*
     * 双光：白光告警结束后退回红外。仍满足开灯条件时直接拉满红外，避免再走一遍开灯倒计时。
     */
    if (isDouble && nightAlarm && anj_mw_media_isp_is_darker(sensitiveValue, openTh) && (control->pwm_max > 0))
    {
        layer->red_pwm = control->pwm_max;
        anj_ispctl_delay_time_default();
        return PQBIN_MODE_MONO;
    }

    return anj_ispctl_process_single_light_channel(sensitiveValue, openTh, led_brightness_mode,
                                                   currentRedPwm, closeTh, aeStable, &layer->red_pwm);
}

static ISPCTL_PQBIN_MODE anj_ispctl_soft_photosensor_wlight_process(int sensitiveValue, int aeStable,
                                                                    int led_brightness_mode, IspCtlPwmLayerState *layer)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int currentWhitePwm = anj_mw_hwctrl_pwm_wlight_get(ISPCTL_CONTROL_CAMERA_INDEX);
    int openTh = control->light_threshold.white_open_th;
    int closeTh = control->light_threshold.white_close_th;

    return anj_ispctl_process_single_light_channel(sensitiveValue, openTh, led_brightness_mode, currentWhitePwm,
                                                   closeTh, aeStable, &layer->white_pwm);
}

/* AIISP：判断/开关在 middleware；10s 冷却在这里；边沿按当前日夜态重载 PQ */
static int anj_ispctl_ai_process(float curGain, int BV, ISPCTL_PQBIN_MODE curMode)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    ISPCTL_PQBIN_MODE pqMode = curMode;
    int prev = control->aiisp_enabled;
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    LedMode led_mode = pMediacfg->videoConfig[0].videoCapture.led_mode;
    int is_mono = (control->pqbin_mode == PQBIN_MODE_MONO) ||
                  ((curMode == PQBIN_MODE_MONO) && (led_mode != LED_PURE_WHITE));

    if (control->aiisp_hold_ticks > 0)
    {
        control->aiisp_hold_ticks--;
    }

    if (anj_mw_media_isp_ai_update(ISPCTL_CONTROL_CAMERA_INDEX, curGain, BV,
                                   control->alarm_pwm.white_pwm, &control->aiisp_enabled,
                                   &control->aiisp_hold_ticks, &control->aiisp_switch_cnt,
                                   is_mono) != 0)
    {
        return pqMode;
    }

    if (control->aiisp_enabled != prev)
    {
        if (pqMode == PQBIN_MODE_NONE)
        {
            pqMode = control->pqbin_mode;
        }
        control->aiisp_hold_ticks = DELAY_TIME_AIISP_SWITCH;
    }

    return pqMode;
}

/*
 * 先跑基础模式逻辑：Passive/Active/DayNight/Always/Aging。
 * 只写入 control->base_pwm，不修改 white_pwm_target。
 */
static ISPCTL_PQBIN_MODE anj_ispctl_process_base_mode(VideoCaptureCfg *pVideoCapCfg,
                                                      const AnjIspSensitiveInfo *pSensitive, int bvTarget,
                                                      int nightAlarm)
{
    ISPCTL_PQBIN_MODE pqMode = PQBIN_MODE_NONE;
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    IspCtlPwmLayerState *base = &control->base_pwm;

    anj_ispctl_pwm_layer_init_from_target(base);

    switch (pVideoCapCfg->ircut_mode)
    {
    case IRCUT_Mode_Passive:
    case IRCUT_Mode_AUTO_BY_HARDWARE:
        pqMode = anj_ispctl_passive_process();
        anj_ispctl_apply_fixed_mode_targets_on(base, pqMode, pVideoCapCfg->led_mode);
        break;
    case IRCUT_Mode_Active:
        if (pVideoCapCfg->led_mode == LED_PURE_WHITE)
        {
            base->red_pwm = 0;
            pqMode = anj_ispctl_soft_photosensor_wlight_process(pSensitive->value, pSensitive->ae_stable,
                                                                pVideoCapCfg->led_brightness_mode, base);
        }
        else
        {
            pqMode = anj_ispctl_soft_photosensor_rlight_process(pSensitive->value, pSensitive->ae_stable,
                                                                pVideoCapCfg->led_brightness_mode,
                                                                pVideoCapCfg->led_mode, nightAlarm, bvTarget, base);
        }
        break;
    case IRCUT_Mode_DayNight:
        pqMode = anj_ispctl_time_process(&pVideoCapCfg->ircut_nighttime);
        anj_ispctl_apply_fixed_mode_targets_on(base, pqMode, pVideoCapCfg->led_mode);
        break;
    case IRCUT_Mode_LIGHT_ALWAYS_ON:
        pqMode = anj_ispctl_manual_always_process(1);
        anj_ispctl_apply_fixed_mode_targets_on(base, pqMode, pVideoCapCfg->led_mode);
        break;
    case IRCUT_Mode_LIGHT_ALWAYS_OFF:
        pqMode = anj_ispctl_manual_always_process(0);
        anj_ispctl_clear_pwm_layer(base);
        break;
    case IRCUT_Mode_AGING_TEST:
        pqMode = anj_ispctl_aging_process(control->pqbin_mode);
        break;
    default:
        break;
    }

    base->pq_mode = pqMode;
    return pqMode;
}

/* 2s 无人后 10s 保持当前 HW 亮度，期满 alarm 收到 0。 */
static void anj_ispctl_alarm_white_close(IspCtlWhiteAlarmState *wa, int whitePwm, IspCtlPwmLayerState *alarm)
{
    unsigned long long nowMs = anj_mw_get_cputime_ms(NULL);

    if (whitePwm <= 0)
    {
        wa->fade_close_start_ms = 0;
        alarm->white_pwm = 0;
        return;
    }

    if (wa->fade_close_start_ms == 0)
    {
        wa->fade_close_start_ms = nowMs;
    }

    if ((nowMs - wa->fade_close_start_ms) < ISPCTL_WHITE_CLOSE_WINDOW_MS)
    {
        alarm->white_pwm = whitePwm;
        return;
    }

    wa->fade_close_start_ms = 0;
    anj_ispctl_white_alarm_force_set(ISPCTL_WHITE_FORCE_STATE_NONE, 0);
    alarm->white_pwm = 0;
}

/*
 * 告警层：只写 alarm_pwm；仅读 base_pwm 的 PWM，不读 base->pq_mode。
 */
static void anj_ispctl_process_alarm_override(VideoCaptureCfg *pVideoCapCfg,
                                              const AnjIspSensitiveInfo *pSensitive, const AnjIspAeInfo *pAeInfo,
                                              int *nightAlarm)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    IspCtlWhiteAlarmState *wa = anj_ispctl_white_alarm();
    const IspCtlPwmLayerState *base = &control->base_pwm;
    IspCtlPwmLayerState *alarm = &control->alarm_pwm;
    int isPureWhite = (pVideoCapCfg->led_mode == LED_PURE_WHITE);
    int isDouble = (pVideoCapCfg->led_mode == LED_INFRAED_THEN_WHITE) || (pVideoCapCfg->led_mode == LED_WHITE_THEN_INFRAED);
    int whitePwm = anj_mw_hwctrl_pwm_wlight_get(ISPCTL_CONTROL_CAMERA_INDEX);
    int redPwm = isPureWhite ? 0 : anj_mw_hwctrl_pwm_rlight_get(ISPCTL_CONTROL_CAMERA_INDEX);
    IspCtlWhiteAlarmPermission permission;
    int ledBrightnessMode = pVideoCapCfg->led_brightness_mode;
    int baseWhitePwm = base->white_pwm;
    /*
     * 双光告警入口：夜态 MONO、base/硬件红外已开、上一帧 night_alarm、或白光会话未结束。
     * 切到 RGB 白光后仍要保持资格，不能只认 MONO。
     */
    int dualNightAssist = isDouble && ((control->pqbin_mode == PQBIN_MODE_MONO) || (*nightAlarm != 0) ||
                                       anj_ispctl_white_alarm_session_active() || (base->red_pwm > 0) ||
                                       (redPwm > 0));

    anj_ispctl_alarm_layer_reset(alarm);

    if (pVideoCapCfg->led_mode == LED_PURE_INFRAED)
    {
        *nightAlarm = 0;
        return;
    }

    /* 低照进入条件仍看当前已生效 PQ（pqbin_mode），与 base 目标无关。 */
    anj_ispctl_low_bv_status_update(pAeInfo->curGain, control->pqbin_mode, whitePwm);

    permission = anj_ispctl_white_alarm_permission_update(pAeInfo->bvTarget, baseWhitePwm, dualNightAssist);
    if (permission == ISPCTL_WHITE_ALARM_ALLOWED)
    {
        if (isDouble)
        {
            *nightAlarm = 1;
        }

        anj_ispctl_alarm_apply_white_pwm(alarm, pSensitive->value, pAeInfo->bvTarget, pSensitive->ae_stable, whitePwm,
                                         redPwm, ledBrightnessMode, baseWhitePwm, pVideoCapCfg->ircut_mode);
    }
    else if (anj_ispctl_white_alarm_session_active())
    {
        /* 渐关期间仍标记 night_alarm，供结束后 base 红外快速拉起。 */
        if (isDouble)
        {
            *nightAlarm = 1;
        }
        else
        {
            *nightAlarm = 0;
        }
        anj_ispctl_alarm_white_close(wa, whitePwm, alarm);
    }
    else
    {
        /*
         * 会话已结束。base 已在本帧用 nightAlarm 拉过红外；
         * 白光硬件灭后清 night_alarm（本分支 alarm.white 已是 0）。
         */
        if (!isDouble || whitePwm <= 0)
        {
            *nightAlarm = 0;
        }
    }
}

/*
 * PWM：alarm 白光>0 用告警白光并强制关红外；否则用 base。
 * 告警层只驱动白光，不写红外（alarm.red_pwm 无业务含义）。
 * PQ：告警白光有效强制 RGB，否则用 base->pq_mode。
 */
static ISPCTL_PQBIN_MODE anj_ispctl_resolve_pwm_targets(void)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    const IspCtlPwmLayerState *base = &control->base_pwm;
    const IspCtlPwmLayerState *alarm = &control->alarm_pwm;
    int whiteTarget = (alarm->white_pwm > 0) ? alarm->white_pwm : base->white_pwm;
    int redTarget = base->red_pwm;
    ISPCTL_PQBIN_MODE pqMode = base->pq_mode;

    if (alarm->white_pwm > 0)
    {
        /* 告警白光生效时必须关红外，不能回落到 base 红外。 */
        redTarget = 0;
        pqMode = PQBIN_MODE_RGB;
    }

    anj_ispctl_set_pwm_targets(whiteTarget, redTarget);
    return pqMode;
}

static IspCtlLoopDecision anj_ispctl_process_control_camera(VideoCaptureCfg *pVideoCapCfg,
                                                            const AnjIspSensitiveInfo *pSensitive,
                                                            const AnjIspAeInfo *pAeInfo,
                                                            int nightAlarm)
{
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    IspCtlLoopDecision decision = control->mLoopDecision;

    decision.night_alarm = nightAlarm;
    control->sensitive = *pSensitive;
    anj_ispctl_process_base_mode(pVideoCapCfg, pSensitive, pAeInfo->bvTarget, decision.night_alarm);
    anj_ispctl_process_alarm_override(pVideoCapCfg, pSensitive, pAeInfo, &decision.night_alarm);
    decision.requested_pq_mode = anj_ispctl_resolve_pwm_targets();
    if (decision.requested_pq_mode == decision.apply_pq_mode)
    {
        decision.requested_pq_mode = PQBIN_MODE_NONE;
    }
    decision.apply_pq_mode = anj_ispctl_ai_process(pAeInfo->curGain, pAeInfo->bvTarget, decision.requested_pq_mode);

    return decision;
}

static int anj_ispctl_minshutter_set(int iCamerIndex)
{
    int MinShutterUS = 30;
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig[iCamerIndex].videoCapture;

    if (pVideoCapCfg->forct_antiflicker == 1)
    {
        MinShutterUS = (pVideoCapCfg->tvsystem == 0) ? 8333 : 10000;
    }

    return anj_mw_media_isp_shutterus_set(iCamerIndex, MinShutterUS, 0);
}

static int anj_ispctl_img_faceae_process(int iCamerIndex, int Current_pwm, ISPCTL_PQBIN_MODE pqbin_mode, VideoCaptureCfg *pVideoCapCfg)
{
    IspCtlCameraRuntime *cameraRuntime = anj_ispctl_camera_runtime(iCamerIndex);
    int iRet = 0;
    int cross = 0;
    int conv_speed = 3;
    unsigned int curTargetY[16] = {0};
    anj_mw_media_isp_aetarget_get(iCamerIndex, curTargetY);
    if (Current_pwm <= 0)
    {
        anj_ispctl_aetarget_set(iCamerIndex);
        return iRet;
    }

    anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
    event_rect_s humanRect = {0};
    event_rect_s faceRect = {0};
    event_rect_s checkRect = {0};
    IspDetectRuntime *detectRuntime = anj_ispctl_detect_runtime();
    if (detectRuntime->detect_updated && (iCamerIndex == detectRuntime->camera_index))
    {
        humanRect = detectRuntime->detect_rect;
        // 2s无新检测区域 检测结束
        if ((anj_mw_get_cputime_ms(NULL) - detectRuntime->detect_start_time_ms) > 2000)
        {
            detectRuntime->detect_updated = 0;
        }
    }
    anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);

    faceRect = humanRect;
    faceRect.height = humanRect.height * 3 / 4;
    checkRect = faceRect;
    int diff_day = 0;
    int diff_night = 0;
    // 检测区域大于3像素点为有效检测
    if (humanRect.width >= 3)
    {
        for (int rectIdx = 0; rectIdx < (int)(sizeof(g_detect_rect_cfgs) / sizeof(g_detect_rect_cfgs[0])); rectIdx++)
        {
            IspDetectRectCfg *pstDetecRecCfg = &g_detect_rect_cfgs[rectIdx];
            if (anj_smart_detect_rect_cross(checkRect.pos_x, checkRect.pos_y,
                                            checkRect.pos_x + checkRect.width, checkRect.pos_y + checkRect.height,
                                            pstDetecRecCfg->x, pstDetecRecCfg->y,
                                            pstDetecRecCfg->x + pstDetecRecCfg->w, pstDetecRecCfg->y + pstDetecRecCfg->h))
            {
                int chIdx = 0;
                cameraRuntime->detect_close_ticks = 0;
                cross = 1;
                int target_y = pstDetecRecCfg->target_day;
                // 近距离大目标处理
                if (rectIdx < 3 && humanRect.width >= 10)
                {
                    diff_day = curTargetY[0] - g_detect_rect_cfgs[0].target_day;
                    diff_night = curTargetY[0] - g_detect_rect_cfgs[0].target_night;
                    if (pqbin_mode == PQBIN_MODE_MONO)
                    {
                        target_y = curTargetY[0] - diff_night / conv_speed;
                    }
                    else
                    {
                        target_y = curTargetY[0] - diff_day / conv_speed;
                    }
                }
                else
                {
                    diff_day = pstDetecRecCfg->target_day - curTargetY[0];
                    diff_night = pstDetecRecCfg->target_night - curTargetY[0];
                    if (pqbin_mode == PQBIN_MODE_MONO)
                    {
                        target_y = curTargetY[0] + diff_night / conv_speed;
                    }
                    else
                    {
                        target_y = curTargetY[0] + diff_day / conv_speed;
                    }
                }

                usleep(300 * 1000);
                if (curTargetY[0] == (unsigned int)target_y && curTargetY[1] == (unsigned int)target_y &&
                    curTargetY[2] == (unsigned int)target_y)
                {
                    return 0;
                }

                for (chIdx = 0; chIdx < (int)(sizeof(curTargetY) / sizeof(curTargetY[0])); chIdx++)
                {
                    curTargetY[chIdx] = (unsigned int)target_y;
                }
                anj_mw_media_isp_aetarget_set(iCamerIndex, curTargetY);
                break;
            }
        }
        if (cross == 0)
        {
            cameraRuntime->detect_close_ticks++;
            __DBG("human rect not cross any rect(check rect(%d,%d,%d,%d, human rect(%d,%d,%d,%d)\n",
                   checkRect.pos_x, checkRect.pos_y, checkRect.width, checkRect.height,
                   humanRect.pos_x, humanRect.pos_y, humanRect.width, humanRect.height);
        }
    }
    else
    {
        int i = 0;
        for (i = 0; i < sizeof(g_detect_rect_cfgs) / sizeof(g_detect_rect_cfgs[0]); i++)
        {
            IspDetectRectCfg *pstDetecRecCfg = &g_detect_rect_cfgs[i];
            if (anj_smart_detect_rect_cross(checkRect.pos_x, checkRect.pos_y,
                                            checkRect.pos_x + checkRect.width, checkRect.pos_y + checkRect.height,
                                            pstDetecRecCfg->x, pstDetecRecCfg->y,
                                            pstDetecRecCfg->x + pstDetecRecCfg->w, pstDetecRecCfg->y + pstDetecRecCfg->h))
            {
                cameraRuntime->detect_close_ticks = 0;
                cross = 1;
            }
        }

        if (cross)
        {
            unsigned int target_y = 260;
            if (curTargetY[0] == target_y && curTargetY[1] == target_y && curTargetY[2] == target_y)
            {
                return 0;
            }

            for (i = 0; i < (sizeof(curTargetY) / sizeof(curTargetY[0])); i++)
            {
                curTargetY[i] = target_y;
            }
            anj_mw_media_isp_aetarget_set(iCamerIndex, curTargetY);
        }
        else
        {
            cameraRuntime->detect_close_ticks++;
        }
    }

    if (cameraRuntime->detect_close_ticks >= AETARGET_SET_TIME)
    {
        cameraRuntime->detect_close_ticks = 0;
        anj_ispctl_aetarget_set(iCamerIndex);
    }
    return iRet;
}

static int anj_ispctl_dynamic_flicker_set(int iCamerIndex, VideoCaptureCfg *pVideoCapCfg, float curGain,
                                          ISPCTL_PQBIN_MODE pqbin_mode)
{
    int hz = 0;

    if (pqbin_mode == PQBIN_MODE_RGB)
    {
        if (curGain < 10.0f)
        {
            hz = (pVideoCapCfg->tvsystem == 0) ? 60 : 50;
        }
        else if (curGain > 12.0f)
        {
            hz = 0;
        }
        else
        {
            return 0;
        }
    }

    return anj_mw_media_isp_filcker_set(iCamerIndex, hz);
}

static void anj_ispctl_img_process(int iCamerIndex, VideoCaptureCfg *pVideoCapCfg,
                                   int defaultFps, AnjIspAeInfo *pstAnjIspAeInfo, ISPCTL_PQBIN_MODE pqbin_mode)
{
#if _SUPPORT_IQTOOL_
    return;
#endif
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int Current_pwm = 0;
    if (control->white_pwm_target)
    {
        Current_pwm = control->white_pwm_target;
    }
    if (control->red_pwm_target)
    {
        Current_pwm = control->red_pwm_target;
    }

    if (pVideoCapCfg->led_mode != LED_WHITE_THEN_INFRAED &&
        pVideoCapCfg->ispadvmode == LED_IMAGE_FACE_EXPOSURE_PREVENTION &&
        control->aiisp_enabled == 0)
    {
        anj_ispctl_img_faceae_process(iCamerIndex, Current_pwm, pqbin_mode, pVideoCapCfg);
    }

    anj_ispctl_dynamic_flicker_set(iCamerIndex, pVideoCapCfg, pstAnjIspAeInfo->curGain, pqbin_mode);
    int weight = 0;
    if (pstAnjIspAeInfo->curGain < 128.0)
    {
        weight = 1;
    }
    anj_mw_media_isp_weight_set(iCamerIndex, weight);

    /* 告警配置的 sensitivity 是 0~10 档，这里统一换算成 0.0~1.0 的智能检测灵敏度。 */
    float sensitivity =
        (ISPCTL_AIISP_PD_SENSITIVITY_SCALE - pstAlarmCfg->aiAlarm.pdAlarm[iCamerIndex].sensitivity) /
        ISPCTL_AIISP_PD_SENSITIVITY_SCALE;
    if (DOUBLE_GREATER(pstAnjIspAeInfo->curGain, ISPCTL_AIISP_PD_SENSITIVITY_GAIN_TH))
    {
        /* 高 gain 场景容易误报，额外下调一点灵敏度。 */
        sensitivity -= ISPCTL_AIISP_PD_SENSITIVITY_HIGH_GAIN_OFFSET;
    }
    anj_smart_sensitivity_update(iCamerIndex, sensitivity);
}

static void anj_ispctl_set_image_params(int iCamerIndex, VideoCaptureCfg *pVideoCapCfg,
                                        VideoEncodeCfg *pVideoEncCfg)
{
#if _SUPPORT_IQTOOL_
    return;
#endif
    IspCtlCameraRuntime *cameraRuntime = anj_ispctl_camera_runtime(iCamerIndex);
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    int new_hflip = 0;
    int new_vflip = 0;
    int hz = pVideoCapCfg->tvsystem == 0 ? 60 : 50;
    anj_mw_media_isp_wdr_value_set(iCamerIndex, cameraRuntime->wdr_cfg, pVideoCapCfg->wdr_value);
    usleep(1000 * 1000);
    anj_mw_media_isp_wdr_value_set(iCamerIndex, cameraRuntime->wdr_cfg, pVideoCapCfg->wdr_value);
    if (control->aiisp_enabled == 0)
    {
        anj_mw_media_isp_fps_set(iCamerIndex, pVideoEncCfg->frameRate);
    }

    anj_mw_media_isp_filcker_set(iCamerIndex, hz);
    anj_mw_media_isp_brightness_set(iCamerIndex, pVideoCapCfg->brightness);
    anj_mw_media_isp_contrast_set(iCamerIndex, pVideoCapCfg->contrast);
    anj_mw_media_isp_backlight_set(iCamerIndex, cameraRuntime->blc_cfg, pVideoCapCfg->backlight);
    anj_mw_media_isp_sharpness_set(iCamerIndex, cameraRuntime->sharpness_ud0_cfg, cameraRuntime->sharpness_ud1_cfg,
                                   cameraRuntime->sharpness_ud2_cfg, pVideoCapCfg->sharpness);
    anj_mw_media_isp_saturation_set(iCamerIndex, cameraRuntime->saturation_cfg, pVideoCapCfg->saturation);
    anj_mw_media_isp_rotate_set(iCamerIndex, pVideoCapCfg->rotate);
    anj_config_image_flip_trans(pVideoCapCfg->hflip, pVideoCapCfg->vflip, &new_hflip, &new_vflip);
    anj_mw_media_isp_flip_set(iCamerIndex, new_hflip, new_vflip);
    anj_ispctl_minshutter_set(iCamerIndex);
}

static int anj_ispctl_thread(void *ctx, int *bStart)
{
    int iCamerIndex = 0;
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    anj_ispctl_light_init(LIGHT_PWM_MAX_VALUE, 0);
    anj_mw_hwctrl_ircut_init();

    control->aiisp_enabled = 0;
    for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig[i].videoCapture;
        VideoEncodeCfg *pVideoEncCfg = &pMediacfg->videoConfig[i].videoEncode.encodeCfg[0];
        // 灯控、IRCUT 和 PQ 是全局控制域，默认由 control camera 驱动。
        if (i == 0)
        {
            anj_ispctl_pqmode_process(PQBIN_MODE_RGB, pVideoCapCfg->led_mode);
            control->pqbin_mode = PQBIN_MODE_RGB;
            anj_ispctl_light_thr_init(pVideoCapCfg);

            // 补光亮度手动，需要根据设置的不光亮度值来设置最大PWM值
            control->pwm_max = (pVideoCapCfg->led_brightness_value * LIGHT_PWM_MAX_VALUE) / 100;
            __INFO("pwm_max=%d\n", control->pwm_max);
        }
        anj_ispctl_set_image_params(i, pVideoCapCfg, pVideoEncCfg);
    }

    control->aiisp_hold_ticks = DELAY_TIME_AIISP_SWITCH;
    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
    {
        control->pwm_min = 500;
    }
    else
    {
        control->pwm_min = 85;
    }
    anj_ispctl_delay_time_default();

    anj_ispctl_rlight_set(0);
    anj_ispctl_wlight_set(0);

    AnjIspAeInfo stAnjIspAeInfo = {0};
    AnjIspSensitiveInfo stSensitive = {0};
    DevInfo *pstDevInfo = getDevInfo();
    int nightAlarm = 0;
    while (*bStart)
    {
        if (pstDevInfo->bFactoryMode)
        {
            usleep(ISPCTL_THREAD_USLEEP);
            continue;
        }

        anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
        VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig[0].videoCapture;

        if (control->pending_reset)
        {
            __INFO("isp reset now\n");
            control->pending_reset = 0;
            anj_ispctl_clear_pwm_layer(&control->base_pwm);
            anj_ispctl_clear_pwm_layer(&control->alarm_pwm);
            control->white_pwm_target = 0;
            control->red_pwm_target = 0;
            anj_ispctl_white_alarm_reset();
            anj_ispctl_delay_time_memset();
            anj_ispctl_pqmode_process(PQBIN_MODE_RGB, pVideoCapCfg->led_mode);
        }

        // 灯光控制、IRCUT控制在这里
        memset(&stAnjIspAeInfo, 0, sizeof(AnjIspAeInfo));
        memset(&stSensitive, 0, sizeof(AnjIspSensitiveInfo));
        anj_mw_media_isp_aeinfo_get(0, &stAnjIspAeInfo);
        anj_mw_media_isp_sensitive_get(&stSensitive, &stAnjIspAeInfo);
        IspCtlLoopDecision decision =
            anj_ispctl_process_control_camera(pVideoCapCfg, &stSensitive, &stAnjIspAeInfo, nightAlarm);
        nightAlarm = decision.night_alarm;

        anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);

        anj_ispctl_pqmode_process(decision.apply_pq_mode, pVideoCapCfg->led_mode);
        for (iCamerIndex = 0; iCamerIndex < ANJ_CAMERA_MAX_NUMS; iCamerIndex++)
        {
            VideoEncodeCfg *pVideoEncCfg = &pMediacfg->videoConfig[iCamerIndex].videoEncode.encodeCfg[0];
            pVideoCapCfg = &pMediacfg->videoConfig[iCamerIndex].videoCapture;
            memset(&stAnjIspAeInfo, 0, sizeof(AnjIspAeInfo));
            anj_mw_media_isp_aeinfo_get(iCamerIndex, &stAnjIspAeInfo);
            anj_ispctl_img_process(iCamerIndex, pVideoCapCfg, pVideoEncCfg->frameRate, &stAnjIspAeInfo, decision.apply_pq_mode);
        }

        usleep(ISPCTL_THREAD_USLEEP);
    }
    __INFO("EXIT!\n");
    return 0;
}

static int anj_ispctl_init(void)
{
    int iRet = 0;
    if (s_stIspctlInit)
	{
	    __ERR("had been init!\n");
		return iRet;
	}
    iRet = anj_mw_media_isp_init();
    if (iRet)
    {
        __ERR("anj_mw_media_isp_init failed\n");
        return -1;
    }

    s_stIspCtlThread.bAutoDestroy = 0;
    strncpy(s_stIspCtlThread.iThreadName, "anj_ispctl_thread", sizeof(s_stIspCtlThread.iThreadName) - 1);
    s_stIspCtlThread.iThreadjob.ctx = &s_stIspCtlThread;
    s_stIspCtlThread.iThreadjob.func = anj_ispctl_thread;
    iRet = anj_thread_task_create(&s_stIspCtlThread);
    if (iRet != 0)
    {
        __ERR("wifi check create failed: %d\n", iRet);
        return iRet;
    }

    s_stIspctlInit = 1;
    return 0;
}

static int anj_ispctl_uninit(void)
{
    int iRet = 0;
    if (s_stIspctlInit == 0)
	{
	    __ERR("not init!\n");
		return iRet;
	}
    s_stIspctlInit = 0;
    iRet = anj_mw_media_isp_uninit();

    anj_thread_task_destroy(&s_stIspCtlThread, 0);

    anj_mw_hwctrl_ircut_uninit();
    anj_ispctl_light_uninit();
    return iRet;
}

void anj_ispctl_config_set()
{
    if (s_stIspctlInit == 0)
	{
		return ;
	}
    anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
    s_stAnjIspCtlInfo.control_runtime.pending_reset = 1;
    anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);
}

int anj_ispctl_day_night_get()
{
    int status = 0;
    ISPCTL_PQBIN_MODE appliedMode;
    if (s_stIspctlInit == 0)
	{
		return status;
	}

    anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
    appliedMode = s_stAnjIspCtlInfo.control_runtime.mLoopDecision.apply_pq_mode;
    if (appliedMode == PQBIN_MODE_NONE)
    {
        appliedMode = s_stAnjIspCtlInfo.control_runtime.pqbin_mode;
    }
    anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);

    status = (appliedMode == PQBIN_MODE_MONO) ? 0 : 1;
    return status;
}

void anj_ispctl_light_threshold_update(int open_delay, int off_sensitivity)
{
    IspCtlLightThresholdState *th;

    if (s_stIspctlInit == 0)
	{
		return ;
	}
    anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
    anj_ispctl_white_alarm_reset();
    th = &s_stAnjIspCtlInfo.control_runtime.light_threshold;
    anj_ispctl_get_open_ths(open_delay, &th->white_open_th, &th->red_open_th);
    th->red_close_th = anj_ispctl_get_close_th(0, off_sensitivity);
    th->white_close_th = anj_ispctl_get_close_th(1, off_sensitivity);

    __INFO("Open delay=%d, Off sensitivity=%d, Open white=%d red=%d, Close red=%d white=%d\n",
           open_delay, off_sensitivity, th->white_open_th, th->red_open_th, th->red_close_th, th->white_close_th);
    anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);
}

void anj_ispctl_light_manual_ctrl(int lightIdx, int brightness)
{
    if (s_stIspctlInit == 0)
	{
		return ;
	}
    int whitePwm = 0;
    int redPwm = 0;

    __INFO("isp light manual ctrl lightidx:%d, brightness:%d\n", lightIdx, brightness);

    if (lightIdx == 1)
    {
        whitePwm = (brightness * LIGHT_PWM_MAX_VALUE) / 100;
        anj_ispctl_wlight_set(whitePwm);
    }
    else if (lightIdx == 2)
    {
        redPwm = (brightness * LIGHT_PWM_MAX_VALUE) / 100;
        anj_ispctl_rlight_set(redPwm);
    }
}

void anj_ispctl_ircut_manual_ctrl(int daynight, int cameraIndex)
{
    if (s_stIspctlInit == 0)
	{
		return ;
	}
    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pVideoCapCfg = &pMediacfg->videoConfig[cameraIndex].videoCapture;

    __INFO("ircut manual ctrl daynight:%d, ircut_mode:%d\n", daynight, pVideoCapCfg->ircut_mode);

    // 只能在手动模式下才能外部设置
    if (pVideoCapCfg->ircut_mode == IRCUT_Mode_Manual)
    {
        ISPCTL_PQBIN_MODE mode = (daynight == 1) ? PQBIN_MODE_RGB : PQBIN_MODE_MONO;
        anj_ispctl_pqmode_manual_ctrl(mode, cameraIndex);

        __INFO("ircut_mode manual daynight:%d set to:%d\n", daynight, mode);
    }
    else
    {
        __ERR("ircut_mode:%d don't support manual control set to:%d\n", pVideoCapCfg->ircut_mode, daynight);
    }
}

int anj_ispctl_update_base_param(VideoCaptureCfg *pVideoCapCfg, int cameraIndex)
{
    int iRet = 0;
    if (s_stIspctlInit == 0)
	{
		return iRet;
	}
    IspCtlCameraRuntime *cameraRuntime = NULL;
    IspCtlControlRuntime *control = anj_ispctl_ctrl_runtime();

    if ((cameraIndex < 0) || (cameraIndex >= ANJ_CAMERA_MAX_NUMS))
    {
        __ERR("cameraIndex %d is invalid\n", cameraIndex);
        return -1;
    }

    MediaConfig *pMediacfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pMediacfg->videoConfig[cameraIndex].videoCapture;
    cameraRuntime = anj_ispctl_camera_runtime(cameraIndex);

    do
    {
        if (pVideoCapCfg->brightness != pstVideoCapCfg->brightness)
        {
            iRet = anj_mw_media_isp_brightness_set(cameraIndex, pVideoCapCfg->brightness);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->contrast != pstVideoCapCfg->contrast)
        {
            iRet = anj_mw_media_isp_contrast_set(cameraIndex, pVideoCapCfg->contrast);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->saturation != pstVideoCapCfg->saturation)
        {
            iRet = anj_mw_media_isp_saturation_set(cameraIndex, cameraRuntime->saturation_cfg, pVideoCapCfg->saturation);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->sharpness != pstVideoCapCfg->sharpness)
        {
            iRet = anj_mw_media_isp_sharpness_set(cameraIndex, cameraRuntime->sharpness_ud0_cfg,
                                                  cameraRuntime->sharpness_ud1_cfg,
                                                  cameraRuntime->sharpness_ud2_cfg,
                                                  pVideoCapCfg->sharpness);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->backlight != pstVideoCapCfg->backlight)
        {
            iRet = anj_mw_media_isp_backlight_set(cameraIndex, cameraRuntime->blc_cfg, pVideoCapCfg->backlight);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->tvsystem != pstVideoCapCfg->tvsystem)
        {
            int hz = pVideoCapCfg->tvsystem == 0 ? 60 : 50;
            iRet = anj_mw_media_isp_filcker_set(cameraIndex, hz);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->forct_antiflicker != pstVideoCapCfg->forct_antiflicker)
        {
            iRet = anj_ispctl_minshutter_set(cameraIndex);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->vflip != pstVideoCapCfg->vflip || pVideoCapCfg->hflip != pstVideoCapCfg->hflip)
        {
            int new_hflip = 0;
            int new_vflip = 0;
            anj_config_image_flip_trans(pVideoCapCfg->hflip, pVideoCapCfg->vflip, &new_hflip, &new_vflip);
            iRet = anj_mw_media_isp_flip_set(cameraIndex, new_hflip, new_vflip);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->cropxpix != pstVideoCapCfg->cropxpix || pVideoCapCfg->cropypix != pstVideoCapCfg->cropypix)
        {
            iRet = anj_mw_media_scl_crop_set(cameraIndex, pVideoCapCfg->cropxpix, pVideoCapCfg->cropypix);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->rotate != pstVideoCapCfg->rotate)
        {
            iRet = anj_mw_media_isp_rotate_set(cameraIndex, pVideoCapCfg->rotate);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->whitebalance != pstVideoCapCfg->whitebalance)
        {
            iRet = anj_mw_media_isp_awb_set(cameraIndex, pVideoCapCfg->whitebalance);
            if (iRet)
                break;
        }

        if (pVideoCapCfg->wdr_value != pstVideoCapCfg->wdr_value)
        {
            if (0 != pVideoCapCfg->wdr_mode)
            {
                iRet = anj_mw_media_isp_wdr_value_set(cameraIndex, cameraRuntime->wdr_cfg, pVideoCapCfg->wdr_value);
            }
            else
            {
                ; // wdr_mode 为 0 时不调 wdr_value，只保存配置
            }

            if (iRet)
                break;
        }

        if (pVideoCapCfg->HLC != pstVideoCapCfg->HLC)
        {

            if (VIDEO_WDR_MODE_HDR != pVideoCapCfg->wdr_mode)
            {
                iRet = anj_mw_media_isp_hlc_set(cameraIndex, pVideoCapCfg->HLC, pstVideoCapCfg->brightness);
            }

            if (iRet)
                break;
        }

        if (pVideoCapCfg->tnf != pstVideoCapCfg->tnf)
        {
            if (VIDEO_WDR_MODE_HDR != pVideoCapCfg->wdr_mode)
            {
                iRet = anj_mw_media_isp_2dnr_set(cameraIndex, cameraRuntime->nr_2d_cfg, pVideoCapCfg->tnf);
            }

            if (iRet)
                break;
        }

        if (pVideoCapCfg->snf != pstVideoCapCfg->snf)
        {
            if (VIDEO_WDR_MODE_HDR != pVideoCapCfg->wdr_mode)
            {
                iRet = anj_mw_media_isp_3dnr_set(cameraIndex, cameraRuntime->nr_3d_cfg, pVideoCapCfg->snf);
            }

            if (iRet)
                break;
        }

        /*
         * 灯控阈值和 PWM 上限是全局共享硬件状态，只接受 control camera 的配置驱动。
         * 其他 camera 在多路场景下只更新自己的图像参数。
         */
        if (cameraIndex == ISPCTL_CONTROL_CAMERA_INDEX &&
            (pVideoCapCfg->ircut_openled_delay != pstVideoCapCfg->ircut_openled_delay ||
             pVideoCapCfg->light_off_sensitivity != pstVideoCapCfg->light_off_sensitivity))
        {
            anj_ispctl_light_threshold_update(pVideoCapCfg->ircut_openled_delay, pVideoCapCfg->light_off_sensitivity);
        }

        if (cameraIndex == ISPCTL_CONTROL_CAMERA_INDEX)
        {
            if (pVideoCapCfg->led_mode != pstVideoCapCfg->led_mode)
            {
                anj_ispctl_white_alarm_reset();
            }
            /* shutter 变更：手动且与当前 PQ 一致则下发限制；由手动切回全自动则恢复按帧率的 AE 曝光上限 */
            if (memcmp(&pVideoCapCfg->shutterSetting, &pstVideoCapCfg->shutterSetting, sizeof(VideoShutter)) != 0)
            {
                ISPCTL_PQBIN_MODE pq = PQBIN_MODE_NONE;

                anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
                pq = s_stAnjIspCtlInfo.control_runtime.mLoopDecision.apply_pq_mode;
                if (pq == PQBIN_MODE_NONE)
                {
                    pq = s_stAnjIspCtlInfo.control_runtime.pqbin_mode;
                }
                anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);
                anj_ispctl_maxshutter_set(cameraIndex, 2, pq, pVideoCapCfg);
            }
            control->pwm_max = (pVideoCapCfg->led_brightness_value * LIGHT_PWM_MAX_VALUE) / 100;
        }
    } while (0);

    return iRet;
}

void anj_ispctl_smart_set(event_rect_param_s *pstNowRectParam)
{
    if (s_stIspctlInit == 0)
	{
		return ;
	}
    IspDetectRuntime *detectRuntime = anj_ispctl_detect_runtime();
    if (pstNowRectParam == NULL)
    {
        __ERR("input invalid!\n");
        return;
    }
    if (pstNowRectParam->s32RectCnt > 0)
    {
        IspCtlCameraRuntime *cameraRuntime = anj_ispctl_camera_runtime(pstNowRectParam->camera);
        anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
        detectRuntime->detect_rect = anj_smart_rec_get(pstNowRectParam);
        detectRuntime->camera_index = pstNowRectParam->camera;
        cameraRuntime->detect_start_ticks++;
        if (cameraRuntime->detect_start_ticks > ISP_DEFAUT_START_TIMES)
        {
            cameraRuntime->detect_start_ticks = 0;
            detectRuntime->detect_updated = 1;
        }

        detectRuntime->detect_start_time_ms = anj_mw_get_cputime_ms(NULL);
        if (pstNowRectParam->move)
            s_stAnjIspCtlInfo.control_runtime.last_human_move_time_ms = anj_mw_get_cputime_ms(NULL);
        anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);
    }
    else
    {
        anj_mutex_lock(&s_AnjIspCtrlInfoMutex);
        anj_ispctl_camera_runtime(pstNowRectParam->camera)->detect_start_ticks = 0;
        anj_mutex_unlock(&s_AnjIspCtrlInfoMutex);
    }
}

AnjIspCtlInfo *getIspctlInfo()
{
    return &s_stAnjIspCtlInfo;
}

REGISTER_MODULE(anj_ispctl, MODULE_PRIORITY_ISP);
