#include "anj_mw_comm.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>
#include <unistd.h>
#include <math.h>

#include "anj_mw_net.h"
#include "anj_mw_crypt.h"
#include "anj_mw_media_sys.h"
#include "anj_mw_hwctrl.h"
#include "anj_mw_watchdog.h"
#include "anj_mw_thread.h"
#include "anj_mw_crypt.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_systime.h"
#include "anj_module.h"
#include "anj_sysctl.h"
#include "anj_net.h"
#include "anj_sys.h"
#include "anj_ser.h"
#include "anj_audio.h"
#include "anj_osd.h"
#include "anj_sdcard.h"
#include "anj_record.h"
#include "record_log.h"
#include "function_list.h"
#include "firmware_util.h"
#include "driver_interface.h"
#include "user_auth.h"
#include "eventhub.h"
#include "anj_search.h"
#include "anj_factory.h"
#include "alarm_link.h"

#define ENCRIPT_DATA_LEN (8)
#define ENCRIPT_DATA_FILE "/mnt/nand/encript.dat"
#define CLEAR_SN_FILE "/tmp/sn_clear"
#define SHOW_SN_FILE "/tmp/sn_show"

#define DEF_CONFIG_NAME "config.default.xml"
#define FAKE_VIDEO_RES_FILE "fakevideo.xml"
#define PTZ_STEP_CONFIG_NAME "ptzstep.cfg"
#define AF_FOCUS_CONFIG_NAME "af_focus.config.xml"
#define AUDIO_CONFIG_NAME "audio.config.xml"
#define DZOOM_CUST_SETTING_FILE "/dzoom.config.xml"                            // 客户定制的数字变倍倍率设置
#define DZOOM_CUST_SETTING_FILE_FULL DATA_BLOCK_MOUNT_PATH "/dzoom.config.xml" // 客户定制的数字变倍倍率设置
#define DZOOM_ENCODE_REAL_SETTING_FILE "/tmp/flag.dzoom.setting"               // 编码支持的数字变倍最大倍率与设置倍率

#define TMP_OEM_FILE_NAME OEM_MOUNT_PATH ".update"
#define OEM_MP3_PATH OEM_MOUNT_PATH "/mp3"
#define OEM_APP_PATH OEM_MOUNT_PATH "/app"

#define TMP_UBOOT_FILE_NAME "/tmp/uboot.image"
#define TMP_TGZ_FILE_NAME "/tmp/config.7z"
#define TMP_KERNEL_FILE_NAME "/tmp/kernel.image"
#define TMP_FILESYSTEM_FILE_NAME "/tmp/filesystem.image"

#define AJ_CURR_ROM_MD5_FILE_NAME DATA_BLOCK_MOUNT_PATH "/rom_md5.txt"
#define USERDEF_CONFIG_PATH DATA_BLOCK_MOUNT_PATH "/userdef.cfg"

#define ANJ_PARTNER_FILE_NAME DATA_BLOCK_MOUNT_PATH "/partner.xml"

// 支持uboot tf卡升级的最小版本
#define MIN_FS_VERSION "V3.4.1.6"

#define HOTSPOT_KEY_HOLD_TIME_MS (500)
#define RESTORE_KEY_HOLD_TIME_MS (5000)

#define MIN_KERNEL_SIZE (1024 * 1024)
#define MIN_APPBIN_SIZE (3 * 1024 * 1024)
#define MIN_UBOOT_SIZE (80 * 1024)
#define MIN_TGZ_SIZE (0 * 1024)
#define MAX_TGZ_SIZE (160 * 1024)

#define ANJ_SYSMNG_CPU_INFO_STR_LEN 512

#define ANJ_SYSMNG_CALC_TOTAL_DIFF                                 \
    do                                                             \
    {                                                              \
        total_diff = (unsigned)(p_jif->total - p_prev_jif->total); \
        if (total_diff == 0)                                       \
            total_diff = 1;                                        \
    } while (0)
#define ANJ_SYSMNG_CALC_STAT(xxx) double xxx = 100.0 * (double)(p_jif->xxx - p_prev_jif->xxx) / (double)total_diff

enum
{
    TYPE_ID_TYPE_AIOT = 0,
    TYPE_ID_TYPE_MAX,
};

typedef struct
{
    char *filename;
    char *bufptr;
    int buflen;
} anj_firm_update_s;

typedef struct anj_sysmng_jiffy_counts_t
{
    unsigned long long usr, nic, sys, idle;
    unsigned long long iowait, irq, softirq, steal;
    unsigned long long total;
    unsigned long long busy;
} anj_sysmng_jiffy_counts_t;

extern int ReadEncriptDataFromSoft_ex(unsigned char *buf, int buflen);
extern int WriteEncriptDataToSoft(unsigned char *buf, int len, int version);
extern int soft_enc_xml_sn_data_parse(const char *xmlBuf, char *cameraid, int buflen1, char *data, int buflen2, char *checksum, int buflen3, int *version);
extern const char *get_uuid();
extern void sn_password(const char *sn, const char *uuid, char *out_password);
extern int SupportBootAutoUpdate();

static DevInfo gstDevInfo = {0};
static pthread_mutex_t s_stViewerMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_stViewerNum = 0;
static anj_thread_s s_stResetThread = {0};

static int s_RestoreSaveNetCfg = 0;

static anj_sysmng_jiffy_counts_t s_cpu_data_stat0;
static anj_sysmng_jiffy_counts_t s_cpu_data_stat1;
static char s_cpu_info_str[ANJ_SYSMNG_CPU_INFO_STR_LEN] = {0};

static int is_version_part(const char *s)
{
    if (s == NULL || s[0] != 'V')
    {
        return 0;
    }
    const char *version = s + 1;
    int len = strlen(version);
    int i;

    // 无内容或无点
    if (len == 0 || strchr(version, '.') == NULL)
    {
        return 0;
    }
    // 检查所有字符是否为数字或点
    for (i = 0; i < len; i++)
    {
        if (!isdigit((unsigned char)version[i]) && version[i] != '.')
        {
            return 0;
        }
    }
    // 检查开头或结尾是否为点
    if (version[0] == '.' || version[len - 1] == '.')
    {
        return 0;
    }
    // 检查连续的点
    for (i = 0; i < len - 1; i++)
    {
        if (version[i] == '.' && version[i + 1] == '.')
        {
            return 0;
        }
    }

    return 1;
}

static void parse_version(const char *version, int parts[4])
{
    sscanf(version + 1, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]);
}

static int str_compare_versions(const char *v1, const char *v2)
{
    int i;
    int parts1[4], parts2[4];
    parse_version(v1, parts1);
    parse_version(v2, parts2);

    for (i = 0; i < 4; ++i)
    {
        if (parts1[i] > parts2[i])
        {
            return 1;
        }
        else if (parts1[i] < parts2[i])
        {
            return -1;
        }
    }

    return 0;
}

// 这里只调用一次
static RESOLUTION_ENTRY *video_delete_res_get(int *resEntryLength)
{
    int max_delete_entry_cnt = 10;
    RESOLUTION_ENTRY *p_delete_resEntrys = NULL;

    *resEntryLength = 0;

    if (access(VIDEO_RES_DELETE_FILE, F_OK) != 0)
    {
        __DBG("video res delete file don't exist!\n");
        return NULL;
    }

    p_delete_resEntrys = anj_mw_malloc(max_delete_entry_cnt * sizeof(RESOLUTION_ENTRY));
    if (p_delete_resEntrys == NULL)
    {
        return NULL;
    }

    memset(p_delete_resEntrys, 0, sizeof(RESOLUTION_ENTRY) * max_delete_entry_cnt);

    int iRet = -1;
    int iIndex = 0;

    char *buf = anj_mw_read_file_buffer(VIDEO_RES_DELETE_FILE);
    if (buf == NULL || strlen(buf) == 0)
    {
        __ERR("video res delete file exist but read file error\n");
        goto __failed;
    }

    char *ptr = strtok(buf, "\n");
    while (ptr != NULL)
    {
        char *pc = NULL;
        char *pos = NULL;
        char tmpbuf[128] = {0};

        memcpy(tmpbuf, ptr, strlen(ptr));

        if (strlen(tmpbuf) < 2)
        {
            ptr = strtok(NULL, "\n");
            continue;
        }

        pc = strchr(tmpbuf, '\r');
        if (pc)
        {
            *pc = '\0';
        }

        if (iIndex >= (max_delete_entry_cnt - 1))
        {
            return p_delete_resEntrys;
        }

        pos = tmpbuf;
        pc = strchr(pos, ',');
        if (pc)
        {
            *pc = '\0';
            memset(p_delete_resEntrys[iIndex].res_name, 0, 16);
            StrCpy(p_delete_resEntrys[iIndex].res_name, 16, tmpbuf);
        }
        else
        {
            goto __failed;
        }

        pos = pc + 1;
        pc = strchr(pos, ',');
        if (pc)
        {
            *pc = '\0';
            memset(p_delete_resEntrys[iIndex].codec_name, 0, 16);
            StrCpy(p_delete_resEntrys[iIndex].res_name, 16, pos);
        }
        else
        {
            goto __failed;
        }

        int stream_type = 0;
        iRet = sscanf(pc + 1, "%d", &stream_type);
        if (iRet == 1)
        {
            p_delete_resEntrys[iIndex].stream_type = stream_type;
        }
        else
        {
            goto __failed;
        }

        iIndex++;
        ptr = strtok(NULL, "\n");
    }

    *resEntryLength = iIndex;
    return p_delete_resEntrys;

__failed:
    if (p_delete_resEntrys)
    {
        anj_mw_free(p_delete_resEntrys);
        p_delete_resEntrys = NULL;
    }

    return NULL;
}

static RESOLUTION_ENTRY *video_cust_res_get(int *resEntryLength)
{
    int max_entry_cnt = 10;
    RESOLUTION_ENTRY *p_cust_resEntrys = NULL;

    *resEntryLength = 0;

    if (access(VIDEO_RES_CUST_FILE, F_OK) != 0)
    {
        __DBG("video res cust file don't exist!\n");
        return NULL;
    }

    p_cust_resEntrys = (RESOLUTION_ENTRY *)anj_mw_malloc(max_entry_cnt * sizeof(RESOLUTION_ENTRY));
    if (p_cust_resEntrys == NULL)
    {
        return NULL;
    }
    memset(p_cust_resEntrys, 0, sizeof(RESOLUTION_ENTRY) * max_entry_cnt);

    int iRet = -1;
    int iIndex = 0;

    char *buf = anj_mw_read_file_buffer(VIDEO_RES_DELETE_FILE);
    if (buf == NULL || strlen(buf) == 0)
    {
        __ERR("video res delete file exist but read file error\n");
        goto __failed;
    }

    char *ptr = strtok(buf, "\n");
    while (ptr != NULL)
    {
        char *pc = NULL;
        char *pos = NULL;
        char tmpbuf[128] = {0};

        memcpy(tmpbuf, ptr, strlen(ptr));

        if (strlen(tmpbuf) < 20)
        {
            ptr = strtok(NULL, "\n");
            continue;
        }

        pc = strchr(tmpbuf, '\r');
        if (pc)
        {
            *pc = '\0';
        }

        if (iIndex >= (max_entry_cnt - 1))
        {
            return p_cust_resEntrys;
        }

        pos = tmpbuf;
        pc = strchr(pos, ',');
        if (pc)
        {
            *pc = '\0';
            memset(p_cust_resEntrys[iIndex].res_name, 0, 16);
            StrCpy(p_cust_resEntrys[iIndex].res_name, 16, tmpbuf);
        }
        else
        {
            goto __failed;
        }

        pos = pc + 1;
        pc = strchr(pos, ',');
        if (pc)
        {
            *pc = '\0';
            memset(p_cust_resEntrys[iIndex].codec_name, 0, 16);
            StrCpy(p_cust_resEntrys[iIndex].res_name, 16, pos);
        }
        else
        {
            goto __failed;
        }

        int stream_type = 0;
        int def_bitrate = 0;
        int min_bitrate = 0;
        int max_bitrate = 0;
        int def_framerate = 0;
        int min_framerate = 0;
        int max_framerate = 0;
        int dual_stream = 0;
        int def_config = 0;
        int max_display_framerate = 0;

        iRet = sscanf(pc + 1, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &stream_type, &def_bitrate, &min_bitrate, &max_bitrate, &def_framerate, &min_framerate, &max_framerate, &dual_stream, &def_config, &max_display_framerate);
        if (iRet == 10)
        {
            p_cust_resEntrys[iIndex].stream_type = stream_type;
            p_cust_resEntrys[iIndex].def_bitrate = def_bitrate;
            p_cust_resEntrys[iIndex].min_bitrate = min_bitrate;
            p_cust_resEntrys[iIndex].max_bitrate = max_bitrate;
            p_cust_resEntrys[iIndex].def_framerate = def_framerate;
            p_cust_resEntrys[iIndex].min_framerate = min_framerate;
            p_cust_resEntrys[iIndex].max_framerate = max_framerate;
            p_cust_resEntrys[iIndex].dual_stream = dual_stream;
            p_cust_resEntrys[iIndex].def_config = def_config;
            p_cust_resEntrys[iIndex].max_display_framerate = max_display_framerate;
        }
        else
        {
            goto __failed;
        }

        iIndex++;
        ptr = strtok(NULL, "\n");
    }

    *resEntryLength = iIndex;
    return p_cust_resEntrys;

__failed:
    if (p_cust_resEntrys != NULL)
    {
        anj_mw_free(p_cust_resEntrys);
        p_cust_resEntrys = NULL;
    }

    return NULL;
}

static int video_res_is_in_array(const RESOLUTION_ENTRY *pUnit, const RESOLUTION_ENTRY *pEntry, int nMaxEntryNum)
{
    if (NULL == pUnit || NULL == pEntry)
    {
        return 0;
    }

    if (pUnit->codec_name == NULL || pUnit->res_name == NULL ||
        strlen(pUnit->codec_name) == 0 || strlen(pUnit->res_name) == 0)
    {
        return 0;
    }

    int iIndex = 0;
    for (iIndex = 0; iIndex < nMaxEntryNum; iIndex++)
    {
        if (pEntry[iIndex].codec_name == NULL || pEntry[iIndex].res_name == NULL ||
            strlen(pEntry[iIndex].codec_name) == 0 || strlen(pEntry[iIndex].res_name) == 0)
        {
            break;
        }

        if (strcasecmp(pEntry[iIndex].codec_name, pUnit->codec_name) == 0 &&
            strcasecmp(pEntry[iIndex].res_name, pUnit->res_name) == 0 &&
            pEntry[iIndex].stream_type == pUnit->stream_type)
        {
            return 1;
        }
    }

    return 0;
}

static void video_res_add_entry(RESOLUTION_ENTRY *pEntry, int nMaxNum, const RESOLUTION_ENTRY *pData)
{
    int iIndex = 0;
    if (NULL == pEntry || NULL == pData)
    {
        return;
    }

    for (iIndex = 0; iIndex < nMaxNum; iIndex++)
    {
        if (strlen(pEntry[iIndex].codec_name) == 0 || strlen(pEntry[iIndex].res_name) == 0)
        {
            break;
        }

        if (strcasecmp(pEntry[iIndex].codec_name, pData->codec_name) == 0 &&
            strcasecmp(pEntry[iIndex].res_name, pData->res_name) == 0 &&
            pEntry[iIndex].stream_type == pData->stream_type)
        {
            memcpy(&pEntry[iIndex], pData, sizeof(RESOLUTION_ENTRY));
            return;
        }
    }

    if (iIndex < nMaxNum && strlen(pEntry[iIndex].codec_name) == 0)
    {
        memcpy(&pEntry[iIndex], pData, sizeof(RESOLUTION_ENTRY));
    }
}

