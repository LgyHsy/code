#ifndef _ANJ_CONFIG_SYSTEM_H_
#define _ANJ_CONFIG_SYSTEM_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    CONFIG_LANGUAGE_CN = 0,
    CONFIG_LANGUAGE_HK,
    CONFIG_LANGUAGE_TW,
    CONFIG_LANGUAGE_EN,
    CONFIG_LANGUAGE_RUSSION
} CONFIG_LANGUAGE;

#define MAX_IP_NAME_LEN 64

#define GROUP_NAME_MAX_LEN 32
typedef struct
{
    char groupName[GROUP_NAME_MAX_LEN];
} Group;

#define ACCOUNT_STATUS_MAX_LEN 8
#define ACCOUNT_NAME_MAX_LEN 40
#define ACCOUNT_PASSWORD_MAX_LEN 40

typedef struct
{
    char userName[ACCOUNT_NAME_MAX_LEN];
    char password[ACCOUNT_PASSWORD_MAX_LEN];
    Group group;
    char status[ACCOUNT_STATUS_MAX_LEN];
} UserAccount;

#define MAX_ACCOUNT_COUNT 10

typedef struct
{
    int count;
    UserAccount accounts[MAX_ACCOUNT_COUNT];
    int secureLoginMode; // 0普通登录 1安全登录
} UserConfig;

typedef struct
{
    char serverIP[MAX_IP_NAME_LEN];
    unsigned short serverPort;
    unsigned int refreshInterval;
} NTPConfig;

typedef struct DstConfig_
{
    char szDstName[64];
    char szDstCfg[64];
    int nTimeZone;

    int nOffsetMin; // 偏移时间(分钟)
    unsigned char nStartMonth;
    unsigned char nStartWeek;    // 该月第几周
    unsigned char nStartWeekday; // 周几
    unsigned char nStartHour;    // 小时
    unsigned char nToMonth;
    unsigned char nToWeek;    // 该月第几周
    unsigned char nToWeekday; // 周几
    unsigned char nToHour;    // 小时

    struct DstConfig_ *next;
} DstConfig;

#define TIME_MODE_MAX_LEN 32
#define TIME_MODE_NAME_NTP "NTP"
#define TIME_MODE_NAME_MANUAL "MANUAL"
#define TIME_MODE_NAME_P2P "P2P"

typedef struct
{
    char modeName[TIME_MODE_MAX_LEN];
} TimeMode;

typedef struct
{
    unsigned char nEnable; // 启用标记
    unsigned char bAuto;   // 根据时区自动配置
    short nOffsetMin;      // 偏移时间(分钟)
    unsigned char nStartMonth;
    unsigned char nStartWeek;    // 该月第几周
    unsigned char nStartWeekday; // 周几
    unsigned char nStartHour;    // 小时
    unsigned char nToMonth;
    unsigned char nToWeek;    // 该月第几周
    unsigned char nToWeekday; // 周几
    unsigned char nToHour;    // 小时
} SummerTimeConfig;

typedef struct
{
    TimeMode timeMode;
    int timeZone;
    NTPConfig ntpConfig;
    SummerTimeConfig summerConfig;
} TimeConfig;

#define PTZ_PROTOCOL_NAME_MAX_LEN 32
typedef struct
{
    char protocolName[PTZ_PROTOCOL_NAME_MAX_LEN];
} PTZProtocol;

#define VERIFY_NAME_MAX_LEN 32
typedef struct
{
    char verifyName[VERIFY_NAME_MAX_LEN];
} Verify;

#define FLOW_CONTROL_MAX_LEN 32
typedef struct
{
    char flowControlName[FLOW_CONTROL_MAX_LEN];
} FlowControl;

#define PTZ_FUNCTION_TYPE_MAX_LEN 28

typedef struct
{
    char typeName[PTZ_FUNCTION_TYPE_MAX_LEN];
} PTZFunctionType;

#define PTZ_FUNCTION_NAME_MAX_LEN 32

