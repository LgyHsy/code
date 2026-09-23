/** ===========================================================================
 * Hik private command protocol types (NVR connect subset)
 * =========================================================================== */

#ifndef __HIK_NET_TYPES_H__
#define __HIK_NET_TYPES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UINT32
typedef uint32_t UINT32;
#endif
#ifndef UINT16
typedef uint16_t UINT16;
#endif
#ifndef UINT8
typedef uint8_t UINT8;
#endif

#define MAKE_NETSDK_VERSION(_major, _minor, _year, _month, _day) \
    ((((_major)&0xff) << 24) | (((_minor)&0xff) << 16) | ((((_year)-2000) & 0x3f) << 10) | \
     (((_month)&0xf) << 6) | ((_day)&0x3f))

#define CURRENT_NETSDK_VERSION MAKE_NETSDK_VERSION(2, 2, 2009, 3, 17)
#define NEW_NETSDK_INTERFACE 90

#define NETCMD_LOGIN 0x010000
#define NETCMD_RELOGIN 0x010010
#define NETCMD_LOGOUT 0x010100
#define NETCMD_USEREXCHANGE 0x010200
#define NETCMD_GET_CAPABILITES 0x011000
#define NETCMD_GET_NEW_CAPABILITES 0x117000
#define NETCMD_GET_DEVICECFG 0x020000
#define NETCMD_SET_CCDPARAMCFG 0x111095
#define NETCMD_GET_CCDPARAMCFG 0x111096
#define NETCMD_GET_NETCFG 0x020100
#define NETCMD_SET_NETCFG 0x020101
#define NETCMD_GET_NETAPPCFG 0x020110
#define NETCMD_GET_PICCFG 0x020200
#define NETCMD_GET_PICCFG_EX 0x020232
#define NETCMD_SET_PICCFG_EX 0x020233
#define NETCMD_GET_ALARMINCFG 0x020410
#define NETCMD_GET_TIMECFG 0x020500
#define NETCMD_SET_TIMECFG 0x020501
#define NETCMD_SET_USERCFG 0x020801
#define NETCMD_GET_USERCFG_EX 0x020802
#define NETCMD_SET_USERCFG_EX 0x020803
#define NETCMD_GET_RTSPPORT 0x020c04
#define DVR_GET_SCALECFG 0x020a04
#define DVR_SET_SCALECFG 0x020a05
#define NETCMD_GET_COMPRESSCFG_EX_V30 0x110040
#define NETCMD_SET_COMPRESSCFG_EX_V30 0x110041
#define NETCMD_GET_COMPRESSCFG_AUD 0x110042
#define NETCMD_GET_COMPRESSCFG_AUD_CURRENT 0x110044
#define NETCMD_GET_VIDEOEFFECT 0x030007
#define NETCMD_SET_VIDEOEFFECT 0x030008
#define NETCMD_GET_JPEGPICTURE 0x030009
#define NETCMD_PTZ 0x030200
#define DVR_PTZWITHSPEED 0x030203
#define NETCMD_ALARMCHAN 0x030400
#define NETCMD_STARTVOICECOM 0x030500
#define NETCMD_GET_WORKSTATUS 0x040000
#define NETCMD_MAKEIFRAME 0x090100
#define NETCMD_MAKEIFRAME2 0x090101

#define NETRET_QUALIFIED 1
#define NETRET_EXCHANGE 2
#define NETRET_ERRORPASSWD 3
#define NETRET_VER_DISMATCH 6
#define NETRET_NOT_SUPPORT 13
#define NETRET_NO_CHANNEL 17
#define NETRET_NEEDRECVDATA 20
#define NETRET_ERROR_DATA 23
#define NETRET_NO_USERID 30
#define NETRET_DVR_OPER_FAILED 33
#define NETRET_NEED_RELOGIN 39

