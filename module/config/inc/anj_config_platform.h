#ifndef _ANJ_CONFIG_PLATFORM_H_
#define _ANJ_CONFIG_PLATFORM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_IP_NAME_LEN 64
#define MAX_IPC_FILENAME_LEN 128

#define ACCOUNT_STATUS_MAX_LEN 8
#define ACCOUNT_NAME_MAX_LEN 40
#define ACCOUNT_PASSWORD_MAX_LEN 40

typedef struct
{
    int enable;
    char server[MAX_IP_NAME_LEN];
    unsigned short port;
    char username[ACCOUNT_NAME_MAX_LEN];
    char password[ACCOUNT_PASSWORD_MAX_LEN];
} VmPlatformConfig;

typedef struct
{
    unsigned char PlayTone;         // 启动播放声音
    unsigned char InRingTimes;      // 来电振铃次数后自动接通
    unsigned char reserved;         /// 韩国门铃，默认开启广播信息
    unsigned short keyPressTimeLen; // 触发呼叫的按键时长(毫秒, 100ms-5000ms)，避免误按
    char filename[MAX_IPC_FILENAME_LEN];
} VoipConfig;

typedef struct
{
    VmPlatformConfig vmCfg;
    VoipConfig voipCfg;
} PlatformConfig;

#define PLATFORM_REGISGER_RESULT_FILE "/tmp/flag_plat_reg_result"
typedef enum
{
    PLAT_REG_RESULT_REG_NO = 0,
    PLAT_REG_RESULT_REGGING,
    PLAT_REG_RESULT_REG_FAILED,
    PLAT_REG_RESULT_REG_OK,
} PlatRegStatus;
typedef struct
{
    PlatRegStatus result; // 0: 未注册 1: 注册中 2: 注册失败 3: 注册成功
    char szPlatDevId[32];
    char reservedInfo[128];
} PlatRegResult;


/* 读取 / 解析 **************************************************************/
int anj_config_platform_get(IXML_Node *pNode, PlatformConfig *pPlatformCfg);
int anj_config_platform_get_by_xml(PlatformConfig *pPlatformCfg, char *xmlBuf);

char *anj_config_platform_conver_xml(PlatformConfig * pPlatformCfg, int bPwdEntrypt);

/* 保存 / 设置 **************************************************************/
int anj_config_platform_save(PlatformConfig *pPlatformCfg);
int anj_config_platform_set(PlatformConfig *pPlatformCfg);

/* 加载 ********************************************************************/
int anj_config_platform_load(PlatformConfig *pPlatformCfg);

#ifdef __cplusplus
}
#endif

#endif
