#include "anj_mw_smart.h"
#include "anj_mw_smart_provider.h"
#include <math.h>

int anj_mw_smart_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_provider_process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, pstAnjSmartAttr);
}

int anj_mw_smart_init(AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_provider_init(pstAnjSmartAttr);
}

int anj_mw_smart_uninit()
{
    return anj_mw_smart_provider_uninit();
}

float anj_mw_smart_set_sensitivity(float sensitivity)
{
    return anj_mw_smart_provider_set_sensitivity(sensitivity);
}

int anj_mw_smart_rect_map_pd_to_zoom(const DOUBLE_AREA_ENTRY *cur_area,
                                     int *box_x, int *box_y, int *box_w, int *box_h)
{
    DOUBLE_AREA_ENTRY area = {0};
    int box_left_top_x;
    int box_left_top_y;
    int box_width;
    int box_height;
    int crop_left_top_x;
    int crop_left_top_y;
    int crop_width;
    int crop_height;
    int box_right_bottom_x;
    int box_right_bottom_y;
    int crop_right_bottom_x;
    int crop_right_bottom_y;

    if (box_x == NULL || box_y == NULL || box_w == NULL || box_h == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }

    box_left_top_x = *box_x;
    box_left_top_y = *box_y;
    box_width = *box_w;
    box_height = *box_h;

    /* MStar: smart 看全图，需按 zoom 裁剪区换算到当前画面 */
    if (cur_area != NULL)
    {
        area = *cur_area;
    }
    if (area.width < 1.0 || area.height < 1.0)
    {
        __ERR("cur_area: width:%f height:%f < 1.0, set to 1.0\n", area.width, area.height);
        area.xPos = 0.0;
        area.yPos = 0.0;
        area.width = 1.0;
        area.height = 1.0;
    }

    crop_left_top_x = (int)round(area.xPos * SMART_PD_WIDTH);
    crop_left_top_y = (int)round(area.yPos * SMART_PD_HEIGHT);
    crop_width = (int)round(SMART_PD_WIDTH / area.width);
    crop_height = (int)round(SMART_PD_HEIGHT / area.height);
    if (crop_width <= 0 || crop_height <= 0)
    {
        __ERR("crop_width:%d crop_height:%d \n", crop_width, crop_height);
        return -1;
    }

    box_right_bottom_x = box_left_top_x + box_width;
    box_right_bottom_y = box_left_top_y + box_height;
    crop_right_bottom_x = crop_left_top_x + crop_width;
    crop_right_bottom_y = crop_left_top_y + crop_height;

    /* case1: 检测框完全不位于裁剪区域内 */
    if (box_right_bottom_x <= crop_left_top_x || box_left_top_x >= crop_right_bottom_x ||
        box_right_bottom_y <= crop_left_top_y || box_left_top_y >= crop_right_bottom_y)
    {
        return -1;
    }

    /* case2/3: 完全或部分位于裁剪区域内，取交集后映射到当前画面 */
    box_left_top_x = MAX(box_left_top_x, crop_left_top_x);
    box_left_top_y = MAX(box_left_top_y, crop_left_top_y);
    box_width = MIN(box_right_bottom_x, crop_right_bottom_x) - box_left_top_x;
    box_height = MIN(box_right_bottom_y, crop_right_bottom_y) - box_left_top_y;
    if (box_width <= 0 || box_height <= 0)
    {
        __ERR("box_width <= 0 || box_height <= 0\n");
        return -1;
    }

    *box_x = (int)round((box_left_top_x - crop_left_top_x) * area.width);
    *box_y = (int)round((box_left_top_y - crop_left_top_y) * area.height);
    *box_w = (int)round(box_width * area.width);
    *box_h = (int)round(box_height * area.height);
    return 0;
}

int anj_mw_smart_rect_map_zoom_to_pd(const DOUBLE_AREA_ENTRY *cur_area,
                                     int *box_x, int *box_y, int *box_w, int *box_h)
{
    (void)cur_area;

    /* MStar 平台当前无需 zoom->pd 坐标换算，保留空实现用于统一接口 */
    if (box_x == NULL || box_y == NULL || box_w == NULL || box_h == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }
    return 0;
}

int anj_mw_smart_register_yuv_cb(void *pfnYuvCb)
{
    return 0;
}

int anj_mw_smart_size_get(AnjSmartAttr *pstAnjSmartAttr, unsigned int *width, unsigned int *height)
{
    if ((pstAnjSmartAttr == NULL) || (width == NULL) || (height == NULL))
    {
        return -1;
    }

    if (anj_mw_smart_provider_size_get(pstAnjSmartAttr, width, height) != 0)
    {
        *width = DEFAULT_SMART_WIDTH;
        *height = DEFAULT_SMART_HEIGHT;
    }

    return 0;
}
