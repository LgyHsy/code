#include "anj_mw_comm.h"
#include "anj_module.h"
#include "anj_osd.h"
#include "anj_ispctl.h"
#include "anj_sysmng.h"
#include "anj_net.h"
#include "anj_video.h"
#include "anj_smart.h"
#include "anj_config_ptz.h"
#include "anj_zoom.h"
#include "anj_mw_smart.h"
#include "anj_mw_media_osd.h"
#include "anj_mw_media_isp.h"
#include "anj_mw_media_sys.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>

#define OSD_THREAD_TIME_US (100 * 1000)
#define OSD_LINE_UPDATE_TIME_US (5 * 1000 * 1000)
#define OSD_BMP_UPDATE_TIME_US (5 * 1000 * 1000)
#define OSD_NORMAL_UPDATE_TIME_US (400 * 1000)

#define INVALID_REGION_HANDLE 0xffff

#define FONT_W 16
#define FONT_H 16
#define FONT_BYTE 32
#define FONT_MAGIC "cham.li.font.16x16"

#define OSD_AUTH_FAILED "Copyright check failed^版权芯片授权失败"
#define ISP_OSD_DEBUG_FLAG "/tmp/isp_osd.flag"
#define OSD_OVERLAY_FRAMERATE_FLAG "/mnt/nand/flag.overlay.framerate"
#define OSD_SHOW_CONTENT_BUF_LEN 256

#define OSD_INVERSE_LUMA_LOW (40)
#define OSD_INVERSE_LUMA_HIGH (150)
#define OSD_INVERSE_BLOCK_SIZE (20)
#define OSD_INVERSE_BLOCK_W ((DEFAULT_SMART_WIDTH + OSD_INVERSE_BLOCK_SIZE - 1) / OSD_INVERSE_BLOCK_SIZE)
#define OSD_INVERSE_BLOCK_H ((DEFAULT_SMART_HEIGHT + OSD_INVERSE_BLOCK_SIZE - 1) / OSD_INVERSE_BLOCK_SIZE)

#define BMP_MAKECOLOR_I24(r, g, b) \
    (((unsigned int)(r) & 0xff) | (((unsigned int)(g) & 0xff) << 8) | (((unsigned int)(b) & 0xff) << 16))

#define BMP_MAKECOLOR_I16(r, g, b)                                      \
    (0x8000 |                                    /* Alpha=1 (不透明) */ \
     ((((unsigned int)(r) >> 3) & 0x1F) << 10) | /* R: 5位 */           \
     ((((unsigned int)(g) >> 3) & 0x1F) << 5) |  /* G: 5位 */           \
     (((unsigned int)(b) >> 3) & 0x1F))          /* B: 5位 */

#define BMP_MAKECOLOR_I8(r, g, b) \
    ((unsigned int)(r) & 0xFF) // 直接取R颜色值

#define OSD_DRIECTION_LINE_LEN (10)
#define OSD_DRIECTION_ARROW_LEN (3)

#define OSD_TWINKE_TIMES (6)

#define OSD_DEFAULT_STYLE AJ_OVERLAY_STYLE_TRANSPARENT_WHITEBLACK

#pragma pack(push, 1)
typedef struct
{
    unsigned short bfType;      // 文件类型，必须是0x4D42
    unsigned int bfSize;        // 文件大小
    unsigned short bfReserved1; // 保留字段
    unsigned short bfReserved2; // 保留字段
    unsigned int bfOffBits;     // 位图数据偏移量
} BMPFileHeader;

typedef struct
{
    unsigned int biSize;         // 信息头大小
    int biWidth;                 // 图像宽度
    int biHeight;                // 图像高度
    unsigned short biPlanes;     // 颜色平面数
    unsigned short biBitCount;   // 每像素位数
    unsigned int biCompression;  // 压缩类型
    unsigned int biSizeImage;    // 图像数据大小
    int biXPelsPerMeter;         // 水平分辨率
    int biYPelsPerMeter;         // 垂直分辨率
    unsigned int biClrUsed;      // 使用的颜色数
    unsigned int biClrImportant; // 重要颜色数
} BMPInfoHeader;
#pragma pack(pop)

typedef struct
{
    char magic[32];
    unsigned int chars_count;
} Unicode_FontHead_t;

typedef struct
{
    unsigned short unicode;
    unsigned char font_w;
    unsigned char font_h;
    unsigned char mask[FONT_BYTE];
} Unicode_FontData_t;

typedef enum
{
    ENC_LANGUAGE_CN = 0, /* Simplified Chinese */
    ENC_LANGUAGE_TW,     /* Traditional Chinese */
    ENC_LANGUAGE_EN,     /* English */
    ENC_LANGUAGE_JP,     /* Japan */
    ENC_LANGUAGE_KO,     /* korean */
    ENC_LANGUAGE_RU,     /* Russia */
    ENC_LANGUAGE_MAX     /* MAX NUM */
} ENC_LANGUAGE_E;

typedef struct
{
    AJ_POINT_F point_left;  // 垂直线左端点
    AJ_POINT_F point_right; // 垂直线右端点

    AJ_POINT_F arrow_point_left_point_left;  // 垂直线左箭头左端点位置
    AJ_POINT_F arrow_point_left_point_right; // 垂直线左箭头左右点位置

    AJ_POINT_F arrow_point_right_point_left;  // 垂直线右箭头左端点位置
    AJ_POINT_F arrow_point_right_point_right; // 垂直线右箭头左右点位置

    int bDrawLeftArrow;
    int bDrawRightArrow;
} AuxLineStruct;

#define GBK_LEAD_BYTE_MIN (0x81) // GBK首字节最小值（0x81~0xFE）

static const char g_FontList[ENC_LANGUAGE_MAX][64] =
    {
        "unicode_16x16.font",
        "unicode_16x16-ch.font",
        "unicode_16x16-en.font",
        "unicode_16x16_jp.font",
        "unicode_16x16-ko.font",
        "unicode_16x16-ru.font"};

static const char *week_day_chs[16] = // UTF8
    {
        "\xE6\x98\x9F\xE6\x9C\x9F\xE6\x97\xA5",
        "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xB8\x80",
        "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xBA\x8C",
        "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xB8\x89",
        "\xE6\x98\x9F\xE6\x9C\x9F\xE5\x9B\x9B",
        "\xE6\x98\x9F\xE6\x9C\x9F\xE4\xBA\x94",
        "\xE6\x98\x9F\xE6\x9C\x9F\xE5\x85\xAD",
};

