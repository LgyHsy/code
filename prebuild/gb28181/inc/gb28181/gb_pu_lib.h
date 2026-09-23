#ifndef __MS_PU_LIB_H__
#define __MS_PU_LIB_H__

#include "gb_extend.h"

#ifndef MAX_CHANNEL_NUM
#define MAX_CHANNEL_NUM 64
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	//type 0=CMU 1=MDU
	typedef int (* net_register_proc )( int type, char* ip, unsigned short port, unsigned long error, unsigned long times, unsigned long param );
	//cmd=NET_MDU_STREAM  NET_MDU_STREAM* info = (NET_MDU_STREAM*)buf;
	typedef int (* net_msg_proc )(const char* cmd, char* buf, unsigned long* size, unsigned long param );
	typedef int (* net_alarm_proc )(const char* cmd );
	typedef int (* net_stream_proc )( unsigned long id, const char* cmd, char* buf, unsigned long size, unsigned long param );
	typedef int (* net_audio_proc)(unsigned long id, char *data, unsigned long size);

	typedef enum
	{
		GB_PLATFORM_GENERIC = 0,
		GB_PLATFORM_HI35XX = 10,
		GB_PLATFORM_HI3516A,
		GB_PLATFORM_HI3518EV200,
		GB_PLATFORM_HI3516CV300,
		GB_PLATFORM_DM365 = 20,
		GB_PLATFORM_AMBAR_S2L = 30,
		GB_PLATFORM_AMBAR_S2LM,
		GB_PLATFORM_AMBAR_S2LM2,
		GB_PLATFORM_MSTAR = 40,
		GB_PLATFORM_MSTAR_I5,
		GB_PLATFORM_MSTAR_I5_UCLIBC,
		GB_PLATFORM_MSTAR_I6E,
		GB_PLATFORM_MSTAR_620Q,
		GB_PLATFORM_MSTAR_621,
		GB_PLATFORM_MSTAR_650,
		GB_PLATFORM_MSTAR_931,
		GB_PLATFORM_INVALID
	} GB_PLATFORM_TYPE;

	/*目录信息结构体,暂时注掉为可选的选项*/
	typedef struct 
	{
		char deviceid[32];//系统ID
		char name[32];
		char manufacturer[32];
		char model[32];
		char owner[32];
		char civilcode[32];
		char block[32];
		char address[32];
		int parental;
		char parentld[32];
		// int safetyway;
		int registerway;//default  1
		char certnum[32];
		int certifiable;
		int errcode;
		char endtime[32];
		int secrecy;//default 0
		//  char ipaddress[128];
		//    int port;
		//   char password[128];
		char  status[4];
		double longitude;
		double latitude;
	}DEVICE_CATALOG_INFO;

	typedef struct 
	{
		char deviceid[32];
		char dutystatus[8];
		char status[4];
	} DEVICE_ARLARMSTATUS_INFO;

	typedef struct
	{
		char deviceid[32];
	} DEVICE_ALARMOUT_INFO;

	typedef struct 
	{
		//上级平台 信息
		int	hcPort;
		char	hcIp[16];
		char	hcName[128];
		char	hcPwd[32];
		char	hcId[32];

		//本代理网关的信息
		int		lcPort;
		char	lcIp[16];
		char	lcName[128];
		char	lcPwd[32];
		char	lcId[32];
		char lcUsername[32];

        char camId[32]; //Add by right. 2015/07/13
		int alarm_num;
		int channel_num;//通道数
		int alarmout_num;
		int alarm_timeinterval;

		DEVICE_CATALOG_INFO  channel_info[MAX_CHANNEL_NUM];//通道名称
		DEVICE_ARLARMSTATUS_INFO  alarmstatus_info[MAX_CHANNEL_NUM];
		DEVICE_ALARMOUT_INFO  alarmout_info[MAX_CHANNEL_NUM];

		int     keeplive;
		int     keeplivenum;
		int     expires;

		NET_SERVER_FTP_INFO ftp_info;

		int serial_port_num;
		NET_SERVER_SERIAL_PORT_INFO serial_port_info[16];
		NET_SERVER_TIME_CONFIG_INFO  server_time_config_info;

		NET_BASIC_DEVICE_INFO basic_device_info;

		char user_agent[32];

		char device_name[256];
		char host_name[256];
	}sip_net_info;

	/*
	typedef struct 
	{
		//上级平台 信息
		char	ServerIP[16];
		int	ServerPort;
		char	ServerID[128];
		char	ServerDomain[32];
		
		//本代理网关的信息
		char	DeviceIP[16];
		int	DevicePort;
		char	DeviceID[128];
		char DeviceDomain[32];
		char	Username[32];
		char	Password[32];

		int alarm_num;
		int channel_num;//通道数
		int alarmout_num;

		DEVICE_CATALOG_INFO  **channel_info;//通道名称
		DEVICE_ARLARMSTATUS_INFO  **alarmstatus_info;
		DEVICE_ALARMOUT_INFO  **alarmout_info;

		int     keeplive;
		int     keeplivenum;
		int     expires;
	}sip_net_info;
	*/
	typedef struct  
	{
		sip_net_info* sip_net;
	} net_info_t;

	/* 运行时状态类型（按通道） */
	typedef enum NET_RUNTIME_STATUS_TYPE
	{
		NET_RUNTIME_STATUS_BREAK_LIVE = 0,
		NET_RUNTIME_STATUS_BREAK_PLAYBACK,
		NET_RUNTIME_STATUS_BREAK_DOWNLOAD,

		NET_RUNTIME_STATUS_ACTIVE_LIVE,
		NET_RUNTIME_STATUS_ACTIVE_PLAYBACK,
		NET_RUNTIME_STATUS_ACTIVE_DOWNLOAD,

		NET_RUNTIME_STATUS_SENDVIDEO_LIVE,
		NET_RUNTIME_STATUS_SENDVIDEO_PLAYBACK,
		NET_RUNTIME_STATUS_SENDVIDEO_DOWNLOAD,
	} NET_RUNTIME_STATUS_TYPE;

	/* 视频编码类型（对应PSM stream_type） */
	typedef enum NET_VIDEO_CODEC_TYPE
	{
		NET_VIDEO_CODEC_H264 = 0x1B,
		NET_VIDEO_CODEC_H265 = 0x24
	} NET_VIDEO_CODEC_TYPE;

	/* 音频编码类型（对应PSM stream_type） */
	typedef enum NET_AUDIO_CODEC_TYPE
	{
		NET_AUDIO_CODEC_AAC = 0x0F,
		NET_AUDIO_CODEC_G711A = 0x90,
		NET_AUDIO_CODEC_G711U = 0x91
	} NET_AUDIO_CODEC_TYPE;

	/* 媒体编码配置（按通道） */
	typedef struct NET_MEDIA_CONFIG
	{
		NET_VIDEO_CODEC_TYPE video_codec;
		NET_AUDIO_CODEC_TYPE audio_codec;
	} NET_MEDIA_CONFIG;

	/* 获取/设置运行时状态（按通道）。开关类严格只允许 0/1，AUDIO_STREAM_TYPE 允许 0~255。 */
	int net_get_runtime_status(int channel, NET_RUNTIME_STATUS_TYPE type, int* value);
	int net_set_runtime_status(int channel, NET_RUNTIME_STATUS_TYPE type, int value);
	int net_get_media_config(int channel, NET_MEDIA_CONFIG* config);
	int net_set_media_config(int channel, const NET_MEDIA_CONFIG* config);

	int net_initlib(net_info_t info, net_register_proc reg_proc, net_msg_proc msg_proc, net_stream_proc stream_proc, net_audio_proc audio_proc, unsigned long param,int		udp_tcp);

	int net_fililib();
	int net_updata_channel(DEVICE_CATALOG_INFO		*info);

	int net_get_version(unsigned long* version);

	int net_set_platform(GB_PLATFORM_TYPE model);
	int net_get_platform(GB_PLATFORM_TYPE* model);
	int net_get_platform_channel_limit(GB_PLATFORM_TYPE model);
	
	int net_send_alarm(int ch, int alarmtype);

	/* stream_mode: 0 实时流, 1 回放; stream_type: 0 音频, 1 视频; timestamp 单位 ms */
	int net_send_stream(unsigned long id, char* buf, unsigned long size, unsigned long long timestamp, int i_flag, int stream_mode, int stream_type);
	
	int gb_file_to_end(unsigned long stream_id, int channel);

	int net_set_video_framerate(unsigned long id, int framerate);

	int gb_get_status();

	

#ifdef __cplusplus
}
#endif

#endif//__MS_PU_LIB_H__
