#ifndef _GCT_SDCARD_PLAYBACK_APIV4_H__
#define _GCT_SDCARD_PLAYBACK_APIV4_H__
#include "gct_common.h"
#include <stdbool.h>
#include "gct_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gct_sdcard_playback_param_t{
	GCT_UINT32 nSessionId; 		//唯一性，每个连接的唯一值 播放回调
	GCT_UINT32 nIsVideo;		//是否是视频,0 = 音频 1= 视频
	GCT_CHAR* pData;			//流数据
	GCT_INT32 nDataLen;			//流数据长度
	GCT_INT32 nIsIFrame;		//是否是i帧 0 = 非i帧 1=i帧 如果是音频，这个字段不检查，填0即可
	GCT_INT32 nYear;			//年，设备本地时间
	GCT_INT32 nMonth;			//月，设备本地时间
	GCT_INT32 nDay;				//日，设备本地时间
	GCT_INT32 nHour;			//时，设备本地时间
	GCT_INT32 nMin;				//分，设备本地时间
	GCT_INT32 nSec;				//秒，设备本地时间
	GCT_INT32 nMs;				//毫秒，设备本地时间
}gct_sdcard_playback_param;
//推音视频流
//return 0 = 成功 1 = 内存满,暂停一下,当前推的帧也要重新推
GCT_UINT32 gct_sdcard_playback_apiv4_push_avstream(const gct_sdcard_playback_param sdcard_playback_param);

//中途修改了编码格式
//return 0 = success other = fail
GCT_INT32 gct_sdcard_playback_apiv4_avformat_change(const GCT_UINT32 nSessionId,const gct_stream_data_format stream_data_format);

//播放结束
GCT_VOID gct_sdcard_playback_apiv4_push_end(const GCT_UINT32 nSessionId);

//清空回放缓冲数据，避免SEEK的时候继续发送以前的数据
GCT_VOID gct_sdcard_playback_apiv4_clearbuffer(const GCT_UINT32 nSessionId);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

