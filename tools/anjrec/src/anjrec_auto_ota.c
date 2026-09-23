#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_net.h"
#include "anj_mw_thread.h"
#include "anj_module.h"
#include "anj_net.h"
#include "anj_sysmng.h"
#include "anjrec_auto_ota.h"
#include "anjrec_version_match.h"
#include "file_receiver.h"

#define AUTO_OTA_FW_PATH "/tmp/ota_firmware.bin"
#define AUTO_OTA_WAIT_NET_SEC 60
#define AUTO_OTA_WAIT_GID_SEC 180
#define AUTO_OTA_MATCH_RETRY 3
#define AUTO_OTA_MATCH_RETRY_INTERVAL_SEC 10

static anj_thread_s s_stAutoOtaThread = {0};

static int auto_ota_iface_usable(const char *ifname)
{
    struct NET_CONFIG netcfg;

    if (ifname == NULL || net_get_info(ifname, &netcfg) != 0)
    {
        return -1;
    }
    if (netcfg.ifaddr == 0 || netcfg.gateway == 0)
    {
        return -1;
    }
    return 0;
}

static int auto_ota_network_ready(void)
{
    anj_net_status_e netStatus;

    if (auto_ota_iface_usable(WIRE_INTERFACE_NAME) == 0)
    {
        return 1;
    }

    netStatus = anj_net_status_check();
    if (netStatus == ANJ_NET_STATUS_WIFI && auto_ota_iface_usable(WIFI_INTERFACE_NAME) == 0)
    {
        return 1;
    }
    if (netStatus == ANJ_NET_STATUS_4G && auto_ota_iface_usable(WIRE_INTERFACE_NAME1) == 0)
    {
        return 1;
    }
    return 0;
}

static int auto_ota_wait_network(int *bStart)
{
    int wait_time = AUTO_OTA_WAIT_NET_SEC;

    while (bStart && *bStart && wait_time-- > 0)
    {
        if (auto_ota_network_ready())
        {
            __INFO("auto_ota: network ready\n");
            return 0;
        }
        sleep(1);
    }
    return -1;
}

static int auto_ota_wait_gid(int *bStart)
{
    int wait_time = AUTO_OTA_WAIT_GID_SEC;
    char sn_str[64] = {0};

    while (bStart && *bStart && wait_time-- > 0)
    {
        memset(sn_str, 0, sizeof(sn_str));
        if (anj_sysmng_load_sn(sn_str, sizeof(sn_str)) == 0 && sn_str[0] != '\0')
        {
            __INFO("auto_ota: DevGID(SN) ready: %s\n", sn_str);
            return 0;
        }
        sleep(1);
    }
    return -1;
}

static int auto_ota_is_busy(void)
{
    DevInfo *pstDevInfo = getDevInfo();

    if (pstDevInfo != NULL && pstDevInfo->bUpgrading)
    {
        __INFO("auto_ota: busy, bUpgrading=1\n");
        return 1;
    }
    if (getFileRecver() != NULL)
    {
        __INFO("auto_ota: busy, file_recver active\n");
        return 1;
    }
    return 0;
}

static int auto_ota_file_md5(const char *path, char *md5_buf, int buf_len)
{
    char cmd[512];
    FILE *stream;
    int iRet;

    if (!path || !md5_buf || buf_len < 33)
    {
        return -1;
    }
    memset(md5_buf, 0, (size_t)buf_len);
    snprintf(cmd, sizeof(cmd), "md5sum %s", path);
    stream = popen(cmd, "r");
    if (!stream)
    {
        return -1;
    }
    if (fgets(md5_buf, 33, stream) == NULL)
    {
        pclose(stream);
        return -1;
    }
    iRet = pclose(stream);
    if (iRet != 0)
    {
        return -1;
    }
    return 0;
}

