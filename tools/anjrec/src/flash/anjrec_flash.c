/*
 * In-process MTD flash for anjrec recovery.
 * Adapted from tools/anjflash/src/anjflash.c.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
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

#include "anj_mw_comm.h"
#include "anjrec_flash.h"
#include "anjrec_uboot_env.h"

#define ANJREC_FLASH_FW_PATH "/tmp/ota_firmware.bin"
#define ROM_MD5_PATH "/mnt/nand/rom_md5.txt"
#define OTA_FLAG_PATH "/mnt/nand/flag.ota.updated"

#define FIRMWARE_MAGIC_NUMBER "ANJOY888"
#define FW_TYPE_KERNEL (1 << 0)
#define FW_TYPE_FILESYSTEM (1 << 1)

#define ANJREC_FLASH_KERNEL_MTD KERNEL_BLOCK
#define ANJREC_FLASH_ROOTFS_MTD FILESYS_BLOCK

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
} anjrec_fw_header_t;

typedef struct
{
    char *fw;
    int owns_fw;
} anjrec_firmware_buf_t;

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

static int parse_firmware_header(const char *fw, int fw_len, anjrec_fw_header_t *hdr)
{
    if (fw == NULL || hdr == NULL || fw_len < (int)sizeof(anjrec_fw_header_t))
    {
        return -1;
    }

    memcpy(hdr, fw, sizeof(anjrec_fw_header_t));

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

static void do_reboot(void)
{
    sync();
    watchdog_force_reset();

    printf("watchdog reset failed or timeout, try reboot()\n");
    reboot(RB_AUTOBOOT);
}

static void release_firmware_buf(anjrec_firmware_buf_t *fw_buf)
{
    if (fw_buf == NULL)
    {
        return;
    }

    if (fw_buf->owns_fw && fw_buf->fw != NULL)
    {
        free(fw_buf->fw);
    }

    memset(fw_buf, 0, sizeof(*fw_buf));
}

static int prepare_firmware(APPBIN_UPDATE_DATA *updateData, anjrec_firmware_buf_t *fw_buf,
                            int *fw_len)
{
    char *fw = NULL;
    int len = 0;
    const char *path = ANJREC_FLASH_FW_PATH;

    if (updateData == NULL || fw_buf == NULL || fw_len == NULL)
    {
        return -1;
    }

    memset(fw_buf, 0, sizeof(*fw_buf));

    if (updateData->filePath[0] != '\0')
    {
        path = updateData->filePath;
    }

    printf("anjrec_flash start path=%s len=%d\n", path, updateData->nFileLen);
    sleep(1);

    if (read_firmware_file(path, &fw, &len) != 0)
    {
        printf("read firmware %s failed\n", path);
        return -1;
    }

    fw_buf->fw = fw;
    fw_buf->owns_fw = 1;
    *fw_len = len;
    return 0;
}

static int anjrec_flash_apply_buffer(char *fw, int fw_len)
{
    anjrec_fw_header_t hdr;
    int ret = 0;

    memset(&hdr, 0, sizeof(hdr));

    if (parse_firmware_header(fw, fw_len, &hdr) != 0)
    {
        printf("parse firmware header failed\n");
        return -1;
    }

    if (hdr.file_size > fw_len)
    {
        printf("firmware truncated: header=%d buf=%d\n", hdr.file_size, fw_len);
        return -1;
    }

    if (hdr.type & FW_TYPE_KERNEL)
    {
        printf("flash kernel %s\n", ANJREC_FLASH_KERNEL_MTD);
        if (flash_mtd_buffer(fw, hdr.kernel_start, hdr.kernel_size, ANJREC_FLASH_KERNEL_MTD) != 0)
        {
            ret = -1;
        }
    }

    if (ret == 0 && (hdr.type & FW_TYPE_FILESYSTEM))
    {
        printf("flash rootfs %s\n", ANJREC_FLASH_ROOTFS_MTD);
        set_boot_mode("recovery");
        if (flash_mtd_buffer(fw, hdr.fs_start, hdr.fs_size, ANJREC_FLASH_ROOTFS_MTD) != 0)
        {
            ret = -1;
        }
        else
        {
            set_boot_mode("normal");
        }
    }

    return ret;
}

int anjrec_flash_apply_update(APPBIN_UPDATE_DATA *updateData)
{
    anjrec_firmware_buf_t fw_buf;
    int fw_len = 0;
    int ret = 0;

    if (updateData == NULL)
    {
        return -1;
    }

    if (prepare_firmware(updateData, &fw_buf, &fw_len) != 0)
    {
        do_reboot();
        return -1;
    }

    ret = anjrec_flash_apply_buffer(fw_buf.fw, fw_len);
    release_firmware_buf(&fw_buf);

    if (ret != 0)
    {
        printf("anjrec_flash failed, reboot\n");
        do_reboot();
        return -1;
    }

    write_done_flags();
    printf("anjrec_flash ok, reboot\n");
    do_reboot();
    return 0;
}

typedef struct
{
    APPBIN_UPDATE_DATA updateData;
} anjrec_flash_job_t;

static void *anjrec_flash_thread(void *arg)
{
    anjrec_flash_job_t *job = (anjrec_flash_job_t *)arg;

    anjrec_flash_apply_update(&job->updateData);
    free(job);
    return NULL;
}

static int anjrec_flash_spawn_async(const APPBIN_UPDATE_DATA *updateData)
{
    pthread_t tid;
    anjrec_flash_job_t *job = (anjrec_flash_job_t *)calloc(1, sizeof(*job));

    if (job == NULL || updateData == NULL)
    {
        return -1;
    }

    memcpy(&job->updateData, updateData, sizeof(job->updateData));

    if (pthread_create(&tid, NULL, anjrec_flash_thread, job) != 0)
    {
        free(job);
        return -1;
    }

    pthread_detach(tid);
    return 0;
}

int anjrec_flash_from_shell_cmd(const char *cmd)
{
    int background = 0;
    char buf[256];
    APPBIN_UPDATE_DATA updateData = {0};

    if (cmd == NULL)
    {
        return -1;
    }

    snprintf(buf, sizeof(buf), "%s", cmd);
    if (strstr(buf, " &") != NULL || (strlen(buf) > 0 && buf[strlen(buf) - 1] == '&'))
    {
        background = 1;
        buf[strcspn(buf, "&")] = '\0';
    }

    (void)buf;
    snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", ANJREC_FLASH_FW_PATH);

    if (background)
    {
        return anjrec_flash_spawn_async(&updateData);
    }

    return anjrec_flash_apply_update(&updateData);
}
