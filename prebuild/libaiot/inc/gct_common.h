#ifndef __GCT_COMMON_H__
#define __GCT_COMMON_H__
#include <stdio.h>
#include <stdint.h>
#include "gct_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//使能属性功能
typedef enum _GCT_DD_ATTR_SWITCH_TYPE{
	GCT_DD_ATTR_SWITCH_TYPE_CALL_PROPERTY 	= 0,	//是否显示物业呼叫
	GCT_DD_ATTR_SWITCH_TYPE_SHOW_RENTAL		= 1, 	//显示租赁
	GCT_DD_ATTR_SWITCH_TYPE_FACE			= 2, 	//人脸识别
	GCT_DD_ATTR_SWITCH_TYPE_QR				= 3, 	//二维码
	GCT_DD_ATTR_SWITCH_TYPE_NFC				= 4, 	//门禁卡
	GCT_DD_ATTR_SWITCH_TYPE_MAX,					//计数 不用
}GCT_DD_ATTR_SWITCH_TYPE;

typedef enum _INNDEV_OPENLOCK_TYPE{
	INNDEV_OPENLOCK_TYPE_UNKNOW,
	INNDEV_OPENLOCK_TYPE_PASSWD,		//固件密码或者临时密码
	INNDEV_OPENLOCK_TYPE_NFC,			//IC卡或者NFC卡开锁
	INNDEV_OPENLOCK_TYPE_HUMAN,			//人脸
}INNDEV_OPENLOCK_TYPE;

typedef enum _INNDEV_NOTIFY_TYPE{
	INNDEV_NOTIFY_TYPE_USER_LIST,			//用户列表有变更
	INNDEV_NOTIFY_TYPE_GIDINFO,				//gid用户名密码等有变更
	INNDEV_NOTIFY_TYPE_CALL_TO,				//呼叫,超时没人接听
	INNDEV_NOTIFY_TYPE_ACCEPT,				//APP接听
	INNDEV_NOTIFY_TYPE_ACCEPT_TO,			//接听中,超时时限,比如10分钟超时就关闭
	INNDEV_NOTIFY_TYPE_SHUTDOWN,			//APP挂断
	INNDEV_NOTIFY_TYPE_OPENLOCK_APP,		//APP开锁
	INNDEV_NOTIFY_TYPE_OPENLOCK_MZ,			//门钟开锁
	INNDEV_NOTIFY_TYPE_OPENLOCK_INNERPAD,	//室内机开锁
	INNDEV_NOTIFY_TYPE_OPENLOCK_INNER,		//内部物理开锁
	INNDEV_NOTIFY_TYPE_WIFI_CONN_SUCC,		//WiFi连接成功
	INNDEV_NOTIFY_TYPE_WIFI_CONN_FAIL,		//WiFi连接失败
	INNDEV_NOTIFY_TYPE_APP_CONN_ERR,		//接听过程中,APP连接中断
	INNDEV_NOTIFY_TYPE_LOCKLIST_UPDATE,		//锁列表更新
}INNDEV_NOTIFY_TYPE;

typedef enum _GCT_INNERDEV_COMMON_TYPE{
	GCT_INNERDEV_COMMON_TYPE_UNKNOW,
	GCT_INNERDEV_COMMON_TYPE_RESET,	//恢复出厂
}GCT_INNERDEV_COMMON_TYPE;

//请求i帧的模块
typedef enum _GCT_IFRAME_REASON{
	GCT_IFRAME_REASON_REAL_PLAY,		//实时流模块
	GCT_IFRAME_REASON_CLOUD,			//云存储模块
	GCT_IFRAME_REASON_SD_PB,			//sd卡模块
}GCT_IFRAME_REASON;

typedef enum _GCT_STATE_TO_SERVER{
	GCT_STATE_TO_SERVER_NO_START 	= 0,	//连接失败，模块未启动
	GCT_STATE_TO_SERVER_CONN_LBS 	= 1,	//开始连接lbs
	GCT_STATE_TO_SERVER_CONN_GOO	= 2,	//开始连接gooserver
	GCT_STATE_TO_SERVER_GOO_CONNING	= 3,	//gooserver连接中(尚未连接上)
	GCT_STATE_TO_SERVER_US		 	= 4,	//等待p2p
	GCT_STATE_TO_SERVER_REG_SUCESS 	= 5,	//gooserver注册成功，设备在线
	GCT_STATE_TO_SERVER_REG_FAIL	= 7,	//注册失败,可能是gid复用或者非法gid
	GCT_STATE_TO_SERVER_WEBPWD_FAIL	= 8,	//设备密码校验失败
}GCT_STATE_TO_SERVER;

