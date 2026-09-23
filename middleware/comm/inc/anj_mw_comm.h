#ifndef _ANJ_MW_COMM_H_
#define _ANJ_MW_COMM_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include "anj_mw_log.h"
#include "anj_mw_mem.h"
#include "anj_mw_thread.h"
#include "anj_mw_mutex.h"
#include "anj_mw_time.h"
#include "anj_mw_str.h"
#include "anj_mw_file.h"
#include "anj_mw_list.h"
#include "sdk_option.h"

//***********************************FLASH相关配置***********************************//
#ifdef _USE_NAND_FLASH_
#define SUPPORT_NAND_FLASH (1)
#else
#define SUPPORT_NAND_FLASH (0)
#endif

#define MAX_APPBIN_SIZE (50 * 1024 * 1024)
#define MAX_KERNEL_SIZE (1664 * 1024)
#define MAX_UBOOT_SIZE (280 * 1024)

#if SUPPORT_NAND_FLASH
#define SN_MTD_DEV ""
#define SN_BLOCK ""
#define SN_BLOCK_MTD ""

#define UBOOT_MTD_DEV "/dev/mtd0"
#define UBOOT_MTD_DEV_BK "/dev/mtd1"
#define UBOOT_MTD_BLOCK "/dev/mtdblock0"
#define UBOOT_MTD_BLOCK_BK "/dev/mtdblock1"
#define UBOOT_MTD_BLOCK_ENV "/dev/mtdblock2"
#define UBOOT_BLOCK_MTD "mtd2"

#define KERNEL_BLOCK "/dev/mtd3"
#define KERNEL_BLOCK_BK "/dev/mtd4"
#define KERNEL_BLOCK_MTD "mtd3"
#define FILESYS_BLOCK "/dev/mtd5"
#define FILESYS_BLOCK_BK "/dev/mtd11"

#define OEM_MTD_DEV ""
#define OEM_BLOCK ""

#define DATA_BLOCK1_DEV ""
#define DATA_BLOCK1 ""

#else
#define SN_MTD_DEV "/dev/mtd0"
#define SN_BLOCK "/dev/mtdblock0"
#define SN_BLOCK_MTD "mtd0"

#define UBOOT_MTD_DEV SN_MTD_DEV
#define UBOOT_MTD_DEV_BK ""
#define UBOOT_MTD_BLOCK SN_BLOCK
#define UBOOT_MTD_BLOCK_ENV UBOOT_MTD_BLOCK
#define UBOOT_MTD_BLOCK_BK ""
#define UBOOT_BLOCK_MTD SN_BLOCK_MTD

#define KERNEL_BLOCK "/dev/mtd1"
#define KERNEL_BLOCK_BK ""
#define KERNEL_BLOCK_MTD "mtd1"
#define FILESYS_BLOCK "/dev/mtd2"
#define FILESYS_BLOCK_BK "/dev/mtd3"

#define OEM_MTD_DEV "/dev/mtd5"
#define OEM_BLOCK "/dev/mtdblock5"

#define DATA_BLOCK1_DEV "/dev/mtd4"
#define DATA_BLOCK1 "/dev/mtdblock4"

#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern int hexStrToUInt(const char *, unsigned int, unsigned char *);
extern int hexdataTohexStr(const char *buf, unsigned int len, char *out, unsigned int outbuflen);
extern int WriteP2pid(char *buf, int len, unsigned int nType);


