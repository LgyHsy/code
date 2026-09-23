#ifndef _ANJ_MW_SMART_H_
#define _ANJ_MW_SMART_H_

#include <pthread.h>
#include "project_option.h"
#include "anj_mw_comm.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef enum
{
    SMART_TYPE_MD,
    SMART_TYPE_PD,
    SMART_TYPE_FD,
    SMART_TYPE_PVD,
    SMART_TYPE_MAX,
} ANJ_SMART_TYPE_E;

typedef struct
{
    int x;
    int y;
    int width;
    int height;
    int class_id;
    float score;
} AnjSmartBoxInfo;

typedef struct
{
    int boxCnt;
    AnjSmartBoxInfo stBoxInfo[SMART_MAX_DETECT_RECT];
} AnjSmartInfo;

typedef struct
{
    int enable;
    float sensitivity;
    int minRectFilter;
    int mdFilter;
} AnjSmartPdAttr;

typedef struct
{
    int enable;
    float sensitivity;
} AnjSmartFdAttr;

typedef struct
{
    AnjSmartPdAttr stAnjPdAttr;
    AnjSmartFdAttr stAnjFdAttr;
} AnjSmartAttr;

typedef struct
{
    unsigned char *cache[ANJ_CAMERA_MAX_NUMS];
    int width;
    int height;
    unsigned char valid[ANJ_CAMERA_MAX_NUMS];
    pthread_mutex_t mutex;
} AnjSmartLuma;

int anj_mw_smart_size_get(AnjSmartAttr *pstAnjSmartAttr, unsigned int *width, unsigned int *height);

int anj_mw_smart_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartBoxInfo, AnjSmartAttr *pstAnjSmartAttr);

int anj_mw_smart_init(AnjSmartAttr *pstAnjSmartAttr);

int anj_mw_smart_uninit();

float anj_mw_smart_set_sensitivity(float sensitivity);

int anj_mw_smart_cpm_start(int cameraIndex, int vencChn, unsigned int width, unsigned int height);

int anj_mw_smart_cpm_stop(int cameraIndex, int vencChn);

int anj_mw_smart_register_yuv_cb(void *pfnYuvCb);

int anj_mw_smart_rect_map_pd_to_zoom(const DOUBLE_AREA_ENTRY *cur_area, int *box_x, int *box_y, int *box_w, int *box_h);
int anj_mw_smart_rect_map_zoom_to_pd(const DOUBLE_AREA_ENTRY *cur_area, int *box_x, int *box_y, int *box_w, int *box_h);

#ifdef __cplusplus
}
#endif

#endif