static const char *week_day_eng[16] =
    {
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday"};

static const char week_day_russion[8][16] =
    {
        "\xD0\x92\xD0\xA1",
        "\xD0\x9F\xD0\x9D",
        "\xD0\x92\xD0\xA2",
        "\xD0\xA1\xD0\xA0",
        "\xD0\xA7\xD0\xA2",
        "\xD0\x9F\xD0\xA2",
        "\xD0\xA1\xD0\x91"};

static unsigned char s_u8OsdInverseLastColor[ANJ_CAMERA_MAX_NUMS][OSD_INVERSE_BLOCK_W * OSD_INVERSE_BLOCK_H] = {0};

typedef struct
{
    OverlayTextEnum nType;
    char *szTextChn;
    char *szTextEng;
} OverlayTextParam;

OverlayTextParam s_stOverlayText[] =
    {
        {OVERLAY_PTZ_RESET, "云台复位中...", "PTZ Reseting..."},
        {OVERLAY_PTZ_GOTO_GARDPOS, "云台回看守位中", "PTZ returning to guard position "},
        {OVERLAY_PTZ_GOTO_LASTPOS, "云台回记忆位中", "PTZ returning to last position "},
        {OVERLAY_PTZ_GOTO_DEFAULTPOS, "云台回默认位置中", "PTZ returning to default position "},
        {OVERLAY_PTZ_GOTO_PRESET, "云台正在前往预置位", "PTZ going to preset position "},
        {OVERLAY_SDCARD_UNINIT, "存储卡未初始化", "SDCard NOT Initialized "},
        {OVERLAY_SDCARD_RDONLY, "存储卡只读", "SDCard Readonly "},
        {OVERLAY_SDCARD_ERROR, "存储卡写入失败", "SDCard NOT Initialized "},
        {OVERLAY_RECORD_BMP, "/opt/ch/record32_for_i2_square.bmp", "/opt/ch/record16_for_i2_square.bmp"},
        {OVERLAY_RECORD_BMP0, "/opt/ch/record32_for_i2_square0.bmp", "/opt/ch/record16_for_i2_square0.bmp"},
        {OVERLAY_RECORD_BMP1, "/opt/ch/record32_for_i2_square1.bmp", "/opt/ch/record16_for_i2_square1.bmp"},
        {OVERLAY_RECORD_BMP2, "/opt/ch/record32_for_i2_square2.bmp", "/opt/ch/record16_for_i2_square2.bmp"},
        {OVERLAY_CLOUD_BMP, "/opt/ch/cloud32_for_i2.bmp", "/opt/ch/cloud16_for_i2.bmp"},
        {OVERLAY_LENS_COVER, "镜头遮挡中", "Lens Cover"},
        {OVERLAY_ZOOM_DIGITAL, "", ""},
        {OVERLAY_ZOOM_PROGRESS, "", ""},
        {OVERLAY_PTZ_SWITCH_TO_IR_MODE, "切换到纯红外模式", "Switch to IR mode"},
        {OVERLAY_PTZ_SWITCH_TO_WHITE_MODE, "切换到纯白光模式", "Switch to white mode"},
        {OVERLAY_PTZ_SWITCH_TO_DOUBLE_MODE, "切换到双光模式", "Switch to dual-light mode"},
        {OVERLAY_PTZ_LINE_SCAN_ON, "线扫开启", "Line scan enabled"},
        {OVERLAY_PTZ_CRUISE_ON, "巡航开启", "Cruise enabled"},
        {OVERLAY_PTZ_LINE_SCAN_OFF, "线扫关闭", "Line scan disabled"},
        {OVERLAY_PTZ_CRUISE_OFF, "巡航关闭", "Cruise disabled"},
        {OVERLAY_PTZ_LINE_SCAN_SET_LEFT, "设置左边界", "Left scan boundary set"},
        {OVERLAY_PTZ_LINE_SCAN_SET_RIGHT, "设置右边界", "Right scan boundary set"},
        {OVERLAY_PTZ_WATCH_GUARD_SET_OK, "看守位设置成功", "Guard preset set successfully"},
        {OVERLAY_PTZ_WATCH_GUARD_SET_FAIL, "看守位设置失败", "Guard preset set failed"},
        {OVERLAY_PTZ_WATCH_GUARD_CLEAR, "删除看守位", "Guard preset deleted successfully"},
        {OVERLAY_PTZ_ZOOM_TRACK_OFF, "变倍跟踪关闭", "Zoom tracking disabled"},
        {OVERLAY_PTZ_ZOOM_TRACK_ON, "变倍跟踪启用", "Zoom tracking enabled"},
        {OVERLAY_PTZ_SET_PRESET, "预置点添加成功", "Preset added successfully"},
        {OVERLAY_PTZ_CLEAR_PRESET, "预置点删除成功", "Preset deleted successfully"},
        {}};

#define OSD_CHARACTER_PIXEL_WIDTH (8)
#define OSD_CHARACTER_PIXEL_HEIGHT (16)
#define OSD_CHARACTER_PIXEL_EDGE (1)

typedef struct font_library
{
    Unicode_FontHead_t font_library_head;
    Unicode_FontData_t *font_library_data;
    ENC_LANGUAGE_E font_libray_language;
} font_library_t;

typedef struct overlay_thread_param
{
    int b_overlay_update;
    int b_use_simple_font_lib;
    int nic_type;
    rgn_pixel_format_e rgn_pixel_format;
    osd_custom_content_s custom_content;
    osd_custom_content_s content_4g;
    osd_custom_content_s bitmap_content;
    osd_custom_content_s cloud_content;
    osd_custom_content_s battery_content;
    osd_custom_content_s sdcard_content;
    osd_custom_content_s isp_debug_content;
    int b_custom_update;
    int b_4g_update;
    int b_bitmap_update;
    int b_cloud_update;
    int b_sdcard_update;
    int b_isp_debug_update;
    int b_battery_update;

    int PolygonTwinkleTimes;
    int FrameBorderTwinkleTimes;
    int CrossLineTwinkleTimes[MAX_VIDEO_VG_LINE];
    ANJ_SIZE_S astResolution[MAX_VIDEO_NUM];
    ANJ_SIZE_S astRealRes[MAX_VIDEO_NUM];
    anj_thread_s osd_thread;
} overlay_thread_param_t;

static overlay_thread_param_t *pstOverlay = NULL;
static cover_param_s gstCoverRaram[MAX_VIDEO_NUM][MAX_VIDEO_MASK_AREA] = {0};
static rect_param_s gstRectRaram[MAX_VIDEO_NUM] = {0};
static line_param_s gstPolygonParam[MAX_VIDEO_NUM] = {0};
static line_param_s gstCrossLineParam[MAX_VIDEO_NUM] = {0};
static line_param_s gstFrameBorderParam[MAX_VIDEO_NUM] = {0};
static pthread_mutex_t s_stOsdRectMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t s_gAnjOsdCtrlMutex;
static char s_osd_cpu_info[OSD_SHOW_CONTENT_BUF_LEN] = {0};

#define PAI_VALUE 3.14159265358979323846

#define MAX_GLYPH_CACHE 1024
#define OSD_BITMAP_PIXEL_TEXT (1)
#define OSD_BITMAP_PIXEL_EDGE (2)

static void anj_osd_isp_debug_flag_check(void);

static int anj_osd_lens_cover_area_check(MASK_AREA_ENTRY *pstMaskArea, ANJ_SIZE_S *resolution)
{
    if (pstMaskArea == NULL)
    {
        return 0;
    }

    return (pstMaskArea->xPos == 0 &&
            pstMaskArea->yPos == 0 &&
            pstMaskArea->width >= resolution->u32Width &&
            pstMaskArea->height >= resolution->u32Height);
}

typedef struct
{
    unsigned int unicode;
    int font_factor;
    int w;
    int h;
    unsigned char *bitmap;
    int used;
} glyph_cache_entry_t;

static glyph_cache_entry_t g_glyph_cache[MAX_GLYPH_CACHE];

typedef struct
{
    unsigned char text_color;
    unsigned char bg_color;
    unsigned char edge_color;
    unsigned char draw_bg;
    unsigned char draw_edge;
    unsigned char inverse_pixel;
} anj_osd_style_render_t;

static void anj_osd_style_render_get(short style, anj_osd_style_render_t *pStyle)
{
    if (pStyle == NULL)
    {
        return;
    }

    pStyle->text_color = RGB_VALUE_WHITE;
    pStyle->bg_color = RGB_VALUE_TRANSPARENT;
    pStyle->edge_color = RGB_VALUE_BLACK;
    pStyle->draw_bg = 0;
    pStyle->draw_edge = 0;
    pStyle->inverse_pixel = 0;

    switch (style)
    {
    case AJ_OVERLAY_STYLE_BLACK_WHITE:
        pStyle->text_color = RGB_VALUE_BLACK;
        pStyle->bg_color = RGB_VALUE_WHITE;
        pStyle->draw_bg = 1;
        break;
    case AJ_OVERLAY_STYLE_WHITE_BLACK:
        pStyle->text_color = RGB_VALUE_WHITE;
        pStyle->bg_color = RGB_VALUE_BLACK;
        pStyle->draw_bg = 1;
        break;
    case AJ_OVERLAY_STYLE_TRANSPARENT_BLACKWHITE:
        pStyle->text_color = RGB_VALUE_BLACK;
        pStyle->edge_color = RGB_VALUE_WHITE;
        pStyle->draw_edge = 1;
        break;
    case AJ_OVERLAY_STYLE_TRANSPARENT_WHITEBLACK:
        pStyle->text_color = RGB_VALUE_WHITE;
        pStyle->edge_color = RGB_VALUE_BLACK;
        pStyle->draw_edge = 1;
        break;
    case AJ_OVERLAY_STYLE_TRANSPARENT_BLACK:
        pStyle->text_color = RGB_VALUE_BLACK;
        break;
    case AJ_OVERLAY_STYLE_TRANSPARENT_WHITE:
        pStyle->text_color = RGB_VALUE_WHITE;
        break;
    case AJ_OVERLAY_STYLE_INVERSE_COLOR:
        pStyle->text_color = RGB_VALUE_WHITE;
        pStyle->bg_color = RGB_VALUE_TRANSPARENT;
        pStyle->draw_bg = 0;
        pStyle->inverse_pixel = 1;
        break;
    default:
        pStyle->text_color = RGB_VALUE_WHITE;
        pStyle->edge_color = RGB_VALUE_BLACK;
        pStyle->draw_edge = 1;
        break;
    }
}

static void anj_osd_overlay_style_apply(overlay_param_s *overlay_param, short style)
{
    anj_osd_style_render_t stRender = {0};
    if (overlay_param == NULL)
    {
        return;
    }

    anj_osd_style_render_get(style, &stRender);
    overlay_param->style = style;
    overlay_param->text_color = stRender.text_color;
    overlay_param->bg_color = stRender.bg_color;
    overlay_param->edge_color = stRender.edge_color;
    overlay_param->draw_bg = stRender.draw_bg;
    overlay_param->draw_edge = stRender.draw_edge;
    overlay_param->inverse_pixel = stRender.inverse_pixel;
}

void vRotationTransform(double dAngle, AJ_POINT_F *point, AJ_POINT_F *point_new)
{
    point_new->fX = point->fX * cos(dAngle) - point->fY * sin(dAngle);
    point_new->fY = point->fX * sin(dAngle) + point->fY * cos(dAngle);
}

int GetSquarePointBD(AJ_POINT_F *pointC, AJ_POINT_F *pointB, AJ_POINT_F *pointD)
{
    // 正方形以A点为原点，已知C点x y坐标，求B/D坐标

    double angle45 = PAI_VALUE / 4.0;
    double angle270 = PAI_VALUE * 2 * 3 / 4.0;
    double lengthBeveledge = sqrt(pow(pointC->fX, 2) + pow(pointC->fY, 2));    // 斜边长
    double lengthSide = sqrt((pow(pointC->fX, 2) + pow(pointC->fY, 2)) / 2.0); // 正方形边长

    AJ_POINT_F point_rotate;
    point_rotate.fX = (lengthSide / lengthBeveledge) * pointC->fX;
    point_rotate.fY = (lengthSide / lengthBeveledge) * pointC->fY;

    vRotationTransform(angle45, &point_rotate, pointD);
    vRotationTransform(angle270, pointD, pointB);

    return 0;
}

static unsigned int utf8_to_unicode(const char *utf8, int *bytes_used)
{
    unsigned char c = (unsigned char)utf8[0];
    *bytes_used = 1;

    if (c <= 0x7F)
    {
        // 1byte
        return c;
    }
    if ((c & 0xE0) == 0xC0)
    {
        // 2byte
        if (utf8[1])
        {
            *bytes_used = 2;
            return ((c & 0x1F) << 6) | (utf8[1] & 0x3F);
        }
    }
    if ((c & 0xF0) == 0xE0)
    {
        // 3byte
        if (utf8[1] && utf8[2])
        {
            *bytes_used = 3;
            return ((c & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
        }
    }
    return 0xFFFD;
}

static Unicode_FontData_t *unicode_font_data_get(unsigned int unicode, font_library_t *font_library)
{
    for (int i = 0; i < font_library->font_library_head.chars_count; i++)
    {
        if (font_library->font_library_data[i].unicode == unicode)
        {
            return &font_library->font_library_data[i];
        }
    }

    return NULL;
}

/* 查找缓存 */
static glyph_cache_entry_t *glyph_cache_find(unsigned int unicode, int font_factor)
{
    for (int i = 0; i < MAX_GLYPH_CACHE; i++)
    {
        if (g_glyph_cache[i].used && g_glyph_cache[i].unicode == unicode && g_glyph_cache[i].font_factor == font_factor)
            return &g_glyph_cache[i];
    }
    return NULL;
}

/* 简单淘汰：找空槽或替换最久未使用的 */
static glyph_cache_entry_t *glyph_cache_alloc_slot(void)
{
    for (int i = 0; i < MAX_GLYPH_CACHE; i++)
    {
        if (!g_glyph_cache[i].used)
            return &g_glyph_cache[i];
    }
    glyph_cache_entry_t *p = &g_glyph_cache[0];
    if (p->bitmap)
    {
        anj_mw_free(p->bitmap);
        p->bitmap = NULL;
    }
    p->used = 0;
    return p;
}

static glyph_cache_entry_t *glyph_cache_render(unsigned int unicode, int font_factor, font_library_t *font_library)
{
    if (!font_library)
        return NULL;

    glyph_cache_entry_t *exist = glyph_cache_find(unicode, font_factor);
    if (exist)
    {
        return exist;
    }

    Unicode_FontData_t *pFont = unicode_font_data_get(unicode, font_library);
    if (!pFont)
    {
        return NULL;
    }

    glyph_cache_entry_t *slot = glyph_cache_alloc_slot();
    if (!slot)
    {
        return NULL;
    }

    int src_canvas_w = OSD_CHARACTER_PIXEL_WIDTH * 2;
    int src_h = OSD_CHARACTER_PIXEL_HEIGHT;
    int src_w = src_canvas_w;
    if (unicode < 0x80)
    {
        src_w = OSD_CHARACTER_PIXEL_WIDTH;
    }
    int dst_w = src_w * font_factor;
    int dst_h = src_h * font_factor;
    int buf_size = dst_w * dst_h;
    unsigned char src_bitmap[OSD_CHARACTER_PIXEL_WIDTH * 2 * OSD_CHARACTER_PIXEL_HEIGHT] = {0};
    int edge_thickness = (font_factor > 1) ? 2 : 1;

    if (edge_thickness > font_factor)
    {
        edge_thickness = font_factor;
    }

    slot->bitmap = (unsigned char *)anj_mw_malloc(buf_size);
    if (!slot->bitmap)
    {
        return NULL;
    }
    memset(slot->bitmap, 0, buf_size);

    for (int sy = 0; sy < src_h; sy++)
    {
        unsigned char byte1 = pFont->mask[sy * 2];
        unsigned char byte2 = pFont->mask[sy * 2 + 1];
        for (int sx = 0; sx < src_canvas_w; sx++)
        {
            int bit = 0;
            if (sx < 8)
                bit = (byte1 >> (7 - sx)) & 1;
            else
                bit = (byte2 >> (7 - (sx - 8))) & 1;

            if (bit && sx < src_w)
            {
                src_bitmap[sy * src_w + sx] = OSD_BITMAP_PIXEL_TEXT;
            }
        }
    }

    /* 先在原始字模上描边，再整体放大，避免放大后再描边造成角点阴影感 */
    for (int y = 0; y < src_h; y++)
    {
        for (int x = 0; x < src_w; x++)
        {
            if (src_bitmap[y * src_w + x] == OSD_BITMAP_PIXEL_TEXT)
            {
                for (int oy = -1; oy <= 1; oy++)
                {
                    int ny = y + oy;
                    if (ny < 0 || ny >= src_h)
                        continue;
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if ((ox == 0 && oy == 0) || (ox != 0 && oy != 0))
                            continue;
                        int nx = x + ox;
                        if (nx < 0 || nx >= src_w)
                            continue;
                        unsigned char *p = &src_bitmap[ny * src_w + nx];
                        if (*p == 0)
                            *p = OSD_BITMAP_PIXEL_EDGE;
                    }
                }
            }
        }
    }

    /* 文字按块放大 */
    for (int sy = 0; sy < src_h; sy++)
    {
        for (int sx = 0; sx < src_w; sx++)
        {
            if (src_bitmap[sy * src_w + sx] != OSD_BITMAP_PIXEL_TEXT)
            {
                continue;
            }

            int dst_x0 = sx * font_factor;
            int dst_y0 = sy * font_factor;
            for (int yy = 0; yy < font_factor; yy++)
            {
                unsigned char *row = slot->bitmap + (dst_y0 + yy) * dst_w;
                for (int xx = 0; xx < font_factor; xx++)
                {
                    row[dst_x0 + xx] = OSD_BITMAP_PIXEL_TEXT;
                }
            }
        }
    }

    for (int sy = 0; sy < src_h; sy++)
    {
        for (int sx = 0; sx < src_w; sx++)
        {
            if (src_bitmap[sy * src_w + sx] != OSD_BITMAP_PIXEL_EDGE)
            {
                continue;
            }

            int dst_x0 = sx * font_factor;
            int dst_y0 = sy * font_factor;

            if (sx > 0 && src_bitmap[sy * src_w + sx - 1] == OSD_BITMAP_PIXEL_TEXT)
            {
                for (int yy = 0; yy < font_factor; yy++)
                {
                    unsigned char *row = slot->bitmap + (dst_y0 + yy) * dst_w;
                    for (int xx = 0; xx < edge_thickness; xx++)
                    {
                        row[dst_x0 + xx] = OSD_BITMAP_PIXEL_EDGE;
                    }
                }
            }

            if (sx + 1 < src_w && src_bitmap[sy * src_w + sx + 1] == OSD_BITMAP_PIXEL_TEXT)
            {
                for (int yy = 0; yy < font_factor; yy++)
                {
                    unsigned char *row = slot->bitmap + (dst_y0 + yy) * dst_w;
                    for (int xx = 0; xx < edge_thickness; xx++)
                    {
                        row[dst_x0 + font_factor - edge_thickness + xx] = OSD_BITMAP_PIXEL_EDGE;
                    }
                }
            }

            if (sy > 0 && src_bitmap[(sy - 1) * src_w + sx] == OSD_BITMAP_PIXEL_TEXT)
            {
                for (int yy = 0; yy < edge_thickness; yy++)
                {
                    unsigned char *row = slot->bitmap + (dst_y0 + yy) * dst_w;
                    for (int xx = 0; xx < font_factor; xx++)
                    {
                        row[dst_x0 + xx] = OSD_BITMAP_PIXEL_EDGE;
                    }
                }
            }

            if (sy + 1 < src_h && src_bitmap[(sy + 1) * src_w + sx] == OSD_BITMAP_PIXEL_TEXT)
            {
                for (int yy = 0; yy < edge_thickness; yy++)
                {
                    unsigned char *row = slot->bitmap + (dst_y0 + font_factor - edge_thickness + yy) * dst_w;
                    for (int xx = 0; xx < font_factor; xx++)
                    {
                        row[dst_x0 + xx] = OSD_BITMAP_PIXEL_EDGE;
                    }
                }
            }
        }
    }

    slot->unicode = unicode;
    slot->font_factor = font_factor;
    slot->w = dst_w;
    slot->h = dst_h;
    slot->used = 1;

    return slot;
}

static int anj_osd_border_width_get(int u32Width)
{
    int border_width = 3;
    if (u32Width <= 1280)
        border_width = 2;
    else if (u32Width <= 1920)
        border_width = 3;
    else if (u32Width <= 2304)
        border_width = 3;
    else if (u32Width <= 2560)
        border_width = 4;
    else if (u32Width <= 2688)
        border_width = 4;
    else if (u32Width <= 3072)
        border_width = 5;
    else if (u32Width <= 3840)
        border_width = 6;

    return border_width;
}

static int anj_osd_language_type_get(ENC_LANGUAGE_E *language_type, char *language)
{
    int iRet = 0;
    ANJ_CHK(((language_type != NULL) && (language != NULL)), -1, "input Invalid");

    if ((strstr(language, "zh-cn") != NULL) || (strstr(language, "zh_cn") != NULL))
    {
        *language_type = ENC_LANGUAGE_CN;
    }
    else if ((strstr(language, "en-us") != NULL) || (strstr(language, "en_us") != NULL))
    {
        *language_type = ENC_LANGUAGE_EN;
    }
    else if ((strstr(language, "zh-tw") != NULL) || (strstr(language, "zh_tw") != NULL))
    {
        *language_type = ENC_LANGUAGE_TW;
    }
    else if ((strstr(language, "ru-ru") != NULL) || (strstr(language, "ru_ru") != NULL))
    {
        *language_type = ENC_LANGUAGE_RU;
    }
    else if (strstr(language, "korean") != NULL)
    {
        *language_type = ENC_LANGUAGE_KO;
    }
    else if (strstr(language, "Japanese") != NULL)
    {
        *language_type = ENC_LANGUAGE_JP;
    }

endFunc:
    return iRet;
}

static const char *anj_osd_weekday_to_str(int index, ENC_LANGUAGE_E language)
{
    if (ENC_LANGUAGE_CN == language || ENC_LANGUAGE_TW == language)
    {
        return week_day_chs[index];
    }
    else if (ENC_LANGUAGE_RU == language)
    {
        return week_day_russion[index];
    }
    else
    {
        return week_day_eng[index];
    }
}

static void anj_osd_isp_debug_build(int iCameraIdex, int VencChn, char *buf, int buf_len)
{
    AnjIspAeInfo stAe = {0};
    AnjIspCtlInfo *pIsp = getIspctlInfo();

    if (buf == NULL || buf_len <= 0)
    {
        return;
    }

    anj_mw_media_isp_aeinfo_get(iCameraIdex, &stAe);
    snprintf(buf, buf_len,
            "%6d %7d %5.1f %3d %3d %3d^[%5d %5d] [%7d %7d]",
            anj_mw_media_sys_temp_get(), stAe.bvTarget, stAe.curGain, stAe.expShutter, stAe.sceneTarget, stAe.lv,
            pIsp->control_runtime.white_pwm_target, pIsp->control_runtime.red_pwm_target,
            pIsp->control_runtime.light_threshold.white_open_th, pIsp->control_runtime.light_threshold.red_close_th);
}

/*****************************************************************************
 函 数 名  : anj_osd_str_replace
 功能描述  : 将OSD替换成有效的调试信息
 输入参数  : input OSD字符串
 输出参数  : 无
 返 回 值  : 无
*****************************************************************************/
static void anj_osd_str_replace(int iCameraIdex, int VencChn, char *input)
{
    typedef struct
    {
        const char *key;
        int max_replace_len;
    } osd_token_s;

    static const osd_token_s tokens[] = {
        {"gain", 16},
        {"bv", 16},
        {"t", 16},
        {"lv", 16},
        {"lumy", 16},
        {"sTarget", 16},
        {"Temp", 16},
        {"wpwm", 16},
        {"rpwm", 16},
        {"openW", 20},
        {"closeR", 24},
        {"closeW", 24},
        {"dn", 16},
        {"viewer", 16},
        {"fps", 16},
        {"lowBv", 12},
        {"aiisp", 12},
        {"wForce", 16},
        {"cpu", OSD_SHOW_CONTENT_BUF_LEN},
        {"gyro", 80},
    };

    const char *src = input;
    char output[OSD_SHOW_CONTENT_BUF_LEN] = {0};
    char cpu_replacement[OSD_SHOW_CONTENT_BUF_LEN] = {0};
    char gyro_replacement[80] = {0};
    int out_len = 0;
    int replaced = 0;
    AnjIspCtlInfo *pstIspctlInfo = getIspctlInfo();
    AnjIspAeInfo stAnjIspAeInfo = {0};

    if (input == NULL || input[0] == '\0')
    {
        return;
    }

    anj_mw_media_isp_aeinfo_get(iCameraIdex, &stAnjIspAeInfo);

    while (*src != '\0' && out_len < (int)sizeof(output) - 1)
    {
        unsigned int i = 0;
        int matched = 0;

        for (i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
        {
            int key_len = strlen(tokens[i].key);

            if (strncmp(src, tokens[i].key, key_len) != 0 || src[key_len] != '[')
            {
                continue;
            }

            const char *end = strchr(src + key_len + 1, ']');
            char replacement[32] = {0};
            const char *replace_ptr = replacement;
            int written = 0;

            if (end == NULL)
            {
                continue;
            }

            switch (i)
            {
            case 0:
                written = snprintf(replacement, sizeof(replacement), "gain[%5.1f]", stAnjIspAeInfo.curGain);
                break;
            case 1:
                written = snprintf(replacement, sizeof(replacement), "bv[%7d]", stAnjIspAeInfo.bvTarget);
                break;
            case 2:
                written = snprintf(replacement, sizeof(replacement), "t[%3d]", stAnjIspAeInfo.expShutter);
                break;
            case 3:
                written = snprintf(replacement, sizeof(replacement), "lv[%3d]", stAnjIspAeInfo.lv);
                break;
            case 4:
                written = snprintf(replacement, sizeof(replacement), "lumy[%4d]", stAnjIspAeInfo.lumy);
                break;
            case 5:
                written = snprintf(replacement, sizeof(replacement), "sTarget[%3d]", stAnjIspAeInfo.sceneTarget);
                break;
            case 6:
                written = snprintf(replacement, sizeof(replacement), "Temp[%6d]", anj_mw_media_sys_temp_get());
                break;
            case 7:
                written = snprintf(replacement, sizeof(replacement), "wpwm[%5d]", pstIspctlInfo->control_runtime.white_pwm_target);
                break;
            case 8:
                written = snprintf(replacement, sizeof(replacement), "rpwm[%5d]", pstIspctlInfo->control_runtime.red_pwm_target);
                break;
            case 9:
                written = snprintf(replacement, sizeof(replacement), "openW[%7d]", pstIspctlInfo->control_runtime.light_threshold.white_open_th);
                break;
            case 10:
                written = snprintf(replacement, sizeof(replacement), "closeR[%7d]", pstIspctlInfo->control_runtime.light_threshold.red_close_th);
                break;
            case 11:
                written = snprintf(replacement, sizeof(replacement), "closeW[%7d]", pstIspctlInfo->control_runtime.light_threshold.white_close_th);
                break;
            case 12:
                if (pstIspctlInfo->control_runtime.mLoopDecision.apply_pq_mode == PQBIN_MODE_NONE)
                {
                    written = snprintf(replacement, sizeof(replacement), "dn[ null]");
                }
                else if (pstIspctlInfo->control_runtime.mLoopDecision.apply_pq_mode == PQBIN_MODE_RGB)
                {
                    written = snprintf(replacement, sizeof(replacement), "dn[  day]");
                }
                else if (pstIspctlInfo->control_runtime.mLoopDecision.apply_pq_mode == PQBIN_MODE_MONO)
                {
                    written = snprintf(replacement, sizeof(replacement), "dn[night]");
                }
                else
                {
                    written = snprintf(replacement, sizeof(replacement), "dn[xxxxx]");
                }
                break;
            case 13:
                written = snprintf(replacement, sizeof(replacement), "viewer[%2d]", anj_sysmng_viewer_get());
                break;
            case 14:
                written = snprintf(replacement, sizeof(replacement), "fps[%2d]", anj_video_fps_get(iCameraIdex, VencChn));
                break;
            case 15:
                written = snprintf(replacement, sizeof(replacement), "lowBv[%d]",
                                   pstIspctlInfo->control_runtime.low_bv_state.low_bv);
                break;
            case 16:
                written = snprintf(replacement, sizeof(replacement), "aiisp[%s]",
                                   pstIspctlInfo->control_runtime.aiisp_enabled ? "on" : "off");
                break;
            case 17:
            {
                const char *forceState = "unk";
                switch (pstIspctlInfo->control_runtime.white_alarm.force_recover)
                {
                case ISPCTL_WHITE_FORCE_STATE_NONE:
                    forceState = "none";
                    break;
                case ISPCTL_WHITE_FORCE_STATE_WAIT_MOTION:
                    forceState = "wmot";
                    break;
                case ISPCTL_WHITE_FORCE_STATE_WAIT_REDETECT:
                    forceState = "wred";
                    break;
                default:
                    break;
                }
                written = snprintf(replacement, sizeof(replacement), "wForce[%s]", forceState);
                break;
            }
            case 18:
            {
                written = snprintf(cpu_replacement, sizeof(cpu_replacement), "%s", s_osd_cpu_info);
                replace_ptr = cpu_replacement;
                break;
            }
            case 19:
            {
                event_gyro_data_t gyro_data;
                EventResult event_result = {0};
                memset(&gyro_data, 0, sizeof(gyro_data));
                event_result.ret = -1;
                eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_GYRO_DATA_GET, &event_result, &gyro_data);
                if (event_result.ret != 0)
                {
                    written = snprintf(gyro_replacement, sizeof(gyro_replacement),
                                       "gyro[  ---,  ---,  ---]\n"
                                       "acc[ ---, ---, ---]\n");
                }
                else
                {
                    written = snprintf(gyro_replacement, sizeof(gyro_replacement),
                                       "gyro[%5.1f,%5.1f,%5.1f]\n"
                                       "acc[%4.2f,%4.2f,%4.2f]\n",
                                       gyro_data.gyro_x, gyro_data.gyro_y, gyro_data.gyro_z,
                                       gyro_data.accel_x, gyro_data.accel_y, gyro_data.accel_z);
                }
                replace_ptr = gyro_replacement;
                break;
            }
            default:
                break;
            }

            if (written <= 0)
            {
                break;
            }

            if (written >= tokens[i].max_replace_len)
            {
                __DBG("OSD replace too long, key='%s', replacement='%s', written=%d, max=%d\n",
                      tokens[i].key, replace_ptr, written, tokens[i].max_replace_len);
                break;
            }

            if (out_len + written >= (int)sizeof(output))
            {
                __DBG("OSD replace truncated, input='%s'\n", input);
                output[out_len] = '\0';
                snprintf(input, sizeof(output), "%s", output);
                return;
            }

            memcpy(output + out_len, replace_ptr, written);
            out_len += written;
            src = end + 1;
            matched = 1;
            replaced = 1;
            break;
        }

        if (!matched)
        {
            output[out_len++] = *src++;
        }
    }

    output[out_len] = '\0';
    if (replaced)
    {
        snprintf(input, sizeof(output), "%s", output);
    }
}

static int anj_osd_load_bmp_file(const char *filename, overlay_param_s *overlay_param)
{
    int iRet = -1;
    FILE *fp = NULL;
    ANJ_CHK(((filename != NULL) && (overlay_param != NULL)), -1, "input Invalid");

    fp = fopen(filename, "rb");
    ANJ_CHK((fp != NULL), -1, "load err!");

    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    // 读取文件头
    anj_mw_fread(fp, &file_header, sizeof(BMPFileHeader));

    // 检查BMP文件标识
    if (file_header.bfType != 0x4D42)
    { // "BM"
        __ERR("Invalid BMP file format\n");
        goto endFunc;
    }

    // 读取信息头
    anj_mw_fread(fp, &info_header, sizeof(BMPInfoHeader));

    // 只支持24位BMP
    if (info_header.biBitCount != 24)
    {
        __ERR("Unsupported BMP bit depth: %d\n", info_header.biBitCount);
        goto endFunc;
    }

    overlay_param->width = info_header.biWidth;
    overlay_param->height = abs(info_header.biHeight); // 高度可能是负数

    // 计算每行字节数（BMP每行需要4字节对齐）
    int bytes_per_pixel = info_header.biBitCount / 8;
    int stride = (overlay_param->width * bytes_per_pixel + 3) & ~3;

    // 分配内存存储BMP数据
    int bmp_data_size = stride * overlay_param->height;
    overlay_param->bmp_data = (unsigned char *)anj_mw_malloc(bmp_data_size);
    overlay_param->bmp_len = bmp_data_size;
    ANJ_CHK((overlay_param->bmp_data != NULL), -1, "malloc failed!");

    // 跳转到位图数据
    fseek(fp, file_header.bfOffBits, SEEK_SET);

    // 读取位图数据（BMP是倒序存储的）
    for (int y = overlay_param->height - 1; y >= 0; y--)
    {
        anj_mw_fread(fp, overlay_param->bmp_data + y * stride, stride);
    }
    iRet = 0;
endFunc:
    if (fp)
    {
        anj_mw_fclose(fp);
    }
    __INFO("iRet:%d bmp path:%s\n", iRet, filename);
    return iRet;
}

static void anj_osd_bmp_path_extract_by_index(const char *src, char *dst, size_t dst_size, int index)
{
    if (dst == NULL || dst_size == 0)
    {
        return;
    }
    dst[0] = '\0';
    if (src == NULL)
    {
        return;
    }

    if (index < 0)
    {
        return;
    }

    // 把字符串按 ';' 分段：index=0 取第1段，index=1 取第2段（主码流/子码流）。
    const char *p = src;
    for (int i = 0; i < index; i++)
    {
        const char *q = strchr(p, ';');
        if (q == NULL)
        {
            return; // 第 index 段不存在
        }
        p = q + 1;
    }

    const char *end = strchr(p, ';');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len >= dst_size)
    {
        len = dst_size - 1;
    }
    if (len > 0)
    {
        memcpy(dst, p, len);
    }
    dst[len] = '\0';
}

static int anj_osd_location_get(int posX, int posY, int posType, int *ratioX, int *ratioY)
{
    int overlay_location = OSD_LOCATION_CNT;
    *ratioX = posX;
    *ratioY = posY;
    if (posType == POSITION_TYPE_BY_FOUR_CORNER)
    {
        overlay_location = OSD_LOCATION_LEFT_TOP;
        if (posX == 0 && posY == 0)
        {
            *ratioX = 0;
            *ratioY = 0;
            overlay_location = OSD_LOCATION_LEFT_TOP;
        }
        if (posX == 0 && posY == 1)
        {
            *ratioX = 0;
            *ratioY = OSD_IOT_COORDINATE_RATIO;
            overlay_location = OSD_LOCATION_LEFT_BOTTOM;
        }
        if (posX == 1 && posY == 0)
        {
            *ratioX = OSD_IOT_COORDINATE_RATIO;
            *ratioY = 0;
            overlay_location = OSD_LOCATION_RIGHT_TOP;
        }
        if (posX == 1 && posY == 1)
        {
            *ratioX = OSD_IOT_COORDINATE_RATIO;
            *ratioY = OSD_IOT_COORDINATE_RATIO;
            overlay_location = OSD_LOCATION_RIGHT_BOTTOM;
        }
    }
    return overlay_location;
}

static int anj_osd_time_format_to_str(char *date_format, short time24or12, ENC_LANGUAGE_E language_type, struct tm *ptm, char *result_str, int result_str_len)
{
    int iRet = 0;
    ANJ_CHK(((ptm != NULL) && (date_format != NULL) && (result_str != NULL)), -1, "input Invalid");
    ANJ_CHK((result_str_len > 0), -1, "input Invalid");

    int bEnglish = 0;
    if (ENC_LANGUAGE_CN != language_type && ENC_LANGUAGE_TW != language_type)
    {
        bEnglish = 1;
    }
    const char *am_pm_chs[16] =
        {
            "  \xE4\xB8\x8A\xE5\x8D\x88",
            "  \xE4\xB8\x8B\xE5\x8D\x88"};

    const char *am_pm_eng[16] =
        {
            " AM",
            " PM"};

    char time_suffix[16] = {0};
    if (time24or12)
    {
        if (ptm->tm_hour >= 0 && ptm->tm_hour <= 12)
        {
            if (bEnglish)
                snprintf(time_suffix, sizeof(time_suffix), "%s", am_pm_eng[0]);
            else
                snprintf(time_suffix, sizeof(time_suffix), "%s", am_pm_chs[0]);
        }
        else
        {
            ptm->tm_hour = ptm->tm_hour - 12;
            if (bEnglish)
                snprintf(time_suffix, sizeof(time_suffix), "%s", am_pm_eng[1]);
            else
                snprintf(time_suffix, sizeof(time_suffix), "%s", am_pm_chs[1]);
        }
    }

    if (strcasecmp(date_format, anj_config_media_time_list_get(0)) == 0)
    {
        snprintf(result_str, result_str_len, "%04d-%02d-%02d %02d:%02d:%02d%s",
                 ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(1)) == 0)
    {
        snprintf(result_str, result_str_len, "%04d/%02d/%02d %02d:%02d:%02d%s",
                 ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(2)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d-%02d-%02d %02d:%02d:%02d%s",
                 (ptm->tm_year + 1900) % 100, ptm->tm_mon + 1, ptm->tm_mday,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(3)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d/%02d/%02d %02d:%02d:%02d%s",
                 (ptm->tm_year + 1900) % 100, ptm->tm_mon + 1, ptm->tm_mday,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(4)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d:%02d:%02d%s %02d/%02d/%04d",
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix,
                 ptm->tm_mday, ptm->tm_mon + 1, ptm->tm_year + 1900);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(5)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d:%02d:%02d%s %02d-%02d-%04d",
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix,
                 ptm->tm_mday, ptm->tm_mon + 1, ptm->tm_year + 1900);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(6)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d:%02d:%02d%s %02d/%02d/%04d",
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix,
                 ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_year + 1900);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(7)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d:%02d:%02d%s %02d-%02d-%04d",
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix,
                 ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_year + 1900);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(8)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d/%02d/%04d %02d:%02d:%02d%s",
                 ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_year + 1900,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix);
    }
    else if (strcasecmp(date_format, anj_config_media_time_list_get(9)) == 0)
    {
        snprintf(result_str, result_str_len, "%02d-%02d-%04d %02d:%02d:%02d%s",
                 ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_year + 1900,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix);
    }
    else
    {
        snprintf(result_str, result_str_len, "%04d-%02d-%02d %02d:%02d:%02d%s",
                 ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec, time_suffix);
    }

