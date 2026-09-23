/*
 * 独立 MTD 刷写小工具。
 * 用法: anjflash <phy_addr> <map_size>
 *   MMA 模式通过平台 mmap 映射固件；phy_addr/map_size 均为 0 时读 /tmp/ota_firmware.bin。
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <linux/watchdog.h>
#include <mtd/mtd-user.h>

#include "anjflash_mmap.h"
#include "anjrec_uboot_env.h"
#include "anj_mw_comm.h"

#define ANJ_FLASH_FW_PATH "/tmp/ota_firmware.bin"

#define ROM_MD5_PATH "/mnt/nand/rom_md5.txt"
#define OTA_FLAG_PATH "/mnt/nand/flag.ota.updated"

#define FIRMWARE_MAGIC_NUMBER "ANJOY888"
#define FW_TYPE_KERNEL (1 << 0)
#define FW_TYPE_FILESYSTEM (1 << 1)

#define ANJ_FLASH_KERNEL_MTD KERNEL_BLOCK
#define ANJ_FLASH_ROOTFS_MTD FILESYS_BLOCK

#define MTD_IO_BUFSIZE (4 * 1024)

#define WATCH_DOG_FILE "/dev/watchdog"
#define WATCH_DOG_FORCE_RESET 0xF000

typedef struct
{
    char magicNumber[8];
    int header_crc;
    int file_size;
    int type;
    int kernel_start;
    int kernel_size;
    int kernel_crc;
    char kernel_version_info[256];
    int fs_start;
    int fs_size;
} anj_fw_header_t;

typedef struct
{
    char *fw;
    void *map_base;
    size_t map_size;
} anj_flash_phys_map_t;

static ssize_t flash_full_write(int fd, const void *buf, size_t len)
{
    ssize_t total = 0;
    const char *p = (const char *)buf;

    while (len > 0)
    {
        ssize_t cc = write(fd, p, len);
        if (cc < 0)
        {
            return (total > 0) ? total : cc;
        }
        total += cc;
        p += cc;
        len -= (size_t)cc;
    }

    return total;
}

static int flash_mtd_buffer(const char *buffer, int offset, int size, const char *dev)
{
    int err = 0;
    const char *pSrc = NULL;
    int fd = -1;
    struct mtd_info_user mtd;
    struct erase_info_user e;
    char buf[MTD_IO_BUFSIZE];
    char buf2[MTD_IO_BUFSIZE];
    int pass;

    if (buffer == NULL || dev == NULL || size <= 0)
    {
        return -1;
    }

    fd = open(dev, O_SYNC | O_RDWR);
    if (fd < 0)
    {
        printf("open %s failed: %s\n", dev, strerror(errno));
        return -1;
    }

    if (ioctl(fd, MEMGETINFO, &mtd) < 0)
    {
        printf("%s is not MTD\n", dev);
        close(fd);
        return -1;
    }

    if ((unsigned int)size > mtd.size)
    {
        printf("size %d too large for %s\n", size, dev);
        close(fd);
        return -1;
    }

    e.start = 0;
    {
        int erase_size = 64 * 1024;
        int left = size;

        while (left > 0)
        {
            erase_size = (left >= 64 * 1024) ? (64 * 1024) : (int)mtd.erasesize;
            e.length = erase_size;
            if (ioctl(fd, MEMERASE, &e) < 0)
            {
                printf("erase 0x%llx on %s failed\n", (long long)e.start, dev);
                err = -1;
                break;
            }
            e.start += erase_size;
            left -= erase_size;
        }
    }

    if (err != 0)
    {
        close(fd);
        return err;
    }

    for (pass = 0; pass <= 1; pass++)
    {
        int done = 0;
        unsigned count = MTD_IO_BUFSIZE;

        pSrc = buffer + offset;
        lseek(fd, 0, SEEK_SET);

        while (1)
        {
            int rem = size - done;

            if (rem == 0)
            {
                break;
            }
            if (rem < (int)MTD_IO_BUFSIZE)
            {
                count = (unsigned)rem;
            }

            printf("\r %s kb %d/%d", (pass == 0) ? "write" : "verify", done / 1024, size / 1024);

            memcpy(buf, pSrc, count);
            pSrc += count;

            if (pass == 0)
            {
                int ret;
                if (count < MTD_IO_BUFSIZE)
                {
                    memset(buf + count, 0, MTD_IO_BUFSIZE - count);
                }
                ret = (int)flash_full_write(fd, buf, MTD_IO_BUFSIZE);
                if (ret != MTD_IO_BUFSIZE)
                {
                    printf("\nwrite error at 0x%x ret=%d\n", done, ret);
                    err = -1;
                    break;
                }
            }
            else
            {
                read(fd, buf2, count);
                if (memcmp(buf, buf2, count) != 0)
                {
                    printf("\nverify mismatch at 0x%x\n", done);
                    err = -1;
                    break;
                }
            }

            done += (int)count;
        }

        printf("\n");
        if (err != 0)
        {
            break;
        }
    }

    close(fd);
    if (err == 0)
    {
        printf("flash %s done\n", dev);
    }
    return err;
}

static unsigned long long parse_u64(const char *s)
{
    if (s == NULL || s[0] == '\0')
    {
        return 0;
    }

    if (strncmp(s, "0x", 2) == 0 || strncmp(s, "0X", 2) == 0)
    {
        return strtoull(s + 2, NULL, 16);
    }

    return strtoull(s, NULL, 0);
}

static int map_firmware_from_mma(unsigned long long phy_addr, unsigned int map_size, anj_flash_phys_map_t *out)
{
    void *vir = NULL;

    if (out == NULL || phy_addr == 0 || map_size == 0)
    {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    if (anjflash_mmap_phys(phy_addr, map_size, &vir) != 0)
    {
        return -1;
    }

    out->map_base = vir;
    out->map_size = map_size;
    out->fw = (char *)vir;
    return 0;
}

static int read_firmware_file(const char *path, char **out, int *out_len)
{
    FILE *fp = NULL;
    long sz = 0;
    char *buf = NULL;

    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return -1;
    }

    sz = ftell(fp);
    if (sz <= 0)
    {
        fclose(fp);
        return -1;
    }

    rewind(fp);
    buf = (char *)malloc((size_t)sz);
    if (buf == NULL)
    {
        fclose(fp);
        return -1;
    }

    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz)
    {
        free(buf);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    *out = buf;
    *out_len = (int)sz;
    return 0;
}

static int parse_firmware_header(const char *fw, int fw_len, anj_fw_header_t *hdr)
{
    if (fw == NULL || hdr == NULL || fw_len < (int)sizeof(anj_fw_header_t))
    {
        return -1;
    }

    memcpy(hdr, fw, sizeof(anj_fw_header_t));

    if (strncmp(hdr->magicNumber, FIRMWARE_MAGIC_NUMBER, 8) != 0)
    {
        printf("bad firmware magic\n");
        return -1;
    }

    if (hdr->file_size <= 0 || hdr->file_size > fw_len)
    {
        printf("bad firmware size %d (buf=%d)\n", hdr->file_size, fw_len);
        return -1;
    }

    return 0;
}

static int watchdog_force_reset(void)
{
    int fd;
    int data = 0;

    fd = open(WATCH_DOG_FILE, O_RDWR);
    if (fd < 0)
    {
        printf("open %s failed: %s\n", WATCH_DOG_FILE, strerror(errno));
        return -1;
    }

    if (ioctl(fd, WATCH_DOG_FORCE_RESET, &data) < 0)
    {
        printf("watchdog force reset ioctl failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static void set_boot_mode(const char *mode)
{
    int ret;

    if (mode == NULL || (strcmp(mode, "recovery") != 0 && strcmp(mode, "normal") != 0))
        return;

    ret = anjrec_uboot_env_set("boot_mode", mode, 0);
    if (ret < 0)
    {
        printf("set boot_mode=%s failed\n", mode);
        return;
    }
    printf("set boot_mode=%s done\n", mode);
    sync();
}

static void write_done_flags(void)
{
    FILE *fp = fopen(OTA_FLAG_PATH, "w");

    if (fp != NULL)
    {
        fclose(fp);
    }

    fp = fopen(ROM_MD5_PATH, "w");
    if (fp != NULL)
    {
        fclose(fp);
    }
}

static void do_reboot(void)
{
    sync();
    watchdog_force_reset();

    printf("watchdog reset failed or timeout, try reboot()\n");
    reboot(RB_AUTOBOOT);
}

int main(int argc, char *argv[])
{
    unsigned long long phy_addr = 0;
    unsigned int map_size = 0;
    char *fw = NULL;
    int fw_len = 0;
    anj_flash_phys_map_t phys_map;
    anj_fw_header_t hdr;
    int ret = 0;

    memset(&phys_map, 0, sizeof(phys_map));
    memset(&hdr, 0, sizeof(hdr));

    if (argc < 3)
    {
        printf("usage: %s <phy_addr> <map_size>\n", argv[0]);
        do_reboot();
        return 1;
    }

    phy_addr = parse_u64(argv[1]);
    map_size = (unsigned int)parse_u64(argv[2]);

    printf("anj_flash start phy=%#llx map_size=%u\n", phy_addr, map_size);

    printf("kill app procs\n");
    system("killall anjdaemon");
    system("killall anjcam");
    printf("kill app procs done\n");

    sleep(1);

    if (phy_addr != 0)
    {
        if (map_size == 0)
        {
            printf("map_size required for MMA mode\n");
            do_reboot();
            return 1;
        }

        if (map_firmware_from_mma(phy_addr, map_size, &phys_map) != 0)
        {
            printf("map firmware from MMA failed\n");
            do_reboot();
            return 1;
        }

        fw = phys_map.fw;
        fw_len = (int)map_size;
    }
    else
    {
        if (read_firmware_file(ANJ_FLASH_FW_PATH, &fw, &fw_len) != 0)
        {
            printf("read firmware failed\n");
            do_reboot();
            return 1;
        }
    }

    if (parse_firmware_header(fw, fw_len, &hdr) != 0)
    {
        printf("parse firmware header failed\n");
        do_reboot();
        return 1;
    }

    if (hdr.file_size > fw_len)
    {
        printf("firmware truncated: header=%d buf=%d\n", hdr.file_size, fw_len);
        do_reboot();
        return 1;
    }

    if (hdr.type & FW_TYPE_KERNEL)
    {
        printf("flash kernel %s\n", ANJ_FLASH_KERNEL_MTD);
        if (flash_mtd_buffer(fw, hdr.kernel_start, hdr.kernel_size, ANJ_FLASH_KERNEL_MTD) != 0)
        {
            ret = -1;
        }
    }

    set_boot_mode("recovery");
    if (ret == 0 && (hdr.type & FW_TYPE_FILESYSTEM))
    {
        printf("flash rootfs %s\n", ANJ_FLASH_ROOTFS_MTD);
        if (flash_mtd_buffer(fw, hdr.fs_start, hdr.fs_size, ANJ_FLASH_ROOTFS_MTD) != 0)
        {
            ret = -1;
        }
    }

    if (ret != 0)
    {
        printf("anj_flash failed, reboot\n");
        do_reboot();
        return 1;
    }

    write_done_flags();
    set_boot_mode("normal");
    printf("anj_flash ok, reboot\n");
    do_reboot();
    return 0;
}
