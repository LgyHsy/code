#ifndef _ANJ_MW_MEDIA_OSD_H_
#define _ANJ_MW_MEDIA_OSD_H_

#include "anj_mw_comm.h"
#include "anj_mw_media_video.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CLAMP(v, min, max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))
#define SWAP(a, b)          \
    do                      \
    {                       \
        typeof(a) _tmp = a; \
        a = b;              \
        b = _tmp;           \
    } while (0)

typedef enum
{
    OSD_TYPE_TIME,
    OSD_TYPE_TITLE,
    OSD_TYPE_DBG,
    OSD_TYPE_4G,
    OSD_TYPE_BITMAP,
    OSD_TYPE_CLOUD,
    OSD_TYPE_BATTERY,
    OSD_TYPE_SDCARD,
    OSD_TYPE_ISP_DEBUG,
    OSD_TYPE_USER_0,
    OSD_TYPE_USER_1,
    OSD_TYPE_USER_2,
    OSD_TYPE_USER_3,
    OSD_TYPE_USER_4,
    OSD_TYPE_MAX,
} ANJ_OSD_TYPE_E;

typedef enum
{
    PIXEL_FORMAT_ARGB1555 = 0,
    PIXEL_FORMAT_I2,
    PIXEL_FORMAT_I4,
    PIXEL_FORMAT_CNT
} rgn_pixel_format_e;

#define MAX_OSD_HANDLE (MAX_VIDEO_NUM * OSD_TYPE_MAX)
#define MAX_RECT_NUM (20)
#define MAX_POINT_NUM (30)
#define MAX_RGN_MASK_AREA 4

typedef struct
{
    int pos_x;
    int pos_y;
    int width;
    int height;
    int u32Color;
} overlay_rect_s;

typedef struct
{
    int pos_x;
    int pos_y;
} overlay_point_s;

typedef struct
{
    int stream_type;
    int stream_width;
    int stream_height;
    int rgn_handle;
    int pos_x;
    int pos_y;
    int width;
    int height;
    int byte_stride;
    int font_factor;
    char show_content_buffer[256];
    char *bitmap_data;
    int bitmap_len;
    unsigned char *bmp_data;
    int bmp_len;
    short style;
    unsigned char text_color;
    unsigned char bg_color;
    unsigned char edge_color;
    unsigned char draw_bg;
    unsigned char draw_edge;
    unsigned char inverse_pixel;
    unsigned char linegap;
} overlay_param_s;

typedef struct
{
    int stream_type;
    int rgn_handle;
    overlay_rect_s cover_rect;
} cover_param_s;

typedef struct
{
    int rgn_handle;
    int stream_type;
    int stream_width;
    int stream_height;
    int u32Ratio_w;
    int u32Ratio_h;
    overlay_rect_s draw_rect[MAX_RECT_NUM];
    int s32RectCnt;
    unsigned char u8BorderWidth;
    int bFill;
    int bShow;
} rect_param_s;

typedef struct
{
    int rgn_handle;
    int stream_type;
    int stream_width;
    int stream_height;
    int u32Ratio_w;
    int u32Ratio_h;
    overlay_point_s stPoint[MAX_POINT_NUM];
    overlay_point_s enPoint[MAX_POINT_NUM];
    int s32LineCnt;
    int u32Color[MAX_POINT_NUM];
    unsigned char u8BorderWidth;
    int bFill;
    int bShow;
    int bTwinkle;
} line_param_s;

int anj_mw_osd_init(int pixel_fmt);
int anj_mw_osd_uninit(void);

int anj_mw_osd_create(overlay_param_s *overlay_param, ANJ_SIZE_S *resolution, int rgn_pixel_formats);
int anj_mw_osd_destroy(overlay_param_s *overlay_param);

int anj_mw_osd_canvas_info_clear(overlay_param_s *overlay_param);
int anj_mw_osd_canvas_update(overlay_param_s *overlay_param);

int anj_mw_osd_bitmap_data_free(overlay_param_s *overlay_param);
int anj_mw_osd_bitmap_data_malloc(rgn_pixel_format_e ePixelFmt, overlay_param_s *overlay_param);

int anj_mw_osd_draw_pixel(rgn_pixel_format_e ePixelFmt, unsigned short value, int offset_x, int offset_y, overlay_param_s *overlay_param);
int anj_mw_osd_update_rect(rect_param_s *rect_param);
int anj_mw_osd_clean_rect(rect_param_s *rect_param);

int anj_mw_osd_update_line(line_param_s *line_param);
int anj_mw_osd_clean_line(line_param_s *line_param);

int anj_mw_osd_cover_create(cover_param_s *cover_param);
int anj_mw_osd_cover_destroy(cover_param_s *cover_param);

int anj_mw_osd_draw_overlay_create(int stream_type, int stream_width, int stream_height, int rgn_handle);
int anj_mw_osd_draw_overlay_destroy(int stream_type, int rgn_handle);

void anj_mw_osd_line_param_init(line_param_s *line_param, int stream_type, int stream_width, int stream_height);
void anj_mw_osd_rect_param_init(rect_param_s *rect_param, int stream_type, int stream_width, int stream_height);

#ifdef __cplusplus
}
#endif

#endif
