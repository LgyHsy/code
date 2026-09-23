#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/vfs.h>
#include <time.h>
#include <stdio.h>

#include "anj_mw_comm.h"
#include "anj_sysmng.h"
#include "anj_audio.h"
#include "ajupgrade.h"
#include "ota_update.h"
#include "anj_module.h"
#include "record_log.h"

#define OTA_SERVER_LINK "download.icamra.com"

enum OnlineUpStatus
{
    OnlineUpStatus_Latest = -1,
    OnlineUpStatus_Init = 0,
    OnlineUpStatus_Checking = 1,
    OnlineUpStatus_NewVer = 2,
};

typedef struct
{
    int m_result;
    int m_bChecking;
    int m_bUpgrading;
    int m_bMemoryFreed;

    char m_latestVersion[32];
    char m_releaseNotes[512];
} ota_update_t;

static pthread_mutex_t s_stOtaMutex = PTHREAD_MUTEX_INITIALIZER;
static ota_update_t g_otaHandler = {0};

static void api_upgrade_debuglog(const char *fmt, ...)
{
    char content_buf[1024];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(content_buf, 1024, fmt, ap);
    va_end(ap);

    char timestr[64];
    struct tm tbuf;
    time_t tsec = time(0);

    localtime_r(&tsec, &tbuf);

    int iRet = snprintf(timestr, sizeof(timestr), "%04d-%02d-%02d %02d:%02d:%02d",
                        2000 + tbuf.tm_year - 100, tbuf.tm_mon + 1,
                        tbuf.tm_mday, tbuf.tm_hour, tbuf.tm_min, tbuf.tm_sec);
    if (iRet < 0)
    {
        memset(timestr, 0, sizeof(timestr));
    }

    __ERR("%s %s\n", timestr, content_buf);
}

static void ota_free_memory_cb(void)
{
    anj_audio_prompt_play(ANJ_MP3_OTA_PATH, ANJ_MP3_DEVICE_START_UPDATE, 1);
    modules_uninit("anj_ser", "anj_net");
    g_otaHandler.m_bMemoryFreed = 1;
}

static int ota_check_cb(int result, const char *latestversion, const char *firmwarename, const char *releasenotes)
{
    anj_mutex_lock(&s_stOtaMutex);
    g_otaHandler.m_bChecking = 0;

    if (NULL != latestversion)
        __ERR("latestversion=%s\n", latestversion);
    if (NULL != firmwarename)
        __ERR("firmwarename=%s\n", firmwarename);
    if (NULL != releasenotes)
        __ERR("releasenotes=%s\n", releasenotes);

    if (result == 0 && latestversion != NULL && strlen(latestversion) > 0)
    {
        g_otaHandler.m_result = OnlineUpStatus_NewVer;
        strncpy(g_otaHandler.m_latestVersion, latestversion, sizeof(g_otaHandler.m_latestVersion) - 1);
        g_otaHandler.m_latestVersion[sizeof(g_otaHandler.m_latestVersion) - 1] = 0;
        anj_mw_read_file_limit_len(releasenotes, g_otaHandler.m_releaseNotes, sizeof(g_otaHandler.m_releaseNotes));
    }
    else
    {
        g_otaHandler.m_result = OnlineUpStatus_Latest;
        memset(g_otaHandler.m_latestVersion, 0, sizeof(g_otaHandler.m_latestVersion));
        memset(g_otaHandler.m_releaseNotes, 0, sizeof(g_otaHandler.m_releaseNotes));
    }

    if (result == 0 && firmwarename != NULL && strlen(firmwarename) > 0)
    {
        g_otaHandler.m_bUpgrading = 1;
        anj_mutex_unlock(&s_stOtaMutex);

        APPBIN_UPDATE_DATA updateData = {0};
        snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", firmwarename);
        updateData.nPhyAddr = 0;
        updateData.nFileLen = 0;
        int iRet = anj_sysmng_app_update(&updateData);
        if (0 != iRet)
        {
            anj_mutex_lock(&s_stOtaMutex);
            g_otaHandler.m_bUpgrading = 0;
            anj_mutex_unlock(&s_stOtaMutex);
        }
    }
    else
    {
        anj_mutex_unlock(&s_stOtaMutex);

        if (g_otaHandler.m_bMemoryFreed)
        {
            __WARN("ota check failed, reboot\n");
            __RECORD_LOG_INFO("ota check failed, reboot\n");
            anj_sysmng_reboot();
        }
    }

    return 0;
}

int ota_check_version(int bDownload)
{
    DevInfo *pstDevInfo = getDevInfo();
    int iRet = 0;
    char *szPath = "/tmp";

    if (g_otaHandler.m_bChecking > 0 || g_otaHandler.m_bUpgrading > 0)
    {
        __ERR("m_bChecking=%d, m_bUpgrading=%d\n", g_otaHandler.m_bChecking, g_otaHandler.m_bUpgrading);
        return -1;
    }

    AjUpgradeApiStop();

    anj_mutex_lock(&s_stOtaMutex);

    iRet = AjUpgradeApiStart(pstDevInfo->sn, 0, pstDevInfo->devType, pstDevInfo->custom_name, pstDevInfo->version_name, pstDevInfo->uuid, szPath,
                             bDownload, OTA_SERVER_LINK, ota_free_memory_cb, ota_check_cb, api_upgrade_debuglog);
    __INFO("AjUpgradeApiStart iRet=%d, Devicetype:%s CustomName:%s Version:%s\n",
           iRet, pstDevInfo->devType, pstDevInfo->custom_name, pstDevInfo->version_name);
    if (0 == iRet)
    {
        g_otaHandler.m_bChecking = 1;
    }

    anj_mutex_unlock(&s_stOtaMutex);

    if (0 == bDownload)
    {
        if (g_otaHandler.m_bChecking)
        {
            sleep(10);
        }
    }

    return iRet;
}

int ota_version_get(int *status, char *latestversion, char *releasenotes)
{
    if (g_otaHandler.m_bChecking)
    {
        *status = OnlineUpStatus_Checking;
        return -1;
    }
    else
    {
        *status = g_otaHandler.m_result;
        strcpy(latestversion, g_otaHandler.m_latestVersion);
        strcpy(releasenotes, g_otaHandler.m_releaseNotes);
    }
    return 0;
}
