#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <dirent.h>
#include <errno.h>

#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_module.h"
#include "anj_sdcard.h"
#include "anj_record.h"
#include "anj_ser.h"
#include "anj_osd.h"
#include "anj_audio.h"
#include "anj_pri_cmd.h"
#include "anj_sysctl.h"
#include "anj_alarm.h"
#include "fdisk.h"
#include "function_list.h"

static int s_stSdcardInit = 0;
static int s_stMountdevIndex = 0;
static anj_thread_s s_stSdCardThread = {0};
static anj_thread_s s_stSdCardFormatThread = {0};
static anj_sdcard_status_e s_stSdCardStatus = ANJ_SDCARD_STATUS_NOT_INSERT;
static pthread_mutex_t s_stSdCardMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_stSdInfoMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_stSdMountMutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int s_stFormatPercent = 0;
static int s_format_session = 0;
static anj_sdcard_info s_stSdInfo = {0};

__attribute__((weak)) int g_feature_sdcard_enabled = 1;

static int anj_sdcard_format_check_empty(const char *mountPath)
{
    DIR *pDir = NULL;
    struct dirent *pEntry = NULL;

    if (mountPath == NULL)
    {
        return -1;
    }

    pDir = opendir(mountPath);
    if (pDir == NULL)
    {
        __ERR("opendir %s failed: %s\n", mountPath, strerror(errno));
        return -1;
    }

    while ((pEntry = readdir(pDir)) != NULL)
    {
        if ((strcmp(pEntry->d_name, ".") == 0) || (strcmp(pEntry->d_name, "..") == 0))
        {
            continue;
        }

        __ERR("format remains file after mkfs, mount:%s, name:%s\n", mountPath, pEntry->d_name);
        closedir(pDir);
        return -1;
    }

    closedir(pDir);
    return 0;
}

static void anj_sdcard_osd_set(anj_sdcard_status_e eSdCardStatus)
{
    osd_custom_content_s osdSdcardCustom = {0};
    osdSdcardCustom.custom_x = 0;
    osdSdcardCustom.custom_y = 1;
    osdSdcardCustom.custom_location = POSITION_TYPE_BY_FOUR_CORNER;
    if (eSdCardStatus == ANJ_SDCARD_STATUS_NOT_INIT)
    {
        osdSdcardCustom.custom_show = 1;
        osdSdcardCustom.overlayText = OVERLAY_SDCARD_UNINIT;
    }
    else if (eSdCardStatus == ANJ_SDCARD_STATUS_RWERROR)
    {
        osdSdcardCustom.custom_show = 1;
        osdSdcardCustom.overlayText = OVERLAY_SDCARD_ERROR;
    }
    else if (eSdCardStatus == ANJ_SDCARD_STATUS_RONLY)
    {
        osdSdcardCustom.custom_show = 1;
        osdSdcardCustom.overlayText = OVERLAY_SDCARD_RDONLY;
    }
    else
    {
        osdSdcardCustom.custom_show = 0;
    }
    anj_osd_sdcard_set(&osdSdcardCustom);
}

static int anj_sdcard_check_insert()
{
    int devIndex = 0;
    char stdevPath[128] = {0};
    s_stMountdevIndex = 0;
    for (devIndex = 0; devIndex < SDCARD_MAX_DEV; devIndex++)
    {
        snprintf(stdevPath, sizeof(stdevPath), SDCARD_DEV_NAME, devIndex);
        if (access(stdevPath, F_OK) == 0)
        {
            s_stMountdevIndex = 0;
            return 1;
        }
    }

    return 0;
}

