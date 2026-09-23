#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_thread.h"
#include "anj_mw_crypt.h"
#include "anj_mw_watchdog.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "firmware_util.h"

#define BUFSIZE (4 * 1024)

#define ERR_EXIT                \
	{                           \
		if (kernel_fd != -1)    \
		{                       \
			close(kernel_fd);   \
		}                       \
		if (fs_fd != -1)        \
		{                       \
			close(fs_fd);       \
		}                       \
		if (firmware_fd != -1)  \
		{                       \
			close(firmware_fd); \
		}                       \
		return -1;              \
	}

#define ERR_EXIT_FREE           \
	{                           \
		if (kernel_fd != -1)    \
		{                       \
			close(kernel_fd);   \
		}                       \
		if (fs_fd != -1)        \
		{                       \
			close(fs_fd);       \
		}                       \
		if (firmware_fd != -1)  \
		{                       \
			close(firmware_fd); \
		}                       \
		if (file_data != NULL)  \
		{                       \
			free(file_data);    \
			file_data = NULL;   \
		}                       \
		return -1;              \
	}

static int GetFirmWareHeaderCrc(FirmWareHeader *header)
{
	FirmWareHeader tmpHeader;
	memcpy(&tmpHeader, header, sizeof(FirmWareHeader));

	tmpHeader.header_crc = 0;

	return anj_crc32_update(0, (unsigned char *)header, sizeof(FirmWareHeader));
}

void ShowHeader(FirmWareHeader *header)
{
	__INFO("%-20s\t%-#20x\n", "header_crc", header->header_crc);
	__INFO("%-20s\t%-#20x\n", "file_size", header->file_size);
	__INFO("%-20s\t%-20d\n", "type", header->type);
	__INFO("%-20s\t%-#20x\n", "kernel_start", header->kernel_start);
	__INFO("%-20s\t%-#20x\n", "kernel_size", header->kernel_size);
	__INFO("%-20s\t%-#20x\n", "kernel_crc", header->kernel_crc);
	__INFO("%-20s\t%-20s\n", "kernel_version_info", header->kernel_version_info);
	__INFO("%-20s\t%-#20x\n", "fs_start", header->fs_start);
	__INFO("%-20s\t%-#20x\n", "fs_size", header->fs_size);
	__INFO("%-20s\t%-#20x\n", "fs_crc", header->fs_crc);
	__INFO("%-20s\t%-20s\n", "fs_version_info", header->fs_version_info);
	__INFO("%-20s\t%-#20x\n", "uboot_start", header->newInfo.tgz_crc);
	__INFO("%-20s\t%-#20x\n", "uboot_size", header->newInfo.uboot_size);
	__INFO("%-20s\t%-#20x\n", "uboot_crc", header->newInfo.uboot_crc);
	__INFO("%-20s\t%-#20x\n", "tgz_start", header->newInfo.tgz_start);
	__INFO("%-20s\t%-#20x\n", "tgz_size", header->newInfo.tgz_size);
	__INFO("%-20s\t%-#20x\n", "tgz_crc", header->newInfo.tgz_crc);
	__INFO("%-20s\t%-#20x\n", "oem_start", header->newInfo.oem_start);
	__INFO("%-20s\t%-#20x\n", "oem_size", header->newInfo.oem_size);
	__INFO("%-20s\t%-#20x\n", "oem_crc", header->newInfo.oem_crc);
}

