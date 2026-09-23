#ifndef FIRM_WARE_H
#define FIRM_WARE_H
#if defined(__cplusplus)
extern "C"
{
#endif

#define FIRMWARE_MAGIC_NUMBER "ANJOY888"

#define FIRMWARE_TYPE_KERNAL (1 << 0)
#define FIRMWARE_TYPE_FILESYSTEM (1 << 1)
#define FIRMWARE_TYPE_UBOOT (1 << 2)
#define FIRMWARE_TYPE_TGZ (1 << 3)
#define FIRMWARE_TYPE_OEM (1 << 4)
#define FIRMWARE_TYPE_LOGO (1 << 5)
#define FIRMWARE_TYPE_FACTTORY (1 << 6)										  // 清空所有定制，恢复出厂
#define FIRMWARE_TYPE_MIXED (FIRMWARE_TYPE_KERNAL | FIRMWARE_TYPE_FILESYSTEM) // kernal and filesystem

#define KERNAL_VERSION_INFO_LEN 256
#define FILESYSTEM_VERSION_INFO_LEN 256
#define APP_VERSION_INFO_LEN 256
#define FIRMWARE_MAGIC_NUMBER_LEN 8

#define MAX_FILE_NAME_LEN 256

/*
Firmware file struct
---------------------------------------
			|        |       |         |        |
HEADER   | kernal |    fs |   uboot | tgz    |
			|		  |	      |		    |        |
---------------------------------------
*/

typedef struct
{
	int uboot_start;
	int uboot_size;
	int uboot_crc;
	int tgz_start;
	int tgz_size;
	int tgz_crc;
	int oem_start;
	int oem_size;
	int oem_crc;
} FirmwareNewPartInfo;

#define FIRMWARE_RESERVER_SIZE (1000 - sizeof(FirmwareNewPartInfo) - APP_VERSION_INFO_LEN)

typedef struct
{
	char magicNumber[FIRMWARE_MAGIC_NUMBER_LEN];
	int header_crc; // check for header;
	int file_size;
	int type; // 1 for kernal 2 for fs, 3 for mixed
	int kernel_start;
	int kernel_size;
	int kernel_crc;
	char kernel_version_info[KERNAL_VERSION_INFO_LEN];
	int fs_start;
	int fs_size;
	char fs_version_info[FILESYSTEM_VERSION_INFO_LEN];
	int fs_crc;
	FirmwareNewPartInfo newInfo;
	char app_version_info[APP_VERSION_INFO_LEN];
	char reserve[FIRMWARE_RESERVER_SIZE]; // set for 0 ;
} FirmWareHeader;

int GetFileFromBufEx(char *bufPtr, const FirmWareHeader *header, unsigned int nFirmwareType, char *szOutputFile);

int GetFileFromFirmwareEx(const char *firmwareFile, const FirmWareHeader *header,
						  unsigned int nFirmwareType, char *szOutputFile);

int CheckFirmwareFile(char *firmwareFile, int *kfs_size, int *fs_size, int *uboot_size, int *tgz_size, int *oem_size, FirmWareHeader *header);
int CheckFirmwareBuf(char *bufPtr, int bufSize, int *kfs_size, int *fs_size, int *uboot_size, int *tgz_size, int *oem_size, FirmWareHeader *header);

int SeparateFirmWareFile(char *firmwareFile, char *kernelFile, char *fsFile, char *ubootFile, char *tgzFile, char *oemFile, FirmWareHeader *header);

int SeparateFirmWareFromBuf(char *bufPtr, int bufSize, char *kernelFile, char *fsFile, char *ubootFile, char *tgzFile, char *oemFile, FirmWareHeader *header);

int FlashNand(char *pFileName, const char *pBlockName, unsigned long offset);

int FlashMTD_byFileOffset(const char *filename, int offset, int size, const char *devicename);
int FlashMTD_byBufferOffset(char *buffer, int offset, int size, const char *devicename);

#if defined(__cplusplus)
}
#endif

#endif
