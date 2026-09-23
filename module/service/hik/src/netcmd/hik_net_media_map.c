#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "hik_net_media_map.h"

/* Resolution codes aligned with trunk aj_common.h / GetHikResolutionByWH */
#define HIK_RESOLUTION_352X288 1
#define HIK_RESOLUTION_176X144 2
#define HIK_RESOLUTION_704X576 3
#define HIK_RESOLUTION_704X288 4
#define HIK_RESOLUTION_96X80_OR_96X64 6
#define HIK_RESOLUTION_320X240 15
#define HIK_RESOLUTION_160X120 13
#define HIK_RESOLUTION_528X384_OR_528X320 12
#define HIK_RESOLUTION_384X288 11
#define HIK_RESOLUTION_576X576 10
#define HIK_RESOLUTION_640X480 16
#define HIK_RESOLUTION_1600X1200 17
#define HIK_RESOLUTION_800X600 18
#define HIK_RESOLUTION_1280X720 19
#define HIK_RESOLUTION_1280X960 20
#define HIK_RESOLUTION_1600X900 21
#define HIK_RESOLUTION_1360X1024 22
#define HIK_RESOLUTION_1536X1536 23
#define HIK_RESOLUTION_1920X1920 24
#define HIK_RESOLUTION_1080P 27
#define HIK_RESOLUTION_2560X1920 28
#define HIK_RESOLUTION_1600X304 29
#define HIK_RESOLUTION_2048X1536 30
#define HIK_RESOLUTION_3MP 31
#define HIK_RESOLUTION_2448X2048 32
#define HIK_RESOLUTION_2448X1200 33
#define HIK_RESOLUTION_2448X800 34
#define HIK_RESOLUTION_1024X768 35
#define HIK_RESOLUTION_1280X1024 36
#define HIK_RESOLUTION_960X576 37
#define HIK_RESOLUTION_1080I 38
#define HIK_RESOLUTION_1440X900 39
#define HIK_RESOLUTION_HD_F 40
#define HIK_RESOLUTION_1920X540 41
#define HIK_RESOLUTION_960X540 42
#define HIK_RESOLUTION_1366X768_1 46
#define HIK_RESOLUTION_1680X1050 49
#define HIK_RESOLUTION_720X720 50
#define HIK_RESOLUTION_1280X1280 51
#define HIK_RESOLUTION_2048X768 52
#define HIK_RESOLUTION_2048X2048 53
#define HIK_RESOLUTION_2560X2048 54
#define HIK_RESOLUTION_3072X2048 55
#define HIK_RESOLUTION_2304X1296 56
#define HIK_RESOLUTION_1280X800 57
#define HIK_RESOLUTION_1600X900_1 58
#define HIK_RESOLUTION_2752X2208 60
#define HIK_RESOLUTION_4000X3000 62
#define HIK_RESOLUTION_4096X2160 63
#define HIK_RESOLUTION_3840X2160 64
#define HIK_RESOLUTION_4000X2250 65
#define HIK_RESOLUTION_3072X1728 66
#define HIK_RESOLUTION_2592X1944 67
#define HIK_RESOLUTION_2464X1520 68
#define HIK_RESOLUTION_1280X1920 69
#define HIK_RESOLUTION_2560X1440 70
#define HIK_RESOLUTION_1024X1024 71

typedef struct
{
    int index;
    int value;
} hik_index_value_t;

static const hik_index_value_t s_framerate_table[] = {
    {5, 1},   {6, 2},   {7, 4},   {8, 6},   {9, 8},   {10, 10}, {11, 12},
    {12, 16}, {13, 20}, {14, 15}, {15, 18}, {16, 22}, {17, 25}, {18, 30},
    {19, 35}, {20, 40}, {21, 45}, {22, 50}, {23, 55}, {24, 60}, {29, 100},
    {30, 120},
};