int CheckVendor(char *vendor_id)
{
	*vendor_id = 0;
	int ret;
	// FILE *fp = fopen("/tmp/vendor_id_md5.dat","rb");
	FILE *fp = fopen("/tmp/vendor_check.dat", "rb");
	if (fp != NULL)
	{
		ret = fread(vendor_id, 1, 256, fp);
		fclose(fp);

		if (ret > 0)
		{
			vendor_id[ret] = '\0';

			__ERR("vendor_id = %s\n", vendor_id);

			char map_list[5][16][2] =
				{
					// mode 0
					{
						{'0', 'F'},
						{'1', 'E'},
						{'2', 'D'},
						{'3', 'C'},
						{'4', 'B'},
						{'5', 'A'},
						{'6', '9'},
						{'7', '8'},
						{'8', '7'},
						{'9', '6'},
						{'A', '5'},
						{'B', '4'},
						{'C', '3'},
						{'D', '2'},
						{'E', '1'},
						{'F', '0'},
					},
					// mode 1
					{
						{'0', 'B'},
						{'1', 'A'},
						{'2', 'C'},
						{'3', 'E'},
						{'4', 'D'},
						{'5', 'F'},
						{'6', '0'},
						{'7', '2'},
						{'8', '4'},
						{'9', '6'},
						{'A', '8'},
						{'B', '1'},
						{'C', '3'},
						{'D', '5'},
						{'E', '7'},
						{'F', '9'},
					},
					// mode 2
					{
						{'0', '9'},
						{'1', '3'},
						{'2', '5'},
						{'3', '7'},
						{'4', '1'},
						{'5', '0'},
						{'6', 'A'},
						{'7', 'F'},
						{'8', 'B'},
						{'9', 'D'},
						{'A', 'C'},
						{'B', 'E'},
						{'C', '2'},
						{'D', '4'},
						{'E', '6'},
						{'F', '8'},
					},
					// mode 3
					{
						{'0', '5'},
						{'1', '7'},
						{'2', '8'},
						{'3', '2'},
						{'4', '1'},
						{'5', '0'},
						{'6', 'A'},
						{'7', 'C'},
						{'8', 'F'},
						{'9', 'B'},
						{'A', 'D'},
						{'B', 'E'},
						{'C', '6'},
						{'D', '4'},
						{'E', '9'},
						{'F', '3'},
					},
					// mode 4
					{
						{'0', '8'},
						{'1', '7'},
						{'2', '6'},
						{'3', '3'},
						{'4', '2'},
						{'5', '1'},
						{'6', '9'},
						{'7', '4'},
						{'8', '5'},
						{'9', '0'},
						{'A', 'C'},
						{'B', 'F'},
						{'C', 'A'},
						{'D', 'E'},
						{'E', 'D'},
						{'F', 'B'},
					},
				};

			int i, j;
			int sum = 0;
			int mapmod;
			for (i = 0; i < strlen(vendor_id); i++)
			{
				sum += vendor_id[i];
			}

			mapmod = sum % 5;

			__ERR("sum: %d, map mode is : %d\n", sum, mapmod);

			for (i = 0; i < strlen(vendor_id); i++)
			{
				for (j = 0; j < 16; j++)
				{
					if (vendor_id[i] == map_list[mapmod][j][0])
					{
						vendor_id[i] = map_list[mapmod][j][1];
						break;
					}
				}
			}

			__ERR("mapped vendor_id: [%s]\n", vendor_id);
		}
		else
		{
			__ERR("read file: vendor_check.dat failed, err = %s\n", strerror(errno));
		}
	}

	return 0;
}

int CheckVersionMatch(FirmWareHeader *header)
{
	char szDeviceType[32];
	memset(szDeviceType, 0, 32);
	if (anj_sysmng_dev_str_get(szDeviceType) < 0)
	{
		__ERR("Unknown device type.\n");
		return 0;
	}

	char vendor_id[256] = "";
	//	CheckVendor(vendor_id);

	if (header->type & FIRMWARE_TYPE_KERNAL)
	{
		if (strlen(vendor_id) > 0)
		{
			__ERR("we have vendor_id %s, check vendor id of kernel info first!!!\n", vendor_id);

			if (!strstr(header->kernel_version_info, vendor_id))
			{
				__ERR("vendor_id not match: [%s], can not update!!!\n", header->kernel_version_info);
				return 0;
			}
		}

		if (strstr(header->kernel_version_info, SDK_KERNEL_IDENTITY) == NULL)
		{
			__ERR("bad kernel info %s\n", header->kernel_version_info);
			return 0;
		}
		else
		{
			__INFO("kernel info OK: %s\n", header->kernel_version_info);
		}
	}

	if (header->type & FIRMWARE_TYPE_FILESYSTEM)
	{
		if (strlen(vendor_id))
		{
			__ERR("we have vendor_id, check vendor id of filesys first!!!\n");

			if (!strstr(header->fs_version_info, vendor_id))
			{
				__ERR("vendor_id not match: [%s], can not update!!!\n", header->fs_version_info);
				return 0;
			}
		}

		if (strstr(header->fs_version_info, szDeviceType) == NULL)
		{
			__ERR("bad fs info: %s\n", header->fs_version_info);
			return 0;
		}
		char szNeedStr[64];

		if (anj_mw_file_exists(AJ_APP_PATH "/SNCHECKNO.flag"))
		{
			snprintf(szNeedStr, sizeof(szNeedStr), "%s_CHKNO_V%s", szDeviceType, anj_sysmng_product_version_get());
		}
		else
		{
			snprintf(szNeedStr, sizeof(szNeedStr), "%s_V%s", szDeviceType, anj_sysmng_product_version_get());
		}

		if (strstr(header->fs_version_info, szNeedStr) == NULL)
		{
			__ERR("bad fs info %s for %s\n", header->fs_version_info, szNeedStr);
			return 0;
		}
		else
		{
			char file_ver[64] = {0};
			anj_mw_read_file_limit_len("/etc/flag.customize", file_ver, sizeof(file_ver));
			if (strstr(file_ver, "_HB") != NULL && strstr(file_ver, "_PJ") != NULL)
			{
				// 限制汉邦项目用的不同OEM型号的固件不能互升
				AjOemStruct oemInfo;
				memset(&oemInfo, 0, sizeof(oemInfo));
				anj_config_oem_factory_get(&oemInfo);

				if (strlen(oemInfo.szDeviceType) > 0)
				{
					if (strstr(header->fs_version_info, oemInfo.szDeviceType) == NULL)
					{
						__ERR("bad fs info %s for %s\n", header->fs_version_info, oemInfo.szDeviceType);
						return 0;
					}
				}
			}

			__INFO("fs info OK: %s\n", header->fs_version_info);
		}
	}

	return 1;
}

