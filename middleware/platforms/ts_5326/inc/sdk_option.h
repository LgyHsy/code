#ifndef _SDK_OPTION_H_
#define _SDK_OPTION_H_

#include "project_option.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SDK_KERNEL_IDENTITY "TS_LINUX_5326"
#define ISP_AI_SUPPORT (0)
#define GET_SYSTEM_TIME_RELATIVE 1

/* cover 使用像素坐标：base 取码流宽/高，公式 base*val/size 恒等于 val */
#define RGN_COVER_COORD_BASE(stream_size) (stream_size)
#define RGN_COVER_COLOR 0xff000000

#define PTZ_ZOOM_MAX_MULTIPLE (1.5)
#define SMART_MAX_DETECT_RECT (8)

/* 软光敏关灯回差（gain）：sens=40 时第 0 档 close≈2 */
#define ISP_LIGHT_CLOSE_MIN_DIFF (2)
#define ISP_LIGHT_CLOSE_MAX_DIFF_WHITE (20)
#define ISP_LIGHT_CLOSE_MAX_DIFF_RED (7)
#define ISP_LIGHT_CLOSE_MAX_LIMIT (0) /* gain 不用上限钳位 */
#define ISP_LIGHT_CLOSE_VALUE_MIN (1)
#define ISP_LIGHT_MID_MARGIN (2)

#define ISP_DETECT_RECT_CFG     \
    {5,  90, 90, 10, 35,  30},  \
    {5,  85, 90, 5,  40,  35},  \
    {10, 80, 80, 5,  50,  40},  \
    {10, 75, 80, 5,  60,  55},  \
    {10, 70, 80, 5,  70,  65},  \
    {15, 65, 70, 5,  85,  75},  \
    {15, 60, 70, 5,  90,  85},  \
    {15, 55, 70, 5,  100, 90},  \
    {15, 50, 70, 5,  110, 100}, \
    {15, 45, 70, 5,  120, 120}, \

#ifdef __cplusplus
}
#endif
#endif