static void anj_sdcard_fdisk()
{
    /*判断是否存在P1分区*/
    int bFdisk = 1;
    if (anj_sdcard_check_insert())
    {
        char stdevPath[128] = {0};
        char stMountPartion[128] = {0};
        snprintf(stdevPath, sizeof(stdevPath), SDCARD_DEV_NAME, s_stMountdevIndex);
        snprintf(stMountPartion, sizeof(stMountPartion), SDCARD_DEV_PARTITION_NAME, s_stMountdevIndex, 1);
        if (access(stMountPartion, F_OK) == 0)
        {
            unsigned long long iTotalSpace = 0;
            unsigned long long iP1Space = 0;
            int MMCfd = open(stdevPath, O_RDWR);
            if (MMCfd > 0)
            {
                if (ioctl(MMCfd, BLKGETSIZE64, &iTotalSpace) == 0)
                {
                    __INFO("Total get size %d GB\n", (unsigned int)(iTotalSpace >> 30));
                }
                close(MMCfd);
            }

            MMCfd = open(stMountPartion, O_RDWR);
            if (MMCfd > 0)
            {
                if (ioctl(MMCfd, BLKGETSIZE64, &iP1Space) == 0)
                {
                    __INFO("p1 get size %d GB\n", (unsigned int)(iP1Space >> 30));
                }
                close(MMCfd);
            }

            if ((iTotalSpace >> 30) - (iP1Space >> 30) > 1)
            {
                bFdisk = 1;
            }
            else
            {
                bFdisk = 0;
            }
        }
        else
        {
            if (bFdisk)
            {
                fdisk_repair_sd(stdevPath);
            }
        }
    }
}

static anj_sdcard_status_e anj_sdcard_info_test()
{
    anj_sdcard_status_e iStatus;

    int iTestLen = strlen(SDCARD_TEST_DATA);
    char stTestPath[128] = {0};
    snprintf(stTestPath, sizeof(stTestPath), SDCARD_TEST_FILE, anj_sdcard_mount_index_get());
    if (access(stTestPath, F_OK) == 0)
    {
        unlink(stTestPath);
    }

    if (access(stTestPath, F_OK) != 0)
    {
        int fd = open(stTestPath, O_RDWR | O_CREAT | O_SYNC);
        if (fd == -1)
        {
            iStatus = ANJ_SDCARD_STATUS_RWERROR;
            __ERR("%s open failed (ANJ_SDCARD_STATUS_RWERROR) %s\n", stTestPath, strerror(errno));
        }
        else
        {
            int len = write(fd, SDCARD_TEST_DATA, iTestLen);
            close(fd);
            if (len != iTestLen)
            {
                iStatus = ANJ_SDCARD_STATUS_RONLY;
                __ERR("%s read failed (ANJ_SDCARD_STATUS_RONLY), %d != %d\n", stTestPath, len, iTestLen);
            }
            else
            {
                anj_mw_system_free_cache();
                fd = open(stTestPath, O_RDONLY);
                if (fd < 0)
                {
                    iStatus = ANJ_SDCARD_STATUS_RWERROR;
                    __ERR("%s open failed (ANJ_SDCARD_STATUS_RWERROR) %s\n", stTestPath, strerror(errno));
                }
                else
                {
                    char buf[32] = {0};
                    memset(buf, 0, sizeof(buf));
                    len = read(fd, buf, iTestLen);
                    close(fd);
                    if ((len != iTestLen) || (strcmp(buf, SDCARD_TEST_DATA) != 0))
                    {
                        iStatus = ANJ_SDCARD_STATUS_RWERROR;
                        __ERR("%s read failed (%d!=%d)(%s != %s)\n", stTestPath, len, iTestLen, buf, SDCARD_TEST_DATA);
                    }
                    else
                    {
                        if (anj_record_check_valid() != 0)
                        {
                            iStatus = ANJ_SDCARD_STATUS_RWERROR;
                        }
                        else
                        {
                            iStatus = ANJ_SDCARD_STATUS_NORMAL;
                        }
                    }
                }
            }
        }
    }
    else
    {
        iStatus = ANJ_SDCARD_STATUS_RWERROR;
    }
    if (access(stTestPath, F_OK) == 0)
    {
        unlink(stTestPath);
    }
    return iStatus;
}
static unsigned int anj_sdcard_remain_capacity()
{
    char stMountPath[128] = {0};
    snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, anj_sdcard_mount_index_get());
    struct statfs diskInfo;
    memset(&diskInfo, 0, sizeof(diskInfo));
    statfs(stMountPath, &diskInfo);
    unsigned long long int freeDisk = (unsigned long long int)diskInfo.f_bfree * (unsigned long long int)diskInfo.f_bsize;
    return (unsigned int)(freeDisk >> 20);
}

