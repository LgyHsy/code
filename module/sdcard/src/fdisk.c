#include "fdisk.h"

#include <assert.h>             /* assert */
#include <sys/mount.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "anj_mw_comm.h"

typedef unsigned long uoff_t;

#define EXTENDED                  (0x05)
#define WIN98_EXTENDED            (0x0f)
#define LINUX_PARTITION           (0x81)
#define LINUX_SWAP                (0x82)
#define LINUX_NATIVE              (0x83)
#define LINUX_EXTENDED            (0x85)
#define LINUX_LVM                 (0x8e)
#define LINUX_RAID                (0xfd)

#define MAX_SECTOR_SIZE           (2048)
#define MAXIMUM_PARTS             (60)          /*最大扇区数*/
#define HDIO_GETGEO               (0x0301)      /* get device geometry */

#define bb_dev_null               "/dev/null"

static int gst_fd = -1; 

struct fdisk_data {
    int g_partitions;
    int sector_size;
    char MBRbuffer[MAX_SECTOR_SIZE];
    int g_heads;
    int g_cylinders;
    int g_sectors;
    int kern_heads;
    int kern_sectors;
    int pt_heads;
    int pt_sectors;
    int user_heads;
    int user_sectors;
    int user_cylinders;
    int total_number_of_sectors;
    int sector_offset;
    int dos_compatible_flag;
    int display_in_cyl_units;
    int units_per_sector;
} ;

struct hd_geometry {
    unsigned char heads;
    unsigned char sectors;
    unsigned short cylinders;
    unsigned long start;
};

struct partition {
    unsigned char boot_ind;         /* 0x80 - active */
    unsigned char head;             /* starting head */
    unsigned char sector;           /* starting sector */
    unsigned char cyl;              /* starting cylinder */
    unsigned char sys_ind;          /* what partition type */
    unsigned char end_head;         /* end head */
    unsigned char end_sector;       /* end sector */
    unsigned char end_cyl;          /* end cylinder */
    unsigned char start4[4];        /* starting sector counting from 0 */
    unsigned char size4[4];         /* nr of sectors in partition */
} PACKED;

struct pte
{
    struct partition *part_table;   /* points into sectorbuffer */
    struct partition *ext_pointer;  /* points into sectorbuffer */
    uint32_t offset_from_dev_start; /* disk sector number */
    char *sectorbuffer;             /* disk sector contents */
    char changed;                   /* boolean */
};

struct pte ptes[MAXIMUM_PARTS];
struct fdisk_data m_FdiskData;

#define IS_EXTENDED(i) \
    ((i) == EXTENDED || (i) == WIN98_EXTENDED || (i) == LINUX_EXTENDED)
    
#define pt_offset(b, n) \
    ((struct partition *)((b) + 0x1be + (n) * sizeof(struct partition)))
    
#define cround(n)       (m_FdiskData.display_in_cyl_units ? ((n) / m_FdiskData.units_per_sector) + 1 : (n))

#define move_from_unaligned32(v, u32p) ((v) = *(uint32_t*)(u32p))
#define move_to_unaligned32(u32p, v)   (*(uint32_t*)(u32p) = (v))

//#define isdigit(a) ((unsigned char)((a) - '0') <= 9)
#define SWAP_LE32(x) (x)

#define set_hsc(h, s, c, sector) do \
{ \
    s = sector % m_FdiskData.g_sectors + 1;  \
    sector /= m_FdiskData.g_sectors;         \
    h = sector % m_FdiskData.g_heads;        \
    sector /= m_FdiskData.g_heads;           \
    c = sector & 0xff;           \
    s |= (sector >> 2) & 0xc0;   \
} while (0)

static ssize_t  full_read(int fd, void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;

    total = 0;

    while (len)
    {
        cc = safe_read(fd, buf, len);
        if (cc < 0)
        {
            if (total)
            {
                return total;
            }
            return cc;
        }
        if (cc == 0)
            break;
        buf = ((char *)buf) + cc;
        total += cc;
        len -= cc;
    }

    return total;
}

static ssize_t  full_write(int fd, void *buf, size_t len)
{
    ssize_t cc;
    ssize_t total;

    total = 0;

    while (len)
    {
        cc = safe_write(fd, buf, len);
        if (cc < 0)
        {
            if (total)
            {
                return total;
            }
            return cc;
        }

        total += cc;
        buf = ((char *)buf) + cc;
        len -= cc;
    }

    return total;
}


