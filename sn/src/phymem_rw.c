#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "platform_sn.h"
#include "phymem_rw.h"

#define PAGE_SIZE   (4096)
#define PHY_MEM_MAP_BASE_OFFSET     0x20000000      //虚拟内存基地址

static int cmd_get_value(const char *str, const char *name, char *buffer, int buflen)
{
    int iIndex=0;
    const char *pIndex = strstr(str, name);

    if(pIndex)
    {
        pIndex += strlen(name);
        if(*pIndex == '=')
        {
            pIndex++;
            for(iIndex = 0; (iIndex<(buflen-1)) && (*pIndex != '&') && (*pIndex != ' ') && (*pIndex != '\0'); iIndex++, pIndex++)
            {
                buffer[iIndex] = *pIndex;
            }
        }
    }

    if(iIndex > 0)
        buffer[iIndex] = '\0';

    return iIndex;
}

unsigned int string_atoi(const char *str) 
{
    if( strncasecmp(str, "0x", 2) == 0)
    {
        unsigned int data = 0;
        if(sscanf(str + 2, "%x", &data) == 1)
        {
            return data;
        }
    }
    else
    {
        unsigned int data = 0;
        if(sscanf(str, "%u", &data) == 1)
        {
            return data;
        }
    }

    return 0;
}

unsigned int phy_mem_total()
{
    unsigned int data = 0;

    int phymem_ability = platform_phymem_ability_get();
    if (phymem_ability <= 0)
    {
        return 0;
    }
    
    char buffer[256] = {0};
	FILE *pFile = anj_mw_fopen("/proc/cmdline", "r");
    if (NULL == pFile)
    {
        return 0;
    }

	anj_mw_fread(pFile, buffer, sizeof(buffer));
	anj_mw_fclose(pFile);

    char szvalue[64] = {0};
    cmd_get_value(buffer, "LX_MEM", szvalue, sizeof(szvalue));
    data = string_atoi(szvalue);

	return data;
}

int phy_mem_read(unsigned int addr, char *buffer, int buflen)
{
    int ret = -1;
    int phymem_ability = platform_phymem_ability_get();
    if (phymem_ability <= 0)
    {
        return -1;
    }

    unsigned int totalmen = phy_mem_total();
    if( addr >= totalmen)
    {
        __ERR("phymem %#x > %#x\n", addr, totalmen);
        return -1;
    }

    if( addr + buflen >= totalmen)
    {
        __ERR("phymem %#x+%d > %#x\n", addr, buflen, totalmen);
        return -1;
    }

    addr += PHY_MEM_MAP_BASE_OFFSET;

    int fd = -1;
    void *map_base = NULL;
    void *virt_addr = NULL;    
    
    size_t page_size = PAGE_SIZE;
    unsigned int phy_base = addr & ~(page_size - 1);
    size_t map_size = page_size;
    unsigned int offset = addr - phy_base;

    __INFO("phymem addr=%#x, page_size=%#x, phy_base=%#x, offset=%#x, buflen=%d\n", 
        addr, page_size, phy_base, offset, buflen);

    fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd == -1)
    {
        __ERR("open /dev/mem failed.\n");
        return -1;
    }

    map_base = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fd, phy_base);
    if (map_base == MAP_FAILED)
    {
        close(fd);
        fd = -1;
        __ERR("phymem mmap /dev/mem failed.\n");
        return -1;
    }

    virt_addr = (void*)((uintptr_t)map_base + offset);
    memcpy(buffer, virt_addr, buflen);
    ret = 0;

    if (munmap(map_base, map_size) == -1)
    {
        __ERR("phymem munmap /dev/mem failed.\n");
    }

    close(fd);
    fd = -1;
  
	return ret;
}



int phy_mem_write(unsigned int addr, char *buffer, int buflen)
{
    int ret = -1;
    int phymem_ability = platform_phymem_ability_get();
    if (phymem_ability <= 0)
    {
        return -1;
    }

    unsigned int totalmen = phy_mem_total();
    if( addr >= totalmen)
    {
        __ERR("phymem %#x > %#x\n", addr, totalmen);
        return -1;
    }

    if( addr + buflen >= totalmen)
    {
        __ERR("phymem %#x+%d > %#x\n", addr, buflen, totalmen);
        return -1;
    }

    addr += PHY_MEM_MAP_BASE_OFFSET;

    int fd = -1;
    void *map_base = NULL;
    void *virt_addr = NULL;    

    size_t page_size = PAGE_SIZE;
    unsigned int phy_base = addr & ~(page_size - 1);
    size_t map_size = page_size;
    unsigned int offset = addr - phy_base;
    __INFO("phymem addr=%#x, page_size=%#x, phy_base=%#x, offset=%#x, buflen=%d\n", 
        addr, page_size, phy_base, offset, buflen);

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1)
    {
        __ERR("phymem open /dev/mem failed.\n");
        return -1;
    }

    map_base = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phy_base);
    if (map_base == MAP_FAILED)
    {
        close(fd);
        fd = -1;
        __ERR("phymem mmap /dev/mem failed.\n");
        return -1;
    }

    virt_addr = (void*)((uintptr_t)map_base + offset);
    memcpy(virt_addr, buffer, buflen);   
    ret = 0;

    msync(virt_addr, buflen, MS_SYNC);
    if (munmap(map_base, map_size) == -1)
    {
        __ERR("phymem munmap /dev/mem failed.\n");
    }

    close(fd);
    fd = -1;

	return ret;
}