endFunc:
    return iRet;
}

static int anj_osd_time_format_is_time_first(const char *date_format)
{
    if (date_format == NULL)
    {
        return 0;
    }

    return (strcasecmp(date_format, anj_config_media_time_list_get(4)) == 0 ||
            strcasecmp(date_format, anj_config_media_time_list_get(5)) == 0 ||
            strcasecmp(date_format, anj_config_media_time_list_get(6)) == 0 ||
            strcasecmp(date_format, anj_config_media_time_list_get(7)) == 0);
}

static void anj_osd_insert_week_in_middle(char *output, int output_len, const char *week, int time_first, short time24or12)
{
    if (output == NULL || week == NULL || output_len <= 0)
    {
        return;
    }

    if (week[0] == '\0')
    {
        return;
    }

    char *split = strchr(output, ' ');
    if (split == NULL)
    {
        return;
    }

    /*
     * time_first + 12小时制时，字符串为 "HH:MM:SS AM DD/.."
     * 第一个空格在 AM 前，星期应插在 AM 与日期之间（第二个空格处）。
     */
    if (time_first && time24or12)
    {
        split = strchr(split + 1, ' ');
        if (split == NULL)
        {
            return;
        }
    }

    size_t head_len = (size_t)(split - output);
    size_t output_len_now = strlen(output);
    size_t week_len = strlen(week);
    size_t insert_len = week_len + 1; // " " + week

    if (output_len_now + insert_len >= (size_t)output_len)
    {
        return;
    }

    memmove(split + insert_len, split, output_len_now - head_len + 1);
    split[0] = ' ';
    memcpy(split + 1, week, week_len);
}

static int anj_osd_update_time_string(VideoOverlay *osd_cfg, ENC_LANGUAGE_E language_type, char *output, int output_len)
{
    int iRet = 0;
    ANJ_CHK(((osd_cfg != NULL) && (output != NULL)), -1, "input Invalid");
    ANJ_CHK((output_len > 0), -1, "input Invalid");

    struct tm ptm;
    SystemLocalTime(&ptm);

    const char *week = NULL;
    if (osd_cfg->bDsplayWeek == 1 || osd_cfg->bDsplayWeek == 2)
    {
        week = anj_osd_weekday_to_str(ptm.tm_wday, language_type);
    }

    anj_osd_time_format_to_str(osd_cfg->timeOverlay.timeFormat.format,
                               osd_cfg->time24or12,
                               language_type,
                               &ptm,
                               output,
                               output_len);

    /* bDsplayWeek: 0 不显示；1 日期与时间中间；2 末尾 */
    if (osd_cfg->bDsplayWeek == 1)
    {
        int time_first = anj_osd_time_format_is_time_first(osd_cfg->timeOverlay.timeFormat.format);
        anj_osd_insert_week_in_middle(output, output_len, week, time_first, osd_cfg->time24or12);
    }

    if (osd_cfg->bDsplayWeek == 2 && week && week[0] && strlen(output) + strlen(week) + 1 < (size_t)output_len)
    {
        strcat(output, " ");
        strcat(output, week);

        int i = 0;
        for (i = 0; i < (int)strlen("Wednesday") - (int)strlen(week); i++)
        {
            if (strlen(output) + 1 < (size_t)output_len)
                strcat(output, " ");
        }
    }

endFunc:
    return iRet;
}

static int anj_osd_font_factor_from_fontsize(short fontsize, ANJ_SIZE_S *resolution)
{
    int base_factor = 1;

    if (resolution->u32Width * resolution->u32Height >= 2560 * 1440)
    {
        base_factor = 4;
    }
    else if (resolution->u32Width * resolution->u32Height >= 1920 * 1080)
    {
        base_factor = 3;
    }
    else if (resolution->u32Width * resolution->u32Height >= 1280 * 720)
    {
        base_factor = 2;
    }

    if (fontsize <= 0)
    {
        return base_factor;
    }

    if (fontsize == 1)
    {
        return base_factor * 3 / 2;
    }

    return base_factor * 2;
}

static void anj_osd_four_corner_pos_calc(ANJ_SIZE_S *resolution, int overlay_location,
                                         int font_factor, int overlay_width, int overlay_height,
                                         int *pos_x, int *pos_y)
{
    int x_left;
    int x_right;
    int y_top;

    if (font_factor <= 0)
    {
        font_factor = 1;
    }

    x_left = FONT_W * font_factor;
    x_right = (int)resolution->u32Width - overlay_width - FONT_W * font_factor;
    if (x_right < FONT_W)
    {
        x_right = FONT_W;
    }

    switch (overlay_location)
    {
    case OSD_LOCATION_LEFT_BOTTOM:
        y_top = (int)resolution->u32Height - overlay_height - FONT_H * font_factor;
        if (y_top < FONT_H)
        {
            y_top = FONT_H;
        }
        *pos_x = x_left;
        *pos_y = y_top;
        break;
    case OSD_LOCATION_RIGHT_TOP:
        y_top = FONT_H * font_factor;
        if (y_top < FONT_H)
        {
            y_top = FONT_H;
        }
        *pos_x = ANJ_ALIGN_DOWN(x_right, 32);
        *pos_y = y_top;
        break;
    case OSD_LOCATION_RIGHT_BOTTOM:
        y_top = (int)resolution->u32Height - overlay_height - FONT_H * font_factor;
        if (y_top < FONT_H)
        {
            y_top = FONT_H;
        }
        *pos_x = ANJ_ALIGN_DOWN(x_right, 32);
        *pos_y = y_top;
        break;
    default:
        y_top = FONT_H * font_factor;
        if (y_top < FONT_H)
        {
            y_top = FONT_H;
        }
        *pos_x = x_left;
        *pos_y = y_top;
        break;
    }
}

static int anj_osd_region_info_get(ANJ_SIZE_S *resolution, int ratioX, int ratioY,
                                   int overlay_location, int font_factor, overlay_param_s *overlay_param)
{
    int iRet = 0;
    ANJ_CHK((overlay_param != NULL), -1, "input Invalid");

    if (font_factor <= 0)
    {
        if (resolution->u32Width * resolution->u32Height >= 2560 * 1440)
        {
            font_factor = 4;
        }
        else if (resolution->u32Width * resolution->u32Height >= 1920 * 1080)
        {
            font_factor = 3;
        }
        else if (resolution->u32Width * resolution->u32Height >= 1280 * 720)
        {
            font_factor = 2;
        }
        else
        {
            font_factor = 1;
        }
    }
    overlay_param->font_factor = font_factor;

    // 计算行数
    int line_count = 1;
    char *ptr = overlay_param->show_content_buffer;
    while (*ptr)
    {
        if (*ptr == '^')
            line_count++;
        ptr++;
    }
    // 计算最大宽度和总高度
    int max_line_width = 0;
    char buffer[128] = {0};
    strncpy(buffer, overlay_param->show_content_buffer, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char *line = strtok(buffer, "^");
    while (line)
    {
        int line_width = 0;
        char *char_ptr = line;
        while (*char_ptr)
        {
            int bytes_used = 0;
            utf8_to_unicode(char_ptr, &bytes_used);
            if (bytes_used > 1)
            {
                // 中文字符，占2个英文字符宽度
                line_width += OSD_CHARACTER_PIXEL_WIDTH * 2;
            }
            else
            {
                // 英文字符
                line_width += OSD_CHARACTER_PIXEL_WIDTH;
            }
            char_ptr += bytes_used;
        }

        line_width *= overlay_param->font_factor;
        if (line_width > max_line_width)
        {
            max_line_width = line_width;
        }
        line = strtok(NULL, "^");
    }

    overlay_param->width = max_line_width + (2 * OSD_CHARACTER_PIXEL_EDGE);

    if (overlay_param->width > resolution->u32Width)
    {
        overlay_param->width = resolution->u32Width;
    }

    // 总高度 = 行数 * 单行高度 + 边缘
    int linegap_pixel = (overlay_param->linegap * OSD_CHARACTER_PIXEL_HEIGHT * overlay_param->font_factor) / 8;
    overlay_param->height = (line_count * OSD_CHARACTER_PIXEL_HEIGHT * overlay_param->font_factor) +
                            ((line_count - 1) * linegap_pixel) +
                            (2 * OSD_CHARACTER_PIXEL_EDGE);
    if (overlay_param->height > resolution->u32Height)
    {
        overlay_param->height = resolution->u32Height;
    }

    /*
     * 坐标点X和Y是以100为单位保存，所以除以100获得比例后，乘以分辨率的宽高算出实际坐标
     */
    if (overlay_location != OSD_LOCATION_CNT)
    {
        anj_osd_four_corner_pos_calc(resolution, overlay_location, font_factor,
                                    overlay_param->width, overlay_param->height,
                                    &overlay_param->pos_x, &overlay_param->pos_y);
    }
    else
    {
        overlay_param->pos_x = ratioX * resolution->u32Width / OSD_IOT_COORDINATE_RATIO;
        overlay_param->pos_y = ratioY * resolution->u32Height / OSD_IOT_COORDINATE_RATIO;
    }

    // X方向如果是特殊的居中位置需要根据osd宽度计算开始位置坐标
    if (ratioX == 50)
    {
        overlay_param->pos_x = (resolution->u32Width - overlay_param->width) / 2;
    }

    if (overlay_param->pos_x + overlay_param->width > resolution->u32Width)
    {
        overlay_param->pos_x = resolution->u32Width - overlay_param->width;
    }
    if (overlay_param->pos_y + overlay_param->height > resolution->u32Height)
    {
        overlay_param->pos_y = resolution->u32Height - overlay_param->height;
    }

    // 避免 BMP 比分辨率大导致 pos_x/pos_y 为负，从而整块 OSD 不显示
    if (overlay_param->pos_x < 0)
        overlay_param->pos_x = 0;
    if (overlay_param->pos_y < 0)
        overlay_param->pos_y = 0;

    overlay_param->pos_x = ANJ_ALIGN_DOWN(overlay_param->pos_x, 4);
    overlay_param->pos_y = ANJ_ALIGN_DOWN(overlay_param->pos_y, 4);
    overlay_param->width = ANJ_ALIGN_UP(overlay_param->width, 4);
    overlay_param->height = ANJ_ALIGN_UP(overlay_param->height, 4);

    if (overlay_param->pos_x + overlay_param->width > resolution->u32Width)
    {
        overlay_param->width = ANJ_ALIGN_DOWN(resolution->u32Width - overlay_param->pos_x, 4);
    }
    if (overlay_param->pos_y + overlay_param->height > resolution->u32Height)
    {
        overlay_param->height = ANJ_ALIGN_DOWN(resolution->u32Height - overlay_param->pos_y, 4);
    }
    if (overlay_param->width < 0)
    {
        overlay_param->width = 0;
    }
    if (overlay_param->height < 0)
    {
        overlay_param->height = 0;
    }

endFunc:
    return iRet;
}

static int anj_osd_region_info_bmp_get(ANJ_SIZE_S *resolution, int ratioX, int ratioY,
                                       int overlay_location, overlay_param_s *overlay_param)
{
    int iRet = 0;
    ANJ_CHK((overlay_param != NULL), -1, "input Invalid");

    /*
     * 坐标点X和Y是以100为单位保存，所以除以100获得比例后，乘以分辨率的宽高算出实际坐标
     */
    if (overlay_location != OSD_LOCATION_CNT)
    {
        anj_osd_four_corner_pos_calc(resolution, overlay_location, overlay_param->font_factor,
                                    overlay_param->width, overlay_param->height,
                                    &overlay_param->pos_x, &overlay_param->pos_y);
    }
    else
    {
        int nWidth = (int)resolution->u32Width - overlay_param->width;
        int nHeight = (int)resolution->u32Height - overlay_param->height;
        if (nWidth < 0)
        {
            nWidth = 0;
        }
        if (nHeight < 0)
        {
            nHeight = 0;
        }
        overlay_param->pos_x = nWidth * ratioX / OSD_IOT_COORDINATE_RATIO;
        overlay_param->pos_y = nHeight * ratioY / OSD_IOT_COORDINATE_RATIO;
    }

    if (overlay_param->pos_x + overlay_param->width > resolution->u32Width)
    {
        overlay_param->pos_x = resolution->u32Width - overlay_param->width;
    }
    if (overlay_param->pos_y + overlay_param->height > resolution->u32Height)
    {
        overlay_param->pos_y = resolution->u32Height - overlay_param->height;
    }

    if (overlay_param->pos_x < 0)
    {
        overlay_param->pos_x = 0;
    }
    if (overlay_param->pos_y < 0)
    {
        overlay_param->pos_y = 0;
    }

    overlay_param->pos_x = ANJ_ALIGN_DOWN(overlay_param->pos_x, 4);
    overlay_param->pos_y = ANJ_ALIGN_DOWN(overlay_param->pos_y, 4);
    overlay_param->width = ANJ_ALIGN_UP(overlay_param->width, 4);
    overlay_param->height = ANJ_ALIGN_UP(overlay_param->height, 4);

endFunc:
    return iRet;
}

/*
 * 统一堆叠/避免覆盖布局函数
 * 规则：
 * 1) 如果 TIME 与 TITLE 在同一角：
 *    - 顶部角（左上/右上）：TIME 在 TITLE 之上
 *    - 底部角（左下/右下）：TIME 在 TITLE 之下
 * 2) 如果 TITLE 在左下或右下，则按从下到上堆叠（仅当对应 overlay 有内容时）：
 *    TITLE -> 4G -> SDCARD -> BATTERY （即底部为 TITLE，最上为 BATTERY）
 */
static void anj_osd_layout_stack(overlay_param_s *overlay_param, ANJ_SIZE_S *resolution, VideoOverlay *pstVideoOverlay, int video_stream_index)
{
    DevInfo *pInfo = getDevInfo();
    overlay_param_s *pTitle = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_TITLE];
    overlay_param_s *pTime = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_TIME];
    overlay_param_s *pDbg = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_DBG];
    overlay_param_s *pSdcard = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_SDCARD];
    overlay_param_s *p4g = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_4G];
    overlay_param_s *pBat = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_BATTERY];
    overlay_param_s *pBmp = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_BITMAP];
    overlay_param_s *pCloud = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_CLOUD];
    overlay_param_s *pIspDbg = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_ISP_DEBUG];

    int loc_title = 0, rx_title = 0, ry_title = 0;
    int loc_time = 0, rx_time = 0, ry_time = 0;
    int rx_4g = 0, ry_4g = 0, loc_4g = 0;
    int rx_dbg = 0, ry_dbg = 0, loc_dbg = 0;
    int rx_sdcard = 0, ry_sdcard = 0, loc_sdcard = 0;
    int rx_isp_dbg = 0, ry_isp_dbg = 0, loc_isp_dbg = 0;
    int rx_bat = 0, ry_bat = 0, loc_bat = 0;
    int rx_bmp = 0, ry_bmp = 0, loc_bmp = 0;
    int rx_cloud = 0, ry_cloud = 0, loc_cloud = 0;
    if (pInfo->activated)
    {
        loc_title = anj_osd_location_get(pstVideoOverlay->titleOverlay.posX, pstVideoOverlay->titleOverlay.posY, pstVideoOverlay->titleOverlay.posType, &rx_title, &ry_title);
    }
    else
    {
        loc_title = OSD_LOCATION_CNT;
        rx_title = 50;
        ry_title = 50;
    }
    loc_time = anj_osd_location_get(pstVideoOverlay->timeOverlay.posX, pstVideoOverlay->timeOverlay.posY, pstVideoOverlay->timeOverlay.posType, &rx_time, &ry_time);

    int overlay_font_factor = anj_osd_font_factor_from_fontsize(pstVideoOverlay->fontsize, resolution);

    anj_osd_region_info_get(resolution, rx_title, ry_title, loc_title, overlay_font_factor, pTitle);
    anj_osd_region_info_get(resolution, rx_time, ry_time, loc_time, overlay_font_factor, pTime);

    /* 如果 TIME 与 TITLE 在同一角，按上下调整避免覆盖 */
    if (loc_time == loc_title && loc_time != OSD_LOCATION_CNT)
    {
        if (loc_title == OSD_LOCATION_LEFT_TOP || loc_title == OSD_LOCATION_RIGHT_TOP)
        {
            pTitle->pos_y = pTime->pos_y + pTime->height;
            if (pTitle->pos_y + pTitle->height > resolution->u32Height)
                pTitle->pos_y = resolution->u32Height - pTitle->height;
            if (pTitle->pos_y < 0)
                pTitle->pos_y = 0;
        }
        else
        {
            pTitle->pos_y = pTitle->pos_y - pTime->height;
            if (pTitle->pos_y < 0)
                pTitle->pos_y = 0;
        }
        pTime->pos_y = ANJ_ALIGN_DOWN(pTime->pos_y, 4);
    }

    if (p4g->show_content_buffer[0] != '\0')
    {
        loc_4g = anj_osd_location_get(pstOverlay->content_4g.custom_x, pstOverlay->content_4g.custom_y,
                                      pstOverlay->content_4g.custom_location, &rx_4g, &ry_4g);
        anj_osd_region_info_get(resolution, rx_4g, ry_4g, loc_4g, 0, p4g);
    }
    else if (pstOverlay->b_4g_update)
    {
        loc_4g = anj_osd_location_get(pstOverlay->content_4g.custom_x, pstOverlay->content_4g.custom_y,
                                      pstOverlay->content_4g.custom_location, &rx_4g, &ry_4g);
    }

    if (pDbg->show_content_buffer[0] != '\0')
    {
        loc_dbg = anj_osd_location_get(pstOverlay->custom_content.custom_x, pstOverlay->custom_content.custom_y,
                                       pstOverlay->custom_content.custom_location, &rx_dbg, &ry_dbg);
        anj_osd_region_info_get(resolution, rx_dbg, ry_dbg, loc_dbg, 0, pDbg);
    }
    else if (pstOverlay->b_custom_update)
    {
        loc_dbg = anj_osd_location_get(pstOverlay->custom_content.custom_x, pstOverlay->custom_content.custom_y,
                                       pstOverlay->custom_content.custom_location, &rx_dbg, &ry_dbg);
    }

    if (pSdcard->show_content_buffer[0] != '\0')
    {
        loc_sdcard = anj_osd_location_get(pstOverlay->sdcard_content.custom_x, pstOverlay->sdcard_content.custom_y,
                                          pstOverlay->sdcard_content.custom_location, &rx_sdcard, &ry_sdcard);
        anj_osd_region_info_get(resolution, rx_sdcard, ry_sdcard, loc_sdcard, 0, pSdcard);
    }
    else if (pstOverlay->b_sdcard_update)
    {
        loc_sdcard = anj_osd_location_get(pstOverlay->sdcard_content.custom_x, pstOverlay->sdcard_content.custom_y,
                                          pstOverlay->sdcard_content.custom_location, &rx_sdcard, &ry_sdcard);
    }

    if (pIspDbg->show_content_buffer[0] != '\0')
    {
        loc_isp_dbg = anj_osd_location_get(pstOverlay->isp_debug_content.custom_x, pstOverlay->isp_debug_content.custom_y,
                                           pstOverlay->isp_debug_content.custom_location, &rx_isp_dbg, &ry_isp_dbg);
        anj_osd_region_info_get(resolution, rx_isp_dbg, ry_isp_dbg, loc_isp_dbg, overlay_font_factor, pIspDbg);
    }
    else if (pstOverlay->b_isp_debug_update)
    {
        loc_isp_dbg = anj_osd_location_get(pstOverlay->isp_debug_content.custom_x, pstOverlay->isp_debug_content.custom_y,
                                           pstOverlay->isp_debug_content.custom_location, &rx_isp_dbg, &ry_isp_dbg);
    }

    if (pBat->show_content_buffer[0] != '\0')
    {
        loc_bat = anj_osd_location_get(pstOverlay->battery_content.custom_x, pstOverlay->battery_content.custom_y,
                                       pstOverlay->battery_content.custom_location, &rx_bat, &ry_bat);
        anj_osd_region_info_get(resolution, rx_bat, ry_bat, loc_bat, 0, pBat);
    }
    else if (pstOverlay->b_battery_update)
    {
        loc_bat = anj_osd_location_get(pstOverlay->battery_content.custom_x, pstOverlay->battery_content.custom_y,
                                       pstOverlay->battery_content.custom_location, &rx_bat, &ry_bat);
    }

    if (pBmp->bmp_data)
    {
        loc_bmp = anj_osd_location_get(pstOverlay->bitmap_content.custom_x, pstOverlay->bitmap_content.custom_y,
                                       pstOverlay->bitmap_content.custom_location, &rx_bmp, &ry_bmp);
        anj_osd_region_info_bmp_get(resolution, rx_bmp, ry_bmp,
                                    loc_bmp, pBmp);
    }
    else if (pstOverlay->b_bitmap_update)
    {
        loc_bmp = anj_osd_location_get(pstOverlay->bitmap_content.custom_x, pstOverlay->bitmap_content.custom_y,
                                       pstOverlay->bitmap_content.custom_location, &rx_bmp, &ry_bmp);
    }

    if (pCloud->bmp_data)
    {
        loc_cloud = anj_osd_location_get(pstOverlay->cloud_content.custom_x, pstOverlay->cloud_content.custom_y,
                                         pstOverlay->cloud_content.custom_location, &rx_cloud, &ry_cloud);
        anj_osd_region_info_bmp_get(resolution, rx_cloud, ry_cloud,
                                    loc_cloud, pCloud);
    }
    else if (pstOverlay->b_cloud_update)
    {
        loc_cloud = anj_osd_location_get(pstOverlay->cloud_content.custom_x, pstOverlay->cloud_content.custom_y,
                                         pstOverlay->cloud_content.custom_location, &rx_cloud, &ry_cloud);
    }

    int cur_top = 0;
    if (loc_title == OSD_LOCATION_LEFT_BOTTOM)
    {
        cur_top = pTitle->pos_y - pTitle->height;
    }
    if (pstOverlay->content_4g.custom_show && loc_4g == OSD_LOCATION_LEFT_BOTTOM)
    {
        if (cur_top > 0)
        {
            int y = cur_top - p4g->height;
            if (y < 0)
                y = 0;
            p4g->pos_y = ANJ_ALIGN_DOWN(y, 4);
        }
        cur_top = p4g->pos_y - p4g->height;
    }

    if (pstOverlay->sdcard_content.custom_show && loc_sdcard == OSD_LOCATION_LEFT_BOTTOM)
    {
        if (cur_top > 0)
        {
            int y = cur_top - pSdcard->height;
            if (y < 0)
                y = 0;
            pSdcard->pos_y = ANJ_ALIGN_DOWN(y, 4);
        }
        cur_top = pSdcard->pos_y - pSdcard->height;
    }

    if (pstOverlay->battery_content.custom_show && loc_bat == OSD_LOCATION_LEFT_BOTTOM)
    {
        if (cur_top > 0)
        {
            int y = cur_top - pBat->height;
            if (y < 0)
                y = 0;
            pBat->pos_y = ANJ_ALIGN_DOWN(y, 4);
        }
        cur_top = pBat->pos_y - pBat->height;
    }
}