typedef struct
{
    char functionName[PTZ_FUNCTION_NAME_MAX_LEN];
    int presetNum;
    PTZFunctionType functionType;
    int reserveValue;
    int presetNum2;
    PTZFunctionType functionType2;
    int interval_sec;
} PTZFunction;

#define MAX_PTZFUCTION_COUNT 64
typedef struct
{
    int functionCnt;
    PTZFunction functions[MAX_PTZFUCTION_COUNT];
} PTZAdvanceConfig;

typedef struct
{
    PTZProtocol ptzProtocol;
    unsigned int comPort;
    unsigned int baudrate;
    unsigned int dataBits;
    unsigned int stopBits;
    Verify verify;
    FlowControl flowControl;
    unsigned int bootAction;
} PTZCommonConfig;

typedef struct
{
    unsigned int cruiseSpeed;
    unsigned int cruiseTime;
    unsigned int lineScanTime;
} PTZScanConfig;

typedef struct
{
    int enable;
    int type;              // AF类型: 0: BSD 1: ZUO
    short bSendAFAlways;   // 始终发AF数据
    short bSendCoordinate; // 发送坐标
} AfConfig;

typedef struct
{
    double multiple_max; // 支持最大倍数，用于CLIENT显示列表，不允许CLIENT更改
    double multiple_set; // 设置最大倍数
} DZoomConfig;

typedef struct
{
    PTZCommonConfig commonCfg;
    PTZAdvanceConfig advanceCfg;
    AfConfig afCfg;
    DZoomConfig dzoomCfg;
    PTZScanConfig scanConfig;
} PTZConfig;

#define LOG_LEVEL_NAME_MAX_LEN 200
typedef struct
{
    char levelName[LOG_LEVEL_NAME_MAX_LEN];
} LogLevel;

#define STORE_MEDIA_NAME_MAX_LEN 32

typedef struct
{
    char mediaName[STORE_MEDIA_NAME_MAX_LEN];
} StoreMedia;

#define STORE_POLICY_NAME_MAX_LEN 32

typedef struct
{
    char policyName[STORE_POLICY_NAME_MAX_LEN];
} StorePolicy;

#define BACKUP_WAY_NAME_MAX_LEN 32

typedef struct
{
    char wayName[BACKUP_WAY_NAME_MAX_LEN];
} BackupWay;

#define SYSLOG_FILE_NAME_MAX_LEN 32

typedef struct
{
    LogLevel logLevel;
    unsigned int maxDays;
    // unsigned int   		maxEventPerday;
    StoreMedia storeMedia;
    StorePolicy storePolicy;
    int autoBackup; // 1  for auto  0 for manual
    BackupWay backupWay;
} SyslogConfig;

#define MAX_LANGUAGE_LEN 32

typedef struct
{
    char language[MAX_LANGUAGE_LEN];
} MiscConfig;

typedef struct
{
    int enable;
    int day; // 日期： 0-6 = 星期一到星期天， 7=每天
    DayTime time;
} MaintainConfig;

#define MAX_ALOW_IP_NUM 5
typedef struct
{
    int enable;
    unsigned int nAllowIp[MAX_ALOW_IP_NUM];
} SysAlowIpConfig;

#define MAX_ALARM_CLOCK_NUM 5
typedef struct
{
    unsigned char enable;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
} AlarmClock; // 闹钟设置

typedef struct
{
    unsigned char enable;
    unsigned char from_hour;
    unsigned char from_minute;
    unsigned char from_second;
    unsigned char to_hour;
    unsigned char to_minute;
    unsigned char to_second;
    unsigned char reserved;
} AlarmOClock; // 整点报时

typedef struct
{
    AlarmOClock oclock;
    AlarmClock alarmclock[MAX_ALARM_CLOCK_NUM];
} AlarmClockConfig;