// Die with an error message if we can't write the entire buffer.
static void xwrite(int fd, void *buf, size_t count)
{
    if (count)
    {
        ssize_t size = full_write(fd, buf, count);
        if ((size_t)size != count)
        {
            __INFO("write error\n");
        }
    }
}

/* TODO: move to libbb? */
/* TODO: return unsigned long long, FEATURE_FDISK_BLKSIZE _can_ handle
 * disks > 2^32 sectors
 */
static uint32_t bb_BLKGETSIZE_sectors(int fd)
{
    uint64_t v64;
    unsigned long longsectors;

    if (ioctl(fd, BLKGETSIZE64, &v64) == 0)
    {
        /* Got bytes, convert to 512 byte sectors */
        v64 >>= 9;
        if (v64 != (uint32_t)v64)
        {
 ret_trunc:
            /* Not only DOS, but all other partition tables
             * we support can't record more than 32 bit
             * sector counts or offsets
             */
            __INFO("device has more than 2^32 sectors, can't use all of them\n");
            v64 = (uint32_t)-1L;
        }
        return v64;
    }
    /* Needs temp of type long */
    if (ioctl(fd, BLKGETSIZE, &longsectors))
    {
        /* Perhaps this is a disk image */
        off_t sz = lseek(fd, 0, SEEK_END);
        longsectors = 0;
        if (sz > 0)
        {
            longsectors = (uoff_t)sz / m_FdiskData.sector_size;
        }
        lseek(fd, 0, SEEK_SET);
    }
    if (sizeof(long) > sizeof(uint32_t) && (longsectors != (uint32_t)longsectors))
    {
        goto ret_trunc;
    }
    return longsectors;
}

static int valid_part_table_flag(const char *mbuffer)
{
    return (mbuffer[510] == 0x55 && (uint8_t)mbuffer[511] == 0xaa);
}

static void get_partition_table_geometry(void)
{
    const unsigned char *bufp = (const unsigned char *)m_FdiskData.MBRbuffer;
    struct partition *p;
    int i = 0, h = 0, s = 0, hh = 0, ss = 0;
    int first = 1;
    int bad = 0;

    if (!(valid_part_table_flag((char*)bufp)))
    {
        return;
    }

    hh = ss = 0;
    for (i = 0; i < 4; i++)
    {
        p = pt_offset(bufp, i);
        if (p->sys_ind != 0)
        {
            h = p->end_head + 1;
            s = (p->end_sector & 077);
            if (first)
            {
                hh = h;
                ss = s;
                first = 0;
            }
            else if (hh != h || ss != s)
            {
                bad = 1;
            }
        }
    }

    if (!first && !bad)
    {
        m_FdiskData.pt_heads = hh;
        m_FdiskData.pt_sectors = ss;
    }
}

static void get_geometry(void)
{
    int sec_fac;

    int arg;
    if (gst_fd > 0)
    {
        if (ioctl(gst_fd, BLKSSZGET, &arg) == 0)
        {
            m_FdiskData.sector_size = arg;
        }
        
        sec_fac = m_FdiskData.sector_size / 512;
        m_FdiskData.g_heads = m_FdiskData.g_cylinders = m_FdiskData.g_sectors = 0;
        m_FdiskData.kern_heads = m_FdiskData.kern_sectors = 0;
        m_FdiskData.pt_heads = m_FdiskData.pt_sectors = 0;
        
        struct hd_geometry geometry;
        if (!ioctl(gst_fd, HDIO_GETGEO, &geometry))
        {
            m_FdiskData.kern_heads = geometry.heads;
            m_FdiskData.kern_sectors = geometry.sectors;
        }
        get_partition_table_geometry();
        
        m_FdiskData.g_heads = m_FdiskData.user_heads ? m_FdiskData.user_heads :
            m_FdiskData.pt_heads ? m_FdiskData.pt_heads :
            m_FdiskData.kern_heads ? m_FdiskData.kern_heads : 255;
        m_FdiskData.g_sectors = m_FdiskData.user_sectors ? m_FdiskData.user_sectors :
            m_FdiskData.pt_sectors ? m_FdiskData.pt_sectors :
            m_FdiskData.kern_sectors ? m_FdiskData.kern_sectors : 63;
        m_FdiskData.total_number_of_sectors = bb_BLKGETSIZE_sectors(gst_fd);
        
        m_FdiskData.sector_offset = 1;
        if (m_FdiskData.dos_compatible_flag)
        {
            m_FdiskData.sector_offset = m_FdiskData.g_sectors;
        }
        
        m_FdiskData.g_cylinders = m_FdiskData.total_number_of_sectors / (m_FdiskData.g_heads * m_FdiskData.g_sectors * sec_fac);
        if (!m_FdiskData.g_cylinders)
        {
            m_FdiskData.g_cylinders = m_FdiskData.user_cylinders;
        }
    }
}

