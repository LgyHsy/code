#ifndef _SDK_OPTION_H_
#define _SDK_OPTION_H_

#include "project_option.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SDK_KERNEL_IDENTITY "MSTAR_I6C_LINUX_377"
#define ISP_AI_SUPPORT (0)
#define GET_SYSTEM_TIME_RELATIVE 1

/* cover 归一化到 [0, MAX_RGN_COVER_W] */
#define MAX_RGN_COVER_W 8192
#define RGN_COVER_COORD_BASE(stream_size) (MAX_RGN_COVER_W)
#define RGN_COVER_COLOR 0xff801080

#define PTZ_ZOOM_MAX_MULTIPLE (1.9)
#define SMART_MAX_DETECT_RECT (3)

/* 软光敏关灯回差（BV） */
#define ISP_LIGHT_CLOSE_MIN_DIFF (5000)
#define ISP_LIGHT_CLOSE_MAX_DIFF_WHITE (60000)
#define ISP_LIGHT_CLOSE_MAX_DIFF_RED (60000)
#define ISP_LIGHT_CLOSE_MAX_LIMIT (12500)
#define ISP_LIGHT_CLOSE_VALUE_MIN (0) /* BV 不用下限钳位 */
#define ISP_LIGHT_MID_MARGIN (10000)

#define ISP_DETECT_RECT_CFG     \
    {5, 90, 90, 10, 75, 70},    \
    {5, 85, 90, 5, 85, 80},     \
    {10, 80, 80, 5, 105, 90},   \
    {10, 75, 80, 5, 130, 115},  \
    {10, 70, 80, 5, 155, 140},  \
    {15, 65, 70, 5, 180, 160},  \
    {15, 60, 70, 5, 200, 180},  \
    {15, 55, 70, 5, 220, 200},  \
    {15, 50, 70, 5, 240, 220},  \
    {15, 45, 70, 5, 260, 260},  \

#ifdef __cplusplus
}
#endif
#endif