static int anj_osd_update_title_string(VideoOverlay *osd_cfg, ANJ_SIZE_S *resolution, int bitrate, int fps,
                                       VideoEncodeCfg *pstVideoEncode, char *output, int output_len)
{
    int iRet = 0;
    ANJ_CHK(((osd_cfg != NULL) && (output != NULL)), -1, "input Invalid");
    ANJ_CHK((output_len > 0), -1, "input Invalid");

    if (osd_cfg->titleOverlay.titleType == TYPE_TYPE_BY_TEXT)
    {
        int len = snprintf(output, output_len, "%s", osd_cfg->titleOverlay.title_utf8);
        if (osd_cfg->transparency == TITLE_ADD_RESOLUTION)
        {
            if (output_len - len > 0)
            {
                len = snprintf(output + len, output_len - len, " %s %dx%d",
                               pstVideoEncode->encodeFormat.name, resolution->u32Width, resolution->u32Height);
            }
        }
        else if (osd_cfg->transparency == TITLE_ADD_BITRATE)
        {
            if (output_len - len > 0)
            {
                len = snprintf(output + len, output_len - len, " %s %5dKBps",
                               pstVideoEncode->bitRateControl.name, bitrate / 8);
            }
        }
        else if (osd_cfg->transparency == TITLE_ADD_RESOLUTION_AND_BITRATE)
        {
            if (output_len - len > 0)
            {
                len = snprintf(output + len, output_len - len, " %s %dx%d %s %5dKBps",
                               pstVideoEncode->encodeFormat.name, resolution->u32Width, resolution->u32Height,
                               pstVideoEncode->bitRateControl.name, bitrate / 8);
            }
        }

        if (access(OSD_OVERLAY_FRAMERATE_FLAG, F_OK) == 0 && output_len - len > 0)
        {
            len = snprintf(output + len, output_len - len, " %2dfps", fps);
        }
    }

endFunc:
    return iRet;
}

static int anj_osd_param_get(int osd_type_index, int video_stream_index, overlay_param_s *overlay_param,
                             VideoOverlay *osd_cfg, ANJ_SIZE_S *resolution, ENC_LANGUAGE_E language_type)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL) && (osd_cfg != NULL)), -1, "input Invalid");

    int ratioX = 0;
    int ratioY = 0;
    int overlay_location = OSD_LOCATION_CNT;
    int overlay_font_factor = 0;
    int b_user_bmp = 0;

    overlay_param->stream_type = video_stream_index;
    overlay_param->stream_width = resolution->u32Width;
    overlay_param->stream_height = resolution->u32Height;
    anj_osd_overlay_style_apply(overlay_param, osd_cfg->style);

    if (osd_type_index == OSD_TYPE_TIME)
    {
        overlay_param->rgn_handle = video_stream_index * OSD_TYPE_MAX + osd_type_index;
        overlay_font_factor = anj_osd_font_factor_from_fontsize(osd_cfg->fontsize, resolution);

        overlay_location = anj_osd_location_get(osd_cfg->timeOverlay.posX, osd_cfg->timeOverlay.posY, osd_cfg->timeOverlay.posType,
                                                &ratioX, &ratioY);
        ANJ_CHK_FUNC(anj_osd_update_time_string(osd_cfg, language_type, overlay_param->show_content_buffer, sizeof(overlay_param->show_content_buffer)),
                     0, "anj_osd_update_time_string failed!\n");
    }
    else if (osd_type_index == OSD_TYPE_TITLE)
    {
        overlay_param->rgn_handle = video_stream_index * OSD_TYPE_MAX + osd_type_index;
        overlay_font_factor = anj_osd_font_factor_from_fontsize(osd_cfg->fontsize, resolution);

        overlay_location = anj_osd_location_get(osd_cfg->titleOverlay.posX, osd_cfg->titleOverlay.posY, osd_cfg->titleOverlay.posType,
                                                &ratioX, &ratioY);
        int vencChn = video_stream_index % MAX_VENC_CHN;
        int cameraIndex = video_stream_index / MAX_VENC_CHN;
        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        VideoEncodeCfg *pVideoEncCfg = &pstMediaCfg->videoConfig[cameraIndex].videoEncode.encodeCfg[vencChn];
        ANJ_CHK_FUNC(anj_osd_update_title_string(osd_cfg, &pstOverlay->astResolution[vencChn], 0, 0, pVideoEncCfg,
                                                 overlay_param->show_content_buffer, sizeof(overlay_param->show_content_buffer)),
                     0, "anj_osd_update_title_string failed!\n");
        DevInfo *pInfo = getDevInfo();
        if (pInfo && pInfo->activated == 0)
        {
            overlay_location = OSD_LOCATION_CNT;
            ratioX = 50;
            ratioY = 50;
            snprintf(overlay_param->show_content_buffer, sizeof(overlay_param->show_content_buffer),
                     "%s", OSD_AUTH_FAILED);
        }
    }
    else if (osd_type_index == OSD_TYPE_DBG)
    {
        anj_osd_overlay_style_apply(overlay_param, OSD_DEFAULT_STYLE);
    }
    else if (osd_type_index == OSD_TYPE_ISP_DEBUG)
    {
        overlay_param->rgn_handle = video_stream_index * OSD_TYPE_MAX + osd_type_index;
        overlay_font_factor = anj_osd_font_factor_from_fontsize(osd_cfg->fontsize, resolution);
        overlay_param->font_factor = overlay_font_factor;
        anj_osd_overlay_style_apply(overlay_param, OSD_DEFAULT_STYLE);
        overlay_location = anj_osd_location_get(pstOverlay->isp_debug_content.custom_x,
                                                pstOverlay->isp_debug_content.custom_y,
                                                pstOverlay->isp_debug_content.custom_location,
                                                &ratioX, &ratioY);
    }
    else if (osd_type_index >= OSD_TYPE_USER_0 && osd_type_index <= OSD_TYPE_USER_4)
    {
        overlay_param->rgn_handle = video_stream_index * OSD_TYPE_MAX + osd_type_index;
        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        ANJ_CHK(pstMediaCfg != NULL, -1, "getMediaConfig failed");

        int cameraIndex = video_stream_index / MAX_VENC_CHN;
        ANJ_CHK((cameraIndex >= 0 && cameraIndex < ANJ_CAMERA_MAX_NUMS), -1, "cameraIndex out of range");

        int userIndex = osd_type_index - OSD_TYPE_USER_0;
        ANJ_CHK((userIndex >= 0 && userIndex < MAX_USER_OSD_NUM), -1, "userIndex out of range");

        UserOSD *pstUserOsd = &pstMediaCfg->videoConfig[cameraIndex].useroverlay.data[userIndex];
        overlay_location =
            anj_osd_location_get(pstUserOsd->pos_xscale, pstUserOsd->pos_yscale, pstUserOsd->posType, &ratioX, &ratioY);
        anj_osd_overlay_style_apply(overlay_param, OSD_DEFAULT_STYLE);

        overlay_font_factor = anj_osd_font_factor_from_fontsize(pstUserOsd->fontsize, resolution);
        overlay_param->font_factor = overlay_font_factor;

        if (pstUserOsd->titleType == TYPE_TYPE_BY_BMP)
        {
            b_user_bmp = 1;
            overlay_param->linegap = 0;
            overlay_param->show_content_buffer[0] = '\0';
            int vencChn = video_stream_index % MAX_VENC_CHN;
            char bmp_path[TITLE_MAX_LEN] = {0};
            anj_osd_bmp_path_extract_by_index(pstUserOsd->title_utf8, bmp_path, sizeof(bmp_path), vencChn);
            if (bmp_path[0] == '\0')
            {
                b_user_bmp = 0;
            }
            else
            {
                int ret = anj_osd_load_bmp_file(bmp_path, overlay_param);
                if (ret != 0)
                {
                    // USER BMP 加载失败时不应影响 TIME/TITLE 等其它 OSD 创建
                    if (overlay_param->bmp_data)
                    {
                        anj_mw_free(overlay_param->bmp_data);
                        overlay_param->bmp_data = NULL;
                        overlay_param->bmp_len = 0;
                    }
                    b_user_bmp = 0;
                }
            }
        }
        else
        {
            b_user_bmp = 0;
            overlay_param->linegap = pstUserOsd->linegap;
            snprintf(overlay_param->show_content_buffer, sizeof(overlay_param->show_content_buffer), "%s", pstUserOsd->title_utf8);
        }
    }

    if (b_user_bmp)
    {
        ANJ_CHK_FUNC(anj_osd_region_info_bmp_get(resolution, ratioX, ratioY, overlay_location, overlay_param),
                     0, "anj_osd_region_info_bmp_get failed!\n");
    }
    else
    {
        ANJ_CHK_FUNC(anj_osd_region_info_get(resolution, ratioX, ratioY, overlay_location, overlay_font_factor, overlay_param),
                     0, "anj_osd_region_info_get failed!\n");
    }

    __INFO("osd_type_index=%d, video_stream_index=%d, rgn_handle=%d, (%d:%d:%d:%d)\n",
           osd_type_index, video_stream_index, overlay_param->rgn_handle,
           overlay_param->pos_x, overlay_param->pos_y, overlay_param->width, overlay_param->height);

endFunc:
    return iRet;
}

static int anj_osd_cover_create()
{
    int iRet = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
        int VencChn = (ANJ_CAMERA_MAX_NUMS == 1) ? video_stream_index : (video_stream_index % ANJ_CAMERA_MAX_NUMS);
        VideoMaskConfig *pstVideoMask = &pstMediaCfg->videoConfig[iCameraIdex].videoMask;
        for (int i = 0; i < MAX_VIDEO_MASK_AREA; i++)
        {
            cover_param_s *pstCoverParam = &gstCoverRaram[video_stream_index][i];
            pstCoverParam->rgn_handle = MAX_VIDEO_NUM * OSD_TYPE_MAX + MAX_VIDEO_MASK_AREA * video_stream_index + i + 1;
            unsigned int stream_w = pstOverlay->astResolution[VencChn].u32Width;
            unsigned int stream_h = pstOverlay->astResolution[VencChn].u32Height;
            MASK_AREA_ENTRY *pstMask = (VencChn == 0) ? &pstVideoMask->mainStreamMaskList[i]
                                                      : &pstVideoMask->subStreamMaskList[i];

            if (anj_osd_lens_cover_area_check(&pstVideoMask->mainStreamMaskList[0], &pstOverlay->astResolution[0]))
            {
                pstCoverParam->cover_rect.pos_x = 0;
                pstCoverParam->cover_rect.pos_y = 0;
                pstCoverParam->cover_rect.width = (i == 0) ? RGN_COVER_COORD_BASE(stream_w) : 0;
                pstCoverParam->cover_rect.height = (i == 0) ? RGN_COVER_COORD_BASE(stream_h) : 0;
            }
            else
            {
                pstCoverParam->cover_rect.pos_x = RGN_COVER_COORD_BASE(stream_w) * pstMask->xPos / stream_w;
                pstCoverParam->cover_rect.pos_y = RGN_COVER_COORD_BASE(stream_h) * pstMask->yPos / stream_h;
                pstCoverParam->cover_rect.width = RGN_COVER_COORD_BASE(stream_w) * pstMask->width / stream_w;
                pstCoverParam->cover_rect.height = RGN_COVER_COORD_BASE(stream_h) * pstMask->height / stream_h;
            }
            pstCoverParam->stream_type = VencChn;
            pstCoverParam->cover_rect.u32Color = RGN_COVER_COLOR; // ARGB888_BLACK;
            iRet |= anj_mw_osd_cover_create(pstCoverParam);
        }
    }

    return iRet;
}

static void anj_osd_cover_destory()
{
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        for (int i = 0; i < MAX_VIDEO_MASK_AREA; i++)
        {
            cover_param_s *pstCoverParam = &gstCoverRaram[video_stream_index][i];
            anj_mw_osd_cover_destroy(pstCoverParam);
        }
    }
}

static int anj_osd_draw_overlay_create()
{
    int iRet = 0;

    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        line_param_s *pstLineParam = &gstPolygonParam[video_stream_index];
        rect_param_s *pstRectParam = &gstRectRaram[video_stream_index];

        iRet |= anj_mw_osd_draw_overlay_create(pstLineParam->stream_type, pstLineParam->stream_width, pstLineParam->stream_height,
                                               pstLineParam->rgn_handle);
        iRet |= anj_mw_osd_draw_overlay_create(pstRectParam->stream_type, pstRectParam->stream_width, pstRectParam->stream_height,
                                               pstRectParam->rgn_handle);
    }

    return iRet;
}

static void anj_osd_draw_overlay_destroy()
{
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        line_param_s *pstLineParam = &gstPolygonParam[video_stream_index];
        rect_param_s *pstRectParam = &gstRectRaram[video_stream_index];

        anj_mw_osd_draw_overlay_destroy(pstLineParam->stream_type, pstLineParam->rgn_handle);
        anj_mw_osd_draw_overlay_destroy(pstRectParam->stream_type, pstRectParam->rgn_handle);
    }
}

static void anj_osd_line_draw_param_init(line_param_s *pstLineParam, int video_stream_index)
{
    anj_mw_osd_line_param_init(pstLineParam, video_stream_index,
                               (int)pstOverlay->astResolution[video_stream_index].u32Width,
                               (int)pstOverlay->astResolution[video_stream_index].u32Height);
}

static int anj_osd_create(int osd_type_index, int video_stream_index, overlay_param_s *overlay_param,
                          VideoOverlay *osd_cfg, ANJ_SIZE_S *resolution, ENC_LANGUAGE_E language_type)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL) && (osd_cfg != NULL)), -1, "input Invalid");

    int bEnable = osd_cfg->enable;
    if (osd_type_index >= OSD_TYPE_USER_0 && osd_type_index <= OSD_TYPE_USER_4)
    {
        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        ANJ_CHK(pstMediaCfg != NULL, -1, "getMediaConfig failed");

        int cameraIndex = video_stream_index / MAX_VENC_CHN;
        int userIndex = osd_type_index - OSD_TYPE_USER_0;

        if (cameraIndex < 0 || cameraIndex >= ANJ_CAMERA_MAX_NUMS || userIndex < 0 || userIndex >= MAX_USER_OSD_NUM)
        {
            bEnable = 0;
        }
        else
        {
            UserOSD *pstUserOsd = &pstMediaCfg->videoConfig[cameraIndex].useroverlay.data[userIndex];
            if (pstUserOsd->posType == POSITION_TYPE_DISABLE)
            {
                bEnable = 0;
            }
            else
            {
                if (pstUserOsd->titleType == TYPE_TYPE_BY_BMP)
                {
                    int vencChn = video_stream_index % MAX_VENC_CHN;
                    char bmp_path[TITLE_MAX_LEN] = {0};
                    anj_osd_bmp_path_extract_by_index(pstUserOsd->title_utf8, bmp_path, sizeof(bmp_path), vencChn);
                    bEnable = (bmp_path[0] != '\0' && anj_mw_file_exists(bmp_path)) ? pstUserOsd->enable : 0;
                }
                else if (pstUserOsd->titleType == TYPE_TYPE_BY_TEXT)
                {
                    bEnable = pstUserOsd->enable;
                }
                else
                {
                    bEnable = 0;
                }
            }
        }
    }

    if (bEnable)
    {
        int ret = anj_osd_param_get(osd_type_index, video_stream_index, overlay_param, osd_cfg, resolution, language_type);
        if (ret != 0)
        {
            goto endFunc;
        }
        ANJ_CHK_FUNC(anj_mw_osd_create(overlay_param, resolution, pstOverlay->rgn_pixel_format), 0, "anj_mw_osd_create failed!\n");
    }

    return iRet;
endFunc:
    anj_mw_osd_destroy(overlay_param);
    overlay_param->rgn_handle = INVALID_REGION_HANDLE;
    return iRet;
}

static int anj_osd_create_all(overlay_param_s *overlay_param, ENC_LANGUAGE_E language_type)
{
    int iRet = 0;
    ANJ_CHK(overlay_param != NULL, 0, "input invalid!\n");
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    overlay_param_s *overlay_param_tmp = NULL;
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        int cameraIndex = video_stream_index / MAX_VENC_CHN;
        VideoOverlay *pstVideoOverlay = &pstMediaCfg->videoConfig[cameraIndex].overlay;
        for (ANJ_OSD_TYPE_E osd_type_index = 0; osd_type_index < OSD_TYPE_MAX; osd_type_index++)
        {
            overlay_param_tmp = &overlay_param[video_stream_index * OSD_TYPE_MAX + osd_type_index];
            iRet = anj_osd_create(osd_type_index, video_stream_index, overlay_param_tmp, pstVideoOverlay,
                                  &pstOverlay->astResolution[video_stream_index], language_type);
            if (iRet != 0)
            {
                __ERR("(osd_type_index=%d, video_stream_index=%d) anj_osd_create failed!\n", osd_type_index, video_stream_index);
                goto endFunc;
            }
        }
    }

