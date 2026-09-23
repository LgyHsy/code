#include "anjrec_uboot_env.h"

#include "anj_mw_comm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UBOOT_ENV_SECTOR_SIZE   (4 * 1024)
#define UBOOT_ENV_TOTAL_SIZE    (4 * 1024)
/* Boot layout: [uboot ... | SN 4K | ENV 4K], env = last sector */

static const unsigned int g_crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static unsigned int uboot_env_crc32(const char *data, int len)
{
    unsigned int crc = 0xFFFFFFFFU;
    int i;

    for (i = 0; i < len; i++)
    {
        crc = ((crc >> 8) & 0x00FFFFFFU) ^ g_crc32_table[(crc ^ (unsigned char)data[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFFU;
}

static unsigned int uboot_env_crc32_continue(unsigned int crc, const char *data, int len)
{
    int i;

    crc ^= 0xFFFFFFFFU;
    for (i = 0; i < len; i++)
    {
        crc = ((crc >> 8) & 0x00FFFFFFU) ^ g_crc32_table[(crc ^ (unsigned char)data[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFFU;
}

static int uboot_env_mtd_size_get(const char *mtd_name)
{
    char path[128];
    FILE *fp;
    unsigned long size = 0;

    if (mtd_name == NULL || *mtd_name == 0)
    {
        return -1;
    }

    snprintf(path, sizeof(path), "/sys/class/mtd/%s/size", mtd_name);
    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return -1;
    }

    if (fscanf(fp, "%lu", &size) != 1)
    {
        size = 0;
    }

    fclose(fp);
    return (int)size;
}

static unsigned int uboot_env_sect_no_get(void)
{
    static int s_ready = 0;
    static unsigned int s_env_sect = 0;

    if (!s_ready)
    {
        int size = uboot_env_mtd_size_get(UBOOT_BLOCK_MTD);

        if (size <= 0 || (size % UBOOT_ENV_SECTOR_SIZE) != 0)
        {
            printf("uboot mtd %s size %d invalid\n", UBOOT_BLOCK_MTD, size);
            return 0;
        }

        /* env is the last 4K sector */
        s_env_sect = (unsigned int)(size / UBOOT_ENV_SECTOR_SIZE - 1);
        s_ready = 1;
    }

    return s_env_sect;
}

static int uboot_env_flash_rw(int write_flag, const char *mtd_block,
                              unsigned int size_sect, unsigned int sect_number,
                              unsigned char *buffer)
{
    int fd;
    int ret = 0;
    ssize_t io_len;
    off_t offset;

    if (mtd_block == NULL || *mtd_block == 0 || buffer == NULL)
    {
        return -1;
    }

    fd = open(mtd_block, O_RDWR | O_SYNC);
    if (fd < 0)
    {
        return -1;
    }

    offset = (off_t)size_sect * sect_number;
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        close(fd);
        return -1;
    }

    if (write_flag == 0)
    {
        io_len = read(fd, buffer, size_sect);
    }
    else
    {
        io_len = write(fd, buffer, size_sect);
        if (io_len == (ssize_t)size_sect)
        {
            fsync(fd);
        }
    }

    if (io_len != (ssize_t)size_sect)
    {
        ret = -1;
    }

    close(fd);
    return ret;
}

static int uboot_env_write_sector(const char *mtd_block, unsigned int sect_number,
                                  unsigned char *buffer, unsigned int size_sect)
{
    return uboot_env_flash_rw(1, mtd_block, size_sect, sect_number, buffer);
}

static int uboot_env_update_crc(char *sector_buf, unsigned int size_sect,
                                const char *read_mtd, unsigned int sect_number,
                                unsigned int env_total_size)
{
    unsigned int crc;
    char *tmp = NULL;
    unsigned int sects;
    unsigned int i;

    crc = uboot_env_crc32(sector_buf + 4, (int)size_sect - 4);
    if (env_total_size > size_sect)
    {
        tmp = (char *)malloc(size_sect);
        if (tmp == NULL)
        {
            return -1;
        }

        sects = env_total_size / size_sect;
        for (i = 1; i < sects; i++)
        {
            if (uboot_env_flash_rw(0, read_mtd, size_sect, sect_number + i,
                                   (unsigned char *)tmp) != 0)
            {
                free(tmp);
                return -1;
            }
            crc = uboot_env_crc32_continue(crc, tmp, (int)size_sect);
        }
        free(tmp);
    }

    *(unsigned int *)sector_buf = crc;
    return 0;
}

static int uboot_env_commit(char *sector_buf, const char *read_mtd, const char *write_mtd,
                            unsigned int sect_number, unsigned int size_sect,
                            unsigned int env_total_size)
{
    if (uboot_env_update_crc(sector_buf, size_sect, read_mtd, sect_number, env_total_size) != 0)
    {
        return -1;
    }

    return uboot_env_write_sector(write_mtd, sect_number,
                                (unsigned char *)sector_buf, size_sect);
}

int anjrec_uboot_env_set(const char *name, const char *value, int verbose)
{
    char cmd[512] = {0};
    char findstr[64] = {0};
    char old_entry[512] = {0};
    char *sector_buf = NULL;
    char *sector_modify = NULL;
    const char *read_mtd = UBOOT_MTD_BLOCK;
    const char *write_mtd = UBOOT_MTD_BLOCK_ENV;
    unsigned int size_sect = UBOOT_ENV_SECTOR_SIZE;
    unsigned int env_total_size = UBOOT_ENV_TOTAL_SIZE;
    unsigned int sect_number = uboot_env_sect_no_get();
    int pos_from = -1;
    int pos_end = -1;
    int i;
    int ret = -1;

    if (name == NULL || *name == 0)
    {
        return -1;
    }

    if (value != NULL && *value != 0)
    {
        snprintf(cmd, sizeof(cmd), "%s=%s", name, value);
    }

#if !SUPPORT_NAND_FLASH
    {
        int mtd_size = uboot_env_mtd_size_get(UBOOT_BLOCK_MTD);

        if (mtd_size <= 0)
        {
            if (verbose)
            {
                printf("uboot mtd %s size %d error\n", UBOOT_BLOCK_MTD, mtd_size);
            }
            return -1;
        }
    }
#endif

    sector_buf = (char *)malloc(size_sect);
    sector_modify = (char *)malloc(size_sect);
    if (sector_buf == NULL || sector_modify == NULL)
    {
        ret = -1;
        goto exit;
    }

    if (uboot_env_flash_rw(0, read_mtd, size_sect, sect_number,
                           (unsigned char *)sector_buf) != 0)
    {
        if (verbose)
        {
            printf("read uboot env sector %u failed\n", sect_number);
        }
        ret = -1;
        goto exit;
    }

    for (i = 4; i < (int)size_sect; i++)
    {
        if ((unsigned char)sector_buf[i] == 0xff)
        {
            sector_buf[i] = 0;
        }
    }

    memcpy(sector_modify, sector_buf, size_sect);
    snprintf(findstr, sizeof(findstr), "%s=", name);

    for (i = 0; i < (int)size_sect; i++)
    {
        char *p = sector_modify + i;

        if (strncmp(p, findstr, strlen(findstr)) == 0)
        {
            if (i > 4 && *(p - 1) != 0)
            {
                continue;
            }
            pos_from = i;
            break;
        }
    }

    if (pos_from > 0)
    {
        for (i = pos_from; i < (int)size_sect; i++)
        {
            if (sector_modify[i] == 0)
            {
                pos_end = i;
                break;
            }
        }
    }

    if (pos_from > 0 && pos_end > 0)
    {
        int old_len = pos_end - pos_from;

        if (old_len <= 0 || old_len >= (int)sizeof(old_entry))
        {
            ret = -1;
            goto exit;
        }

        memcpy(old_entry, sector_modify + pos_from, old_len);
        if (verbose)
        {
            printf("old: %s\n", old_entry);
        }

        if (strcmp(old_entry, cmd) == 0)
        {
            ret = 0;
            goto exit;
        }

        if (verbose)
        {
            printf("new: %s\n", cmd);
        }

        strcpy(sector_modify + pos_from, cmd);

        {
            char *original_tail = sector_buf + pos_end;
            char *new_tail = sector_modify + pos_from + strlen(cmd);
            int original_left = (int)size_sect - pos_end;
            int new_left = (int)size_sect - pos_from - (int)strlen(cmd);
            int copy_len = original_left < new_left ? original_left : new_left;

            if (strlen(cmd) == 0)
            {
                new_tail -= 1;
            }

            memcpy(new_tail, original_tail, (size_t)copy_len);
        }

        if (uboot_env_commit(sector_modify, read_mtd, write_mtd, sect_number,
                             size_sect, env_total_size) != 0)
        {
            ret = -1;
            goto exit;
        }

        ret = 1;
        goto exit;
    }

    if (pos_from < 0 && pos_end < 0)
    {
        for (i = 0; i < (int)size_sect; i++)
        {
            if (sector_modify[i] == 0 && sector_modify[i + 1] == 0)
            {
                pos_from = i + 1;
                break;
            }
        }

        if (pos_from > 0)
        {
            strcpy(sector_modify + pos_from, cmd);
            if (uboot_env_commit(sector_modify, read_mtd, write_mtd, sect_number,
                                 size_sect, env_total_size) != 0)
            {
                ret = -1;
                goto exit;
            }
            ret = 1;
            goto exit;
        }
    }

    ret = -1;

exit:
    free(sector_modify);
    free(sector_buf);
    return ret;
}

int anjrec_uboot_env_get(const char *name, char *buf, int buflen)
{
    char findstr[64];
    char *sector_buf = NULL;
    const char *read_mtd = UBOOT_MTD_BLOCK;
    unsigned int size_sect = UBOOT_ENV_SECTOR_SIZE;
    unsigned int sect_number = uboot_env_sect_no_get();
    int pos_from = -1;
    int pos_end = -1;
    int i;
    int ret = -1;

    if (name == NULL || *name == 0 || buf == NULL || buflen <= 0)
    {
        return -1;
    }

    buf[0] = '\0';
    sector_buf = (char *)malloc(size_sect);
    if (sector_buf == NULL)
    {
        return -1;
    }

    if (uboot_env_flash_rw(0, read_mtd, size_sect, sect_number,
                           (unsigned char *)sector_buf) != 0)
    {
        free(sector_buf);
        return -1;
    }

    for (i = 4; i < (int)size_sect; i++)
    {
        if ((unsigned char)sector_buf[i] == 0xff)
        {
            sector_buf[i] = 0;
        }
    }

    snprintf(findstr, sizeof(findstr), "%s=", name);
    for (i = 0; i < (int)size_sect; i++)
    {
        char *p = sector_buf + i;

        if (strncmp(p, findstr, strlen(findstr)) == 0)
        {
            if (i > 4 && *(p - 1) != 0)
            {
                continue;
            }
            pos_from = i + (int)strlen(findstr);
            break;
        }
    }

    if (pos_from > 0)
    {
        for (i = pos_from; i < (int)size_sect; i++)
        {
            if (sector_buf[i] == 0)
            {
                pos_end = i;
                break;
            }
        }

        if (pos_end > pos_from)
        {
            int copy_len = pos_end - pos_from;
            if (copy_len >= buflen)
            {
                copy_len = buflen - 1;
            }
            memcpy(buf, sector_buf + pos_from, (size_t)copy_len);
            buf[copy_len] = '\0';
            ret = 0;
        }
    }

    free(sector_buf);
    return ret;
}
