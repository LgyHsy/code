#ifndef __HTTP_DEF_H__
#define __HTTP_DEF_H__

#ifdef __cplusplus
extern "C"
{
#endif


#define HTTP_RES_STATUS_OK          200
#define HTTP_RES_STATUS_NOT_MODIFY  304
#define HTTP_RES_STATUS_BAD_REQUEST 400
#define HTTP_RES_STATUS_UNAUTH      403
#define HTTP_RES_STATUS_NOT_FOUND   404

#define HTTP_OK             0
#define SOAP_FILE           1003
#define SOAP_FAULT          12

#define HTTP_POST   "POST"
#define HTTP_GET    "GET"
#define HTTP_PUT    "PUT"

#define HUAWEI_WIFI_AUTH_NONE 0
#define HUAWEI_WIFI_AUTH_WEP_SHARED 1
#define HUAWEI_WIFI_AUTH_WEP_NONE 2
#define HUAWEI_WIFI_AUTH_WPA_PSK_TKIP 3
#define HUAWEI_WIFI_AUTH_WPA_PSK_AES 4
#define HUAWEI_WIFI_AUTH_WPA2_PSK_TKIP 5
#define HUAWEI_WIFI_AUTH_WPA2_PSK_AES 6

typedef enum 
{
    WebForm_MP3=1,
    WebForm_APP=2,
    WebForm_LOGO=3,
}WebFormFileType;

typedef enum 
{
    PostFileType_FirmwareUpgrade=1,
    PostFileType_DANALE_ID=2,
    PostFileType_CUSTOM=3,
    PostFileType_WebForm=4,
    PostFileType_CertificateForm=5,
    PostFileType_KeyForm=6,
    PostFileType_ConfigForm=7,
}PostFileType;

typedef struct
{
    unsigned char postFileType;
    int  socket;
    int fileSize;
    int recvSize;
    int hasStartBoundary;
    char strBoundary[256];
    char md5_str[64];
}FormDataBoundary;

typedef struct
{
    unsigned char postFileType;
    int  socket;
    unsigned long fileSize;
    unsigned long recvSize;
}HttpPostFileInfo;

typedef struct
{
    int ID;
    char file_pathname[128];
    char file_onlyname[128];
}AudioFile_Pathname;

typedef struct
{
    int Num;
    AudioFile_Pathname Item[32];
}AudioFileList;


typedef int (*cb_func_http_response) (void *pInst, const char *pResponse, int status);
typedef int (*cb_func_http_sendfile)(void *pInst, void *pmsgt, const char *filepath, const char *type);

int http_handle_init(cb_func_http_response cbResponse, cb_func_http_sendfile cbGetFile);

#ifdef __cplusplus
}
#endif

#endif
