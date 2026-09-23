#ifndef __ANJ_SYSMNG_H__
#define __ANJ_SYSMNG_H__

#include "anj_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define P2P_ID_NETCONFIGED_AIOT "/mnt/nand/aiot_netconfiged.db"    // 设备配网标记
#define P2P_ID_NETCONFIGED_ALI "/mnt/nand/skyworth_netconfiged.db" // 设备配网标记
#define PASSWORD_ENCODE_FLAG "/opt/ch/flag.set.passwd" // 设备密码加密标记

#define SN_RANDOM_FLAG          "/tmp/flag.anjvision.flag"


#define VIDEO_RES_CUST_FILE     "/opt/ch/cust_video_res.txt"
#define VIDEO_RES_DELETE_FILE   "/opt/ch/delete_video_res.txt"


#define ANJ_DEFAULT_PARTER "anjvision"

#define AJ_MAX_PRODUCT_TYPE_LEN (16)

typedef struct
{
    char kernelVersion[256];
    char fsVersion[256];
} SYSTEM_VERSION_DATA;

typedef struct
{
    char filePath[256];
    char origName[256];
    unsigned int nPhyAddr;
    unsigned int nFileLen;
} APPBIN_UPDATE_DATA;

typedef enum
{
    CONFIG_RESTORE_RESERVE_BIT_NETWORK = 0,
    CONFIG_RESTORE_RESERVE_BIT_LANGUAGE = 1,
    CONFIG_RESTORE_RESERVE_BIT_TIME = 2,
    CONFIG_RESTORE_RESERVE_BIT_USER = 3,
    CONFIG_RESTORE_RESERVE_BIT_MEDIACODE = 4,
    CONFIG_RESTORE_RESERVE_BIT_PTZ = 5,
    CONFIG_RESTORE_RESERVE_BIT_STREAMACCESS = 6,
    CONFIG_RESTORE_RESERVE_BIT_RECORD = 7,
    CONFIG_RESTORE_RESERVE_BIT_GB28181 = 8,
    CONFIG_RESTORE_RESERVE_BIT_ALARM = 9,
    CONFIG_RESTORE_RESERVE_BIT_TITLE = 10,
} ConfigBitEnum;

typedef struct tag_encode_resolution
{
    char res_name[16];         // 分辨率。更改分辨率选择时，其他值需要根据本结构相应更改
    char codec_name[16];       // 编码格式，H.264 H.265 MJPEG等
    int stream_type;           // 0: main, 1: sub, 2: third
    int def_bitrate;           // kbps
    int min_bitrate;           // kbps
    int max_bitrate;           // kbps
    int def_framerate;         // 默认帧率
    int min_framerate;         // 最小帧率
    int max_framerate;         // 实际最大帧率(实际生效的帧率可选值只能是这2者之间)
    int dual_stream;           // if stream_type == 0, dual_stream == 0, means disable sub stream
    int def_config;            // if def_config = 1, use for default config of video encode
    int max_display_framerate; // 显示最大帧率(帧率可选值只能是这2者之间)
} RESOLUTION_ENTRY;

typedef struct tag_audio_mode
{
    char codec_name[16]; // 编码方式
    int channels;        // 通道数
    int bitspersample;   // 比特率
    int samplerate;      // 采样率
    int bitrate;         // 比特率
    int def_config;      // 是否默认配置
} AUDIO_CODEC_ENTRY;

typedef struct tag_YUV_ENTRY_W
{
    char res_name[16]; // 分辨率。
    int format;        // 固定为0，目前只支持YUV420SP(NV12)。
    int def_framerate;
    int min_framerate;
    int max_framerate;
    int def_config; // if def_config = 1, use for default config of video encode
} YUV_ENTRY;

typedef struct
{
    char sn[128];       // 序列号:未授权时使用随机序列;授权后使用授权序列号
    int activated;
    int bFactoryMode;
    int bFactoryReload;
    int bUpgrading;
    char devType[AJ_MAX_PRODUCT_TYPE_LEN];
    char platformType[AJ_MAX_PRODUCT_TYPE_LEN];
    char subDevType[AJ_MAX_PRODUCT_TYPE_LEN];
    char productVersion[16];
    char search_devicetype[64];
    char oem_sn[64];
    char custom_name[32];
    SYSTEM_VERSION_DATA stVersionInfo;
    char version_name[32];
    char release_date[64];
    char uuid[128];
    char mcu_version[128];
} DevInfo;

int anj_sysmng_sn_validate(unsigned char *sn);
int anj_sysmng_load_sn(char *sn_str, int str_len);
int anj_sysmng_load_enc_sn(char *sn_str, int str_len);

int anj_sysmng_get_sn(unsigned char *buf, int buflen);
int anj_sysmng_sn_check();

int anj_sysmng_check_process(char *process_name);

void anj_sysmng_reboot();
void anj_sysmng_delay_reboot(int sec);

int anj_sysmng_version_info_get(SYSTEM_VERSION_DATA *pVersionInfo, int bGetRealVersion);
int anj_sysmng_parse_fsversion(char *fsver, char *deviceType, int deviceTypeLen,
                               char *version, int versionLen, char *date, int dateLen);
char *anj_sysmng_product_version_get();
int anj_sysmng_dev_str_get(char *szDeviceType);
int anj_sysmng_platform_type_get(char *szPlatformType, int bufLen);

int anj_sysmng_app_update(APPBIN_UPDATE_DATA *updateData);
int anj_sysmng_config_update(char *filePath);

int anj_sysmng_get_totalmem(void);

int anj_sysmng_get_availablemem(void);

int anj_sysmng_get_freemem(void);

const char *anj_sysmng_cpu_info_update(void);

int anj_sysmng_is_limit_ip(unsigned int remoteip);

void anj_sysmng_videolist_get(char *retBuf, int size);
void anj_sysmng_audiolist_get(char *retBuf, int size);
void anj_sysmng_yuvlist_get(char *retBuf, int size);
void anj_sysmng_max_res_get(int chn, int *pWidth, int *pHeight);
int anj_sysmng_video_res_array_get(RESOLUTION_ENTRY **pEntry);
int anj_sysmng_audio_res_array_get(AUDIO_CODEC_ENTRY **pEntry);

int anj_sysmng_config_restore(unsigned int reserved_bits);
void anj_sysmng_delay_restore(int sec);
void anj_sysmng_factory_config_restore(GlobalConfig *pstGlobalConfig);
void anj_sysmng_cust_language_restore(GlobalConfig *pstGlobalConfig);
void anj_sysmng_mac_restore(GlobalConfig *cfg);
int anj_sysmng_second_config_copy(const char *srcFile);
void anj_sysmng_second_config_apply(GlobalConfig *cfg);
void anj_sysmng_second_config_led_apply(void);
void anj_sysmng_second_config_capability_apply(void);

void anj_sysmng_restore_netconfig_set(int flag);

int anj_sysmng_eraseall_mp3();

void anj_sysmng_partner_info_get(char *szPartner, char *szDatestr, char *szMacaddr);
int anj_sysmng_partner_info_set(const char *partner, const char *datestr, const char *macaddr);

void anj_sysmng_viewer_add();
void anj_sysmng_viewer_del();
int anj_sysmng_viewer_get();

DevInfo *getDevInfo(void);

#ifdef __cplusplus
}
#endif

#endif
