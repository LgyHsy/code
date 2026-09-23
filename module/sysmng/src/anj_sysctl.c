#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_file.h"
#include "anj_mw_crypt.h"
#include "anj_mw_mutex.h"
#include "anj_sysctl.h"
#include "anj_sysmng.h"
#include "anj_config.h"
#include "function_list.h"

#include <stdio.h>
#include <errno.h>
#include <dirent.h>

#define LEN_PATH 256

#define FILE_AUDIOPROMPT_PATH_DEFAULT "/opt/ch/audioprompt"

#define CHECK_CUR_POS(pos)                                                                                                \
	do                                                                                                                    \
	{                                                                                                                     \
		if (pos > dataLen)                                                                                                \
		{                                                                                                                 \
			__ERR("check cur pos (pos = %d, datalen = %d) system control file error(line %d)\n", pos, dataLen, __LINE__); \
			return -1;                                                                                                    \
		}                                                                                                                 \
	} while (0);

#define __SHOWLOG__(format, arg...) printf("%llu [%s:%d] " format "\n", anj_mw_get_cputime_ms(NULL), __func__, __LINE__, ##arg)

static char g_system_control_string[MAX_SYSTEM_CONTROL_STRING_LEN] = {0};
static pthread_mutex_t s_stSysCtlStrMutex = PTHREAD_MUTEX_INITIALIZER;

static int is_zombie(int pid)
{
	char path[64] = {0};
	char buffer[1024] = {0};
	snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
	if (read_file_to_buffer(path, buffer, sizeof(buffer)) == -1)
	{
		__ERR("is_zombie read_file_to_buffer error\n");
		return -1; // 读取失败
	}
	// 查找"State"行，并检查是否包含'Z'（表示僵尸进程）
	const char *state = strstr(buffer, "State:\t");
	if (state && strchr(state + 7, 'Z'))
	{
		__ERR("pid:%d is_zombie\n", pid);
		return 1; // 是僵尸进程
	}
	__ERR("pid:%d no is_zombie\n", pid);
	return 0; // 不是僵尸进程
}