endFunc:
    return iRet;
}

static int anj_osd_destroy(overlay_param_s *overlay_param)
{
    int iRet = 0;
    ANJ_CHK(overlay_param != NULL, 0, "input invalid!\n");
    if (overlay_param->rgn_handle != INVALID_REGION_HANDLE)
    {
        iRet = anj_mw_osd_destroy(overlay_param);
        if (iRet != 0)
        {
            __ERR("osd destroy failed! iRet=%d\n", iRet);
        }
        // ts sdk 有 bug，需要手动设置为无效
        overlay_param->rgn_handle = INVALID_REGION_HANDLE;
    }
endFunc:
    return iRet;
}

static int anj_osd_destroy_all(overlay_param_s *overlay_param)
{
    int iRet = 0;
    ANJ_CHK(overlay_param != NULL, 0, "input invalid!\n");
    overlay_param_s *overlay_param_tmp = NULL;
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        for (int osd_type_index = 0; osd_type_index < OSD_TYPE_MAX; osd_type_index++)
        {
            overlay_param_tmp = &overlay_param[video_stream_index * OSD_TYPE_MAX + osd_type_index];
            anj_osd_destroy(overlay_param_tmp);
            memset(overlay_param_tmp->show_content_buffer, 0, sizeof(overlay_param_tmp->show_content_buffer));
            if (overlay_param_tmp->bmp_data)
            {
                anj_mw_free(overlay_param_tmp->bmp_data);
                overlay_param_tmp->bmp_data = NULL;
                overlay_param_tmp->bmp_len = 0;
            }
            anj_mw_osd_bitmap_data_free(overlay_param_tmp);
        }
        anj_mw_osd_clean_line(&gstPolygonParam[video_stream_index]);
    }

endFunc:
    return iRet;
}

static int anj_osd_camera_index_from_stream(int stream_type)
{
    if (stream_type < 0)
    {
        return -1;
    }

    return (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (stream_type / ANJ_CAMERA_MAX_NUMS);
}

/*
 * inverse OSD 字体像素应使用的字色（黑/白）：随「该字在画面背景上的亮度」翻转，保证可读性。
 *
 * 坐标关系（自顶向下）：
 *   (canvas_x, canvas_y) —— OSD 位图内偏移；加上 pos 得编码帧像素 (frame_x, frame_y)。
 *   再映射到 Smart 固定分辨率网格 (smart_grid_x, smart_grid_y)，与 anj_smart_luma_get 对齐。
 *
 * 分块采样：
 *   Smart 网格按 OSD_INVERSE_BLOCK_SIZE 划块 (block_col, block_row)。同块共用一个取样点，避免
 *   逐像素阈值附近跳色。块心在 (smart_block_center_x, smart_block_center_y)，再映到码流上
 *   (stream_luma_sample_x, stream_luma_sample_y) 取亮度。
 */
static unsigned char anj_osd_inverse_color_pick_by_luma(overlay_param_s *overlay_param, int canvas_x, int canvas_y)
{
    int camera_index = -1;
    int stream_width = overlay_param->stream_width;    // 码流宽度
    int stream_height = overlay_param->stream_height;  // 码流高度
    int frame_x = 0;                                   // 编码帧x坐标
    int frame_y = 0;                                   // 编码帧y坐标
    int smart_grid_x = 0;                              // Smart网格x坐标
    int smart_grid_y = 0;                              // Smart网格y坐标
    int block_col = 0;                                 // 块列索引
    int block_row = 0;                                 // 块行索引
    int smart_block_center_x = 0;                      // 块中心x坐标
    int smart_block_center_y = 0;                      // 块中心y坐标
    int stream_luma_sample_x = 0;                      // 码流x坐标
    int stream_luma_sample_y = 0;                      // 码流y坐标
    int block_flat_index = 0;                          // 块平面索引
    unsigned char background_luma = 0;                 // 背景亮度
    unsigned char last_color_in_block = 0;             // 块内上次颜色
    unsigned char chosen_font_color = RGB_VALUE_WHITE; // 选择的颜色

    // 获取相机索引
    camera_index = anj_osd_camera_index_from_stream(overlay_param->stream_type);
    if (camera_index < 0)
    {
        __ERR("camera_index < 0, camera_index=%d\n", camera_index);
        return RGB_VALUE_WHITE;
    }

    // 检查码流宽高是否有效
    if (stream_width <= 0 || stream_height <= 0)
    {
        __ERR("stream_width <= 0 || stream_height <= 0, stream_width=%d, stream_height=%d\n",
              stream_width, stream_height);
        return RGB_VALUE_WHITE;
    }

    // 编码帧坐标 = OSD位置 + OSD文本像素坐标
    frame_x = overlay_param->pos_x + canvas_x;
    frame_y = overlay_param->pos_y + canvas_y;
    // Smart网格坐标 = 编码帧坐标 * Smart网格宽度 / 码流宽度
    smart_grid_x = frame_x * DEFAULT_SMART_WIDTH / stream_width;
    smart_grid_y = frame_y * DEFAULT_SMART_HEIGHT / stream_height;

    if (smart_grid_x < 0)
    {
        smart_grid_x = 0;
    }
    else if (smart_grid_x >= DEFAULT_SMART_WIDTH)
    {
        smart_grid_x = DEFAULT_SMART_WIDTH - 1;
    }

    if (smart_grid_y < 0)
    {
        smart_grid_y = 0;
    }
    else if (smart_grid_y >= DEFAULT_SMART_HEIGHT)
    {
        smart_grid_y = DEFAULT_SMART_HEIGHT - 1;
    }

    /* 落在哪一块：同块共享亮度与历史决策 */
    block_col = smart_grid_x / OSD_INVERSE_BLOCK_SIZE;
    block_row = smart_grid_y / OSD_INVERSE_BLOCK_SIZE;

    if (block_col < 0)
    {
        block_col = 0;
    }
    else if (block_col >= OSD_INVERSE_BLOCK_W)
    {
        block_col = OSD_INVERSE_BLOCK_W - 1;
    }

    if (block_row < 0)
    {
        block_row = 0;
    }
    else if (block_row >= OSD_INVERSE_BLOCK_H)
    {
        block_row = OSD_INVERSE_BLOCK_H - 1;
    }

    /* 块中心（Smart 网格）→ 码流像素，作为 anj_smart_luma_get 的取样点 */
    smart_block_center_x = block_col * OSD_INVERSE_BLOCK_SIZE + (OSD_INVERSE_BLOCK_SIZE / 2);
    smart_block_center_y = block_row * OSD_INVERSE_BLOCK_SIZE + (OSD_INVERSE_BLOCK_SIZE / 2);
    if (smart_block_center_x >= DEFAULT_SMART_WIDTH)
    {
        smart_block_center_x = DEFAULT_SMART_WIDTH - 1;
    }
    if (smart_block_center_y >= DEFAULT_SMART_HEIGHT)
    {
        smart_block_center_y = DEFAULT_SMART_HEIGHT - 1;
    }

    stream_luma_sample_x = smart_block_center_x * stream_width / DEFAULT_SMART_WIDTH;
    stream_luma_sample_y = smart_block_center_y * stream_height / DEFAULT_SMART_HEIGHT;
    if (stream_luma_sample_x >= stream_width)
    {
        stream_luma_sample_x = stream_width - 1;
    }
    if (stream_luma_sample_y >= stream_height)
    {
        stream_luma_sample_y = stream_height - 1;
    }

    if (anj_smart_luma_get(camera_index,
                           stream_luma_sample_x,
                           stream_luma_sample_y,
                           stream_width,
                           stream_height,
                           &background_luma) != 0)
    {
        __ERR("anj_smart_luma_get failed, camera_index=%d, stream_luma_sample_x=%d, "
              "stream_luma_sample_y=%d, stream_width=%d, stream_height=%d\n",
              camera_index, stream_luma_sample_x, stream_luma_sample_y, stream_width, stream_height);
        return RGB_VALUE_WHITE;
    }

    block_flat_index = block_row * OSD_INVERSE_BLOCK_W + block_col;
    last_color_in_block = s_u8OsdInverseLastColor[camera_index][block_flat_index];

    /* 亮底黑字、暗底白字；中间带沿用块内上次颜色或中点切断，减轻闪动 */
    if (background_luma >= OSD_INVERSE_LUMA_HIGH)
    {
        chosen_font_color = RGB_VALUE_BLACK;
    }
    else if (background_luma <= OSD_INVERSE_LUMA_LOW)
    {
        chosen_font_color = RGB_VALUE_WHITE;
    }
    else if ((last_color_in_block == RGB_VALUE_BLACK) ||
             (last_color_in_block == RGB_VALUE_WHITE))
    {
        chosen_font_color = last_color_in_block;
    }
    else
    {
        chosen_font_color = (background_luma >= ((OSD_INVERSE_LUMA_LOW + OSD_INVERSE_LUMA_HIGH) / 2)) ? 
                            RGB_VALUE_BLACK : RGB_VALUE_WHITE;
    }

    s_u8OsdInverseLastColor[camera_index][block_flat_index] = chosen_font_color;
    return chosen_font_color;
}

static void anj_osd_draw_pixel_block(glyph_cache_entry_t *glyph, int dst_x, int dst_y, overlay_param_s *overlay_param)
{
    if (!glyph || !overlay_param || !overlay_param->bitmap_data)
        return;

    for (int y = 0; y < glyph->h; y++)
    {
        int py = dst_y + y;
        if (py < 0 || py >= overlay_param->height)
        {
            continue;
        }

        for (int x = 0; x < glyph->w; x++)
        {
            int px = dst_x + x;
            if (px < 0 || px >= overlay_param->width)
            {
                continue;
            }

            unsigned char val = glyph->bitmap[y * glyph->w + x];
            int color = RGB_VALUE_TRANSPARENT;

            if (overlay_param->inverse_pixel)
            {
                if (val != OSD_BITMAP_PIXEL_TEXT)
                {
                    continue;
                }

                color = anj_osd_inverse_color_pick_by_luma(overlay_param, px, py);
            }
            else
            {
                if (val == OSD_BITMAP_PIXEL_TEXT)
                {
                    color = overlay_param->text_color;
                }
                else if (val == OSD_BITMAP_PIXEL_EDGE && overlay_param->draw_edge)
                {
                    color = overlay_param->edge_color;
                }
                else
                {
                    continue;
                }
            }

            anj_mw_osd_draw_pixel(pstOverlay->rgn_pixel_format, color, px, py, overlay_param);
        }
    }
}

static void anj_osd_fill_bg(overlay_param_s *overlay_param)
{
    if (!overlay_param || !overlay_param->draw_bg)
    {
        return;
    }

    for (int y = 0; y < overlay_param->height; y++)
    {
        for (int x = 0; x < overlay_param->width; x++)
        {
            anj_mw_osd_draw_pixel(pstOverlay->rgn_pixel_format, overlay_param->bg_color, x, y, overlay_param);
        }
    }
}

static int anj_osd_draw_hz(unsigned int unicode, int offset_x, int offset_y, int nEdgeSize, overlay_param_s *overlay_param, font_library_t *font_library)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL) && (font_library != NULL)), 0, "input invalid!\n");

    int font_factor = overlay_param->font_factor;
    glyph_cache_entry_t *g = glyph_cache_render(unicode, font_factor, font_library);
    if (g)
        anj_osd_draw_pixel_block(g, offset_x, offset_y, overlay_param);
endFunc:
    return iRet;
}

static int anj_osd_draw_ascii(unsigned int unicode, int offset_x, int offset_y, int nEdgeSize, overlay_param_s *overlay_param, font_library_t *font_library)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL) && (font_library != NULL)), 0, "input invalid!\n");

    int font_factor = overlay_param->font_factor;
    glyph_cache_entry_t *g = glyph_cache_render(unicode, font_factor, font_library);
    if (g)
        anj_osd_draw_pixel_block(g, offset_x, offset_y, overlay_param);
endFunc:
    return iRet;
}

static int anj_osd_fill_bitmap(overlay_param_s *overlay_param, font_library_t *font_library)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL) && (font_library != NULL)), 0, "input invalid!\n");
    if (overlay_param->bitmap_data == NULL)
    {
        return iRet;
    }
    int osd_edge_size = (overlay_param->font_factor > OSD_CHARACTER_PIXEL_EDGE) ? OSD_CHARACTER_PIXEL_EDGE : overlay_param->font_factor;

    memset(overlay_param->bitmap_data, 0, overlay_param->bitmap_len);
    anj_osd_fill_bg(overlay_param);

    // 分割文本为多行，连续 ^ 也保留为空行
    char lines[10][128]; // 假设最多10行
    int line_count = 0;
    int line_char_index = 0;
    const char *src = overlay_param->show_content_buffer;

    memset(lines, 0, sizeof(lines));
    while (*src && line_count < 10)
    {
        if (*src == '^')
        {
            line_count++;
            line_char_index = 0;
            src++;
            continue;
        }

        if (line_char_index < (int)sizeof(lines[0]) - 1)
        {
            lines[line_count][line_char_index++] = *src;
        }
        src++;
    }
    if (line_count < 10)
    {
        line_count++;
    }
    else
    {
        line_count = 10;
    }

    // 计算单行高度
    int line_height = OSD_CHARACTER_PIXEL_HEIGHT * overlay_param->font_factor;
    // 计算行间距
    int linegap_pixel = (overlay_param->linegap * OSD_CHARACTER_PIXEL_HEIGHT * overlay_param->font_factor) / 8;

    // 从第一行开始绘制（正常顺序：第一行在顶部，第二行在底部）
    for (int i = 0; i < line_count; i++)
    {
        int y_offset = osd_edge_size + i * (line_height + linegap_pixel);
        if (y_offset >= overlay_param->height)
        {
            break;
        }
        int x_offset = osd_edge_size;

        // 计算当前行的宽度（用于居中显示）
        int current_line_width = 0;
        char *char_ptr = lines[i];
        while (*char_ptr)
        {
            int bytes_used = 0;
            utf8_to_unicode(char_ptr, &bytes_used);
            if (bytes_used > 1)
            {
                current_line_width += (OSD_CHARACTER_PIXEL_WIDTH * 2 * overlay_param->font_factor);
            }
            else
            {
                current_line_width += (OSD_CHARACTER_PIXEL_WIDTH * overlay_param->font_factor);
            }
            char_ptr += bytes_used;
        }

        // 计算水平居中位置
        int center_x = (overlay_param->width - 2 * osd_edge_size - current_line_width) / 2;
        if (center_x > 0)
        {
            x_offset += center_x;
        }

        // 绘制当前行文本
        char *char_ptr2 = lines[i];
        int char_x = x_offset;

        while (*char_ptr2)
        {
            if (char_x >= overlay_param->width)
            {
                break;
            }

            int bytes_used = 0;
            unsigned int unicode = utf8_to_unicode(char_ptr2, &bytes_used);

            if (bytes_used > 1)
            {
                // 中文字符
                anj_osd_draw_hz(unicode, char_x, y_offset, osd_edge_size, overlay_param, font_library);
                char_x += (OSD_CHARACTER_PIXEL_WIDTH * overlay_param->font_factor) * 2;
            }
            else if (bytes_used == 1)
            {
                // 英文字符
                anj_osd_draw_ascii(unicode, char_x, y_offset, osd_edge_size, overlay_param, font_library);
                char_x += OSD_CHARACTER_PIXEL_WIDTH * overlay_param->font_factor;
            }

            char_ptr2 += bytes_used;
        }
    }

endFunc:
    return iRet;
}

static int anj_osd_fill_bitmap_bmp(overlay_param_s *overlay_param)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL)), 0, "input invalid!\n");
    if (overlay_param->bitmap_data == NULL)
    {
        return iRet;
    }

    // 设备只支持24的bmp
    int bytes_per_pixel = 24 / 8;
    int stride = (overlay_param->width * bytes_per_pixel + 3) & ~3;
    stride = ANJ_ALIGN_UP(stride, 4);
    memset(overlay_param->bitmap_data, 0, overlay_param->bitmap_len);

    // 直接转换BMP颜色到OSD格式
    for (int y = 0; y < overlay_param->height; y++)
    {
        for (int x = 0; x < overlay_param->width; x++)
        {
            int bmp_offset = y * stride + x * bytes_per_pixel;
            // BMP 24-bit 存储顺序为 B,G,R
            unsigned char b = overlay_param->bmp_data[bmp_offset];
            unsigned char g = overlay_param->bmp_data[bmp_offset + 1];
            unsigned char r = overlay_param->bmp_data[bmp_offset + 2];
            // int colorRGB = (r & 0xff) + ((g & 0xff) << 8) + ((b & 0xff) << 16);
            int dstcolor = 0;
            // BMP 24bit 没有 alpha，因此不应把纯黑/纯白直接当透明；
            // 否则会导致 BMP 中的黑/白区域被调色板 index=0(alpha=0) 抹掉。
            if (PIXEL_FORMAT_I2 == pstOverlay->rgn_pixel_format || PIXEL_FORMAT_I4 == pstOverlay->rgn_pixel_format)
            {
                unsigned int Y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;

                if (Y < 80)
                    dstcolor = RGB_VALUE_BLACK;
                else if (Y >= 80 && Y <= 160)
                    dstcolor = RGB_VALUE_GREEN;
                else
                    dstcolor = RGB_VALUE_TRANSPARENT;
            }
            else
            {
                dstcolor = RGB_VALUE_TRANSPARENT;
            }

            anj_mw_osd_draw_pixel(pstOverlay->rgn_pixel_format, dstcolor, x, y, overlay_param);
        }
    }

endFunc:
    return iRet;
}

static int anj_osd_fresh(int osd_type_index, int video_stream_index, overlay_param_s *overlay_param, font_library_t *font_library)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL) && (font_library != NULL)), 0, "input invalid!\n");
    if (overlay_param->rgn_handle == INVALID_REGION_HANDLE)
    {
        return iRet;
    }
    int bUserType = (osd_type_index >= OSD_TYPE_USER_0 && osd_type_index <= OSD_TYPE_USER_4);
    int bUserBmp = (bUserType && overlay_param->bmp_data != NULL);
    if ((osd_type_index == OSD_TYPE_BITMAP || osd_type_index == OSD_TYPE_CLOUD) && overlay_param->bmp_data == NULL)
    {
        return iRet;
    }

    ANJ_CHK_FUNC(anj_mw_osd_bitmap_data_malloc(pstOverlay->rgn_pixel_format, overlay_param), 0, "bitmap malloc failed!\n");
    if (osd_type_index == OSD_TYPE_BITMAP || osd_type_index == OSD_TYPE_CLOUD || bUserBmp)
    {
        // BMP 的像素需要 1:1 映射，避免 font_factor 放大导致形状异常。
        overlay_param->font_factor = 1;
        ANJ_CHK_FUNC(anj_osd_fill_bitmap_bmp(overlay_param), 0, "fill bitmap bmp failed!\n");
    }
    else
    {
        ANJ_CHK_FUNC(anj_osd_fill_bitmap(overlay_param, font_library), 0, "fill bitmap failed!\n");
    }
    ANJ_CHK_FUNC(anj_mw_osd_canvas_update(overlay_param), 0, "canvas update failed!\n");

endFunc:
    return iRet;
}

static int anj_osd_fresh_all_stream(ANJ_OSD_TYPE_E osd_type_index, overlay_param_s *overlay_param, font_library_t *font_library)
{
    int iRet = 0;
    ANJ_CHK(((overlay_param != NULL) && (font_library != NULL)), 0, "input invalid!\n");

    int video_stream_index = 0;
    overlay_param_s *overlay_param_tmp = NULL;
    for (video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        overlay_param_tmp = &overlay_param[video_stream_index * OSD_TYPE_MAX + osd_type_index];
        anj_osd_fresh(osd_type_index, video_stream_index, overlay_param_tmp, font_library);
    }

endFunc:
    return iRet;
}

static int anj_osd_resolution_get()
{
    int iRet = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();

    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        int vencChn = video_stream_index % MAX_VENC_CHN;
        int cameraIndex = video_stream_index / MAX_VENC_CHN;
        VideoCaptureCfg *pVideoCaptureCfg = &pstMediaCfg->videoConfig[cameraIndex].videoCapture;
        VideoEncodeCfg *pVideoEncCfg = &pstMediaCfg->videoConfig[cameraIndex].videoEncode.encodeCfg[vencChn];
        if (pVideoEncCfg->enable)
        {
            pstOverlay->astResolution[video_stream_index] =
                getPicSize(pVideoEncCfg->resolution.name, pVideoCaptureCfg->tvsystem, pVideoCaptureCfg->rotate, 0);
            pstOverlay->astRealRes[video_stream_index] =
                getPicSize(pVideoEncCfg->resolution.name, pVideoCaptureCfg->tvsystem, pVideoCaptureCfg->rotate, 1);
        }
    }

    return iRet;
}