static int video_res_compare(const void *__e1, const void *__e2)
{
    const RESOLUTION_ENTRY *pItem1 = (const RESOLUTION_ENTRY *)__e1;
    const RESOLUTION_ENTRY *pItem2 = (const RESOLUTION_ENTRY *)__e2;

    if (pItem1->stream_type > pItem2->stream_type)
    {
        return 1;
    }
    else if (pItem1->stream_type == pItem2->stream_type)
    {
        if (strcasecmp(pItem1->codec_name, pItem2->codec_name) > 0)
        {
            return 1;
        }
        else if (strcasecmp(pItem1->codec_name, pItem2->codec_name) == 0)
        {
            int w1 = 0;
            int h1 = 0;
            int w2 = 0;
            int h2 = 0;

            GetVideoSize(pItem1->res_name, 0, &w1, &h1);
            GetVideoSize(pItem2->res_name, 0, &w2, &h2);

            if (w1 * h1 < w2 * h2)
            {
                return 1;
            }
            else if (w1 * h1 == w2 * h2)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

static void video_res_defalut_config(RESOLUTION_ENTRY *pEntry)
{
    int iIndex = 0;
    int bMainDefaultFound = 0;
    int bSubDefaultFound = 0;
    int mainDefaultIndex = -1;
    int subDefaultIndex = -1;

    if (NULL == pEntry)
    {
        return;
    }

    // 1. 查找现有默认配置并记录第一个出现的码流
    iIndex = 0;
    while (strlen(pEntry[iIndex].res_name) > 0)
    {
        if (pEntry[iIndex].stream_type == 0)
        {
            if (pEntry[iIndex].def_config > 0)
            {
                bMainDefaultFound = 1;
            }
            if (mainDefaultIndex == -1)
            {
                mainDefaultIndex = iIndex;
            }
        }
        else if (pEntry[iIndex].stream_type == 1)
        {
            if (pEntry[iIndex].def_config > 0)
            {
                bSubDefaultFound = 1;
            }
            if (subDefaultIndex == -1)
            {
                subDefaultIndex = iIndex;
            }
        }

        // 主子码流都存在默认配置时退出
        if (bMainDefaultFound == 1 && bSubDefaultFound == 1)
        {
            break;
        }

        iIndex++;
    }

    if (bMainDefaultFound == 0 && mainDefaultIndex != -1)
    {
        pEntry[mainDefaultIndex].def_config = 1;
    }

    if (bSubDefaultFound == 0 && subDefaultIndex != -1)
    {
        pEntry[subDefaultIndex].def_config = 1;
    }

    iIndex = 0;
    while (strlen(pEntry[iIndex].codec_name) > 0)
    {
        // 2. 设置码率对齐 1024或者32对齐
        int data = pEntry[iIndex].max_bitrate;
        if (data % 1000 == 0)
        {
            data = ANJ_ALIGN_UP(data, 1024);
        }
        else
        {
            data = ANJ_ALIGN_UP(data, 32);
        }

        pEntry[iIndex].max_bitrate = data;

        data = pEntry[iIndex].min_bitrate;
        if (data % 1000 == 0)
        {
            data = ANJ_ALIGN_UP(data, 1024);
        }
        else
        {
            data = ANJ_ALIGN_UP(data, 32);
        }

        pEntry[iIndex].min_bitrate = data;

        // 3. 设置最小帧率（默认为5）
        if (pEntry[iIndex].stream_type == 0 || pEntry[iIndex].stream_type == 1)
        {
            pEntry[iIndex].min_framerate = 5;
        }

        iIndex++;
    }
}

static RESOLUTION_ENTRY *video_res_entry_get(int bGetFakeVideoResolution)
{
    int iIndex = 0;
    int max_entry_cnt = 50;
    static RESOLUTION_ENTRY *s_VideoResEntry = NULL;

    if (NULL == s_VideoResEntry)
    {
        RESOLUTION_ENTRY *pEntryOrg = NULL;    // 默认分辨率数组
        RESOLUTION_ENTRY *pEntryDelete = NULL; // 用户定制需要删除的分辨率
        RESOLUTION_ENTRY *pEntryCust = NULL;   // 用户定制需要添加的分辨率

        RESOLUTION_ENTRY stDefaultResList[] = {RESOLUTION_LIST};
        pEntryOrg = stDefaultResList;

        int currentResLength = 0;
        int deleteResEntryLength = 0;
        int custResEntryLength = 0;
        unsigned int nUnitLen = 0;

        s_VideoResEntry = (RESOLUTION_ENTRY *)anj_mw_malloc(max_entry_cnt * sizeof(RESOLUTION_ENTRY));
        if (NULL == s_VideoResEntry)
        {
            __ERR("s_VideoResEntry malloc failed\n");
            return NULL;
        }
        memset(s_VideoResEntry, 0, max_entry_cnt * sizeof(RESOLUTION_ENTRY));

        // 获取需要删除的编码分辨率
        pEntryDelete = video_delete_res_get(&deleteResEntryLength);

        for (iIndex = 0; iIndex < max_entry_cnt; iIndex++)
        {
            if (pEntryOrg[iIndex].codec_name == NULL || pEntryOrg[iIndex].res_name == NULL ||
                strlen(pEntryOrg[iIndex].codec_name) == 0 || strlen(pEntryOrg[iIndex].res_name) == 0)
            {
                break;
            }

            // 删除指定的编码
            RESOLUTION_ENTRY data;
            memcpy(&data, &pEntryOrg[iIndex], sizeof(data));
            if (0 == video_res_is_in_array(&data, pEntryDelete, deleteResEntryLength))
            {
                memcpy(&s_VideoResEntry[currentResLength], &data, sizeof(RESOLUTION_ENTRY));
                currentResLength++;
            }
        }

        // 获取需要添加的编码分辨率
        pEntryCust = video_cust_res_get(&custResEntryLength);
        if (pEntryCust != NULL)
        {
            __ERR("video res cust entry len:%d\n", custResEntryLength);
            for (iIndex = 0; iIndex < custResEntryLength; iIndex++)
            {
                if (pEntryCust[iIndex].codec_name == NULL || pEntryCust[iIndex].res_name == NULL ||
                    strlen(pEntryCust[iIndex].codec_name) == 0 || strlen(pEntryCust[iIndex].res_name) == 0)
                {
                    break;
                }

                video_res_add_entry(s_VideoResEntry, max_entry_cnt, &pEntryCust[iIndex]);
            }
        }

        if (pEntryDelete != NULL)
        {
            anj_mw_free(pEntryDelete);
            pEntryDelete = NULL;
        }

        if (pEntryCust != NULL)
        {
            anj_mw_free(pEntryCust);
            pEntryCust = NULL;
        }

        // 重新计算条目
        for (iIndex = 0; iIndex < max_entry_cnt; iIndex++)
        {
            if (s_VideoResEntry[iIndex].codec_name == NULL || s_VideoResEntry[iIndex].res_name == NULL ||
                strlen(s_VideoResEntry[iIndex].codec_name) == 0 || strlen(s_VideoResEntry[iIndex].res_name) == 0)
            {
                break;
            }
        }
        currentResLength = iIndex;
        __WARN("video res real num:%d\n", currentResLength);

        nUnitLen = sizeof(RESOLUTION_ENTRY);
        qsort(s_VideoResEntry, currentResLength, nUnitLen, video_res_compare);

        video_res_defalut_config(s_VideoResEntry);
    }

    if (bGetFakeVideoResolution)
    {
        // InitFakeResolution(pEntry);
    }

    return s_VideoResEntry;
}

static int anj_sysmng_reboot_thread(void *ctx, int *bStart)
{
    int sec = *(int *)ctx;
    if (sec < 0 || sec > 30)
        sec = 10;
    anj_mw_free(ctx);
    sleep(sec);
    __WARN("call reboot to reboot system...\n");
    anj_sysmng_reboot();
    return 0;
}

int anj_sysmng_dev_str_get(char *szDeviceType)
{
    strncpy(gstDevInfo.devType, ANJ_PROJECT_NAME, sizeof(gstDevInfo.devType));
    strcpy(szDeviceType, ANJ_PROJECT_NAME);

    __INFO("Get Device Type: %s\n", szDeviceType);
    return 0;
}

int anj_sysmng_platform_type_get(char *szPlatformType, int bufLen)
{
    if (szPlatformType == NULL || bufLen <= 0)
        return -1;

    snprintf(szPlatformType, bufLen, "%s", SDK_KERNEL_IDENTITY);
    __INFO("platform type: %s\n", szPlatformType);
    snprintf(gstDevInfo.platformType, sizeof(gstDevInfo.platformType), "%s", szPlatformType);

    return 0;
}

int anj_sysmng_sn_validate(unsigned char *sn)
{
    // SN V1: 0xEF, SN V2: 0xFF
    if (sn[0] == 0xEF || sn[0] == 0xFF)
    {
        if (memcmp((void *)(sn + 8), "AJSOSN", 6) == 0)
        {
            return 0;
        }
    }

    char szDeviceType[32] = {0};

    if (anj_sysmng_dev_str_get(szDeviceType) < 0)
    {
        __ERR("Get device type failed.\n");
        return -1;
    }

    if (strncmp((char *)sn + 8, ANJ_CRYPT_NAME, 6) == 0 &&
        sn[14] == 0xFF && sn[15] == 0xFF)
    {
        return 0;
    }

    char tmpStr[8] = {0};
    char szString[128] = {0};
    for (int i = 8; i < 16; i++)
    {
        sprintf(tmpStr, "%02x ", sn[i]);
        strcat(szString, tmpStr);
    }

    sprintf(szString + strlen(szString), "\t");
    for (int i = 8; i < 14; i++)
    {
        if (isprint(sn[i]))
        {
            sprintf(szString + strlen(szString), "%c", sn[i]);
        }
    }

    __ERR("error sn device type: %s\n", szString);

    return -1;
}

int anj_sysmng_get_random_sn(unsigned char *buf)
{
    int iRet = 0;
    FILE *pFd = NULL;
    int bCreateRandom = 1;

    if (anj_mw_file_exists(ENCRIPT_DATA_FILE))
    {
        pFd = anj_mw_fopen(ENCRIPT_DATA_FILE, "rb");
        if (pFd < 0)
        {
            __ERR("open %s failed!\n", ENCRIPT_DATA_FILE);
        }
        else
        {
            iRet = anj_mw_fread(pFd, buf, 16);
            anj_mw_fclose(pFd);
            if (iRet < 0)
            {
                __ERR("read %s failed!\n", ENCRIPT_DATA_FILE);
                return -1;
            }

            if (buf[0] != 0)
            {
                bCreateRandom = 1;
            }
            else
            {
                for (int i = 0; i < 16; i++)
                    printf("%02x ", buf[i]);
                printf("\n");
                bCreateRandom = 0;
            }
        }
    }

    if (bCreateRandom)
    {
        // 根据序列号生成
        // 第一个字节需要为0
        struct timeval tv;
        SystemGetTimeofRun(&tv, NULL);
        int svalud = tv.tv_sec + tv.tv_usec;

        buf[0] = 0;
        srand(svalud);
        for (int iIndex = 1; iIndex < ENCRIPT_DATA_LEN; iIndex++)
        {
            buf[iIndex] = (rand() + iIndex) % 0x100;
        }

        sprintf((char *)(buf + ENCRIPT_DATA_LEN), ANJ_CRYPT_NAME);

        *(buf + 14) = 0xFF;
        *(buf + 15) = 0xFF;
        *(buf + 16) = 0;

        // 保存到文件，用于下次读取
        FILE *pFd = anj_mw_fopen(ENCRIPT_DATA_FILE, "wb");
        if (pFd == NULL)
        {
            __ERR("open %s failed!\n", ENCRIPT_DATA_FILE);
        }
        else
        {

            int writeret = anj_mw_fwrite(pFd, buf, 16);
            if (writeret < 0)
            {
                __ERR("save encript data to %s failed. iRet = %d.\n", ENCRIPT_DATA_FILE, writeret);
            }
            anj_mw_fclose(pFd);
        }

        for (int i = 0; i < 16; i++)
            printf("%02x ", buf[i]);
        printf("\n");
    }

    return 0;
}

int anj_sysmng_get_sn(unsigned char *buf, int buflen)
{
    int iRet = -1;
    iRet = ReadEncriptDataFromSoft_ex(buf, buflen);
    if (iRet == 0)
    {
        if (anj_sysmng_sn_validate(buf) < 0)
        {
            iRet = -1;
        }
        else
        {
            __INFO("Check soft SN OK!!!\n");
        }
    }

    if (iRet != 0)
    {
        __WARN("Get random encript data.\n");
        iRet = anj_sysmng_get_random_sn(buf);
    }

    return iRet;
}

int anj_sysmng_load_sn(char *sn_str, int str_len)
{
    if (sn_str == NULL || str_len == 0)
    {
        return -1;
    }

    int i = 0;
    char sn[256] = {0};
    char recv_buf[8] = {0};
    strcpy(sn_str, "");

    if (strlen(gstDevInfo.sn) > 0)
    {
        StrCpy(sn_str, str_len, gstDevInfo.sn);
        return 0;
    }

    if (anj_sysmng_get_sn((unsigned char *)sn, sizeof(sn)) < 0)
    {
        return -1;
    }
    else
    {
        for (i = 0; i < sizeof(recv_buf); i++)
        {
            sprintf(recv_buf, "%02X", sn[i]);
            strcat(sn_str, recv_buf);
        }

        StrCpy(gstDevInfo.sn, sizeof(gstDevInfo.sn), sn_str);
    }

    return 0;
}

int anj_sysmng_load_enc_sn(char *sn_str, int str_len)
{
    if (sn_str == NULL || str_len == 0)
    {
        return -1;
    }

    int i = 0;
    char sn[256] = {0};
    char recv_buf[8] = {0};
    strcpy(sn_str, "");

    if (anj_sysmng_get_sn((unsigned char *)sn, sizeof(sn)) < 0)
    {
        return -1;
    }
    else
    {
        for (i = 0; i < sizeof(recv_buf); i++)
        {
            sprintf(recv_buf, "%02X", sn[i]);
            strcat(sn_str, recv_buf);
        }

        StrCpy(gstDevInfo.sn, sizeof(gstDevInfo.sn), sn_str);
    }

    return 0;
}

int anj_sysmng_check_process(char *process_name)
{
    char cmd_str[128] = {0};
    char get_str[128] = {0};
    sprintf(cmd_str, "ps  | grep \"%s*\" | grep -v grep", process_name);

    FILE *fp = popen(cmd_str, "r");
    if (fp != NULL)
    {
        int flen = anj_mw_fread(fp, get_str, sizeof(get_str));
        if (flen > 0)
        {
            pclose(fp);
            return 0;
        }
        else
        {
            pclose(fp);
        }
    }
    else
    {
        __ERR("popen %s failed\n", cmd_str);
    }
    return -1;
}

void anj_sysmng_reboot()
{
    if (gstDevInfo.bUpgrading)
    {
        __WARN("skip reboot while firmware upgrading\n");
        return;
    }

    anj_record_uninit();
    __WARN("system_reboot\n");

    // system("reboot");
    sync();
    sleep(1);
    WatchDogSetTimeOut(1, 1);
    reboot(RB_AUTOBOOT);
}

void anj_sysmng_delay_reboot(int sec)
{
    __WARN("schedule system reboot after %d sec\n", sec);
    int *arg = (int *)anj_mw_malloc(sizeof(int));
    *arg = sec;
    static anj_thread_s thd = {0};
    thd.bAutoDestroy = 1;
    strncpy(thd.iThreadName, "reboot_thread", sizeof(thd.iThreadName) - 1);
    thd.iThreadjob.ctx = arg;
    thd.iThreadjob.func = anj_sysmng_reboot_thread;
    if (0 != anj_thread_task_create(&thd))
    {
        __ERR("file_transport_thread create failed\n");
        anj_mw_free(arg);
    }
}

/**
 * @brief parseFsVersion 从系统信息fs中解析出deviceType与version
 * @param fsver          SYSTEM_VERSION_DATA.fsVersion
 * @param deviceType     [OUT] e.g. MTE6_V0_BU-H5
 * @param deviceTypeLen
 * @param version        [OUT] e.g. V3.0.2.1
 * @param versionLen
 * @param date           [OUT] e.g. 2025-09-11 11:43:58
 * @param dateLen
 * @return
 */
int anj_sysmng_parse_fsversion(char *fsver, char *deviceType, int deviceTypeLen,
                               char *version, int versionLen, char *date, int dateLen)
{
    const char *flag1 = " V";
    const char *flag2 = " build ";
    char *p1 = strstr(fsver, flag1);
    char *p2 = strstr(fsver, flag2);

    if (p1 != NULL && p2 != NULL)
    {
        strcpy(date, p2 + strlen(flag2));
        string_trim_head(date);
        string_trim_tail(date);

        int strLen = p1 - fsver;
        if (strLen < deviceTypeLen)
        {
            strncpy(deviceType, fsver, strLen);
        }

        char *pVerStart = p1 + strlen(flag1) - 1;
        char *pVerEnd = strstr(pVerStart, " ");
        strLen = pVerEnd - pVerStart;
        if (strLen < versionLen)
        {
            strncpy(version, pVerStart, strLen);
        }

        return 0;
    }
    return -1;
}

int anj_sysmng_version_info_get(SYSTEM_VERSION_DATA *pVersionInfo, int bGetRealVersion)
{
    int iRet = 0;
    FILE *fp = NULL;
    AjOemStruct *p_oemInfo = NULL;
    AjOemStruct *p_oemInfo_factory = NULL;
    ANJ_CHK((pVersionInfo != NULL), -1, "input Invalid");

    int kernelLen = 256;
    int fsLen = 256;
    int len = 0;
    anj_mw_system("uname -srmv > /tmp/linux.ver");
    fp = anj_mw_fopen("/tmp/linux.ver", "r");
    ANJ_CHK((fp != NULL), -1, "anj_mw_fopen failed!");

    len = anj_mw_fread(fp, pVersionInfo->kernelVersion, kernelLen);
    anj_mw_fclose(fp);

    if (len > 0)
    {
        len = strlen(pVersionInfo->kernelVersion);

        if (pVersionInfo->kernelVersion[len - 1] == '\n')
            pVersionInfo->kernelVersion[len - 1] = 0;
    }

    p_oemInfo = (AjOemStruct *)anj_mw_malloc(sizeof(AjOemStruct));
    if (p_oemInfo)
    {
        memset(p_oemInfo, 0, sizeof(AjOemStruct));
        anj_config_oem_get(p_oemInfo);
    }

    p_oemInfo_factory = (AjOemStruct *)anj_mw_malloc(sizeof(AjOemStruct));
    if (p_oemInfo_factory)
    {
        memset(p_oemInfo_factory, 0, sizeof(AjOemStruct));
        anj_config_oem_factory_get(p_oemInfo_factory);
    }

    char szDeviceType[32] = {0};
    if (!bGetRealVersion && NULL != p_oemInfo && strlen(p_oemInfo->szDeviceType) > 0)
    {
        strcpy(szDeviceType, p_oemInfo->szDeviceType);
    }
    else
    {
        if (anj_mw_file_exists(OEM_SECOND_CONFIG_PATH) && bGetRealVersion == 0)
        {
            SECOND_DEFAULTCONFIG_DATA myconfig = {0};
            anj_config_oem_second_load(&myconfig);
            if (strlen(myconfig.device_name) > 0)
            {
                strcpy(szDeviceType, myconfig.device_name);
            }
            else if (anj_sysmng_dev_str_get(szDeviceType) < 0)
            {
                __ERR("default_2_priority.xml device_name null!\n");
                iRet = -1;
                goto endFunc;
            }
        }
        else if (NULL != p_oemInfo_factory && strlen(p_oemInfo_factory->szDeviceType) > 0)
        {
            strcpy(szDeviceType, p_oemInfo_factory->szDeviceType);
        }
        else if (anj_sysmng_dev_str_get(szDeviceType) < 0)
        {
            __ERR("Unknown device type.\n");
            iRet = -1;
            goto endFunc;
        }
    }

    int namelen = 0;
    snprintf(pVersionInfo->fsVersion, sizeof(pVersionInfo->fsVersion), "%s", szDeviceType);

    namelen = strlen(pVersionInfo->fsVersion);
    if (bGetRealVersion || NULL == p_oemInfo || strlen(p_oemInfo->szOemHWVersion) == 0)
    {
        if (NULL == p_oemInfo_factory || strlen(p_oemInfo_factory->szOemHWVersion) == 0)
        {
            sprintf(pVersionInfo->fsVersion + namelen, "_V%s", anj_sysmng_product_version_get());
        }
        else
        {
            sprintf(pVersionInfo->fsVersion + namelen, "_V%s", p_oemInfo_factory->szOemHWVersion);
        }
    }
    else
    {
        sprintf(pVersionInfo->fsVersion + namelen, "_V%s", p_oemInfo->szOemHWVersion);
    }

    if (SupportBootAutoUpdate() > 0)
    {
        namelen = strlen(pVersionInfo->fsVersion);
        sprintf(pVersionInfo->fsVersion + namelen, "_BU");
    }

    if (IPC_NETWORK_TYPE == NET_DEV_TYPE_WIRE_4G || IPC_NETWORK_TYPE == NET_DEV_TYPE_4G)
    {
        namelen = strlen(pVersionInfo->fsVersion);
        sprintf(pVersionInfo->fsVersion + namelen, "_4G");
    }

    namelen = strlen(pVersionInfo->fsVersion);
    if (!bGetRealVersion && NULL != p_oemInfo && strlen(p_oemInfo->szVersion) > 0 && strlen(p_oemInfo->szBuildtime) > 0)
    {
        sprintf(pVersionInfo->fsVersion + namelen, " V%s build %s", p_oemInfo->szVersion, p_oemInfo->szBuildtime);
    }
    else
    {
        if (NULL != p_oemInfo_factory && strlen(p_oemInfo_factory->szVersion) > 0 && strlen(p_oemInfo_factory->szBuildtime) > 0)
        {
            sprintf(pVersionInfo->fsVersion + namelen, " V%s build %s", p_oemInfo_factory->szVersion, p_oemInfo_factory->szBuildtime);
        }
        else
        {
            fp = anj_mw_fopen("/etc/filesys.ver", "r");
            ANJ_CHK((fp != NULL), -1, "anj_mw_fopen failed!");
            char fsVersion[256] = {0};
            len = anj_mw_fread(fp, fsVersion, sizeof(fsVersion));
            ANJ_CHK((len > 0), -1, "anj_mw_fread failed!");

            snprintf(pVersionInfo->fsVersion + namelen, fsLen - namelen, "%s", fsVersion);
            len = strlen(fsVersion);

            if (pVersionInfo->fsVersion[namelen + len - 1] == '\n')
                pVersionInfo->fsVersion[namelen + len - 1] = 0;
        }
    }

endFunc:
    __INFO("%s, %s\n", pVersionInfo->fsVersion, pVersionInfo->kernelVersion);
    if (p_oemInfo)
    {
        anj_mw_free(p_oemInfo);
    }
    if (p_oemInfo_factory)
    {
        anj_mw_free(p_oemInfo_factory);
    }
    if (fp)
    {
        anj_mw_fclose(fp);
    }
    return iRet;
}

char *anj_sysmng_product_version_get()
{
    if (strlen(gstDevInfo.productVersion) > 0)
    {
        return gstDevInfo.productVersion;
    }

    FILE *fp = NULL;
    fp = anj_mw_fopen("/etc/product.ver", "r");
    if (fp != NULL)
    {
        anj_mw_fread(fp, gstDevInfo.productVersion, sizeof(gstDevInfo.productVersion));
        anj_mw_fclose(fp);

        int length = strlen(gstDevInfo.productVersion);

        if (length > 15)
            gstDevInfo.productVersion[0] = 0;

        length = strlen(gstDevInfo.productVersion);
        if (length > 0 && gstDevInfo.productVersion[length - 1] == '\n')
            gstDevInfo.productVersion[length - 1] = 0;
    }
    else
    {
        gstDevInfo.productVersion[0] = '0';
    }

    return gstDevInfo.productVersion;
}

int anj_sysmng_search_device_get()
{
    gstDevInfo.oem_sn[0] = 0;
    char szDeviceType[64] = {0};
    AjOemStruct oemInfo = {0};
    AjOemStruct *p_oemInfo = &oemInfo;

    anj_config_oem_get(p_oemInfo);

    if (strlen(oemInfo.szOemSN) > 0)
    {
        snprintf(gstDevInfo.oem_sn, sizeof(gstDevInfo.oem_sn), "%s", oemInfo.szOemSN);
    }

    if (strlen(p_oemInfo->szDeviceType) == 0)
    {
        if (anj_mw_file_exists(OEM_SECOND_CONFIG_PATH))
        {
            SECOND_DEFAULTCONFIG_DATA *myconfig = anj_mw_malloc(sizeof(SECOND_DEFAULTCONFIG_DATA));
            memset(myconfig, 0, sizeof(SECOND_DEFAULTCONFIG_DATA));
            anj_config_oem_second_load(myconfig);
            if (strlen(myconfig->device_name) > 0)
            {
                strcpy(szDeviceType, myconfig->device_name);
            }
            else if (anj_sysmng_dev_str_get(szDeviceType) < 0)
            {
                __ERR("default_2_priority.xml device_name null!\n");
                strcpy(szDeviceType, "HD");
            }
            free(myconfig);
        }
        else if (anj_sysmng_dev_str_get(szDeviceType) < 0)
        {
            __ERR("Unknown device type. set to HD\n");
            strcpy(szDeviceType, "HD");
        }
    }
    else
    {
        strcpy(szDeviceType, p_oemInfo->szDeviceType);
    }

    strcat(szDeviceType, "_V");

    if (strlen(p_oemInfo->szOemHWVersion) == 0)
    {
        strcat(szDeviceType, anj_sysmng_product_version_get());
    }
    else
    {
        strcat(szDeviceType, p_oemInfo->szOemHWVersion);
    }

    strcpy(gstDevInfo.search_devicetype, szDeviceType);

    __INFO("gstDevInfo.search_devicetype:%s\n", gstDevInfo.search_devicetype);

    return 0;
}

void anj_sysmng_mac_restore(GlobalConfig *cfg)
{
    // 默认配置中的非法MAC：需要设置为SN的MAC或者OEM MAC
    // 初始MAC:需要判断有没有OEM MAC

    // 初始MAC
    char szInitMac[32] = {0};
    unsigned char macaddr[6] = {0};
    anj_net_mac_create_by_sn(macaddr);
    format_mac_addr_from_digit_to_string((char *)macaddr, 6, (char *)szInitMac, MAC_ADDRESS_LEN);
    __ERR("init macaddr=%s\n", szInitMac);

    // OEM MAC
    AjOemStruct oemInfo = {0};
    anj_config_oem_get(&oemInfo);
    if (strlen(oemInfo.szOemEthMac) > 0 && is_mac_addr_valid(oemInfo.szOemEthMac))
    {
        __ERR("MAC in OEM: %s\n", oemInfo.szOemEthMac);
    }
    else
    {
        oemInfo.szOemEthMac[0] = 0;
    }

    LANConfig *p = &cfg->networkCfgNew.lanCfg;
    if (!is_mac_addr_valid((char *)p->MACAddress)) // MAC无效，或者为初始MAC，设置为OEM或者初始MAC
    {
        char szSetMac[32] = {0};
        if (strlen(oemInfo.szOemEthMac) > 0)
            strcpy(szSetMac, oemInfo.szOemEthMac);
        else
            strcpy(szSetMac, szInitMac);

        strcpy((char *)p->MACAddress, szSetMac);

        __ERR("Reset mac address to: %s\n", (char *)cfg->networkCfgNew.lanCfg.MACAddress);
        anj_config_save_all(cfg, CONFIG_FILE_PATH);
    }
    else
    {
        if (strcasecmp((char *)p->MACAddress, szInitMac) == 0)
        {
            if (strlen(oemInfo.szOemEthMac) > 0)
            {
                strcpy((char *)p->MACAddress, oemInfo.szOemEthMac);
                __ERR("Reset mac address to: %s\n", (char *)cfg->networkCfgNew.lanCfg.MACAddress);
                anj_config_save_all(cfg, CONFIG_FILE_PATH);
            }
        }
    }
}

static void anj_sysmng_second_overlay_xy(const char *xy, Positiontype *posType, int *posX, int *posY)
{
    if (xy == NULL || xy[0] == 0)
    {
        return;
    }

    if (strcmp(xy, "00") == 0)
    {
        *posType = POSITION_TYPE_BY_FOUR_CORNER;
        *posX = 0;
        *posY = 0;
    }
    else if (strcmp(xy, "01") == 0)
    {
        *posType = POSITION_TYPE_BY_FOUR_CORNER;
        *posX = 0;
        *posY = 1;
    }
    else if (strcmp(xy, "10") == 0)
    {
        *posType = POSITION_TYPE_BY_FOUR_CORNER;
        *posX = 1;
        *posY = 0;
    }
    else if (strcmp(xy, "11") == 0)
    {
        *posType = POSITION_TYPE_BY_FOUR_CORNER;
        *posX = 1;
        *posY = 1;
    }
}

static void anj_sysmng_second_encode_set(char *name, int namelen, int encode)
{
    if (name == NULL || namelen <= 0)
    {
        return;
    }

    memset(name, 0, namelen);
    if (encode == 0)
    {
        strncpy(name, "H265+", namelen - 1);
    }
    else if (encode == 1)
    {
        strncpy(name, "H265", namelen - 1);
    }
    else if (encode == 2)
    {
        strncpy(name, "H264", namelen - 1);
    }
    else if (encode == 3)
    {
        strncpy(name, "MJPEG", namelen - 1);
    }
}

static void anj_sysmng_second_stream_res(GlobalConfig *cfg, int idx, const char *res)
{
    if (cfg == NULL || idx < 0 || idx >= MAX_VENC_CHN)
    {
        return;
    }
    if (res == NULL || res[0] == 0)
    {
        return;
    }

    char *dst = cfg->mediaCfg.videoConfig[0].videoEncode.encodeCfg[idx].resolution.name;
    int n = RESOLUTION_NAME_MAX_LEN - 1;
    int srcLen = (int)strlen(res);

    if (srcLen < n)
    {
        n = srcLen;
    }
    memset(dst, 0, RESOLUTION_NAME_MAX_LEN);
    memcpy(dst, res, n);
}

static void anj_sysmng_second_fake_video_update(SECOND_DEFAULTCONFIG_DATA *myconfig)
{
    if (strlen(myconfig->resolution_value_0_fake) > 0 ||
        strlen(myconfig->resolution_value_1_fake) > 0 ||
        strlen(myconfig->resolution_value_2_fake) > 0)
    {
        char buf[256] = {0};
        int used = 0;

        remove("/mnt/nand/fakevideo.xml");
        remove("/mnt/nand/Myfakevideo_flag");
        anj_mw_write_file("/mnt/nand/Myfakevideo_flag", 0, "1", 1);

        used = snprintf(buf, sizeof(buf), "<FAKE_VIDEO ");
        if (strlen(myconfig->resolution_value_0_fake) > 0 && used > 0 && used < (int)sizeof(buf))
        {
            used += snprintf(buf + used, sizeof(buf) - used, "V0=\"%s\" ", myconfig->resolution_value_0_fake);
        }
        if (strlen(myconfig->resolution_value_1_fake) > 0 && used > 0 && used < (int)sizeof(buf))
        {
            used += snprintf(buf + used, sizeof(buf) - used, "V1=\"%s\" ", myconfig->resolution_value_1_fake);
        }
        if (strlen(myconfig->resolution_value_2_fake) > 0 && used > 0 && used < (int)sizeof(buf))
        {
            used += snprintf(buf + used, sizeof(buf) - used, "V2=\"%s\" ", myconfig->resolution_value_2_fake);
        }
        if (used > 0 && used < (int)sizeof(buf))
        {
            snprintf(buf + used, sizeof(buf) - used, ">\r\n</FAKE_VIDEO>");
        }
        anj_mw_write_file("/mnt/nand/fakevideo.xml", 0, buf, (int)strlen(buf));
    }
    else
    {
        remove("/mnt/nand/fakevideo.xml");
        remove("/mnt/nand/Myfakevideo_flag");
    }
}

int anj_sysmng_second_config_copy(const char *srcFile)
{
    char szDir[64] = {0};

    if (srcFile == NULL)
    {
        return -1;
    }

    snprintf(szDir, sizeof(szDir), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);
    mkdir(szDir, 0777);
    anj_mw_write_file(OEM_SECOND_INIT_FLAG_PATH, 0, "1", 1);
    anj_mw_write_file(OEM_SECOND_SAVE_FLAG_PATH, 0, "1", 1);
    return anj_mw_file_copy(srcFile, OEM_SECOND_CONFIG_PATH);
}

void anj_sysmng_second_config_apply(GlobalConfig *cfg)
{
    SECOND_DEFAULTCONFIG_DATA myconfig = {0};
    VideoConfig *pstVideo = NULL;
    VideoCaptureCfg *pstCap = NULL;

    if (cfg == NULL || anj_config_oem_second_gate_a() == 0)
    {
        return;
    }

    if (anj_config_oem_second_load(&myconfig) != 0)
    {
        __ERR("load default_2_priority.xml fail\n");
        return;
    }

    pstVideo = &cfg->mediaCfg.videoConfig[0];
    pstCap = &pstVideo->videoCapture;

    anj_sysmng_second_overlay_xy(myconfig.title_xy,
                                 &pstVideo->overlay.titleOverlay.posType,
                                 &pstVideo->overlay.titleOverlay.posX,
                                 &pstVideo->overlay.titleOverlay.posY);
    anj_sysmng_second_overlay_xy(myconfig.time_xy,
                                 &pstVideo->overlay.timeOverlay.posType,
                                 &pstVideo->overlay.timeOverlay.posX,
                                 &pstVideo->overlay.timeOverlay.posY);

    if (strlen(myconfig.lan) > 0)
    {
        strncpy(cfg->systemCfg.miscCfg.language, myconfig.lan, MAX_LANGUAGE_LEN - 1);
        cfg->systemCfg.miscCfg.language[MAX_LANGUAGE_LEN - 1] = 0;
    }
    if (strlen(myconfig.password) > 0)
    {
        strncpy(cfg->systemCfg.userCfg.accounts[0].password, myconfig.password, ACCOUNT_PASSWORD_MAX_LEN - 1);
        cfg->systemCfg.userCfg.accounts[0].password[ACCOUNT_PASSWORD_MAX_LEN - 1] = 0;
    }
    if (strlen(myconfig.title) > 0)
    {
        pstVideo->overlay.titleOverlay.titleType = TYPE_TYPE_BY_TEXT;
        strncpy(pstVideo->overlay.titleOverlay.title_utf8, myconfig.title, TITLE_MAX_LEN - 1);
        pstVideo->overlay.titleOverlay.title_utf8[TITLE_MAX_LEN - 1] = 0;
    }
    if (strlen(myconfig.device_name) > 0)
    {
        anj_sysmng_version_info_get(&gstDevInfo.stVersionInfo, 0);
        anj_sysmng_search_device_get();
    }

    if (myconfig.sen_c_li != 0xffff)
    {
        pstCap->light_off_sensitivity = (unsigned char)myconfig.sen_c_li;
    }
    if (myconfig.sen_o_li != 0xffff)
    {
        pstCap->ircut_openled_delay = (unsigned char)myconfig.sen_o_li;
    }
    if (myconfig.audio_in != 0xffff)
    {
        cfg->mediaCfg.audioConfig.audioCapture.volume_capture = (short)myconfig.audio_in;
    }
    if (myconfig.audio_out != 0xffff)
    {
        cfg->mediaCfg.audioConfig.audioCapture.volume_play = (short)myconfig.audio_out;
    }
    if (myconfig.li_pw != 0xffff)
    {
        pstCap->led_brightness_value = (unsigned char)myconfig.li_pw;
    }
    if (myconfig.brightness != 0xffff)
    {
        pstCap->brightness = myconfig.brightness;
    }
    if (myconfig.saturation != 0xffff)
    {
        pstCap->saturation = myconfig.saturation;
    }
    if (myconfig.sharpness != 0xffff)
    {
        pstCap->sharpness = myconfig.sharpness;
    }
    if (myconfig.contrast != 0xffff)
    {
        pstCap->contrast = myconfig.contrast;
    }
    if (myconfig.led != 0xffff)
    {
        pstCap->led_mode = (LedMode)myconfig.led;
    }

    anj_sysmng_second_stream_res(cfg, 0, myconfig.resolution_value_0);
    anj_sysmng_second_stream_res(cfg, 1, myconfig.resolution_value_1);
    anj_sysmng_second_stream_res(cfg, 2, myconfig.resolution_value_2);

    if (myconfig.aov_workmode != 0xffff)
    {
        if (myconfig.aov_workmode == 0)
        {
            pstCap->aov_mode = 2;
            pstCap->aov_fps = 5;
        }
        else if (myconfig.aov_workmode == 1)
        {
            pstCap->aov_mode = 2;
            pstCap->aov_fps = 2;
        }
        else if (myconfig.aov_workmode == 2)
        {
            pstCap->aov_mode = 2;
            pstCap->aov_fps = 1;
        }
        else if (myconfig.aov_workmode == 3)
        {
            pstCap->aov_mode = 1;
            pstCap->aov_fps = 1;
        }
        else if (myconfig.aov_workmode == 4)
        {
            pstCap->aov_mode = 0;
            pstCap->aov_fps = 1;
        }
    }

    anj_sysmng_second_fake_video_update(&myconfig);

    if (myconfig.encoder_0 != 0xffff && MAX_VENC_CHN > 0)
    {
        anj_sysmng_second_encode_set(pstVideo->videoEncode.encodeCfg[0].encodeFormat.name,
                                     VIDEO_ENCODE_FORAMT_MAX_LEN, myconfig.encoder_0);
    }
    if (myconfig.encoder_1 != 0xffff && MAX_VENC_CHN > 1)
    {
        anj_sysmng_second_encode_set(pstVideo->videoEncode.encodeCfg[1].encodeFormat.name,
                                     VIDEO_ENCODE_FORAMT_MAX_LEN, myconfig.encoder_1);
    }
    if (myconfig.encoder_2 != 0xffff && MAX_VENC_CHN > 2)
    {
        anj_sysmng_second_encode_set(pstVideo->videoEncode.encodeCfg[2].encodeFormat.name,
                                     VIDEO_ENCODE_FORAMT_MAX_LEN, myconfig.encoder_2);
    }
    if (myconfig.tvsystem != 0xffff)
    {
        pstCap->tvsystem = (unsigned char)myconfig.tvsystem;
    }
    if (myconfig.low_li_pw != 0xffff)
    {
        pstCap->led_brightness_value = (unsigned char)myconfig.low_li_pw;
    }
    if (myconfig.alarm_audio_switch != 0xffff)
    {
        ArmingMode audioFlag = ARMING_DISABLE;
        if (myconfig.alarm_audio_switch == 1)
        {
            audioFlag = ARMING_ALLDAY;
        }
        cfg->alarmCfg.aiAlarm.pdAlarm[0].alarmAction.audioAction.enable.enable_flag = audioFlag;
        cfg->alarmCfg.normalAlarm.motionDetectAlarm[0].alarmAction.audioAction.enable.enable_flag = audioFlag;
    }

    remove(OEM_SECOND_INIT_FLAG_PATH);
    remove(OEM_SECOND_SAVE_FLAG_PATH);
    remove(OEM_SECOND_RESTORE_FLAG_PATH);
    __ERR("InitSecondConfig applied\n");
}

void anj_sysmng_second_config_led_apply(void)
{
    SECOND_DEFAULTCONFIG_DATA myconfig = {0};
    GlobalConfig *cfg = NULL;
    LedMode newMode;
    int needSave = 0;

    if (anj_config_oem_second_gate_b() == 0)
    {
        return;
    }
    if (anj_config_oem_second_load(&myconfig) != 0)
    {
        return;
    }
    if (myconfig.led_type == 0xffff)
    {
        return;
    }

    cfg = (GlobalConfig *)getGlbConfig();
    if (cfg == NULL)
    {
        return;
    }

    if (myconfig.led_type == 1)
    {
        newMode = (LedMode)0;
        if (cfg->mediaCfg.videoConfig[0].videoCapture.led_mode != newMode)
        {
            cfg->mediaCfg.videoConfig[0].videoCapture.led_mode = newMode;
            needSave = 1;
        }
    }
    else if (myconfig.led_type == 2)
    {
        newMode = (LedMode)1;
        if (cfg->mediaCfg.videoConfig[0].videoCapture.led_mode != newMode)
        {
            cfg->mediaCfg.videoConfig[0].videoCapture.led_mode = newMode;
            needSave = 1;
        }
    }

    if (needSave)
    {
        anj_config_save_all(cfg, CONFIG_FILE_PATH);
    }
}

void anj_sysmng_second_config_capability_apply(void)
{
    SECOND_DEFAULTCONFIG_DATA myconfig = {0};

    if (anj_config_oem_second_gate_b() == 0)
    {
        return;
    }
    if (anj_config_oem_second_load(&myconfig) != 0)
    {
        return;
    }

    if (myconfig.ptz_yuntai == 1)
    {
        anj_sysctl_capability_add(FUNCTION_PTZ_CONTROL);
    }
    else if (myconfig.ptz_yuntai == 0)
    {
        anj_sysctl_capability_remove(FUNCTION_PTZ_CONTROL);
    }

    if (myconfig.ptz_zoom == 1)
    {
        anj_sysctl_capability_add(FUNCTION_PTZ_ZOOM);
    }
    else if (myconfig.ptz_zoom == 0)
    {
        anj_sysctl_capability_remove(FUNCTION_PTZ_ZOOM);
    }

    if (myconfig.ptz_af == 1)
    {
        anj_sysctl_capability_add(FUNCTION_PTZ_FOCUS);
        anj_sysctl_capability_add(FUNCTION_PTZ_IRIS);
    }
    else if (myconfig.ptz_af == 0)
    {
        anj_sysctl_capability_remove(FUNCTION_PTZ_FOCUS);
        anj_sysctl_capability_remove(FUNCTION_PTZ_IRIS);
    }

    if (myconfig.cover == 1 || myconfig.cover == 0)
    {
        anj_sysctl_capability_add(FUNCTION_PRIVACY_PROTECTION);
    }
    else if (myconfig.cover == -1)
    {
        anj_sysctl_capability_remove(FUNCTION_PRIVACY_PROTECTION);
    }

    if (myconfig.low_pw == 1)
    {
        anj_sysctl_capability_add(FUNCTION_LOW_POWER);
    }
    else if (myconfig.low_pw == 0)
    {
        anj_sysctl_capability_remove(FUNCTION_LOW_POWER);
    }

    if (myconfig.ptz_track == 1)
    {
        anj_sysctl_capability_add(FUNCTION_PD_TRACK_HUMAN);
    }
    else if (myconfig.ptz_track == 0)
    {
        anj_sysctl_capability_remove(FUNCTION_PD_TRACK_HUMAN);
    }

    if (myconfig.led_type != 0xffff)
    {
        if (myconfig.led_type == 1)
        {
            anj_sysctl_capability_add(FUNCTION_LEDPANEL_IR);
            anj_sysctl_capability_remove(FUNCTION_LEDPANEL_WHITE);
            anj_sysctl_capability_remove(FUNCTION_LEDPANEL_DOUBLE);
        }
        else if (myconfig.led_type == 2)
        {
            anj_sysctl_capability_add(FUNCTION_LEDPANEL_WHITE);
            anj_sysctl_capability_remove(FUNCTION_LEDPANEL_IR);
            anj_sysctl_capability_remove(FUNCTION_LEDPANEL_DOUBLE);
        }
        else if (myconfig.led_type == 3)
        {
            anj_sysctl_capability_add(FUNCTION_LEDPANEL_DOUBLE);
            anj_sysctl_capability_remove(FUNCTION_LEDPANEL_IR);
            anj_sysctl_capability_remove(FUNCTION_LEDPANEL_WHITE);
        }
    }

    if (myconfig.aov_workmode == 4)
    {
        anj_sysctl_capability_remove(FUNCTION_AOV_SUPPORT);
    }

    __ERR("second config capability applied\n");
}

static int anj_sysmng_user_config_update(char *filePath)
{
    if (anj_mw_file_copy(filePath, CONFIG_FILE_PATH) != 0)
    {
        remove(filePath);
        return -1;
    }

    remove(filePath);

    GlobalConfig *pstGlbConfig = (GlobalConfig *)getGlbConfig();
    LANConfig stLANConfig = pstGlbConfig->networkCfgNew.lanCfg;
    if (anj_config_load(NULL, pstGlbConfig, CONFIG_FILE_PATH) != 0)
    {
        __ERR("get config faill\r\n");
        return -1;
    }
    // 保留之前的网络参数
    pstGlbConfig->networkCfgNew.lanCfg = stLANConfig;
    anj_config_network_save(&pstGlbConfig->networkCfgNew);

    anj_sysmng_mac_restore(pstGlbConfig);

    // 同步config.xml与dzoom.config.xml
    if (!anj_config_zoom_exist(CONFIG_FILE_PATH)) // 不存在，则从之前的配置中装载
    {
        double fSetValue = anj_config_zoom_multile_get_by_xml();
        if (DOUBLE_GREATER(fSetValue, 0.0))
        {
            pstGlbConfig->systemCfg.ptzCfg.dzoomCfg.multiple_set = fSetValue;
            anj_config_save_all(pstGlbConfig, CONFIG_FILE_PATH); // 保存以前的变倍配置到XML
        }
    }
    else
    {
        anj_config_zoom_multile_save(pstGlbConfig->systemCfg.ptzCfg.dzoomCfg.multiple_set);
    }

    return 0;
}

int anj_sysmng_config_update(char *filePath)
{
    __ERR("update file path = %s\r\n", filePath);

    int ret = 0;

    char buf[512] = {0};
    anj_mw_read_file_limit_len(filePath, buf, sizeof(buf) - 1);

    if (strlen(buf) > 0)
    {
        if (strstr(buf, "<ENCRYPT>") && strstr(buf, "SYSTEM_ENCRYPTSN_MESSAGE"))
        {
            char cameraid[128] = {0};
            char sndata[256] = {0};
            char checksum[64] = {0};
            int sn_version = 0;
            if (0 != soft_enc_xml_sn_data_parse(buf, cameraid, sizeof(cameraid), sndata, sizeof(sndata), checksum, sizeof(checksum), &sn_version))
            {
                __ERR("CANNOT GET SN\n");
                ret = -1;
            }
            else
            {
                ret = 0;
            }

            if (ret == 0)
            {
                if (strcasecmp(gstDevInfo.uuid, cameraid) != 0)
                {
                    __ERR("UUID is not correct!\n");
                    ret = -1;
                }
            }

            if (ret == 0)
            {
                char buffer[512] = {0};
                snprintf(buffer, sizeof(buffer), "%s%s%s", gstDevInfo.uuid, sndata, "zzcc@anjvision.com");

                char md5String[64] = {0};
                our_md5_encode(md5String, (unsigned char *)buffer, strlen(buffer));
                if (strcmp(md5String, checksum) != 0)
                {
                    __ERR("checksum %s is not correct, should be %s!\n", checksum, md5String);
                    __ERR("buffer: %s\n", buffer);
                    ret = -1;
                }
            }

            if (ret == 0)
            {
                if (WriteEncriptDataToSoft((unsigned char *)sndata, strlen(sndata), sn_version) < 0)
                    __ERR("WriteEncriptDataToSoft failed\n");
                else
                {
                    // 软加密OK的时候，根据SN生成MAC地址
                    char szInitMac[32] = {0};
                    unsigned char macaddr[6];
                    anj_net_mac_create_by_sn(macaddr);
                    format_mac_addr_from_digit_to_string((char *)macaddr, 6, (char *)szInitMac, MAC_ADDRESS_LEN);

                    __ERR("Set init mac: %s\n", (char *)szInitMac);
                    GlobalConfig *pstGlbConfig = (GlobalConfig *)getGlbConfig();

                    strcpy((char *)pstGlbConfig->networkCfgNew.lanCfg.MACAddress, (char *)szInitMac);
                    anj_config_network_save(&pstGlbConfig->networkCfgNew);

                    __WARN("set init mac, reboot\n");
                    __RECORD_LOG_INFO("set init mac, reboot\n");
                    anj_sysmng_delay_reboot(5);
                }
            }

            return 0;
        }
        else if (NULL != strstr(buf, "<ONVIF_CONFIG>") && NULL != strstr(buf, "</ONVIF_CONFIG>"))
        {
            char szOemXMLFileName[64] = {0};
            snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);
            mkdir(szOemXMLFileName, 0777);
            memset(szOemXMLFileName, 0, sizeof(szOemXMLFileName));
            snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s/%s",
                     DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_ONVIF_OEM_FILE_NAME);
            rename(filePath, szOemXMLFileName);

            module_uninit_single("anj_onvif");
            usleep(10 * 1000);
            module_init_single("anj_onvif");
            return 0;
        }
        else if (NULL != strstr(buf, "<CLEAR_ONVIF_CONFIG>"))
        {
            __ERR("GET ONVIF_CONFIG: %s\n", buf);
            char szOemXMLFileName[64] = {0};
            snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_ONVIF_OEM_FILE_NAME);
            remove(szOemXMLFileName);

            module_uninit_single("anj_onvif");
            usleep(10 * 1000);
            module_init_single("anj_onvif");
            return 0;
        }
        else if (NULL != strstr(buf, "<FAKE_VIDEO"))
        {
            __ERR("GET FAKE_VIDEO: %s\n", buf);
            char szOemXMLFileName[64] = {0};
            snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", DATA_BLOCK_MOUNT_PATH, FAKE_VIDEO_RES_FILE);
            rename(filePath, szOemXMLFileName);
            __WARN("set fake video, reboot\n");
            __RECORD_LOG_INFO("set fake video, reboot\n");
            anj_sysmng_delay_reboot(5);
            return 0;
        }
        else if (NULL != strstr(buf, "<PTZSTEP_CONFIG>") && NULL != strstr(buf, "</PTZSTEP_CONFIG>"))
        {
            char ptzStepFileName[64] = {0};
            snprintf(ptzStepFileName, sizeof(ptzStepFileName), "%s/%s", DATA_BLOCK_MOUNT_PATH, "ptzstep.cfg");
            rename(filePath, ptzStepFileName);
            module_uninit_single("anj_ptz");
            usleep(10 * 1000);
            module_init_single("anj_ptz");
            return 0;
        }
        else if (NULL != strstr(buf, "<AF_FOCUS_CONFIG>") && NULL != strstr(buf, "</AF_FOCUS_CONFIG>"))
        {
            __ERR("GET AF_FOCUS_CONFIG: %s \n", buf);

            char ptzStepFileName[64] = {0};
            snprintf(ptzStepFileName, sizeof(ptzStepFileName), "/tmp/%s", "af_focus.config.xml");
            rename(filePath, ptzStepFileName);

            // todo
            // anj_mw_system_with_param("killall media_server");
            return 0;
        }
        else if (NULL != strstr(buf, "<OEM_CONFIG>") /* && NULL != strstr(buf, "</OEM_CONFIG>")*/) // 不判断结尾，因为文件可能没读全
        {
            __ERR("GET OEM_CONFIG: %s\n", buf);

            AjOemStruct oemInfo_old = {0};
            anj_config_oem_get(&oemInfo_old);

            char szOemXMLFileName[64] = {0};
            snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);
            mkdir(szOemXMLFileName, 0777);
            snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_SPECIFIC_FILE_NAME);
            rename(filePath, szOemXMLFileName);

            anj_sysmng_version_info_get(&gstDevInfo.stVersionInfo, 0);
            anj_sysmng_search_device_get();

            AjOemStruct oemInfo = {0};
            anj_config_oem_get(&oemInfo);

            int bNeedSave = 0;
            NetworkConfigNew *pstNetWorkConfig = (NetworkConfigNew *)getNetWorkConfig();
            if (strlen(oemInfo.szOemEthMac) > 0 && is_mac_addr_valid(oemInfo.szOemEthMac))
            {
                strcpy((char *)pstNetWorkConfig->lanCfg.MACAddress, oemInfo.szOemEthMac);
                __ERR("set oem mac: %s\n", oemInfo.szOemEthMac);
                set_mac_addr(WIRE_INTERFACE_NAME, oemInfo.szOemEthMac, DEFAULT_WIRE_MAC_ADDR);
            }
            else if (strlen(oemInfo_old.szOemEthMac) > 0)
            {
                strcpy(oemInfo.szOemEthMac, oemInfo_old.szOemEthMac);
                bNeedSave = 1;
            }

            if (strlen(oemInfo.szOemSN) == 0 && strlen(oemInfo_old.szOemSN) > 0)
            {
                strcpy(oemInfo.szOemSN, oemInfo_old.szOemSN);
                bNeedSave = 1;
            }
            if (bNeedSave)
            {
                anj_config_oem_save(&oemInfo);
            }

            return 0;
        }
        else if (NULL != strstr(buf, "SET_OEM_SN_FLAG="))
        {
            char *p = buf + strlen("SET_OEM_SN_FLAG=");
            int iIndex = 0;
            for (iIndex = 0; iIndex < strlen(p); iIndex++)
            {
                if (!isprint(*p))
                {
                    *p = 0;
                    break;
                }
            }
            AjOemStruct oemInfo = {0};
            anj_config_oem_get(&oemInfo);

            if (strlen(p))
            {
                snprintf(oemInfo.szOemSN, sizeof(oemInfo.szOemSN), "%s", p);
            }

            anj_config_oem_save(&oemInfo);
            return 0;
        }
        else if (NULL != strstr(buf, "<AUDIOPARAM>"))
        {
            __ERR("GET AUDIOPARAM: %s\n", buf);
            char szOemXMLFileName[64] = {0};
            snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", DATA_BLOCK_MOUNT_PATH, "audio.config.xml");
            rename(filePath, szOemXMLFileName);
            anj_audio_restart();
            anj_record_restart();
            return 0;
        }
        else if (NULL != strstr(buf, "<DEBUG_LIGHT_CONFIG>"))
        {
            __ERR("GET DEBUG_LIGHT_CONFIG: %s\n", buf);
            char szFileName[64];
            sprintf(szFileName, "%s/%s", "/tmp", "light.debug.xml");
            rename(filePath, szFileName);

            // todo restart encode
            return 0;
        }
        else if (NULL != strstr(buf, "<DZOOMSETTING>"))
        {
            __ERR("GET DZOOM SETTING: %s\n", buf);
            rename(filePath, DZOOM_CUST_SETTING_FILE_FULL);

            double fSetValue = anj_config_zoom_multile_get_by_xml();
            if (DOUBLE_GREATER(fSetValue, 0.0))
            {
                SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
                SystemConfig stSystemConfig = *pstSystemConfig;
                stSystemConfig.ptzCfg.dzoomCfg.multiple_set = fSetValue;
                anj_config_system_set(&stSystemConfig);
            }

            remove(DZOOM_ENCODE_REAL_SETTING_FILE);
            return 0;
        }
        else if (strstr(buf, "SET_OEM_DEVTYPE_FLAG="))
        {
            char *p = buf + strlen("SET_OEM_DEVTYPE_FLAG=");
            int iIndex = 0;
            for (iIndex = 0; iIndex < strlen(p); iIndex++)
            {
                if (!isprint(*(p + iIndex)))
                {
                    *(p + iIndex) = 0;
                    break;
                }
            }

            __ERR("SET_OEM_DEVTYPE_FLAG=%s\n", p);

            AjOemStruct oemInfo = {0};
            anj_config_oem_get(&oemInfo);
            snprintf(oemInfo.szDeviceType, sizeof(oemInfo.szDeviceType), "%s", p);
            anj_config_oem_save(&oemInfo);

            if (remove(filePath) != 0)
            {
                __ERR("can not remove tmp config  file = %s for (%s)\n", filePath, strerror(errno));
            }

            anj_sysmng_version_info_get(&gstDevInfo.stVersionInfo, 0);
            anj_sysmng_search_device_get();

            return 0;
        }
        else if (strstr(buf, "DEL_OEM_DEVTYPE_FLAG="))
        {
            __ERR("DEL_OEM_DEVTYPE_FLAG\n");
            AjOemStruct oemInfo = {0};
            anj_config_oem_get(&oemInfo);
            strcpy(oemInfo.szDeviceType, "");
            anj_config_oem_save(&oemInfo);

            anj_sysmng_version_info_get(&gstDevInfo.stVersionInfo, 0);
            anj_sysmng_search_device_get();

            return 0;
        }
        else if (strstr(buf, "USER_DEFINE_CONFIG_FLAG"))
        {
            __ERR("USER_DEFINE_CONFIG_FLAG found, it's an user defined config file!!!\n");

            anj_mw_file_copy(filePath, USERDEF_CONFIG_PATH);

            if (remove(filePath) != 0)
            {
                __ERR("can not remove tmp config  file = %s for (%s)\n", filePath, strerror(errno));
            }
            return 0;
        }
        else if (strstr(buf, "SET_OEM_DEFAULT_CONFIG"))
        {
            __ERR("SET_OEM_DEFAULT_CONFIG found, it's an user defined default config file!!!\n");
            GlobalConfig *pCfg = anj_mw_malloc(sizeof(GlobalConfig));
            if (pCfg)
            {
                int iRet = anj_config_parse_file(filePath, pCfg);
                if (remove(filePath) != 0)
                {
                    __ERR("can not remove tmp config  file = %s for (%s)\n", filePath, strerror(errno));
                }

                if (iRet != 0)
                {
                    __ERR("parse failed: %s\n", filePath);
                    iRet = -1;
                }
                else
                {

                    char szCustFilePath[64] = {0};
                    snprintf(szCustFilePath, sizeof(szCustFilePath), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);
                    mkdir(szCustFilePath, 0777);

                    snprintf(szCustFilePath, sizeof(szCustFilePath), "%s/%s/%s",
                             DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, DEF_CONFIG_NAME);
                    remove(szCustFilePath);

                    char szFile[128] = {0};
                    anj_config_cust_default_path(szFile, sizeof(szFile));
                    anj_config_save_all(pCfg, szFile);
                    iRet = 0;
                }
                anj_mw_free(pCfg);
                return 0;
            }
            else
            {
                return -1;
            }
        }
        else if (NULL != strstr(buf, "<EXECUTE_USER_CMD>") && NULL != strstr(buf, "</EXECUTE_USER_CMD>"))
        {
            char *p = strstr(buf, "</EXECUTE_USER_CMD>");
            if (NULL != p)
            {
                *(p + strlen("</EXECUTE_USER_CMD>")) = 0;
            }

            anj_config_parse_user_cmd(buf);
            return 0;
        }
        else if (strstr(buf, "FIRMWARE_CONTROL_FILE"))
        {
            int changed = 0;

            __ERR("FIRMWARE_CONTROL_FILE flag found!!!\n");

            if (strstr(buf, "CLEARALL"))
            {
                __ERR("set CLEARALL flag!!!\n");

                remove("/mnt/nand/osdbigfont.flag");
                remove("/mnt/nand/jfs.flag");
                remove("/mnt/nand/p2plog.flag");
                remove("/mnt/nand/user_admin_login.flag");
                remove("/mnt/nand/shanghai_test.flag");
                remove("/mnt/nand/rtpsize.small.flag");
                remove("/mnt/nand/rtpsize.big.flag");
                remove("/mnt/nand/onvif_auth_enable.flag");
                remove("/mnt/nand/noweekday.flag");
                remove("/mnt/nand/ar0130_no1080p.flag");
                remove("/mnt/nand/ov9712_1080p.flag");
                changed = 1;
            }
            else
            {
                if (strstr(buf, "flag.allow.nvr"))
                {
                    anj_mw_create_file("/mnt/nand/flag.allow.nvr", 0);
                    changed = 1;
                }
                if (strstr(buf, "flag.forbit.nvr"))
                {
                    remove("/mnt/nand/flag.allow.nvr");
                    changed = 1;
                }
                if (strstr(buf, "AR0130_NO1080P"))
                {
                    if (anj_mw_file_exists("/mnt/nand/ar0130_no1080p.flag") == 0)
                    {
                        __ERR("set AR0130_NO1080P flag!!!\n");
                        anj_mw_create_file("/mnt/nand/ar0130_no1080p.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "OV9712_1080P"))
                {
                    if (anj_mw_file_exists("/mnt/nand/ov9712_1080p.flag") == 0)
                    {
                        __ERR("set OV9712_1080P flag!!!\n");

                        anj_mw_create_file("/mnt/nand/ov9712_1080p.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "OSDBIGFONT"))
                {
                    if (anj_mw_file_exists("/mnt/nand/osdbigfont.flag") == 0)
                    {
                        __ERR("set OSDBIGFONT flag!!!\n");

                        anj_mw_create_file("/mnt/nand/osdbigfont.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "JFS"))
                {
                    if (anj_mw_file_exists("/mnt/nand/jfs.flag") == 0)
                    {
                        __ERR("set JFS flag!!!\n");

                        anj_mw_create_file("/mnt/nand/jfs.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "P2PLOG"))
                {
                    if (anj_mw_file_exists("/mnt/nand/p2plog.flag") == 0)
                    {
                        __ERR("set P2PLOG flag!!!\n");

                        anj_mw_create_file("/mnt/nand/p2plog.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "USERNOLOGIN"))
                {
                    if (anj_mw_file_exists("/mnt/nand/user_admin_login.flag") == 0)
                    {
                        __ERR("set USERNOLOGIN flag!!!\n");

                        anj_mw_create_file("/mnt/nand/user_admin_login.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "SHANGHAITEST"))
                {
                    if (anj_mw_file_exists("/mnt/nand/shanghai_test.flag") == 0)
                    {
                        __ERR("set SHANGHAITEST flag!!!\n");

                        anj_mw_create_file("/mnt/nand/shanghai_test.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "RTPSMALL"))
                {
                    if (anj_mw_file_exists("/mnt/nand/rtpsize.small.flag") == 0)
                    {
                        __ERR("set RTPSMALL flag!!!\n");

                        anj_mw_create_file("/mnt/nand/rtpsize.small.flag", "1");

                        changed = 1;
                    }
                }
                else if (strstr(buf, "RTPBIG"))
                {
                    if (anj_mw_file_exists("/mnt/nand/rtpsize.big.flag") == 0)
                    {
                        __ERR("set RTPBIG flag!!!\n");

                        anj_mw_create_file("/mnt/nand/rtpsize.big.flag", "1");

                        changed = 1;
                    }
                }
                if (strstr(buf, "ONVIFAUTH"))
                {
                    if (anj_mw_file_exists("/mnt/nand/onvif_auth_enable.flag") == 0)
                    {
                        __ERR("set ONVIFAUTH flag!!!\n");

                        anj_mw_create_file("/mnt/nand/onvif_auth_enable.flag", "1");
                        changed = 1;
                    }
                }
                if (strstr(buf, "NOWEEKDAY"))
                {
                    if (anj_mw_file_exists("/mnt/nand/noweekday.flag") == 0)
                    {
                        __ERR("set noweekday flag!!!\n");

                        anj_mw_create_file("/mnt/nand/noweekday.flag", "1");
                        changed = 1;
                    }
                }
            }

            if (remove(filePath) != 0)
            {
                __ERR("can not remove tmp config  file = %s for (%s)\n", filePath, strerror(errno));
            }

            if (changed)
            {
                return 0;
            }
            else
            {
                return -1;
            }
        }
    }

    // firset check it is a anj_mw_system control file
    char control_buf[MAX_SYSTEM_CONTROL_STRING_LEN];
    if (GetSystemControlFromFile(filePath, control_buf) > 0)
    {
        return 0;
    }

    __ERR("NOT A SYSTEM CONTRL FILE\n");
    // than check it is a user config file
    GlobalConfig cfg = {0};
    if (anj_config_load(NULL, &cfg, filePath) == 0)
    {
        __ERR("UPDATE USER CONFIG\n");
        int iRet = anj_sysmng_user_config_update(filePath);
        __WARN("update config, reboot\n");
        __RECORD_LOG_INFO("update config, reboot\n");
        anj_sysmng_reboot();
        return iRet;
    }

    __ERR("config file is not valid\n");

    // just remove the file;
    if (remove(filePath) != 0)
    {
        __ERR("can not remove tmp config  file = %s for (%s)\n", filePath, strerror(errno));
    }

    __ERR("config file is not valid, reboot\n");
    __RECORD_LOG_INFO("config file is not valid, reboot\n");
    anj_sysmng_reboot();

    return -1;
}

static int anj_sysmng_compare_fs_version(char *fsVersion)
{
    int i = 0;
    int part_count = 0;
    char *parts[16] = {0};

    if (fsVersion == NULL)
    {
        return 0;
    }

    __INFO("Input fs Info: %s", fsVersion);

    char *token = strtok(fsVersion, "_");
    while (token != NULL && part_count < 16)
    {
        parts[part_count++] = token;
        token = strtok(NULL, "_");
    }

    // 获取版本号
    char *version = NULL;
    for (i = 0; i < part_count; i++)
    {
        if (is_version_part(parts[i]))
        {
            version = parts[i];
            break;
        }
    }

    if (version == NULL)
    {
        return 0;
    }

    __INFO("Parse get version: %s", version);

    if (str_compare_versions(version, MIN_FS_VERSION) >= 0)
    {
    }

    return 0;
}

int anj_sysmng_firmware_md5(const char *firmwareFile, char *md5_buf, int buf_len)
{
    int iRet;
    if (buf_len < 32)
        return -1;
    memset(md5_buf, 0, buf_len);

    char cmd[512] = {0};
    snprintf(cmd, sizeof(cmd), "md5sum %s", firmwareFile); // output md5 and path.
    FILE *stream = popen(cmd, "r");
    if (!stream)
    {
        return -1;
    }
    fgets(md5_buf, 33, stream);

    iRet = pclose(stream);
    if (iRet != 0)
        return -1;
    return 0;
}

static int anj_sysmng_firmware_flash_mtd(const FirmWareHeader *header)
{
    int iRet = 0;

    ANJ_CHK((header != NULL), -1, "input invalid!");

    if (header->type & FIRMWARE_TYPE_KERNAL)
    {
        __INFO("ready to flash kernal to nand %s\n", GET_KERNEL_MTD_DEV());
        iRet = FlashNand(TMP_KERNEL_FILE_NAME, GET_KERNEL_MTD_DEV(), 0);

        const char *szBkDev = GET_KERNEL_MTD_DEV_BK();
        if (iRet == 0 && szBkDev != NULL && *szBkDev != 0)
        {
            iRet = FlashNand(TMP_KERNEL_FILE_NAME, szBkDev, 0);
        }

        remove(TMP_KERNEL_FILE_NAME);
        if (iRet != 0)
        {
            __ERR("flash kernel fail\n");
            goto endFunc;
        }
    }

    if (header->type & FIRMWARE_TYPE_FILESYSTEM)
    {
        __INFO("ready to flash rootfs to nand %s\n", GET_FILESYS_MTD_DEV());
        iRet = FlashNand(TMP_FILESYSTEM_FILE_NAME, GET_FILESYS_MTD_DEV(), 0);
        if (iRet != 0)
        {
            __ERR("flash rootfs fail\n");
        }
    }

endFunc:
    return iRet;
}
static int anj_sysmng_flash_exec_process(char *bufPtr, unsigned long long phyAddr, int fileSize)
{
    FILE *fp = NULL;
    char phy_str[32];
    char map_str[32];
    unsigned int map_size = 0;
    if (fileSize <= 0)
    {
        return -1;
    }

    if (phyAddr == 0 && bufPtr == NULL)
    {
        return -1;
    }

    map_size = ANJ_ALIGN_UP((unsigned int)fileSize, 1024 * 1024);
    snprintf(phy_str, sizeof(phy_str), "%#llx", phyAddr);
    snprintf(map_str, sizeof(map_str), "%u", map_size);

    anj_mw_system("cp /opt/ch/anjflash /tmp/anjflash && chmod +x /tmp/anjflash");

    if (phyAddr == 0)
    {
        fp = fopen("/tmp/ota_firmware.bin", "wb");
        if (fp == NULL)
        {
            __ERR("open /tmp/ota_firmware.bin failed\n");
            return -1;
        }

        if ((int)fwrite(bufPtr, 1, (size_t)fileSize, fp) != fileSize)
        {
            fclose(fp);
            __ERR("write /tmp/ota_firmware.bin failed\n");
            return -1;
        }
        fclose(fp);
    }
    else
    {
        __WARN("flash via MMA phys=%#llx map_size=%u\n", phyAddr, map_size);
    }

    __WARN("exec /tmp/anjflash %s %s\n", phy_str, map_str);
    return anj_mw_system_with_param("/tmp/anjflash %s %s &", phy_str, map_str);
}

static int anj_sysmng_firmware_save_sdcard(char *firmwareFile, char *bufPtr, int fileSize)
{
    anj_sdcard_info *pstSdInfo = anj_sdcard_info_get();
    int bHaveSDCard = (pstSdInfo->eStatus != ANJ_SDCARD_STATUS_NOT_INSERT) ? 1 : 0;
    int iIndex = pstSdInfo->iIndex;

    char resetfile[] = "/mnt/nand/reset_mcu_flag";
    // MCU升级失败重启标志
    if ((ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV) && anj_mw_file_exists(resetfile))
    {
        remove(resetfile);
        __INFO("Del reset file:%s", resetfile);
        return 0;
    }

    if (bHaveSDCard)
    {
        char filePath[64] = {0};
        snprintf(filePath, sizeof(filePath), SDCARD_MOUNT_PATH "/firmware_reupdate_aj.bin", iIndex);
        remove(filePath);

        if (firmwareFile != NULL && strlen(firmwareFile) > 0)
        {
            anj_mw_file_copy(firmwareFile, filePath);
        }

        if (bufPtr)
        {
            FILE *file = anj_mw_fopen(filePath, "wb");
            if (file == NULL)
            {
                __ERR("Error opening file");
                return EXIT_FAILURE;
            }

            size_t result = anj_mw_fwrite(file, bufPtr, fileSize);
            if (result != fileSize)
            {
                __ERR("Error writing to file");
                anj_mw_fclose(file);
                return EXIT_FAILURE;
            }

            anj_mw_fclose(file);
        }
    }
    return 0;
}

static int anj_sysmng_firmware_update(char *firmwareFile, char *bufPtr, int fileSize, unsigned long long extPhyAddr)
{
    int iRet = 0;
    FILE *p_upgrade_fd = NULL;
    unsigned long long phyAddr = extPhyAddr;
    void *pMappedAddr = NULL;
    ANJ_CHK((firmwareFile != NULL) && (fileSize > 0), -1, "input invalid!");

    if (extPhyAddr > 0 && bufPtr != NULL)
    {
        pMappedAddr = bufPtr;
    }

    int i = 0;
    char cmd[256] = {0};
    int del_firmware_file = 1;

    if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
    {
        anj_mw_system("cp /bin/busybox /tmp/busybox && chmod +x /tmp/busybox");
        anj_mw_system("cp /bin/reboot /tmp/reboot && chmod +x /tmp/reboot");
        anj_mw_system("sed -i 's|/bin/|/tmp/|g' /tmp/reboot");
    }

    for (i = 0; i < SDCARD_MAX_DEV; i++)
    {
        char stMountPath[128] = {0};
        snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, i);
        if (strncmp(firmwareFile, stMountPath, strlen(stMountPath)) == 0)
        {
            char del_firmware_flag_file[256] = {0};
            snprintf(del_firmware_flag_file, sizeof(del_firmware_flag_file), "%s/not_del_firmware_file.flag", stMountPath);
            if (anj_mw_file_exists(del_firmware_flag_file))
            {
                del_firmware_file = 0;
            }
            break;
        }
    }

    __INFO("prepare for firmware update, filename=%s, buf=%#x, len=%d\n",
           firmwareFile, bufPtr, fileSize);

    __INFO("WEB_PROCESS_KEY = %d, TPSSERVER_PROCESS_KEY =%d, AUX_PROCESS_KEY = %d\n",
           WEB_PROCESS_KEY,
           TPSSERVER_PROCESS_KEY,
           AUX_PROCESS_KEY);

    FirmWareHeader header = {0};
    int nKernelSize = 0;
    int nFsSize = 0;
    int nUbootSize = 0;
    int nTgzSize = 0;
    int nOemSize = 0;

    char rom_md5[64] = "";

    if (bufPtr == NULL)
    {
        __INFO("FirmwareUpdate use mmap mode\n");
        if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
        {
            sprintf(cmd, "echo -e -n \"\\xFC\\x01\\xAE\\x02\\x00\\x00\\xAD\" > /dev/ttyS2");
            anj_mw_system(cmd);
            usleep(10 * 1000);
            anj_mw_system(cmd);
        }

        //////////分配一块共享内存，存储升级文件//////////////
        __INFO("alloc sys mem for firmware update, fileSize=%d\n", fileSize);
        ANJ_CHK_FUNC(anj_sys_alloc(fileSize, &phyAddr), 0, "sys alloc failed!");
        pMappedAddr = anj_sys_mmap(phyAddr, fileSize);
        ANJ_CHK((pMappedAddr != NULL), -1, "sys mmap failed!");

        p_upgrade_fd = anj_mw_fopen(firmwareFile, "r+b");
        ANJ_CHK((p_upgrade_fd != NULL), -1, "file opne failed!");

        size_t read_len = 0;
        int total_read = 0;
        char *pA = pMappedAddr;

        while (1)
        {
            read_len = anj_mw_fread(p_upgrade_fd, pA + total_read, 1024 * 1024);
            if (0 == read_len)
            {
                __INFO("%s read finished, data len is %d !!!", firmwareFile, total_read);
                break;
            }
            total_read += read_len;
        }
        anj_mw_fclose(p_upgrade_fd);
        p_upgrade_fd = NULL;
        bufPtr = pMappedAddr;

        if (del_firmware_file)
        {
            remove(firmwareFile);
        }
        // memset(firmwareFile, 0, fileSize);
    }

    if (anj_mw_file_exists(firmwareFile))
    {
        if (anj_sysmng_firmware_md5(firmwareFile, rom_md5, sizeof(rom_md5)) != 0)
        {
            __ERR("!!!Warm: Get firmware md5 value fail.\n");
        }
        __INFO("Get firmware md5 value:%s.\n", rom_md5);

        if (CheckFirmwareFile(firmwareFile,
                              &nKernelSize,
                              &nFsSize, &nUbootSize, &nTgzSize, &nOemSize, &header) != 0)
        {
            __ERR("check firmware %s fail(%s)\n", firmwareFile, strerror(errno));
            iRet = -1;
            goto endFunc;
        }
    }
    else
    {
        if (CheckFirmwareBuf(bufPtr, fileSize,
                             &nKernelSize,
                             &nFsSize, &nUbootSize, &nTgzSize, &nOemSize, &header) != 0)
        {
            __ERR("check firmware buf %#x:%d fail\n", bufPtr, fileSize);
            iRet = -1;
            goto endFunc;
        }
        else
        {
            __INFO("check firmware buf %#x:%d OK\n", bufPtr, fileSize);
        }
    }

    anj_sysmng_compare_fs_version(header.fs_version_info);

    if (anj_mw_file_exists(firmwareFile))
    {
        anj_sysmng_firmware_save_sdcard(firmwareFile, bufPtr, fileSize);
    }

    usleep(100 * 1000);

    int update_tmpby_file = (bufPtr == NULL) ? 1 : 0;
    int freemem = anj_sysmng_get_freemem();
    __INFO("freemem before umount is %d kB\n", freemem);

    if (strstr(firmwareFile, "/mnt/"))
    {
        __ERR("firmware in /mnt, no umount\n");
    }
    else
    {
        anj_sdcard_umount();
        module_uninit_single("anj_sdcard");
    }
    anj_mw_system_free_cache();
    int totalmem = anj_sysmng_get_totalmem();
    freemem = anj_sysmng_get_freemem();
    __INFO("freemem after umount is %d kB. total memory %d kB\n", freemem, totalmem);

    __INFO("start updating firmware...\n");
    __INFO("begin to seperate file...\n");

    freemem = anj_sysmng_get_freemem();
    __INFO("freemem before upgrading is %d kB\n", freemem);

    if (header.type & FIRMWARE_TYPE_FACTTORY)
    {
        __INFO("clear all custon and restore to factory\n");
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, sizeof(cmd), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);
        remove(cmd);
        remove(CONFIG_FILE_PATH);
    }

    if (header.type & FIRMWARE_TYPE_OEM)
    {
        if (update_tmpby_file)
        {
            iRet = GetFileFromFirmwareEx(firmwareFile, &header, FIRMWARE_TYPE_OEM, TMP_OEM_FILE_NAME);
        }
        else
        {
            iRet = GetFileFromBufEx(bufPtr, &header, FIRMWARE_TYPE_OEM, TMP_OEM_FILE_NAME);
        }
        if (iRet != 0)
        {
            __ERR("Get firmware file %s failed\n", TMP_OEM_FILE_NAME);
            iRet = -1;
            goto endFunc;
        }

        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, sizeof(cmd), AJ_APP_PATH "/7zDec x %s -o%s/&& sync", TMP_OEM_FILE_NAME, OEM_MOUNT_PATH);
        anj_mw_system(cmd);

        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, sizeof(cmd), "chmod +x %s/*", OEM_APP_PATH);
        anj_mw_system(cmd);

        char fileVer[64] = {0};
        anj_mw_read_file_limit_len("/etc/flag.customize", fileVer, sizeof(fileVer));
        if (strstr(fileVer, "_CWYY") != NULL)
        {
            mkdir(OEM_MOUNT_PATH2, 0777);
        }
    }

    remove(TMP_OEM_FILE_NAME);

    if (header.type & FIRMWARE_TYPE_UBOOT)
    {
        char *szUpdateFile = TMP_UBOOT_FILE_NAME;
        if (update_tmpby_file)
        {
            iRet = GetFileFromFirmwareEx(firmwareFile, &header, FIRMWARE_TYPE_UBOOT, szUpdateFile);
        }
        else
        {
            iRet = GetFileFromBufEx(bufPtr, &header, FIRMWARE_TYPE_UBOOT, szUpdateFile);
        }
        if (iRet != 0)
        {
            __ERR("Get firmware file %s failed\n", szUpdateFile);
            iRet = -1;
            goto endFunc;
        }

        __INFO("seperate uboot file ok!\n");
        if (soft_enc_uboot_write(szUpdateFile) != 0)
        {
            __ERR("Write uboot failed: %s\n", szUpdateFile);
        }
    }
    remove(TMP_UBOOT_FILE_NAME);

    if (header.type & FIRMWARE_TYPE_TGZ)
    {
        char *szUpdateFile = TMP_TGZ_FILE_NAME;
        if (update_tmpby_file)
        {
            iRet = GetFileFromFirmwareEx(firmwareFile, &header, FIRMWARE_TYPE_TGZ, szUpdateFile);
        }
        else
        {
            iRet = GetFileFromBufEx(bufPtr, &header, FIRMWARE_TYPE_TGZ, szUpdateFile);
        }
        if (iRet != 0)
        {
            __ERR("Get firmware file %s failed\n", szUpdateFile);
            iRet = -1;
            goto endFunc;
        }

        __INFO("seperate TGZ file ok!\n");

        char fileVer[64] = {0};
        anj_mw_read_file_limit_len("/etc/filesys.ver", fileVer, sizeof(fileVer));
        if (strstr(fileVer, "_ESA") != NULL)
        {
            memset(cmd, 0, sizeof(cmd));
            snprintf(cmd, sizeof(cmd), AJ_APP_PATH "/7zDec x %s -o%s/&& sync", TMP_TGZ_FILE_NAME, OEM_APP_PATH);
            anj_mw_system(cmd);
        }
        else
        {
            AjOemStruct *pInfoOld = NULL;
            AjOemStruct *pInfoNew = NULL;
            pInfoOld = (AjOemStruct *)anj_mw_malloc(sizeof(AjOemStruct));
            pInfoNew = (AjOemStruct *)anj_mw_malloc(sizeof(AjOemStruct));
            if (NULL != pInfoOld)
            {
                memset(pInfoOld, 0, sizeof(AjOemStruct));
                anj_config_oem_get(pInfoOld);
            }

            memset(cmd, 0, sizeof(cmd));
            snprintf(cmd, sizeof(cmd), AJ_APP_PATH "/7zDec x %s -o%s/&& sync", TMP_TGZ_FILE_NAME, DATA_BLOCK_MOUNT_PATH);
            anj_mw_system(cmd);

            char szCustDefaultConfigFileName[128] = {0};
            snprintf(szCustDefaultConfigFileName, sizeof(szCustDefaultConfigFileName),
                     "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, "config.default.xml");
            if (anj_mw_file_exists(szCustDefaultConfigFileName))
            {
                // 同步config.xml与dzoom.config.xml
                if (!anj_config_zoom_exist(szCustDefaultConfigFileName)) // 不存在，不用管
                {
                }
                else
                {
                    GlobalConfig *pCfg = (GlobalConfig *)anj_mw_malloc(sizeof(GlobalConfig));
                    if (NULL != pCfg)
                    {
                        anj_config_parse_file(szCustDefaultConfigFileName, pCfg);
                        anj_config_zoom_multile_save(pCfg->systemCfg.ptzCfg.dzoomCfg.multiple_set);
                        anj_mw_free(pCfg);
                    }
                }
            }

            if (NULL != pInfoNew)
            {
                memset(pInfoNew, 0, sizeof(AjOemStruct));
                anj_config_oem_get(pInfoNew);
            }

            if (NULL != pInfoNew && NULL != pInfoOld)
            {
                int bSet = 0;
                if (strlen(pInfoNew->szOemSN) == 0 && strlen(pInfoOld->szOemSN) > 0)
                {
                    __ERR("Restore old oem SN: %s\n", pInfoOld->szOemSN);
                    strcpy(pInfoNew->szOemSN, pInfoOld->szOemSN);
                    bSet = 1;
                }
                if (strlen(pInfoNew->szOemEthMac) == 0 && strlen(pInfoOld->szOemEthMac) > 0)
                {
                    __ERR("Restore old oem EthMac: %s\n", pInfoOld->szOemEthMac);
                    strcpy(pInfoNew->szOemEthMac, pInfoOld->szOemEthMac);
                    bSet = 1;
                }
                if (strlen(pInfoNew->szDeviceType) == 0 && strlen(pInfoOld->szDeviceType) > 0)
                {
                    __ERR("Restore old oem DeviceType: %s\n", pInfoOld->szDeviceType);
                    strcpy(pInfoNew->szDeviceType, pInfoOld->szDeviceType);
                    bSet = 1;
                }
                if (strlen(pInfoNew->szOemMBL) == 0 && strlen(pInfoOld->szOemMBL) > 0)
                {
                    __ERR("Restore old oem MBL: %s\n", pInfoOld->szOemMBL);
                    strcpy(pInfoNew->szOemMBL, pInfoOld->szOemMBL);
                    bSet = 1;
                }
                if (strlen(pInfoNew->szOemLanguage) == 0 && strlen(pInfoOld->szOemLanguage) > 0)
                {
                    __ERR("Restore old language: %s\n", pInfoOld->szOemLanguage);
                    strcpy(pInfoNew->szOemLanguage, pInfoOld->szOemLanguage);
                    bSet = 1;
                }

                if (bSet > 0)
                {
                    anj_config_oem_save(pInfoNew);
                }

                free(pInfoNew);
                free(pInfoOld);
            }

            anj_config_load_cust();
        }
    }
    remove(TMP_TGZ_FILE_NAME);

    if (SUPPORT_NAND_FLASH)
    {
        if (header.type & FIRMWARE_TYPE_KERNAL)
        {
            unsigned int nFirmwareType = FIRMWARE_TYPE_KERNAL;
            char *szUpdateFile = TMP_KERNEL_FILE_NAME;

            if (update_tmpby_file)
            {
                iRet = GetFileFromFirmwareEx(firmwareFile, &header, nFirmwareType, szUpdateFile);
            }
            else
            {
                iRet = GetFileFromBufEx(bufPtr, &header, nFirmwareType, szUpdateFile);
            }
            if (iRet != 0)
            {
                __ERR("Get firmware file %s failed\n", szUpdateFile);
                iRet = -1;
                goto endFunc;
            }
            __INFO("seperate kernel file ok!\n");
        }

        if (header.type & FIRMWARE_TYPE_FILESYSTEM)
        {
            unsigned int nFirmwareType = FIRMWARE_TYPE_FILESYSTEM;
            char *szUpdateFile = TMP_FILESYSTEM_FILE_NAME;
            if (update_tmpby_file)
            {
                iRet = GetFileFromFirmwareEx(firmwareFile, &header, nFirmwareType, szUpdateFile);
            }
            else
            {
                iRet = GetFileFromBufEx(bufPtr, &header, nFirmwareType, szUpdateFile);
            }
            if (iRet != 0)
            {
                __ERR("Get firmware file %s failed\n", szUpdateFile);
                iRet = -1;
                goto endFunc;
            }
            __INFO("seperate file rootfs file ok!\n");
        }

        if (del_firmware_file)
        {
            remove(firmwareFile);
        }
        freemem = anj_sysmng_get_freemem();
        __INFO("freemem after sperate fs: %d kB\n", freemem);
    }

    gstDevInfo.bUpgrading = 1;

    __INFO("firmware update: uninit all modules before flash\n");
    modules_uninit(NULL);
    anj_mw_system_free_cache();
    usleep(100 * 1000);

    WatchDogSetTimeOut(600, 0);
    __INFO("watchdog timeout set to 600 sec before flash\n");

    if (SUPPORT_NAND_FLASH)
    {
        iRet = anj_sysmng_firmware_flash_mtd(&header);
        if (iRet == 0)
        {
            __INFO("finish updating firmware...\n");

            anj_audio_prompt_play(ANJ_MP3_OTA_PATH, ANJ_MP3_DEVICE_UPDATED, 1);
            if (del_firmware_file)
            {
                remove(firmwareFile);
            }

            FILE *rom_md5_file = anj_mw_fopen(AJ_CURR_ROM_MD5_FILE_NAME, "w");
            if (rom_md5_file == NULL)
            {
                __ERR("!!Error: open rom md5 file failed.\n");
            }
            else
            {
                anj_mw_fwrite(rom_md5_file, rom_md5, strlen(rom_md5));
                anj_mw_fclose(rom_md5_file);
            }

            anj_mw_create_file(OTA_FINISHED_FLAG, NULL);
            sync();
        }
        else
        {
            __ERR("firmware flash failed, iRet=%d\n", iRet);
            if (header.type & FIRMWARE_TYPE_KERNAL)
            {
                WatchDogForceReset();
            }
            __RECORD_LOG_INFO("firmware flash failed, reboot\n");
        }

        if (pMappedAddr != NULL)
        {
            anj_sys_munmap(pMappedAddr, fileSize);
        }

        if (phyAddr)
        {
            anj_sys_free(phyAddr);
        }

        gstDevInfo.bUpgrading = 0;
        __WARN("call delay reboot...\n");
        anj_sysmng_delay_reboot(iRet == 0 ? 3 : 5);
        return iRet;
    }
    else
    {
        iRet = anj_sysmng_flash_exec_process(bufPtr, phyAddr, fileSize);
        if (iRet != 0)
        {
            __ERR("firmware flash exec failed, iRet=%d\n", iRet);
        }
        __WARN("flash exec process done\n");
    }

endFunc:
    return iRet;
}

__attribute__((weak))
int anj_sysmng_apply_firmware_update(APPBIN_UPDATE_DATA *updateData, char *bufPtr, int nFileLen)
{
    return anj_sysmng_firmware_update(updateData->filePath, bufPtr, nFileLen, updateData->nPhyAddr);
}

static int anj_sysmng_rootfs_bak_name_ok(const char *origName)
{
    char need[96] = {0};

    if (origName == NULL || origName[0] == 0)
    {
        __ERR("rootfs_bak origName empty\n");
        return 0;
    }

    if (strstr(origName, "rootfs_bak_") == NULL)
    {
        __ERR("rootfs_bak name missing prefix: %s\n", origName);
        return 0;
    }

    /* 与主固件 CheckVersionMatch 一致：TCA55_V0（product.ver） */
    snprintf(need, sizeof(need), "%s_V%s", ANJ_PROJECT_NAME, anj_sysmng_product_version_get());
    if (strstr(origName, need) == NULL)
    {
        __ERR("rootfs_bak name mismatch: orig=%s need=%s\n", origName, need);
        return 0;
    }
    return 1;
}

static int anj_sysmng_rootfs_bak_update(APPBIN_UPDATE_DATA *updateData, char *bufPtr, int nFileLen)
{
    int iRet = -1;
    const char *bk = GET_FILESYS_MTD_DEV_BK();
    int fd = -1;
    struct mtd_info_user mtd;

    if (!anj_sysmng_rootfs_bak_name_ok(updateData->origName))
        return -1;

    if (bk == NULL || bk[0] == 0)
    {
        __ERR("rootfs_bak mtd not defined\n");
        return -1;
    }

    if (nFileLen <= 64 * 1024)
    {
        __ERR("rootfs_bak size %d too small\n", nFileLen);
        return -1;
    }

    fd = open(bk, O_RDONLY);
    if (fd < 0)
    {
        __ERR("open %s failed\n", bk);
        return -1;
    }
    if (ioctl(fd, MEMGETINFO, &mtd) < 0)
    {
        __ERR("%s is not a MTD flash device\n", bk);
        close(fd);
        return -1;
    }
    close(fd);

    if ((unsigned int)nFileLen > mtd.size)
    {
        __ERR("rootfs_bak size %d bigger than %s size %u\n", nFileLen, bk, mtd.size);
        return -1;
    }

    gstDevInfo.bUpgrading = 1;
    WatchDogSetTimeOut(600, 0);

    /* NOR busybox 无 flash_eraseall，不用 FlashNand；与 anjflash 一样走 ioctl 直刷 */
    if (updateData->nPhyAddr > 0 && bufPtr != NULL)
    {
        __INFO("flash rootfs_bak buf -> %s, len=%d\n", bk, nFileLen);
        iRet = FlashMTD_byBufferOffset(bufPtr, 0, nFileLen, bk);
    }
    else
    {
        __INFO("flash rootfs_bak %s -> %s, len=%d\n", updateData->filePath, bk, nFileLen);
        iRet = FlashMTD_byFileOffset(updateData->filePath, 0, nFileLen, bk);
    }
    if (iRet != 0)
    {
        __ERR("flash rootfs_bak to %s fail, ret=%d\n", bk, iRet);
        iRet = -1;
    }

    gstDevInfo.bUpgrading = 0;
    if (iRet == 0)
    {
        __WARN("rootfs_bak update ok, reboot\n");
        __RECORD_LOG_INFO("rootfs_bak update ok, reboot\n");
        anj_sysmng_delay_reboot(3);
    }
    return iRet;
}

int anj_sysmng_app_update(APPBIN_UPDATE_DATA *updateData)
{
    int iRet = 0;
    int kfs_size = 0;
    int fs_size = 0;
    int uboot_size = 0;
    int tgz_size = 0;
    int oem_size = 0;
    char *bufPtr = NULL;
    FirmWareHeader header;
    char magic[4] = {0};

    int nFileLen = 0;

    if (updateData->nPhyAddr > 0)
    {
        bufPtr = anj_sys_mmap((unsigned long long)updateData->nPhyAddr, updateData->nFileLen);
        nFileLen = updateData->nFileLen;
        if (bufPtr != NULL && nFileLen >= 4)
            memcpy(magic, bufPtr, 4);
        __INFO("firmware phy addr = %#x, mapaddr = %#p, length=%d \n", updateData->nPhyAddr, bufPtr, nFileLen);
    }
    else
    {
        struct stat file_info;
        int fd;

        memset(&file_info, 0, sizeof(file_info));
        if (stat(updateData->filePath, &file_info) == 0)
            nFileLen = file_info.st_size;

        __INFO("firmware file = %s, length=%d \n", updateData->filePath, nFileLen);

        fd = open(updateData->filePath, O_RDONLY);
        if (fd >= 0)
        {
            if (read(fd, magic, 4) != 4)
                memset(magic, 0, sizeof(magic));
            close(fd);
        }
    }

    if (memcmp(magic, "hsqs", 4) == 0)
    {
        iRet = anj_sysmng_rootfs_bak_update(updateData, bufPtr, nFileLen);
        goto EXIT;
    }

    if (updateData->nPhyAddr > 0)
    {
        if (CheckFirmwareBuf(bufPtr, updateData->nFileLen,
                             &kfs_size,
                             &fs_size, &uboot_size, &tgz_size, &oem_size, &header) < 0)
        {
            __ERR("check firmware buf %#x:%d fail\n", bufPtr, nFileLen);
            iRet = -1;
            goto EXIT;
        }
    }
    else
    {
        if (nFileLen > (MAX_APPBIN_SIZE + MAX_KERNEL_SIZE))
        {
            __ERR("uploaded file size %d too big!\n", nFileLen);
            iRet = -1;
        }
        else
        {
            if (CheckFirmwareFile(updateData->filePath,
                                  &kfs_size,
                                  &fs_size, &uboot_size, &tgz_size, &oem_size, &header) < 0)
            {
                __ERR("firmware check failed!\n");
                iRet = -1;
            }
        }
    }

    if (iRet == 0)
    {
        if (fs_size > 0)
        {
            if (fs_size < MIN_APPBIN_SIZE)
            {
                __ERR("filesys file size %d too small.\n", fs_size);
                iRet = -1;
            }
            else if (fs_size > MAX_APPBIN_SIZE)
            {
                __ERR("filesys file size %d too large.\n", fs_size);
                iRet = -2;
            }
        }

        if (kfs_size > 0)
        {
            if (kfs_size < MIN_KERNEL_SIZE)
            {
                __ERR("kernel file size %d too small.\n", kfs_size);
                iRet = -1;
            }
            else if (kfs_size > MAX_KERNEL_SIZE)
            {
                __ERR("kernel file size %d too large.\n", kfs_size);
                iRet = -2;
            }
        }

        if (uboot_size > 0)
        {
            if (uboot_size < MIN_UBOOT_SIZE)
            {
                __ERR("uboot file size %d too small.\n", uboot_size);
                iRet = -1;
            }
            else if (uboot_size > MAX_UBOOT_SIZE)
            {
                __ERR("uboot file size %d too large.\n", uboot_size);
                iRet = -2;
            }
        }

        if (tgz_size > 0)
        {
            int nMaxSize = MAX_TGZ_SIZE;
            if (tgz_size < MIN_TGZ_SIZE)
            {
                __ERR("filesys file size %d too small.\n", tgz_size);
                iRet = -1;
            }
            else if (tgz_size > nMaxSize)
            {
                __ERR("filesys file size %d too large, max value: %d.\n", tgz_size, nMaxSize);
                iRet = -2;
            }
        }

        if (iRet == 0)
        {
            iRet = anj_sysmng_apply_firmware_update(updateData, bufPtr, nFileLen);
        }
    }

EXIT:
    __INFO("iRet = %d\n", iRet);

    if (iRet != 0)
    {
        gstDevInfo.bUpgrading = 0;
        remove(updateData->filePath);
        __WARN("firmware update failed, reboot\n");
        __RECORD_LOG_INFO("firmware update failed, reboot\n");
        anj_sysmng_delay_reboot(5);
    }

    return iRet;
}

int anj_sysmng_p2p_update(char *filename, int p2ptype, const char *szP2pidFile)
{
    char szDstFile[128] = {0};
    struct timeval now_tm;
    struct tm *ptm;
    gettimeofday(&now_tm, NULL);
    ptm = localtime(&now_tm.tv_sec);
    sprintf(szDstFile, "%s.bk_%04d%02d%02d_%02d%02d%02d",
            szP2pidFile,
            (1900 + ptm->tm_year),
            (1 + ptm->tm_mon),
            ptm->tm_mday,
            ptm->tm_hour,
            ptm->tm_min,
            ptm->tm_sec);

    rename(szP2pidFile, szDstFile);

    char buffer1[256] = {0};
    int len = 0;

    len = anj_mw_read_file_limit_len(filename, buffer1, sizeof(buffer1));
    if (len > 0 && len < 256)
    {
        char buffer2[512] = {0};

        if (0 == hexdataTohexStr(buffer1, len, buffer2, 512))
        {
            WriteP2pid(buffer2, len * 2, p2ptype);
        }
    }

    rename(filename, szP2pidFile);
    module_uninit_single("anj_ser");
    usleep(10 * 1000);
    module_init_single("anj_ser");
    return 0;
}

int anj_sysmng_file_update(char *filename)
{
    const char *szFileName = strGetFilename((const char *)filename);
    __ERR("upload file: %s ok, filename %s\n", filename, szFileName);

    int bAiot = 0;
    if (strcmp(szFileName, "clean_secondconfig.flag") == 0)
    {
        remove(OEM_SECOND_CONFIG_PATH);
        remove(OEM_SECOND_RESTORE_FLAG_PATH);
        remove(OEM_SECOND_INIT_FLAG_PATH);
        remove(OEM_SECOND_SAVE_FLAG_PATH);
        remove("/mnt/nand/fakevideo.xml");
        remove("/mnt/nand/Myfakevideo_flag");
        __ERR("clean_secondconfig over\n");
        __RECORD_LOG_INFO("restore config reserved_bits=0\n");
        anj_sysmng_config_restore(0);
    }
    else if (strcmp(szFileName, ".aiot") == 0)
    {
        struct stat mstat;
        memset(&mstat, 0, sizeof(mstat));
        if (lstat(filename, &mstat) == -1)
        {
            __ERR("get file stat error: %s\n", filename);
        }
        else
        {
            int filesize = mstat.st_size;
            if (filesize > 0 && filesize <= 512)
            {
                bAiot = 1;
            }
        }
    }
    else if (strcmp(szFileName, ".txt") == 0 &&
             strcmp(szFileName, "dotid") == 0)
    {
        struct stat mstat;
        memset(&mstat, 0, sizeof(mstat));
        if (lstat(filename, &mstat) == -1)
        {
            __ERR("get file stat error: %s\n", filename);
        }
        else
        {
            int filesize = mstat.st_size;
            if (filesize > 0 && filesize <= 8192)
            {
                anj_mw_system_with_param("cp %s /mnt/nand/dotid.txt -rf", filename);
            }
        }
    }
    else if (strcmp(szFileName, ".txt") == 0 &&
             strcmp(szFileName, "haier_id") == 0)
    {
        struct stat mstat;
        memset(&mstat, 0, sizeof(mstat));
        if (lstat(filename, &mstat) == -1)
        {
            __ERR("get file stat error: %s\n", filename);
        }
        else
        {
            int filesize = mstat.st_size;
            if (filesize > 0 && filesize <= 8192)
            {
                anj_mw_system_with_param("cp %s /mnt/nand/haier_id.txt -rf", filename);
            }
        }
    }
    else if (strcmp(szFileName, ".bin") == 0 &&
             strcmp(szFileName, "haier_cert_data") == 0)
    {
        struct stat mstat;
        memset(&mstat, 0, sizeof(mstat));
        if (lstat(filename, &mstat) == -1)
        {
            __ERR("get file stat error: %s\n", filename);
        }
        else
        {
            int filesize = mstat.st_size;
            if (filesize > 0 && filesize <= 8192)
            {
                anj_mw_system_with_param("cp %s /mnt/nand/haier_cert_data.bin -rf", filename);
            }
        }
    }
    else if (strcmp(szFileName, "isp_day.bin") == 0)
    {
        struct stat mstat;
        memset(&mstat, 0, sizeof(mstat));
        if (lstat(filename, &mstat) == -1)
        {
            __ERR("get file stat error: %s\n", filename);
        }
        else
        {
            module_uninit_single("anj_ispctl");
            usleep(100 * 1000);
            module_init_single("anj_ispctl");
        }
    }
    else if (strcmp(szFileName, "isp_night.bin") == 0)
    {
        struct stat mstat;
        memset(&mstat, 0, sizeof(mstat));
        if (lstat(filename, &mstat) == -1)
        {
            __ERR("get file stat error: %s\n", filename);
        }
        else
        {
            module_uninit_single("anj_ispctl");
            usleep(100 * 1000);
            module_init_single("anj_ispctl");
        }
    }
    else if (strcmp(szFileName, "aiisp.bin") == 0)
    {
        struct stat mstat;
        memset(&mstat, 0, sizeof(mstat));
        if (lstat(filename, &mstat) == -1)
        {
            __ERR("get file stat error: %s\n", filename);
        }
        else
        {
            module_uninit_single("anj_ispctl");
            usleep(100 * 1000);
            module_init_single("anj_ispctl");
        }
    }
    else if (strcmp(szFileName, ".flag") == 0 &&
             strcmp(szFileName, "openslog") == 0)
    {
        anj_mw_create_file("/tmp/flag.open.slog.tcp", NULL);
    }
    else if (strcmp(szFileName, ".flag") == 0 &&
             strcmp(szFileName, "set_fix_dev_config") == 0)
    {
        anj_mw_create_file("/tmp/fix_dev_config.flag", NULL);
        anj_mw_create_file("/mnt/nand/fix_dev_config.flag", NULL);
    }
    else if (strcmp(szFileName, ".flag") == 0 &&
             strcmp(szFileName, "clear_fix_dev_config") == 0)
    {
        remove("/tmp/fix_dev_config.flag");
        remove("/mnt/nand/fix_dev_config.flag");
    }
    else if (strcmp(szFileName, ".flag") == 0 &&
             strcmp(szFileName, "set_disable_notacp_config") == 0)
    {
        anj_mw_create_file("/tmp/disable_notacp_config.flag", NULL);
        anj_mw_create_file("/mnt/nand/disable_notacp_config.flag", NULL);
    }
    else if (strcmp(szFileName, ".flag") == 0 &&
             strcmp(szFileName, "clear_disable_notacp_config") == 0)
    {
        remove("/tmp/disable_notacp_config.flag");
        remove("/mnt/nand/disable_notacp_config.flag");
    }
    else if (strcmp(szFileName, "https.crt") == 0)
    {
        anj_mw_file_copy(filename, "/mnt/nand/https.crt");
        // todo
        // anj_mw_system_with_param("killall -9 %s", "web_server");
    }
    else if (strcmp(szFileName, "https.key") == 0)
    {
        anj_mw_file_copy(filename, "/mnt/nand/https.key");
        // anj_mw_system_with_param("killall -9 %s", "web_server");
    }
    else if (strcmp(szFileName, ".cfg") == 0 &&
             strcmp(szFileName, "ptzstep") == 0)
    {
        anj_mw_file_copy(filename, "/mnt/nand/ptzstep.cfg");
        module_uninit_single("anj_ptz");
        usleep(10 * 1000);
        module_init_single("anj_ptz");
    }
    else if (strcmp(szFileName, ".tgz") == 0 &&
             strcmp(szFileName, "multi_ldc_bin") == 0)
    {
        remove("/mnt/nand/multi_ldc_bin.7z");
        remove("/mnt/nand/multi_ldc_bin.tgz");
        anj_mw_file_copy(filename, "/mnt/nand/multi_ldc_bin.tgz");
        anj_mw_create_file("/tmp/reload_stitch_param", NULL);
    }
    else if (strcmp(szFileName, ".7z") == 0 &&
             strcmp(szFileName, "multi_ldc_bin") == 0)
    {
        remove("/mnt/nand/multi_ldc_bin.7z");
        remove("/mnt/nand/multi_ldc_bin.tgz");
        anj_mw_file_copy(filename, "/mnt/nand/multi_ldc_bin.7z");
        anj_mw_create_file("/tmp/reload_stitch_param", NULL);
    }
    else if (strcmp(szFileName, ".bin") == 0 &&
             strcmp(szFileName, "ldc_bin_new") == 0)
    {
        remove("/mnt/nand/ldc_bin_new.bin");
        anj_mw_file_copy(filename, "/mnt/nand/ldc_bin_new.bin");
        anj_mw_create_file("/tmp/reload_stitch_param", NULL);
    }
    else if (strcmp(szFileName, ".7z") == 0 &&
             strcmp(szFileName, "ldc_config_file") == 0)
    {
        remove("/mnt/nand/ldc_config_file.7z");
        anj_mw_file_copy(filename, "/mnt/nand/ldc_config_file.7z");
        anj_mw_create_file("/tmp/reload_stitch_param", NULL);
    }
    else if (strcmp(szFileName, ".png") == 0 &&
             strcmp(szFileName, "login_logo_big") == 0)
    {
        if (!anj_mw_file_exists("/mnt/nand/cust"))
        {
            mkdir("/mnt/nand/cust", 0777);
        }
        anj_mw_file_copy(filename, "/mnt/nand/cust/login_logo_big.png");
        remove("/tmp/login_logo_big.png");
    }
    else if (strcmp(szFileName, ".png") == 0 &&
             strcmp(szFileName, "login_logo") == 0)
    {
        if (!anj_mw_file_exists("/mnt/nand/cust"))
        {
            mkdir("/mnt/nand/cust", 0777);
        }
        anj_mw_file_copy(filename, "/mnt/nand/cust/login_logo.png");
        remove("/tmp/login_logo.png");
    }
    else if (strcmp(szFileName, ".png") == 0 &&
             strcmp(szFileName, "login_title") == 0)
    {
        if (!anj_mw_file_exists("/mnt/nand/cust"))
        {
            mkdir("/mnt/nand/cust", 0777);
        }
        anj_mw_file_copy(filename, "/mnt/nand/cust/login_title.png");
        remove("/tmp/login_title.png");
    }
    else if (strcmp(szFileName, ".png") == 0 &&
             strcmp(szFileName, "main_logo") == 0)
    {
        if (!anj_mw_file_exists("/mnt/nand/cust"))
        {
            mkdir("/mnt/nand/cust", 0777);
        }
        anj_mw_file_copy(filename, "/mnt/nand/cust/main_logo.png");
        remove("/tmp/main_logo.png");
    }

    if (bAiot > 0)
    {
        anj_sysmng_p2p_update(filename, TYPE_ID_TYPE_AIOT, AIOT_P2PID_FILE_NAME);
    }
    else
    {
        anj_mw_system_with_param("chmod +x %s", filename);
    }

    return 0;
}

int anj_sysmng_get_totalmem(void)
{
    int getdata = 0;

    char szProcFile[64] = {0};
    sprintf(szProcFile, "/proc/meminfo");

    char buff[1024] = "";
    int iRet;
    FILE *fp = anj_mw_fopen(szProcFile, "rb");
    if (fp)
    {
        while ((iRet = anj_mw_fread(fp, buff, 1023)) > 0)
        {
            buff[iRet] = 0;
            char *pSize = NULL;

            pSize = strstr(buff, "MemTotal:");
            if (pSize)
            {
                pSize += strlen("MemTotal:");

                while (pSize)
                {
                    if (!isspace(*pSize))
                        break;

                    pSize++;
                }

                getdata = atoi(pSize);
            }
        }

        anj_mw_fclose(fp);
    }

    return getdata;
}

int anj_sysmng_get_availablemem(void)
{
    int getdata = 0;
    char szProcFile[64] = {0};
    sprintf(szProcFile, "/proc/meminfo");

    char buff[1024] = "";
    int iRet;
    FILE *fp = anj_mw_fopen(szProcFile, "rb");
    if (fp)
    {
        while ((iRet = anj_mw_fread(fp, buff, 1023)) > 0)
        {
            buff[iRet] = 0;
            char *pSize = NULL;

            pSize = strstr(buff, "MemAvailable:");
            if (pSize)
            {
                pSize += strlen("MemAvailable:");

                while (pSize)
                {
                    if (!isspace(*pSize))
                        break;

                    pSize++;
                }

                getdata = atoi(pSize);
            }
        }

        anj_mw_fclose(fp);
    }

    return getdata;
}

int anj_sysmng_get_freemem(void)
{
    int getdata = 0;
    char szProcFile[64] = {0};
    sprintf(szProcFile, "/proc/meminfo");

    char buff[1024] = "";
    int iRet;
    FILE *fp = anj_mw_fopen(szProcFile, "rb");
    if (fp)
    {
        while ((iRet = anj_mw_fread(fp, buff, 1023)) > 0)
        {
            buff[iRet] = 0;
            char *pSize = NULL;

            pSize = strstr(buff, "MemFree:");
            if (pSize)
            {
                pSize += strlen("MemFree:");

                while (pSize)
                {
                    if (!isspace(*pSize))
                        break;

                    pSize++;
                }

                getdata = atoi(pSize);
            }
        }

        anj_mw_fclose(fp);
    }

    return getdata;
}

const char *anj_sysmng_cpu_info_update(void)
{
    static const char szCpuFmt[] = "cpu %llu %llu %llu %llu %llu %llu %llu %llu";
    char line_buf[256] = {0};
    FILE *fd = NULL;
    anj_sysmng_jiffy_counts_t *p_jif = &s_cpu_data_stat1;
    anj_sysmng_jiffy_counts_t *p_prev_jif = &s_cpu_data_stat0;
    unsigned total_diff = 0;
    int bCal = 0;
    int ret = 0;
    int nMemFree = 0;
    int nMemAvailable = 0;
    int len = 0;

    fd = fopen("/proc/stat", "r");
    if (fd == NULL)
    {
        return s_cpu_info_str;
    }

    fgets(line_buf, sizeof(line_buf), fd);

    if (p_prev_jif->busy == 0 && p_prev_jif->idle == 0 && p_prev_jif->total == 0)
    {
        p_jif = p_prev_jif;
        bCal = 0;
    }
    else
    {
        bCal = 1;
    }

    ret = sscanf(line_buf, szCpuFmt,
                 &p_jif->usr, &p_jif->nic, &p_jif->sys, &p_jif->idle,
                 &p_jif->iowait, &p_jif->irq, &p_jif->softirq,
                 &p_jif->steal);
    if (ret >= 4)
    {
        p_jif->total = p_jif->usr + p_jif->nic + p_jif->sys + p_jif->idle +
                       p_jif->iowait + p_jif->irq + p_jif->softirq + p_jif->steal;
        p_jif->busy = p_jif->total - p_jif->idle - p_jif->iowait;
    }
    else if (bCal > 0)
    {
        bCal = 0;
    }

    fclose(fd);

    if (bCal <= 0)
    {
        return s_cpu_info_str;
    }

    ANJ_SYSMNG_CALC_TOTAL_DIFF;

    ANJ_SYSMNG_CALC_STAT(busy);
    ANJ_SYSMNG_CALC_STAT(usr);
    ANJ_SYSMNG_CALC_STAT(sys);
    ANJ_SYSMNG_CALC_STAT(nic);
    ANJ_SYSMNG_CALC_STAT(idle);
    ANJ_SYSMNG_CALC_STAT(iowait);
    ANJ_SYSMNG_CALC_STAT(irq);
    ANJ_SYSMNG_CALC_STAT(softirq);

    snprintf(s_cpu_info_str, sizeof(s_cpu_info_str),
             "busy:%.1f%% idle:%.1f%% usr:%.1f%% sys:%.1f%%",
             busy, idle, usr, sys);

    len = (int)strlen(s_cpu_info_str);
    snprintf(s_cpu_info_str + len, sizeof(s_cpu_info_str) - len,
             " ^nic:%.1f%% io:%.1f%% irq:%.1f%% sirq:%.1f%%",
             nic, iowait, irq, softirq);

    nMemFree = anj_sysmng_get_freemem();
    nMemAvailable = anj_sysmng_get_availablemem();
    len = (int)strlen(s_cpu_info_str);
    snprintf(s_cpu_info_str + len, sizeof(s_cpu_info_str) - len,
             " ^Free: %d kB (%d MB) Avail: %d kB (%d MB)",
             nMemFree, nMemFree / 1024,
             nMemAvailable, nMemAvailable / 1024);

    memcpy(p_prev_jif, p_jif, sizeof(anj_sysmng_jiffy_counts_t));
    return s_cpu_info_str;
}

int anj_sysmng_is_limit_ip(unsigned int remoteip)
{
    SystemConfig *pstSystemConfig = (SystemConfig *)getSystemConfig();
    SysAlowIpConfig data = pstSystemConfig->alowipCfg;

    if (data.enable == 0)
        return 0;

    // 传入参数为0,都是限制IP
    if (remoteip == 0)
        return 1;

    unsigned int nAdminIpAddr = 0xbcbcbcbc; // 188.188.188.188
    if (nAdminIpAddr == remoteip)
        return 0;

    int bAlowd = 0;
    int iIndex = 0;
    for (iIndex = 0; iIndex < MAX_ALOW_IP_NUM; iIndex++)
    {
        if ((data.nAllowIp[iIndex] > 0) && (remoteip == data.nAllowIp[iIndex]))
        {
            bAlowd = 1;
            break;
        }
    }

    if (bAlowd == 0)
        return 1;

    return 0;
}

void anj_sysmng_videolist_get(char *retBuf, int size)
{
    char *pe = retBuf + size - 1;
    char *pb = retBuf;

    RESOLUTION_ENTRY video_list[] = {RESOLUTION_LIST};
    int index = 0;
    while (strlen(video_list[index].res_name) > 0)
    {
        pb += snprintf(pb, pe - pb, "%s,", video_list[index].res_name);
        pb += snprintf(pb, pe - pb, "%s,", video_list[index].codec_name);
        pb += snprintf(pb, pe - pb, "%d,", video_list[index].stream_type);

        pb += snprintf(pb, pe - pb, "%d,", video_list[index].def_bitrate);
        pb += snprintf(pb, pe - pb, "%d,", video_list[index].min_bitrate);
        pb += snprintf(pb, pe - pb, "%d,", video_list[index].max_bitrate);
        pb += snprintf(pb, pe - pb, "%d,", video_list[index].def_framerate);
        pb += snprintf(pb, pe - pb, "%d,", video_list[index].min_framerate);
        pb += snprintf(pb, pe - pb, "%d,", video_list[index].max_display_framerate);

        pb += snprintf(pb, pe - pb, "%d,", video_list[index].dual_stream);
        pb += snprintf(pb, pe - pb, "%d;", video_list[index].def_config);

        index++;
    }

    __INFO("GetVideoResList, len = %d, str= %s\n", strlen(retBuf), retBuf);
}

void anj_sysmng_audiolist_get(char *retBuf, int size)
{
    char *pe = retBuf + size - 1;
    char *pb = retBuf;

    AUDIO_CODEC_ENTRY audio_list[] = {AUDIO_CODEC_LIST};
    int index = 0;
    while (strlen(audio_list[index].codec_name) > 0)
    {
        pb += snprintf(pb, pe - pb, "%s,", audio_list[index].codec_name);
        pb += snprintf(pb, pe - pb, "%d,", audio_list[index].channels);
        pb += snprintf(pb, pe - pb, "%d,", audio_list[index].bitspersample);
        pb += snprintf(pb, pe - pb, "%d,", audio_list[index].samplerate);
        pb += snprintf(pb, pe - pb, "%d,", audio_list[index].bitrate);
        pb += snprintf(pb, pe - pb, "%d;", audio_list[index].def_config);

        index++;
    }
    __INFO("GetAudioCodecList, str= %s\n", retBuf);
}

void anj_sysmng_yuvlist_get(char *retBuf, int size)
{
    char *pe = retBuf + size - 1;
    char *pb = retBuf;

    YUV_ENTRY yuv_list[] = {YUV_LIST};
    int index = 0;
    while (strlen(yuv_list[index].res_name) > 0)
    {
        pb += snprintf(pb, pe - pb, "%s,", yuv_list[index].res_name);
        pb += snprintf(pb, pe - pb, "%d,", yuv_list[index].format);
        pb += snprintf(pb, pe - pb, "%d,", yuv_list[index].def_framerate);
        pb += snprintf(pb, pe - pb, "%d,", yuv_list[index].min_framerate);
        pb += snprintf(pb, pe - pb, "%d,", yuv_list[index].max_framerate);
        pb += snprintf(pb, pe - pb, "%d;", yuv_list[index].def_config);
        index++;
    }
    __INFO("GetYuvResList, len = %d, str= %s\n", strlen(retBuf), retBuf);
}

void anj_sysmng_max_res_get(int chn, int *pWidth, int *pHeight)
{
    int iIndex = 0;
    RESOLUTION_ENTRY video_list[] = {RESOLUTION_LIST};
    if (NULL == pWidth || NULL == pHeight)
    {
        return;
    }

    while (strlen(video_list[iIndex].res_name) > 0)
    {
        if (video_list[iIndex].stream_type != chn)
        {
            iIndex++;
            continue;
        }

        unsigned int width = 0;
        unsigned int height = 0;
        GetVideoSize(video_list[iIndex].res_name, 1, (int *)&width, (int *)&height);
        if ((width * height) > ((*pWidth) * (*pHeight)))
        {
            *pWidth = width;
            *pHeight = height;
        }
        iIndex++;
    }
    __INFO("InitMaxVencSize[%d] m_max_encSize [%u %u]", chn, *pWidth, *pHeight);
}

int anj_sysmng_video_res_array_get(RESOLUTION_ENTRY **pEntry)
{
    int count = 0;
    RESOLUTION_ENTRY *pEntryTmp = NULL;

    pEntryTmp = video_res_entry_get(1);
    if (NULL == pEntryTmp)
    {
        return 0;
    }

    *pEntry = pEntryTmp;
    count = 0;
    while (strlen(pEntryTmp[count].res_name) > 0)
    {
        count++;
    }

    return count;
}

int anj_sysmng_audio_res_array_get(AUDIO_CODEC_ENTRY **pEntry)
{
    int count = 0;
    AUDIO_CODEC_ENTRY *pDstEntry = NULL;
    AUDIO_CODEC_ENTRY audioCodecList[] = {AUDIO_CODEC_LIST};

    if (pEntry == NULL)
    {
        return 0;
    }

    while (strlen(audioCodecList[count].codec_name) > 0)
    {
        count++;
    }

    // 这里的count不包括宏定义中音频能力的最后一行：{"", 0, 0, 0, 0, 0}
    if (count == 0)
    {
        *pEntry = NULL;
        return 0;
    }

    pDstEntry = (AUDIO_CODEC_ENTRY *)anj_mw_malloc((count) * sizeof(AUDIO_CODEC_ENTRY));
    if (NULL == pDstEntry)
    {
        *pEntry = NULL;
        return 0;
    }

    memcpy(pDstEntry, audioCodecList, (count) * sizeof(AUDIO_CODEC_ENTRY));
    *pEntry = pDstEntry;
    return count;
}

void anj_sysmng_restore_netconfig_set(int flag)
{
    s_RestoreSaveNetCfg = flag;
}

void anj_sysmng_factory_config_restore(GlobalConfig *pstGlobalConfig)
{
    int iRet = 0;
    FactoryDefaultCfg stFactoryCfg = {0};
    stFactoryCfg.LedMode = pstGlobalConfig->mediaCfg.videoConfig[0].videoCapture.led_mode;
    stFactoryCfg.IrCutMode = pstGlobalConfig->mediaCfg.videoConfig[0].videoCapture.ircut_mode;

    char szDefaultConfigFileName[64] = {0};
    char szDefaultConfigFileName2[32] = {0};
    char szSrcFile[128] = {0};

    snprintf(szDefaultConfigFileName, sizeof(szDefaultConfigFileName), "config.default.%s.xml", ANJ_PROJECT_NAME);
    toLowerStr(szDefaultConfigFileName);
    snprintf(szDefaultConfigFileName2, sizeof(szDefaultConfigFileName2), "config.default.xml");

    snprintf(szSrcFile, sizeof(szSrcFile), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, szDefaultConfigFileName);
    if (anj_mw_file_exists(szSrcFile))
    {
        __ERR("use cust default cfg, not use factory cfg\n");
        return;
    }

    memset(szSrcFile, 0, sizeof(szSrcFile));
    snprintf(szSrcFile, sizeof(szSrcFile), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, szDefaultConfigFileName2);
    if (anj_mw_file_exists(szSrcFile))
    {
        __ERR("use cust default cfg, not use factory cfg\n");
        return;
    }

    iRet = anj_factory_defcfg_load(&stFactoryCfg);
    if (iRet == 0)
    {
        __INFO("factory cfg ircut:%d, led:%d! device ircut:%d, led:%d\n",
               stFactoryCfg.IrCutMode, stFactoryCfg.LedMode,
               pstGlobalConfig->mediaCfg.videoConfig[0].videoCapture.ircut_mode,
               pstGlobalConfig->mediaCfg.videoConfig[0].videoCapture.led_mode);

        pstGlobalConfig->mediaCfg.videoConfig[0].videoCapture.ircut_mode = stFactoryCfg.IrCutMode;
        pstGlobalConfig->mediaCfg.videoConfig[0].videoCapture.led_mode = stFactoryCfg.LedMode;
    }
}

void anj_sysmng_cust_language_restore(GlobalConfig *pstGlobalConfig)
{
    AjOemStruct oemInfo = {0};
    anj_config_oem_get(&oemInfo);
    if (strlen(oemInfo.szOemLanguage) > 0)
    {
        strcpy(pstGlobalConfig->systemCfg.miscCfg.language, oemInfo.szOemLanguage);
    }
}

int anj_sysmng_config_restore(unsigned int reserved_bits)
{
    int bSaveConfigForce = 0;
    static unsigned long long stRestoreTime = 0;
    if (stRestoreTime && (anj_mw_get_cputime_ms(NULL) - stRestoreTime) < 30000)
    {
        __ERR("restore config now...\n");
        return 0;
    }
    stRestoreTime = anj_mw_get_cputime_ms(NULL);

    module_uninit_single("anj_record");
    __ERR("reserved_bits=%u\n", reserved_bits);

    remove("/mnt/nand/flag.yensdm");
    remove("/mnt/nand/flag.yensmd");
    if (anj_config_copy_default() != 0)
    {
        __ERR("copy default config fail\n");
        return -1;
    }

    if (anj_mw_file_exists(OEM_SECOND_CONFIG_PATH))
    {
        anj_mw_write_file(OEM_SECOND_RESTORE_FLAG_PATH, 0, "1", 1);
    }

    GlobalConfig *pstGlobalConfig = (GlobalConfig *)getGlbConfig();
    GlobalConfig stGlobalConfig = *pstGlobalConfig;

    if (anj_config_load(NULL, pstGlobalConfig, CONFIG_FILE_PATH) != 0)
    {
        __ERR("get config faill\r\n");
        return -1;
    }

    // 保留之前的MAC
    memcpy(pstGlobalConfig->networkCfgNew.lanCfg.MACAddress, stGlobalConfig.networkCfgNew.lanCfg.MACAddress, MAC_ADDRESS_LEN);
    bSaveConfigForce = 1;

    // 保留之前的IP地址和端口号等信息
    if (s_RestoreSaveNetCfg)
    {
        __INFO("restore config but save network info\n");
        memcpy(&pstGlobalConfig->networkCfgNew, &stGlobalConfig.networkCfgNew, sizeof(NetworkConfigNew));
        anj_config_network_save(&pstGlobalConfig->networkCfgNew);

        memcpy(&pstGlobalConfig->mediaStreamCfg, &stGlobalConfig.mediaStreamCfg, sizeof(MediaStreamConfig));
        anj_config_stream_save(&pstGlobalConfig->mediaStreamCfg);

        bSaveConfigForce = 1;
    }

    int remove_db = 1;
    if (reserved_bits > 0 && BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_NETWORK))
    {
        remove_db = 0;
    }

    if (remove_db)
    {
        remove(P2P_ID_NETCONFIGED_ALI);
        remove(P2P_ID_NETCONFIGED_AIOT);
    }

    remove(CONFIG_PTZ_PATH);
    remove(WPA_SUPPLICANT_CONF_PATH);
    remove("/mnt/nand/multiple_ext.txt");
    remove("/mnt/nand/power_save_default");
    remove("/mnt/nand/power_save_disable");
    // 腾讯云
    remove("/mnt/nand/TencentPreset.cfg");

    anj_sysmng_factory_config_restore(pstGlobalConfig);
    anj_sysmng_cust_language_restore(pstGlobalConfig);
    
    if (reserved_bits > 0)
    {
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_NETWORK))
        {
            __ERR("Reserver network\n");
            memcpy(&(pstGlobalConfig->networkCfgNew), &(stGlobalConfig.networkCfgNew), sizeof(stGlobalConfig.networkCfgNew));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_LANGUAGE))
        {
            __ERR("Reserver language\n");
            memcpy(&(pstGlobalConfig->systemCfg.miscCfg), &(stGlobalConfig.systemCfg.miscCfg), sizeof(stGlobalConfig.systemCfg.miscCfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_TIME))
        {
            __ERR("Reserver time\n");
            memcpy(&(pstGlobalConfig->systemCfg.timeCfg), &(stGlobalConfig.systemCfg.timeCfg), sizeof(stGlobalConfig.systemCfg.timeCfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_USER))
        {
            __ERR("Reserver usercfg\n");
            memcpy(&(pstGlobalConfig->systemCfg.userCfg), &(stGlobalConfig.systemCfg.userCfg), sizeof(stGlobalConfig.systemCfg.userCfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_MEDIACODE))
        {
            __ERR("Reserver media config\n");
            memcpy(&(pstGlobalConfig->mediaCfg), &(stGlobalConfig.mediaCfg), sizeof(stGlobalConfig.mediaCfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_PTZ))
        {
            __ERR("Reserver ptz\n");
            memcpy(&(pstGlobalConfig->systemCfg.ptzCfg), &(stGlobalConfig.systemCfg.ptzCfg), sizeof(stGlobalConfig.systemCfg.ptzCfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_STREAMACCESS))
        {
            __ERR("Reserver stream access\n");
            memcpy(&(pstGlobalConfig->mediaStreamCfg), &(stGlobalConfig.mediaStreamCfg), sizeof(stGlobalConfig.mediaStreamCfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_RECORD))
        {
            __ERR("Reserver record config\n");
            memcpy(&(pstGlobalConfig->recordCfg), &(stGlobalConfig.recordCfg), sizeof(stGlobalConfig.recordCfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_GB28181))
        {
            __ERR("Reserver gb28181 config\n");
            memcpy(&(pstGlobalConfig->gb28181Cfg), &(stGlobalConfig.gb28181Cfg), sizeof(stGlobalConfig.gb28181Cfg));
        }
        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_ALARM))
        {
            __ERR("Reserver alarm and server config\n");
            memcpy(&(pstGlobalConfig->alarmCfg), &(stGlobalConfig.alarmCfg), sizeof(stGlobalConfig.alarmCfg));
            memcpy(&(pstGlobalConfig->serverCfg), &(stGlobalConfig.serverCfg), sizeof(stGlobalConfig.serverCfg));
        }

        if (BIT_GET_32(reserved_bits, CONFIG_RESTORE_RESERVE_BIT_TITLE))
        {
            __ERR("Reserver title config\n");
            for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                memcpy(&(pstGlobalConfig->mediaCfg.videoConfig[cameraIndex].overlay),
                       &(stGlobalConfig.mediaCfg.videoConfig[cameraIndex].overlay),
                       sizeof(stGlobalConfig.mediaCfg.videoConfig[cameraIndex].overlay));
            }
        }

        bSaveConfigForce = 1;
    }

    __INFO("config file save:%d\n", bSaveConfigForce);
    if (bSaveConfigForce > 0)
    {
        anj_config_save_all(pstGlobalConfig, CONFIG_FILE_PATH);
    }

    remove("/mnt/nand/force_ircut_manual.flag");
    remove("/mnt/nand/wire_dhcp_netinfo");

    __WARN("restore config, reboot\n");
    __RECORD_LOG_INFO("restore config, reboot\n");
    anj_alarm_event_handle(0, ALARM_CODE_RESET_TO_FACTORY, ALARM_FLAG_OCCUR,
                           ALARM_LEVEL_EVENT, 1, "restore to factory configuration", NULL);
    anj_sysmng_delay_reboot(1);
    return 0;
}

static int anj_sysmng_restore_thread(void *ctx, int *bStart)
{
    int sec = *(int *)ctx;
    if (sec < 0 || sec > 30)
        sec = 10;
    anj_mw_free(ctx);

    sleep(sec);

    __INFO("restore device delay %d sec end\n", sec);
    unsigned int reserved_bits = 0;
    __RECORD_LOG_INFO("restore config reserved_bits=%u\n", reserved_bits);
    anj_sysmng_config_restore(reserved_bits);
    return 0;
}

void anj_sysmng_delay_restore(int sec)
{
    int *arg = (int *)anj_mw_malloc(sizeof(int));
    *arg = sec;
    static anj_thread_s thd = {0};
    thd.bAutoDestroy = 1;
    strncpy(thd.iThreadName, "delay_restore_thread", sizeof(thd.iThreadName) - 1);
    thd.iThreadjob.ctx = arg;
    thd.iThreadjob.func = anj_sysmng_restore_thread;
    if (0 != anj_thread_task_create(&thd))
    {
        __ERR("delay_restore_thread create failed\n");
        anj_mw_free(arg);
    }
}

int anj_sysmng_eraseall_mp3()
{
    __INFO("device earse all mp3, start to reboot system...\n");

    anj_mw_system("cp -R /mnt/nand /tmp/");
    anj_mw_system("rm -f /tmp/nand/*.mp3");
    anj_mw_system("rm -fr /tmp/nand/mp3");
    anj_mw_system("fuser -k /mnt/nand/");

    sleep(2);

    if (!SUPPORT_NAND_FLASH)
    {
        anj_mw_system_with_param("umount -fl %s", DATA_BLOCK_MOUNT_PATH);
        anj_mw_system_with_param("/bin/flash_eraseall -j %s", GET_DATA_BLOCK1_DEV());

        anj_mw_system_with_param("mount -t jffs2 %s %s", GET_DATA_BLOCK1(), DATA_BLOCK_MOUNT_PATH);
    }
    anj_mw_system_with_param("cp -R /tmp/nand/* %s/", DATA_BLOCK_MOUNT_PATH);

    __WARN("erase all mp3, reboot\n");
    __RECORD_LOG_INFO("erase all mp3, reboot\n");
    anj_sysmng_reboot();
    return 0;
}

/*
ANJ_PARTNER_FILE_NAME:
<PARTNERROOT>
    <DATA partner="anjvision" datestr="20230620" macaddr="F0:00:00:00:00:F1" sn="EF000000000000F1" uuid="195383531912673" />
</PARTNERROOT>
*/
void anj_sysmng_partner_info_get(char *szPartner, char *szDatestr, char *szMacaddr)
{
    if (NULL == szPartner || szDatestr == NULL || szMacaddr == NULL)
        return;

    *szPartner = 0;
    *szDatestr = 0;
    *szMacaddr = 0;

    int bNeedBackup = 0; // backup orignal file
    if (!anj_mw_file_exists(ANJ_PARTNER_FILE_NAME))
        return;

    unsigned long long buflen = 0;
    anj_mw_read_file_len(ANJ_PARTNER_FILE_NAME, &buflen);
    buflen += 4;
    char *buffer = (char *)anj_mw_malloc(buflen);
    if (buffer == NULL)
        return;

    memset(buffer, 0, buflen);

    int redlen = anj_mw_read_file_limit_len(ANJ_PARTNER_FILE_NAME, buffer, buflen);
    if (redlen <= 0)
    {
        anj_mw_free(buffer);
        return;
    }

    char szMySN[32] = {0};
    const char *pUUID = gstDevInfo.uuid;
    anj_sysmng_load_sn(szMySN, sizeof(szMySN));

    IXML_Document *pDoc = NULL;
    IXML_NodeList *pNodeRoot = NULL;
    pDoc = ixmlParseBuffer(buffer);
    if (pDoc == NULL)
    {
        __ERR("ixmlParseBuffer failed, xml=\n%s\n", buffer);
        bNeedBackup = 1;
        goto EXIT;
    }

    pNodeRoot = ixmlDocument_getElementsByTagName(pDoc, "PARTNERROOT");
    if (pNodeRoot == NULL)
    {
        bNeedBackup = 1;
        goto EXIT;
    }
    else
    {
        IXML_Node *pNodeChild = NULL;
        pNodeChild = pNodeRoot->nodeItem->firstChild;
        if (0 == strcmp(pNodeChild->nodeName, "DATA"))
        {
            IXML_Node *tmpAttr;
            char szPartnerSN[128] = {0};
            char szPartenerUUID[128] = {0};

            tmpAttr = pNodeChild->firstAttr; // 先比较UUID与SN
            while (tmpAttr)
            {
                if (0 == strcmp(tmpAttr->nodeName, "uuid"))
                {
                    if (tmpAttr->nodeValue != NULL)
                        strcpy(szPartenerUUID, tmpAttr->nodeValue);
                }
                else if (0 == strcmp(tmpAttr->nodeName, "sn"))
                {
                    if (tmpAttr->nodeValue != NULL)
                        strcpy(szPartnerSN, tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (strcmp(szPartenerUUID, pUUID) != 0)
            {
                __ERR("uuid check failed: %s <-> %s\n", szPartenerUUID, pUUID);
                bNeedBackup = 1;
                goto EXIT;
            }
            if (strcmp(szPartnerSN, szMySN) != 0)
            {
                __ERR("SN check failed: %s <-> %s\n", szPartnerSN, szMySN);
                bNeedBackup = 1;
                goto EXIT;
            }

            tmpAttr = pNodeChild->firstAttr;
            while (tmpAttr)
            {
                if (0 == strcmp(tmpAttr->nodeName, "partner"))
                {
                    if (tmpAttr->nodeValue != NULL)
                        strcpy(szPartner, tmpAttr->nodeValue);
                }
                else if (0 == strcmp(tmpAttr->nodeName, "datestr"))
                {
                    if (tmpAttr->nodeValue != NULL)
                        strcpy(szDatestr, tmpAttr->nodeValue);
                }
                else if (0 == strcmp(tmpAttr->nodeName, "macaddr"))
                {
                    if (tmpAttr->nodeValue != NULL)
                        strcpy(szMacaddr, tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }
        }
    }

EXIT:
    if (buffer != NULL)
        free(buffer);
    if (NULL != pNodeRoot)
        ixmlNodeList_free(pNodeRoot);
    if (NULL != (pDoc))
        ixmlDocument_free(pDoc);

    if (bNeedBackup)
    {
        char szDestBkFile[256];
        struct tm *t, tbuf;
        time_t tsec = time(0);
        t = localtime_r(&tsec, &tbuf);

        sprintf(szDestBkFile, "%s_bk%04d-%02d%02d%02d%02d%02d",
                ANJ_PARTNER_FILE_NAME,
                2000 + t->tm_year - 100, t->tm_mon + 1,
                t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

        rename(ANJ_PARTNER_FILE_NAME, szDestBkFile);
    }

    return;
}

int anj_sysmng_partner_info_set(const char *partner, const char *datestr, const char *macaddr)
{
    char szMySN[32] = {0};
    const char *pUUID = gstDevInfo.uuid;
    anj_sysmng_load_sn(szMySN, sizeof(szMySN));

    char *pBuf = NULL;
    int initSize = 256;
    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<PARTNERROOT>\r\n");
    pb += snprintf(pb, pe - pb, "<DATA partner=\"%s\" datestr=\"%s\" macaddr=\"%s\" sn=\"%s\" uuid=\"%s\" />\r\n",
                   partner, datestr, macaddr, szMySN, pUUID);
    pb += snprintf(pb, pe - pb, "</PARTNERROOT>");

    anj_mw_write_file(ANJ_PARTNER_FILE_NAME, 0, pBuf, strlen(pBuf));
    anj_mw_free(pBuf);

    return 0;
}

void anj_sysmng_viewer_add()
{
    anj_mutex_lock(&s_stViewerMutex);
    s_stViewerNum++;
    anj_mutex_unlock(&s_stViewerMutex);
}

void anj_sysmng_viewer_del()
{
    anj_mutex_lock(&s_stViewerMutex);
    s_stViewerNum--;
    if (s_stViewerNum < 0)
    {
        s_stViewerNum = 0;
    }
    anj_mutex_unlock(&s_stViewerMutex);
}

int anj_sysmng_viewer_get()
{
    return s_stViewerNum;
}

DevInfo *getDevInfo(void)
{
    return &gstDevInfo;
}

static int anj_sysmng_uboot_version_init()
{
    AjOemStruct devOemInfo = {0};
    char szDeviceType[32] = {0};

    if (anj_sysmng_dev_str_get(szDeviceType) < 0)
    {
        __ERR("Unknown device type.\n");
        return 0;
    }

    soft_enc_uboot_anlyargs("version_kernel", SDK_KERNEL_IDENTITY);

    anj_config_oem_get(&devOemInfo);
    __INFO("devOemInfo szDeviceType:%s, szVersion:%s \n", devOemInfo.szDeviceType, devOemInfo.szVersion);

    char szNeedStr[64] = {0};
    sprintf(szNeedStr, "%s_V%s", szDeviceType, anj_sysmng_product_version_get());
    soft_enc_uboot_anlyargs("version_rootfs", szNeedStr);

    char fsVersion[128] = {0};
    char version[32] = {0};
    char builddate[32] = {0};

    anj_mw_read_file_limit_len("/etc/filesys.ver", fsVersion, sizeof(fsVersion));

    const char *flag1 = " V";
    const char *flag2 = " build ";
    char *p1 = strstr(fsVersion, flag1);
    char *p2 = strstr(fsVersion, flag2);

    if (p1 != NULL && p2 != NULL)
    {
        char *pStart = p1 + strlen(flag1);
        char *pEnd = strstr(pStart, " ");
        *pEnd = 0;

        strncpy(version, pStart, sizeof(version) - 1);

        pEnd += strlen(flag2);
        strncpy(builddate, pEnd, sizeof(builddate) - 1);

        string_trim_head(version);
        string_trim_tail(version);
        string_trim_head(builddate);
        string_trim_tail(builddate);
        string_remove(builddate, '-');
        string_remove(builddate, ' ');
        string_remove(builddate, ':');
        builddate[12] = 0; // 只保留到分钟

        __INFO("version: %s, build date: %s\n", version, builddate);
        soft_enc_uboot_anlyargs("fsversion", version);
        soft_enc_uboot_anlyargs("builddate", builddate);

        return 0;
    }
    return -1;
}

static int anj_sysmng_reset_thread(void *ctx, int *bStart)
{
    int bReseted = 0;                   /*复位状态*/
    unsigned long long stResetTime = 0; /*摁住复位键持续时间*/
    int stChangeCnt = 0;                /*摁住复位和松开复位键的次数*/
    unsigned long long stLastTime = 0;  /*上次复位摁住松开的时间*/

    while (bStart && *bStart)
    {
        if (anj_mw_file_exists(CLEAR_SN_FILE))
        {
            remove(CLEAR_SN_FILE);
            soft_enc_clear();
        }

        if (anj_mw_file_exists(SHOW_SN_FILE))
        {
            remove(SHOW_SN_FILE);
            soft_enc_show();
        }

        SystemConfig *pstSystemCfg = getSystemConfig();
        MaintainConfig *pstMaintainCfg = &pstSystemCfg->maintainCfg;
        if (pstMaintainCfg->enable)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            struct tm *ptm = localtime(&tv.tv_sec);
            if (ptm != NULL)
            {
                int weekDay = (ptm->tm_wday == 0) ? 6 : (ptm->tm_wday - 1);
                int bDayMatch = (pstMaintainCfg->day == 7 || pstMaintainCfg->day == weekDay);
                int bTimeMatch = (ptm->tm_hour == pstMaintainCfg->time.hour &&
                                  ptm->tm_min == pstMaintainCfg->time.minute &&
                                  ptm->tm_sec == pstMaintainCfg->time.sec);

                if (bDayMatch && bTimeMatch)
                {
                    if (gstDevInfo.bUpgrading)
                    {
                        __INFO("skip timer reboot while ota is upgrading\n");
                    }
                    else
                    {
                        __WARN("timer maintain reboot start\n");
                        __RECORD_LOG_INFO("timer maintain reboot start\n");
                        anj_sysmng_reboot();
                        return 0;
                    }
                }
            }
        }

        int iResetStatus = anj_mw_hwctrl_reset_get();
        if (iResetStatus == -1)
        {
            __ERR("reset key: get reset status failed\n");
            anj_mw_rsleep(100 * 1000);
            continue;
        }
        else if (iResetStatus == 0)
        {
            if (bReseted == 0)
            {
                stResetTime = anj_mw_get_cputime_ms(NULL);
            }
            else
            {
                /*持续复位3s，重启*/
                if (anj_mw_get_cputime_ms(NULL) - stResetTime >= RESTORE_KEY_HOLD_TIME_MS)
                {
                    anj_audio_prompt_play(ANJ_MP3_DEFAULT_PATH, ANJ_MP3_DI_DI, 1);
                    sleep(1);
                    if (pstSystemCfg->audioPromptCfg.reset != 0)
                    {
                        anj_audio_prompt_play(ANJ_MP3_COMM_PATH, ANJ_MP3_FACTORY_RESTORE, 1);
                    }
                    anj_ser_unbind();
                    __RECORD_LOG_INFO("restore config reserved_bits=0\n");
                    anj_sysmng_config_restore(0);
                    return 0;
                }
            }
            bReseted = 1;
        }
        else
        {
            unsigned long long stNowTime = anj_mw_get_cputime_ms(NULL);
            /*复位摁住再松开，次数+1*/
            if (bReseted == 1)
            {
                stLastTime = stNowTime;
                stChangeCnt++;
                __INFO("######stChangeCnt:%d\n", stChangeCnt);

                if (stChangeCnt == 1)
                {
                    if (!anj_mw_file_exists(P2P_DEVICEBIND_FLAG) &&
                        (anj_net_status_check() == ANJ_NET_STATUS_WIFI))
                    {
                        anj_audio_prompt_play(ANJ_MP3_BIND_PATH, ANJ_MP3_AP_MODE, 1);
                        if (anj_net_hotspot_enable() != 0)
                        {
                            __ERR("reset key: wifi ap mode enter failed\n");
                        }
                    }
                }
            }

            if (stChangeCnt > 0 && stChangeCnt < 3)
            {
                /*最慢1s摁一次，超时，重算*/
                unsigned long long CurTime = stNowTime;
                if (CurTime - stLastTime > 1000)
                {
                    stChangeCnt = 0;
                }
            }
            else if (stChangeCnt == 3)
            {
                stChangeCnt = 0;
            }

            stResetTime = 0;
            bReseted = 0;
        }
        anj_mw_rsleep(100 * 1000);
    }
    return 0;
}

void anj_sysmng_get_random_mac(unsigned char *mac_addr)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int svalud = tv.tv_sec + tv.tv_usec;
    srand(svalud);

    mac_addr[0] = 0;
    mac_addr[1] = rand() % 256;
    mac_addr[2] = rand() % 256;
    mac_addr[3] = rand() % 256;
    mac_addr[4] = rand() % 256;
    mac_addr[5] = rand() % 256;

    mac_addr[5] += rand() % 256;
    mac_addr[4] += rand() % 256;
    mac_addr[3] += rand() % 256;
    mac_addr[2] += rand() % 256;
    mac_addr[1] += rand() % 256;

    __ERR("eth0 use new random mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
          mac_addr[0],
          mac_addr[1],
          mac_addr[2],
          mac_addr[3],
          mac_addr[4],
          mac_addr[5]);
}

void anj_sysmng_check_p2pid(NetworkConfigNew *pNetworkCfg)
{
    if (pNetworkCfg->p2pCfg.enable == 0)
    {
        __ERR("p2p disable\n");
        return;
    }

    int iRet = 0;
    NETWORK_STATUS_DATA networkStatus;
    iRet = anj_net_info_get(&networkStatus);
    if (iRet)
    {
        __ERR("p2p get net info failed\n");
        return;
    }

    // 目前只有AIOT，暂时不再判断AC18PRO等其他P2P类型
    __INFO("p2p type:%d\n", networkStatus.cloudType);

    if (networkStatus.cloudType > P2P_TYPE_NOTDEFINED)
    {
        // 其他类型需要
        // set_p2pid_cb(get_p2pid_ok);
        // start_p2pid_thread(&param);
    }
}

/*
OEM信息字符串:04:D5:90:C8:03:F4
或者：SN=FCMD5B5K20000001,MAC=04:D5:90:C8:03:F4
后期可扩充其他需要的信息
*/
void anj_sysmng_oem_get_ok_callback(const char *oemstr)
{
    AjOemStruct *p_oemInfo = (AjOemStruct *)malloc(sizeof(AjOemStruct));
    if (NULL == p_oemInfo)
    {
        __ERR("p_oemInfo malloc failed for macaddr %s.\n", oemstr);
        return;
    }

    memset(p_oemInfo, 0, sizeof(AjOemStruct));
    anj_config_oem_get(p_oemInfo);

    int bSetMac = 0;
    char szTmp[512] = {0};
    strncpy(szTmp, oemstr, 512 - 1);

    const char s[4] = ",";
    char *token = NULL;

    /* 获取第一个子字符串 */
    token = strtok(szTmp, s);
    /* 继续获取其他的子字符串 */
    while (token != NULL)
    {
        char *p1 = strcasestr(token, "SN=");
        if (p1 != NULL)
        {
            p1 += 3;
            strncpy(p_oemInfo->szOemSN, p1, AJ_OEM_STR_LEN - 1);
            __ERR("OEM SN=%s\n", p_oemInfo->szOemSN);
        }

        char *p2 = strcasestr(token, "MAC=");
        if (p2 != NULL)
        {
            p2 += 4;
            strncpy(p_oemInfo->szOemEthMac, p2, AJ_OEM_STR_LEN - 1);
            __ERR("OEM MAC=%s\n", p_oemInfo->szOemEthMac);
            bSetMac = 1;
        }

        if (p1 == NULL && p2 == NULL)
        {
            strncpy(p_oemInfo->szOemEthMac, token, AJ_OEM_STR_LEN - 1);
            __ERR("OEM MAC=%s\n", p_oemInfo->szOemEthMac);
            bSetMac = 1;
        }

        token = strtok(NULL, s);
    }

    anj_config_oem_save(p_oemInfo);

    if (bSetMac > 0 && strlen(p_oemInfo->szOemEthMac) > 0 && is_mac_addr_valid(p_oemInfo->szOemEthMac))
    {
        NetworkConfigNew *pNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
        __INFO("OEM set mac:%s\n", p_oemInfo->szOemEthMac);

        StrCpy((char *)pNetworkCfg->lanCfg.MACAddress, sizeof(pNetworkCfg->lanCfg.MACAddress), p_oemInfo->szOemEthMac);
        set_mac_addr(WIRE_INTERFACE_NAME, p_oemInfo->szOemEthMac, DEFAULT_WIRE_MAC_ADDR);
    }
    else
    {
        __ERR("OEM macaddr:%s format error.", p_oemInfo->szOemEthMac);
    }

    anj_sysmng_search_device_get();

    anj_mw_free(p_oemInfo);
}

void anj_sysmng_check_oeminfo()
{
    if (anj_mw_file_exists(AJ_APP_PATH "/flag.request.mac") == 0)
    {
        return;
    }

    AjOemApply_t param = {0};
    param.nType = TYPE_ID_TYPE_MACADDR;

    unsigned int size = sizeof(AjOemStruct);
    AjOemStruct *p_oemInfo = (AjOemStruct *)anj_mw_malloc(size);
    if (NULL == p_oemInfo)
    {
        __ERR("malloc size %u failed\n", size);
        return;
    }

    memset(p_oemInfo, 0, size);
    anj_config_oem_get(p_oemInfo);
    if (strlen(p_oemInfo->szOemEthMac) > 0)
    {
        __ERR("oeminfo already have macaddr:%s!", p_oemInfo->szOemEthMac);
        anj_mw_free(p_oemInfo);
        return;
    }

    anj_mw_free(p_oemInfo);

    char sn_str[32] = {0};
    char uuid_str[32] = {0};
    anj_sysmng_load_sn(sn_str, sizeof(sn_str));

    const char *p = get_uuid();
    if (p != NULL)
    {
        strncpy(uuid_str, p, 31);
    }

    strcpy(param.szSN, sn_str);
    strcpy(param.szUUID, uuid_str);

    set_oemapply_cb(anj_sysmng_oem_get_ok_callback);
    start_oemapply_thread(&param);
}

void anj_sysmng_get_softsn_ok()
{
    __INFO("get softsn ok!\n");
    gstDevInfo.activated = 1;

    char sSoftEncMac[32] = {0};
    unsigned char mac_addr[6] = {0};
    anj_net_mac_create_by_sn(mac_addr);
    format_mac_addr_from_digit_to_string((char *)mac_addr, sizeof(mac_addr), (char *)sSoftEncMac, MAC_ADDRESS_LEN);
    __INFO("get new mac:%s\n", (char *)sSoftEncMac);

    NetworkConfigNew *pNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
    strcpy((char *)pNetworkCfg->lanCfg.MACAddress, (char *)sSoftEncMac);
    set_mac_addr(WIRE_INTERFACE_NAME, (char *)sSoftEncMac, DEFAULT_WIRE_MAC_ADDR);

    anj_sysmng_check_p2pid(pNetworkCfg);
    anj_sysmng_check_oeminfo();

    anj_osd_update_config();
}

int anj_sysmng_sn_check()
{
    int iRet = 0;
    unsigned char buf[256] = {0};

    iRet = ReadEncriptDataFromSoft_ex(buf, sizeof(buf));
    if (iRet == 0)
    {
        if (anj_sysmng_sn_validate(buf) == 0)
        {
            __INFO("check sn ok...\n");
            gstDevInfo.activated = 1;
            return 0;
        }
    }
    __INFO("check sn failed...\n");

    gstDevInfo.activated = 0;

    set_softsn_v2_cb(anj_sysmng_get_softsn_ok);
    start_softsn_thread_v2();

    return 1;
}

void anj_sysmng_set_password()
{
    if (!anj_mw_file_exists(PASSWORD_ENCODE_FLAG))
    {
        return;
    }

    char password[64] = {0};
    const char *uuid = get_uuid();

    if (gstDevInfo.sn[0] == '\0' || uuid == NULL || uuid[0] == '\0')
    {
        __ERR("set password skip: sn/uuid empty\n");
        return;
    }

    sn_password(gstDevInfo.sn, uuid, password);
    if (password[0] == '\0')
    {
        __ERR("set password skip: sn_password empty\n");
        return;
    }

    char cmd[256] = {0};
    snprintf(cmd, sizeof(cmd), "echo -e \"%s\n%s\" | passwd root", password, password);
    anj_mw_system(cmd);
    anj_mw_system(cmd);
}

static int anj_sysmng_init()
{
    __LOG_ENTER();
    int iRet = 0;

    set_softsn_platform_type_cb(anj_sysmng_platform_type_get);

    GlobalConfig *pConfig = (GlobalConfig *)getGlbConfig();
    anj_sysmng_mac_restore(pConfig);

    anj_sysmng_uboot_version_init();
    iRet = anj_sysmng_version_info_get(&gstDevInfo.stVersionInfo, 0);
    if (iRet != 0)
    {
        __ERR("Read version failed.\n");
    }
    iRet = anj_sysmng_dev_str_get(gstDevInfo.devType);
    if (iRet != 0)
    {
        __ERR("Read device type failed.\n");
    }
    iRet = anj_sysmng_search_device_get();
    if (iRet != 0)
    {
        __ERR("Read search type failed.\n");
    }
    anj_sysmng_parse_fsversion(gstDevInfo.stVersionInfo.fsVersion,
                               gstDevInfo.subDevType, sizeof(gstDevInfo.subDevType),
                               gstDevInfo.version_name, sizeof(gstDevInfo.version_name),
                               gstDevInfo.release_date, sizeof(gstDevInfo.release_date));

    strcpy(gstDevInfo.uuid, get_uuid());
    char sn_str[64] = {0};
    anj_sysmng_load_sn(sn_str, sizeof(sn_str));

    anj_sysmng_sn_check();

    anj_sysctl_capability_init();
    anj_sysmng_second_config_led_apply();
    UserAuthInit();
    anj_systime_load_zone();

    anj_sysmng_set_password();

    s_stResetThread.bAutoDestroy = 1;
    strncpy(s_stResetThread.iThreadName, "reset_thread", sizeof(s_stResetThread.iThreadName) - 1);
    s_stResetThread.iThreadjob.ctx = &s_stResetThread;
    s_stResetThread.iThreadjob.func = anj_sysmng_reset_thread;
    iRet = anj_thread_task_create(&s_stResetThread);

    anj_systime_init();

    __LOG_LEAVE();

    return iRet;
}

static int anj_sysmng_uninit()
{
    anj_systime_uninit();

    anj_thread_task_destroy(&s_stResetThread, -1);
    return 0;
}

REGISTER_MODULE(anj_sysmng, MODULE_PRIORITY_SYSTEM);