static unsigned int anj_sdcard_capacity()
{
    unsigned long long devSize = 0;
    int mountIndex = 0;
    int devIndex = anj_sdcard_mount_index_get();
    char stDevPath[128] = {0};

    for (mountIndex = SDCARD_MAX_PARTITION; mountIndex >= 0; mountIndex--)
    {
        if (mountIndex == 0)
        {
            snprintf(stDevPath, sizeof(stDevPath), SDCARD_DEV_NAME, devIndex);
        }
        else
        {
            snprintf(stDevPath, sizeof(stDevPath), SDCARD_DEV_PARTITION_NAME, devIndex, mountIndex);
        }

        if (!anj_sdcard_file_exists(stDevPath))
        {
            continue;
        }

        int fd = open(stDevPath, O_RDONLY);
        if (fd < 0)
        {
            __ERR("open %s failed: %s\n", stDevPath, strerror(errno));
            return 0;
        }

        if (ioctl(fd, BLKGETSIZE64, &devSize) != 0)
        {
            __ERR("ioctl BLKGETSIZE64 %s failed: %s\n", stDevPath, strerror(errno));
            close(fd);
            return 0;
        }

        close(fd);
        return (unsigned int)(devSize >> 20);
    }

    return 0;
}

static int anj_sdcard_format_thread(void *ctx, int *bStart)
{
    int iRet = -1;
    int mountIndex = 0;
    char cmd[128] = {0};
    char stMountPath[64] = {0};
    char stMountPartion[64] = {0};

    if ((anj_sdcard_check_insert() == 0) || (ctx == NULL))
    {
        __ERR("SDCARD NOT INSERT!\n");
        return iRet;
    }

    anj_record_uninit();

    anj_sdcard_status_set(ANJ_SDCARD_STATUS_FORMAT, 0);
    anj_ser_reponse(SER_RESPONSE_SDCARD, NULL);

    anj_sdcard_info_update();

    anj_sdcard_fomat_percent_set(0);

    anj_sdcard_umount();

    anj_sdcard_fomat_percent_set((anj_sdcard_fomat_percent_get() + 5));

    anj_sdcard_fdisk();

    anj_sdcard_fomat_percent_set((anj_sdcard_fomat_percent_get() + 5));

    for (mountIndex = SDCARD_MAX_PARTITION; mountIndex >= 0; mountIndex--)
    {
        snprintf(stMountPartion, sizeof(stMountPartion), SDCARD_DEV_PARTITION_NAME, s_stMountdevIndex, mountIndex);
        if (anj_sdcard_file_exists(stMountPartion))
        {
            snprintf(cmd, sizeof(cmd), "mkfs.vfat %s", stMountPartion);
            iRet = anj_mw_system(cmd);
            snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, s_stMountdevIndex);
            break;
        }
    }

    if (iRet != 0)
    {
        if (anj_sdcard_mount() == 0)
        {
            anj_sdcard_status_set(anj_sdcard_info_test(), 1);
        }
        else
        {
            anj_sdcard_status_set(ANJ_SDCARD_STATUS_INSERT, 1);
        }
    }
    else
    {
        anj_sdcard_fomat_percent_set((anj_sdcard_fomat_percent_get() + 5));

        iRet = anj_sdcard_mount();
        if (iRet == 0)
        {
            iRet = anj_sdcard_format_check_empty(stMountPath);
            if (iRet == 0)
            {
                iRet = anj_record_fallocate(stMountPath, 1);
            }
            if (iRet == 0)
            {
                iRet = anj_record_init(stMountPath, 1, 0);
                if (iRet == 0)
                {
                    anj_sdcard_status_set(anj_sdcard_info_test(), 1);
                }
                else
                {
                    anj_sdcard_status_set(ANJ_SDCARD_STATUS_NOT_INIT, 1);
                }
            }
            else
            {
                anj_sdcard_umount();
                anj_sdcard_status_set(ANJ_SDCARD_STATUS_NOT_INIT, 1);
            }
        }
        else
        {
            anj_sdcard_status_set(ANJ_SDCARD_STATUS_INSERT, 1);
        }
        anj_sdcard_fomat_percent_set(100);
    }

    anj_alarm_sdcard_format_start(!iRet);
    anj_pri_cmd_formart_reponse(iRet, *(int *)ctx);
    anj_ser_reponse(SER_RESPONSE_SDCARD, NULL);

    memset(&s_stSdCardFormatThread, 0, sizeof(s_stSdCardFormatThread));
    return 0;
}

