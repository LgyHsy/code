#include "anj_mw_media_common.h"
#include "anj_mw_media_sys.h"

#include "anj_mw_comm.h"

char *anj_mw_media_sys_mmap(unsigned long long nPhyAddr, unsigned int len)
{
    unsigned int mapsize = ALIGN_UP(len, 1024 * 1024);
    return ST_Common_SysMmap(nPhyAddr, mapsize);
}

void anj_mw_media_sys_munmap(void *pVirtualAddress, unsigned int mapsize)
{
    ST_Common_SysMunmap(pVirtualAddress, mapsize);
}

int anj_mw_media_sys_alloc(unsigned int u32BlkSize, unsigned long long *phyAddr)
{
    u32BlkSize = ALIGN_UP(u32BlkSize, 1024 * 1024);
    return ST_Common_SysMma_Alloc((unsigned char *)"mma_heap_name0", u32BlkSize, phyAddr);
}

void anj_mw_media_sys_free(unsigned long long phyAddr)
{
    ST_Common_SysMma_Free(phyAddr);
}

int anj_mw_media_sys_temp_get()
{
    int temp_value = 0;
    char buffer[64] = {0};
    anj_mw_read_file_limit_len("/sys/devices/virtual/mstar/msys/TEMP_R", buffer, sizeof(buffer));
    if (strlen(buffer) > 0)
    {
        const char *pIdentiry = "Temperature ";
        const char *p = strstr(buffer, pIdentiry);
        if (NULL != p)
        {
            p += strlen(pIdentiry);
            temp_value = atoi(p);
        }
    }
    return temp_value;
}