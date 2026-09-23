#ifndef __ANJ_COMM_H__
#define __ANJ_COMM_H__

#include "ixml.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

enum
{
   SERACH_FILE_MEDIA_TYPE_BASE = 0,
   SERACH_FILE_MEDIA_TYPE_AUDIO,
   SERACH_FILE_MEDIA_TYPE_VIDEO,
   SERACH_FILE_MEDIA_TYPE_AUDIOVIDEO,
   SERACH_FILE_MEDIA_TYPE_IMAGE,
   SERACH_FILE_MEDIA_TYPE_OEMMP3,
   SERACH_FILE_MEDIA_TYPE_OEMAPP,
   SERACH_FILE_MEDIA_TYPE_OEMLOGO,
   SERACH_FILE_MEDIA_TYPE_CERTIFICATION,
   SERACH_FILE_MEDIA_TYPE_KEY,
};


#define DATA_BLOCK_MOUNT_PATH   "/mnt/nand"
#define OEM_MOUNT_PATH          "/tmp/oem"
#define OEM_MOUNT_PATH2         "/tmp/oem2"


#define OTA_FINISHED_FLAG DATA_BLOCK_MOUNT_PATH "/flag.ota.updated"

void ShowString(const char *szFunc, int nType, const char *szPrintLine);

void debug_show_data_hex(const unsigned char *data, unsigned short length, int nType);

void debug_show_data_hex_ex(const unsigned char *data, unsigned int length, int nType);

typedef enum
{
    THREAD_STATUS_RUNNING = 0, /* 线程运行*/
    THREAD_STATUS_WAIT,        /* 线程等待暂停 */
    THREAD_STATUS_PAUSE,       /* 线程暂停 */
} THREAD_RUN_STATUS;

char *GetRequestParamValue(IXML_Document *pDoc, char *fieldName);
char *GetRequestParamValueByName(IXML_Document *pDoc, char *tag_name, const char *fieldName);

char *GetFileNameFromFullName(const char *full_file_name);
unsigned long long GetPathFreeSpace(const char *path);

int GetDeviceTypeStr(char *szDeviceType);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
