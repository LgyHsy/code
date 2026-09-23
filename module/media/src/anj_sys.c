#include "anj_mw_comm.h"
#include "anj_mw_media_sys.h"

#include "anj_sys.h"

char *anj_sys_mmap(unsigned long long nPhyAddr, unsigned int len)
{
    return anj_mw_media_sys_mmap(nPhyAddr, len);
}

void anj_sys_munmap(void *pVirtualAddress, unsigned int mapsize)
{
    anj_mw_media_sys_munmap(pVirtualAddress, mapsize);
}

int anj_sys_alloc(unsigned int u32BlkSize, unsigned long long *phyAddr)
{
    return anj_mw_media_sys_alloc(u32BlkSize, phyAddr);
}

void anj_sys_free(unsigned long long phyAddr)
{
    anj_mw_media_sys_free(phyAddr);
}