//设备内部功能能力集
typedef enum _GCT_DEV_ABILITY{
	GCT_DEV_ABILITY_ONVIF 			= 10000,	//支持ONVIF
	GCT_DEV_ABILITY_DOUBLE_AV,					//支持双向对讲
}GCT_DEV_ABILITY;

//码流类型
typedef enum _GT_COMMON_STREAM_TYPE{
	GT_COMMON_STREAM_TYPE_MAIN	= 0,	//主码流
	GT_COMMON_STREAM_TYPE_SUB	= 1,	//次码流
	GT_COMMON_STREAM_TYPE_SD	= 2,	//sd卡回放
	GT_COMMON_STREAM_TYPE_TRANS	= 3,	//透明通道
	GT_COMMON_STREAM_TYPE_LONG	= 7,	//长连接
	GT_COMMON_STREAM_TYPE_AUDIO = 1000,	//纯音频码流
}GT_COMMON_STREAM_TYPE;

//流类型
typedef enum _GT_COMMON_DATA_TYPE{
	GT_COMMON_DATA_TYPE_VIDEO	= 0,	//视频
	GT_COMMON_DATA_TYPE_AUDIO	= 1,	//音频
	GT_COMMON_DATA_TYPE_AV		= 2,	//音视
}GT_COMMON_DATA_TYPE;

//云存储套餐信息
typedef struct gt_cloud_package_info_t{
	GCT_UINT32 nChannelNo;
	GCT_UINT32 nMainOrSub;
}gt_cloud_package_info;

typedef GCT_VOID (*fun_server_ext_info_callback)(const GCT_CHAR* pJson);

//设备库功能集
typedef enum _GCT_MODULE{
	GCT_MODULE_CLOUD 	= 0x1<<0,	//只要云存储功能(不包含ipc功能)
	GCT_MODULE_IPC 		= 0x1<<1,	//只要ipc(支持DVR和NVR多路)功能(不包含云存储)
	GCT_MODULE_DOORBELL	= 0x1<<2,	//需要门铃功能(包含云存储和ipc)
}GCT_MODULE;

//设备库支持的码流类型
typedef enum _STREAM_SUPPORT_TYPE{
	STREAM_SUPPORT_TYPE_MAIN 	= 0x1<<0,	//支持主码流
	STREAM_SUPPORT_TYPE_SUB		= 0x1<<1,	//支持次码流
}STREAM_SUPPORT;

typedef enum _GCT_EXT_PRODUCT_TYPE{
	GCT_EXT_PRODUCT_TYPE_UNKNOW,			//未知
	GCT_EXT_PRODUCT_TYPE_DOORBELL_SINGLE,	//单户门铃(单个摄像头)
	GCT_EXT_PRODUCT_TYPE_DOORBELL_MUTIL,	//多户门铃(公寓)(单个摄像头)
	GCT_EXT_PRODUCT_TYPE_DOORBELL_NVR,		//NVR门铃(多个摄像头)
	GCT_EXT_PRODUCT_TYPE_4G_NORMAL,			//普通4G
}GCT_EXT_PRODUCT_TYPE;

typedef enum
{
    AIOT_CLOUD_TYPE_ALI = 0,
    AIOT_CLOUD_TYPE_CTCLOUD = 1,    
}AiotCloudType;

typedef struct gct_param_t{
	GCT_CHAR	szGid[128];				//设备id，唯一，每个设备只能填死一个
	GCT_CHAR 	szModel[128];		//设备型号
	GCT_CHAR 	szVersion[128];		    //固件版本号
	GCT_CHAR 	szAuthKey[128];			//授权的key,请询问浪涛商务提供
	GCT_CHAR 	szComId[128];			//授权的comid,请询问浪涛商务提供
	GCT_UINT32	nChannelCount;			//通道数为1的话，默认为ipc 大于1则为DVR或者NVR
	GCT_UINT32	nVideoSessionLimit;		//允许的最大视频连接数
	GCT_UINT32	nReplaySessionLimit;		//允许的最大回放连接数
	GCT_UINT32	nStreamSupportType;		//支持的码流类型,看STREAM_SUPPORT定义，比如支持主码流，填STREAM_SUPPORT_TYPE_MAIN，主次都支持填STREAM_SUPPORT_TYPE_MAIN | STREAM_SUPPORT_TYPE_SUB
	GCT_UINT8 	nConnectionCount;		//最大连接数
	GCT_UINT32 	nSupportModule;			//需要设备库支持的模块
	AiotCloudType 	nCloudType;			//云存储类型
	GCT_CHAR 	szLocalCfgFilePath[64]; //pPath 一定要存在,如果不存在则先创建再调用这个接口,这个目录要求重启后不能删除
	GCT_CHAR	szSdcardAbsPath[128];	//sdcard的绝对路径 比如"/mnt"
	GCT_CHAR	szGidPwd[128];
	GCT_EXT_PRODUCT_TYPE euGCT_EXT_PRODUCT_TYPE;
}gct_param;

