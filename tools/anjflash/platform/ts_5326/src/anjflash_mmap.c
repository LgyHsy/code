#include <stdio.h>

#include "anjflash_mmap.h"

extern int mpp_mem_init(void);
extern void *mpp_mem_mmap_by_phyaddr(unsigned long long phy_addr, unsigned int map_size);
extern int mpp_mem_munmap_by_viraddr(void *vir, unsigned int map_size);

int anjflash_mmap_phys(unsigned long long phy_addr, unsigned int map_size, void **vir_out)
{
    void *vir = NULL;

    if (vir_out == NULL || phy_addr == 0 || map_size == 0)
    {
        return -1;
    }

    if (mpp_mem_init() != 0)
    {
        printf("mpp_mem_init failed\n");
        return -1;
    }

    vir = mpp_mem_mmap_by_phyaddr(phy_addr, map_size);
    if (vir == NULL)
    {
        printf("mpp_mem_mmap_by_phyaddr phys %#llx size %u failed\n", phy_addr, map_size);
        return -1;
    }

    *vir_out = vir;
    printf("mmz mmap ok phys=%#llx map_size=%u vir=%p\n", phy_addr, map_size, vir);
    return 0;
}

void anjflash_munmap_phys(void *vir, unsigned int map_size)
{
    if (vir != NULL && map_size > 0)
    {
        mpp_mem_munmap_by_viraddr(vir, map_size);
    }
}