static int anj_sdcard_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    int bInsertSdCard = 0;
    int bPlayErr = 0;
    char stMountPath[128] = {0};

    anj_sdcard_status_set(ANJ_SDCARD_STATUS_NOT_INSERT, 1);

    anj_sdcard_umount();

    int time_count = SDCARD_INFO_OVER_TIME;
    anj_sdcard_info stSdInfo;
    memset(&stSdInfo, 0, sizeof(anj_sdcard_info));
    while (bStart && *bStart)
    {
        if (anj_record_status_get() == REC_STATUS_BAD_RECOVER)
        {
            sleep(1);
            continue;;
        }
        int bCurInsertSdCard = anj_sdcard_check_insert();
        if (bInsertSdCard != bCurInsertSdCard)
        {
            bPlayErr = 0;
            anj_sdcard_osd_set(ANJ_SDCARD_STATUS_NOT_INSERT);
            anj_sdcard_info_update();
            bInsertSdCard = bCurInsertSdCard;
            if (bInsertSdCard)
            {
                __INFO("Sdcard insert status\n");
                anj_audio_prompt_play(ANJ_MP3_SDCARD_PATH, ANJ_MP3_SD_INSERT, 1);
                anj_sdcard_status_set(ANJ_SDCARD_STATUS_INSERT, 1);
                iRet = anj_sdcard_check_mount();
                if (0 != iRet)
                {
                    iRet = anj_sdcard_mount();
                    if (iRet == 0)
                    {
                        anj_sdcard_status_set(ANJ_SDCARD_STATUS_MOUNT, 1);
                    }
                    else
                    {
                        anj_sdcard_status_set(ANJ_SDCARD_STATUS_INSERT, 1);
                    }
                }
                else
                {
                    anj_sdcard_status_set(ANJ_SDCARD_STATUS_MOUNT, 1);
                }
                if (s_stSdCardStatus == ANJ_SDCARD_STATUS_MOUNT)
                {
                    snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, s_stMountdevIndex);
                    anj_record_init(stMountPath, 1, 0);
                }
                anj_sdcard_osd_set(s_stSdCardStatus);
            }
            else
            {
                __INFO("Sdcard not insert status\n");
                anj_audio_prompt_play(ANJ_MP3_SDCARD_PATH, ANJ_MP3_SD_REMOVE, 1);
                anj_record_uninit();
                anj_sdcard_umount();
                anj_sdcard_status_set(ANJ_SDCARD_STATUS_NOT_INSERT, 1);
                memset(&stSdInfo, 0, sizeof(anj_sdcard_info));
            }
        }

        stSdInfo.eStatus = anj_sdcard_status_get();
        if (s_stSdInfo.bInfoUpdate || (time_count >= SDCARD_INFO_OVER_TIME) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_FORMAT))
        {
            time_count = 0;
            /* No card: keep NOT_INSERT and clear stale OSD (e.g. after wrongly set NORMAL). */
            if ((bCurInsertSdCard == 0) || (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NOT_INSERT))
            {
                if (stSdInfo.eStatus != ANJ_SDCARD_STATUS_NOT_INSERT)
                {
                    anj_sdcard_status_set(ANJ_SDCARD_STATUS_NOT_INSERT, 1);
                    stSdInfo.eStatus = ANJ_SDCARD_STATUS_NOT_INSERT;
                }
                stSdInfo.iSize = 0;
                stSdInfo.iRemainSize = 0;
                stSdInfo.iFormatPercent = 0;
                anj_sdcard_osd_set(ANJ_SDCARD_STATUS_NOT_INSERT);
            }
            else if (stSdInfo.eStatus >= ANJ_SDCARD_STATUS_MOUNT)
            {
                __INFO("Get sdInfo\n");
                pthread_mutex_lock(&s_stSdMountMutex);
                stSdInfo.iSize = anj_sdcard_capacity();
                stSdInfo.iRemainSize = anj_sdcard_remain_capacity();
                stSdInfo.iFormatPercent = anj_sdcard_fomat_percent_get();

                if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NORMAL)
                {
                    stSdInfo.eStatus = anj_sdcard_info_test();
                    if (stSdInfo.eStatus != ANJ_SDCARD_STATUS_NORMAL)
                    {
                        pthread_mutex_unlock(&s_stSdMountMutex);
                        anj_record_uninit();
                        pthread_mutex_lock(&s_stSdMountMutex);
                        anj_sdcard_umount();
                        anj_sdcard_mount();
                        stSdInfo.eStatus = anj_sdcard_info_test();
                        if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NORMAL)
                        {
                            anj_record_init(stMountPath, 1, 1);
                        }
                    }

                    /* Only map rec status when card R/W test still passes. */
                    if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_NORMAL)
                    {
                        rec_status_e recStatus = anj_record_status_get();
                        if (recStatus == REC_STATUS_ERROR)
                        {
                            stSdInfo.eStatus = ANJ_SDCARD_STATUS_RWERROR;
                        }
                        else if (recStatus == REC_STATUS_UNINIT)
                        {
                            stSdInfo.eStatus = ANJ_SDCARD_STATUS_NOT_INIT;
                        }
                    }
                }
                pthread_mutex_unlock(&s_stSdMountMutex);
                anj_sdcard_osd_set(stSdInfo.eStatus);
                int bFull = 0, nextFileNo = 0;
                int iMediaMaxFiles = anj_record_max_file_get(&bFull, &nextFileNo);
                if (iMediaMaxFiles >= nextFileNo)
                {
                    if (bFull)
                    {
                        stSdInfo.iRemainSize = 0;
                    }
                    else
                    {
                        stSdInfo.iRemainSize = ((iMediaMaxFiles - nextFileNo) * stSdInfo.iSize) / iMediaMaxFiles;
                    }
                }

                if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_RWERROR || stSdInfo.eStatus == ANJ_SDCARD_STATUS_RONLY ||
                    stSdInfo.eStatus == ANJ_SDCARD_STATUS_NOT_INIT)
                {
                    if ((stSdInfo.eStatus == ANJ_SDCARD_STATUS_RWERROR || stSdInfo.eStatus == ANJ_SDCARD_STATUS_RONLY) &&
                        (bPlayErr == 0))
                    {
                        bPlayErr = 1;
                        anj_audio_prompt_play(ANJ_MP3_SDCARD_PATH, ANJ_MP3_SD_ABNORMAL, 1);
                    }
                    anj_sdcard_status_set(stSdInfo.eStatus, 0);
                }
            }
            else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_INSERT)
            {
                stSdInfo.iSize = anj_sdcard_capacity();
                stSdInfo.iRemainSize = 0;
                stSdInfo.iFormatPercent = anj_sdcard_fomat_percent_get();
            }
            else if (stSdInfo.eStatus == ANJ_SDCARD_STATUS_FORMAT)
            {
                stSdInfo.iSize = 0;
                stSdInfo.iRemainSize = 0;
                stSdInfo.iFormatPercent = anj_sdcard_fomat_percent_get();
                time_count = SDCARD_INFO_OVER_TIME;
            }
            stSdInfo.iIndex = anj_sdcard_mount_index_get();
            pthread_mutex_lock(&s_stSdInfoMutex);
            memcpy(&s_stSdInfo, &stSdInfo, sizeof(anj_sdcard_info));
            s_stSdInfo.bInfoValid = 1;
            pthread_mutex_unlock(&s_stSdInfoMutex);
        }
        time_count++;

        if (access("/tmp/event", F_OK) == 0)
        {
            remove("/tmp/event");
            anj_record_start_event(0, REC_EVENT_MOTION_ALARM_MASK);
        }
        usleep(100 * 1000);
    }
    anj_record_uninit();

    return iRet;
}