static void update_units(void)
{
    int cyl_units = m_FdiskData.g_heads * m_FdiskData.g_sectors;

    if (m_FdiskData.display_in_cyl_units && cyl_units)
    {
        m_FdiskData.units_per_sector = cyl_units;
    }
    else
    {
        m_FdiskData.units_per_sector = 1;   /* in sectors */
    }
}

static int get_boot(char *pstdevPath)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        struct pte *pe = &ptes[i];
        pe->part_table = pt_offset(m_FdiskData.MBRbuffer, i);
        pe->ext_pointer = NULL;
        pe->offset_from_dev_start = 0;
        pe->sectorbuffer = m_FdiskData.MBRbuffer;
        pe->changed = 0;
    }

    gst_fd = open(pstdevPath, O_RDWR);
    if (gst_fd < 0)
    {
        __INFO("open %s failed!\n", pstdevPath);
        return -1;
    }
    full_read(gst_fd, m_FdiskData.MBRbuffer, 512);

    get_geometry();
    update_units();

    __INFO("\n"
    "The number of cylinders for this disk is set to %u.\n"
    "There is nothing wrong with that, but this is larger than 1024,\n"
    "and could in certain setups cause problems with:\n"
    "1) software that runs at boot time (e.g., old versions of LILO)\n"
    "2) booting and partitioning software from other OSs\n"
    "   (e.g., DOS FDISK, OS/2 FDISK)\n", m_FdiskData.g_cylinders);

    return 0;
}

static int is_cleared_partition(const struct partition *p)
{
    /* We consider partition "cleared" only if it has only zeros */
    const char *cp = (const char *)p;
    int cnt = sizeof(*p);
    char bits = 0;
    while (--cnt >= 0)
    {
        bits |= *cp++;
    }
    return (bits == 0);
}

static void delete_partition(int i)
{
    struct pte *pe = &ptes[i];
    struct partition *p = pe->part_table;

    pe->changed = 1;

    if (i < 4)
    {
        if (p)
        {
            memset(p, 0, sizeof(*p));
        }
        return;
    }
}

static int delete_existing_partition(void)
{
    int ret = -1;
    int i = 0;
    for (i = 0; i < m_FdiskData.g_partitions; i++)
    {
        struct pte *pe = &ptes[i];
        struct partition *p = pe->part_table;

        if (p && !is_cleared_partition(p))
        {
            ret = 0;
            __INFO("delete partition %u\n", i + 1);
            delete_partition(i);
        }
    }
    
    if (ret == -1)
    {
        __INFO("No partition is defined yet!\n");
    }
    return ret;
}

static unsigned read4_little_endian(const unsigned char *cp)
{
    uint32_t v;
    move_from_unaligned32(v, cp);
    return SWAP_LE32(v);
}

static uint32_t get_start_sect(const struct partition *p)
{
    return read4_little_endian(p->start4);
}

static uint32_t get_partition_start_from_dev_start(const struct pte *pe)
{
    return pe->offset_from_dev_start + get_start_sect(pe->part_table);
}

static uint32_t get_nr_sects(const struct partition *p)
{
    return read4_little_endian(p->size4);
}

static void fill_bounds(uint32_t *first, uint32_t *last)
{
    int i = 0;
    const struct pte *pe = &ptes[0];
    const struct partition *p;

    for (i = 0; i < m_FdiskData.g_partitions; pe++, i++)
    {
        p = pe->part_table;
        if (!p->sys_ind || IS_EXTENDED(p->sys_ind))
        {
            first[i] = 0xffffffff;
            last[i] = 0;
        }
        else
        {
            first[i] = get_partition_start_from_dev_start(pe);
            last[i] = first[i] + get_nr_sects(p) - 1;
        }
    }
}

static void store4_little_endian(unsigned char *cp, unsigned val)
{
    uint32_t v = SWAP_LE32(val);
    move_to_unaligned32(cp, v);
}

static void set_start_sect(struct partition *p, unsigned start_sect)
{
    store4_little_endian(p->start4, start_sect);
}

static void set_nr_sects(struct partition *p, unsigned nr_sects)
{
    store4_little_endian(p->size4, nr_sects);
}