UINT8 hik_resolution_by_wh(int width, int height)
{
    if (width == 352 && height == 288)
        return HIK_RESOLUTION_352X288;
    if (width == 176 && height == 144)
        return HIK_RESOLUTION_176X144;
    if (width == 704 && height == 576)
        return HIK_RESOLUTION_704X576;
    if (width == 704 && height == 480)
        return HIK_RESOLUTION_704X576;
    if (width == 704 && height == 288)
        return HIK_RESOLUTION_704X288;
    if ((width == 96 && height == 80) || (width == 96 && height == 64))
        return HIK_RESOLUTION_96X80_OR_96X64;
    if (width == 320 && height == 240)
        return HIK_RESOLUTION_320X240;
    if (width == 160 && height == 120)
        return HIK_RESOLUTION_160X120;
    if ((width == 528 && height == 384) || (width == 528 && height == 320))
        return HIK_RESOLUTION_528X384_OR_528X320;
    if (width == 384 && height == 288)
        return HIK_RESOLUTION_384X288;
    if (width == 576 && height == 576)
        return HIK_RESOLUTION_576X576;
    if (width == 640 && height == 480)
        return HIK_RESOLUTION_640X480;
    if (width == 1600 && height == 1200)
        return HIK_RESOLUTION_1600X1200;
    if (width == 800 && height == 600)
        return HIK_RESOLUTION_800X600;
    if (width == 1280 && height == 720)
        return HIK_RESOLUTION_1280X720;
    if (width == 1280 && height == 960)
        return HIK_RESOLUTION_1280X960;
    if (width == 1600 && height == 900)
        return HIK_RESOLUTION_1600X900_1;
    if (width == 1360 && height == 1024)
        return HIK_RESOLUTION_1360X1024;
    if (width == 1536 && height == 1536)
        return HIK_RESOLUTION_1536X1536;
    if (width == 1920 && height == 1920)
        return HIK_RESOLUTION_1920X1920;
    if (width == 1920 && height == 1080)
        return HIK_RESOLUTION_1080P;
    if (width == 2560 && height == 1920)
        return HIK_RESOLUTION_2560X1920;
    if (width == 1600 && height == 304)
        return HIK_RESOLUTION_1600X304;
    if (width == 2048 && height == 1536)
        return HIK_RESOLUTION_2048X1536;
    if (width == 1920 && height == 1536)
        return HIK_RESOLUTION_3MP;
    if (width == 2448 && height == 2048)
        return HIK_RESOLUTION_2448X2048;
    if (width == 2448 && height == 1200)
        return HIK_RESOLUTION_2448X1200;
    if (width == 2448 && height == 800)
        return HIK_RESOLUTION_2448X800;
    if (width == 1024 && height == 768)
        return HIK_RESOLUTION_1024X768;
    if (width == 1280 && height == 1024)
        return HIK_RESOLUTION_1280X1024;
    if (width == 960 && height == 576)
        return HIK_RESOLUTION_960X576;
    if (width == 1440 && height == 900)
        return HIK_RESOLUTION_1440X900;
    if (width == 1920 && height == 540)
        return HIK_RESOLUTION_1920X540;
    if (width == 960 && height == 540)
        return HIK_RESOLUTION_960X540;
    if (width == 1366 && height == 768)
        return HIK_RESOLUTION_1366X768_1;
    if (width == 1680 && height == 1050)
        return HIK_RESOLUTION_1680X1050;
    if (width == 720 && height == 720)
        return HIK_RESOLUTION_720X720;
    if (width == 1280 && height == 1280)
        return HIK_RESOLUTION_1280X1280;
    if (width == 2048 && height == 768)
        return HIK_RESOLUTION_2048X768;
    if (width == 2048 && height == 2048)
        return HIK_RESOLUTION_2048X2048;
    if (width == 2560 && height == 2048)
        return HIK_RESOLUTION_2560X2048;
    if (width == 3072 && height == 2048)
        return HIK_RESOLUTION_3072X2048;
    if (width == 2304 && height == 1296)
        return HIK_RESOLUTION_2304X1296;
    if (width == 1280 && height == 800)
        return HIK_RESOLUTION_1280X800;
    if (width == 2752 && height == 2208)
        return HIK_RESOLUTION_2752X2208;
    if (width == 4000 && height == 3000)
        return HIK_RESOLUTION_4000X3000;
    if (width == 4096 && height == 2160)
        return HIK_RESOLUTION_4096X2160;
    if (width == 3840 && height == 2160)
        return HIK_RESOLUTION_3840X2160;
    if (width == 4000 && height == 2250)
        return HIK_RESOLUTION_4000X2250;
    if (width == 3072 && height == 1728)
        return HIK_RESOLUTION_3072X1728;
    if (width == 2592 && height == 1944)
        return HIK_RESOLUTION_2592X1944;
    if (width == 2464 && height == 1520)
        return HIK_RESOLUTION_2464X1520;
    if (width == 1280 && height == 1920)
        return HIK_RESOLUTION_1280X1920;
    if (width == 2560 && height == 1440)
        return HIK_RESOLUTION_2560X1440;
    if (width == 1024 && height == 1024)
        return HIK_RESOLUTION_1024X1024;
    (void)HIK_RESOLUTION_1600X900;
    (void)HIK_RESOLUTION_1080I;
    (void)HIK_RESOLUTION_HD_F;
    return 0;
}

