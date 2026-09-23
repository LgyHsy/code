#ifndef ANJREC_FLASH_H
#define ANJREC_FLASH_H

#include "anj_sysmng.h"

int anjrec_flash_apply_update(APPBIN_UPDATE_DATA *updateData);

/* Run in-process MTD flash from a firmware file on disk. */
int anjrec_flash_from_shell_cmd(const char *cmd);

#endif