typedef enum
{
    SENSOR_TYPE_NONE = 0, // no sensor

    SENSOR_TYPE_OV2710 = 101,
    SENSOR_TYPE_OV4689,
    SENSOR_TYPE_OV12D40,

    SENSOR_TYPE_SC2135 = 201,
    SENSOR_TYPE_SC2235,
    SENSOR_TYPE_SC2232,
    SENSOR_TYPE_SC2310,
    SENSOR_TYPE_SC4236,
    SENSOR_TYPE_SC2236,
    SENSOR_TYPE_SC3235,
    SENSOR_TYPE_SC8235,
    SENSOR_TYPE_SC2239,
    SENSOR_TYPE_SC2332,
    SENSOR_TYPE_SC2320,
    SENSOR_TYPE_SC200AI,
    SENSOR_TYPE_SC3335,
    SENSOR_TYPE_SC500AI,
    SENSOR_TYPE_SC401AI,
    SENSOR_TYPE_SC2336,
    SENSOR_TYPE_SC3338,
    SENSOR_TYPE_SC830AI,
    SENSOR_TYPE_SC4336P,
    SENSOR_TYPE_SC465SL,

    SENSOR_TYPE_IMX323 = 301,
    SENSOR_TYPE_IMX307,
    SENSOR_TYPE_IMX307_2,
    SENSOR_TYPE_IMX335,
    SENSOR_TYPE_IMX290,
    SENSOR_TYPE_IMX291,
    SENSOR_TYPE_IMX274,
    SENSOR_TYPE_IMX326,
    SENSOR_TYPE_IMX415,
    SENSOR_TYPE_IMX347,
    SENSOR_TYPE_IMX334,
    SENSOR_TYPE_IMX577,
    SENSOR_TYPE_IMX675,

    SENSOR_TYPE_AR0237 = 401,
    SENSOR_TYPE_BG0806,
    SENSOR_TYPE_PS5230,
    SENSOR_TYPE_PS5280,
    SENSOR_TYPE_GC2053,
    SENSOR_TYPE_GC2063,
    SENSOR_TYPE_SP2305,
    SENSOR_TYPE_SP4329,
    SENSOR_TYPE_GC4653,
    SENSOR_TYPE_C4390,
    SENSOR_TYPE_C2399,
    SENSOR_TYPE_GC5603,
    SENSOR_TYPE_GC2083,
    SENSOR_TYPE_GC4023,
    SENSOR_TYPE_MIS4001,

    SENSOR_TYPE_OS08A10 = 501,
    SENSOR_TYPE_OS05A10,
    SENSOR_TYPE_OS05A20,
    SENSOR_TYPE_OS02G10,
    SENSOR_TYPE_OS03B10,
    SENSOR_TYPE_OS03A10,
    SENSOR_TYPE_OS04D10,
    SENSOR_TYPE_OS06A10,

    SENSOR_TYPE_2315E = 601,

    SENSOR_TYPE_SP2306 = 701,

    SENSOR_TYPE_BT1120_720P = 0xf001,
    SENSOR_TYPE_BT1120_1080P,
    SENSOR_TYPE_BT1120_1080I,
} AjSensorType;

typedef enum
{
    PROJECT_TYPE_NORMAL = 0,
    PROJECT_TYPE_LP,
    PROJECT_TYPE_AOV,
} AjProjectType;

typedef enum
{
    CUSTOMER_NORMAL = 0,
    CUSTOMER_WTD,
} AjCustomerType;

typedef enum
{
    LIGHTBOARD_TYPE_WHITE = 0,
    LIGHTBOARD_TYPE_RED,
    LIGHTBOARD_TYPE_WHITE_RED,
} AjLightBoardType;

enum
{
    PROCESS_KEY_START = 0,

    SYSTEM_PROCESS_KEY = 1,
    MEDIA_PROCESS_KEY = 2,
    WEB_PROCESS_KEY = 3,
    RTSP_PROCESS_KEY = 4,
    RECORD_PROCESS_KEY = 5,
    AUX_PROCESS_KEY = 6,

    HIK_PROCESS_KEY = 7,
    JUAN_PROCESS_KEY = 8,
    P2P_PROCESS_KEY = 9,
    TPSSERVER_PROCESS_KEY = 10, // devsdk, ipvs,uvc_server,szy_adaptor
    AC1_PROCESS_KEY = 11,       // DEVSDK(demo), DANALE(有goolink的时候),mobile
    AC2_PROCESS_KEY = 12,       // taoshi_server, xm_hank_server,anko_adaptor
    RTMP_PROCESS_KEY = 13,
    H5LIVE_PROCESS_KEY = 14,
    GBT28181_PROCESS_KEY = 15,
    HB_PROCESS_KEY = 16,
    IVE_PROCESS_KEY = 17,

    // 下面这些不用配置配置
    DEBUG_PROCESS_KEY,
    LOG_PROCESS_KEY,
    AUDIO_TALKBACK_KEY,
    STREAM_PROCESS_KEY,
    PROCESS_KEY_END
};

typedef struct
{
    unsigned int u32Width;
    unsigned int u32Height;
} ANJ_SIZE_S;

typedef struct
{
    double xPos;    // 裁剪区域左上角坐标的X，归一化到[0,1]
    double yPos;    // 裁剪区域左上角坐标的Y，归一化到[0,1]
    double width;   // 水平放大倍数，将裁剪区域放大到到原图的宽度
    double height;  // 垂直放大倍数，将裁剪区域放大到到原图的高度
} DOUBLE_AREA_ENTRY;

