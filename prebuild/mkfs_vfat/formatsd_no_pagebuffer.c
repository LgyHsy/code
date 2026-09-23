/**
 *	Copy from busybox.
 *  Licensed under GPLv2
 *	Busybox'ed (2009) by Vladimir Dronnikov <dronnikov@gmail.com>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/hdreg.h> /* HDIO_GETGEO */
#include <linux/fd.h> 
#include <sys/mount.h>   /* BLKSSZGET */
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE     /* See feature_test_macros(7) */
#endif
#include <sys/types.h>

#define RETURNS_MALLOC __attribute__ ((malloc))
#define PACKED __attribute__ ((__packed__))
#define ALIGNED(m) __attribute__ ((__aligned__(m)))
typedef off64_t uoff_t;

/* Useful for defeating gcc's alignment of "char message[]"-like data */
#if !defined(__s390__)
    /* on s390[x], non-word-aligned data accesses require larger code */
# define ALIGN1 __attribute__((aligned(1)))
# define ALIGN2 __attribute__((aligned(2)))
# define ALIGN4 __attribute__((aligned(4)))
#else
/* Arches which MUST have 2 or 4 byte alignment for everything are here */
# define ALIGN1
# define ALIGN2
# define ALIGN4
#endif

# define SWAP_BE16(x) bswap_16(x)
# define SWAP_BE32(x) bswap_32(x)
# define SWAP_BE64(x) bb_bswap_64(x)
# define SWAP_LE16(x) (x)
# define SWAP_LE32(x) (x)
# define SWAP_LE64(x) (x)
# define IF_BIG_ENDIAN(...)
# define IF_LITTLE_ENDIAN(...) __VA_ARGS__

// storage helpers for mk*fs utilities
char BUG_wrong_field_size(void);
#define STORE_LE(field, value) \
do { \
	if (sizeof(field) == 4) \
		field = SWAP_LE32((uint32_t)(value)); \
	else if (sizeof(field) == 2) \
		field = SWAP_LE16((uint16_t)(value)); \
	else if (sizeof(field) == 1) \
		field = (uint8_t)(value); \
	else \
		BUG_wrong_field_size(); \
} while (0)

#define MAXINT(T) (T)( \
	((T)-1) > 0 \
	? (T)-1 \
	: (T)~((T)1 << (sizeof(T)*8-1)) \
	)

#define MININT(T) (T)( \
	((T)-1) > 0 \
	? (T)0 \
	: ((T)1 << (sizeof(T)*8-1)) \
	)


#if !defined(BLKSSZGET)
# define BLKSSZGET _IO(0x12, 104)
#endif
//#include <linux/msdos_fs.h>

#define SECTOR_SIZE             512

#define SECTORS_PER_BLOCK	(BLOCK_SIZE / SECTOR_SIZE)

// M$ says the high 4 bits of a FAT32 FAT entry are reserved
#define EOF_FAT32       0x0FFFFFF8
#define BAD_FAT32       0x0FFFFFF7
#define MAX_CLUST_32    0x0FFFFFF0

#define ATTR_VOLUME     8

#define NUM_FATS        2

/* FAT32 filesystem looks like this:
 * sector -nn...-1: "hidden" sectors, all sectors before this partition
 * (-h hidden-sectors sets it. Useful only for boot loaders,
 *  they need to know _disk_ offset in order to be able to correctly
 *  address sectors relative to start of disk)
 * sector 0: boot sector
 * sector 1: info sector
 * sector 2: set aside for boot code which didn't fit into sector 0
 * ...(zero-filled sectors)...
 * sector B: backup copy of sector 0 [B set by -b backup-boot-sector]
 * sector B+1: backup copy of sector 1
 * sector B+2: backup copy of sector 2
 * ...(zero-filled sectors)...
 * sector R: FAT#1 [R set by -R reserved-sectors]
 * ...(FAT#1)...
 * sector R+fat_size: FAT#2
 * ...(FAT#2)...
 * sector R+fat_size*2: cluster #2
 * ...(cluster #2)...
 * sector R+fat_size*2+clust_size: cluster #3
 * ...(the rest is filled by clusters till the end)...
 */