int ConstructSystemControlDataByDecryptedContent(char *decryptedData, int dataLen, SYSTEM_CONTROL_DATA *sysCtrlData)
{
	unsigned char sn[256] = {0};

	char tmp[32] = {0};
	char start_sn[64] = {0};
	char end_sn[64] = {0};
	char my_sn[64] = {0};
	int i;

	if (anj_sysmng_get_sn(sn, sizeof(sn)) < 0)
	{
		__ERR("read serial number failed\n");
		return -1;
	}

	memset(sysCtrlData, 0, sizeof(SYSTEM_CONTROL_DATA));

	if (dataLen < MIN_SYSTEM_CONTROL_DATA_LEN)
	{
		__ERR("system control file error\n");
		return -1;
	}

	int curPos = 0;

	memcpy(sysCtrlData->manufactuerCode, decryptedData, MANUFACTUER_CODE_LEN);
	curPos += MANUFACTUER_CODE_LEN;

	sysCtrlData->serialControlFlag = *((int *)(decryptedData + curPos));
	curPos += sizeof(int);

	__ERR("sysCtrlData->serialControlFlag = %d\n", sysCtrlData->serialControlFlag);

	if (sysCtrlData->serialControlFlag == SERIAL_CONTROL_FLAG_SINGLE)
	{
		memcpy(sysCtrlData->serialNumData.singleMode.serialNum, (decryptedData + curPos), SERIAL_NUMBER_LEN);
		curPos += SERIAL_NUMBER_LEN;

		for (i = 0; i < 8; i++)
		{
			if (sysCtrlData->serialNumData.singleMode.serialNum[i] != 0)
			{
				if (memcmp(sysCtrlData->serialNumData.singleMode.serialNum, sn, 8))
				{
					__ERR("serial number not match, update system control failed.\n");
					return -1;
				}
			}
		}
	}
	else if (sysCtrlData->serialControlFlag == SERIAL_CONTROL_FLAG_RANGE)
	{
		CHECK_CUR_POS(curPos + SERIAL_NUMBER_LEN)

		memcpy(sysCtrlData->serialNumData.rangeMode.startSerialNum, (decryptedData + curPos),
			   SERIAL_NUMBER_LEN);
		curPos += SERIAL_NUMBER_LEN;

		CHECK_CUR_POS(curPos + SERIAL_NUMBER_LEN)

		memcpy(sysCtrlData->serialNumData.rangeMode.endSerialNum, (decryptedData + curPos),
			   SERIAL_NUMBER_LEN);
		curPos += SERIAL_NUMBER_LEN;

		strcpy(start_sn, "");
		strcpy(end_sn, "");
		strcpy(my_sn, "");
		for (i = 0; i < 8; i++)
		{
			sprintf(tmp, "%02X", sn[i]);
			strcat(my_sn, tmp);

			sprintf(tmp, "%02X", sysCtrlData->serialNumData.rangeMode.startSerialNum[i]);
			strcat(start_sn, tmp);

			sprintf(tmp, "%02X", sysCtrlData->serialNumData.rangeMode.endSerialNum[i]);
			strcat(end_sn, tmp);
		}

		__ERR("start sn: %s, end_sn: %s, my_sn: %s\n",
			  start_sn, end_sn, my_sn);

		if (strcmp(my_sn, start_sn) < 0)
		{
			__ERR("serial number not match, update system control failed.\n");
			return -1;
		}

		if (strcmp(my_sn, end_sn) > 0)
		{
			__ERR("serial number not match, update system control failed.\n");
			return -1;
		}
	}
	else if (sysCtrlData->serialControlFlag == SERIAL_CONTROL_FLAG_MULTI)
	{
		CHECK_CUR_POS(curPos + sizeof(int))

		sysCtrlData->serialNumData.multiMode.serialNumCount = *((int *)(decryptedData + curPos));
		curPos += sizeof(int);

		int i = 0;
		int match = 0;
		for (; i < sysCtrlData->serialNumData.multiMode.serialNumCount; i++)
		{
			CHECK_CUR_POS(curPos + SERIAL_NUMBER_LEN)

			memcpy(&(sysCtrlData->serialNumData.multiMode.serialNums[i][0]), (decryptedData + curPos), SERIAL_NUMBER_LEN);
			curPos += SERIAL_NUMBER_LEN;

			if (memcmp(sn, sysCtrlData->serialNumData.multiMode.serialNums[i], 8) == 0)
			{
				match = 1;
			}
		}

		if (!match)
		{
			__ERR("serial number not match, update system control failed.\n");
			return -1;
		}
	}

	CHECK_CUR_POS(curPos + sizeof(int))

	sysCtrlData->xmlLen = *((int *)(decryptedData + curPos));
	curPos += sizeof(int);

	CHECK_CUR_POS(curPos + sysCtrlData->xmlLen)

	memcpy(sysCtrlData->xmlControlData, (decryptedData + curPos), sysCtrlData->xmlLen);
	curPos += sysCtrlData->xmlLen;

	return 0;
}

static int DecryptForNone(SYSTEM_CONTROL_FILE *sysCtrlFile, SYSTEM_CONTROL_DATA *sysCtrlData)
{
	return ConstructSystemControlDataByDecryptedContent(sysCtrlFile->encryptedData, sysCtrlFile->encryptedDataLen, sysCtrlData);
}

static int DecryptForXor(SYSTEM_CONTROL_FILE *sysCtrlFile, SYSTEM_CONTROL_DATA *sysCtrlData)
{
	int iRet = 0;
	char *oriContent = anj_mw_malloc(sysCtrlFile->encryptedDataLen);
	if (oriContent == NULL)
	{
		__ERR("malloc failed\n");
		return -1;
	}

	int i = 0;
	int j = 0;

	for (; i < sysCtrlFile->encryptedDataLen; i++)
	{
		oriContent[i] = sysCtrlFile->encryptedData[i] ^ sysCtrlFile->encryptKey[j];

		j++;
		if (j == SYSTEM_CONTROL_FILE_ENCRYPT_KEY_LEN)
		{
			j = 0;
		}
	}

	iRet = ConstructSystemControlDataByDecryptedContent(oriContent, sysCtrlFile->encryptedDataLen, sysCtrlData);
	anj_mw_free(oriContent);
	return iRet;
}