int anj_sdcard_init(void)
{
    int iRet = 0;
    __INFO("anj_sdcard_init g_feature_sdcard_enabled:%d\n", g_feature_sdcard_enabled);
    if (!g_feature_sdcard_enabled)
    {
        return 0;
    }
    if (s_stSdcardInit)
	{
	    __ERR("had been init\n");
		return iRet;
	}
    memset(&s_stSdCardThread, 0, sizeof(anj_thread_s));

    anj_sysctl_capability_add(FUNCTION_SUPPORT_STORAGE);
    anj_sysctl_capability_add(FUNCTION_FALLOCATE_REC);

    s_stSdCardThread.bAutoDestroy = 1;
    strncpy(s_stSdCardThread.iThreadName, "anj_sdcard_thread", sizeof(s_stSdCardThread.iThreadName) - 1);
    s_stSdCardThread.iThreadjob.ctx = &s_stSdCardThread;
    s_stSdCardThread.iThreadjob.func = anj_sdcard_thread;
    iRet = anj_thread_task_create(&s_stSdCardThread);
    s_stSdcardInit = 1;
    return iRet;
}

int anj_sdcard_uninit(void)
{
    if (s_stSdcardInit == 0)
	{
	    __ERR("not init\n");
		return 0;
	}
    anj_thread_task_destroy(&s_stSdCardThread, -1);
    s_stSdcardInit = 0;
    return 0;
}