enum {
// Perhaps this should remain constant
	info_sector_number = 1,
// TODO: make these cmdline options
// dont forget sanity check: backup_boot_sector + 3 <= reserved_sect
	backup_boot_sector = 3,
	reserved_sect      = 6,
};

// how many blocks we try to read while testing
#define TEST_BUFFER_BLOCKS      16

struct msdos_dir_entry {
	char     name[11];       /* 000 name and extension */
	uint8_t  attr;           /* 00b attribute bits */
	uint8_t  lcase;          /* 00c case for base and extension */
	uint8_t  ctime_cs;       /* 00d creation time, centiseconds (0-199) */
	uint16_t ctime;          /* 00e creation time */
	uint16_t cdate;          /* 010 creation date */
	uint16_t adate;          /* 012 last access date */
	uint16_t starthi;        /* 014 high 16 bits of cluster in FAT32 */
	uint16_t time;           /* 016 time */
	uint16_t date;           /* 018 date */
	uint16_t start;          /* 01a first cluster */
	uint32_t size;           /* 01c file size in bytes */
} PACKED;

/* Example of boot sector's beginning:
0000  eb 58 90 4d 53 57 49 4e  34 2e 31 00 02 08 26 00  |...MSWIN4.1...&.|
0010  02 00 00 00 00 f8 00 00  3f 00 ff 00 3f 00 00 00  |........?...?...|
0020  54 9b d0 00 0d 34 00 00  00 00 00 00 02 00 00 00  |T....4..........|
0030  01 00 06 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
0040  80 00 29 71 df 51 e0 4e  4f 20 4e 41 4d 45 20 20  |..)q.Q.NO NAME  |
0050  20 20 46 41 54 33 32 20  20 20 33 c9 8e d1 bc f4  |  FAT32   3.....|
*/
struct msdos_volume_info { /* (offsets are relative to start of boot sector) */
	uint8_t  drive_number;    /* 040 BIOS drive number */
	uint8_t  reserved;        /* 041 unused */
	uint8_t  ext_boot_sign;	  /* 042 0x29 if fields below exist (DOS 3.3+) */
	uint32_t volume_id32;     /* 043 volume ID number */
	char     volume_label[11];/* 047 volume label */
	char     fs_type[8];      /* 052 typically "FATnn" */
} PACKED;                         /* 05a end. Total size 26 (0x1a) bytes */

struct msdos_boot_sector {
	/* We use strcpy to fill both, and gcc-4.4.x complains if they are separate */
	char     boot_jump_and_sys_id[3+8]; /* 000 short or near jump instruction */
	/*char   system_id[8];*/     /* 003 name - can be used to special case partition manager volumes */
	uint16_t bytes_per_sect;     /* 00b bytes per logical sector */
	uint8_t  sect_per_clust;     /* 00d sectors/cluster */
	uint16_t reserved_sect;      /* 00e reserved sectors (sector offset of 1st FAT relative to volume start) */
	uint8_t  fats;               /* 010 number of FATs */
	uint16_t dir_entries;        /* 011 root directory entries */
	uint16_t volume_size_sect;   /* 013 volume size in sectors */
	uint8_t  media_byte;         /* 015 media code */
	uint16_t sect_per_fat;       /* 016 sectors/FAT */
	uint16_t sect_per_track;     /* 018 sectors per track */
	uint16_t heads;              /* 01a number of heads */
	uint32_t hidden;             /* 01c hidden sectors (sector offset of volume within physical disk) */
	uint32_t fat32_volume_size_sect; /* 020 volume size in sectors (if volume_size_sect == 0) */
	uint32_t fat32_sect_per_fat; /* 024 sectors/FAT */
	uint16_t fat32_flags;        /* 028 bit 8: fat mirroring, low 4: active fat */
	uint8_t  fat32_version[2];   /* 02a major, minor filesystem version (I see 0,0) */
	uint32_t fat32_root_cluster; /* 02c first cluster in root directory */
	uint16_t fat32_info_sector;  /* 030 filesystem info sector (usually 1) */
	uint16_t fat32_backup_boot;  /* 032 backup boot sector (usually 6) */
	uint32_t reserved2[3];       /* 034 unused */
	struct msdos_volume_info vi; /* 040 */
	char     boot_code[0x200 - 0x5a - 2]; /* 05a */
#define BOOT_SIGN 0xAA55
	uint16_t boot_sign;          /* 1fe */
} PACKED;

