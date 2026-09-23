#ifndef __ANJ_SYS_H__
#define __ANJ_SYS_H__

#ifdef __cplusplus
extern "C"
{
#endif

char *anj_sys_mmap(unsigned long long nPhyAddr, unsigned int len);
void anj_sys_munmap(void *pVirtualAddress, unsigned int mapsize);

int anj_sys_alloc(unsigned int u32BlkSize, unsigned long long *phyAddr);
void anj_sys_free(unsigned long long phyAddr);


#ifdef __cplusplus
}
#endif

#endif