UINT32 hik_framerate_index(int fps)
{
    size_t i = 0;

    if (fps <= 0)
    {
        return 18; /* 30fps default */
    }
    for (i = 0; i < sizeof(s_framerate_table) / sizeof(s_framerate_table[0]); i++)
    {
        if (s_framerate_table[i].value == fps)
        {
            return (UINT32)s_framerate_table[i].index;
        }
    }
    /* nearest by absolute difference */
    {
        int best_idx = 18;
        int best_diff = 1000;
        for (i = 0; i < sizeof(s_framerate_table) / sizeof(s_framerate_table[0]); i++)
        {
            int d = s_framerate_table[i].value - fps;
            if (d < 0)
            {
                d = -d;
            }
            if (d < best_diff)
            {
                best_diff = d;
                best_idx = s_framerate_table[i].index;
            }
        }
        return (UINT32)best_idx;
    }
}

UINT8 hik_video_enc_type(const char *encode_name)
{
    if (encode_name == NULL)
    {
        return STD_H264;
    }
    if (strncasecmp(encode_name, "H265", 4) == 0 || strncasecmp(encode_name, "Smart265", 8) == 0 ||
        strncasecmp(encode_name, "H265+", 5) == 0)
    {
        return STD_H265;
    }
    return STD_H264;
}

UINT8 hik_audio_enc_type(const char *audio_name)
{
    if (audio_name == NULL)
    {
        return 0;
    }
    if (strcasecmp(audio_name, "G.711U") == 0 || strcasecmp(audio_name, "PCMU") == 0 ||
        strcasecmp(audio_name, "G711U") == 0 || strcasecmp(audio_name, "G.711") == 0)
    {
        return HIK_AUDIOTYPE_G711U;
    }
    if (strcasecmp(audio_name, "G.711A") == 0 || strcasecmp(audio_name, "PCMA") == 0 ||
        strcasecmp(audio_name, "G711A") == 0)
    {
        return HIK_AUDIOTYPE_G711A;
    }
    return 0;
}

