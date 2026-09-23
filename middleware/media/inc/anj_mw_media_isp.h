#ifndef _ANJ_MW_MEDIA_ISP_H_
#define _ANJ_MW_MEDIA_ISP_H_

#include "anj_mw_comm.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_IRCUT_STATUS_NUMBER 3

typedef struct FACE_DETECT_RECT_S
{
    int x;
    int y;
    int w;
    int h;
    int target_day;
    int target_night;
} FACE_DETECT_RECT_T;

typedef struct
{
    unsigned int ledNo;
    unsigned int bOpenManual;
} LightCtrlCmd;

typedef struct
{
    int daynight;
    int switchtimes;
    unsigned int timestamp;
} DayNightSwitchStruct;

typedef struct
{
    float curGain;
    int bvTarget;
    int expShutter;
    int lumy;
    int lv;
    int bIsStable;
    int sceneTarget;
} AnjIspAeInfo;

typedef enum
{
    ANJ_ISP_SENSITIVE_BV = 0, /* 数值越小越暗，关灯阈值 = open + diff */
    ANJ_ISP_SENSITIVE_GAIN,   /* 数值越大越暗，关灯阈值 = open - diff */
} AnjIspSensitiveType;

/* 软光敏用光敏度量：mstar 填 BV，ts 填 gain */
typedef struct
{
    AnjIspSensitiveType type;
    int value;
    int ae_stable;
} AnjIspSensitiveInfo;

#define ISP_ZOOM_ENTRY_CNT 1000

int anj_mw_media_isp_init(void);
int anj_mw_media_isp_uninit(void);
int anj_mw_media_isp_param_init(int iCameraIdex);
int anj_mw_media_isp_load(int iCameraIdex, char *filepath, int *bStart);
int anj_mw_media_isp_fps_set(int iCameraIdex, int fps);

int anj_mw_media_isp_brightness_set(int iCameraIdex, int brightness);
int anj_mw_media_isp_contrast_set(int iCameraIdex, int contrast);
int anj_mw_media_isp_saturation_get(int iCameraIdex, char *saturation);
int anj_mw_media_isp_saturation_set(int iCameraIdex, char *oriSaturation, int saturation);
int anj_mw_media_isp_sharpness_get(int iCameraIdex, char *sharpness0, char *sharpness1, char *sharpness2);
int anj_mw_media_isp_sharpness_set(int iCameraIdex, char *sharpness0, char *sharpness1, char *sharpness2, int sharpness);
int anj_mw_media_isp_backlight_get(int iCameraIdex, char *backlight);
int anj_mw_media_isp_backlight_set(int iCameraIdex, char *oriBacklight, int backlight);
int anj_mw_media_isp_filcker_set(int iCameraIdex, int hz);
int anj_mw_media_isp_shutterus_set(int iCameraIdex, int minShutter, int maxShutter);
int anj_mw_media_isp_gain_set(int iCameraIdex, int maxGain);
int anj_mw_media_isp_rotate_set(int iCameraIdex, int rotate);
int anj_mw_media_isp_flip_set(int iCameraIdex, int hflip, int vflip);
int anj_mw_media_isp_awb_set(int iCameraIdex, int whitebalance);
int anj_mw_media_isp_wdr_value_get(int iCameraIdex, char *wdrvalue);
int anj_mw_media_isp_wdr_value_set(int iCameraIdex, char *oriWdrValue, int wdrvalue);
int anj_mw_media_isp_wdr_set(int iCameraIdex, int enable);
int anj_mw_media_isp_hlc_set(int iCameraIdex, int hlc, int brightness);
int anj_mw_media_isp_2dnr_get(int iCameraIdex, char *tnf);
int anj_mw_media_isp_2dnr_set(int iCameraIdex, char *oriSnf, int tnf);
int anj_mw_media_isp_3dnr_get(int iCameraIdex, char *snf);
int anj_mw_media_isp_3dnr_set(int iCameraIdex, char *oriSnf, int snf);
int anj_mw_media_isp_converger_get(int iCameraIdex, unsigned int *u32SpeedY);
int anj_mw_media_isp_converger_set(int iCameraIdex, unsigned int *u32SpeedY);
int anj_mw_media_vpss_crop_set(int iCameraIdex, unsigned short cropx, unsigned short cropy);
int anj_mw_media_scl_crop_set(int iCameraIdex, unsigned short cropx, unsigned short cropy);

int anj_mw_media_isp_aeinfo_get(int iCameraIdex, AnjIspAeInfo *pstAnjIspAeInfo);
/* 由 AE 信息得到软光敏度量（mstar=BV，ts=gain） */
int anj_mw_media_isp_sensitive_get(AnjIspSensitiveInfo *pstSensitive, const AnjIspAeInfo *pstAeInfo);
int anj_mw_media_isp_is_darker(int value, int threshold);
int anj_mw_media_isp_is_brighter(int value, int threshold);
int anj_mw_media_isp_aetarget_get(int iCameraIdex, unsigned int *u32Y);
int anj_mw_media_isp_aetarget_set(int iCameraIdex, unsigned int *u32Y);
int anj_mw_media_isp_weight_set(int iCameraIdex, int weight);

int anj_mw_media_isp_zoom_stop(int iCameraIdex);
int anj_mw_media_isp_zoom_center_init(int iCameraIdex, double max_multiple, int speed);
int anj_mw_media_isp_zoom_move_init(int iCameraIdex, double max_multiple, DOUBLE_AREA_ENTRY *pCurArea, DOUBLE_AREA_ENTRY *pTargetArea);
int anj_mw_media_isp_zoom_set(int iCameraIdex, double cur_multiple, double run_multiple);
int anj_mw_media_isp_zoom_get(int iCameraIdex, DOUBLE_AREA_ENTRY *cur_area, double *pRunPercent);
void anj_mw_media_isp_zoom_track_init(void);

/* bEnable: 0=关 1=开；ts 开=RFR，关=AIMVD。 */
int anj_mw_media_isp_ai_start(int iCameraIdx, int bEnable);
/* *enabled 为 0/1 入出；*hold_ticks / *switch_cnt 入出。is_mono=1 时 ts 保持 AIMVD。不支持返回 -1。 */
int anj_mw_media_isp_ai_update(int iCameraIdx, float curGain, int bv, int white_pwm,
                               int *enabled, int *hold_ticks, int *switch_cnt, int is_mono);

#ifdef __cplusplus
}
#endif

#endif