int CheckFirmwareFile(char *firmwareFile, int *kfs_size, int *fs_size, int *uboot_size, int *tgz_size, int *oem_size, FirmWareHeader *header)
{
	int firmware_fd = -1;
	int readCnt;
	int total_size;

	*kfs_size = 0;
	*fs_size = 0;
	*uboot_size = 0;
	*tgz_size = 0;
	*oem_size = 0;

	firmware_fd = open(firmwareFile, O_RDONLY);
	if (firmware_fd == -1)
	{
		__ERR("open file (%s) fail(%s)\n", firmwareFile, strerror(errno));
		return -1;
	}

	total_size = lseek(firmware_fd, 0, SEEK_END);

	lseek(firmware_fd, 0, SEEK_SET);

	readCnt = read(firmware_fd, header, sizeof(FirmWareHeader));

	if (firmware_fd != -1)
	{
		close(firmware_fd);
		firmware_fd = -1;
	}

	if (readCnt != sizeof(FirmWareHeader))
	{
		__ERR("read error\n");
		return -1;
	}

	int header_crc = header->header_crc;
	header->header_crc = 0;
	int crc_tmp = GetFirmWareHeaderCrc(header);
	if (header_crc != crc_tmp)
	{
		__ERR("crc error\n");
		return -1;
	}
	header->header_crc = header_crc;

	if (header->file_size > total_size)
	{
		__ERR("size error(header->file_size = %d, total_size = %d\n", header->file_size, total_size);
		return -1;
	}

	if (header->file_size < total_size)
	{
		__ERR("warnning: header->file_size=%d, file size=%d\n", header->file_size, total_size);
	}

	if (strncmp(header->magicNumber, FIRMWARE_MAGIC_NUMBER, FIRMWARE_MAGIC_NUMBER_LEN) != 0)
	{
		__ERR("magic number error\n");
		return -1;
	}

	int expect_size = sizeof(FirmWareHeader);

	if (header->type & FIRMWARE_TYPE_KERNAL)
	{
		*kfs_size = header->kernel_size;
		expect_size += header->kernel_size;
	}

	if (header->type & FIRMWARE_TYPE_FILESYSTEM)
	{
		*fs_size = header->fs_size;
		expect_size += header->fs_size;
	}

	if (header->type & FIRMWARE_TYPE_UBOOT)
	{
		*uboot_size = header->newInfo.uboot_size;
		expect_size += header->newInfo.uboot_size;
	}

	if (header->type & FIRMWARE_TYPE_TGZ)
	{
		*tgz_size = header->newInfo.tgz_size;
		expect_size += header->newInfo.tgz_size;
	}

	if (header->type & FIRMWARE_TYPE_OEM)
	{
		*oem_size = header->newInfo.oem_size;
		expect_size += header->newInfo.oem_size;
	}

	if (header->file_size != expect_size)
	{
		__ERR("size error\n");
		return -1;
	}

	ShowHeader(header);

	if (!CheckVersionMatch(header))
	{
		return -1;
	}

	return 0;
}

int GetFileFromFirmware(int firmware_fd, char *filepath,
						unsigned int start,
						unsigned int size,
						unsigned int crc)
{
	int file_fd = -1;
	int left_cnt = size;
	int readCnt;
	int bufLen = 4096; // 1024;
	int buf[4096];	   // bufLen];
	unsigned int tmp_crc = 0;

	__ERR("Ready to get file %s, indicate crc %#x.\n", filepath, crc);

	file_fd = open(filepath, O_WRONLY | O_CREAT, S_IRWXU);
	lseek(firmware_fd, start, SEEK_SET);

	if (file_fd == -1)
	{
		__ERR("open file (%s) for(%s)\n", filepath, strerror(errno));
		return -1;
	}

	while (left_cnt > 0)
	{
		readCnt = read(firmware_fd, buf, bufLen);
		if (readCnt == 0)
		{
			break;
		}
		else if (readCnt == -1)
		{
			__ERR("read data error\n");
			if (file_fd != -1)
				close(file_fd);
			return -1;
		}
		else
		{
			left_cnt = left_cnt - readCnt;

			int writeCnt;
			if (left_cnt < 0)
			{
				writeCnt = left_cnt + readCnt;
			}
			else
			{
				writeCnt = readCnt;
			}

			if (write(file_fd, buf, writeCnt) == -1)
			{
				__ERR("write file %s error, err=%s\n", filepath, strerror(errno));
				if (file_fd != -1)
					close(file_fd);
				return -1;
			}
		}

		// usleep(10*1000);
	}

	if (file_fd != -1)
		close(file_fd);

	if (size > 0)
	{
		int ret = anj_crc32_file(filepath, &tmp_crc);
		if (ret != 0)
		{
			__ERR("get file %s crc error\n", filepath);
			return -1;
		}
	}

	if (crc != tmp_crc)
	{
		__ERR("file %s crc error. header->size=%u, header->crc=%#x, calc crc=%#x\n",
			  filepath, size, crc, tmp_crc);
		return -1;
	}
	else
	{
		__ERR("Get file %s, crc %#x.\n", filepath, tmp_crc);
	}

	return 0;
}

