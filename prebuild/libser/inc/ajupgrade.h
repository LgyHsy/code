#ifndef _AJ_UPGRADE_LIB_API_H_
#define _AJ_UPGRADE_LIB_API_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C"
{
#endif

#define AJ_SUCC 0
#define AJ_ERROR -1

typedef void (*aj_p2p_log_cb)(const char *fmt, ...);

/*************************************************
Function:         aj_get_firmware_callback
Description:      The callbcack of aj firmware upgrade infomation
result:			  0: get ok, otherwace not OK
firmwarename:	  firmware full path name. if NULL OR empty means no need to upgrade.
releasenotes:
Return:           The total number of data successfully load is returned.
*************************************************/
typedef int (*aj_get_firmware_callback)(int result, const char *latestversion, const char *firmwarename, const char *releasenotes);

/*************************************************
Function:         aj_firmware_before_download_cb
Description:      The callbcack before downloading firmware, so that caller free memory for storage firmware in memory
*************************************************/
typedef void (*aj_firmware_before_download_cb)();

int AjUpgradeApiStart(
    const char *p_szSerialNo,   // 序列号，最长31位
    unsigned int devvalue,      // 设备类型值，为0则填写下面的devicetype
    const char *p_szDeviceType, // 设备型号，最长31位
    const char *p_szCustomName, // 定制商名称,最长31位
    const char *p_szVersion,    // 版本号，V开头，支持1-4位,如V1.0.3.2
    const char *p_szUUID,       // 芯片唯一标识字符串
    const char *p_savePath,     // 固件下载路径
    int bGetFirmware,           // 是否下载固件。为0则仅获取最新版本号和releasenotes
    const char *szserveraddr,   // 升级服务器地址。为空则使用默认地址
    aj_firmware_before_download_cb cb_beforedownload,
    aj_get_firmware_callback cb_checkok,
    aj_p2p_log_cb cblog);
void AjUpgradeApiStop();

#ifdef __cplusplus
}
#endif

#endif
