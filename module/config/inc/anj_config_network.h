#ifndef _ANJ_CONFIG_NETWORK_H_
#define _ANJ_CONFIG_NETWORK_H_

#include "anj_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ACCOUNT_STATUS_MAX_LEN 8
#define ACCOUNT_NAME_MAX_LEN 40
#define ACCOUNT_PASSWORD_MAX_LEN 40

#define UNICOM_STR_MAX_LEN 64
typedef struct
{
    char url[UNICOM_STR_MAX_LEN];           // 密服平台地址
    char machineId[UNICOM_STR_MAX_LEN];     // 客户端国标id
    char machineSalt[UNICOM_STR_MAX_LEN];   // 设备设置的盐值
    char productID[UNICOM_STR_MAX_LEN];     // 单点登录的应用Idc
    char productSecret[UNICOM_STR_MAX_LEN]; // 单点登录的应用凭证
    int vkekInterval;                       // vkek的更新周期
    char authPasswd[UNICOM_STR_MAX_LEN];    // 认证密码
} EncryptionConfig;
typedef enum RLS_TYPE
{
    RLS_TYPE_CERTREQ = 0x01,    // 证书请求
    RLS_TYPE_CERTIMPORT = 0x02, // 证书及加密密钥对导入
    RLS_TYPE_PLATCERT = 0x03,   // 平台证书处理
} RLS_TYPE;
typedef struct
{
    int certMode;                      // 0:标准模式 1:GA模式
    char devId[UNICOM_STR_MAX_LEN];    // 设备ID
    int devCertIsAuth;                 // 设备证书是否已认证
    int p10CertType;                   // p10导出证书类型 0:签名证书 1:加密证书
    int devCertType;                   // 导入设备证书类型
    int secretKeyType;                 // 秘钥格式 1:国密09 2:国密0016 3:标准P7B 默认是1
    int platCerType;                   // 平台证书类型 0:签名证书 1:加密证书
    int inOrOutType;                   // 导入还是导出
    char filePath[UNICOM_STR_MAX_LEN]; // 文件路径
} Gb35114CertConfig;
typedef struct
{
    char serialNo[32];     // 终端 SN 号,需和外壳条码一致
    char devFactory[32];   // 设备厂商编号，需和在平台登记的一致
    char devType[32];      // 设备型号，需和在平台登记的一致
    char serverIp[32];     // 亿迅平台服务器的IP地址
    char serverPort[8];    // 亿迅平台服务器的端口号
    char ethMac[32];       // 联网网卡的 mac地址
    char firmwareVer[128]; // 设备固件的当前版本号
    char netAdsl[32];      // 宽带账号
    char encryKey[32];     // 通信密钥
    char keyIV[32];        // 通信密钥IV
} TelecomDevParamInfo;
typedef struct VSEC_EXTSDK_MODINFO_T
{
    char vender[128]; // 密码模块厂商信息,有可能是乱码
    char modSN[64];   // 密码模块ID
    char res[32];
} VSEC_EXTSDK_MODINFO_T;
typedef struct
{
    int digestAlg;
    int rand;
    int fileIo;
    int asymmetryAlg;
    int symmetryAlg;
    int devStatus;
    int isCert;
    int sdkVersion;
    char sdkSn[64];
    char chipVersion[20];
} VsecDevInfo;
typedef struct VSEC_UKEY_FIRST_
{
    char ukeyId[32];         // ukeyID
    char ukeyCertData[1024]; // ukey文件
} VSEC_UKEY_FIRST;
typedef struct VSEC_UKEY_SECOND_
{
    char devId[32];         // 设备ID
    char devCertData[1024]; // 设备证书文件
    char devR1Num[64];      // 设备R1随机数
} VSEC_UKEY_SECOND;
typedef struct VSEC_UKEY_THIRD_
{
    char webR2Num[64];  // webR2随机数
    char webSign1[256]; // web 计算R1+R2+devId产生的Sign1
} VSEC_UKEY_THIRD;
typedef struct VSEC_UKEY_FOUR_
{
    char devSign2[256]; // R1+R2+UKEYID+CryptKey产生的Sign2
    char cryptKey[256];
} VSEC_UKEY_FOUR;
typedef struct VSEC_UKEY_INFO_
{
    int ret;                      // 返回值
    int curStepNum;               // 当前步数
    VSEC_UKEY_FIRST first_step;   // 第一步
    VSEC_UKEY_SECOND second_step; // 第二步
    VSEC_UKEY_THIRD third_step;   // 第三步
    VSEC_UKEY_FOUR four_step;     // 第四步
} VSEC_UKEY_INFO;

