#ifndef _ANJ_MW_MEDIA_SYS_H_
#define _ANJ_MW_MEDIA_SYS_H_

#include "sdk_option.h"

#ifdef __cplusplus
extern "C"
{
#endif

char *anj_mw_media_sys_mmap(unsigned long long nPhyAddr, unsigned int len);
void anj_mw_media_sys_munmap(void *pVirtualAddress, unsigned int mapsize);

int anj_mw_media_sys_alloc(unsigned int u32BlkSize, unsigned long long *phyAddr);
void anj_mw_media_sys_free(unsigned long long phyAddr);

int anj_mw_media_sys_temp_get(void);

#ifdef __cplusplus
}
#endif

#endif