#define AJ_APP_PATH "/opt/ch"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define DOUBLE_EPS (1e-8)
#define DOUBLE_EQUAL(a, b) ((fabs((a) - (b))) < (DOUBLE_EPS))                                  // 等于
#define DOUBLE_NOT_EQUAL(a, b) ((fabs((a) - (b))) >= (DOUBLE_EPS))                             // 不等于
#define DOUBLE_GREATER(a, b) ((a) > (b) && fabs((a) - (b)) >= DOUBLE_EPS)                      /// 大于
#define DOUBLE_LESS(a, b) ((a) < (b) && fabs((a) - (b)) >= DOUBLE_EPS)                         // 小于
#define DOUBLE_GREATER_EQUAL(a, b) ((a) >= (b) || (fabs((a) - (b)) < DOUBLE_EPS && (a) > (b))) // 大于等于
#define DOUBLE_LESS_EQUAL(a, b) ((a) <= (b) || (fabs((a) - (b)) < DOUBLE_EPS && (a) < (b)))    // 小于等于
#define AREA_EQUAL(a, b) (                 \
    fabs((a).xPos - (b).xPos) < 1e-10 &&   \
    fabs((a).yPos - (b).yPos) < 1e-10 &&   \
    fabs((a).width - (b).width) < 1e-10 && \
    fabs((a).height - (b).height) < 1e-10)

#define ANJ_ALIGN_UP(val, alignment) ((((val) + (alignment) - 1) / (alignment)) * (alignment))
#define ANJ_ALIGN_DOWN(val, alignment) (((val) / (alignment)) * (alignment))

#define ALIGN_BACK(x, a) (((x) / (a)) * (a))
#define ALIGN_FRONT(x, a) ((((x) + (a) / 2) / (a)) * (a))

#define CHECK_IN_RANGE(data, low, high) ((data) >= (low) && (data) <= (high))

#define CheckAtoU(arg) ((((const char *)arg) == NULL) ? 0 : strtoul(arg, 0, 10))

#define BIT_GET_64(res, index) (((unsigned long long)(res)) & (((unsigned long long)0x1) << (index)))
#define BIT_SET_64(res, index) (res = (((unsigned long long)(res)) | (((unsigned long long)0x1) << (index))))
#define BIT_UNSET_64(res, index) (res = ((unsigned long long)(res) & (~(((unsigned long long)0x1) << (index)))))

#define BIT_GET_32(res, index) (((unsigned int)(res)) & (((unsigned int)0x1) << (index)))
#define BIT_SET_32(res, index) (res = (((unsigned int)(res)) | (((unsigned int)0x1) << (index))))
#define BIT_UNSET_32(res, index) (res = ((unsigned int)(res) & (~(((unsigned int)0x1) << (index)))))

#define BIT_GET_16(res, index) (((unsigned short)(res)) & (((unsigned short)0x1) << (index)))
#define BIT_SET_16(res, index) (res = (((unsigned short)(res)) | (((unsigned short)0x1) << (index))))
#define BIT_UNSET_16(res, index) (res = ((unsigned short)(res) & (~(((unsigned short)0x1) << (index)))))

#define BIT_GET_8(res, index) (((unsigned char)(res)) & (((unsigned char)0x1) << (index)))
#define BIT_SET_8(res, index) (res = (((unsigned char)(res)) | (((unsigned char)0x1) << (index))))
#define BIT_UNSET_82(res, index) (res = ((unsigned char)(res) & (~(((unsigned char)0x1) << (index)))))

#define STR_TO_LOWER(str)                                   \
do                                                      \
{                                                       \
    if (str != NULL)                                    \
    {                                                   \
        for (char *_ptr = (str); *_ptr != '\0'; _ptr++) \
        {                                               \
            if (*_ptr >= 'A' && *_ptr <= 'Z')           \
            {                                           \
                *_ptr = *_ptr - 'A' + 'a';              \
            }                                           \
        }                                               \
    }                                                   \
} while (0)

#define CHECK_VALUE_LIMIT_RANGE(value, min_value, max_value) \
do                                                       \
{                                                        \
    if ((value) > (max_value))                           \
        (value) = (max_value);                           \
    else if ((value) < (min_value))                      \
        (value) = (min_value);                           \
} while (0)

#define CHECK_VALUE_LIMIT_RANGE_DEF(value, min_value, max_value, default_value) \
do                                                                          \
{                                                                           \
    if ((value) < (min_value) || (value) > (max_value))                     \
    {                                                                       \
        (value) = (default_value);                                          \
    }                                                                       \
} while (0)

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(expr) \
do                     \
{                      \
    (void)(expr);      \
} while (0)
#endif