DecryptMethodPair decryptMethods[] =
	{
		{ENCRYPT_TYPE_NONE, DecryptForNone},
		{ENCRYPT_TYPE_XOR, DecryptForXor},
		{0XFFFFFFFF, 0}};

int DecryptSystemControlData(SYSTEM_CONTROL_FILE *sysCtrlFile, SYSTEM_CONTROL_DATA *sysCtrlData)
{
	int encryptType = sysCtrlFile->encryptType;
	__INFO("encryptType = %d\n", encryptType);

	int i = 0;
	DecryptMethod method = NULL;

	while (decryptMethods[i].encryptType != 0XFFFFFFFF)
	{
		if (decryptMethods[i].encryptType == encryptType)
		{
			method = decryptMethods[i].decryptMethod;
			break;
		}
		i++;
	}

	if (method == NULL)
	{
		__ERR("No DecryptMethod Mathch  EncryptType(%d)\n", encryptType);
	}

	return (*method)(sysCtrlFile, sysCtrlData);
}

static int ConstructSystemControlFile(char *buf, int bufLen, SYSTEM_CONTROL_FILE *sysCtrlFile)
{
	if (bufLen <= MIN_SYSTEM_CONTROL_FILE_LEN)
	{
		__ERR("system control file  file_len error\n");
		return -1;
	}

	memcpy(sysCtrlFile, buf, MIN_SYSTEM_CONTROL_FILE_LEN);
	sysCtrlFile->encryptedDataLen = bufLen - MIN_SYSTEM_CONTROL_FILE_LEN;

	if (sysCtrlFile->encryptedDataLen > MAX_ENCRYPT_DATA_LEN)
	{
		return -1;
	}

	memcpy(sysCtrlFile->encryptedData, buf + MIN_SYSTEM_CONTROL_FILE_LEN, sysCtrlFile->encryptedDataLen);

	return 0;
}

int GetSystemControlFromFile(const char *filePath, char *control_buf)
{
	int iRet = -1;
	char *buf = NULL;
	ANJ_CHK((filePath != NULL) && (control_buf != NULL), -1, "input Invalid");
	unsigned long long fileLen = 0;
	ANJ_CHK_FUNC(anj_mw_read_file_len(filePath, &fileLen), 0, "read file err");

	buf = (char *)anj_mw_malloc(fileLen);
	ANJ_CHK((buf != NULL), -1, "malloc failed");

	ANJ_CHK_FUNC(anj_mw_read_file(filePath, buf, &fileLen), 0, "read file err");

	SYSTEM_CONTROL_FILE sysCtrlFile = {0};
	ANJ_CHK_FUNC(ConstructSystemControlFile(buf, fileLen, &sysCtrlFile), 0, "construct system control file error");

	anj_mw_free(buf);
	buf = NULL;

	// check the flag;
	if (memcmp(sysCtrlFile.flag, SYSTEM_CONTROL_FILE_FLAG, strlen(SYSTEM_CONTROL_FILE_FLAG)) != 0)
	{
		__ERR("FLAG NOT MATCH\n");
		iRet = 0;
		goto endFunc;
	}

	__ERR("flag match.\n");

	// check the filesize ;
	if (sysCtrlFile.fileSize != fileLen)
	{
		__ERR("FILE SIZE  MATCH\n");
		iRet = 0;
		goto endFunc;
	}

	__ERR("file size mathc.\n");

	// check crc
	int actualCrc = anj_crc32_update(0, (unsigned char *)sysCtrlFile.encryptedData, sysCtrlFile.encryptedDataLen);
	if (sysCtrlFile.crc != actualCrc)
	{
		__ERR("CRC NOT MATCH\n");
		iRet = 0;
		goto endFunc;
	}

	__ERR("crc ok.\n");

	SYSTEM_CONTROL_DATA ctrlData = {0};
	ANJ_CHK_FUNC(DecryptSystemControlData(&sysCtrlFile, &ctrlData), 0, "decrypt system control file error");

	strncpy(control_buf, ctrlData.xmlControlData, ctrlData.xmlLen);
	control_buf[ctrlData.xmlLen] = '\0';
	iRet = ctrlData.xmlLen;
	if (ctrlData.xmlLen)
	{
		__ERR("decrypt ok, len=%d, str=%s\n", ctrlData.xmlLen, control_buf);
	}
	else
	{
		__ERR("decrypt ok, len=0\n");
	}

endFunc:
	if (buf)
	{
		anj_mw_free(buf);
	}
	return iRet;
}

