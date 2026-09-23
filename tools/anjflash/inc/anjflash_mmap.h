#ifndef ANJFLASH_MMAP_H
#define ANJFLASH_MMAP_H

int anjflash_mmap_phys(unsigned long long phy_addr, unsigned int map_size, void **vir_out);
void anjflash_munmap_phys(void *vir, unsigned int map_size);

#endif