int GetFileFromFirmwareEx(const char *firmwareFile, const FirmWareHeader *header,
						  unsigned int nFirmwareType, char *szOutputFile)
{
	int nRet = 0;
	int firmware_fd = -1;

	firmware_fd = open(firmwareFile, O_RDONLY);
	if (firmware_fd == -1)
	{
		__ERR("open file (%s) fail(%s)\n", firmwareFile, strerror(errno));
		return -1;
	}

	switch (nFirmwareType)
	{
	case FIRMWARE_TYPE_KERNAL:
		nRet = GetFileFromFirmware(firmware_fd, szOutputFile, header->kernel_start, header->kernel_size, header->kernel_crc);
		break;
	case FIRMWARE_TYPE_FILESYSTEM:
		nRet = GetFileFromFirmware(firmware_fd, szOutputFile, header->fs_start, header->fs_size, header->fs_crc);
		break;
	case FIRMWARE_TYPE_UBOOT:
		nRet = GetFileFromFirmware(firmware_fd, szOutputFile, header->newInfo.uboot_start, header->newInfo.uboot_size, header->newInfo.uboot_crc);
		break;
	case FIRMWARE_TYPE_TGZ:
		nRet = GetFileFromFirmware(firmware_fd, szOutputFile, header->newInfo.tgz_start, header->newInfo.tgz_size, header->newInfo.tgz_crc);
		break;
	case FIRMWARE_TYPE_OEM:
		nRet = GetFileFromFirmware(firmware_fd, szOutputFile, header->newInfo.oem_start, header->newInfo.oem_size, header->newInfo.oem_crc);
		break;
	default:
		__ERR("unknown firmware type %u\n", nFirmwareType);
		szOutputFile = "";
		nRet = -1;
	}

	if (0 != nRet)
	{
		__ERR("get file (%s) fail.\n", szOutputFile);
	}
	else
	{
		__ERR("get file (%s) OK.\n", szOutputFile);
	}

	if (firmware_fd != -1)
	{
		close(firmware_fd);
	}

	return nRet;
}