#define FAT_FSINFO_SIG1 0x41615252
#define FAT_FSINFO_SIG2 0x61417272
struct fat32_fsinfo {
	uint32_t signature1;         /* 0x52,0x52,0x41,0x61, "RRaA" */
	uint32_t reserved1[128 - 8];
	uint32_t signature2;         /* 0x72,0x72,0x61,0x41, "rrAa" */
	uint32_t free_clusters;      /* free cluster count.  -1 if unknown */
	uint32_t next_cluster;       /* most recently allocated cluster */
	uint32_t reserved2[3];
	uint16_t reserved3;          /* 1fc */
	uint16_t boot_sign;          /* 1fe */
} PACKED;

struct bug_check {
	char BUG1[sizeof(struct msdos_dir_entry  ) == 0x20 ? 1 : -1];
	char BUG2[sizeof(struct msdos_volume_info) == 0x1a ? 1 : -1];
	char BUG3[sizeof(struct msdos_boot_sector) == 0x200 ? 1 : -1];
	char BUG4[sizeof(struct fat32_fsinfo     ) == 0x200 ? 1 : -1];
};

static const char boot_code[] ALIGN1 =
	"\x0e"          /* 05a:         push  cs */
	"\x1f"          /* 05b:         pop   ds */
	"\xbe\x77\x7c"  /*  write_msg:  mov   si, offset message_txt */
	"\xac"          /* 05f:         lodsb */
	"\x22\xc0"      /* 060:         and   al, al */
	"\x74\x0b"      /* 062:         jz    key_press */
	"\x56"          /* 064:         push  si */
	"\xb4\x0e"      /* 065:         mov   ah, 0eh */
	"\xbb\x07\x00"  /* 067:         mov   bx, 0007h */
	"\xcd\x10"      /* 06a:         int   10h */
	"\x5e"          /* 06c:         pop   si */
	"\xeb\xf0"      /* 06d:         jmp   write_msg */
	"\x32\xe4"      /*  key_press:  xor   ah, ah */
	"\xcd\x16"      /* 071:         int   16h */
	"\xcd\x19"      /* 073:         int   19h */
	"\xeb\xfe"      /*  foo:        jmp   foo */
	/* 077: message_txt: */
	"This is not a bootable disk\r\n";


#define MARK_CLUSTER(cluster, value) \
	((uint32_t *)fat)[cluster] = SWAP_LE32(value)

long long GetTick(void)
{
	struct timespec t1;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	long long T = (1000000*(t1.tv_sec)+(t1.tv_nsec)/1000)/1000 ;
	return T;
}

off64_t xlseek(int fd, off64_t offset, int whence)
{
	off64_t off = lseek64(fd, offset, whence);
	if (off == (off64_t)-1) {
		if (whence == SEEK_SET)
		{
			printf("error: lseek(%u) fail", offset);
			return 0;
		}
		printf("lseek fail %d\n", whence);
		return 0;
	}
	return off;
}

uoff_t get_volume_size_in_bytes(int fd,
		const char *override,
		unsigned override_units,
		int extend)
{
	uoff_t result;

	if (override) {
#if 0
		result = XATOOFF(override);
		if (result >= (uoff_t)(MAXINT(off64_t)) / override_units)
		{
			printf("image size is too big\n");
			return 0;
		}
		result *= override_units;
		/* seek past end fails on block devices but works on files */
		if (lseek(fd, result - 1, SEEK_SET) != (off64_t)-1) {
			if (extend)
				write(fd, "", 1); /* file grows if needed */
		}
		//else {
		//	bb_error_msg("warning, block device is smaller");
		//}
#endif
	} else {
		/* more portable than BLKGETSIZE[64] */
		result = xlseek(fd, 0, SEEK_END);
		
	}

	xlseek(fd, 0, SEEK_SET);

	/* Prevent things like this:
	 * $ dd if=/dev/zero of=foo count=1 bs=1024
	 * $ mkswap foo
	 * Setting up swapspace version 1, size = 18446744073709548544 bytes
	 *
	 * Picked 16k arbitrarily: */
	if (result < 16*1024)
		printf("error: image is too small\n");

	return result;
}