/*
typedef struct
{
    char  IPAddress[MAX_IP_NAME_LEN];
    char  netMask[MAX_IP_NAME_LEN];
    char  gateWay[MAX_IP_NAME_LEN];
    char  DNS1[MAX_IP_NAME_LEN];
    char  DNS2[MAX_IP_NAME_LEN];
}StaticIPConfig;

typedef struct
{
    char reserved[16];
}DHCPConfig;

typedef struct
{
    int enable;
    int interval;
}ADSLAutoConnect;

typedef struct
{
    char IPAddress[MAX_IP_NAME_LEN];
    char netMask[MAX_IP_NAME_LEN];
    char gateWay[MAX_IP_NAME_LEN];
}ADSLLanConfig;

#define ADSL_NAME_MAX_LEN 32
#define ADSL_PASSWORD_MAX_LEN 32

typedef struct
{
    char userName[ADSL_NAME_MAX_LEN];
    char password[ADSL_PASSWORD_MAX_LEN];
    ADSLAutoConnect autoConnect;
    ADSLLanConfig   adslLanCfg;
}ADSLConfig;

*/

#define MAX_WEPENCRYPT_AUTHMODE_NAME_LEN 64
#define MAX_WEPENCRYPT_ENCRYPTTYPE_NAME_LEN 64
#define MAX_WEPENCRYPT_KEYMODE_NAME_LEN 64
#define MAX_WEPENCRYPT_KEYVALUE_LEN 64

#define WEPENCRYPT_AUTHMODE_NAME_OPENSYSTEM "open system"
#define WEPENCRYPT_AUTHMODE_NAME_SHAREDKEY "shared key"

#define WEPENCRYPT_AUTHMODE_VALUE_OPENSYSTEM 0
#define WEPENCRYPT_AUTHMODE_VALUE_SHAREDKEY 1
#define WEPENCRYPT_AUTHMODE_VALUE_UNKNOWN -1

typedef struct
{
    char authMode[MAX_WEPENCRYPT_AUTHMODE_NAME_LEN];
    char encryptType[MAX_WEPENCRYPT_ENCRYPTTYPE_NAME_LEN];
    char keyMode[MAX_WEPENCRYPT_KEYMODE_NAME_LEN];
    char keyValue[MAX_WEPENCRYPT_KEYVALUE_LEN];
    int keyIndex;
} WepEncrypt;

#define MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN 64
#define MAX_WPAENCRYPT_AUTHMODE_NAME_LEN 64
#define MAX_WPAENCRYPT_KEYVALUE_LEN 64

#define WPAENCRYPT_ENCRYPTTYPE_NAME_TKIP "tkip"
#define WPAENCRYPT_ENCRYPTTYPE_NAME_AES "aes"

typedef struct
{
    char encryptType[MAX_WPAENCRYPT_ENCRYPTTYPE_NAME_LEN];
    char authMode[MAX_WPAENCRYPT_AUTHMODE_NAME_LEN];
    char keyValue[MAX_WPAENCRYPT_KEYVALUE_LEN];
} WpaEncrypt;

#define MAX_WIRELESSENCRYPT_ENCRYPTTYPE_NAME_LEN 64
#define WIRELESSENCRYPT_ENCRYPTTYPE_NAME_WEP "wep"
#define WIRELESSENCRYPT_ENCRYPTTYPE_NAME_WPA "wpa"

typedef struct
{
    int enable;
    char encryptType[MAX_WIRELESSENCRYPT_ENCRYPTTYPE_NAME_LEN];
    WepEncrypt wepEncrypt;
    WpaEncrypt wpaEncrypt;
} WirelessEncrypt;