static void set_hsc_start_end(struct partition *p, uint32_t start, uint32_t stop)
{
    if (m_FdiskData.dos_compatible_flag && (start / (m_FdiskData.g_sectors * m_FdiskData.g_heads) > 1023))
    {
        start = m_FdiskData.g_heads * m_FdiskData.g_sectors * 1024 - 1;
    }
    set_hsc(p->head, p->sector, p->cyl, start);

    if (m_FdiskData.dos_compatible_flag && (stop / (m_FdiskData.g_sectors * m_FdiskData.g_heads) > 1023))
    {
        stop = m_FdiskData.g_heads * m_FdiskData.g_sectors * 1024 - 1;
    }
    set_hsc(p->end_head, p->end_sector, p->end_cyl, stop);
}

static void set_partition(int i, int doext, uint32_t start, uint32_t stop, int sysid)
{
    struct partition *p;
    uint32_t offset;

    p = ptes[i].part_table;
    offset = ptes[i].offset_from_dev_start;
    p->boot_ind = 0;
    p->sys_ind = sysid;
    set_start_sect(p, start - offset);
    set_nr_sects(p, stop - start + 1);
    set_hsc_start_end(p, start, stop);
    ptes[i].changed = 1;
}

static void add_partition(int n, int sys)
{
    int i, num_read = 0;
    struct partition *p = ptes[n].part_table;
    uint32_t limit = 0, temp = 0;
    uint32_t start = 0, stop = 0;
    uint32_t first[m_FdiskData.g_partitions], last[m_FdiskData.g_partitions];

    if (p && p->sys_ind)
    {
        __INFO("msg_part_already_defined  %d\n", n + 1);
        return;
    }
    
    fill_bounds(first, last);
    
    if (n < 4)
    {
        start = m_FdiskData.sector_offset;
        if (m_FdiskData.display_in_cyl_units || !m_FdiskData.total_number_of_sectors)
        {
            limit = (uint32_t)m_FdiskData.g_heads * m_FdiskData.g_sectors * m_FdiskData.g_cylinders - 1;
        }
        else
        {
            limit = m_FdiskData.total_number_of_sectors - 1;
        }
    }
    if (m_FdiskData.display_in_cyl_units)
    {
        for (i = 0; i < m_FdiskData.g_partitions; i++)
        {
            first[i] = (cround(first[i]) - 1) * m_FdiskData.units_per_sector;
        }
    }

    do
    {
        temp = start;
        for (i = 0; i < m_FdiskData.g_partitions; i++)
        {
            uint32_t lastplusoff;

            if (start == ptes[i].offset_from_dev_start)
            {
                start += m_FdiskData.sector_offset;
            }
            lastplusoff = last[i] + ((n < 4) ? 0 : m_FdiskData.sector_offset);
            if (start >= first[i] && start <= lastplusoff)
            {
                start = lastplusoff + 1;
            }
        }
        if (start > limit)
        {
            break;
        }
        if (start >= (temp + m_FdiskData.units_per_sector) && num_read)
        {
            __INFO("Sector %u is already allocated\n", temp);
            temp = start;
            num_read = 0;
        }
        if (!num_read && start == temp)
        {
            uint32_t saved_start;

            saved_start = start;
            start = cround(saved_start);
            if (m_FdiskData.display_in_cyl_units)
            {
                start = (start - 1) * m_FdiskData.units_per_sector;
                if (start < saved_start)
                {
                    start = saved_start;
                }
            }
            num_read = 1;
        }
    } while (start != temp || !num_read);
        
    if (n > 4)
    {                    /* NOT for fifth partition */
        struct pte *pe = &ptes[n];

        pe->offset_from_dev_start = start - m_FdiskData.sector_offset;
    }

    for (i = 0; i < m_FdiskData.g_partitions; i++)
    {
        struct pte *pe = &ptes[i];

        if (start < pe->offset_from_dev_start && limit >= pe->offset_from_dev_start)
        {
            limit = pe->offset_from_dev_start - 1;
        }
        if ((start < first[i]) && (limit >= first[i]))
        {
            limit = first[i] - 1;
        }
    }
    
    if (start > limit)
    {
        __INFO("No free sectors available\n");
        if (n > 4)
        {
            m_FdiskData.g_partitions--;
        }
        return;
    }
    
    if (cround(start) == cround(limit))
    {
        stop = limit;
    }
    else
    {
        stop = cround(limit);
        if (m_FdiskData.display_in_cyl_units)
        {
            stop = stop * m_FdiskData.units_per_sector - 1;
            if (stop >limit)
            {
                stop = limit;
            }
        }
    }

    set_partition(n, 0, start, stop, sys);
    if (n > 4)
    {
        set_partition(n - 1, 1, ptes[n].offset_from_dev_start, stop, 0x05);
    }
}