// 查找具有指定名称的进程PID，并检查是否为僵尸进程
int find_and_check_zombie(const char *process_name)
{
	DIR *dir;
	struct dirent *entry;
	int pid;
	dir = opendir("/proc");
	if (!dir)
	{
		__ERR("Failed to open /proc");
		return -1;
	}
	while ((entry = readdir(dir)) != NULL)
	{
		if (sscanf(entry->d_name, "%d", &pid) == 1)
		{
			char cmdline_path[64] = {0};
			char cmdline_buffer[4096] = {0};
			snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%ld/status", (long)pid);
			if (read_file_to_buffer(cmdline_path, cmdline_buffer, sizeof(cmdline_buffer)) == -1)
			{
				__ERR("find_and_check_zombie read_file_to_buffer error\n");
				continue; // 读取失败，跳过
			}
			// 检查进程名（这里简单处理为包含关系）
			if (strstr(cmdline_buffer, process_name) != NULL)
			{
				int is_z = is_zombie(pid);
				closedir(dir);
				return is_z; // 找到匹配项，返回是否为僵尸进程
			}
		}
	}
	closedir(dir);
	__ERR("find_and_check_zombie no process_name\n");
	return -1; // 未找到匹配项
}

int check_process_finish(const char *process_name)
{
	char cmd[256] = {0};
	char pid_str[256] = {0};

	sprintf(cmd, "ps ");

	sprintf(cmd + strlen(cmd), "|grep %s|grep -v grep|grep -v sh|awk \'{print $1}\'", process_name);

	sprintf(cmd, "ps | grep %s | grep -v grep | grep -v sh", process_name);

	FILE *fp = popen(cmd, "r");
	if (fp == NULL)
	{
		__ERR("popen return NULL.\n");
		return 1;
	}
	else
	{
		int flen = anj_mw_fread(fp, pid_str, sizeof(pid_str));
		pclose(fp);

		if (flen > 0)
		{
			pid_str[flen] = 0;
			return 0;
		}
		else
		{
			return 1;
		}
	}
}

int killprocess(const char *processname)
{
	int ret = 0;
	int i = 0;

	ret = check_process_finish(processname);
	if (ret)
	{
		return 1;
	}

	ret = 0;

	__ERR("start to kill %s...\n", processname);

	char cmd[128] = {0};
	sprintf(cmd, "killall %s", processname);

	anj_mw_system(cmd);
	usleep(1 * 1000);

	ret = 0;
	for (i = 0; i < 100; i++)
	{
		ret = check_process_finish(processname);
		if (ret)
		{
			__ERR("%s stop ok!!!\n", processname);
			break;
		}

		anj_mw_system(cmd);

		usleep(1 * 1000);
	}

	if (ret == 0)
	{
		__ERR("wait %s exit time out, please try again.\n", processname);

		// 海思平台编码必须正常退出，才能释放MMZ，MMZ可用于升级
		// MSTAR平台可能编码异常始终不退出，所以强制杀掉
		sprintf(cmd, "killall -9 %s", processname);
		anj_mw_system(cmd);
	}

	ret = check_process_finish(processname);
	if (ret)
	{
		return 1;
	}

	return 0;
}