#define MAC_ADDRESS_LEN 18 // 00:11:11:11:11:11
#define MAX_WIRELESS_OPERATIONMODE_NAME_LEN 32
#define MAX_WIRELESS_MACMODE_NAME_LEN 64
#define MAX_WIRELESS_ESSID_NAME_LEN 64
#define MAX_WIRELESS_REGION_NAME_LEN 64
#define MAX_WIRELESS_BITRATE_NAME_LEN 10
#define MAX_WIFI_VERSION_LEN 10

#define WIRELESS_OPERATIONMODE_MASTER_NAME "master"
#define WIRELESS_OPERATIONMODE_MANAGED_NAME "managed"

#define WIRELESS_MACMODE_NAME_A "A"
#define WIRELESS_MACMODE_NAME_B "B"
#define WIRELESS_MACMODE_NAME_G "G"
#define WIRELESS_MACMODE_NAME_MIXED "MIXED"

#define WIRELESS_MACMODE_VALUE_A 4
#define WIRELESS_MACMODE_VALUE_B 3
#define WIRELESS_MACMODE_VALUE_G 2
#define WIRELESS_MACMODE_VALUE_MIXED 1
#define WIRELESS_MACMODE_VALUE_UNKNOWN -1

#define WIRELESS_REGION_NAME_TAIWAN "TAIWAN"
#define WIRELESS_REGION_NAME_USA "USA"
#define WIRELESS_REGION_NAME_FRANCE "FRANCE"
#define WIRELESS_REGION_NAME_ISRAEL "ISRAEL"

#define WIRELESS_REGION_VALUE_TAIWAN 2
#define WIRELESS_REGION_VALUE_USA 1
#define WIRELESS_REGION_VALUE_FRANCE 3
#define WIRELESS_REGION_VALUE_ISRAEL 5

#define WIRELESS_REGION_VALUE_UNKNOWN -1

/*
typedef struct
{
    int enable;
    char operationMode[MAX_WIRELESS_OPERATIONMODE_NAME_LEN];
    char macMode[MAX_WIRELESS_MACMODE_NAME_LEN];
    char bitRate[MAX_WIRELESS_BITRATE_NAME_LEN];
    char essid[MAX_WIRELESS_ESSID_NAME_LEN];
    char region[MAX_WIRELESS_REGION_NAME_LEN];
    int channelNum;
    char MACAddress[MAC_ADDRESS_LEN];
    WirelessEncrypt wirelessEncrypt;
}WirelessConfig;

typedef struct
{
    unsigned char MACAddress[MAC_ADDRESS_LEN];
}WireConfig;
*/

#define MAX_DDNS_SERVER_NAME_LEN 256
#define MAX_DDNS_DOMAIN_NAME_LEN 256
#define MAX_DDNS_USERNAME_LEN 256
#define MAX_DDNS_PASSWORD_LEN 256
typedef struct
{
    int enable;
    char server[MAX_DDNS_SERVER_NAME_LEN];
    char domain[MAX_DDNS_DOMAIN_NAME_LEN];
    char userName[MAX_DDNS_USERNAME_LEN];
    char password[MAX_DDNS_PASSWORD_LEN];
    int freshInterval;
} DDNSConfig;

typedef struct
{
    int enable;
} UPNPConfig;

typedef enum
{
    P2P_TYPE_NOTDEFINED = 0,
    P2P_TYPE_DANALE = 1,
    P2P_TYPE_ANKO = 2,
    P2P_TYPE_GOOLINK = 3,
    P2P_TYPE_YUECAM = 4,
    P2P_TYPE_QQCONNECT = 5,
    P2P_TYPE_TUTK = 6,
    P2P_TYPE_EYEPLUS = 7,
    P2P_TYPE_SKYWORTH = 8,
    P2P_TYPE_TUYA = 9,
    P2P_TYPE_AC18PRO = 10,
    P2P_TYPE_TENCENT = 11,
    P2P_TYPE_HAIER = 12,
    P2P_TYPE_DOT = 13,
    P2P_TYPE_AIOT = 14,
    P2P_TYPE_AGORA = 15
} P2pType;

