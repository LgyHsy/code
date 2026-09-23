#ifndef __PHYMEM_RW_H__
#define __PHYMEM_RW_H__

unsigned int phy_mem_total();

int phy_mem_read(unsigned int addr, char *buffer, int buflen);

int phy_mem_write(unsigned int addr, char *buffer, int buflen);


#endif