static int auto_ota_download_and_flash(const anjrec_fw_meta_t *meta)
{
    unsigned long long file_len = 0;
    unsigned long expect_size;
    char local_md5[40] = {0};
    APPBIN_UPDATE_DATA updateData = {0};

    if (meta == NULL || meta->downurl[0] == '\0')
    {
        return -1;
    }

    if (auto_ota_is_busy())
    {
        return -1;
    }

    __INFO("auto_ota: teardown anj_ser before download\n");
    modules_uninit("anj_ser", NULL);

    unlink(AUTO_OTA_FW_PATH);
    __INFO("auto_ota: wget '%s' -> %s\n", meta->downurl, AUTO_OTA_FW_PATH);
    {
        /* anj_mw_system_with_param only has 256B; OSS URLs need a larger buffer. */
        char cmd[768];
        snprintf(cmd, sizeof(cmd), "wget -c -T 60 -O %s '%s'", AUTO_OTA_FW_PATH, meta->downurl);
        anj_mw_system(cmd);
    }

    if (!anj_mw_file_exists(AUTO_OTA_FW_PATH))
    {
        __ERR("auto_ota: download failed, file missing\n");
        return -1;
    }

    if (anj_mw_read_file_len(AUTO_OTA_FW_PATH, &file_len) != 0 || file_len == 0)
    {
        __ERR("auto_ota: read file len failed\n");
        unlink(AUTO_OTA_FW_PATH);
        return -1;
    }

    if (meta->file_size[0] != '\0')
    {
        expect_size = strtoul(meta->file_size, NULL, 10);
        if (expect_size == 0 || (unsigned long long)expect_size != file_len)
        {
            __ERR("auto_ota: file_size mismatch expect=%s actual=%llu\n",
                  meta->file_size, file_len);
            unlink(AUTO_OTA_FW_PATH);
            return -1;
        }
    }

    if (meta->md5[0] != '\0')
    {
        if (auto_ota_file_md5(AUTO_OTA_FW_PATH, local_md5, sizeof(local_md5)) != 0)
        {
            __ERR("auto_ota: calc md5 failed\n");
            unlink(AUTO_OTA_FW_PATH);
            return -1;
        }
        if (strcasecmp(local_md5, meta->md5) != 0)
        {
            __ERR("auto_ota: md5 mismatch expect=%s actual=%s\n", meta->md5, local_md5);
            unlink(AUTO_OTA_FW_PATH);
            return -1;
        }
    }

    __INFO("auto_ota: verify ok, size=%llu md5=%s, start update\n",
           file_len, meta->md5[0] ? meta->md5 : "(skip)");
    snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", AUTO_OTA_FW_PATH);
    updateData.nPhyAddr = 0;
    updateData.nFileLen = (unsigned int)file_len;
    anj_sysmng_app_update(&updateData);
    return 0;
}

static int anjrec_auto_ota_thread(void *ctx, int *bStart)
{
    anjrec_fw_meta_t meta;
    int attempt;
    int match_ret;

    (void)ctx;

    if (auto_ota_wait_network(bStart) != 0)
    {
        __ERR("auto_ota: network not ready, give up\n");
        return -1;
    }

    if (auto_ota_wait_gid(bStart) != 0)
    {
        __ERR("auto_ota: DevGID(SN) not ready, give up\n");
        return -1;
    }

    if (!bStart || !*bStart)
    {
        return -1;
    }

    if (auto_ota_is_busy())
    {
        __INFO("auto_ota: skip because busy\n");
        return 0;
    }

    match_ret = -1;
    for (attempt = 0; attempt < AUTO_OTA_MATCH_RETRY && bStart && *bStart; attempt++)
    {
        memset(&meta, 0, sizeof(meta));
        match_ret = anjrec_version_match_query(&meta);
        if (match_ret == 0)
        {
            break;
        }
        if (match_ret == 1)
        {
            __INFO("auto_ota: no firmware package available\n");
            return 0;
        }
        __ERR("auto_ota: version_match failed ret=%d, retry=%d/%d\n",
              match_ret, attempt + 1, AUTO_OTA_MATCH_RETRY);
        if (attempt + 1 < AUTO_OTA_MATCH_RETRY)
        {
            sleep(AUTO_OTA_MATCH_RETRY_INTERVAL_SEC);
        }
    }

    if (match_ret != 0)
    {
        __ERR("auto_ota: version_match gave up\n");
        return -1;
    }

    if (!bStart || !*bStart)
    {
        return -1;
    }

    if (auto_ota_download_and_flash(&meta) != 0)
    {
        __ERR("auto_ota: download/flash failed, stay in recovery\n");
        return -1;
    }

    return 0;
}

int anjrec_auto_ota_init(void)
{
    if (s_stAutoOtaThread.start)
    {
        return 0;
    }

    memset(&s_stAutoOtaThread, 0, sizeof(s_stAutoOtaThread));
    s_stAutoOtaThread.bAutoDestroy = 1;
    strncpy(s_stAutoOtaThread.iThreadName, "anjrec_auto_ota",
            sizeof(s_stAutoOtaThread.iThreadName) - 1);
    s_stAutoOtaThread.iThreadjob.ctx = &s_stAutoOtaThread;
    s_stAutoOtaThread.iThreadjob.func = anjrec_auto_ota_thread;
    return anj_thread_task_create(&s_stAutoOtaThread);
}

void anjrec_auto_ota_uninit(void)
{
    anj_thread_task_destroy(&s_stAutoOtaThread, -1);
    memset(&s_stAutoOtaThread, 0, sizeof(s_stAutoOtaThread));
}