typedef struct
{
    unsigned char adjust_bitrate;
    unsigned char adjust_fps;
    unsigned char reserved1[2];

    unsigned char enable_by_network;        // wifi/4g or network condition
    unsigned char enable_by_sdcard;         // sdcard inserted
    unsigned char enable_by_cloud;          // cloud enabled
    unsigned char enable_by_sdcardandcloud; // sdcard inserted and cloud enabled
    unsigned char enable_by_cloudstorage;   // cloud storage activated
    unsigned char reserved2[7];
} VideoQoSConfig;

typedef struct
{
    unsigned char startup; // 开机提示
    unsigned char ota;     // 升级提示
    unsigned char network; // 联网提示
    unsigned char sdcard;  // TF卡状态提示
    unsigned char reset;   // 复位提示音
    unsigned char reserved1[3];
} AudioPromptConfig;

typedef struct
{
    unsigned char mac;
    unsigned char network;
    unsigned char dhcpOnReboot; // 始终保持以DHCP启动
    unsigned char reserved[29];
} TamperProofConfig; // 防篡改配置。为1时不允许改动相关配置

typedef struct
{
    int push_reset;
} LocationConfig;

typedef struct
{
    int mode; // 0:auto, 1:timing, 2:Manual
    int manualEnable;
    int openSensitivity;
    int closeSensitivity;
    DayTimeSpan nightStartTime;
    DayTimeSpan nightEndTime;
} StreetLampConfig;

typedef struct
{
    PTZConfig ptzCfg;
    TimeConfig timeCfg;
    UserConfig userCfg;
    SyslogConfig syslogCfg;
    MiscConfig miscCfg;
    MaintainConfig maintainCfg;
    SysAlowIpConfig alowipCfg;
    AlarmClockConfig clockSetting;    // 闹钟设置
    VideoQoSConfig videoQosCfg;       // 视频服务质量配置
    AudioPromptConfig audioPromptCfg; // 语音提示配置
    TamperProofConfig tamperProofCfg; // 防篡改配置。为1时不允许改动相关配置
    LocationConfig locationCfg;
    StreetLampConfig streetLampCfg;
} SystemConfig;

int anj_config_system_get(IXML_Node *pNode, SystemConfig *pSystemCfg);
int anj_config_system_save(SystemConfig *pSystemCfg);
int anj_config_system_set(SystemConfig *pSystemCfg);

int anj_config_system_ptz_comm_set(PTZCommonConfig *pPtzCommCfg);
int anj_config_system_ptz_advance_set(PTZAdvanceConfig *pPtzAdvanceCfg);
int anj_config_system_ptz_af_set(AfConfig *pAfCfg);
int anj_config_system_ptz_dzoom_set(DZoomConfig *pDZoomCfg);
int anj_config_system_ptz_scan_set(PTZScanConfig *pPtzScanCfg);

int anj_config_system_ptz_set(PTZConfig *pPtzCfg);

int anj_config_system_time_set(TimeConfig *pTimeCfg);

int anj_config_system_user_set(UserConfig *pUserCfg);

int anj_config_system_syslog_set(SyslogConfig *pSyslogCfg);

int anj_config_system_misc_set(MiscConfig *pMiscCfg);

int anj_config_system_maintain_set(MaintainConfig *pMaintianCfg);

int anj_config_system_allowip_set(SysAlowIpConfig *pAllowIpCfg);

int anj_config_system_alarmclock_set(AlarmClockConfig *pAlarmClockCfg);

void anj_config_system_videoq_default(VideoQoSConfig *pVideoqCfg);

int anj_config_system_videoq_set(VideoQoSConfig *pVideoqCfg);

int anj_config_system_audioprompt_set(AudioPromptConfig *pAudioPromptCfg);

int anj_config_system_tamperproof_set(TamperProofConfig *pTamperProofCfg);

int anj_config_system_location_set(LocationConfig *pLocationCfg);

int anj_config_system_streetlamp_set(StreetLampConfig *pStreetLampCfg);

