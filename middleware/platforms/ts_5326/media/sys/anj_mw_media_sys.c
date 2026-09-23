#include "anj_mw_media_common.h"
#include "anj_mw_media_sys.h"

#include "anj_mw_comm.h"

char *anj_mw_media_sys_mmap(unsigned long long nPhyAddr, unsigned int len)
{
    unsigned int mapsize = ANJ_ALIGN_UP(len, 1024 * 1024);
    return TS_Common_SysMmap(nPhyAddr, mapsize);
}

void anj_mw_media_sys_munmap(void *pVirtualAddress, unsigned int mapsize)
{
    TS_Common_SysMunmap(pVirtualAddress, mapsize);
}

int anj_mw_media_sys_alloc(unsigned int u32BlkSize, unsigned long long *phyAddr)
{
    u32BlkSize = ANJ_ALIGN_UP(u32BlkSize, 1024 * 1024);
    return TS_Common_SysMmz_Alloc(NULL, u32BlkSize, phyAddr, NULL);
}

void anj_mw_media_sys_free(unsigned long long phyAddr)
{
    TS_Common_SysMmz_Free(phyAddr, NULL);
}

int anj_mw_media_sys_temp_get()
{
    return 0;
}