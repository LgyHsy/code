#ifndef _CCT_DD_API_H__
#define _CCT_DD_API_H__
#include "cct_common.h"
#include "cct_types.h"

/*		报警类型		*/
typedef enum _CCT_ALARM_TYPE{
	CCT_ALARM_TYPE_NONE			= -1,		//无告警类型
	CCT_VIDEO_FRAME    			= 0,       	// IP地址冲突报警
	CCT_DEVICE_RESTART 			= 1,       	// 设备重启
	CCT_MOTION_DETECT  			= 2,       	// 移动侦测报警
	CCT_VIDEO_LOSS    			= 3,       	// 视频丢失
	CCT_VIDEO_SHEILD      		= 4,       	// 视频遮挡报警
	CCT_BORDER_DETECT   		= 5,       	// 越界侦测报警
	CCT_ZONE_INTRUSION       	= 6,       	// 区域入侵报警
	CCT_SWITCH_SENSOR  			= 7,       	// 开关量探头报警
	CCT_PIR_ALARM      			= 8,       	// 红外报警
	CCT_SMOKE_ALARM   			= 9,       	// 烟雾报警
	CCT_NOISE_ALARM    			= 10,      	// 噪声报警
	CCT_TEMPERATURE_ALARM    	= 11,      	// 温度异常报警
	CCT_HUMIDITY_ALARM          = 12,      	// 湿度异常报警
	CCT_GAS_ALARM       		= 13,      	// 气体报警
	CCT_CALLING_ALARM  			= 14,	  	// 门铃呼叫
	CCT_SDCARDERROR_ALARM		= 15,		// SD卡/硬盘异常报警
	CCT_SDCARDOUT_ALARM 		= 16, 		// SD卡/硬盘拔出报警
	CCT_SDCARDFULL_ALARM 		= 17, 		// SD卡/硬盘容量满报警
	CCT_DEVICEMOVE_ALARM 		= 18, 		// 设备移动报警
	CCT_ENERGYREMOVE_ALARM 		= 19, 		// 电源拔出报警
	CCT_EXTERNALPOWER_ALARM 	= 20, 		// 外部电源输入报警
	CCT_LOWPOWER_ALARM 			= 21, 		// 低电量报警
	CCT_IO_ALARM 				= 22,  		// I/O报警
	CCT_RINGING_ALARM			= 25,		// 有人呼叫您
	CCT_CALLANSWERED_ALARM		= 26,		// 呼叫已接听
	CCT_DEV_CLOSE	            = 27,		// 设备关机
	CCT_BATTERY_FULL		    = 28,		// 电池电量已满
	//PAT_XX		                = 29,		// 布防
	//PAT_XX		                = 30,		// 撤防
	//PAT_XX		                = 31,		// 留守
	CCT_HUMANOID_FRAME          = 32,		// 人形框
	CCT_DEVICE_UNBIND            = 33,		// 设备复位解绑
	CCT_DOORBELL_CALLING		= 34,		// 门铃设备正在通话中
	CCT_DOORBELL_OPENLOCK		= 35,		// 门口机开锁了
	CCT_DOORBELL_CALL			= 36,		// 门铃呼叫
	CCT_DOORBELL_HANGUP			= 37,		// 呼叫中,设备主动挂断(即未接听事件)
	CCT_LEAVE_WORD		        = 40,		// 留言告警
	CCT_AI_CAR = 41,				//汽车
	CCT_AI_MOTO = 42,				//摩托车
	CCT_AI_ELECTRICBICYCLE = 43,				//电单车
	CCT_AI_BICYCLE = 44,				//自行车
	CCT_AI_GASTANK = 45,				//煤气罐
	CCT_AI_Lpr = 46,				//车牌
	CCT_AI_FD=47,				//FACE DETE
	CCT_AI_REGION_DETECT_ENTER = 48,				//区域侦测进入
	CCT_AI_REGION_DETECT_LEAVE   = 49,				//区域侦测离开
	CCT_AI_REGION_DETECT_STAY = 50,				//区域侦测逗留
	CCT_AI_FALLINGOBJECT = 51, 			//高空抛物
	CCT_AI_FALLHUMAN = 52,	//跌倒
	CCT_DOORBELL_ALAEM			= 100,  	// 门口机来电
	CCT_CUSTOM					= 999,		// 自定义告警内容
} CCT_ALARM_TYPE;

#ifdef __cplusplus
extern "C" {
#endif

//推送告警
//nChannelNo 通道号，从0开始
//nAlarmType 告警类型,自定义,目前告警服务器已经支持的告警类型请参考 CCT_ALARM_TYPE
//bUploadCloud  CCT_TRUE = 该报警上传云存储 ,CCT_FALSE = 该报警不上传云存储 ,该字段对于报警套餐和图片套餐有效
//pSnapJpgBuf: 已抓图数据内存指针
//nSnapJpgBufLen: 已抓图数据长度
CCT_VOID cct_svr_api_push_alarm(const CCT_UINT32 nChannelNo,const CCT_UINT32 nAlarmType,const CCT_BOOL bUploadCloud, const CCT_CHAR * pSnapJpgBuf, const CCT_UINT32 nSnapJpgBufLen);
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

