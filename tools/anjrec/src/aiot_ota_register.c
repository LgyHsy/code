#include <string.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_systime.h"
#include "anj_ser_api.h"
#include "anj_module.h"
#include "anj_net.h"
#include "anj_bind.h"
#include "anj_aiot.h"
#include "file_receiver.h"
#include "cmd_def.h"
#include "record_log.h"

#include "gct_common.h"
#include "gct_types.h"
#include "gct_callback.h"
#include "gct_apiv4.h"
#include "aiot_cmd_recovery.h"

#define FIRM_UPLOAD_FILENAME "ota.bin"

GCT_INT32 anj_aiot_ota_version_cb(GCT_CHAR *appbuf, GCT_CHAR *solbuf, GCT_CHAR *date, GCT_CHAR *hardware)
{
    DevInfo *pstDevInfo = getDevInfo();
    const char *model = pstDevInfo->search_devicetype;

    if (model[0] == '\0')
    {
        model = pstDevInfo->subDevType;
    }

    strcpy(hardware, model);
    strcpy(appbuf, "public");
    strcpy(solbuf, "AIOT");
    strcpy(date, pstDevInfo->release_date);
    *(date + 8) = 0;

    __INFO("%s %s %s %s\n", appbuf, solbuf, hardware, date);
    return 0;
}

GCT_INT32 anj_aiot_ota_req_cb(const GCT_UINT64 nFirmFileLen, const GCT_CHAR *pMd5Value)
{
    __INFO("recovery ota req len=%lld md5=%s\n", (long long)nFirmFileLen, pMd5Value ? pMd5Value : "");
    return file_recver_init("/tmp/ota_firmware.bin", nFirmFileLen, UPLOAD_FIRMWARE_FILE_TYPE, 0, MSG_SRC_SER);
}

GCT_INT32 anj_aiot_ota_feed_cb(const GCT_CHAR *pBody, const GCT_UINT64 nBodyLen)
{
    file_recver_feed_data(pBody, nBodyLen);
    return 0;
}

GCT_INT32 anj_aiot_ota_start_cb(const GCT_CHAR *pFileAbsPath, const GCT_CHAR *pWebFileName)
{
    (void)pWebFileName;
    __INFO("recovery ota start path=%s\n", pFileAbsPath ? pFileAbsPath : "");
    file_recver_t *pFileReceiver = getFileRecver();
    if (pFileAbsPath && anj_mw_file_exists(pFileAbsPath) && pFileReceiver == NULL)
    {
        APPBIN_UPDATE_DATA updateData = {0};
        snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", pFileAbsPath);
        anj_sysmng_app_update(&updateData);
        return 0;
    }

    if (pFileReceiver)
    {
        file_recver_stop(0);

        APPBIN_UPDATE_DATA updateData = {0};
        snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", pFileReceiver->filename);
        updateData.nFileLen = pFileReceiver->writelen;
        anj_sysmng_app_update(&updateData);
    }
    return 0;
}

static GCT_INT32 anj_aiot_reboot_cb(GCT_BOOL bForce)
{
    (void)bForce;
    anj_sysmng_reboot();
    return 0;
}

static GCT_VOID anj_aiot_network_status_cb(GCT_CHAR *pIp, GCT_CHAR *pMask, GCT_CHAR *pGateway, GCT_CHAR *pMac)
{
    (void)pIp; (void)pMask; (void)pGateway; (void)pMac;
}

static GCT_INT32 anj_aiot_trans_push_cb(const GCT_CHAR *pBuf, const GCT_UINT32 nBufLen, const GCT_UINT32 sid)
{
    anjrec_aiot_cmd_push((char *)pBuf, nBufLen, sid);
    return 1;
}

void anj_aiot_callback_init(void)
{
    gct_cb_reg_firmupdate_getversion(anj_aiot_ota_version_cb);
    gct_cb_reg_firmupdate_req(anj_aiot_ota_req_cb);
    gct_cb_reg_firmupdate_start(anj_aiot_ota_start_cb);
    gct_cb_reg_firmupdate_body(anj_aiot_ota_feed_cb);
    gct_cb_reg_reboot_dev(anj_aiot_reboot_cb);
    gct_cb_reg_dev_network_information(anj_aiot_network_status_cb);
    gct_cb_reg_trans_channel(anj_aiot_trans_push_cb);
}