#define P2P_AUTH_CODE_LEN 16
typedef struct
{
    int enable;
    int p2ptype; //
    char authcode[P2P_AUTH_CODE_LEN];
} P2PConfig;

#define ADSL_NAME_MAX_LEN 32
#define ADSL_PASSWORD_MAX_LEN 32

typedef struct
{
    int enable;
    char userName[ADSL_NAME_MAX_LEN];
    char password[ADSL_PASSWORD_MAX_LEN];
} ADSLConfigNew;

typedef struct
{
    unsigned char MACAddress[MAC_ADDRESS_LEN];
    char dhcpEnable;
    char dhcpOffTime;        // DHCP获取IP后，多久变成固定IP:0-48。小于0表示DHCP一直生效，不会拿到IP后变成固定IP
    short onvifAllnetEnable; // 是否启用ONVIF全网通
    char IPAddress[MAX_IP_NAME_LEN];
    char netMask[MAX_IP_NAME_LEN];
    char gateWay[MAX_IP_NAME_LEN];
    char DNS1[MAX_IP_NAME_LEN];
    char DNS2[MAX_IP_NAME_LEN];
    char hostname[MAX_IP_NAME_LEN]; // hostname
    unsigned int mtu;
} LANConfig;

#define MAX_WIFI_AP_CNT (50)

typedef struct
{
    char ssid[128];
    char workMode[64];
    char wirelessMode[64];
    char mac[20];
    int authMode;
    int encryType;
    int quality;
    int maxquality;
    int signalLevel;
    int noiseLevel;
    int channel;

    float cfo;
    float txevm;
    float rxevm;
    int dcxo;
    float txrssi;
    float rxrssi;
    int result;
} WIFI_AP_INFO;

typedef struct
{
    int apCnt;
    WIFI_AP_INFO apInfos[MAX_WIFI_AP_CNT];
} WIFI_AP_SCAN;

typedef struct
{
    int enable;
    char address[MAX_IP_NAME_LEN];
    int interval;
    int maxFail;
} WIFIPingWatchConfig;

typedef struct
{
    int enable;
    char version[MAX_WIFI_VERSION_LEN];
    int dhcpEnable;
    char IPAddress[MAX_IP_NAME_LEN];
    char netMask[MAX_IP_NAME_LEN];
    char gateWay[MAX_IP_NAME_LEN];
    char operationMode[MAX_WIRELESS_OPERATIONMODE_NAME_LEN];
    char macMode[MAX_WIRELESS_MACMODE_NAME_LEN];
    char bitRate[MAX_WIRELESS_BITRATE_NAME_LEN];
    char essid[MAX_WIRELESS_ESSID_NAME_LEN];
    char region[MAX_WIRELESS_REGION_NAME_LEN];
    int channelNum;
    char MACAddress[MAC_ADDRESS_LEN];
    WirelessEncrypt wirelessEncrypt;
    WIFIPingWatchConfig pingWatchCfg;
} WIFIConfig;

typedef struct
{
    int enable;
    char version[MAX_WIFI_VERSION_LEN];
    char IPAddress[MAX_IP_NAME_LEN];
    char netMask[MAX_IP_NAME_LEN];
    char macMode[MAX_WIRELESS_MACMODE_NAME_LEN];
    char bitRate[MAX_WIRELESS_BITRATE_NAME_LEN];
    char essid[MAX_WIRELESS_ESSID_NAME_LEN];
    char region[MAX_WIRELESS_REGION_NAME_LEN];
    int channelNum;
    char MACAddress[MAC_ADDRESS_LEN];
    WirelessEncrypt wirelessEncrypt;
} WIFIApConfig;

typedef enum
{
    G4_SERVICE_NO = 0,               // 0--- No service
    G4_SERVICE_LIMITED = 1,          // 1--- Limited service
    G4_SERVICE_AVAILABLE = 2,        // 2--- Service available
    G4_SERVICE_LIMITED_REGIONAL = 3, // 3--- Limited regional service
    G4_SERVICE_POWER_SAVE = 4,       // 4--- Power save or deep sleep
} G4_srv_status;