int SeparateFirmWareFile(char *firmwareFile, char *kernelFile, char *fsFile, char *ubootFile, char *tgzFile, char *oemFile, FirmWareHeader *header)
{
	int nKernelSize = 0;
	int nFsSize = 0;
	int nUbootSize = 0;
	int nTgzSize = 0;
	int nOemSize = 0;

	if (CheckFirmwareFile(firmwareFile,
						  &nKernelSize,
						  &nFsSize, &nUbootSize, &nTgzSize, &nOemSize, header) != 0)
	{
		__ERR("check firmware %s fail(%s)\n", firmwareFile, strerror(errno));
		return -1;
	}

	unsigned int nFirmwareType = FIRMWARE_TYPE_KERNAL;
	char *szUpdateFile = kernelFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromFirmwareEx(firmwareFile, header,
								  nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_FILESYSTEM;
	szUpdateFile = fsFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromFirmwareEx(firmwareFile, header,
								  nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_UBOOT;
	szUpdateFile = ubootFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromFirmwareEx(firmwareFile, header,
								  nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_TGZ;
	szUpdateFile = tgzFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromFirmwareEx(firmwareFile, header,
								  nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_OEM;
	szUpdateFile = oemFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromFirmwareEx(firmwareFile, header,
								  nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	return 0;
}

int CheckFirmwareBuf(char *bufPtr, int bufSize, int *kfs_size, int *fs_size, int *uboot_size, int *tgz_size, int *oem_size, FirmWareHeader *header)
{
	*kfs_size = 0;
	*fs_size = 0;
	*uboot_size = 0;
	*tgz_size = 0;
	*oem_size = 0;

	__INFO("ptr = %x, bufSize = %d\n", bufPtr, bufSize);

	memcpy(header, bufPtr, sizeof(FirmWareHeader));

	int header_crc = header->header_crc;
	header->header_crc = 0;
	int crc_tmp = GetFirmWareHeaderCrc(header);
	if (header_crc != crc_tmp)
	{
		__ERR("crc error\n");
		return -1;
	}
	header->header_crc = header_crc;

	if (header->file_size > bufSize)
	{
		__ERR("size error(header->file_size = %d, total_size = %d\n", header->file_size, bufSize);
		return -1;
	}

	if (header->file_size < bufSize)
	{
		__ERR("warnning: header->file_size=%d, file size=%d\n", header->file_size, bufSize);
	}

	int expect_size = sizeof(FirmWareHeader);
	if (header->type & FIRMWARE_TYPE_KERNAL)
	{
		*kfs_size = header->kernel_size;
		expect_size += header->kernel_size;
	}

	if (header->type & FIRMWARE_TYPE_FILESYSTEM)
	{
		*fs_size = header->fs_size;
		expect_size += header->fs_size;
	}

	if (header->type & FIRMWARE_TYPE_UBOOT)
	{
		*uboot_size = header->newInfo.uboot_size;
		expect_size += header->newInfo.uboot_size;
	}

	if (header->type & FIRMWARE_TYPE_TGZ)
	{
		*tgz_size = header->newInfo.tgz_size;
		expect_size += header->newInfo.tgz_size;
	}

	if (header->type & FIRMWARE_TYPE_OEM)
	{
		*oem_size = header->newInfo.oem_size;
		expect_size += header->newInfo.oem_size;
	}
	if (header->file_size != expect_size)
	{
		__ERR("size error\n");
		return -1;
	}

	if (strncmp(header->magicNumber, FIRMWARE_MAGIC_NUMBER, FIRMWARE_MAGIC_NUMBER_LEN) != 0)
	{
		__ERR("magic number error\n");
		return -1;
	}

	ShowHeader(header);

	if (!CheckVersionMatch(header))
	{
		return -1;
	}

	return 0;
}

int GetFileFromBuf(char *bufPtr, char *filepath,
				   unsigned int start,
				   unsigned int size,
				   unsigned int crc)
{
	int ret;
	int offset = start;
	int file_fd = -1;
	int left_cnt = size;
	int bufLen = 4096; // 1024;
	unsigned int tmp_crc = 0;

	__INFO("Ready to get file %s, indicate crc %#x.\n", filepath, crc);

	file_fd = open(filepath, O_WRONLY | O_CREAT, S_IRWXU);

	if (file_fd == -1)
	{
		__ERR("open file (%s) for(%s)\n", filepath, strerror(errno));
		return -1;
	}

	left_cnt = size;
	while (left_cnt > 0)
	{
		if (left_cnt >= bufLen)
		{
			ret = write(file_fd, bufPtr + offset, bufLen);
			offset += bufLen;
			left_cnt -= bufLen;
		}
		else
		{
			ret = write(file_fd, bufPtr + offset, left_cnt);
			offset += left_cnt;
			left_cnt -= left_cnt;
		}

		if (ret < 0)
		{
			__ERR("write  file failed!!! err = %s\n", strerror(errno));

			close(file_fd);
			return -1;
		}
	}

	__INFO("write file fininished.\n");

	if (file_fd != -1)
		close(file_fd);

	if (size > 0)
	{
		int ret = anj_crc32_file(filepath, &tmp_crc);
		if (ret != 0)
		{
			__ERR("get file %s crc error\n", filepath);
			return -1;
		}
	}

	if (crc != tmp_crc)
	{
		__ERR("file %s crc error. header->size=%u, header->crc=%#x, calc crc=%#x\n",
			  filepath, size, crc, tmp_crc);
		return -1;
	}
	else
	{
		__INFO("Get file %s, crc %#x.\n", filepath, tmp_crc);
	}

	return 0;
}

int GetFileFromBufEx(char *bufPtr, const FirmWareHeader *header,
					 unsigned int nFirmwareType, char *szOutputFile)
{
	int nRet = 0;

	switch (nFirmwareType)
	{
	case FIRMWARE_TYPE_KERNAL:
		nRet = GetFileFromBuf(bufPtr, szOutputFile, header->kernel_start, header->kernel_size, header->kernel_crc);
		break;
	case FIRMWARE_TYPE_FILESYSTEM:
		nRet = GetFileFromBuf(bufPtr, szOutputFile, header->fs_start, header->fs_size, header->fs_crc);
		break;
	case FIRMWARE_TYPE_UBOOT:
		nRet = GetFileFromBuf(bufPtr, szOutputFile, header->newInfo.uboot_start, header->newInfo.uboot_size, header->newInfo.uboot_crc);
		break;
	case FIRMWARE_TYPE_TGZ:
		nRet = GetFileFromBuf(bufPtr, szOutputFile, header->newInfo.tgz_start, header->newInfo.tgz_size, header->newInfo.tgz_crc);
		break;
	case FIRMWARE_TYPE_OEM:
		nRet = GetFileFromBuf(bufPtr, szOutputFile, header->newInfo.oem_start, header->newInfo.oem_size, header->newInfo.oem_crc);
		break;
	default:
		__ERR("unknown firmware type %u\n", nFirmwareType);
		szOutputFile = "";
		nRet = -1;
	}

	if (0 != nRet)
	{
		__ERR("get file (%s) fail.\n", szOutputFile);
	}
	else
	{
		__INFO("get file (%s) OK.\n", szOutputFile);
	}

	return nRet;
}

int SeparateFirmWareFromBuf(char *bufPtr, int bufSize, char *kernelFile, char *fsFile, char *ubootFile, char *tgzFile, char *oemFile, FirmWareHeader *header)
{
	int nKernelSize = 0;
	int nFsSize = 0;
	int nUbootSize = 0;
	int nTgzSize = 0;
	int nOemSize = 0;

	if (CheckFirmwareBuf(bufPtr, bufSize,
						 &nKernelSize,
						 &nFsSize, &nUbootSize, &nTgzSize, &nOemSize, header) != 0)
	{
		__ERR("check firmware buf %#x:%d fail\n", bufPtr, bufSize);
		return -1;
	}

	unsigned int nFirmwareType = FIRMWARE_TYPE_KERNAL;
	char *szUpdateFile = kernelFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromBufEx(bufPtr, header,
							 nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_FILESYSTEM;
	szUpdateFile = fsFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromBufEx(bufPtr, header,
							 nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_UBOOT;
	szUpdateFile = ubootFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromBufEx(bufPtr, header,
							 nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_TGZ;
	szUpdateFile = tgzFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromBufEx(bufPtr, header,
							 nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	nFirmwareType = FIRMWARE_TYPE_OEM;
	szUpdateFile = oemFile;
	if ((header->type & nFirmwareType))
	{
		if (GetFileFromBufEx(bufPtr, header,
							 nFirmwareType, szUpdateFile) != 0)
		{
			__ERR("Get firmware file %s failed\n", szUpdateFile);
			return -1;
		}
	}

	return 0;
}

static int FlashSpiNOR(const char *pFileName, const char *pBlockName, unsigned long offset)
{
	if (NULL == pFileName || *pFileName == 0)
		return -1;

	if (NULL == pBlockName || *pBlockName == 0)
		return -1;

	char cmd[200];
	anj_mw_system("cp /bin/busybox /tmp/busybox && chmod +x /tmp/busybox");
	anj_mw_system("ln -s /tmp/busybox /tmp/flash_eraseall");
	anj_mw_system("ln -s /tmp/busybox /tmp/flashcp");
	anj_mw_system("reboot --help"); // use to reboot

	WatchDogFeed();

	int i = 0;
	for (i = 0; i < 5; i++)
	{
		__ERR("try update firmware, times=%d\n", i);
		sprintf(cmd, "/tmp/flash_eraseall %s", pBlockName);
		if (anj_mw_system(cmd))
		{
			__ERR("flash_eraseall block %s fail\n", pBlockName);
			continue;
		}

		WatchDogFeed();

		sprintf(cmd, "/tmp/flashcp -v %s %s", pFileName, pBlockName);
		if (anj_mw_system(cmd))
		{
			__ERR("flashcp file(%s) to block(%s) fail\n", pFileName, pBlockName);
			continue;
		}

		break;
	}

	WatchDogFeed();

	return 0;
}

int FlashSpiNAND(char *pFileName, const char *pBlockName, unsigned long offset)
{
	if (NULL == pFileName || *pFileName == 0)
		return -1;

	if (NULL == pBlockName || *pBlockName == 0)
		return -1;

	char cmd[256];

	WatchDogFeed();

	anj_mw_system("cp /bin/busybox /tmp/busybox && chmod +x /tmp/busybox");
	anj_mw_system("ln -s /tmp/busybox /tmp/flash_eraseall");
	anj_mw_system("ln -s /tmp/busybox /tmp/nandwrite");

	anj_mw_system("echo --help > /tmp/nandwrite.txt");	// use to force reset
	anj_mw_system("reboot --help > /tmp/nandwrite.txt"); // use to reboot

	anj_mw_system("sync");
	usleep(100 * 1000);

	sprintf(cmd, "/tmp/flash_eraseall %s", pBlockName);
	if (anj_mw_system(cmd))
	{
		__ERR("flash_eraseall block %s fail\n", pBlockName);
		return -1;
	}

	WatchDogFeed();
	sprintf(cmd, "/tmp/nandwrite -s %ld -p %s %s > /tmp/nandwrite.txt", offset, pBlockName, pFileName);
	int ret = anj_mw_system(cmd);
	if (ret != 0)
	{
		__ERR("nandwrite %s->%s return %d, error %d(%s).\n", pFileName, pBlockName, ret, errno, strerror(errno));
		if (-1 == access(pFileName, 0))
		{
			__ERR("not exist: %s\n", pFileName);
		}
		else
		{
			struct stat mstat;
			memset(&mstat, 0, sizeof(mstat));

			if (lstat(pFileName, &mstat) == -1)
			{
				__ERR("get file stat error: %s\n", pFileName);
			}
			else
			{
				unsigned int crc = 0;
				int filesize = mstat.st_size;
				__ERR("file %s size: %d\n", pFileName, filesize);
				int ret = anj_crc32_file(pFileName, &crc);
				if (ret != 0)
				{
					printf("get file %s crc error\n", pFileName);
				}
				else
				{
					printf("get file %s crc: %#x\n", pFileName, crc);
				}
			}
		}
		anj_mw_system("cat /tmp/nandwrite.txt");
		WatchDogFeed();
		usleep(1000 * 1000);
		return -1;
	}

	anj_mw_system("cat /tmp/nandwrite.txt");

	anj_mw_system("sync");
	WatchDogFeed();
	usleep(1000 * 1000);
	return 0;
}

int FlashNand(char *pFileName, const char *pBlockName, unsigned long offset)
{
	if (NULL == pFileName || *pFileName == 0)
		return -1;

	if (NULL == pBlockName || *pBlockName == 0)
		return -1;

	if (SUPPORT_NAND_FLASH)
		return FlashSpiNAND(pFileName, pBlockName, offset);
	else
		return FlashSpiNOR(pFileName, pBlockName, offset);

	return 0;
}

static ssize_t full_write(int fd, void *buf, size_t len)
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
				/* we already wrote some! */
				/* user can do another write to know the error code */
				return total;
			}
			return cc; /* write() returns -1 on failure. */
		}

		total += cc;
		buf = buf + cc;
		len -= cc;
	}

	return total;
}

int FlashMTD_byFileOffset(const char *filename, int offset, int size, const char *devicename)
{
	printf("%s -> %s, offset=%d, size=%d\n", filename, devicename, offset, size);

	int err = 0;
	int fd_f = -1, fd_d = -1;
	int i;
	struct mtd_info_user mtd;
	struct erase_info_user e;
	char buf[BUFSIZE] = "";
	char buf2[BUFSIZE] = "";

	do
	{
		fd_f = open(filename, O_RDONLY);
		if (fd_f < 0)
		{
			__ERR("Open %s failed\n", filename);
			err = 1;
			break;
		}
		fd_d = open(devicename, O_SYNC | O_RDWR);
		if (fd_d < 0)
		{
			__ERR("Open %s failed\n", devicename);
			err = 1;
			break;
		}

		if (ioctl(fd_d, MEMGETINFO, &mtd) < 0)
		{
			__ERR("%s is not a MTD flash device\n", devicename);
			err = 1;
			break;
		}
		if (size > mtd.size)
		{
			__ERR("%s size bigger than %s\n", filename, devicename);
			err = 1;
			break;
		}

		/* always erase a complete block */
		/* erase 1 block at a time to be able to give verbose output */
		e.length = mtd.erasesize;
		e.start = 0;

		int erase_size = 64 * 1024;
		int left_size = size;
		while (left_size > 0)
		{
			if (left_size >= 64 * 1024)
			{
				erase_size = 64 * 1024;
			}
			else
			{
				erase_size = mtd.erasesize;
			}

			e.length = erase_size;

			//		printf("e.start=%d, mtd.erasesize=%d, erase_size=%d\n",e.start,  mtd.erasesize, erase_size);

			if (ioctl(fd_d, MEMERASE, &e) < 0)
			{
				__ERR("erase error at 0x%llx on %s\n",
					  (long long)e.start, devicename);
				err = -1;
				break;
			}

			e.start += erase_size;
			left_size -= erase_size;
		}

		if (err != 0)
		{
			break;
		}

		/*for writing and verifying */
		for (i = 0; i <= 1; i++)
		{
			int done;
			unsigned count;

			lseek(fd_f, offset, SEEK_SET);
			lseek(fd_d, 0, SEEK_SET);
			done = 0;
			count = BUFSIZE;
			while (1)
			{
				int rem;

				if (i == 0)
				{
					printf("\r Writing kb %d/%d", done / 1024, size / 1024);
				}
				else
				{
					printf("\r Verifying kb %d/%d", done / 1024, size / 1024);
				}
				// fflush(stdout);

				rem = size - done;
				if (rem == 0)
					break;
				if (rem < BUFSIZE)
					count = rem;
				read(fd_f, buf, count);
				if (i == 0)
				{
					int ret;
					if (count < BUFSIZE)
						memset((char *)buf + count, 0, BUFSIZE - count);
					// errno = 0;
					ret = full_write(fd_d, buf, BUFSIZE);
					if (ret != BUFSIZE)
					{
						printf("write error at 0x%x on %s, "
							   "write returned %d\n",
							   done, devicename, ret);
						err = -1;
						break;
					}
				}
				else
				{ /* i == 1 */
					read(fd_d, buf2, count);
					if (memcmp(buf, buf2, count) != 0)
					{
						printf("verification mismatch at 0x%x\n", done);
						err = -1;
						break;
					}
				}

				if (err != 0)
				{
					break;
				}

				done += count;
			}

			printf("\n");
			if (err != 0)
			{
				break;
			}
		}

		printf("done!\n");
	} while (0);

	if (fd_f >= 0)
		close(fd_f);
	if (fd_d >= 0)
		close(fd_d);

	return err;
}

int FlashMTD_byBufferOffset(char *buffer, int offset, int size, const char *devicename)
{
	printf("buffer %s -> %s, offset=%d, size=%d\n", buffer, devicename, offset, size);

	int err = 0;
	char *pSrc = NULL;
	int fd_d = -1;
	int i;
	struct mtd_info_user mtd;
	struct erase_info_user e;
	char buf[BUFSIZE] = "";
	char buf2[BUFSIZE] = "";

	do
	{
		fd_d = open(devicename, O_SYNC | O_RDWR);
		if (fd_d < 0)
		{
			__ERR("Open %s failed\n", devicename);
			err = 1;
			break;
		}

		if (ioctl(fd_d, MEMGETINFO, &mtd) < 0)
		{
			__ERR("%s is not a MTD flash device\n", devicename);
			err = 1;
			break;
		}
		if (size > mtd.size)
		{
			__ERR("size %#x bigger than %s\n", size, devicename);
			err = 1;
			break;
		}

		/* always erase a complete block */
		/* erase 1 block at a time to be able to give verbose output */
		e.length = mtd.erasesize;
		e.start = 0;

		int erase_size = 64 * 1024;
		int left_size = size;
		while (left_size > 0)
		{
			if (left_size >= 64 * 1024)
			{
				erase_size = 64 * 1024;
			}
			else
			{
				erase_size = mtd.erasesize;
			}

			e.length = erase_size;
			//	printf("e.start=%d, mtd.erasesize=%d, erase_size=%d, left_size=%d\n",e.start,  mtd.erasesize,  erase_size,  left_size);

			//		printf("\r left_size=%d", left_size);

			if (ioctl(fd_d, MEMERASE, &e) < 0)
			{
				__ERR("erase error at 0x%llx on %s\n",
					  (long long)e.start, devicename);
				err = -1;
				break;
			}

			e.start += erase_size;
			left_size -= erase_size;
		}

		if (err != 0)
		{
			break;
		}

		/*for writing and verifying */
		for (i = 0; i <= 1; i++)
		{
			int done;
			unsigned count;

			pSrc = buffer + offset;
			lseek(fd_d, 0, SEEK_SET);
			done = 0;
			count = BUFSIZE;
			while (1)
			{
				int rem;

				if (i == 0)
				{
					printf("\r Writing kb %d/%d", done / 1024, size / 1024);
				}
				else
				{
					printf("\r Verifying kb %d/%d", done / 1024, size / 1024);
				}
				// fflush(stdout);

				rem = size - done;
				if (rem == 0)
					break;
				if (rem < BUFSIZE)
					count = rem;

				memcpy(buf, pSrc, count);
				pSrc += count;
				if (i == 0)
				{
					int ret;
					if (count < BUFSIZE)
						memset((char *)buf + count, 0, BUFSIZE - count);
					// errno = 0;
					ret = full_write(fd_d, buf, BUFSIZE);
					if (ret != BUFSIZE)
					{
						printf("write error at 0x%x on %s, "
							   "write returned %d\n",
							   done, devicename, ret);
						err = -1;
						break;
					}
				}
				else
				{ /* i == 1 */
					read(fd_d, buf2, count);
					if (memcmp(buf, buf2, count) != 0)
					{
						printf("verification mismatch at 0x%x\n", done);
						err = -1;
						break;
					}
				}

				if (err != 0)
				{
					break;
				}

				done += count;
			}

			printf("\n");
			if (err != 0)
			{
				break;
			}
		}

		printf("done!\n");
	} while (0);

	if (fd_d >= 0)
		close(fd_d);

	return err;
}