static ssize_t safe_write(int fd, const void *buf, size_t count)
{
	ssize_t n;

	do {
		n = write(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

static ssize_t full_write(int fd, const void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len) {
		cc = safe_write(fd, buf, len);

		if (cc < 0) {
			if (total) {
				/* we already wrote some! */
				/* user can do another write to know the error code */
				return total;
			}
			return cc;  /* write() returns -1 on failure. */
		}

		total += cc;
		buf = ((const char *)buf) + cc;
		len -= cc;
	}

	return total;
}

int xwrite(int fd, const void *buf, size_t count)
{
	if (count) {
		ssize_t size = full_write(fd, buf, count);
		if ((size_t)size != count) {
			/*
			 * Two cases: write error immediately;
			 * or some writes succeeded, then we hit an error.
			 * In either case, errno is set.
			 */
			printf("write error!!\n");
		}
	}
}

int format_device(const char *device_name, const char *volume_label, int spc)
{
	int result;
	struct stat st;
	char *buf;
	unsigned bufsize;
	uoff_t volume_size_bytes;
	uoff_t volume_size_sect;
	uint32_t total_clust;
	uint32_t volume_id;
	unsigned bytes_per_sect;
	unsigned sect_per_fat;
	unsigned opts;
	uint16_t sect_per_track;
	uint8_t media_byte;
	uint8_t sect_per_clust;
	uint8_t heads;
	int dev;
	int min_bytes_per_sect;
	
	// default volume ID = creation time
	volume_id = time(NULL);
	buf = NULL;
	bufsize = 0;
	
	dev = open(device_name, O_RDWR|O_DIRECT, 0666);
	if(dev < 0)
	{
		printf("open device fail\n");
		return -1;
	}
	//check is block device
	fstat(dev, &st);
	if (!S_ISBLK(st.st_mode)) 
	{
		printf("not block device\n");
		goto err_not_blk_dev;
	}

	bytes_per_sect = SECTOR_SIZE;
	ioctl(dev, BLKSSZGET, &min_bytes_per_sect);

	if (min_bytes_per_sect > SECTOR_SIZE) {
		bytes_per_sect = min_bytes_per_sect;
		printf("for this device sector size is %u\n", min_bytes_per_sect);
	}

#if 0
	volume_size_bytes = get_volume_size_in_bytes(dev, NULL, 1024, /*extend:*/ 1);
#else
	uint64_t size;
	ioctl(dev, BLKGETSIZE64, &size);
	printf("total size is %llu\n", size);
	volume_size_bytes = size;
#endif
	volume_size_sect = volume_size_bytes / bytes_per_sect;

	/* For FAT32, try to do the same as M$'s format command
	 * (see http://www.win.tue.nl/~aeb/linux/fs/fat/fatgen103.pdf p. 20):
	 * fs size <= 260M: 0.5k clusters
	 * fs size <=   8G: 4k clusters
	 * fs size <=  16G: 8k clusters
	 * fs size >   16G: 16k clusters
	 */
	media_byte = 0xf8;
	heads = 255;
	sect_per_track = 63;
	if(spc != 0)
	{
		sect_per_clust = spc;
	}
	else
	{
		sect_per_clust = 1;

		struct hd_geometry geometry;
		// size (in sectors), sect (per track), head

		// N.B. whether to use HDIO_GETGEO or HDIO_REQ?
		if ( ioctl(dev, HDIO_GETGEO, &geometry) == 0
			 && geometry.sectors
			 && geometry.heads ) 
		{
			// hard drive
			sect_per_track = geometry.sectors;
			heads = geometry.heads;
			
			
			if (volume_size_bytes >= 260*1024*1024) {
				sect_per_clust = 8;
				/* fight gcc: */
				/* "error: integer overflow in expression" */
				/* "error: right shift count >= width of type" */
				if (sizeof(off64_t) > 4) {
					unsigned t = (volume_size_bytes >> 31 >> 1);
					if (t >= 8/4)
						sect_per_clust = 16;
					if (t >= 16/4)
						sect_per_clust = 32;
					if (t >= 27/4)
						sect_per_clust = 128;
				}
			}
			
		}

		if ((off64_t)(volume_size_sect - reserved_sect) < 4)
		{
			printf("the image is too small for FAT32\n");
			goto err_img_too_small;
		}
	}

	sect_per_fat = 1;
	while (1) {
		while (1) {
			int spf_adj;
			uoff_t tcl = (volume_size_sect - reserved_sect - NUM_FATS * sect_per_fat) / sect_per_clust;
			// tcl may be > MAX_CLUST_32 here, but it may be
			// because sect_per_fat is underestimated,
			// and with increased sect_per_fat it still may become
			// <= MAX_CLUST_32. Therefore, we do not check
			// against MAX_CLUST_32, but against a bigger const:
			if (tcl > 0x80ffffff)
				goto next;
			total_clust = tcl; // fits in uint32_t
			// Every cluster needs 4 bytes in FAT. +2 entries since
			// FAT has space for non-existent clusters 0 and 1.
			// Let's see how many sectors that needs.
			//May overflow at "*4":
			//spf_adj = ((total_clust+2) * 4 + bytes_per_sect-1) / bytes_per_sect - sect_per_fat;
			//Same in the more obscure, non-overflowing form:
			spf_adj = ((total_clust+2) + (bytes_per_sect/4)-1) / (bytes_per_sect/4) - sect_per_fat;

			if (spf_adj <= 0) {
				// do not need to adjust sect_per_fat.
				// so, was total_clust too big after all?
				if (total_clust <= MAX_CLUST_32)
					goto found_total_clust; // no
				// yes, total_clust is _a bit_ too big
				goto next;
			}
			// adjust sect_per_fat, go back and recalc total_clust
			// (note: just "sect_per_fat += spf_adj" isn't ok)
			sect_per_fat += ((unsigned)spf_adj / 2) | 1;
		}
next:
//		if (sect_per_clust == 128)
//		{
//			printf("can't make FAT32 with >128 sectors/cluster");
//			goto err_cant_make;
//		}
		sect_per_clust *= 2;
		sect_per_fat = (sect_per_fat / 2) | 1;
	}


	
found_total_clust:
	
	fprintf(stderr,
		"Device '%s'\n"
		"heads:%u, sectors/track:%u, bytes/sector:%u\n"
		"media descriptor:%02x\n"
		"total sectors:%llu, clusters:%u, sectors/cluster:%u\n"
		"FATs:2, sectors/FAT:%u\n"
		"volumeID:%08x, label:'%s'\n",
		device_name,
		heads, sect_per_track, bytes_per_sect,
		(int)media_byte,
		volume_size_sect, (int)total_clust, (int)sect_per_clust,
		sect_per_fat,
		(int)volume_id, volume_label
	);
	
	//ok, let's begin write data.
	//
	// Write filesystem image sequentially (no seeking)
	//
	{
		// (a | b) is poor man's max(a, b)
		bufsize = reserved_sect;
		//bufsize |= sect_per_fat; // can be quite large
		bufsize |= 2; // use this instead
		bufsize |= sect_per_clust;
		//buf = (char*)calloc(1, bufsize * bytes_per_sect);
		int nTemp = posix_memalign((void**)&buf, getpagesize(), bufsize * bytes_per_sect);
		if (0 != nTemp)
		{
			perror("posix_memalign error");
			goto err_mem_alloc;
		}
	}

	{ // boot and fsinfo sectors, and their copies
		struct msdos_boot_sector *boot_blk = (struct msdos_boot_sector *)buf;
		struct fat32_fsinfo *info = (struct fat32_fsinfo *)(buf + bytes_per_sect);

		strcpy(boot_blk->boot_jump_and_sys_id, "\xeb\x58\x90" "mkdosfs");
		STORE_LE(boot_blk->bytes_per_sect, bytes_per_sect);
		STORE_LE(boot_blk->sect_per_clust, sect_per_clust);
		// cast in needed on big endian to suppress a warning
		STORE_LE(boot_blk->reserved_sect, (uint16_t)reserved_sect);
		STORE_LE(boot_blk->fats, 2);
		//STORE_LE(boot_blk->dir_entries, 0); // for FAT32, stays 0
		if (volume_size_sect <= 0xffff)
			STORE_LE(boot_blk->volume_size_sect, volume_size_sect);
		STORE_LE(boot_blk->media_byte, media_byte);
		// wrong: this would make Linux think that it's fat12/16:
		//if (sect_per_fat <= 0xffff)
		//	STORE_LE(boot_blk->sect_per_fat, sect_per_fat);
		// works:
		//STORE_LE(boot_blk->sect_per_fat, 0);
		STORE_LE(boot_blk->sect_per_track, sect_per_track);
		STORE_LE(boot_blk->heads, heads);
		//STORE_LE(boot_blk->hidden, 0);
		STORE_LE(boot_blk->fat32_volume_size_sect, volume_size_sect);
		STORE_LE(boot_blk->fat32_sect_per_fat, sect_per_fat);
		//STORE_LE(boot_blk->fat32_flags, 0);
		//STORE_LE(boot_blk->fat32_version[2], 0,0);
		STORE_LE(boot_blk->fat32_root_cluster, 2);
		STORE_LE(boot_blk->fat32_info_sector, info_sector_number);
		STORE_LE(boot_blk->fat32_backup_boot, backup_boot_sector);
		//STORE_LE(boot_blk->reserved2[3], 0,0,0);
		STORE_LE(boot_blk->vi.ext_boot_sign, 0x29);
		STORE_LE(boot_blk->vi.volume_id32, volume_id);
		memcpy(boot_blk->vi.fs_type, "FAT32   ", sizeof(boot_blk->vi.fs_type));
		strncpy(boot_blk->vi.volume_label, volume_label, sizeof(boot_blk->vi.volume_label));
		memcpy(boot_blk->boot_code, boot_code, sizeof(boot_code));
		STORE_LE(boot_blk->boot_sign, BOOT_SIGN);

		STORE_LE(info->signature1, FAT_FSINFO_SIG1);
		STORE_LE(info->signature2, FAT_FSINFO_SIG2);
		// we've allocated cluster 2 for the root dir
		STORE_LE(info->free_clusters, (total_clust - 1));
		STORE_LE(info->next_cluster, 2);
		STORE_LE(info->boot_sign, BOOT_SIGN);

		// 1st copy
		xwrite(dev, buf, bytes_per_sect * backup_boot_sector);
		// 2nd copy and possibly zero sectors
		xwrite(dev, buf, bytes_per_sect * (reserved_sect - backup_boot_sector));
	}
#if 0
	{ // file allocation tables
		unsigned i,j;
		unsigned char *fat = (unsigned char *)buf;

		long long t1 = GetTick();
		memset(buf, 0, bytes_per_sect * 2);
		// initial FAT entries
		MARK_CLUSTER(0, 0x0fffff00 | media_byte);
		MARK_CLUSTER(1, 0xffffffff);
		// mark cluster 2 as EOF (used for root dir)
		MARK_CLUSTER(2, EOF_FAT32);
		for (i = 0; i < NUM_FATS; i++) {
			xwrite(dev, buf, bytes_per_sect);
			for (j = 1; j < sect_per_fat; j++)
				xwrite(dev, buf + bytes_per_sect, bytes_per_sect);
		}
		
		long long t2 = GetTick();
		printf("format complete 1: %lld ms\n", t2 -t1);
	}
#else
	


	{ // file allocation tables
		unsigned i,j,k;
		unsigned char *fat = (unsigned char *)buf;



		memset(buf, 0, bytes_per_sect * 2);
		// initial FAT entries
		MARK_CLUSTER(0, 0x0fffff00 | media_byte);
		MARK_CLUSTER(1, 0xffffffff);
		// mark cluster 2 as EOF (used for root dir)
		MARK_CLUSTER(2, EOF_FAT32);
		
		int max_alloc_size = 100 * 1024;
		char *empty_buf = NULL;//(char *)calloc( 1, max_alloc_size );
		int nTemp = posix_memalign((void**)&empty_buf, getpagesize(), max_alloc_size);
		if (0 != nTemp)
		{
			perror("posix_memalign error 2");
			goto err_mem_alloc;
		}
		memset(empty_buf, 0, max_alloc_size);
		
		int write_cnt_eachtime = max_alloc_size/bytes_per_sect;
		unsigned total_loop = (sect_per_fat - 1)/write_cnt_eachtime;
		unsigned leave_n = (sect_per_fat - 1) % write_cnt_eachtime;
		
		printf("write total %u MB\n", NUM_FATS * (sect_per_fat - 1) * bytes_per_sect /1024);
		printf("sect_per_fat=%d, bytes_per_sect=%d, total_loop=%d\n", sect_per_fat, bytes_per_sect, total_loop);

		long long t1 = GetTick();
		for (i = 0; i < NUM_FATS; i++) {
			xwrite(dev, buf, bytes_per_sect);
			
			//总共需要写入 （sect_per_fat - 1） * bytes_per_sect 字节
			//sect_per_fat可能有几万个。bytes_per_sect >= 512字节
			
			for (j = 0; j < total_loop; j++)
			{
				xwrite(dev, empty_buf, bytes_per_sect * write_cnt_eachtime);
			}
			
			xwrite(dev, empty_buf, bytes_per_sect * leave_n);
		}
		
		free(empty_buf);
		long long t2 = GetTick();

		printf("format complete 2: %lld ms\n", t2 -t1);
	}
#endif
	// root directory
	// empty directory is just a set of zero bytes
	memset(buf, 0, sect_per_clust * bytes_per_sect);
	if (volume_label[0]) {
		// create dir entry for volume_label
		struct msdos_dir_entry *de;
#if 0
		struct tm tm_time;
		uint16_t t, d;
#endif
		de = (struct msdos_dir_entry *)buf;
		strncpy(de->name, volume_label, sizeof(de->name));
		STORE_LE(de->attr, ATTR_VOLUME);
#if 0
		localtime_r(&create_time, &tm_time);
		t = (tm_time.tm_sec >> 1) + (tm_time.tm_min << 5) + (tm_time.tm_hour << 11);
		d = tm_time.tm_mday + ((tm_time.tm_mon+1) << 5) + ((tm_time.tm_year-80) << 9);
		STORE_LE(de->time, t);
		STORE_LE(de->date, d);
		//STORE_LE(de->ctime_cs, 0);
		de->ctime = de->time;
		de->cdate = de->date;
		de->adate = de->date;
#endif
	}
	xwrite(dev, buf, sect_per_clust * bytes_per_sect);

	
	result = 0;
	goto clean_up;
	
err_not_blk_dev:
err_img_too_small:
err_cant_make:
err_mem_alloc:
	result = -1;
	
clean_up:
	if(dev >= 0)
		close(dev);
	if(buf != NULL)
		free(buf);
	
	return result;
}

int main(int argc, char *argv[])
{
	const char *volume_label = "";
	int spc = 0;
	
	int ch;
	while((ch = getopt(argc, argv, "n:s:")) != -1)
	{
		switch(ch)
		{
			case 'n': 
			{
				volume_label = optarg;
				break;
			}
			case 's': 
			{
				spc = atoi(optarg);
				break;
			}
		}
	}
	argv += optind;
	printf("%s %s %d\n", argv[0], volume_label, spc); 
	format_device(argv[0], volume_label, spc);
	return 0;
}