int hik_wh_by_resolution(UINT8 res_code, int *width, int *height)
{
    int w = 0;
    int h = 0;

    if (width == NULL || height == NULL)
    {
        return -1;
    }

    switch (res_code)
    {
    case HIK_RESOLUTION_352X288:
        w = 352;
        h = 288;
        break;
    case HIK_RESOLUTION_176X144:
        w = 176;
        h = 144;
        break;
    case HIK_RESOLUTION_704X576:
        w = 704;
        h = 576;
        break;
    case HIK_RESOLUTION_704X288:
        w = 704;
        h = 288;
        break;
    case HIK_RESOLUTION_320X240:
        w = 320;
        h = 240;
        break;
    case HIK_RESOLUTION_640X480:
        w = 640;
        h = 480;
        break;
    case HIK_RESOLUTION_800X600:
        w = 800;
        h = 600;
        break;
    case HIK_RESOLUTION_1280X720:
        w = 1280;
        h = 720;
        break;
    case HIK_RESOLUTION_1280X960:
        w = 1280;
        h = 960;
        break;
    case HIK_RESOLUTION_1080P:
        w = 1920;
        h = 1080;
        break;
    case HIK_RESOLUTION_2304X1296:
        w = 2304;
        h = 1296;
        break;
    case HIK_RESOLUTION_2560X1440:
        w = 2560;
        h = 1440;
        break;
    case HIK_RESOLUTION_3840X2160:
        w = 3840;
        h = 2160;
        break;
    case HIK_RESOLUTION_2592X1944:
        w = 2592;
        h = 1944;
        break;
    case HIK_RESOLUTION_2048X1536:
        w = 2048;
        h = 1536;
        break;
    default:
        w = 1280;
        h = 720;
        break;
    }

    *width = w;
    *height = h;
    return 0;
}

UINT32 hik_framerate_from_index(UINT32 idx)
{
    size_t i = 0;

    for (i = 0; i < sizeof(s_framerate_table) / sizeof(s_framerate_table[0]); i++)
    {
        if ((UINT32)s_framerate_table[i].index == idx)
        {
            return (UINT32)s_framerate_table[i].value;
        }
    }
    return 25;
}

UINT32 hik_bitrate_kbps_from_index(UINT32 idx)
{
    /* match old maxBitValueTable (values in bits); return kbps */
    static const hik_index_value_t s_bitrate_table[] = {
        {2, 32},    {3, 48},    {4, 64},    {5, 80},    {6, 96},   {7, 128},
        {8, 160},   {9, 192},   {10, 224},  {11, 256},  {12, 320}, {13, 384},
        {14, 448},  {15, 512},  {16, 640},  {17, 768},  {18, 896}, {19, 1024},
        {20, 1280}, {21, 1536}, {22, 1792}, {23, 2048}, {24, 3072}, {25, 4096},
        {26, 8192}, {27, 16384},
    };
    size_t i = 0;

    for (i = 0; i < sizeof(s_bitrate_table) / sizeof(s_bitrate_table[0]); i++)
    {
        if ((UINT32)s_bitrate_table[i].index == idx)
        {
            return (UINT32)s_bitrate_table[i].value;
        }
    }
    return 512;
}

void hik_resolution_name_by_wh(int width, int height, char *name, int name_len)
{
    if (name == NULL || name_len <= 0)
    {
        return;
    }
    name[0] = '\0';

    if (width == 1920 && height == 1080)
        snprintf(name, (size_t)name_len, "1080P");
    else if (width == 1280 && height == 720)
        snprintf(name, (size_t)name_len, "720P");
    else if (width == 2560 && height == 1440)
        snprintf(name, (size_t)name_len, "1440P");
    else if (width == 3840 && height == 2160)
        snprintf(name, (size_t)name_len, "4K");
    else if (width == 640 && height == 360)
        snprintf(name, (size_t)name_len, "640X360");
    else if (width == 640 && height == 480)
        snprintf(name, (size_t)name_len, "VGA");
    else if (width == 2304 && height == 1296)
        snprintf(name, (size_t)name_len, "2304X1296");
    else if (width == 2592 && height == 1944)
        snprintf(name, (size_t)name_len, "2592X1944");
    else if (width == 2048 && height == 1536)
        snprintf(name, (size_t)name_len, "2048X1536");
    else
        snprintf(name, (size_t)name_len, "%dX%d", width, height);
}
