#include <stdio.h>
#include <sys/mman.h>   //mmap
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "flash_rw.h"

int flash_get_sect_param(const char* mtd_block, unsigned int *size_sect, unsigned int *total_sect)
{
    int devfd = -1;
    *size_sect = 0;
    *total_sect = 0;

    if( NULL == mtd_block || *mtd_block == 0 )
    {
        __ERR("input mtd_block is empty!\n");
        return -1;
    }

    devfd = open(mtd_block, O_RDWR | O_SYNC);
    if (devfd < 0) 
    {
        __ERR("Open %s Failed!\n", mtd_block);
        return -1;
    }

    if (ioctl(devfd, BLKBSZGET, size_sect) < 0)
    {
        __ERR("Get BLKBSZSET Failed!\n");
        close(devfd);
        return -1;
    }

    if (ioctl(devfd, BLKGETSIZE, total_sect) < 0)
    {
        __ERR("Get BLKGETSIZE Failed!\n");
        close(devfd);
        return -1;
    }

    close(devfd);
    return 0;
}

int flash_sect_rw(int bWrite, const char* mtd_block, unsigned int size_sect, unsigned int sect_number, unsigned char *pSectBuffer)
{
    int	iRet = 0;
    int	devfd = -1;
    unsigned int offs = size_sect * sect_number;

    if( NULL == mtd_block || *mtd_block == 0 )
    {
        __ERR("input mtd_block is empty!\n");
        return -1;
    }

    devfd = open(mtd_block, O_RDWR | O_SYNC);
    if (devfd < 0) 
    {
        __ERR("Open %s Failed!\n", mtd_block);
        return -1;
    }

    lseek(devfd, offs, SEEK_SET);

    int readsize = 0;
    if(bWrite == 0)
    {
        readsize = safe_read(devfd, pSectBuffer, size_sect);
        if (readsize != size_sect)
        {
            iRet = -1;
            __ERR("read %s Error, return %d, error %d(%s)\n", mtd_block, iRet, errno, strerror(errno));
        }
    }
    else
    {
        readsize = safe_write(devfd, pSectBuffer, size_sect);
        if (readsize != size_sect)
        {
            iRet = -1;
            __ERR("write %s Error, return %d, error %d(%s)\n", mtd_block, iRet, errno, strerror(errno));
        }
        fsync(devfd);
    }

    close(devfd);
    return iRet;
}

