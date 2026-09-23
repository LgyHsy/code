#ifndef _GCT_DD_APIV4_H__
#define _GCT_DD_APIV4_H__
#include "gct_common.h"
#include "gct_types.h"

/*		报警类型		*/
typedef enum _GCT_ALARM_TYPE{
	GAT_ALARM_TYPE_NONE			= -1,		//无告警类型
	GAT_VIDEO_FRAME    			= 0,       	// IP地址冲突报警
	GAT_DEVICE_RESTART 			= 1,       	// 设备重启
	GAT_MOTION_DETECT  			= 2,       	// 移动侦测报警
	GAT_VIDEO_LOSS    			= 3,       	// 视频丢失
	GAT_VIDEO_SHEILD      		= 4,       	// 视频遮挡报警
	GAT_BORDER_DETECT   		= 5,       	// 越界侦测报警
	GAT_ZONE_INTRUSION       	= 6,       	// 区域入侵报警
	GAT_SWITCH_SENSOR  			= 7,       	// 开关量探头报警
	GAT_PIR_ALARM      			= 8,       	// 红外报警
	GAT_SMOKE_ALARM   			= 9,       	// 烟雾报警
	GAT_NOISE_ALARM    			= 10,      	// 噪声报警
	GAT_TEMPERATURE_ALARM    	= 11,      	// 温度异常报警
	GAT_HUMIDITY_ALARM          = 12,      	// 湿度异常报警
	GAT_GAS_ALARM       		= 13,      	// 气体报警
	GAT_CALLING_ALARM  			= 14,	  	// 门铃呼叫
	GAT_SDCARDERROR_ALARM		= 15,		// SD卡/硬盘异常报警
	GAT_SDCARDOUT_ALARM 		= 16, 		// SD卡/硬盘拔出报警
	GAT_SDCARDFULL_ALARM 		= 17, 		// SD卡/硬盘容量满报警
	GAT_DEVICEMOVE_ALARM 		= 18, 		// 设备移动报警
	GAT_ENERGYREMOVE_ALARM 		= 19, 		// 电源拔出报警
	GAT_EXTERNALPOWER_ALARM 	= 20, 		// 外部电源输入报警
	GAT_LOWPOWER_ALARM 			= 21, 		// 低电量报警
	GAT_IO_ALARM 				= 22,  		// I/O报警
	GAT_RINGING_ALARM			= 25,		// 有人呼叫您
	GAT_CALLANSWERED_ALARM		= 26,		// 呼叫已接听
	GAT_DEV_CLOSE	            = 27,		// 设备关机
	GAT_BATTERY_FULL		    = 28,		// 电池电量已满
	//PAT_XX		                = 29,		// 布防
	//PAT_XX		                = 30,		// 撤防
	//PAT_XX		                = 31,		// 留守
	GAT_HUMANOID_FRAME          = 32,		// 人形框
	GAT_DEVICE_UNBIND            = 33,		// 设备复位解绑
	GAT_DOORBELL_CALLING		= 34,		// 门铃设备正在通话中
	GAT_DOORBELL_OPENLOCK		= 35,		// 门口机开锁了
	GAT_DOORBELL_CALL			= 36,		// 门铃呼叫
	GAT_DOORBELL_HANGUP			= 37,		// 呼叫中,设备主动挂断(即未接听事件)
	GAT_LEAVE_WORD		        = 40,		// 留言告警
	GAT_AI_CAR = 41,				//汽车
	GAT_AI_MOTO = 42,				//摩托车
	GAT_AI_ELECTRICBICYCLE = 43,				//电单车
	GAT_AI_BICYCLE = 44,				//自行车
	GAT_AI_GASTANK = 45,				//煤气罐
	GAT_AI_Lpr = 46,				//车牌
	GAT_AI_FD=47,				//FACE DETE
	GAT_AI_REGION_DETECT_ENTER = 48,				//区域侦测进入
	GAT_AI_REGION_DETECT_LEAVE   = 49,				//区域侦测离开
	GAT_AI_REGION_DETECT_STAY = 50,				//区域侦测逗留
	GAT_AI_FALLINGOBJECT = 51, 			//高空抛物
	GAT_AI_FALLHUMAN = 52,	//跌倒
	GAT_DOORBELL_ALAEM			= 100,  	// 门口机来电
	GAT_CUSTOM_LEVEL1 = 994,		// 自定义告警内容
	GAT_CUSTOM_LEVEL2 = 995,		// 自定义告警内容
	GAT_CUSTOM_LEVEL3 = 996,		// 自定义告警内容
	GAT_CUSTOM_LEVEL4 = 997,		// 自定义告警内容
	GAT_CUSTOM_LEVEL5 = 998,		// 自定义告警内容
	GAT_CUSTOM					= 999,		// 自定义告警内容
} GCT_ALARM_TYPE;

#ifdef __cplusplus
extern "C" {
#endif

//推送告警
//nChannelNo 通道号，从0开始
//nAlarmType 告警类型,自定义,目前告警服务器已经支持的告警类型请参考 GCT_ALARM_TYPE
//bUploadCloud  GCT_TRUE = 该报警上传云存储 ,GCT_FALSE = 该报警不上传云存储 ,该字段对于报警套餐和图片套餐有效
//pSnapJpgBuf: 已抓图数据内存指针
//nSnapJpgBufLen: 已抓图数据长度
GCT_VOID gct_dd_apiv4_push_alarm(const GCT_UINT32 nChannelNo,const GCT_UINT32 nAlarmType,const GCT_BOOL bUploadCloud, const GCT_CHAR * pSnapJpgBuf, const GCT_UINT32 nSnapJpgBufLen);
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

