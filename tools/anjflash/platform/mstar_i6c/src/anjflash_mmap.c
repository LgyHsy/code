#include <stdio.h>

#include "anjflash_mmap.h"
#include "mi_sys.h"

int anjflash_mmap_phys(unsigned long long phy_addr, unsigned int map_size, void **vir_out)
{
    void *vir = NULL;
    MI_S32 ret;

    if (vir_out == NULL || phy_addr == 0 || map_size == 0)
    {
        return -1;
    }

    ret = MI_SYS_Init(0);
    if (ret != MI_SUCCESS)
    {
        printf("MI_SYS_Init failed: %#x\n", ret);
        return -1;
    }

    ret = MI_SYS_Mmap(phy_addr, map_size, &vir, 1);
    if (ret != MI_SUCCESS || vir == NULL)
    {
        printf("MI_SYS_Mmap phys %#llx size %u failed: %#x\n", phy_addr, map_size, ret);
        return -1;
    }

    if (MI_SYS_FlushInvCache(vir, map_size) != MI_SUCCESS)
    {
        printf("MI_SYS_FlushInvCache failed\n");
        MI_SYS_Munmap(vir, map_size);
        return -1;
    }

    *vir_out = vir;
    printf("mma mmap ok phys=%#llx map_size=%u vir=%p\n", phy_addr, map_size, vir);
    return 0;
}

void anjflash_munmap_phys(void *vir, unsigned int map_size)
{
    if (vir != NULL && map_size > 0)
    {
        MI_SYS_Munmap(vir, map_size);
    }
}
