#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_log.h"

#define NVMEN_SIZE                  64
#define EFUSE_PATH                  "/sys/bus/nvmem/devices/ts-ocotp0/nvmem"

/*
 * Boot layout convention (SPI NOR):
 *   [ uboot / other ...... | SN 4K | ENV 4K ]
 * Last 8K: SN then ENV. Sector size 4K.
 * size >= 256K (192K not supported). Formula:
 *   sn_sect  = size / SECTOR - 2
 *   env_sect = size / SECTOR - 1
 */
#define SN_BOOT_SECTOR_SIZE         (4 * 1024)
#define SN_BOOT_MTD_SIZE_MIN        0x40000

#define MAGIC_P2PID_1               0x404E4A50
#define MAGIC_P2PID_2               0x32504944

typedef struct
{
    int ready;
    int mtd_size;
    int sn_sect;
    int env_sect;
} sn_boot_layout_t;

static int sn_boot_layout_get(sn_boot_layout_t *out)
{
    static sn_boot_layout_t s_layout = {0};

    if (out == NULL)
    {
        return -1;
    }

    if (!s_layout.ready)
    {
        int size = anj_mw_mtd_size_get(UBOOT_BLOCK_MTD);
        if (size < SN_BOOT_MTD_SIZE_MIN || (size % SN_BOOT_SECTOR_SIZE) != 0)
        {
            __ERR("boot mtd size %d invalid (need >=256K, 4K align)\n", size);
            return -1;
        }

        s_layout.mtd_size = size;
        s_layout.env_sect = size / SN_BOOT_SECTOR_SIZE - 1;
        s_layout.sn_sect = s_layout.env_sect - 1;
        s_layout.ready = 1;
        __INFO("boot layout size=0x%x sn=%d env=%d\n",
               s_layout.mtd_size, s_layout.sn_sect, s_layout.env_sect);
    }

    *out = s_layout;
    return 0;
}

int platform_sn_mtd_size_get()
{
    sn_boot_layout_t layout;

    if (sn_boot_layout_get(&layout) != 0)
    {
        return -1;
    }

    return layout.mtd_size;
}

int platform_sn_sect_no_get()
{
    sn_boot_layout_t layout;

    if (sn_boot_layout_get(&layout) != 0)
    {
        return -1;
    }

    return layout.sn_sect;
}

int platform_uboot_env_no_get()
{
    sn_boot_layout_t layout;

    if (sn_boot_layout_get(&layout) != 0)
    {
        return -1;
    }

    return layout.env_sect;
}

int platform_p2pid_write(char *buf, int len)
{
    return 0;
}

int platform_p2pid_read(char *buffer, int buffersize, unsigned int nType)
{
    return 0;
}

int platform_phymem_ability_get()
{
    return 1;
}

const char *platform_inner_uuid_get()
{
    static char sz_uuid[128] = {0};
    static uint32_t efuse_buf[NVMEN_SIZE] = {0};
    static int efuse_read = 0;
    uint64_t u64ChipId = 0;

    if (sz_uuid[0] != '\0')
    {
        return sz_uuid;
    }

    if (!efuse_read)
    {
        FILE *fp = fopen(EFUSE_PATH, "rb");
        if (fp == NULL)
        {
            __ERR("fopen error! Path: %s\n", EFUSE_PATH);
            return sz_uuid;
        }

        if (fread(efuse_buf, sizeof(uint32_t), NVMEN_SIZE, fp) < 1)
        {
            __ERR("read efuse error! Path: %s\n", EFUSE_PATH);
            fclose(fp);
            return sz_uuid;
        }
        fclose(fp);
        efuse_read = 1;
    }

    if ((efuse_buf[4] != 0) || (efuse_buf[5] != 0))
    {
        u64ChipId = ((uint64_t)efuse_buf[4] << 32) | (uint64_t)efuse_buf[5];
        snprintf(sz_uuid, sizeof(sz_uuid), "%llx", (unsigned long long)u64ChipId);
    }
    else
    {
        __ERR("no valid chip id in efuse\n");
    }

    return sz_uuid;
}