#define G4_STR_LEN_64 64
#define G4_STR_LEN_32 32
#define G4_STR_LEN_256 256
typedef struct
{
    char Manufacturer[G4_STR_LEN_32]; // 4G模块厂商
    char Model[G4_STR_LEN_32];        // 4G模块型号
    char ICCID[G4_STR_LEN_32];
    char IMEI[G4_STR_LEN_32];
    char IMSI[G4_STR_LEN_32];
    char MSISDN[G4_STR_LEN_32];
    char WorkMode[G4_STR_LEN_32]; // 当前工作网络模式, LAN/NO_SRV/WCDMA/LTE
    char Operator[G4_STR_LEN_32]; // 运营商
    G4_srv_status nSvrStatus;
    int nDialStatus;  // 0:未拨号　1:已拨号
    int nSimStatus;   // 0--- SIM is not availabl 1--- SIM is available
    int nSignalLevel; // 信号强度,0-100
    char IccidList[G4_STR_LEN_256];
    char ImsiList[G4_STR_LEN_256];
    char Revision[G4_STR_LEN_256];
    int ManualMode;
    int CurOperator;
	char P2PID[G4_STR_LEN_256];
    int cellId; /* CREG ci，hex 转十进制 */
    int LAC;
    int MCC;    /* 460 */
    int MNC;    /* Operator 含 MOBILE 为 0，否则 1 */
    int lastCellId;
    int lastLAC;
    char lastOper[G4_STR_LEN_32];
    int locationNeedUpload;
    int locationHasReset;
    char locationResetUser[G4_STR_LEN_256];
} G4InfoStruct;

typedef enum
{
    G4_LOCATION_ACT_COMMIT = 0,
    G4_LOCATION_ACT_SAVE_RESET,
    G4_LOCATION_ACT_CLEAR_RESET,
} G4_LOCATION_ACT;

typedef struct
{
    int action;
    char user[G4_STR_LEN_256];
} G4LocationSet;

typedef struct
{
    int is_manual;
    int cur_operator;
} G4Config;

#define MAX_VPN_SERVER_NAME_LEN 256
#define MAX_VPN_USERNAME_LEN 256
#define MAX_VPN_PASSWORD_LEN 256
typedef struct
{
    int enable;
    char vpnServerIp[MAX_VPN_SERVER_NAME_LEN];
    char userName[MAX_VPN_USERNAME_LEN];
    char password[MAX_VPN_PASSWORD_LEN];
    int mtu;
} PPTPConfig;

#define MAX_URL_LEN 128
typedef struct
{
    char url[MAX_URL_LEN];
    char userName[ACCOUNT_NAME_MAX_LEN];
    char password[ACCOUNT_PASSWORD_MAX_LEN];
    int withattachment;
} AlarmServerConfig;

typedef struct
{
    LANConfig lanCfg;
    WIFIConfig wifiCfg;
    ADSLConfigNew adslCfg;
    DDNSConfig ddnsCfg;
    UPNPConfig upnpCfg;
    PPTPConfig pptpCfg;
    P2PConfig p2pCfg;
    WIFIApConfig wifiApCfg;
    AlarmServerConfig alarmServerCfg;
    G4Config g4Cfg;
    EncryptionConfig encryptionConfig;
    Gb35114CertConfig gb35114CertConfig;
    TelecomDevParamInfo telecomDevParamInfo;
} NetworkConfigNew;

int anj_config_network_default(NetworkConfigNew *pNetworkCfg);

int anj_config_network_get(IXML_Node *pNode, NetworkConfigNew *pNetworkCfg);

int anj_config_network_save(NetworkConfigNew *pNetworkCfg);

int anj_config_network_set(NetworkConfigNew *pstNetworkCfg);

int anj_config_network_lan_set(LANConfig *pstLanCfg);

int anj_config_network_wifi_set(WIFIConfig *pstWifiCfg);

int anj_config_network_wifiap_set(WIFIApConfig *pstWifiApCfg);

int anj_config_network_alarmserver_set(AlarmServerConfig *pstAlarmServerCfg);

int anj_config_network_adsl_set(ADSLConfigNew *pstAdslCfg);

