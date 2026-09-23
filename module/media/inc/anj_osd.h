#ifndef __ANJ_OSD_H__
#define __ANJ_OSD_H__

#include "anj_config.h"
#include "eventhub.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RGB_VALUE_BLACK 1
#define RGB_VALUE_WHITE 2
#define RGB_VALUE_GREEN 3
#define RGB_VALUE_BLUE 5
#define RGB_VALUE_TRANSPARENT 0

#define OSD_ZOOM_MAX_MULTIPLE (12.0)

#define MAX_USER_OSD_TEXT_LEN 	464

typedef enum osd_location
{
    OSD_LOCATION_LEFT_TOP = 0,
    OSD_LOCATION_LEFT_BOTTOM = 1,
    OSD_LOCATION_RIGHT_TOP = 2,
    OSD_LOCATION_RIGHT_BOTTOM = 3,
    OSD_LOCATION_CNT
} osd_location_e;

typedef enum
{
    OVERLAY_PTZ_RESET,
    OVERLAY_PTZ_GOTO_GARDPOS,
    OVERLAY_PTZ_GOTO_LASTPOS,
    OVERLAY_PTZ_GOTO_DEFAULTPOS,
    OVERLAY_PTZ_GOTO_PRESET,
    OVERLAY_SDCARD_UNINIT,
    OVERLAY_SDCARD_RDONLY,
    OVERLAY_SDCARD_ERROR,
    OVERLAY_RECORD_BMP,
    OVERLAY_RECORD_BMP0,
    OVERLAY_RECORD_BMP1,
    OVERLAY_RECORD_BMP2,
    OVERLAY_CLOUD_BMP,
    OVERLAY_LENS_COVER,
    OVERLAY_ZOOM_DIGITAL,
    OVERLAY_ZOOM_PROGRESS,
    OVERLAY_PTZ_SWITCH_TO_IR_MODE,
    OVERLAY_PTZ_SWITCH_TO_WHITE_MODE,
    OVERLAY_PTZ_SWITCH_TO_DOUBLE_MODE,
    OVERLAY_PTZ_LINE_SCAN_ON,
    OVERLAY_PTZ_CRUISE_ON,
    OVERLAY_PTZ_LINE_SCAN_OFF,
    OVERLAY_PTZ_CRUISE_OFF,
    OVERLAY_PTZ_LINE_SCAN_SET_LEFT,
    OVERLAY_PTZ_LINE_SCAN_SET_RIGHT,
    OVERLAY_PTZ_WATCH_GUARD_SET_OK,
    OVERLAY_PTZ_WATCH_GUARD_SET_FAIL,
    OVERLAY_PTZ_WATCH_GUARD_CLEAR,
    OVERLAY_PTZ_ZOOM_TRACK_OFF,
    OVERLAY_PTZ_ZOOM_TRACK_ON,
    OVERLAY_PTZ_SET_PRESET,
    OVERLAY_PTZ_CLEAR_PRESET,
    OVERLAY_BATTERY_POWER,
    OVERLAY_BUIT,
} OverlayTextEnum;

#define OSD_IOT_COORDINATE_RATIO (100)

typedef struct osd_custom_content
{
    int custom_show;
    OverlayTextEnum overlayText;
    char overlayStr[64];
    /**
     * custom_x和custom_y是百分比的数字，根据custom_location不同的布局计算实际坐标。
     * 譬如custom_location=OSD_LOCATION_RIGHT_BOTTOM，custom_x和custom_y为20，实际坐标计算如下：
     * overlay_param->pos_x = (100 - 20) * resolution.u32Width / 100;
     * overlay_param->pos_y = (100 - 20) * resolution.u32Height / 100;
     *
     * 譬如custom_location=OSD_LOCATION_LEFT_TOP，custom_x和custom_y为20，实际坐标计算如下：
     * overlay_param->pos_x = 20 * resolution.u32Width / 100;
     * overlay_param->pos_y = 20 * resolution.u32Height / 100;
     */
    unsigned int custom_x;
    unsigned int custom_y;
    int custom_location;
} osd_custom_content_s;

typedef enum
{
    E_OSD_CONFIG_NORMAL = 0,
    E_OSD_CONFIG_USR,
} E_OSD_CONFIG_TYPE;

void anj_osd_debug_set(osd_custom_content_s *pstOsdCustom);
void anj_osd_4g_set(osd_custom_content_s *pstOsdCustom);
void anj_osd_bmp_set(osd_custom_content_s *pstOsdCustom);
void anj_osd_cloud_set(osd_custom_content_s *pstOsdCustom);
void anj_osd_sdcard_set(osd_custom_content_s *pstOsdCustom);
void anj_osd_isp_debug_set(osd_custom_content_s *pstOsdCustom);
void anj_osd_battery_set(osd_custom_content_s *pstOsdCustom);
int anj_osd_cover_set();
int anj_osd_lens_cover_get(void);
void anj_osd_draw_rect(event_rect_param_s *pstNowRectParam);
void anj_osd_polygon_update(Polygon *pstPolygon);
void anj_osd_cross_line_update(VideoGateAlarm *pstVideoGate);
void anj_osd_polygon_twinkle();
void anj_osd_frame_border_twinkle();
void anj_osd_cross_line_twinkle(int CrossLineTwinkle);
void anj_osd_update_config();

#ifdef __cplusplus
}
#endif

#endif