/* _func_ 函数  */
/* _ret_ 期望返回值  */
/* errMsg 错误日志  */
#define ANJ_CHK_FUNC(_func_, _ret_, errMsg)               \
do                                                    \
{                                                     \
    iRet = (_func_);                                  \
    if (iRet != (_ret_))                              \
    {                                                 \
        __ERR("func err:%x, msg:%s\n", iRet, errMsg); \
        goto endFunc;                                 \
    }                                                 \
    else                                              \
    {                                                 \
        __DBG("func ok\n");                           \
    }                                                 \
} while (0)

/* _condition_ 条件  */
/* _errRet_ 不满足条件时返回错误码  */
/* errMsg 错误日志  */
#define ANJ_CHK(_condition_, _errRet_, errMsg, ...)   \
do                                                \
{                                                 \
    if (!(_condition_))                           \
    {                                             \
        iRet = (_errRet_);                        \
        __ERR("msg:" errMsg "\n", ##__VA_ARGS__); \
        goto endFunc;                             \
    }                                             \
} while (0)

#ifndef _NULL_POINTER_CHECK_
#define _NULL_POINTER_CHECK_(p, errcode)        \
do                                          \
{                                           \
    if (!(p))                               \
    {                                       \
        __ERR("pointer[%s] is NULL\n", #p); \
        return errcode;                     \
    }                                       \
} while (0)
#endif

#define anj_isspace(c)      ((c) == ' ' || ((c) >= '\t' && (c) <= '\r'))
#define anj_isascii(c)      (((c) & ~0x7f) == 0)
#define anj_isupper(c)	    ((c) >= 'A' && (c) <= 'Z')
#define anj_islower(c)      ((c) >= 'a' && (c) <= 'z')
#define anj_isalpha(c)      (anj_isupper(c) || anj_islower(c))
#define anj_isdigit_c(c)    ((c) >= '0' && (c) <= '9')
#define anj_isprint(c)      ((c) >= 0x20 && (c) <= 0x7e)


typedef struct jiffy_counts_t
{
    /* Linux 2.4.x has only first four */
    unsigned long long usr, nic, sys, idle;
    unsigned long long iowait, irq, softirq, steal;
    unsigned long long total;
    unsigned long long busy;
} jiffy_counts_t;

int anj_mw_system(const char *cmd);
int anj_mw_system_with_param(const char* fmt, ...);
void anj_mw_system_free_cache();

void ResGetDmt(const char *resName, const char *stdName, int *width, int *height);

void GetVideoSize(const char *resName, int tvsystem, int *width, int *height);

ANJ_SIZE_S getPicSize(const char *resolution, int tvsystem, int bRotate, int bTrue);

ANJ_SIZE_S anj_mw_sensor_get_size(void);
int anj_mw_sensor_support_wdr(void); /* 1 支持，0 不支持 */

int anj_mw_check_value_in_range(int value, int min_value, int max_value);
int anj_mw_check_value_by_default(int value, int min_value, int max_value, int def_value);

/*
    执行命令pCmd，将输出结果存在pResult中
    return 0 执行成功
            -1 执行失败
*/
int ExecShellCmd(const char *pCmd, char *pResult, int iResultLen);

/*
    执行命令pCmd，在输出中查找字符串string
    return 0 不存在
            1 存在
            -1 错误
    使用 strcasestr会忽略大小写
*/
int SearchStringInCmd(const char *pCmd, const char *string);

char *GetFileNameFromFullName(const char *full_file_name);

unsigned long long GetPathFreeSpace(const char *path);

int mysystem_with_param(const char* fmt, ...);

void cpu_info_get(jiffy_counts_t *cpu_info);

void aj_swap_value(int *a, int *b);

unsigned char GetHexValue(char *str);

/************************************************************************ 
 函数作用：  获取一个字节的bit值 
 参数说明：  data      字节内容 
            nPos        位置,只能是0-7 (从右往左) 
            
 返 回 值： 
           0/1 
************************************************************************/  
unsigned char GetBitValue(char data, const int nPos);

/************************************************************************ 
 函数作用：  更改一个字节的bit值 
 参数说明：  szTemp      原字节内容 
            nPos        位置,只能是0-7 (从右往左) 
            nValue      位值,只能是0/1 
 返 回 值： 无 
************************************************************************/  
void SetBitValue(char * szTemp, const int nPos, const int nValue);
int IsMainStream(int venchn);

#if defined(__cplusplus)
#define ANJ_LINK_KEEP(symbol)                    \
    extern "C" __attribute__((used)) void symbol(void); \
    extern "C" __attribute__((used)) void symbol(void) {}
#else
#define ANJ_LINK_KEEP(symbol)                    \
    __attribute__((used)) void symbol(void);    \
    __attribute__((used)) void symbol(void) {}
#endif

#ifdef __cplusplus
}
#endif
#endif
