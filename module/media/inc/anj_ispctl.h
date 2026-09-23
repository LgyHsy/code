#ifndef __ANJ_ISPCTL_H__
#define __ANJ_ISPCTL_H__

#include "eventhub.h"
#include "anj_mw_media_isp.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    int iCameraIdex;
    char *filepath;
} ispbin_info;

typedef enum
{
    PQBIN_MODE_NONE,
    PQBIN_MODE_RGB,
    PQBIN_MODE_MONO,
} ISPCTL_PQBIN_MODE;

typedef enum
{
    ISPCTL_WHITE_ALARM_BLOCKED = 0,
    ISPCTL_WHITE_ALARM_ALLOWED,
} IspCtlWhiteAlarmPermission;

typedef enum
{
    /* 正常态，未处于 120s 强制关灯后的恢复流程。 */
    ISPCTL_WHITE_FORCE_STATE_NONE = 0,
    /* 120s 关灯后，当前仍有人但已静止较久，等待“再次移动”后放行。 */
    ISPCTL_WHITE_FORCE_STATE_WAIT_MOTION,
    /* 120s 关灯后，当前已无人，等待重新检测到人并持续 5s 后放行。 */
    ISPCTL_WHITE_FORCE_STATE_WAIT_REDETECT,
} IspCtlWhiteForceState;

/*
 * 告警白光状态（会话 / 渐关 / 120s 强制收灯恢复），集中在一处读写。
 * 流程：pd_hit → session_open → (2s 无人) 置 fade_close_start、清 session → 10s 渐关 → 全清。
 */
typedef struct
{
    unsigned long long pd_hit_ms;           /* 最近一次 1s 人形命中 */
    unsigned long long session_open_ms;     /* 本轮告警白光首次点亮，用于 120s 计时 */
    unsigned long long fade_close_start_ms; /* 无资格后的 10s 渐关起点 */
    IspCtlWhiteForceState force_recover;    /* 120s 强制关灯后的恢复子状态机 */
    unsigned long long force_recover_since_ms;
} IspCtlWhiteAlarmState;

/* 低照度告警模式状态（对应 old_code low_bv_status，与 base_pwm 解耦） */
typedef struct
{
    int low_bv;              /* 低照度告警模式 */
    int low_bv_enter_ticks;  /* 进入低照度的倒计时 tick */
} IspCtlLowBvState;

typedef struct
{
    event_rect_s detect_rect;
    int detect_updated;
    int camera_index;
    unsigned long long detect_start_time_ms;
} IspDetectRuntime;

/*
 * PWM 分层状态。base / alarm 共用此结构：
 * - base：white_pwm / red_pwm / pq_mode 都可能有效；
 * - alarm：只写 white_pwm（告警白光）；red_pwm / pq_mode 无业务含义；
 * resolve：alarm.white>0 用告警白光并强制关红外 + RGB，否则用 base。
 */
typedef struct
{
    int white_pwm;
    int red_pwm;
    ISPCTL_PQBIN_MODE pq_mode;
} IspCtlPwmLayerState;

typedef struct
{
    /* 本轮基础灯控/模式判断得出的目标日夜态。 */
    ISPCTL_PQBIN_MODE requested_pq_mode;
    /* 本轮真正需要下发到 PQ/IRCUT 的模式。PQBIN_MODE_NONE 表示仅更新 PWM。 */
    ISPCTL_PQBIN_MODE apply_pq_mode;
    /* 双光模式下白光告警结束后是否处于“退回红外”的过渡态。 */
    int night_alarm;
} IspCtlLoopDecision;

