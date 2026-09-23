#ifndef __ANJ_SYSCTL_H__
#define __ANJ_SYSCTL_H__

#define TIME_INIT_YEAR 2018
#define TIME_INIT_MONTH 1
#define TIME_INIT_DAY 1
#define TIME_INIT_HOUR 0
#define TIME_INIT_MIN 0
#define TIME_INIT_SEC 0
#define TIME_FILE_LAST "manualtime"

#if defined(__cplusplus)
extern "C"
{
#endif

#define SYSTEM_CONTROL_FILE_FLAG "SYSTEM_CONTROL_FILE_FLAG"
#define SYSTEM_CONTROL_FILE_FLAG_LEN 32
#define SYSTEM_CONTROL_FILE_ENCRYPT_KEY_LEN 32
#define MAX_ENCRYPT_DATA_LEN 3000
#define MIN_SYSTEM_CONTROL_FILE_LEN (SYSTEM_CONTROL_FILE_FLAG_LEN + sizeof(int) * 3 + SYSTEM_CONTROL_FILE_ENCRYPT_KEY_LEN)

#define ENCRYPT_TYPE_NONE 0
#define ENCRYPT_TYPE_XOR 1

typedef struct
{
	char flag[SYSTEM_CONTROL_FILE_FLAG_LEN];
	int fileSize;
	int crc;
	int encryptType;
	char encryptKey[SYSTEM_CONTROL_FILE_ENCRYPT_KEY_LEN];
	int encryptedDataLen;
	char encryptedData[MAX_ENCRYPT_DATA_LEN];
} SYSTEM_CONTROL_FILE;

#define MAX_SERIAL_NUMBER_COUNT 1000
#define MANUFACTUER_CODE_LEN 8
#define SERIAL_NUMBER_LEN 8
#define MAX_SYSTEM_CONTROL_XML_DATA_LEN 2000

#define MIN_SYSTEM_CONTROL_DATA_LEN (MANUFACTUER_CODE_LEN + sizeof(int) + SERIAL_NUMBER_LEN + sizeof(int))

#define SERIAL_CONTROL_FLAG_SINGLE 0
#define SERIAL_CONTROL_FLAG_RANGE 1
#define SERIAL_CONTROL_FLAG_MULTI 2

#define MAX_SYSTEM_CONTROL_STRING_LEN 2048

typedef struct
{
	char manufactuerCode[MANUFACTUER_CODE_LEN];
	int serialControlFlag;
	union SerialNumberData
	{
		struct SINGLE_MODE
		{
			char serialNum[SERIAL_NUMBER_LEN];
		} singleMode;
		struct RANGE_MODE
		{
			char startSerialNum[SERIAL_NUMBER_LEN];
			char endSerialNum[SERIAL_NUMBER_LEN];
		} rangeMode;
		struct MULTI_MODE
		{
			int serialNumCount;
			char serialNums[MAX_SERIAL_NUMBER_COUNT][SERIAL_NUMBER_LEN];
		} multiMode;
	} serialNumData;

	int xmlLen;
	char xmlControlData[MAX_SYSTEM_CONTROL_XML_DATA_LEN];
} SYSTEM_CONTROL_DATA;

typedef int (*EncryptMethod)(SYSTEM_CONTROL_FILE *sysCtrlFile, SYSTEM_CONTROL_DATA *sysCtrlData, char *encryptKey, int keyLen);
typedef int (*DecryptMethod)(SYSTEM_CONTROL_FILE *sysCtrlFile, SYSTEM_CONTROL_DATA *sysCtrlData);

typedef struct
{
	int encryptType;
	EncryptMethod encryptMethod;
} EncryptMethodPair;

typedef struct
{
	int encryptType;
	DecryptMethod decryptMethod;
} DecryptMethodPair;

typedef struct
{
	int encryptType;
	char encryptKey[SYSTEM_CONTROL_FILE_ENCRYPT_KEY_LEN];
	SYSTEM_CONTROL_DATA sysCtrlData;
} SystemControlOption;

int GetSystemControlFromFile(const char *filePath, char *control_buf);

int find_and_check_zombie(const char *process_name);

int check_process_finish(const char *process_name);
int killprocess(const char *processname);
int killprocess_force(const char *processname);
void uboot_update();
int get_cloud_authcode_needed();

void anj_sysctl_capability_add(const char *szFunction);
void anj_sysctl_capability_remove(const char *szFunction);
int anj_sysctl_capability_check(const char *szFunction);
int anj_sysctl_capability_init();
char *anj_sysctl_get_capability_string();

#if defined(__cplusplus)
}
#endif

#endif