typedef enum _GCT_VIDEO_CODEC_TYPE{
	GCT_VIDEO_CODEC_TYPE_H264		= 0x34363248,	//H264
	GCT_VIDEO_CODEC_TYPE_MJPEG		= 0x45464758,  //mjpeg
	GCT_VIDEO_CODEC_TYPE_H265		= 0x56565268,	//H265
}GCT_VIDEO_CODEC_TYPE;

typedef enum _GCT_AUDIO_CODEC_TYPE{
	GCT_AUDIO_CODEC_TYPE_G711A	= 0x7A19,
	GCT_AUDIO_CODEC_TYPE_G711U  = 0x7A25,
	GCT_AUDIO_CODEC_TYPE_AAC    = 0x7A26,
	GCT_AUDIO_CODEC_TYPE_MP3    = 0x7A27,
	GCT_AUDIO_CODEC_TYPE_PCM	= 0x7A28,
}GCT_AUDIO_CODEC_TYPE;

//视频数据格式
typedef struct gct_video_data_format_t{
	GCT_VIDEO_CODEC_TYPE 		euGCT_VIDEO_CODEC_TYPE;			//编码方式
	GCT_UINT32 					bitrate;        				//比特率, bps
	GCT_UINT16 					width;							//图像宽度
	GCT_UINT16 					height;							//图像高度
	GCT_UINT8 					framerate;						//帧率, fps
	GCT_UINT8 					frameInterval;   				//I帧间隔
	GCT_UINT8 					reserve;		
} gct_video_data_format;

//音频数据格式
typedef struct gct_audio_data_format_t{
	GCT_UINT32 					samplesRate;					//每秒采样
	GCT_UINT32 					bitrate;						//比特率, bps
	GCT_AUDIO_CODEC_TYPE 		euGCT_AUDIO_CODEC_TYPE;			//编码格式
	GCT_UINT16 					channelNumber;					//音频通道号
	GCT_UINT16 					bitsPerSample;					//每采样比特数（一般是16）
	GCT_UINT16 					reserve;
} gct_audio_data_format;

typedef struct gct_stream_data_format_t{
	gct_video_data_format videoFormat;
	gct_audio_data_format audioFormat;
} gct_stream_data_format;

typedef struct gct_dd_uselist_node_t{
	GCT_CHAR szUnitLabel[32];
	GCT_CHAR szFirstName[32];
	GCT_CHAR szLastName[32];
	GCT_INT64 nVirNo;
	GCT_BOOL bIsAdmin;			//是否是物业
	struct gct_dd_uselist_node_t* pNext;
}gct_dd_uselist_node;

///////// 告警业务相关 /////////////////////////////////
typedef struct gct_push_alarm_info_t{
    GCT_UINT32	nAlarmType;		// 报警类型 自定义类型，从0开始
    GCT_INT32   nChannelNo;		// 通道号		如果不需要显示通道号，则填-1即可，正常的通道号是从0开始。
	GCT_INT64   nVirNo;			// 虚拟通道号
	GCT_CHAR 	szCustomBuf[56];		// 自定义报警文本，如果是GCT_ALARM_TYPE填GAT_CUSTOM类型，则这个字段有效
	GCT_BOOL 	bUploadCloud;
} gct_push_alarm_info;

typedef struct gct_push_alarm_ext_info_t{
   gct_push_alarm_info 	gct_alarm_info;
   GCT_CHAR 			szTimeStr[128];
   GCT_CHAR 			szCloudPicPath[256];		//云存储需要
} gct_push_alarm_ext_info;