/* Hik PTZ opcodes (ptzLib.h) */
#define HIK_PTZ_CLOSE_ALL 0
#define HIK_PTZ_CAMERA_PWRON 1
#define HIK_PTZ_LIGHT_PWRON 2
#define HIK_PTZ_WIPER_PWRON 3
#define HIK_PTZ_FAN_PWRON 4
#define HIK_PTZ_HEATER_PWRON 5
#define HIK_PTZ_AUX_PWRON1 6
#define HIK_PTZ_AUX_PWRON2 7
#define HIK_PTZ_SET_PRESET 8
#define HIK_PTZ_CLE_PRESET 9
#define HIK_PTZ_STOP_ALL 10
#define HIK_PTZ_ZOOM_IN 11
#define HIK_PTZ_ZOOM_OUT 12
#define HIK_PTZ_FOCUS_IN 13
#define HIK_PTZ_FOCUS_OUT 14
#define HIK_PTZ_IRIS_ENLARGE 15
#define HIK_PTZ_IRIS_SHRINK 16
#define HIK_PTZ_TILT_UP 21
#define HIK_PTZ_TILT_DOWN 22
#define HIK_PTZ_PAN_LEFT 23
#define HIK_PTZ_PAN_RIGHT 24
#define HIK_PTZ_UP_LEFT 25
#define HIK_PTZ_UP_RIGHT 26
#define HIK_PTZ_DOWN_LEFT 27
#define HIK_PTZ_DOWN_RIGHT 28
#define HIK_PTZ_AUTO_PAN 29
#define HIK_PTZ_GOTO_PRESET 39

#define ALARMTYPE_ALARMIN 0
#define ALARMTYPE_HD_FULL 1
#define ALARMTYPE_VI_LOST 2
#define ALARMTYPE_MOTDET 3
#define ALARMTYPE_HD_NO_FORMAT 4
#define ALARMTYPE_HD_WRITE_ERR 5
#define ALARMTYPE_MASK_ALARM 6
#define ALARMTYPE_VS_MISMATCH 7
#define ALARMTYPE_ILLEGAL_ACCESS 8

#define NETRET_IPCAMERA 30
#define HIK_IPCAMERA_TYPE_BYTE 31

#define STD_H264 1
#define STD_H265 10

#define HIK_AUDIOTYPE_G711U 1
#define HIK_AUDIOTYPE_G711A 2

#define CONSTANT_BITRATE 0
#define VARIABLE_BITRATE 1

#define HIK_FIXED_USER_ID 0x10001

#ifndef SERIALNO_LEN
#define SERIALNO_LEN 48
#endif
#ifndef NAME_LEN
#define NAME_LEN 32
#endif
#ifndef PASSWD_LEN
#define PASSWD_LEN 16
#endif
#ifndef PATHNAME_LEN
#define PATHNAME_LEN 128
#endif
#ifndef MACADDR_LEN
#define MACADDR_LEN 6
#endif
#ifndef MAX_ETHERNET
#define MAX_ETHERNET 2
#endif
#ifndef MAX_USERNUM
#define MAX_USERNUM 16
#endif
#ifndef MAX_CHANNUM
#define MAX_CHANNUM 1
#endif
#ifndef MAX_LINK
#define MAX_LINK 10
#endif
#ifndef MAX_ALARMIN
#define MAX_ALARMIN MAX_CHANNUM
#endif
#ifndef MAX_ALARMOUT
#define MAX_ALARMOUT 4
#endif
#ifndef MAX_DISKNUM
#define MAX_DISKNUM 16
#endif
#ifndef MAX_DAYS
#define MAX_DAYS 7
#endif
#ifndef MAX_TIMESEGMENT
#define MAX_TIMESEGMENT 4
#endif
#ifndef MASK_MAX_REGION
#define MASK_MAX_REGION 4
#endif
#ifndef HIK_CCD_GET_RESP_RESERVE
#define HIK_CCD_GET_RESP_RESERVE 344
#endif

typedef struct
{
    UINT32 length;
    UINT8 ifVer;
    UINT8 res1[3];
    UINT32 checkSum;
    UINT32 netCmd;
    UINT32 clientIp;
    UINT32 userID;
    UINT8 clientMac[6];
    UINT8 res[2];
} NETCMD_HEADER;

typedef struct
{
    NETCMD_HEADER header;
    UINT32 channel;
} NETCMD_CHAN_HEADER;

