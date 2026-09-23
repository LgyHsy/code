#ifndef _GCT_SDCARD_PLAYBACK_CALLBACK_H__
#define _GCT_SDCARD_PLAYBACK_CALLBACK_H__
#include "gct_common.h"
#include <stdbool.h>
#include "gct_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//搜索录像回调
typedef struct gct_sdcardplayback_callback_search_rsp_param_t{
	GCT_UINT32	nChannelNo; 	//通道号 从0开始
	GCT_UINT8 	nBegHour;		//时，设备本地时间
	GCT_UINT8 	nBegMin;		//分，设备本地时间
	GCT_UINT8 	nBegSec;		//秒，设备本地时间
	GCT_UINT8 	nEndHour;		//时，设备本地时间
	GCT_UINT8 	nEndMin;		//分，设备本地时间
	GCT_UINT8 	nEndSec;		//秒，设备本地时间
	struct gct_sdcardplayback_callback_search_rsp_param_t* pNext;
}gct_sdcardplayback_callback_search_rsp_param;
typedef struct gct_sdplayback_callback_search_req_param_t{
	GCT_INT32 nChannelNo;	//通道号 从0开始
	GCT_INT32 nYear;		//年，设备本地时间
	GCT_INT32 nMonth;		//月，设备本地时间
	GCT_INT32 nDay;			//日，设备本地时间
}gct_sdplayback_callback_search_req_param;
//search_req_param 请求的参数
//ppsearch_rsp_param_list 上层malloc,设备库释放
typedef GCT_VOID (*Fun_Glnk_SdcardPlayback_Search_CallBack)(const gct_sdplayback_callback_search_req_param search_req_param,gct_sdcardplayback_callback_search_rsp_param** ppsearch_rsp_param_list);

typedef struct gct_sdcardplayback_callback_startplay_param_t{
	GCT_UINT32 nSessionId; 		//唯一性，每个连接的唯一值 上层需要记录一下这个值,用于推流或者识别其他的回调是否同一个连接
	GCT_INT32 nChannelNo;		//通道号 从0开始
	GCT_INT32 nYear;			//年，设备本地时间
	GCT_INT32 nMonth;			//月，设备本地时间
	GCT_INT32 nDay;				//日，设备本地时间
	GCT_INT32 nHour;			//时，设备本地时间
	GCT_INT32 nMin;				//分，设备本地时间
	GCT_INT32 nSec;				//秒，设备本地时间
}gct_sdcardplayback_callback_startplay_param;
//开始播放
typedef GCT_VOID (*Fun_Glnk_SdcardPlayback_StartPlay_CallBack)(const gct_sdcardplayback_callback_startplay_param startplay_param);

typedef struct gct_sdcardplayback_callback_seekto_param_t{
	GCT_UINT32 nSessionId; 		//唯一性，每个连接的唯一值 Fun_Glnk_SdcardPlayback_StartPlay_CallBack回调
	GCT_INT32 nYear;			//年，设备本地时间
	GCT_INT32 nMonth;			//月，设备本地时间
	GCT_INT32 nDay;				//日，设备本地时间
	GCT_INT32 nHour;			//时，设备本地时间
	GCT_INT32 nMin;				//分，设备本地时间
	GCT_INT32 nSec;				//秒，设备本地时间
}gct_sdcardplayback_callback_seekto_param;
//拖动到某个位置
typedef GCT_VOID (*Fun_Glnk_SdcardPlayback_SeekTo_CallBack)(const gct_sdcardplayback_callback_seekto_param seekto_param);

typedef enum _GCT_SDCARDPLAYBACK_CTRL_TYPE{
	GCT_SDCARDPLAYBACK_CTRL_TYPE_PAUSE,		//暂停播放
	GCT_SDCARDPLAYBACK_CTRL_TYPE_RESUME,	//恢复播放
	GCT_SDCARDPLAYBACK_CTRL_TYPE_PLUS,		//加速播放 倍数请看nValue字段
	GCT_SDCARDPLAYBACK_CTRL_TYPE_MINUS,		//减速播放 倍数请看nValue字段
	GCT_SDCARDPLAYBACK_CTRL_TYPE_CLOSE,		//取流结束，上传不用推流，可以释放内存
}GCT_SDCARDPLAYBACK_CTRL_TYPE;
//nSessionId 唯一性，每个连接的唯一值 Fun_Glnk_SdcardPlayback_StartPlay_CallBack回调
typedef GCT_VOID (*Fun_Glnk_SdcardPlayback_Ctrl_CallBack)(const GCT_UINT32 nSessionId,const GCT_SDCARDPLAYBACK_CTRL_TYPE euGCT_SDCARDPLAYBACK_CTRL_TYPE,const GCT_INT32 nValue);

typedef struct gct_sdcardplayback_callback_daylist_node_t{
	GCT_UINT32 nYear;
	GCT_UINT32 nMonth;
	GCT_UINT32 nDay;
	struct gct_sdcardplayback_callback_daylist_node_t* pNext;
}gct_sdcardplayback_callback_daylist_node;
//nChannelNo 通道号，从0开始
//ppgct_sdcardplayback_callback_daylist_node_list 日期列表，上层malloc SDK释放
typedef GCT_VOID (*Fun_Glnk_SdcardPlayback_DayList_CallBack)(const GCT_UINT32	nChannelNo,gct_sdcardplayback_callback_daylist_node** ppgct_sdcardplayback_callback_daylist_node_list);

//音视频格式信息的回调
// nSessionId 唯一性，每个连接的唯一值
// nChannelNo 通道号，从0开始
typedef GCT_VOID (*Fun_GLNK_SdcardPlayback_StreamInfo_Callback)(const GCT_UINT32 nSessionId,const GCT_UINT32 nChannelNo,gct_stream_data_format* pstream_info);

GCT_VOID gct_sdcard_playback_callback_reg_search(Fun_Glnk_SdcardPlayback_Search_CallBack fun);
GCT_VOID gct_sdcard_playback_callback_reg_startplay(Fun_Glnk_SdcardPlayback_StartPlay_CallBack fun);
GCT_VOID gct_sdcard_playback_callback_reg_seekto(Fun_Glnk_SdcardPlayback_SeekTo_CallBack fun);
GCT_VOID gct_sdcard_playback_callback_reg_ctrl(Fun_Glnk_SdcardPlayback_Ctrl_CallBack fun);
GCT_VOID gct_sdcard_playback_callback_reg_day_list(Fun_Glnk_SdcardPlayback_DayList_CallBack fun);
GCT_VOID gct_sdcard_playback_callback_reg_streaminfo(Fun_GLNK_SdcardPlayback_StreamInfo_Callback funCallBack);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