int killprocess_force(const char *processname)
{
	int ret = 0;
	int i = 0;

	ret = check_process_finish(processname);
	if (ret)
	{
		return 1;
	}

	ret = 0;

	__ERR("start to kill %s...\n", processname);

	char cmd[128] = {0};
	sprintf(cmd, "killall -9 %s", processname);

	anj_mw_system(cmd);
	usleep(1 * 1000);

	ret = 0;
	for (i = 0; i < 100; i++)
	{
		ret = check_process_finish(processname);
		if (ret)
		{
			__ERR("%s stop ok!!!\n", processname);
			break;
		}

		anj_mw_system(cmd);
		usleep(1000);
	}

	ret = check_process_finish(processname);
	if (ret)
	{
		return 1;
	}

	return 0;
}

void uboot_update()
{
	struct dirent *ent;
	DIR *dp;
	char path[LEN_PATH] = "";
	char szUbootFileName[512] = "";
	char szUbootIdentity[LEN_PATH] = "";
	char szUbootIdentityFile[512] = "";
	sprintf(path, "%s", "/opt/ch/");
	dp = opendir(path);
	if (dp == NULL)
	{
		return;
	}

	for (;;)
	{
		ent = readdir(dp);
		if (ent == NULL)
			break;

		if (ent->d_name[0] == '.')
		{
			if (!ent->d_name[1] || (ent->d_name[1] == '.' && !ent->d_name[2]))
			{
				continue;
			}
		}

		char szFileName[LEN_PATH] = {0};
		snprintf(szFileName, sizeof(szFileName), "%s", (char *)ent->d_name);
		int ret = sscanf(szFileName, "u-boot.xz.img_%[^.].bin", szUbootIdentity);
		if (ret != 1)
		{
			continue;
		}

		snprintf(szUbootFileName, sizeof(szUbootFileName), "%s/%s", path, szFileName);
		__SHOWLOG__("Get uboot %s, identity %s.\n", szUbootFileName, szUbootIdentity);
		break;
	}
	closedir(dp);

	if (strlen(szUbootFileName) == 0 || strlen(szUbootIdentity) == 0)
		return;

	snprintf(szUbootIdentityFile, sizeof(szUbootIdentityFile), "/mnt/nand/uboot_flag_%s", szUbootIdentity);
	if (-1 != access(szUbootIdentityFile, 0))
	{
		__SHOWLOG__("uboot %s already updated. egnored!\n", szUbootFileName);
		return;
	}

	__SHOWLOG__("Ready to update: %s \n", szUbootFileName);

	anj_mw_system("rm -f /mnt/nand/uboot_flag_*");

	if (soft_enc_uboot_write(szUbootFileName) != 0)
	{
		__ERR("Write uboot failed: %s\n", szUbootFileName);
	}

	__SHOWLOG__("Finished update: %s \n", szUbootFileName);

	anj_mw_create_file(szUbootIdentityFile, NULL);

	anj_mw_system("sync && sleep 3 && reboot");
}

// 设置是否在开启P2P后需要校验授权码才自动取P2PID
int get_cloud_authcode_needed()
{
	return CLOUD_AUTH_CODE_NEED;
}

static int anj_sysctl_overlay_capability_add()
{
	anj_sysctl_capability_add(FUNCTION_TITLE_BMP);
	anj_sysctl_capability_add(FUNCTION_USEROSD);

	return 0;
}

static int anj_sysctl_nfs_capability_add()
{
	return 0;
}

static int anj_sysctl_audio_capability_add()
{
	anj_sysctl_capability_add(FUNCTION_AUDIO_AMPLIFY);
	anj_sysctl_capability_remove(FUNCTION_AEC);
	anj_sysctl_capability_add(FUNCTION_AUDIOPLAY_ACTION_DAYNIGHT);
	anj_sysctl_capability_add(FUNCTION_RA_PCM);
	anj_sysctl_capability_add(FUNCTION_RA_MP3STREAM);

	return 0;
}

static int anj_sysctl_ptz_capability_add()
{
	// todo ptz模块能力集上报  移到模块内参考4g和wifi
	return 0;
}

static int anj_sysctl_gpio_capability_add()
{
	return 0;
}