typedef struct
{
    UINT8 startHour;
    UINT8 startMin;
    UINT8 stopHour;
    UINT8 stopMin;
} NETPARAM_TIMESEG;

typedef struct
{
    UINT32 length;
    UINT32 checkSum;
    UINT32 retVal;
    UINT8 res[4];
} NETRET_HEADER;

typedef struct
{
    UINT32 length;
    UINT8 ifVer;
    UINT8 res1[3];
    UINT32 checkSum;
    UINT32 netCmd;
    UINT32 version;
    UINT8 res2[4];
    UINT32 clientIp;
    UINT8 clientMac[6];
    UINT8 res3[2];
    UINT8 username[32];
    UINT8 password[32];
} NET_LOGIN_REQ;

typedef struct
{
    UINT32 length;
    UINT32 checkSum;
    UINT32 retVal;
    UINT32 devSdkVer;
    UINT32 userID;
    UINT8 serialno[SERIALNO_LEN];
    UINT8 devType;
    UINT8 channelNums;
    UINT8 firstChanNo;
    UINT8 alarmInNums;
    UINT8 alarmOutNums;
    UINT8 hdiskNums;
    UINT8 res2[2];
} NET_LOGIN_RET;

typedef struct
{
    UINT32 handleType;
    UINT32 alarmOutTriggered;
} hik_exception_t;

typedef struct
{
    UINT32 length;
    UINT8 DVRName[NAME_LEN];
    UINT32 deviceID;
    UINT32 recycleRecord;
    UINT8 serialno[SERIALNO_LEN];
    UINT32 softwareVersion;
    UINT32 softwareBuildDate;
    UINT32 dspSoftwareVersion;
    UINT32 dspSoftwareBuildDate;
    UINT32 panelVersion;
    UINT32 hardwareVersion;
    UINT8 alarmInNums;
    UINT8 alarmOutNums;
    UINT8 rs232Nums;
    UINT8 rs485Nums;
    UINT8 netIfNums;
    UINT8 hdiskCtrlNums;
    UINT8 hdiskNums;
    UINT8 devType;
    UINT8 channelNums;
    UINT8 firstChanNo;
    UINT8 decodeChans;
    UINT8 vgaNums;
    UINT8 usbNums;
    UINT8 lockFrontPanel;
    UINT8 enableTimingRec;
    UINT8 res;
} NETPARAM_DEVICE_CFG;

typedef struct
{
    UINT32 length;
    UINT16 RTSPPort;
    UINT8 res[54];
} NETPARAM_RTSP_CFG;

typedef struct
{
    UINT32 enableSignalLostAlarm;
    hik_exception_t signalLostAlarmHandleType;
    NETPARAM_TIMESEG signalLostArmTime[MAX_DAYS][MAX_TIMESEGMENT];
} VI_SIGNAL_LOST_CFG_V13;

typedef struct
{
    UINT32 motionLine[18];
    UINT8 motionLevel;
    UINT8 bEnableHandleMotion;
    UINT8 res1[2];
    hik_exception_t motionHandleType;
    NETPARAM_TIMESEG motDetArmTime[MAX_DAYS][MAX_TIMESEGMENT];
    UINT32 motTriggerRecChans;
} NETPARAM_MOTION_CFG_V13;

typedef struct
{
    UINT32 bEnableMaskAlarm;
    UINT16 maskX;
    UINT16 maskY;
    UINT16 maskW;
    UINT16 maskH;
    hik_exception_t maskAlarmHandleType;
    NETPARAM_TIMESEG maskAlarmArmTime[MAX_DAYS][MAX_TIMESEGMENT];
} NETPARAM_MASKALARM_CFG_V13;