static int anj_osd_polygon_param_init()
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
        Polygon *pstPolygon = &pstAlarmCfg->aiAlarm.pdAlarm[iCameraIdex].polygonArea;
        line_param_s *pstLineParam = &gstPolygonParam[video_stream_index];

        pstLineParam->bFill = 0;
        anj_osd_line_draw_param_init(pstLineParam, video_stream_index);
        pstLineParam->u8BorderWidth = anj_osd_border_width_get(pstOverlay->astResolution[video_stream_index].u32Width);
        pstLineParam->u32Ratio_w = pstOverlay->astResolution[video_stream_index].u32Width;
        pstLineParam->u32Ratio_h = pstOverlay->astResolution[video_stream_index].u32Height;
        if (pstPolygon->count > 1)
        {
            pstLineParam->bShow = 1;
            pstLineParam->s32LineCnt = pstPolygon->count;
        }

        for (int i = 0; i < pstLineParam->s32LineCnt; i++)
        {
            pstLineParam->u32Color[i] = RGB_VALUE_WHITE;
            pstLineParam->stPoint[i].pos_x = pstPolygon->points[i].x * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
            pstLineParam->stPoint[i].pos_y = pstPolygon->points[i].y * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
            if (i == (pstLineParam->s32LineCnt - 1))
            {
                pstLineParam->enPoint[i].pos_x = pstPolygon->points[0].x * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->enPoint[i].pos_y = pstPolygon->points[0].y * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
            }
            else
            {
                pstLineParam->enPoint[i].pos_x = pstPolygon->points[i + 1].x * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->enPoint[i].pos_y = pstPolygon->points[i + 1].y * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
            }
            __DBG("line:%d, st_xy:%d %d en_xy:%d %d\n", i, pstLineParam->stPoint[i].pos_x, pstLineParam->stPoint[i].pos_y,
                  pstLineParam->enPoint[i].pos_x, pstLineParam->enPoint[i].pos_y);
        }
    }
    return 0;
}

static int anj_osd_frame_border_param_init()
{
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        int w = pstOverlay->astResolution[video_stream_index].u32Width;
        int h = pstOverlay->astResolution[video_stream_index].u32Height;
        line_param_s *pstLineParam = &gstFrameBorderParam[video_stream_index];

        pstLineParam->bFill = 0;
        pstLineParam->bShow = 0;
        anj_osd_line_draw_param_init(pstLineParam, video_stream_index);
        pstLineParam->u8BorderWidth = (unsigned char)anj_osd_border_width_get(pstOverlay->astResolution[video_stream_index].u32Width);
        pstLineParam->u32Ratio_w = pstOverlay->astResolution[video_stream_index].u32Width;
        pstLineParam->u32Ratio_h = pstOverlay->astResolution[video_stream_index].u32Height;
        pstLineParam->s32LineCnt = 4;

        if (w > 1 && h > 1)
        {
            int x1 = w - 1;
            int y1 = h - 1;
            pstLineParam->u32Color[0] = RGB_VALUE_WHITE;
            pstLineParam->stPoint[0].pos_x = 0;
            pstLineParam->stPoint[0].pos_y = 0;
            pstLineParam->enPoint[0].pos_x = x1;
            pstLineParam->enPoint[0].pos_y = 0;

            pstLineParam->u32Color[1] = RGB_VALUE_WHITE;
            pstLineParam->stPoint[1].pos_x = x1;
            pstLineParam->stPoint[1].pos_y = 0;
            pstLineParam->enPoint[1].pos_x = x1;
            pstLineParam->enPoint[1].pos_y = y1;

            pstLineParam->u32Color[2] = RGB_VALUE_WHITE;
            pstLineParam->stPoint[2].pos_x = x1;
            pstLineParam->stPoint[2].pos_y = y1;
            pstLineParam->enPoint[2].pos_x = 0;
            pstLineParam->enPoint[2].pos_y = y1;

            pstLineParam->u32Color[3] = RGB_VALUE_WHITE;
            pstLineParam->stPoint[3].pos_x = 0;
            pstLineParam->stPoint[3].pos_y = y1;
            pstLineParam->enPoint[3].pos_x = 0;
            pstLineParam->enPoint[3].pos_y = 0;
        }
        else
        {
            pstLineParam->s32LineCnt = 0;
        }
    }
    return 0;
}

static void anj_osd_aux_line_param_init(VideoLineStruct *pstCrossLine, AuxLineStruct *pstAuxLine)
{
    double length_x = (pstCrossLine->x1Pos - pstCrossLine->x0Pos);
    double length_y = (pstCrossLine->y1Pos - pstCrossLine->y0Pos);
    if (length_x != 0 || length_y != 0)
    {
        AJ_POINT_F pointCenter;
        pointCenter.fX = (double)(pstCrossLine->x1Pos + pstCrossLine->x0Pos) / 2.0;
        pointCenter.fY = (double)(pstCrossLine->y1Pos + pstCrossLine->y0Pos) / 2.0;

        double lengthBeveledge = sqrt(pow(length_x, 2) + pow(length_y, 2)); // 斜边长的平方等于两个直角边的平方和
        double length_direction = OSD_DRIECTION_LINE_LEN;

        // 计算固定长度的垂直线2个端点
        {
            double rate = length_direction / lengthBeveledge;
            double length_x2 = length_x * rate;
            double length_y2 = length_y * rate;

            AJ_POINT_F pointA, pointC;
            AJ_POINT_F pointB, pointD;

            // 全屏坐标
            pointA.fX = pointCenter.fX - length_x2 / 2.0;
            pointA.fY = pointCenter.fY - length_y2 / 2.0;
            // 以A点为原点的坐标
            pointC.fX = length_x2;
            pointC.fY = length_y2;

            GetSquarePointBD(&pointC, &pointB, &pointD);

            pointB.fX += pointA.fX;
            pointB.fY += pointA.fY;
            pointD.fX += pointA.fX;
            pointD.fY += pointA.fY;

            pstAuxLine->point_left = pointD;
            pstAuxLine->point_right = pointB;

            pstAuxLine->point_left.fX = CLAMP(pstAuxLine->point_left.fX, 0.0, 100.0);
            pstAuxLine->point_left.fY = CLAMP(pstAuxLine->point_left.fY, 0.0, 100.0);
            pstAuxLine->point_right.fX = CLAMP(pstAuxLine->point_right.fX, 0.0, 100.0);
            pstAuxLine->point_right.fY = CLAMP(pstAuxLine->point_right.fY, 0.0, 100.0);
        }

        // 以中心点为原点，计算固定长度的坐标，然后旋转得到箭头的2个端点
        {
            length_x = (pstAuxLine->point_left.fX - pointCenter.fX);
            length_y = (pstAuxLine->point_left.fY - pointCenter.fY);

            double length_arrow = OSD_DRIECTION_ARROW_LEN;
            double rate = length_arrow * 2 / length_direction;
            double length_x2 = length_x * rate;
            double length_y2 = length_y * rate;

            AJ_POINT_F pointA, pointC;
            AJ_POINT_F pointB, pointD;

            // 全屏坐标
            pointA = pointCenter;

            // 以A点为原点的坐标
            pointC.fX = length_x2;
            pointC.fY = length_y2;

            double angle = PAI_VALUE / 12; // 15度
            vRotationTransform(angle, &pointC, &pointD);
            vRotationTransform(-angle, &pointC, &pointB);

            pointB.fX += pointA.fX;
            pointB.fY += pointA.fY;
            pointD.fX += pointA.fX;
            pointD.fY += pointA.fY;

            pstAuxLine->arrow_point_left_point_left = pointD;
            pstAuxLine->arrow_point_left_point_right = pointB;

            pstAuxLine->arrow_point_left_point_left.fX = CLAMP(pstAuxLine->arrow_point_left_point_left.fX, 0.0, 100.0);
            pstAuxLine->arrow_point_left_point_left.fY = CLAMP(pstAuxLine->arrow_point_left_point_left.fY, 0.0, 100.0);
            pstAuxLine->arrow_point_left_point_right.fX = CLAMP(pstAuxLine->arrow_point_left_point_right.fX, 0.0, 100.0);
            pstAuxLine->arrow_point_left_point_right.fY = CLAMP(pstAuxLine->arrow_point_left_point_right.fY, 0.0, 100.0);
        }

        // 以中心点为原点，计算固定长度的坐标，然后旋转得到箭头的2个端点
        {
            length_x = (pstAuxLine->point_right.fX - pointCenter.fX);
            length_y = (pstAuxLine->point_right.fY - pointCenter.fY);

            double length_arrow = OSD_DRIECTION_ARROW_LEN;
            double rate = length_arrow * 2 / length_direction;
            double length_x2 = length_x * rate;
            double length_y2 = length_y * rate;

            AJ_POINT_F pointA, pointC;
            AJ_POINT_F pointB, pointD;

            // 全屏坐标
            pointA = pointCenter;

            // 以A点为原点的坐标
            pointC.fX = length_x2;
            pointC.fY = length_y2;

            double angle = PAI_VALUE / 6; // 30度
            vRotationTransform(angle, &pointC, &pointD);
            vRotationTransform(-angle, &pointC, &pointB);

            pointB.fX += pointA.fX;
            pointB.fY += pointA.fY;
            pointD.fX += pointA.fX;
            pointD.fY += pointA.fY;

            pstAuxLine->arrow_point_right_point_left = pointD;
            pstAuxLine->arrow_point_right_point_right = pointB;

            pstAuxLine->arrow_point_right_point_left.fX = CLAMP(pstAuxLine->arrow_point_right_point_left.fX, 0.0, 100.0);
            pstAuxLine->arrow_point_right_point_left.fY = CLAMP(pstAuxLine->arrow_point_right_point_left.fY, 0.0, 100.0);
            pstAuxLine->arrow_point_right_point_right.fX = CLAMP(pstAuxLine->arrow_point_right_point_right.fX, 0.0, 100.0);
            pstAuxLine->arrow_point_right_point_right.fY = CLAMP(pstAuxLine->arrow_point_right_point_right.fY, 0.0, 100.0);
        }
    }
}

static int anj_osd_cross_line_param_init()
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
        VideoGateAlarm *pstVideoGate = &pstAlarmCfg->aiAlarm.vgAlarm[iCameraIdex];
        line_param_s *pstLineParam = &gstCrossLineParam[video_stream_index];

        pstLineParam->bFill = 0;
        anj_osd_line_draw_param_init(pstLineParam, video_stream_index);
        pstLineParam->u8BorderWidth = anj_osd_border_width_get(pstOverlay->astResolution[video_stream_index].u32Width);
        pstLineParam->u32Ratio_w = pstOverlay->astResolution[video_stream_index].u32Width;
        pstLineParam->u32Ratio_h = pstOverlay->astResolution[video_stream_index].u32Height;

        int auxLine = 0;
        for (int i = 0; i < MAX_VIDEO_VG_LINE; i++)
        {
            VideoLineStruct *pstCrossLine = &pstVideoGate->data[i];
            if (pstCrossLine->enable)
            {
                AuxLineStruct stAuxLine = {0};
                if (pstCrossLine->direction == 1)
                {
                    stAuxLine.bDrawRightArrow = 1;
                }
                else if (pstCrossLine->direction == 2)
                {
                    stAuxLine.bDrawLeftArrow = 1;
                }
                else
                {
                    stAuxLine.bDrawLeftArrow = 1;
                    stAuxLine.bDrawRightArrow = 1;
                }
                anj_osd_aux_line_param_init(pstCrossLine, &stAuxLine);

                pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                pstLineParam->stPoint[auxLine].pos_x = pstCrossLine->x0Pos * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->stPoint[auxLine].pos_y = pstCrossLine->y0Pos * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->enPoint[auxLine].pos_x = pstCrossLine->x1Pos * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->enPoint[auxLine].pos_y = pstCrossLine->y1Pos * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                auxLine++;

                if (stAuxLine.bDrawLeftArrow || stAuxLine.bDrawRightArrow)
                {
                    // 垂直方向线
                    pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                    pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                    pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                    pstLineParam->enPoint[auxLine].pos_x = stAuxLine.point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                    pstLineParam->enPoint[auxLine].pos_y = stAuxLine.point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                    auxLine++;
                    if (stAuxLine.bDrawLeftArrow)
                    {
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_left_point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_left_point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_left_point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_left_point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                    }
                    if (stAuxLine.bDrawRightArrow)
                    {
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_right_point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_right_point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_right_point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_right_point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                    }
                }
            }
        }

        if (auxLine > 0)
        {
            pstLineParam->bShow = 1;
            pstLineParam->s32LineCnt = auxLine;
        }

        for (int i = 0; i < pstLineParam->s32LineCnt; i++)
        {
            __INFO("line:%d, st_xy:%d %d en_xy:%d %d\n", i, pstLineParam->stPoint[i].pos_x, pstLineParam->stPoint[i].pos_y,
                   pstLineParam->enPoint[i].pos_x, pstLineParam->enPoint[i].pos_y);
        }
    }
    return 0;
}

static int anj_osd_rect_param_init()
{
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        rect_param_s *pstRectParam = &gstRectRaram[video_stream_index];

        pstRectParam->bShow = 0;
        pstRectParam->bFill = 0;
        anj_mw_osd_rect_param_init(pstRectParam, video_stream_index,
                                   (int)pstOverlay->astResolution[video_stream_index].u32Width,
                                   (int)pstOverlay->astResolution[video_stream_index].u32Height);
        pstRectParam->s32RectCnt = 0;
        pstRectParam->u8BorderWidth = anj_osd_border_width_get(pstOverlay->astResolution[video_stream_index].u32Width);
        pstRectParam->u32Ratio_w = pstOverlay->astResolution[video_stream_index].u32Width * 100 / SMART_PD_WIDTH;
        pstRectParam->u32Ratio_h = pstOverlay->astResolution[video_stream_index].u32Height * 100 / SMART_PD_HEIGHT;
    }
    return 0;
}

static int anj_osd_font_uninit(font_library_t *font_library)
{
    if (font_library)
    {
        if (font_library->font_library_data)
        {
            anj_mw_free(font_library->font_library_data);
            font_library->font_library_data = NULL;
        }
    }

    return 0;
}

static int anj_osd_font_init(font_library_t *font_library)
{
    int iRet = 0;
    ANJ_CHK(font_library != NULL, 0, "input invalid!\n");

    char font_path[512] = {0};

    font_library->font_library_data = NULL;
    for (ENC_LANGUAGE_E i = 0; i < ENC_LANGUAGE_MAX; i++)
    {
        snprintf(font_path, sizeof(font_path), "%s/%s", AJ_APP_PATH, g_FontList[i]);
        if (anj_mw_file_exists(font_path))
        {
            FILE *pFile = anj_mw_fopen(font_path, "r");
            if (pFile)
            {
                iRet = anj_mw_fread(pFile, &font_library->font_library_head, sizeof(Unicode_FontHead_t));
                if (iRet != sizeof(Unicode_FontHead_t))
                {
                    printf("%s read head return %d, failed. \n", font_path, iRet);
                    anj_mw_fclose(pFile);
                    iRet = -1;
                    goto endFunc;
                }

                if (strcmp(FONT_MAGIC, font_library->font_library_head.magic) != 0)
                {
                    printf("file %s not font file.\n", font_path);
                    anj_mw_fclose(pFile);
                    iRet = -1;
                    goto endFunc;
                }

                int FontDataSize = sizeof(Unicode_FontData_t) * font_library->font_library_head.chars_count;
                font_library->font_library_data = (Unicode_FontData_t *)anj_mw_malloc(FontDataSize);
                ANJ_CHK(font_library->font_library_data != NULL, 0, "malloc failed");
                memset(font_library->font_library_data, 0, FontDataSize);
                iRet = anj_mw_fread(pFile, font_library->font_library_data, FontDataSize);
                if (iRet != FontDataSize)
                {
                    printf("%s read head return %d, failed. \n", font_path, iRet);
                    anj_mw_fclose(pFile);
                    iRet = -1;
                    goto endFunc;
                }

                iRet = 0;
                anj_mw_fclose(pFile);
            }
            font_library->font_libray_language = i;
            break;
        }
        else
        {
            __ERR("open font path:%s failed!\n", font_path);
        }
    }

    return iRet;
endFunc:
    anj_osd_font_uninit(font_library);
    return iRet;
}

// static void anj_osd_draw_line(line_param_s *pstRectParam)
// {
//     if (pstRectParam == NULL)
//     {
//         __ERR("input invalid!\n");
//         return;
//     }

//     for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
//     {
//         anj_mw_osd_update_line(pstRectParam);
//     }
// }

static int anj_osd_draw_polygon(line_param_s *pstPolygonParam, int bUpdate, int *pTwinkle)
{
    if (pstPolygonParam == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();

    if (*pTwinkle)
    {
        for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
        {
            for (int i = 0; i < pstPolygonParam[video_stream_index].s32LineCnt; i++)
            {
                int *pu32Color = &pstPolygonParam[video_stream_index].u32Color[i];
                *pu32Color = (*pu32Color == RGB_VALUE_WHITE) ? RGB_VALUE_GREEN : RGB_VALUE_WHITE;
                if (*pTwinkle == 1)
                {
                    *pu32Color = RGB_VALUE_WHITE;
                }
            }
        }
    }

    if (bUpdate || *pTwinkle)
    {
        if (*pTwinkle)
        {
            (*pTwinkle)--;
        }
        for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
        {
            int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
            PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[iCameraIdex];
            VideoOverlay *pstVideoOverlay = &pstMediaCfg->videoConfig[iCameraIdex].overlay;

            if (pstPdAlarm->polygonArea.count == 4 && 
                pstPdAlarm->polygonArea.points[0].x == 0   && pstPdAlarm->polygonArea.points[0].y == 0 &&
                pstPdAlarm->polygonArea.points[1].x == 100 && pstPdAlarm->polygonArea.points[1].y == 0 &&
                pstPdAlarm->polygonArea.points[2].x == 100 && pstPdAlarm->polygonArea.points[2].y == 100 &&
                pstPdAlarm->polygonArea.points[3].x == 0   && pstPdAlarm->polygonArea.points[3].y == 100)
            {
                // The default detection area box will not be displayed.
                pstPolygonParam[video_stream_index].bShow = 0;
            }
            if (pstPdAlarm->alarmAction.draw_rect_enable && pstVideoOverlay->enable)
            {
                anj_mw_osd_update_line(&pstPolygonParam[video_stream_index]);
            }
        }
    }
    return 0;
}

static int anj_osd_draw_frame_border(line_param_s *pstFrameParam, int bUpdate, int *pTwinkle)
{
    if (pstFrameParam == NULL || pTwinkle == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }

    int twinkleCountBefore = *pTwinkle;

    if (*pTwinkle)
    {
        for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
        {
            for (int i = 0; i < pstFrameParam[video_stream_index].s32LineCnt; i++)
            {
                int *pu32Color = &pstFrameParam[video_stream_index].u32Color[i];
                *pu32Color = (*pu32Color == RGB_VALUE_WHITE) ? RGB_VALUE_GREEN : RGB_VALUE_WHITE;
                if (*pTwinkle == 1)
                {
                    *pu32Color = RGB_VALUE_WHITE;
                }
            }
        }
    }

    if (bUpdate || *pTwinkle)
    {
        if (*pTwinkle)
        {
            (*pTwinkle)--;
        }

        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();

        for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
        {
            int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
            VideoOverlay *pstVideoOverlay = &pstMediaCfg->videoConfig[iCameraIdex].overlay;
            if (pstVideoOverlay->enable == 0)
            {
                continue;
            }

            if (twinkleCountBefore > 0)
            {
                pstFrameParam[video_stream_index].bShow = 1;
                anj_mw_osd_update_line(&pstFrameParam[video_stream_index]);
            }
        }

        if (twinkleCountBefore > 0 && *pTwinkle == 0 && pstOverlay)
        {
            for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
            {
                pstFrameParam[video_stream_index].bShow = 0;
            }
        }
    }

    return 0;
}

static int anj_osd_cross_aux_line_get(int vgLine, VideoGateAlarm *pstVideoGate)
{
    int auxLine = 0;
    for (size_t i = 0; i < vgLine; i++)
    {
        VideoLineStruct *pstCrossLine = &pstVideoGate->data[i];
        if (pstCrossLine->direction == 0)
        {
            auxLine += 4;
        }
        else
        {
            auxLine += 6;
        }
    }
    return auxLine;
}

static int anj_osd_draw_cross_line(line_param_s *pstCrossLineParam, int bUpdate, int *pTwinkle)
{
    if (pstCrossLineParam == NULL || pTwinkle == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }

    int bTwinkle = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();

    for (int i = 0; i < MAX_VIDEO_VG_LINE; i++)
    {
        if (pTwinkle[i])
        {
            bTwinkle = 1;
            pTwinkle[i]--;
            for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
            {
                int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
                VideoGateAlarm *pstVideoGate = &pstAlarmCfg->aiAlarm.vgAlarm[iCameraIdex];
                VideoLineStruct *pstCrossLine = &pstVideoGate->data[i];
                int auxLineSt = anj_osd_cross_aux_line_get(i, pstVideoGate);
                int auxLineEn = auxLineSt + 4;
                if (pstCrossLine->direction == 0)
                {
                    auxLineEn = auxLineSt + 6;
                }
                for (; auxLineSt < auxLineEn; auxLineSt++)
                {
                    int *pu32Color = &pstCrossLineParam[video_stream_index].u32Color[auxLineSt];
                    *pu32Color = (*pu32Color == RGB_VALUE_WHITE) ? RGB_VALUE_GREEN : RGB_VALUE_WHITE;
                    if (pTwinkle[i] == 0)
                    {
                        *pu32Color = RGB_VALUE_WHITE;
                    }
                }
            }
        }
    }

    if (bUpdate || bTwinkle)
    {
        for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
        {
            int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
            VideoGateAlarm *pstVideoGate = &pstAlarmCfg->aiAlarm.vgAlarm[iCameraIdex];
            VideoOverlay *pstVideoOverlay = &pstMediaCfg->videoConfig[iCameraIdex].overlay;
            if (pstVideoGate->alarmAction.draw_rect_enable && pstVideoOverlay->enable)
            {
                anj_mw_osd_update_line(&pstCrossLineParam[video_stream_index]);
            }
        }
    }
    return 0;
}

static int anj_osd_debug_update(overlay_param_s *pstOverlayParam, int language_type, int video_stream_index)
{
    int bUpdate = 0;

    if (pstOverlayParam->rgn_handle == INVALID_REGION_HANDLE)
    {
        pstOverlayParam->rgn_handle = video_stream_index * OSD_TYPE_MAX + OSD_TYPE_DBG;
    }

    if (pstOverlay->b_custom_update)
    {
        OverlayTextEnum nType = pstOverlay->custom_content.overlayText;
        if (pstOverlay->custom_content.custom_show == 0)
        {
            memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
            bUpdate = 1;
            if (video_stream_index == MAX_VIDEO_NUM - 1)
                pstOverlay->b_custom_update = 0;
            __INFO("custom osd hide, xy:%d %d\n", pstOverlay->custom_content.custom_x, pstOverlay->custom_content.custom_y);
        }
        else
        {
            char *custom_str = NULL;
            if (nType == OVERLAY_ZOOM_DIGITAL || nType == OVERLAY_ZOOM_PROGRESS)
            {
                IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
                DOUBLE_AREA_ENTRY cur_area = {0};
                double multiple = 0;
                int iRet = anj_zoom_run_get(0, &cur_area, NULL);
                if (iRet == 0)
                {
                    multiple =
                        1.0f + (OSD_ZOOM_MAX_MULTIPLE - 1.0f) * (cur_area.width - 1.0f) / (pstIotPtzConfig->m_zoom.max_multiple - 1.0f);
                }
                if (nType == OVERLAY_ZOOM_DIGITAL)
                {
                    snprintf(pstOverlay->custom_content.overlayStr, sizeof(pstOverlay->custom_content.overlayStr),
                             "ZOOM %.1f X", multiple);
                }
                else if (nType == OVERLAY_ZOOM_PROGRESS)
                {
                    snprintf(pstOverlay->custom_content.overlayStr, sizeof(pstOverlay->custom_content.overlayStr),
                             "ZOOM %d%%", (int)(multiple * 100 / OSD_ZOOM_MAX_MULTIPLE));
                }
                custom_str = pstOverlay->custom_content.overlayStr;
            }
            else
            {
                custom_str =
                    (language_type == ENC_LANGUAGE_CN) ? s_stOverlayText[nType].szTextChn : s_stOverlayText[nType].szTextEng;
            }

            // 自定义字串长度发生变化 需要自定义字串画布清空
            if (strlen(pstOverlayParam->show_content_buffer) > 0 &&
                strlen(pstOverlayParam->show_content_buffer) != strlen(custom_str))
            {
                memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
                bUpdate = 1;
            }
            else
            {
                snprintf(pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer),
                            "%s", custom_str);
                bUpdate = 1;
                if ((video_stream_index == MAX_VIDEO_NUM - 1) && (nType != OVERLAY_ZOOM_DIGITAL))
                {
                    pstOverlay->b_custom_update = 0;
                }
            }
        }
    }
    return bUpdate;
}

