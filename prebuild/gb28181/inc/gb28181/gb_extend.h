#ifndef _GB_EXTEND_H_
#define _GB_EXTEND_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
	char DeviceType[32];		//设备类型，IPC、DVR等
	char Manufacturer[32];	//厂商名称
	char Model[32];			//设备型号
	char Firmware[32];		//设备固件版本号
	int MaxCamera;
	int MaxAlarm;
} NET_BASIC_DEVICE_INFO, *LPNET_BASIC_DEVICE_INFO;

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
#define SET_PU_GATEWAY							"GatewaySetting"			//网关设置
#define QUERY_PU_GATEWAY_INFO						"QueryGatewayInfo"		//网关信息查询
typedef struct
{
	char DeviceID[32];
	char Enable[4];
	char ServerIP[16];
	int ServerPort;
	int DevicePort;
	int Expire;
	int HeartBeat;
	char ServerID[32];
	char ServerDomain[32];
	char Username[32];
	char Password[32];
} NET_SERVER_GATEWAY_INFO, *LPNET_SERVER_GATEWAY_INFO;

#define SET_PU_CATALOG							"CatalogSetting"		//目录设置
typedef struct
{
	int Channel;
	char DeviceID[32];
	char Owner[32];
	char CivilCode[32];
	char Block[32];
	char Address[32];
	int Secrecy;
	double Longitude;
	double Latitude;
} NET_SERVER_CATALOG_INFO, *LPNET_SERVER_CATALOG_INFO;

#define OPER_PU_SNAPSHOT							"Snapshot"
typedef struct
{
	int Channel;
	char Picture[1<<19];
	int PictureSize;
} NET_SERVER_SNAPSHOT_INFO, *LPNET_SERVER_SNAPSHOT_INFO;

#define SET_PU_FTP							"FTPSetting"			//FTP设置
#define QUERY_PU_FTP_INFO							"QueryFTPInfo"		//FTP配置查询
typedef struct
{
	char Ftp[128];
	int Port;
	char Ftpuser[16];
	char Ftppassword[16];
	char Path[128];
} NET_SERVER_FTP_INFO, *LPNET_SERVER_FTP_INFO;

#define SET_PU_SERIAL_PORT							"SerialPortSetting"			//串口设置
#define QUERY_PU_SERIAL_PORT_INFO							"QuerySerialPortInfo"		//串口配置查询
typedef struct
{
	int SerialPort;
	int BandRate;
	int DataBit;
	int Parity;
	int StopBit;
	int FlowControl;
	char Mode[8];
} NET_SERVER_SERIAL_PORT_INFO, *LPNET_SERVER_SERIAL_PORT_INFO;

#define SET_PU_TIME_CONFIG							"TimeConfigSetting"			//时间配置设置
#define QUERY_PU_TIME_CONFIG_INFO							"QueryTimeConfigInfo"		//时间配置查询
typedef struct
{
	int Mode;
	char NTPServer[128];
	int NTPPort;
	int NTPInterval;
} NET_SERVER_TIME_CONFIG_INFO, *LPNET_SERVER_TIME_CONFIG_INFO;

#define SET_PU_NETWORK										"NetworkSetting"			//网络信息设置
#define QUERY_PU_NETWORK_INFO							"QueryNetworkInfo"		//网络信息查询
typedef struct
{
	int DHCP;
	char IPAddress[16];
	char SubMask[16];
	char DefaultGateway[16];
	char DNSPrimary[16];
	char DNSSecondary[16];
	char PPPOEUsername[16];
	char PPPOEPassword[16];
} NET_SERVER_NETWORK_INFO, *LPNET_SERVER_NETWORK_INFO;

#define SET_PU_ALARM										"AlarmSetting"			//报警参数设置
#define QUERY_PU_ALARM_INFO							"QueryAlarmInfo"		//报警参数查询
typedef struct
{
	int x;
	int y;
	int w;
	int h;
} NET_SERVER_ALARM_RECT, *LPNET_SERVER_ALARM_RECT;
typedef struct
{
	int Channel;
	char AlarmType[16];
	int Enable;
	int DiskVolume;
	NET_SERVER_ALARM_RECT AlarmRect;
} NET_SERVER_ALARM_INFO, *LPNET_SERVER_ALARM_INFO;