typedef struct
{
    UINT32 length;
    UINT8 chanName[NAME_LEN];
    UINT32 videoFormat;
    UINT8 brightness;
    UINT8 contrast;
    UINT8 saturation;
    UINT8 hue;
    UINT32 bShowChanName;
    UINT16 chanNameX;
    UINT16 chanNameY;
    VI_SIGNAL_LOST_CFG_V13 viLostCfg;
    NETPARAM_MOTION_CFG_V13 motionCfg;
    NETPARAM_MASKALARM_CFG_V13 maskAlarmCfg;
    UINT32 bEnableHide;
    struct
    {
        UINT16 x;
        UINT16 y;
        UINT16 w;
        UINT16 h;
    } hideArea[MASK_MAX_REGION];
    UINT32 bShowOsd;
    UINT16 osdX;
    UINT16 osdY;
    UINT8 osdType;
    UINT8 bDispWeek;
    UINT8 osdAttrib;
    UINT8 res2;
} NETPARAM_PIC_CFG_V14;

typedef struct
{
    UINT8 byStreamType;
    UINT8 byResolution;
    UINT8 byBitrateType;
    UINT8 byPicQuality;
    UINT32 dwVideoBitrate;
    UINT32 dwVideoFrameRate;
    UINT16 wIntervalFrameI;
    UINT8 byIntervalBPFrame;
    UINT8 byENumber;
    UINT8 byVideoEncType;
    UINT8 byAudioEncType;
    UINT8 byres[10];
} NETPARAM_COMP_PARA_V30;

typedef struct
{
    UINT32 length;
    NETPARAM_COMP_PARA_V30 struNormHighRecordPara;
    NETPARAM_COMP_PARA_V30 struNormLowRecordPara;
    NETPARAM_COMP_PARA_V30 struEventRecordPara;
    NETPARAM_COMP_PARA_V30 struNetPara;
} NETPARAM_COMPRESS_CFG_V30;

typedef struct
{
    UINT32 year;
    UINT32 month;
    UINT32 day;
    UINT32 hour;
    UINT32 min;
    UINT32 sec;
} NETPARAM_TIME_CFG;

typedef struct
{
    NETCMD_HEADER header;
    UINT32 capabilityType;
    char pDesParam[1024];
} NETCMD_GET_CAPABILITY_REQ;

typedef struct
{
    NETCMD_HEADER header;
    UINT32 channel;
    UINT32 command;
    union {
        UINT32 presetNo;
        UINT32 speed;
    };
} NET_PTZ_CTRL_DATA;

typedef struct
{
    UINT16 size;
    UINT16 quality;
} JPEG_CFG;

typedef struct
{
    UINT32 alarmType;
    UINT32 alarmInNumber;
    UINT32 triggeredAlarmOut;
    UINT32 triggeredRecChan;
    UINT32 channelNo;
    UINT32 diskNo;
} NETRET_ALARMINFO;

/* video effect */
typedef struct
{
    UINT8 brightness;
    UINT8 contrast;
    UINT8 saturation;
    UINT8 hue;
} NETPARAM_VIDEOPARA;

/* network */
typedef struct
{
    UINT32 devIp;
    UINT32 devIpMask;
    UINT32 mediaType;
    UINT16 ipPortNo;
    UINT8 res1[2];
    UINT8 macAddr[6];
    UINT8 res2[2];
} NETPARAM_ETHER_CFG;

typedef struct
{
    UINT32 length;
    NETPARAM_ETHER_CFG etherCfg[MAX_ETHERNET];
    UINT32 manageHostIp;
    UINT16 manageHostPort;
    UINT16 httpPort;
    UINT32 ipResolverIpAddr;
    UINT32 mcastAddr;
    UINT32 gatewayIp;
    UINT32 nfsIp;
    UINT8 nfsDirectory[PATHNAME_LEN];
    UINT32 bEnablePPPoE;
    UINT8 pppoeName[NAME_LEN];
    UINT8 pppoePassword[PASSWD_LEN];
    UINT8 res2[4];
    UINT32 pppoeIp;
} NETPARAM_NETWORK_CFG;

/* NTP 80 / DDNS 128 / EMAIL wire layouts for NETAPP size parity */
typedef struct
{
    UINT8 ntpServer[64];
    UINT16 interval;
    UINT8 enableNTP;
    signed char timedifferenceH;
    signed char timedifferenceM;
    UINT8 res[11];
} HIK_NTPPARA;