static int anj_osd_sdcard_update(overlay_param_s *pstOverlayParam, int language_type, int video_stream_index)
{
    int bUpdate = 0;

    if (pstOverlayParam->rgn_handle == INVALID_REGION_HANDLE)
    {
        pstOverlayParam->rgn_handle = video_stream_index * OSD_TYPE_MAX + OSD_TYPE_SDCARD;
    }

    if (pstOverlay->b_sdcard_update)
    {
        OverlayTextEnum nType = pstOverlay->sdcard_content.overlayText;
        if (pstOverlay->sdcard_content.custom_show == 0)
        {
            memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
            if (video_stream_index == MAX_VIDEO_NUM - 1)
            {
                pstOverlay->b_sdcard_update = 0;
            }
            bUpdate = 1;
        }
        else
        {
            char *custom_str =
                (language_type == ENC_LANGUAGE_CN) ? s_stOverlayText[nType].szTextChn : s_stOverlayText[nType].szTextEng;

            // 自定义字串长度发生变化 需要自定义字串画布清空
            if (strlen(pstOverlayParam->show_content_buffer) > 0 &&
                strlen(pstOverlayParam->show_content_buffer) != strlen(custom_str))
            {
                memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
                bUpdate = 1;
            }
            else
            {
                if (strcmp(pstOverlayParam->show_content_buffer, custom_str))
                {
                    snprintf(pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer),
                             "%s", custom_str);
                    bUpdate = 1;
                }
                if (video_stream_index == MAX_VIDEO_NUM - 1)
                {
                    pstOverlay->b_sdcard_update = 0;
                }
            }
        }
    }
    return bUpdate;
}

static int anj_osd_isp_debug_update(overlay_param_s *pstOverlayParam, int video_stream_index)
{
    int bUpdate = 0;
    int iCameraIdex = video_stream_index / MAX_VENC_CHN;
    int VencChn = video_stream_index % MAX_VENC_CHN;
    char debug_buf[OSD_SHOW_CONTENT_BUF_LEN] = {0};

    if (pstOverlayParam->rgn_handle == INVALID_REGION_HANDLE)
    {
        pstOverlayParam->rgn_handle = video_stream_index * OSD_TYPE_MAX + OSD_TYPE_ISP_DEBUG;
    }

    if (pstOverlay->b_isp_debug_update)
    {
        if (pstOverlay->isp_debug_content.custom_show == 0)
        {
            memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
            if (video_stream_index == MAX_VIDEO_NUM - 1)
            {
                pstOverlay->b_isp_debug_update = 0;
            }
            bUpdate = 1;
        }
        else
        {
            anj_osd_isp_debug_build(iCameraIdex, VencChn, debug_buf, sizeof(debug_buf));
            snprintf(pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer), "%s", debug_buf);
            bUpdate = 1;
        }
    }

    return bUpdate;
}

static int anj_osd_4g_update(overlay_param_s *pstOverlayParam, int video_stream_index)
{
    int bUpdate = 0;

    if (pstOverlayParam->rgn_handle == INVALID_REGION_HANDLE)
    {
        pstOverlayParam->rgn_handle = video_stream_index * OSD_TYPE_MAX + OSD_TYPE_4G;
    }

    if (pstOverlay->b_4g_update)
    {
        if (pstOverlay->content_4g.custom_show == 0)
        {
            memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
            if (video_stream_index == MAX_VIDEO_NUM - 1)
            {
                pstOverlay->b_4g_update = 0;
            }
            bUpdate = 1;
        }
        else
        {
            // 自定义字串长度发生变化 需要自定义字串画布清空
            if (strlen(pstOverlayParam->show_content_buffer) > 0 &&
                strlen(pstOverlayParam->show_content_buffer) != strlen(pstOverlay->content_4g.overlayStr))
            {
                memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
                bUpdate = 1;
            }
            else
            {
                if (strcmp(pstOverlayParam->show_content_buffer, pstOverlay->content_4g.overlayStr))
                {
                    snprintf(pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer),
                             "%s", pstOverlay->content_4g.overlayStr);
                    bUpdate = 1;
                }
                if (video_stream_index == MAX_VIDEO_NUM - 1)
                {
                    pstOverlay->b_4g_update = 0;
                }
            }
        }
    }
    return bUpdate;
}

static void anj_osd_bmp_update(overlay_param_s *pstOverlayParam, int video_stream_index)
{
    if (pstOverlayParam->rgn_handle == INVALID_REGION_HANDLE)
    {
        pstOverlayParam->rgn_handle = video_stream_index * OSD_TYPE_MAX + OSD_TYPE_BITMAP;
    }

    if (pstOverlay->b_bitmap_update)
    {
        if (pstOverlay->bitmap_content.custom_show == 0)
        {
            if (pstOverlayParam->bmp_data && pstOverlayParam->bmp_len > 0)
            {
                // bmpdata 255是透明色
                memset(pstOverlayParam->bmp_data, 255, pstOverlayParam->bmp_len);
            }
        }
        else
        {
            if (pstOverlayParam->bmp_data)
            {
                anj_mw_free(pstOverlayParam->bmp_data);
                pstOverlayParam->bmp_data = NULL;
                pstOverlayParam->bmp_len = 0;
            }
            OverlayTextEnum nType = pstOverlay->bitmap_content.overlayText;
            int VencChn = (ANJ_CAMERA_MAX_NUMS == 1) ? video_stream_index : (video_stream_index % ANJ_CAMERA_MAX_NUMS);
            char *bmp_path =
                (VencChn == 0) ? s_stOverlayText[nType].szTextChn : s_stOverlayText[nType].szTextEng;

            anj_osd_load_bmp_file(bmp_path, pstOverlayParam);
        }
        if (video_stream_index == MAX_VIDEO_NUM - 1)
            pstOverlay->b_bitmap_update = 0;
    }
}

static void anj_osd_cloud_update(overlay_param_s *pstOverlayParam, int video_stream_index)
{
    if (pstOverlayParam->rgn_handle == INVALID_REGION_HANDLE)
    {
        pstOverlayParam->rgn_handle = video_stream_index * OSD_TYPE_MAX + OSD_TYPE_CLOUD;
    }

    if (pstOverlay->b_cloud_update)
    {
        if (pstOverlay->cloud_content.custom_show == 0)
        {
            if (pstOverlayParam->bmp_data && pstOverlayParam->bmp_len > 0)
            {
                memset(pstOverlayParam->bmp_data, 0, pstOverlayParam->bmp_len);
            }
        }
        else
        {
            if (pstOverlayParam->bmp_data)
            {
                anj_mw_free(pstOverlayParam->bmp_data);
                pstOverlayParam->bmp_data = NULL;
                pstOverlayParam->bmp_len = 0;
            }
            OverlayTextEnum nType = pstOverlay->cloud_content.overlayText;
            int VencChn = (ANJ_CAMERA_MAX_NUMS == 1) ? video_stream_index : (video_stream_index % ANJ_CAMERA_MAX_NUMS);
            char *bmp_path =
                (VencChn == 0) ? s_stOverlayText[nType].szTextChn : s_stOverlayText[nType].szTextEng;

            anj_osd_load_bmp_file(bmp_path, pstOverlayParam);
        }
        if (video_stream_index == MAX_VIDEO_NUM - 1)
            pstOverlay->b_cloud_update = 0;
    }
}

static int anj_osd_battery_update(overlay_param_s *pstOverlayParam, int video_stream_index)
{
    int bUpdate = 0;

    if (pstOverlayParam->rgn_handle == INVALID_REGION_HANDLE)
    {
        pstOverlayParam->rgn_handle = video_stream_index * OSD_TYPE_MAX + OSD_TYPE_BATTERY;
    }

    if (pstOverlay->b_battery_update)
    {
        if (pstOverlay->battery_content.custom_show == 0)
        {
            memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
            bUpdate = 1;
            if (video_stream_index == MAX_VIDEO_NUM - 1)
                pstOverlay->b_battery_update = 0;
        }
        else
        {
            // 自定义字串长度发生变化 需要自定义字串画布清空
            if (strlen(pstOverlayParam->show_content_buffer) > 0 &&
                strlen(pstOverlayParam->show_content_buffer) != strlen(pstOverlay->battery_content.overlayStr))
            {
                memset(pstOverlayParam->show_content_buffer, 0, sizeof(pstOverlayParam->show_content_buffer));
                bUpdate = 1;
            }
            else
            {
                if (strcmp(pstOverlayParam->show_content_buffer, pstOverlay->battery_content.overlayStr))
                {
                    snprintf(pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer),
                             "%s", pstOverlay->battery_content.overlayStr);
                    bUpdate = 1;
                }
                if (video_stream_index == MAX_VIDEO_NUM - 1)
                    pstOverlay->b_battery_update = 0;
            }
        }
    }
    return bUpdate;
}

static int anj_osd_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    SystemConfig *pstSystemCfg = (SystemConfig *)getSystemConfig();
    overlay_param_s overlay_param[MAX_OSD_HANDLE] = {0};
    font_library_t font_library = {0};
    char current_time_buffer[64] = {0};
    char last_time_buffer[ANJ_CAMERA_MAX_NUMS][64] = {0};
    ENC_LANGUAGE_E language_type = ENC_LANGUAGE_CN;
    int drawNormalTime = 0;
    int drawPolygonTime = 0;
    int drawBmpTime = 0;
    int bUpdatePolygon = 0;

    ANJ_CHK_FUNC(anj_osd_font_init(&font_library), 0, "font init failed!\n");

    ANJ_CHK_FUNC(anj_osd_resolution_get(), 0, "anj_osd_resolution_get failed!\n");

    ANJ_CHK_FUNC(anj_osd_rect_param_init(), 0, "anj_osd_rect_param_init failed!\n");

    ANJ_CHK_FUNC(anj_osd_polygon_param_init(), 0, "anj_osd_polygon_param_init failed!\n");
    ANJ_CHK_FUNC(anj_osd_frame_border_param_init(), 0, "anj_osd_frame_border_param_init failed!\n");
    ANJ_CHK_FUNC(anj_osd_cross_line_param_init(), 0, "anj_osd_cross_line_param_init failed!\n");
    ANJ_CHK_FUNC(anj_osd_cover_create(), 0, "anj_osd_cover_create failed!\n");
    ANJ_CHK_FUNC(anj_osd_draw_overlay_create(), 0, "anj_osd_draw_overlay_create failed!\n");

    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        for (int osd_type_index = 0; osd_type_index < OSD_TYPE_MAX; osd_type_index++)
        {
            overlay_param[video_stream_index * OSD_TYPE_MAX + osd_type_index].rgn_handle = INVALID_REGION_HANDLE;
            overlay_param[video_stream_index * OSD_TYPE_MAX + osd_type_index].bitmap_data = NULL;
            overlay_param[video_stream_index * OSD_TYPE_MAX + osd_type_index].bmp_data = NULL;
            overlay_param[video_stream_index * OSD_TYPE_MAX + osd_type_index].bitmap_len = 0;
        }
    }

    pstOverlay->b_overlay_update = 0;
    DevInfo *pInfo = getDevInfo();
    int actived = 0;
    int osdUpdate[OSD_TYPE_MAX] = {0};
    while (bStart && *bStart)
    {
        actived = pInfo->activated;
        anj_osd_isp_debug_flag_check();

        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        if (pstOverlay->b_overlay_update == 0)
        {
            pstOverlay->b_overlay_update = 1;
            pstOverlay->PolygonTwinkleTimes = 0;
            pstOverlay->FrameBorderTwinkleTimes = 0;
            memset(pstOverlay->CrossLineTwinkleTimes, 0, sizeof(pstOverlay->CrossLineTwinkleTimes));
            ANJ_CHK_FUNC(anj_osd_destroy_all(overlay_param), 0, "osd destroy all failed!\n");
            usleep(200 * 1000);
            ANJ_CHK_FUNC(anj_osd_language_type_get(&language_type, pstSystemCfg->miscCfg.language), 0, "get osd type failed!\n");
            ANJ_CHK_FUNC(anj_osd_create_all(overlay_param, language_type), 0, "anj_osd_create_all failed!\n");
            ANJ_CHK_FUNC(anj_osd_draw_polygon(gstPolygonParam, 1, &pstOverlay->PolygonTwinkleTimes), 0, "anj_osd_draw_polygon failed!\n");
            ANJ_CHK_FUNC(anj_osd_draw_cross_line(gstCrossLineParam, 1, pstOverlay->CrossLineTwinkleTimes), 0, "anj_osd_draw_cross_line failed!\n");
            ANJ_CHK_FUNC(anj_osd_draw_frame_border(gstFrameBorderParam, 1, &pstOverlay->FrameBorderTwinkleTimes), 0,
                         "anj_osd_draw_frame_border failed!\n");
            for (ANJ_OSD_TYPE_E osd_type_index = 0; osd_type_index < OSD_TYPE_DBG; osd_type_index++)
            {
                ANJ_CHK_FUNC(anj_osd_fresh_all_stream(osd_type_index, overlay_param, &font_library), 0, "anj_osd_fresh_all_stream failed!\n");
            }
            for (ANJ_OSD_TYPE_E osd_type_index = OSD_TYPE_USER_0; osd_type_index <= OSD_TYPE_USER_4; osd_type_index++)
            {
                ANJ_CHK_FUNC(anj_osd_fresh_all_stream(osd_type_index, overlay_param, &font_library), 0, "anj_osd_fresh_all_stream failed!\n");
            }
            if (pstOverlay->bitmap_content.custom_show)
            {
                pstOverlay->b_bitmap_update = 1;
            }
            if (pstOverlay->cloud_content.custom_show)
            {
                pstOverlay->b_cloud_update = 1;
            }
            if (pstOverlay->content_4g.custom_show)
            {
                pstOverlay->b_4g_update = 1;
            }
            if (pstOverlay->sdcard_content.custom_show)
            {
                pstOverlay->b_sdcard_update = 1;
            }
            if (pstOverlay->isp_debug_content.custom_show)
            {
                pstOverlay->b_isp_debug_update = 1;
            }
            memset(last_time_buffer, '\0', sizeof(last_time_buffer));
        }
        else
        {
            if (drawNormalTime > OSD_NORMAL_UPDATE_TIME_US / OSD_THREAD_TIME_US)
            {
                const char *cpu_info = anj_sysmng_cpu_info_update();
                snprintf(s_osd_cpu_info, sizeof(s_osd_cpu_info), "%s",
                         (cpu_info != NULL) ? cpu_info : "");
            }

            for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
            {
                int iCameraIdex = (ANJ_CAMERA_MAX_NUMS == 1) ? 0 : (video_stream_index / ANJ_CAMERA_MAX_NUMS);
                int VencChn = (ANJ_CAMERA_MAX_NUMS == 1) ? video_stream_index : (video_stream_index % ANJ_CAMERA_MAX_NUMS);
                VideoOverlay *pstVideoOverlay = &pstMediaCfg->videoConfig[iCameraIdex].overlay;
                if (pstVideoOverlay->enable == 0)
                {
                    continue;
                }

                // SD卡状态刷新
                if (pstOverlay->b_sdcard_update ||
                    (drawBmpTime > OSD_BMP_UPDATE_TIME_US / OSD_THREAD_TIME_US))
                {
                    anj_osd_sdcard_update(&overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_SDCARD],
                                                 language_type, video_stream_index);
                    osdUpdate[OSD_TYPE_SDCARD] = 1;
                }

                // 调试信息刷新
                if (pstOverlay->b_custom_update ||
                    (drawBmpTime > OSD_BMP_UPDATE_TIME_US / OSD_THREAD_TIME_US))
                {
                    iRet = anj_osd_debug_update(&overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_DBG],
                                                language_type, video_stream_index);
                    if (iRet)
                    {
                        osdUpdate[OSD_TYPE_DBG] = 1;
                    }
                }

                if (drawBmpTime > OSD_BMP_UPDATE_TIME_US / OSD_THREAD_TIME_US)
                {
                    osdUpdate[OSD_TYPE_BITMAP] = 1;
                    // 自定义bmp刷新
                    anj_osd_bmp_update(&overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_BITMAP],
                                       video_stream_index);

                    osdUpdate[OSD_TYPE_CLOUD] = 1;
                    // cloud bmp刷新
                    anj_osd_cloud_update(&overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_CLOUD],
                                         video_stream_index);
                    if (video_stream_index == MAX_VIDEO_NUM - 1)
                    {
                        drawBmpTime = 0;
                    }
                }

                if (drawNormalTime > OSD_NORMAL_UPDATE_TIME_US / OSD_THREAD_TIME_US)
                {
                    osdUpdate[OSD_TYPE_TIME] = 1;
                    osdUpdate[OSD_TYPE_TITLE] = 1;
                    for (ANJ_OSD_TYPE_E osd_type_index = OSD_TYPE_USER_0; osd_type_index <= OSD_TYPE_USER_4; osd_type_index++)
                    {
                        osdUpdate[osd_type_index] = 1;
                    }

                    // 时间刷新
                    memset(current_time_buffer, '\0', sizeof(current_time_buffer));
                    ANJ_CHK_FUNC(anj_osd_update_time_string(pstVideoOverlay, language_type, current_time_buffer, sizeof(current_time_buffer)),
                                 0, "update time failed!\n");
                    if (strlen(last_time_buffer[iCameraIdex]) == 0)
                    {
                        snprintf(last_time_buffer[iCameraIdex], sizeof(last_time_buffer[iCameraIdex]), "%s", current_time_buffer);
                    }
                    else
                    {
                        if ((strlen(current_time_buffer) > strlen(last_time_buffer[iCameraIdex])))
                        {
                            __INFO("current_time_buffer'length %d is bigger than last_time_buffer'length %d, need update\n",
                                   strlen(current_time_buffer), strlen(last_time_buffer[iCameraIdex]));
                            pstOverlay->b_overlay_update = 0;
                            anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
                            continue;
                        }
                    }
                    if (strlen(current_time_buffer) > 0 && strlen(current_time_buffer) < sizeof(current_time_buffer))
                    {
                        overlay_param_s *pstOverlayParam = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_TIME];
                        snprintf(pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer),
                                 "%s", current_time_buffer);
                    }
                    // 标题信息刷新
                    overlay_param_s *pstOverlayParam = &overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_TITLE];
                    VideoEncodeCfg *pstVideoEncode = &pstMediaCfg->videoConfig[iCameraIdex].videoEncode.encodeCfg[VencChn];
                    int bitrate = anj_video_bitrate_get(iCameraIdex, VencChn);
                    int fps = anj_video_fps_get(iCameraIdex, VencChn);

                    if (actived == 0)
                    {
                        // 未授权osd固定在一个位置
                        snprintf(pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer),
                                 "%s", OSD_AUTH_FAILED);
                    }
                    else
                    {
                        int old_title_len = strlen(pstOverlayParam->show_content_buffer);
                        anj_osd_update_title_string(pstVideoOverlay, &pstOverlay->astRealRes[VencChn], bitrate, fps, pstVideoEncode,
                                                    pstOverlayParam->show_content_buffer, sizeof(pstOverlayParam->show_content_buffer));
                        if (strlen(pstOverlayParam->show_content_buffer) > (size_t)old_title_len)
                        {
                            pstOverlay->b_overlay_update = 0;
                            anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
                            continue;
                        }
                    }
                    anj_osd_str_replace(iCameraIdex, VencChn, pstOverlayParam->show_content_buffer);

                    // 4g信息刷新
                    if (pstOverlay->b_4g_update)
                    {
                        iRet = anj_osd_4g_update(&overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_4G],
                                                 video_stream_index);
                        if (iRet)
                        {
                            osdUpdate[OSD_TYPE_4G] = 1;
                        }
                    }

                    // 电池电量更新
                    if (pstOverlay->b_battery_update)
                    {
                        iRet = anj_osd_battery_update(&overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_BATTERY],
                                                      video_stream_index);
                        if (iRet)
                        {
                            osdUpdate[OSD_TYPE_BATTERY] = 1;
                        }
                    }
                    
                    // ISP调试信息刷新
                    if (pstOverlay->b_isp_debug_update)
                    {
                        iRet = anj_osd_isp_debug_update(&overlay_param[video_stream_index * OSD_TYPE_MAX + OSD_TYPE_ISP_DEBUG],
                                                        video_stream_index);
                        if (iRet)
                        {
                            osdUpdate[OSD_TYPE_ISP_DEBUG] = 1;
                        }
                    }
                    if (video_stream_index == MAX_VIDEO_NUM - 1)
                    {
                        drawNormalTime = 0;
                    }
                }
                anj_osd_layout_stack(overlay_param, &pstOverlay->astResolution[video_stream_index], pstVideoOverlay, video_stream_index);
                for (ANJ_OSD_TYPE_E osd_type_index = 0; osd_type_index < OSD_TYPE_MAX; osd_type_index++)
                {
                    if (osdUpdate[osd_type_index])
                    {
                        osdUpdate[osd_type_index] = 0;
                        ANJ_CHK_FUNC(anj_osd_fresh_all_stream(osd_type_index, overlay_param, &font_library),
                                     0, "anj_osd_fresh_all_stream failed!\n");
                    }
                }
            }

            drawNormalTime++;
            drawPolygonTime++;
            drawBmpTime++;
            if (drawPolygonTime > OSD_LINE_UPDATE_TIME_US / OSD_THREAD_TIME_US)
            {
                drawPolygonTime = 0;
                bUpdatePolygon = 1;
            }
            ANJ_CHK_FUNC(anj_osd_draw_polygon(gstPolygonParam, bUpdatePolygon, &pstOverlay->PolygonTwinkleTimes),
                         0, "anj_osd_draw_polygon failed!\n");
            ANJ_CHK_FUNC(anj_osd_draw_cross_line(gstCrossLineParam, bUpdatePolygon, pstOverlay->CrossLineTwinkleTimes),
                         0, "anj_osd_draw_cross_line failed!\n");
            ANJ_CHK_FUNC(anj_osd_draw_frame_border(gstFrameBorderParam, bUpdatePolygon, &pstOverlay->FrameBorderTwinkleTimes), 0,
                         "anj_osd_draw_frame_border failed!\n");
            bUpdatePolygon = 0;
        }

        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
        usleep(OSD_THREAD_TIME_US);
    }