#define SET_PU_DISPLAY										"DisplaySetting"			//显示参数设置
#define QUERY_PU_DISPLAY_INFO							"QueryDisplayInfo"		//显示参数查询
typedef struct
{
	int Channel;
	int Contrast;
	int Bright;
	int Hue;
	int Saturation;
} NET_SERVER_DISPLAY_INFO, *LPNET_SERVER_DISPLAY_INFO;

#define SET_PU_TEXT										"TextSetting"			//文字参数设置
#define QUERY_PU_TEXT_INFO							"QueryTextInfo"		//文字参数查询
typedef struct
{
	int Channel;
	int EnableTime;
	int EnableText;
	int TimeX;
	int TimeY;
	char Text[256];
	int TextX;
	int TextY;
} NET_SERVER_TEXT_INFO, *LPNET_SERVER_TEXT_INFO;

#define SET_PU_STORAGE_SCHEMA							"StorageSchemaSetting"			//存储策略设置
#define QUERY_PU_STORAGE_SCHEMA_INFO					"QueryStorageSchemaInfo"		//存储策略查询
typedef struct
{
	int StartHour;
	int StartMinute;
	int EndHour;
	int EndMinute;
} NET_SERVER_PERIOD, *LPNET_SERVER_PERIOD;
typedef struct
{
	int Num;
	NET_SERVER_PERIOD Period[4];
} NET_SERVER_SCHEMA, *LPNET_SERVER_SCHEMA;
typedef struct
{
	NET_SERVER_SCHEMA Schema[7];
} NET_SERVER_SCHEMA_LIST, *LPNET_SERVER_SCHEMA_LIST;
typedef struct
{
	int Channel;
	char Policy[8];
	int Content;
	int Status;
	int PreRecordTime;
	int DelayRecordTime;
	NET_SERVER_SCHEMA_LIST SchemaList;
} NET_SERVER_STORAGE_SCHEMA_INFO, *LPNET_SERVER_STORAGE_SCHEMA_INFO;

#define SET_PU_CHANNEL										"ChannelSetting"			//通道状态设置
#define QUERY_PU_CHANNEL_INFO							"QueryChannelInfo"		//通道状态查询
typedef struct
{
	int Type;	//0视频通道，1报警输入通道
	int Channel;
	int Status;
} NET_SERVER_CHANNEL_INFO, *LPNET_SERVER_CHANNEL_INFO;

#define SET_PU_AUDIO_ENCODER										"AudioEncoderSetting"	//音频编码参数设置
#define QUERY_PU_AUDIO_ENCODER_INFO							"QueryAudioEncoderInfo"	//音频编码参数查询
typedef struct
{
	int Channel;
	int EncodeMode;	//0：G.711 A 律
					//1：G.723.1
					//2：G.729
					//3：G.722.1
					//4：SVAC 音频
} NET_SERVER_AUDIO_ENCODER_INFO, *LPNET_SERVER_AUDIO_ENCODER_INFO;

#define SET_PU_VIDEO_ENCODER										"VideoEncoderSetting"	//视频编码参数设置
#define QUERY_PU_VIDEO_ENCODER_INFO							"QueryVideoEncoderInfo"	//视频编码参数查询
typedef struct
{
	int Channel;
	int StreamType;	//0：子码流，1：主码流
	int Quality;	//0：最好，1：次好，2：较好，3：一般，4：较差，5：差
	int EncodeMode;	//0：H.264，1：MPEG-4，2：SVAC
	int BitRate;
	int RateType;	//0：固定码率，1：可变码率
	int FrameRate;
	int ImageSize;	//0:HD1080P(1920*1080)
				//1:HD720P(1280*720)
				//2:XGA(1024*768)
				//3:SVGA(800*600)
				//4:VGA(640*480)
				//5:DCIF(704*576)
				//6:CIF(352*288)
				//7:QCIF(176*144)
} NET_SERVER_VIDEO_ENCODER_INFO, *LPNET_SERVER_VIDEO_ENCODER_INFO;

#define UPLOAD_CHANNEL_PROC										"UploadChannelProc" // 上报通道信息

#define QUERY_PU_PRESET_LIST							"QueryPuPresetList"	// 云台预置点列表查询（PresetQuery）

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