char *anj_config_system_ptz_af_conver_xml(AfConfig *pPtzCfg);
char *anj_config_system_ptz_dzoom_conver_xml(DZoomConfig *pPtzCfg);
char *anj_config_system_ptz_scan_conver_xml(PTZScanConfig *pPtzScanCfg);
char *anj_config_system_ptz_advance_conver_xml(PTZAdvanceConfig *pPtzConfig);
char *anj_config_alarm_clock_conver_xml(AlarmOClock *pCfg);
char *anj_config_alarm_clock_list_conver_xml(AlarmClock *pCfgList);
char *anj_config_system_ptz_conver_xml(PTZConfig *pPtzConfig);
char *anj_config_system_time_conver_xml(TimeConfig *pTimeCfg);
char *anj_config_system_user_password_conver_xml(UserConfig *pUserCfg);
char *anj_config_system_user_conver_xml(UserConfig *pUserCfg, int bPwdEntrypt);
char *anj_config_system_syslog_conver_xml(SyslogConfig *pSyslogCfg);
char *anj_config_system_misc_conver_xml(MiscConfig *pMiscCfg);
char *anj_config_system_maintain_conver_xml(MaintainConfig *pMaintainCfg);
char *anj_config_system_allowip_conver_xml(SysAlowIpConfig *pCfg);
char *anj_config_system_alarmclock_conver_xml(AlarmClockConfig *pCfg);
char *anj_config_system_videoq_conver_xml(VideoQoSConfig *pCfg);
char *anj_config_system_audioprompt_conver_xml(AudioPromptConfig *pCfg);
char *anj_config_system_tamperproof_conver_xml(TamperProofConfig *pCfg);
char *anj_config_system_location_conver_xml(LocationConfig *pCfg);
char *anj_config_system_streetlamp_conver_xml(StreetLampConfig *pCfg);
char *anj_config_system_conver_xml(SystemConfig *pSystemCfg);

int anj_config_system_alarmclock_get_by_xml(AlarmClockConfig *pCfg, char *xmlBuf);
int anj_config_system_audioprompt_get_by_xml(AudioPromptConfig *pCfg, char *xmlBuf);
int anj_config_system_syslog_get_by_xml(SyslogConfig *pSyslogCfg, char *xmlBuf);
int anj_config_system_maintain_get_by_xml(MaintainConfig *pMaintainCfg, char *xmlBuf);
int anj_config_system_allowip_get_by_xml(SysAlowIpConfig *pAllowIpCfg, char *xmlBuf);
int anj_config_system_misc_get_by_xml(MiscConfig *pMiscCfg, char *xmlBuf);
int anj_config_system_ptz_get_by_xml(PTZConfig *pPtzCfg, char *xmlBuf);
int anj_config_system_tamperproof_get_by_xml(TamperProofConfig *pCfg, char *xmlBuf);
int anj_config_system_location_get_by_xml(LocationConfig *pCfg, char *xmlBuf);
int anj_config_system_streetlamp_get_by_xml(StreetLampConfig *pCfg, char *xmlBuf);
int anj_config_system_time_get_by_xml(TimeConfig *pTimeCfg, char *xmlBuf);
int anj_config_system_user_get_by_xml(UserConfig *pUserCfg, char *xmlBuf);
int anj_config_system_videoq_get_by_xml(VideoQoSConfig *pCfg, char *xmlBuf);
int anj_config_system_get_by_xml(SystemConfig *pSystemCfg, char *xmlBuf);
int anj_config_system_ptz_advance_get_by_xml(PTZAdvanceConfig *pPtzAdvanceCfg, char *xmlBuf);
int anj_config_system_ptz_dzoom_get_by_xml(DZoomConfig *pCfg, char *xmlBuf);
int anj_config_system_ptz_common_get_by_xml(PTZCommonConfig *pPtzCommonCfg, char *xmlBuf);
int anj_config_system_ptz_af_get_by_xml(AfConfig *pCfg, char *xmlBuf);
int anj_config_system_ptz_scan_get_by_xml(PTZScanConfig *pCfg, char *xmlBuf);

#ifdef __cplusplus
}
#endif

#endif