// ControlArgData
//   OWSP_PTZ_MV_STOP   ：无
//   OWSP_PTZ_ZOOM_DEC  ：arg1, 步长
//   OWSP_PTZ_ZOOM_INC  ：arg1, 步长
//   OWSP_PTZ_FOCUS_INC ：arg1, 步长
//   OWSP_PTZ_FOCUS_DEC ：arg1, 步长
//   OWSP_PTZ_MV_UP     ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长; arg4, 移动角度（水平方向按照180°计算）
//   OWSP_PTZ_MV_DOWN   ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长; arg4, 移动角度（垂直方向按照90°计算）
//   OWSP_PTZ_MV_LEFT   ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长; arg4, 移动角度（水平方向按照180°计算）
//   OWSP_PTZ_MV_RIGHT  ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长; arg4, 移动角度（垂直方向按照90°计算）
//   OWSP_PTZ_IRIS_INC  ：arg1, 步长
//   OWSP_PTZ_IRIS_DEC  ：arg1, 步长
//   OWSP_PTZ_AUTO_CRUISE  : arg1, 1 = 开始巡航, 0 = 停止巡航; arg2, 水平速度; arg3, 垂直速度
//   OWSP_PTZ_GOTO_PRESET  : arg1, 预置点编号
//   OWSP_PTZ_SET_PRESET   : arg1, 预置点编号
//   OWSP_PTZ_CLEAR_PRESET : arg1, 预置点编号, 如果为0xFFFFFFFF标识清除全部
//   OWSP_PTZ_ACTION_RESET
//   OSWP_PTZ_MV_LEFTUP    ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长
//   OWSP_PTZ_MV_LEFTDOWN  ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长
//   OWSP_PTZ_MV_RIGHTUP   ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长
//   OWSP_PTZ_MV_RIGHTDOWN ：arg1, 水平速度; arg2, 垂直速度; arg3, 步长
//   OWSP_PTZ_CLEAR_TOUR   ：arg1, 线路编号
//   OWSP_PTZ_ADD_PRESET_TO_TOUR : arg1, 预置点编号; arg2, 线路编号
//   OWSP_PTZ_DEL_PRESET_TO_TOUR : arg1, 预置点编号; arg2, 线路编号
typedef struct _ControlArgData{		
	GCT_UINT32 arg1;
	GCT_UINT32 arg2;
	GCT_UINT32 arg3;
	GCT_UINT32 arg4;
} ControlArgData;

typedef enum _GLNK_PTZControlCmd{
	GLNK_PTZ_MV_STOP      = 0,    	//停止运动
	GLNK_PTZ_ZOOM_DEC     = 5,		//放大
	GLNK_PTZ_ZOOM_INC     = 6,		//缩小
	GLNK_PTZ_FOCUS_INC    = 7,    	//焦距放大
	GLNK_PTZ_FOCUS_DEC    = 8,		//焦距缩小
	GLNK_PTZ_MV_UP        = 9,    	//向上
	GLNK_PTZ_MV_DOWN      = 10,   	//向下
	GLNK_PTZ_MV_LEFT      = 11,   	//向左
	GLNK_PTZ_MV_RIGHT     = 12,   	//向右
	GLNK_PTZ_IRIS_INC     = 13,   	//光圈放大
	GLNK_PTZ_IRIS_DEC     = 14,   	//光圈缩小
	GLNK_PTZ_AUTO_CRUISE  = 15,	  	//自动巡航
	GLNK_PTZ_GOTO_PRESET  = 16,   	//跳转预置位
	GLNK_PTZ_SET_PRESET   = 17,   	//设置预置位点
	GLNK_PTZ_CLEAR_PRESET = 18,   	//清除预置位点
	GLNK_PTZ_ACTION_RESET = 20,   	//PTZ复位
	GLNK_PTZ_MV_LEFTUP    = 21,
	GLNK_PTZ_MV_LEFTDOWN  = 22,
	GLNK_PTZ_MV_RIGHTUP   = 23,
	GLNK_PTZ_MV_RIGHTDOWN = 24,
	GLNK_PTZ_CLEAR_TOUR   = 25,
	GLNK_PTZ_ADD_PRESET_TO_TOUR  = 26,
	GLNK_PTZ_DEL_PRESET_TO_TOUR  = 27
} GLNK_PTZControlCmd;

