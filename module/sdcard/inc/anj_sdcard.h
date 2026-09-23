#ifndef __ANJ_SDCARD_H__
#define __ANJ_SDCARD_H__

#include "anj_mw_comm.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SDCARD_MOUNT_PATH                         "/mnt/mmc%d"
#define SDCARD_DEV_NAME                           "/dev/mmcblk%d"
#define SDCARD_DEV_PARTITION_NAME                 SDCARD_DEV_NAME "p%d"
#define SDCARD_MAX_DEV                            (2)
#define SDCARD_MAX_PARTITION                      (4)
#define SDCARD_INFO_OVER_TIME                     (10 * 60 * 10)     //10min
#define SDCARD_TEST_DATA                          "TEST_SD_DATA_IS_NORMAL"
#define SDCARD_TEST_FILE                          SDCARD_MOUNT_PATH "/testfile"

typedef enum
{
    ANJ_SDCARD_STATUS_NOT_INSERT,
    ANJ_SDCARD_STATUS_INSERT,
    ANJ_SDCARD_STATUS_FORMAT,
    ANJ_SDCARD_STATUS_MOUNT,
    ANJ_SDCARD_STATUS_NOT_INIT,
    ANJ_SDCARD_STATUS_NORMAL,
    ANJ_SDCARD_STATUS_RWERROR,
    ANJ_SDCARD_STATUS_RONLY,
} anj_sdcard_status_e;

typedef struct
{
    int iIndex;
    int iSize;
    int iRemainSize;
    anj_sdcard_status_e eStatus;
    int eRecStatus;
    int iFormatPercent;
    int bInfoValid;
    int bInfoUpdate;
}anj_sdcard_info;

anj_sdcard_status_e anj_sdcard_status_get(void);

void anj_sdcard_status_set(anj_sdcard_status_e iStatus, int force);

int anj_sdcard_fomat_percent_get(void);

void anj_sdcard_fomat_percent_set(int iPercent);

int anj_sdcard_check_mount(void);

int anj_sdcard_umount(void);

int anj_sdcard_mount(void);

void anj_sdcard_remount(void);

void anj_sdcard_format(int sessionid);

unsigned long long int anj_sdcard_size_get(const char *filePath);

int anj_sdcard_file_exists(const char *filePath);

int anj_sdcard_mount_index_get(void);

int anj_sdcard_info_query(anj_sdcard_info *pstSdInfo);

void anj_sdcard_info_update();

anj_sdcard_info *anj_sdcard_info_get(void);

#ifdef __cplusplus
}
#endif

#endif