anj_sdcard_status_e anj_sdcard_status_get(void)
{
    if (0 == s_stSdcardInit)
    {
        return ANJ_SDCARD_STATUS_NOT_INSERT;
    }
    else
    {
        return s_stSdCardStatus;
    }
}

void anj_sdcard_status_set(anj_sdcard_status_e iStatus, int force)
{
    if (s_stSdcardInit == 0)
	{
		return ;
	}
    pthread_mutex_lock(&s_stSdCardMutex);
    if (force || (s_stSdCardStatus != ANJ_SDCARD_STATUS_FORMAT))
        s_stSdCardStatus = iStatus;
    pthread_mutex_unlock(&s_stSdCardMutex);
    __INFO("s_stSdCardStatus:%d\n", s_stSdCardStatus);
}

int anj_sdcard_fomat_percent_get(void)
{
    if (s_stSdcardInit == 0)
	{
		return 0;
	}
    return s_stFormatPercent;
}

void anj_sdcard_fomat_percent_set(int iPercent)
{
    if (s_stSdcardInit == 0)
	{
		return ;
	}
    s_stFormatPercent = iPercent;
}

int anj_sdcard_check_mount(void)
{
    int iRet = -1;
    int mountIndex = 0;
    struct statfs statFS;
    char stMountPath[128] = {0};
    for (mountIndex = 0; mountIndex < SDCARD_MAX_DEV; mountIndex++)
    {
        snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, mountIndex);
        if (statfs(stMountPath, &statFS) == -1)
        {
            __ERR("%s is not mounted, f_type:0x%x\n", stMountPath, statFS.f_type);
            continue;
        }

        if (statFS.f_type == 0x4d44) // MSDOS_SUPER_MAGIC
        {
            __INFO("%s is mounted\n", stMountPath);
            iRet = 0;
            break;
        }
        else
        {
            __INFO("%s is not mounted, f_type:0x%x\n", stMountPath, statFS.f_type);
            continue;
        }
    }

    return iRet;
}

int anj_sdcard_umount(void)
{
    int iRet = 0;
    int mountIndex = 0;
    char stMountPath[128] = {0};
    for (mountIndex = 0; mountIndex < SDCARD_MAX_DEV; mountIndex++)
    {
        snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, mountIndex);
        iRet = umount2(stMountPath, (MNT_DETACH | MNT_FORCE));
        if (iRet != 0)
        {
            __ERR("unmount (%s) failed: %s\n", stMountPath, strerror(errno));
        }
    }
    s_stMountdevIndex = 0;

    return 0;
}