typedef struct
{
    /* 开灯阈值偏移量。最终开灯阈值 = delay 表值 + open_offset（BV 项目用）。 */
    int open_offset;
    /* 开灯到关红外之间允许的最大回差。 */
    int red_close_diff_max;
    /* 开灯到关白光之间允许的最大回差。 */
    int white_close_diff_max;
    /* 白光开灯阈值（平台光敏度量：BV 或 gain）。 */
    int white_open_th;
    /* 红外开灯阈值。 */
    int red_open_th;
    /* 白光关灯阈值。 */
    int white_close_th;
    /* 红外关灯阈值。 */
    int red_close_th;
} IspCtlLightThresholdState;

typedef struct
{
    ISPCTL_PQBIN_MODE pqbin_mode;
    int aiisp_enabled;
    /* 切完冷却，各平台共用。 */
    int aiisp_hold_ticks;
    /* >0 连续要进，<0 连续要出；ts 不读。 */
    int aiisp_switch_cnt;
    IspCtlLowBvState low_bv_state;

    int pwm_min;
    int pwm_max;
    /* 环境/软光敏层输出的 PWM 与 PQ。 */
    IspCtlPwmLayerState base_pwm;
    /* 告警/低照度层输出的 PWM 与 PQ。 */
    IspCtlPwmLayerState alarm_pwm;
    /* resolve 后下发到硬件的最终白光目标 PWM。 */
    int white_pwm_target;
    /* resolve 后下发到硬件的最终红外目标 PWM。 */
    int red_pwm_target;

    /* 开灯条件连续命中的剩余等待 tick。 */
    int light_on_hold_ticks;
    /* 关灯条件连续命中的剩余等待 tick。 */
    int light_off_hold_ticks;
    /* 最近一次人形移动时间，由 anj_ispctl_smart_set 刷新。 */
    unsigned long long last_human_move_time_ms;
    IspCtlWhiteAlarmState white_alarm;

    IspCtlLoopDecision mLoopDecision;
    int pending_reset;

    IspCtlLightThresholdState light_threshold;
    /* 当前软光敏度量（type/value/ae_stable），由主循环刷新 */
    AnjIspSensitiveInfo sensitive;
} IspCtlControlRuntime;

typedef struct
{
    unsigned int ae_target_y[16];
    char saturation_cfg[16];
    char sharpness_ud0_cfg[16];
    char sharpness_ud1_cfg[16];
    char sharpness_ud2_cfg[16];
    char blc_cfg[16];
    char wdr_cfg[16];
    char nr_2d_cfg[16];
    char nr_3d_cfg[16];
    int day_speed_y[4];

    float current_gain;
    /* 夜间降帧恢复默认帧率的等待计时，按 camera 独立。 */
    int ae_recover_ticks;
    /* 手动快门模式在日夜态切换时的上一次状态，按 camera 独立。 */
    int last_shutter_mode;
    /* 防过曝逻辑里“多久没有命中检测区域”的计时，按 camera 独立。 */
    int detect_close_ticks;
    /* 连续多少次收到有效检测框后才开始触发防过曝，按 camera 独立。 */
    int detect_start_ticks;
} IspCtlCameraRuntime;

typedef struct
{
    /* 灯控、IRCUT、PQ 和 AIISP 都是全局控制域。 */
    IspCtlControlRuntime control_runtime;
    /* 图像缓存和瞬态计数按 camera 独立保存。 */
    IspCtlCameraRuntime camera_runtime[ANJ_CAMERA_MAX_NUMS];
    IspDetectRuntime detect_runtime;
} AnjIspCtlInfo;

void anj_ispctl_smart_set(event_rect_param_s *pstNowRectParam);

void anj_ispctl_config_set();

int anj_ispctl_update_base_param(VideoCaptureCfg *pNewIspInfo, int cameraIndex);

void anj_ispctl_light_manual_ctrl(int lightIdx, int brightness);
void anj_ispctl_ircut_manual_ctrl(int daynight, int cameraIndex);

/*
    获取日夜状态
*/
int anj_ispctl_day_night_get();

AnjIspCtlInfo *getIspctlInfo();

#ifdef __cplusplus
}
#endif

#endif
