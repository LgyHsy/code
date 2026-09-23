#ifndef DRIVER_INTERFACE_H
#define DRIVER_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

int FlashIsSpiNandFlash();
const char *GET_SN_MTD_DEV();
const char *GET_SN_MTD_BLOCK();
const char *GET_SN_MTD_NAME();
const char *GET_KERNEL_MTD_DEV();
const char *GET_KERNEL_MTD_DEV_BK();
const char *GET_KERNEL_MTD_NAME();
const char *GET_FILESYS_MTD_DEV();
const char *GET_FILESYS_MTD_DEV_BK();
const char *GET_OEM_MTD_DEV();
const char *GET_OEM_MTD_BLOCK();
const char *GET_DATA_BLOCK1_DEV();
const char *GET_DATA_BLOCK1();
const char *GET_UBOOT_MTD_DEV();
const char *GET_UBOOT_MTD_BLOCK();
const char *GET_UBOOT_MTD_DEV_BK();
const char *GET_UBOOT_MTD_BLOCK_BK();
const char *GET_UBOOTENV_MTD_NAME();
const char *GET_UBOOTENV_MTD_BLOCK();

#if defined (__cplusplus)
}
#endif

#endif

