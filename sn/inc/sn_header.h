#ifndef __SOFT_ENC_HEADER_H__
#define __SOFT_ENC_HEADER_H__

#include "ixml.h"
#include "anj_comm.h"
#include "anj_mw_log.h"

#define BROADCASTING_PORT_SN                3002
#define BROADCASTING_PORT_P2PID             3003
#define BROADCASTING_PORT_MACADDR           3004

#define IPC_MESSAGE_REQUEST_ID              101
#define IPC_MESSAGE_REQUEST_ID_RESPONSE     102

#define P2P_MAX_BUFFER_LEN                  5120
#define OEMAPPLY_MAX_BUFFER_LEN             5120
#define SN_MAX_BUFFER_LEN                   1024

#define SOFT_DATA_AUTH_TIME                 (5 * 60 * 1000)     // 5分钟
#define SOFT_DATA_CHECK_TIME                (500)               // 500ms监测一次

// Definitions - old save format magic (V1)
#define MAGIC1_V1 0x10402DE9
#define MAGIC2_V1 0xA0E15A38
#define MAGIC3_V1 0xE90140A0
#define MAGIC4_V1 0xA2DB0200

// New magic values for distinguishing new save format (V2)
#define MAGIC1_V2 (MAGIC1_V1 + 0x55)
#define MAGIC2_V2 (MAGIC2_V1 + 0xAA)
#define MAGIC3_V2 (MAGIC3_V1 + 0xCC)
#define MAGIC4_V2 (MAGIC4_V1 + 0xFF)

#define MAGIC_P2PID_1 0x404E4A50
#define MAGIC_P2PID_2 0x32504944

#define P2PID_SECT_OFFSET   2048
#define P2PID_EACH_SIZE     512     // 2048字节可以放4个
#define P2PID_DATA_OFFSET   sizeof(AjP2pIDHeader)
#define P2PID_MAX_SIZE  (P2PID_EACH_SIZE - P2PID_DATA_OFFSET)

#define FREE_SECT_BUFFER() do{ \
	if( NULL != pSectBuffer ) {	free(pSectBuffer);pSectBuffer = NULL;} } while(0)

#define SN_DEBUG 0

// Message types
typedef enum
{
    IPC_MESSAGE_V1_REQUEST_SN = 11,
    IPC_MESSAGE_V1_REQUEST_SN_RESPONSE = 12,
    IPC_MESSAGE_V1_REQUEST_UUID = 15,
    IPC_MESSAGE_V1_REQUEST_UUID_RESPONSE = 16,

    IPC_MESSAGE_V2_DESCOVERY = 101,
    IPC_MESSAGE_V2_DESCOVERY_RESPONSE = 102,
    IPC_MESSAGE_V2_REQUEST_UUID = 103,
    IPC_MESSAGE_V2_REQUEST_UUID_RESPONSE = 104,
    IPC_MESSAGE_V2_REQUEST_SN = 105,
    IPC_MESSAGE_V2_REQUEST_SN_RESPONSE = 106,
}SoftSnMessageType;

enum
{
    TYPE_ID_TYPE_DANALE             = 0,
    TYPE_ID_TYPE_GOOLINK            = 1,
    TYPE_ID_TYPE_EYEPLUS            = 2,
    TYPE_ID_TYPE_TUTK               = 3,
    TYPE_ID_TYPE_QINIU              = 4,
    TYPE_ID_TYPE_TUYA               = 5,
    TYPE_ID_TYPE_AC18PLUS_CONSUME   = 6,
    TYPE_ID_TYPE_TUYA_NVR           = 7,
    TYPE_ID_TYPE_TUYA_NVR_MAINPID   = 8,
    TYPE_ID_TYPE_TUYA_NVR_SUBPID    = 9,
    TYPE_ID_TYPE_TUYA_NVR_APPURL    = 10,
    TYPE_ID_TYPE_AC18PLUS_NVR       = 11,
    TYPE_ID_TYPE_AC18PRO_CONSUME    = 12,
    TYPE_ID_TYPE_AC18PRO_CMCC4G     = 13,
    TYPE_ID_TYPE_AC18PRO_NVR        = 14,
    TYPE_ID_TYPE_TENCENT_IOT_IPC    = 15,
    TYPE_ID_TYPE_AIOT_IPC           = 16,
    TYPE_ID_TYPE_AIOT_NVR           = 17,
    TYPE_ID_TYPE_DOT                = 18,
    TYPE_ID_TYPE_MAX,
};

typedef struct
{
    unsigned int magic1;
    unsigned int magic2;
    unsigned int len;           // 最大 1024-16
    unsigned int crc;           // CRC
}AjP2pIDHeader;

// AjSnHeaderV1 structure - V1 (backward compatibility)
typedef struct {
    unsigned int magic1;
    unsigned int magic2;
    unsigned int magic3;
    unsigned int magic4;
    unsigned int len;              // Max 64
    unsigned int crc;              // CRC
    unsigned char data[64];        // Serial number encrypted data
    unsigned char uuid_check[32];  // Anti-copy check
} AjSnHeaderV1;

// New save format 256-byte structure - V2
typedef struct {
    unsigned int magic1;
    unsigned int magic2;
    unsigned int magic3;
    unsigned int magic4;
    unsigned int len;              // Max 192
    unsigned int crc;              // CRC
    unsigned char data[192];       // Serial number encrypted data
    unsigned char uuid_check[32];  // Anti-copy check
    unsigned char reserved[24];    // Reserved
} AjSnHeaderV2;

#endif 