endFunc:
    anj_osd_destroy_all(overlay_param);
    anj_osd_font_uninit(&font_library);
    return iRet;
}

static int anj_osd_uninit(void)
{
    if (pstOverlay)
    {
        anj_thread_task_destroy(&pstOverlay->osd_thread, 0);
        anj_osd_draw_overlay_destroy();
        anj_osd_cover_destory();
        sleep(3);
        anj_mw_osd_uninit();
        anj_mw_free(pstOverlay);
        pstOverlay = NULL;
    }

    anj_mutex_destroy(&s_gAnjOsdCtrlMutex);
    return 0;
}

static int anj_osd_init(void)
{
    int iRet = 0;

    ANJ_CHK_FUNC(anj_mutex_create(&s_gAnjOsdCtrlMutex, 0), 0, "anj create osd mutex failed!\n");

    if (pstOverlay == NULL)
    {
        pstOverlay = anj_mw_malloc(sizeof(overlay_thread_param_t));
        ANJ_CHK((pstOverlay != NULL), -1, "malloc failed!\n");

        memset(pstOverlay, 0, sizeof(overlay_thread_param_t));
        pstOverlay->rgn_pixel_format = PIXEL_FORMAT_I2;
        ANJ_CHK_FUNC(anj_mw_osd_init(pstOverlay->rgn_pixel_format), 0, "anj_mw_osd_init failed!\n");

        pstOverlay->osd_thread.bAutoDestroy = 0;
        strncpy(pstOverlay->osd_thread.iThreadName, "anj_osd_thread", sizeof(pstOverlay->osd_thread.iThreadName) - 1);
        pstOverlay->osd_thread.iThreadjob.ctx = &pstOverlay->osd_thread;
        pstOverlay->osd_thread.iThreadjob.func = anj_osd_thread;
        ANJ_CHK_FUNC(anj_thread_task_create(&pstOverlay->osd_thread), 0, "osd thread create failed!\n");
    }

    return iRet;

endFunc:
    anj_osd_uninit();
    return iRet;
}

void anj_osd_debug_set(osd_custom_content_s *pstOsdCustom)
{
    if (pstOverlay)
    {
        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        pstOverlay->b_custom_update = 1;
        pstOverlay->custom_content = *pstOsdCustom;
        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
    }
}

void anj_osd_4g_set(osd_custom_content_s *pstOsdCustom)
{
    if (pstOverlay)
    {
        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        pstOverlay->b_4g_update = 1;
        if (pstOverlay->content_4g.custom_show != pstOsdCustom->custom_show)
        {
            pstOverlay->b_overlay_update = 0;
        }
        pstOverlay->content_4g = *pstOsdCustom;
        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
    }
}

void anj_osd_bmp_set(osd_custom_content_s *pstOsdCustom)
{
    if (pstOverlay)
    {
        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        pstOverlay->b_bitmap_update = 1;
        if (pstOverlay->bitmap_content.custom_show != pstOsdCustom->custom_show)
        {
            // BMP 显隐切换时，需要重建整张 OSD。
            pstOverlay->b_overlay_update = 0;
        }
        pstOverlay->bitmap_content = *pstOsdCustom;
        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
    }
}

void anj_osd_cloud_set(osd_custom_content_s *pstOsdCustom)
{
    if (pstOverlay)
    {
        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        pstOverlay->b_cloud_update = 1;
        if (pstOverlay->cloud_content.custom_show != pstOsdCustom->custom_show)
        {
            pstOverlay->b_overlay_update = 0;
        }
        pstOverlay->cloud_content = *pstOsdCustom;
        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
    }
}

void anj_osd_sdcard_set(osd_custom_content_s *pstOsdCustom)
{
    if (pstOverlay)
    {
        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        pstOverlay->b_sdcard_update = 1;
        if (pstOverlay->sdcard_content.custom_show != pstOsdCustom->custom_show)
        {
            pstOverlay->b_overlay_update = 0;
        }
        pstOverlay->sdcard_content = *pstOsdCustom;
        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
    }
}

void anj_osd_isp_debug_set(osd_custom_content_s *pstOsdCustom)
{
    if (pstOverlay)
    {
        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        pstOverlay->b_isp_debug_update = 1;
        if (pstOverlay->isp_debug_content.custom_show != pstOsdCustom->custom_show)
        {
            pstOverlay->b_overlay_update = 0;
        }
        pstOverlay->isp_debug_content = *pstOsdCustom;
        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
    }
}

static void anj_osd_isp_debug_flag_check(void)
{
    static int s_flag_on = -1;
    int flag_on = (access(ISP_OSD_DEBUG_FLAG, F_OK) == 0);
    osd_custom_content_s stIspDbg = {0};

    if (flag_on == s_flag_on)
    {
        return;
    }
    s_flag_on = flag_on;

    stIspDbg.custom_show = flag_on;
    if (flag_on)
    {
        stIspDbg.custom_x = 0;
        stIspDbg.custom_y = 1;
        stIspDbg.custom_location = POSITION_TYPE_BY_FOUR_CORNER;
    }
    anj_osd_isp_debug_set(&stIspDbg);
}

void anj_osd_battery_set(osd_custom_content_s *pstOsdCustom)
{
    if (pstOverlay)
    {
        anj_mutex_lock(&s_gAnjOsdCtrlMutex);
        pstOverlay->b_battery_update = 1;
        if (pstOverlay->battery_content.custom_show != pstOsdCustom->custom_show)
        {
            pstOverlay->b_overlay_update = 0;
        }
        pstOverlay->battery_content = *pstOsdCustom;
        anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
    }
}

int anj_osd_cover_set()
{
    int iRet = 0;
    if (pstOverlay)
    {
        anj_osd_cover_destory();
        iRet = anj_osd_cover_create();
    }
    return iRet;
}

int anj_osd_lens_cover_get(void)
{
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    if (pstOverlay == NULL)
    {
        return 0;
    }
    return anj_osd_lens_cover_area_check(&pstMediaConfig->videoConfig[0].videoMask.mainStreamMaskList[0],
                                         &pstOverlay->astResolution[0]);
}

void anj_osd_draw_rect(event_rect_param_s *pstNowRectParam)
{
    if (pstOverlay == NULL)
    {
        return;
    }
    if (pstNowRectParam == NULL)
    {
        __ERR("input invalid!\n");
        return;
    }

    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    /* s32RectCnt==0 为清框：uninit/restart 必须能清掉残留，不受画框开关限制 */
    if (pstNowRectParam->s32RectCnt > 0)
    {
        if (pstAlarmCfg->aiAlarm.pdAlarm[pstNowRectParam->camera].alarmAction.draw_human_enable == 0)
        {
            return;
        }
        if (pstMediaCfg->videoConfig[pstNowRectParam->camera].overlay.enable == 0)
        {
            return;
        }
    }

    pthread_mutex_lock(&s_stOsdRectMutex);
    int video_stream_index = pstNowRectParam->camera * MAX_VENC_CHN;
    int max_stream_index = (pstNowRectParam->camera + 1) * MAX_VENC_CHN;
    for (; video_stream_index < max_stream_index; video_stream_index++)
    {
        rect_param_s *pstRectParam = &gstRectRaram[video_stream_index];
        anj_mw_osd_clean_rect(pstRectParam);

        if (pstNowRectParam->s32RectCnt > 0)
        {
            pstRectParam->bShow = 1;
            pstRectParam->s32RectCnt = pstNowRectParam->s32RectCnt;

            int out_idx = 0;
            for (int i = 0; i < pstNowRectParam->s32RectCnt; i++)
            {
                int box_left_top_x = pstNowRectParam->event_rect[i].pos_x;
                int box_left_top_y = pstNowRectParam->event_rect[i].pos_y;
                int box_width      = pstNowRectParam->event_rect[i].width;
                int box_height     = pstNowRectParam->event_rect[i].height;

                DOUBLE_AREA_ENTRY cur_area = {0};
                anj_zoom_run_get(pstNowRectParam->camera, &cur_area, NULL);
                anj_mw_smart_rect_map_pd_to_zoom(&cur_area, &box_left_top_x, &box_left_top_y, &box_width, &box_height);

                pstRectParam->draw_rect[out_idx].u32Color = pstNowRectParam->event_rect[i].u32Color;
                pstRectParam->draw_rect[out_idx].pos_x = box_left_top_x * pstRectParam->u32Ratio_w / 100;
                pstRectParam->draw_rect[out_idx].pos_y = box_left_top_y * pstRectParam->u32Ratio_h / 100;
                pstRectParam->draw_rect[out_idx].width = box_width * pstRectParam->u32Ratio_w / 100;
                pstRectParam->draw_rect[out_idx].height = box_height * pstRectParam->u32Ratio_h / 100;
                __DBG("video_stream_index:%d i:%d x,y:%d %d w,h:%d %d\n", video_stream_index, i,
                      pstRectParam->draw_rect[out_idx].pos_x, pstRectParam->draw_rect[out_idx].pos_y,
                      pstRectParam->draw_rect[out_idx].width, pstRectParam->draw_rect[out_idx].height);
                out_idx++;
            }
            pstNowRectParam->s32RectCnt = out_idx;
            if (out_idx > 0)
            {
                anj_mw_osd_update_rect(pstRectParam);
            }
            else
            {
                pstRectParam->bShow = 0;
            }
        }
        else
        {
            pstRectParam->bShow = 0;
            pstRectParam->s32RectCnt = 0;
        }
    }
    pthread_mutex_unlock(&s_stOsdRectMutex);
}

void anj_osd_polygon_update(Polygon *pstPolygon)
{
    if (pstOverlay == NULL || pstPolygon == NULL)
    {
        __ERR("input invalid!\n");
        return;
    }

    pthread_mutex_lock(&s_gAnjOsdCtrlMutex);
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        line_param_s *pstPolygonParam = &gstPolygonParam[video_stream_index];
        if (pstPolygon->count > 1)
        {
            pstPolygonParam->bShow = 1;
            pstPolygonParam->s32LineCnt = pstPolygon->count;
            for (int i = 0; i < pstPolygonParam->s32LineCnt; i++)
            {
                pstPolygonParam->u32Color[i] = RGB_VALUE_WHITE;
                pstPolygonParam->stPoint[i].pos_x = pstPolygon->points[i].x * pstPolygonParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                pstPolygonParam->stPoint[i].pos_y = pstPolygon->points[i].y * pstPolygonParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                if (i == (pstPolygonParam->s32LineCnt - 1))
                {
                    pstPolygonParam->enPoint[i].pos_x = pstPolygon->points[0].x * pstPolygonParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                    pstPolygonParam->enPoint[i].pos_y = pstPolygon->points[0].y * pstPolygonParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                }
                else
                {
                    pstPolygonParam->enPoint[i].pos_x = pstPolygon->points[i + 1].x * pstPolygonParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                    pstPolygonParam->enPoint[i].pos_y = pstPolygon->points[i + 1].y * pstPolygonParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                }
                __DBG("line:%d, st_xy:%d %d en_xy:%d %d\n", i, pstPolygonParam->stPoint[i].pos_x, pstPolygonParam->stPoint[i].pos_y,
                      pstPolygonParam->enPoint[i].pos_x, pstPolygonParam->enPoint[i].pos_y);
            }
        }
        else
        {
            pstPolygonParam->bShow = 0;
            pstPolygonParam->s32LineCnt = 0;
        }
    }

    if (pstOverlay)
    {
        pstOverlay->b_overlay_update = 0;
    }

    pthread_mutex_unlock(&s_gAnjOsdCtrlMutex);
}

void anj_osd_cross_line_update(VideoGateAlarm *pstVideoGate)
{
    if (pstOverlay == NULL || pstVideoGate == NULL)
    {
        __ERR("input invalid!\n");
        return;
    }

    pthread_mutex_lock(&s_gAnjOsdCtrlMutex);
    for (int video_stream_index = 0; video_stream_index < MAX_VIDEO_NUM; video_stream_index++)
    {
        line_param_s *pstLineParam = &gstCrossLineParam[video_stream_index];
        int auxLine = 0;
        for (int i = 0; i < MAX_VIDEO_VG_LINE; i++)
        {
            VideoLineStruct *pstCrossLine = &pstVideoGate->data[i];
            if (pstCrossLine->enable)
            {
                AuxLineStruct stAuxLine = {0};
                if (pstCrossLine->direction == 1)
                {
                    stAuxLine.bDrawRightArrow = 1;
                }
                else if (pstCrossLine->direction == 2)
                {
                    stAuxLine.bDrawLeftArrow = 1;
                }
                else
                {
                    stAuxLine.bDrawLeftArrow = 1;
                    stAuxLine.bDrawRightArrow = 1;
                }
                anj_osd_aux_line_param_init(pstCrossLine, &stAuxLine);

                pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                pstLineParam->stPoint[auxLine].pos_x = pstCrossLine->x0Pos * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->stPoint[auxLine].pos_y = pstCrossLine->y0Pos * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->enPoint[auxLine].pos_x = pstCrossLine->x1Pos * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                pstLineParam->enPoint[auxLine].pos_y = pstCrossLine->y1Pos * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                auxLine++;

                if (stAuxLine.bDrawLeftArrow || stAuxLine.bDrawRightArrow)
                {
                    // 垂直方向线
                    pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                    pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                    pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                    pstLineParam->enPoint[auxLine].pos_x = stAuxLine.point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                    pstLineParam->enPoint[auxLine].pos_y = stAuxLine.point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                    auxLine++;
                    if (stAuxLine.bDrawLeftArrow)
                    {
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_left_point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_left_point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_left_point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_left_point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                    }
                    if (stAuxLine.bDrawRightArrow)
                    {
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_right_point_left.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_right_point_left.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                        pstLineParam->u32Color[auxLine] = RGB_VALUE_WHITE;
                        pstLineParam->stPoint[auxLine].pos_x = stAuxLine.point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->stPoint[auxLine].pos_y = stAuxLine.point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_x = stAuxLine.arrow_point_right_point_right.fX * pstLineParam->u32Ratio_w / OSD_IOT_COORDINATE_RATIO;
                        pstLineParam->enPoint[auxLine].pos_y = stAuxLine.arrow_point_right_point_right.fY * pstLineParam->u32Ratio_h / OSD_IOT_COORDINATE_RATIO;
                        auxLine++;
                    }
                }
            }
        }

        if (auxLine > 0)
        {
            pstLineParam->bShow = 1;
            pstLineParam->s32LineCnt = auxLine;
        }
        else
        {
            pstLineParam->bShow = 0;
            pstLineParam->s32LineCnt = 0;
        }
    }
    pstOverlay->b_overlay_update = 0;

    pthread_mutex_unlock(&s_gAnjOsdCtrlMutex);
}

void anj_osd_polygon_twinkle()
{
    if (pstOverlay == NULL)
    {
        return;
    }
    pthread_mutex_lock(&s_gAnjOsdCtrlMutex);
    pstOverlay->PolygonTwinkleTimes = OSD_TWINKE_TIMES;
    pthread_mutex_unlock(&s_gAnjOsdCtrlMutex);
}

void anj_osd_frame_border_twinkle(void)
{
    if (pstOverlay == NULL)
    {
        return;
    }
    pthread_mutex_lock(&s_gAnjOsdCtrlMutex);
    pstOverlay->FrameBorderTwinkleTimes = OSD_TWINKE_TIMES;
    pthread_mutex_unlock(&s_gAnjOsdCtrlMutex);
}

void anj_osd_cross_line_twinkle(int CrossLineTwinkle)
{
    if (pstOverlay == NULL)
    {
        return;
    }
    pthread_mutex_lock(&s_gAnjOsdCtrlMutex);
    for (int i = 0; i < MAX_VIDEO_VG_LINE; i++)
    {
        if (1 & (CrossLineTwinkle >> i))
        {
            pstOverlay->CrossLineTwinkleTimes[i] = OSD_TWINKE_TIMES;
        }
    }
    pthread_mutex_unlock(&s_gAnjOsdCtrlMutex);
}

void anj_osd_update_config()
{
    anj_mutex_lock(&s_gAnjOsdCtrlMutex);
    if (pstOverlay)
    {
        pstOverlay->b_overlay_update = 0;
    }
    anj_mutex_unlock(&s_gAnjOsdCtrlMutex);
}

REGISTER_MODULE(anj_osd, MODULE_PRIORITY_OSD);