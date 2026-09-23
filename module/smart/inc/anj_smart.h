#ifndef _ANJ_SMART_H_
#define _ANJ_SMART_H_

#include "eventhub.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _USE_SMART_MD_
#define SMART_MD_DEFAULT_ENABLE 1
#else
#define SMART_MD_DEFAULT_ENABLE 0
#endif

#ifdef _USE_SMART_PD_
#define SMART_PD_DEFAULT_ENABLE 1
#else
#define SMART_PD_DEFAULT_ENABLE 0
#endif

#define MD_MAX_W_DIV_NUM 22
#define MD_MAX_H_DIV_NUM 18

typedef enum
{
    SMART_NULL_MASK = 0,   // 无智能检测1<<0
    SMART_MOTION_MASK = 1, // 移动检测1<<1
    SMART_HUMAN_MASK = 2,  // 人形检测1<<2
    SMART_CAR_MASK = 3,    // 机动车检测1<<3
    SMART_IO_MASK = 4,     // IO检测1<<4
    SMART_FACE_MASK = 5,   // 人脸检测1<<5
    SMART_MASK_MAX
} smart_mask_e;

#define SMART_CHECK_MASK(iSmart, iMask) (iSmart & (1 << iMask))    // 是否有对应智能检测
#define SMART_SET_MASK(iSmart, iMask) (iSmart | (1 << iMask))      // 设置对应智能检测
#define SMART_CLEAR_MASK(iSmart, iMask) (iSmart & (~(1 << iMask))) // 设置对应智能检测

typedef struct
{
    void *p_vir_addr;
    unsigned long long p_phy_addr;
    int len;
} smart_yuv_info;

void anj_smart_data_cb(int u32DevId, void *p_vir_addr, unsigned long long p_phy_addr, int len, void *param);

int anj_smart_size_get(int camera, int *width, int *height);

int anj_smart_luma_get(int camera, int x, int y, int ref_w, int ref_h, unsigned char *luma);

smart_mask_e anj_smart_mask_get();

int anj_smart_detect_rect_cross(int rect1_x1, int rect1_y1, int rect1_x2, int rect1_y2,
                                int rect2_x1, int rect2_y1, int rect2_x2, int rect2_y2);

event_rect_s anj_smart_rec_get(event_rect_param_s *pstRectangle);

int anj_smart_track_rect_update(int camera, event_rect_param_s *pstRectangle, event_rect_s *pstOut);
void anj_smart_track_rect_reset(int camera);

/* 读取 /tmp/ptz_track_ema，无文件则使用 ANJ_SMART_TRACK_EMA_NEW_WEIGHT */
void anj_smart_track_tune_load(void);

void anj_smart_restart();

void anj_smart_sensitivity_update(int camera, float sensitivity);

#ifdef __cplusplus
}
#endif

#endif