static int get_nonexisting_partition(void)
{
    int ret = -1;
    struct pte *pe = &ptes[0];
    struct partition *p = pe->part_table;

    if (p && is_cleared_partition(p))
    {
        ret = 0;
        add_partition(0, LINUX_NATIVE);
    }
    
    if (ret == -1)
    {
        __INFO("1 primary partitions have been defined already!\n");
    }
    return ret;
}

static int new_partition(void)
{
    int ret = -1;
    int i = 0, free_primary = 0;

    for (i = 0; i < 4; i++)
    {
        free_primary += !ptes[i].part_table->sys_ind;
    }

    if (!free_primary && m_FdiskData.g_partitions >= MAXIMUM_PARTS)
    {
        __INFO("The maximum number of partitions has been created\n");
        return ret;
    }

    if (!free_primary)
    {
        __INFO("You must delete some partition and add an extended partition first\n");
    }
    else
    {
        ret = get_nonexisting_partition();
    }
    return ret;
}

static void write_part_table_flag(char *b)
{
    b[510] = 0x55;
    b[511] = 0xaa;
}

static void seek_sector(uint32_t secno)
{
    uint64_t off = (uint64_t)secno * m_FdiskData.sector_size;
    if (gst_fd > 0)
    {
        lseek(gst_fd, (off_t)off, SEEK_SET);
    }
}

static void write_sector(uint32_t secno, void *buf)
{
    seek_sector(secno);
    
    if (gst_fd > 0)
    {
        xwrite(gst_fd, buf, m_FdiskData.sector_size);
    }
}

static void reread_partition_table(void)
{
    __INFO("Calling ioctl() to re-read partition table\n");
    sync();
    
    /* Users with slow external USB disks on a 320MHz ARM system (year 2011)
     * report that sleep is needed, otherwise BLKRRPART may fail with -EIO:*/
    sleep(1);
    if (gst_fd > 0)
    {
        int ret = ioctl(gst_fd, BLKRRPART, NULL);
        if (ret < 0)
        {
            __INFO("failed, kernel still uses old table\n");
        }
    }
}

static void write_table(void)
{
    int i = 0;
    for (i = 0; i < 3; i++)
    {
        if (ptes[i].changed)
        {
            ptes[3].changed = 1;
        }
    }
    
    for (i = 3; i < m_FdiskData.g_partitions; i++)
    {
        struct pte *pe = &ptes[i];
        if (pe->changed)
        {
            write_part_table_flag(pe->sectorbuffer);
            write_sector(pe->offset_from_dev_start, pe->sectorbuffer);
        }
    }

    __INFO("The partition table has been altered.\n");
    reread_partition_table();
}

static int fdisk_data_init(void)
{
    memset(&m_FdiskData, 0, sizeof(m_FdiskData));
    m_FdiskData.g_partitions = 4;
    m_FdiskData.sector_size = 512;
    m_FdiskData.sector_offset = 1;
    m_FdiskData.dos_compatible_flag = 1;
    m_FdiskData.display_in_cyl_units = 1;
    m_FdiskData.units_per_sector = 1;
    return 0;
}

static int fdisk_init(char *pstdevPath)
{
    fdisk_data_init();
    if (get_boot(pstdevPath) != 0)
    {
        return -1;
    }
    return 0;
}

int fdisk_repair_sd(char *pstdevPath)
{
    __INFO("Start Repair SD Card!\n");
    if (fdisk_init(pstdevPath) != 0)
    {
        __INFO("Repair SD Card Failed!\n");
        
        if (gst_fd > 0)
        {
            close(gst_fd);
            gst_fd = -1;
        }
        return -1;
    }
    /*删除SD卡存在的所有分区*/
    if (0 != delete_existing_partition())
    {
        __INFO("delete_existing_partition Failed!\n");
        //return -1;
    }

    /*创建分区1*/
    if (0 != new_partition())
    {
        __INFO("new_partition Failed!\n");
        //return -1;
    }
    
    write_table();  /* does not return */

    if (gst_fd > 0)
    {
        close(gst_fd);
        gst_fd = -1;
    }

    sleep(1);
    return 0;
}