int anj_sdcard_mount(void)
{
    int iRet = 0;
    int mountIndex = SDCARD_MAX_PARTITION;
    char stMountPath[128] = {0};
    char stMountPartion[128] = {0};
    for (; mountIndex >= 0; mountIndex--)
    {
        if (mountIndex == 0)
        {
            snprintf(stMountPartion, sizeof(stMountPartion), SDCARD_DEV_NAME, s_stMountdevIndex);
        }
        else
        {
            snprintf(stMountPartion, sizeof(stMountPartion), SDCARD_DEV_PARTITION_NAME, s_stMountdevIndex, mountIndex);
        }
        if (anj_sdcard_file_exists(stMountPartion))
        {
            snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, s_stMountdevIndex);
            const char *options = "errors=continue";
            iRet = mount(stMountPartion, stMountPath, "vfat", 0, options);
            if (iRet != 0)
            {
                __ERR("mount %s to %s failed: %s\n", stMountPartion, stMountPath, strerror(errno));
                if (mountIndex > 0)
                {
                    anj_sdcard_umount();
                }
                return -1;
            }
            break;
        }
    }
    if (mountIndex < 0)
    {
        __ERR("mount sdcard failed, mountIndex:%d\n", mountIndex);
        return -1;
    }
    iRet = anj_sdcard_check_mount();
    return iRet;
}

void anj_sdcard_remount(void)
{
    pthread_mutex_lock(&s_stSdMountMutex);
    anj_sdcard_umount();
    anj_sdcard_mount();
    pthread_mutex_unlock(&s_stSdMountMutex);
}

unsigned long long int anj_sdcard_size_get(const char *filePath)
{
    if (filePath == NULL)
    {
        __ERR("Invalid Input\n");
        return 0;
    }
    struct statfs dirInfo;
    memset(&dirInfo, 0, sizeof(dirInfo));
    statfs(filePath, &dirInfo);
    unsigned long long int total = (unsigned long long int)dirInfo.f_blocks * (unsigned long long int)dirInfo.f_bsize;
    return total;
}

int anj_sdcard_file_exists(const char *filePath)
{
    int iRet = 0;
    if (filePath == NULL)
    {
        __ERR("Invalid Input\n");
        return 0;
    }

    if (filePath)
    {
        struct stat st;
        int result = stat(filePath, &st);
        iRet = (result == 0) ? 1 : 0;
    }
    return iRet;
}

int anj_sdcard_mount_index_get(void)
{
    return s_stMountdevIndex;
}

int anj_sdcard_info_query(anj_sdcard_info *pstSdInfo)
{
    if (s_stSdcardInit == 0)
	{
		return -1;
	}
    int TimeOut = 0;
    do
    {
        pthread_mutex_lock(&s_stSdInfoMutex);
        memcpy(pstSdInfo, &s_stSdInfo, sizeof(anj_sdcard_info));
        pthread_mutex_unlock(&s_stSdInfoMutex);

        usleep(100 * 1000);
        TimeOut++;
    } while ((pstSdInfo->bInfoValid == 0) && (TimeOut < 20));
    __INFO("anj_sdcard_info_query over\n");
    pthread_mutex_lock(&s_stSdInfoMutex);
    s_stSdInfo.bInfoUpdate = 1;
    pthread_mutex_unlock(&s_stSdInfoMutex);
    return pstSdInfo->bInfoValid ? 0 : -1;
}

void anj_sdcard_format(int sessionid)
{
    if (s_stSdcardInit == 0)
	{
		return ;
	}
    if (s_stSdCardFormatThread.start == 1)
    {
        __INFO("sdcard formating...please wait!\n");
        return;
    }

    anj_audio_prompt_play(ANJ_MP3_SDCARD_PATH, ANJ_MP3_SD_FORMAT, 1);

    s_format_session = sessionid;
    s_stSdCardFormatThread.bAutoDestroy = 1;
    strncpy(s_stSdCardFormatThread.iThreadName, "sd_format", sizeof(s_stSdCardFormatThread.iThreadName) - 1);
    s_stSdCardFormatThread.iThreadjob.ctx = (void *)&s_format_session;
    s_stSdCardFormatThread.iThreadjob.func = anj_sdcard_format_thread;
    anj_thread_task_create(&s_stSdCardFormatThread);
}

void anj_sdcard_info_update()
{
    if (s_stSdcardInit == 0)
	{
		return ;
	}
    pthread_mutex_lock(&s_stSdInfoMutex);
    s_stSdInfo.bInfoValid = 0;
    s_stSdInfo.bInfoUpdate = 1;
    pthread_mutex_unlock(&s_stSdInfoMutex);
}

anj_sdcard_info *anj_sdcard_info_get(void)
{
    return &s_stSdInfo;
}

REGISTER_MODULE(anj_sdcard, MODULE_PRIORITY_SDCARD);