typedef struct
{
    UINT8 username[NAME_LEN];
    UINT8 password[PASSWD_LEN];
    UINT8 domainName[64];
    UINT8 enableDDNS;
    UINT8 hostIdx;
    UINT16 ddnsPort;
    UINT8 res[12];
} HIK_DDNSPARA;

typedef struct
{
    UINT8 username[64];
    UINT8 password[64];
    UINT8 smtpServer[64];
    UINT8 pop3Server[64];
    UINT8 mailAddr[64];
    UINT8 eventMailAddr1[64];
    UINT8 eventMailAddr2[64];
    UINT8 res[16];
} HIK_EMAILPARA;

typedef struct
{
    UINT32 length;
    UINT32 dnsIp;
    HIK_NTPPARA ntpClientParam;
    HIK_DDNSPARA ddnsClientParam;
    HIK_EMAILPARA emailParam;
} NETPARAM_NETAPP_CFG;

/* legacy PIC_CFG (no arm-time / single hide rect) */
typedef struct
{
    UINT32 enableSignalLostAlarm;
    hik_exception_t signalLostAlarmHandleType;
} VI_SIGNAL_LOST_CFG;

typedef struct
{
    UINT32 motionLine[18];
    UINT8 motionLevel;
    UINT8 bEnableHandleMotion;
    UINT8 res1[2];
    hik_exception_t motionHandleType;
    UINT32 motTriggerRecChans;
} NETPARAM_MOTION_CFG;

typedef struct
{
    UINT32 bEnableMaskAlarm;
    UINT16 maskX;
    UINT16 maskY;
    UINT16 maskW;
    UINT16 maskH;
    hik_exception_t maskAlarmHandleType;
} NETPARAM_MASKALARM_CFG;

typedef struct
{
    UINT32 length;
    UINT8 chanName[NAME_LEN];
    UINT32 videoFormat;
    UINT8 brightness;
    UINT8 contrast;
    UINT8 saturation;
    UINT8 hue;
    UINT32 bShowChanName;
    UINT16 chanNameX;
    UINT16 chanNameY;
    VI_SIGNAL_LOST_CFG viLostCfg;
    NETPARAM_MOTION_CFG motionCfg;
    NETPARAM_MASKALARM_CFG maskAlarmCfg;
    UINT32 bEnableHide;
    UINT16 hideX;
    UINT16 hideY;
    UINT16 hideW;
    UINT16 hideH;
    UINT32 bShowOsd;
    UINT16 osdX;
    UINT16 osdY;
    UINT8 osdType;
    UINT8 bDispWeek;
    UINT8 osdAttrib;
    UINT8 res2;
} NETPARAM_PIC_CFG;

typedef struct
{
    UINT8 byAudioEncType;
    UINT8 byres[7];
} NETPARAM_COMPRESS_CFG_AUDIO_V30;

/* alarm in */
typedef struct
{
    UINT8 alarmInName[NAME_LEN];
    UINT8 sensorType;
    UINT8 bEnableAlarmIn;
    UINT8 res[2];
    hik_exception_t alarmInAlarmHandleType;
    NETPARAM_TIMESEG armTime[MAX_DAYS][MAX_TIMESEGMENT];
    UINT8 recordChanTriggered[16];
    UINT8 bEnablePreset[MAX_CHANNUM];
    UINT8 presetNo[MAX_CHANNUM];
    UINT8 bEnablePtzCruise[MAX_CHANNUM];
    UINT8 ptzCruise[MAX_CHANNUM];
    UINT8 bEnablePtzTrack[MAX_CHANNUM];
    UINT8 trackNo[MAX_CHANNUM];
} NETPARAM_ALARMIN;

typedef struct
{
    UINT32 length;
    NETPARAM_ALARMIN alarmIn;
} NETPARAM_ALARMIN_CFG;

/* user */
typedef struct
{
    UINT8 username[NAME_LEN];
    UINT8 password[PASSWD_LEN];
    UINT32 permission;
    UINT32 ipAddr;
    UINT8 macAddr[MACADDR_LEN];
    UINT8 priority;
    UINT8 res;
} NETPARAM_USER;

typedef struct
{
    UINT32 length;
    NETPARAM_USER user[MAX_USERNUM];
} NETPARAM_USER_CFG;

