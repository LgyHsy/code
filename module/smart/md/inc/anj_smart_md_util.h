#ifndef __ANJ_SMART_MD_UTIL_H
#define __ANJ_SMART_MD_UTIL_H

#include <stdint.h>
#include "anj_smart_md.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*** *** ***
 *
 * 移动侦测实现
 * 非线程安全
 *
 ******/

#define ANJ_MD_MAX_W_DIV_NUM (22)
#define ANJ_MD_MAX_H_DIV_NUM (18)
#define ANJ_MD_MAX_REGION_NUM (ANJ_MD_MAX_W_DIV_NUM * ANJ_MD_MAX_H_DIV_NUM)

enum
{
    ANJ_MD_NO_ERR = 0,
    ANJ_MD_ERR_PARAM = -1001,
    ANJ_MD_ERR_NO_MEM = -1002
};

typedef struct
{
    // (0: disable, 1: enable)
    uint8_t enable;
    //(0 ~ 99)
    uint8_t size_percent_min;
    //(1 ~ 100), must be larger than size_percent_min
    uint8_t size_percent_max;
    //(10, 20, 30, ..., 100), 100 is the most sensitive
    uint8_t sensitivity;
} ANJ_MDParamsIn_t;

typedef struct
{
    uint16_t st_x;
    uint16_t st_y;
    uint16_t end_x;
    uint16_t end_y;
} ANJ_MD_ObjPos_t;

typedef struct _ANJ_MOTION_REGION_S
{
    uint16_t lt_x; // 区域左上角的像素坐标
    uint16_t lt_y;
    uint16_t rb_x; // 右下角
    uint16_t rb_y;
    uint16_t motion_cell;
    uint16_t row;
    uint16_t col;
} ANJ_MOTION_REGION_S;

typedef void *ANJ_MD_HANDLE;

ANJ_MD_HANDLE ANJ_MD_Create(uint16_t width, uint16_t height, uint8_t w_div, uint8_t h_div);

void ANJ_MD_Destroy(ANJ_MD_HANDLE handle);

int32_t ANJ_MD_SetDetectRegion(ANJ_MD_HANDLE handle, uint32_t regIndex, ANJ_MDParamsIn_t *param);

int32_t ANJ_MD_Detect(ANJ_MD_HANDLE handle, const uint8_t *imgBuf, int32_t bufSize, uint64_t timestampInMs);

int32_t ANJ_MD_GetTotalCellNum(ANJ_MD_HANDLE handle);

int32_t ANJ_MD_GetPixNumBlink(ANJ_MD_HANDLE handle);

int32_t ANJ_MD_GetMaxPixLumaDiff(ANJ_MD_HANDLE handle);

int32_t ANJ_MD_GetMaxCellLumaDiff(ANJ_MD_HANDLE handle);

int32_t ANJ_MD_GetMotionRegion(ANJ_MD_HANDLE handle, ANJ_MOTION_REGION_S **outMotionRegions);

void ANJ_MD_PrintCfg(ANJ_MD_HANDLE handle);

void ANJ_MD_ShowResult(MD_RESULT_S *pResult);

#if defined(__cplusplus)
}
#endif

#endif // ANJ_MD_UTIL_H