void anj_sysctl_capability_add(const char *szFunction)
{
	if (NULL == szFunction)
		return;
	anj_mutex_lock(&s_stSysCtlStrMutex);
	if (strstr(g_system_control_string, szFunction) == NULL)
	{
		strcat(g_system_control_string, "+");
		strcat(g_system_control_string, szFunction);
	}
	anj_mutex_unlock(&s_stSysCtlStrMutex);
}

void anj_sysctl_capability_remove(const char *szFunction)
{
	if (NULL == szFunction)
		return;

	anj_mutex_lock(&s_stSysCtlStrMutex);
	char *p = strstr(g_system_control_string, szFunction);

	if (p != NULL)
	{
		char *pEnd = p + strlen(szFunction);
		*p = 0;
		if (*pEnd != 0)
			strcat(g_system_control_string, pEnd);
	}
	anj_mutex_unlock(&s_stSysCtlStrMutex);
}

int anj_sysctl_capability_check(const char *szFunction)
{
	if (NULL == szFunction)
		return 0;
	anj_mutex_lock(&s_stSysCtlStrMutex);
	if (strstr(g_system_control_string, szFunction) != NULL)
	{
		anj_mutex_unlock(&s_stSysCtlStrMutex);
		return 1;
	}
	anj_mutex_unlock(&s_stSysCtlStrMutex);

	return 0;
}