/*****************************************************************************
			 	 	 	 	切屏报警协议
 *****************************************************************************
	如下示屏幕格数顺序为书写顺序，切为4分屏，1屏 4屏侦测报警，则
	GLNK_SetScreenAlarmRequest->ScreenListLen = sizeof(GLNK_SetScreenAlarmRequest) + sizeof(GLNK_ScreenList) * 4;
	GLNK_SetScreenAlarmReques->ScreenList[0].ifSetAlarm = 1;
	GLNK_SetScreenAlarmReques->ScreenList[3].ifSetAlarm = 1;
  						---------
  						| 1 | 2 |
  						---------
  						| 3 | 4 |
  						---------												*/
typedef struct _GLNK_ScreenList{
	GCT_CHAR 	ifSetAlarm;						//	1为设置为侦测报警，0为不检测
	GCT_INT32 	reserve;						//保留字段
}GLNK_ScreenList;

typedef struct _GLNK_SetScreenAlarmRequest{
	GCT_INT32 ScreenChannel; 				//选定侦测报警的通道号
	GCT_INT32 ScreenListLen;				//列表的长度 = sizeof(GLNK_SetScreenAlarmRequest) + sizeof(GLNK_ScreenList)*ListNum,num为切屏数量
	GLNK_ScreenList ScreenList[0];			//保留字段
}GLNK_SetScreenAlarmRequest;

typedef enum _GCT_WIFI_SIGNAL_LEVEL{
	GCT_WIFI_SIGNAL_LEVEL_STRONG = 0,			//信号强								
	GCT_WIFI_SIGNAL_LEVEL_MID,					//信号中等					
	GCT_WIFI_SIGNAL_LEVEL_WEAK,					//信号弱			
}GCT_WIFI_SIGNAL_LEVEL;

typedef struct gct_wifi_info_t{
	GCT_CHAR name[32];
	GCT_CHAR ssid[32];
	GCT_WIFI_SIGNAL_LEVEL euGCT_WIFI_SIGNAL_LEVEL;
}gct_wifi_info;

typedef struct gct_wifi_config_req_t{
	GCT_CHAR name[32];			// goolink id
	GCT_CHAR ssid[32];			// ssid
	GCT_CHAR password[32];		// 密码
	GCT_UINT32 networkType;						//
	GCT_UINT32 encryptType;						// 加密类型
}gct_wifi_config_req;

typedef struct GLNK_DeviceStorageList2_t{
	GCT_INT32 StorageID;							//硬盘(sd卡)ID	第一块是1，第二块是2依次类推
	GCT_INT32 StorageCap;							//总存储容量(MB)
	GCT_INT32 StorageCapRemain;						//剩余容量(MB)
	GCT_INT32 reverse;								//保留位
}GLNK_DeviceStorageList2;

typedef struct GLNK_DeviceStorageResponse2_t{
	GCT_INT32 DeviceStorageListLen;							//列表的长度=sizeof(GLNK_DeviceStorageResponset) + sizeof(GLNK_DeviceStorageList)*ListNum,num为硬盘或sd卡的个数
	GLNK_DeviceStorageList2 DeviceStorageList[0];			//硬盘(sd卡)列表变长结构体
}GLNK_DeviceStorageResponse2;

typedef enum _GCT_TIME_SYN_TYPE{
	GCT_TIME_SYN_TYPE_SYN_ZONE_ZERO,	//设备系统的时区是0时区,设备本身的本地时间未必正确的,但是知道设备的时区(设备端对接着填入,或者APP对时时填入)
	GCT_TIME_SYN_TYPE_LOCALTIME,		//设备系统的时区是0时区,设备本身的本地时间是正确的，时区可以不知道(比如通过 NTP服务器【Network Time Protocol（NTP）】 对时)
	GCT_TIME_SYN_TYPE_SYN_ZONE_RIGHT,	//设备系统的时区是非0时区,是正确的时区(运行date -R指令可知道)
}GCT_TIME_SYN_TYPE;

typedef enum _GCT_LOG_LEVEL{
	GCT_LOG_LEVEL_UNKNOW,
	GCT_LOG_LEVEL_ERR		= 0x1<1,		//错误级别
	GCT_LOG_LEVEL_DEBUG		= 0x1<2,		//警告或者调试级别
}GCT_LOG_LEVEL;

