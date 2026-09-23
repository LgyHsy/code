#ifndef __CCT_CALLBACK_H__
#define __CCT_CALLBACK_H__
#include <stdio.h>
#include <stdint.h>
#include "cct_common.h"
#include "cct_types.h"

#ifdef __cplusplus
extern "C" {
#endif



/*  请求I帧，强插I帧注册接口,有新用户登录或切换码流时，直播强出I帧。
 *  nChannelNo   : 为通道
 *  streamtype : 为码流类型（主、次码流）,0->主,1->次
 *  return： 
*/
typedef CCT_VOID (*Fun_CLNK_CallForIFrame_CallBack)(const CCT_UINT32 nChannelNo,const CCT_UINT32 streamtype);


/*	根服务器时间戳回调
 *  nTs			1970年到现在的秒数
 *  
 */
typedef CCT_VOID (*Fun_CLNK_SvrTs_Callback)(const CCT_UINT64 nTs);

/*	音视频格式信息的回调
 * 
 * 
 */
typedef CCT_VOID (*Fun_CLNK_StreamInfo_Callback)(const CCT_UINT32 nChannelNo,const CCT_UINT32 streamtype,cct_stream_data_format* pstream_info);

/*	获取实时截图图片的回调
 * 内存由上层分配，库里面释放
 *  
 */
typedef CCT_VOID (*Fun_CLNK_Screenshots_Callback)(const CCT_UINT32 nChannelNo,const CCT_UINT32 nAlarmType,CCT_CHAR **ppData,CCT_UINT32 *pnLen);


/*	设备重启接口
 *  app -->device	*/
 //bImmediatelyDoIt CCT_TRUE = 立刻重启,不延迟,CCT_FALSE = 不要求立刻重启
typedef CCT_INT32 (*Fun_CLNK_DeviceReboot_Callback)(const CCT_BOOL bImmediatelyDoIt);


//云存储开通套餐回调
//nCloudP 0 = 未开通 1 = 告警套餐 2 = 连续套餐
typedef CCT_VOID (*Fun_CLNK_Cloud_Packet_Callback)(const CCT_INT32 nCloudP);

/////////////////////////////////////////////////以下为回调注册接口////////////////////////////////
CCT_VOID cct_cb_reg_svr_ts(Fun_CLNK_SvrTs_Callback funCallBack);
CCT_VOID cct_cb_reg_iframe(Fun_CLNK_CallForIFrame_CallBack funCallBack);
CCT_VOID cct_cb_reg_streaminfo(Fun_CLNK_StreamInfo_Callback funCallBack);
CCT_VOID cct_cb_reg_screenshots(Fun_CLNK_Screenshots_Callback funCallBack);
CCT_VOID cct_cb_reg_reboot_dev(Fun_CLNK_DeviceReboot_Callback funCallBack);
CCT_VOID cct_cb_reg_cloud_p(Fun_CLNK_Cloud_Packet_Callback funCallBack);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