int anj_sysctl_capability_init()
{
	g_system_control_string[MAX_SYSTEM_CONTROL_STRING_LEN - 1] = '\0';

	anj_sysctl_gpio_capability_add();
	anj_sysctl_ptz_capability_add();
	anj_sysctl_nfs_capability_add();
	anj_sysctl_audio_capability_add();
	anj_sysctl_overlay_capability_add();

    anj_sysctl_capability_add(FUNCTION_ARMING_DAYNIGHT);
    anj_sysctl_capability_add(FUNCTION_ARMING_TOTALSWITCH);
    anj_sysctl_capability_add(FUNCTION_ARMING_BYTIME);
    anj_sysctl_capability_add(FUNCTION_ARMINGAUDIO_DESC);
    anj_sysctl_capability_add(FUNCTION_ALARM_SERVER);

	anj_sysctl_capability_add(FUNCTION_CHECK_INTERNET);
	anj_sysctl_capability_add(FUNCTION_DEVICE_REPORT);
	anj_sysctl_capability_add(FUNCTION_CREATE_TIMELAPSE_RECORD);
	anj_sysctl_capability_add(FUNCTION_CREATE_EXPORT_RECORD);
	anj_sysctl_capability_add(FUNCTION_RESTORE_RETAIN_PART);

	anj_sysctl_capability_add(FUNCTION_P2P_PRIVATE_CMD);
	anj_sysctl_capability_add(FUNCTION_FIXIP);

	anj_sysctl_capability_add(FUNCTION_VIDEO_QOS);
	anj_sysctl_capability_add(FUNCTION_AUDIO_PROMPT);

	anj_sysctl_capability_add(FUNCTION_MULTICAST);
	anj_sysctl_capability_add(FUNCTION_OSD_ANYPOSITION);
	anj_sysctl_capability_add(FUNCTION_ALOWIP_SETTING);

	anj_sysctl_capability_add(FUNCTION_BE_SET_MTU);

	anj_sysctl_capability_add(FUNCTION_COMM_ONVIF_ENABLE);
	anj_sysctl_capability_add(FUNCTION_TIMESPAN_NEW); // 支持新的7X24只精确到小时时间段配置
	anj_sysctl_capability_add(FUNCTION_SCARE_OFF);
	anj_sysctl_capability_add(FUNCTION_CLOUD_AUTHCODE);

	anj_sysctl_capability_add(FUNCTION_H5);

	anj_sysctl_capability_add(FUNCTION_QP);

	anj_sysctl_capability_add(FUNCTION_AF_VERSION);
	anj_sysctl_capability_add(FUNCTION_AF_Coordinate);

	anj_sysctl_capability_add(FUNCTION_VIDEO_CROP);

	anj_sysctl_capability_add(FUNCTION_VIDEO_FORCT_ANTIFLICKER);

	if (ANJ_CAMERA_MAX_NUMS > 1)
	{
		anj_sysctl_capability_add(FUNCTION_PD_TRACK_GUNBALL_SIMPLE);
	}

	anj_sysctl_capability_add(FUNCTION_P2P_CONFIG);

	anj_sysctl_capability_add(FUNCTION_P2P_SKYWORTH);

	anj_sysctl_capability_add(FUNCTION_VIDEO_FORBIT);

	// 支持对配置分区进行格式化，用于释放占用空间，会删除上传的mp3
	anj_sysctl_capability_add(FUNCTION_AUDIO_8M_Repartition);

	anj_sysctl_capability_add(FUNCTION_VIDEOSHUTTER);

	anj_sysctl_capability_add(FUNCTION_AUDIO);
	anj_sysctl_capability_add(FUNCTION_LANGUAGE_ZH_CN);
	anj_sysctl_capability_add(FUNCTION_LANGUAGE_ZH_TW);

	anj_sysctl_capability_add(FUNCTION_LANGUAGE_EN_US);
	anj_sysctl_capability_add(FUNCTION_LANGUAGE_RU_PY);
	anj_sysctl_capability_add(FUNCTION_LANGUAGE_KO_KO);

	anj_sysctl_capability_add(FUNCTION_FRONT_REPLAY);
	anj_sysctl_capability_add(FUNCTION_REPLAY_BYTIME); // 按时间点回放.V2.3.1版本以后支持
	anj_sysctl_capability_add(FUNCTION_MEDIA_CAPABILITY);
	anj_sysctl_capability_add(FUNCTION_PROFLE_SETTING);

	anj_sysctl_capability_add(FUNCTION_SYSTEM_MAINTAIN);

	anj_sysctl_capability_add(FUNCTION_IRCUT_SETTING);
	anj_sysctl_capability_add(FUNCTION_SEARCH_WIFIAP);

	if (TITLE_MAX_LEN >= 200)
		anj_sysctl_capability_add(FUNCTION_LONG_TITLE);

	anj_sysctl_capability_add(FUNCTION_TIMEZONE_HALFHOUR);

	anj_sysctl_capability_add(FUNCTION_P2P_CFG);

	anj_sysctl_capability_add(FUNCTION_VIDEOMASK_ONESET);

	anj_sysctl_capability_add(FUNCTION_WDR_SETTING);
	if (anj_mw_sensor_support_wdr())
	{
		anj_sysctl_capability_add(FUNCTION_HDR_SETTING);
	}

	// anj_sysctl_capability_add(FUNCTION_VIDEO_ENCODE_MODE);

	anj_sysctl_capability_add(FUNCTION_LED_TYPE);
	if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE)
	{
		anj_sysctl_capability_add(FUNCTION_LEDPANEL_WHITE);
	}
	else if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_RED)
	{
		anj_sysctl_capability_add(FUNCTION_LEDPANEL_IR);
	}
	else if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE_RED)
	{
		anj_sysctl_capability_add(FUNCTION_LEDPANEL_DOUBLE);
	}
	anj_sysctl_capability_add(FUNCTION_IRCUT_LED_DELAY);
	if (ANJ_LED_CFG_NEW)
	{
		anj_sysctl_capability_add(FUNCTION_LED_CFG);
	}

	anj_sysctl_capability_add(FUNCTION_IRCUT_LED_MANUAL_SWITCH);

	// anj_sysctl_capability_add(FUNCTION_HISCON_ENCMODE);

	anj_sysctl_capability_add(FUNCTION_EMAIL_SSL);

	if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
	{
		anj_sysctl_capability_add(FUNCTION_AOV_SUPPORT);
	}

	anj_sysctl_capability_add(FUNCTION_VIDEO_MASK);
	anj_sysctl_capability_add(FUNCTION_RECORD_ALARMLIST_SUPPORT);
	anj_sysctl_capability_add(FUNCTION_PRECOMM_PB_SUPPORT);

	anj_sysctl_capability_add(FUNCTION_SUPPORT_MP4);

	// Capability_add_extra_function_list();

	return 1;
}

char *anj_sysctl_get_capability_string()
{
	return g_system_control_string;
}