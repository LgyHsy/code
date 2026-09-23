#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "anj_mw_comm.h"
#include "driver_interface.h"

int FlashIsSpiNandFlash()
{
	return SUPPORT_NAND_FLASH;
}

const char *GET_SN_MTD_DEV()
{
	return SN_MTD_DEV;
}

const char *GET_SN_MTD_BLOCK()
{
	return SN_BLOCK;
}
const char *GET_SN_MTD_NAME()
{
	return SN_BLOCK_MTD;
}

const char *GET_KERNEL_MTD_DEV()
{
	return KERNEL_BLOCK;
}

const char *GET_KERNEL_MTD_DEV_BK()
{
	return KERNEL_BLOCK_BK;
}

const char *GET_KERNEL_MTD_NAME()
{
	return KERNEL_BLOCK_MTD;
}

const char *GET_FILESYS_MTD_DEV()
{
	return FILESYS_BLOCK;
}

const char *GET_FILESYS_MTD_DEV_BK()
{
	return FILESYS_BLOCK_BK;
}

const char *GET_OEM_MTD_DEV()
{
	return OEM_MTD_DEV;
}

const char *GET_OEM_MTD_BLOCK()
{
	return OEM_BLOCK;
}

const char *GET_DATA_BLOCK1_DEV()
{
	return DATA_BLOCK1_DEV;
}

const char *GET_DATA_BLOCK1()
{
	return DATA_BLOCK1;
}

const char *GET_UBOOT_MTD_DEV()
{
	return UBOOT_MTD_DEV;
}

const char *GET_UBOOT_MTD_BLOCK()
{
	return UBOOT_MTD_BLOCK;
}

const char *GET_UBOOT_MTD_DEV_BK()
{
	return UBOOT_MTD_DEV_BK;
}

const char *GET_UBOOT_MTD_BLOCK_BK()
{
	return UBOOT_MTD_BLOCK_BK;
}

const char *GET_UBOOTENV_MTD_NAME()
{
	return UBOOT_BLOCK_MTD;
}

const char *GET_UBOOTENV_MTD_BLOCK()
{
	return UBOOT_MTD_BLOCK_ENV;
}