typedef struct
{
    UINT8 username[NAME_LEN];
    UINT8 password[PASSWD_LEN];
    UINT32 permission;
    UINT32 localPlayPermission;
    UINT32 netPlayPermission;
    UINT32 netPreviewPermission;
    UINT32 ipAddr;
    UINT8 macAddr[MACADDR_LEN];
    UINT8 priority;
    UINT8 res;
} NETPARAM_USER_EX;

typedef struct
{
    UINT32 length;
    NETPARAM_USER_EX user[MAX_USERNUM];
} NETPARAM_USER_CFG_EX;

/* work status */
typedef struct
{
    UINT8 bRecStarted;
    UINT8 bViLost;
    UINT8 chanStatus;
    UINT8 res1;
    UINT32 bitRate;
    UINT32 netLinks;
    UINT32 clientIP[MAX_LINK];
} ENC_CHAN_STATUS;

typedef struct
{
    UINT32 totalSpace;
    UINT32 freeSpace;
    UINT32 diskStatus;
} HDISK_STATUS;

typedef struct
{
    UINT32 deviceStatus;
    HDISK_STATUS hdStatus[MAX_DISKNUM];
    ENC_CHAN_STATUS chanStatus[MAX_CHANNUM];
    UINT8 alarmInStatus[MAX_ALARMIN];
    UINT8 alarmOutStatus[MAX_ALARMOUT];
    UINT32 localDispStatus;
} DVR_WORKSTATUS;

typedef struct
{
    UINT32 deviceStatus;
    HDISK_STATUS hdStatus[8];
    ENC_CHAN_STATUS chanStatus[MAX_CHANNUM];
    UINT8 alarmInStatus[MAX_ALARMIN];
    UINT8 alarmOutStatus[MAX_ALARMOUT];
    UINT32 localDispStatus;
} DVR_WORKSTATUS_V10;

/* CCD front-end params (non-HI3515 layout) */
typedef struct
{
    UINT32 length;
    UINT8 brightnessLevel;
    UINT8 contrastLevel;
    UINT8 sharpnessLevel;
    UINT8 saturationLevel;
    UINT8 hueLevel;
    UINT8 res_VE[3];
    UINT8 gainLevel;
    UINT8 res_GAIN[3];
    UINT32 maxGainValue;
    UINT8 whiteBalanceMode;
    UINT8 whiteBalanceModeRGain;
    UINT8 whiteBalanceModeBGain;
    UINT8 res_WB[5];
    UINT8 exposureMode;
    UINT8 res_EXPOSURE[3];
    UINT32 ExposureSet;
    UINT32 exposureUSERSET;
    UINT32 exposureTarget;
    UINT8 gammaCorrectionEnabled;
    UINT8 gammaCorrectionLevel;
    UINT8 res_GAMA[6];
    UINT8 WDREnabled;
    UINT8 WDRLevel1;
    UINT8 WDRLevel2;
    UINT8 WDRContrastLevel;
    UINT8 res_WDR[16];
    UINT8 dayNightFilterType;
    UINT8 switchScheduleEnabled;
    UINT8 beginTime;
    UINT8 endTime;
    UINT8 dayToNightFilterLevel;
    UINT8 nightToDayFilterLevel;
    UINT8 dayNightFilterTime;
    UINT8 res_DN[5];
    UINT8 BacklightMode;
    UINT8 BacklightLevel;
    UINT8 res1_BL1[2];
    UINT32 positionX0;
    UINT32 positionY0;
    UINT32 positionX1;
    UINT32 positionY1;
    UINT8 res_BL2[4];
    UINT8 DigitalNoiseRemoveEnable;
    UINT8 DigitalNoiseRemoveLevel;
    UINT8 res_NR[6];
    UINT8 powerLineFrequencyMode;
    UINT8 irisMode;
    UINT8 Mirror;
    UINT8 DigitalZoom;
    UINT8 DeadPixelDetect;
    UINT8 blackPwl;
    UINT8 res[30];
} NETPARAM_CCDPARA_CFG;

#ifdef __cplusplus
}
#endif

#endif
