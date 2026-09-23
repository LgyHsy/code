#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_watchdog.h"
#include "anj_sysmng.h"
#include "anjrec_flash.h"

/*
 * Intercept anj_sysmng flash exec ("/tmp/anjflash ... &") and run in-process flash.
 * Keeps anj_sysmng_app_update validation; only replaces external anjflash.
 */
__attribute__((used))
int __wrap_anj_mw_system_with_param(const char *fmt, ...)
{
    char content_buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(content_buf, sizeof(content_buf), fmt, ap);
    va_end(ap);

    if (strstr(content_buf, "anjflash") != NULL)
    {
        return anjrec_flash_from_shell_cmd(content_buf);
    }

    return anj_mw_system(content_buf);
}

/*
 * Strong override of weak anj_sysmng_apply_firmware_update in module/sysmng.
 * Recovery skips full firmware_update teardown and flashes in-process.
 */
int anj_sysmng_apply_firmware_update(APPBIN_UPDATE_DATA *updateData, char *bufPtr, int nFileLen)
{
    (void)bufPtr;
    (void)nFileLen;

    getDevInfo()->bUpgrading = 1;
    WatchDogSetTimeOut(600, 0);
    __INFO("recovery: flash firmware %s (%d bytes)\n",
           updateData->filePath, nFileLen);
    return anjrec_flash_apply_update(updateData);
}