typedef struct gct_innertest_svrstate_t{
	GCT_CHAR 	szLbsIp[256];
	GCT_INT32 	nLbsPort;
	GCT_BOOL 	bLbsConnOk;
	GCT_CHAR 	szGooserverIp[256];
	GCT_INT32 	nGooserverPort;
	GCT_BOOL 	bGooserverConnOk;
	GCT_CHAR 	szGoopsIp[256];
	GCT_INT32 	nGoopsPort;
	GCT_BOOL 	bGoopsConnOk;
	GCT_CHAR 	szGoostorageIp[256];
	GCT_INT32 	nGoostoragePort;
	GCT_BOOL 	bGoostorageConnOk;
}gct_innertest_svrstate;

#define GCT_NONE         "\033[m" 
#define GCT_LIGHT_RED    "\033[1;31m"

//********************** 时间同步 *********************/
GCT_VOID gct_common_time_syn_type(const GCT_TIME_SYN_TYPE euGCT_TIME_SYN_TYPE);
GCT_VOID gct_common_time_syn_timezone(const GCT_INT32 nTimeZone);
GCT_VOID gct_common_time_syn_timezone_get(GCT_INT32* pnTimeZone,GCT_BOOL* pbUpdate,GCT_UINT64* pnSvrTs);

//********************** 日志相关 ********************/
//日志开关 bOpen = TRUE打开,否则为关闭，默认是关闭
GCT_VOID gct_common_log_savelocal(const GCT_BOOL bOpen);

//日志保存的目录
//注册函数为查询目录是否正常，如果为sd卡目录等,则需要如实检查,如果是系统永久目录,则设置为正常即可
//return GCT_TRUE = 目录正常 GCT_FALSE = 目录异常
typedef GCT_BOOL (*fun_gcti_common_chk_log_savepath_normal)();
GCT_VOID gct_common_log_savepath(const GCT_CHAR* pPath,fun_gcti_common_chk_log_savepath_normal fun);

//**********************  其他共用接口 ********************/
//获取系统启动到现在的毫秒数
GCT_INT64 gct_common_get_sys_ms();
//休眠多少毫秒
//nSleepMs 毫秒
GCT_VOID gct_common_sleep_ms(const GCT_UINT32 nSleepMs);
//执行脚本指令
//pResult 需要自己定义，比如 GCT_CHAR szResult[1024] = {0}; ,不需要执行结果的,则填NULL
GCT_VOID gct_common_system_cmd(const GCT_CHAR* pCmd,GCT_CHAR* pResult);

//执行脚本
//pResult 需要自己定义，比如 GCT_CHAR szResult[1024] = {0}; ,不需要执行结果的,则填NULL
//bBlock，需要阻塞等待结果的 写GCT_TRUE,否则写GCT_FALSE
GCT_VOID gct_common_system_cmd_v2(const GCT_CHAR* pCmd,GCT_CHAR* pResult,const GCT_BOOL bBlock);

//md5校验
GCT_INT32 gct_common_md5_auth(const GCT_CHAR* pFilePath,GCT_CHAR* pMd5Result);

GCT_CHAR* gct_common_getlocaltime_str();

GCT_INT32 gct_log_print(const GCT_LOG_LEVEL euGCT_LOG_LEVEL,const GCT_CHAR* pFileName,const GCT_UINT32 nLineNum,const GCT_CHAR* pFunName, const GCT_CHAR *fmt, ...);