int anj_config_network_g4_set(G4Config *pstG4Cfg);

int anj_config_network_pptp_set(PPTPConfig *pstPptpCfg);

int anj_config_network_ddns_set(DDNSConfig *pstDdnsCfg);

int anj_config_network_upnp_set(UPNPConfig *pstUpnpCfg);

int anj_config_network_p2p_set(P2PConfig *pstP2pCfg);

int anj_config_network_encryption_set(EncryptionConfig *pstEncryptionConfig);

int anj_config_network_gb35114_set(Gb35114CertConfig *pstGb35114CertConfig);

int anj_config_network_telecom_set(TelecomDevParamInfo *pstTelecomDevParamInfo);

int anj_config_network_load(NetworkConfigNew *pNetworkCfg);

char *anj_config_network_lan_conver_xml(LANConfig *lanCfg);
char *anj_config_network_wifi_conver_xml(WIFIConfig *wifiCfg);
char *anj_config_network_wifiap_conver_xml(WIFIApConfig *wifiCfg);
char *anj_config_network_alarmserver_conver_xml(AlarmServerConfig *cfg, int bPwdEntrypt);
char *anj_config_network_adsl_conver_xml(ADSLConfigNew *adslCfg, int bPwdEntrypt);
char *anj_config_network_pptp_conver_xml(PPTPConfig *pptpCfg, int bPwdEntrypt);
char *anj_config_network_ddns_conver_xml(DDNSConfig *ddnsCfg, int bPwdEntrypt);
char *anj_config_network_upnp_conver_xml(UPNPConfig *upnpCfg);
char *anj_config_network_g4_conver_xml(G4Config *g4Cfg);
char *anj_config_network_p2p_conver_xml(P2PConfig *pCfg);
char *anj_config_network_encryption_conver_xml(EncryptionConfig *pCfg);
char *anj_config_network_telecom_conver_xml(TelecomDevParamInfo *pCfg);
char *anj_config_network_gb35114_conver_xml(Gb35114CertConfig *pCfg);
char *anj_config_network_vsec_conver_xml(VsecDevInfo *pCfg);
char *anj_config_network_secure_conver_xml(VSEC_UKEY_INFO *pCfg);
char *anj_config_network_status_4g_conver_xml(G4InfoStruct *pCfg);
char *anj_config_network_conver_xml(NetworkConfigNew *pNetworkCfg);

int anj_config_network_adsl_get_by_xml(ADSLConfigNew *adslCfg, char *xmlBuf);
int anj_config_network_alarmserver_get_by_xml(AlarmServerConfig *cfg, char *xmlBuf);
int anj_config_network_ddns_get_by_xml(DDNSConfig *ddnsCfg, char *xmlBuf);
int anj_config_network_encrypt_get_by_xml(EncryptionConfig *pCfg, char *xmlBuf);
int anj_config_network_g4_get_by_xml(G4Config *g4Cfg, char *xmlBuf);
int anj_config_network_gb35114_get_by_xml(Gb35114CertConfig *pCfg, char *xmlBuf);
int anj_config_network_lan_get_by_xml(LANConfig *lanCfg, char *xmlBuf, int bHaveOldCfg);
int anj_config_network_p2p_get_by_xml(P2PConfig *pCfg, char *xmlBuf);
int anj_config_network_pptp_get_by_xml(PPTPConfig *pptpCfg, char *xmlBuf);
int anj_config_network_sercure_get_by_xml(VSEC_UKEY_INFO *pCfg, char *xmlBuf);
int anj_config_network_telecom_get_by_xml(TelecomDevParamInfo *pCfg, char *xmlBuf);
int anj_config_network_upnp_get_by_xml(UPNPConfig *upnpCfg, char *xmlBuf);
int anj_config_network_wifiap_get_by_xml(WIFIApConfig *wifiCfg, char *xmlBuf);
int anj_config_network_wifi_get_by_xml(WIFIConfig *wifiCfg, char *xmlBuf);
int anj_config_network_get_by_xml(NetworkConfigNew *pNetworkCfg, char *xmlBuf, int bHaveOldCfg);


#ifdef __cplusplus
}
#endif

#endif
