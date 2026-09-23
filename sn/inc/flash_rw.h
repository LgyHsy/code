#ifndef __FLASH_RW_H__
#define __FLASH_RW_H__

int flash_get_sect_param(const char* mtd_block, unsigned int *size_sect, unsigned int *total_sect);

int flash_sect_rw(int bWrite, const char* mtd_block, unsigned int size_sect, unsigned int sect_number, unsigned char *pSectBuffer);

#endif