#ifndef _WIN32
#define gct_common_printf_error(fmt, arg...) \
		gct_log_print(GCT_LOG_LEVEL_ERR,__FILE__,__LINE__,__func__,fmt, ##arg);
						
#define gct_common_printf_debug(fmt, arg...) \
		gct_log_print(GCT_LOG_LEVEL_DEBUG,__FILE__,__LINE__,__func__,fmt, ##arg);
#else
#define gct_common_printf_error(fmt, ...) \
		gct_log_print(GCT_LOG_LEVEL_ERR,__FILE__,__LINE__,__func__,fmt, ##__VA_ARGS__);

#define gct_common_printf_debug(fmt, ...) \
		gct_log_print(GCT_LOG_LEVEL_DEBUG,__FILE__,__LINE__,__func__,fmt,##__VA_ARGS__);
#endif

//检测gid是否为随机
GCT_BOOL gct_common_chk_is_rand_gid(const GCT_CHAR* pGid);

//////////******************** 写入文件操作 (一般用于日志保存功能)************************/
typedef GCT_VOID (*Fun_gct_common_del_first_line)(const GCT_CHAR* pFileAbsPath);	//删除前面一行的指令
typedef GCT_INT32 (*Fun_gct_common_get_line_count)(const GCT_CHAR* pFileAbsPath);	//获取文件行数的指令
typedef struct gct_common_file_op_param_t{
	GCT_CHAR 	szFileAbsPath[512];		//文件的绝对路径
	GCT_INT32 	nMaxLineCount;			//最大行数
	GCT_INT32	nMaxCap;				//最大的容量(单位为字节)
	GCT_CHAR  	szBuff[1024];				//当次需要写入的内容(要包含换行符)
	Fun_gct_common_del_first_line fun_gct_common_del_first_line;	//如果不注册，则用库默认的指令 sed -i '1d' /XXXX/XXX.txt
	Fun_gct_common_get_line_count fun_gct_common_get_line_count;	//如果不注册，则用库默认的指令 wc -l /XXXX/XXX.txt
}gct_common_file_op_param;
//return  > 0 写入的字节数,否则为失败
GCT_INT32 gct_common_file_op_write(const gct_common_file_op_param file_op_param);

//nType 0 = open 1 = fopen 默认是0
GCT_VOID gct_common_file_op_type(const GCT_UINT32 nType);

/////////////// 室内机专用 begin
typedef struct gct_common_innerpad_padinfo_node_t{
	GCT_CHAR szGid[64];
	GCT_CHAR szDevName[64];
	GCT_CHAR szPasswd[64];
	GCT_BOOL bIsMain;			//是否是主次室内机
	struct gct_common_innerpad_padinfo_node_t* pNext;
}gct_common_innerpad_padinfo_node;

typedef struct gct_common_innerpad_locknode_t{
	GCT_CHAR szLockId[16]; 		//lock的id 比如"B03"
	GCT_CHAR szOtherName[64]; 	//锁自定义别名
	GCT_BOOL bEnAble;			//是否启用
	struct gct_common_innerpad_locknode_t* pNext;
}gct_common_innerpad_locknode;

typedef struct gct_common_innerpad_bind_devnode_t{
	GCT_CHAR szDevName[128];	//设备别名
	GCT_CHAR szUnitLabel[128];	//房间号
	GCT_CHAR szGid[64];			//绑定的设备id
	GCT_CHAR szPwd[64];			//登陆设备的密码
	GCT_CHAR szLockPasswd[64]; 	//开锁密码
	GCT_CHAR szRingTone[64]; 	//门钟提示音
	GCT_INT64 nVirNo;			//虚拟通道号
	gct_common_innerpad_locknode* pgct_common_innerpad_locknode_list; //锁列表
	struct gct_common_innerpad_bind_devnode_t* pNext;
}gct_common_innerpad_bind_devnode;

typedef struct gct_common_innerpad_devlistinfo_t{
	GCT_CHAR szMainAccount[128];
	gct_common_innerpad_padinfo_node* pgct_common_innerpad_padinfo_node_list;	//室内机列表(包括主次)
	gct_common_innerpad_bind_devnode* pgct_common_innerpad_bind_devnode_list;	//绑定的设备列表
}gct_common_innerpad_devlistinfo;
/////////////// 室内机专用 end

//bForce 1 = 强制重新解析，0 = 不强制，不读过期缓存,2 表示读取缓存，不管是不是在更新
#define  GCT_COMMON_DOMAIN_SET_READY_FALG_READ_IP			0
#define  GCT_COMMON_DOMAIN_SET_READY_FALG_FORCE_UPDATE		1
#define  GCT_COMMON_DOMAIN_SET_READY_FALG_READ_ALL_CACHE	2
GCT_VOID gct_common_domain_get_ready(const GCT_CHAR* pDomain,GCT_CHAR* pOutIp,const int bForce);
//域名解析
GCT_VOID gct_common_domain_get(const GCT_CHAR* pDomain,GCT_CHAR* pOutIp);
GCT_VOID gct_common_domain_save_result(const GCT_CHAR* pDomain,const GCT_CHAR* pIp);
GCT_BOOL gct_common_domain_getone_doit(GCT_CHAR* pDomain);
/// <summary>
/// 只是做过期标记，即使过期，如果DNS解释超时后，也会返回最后一次缓存
/// </summary>
GCT_VOID gct_common_domain_clear(const GCT_CHAR* pDomain);
GCT_VOID gct_common_domain_clearall();
GCT_VOID gct_common_release();

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
