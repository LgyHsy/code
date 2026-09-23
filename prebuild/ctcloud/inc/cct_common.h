#ifndef __CCT_COMMON_H__
#define __CCT_COMMON_H__
#include <stdio.h>
#include <stdint.h>
#include "cct_types.h"

#ifdef __cplusplus
extern "C" {
#endif





typedef struct cct_param_t{
	CCT_CHAR	szGid[128];				//设备id，唯一，每个设备只能填死一个
	CCT_CHAR 	szModel[128];		//设备型号
	CCT_CHAR 	szVersion[128];		    //固件版本号
	CCT_UINT32	nChannelCount;			//通道数为1的话，默认为ipc 大于1则为DVR或者NVR
	CCT_UINT32 	bEnableCloud;			//需要设备库支持的模块
	CCT_CHAR 	szLocalCfgFilePath[64]; //pPath 一定要存在,如果不存在则先创建再调用这个接口,这个目录要求重启后不能删除
	CCT_CHAR	szSdcardAbsPath[128];	//sdcard的绝对路径 比如"/mnt"
}cct_param;

typedef enum _CCT_VIDEO_CODEC_TYPE{
	CCT_VIDEO_CODEC_TYPE_H264		= 0x34363248,	//H264
	CCT_VIDEO_CODEC_TYPE_MJPEG		= 0x45464758,  //mjpeg
	CCT_VIDEO_CODEC_TYPE_H265		= 0x56565268,	//H265
}CCT_VIDEO_CODEC_TYPE;

typedef enum _CCT_AUDIO_CODEC_TYPE{
	CCT_AUDIO_CODEC_TYPE_G711A	= 0x7A19,
	CCT_AUDIO_CODEC_TYPE_G711U  = 0x7A25,
	CCT_AUDIO_CODEC_TYPE_AAC    = 0x7A26,
	CCT_AUDIO_CODEC_TYPE_MP3    = 0x7A27,
	CCT_AUDIO_CODEC_TYPE_PCM	= 0x7A28,
}CCT_AUDIO_CODEC_TYPE;

//视频数据格式
typedef struct cct_video_data_format_t{
	CCT_VIDEO_CODEC_TYPE 		euCCT_VIDEO_CODEC_TYPE;			//编码方式
	CCT_UINT32 					bitrate;        				//比特率, bps
	CCT_UINT16 					width;							//图像宽度
	CCT_UINT16 					height;							//图像高度
	CCT_UINT8 					framerate;						//帧率, fps
	CCT_UINT8 					frameInterval;   				//I帧间隔
	CCT_UINT8 					reserve;		
} cct_video_data_format;

//音频数据格式
typedef struct cct_audio_data_format_t{
	CCT_UINT32 					samplesRate;					//每秒采样
	CCT_UINT32 					bitrate;						//比特率, bps
	CCT_AUDIO_CODEC_TYPE 		euCCT_AUDIO_CODEC_TYPE;			//编码格式
	CCT_UINT16 					channelNumber;					//音频通道号
	CCT_UINT16 					bitsPerSample;					//每采样比特数（一般是16）
	CCT_UINT16 					reserve;
} cct_audio_data_format;

typedef struct cct_stream_data_format_t{
	cct_video_data_format videoFormat;
	cct_audio_data_format audioFormat;
} cct_stream_data_format;

///////// 告警业务相关 /////////////////////////////////
typedef struct cct_push_alarm_info_t{
    CCT_UINT32	nAlarmType;		// 报警类型 自定义类型，从0开始
    CCT_INT32   nChannelNo;		// 通道号		如果不需要显示通道号，则填-1即可，正常的通道号是从0开始。
	CCT_INT64   nVirNo;			// 虚拟通道号
	CCT_CHAR 	szCustomBuf[56];		// 自定义报警文本，如果是CCT_ALARM_TYPE填CCT_CUSTOM类型，则这个字段有效
	CCT_BOOL 	bUploadCloud;
} cct_push_alarm_info;

typedef struct cct_push_alarm_ext_info_t{
   cct_push_alarm_info 	cct_alarm_info;
   CCT_CHAR 			szTimeStr[128];
   CCT_CHAR 			szCloudPicPath[256];		//云存储需要
} cct_push_alarm_ext_info;

typedef enum _CCT_TIME_SYN_TYPE{
	CCT_TIME_SYN_TYPE_SYN_ZONE_ZERO,	//设备系统的时区是0时区,设备本身的本地时间未必正确的,但是知道设备的时区(设备端对接着填入,或者APP对时时填入)
	CCT_TIME_SYN_TYPE_LOCALTIME,		//设备系统的时区是0时区,设备本身的本地时间是正确的，时区可以不知道(比如通过 NTP服务器【Network Time Protocol（NTP）】 对时)
	CCT_TIME_SYN_TYPE_SYN_ZONE_RIGHT,	//设备系统的时区是非0时区,是正确的时区(运行date -R指令可知道)
}CCT_TIME_SYN_TYPE;

typedef enum _CCT_LOG_LEVEL{
	CCT_LOG_LEVEL_UNKNOW,
	CCT_LOG_LEVEL_ERR		= 0x1<1,		//错误级别
	CCT_LOG_LEVEL_DEBUG		= 0x1<2,		//警告或者调试级别
}CCT_LOG_LEVEL;

#define CCT_NONE         "\033[m" 
#define CCT_LIGHT_RED    "\033[1;31m"

//********************** 时间同步 *********************/
CCT_VOID cct_common_time_syn_type(const CCT_TIME_SYN_TYPE euCCT_TIME_SYN_TYPE);
CCT_VOID cct_common_time_syn_timezone(const CCT_INT32 nTimeZone);
CCT_VOID cct_common_time_syn_timezone_get(CCT_INT32* pnTimeZone,CCT_BOOL* pbUpdate,CCT_UINT64* pnSvrTs);

//********************** 日志相关 ********************/
//日志开关 bOpen = TRUE打开,否则为关闭，默认是关闭
CCT_VOID cct_common_log_savelocal(const CCT_BOOL bOpen);

//日志保存的目录
//注册函数为查询目录是否正常，如果为sd卡目录等,则需要如实检查,如果是系统永久目录,则设置为正常即可
//return CCT_TRUE = 目录正常 CCT_FALSE = 目录异常
typedef CCT_BOOL (*fun_gcti_common_chk_log_savepath_normal)();
CCT_VOID cct_common_log_savepath(const CCT_CHAR* pPath,fun_gcti_common_chk_log_savepath_normal fun);

//**********************  其他共用接口 ********************/
//获取系统启动到现在的毫秒数
CCT_INT64 cct_common_get_sys_ms();
//休眠多少毫秒
//nSleepMs 毫秒
CCT_VOID cct_common_sleep_ms(const CCT_UINT32 nSleepMs);
//执行脚本指令
//pResult 需要自己定义，比如 CCT_CHAR szResult[1024] = {0}; ,不需要执行结果的,则填NULL
CCT_VOID cct_common_system_cmd(const CCT_CHAR* pCmd,CCT_CHAR* pResult);

//执行脚本
//pResult 需要自己定义，比如 CCT_CHAR szResult[1024] = {0}; ,不需要执行结果的,则填NULL
//bBlock，需要阻塞等待结果的 写CCT_TRUE,否则写CCT_FALSE
CCT_VOID cct_common_system_cmd_v2(const CCT_CHAR* pCmd,CCT_CHAR* pResult,const CCT_BOOL bBlock);

//md5校验
CCT_INT32 cct_common_md5_auth(const CCT_CHAR* pFilePath,CCT_CHAR* pMd5Result);

CCT_CHAR* cct_common_getlocaltime_str();

CCT_INT32 cct_log_print(const CCT_LOG_LEVEL euCCT_LOG_LEVEL,const CCT_CHAR* pFileName,const CCT_UINT32 nLineNum,const CCT_CHAR* pFunName, const CCT_CHAR *fmt, ...)__attribute__((format(printf,5,6)));

#define cct_common_printf_error(fmt, arg...) \
		cct_log_print(CCT_LOG_LEVEL_ERR,__FILE__,__LINE__,__func__,fmt, ##arg);
						
#define cct_common_printf_debug(fmt, arg...) \
		cct_log_print(CCT_LOG_LEVEL_DEBUG,__FILE__,__LINE__,__func__,fmt, ##arg);

//检测gid是否为随机
CCT_BOOL cct_common_chk_is_rand_gid(const CCT_CHAR* pGid);

//////////******************** 写入文件操作 (一般用于日志保存功能)************************/
typedef CCT_VOID (*Fun_cct_common_del_first_line)(const CCT_CHAR* pFileAbsPath);	//删除前面一行的指令
typedef CCT_INT32 (*Fun_cct_common_get_line_count)(const CCT_CHAR* pFileAbsPath);	//获取文件行数的指令
typedef struct cct_common_file_op_param_t{
	CCT_CHAR 	szFileAbsPath[512];		//文件的绝对路径
	CCT_INT32 	nMaxLineCount;			//最大行数
	CCT_INT32	nMaxCap;				//最大的容量(单位为字节)
	CCT_CHAR  	szBuff[1024];				//当次需要写入的内容(要包含换行符)
	Fun_cct_common_del_first_line fun_cct_common_del_first_line;	//如果不注册，则用库默认的指令 sed -i '1d' /XXXX/XXX.txt
	Fun_cct_common_get_line_count fun_cct_common_get_line_count;	//如果不注册，则用库默认的指令 wc -l /XXXX/XXX.txt
}cct_common_file_op_param;
//return  > 0 写入的字节数,否则为失败
CCT_INT32 cct_common_file_op_write(const cct_common_file_op_param file_op_param);

//nType 0 = open 1 = fopen 默认是0
CCT_VOID cct_common_file_op_type(const CCT_UINT32 nType);


//域名解析
//bForce true = 强制重新解析，false = 不强制
CCT_VOID cct_common_domain_set_ready(const CCT_CHAR* pDomain,CCT_CHAR* pOutIp,const CCT_BOOL bForce);
CCT_VOID cct_common_domain_save_result(const CCT_CHAR* pDomain,const CCT_CHAR* pIp);
CCT_BOOL cct_common_domain_getone_doit(CCT_CHAR* pDomain);
CCT_VOID cct_common_release();

CCT_VOID cct_common_set_aov_flag();

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif
