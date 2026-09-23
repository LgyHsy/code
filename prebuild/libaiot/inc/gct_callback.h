#ifndef __GCT_CALLBACK_H__
#define __GCT_CALLBACK_H__
#include <stdio.h>
#include <stdint.h>
#include "gct_common.h"
#include "gct_types.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////// define /////////////////////////////
// 反馈进度给设备上层
typedef enum _GT_FIRMWARE_PROCESS_TYPE_DEV{
	GT_FIRMWARE_PROCESS_TYPE_DEV_START 	= 1,			//固件开始下载 buff 为NULL,len是总长度
	GT_FIRMWARE_PROCESS_TYPE_DEV_ING 	= 2,			//固件下载中,buff 为单次下载的缓冲区,len是该次buff的长度
	GT_FIRMWARE_PROCESS_TYPE_DEV_FINISH = 3,			//固件下载完成(成功)buff 为NULL,len是总长度
	GT_FIRMWARE_PROCESS_TYPE_DEV_FAIL 	= 4,			//固件下载完成(成功)buff 为NULL,len是总长度
}GT_FIRMWARE_PROCESS_TYPE_DEV;

///////////////////////////////////// callback //////////////////////////////////

/*	密码验证接口
 *  app -->device
 *  username ：		app请求的用户名
 *  pwd：			app请求的密码
 *  nChannelNo:		app请求的通道号,通道号都要从0开始
 *  sid：			当前连接的唯一标志，可用于glnk_CloseSession主动断开
 *  return：			1 = 验证成功,2 = 用户名错误，3 = 密码错误 ,4 = 用户名和密码都错误,5 = 不支持的通道号
*/
typedef GCT_UINT32 (*Fun_GLNK_PwdAuthWithChannel_Callback)(const GCT_CHAR* username,const GCT_CHAR* pwd,const GCT_UINT32 nChannelNo,const GCT_UINT32 sid);

/*	新连接回调
 *  app -->device
 *  nChannelNo:		app请求的通道号,通道号都要从0开始
 *  streamtype : 	为码流类型（主、次码流）,0->主,1->次 2= sd卡回放，3 = 透明通道 7= 长链接
 *  sid：			当前连接的唯一标志
 *  */
typedef GCT_VOID (*Fun_GLNK_login_Callback)(const GCT_UINT32 sid,const GCT_UINT32 nChannelNo,const GCT_UINT32 streamtype);

/*	连接退出回调
 *  app -->device
 *  sid：			当前连接的唯一标志
 *  */
typedef GCT_VOID (*Fun_GLNK_logout_Callback)(const GCT_UINT32 sid);

/*	码流切换回调
 *  app -->device
 *  sid：			当前连接的唯一标志
 *  */
typedef GCT_VOID (*Fun_GLNK_videoswitch_Callback)(const GCT_UINT32 sid,const GCT_UINT32 nFromChannelNo,const GCT_UINT32 nFromStreamtype, const GCT_UINT32 nChannelNo,const GCT_UINT32 streamtype);

/*	高清标清切换回调
 *  app -->device
 *  nChannelNo:		app请求的通道号,通道号都要从0开始
 *  streamtype : 	为新的码流类型（主、次码流）,0->主,1->次
 *  sid：			当前连接的唯一标志
 *  */
typedef GCT_VOID (*Fun_GLNK_StreamTypeSw_Callback)(const GCT_UINT32 sid,const GCT_UINT32 nChannelNo,const GCT_UINT32 streamtype);


/*  请求I帧，强插I帧注册接口,有新用户登录或切换码流时，直播强出I帧。
 *  nChannelNo   : 为通道
 *  streamtype : 为码流类型（主、次码流）,0->主,1->次
 * 	GCT_IFRAME_REASON：请求i帧的模块，调试使用
 *  return： 
*/
typedef GCT_VOID (*Fun_GLNK_CallForIFrame_CallBack)(const GCT_UINT32 nChannelNo,const GCT_UINT32 streamtype,const GCT_IFRAME_REASON euGCT_IFRAME_REASON);

/*	接收数据回调函数接口，透明通道接收数据
 * 	sid	 	           连接ID
 * 	pBuf：              接收到的数据
 * 	nBufLen：			接收到的数据长度
 * 	retutn : 			0--失败，数据继续走老接口
 * 			   			1--成功，数据提取成功   
 */
typedef GCT_INT32 (*Fun_GLNK_TransparentChannel_read_Callback)(const GCT_CHAR *pBuf,const GCT_UINT32 nBufLen,const GCT_UINT32 sid);

/*	根服务器时间戳回调
 *  nTs			1970年到现在的秒数
 *  
 */
typedef GCT_VOID (*Fun_GLNK_SvrTs_Callback)(const GCT_UINT64 nTs);

/*	时间同步回调接口 
 *  app -->device
 *  nTimeZone:			时区值,[-12,12]
 *  nSvrTs: 			服务器的时间戳 1970年到现在的秒数
	return :			1 = 成功 0 = 失败
 */
typedef GCT_INT32 (*Fun_GLNK_TimeSyn_CallBack)(const GCT_INT32 nTimeZone, const GCT_UINT64 nSvrTs);	

/*	音视频格式信息的回调
 * 
 * 
 */
typedef GCT_VOID (*Fun_GLNK_StreamInfo_Callback)(const GCT_UINT32 nChannelNo,const GCT_UINT32 streamtype,gct_stream_data_format* pstream_info);

/*	获取实时截图图片的回调
 * 内存由上层分配，库里面释放
 *  
 */
typedef GCT_VOID (*Fun_GLNK_Screenshots_Callback)(const GCT_UINT32 nChannelNo,const GCT_UINT32 nAlarmType,GCT_CHAR **ppData,GCT_UINT32 *pnLen);

/*	4g设备注册ICCID回调
 *  status = 0 无效, = 1 有效,= 2 没有获取到
 *	pSignalStr   json值,不可以超过128个字节,里面有两个参数。time:表示采集时间，signal：表示信号数值。例子：{"time":"2022-04-20 23:45:11","signal":65}
 */
typedef GCT_VOID (*Fun_GLNK_Get4G_ICCID_Callback)(GCT_CHAR* piccid1, GCT_UINT8* pniccid1_status,GCT_CHAR* piccid2, GCT_UINT8* pniccid2_status,GCT_CHAR* pSignalStr);

/*	4g设备imei
 *  pImei imei字符串
 */
typedef GCT_VOID (*Fun_GLNK_Get4G_Imei_Callback)(GCT_CHAR* pImei);

/*	对讲请求(打开或者关闭)
 *  nChannelNo: 通道号
 *	bOpen:		GCT_TRUE = 打开,GCT_FALSE = 关闭
 *	return:		1 = 允许对讲, 0 = 不允许对讲或者对讲占用中
 */
typedef GCT_INT32 (*Fun_GLNK_Talking_Callback)(const GCT_UINT32 nChannelNo,const GCT_BOOL bOpen);

/*	对讲音频数据回调
 *  nChannelNo: 通道号
 *	nStreamTime:		时间戳
 */
typedef GCT_VOID (*Fun_GLNK_Talking_Audio_Callback)(const GCT_UINT32 nChannelNo,const GCT_UINT32 nStreamTime,const GCT_VOID* pData,const GCT_UINT32 nDataLen);

/*	双向视频数据回调
 *  nChannelNo: 通道号
 *	nStreamTime:		时间戳
 *	nIsIFrame:		1 = i帧，0 = p帧
 */
typedef GCT_VOID (*Fun_GLNK_Talking_Video_Callback)(const GCT_UINT32 nChannelNo,const GCT_UINT32 nStreamTime,const GCT_UINT8 nIsIFrame,const GCT_VOID* pData,const GCT_UINT32 nDataLen);

//////////////////////// 固件升级相关 begin////////////////////////////
/*	服务器获取设备固件版本信息
 *  app -->device
 *  appbuf：		app名字不能超过20个字节（根据不同的app相应写死名字，如菲扬-FeiYang）
 *  solbuf：		方案商名字不能超过20个字节（用公司名字的全拼，如浪涛->LangTao）
 *  date：			设备软件版本更新的日期不能超过20个字节
 *  hardware：		硬件版本---方案商自己定义不能超过64个字节*/
typedef GCT_INT32 (*Fun_GLNK_GetVersionFirmware_Callback)(GCT_CHAR* appbuf, GCT_CHAR* solbuf, GCT_CHAR* date, GCT_CHAR* hardware);

/*	app 开始请求固件下载(可选对接)
 *  nFirmFileLen -1 = 不知道文件大小, > 0 为实际固件大小(单位为字节)
 *	pMd5Value  固件md5校验值
 *  return 0 = 允许下载,-1 = 不能下载
 */
typedef GCT_INT32 (*Fun_GLNK_DeviceUpdateReq_Callback)(const GCT_UINT64 nFirmFileLen,const GCT_CHAR* pMd5Value);

/*	固件下载中(可选对接,如果对接了该接口则不会写入固件文件了)
 *  pBody 该片段数据
 *	nBodyLen  该片段数据长度
 *  return 0 = 成功,其他为失败
 */
typedef GCT_INT32 (*Fun_GLNK_DeviceUpdateBody_Callback)(const GCT_CHAR* pBody,const GCT_UINT64 nBodyLen);

/*	固件下载完成，可以升级
 * pFileAbsPath ,为gct_apiv4_firmware_update_set_savepath 设置的路径
 * pWebFileName	,为提供给服务器时的文件名称
 * return 0 = 成功 其他 = 失败
 */
typedef GCT_INT32 (*Fun_GLNK_DeviceUpdateStart_Callback)(const GCT_CHAR* pFileAbsPath,const GCT_CHAR* pWebFileName);

//////////////////////// 固件升级相关 end////////////////////////////

/*	设备重启接口
 *  app -->device	*/
 //bImmediatelyDoIt GCT_TRUE = 立刻重启,不延迟,GCT_FALSE = 不要求立刻重启
typedef GCT_INT32 (*Fun_GLNK_DeviceReboot_Callback)(const GCT_BOOL bImmediatelyDoIt);

/////////////////			
/*	App获取硬盘(SD卡)列表接口
 *  device -->app
 *  StorageList：	硬盘(SD卡)列表结构体，是一个变长结构体（见goolink_apiv3.h）*/
typedef GCT_INT32 (*Fun_GLNK_GetStorageList_Callback)(GLNK_DeviceStorageResponse2 **StorageList);

/*	格式化硬盘(SD卡)列表接口
 *  app -->device
 *  StorageID：		GLNK_DeviceStorageList->StorageID，硬盘(sd卡)ID，和获取时的id保持一致
 *  return		格式化结果回复0:格式化失败，1:格式化成功，2:无权限
 */
typedef GCT_INT32 (*Fun_GLNK_FormatStorage_Callback)(const GCT_INT32 StorageID);

//设置画面翻转
//bVertical 水平方向,GCT_TRUE = 画面是正的 ,GCT_FALSE = 画面是反的 
//bHorizontal 垂直方向,GCT_TRUE = 画面是正的 ,GCT_FALSE = 画面是反的 
//return GCT_TRUE = 成功,GCT_FALSE = 失败
typedef GCT_BOOL (*Fun_GLNK_Image_Trans_Callback)(const GCT_BOOL bVertical,const GCT_BOOL bHorizontal);

//获取画面翻转默认值
//bVertical 水平方向,GCT_TRUE = 画面是正的 ,GCT_FALSE = 画面是反的 
//bHorizontal 垂直方向,GCT_TRUE = 画面是正的 ,GCT_FALSE = 画面是反的 
//return GCT_TRUE = 成功,GCT_FALSE = 失败
typedef GCT_BOOL (*Fun_GLNK_Image_Trans_Get_Default_Callback)(GCT_BOOL* pbVertical,GCT_BOOL* pbHorizontal);

/*********************************************************************************************************************************************************************************************************************************************/
/*  APP传输大文件或者大数据给设备，分段传输，传输请求
 *  nBuffAllLen : 文件或者数据总长度(字节)
 *  pFileName : 可选，如果为NULL或者长度为0 则没有文件名 为传输大的数据
 *  return ：0 = 允许传输,1=不允许传输			
 */
typedef GCT_INT32 (*Fun_GLNK_Cli_TransBigData_Req_CallBack)(const GCT_INT32 nBuffAllLen,const GCT_CHAR* pFileName);

/*********************************************************************************************************************************************************************************************************************************************/
/*  APP传输大文件或者大数据给设备，分段传输 传输中
 *  nBuffLen : 为当前传输的长度(字节)
 *  pBuff : 为当前传输数据
 *  return ：0 = 保存成功,1=保存失败			 如果失败则会中断传输
 */
typedef GCT_INT32 (*Fun_GLNK_Cli_TransBigData_Ing_CallBack)(const GCT_INT32 nBuffLen,const GCT_CHAR* pBuff);

/*********************************************************************************************************************************************************************************************************************************************/
/*  APP传输大文件或者大数据给设备，分段传输，结束传输
 *  return ：0 = 保存数据成功,1=保存失败				
 */
typedef GCT_INT32 (*Fun_GLNK_Cli_TransBigData_End_CallBack)();

//回调设备是否被APP绑定
//bIsBind, GCT_TRUE = 已被绑定, GCT_FALSE = 已被解绑
typedef GCT_VOID (*Fun_GLNK_Bind_State_CallBack)(const GCT_BOOL bIsBind);

/*	云台控制回调接口
 *  app -->device
 *  ptzcmd：		云台命令
 *  channel：		通道（默认为0）
 *  arg：			额外参数（见goolink_apiv3.h）
 *  return : 1 = 转动成功 0 = 转动到底不能再转 2 =不支持ptz 3 = 不支持的ptz操作(比如有些设备支持左右移动不支持上下) 如果还有其他错误，自行定义，大于3就行了,4 = 协议错误
 */
typedef GCT_INT32 (*Fun_GLNK_PTZCmd_Callback)(const GLNK_PTZControlCmd ptzcmd,const GCT_UINT32 channel,const ControlArgData* arg);

//恢复出厂
//return 1 = 成功,0 = 失败
typedef GCT_INT32 (*Fun_GLNK_FactoryReset_Callback)();

/*	app请求设备搜索wifi信息接口
 *  device -->app
 *  buf ：		设备搜索返回给app的数据（wifi信息）数据结构必须强转为gct_wifi_info（见gct_common.h）	
 *	return:	返回wifi的个数
 */
typedef GCT_INT32 (*Fun_GLNK_SearchWifi_Callback)(GCT_CHAR** ppBuff);

/*	app返回配置wifi信息接口
 *  app -->device
 *  Req ：	配置参数
 *	return: 1 = 成功,开始配置wifi 0 = 失败
 */
typedef GCT_INT32 (*Fun_GLNK_WifiConfig_Callback)(const gct_wifi_config_req *preq);

/*	获取设备网络状态
 *  "Internet status": "Connected/Disconnected", 	//不用申请内存，直接strcpy即可
 *  "Address": "设备IP地址",							//不用申请内存，直接strcpy即可
 *  "Connection": "Wi-Fi/Wired",					//不用申请内存，直接strcpy即可
 *  "Signal": "95"									//不用申请内存，直接strcpy即可
 */
typedef GCT_VOID (*Fun_GLNK_networkInformation_CallBack)(GCT_CHAR *Internet_status,GCT_CHAR *Addr,GCT_CHAR *Connection,GCT_CHAR *Signal);

/*	请求重新加载sd卡驱动和重新mount 的脚本路径(一般是设备库录像遇到只读现象了)
 *	不可以阻塞,如果阻塞可以开线程处理
 */
typedef GCT_VOID (*Fun_GLNK_RemountSdcard_CallBack)();

//检查sdcard是否正常
//return ,0 = 卡正常(sd卡插好+sd卡目录(比如 /mnt)上面的容量显示真实sd卡的容量),1 = 卡异常(sd卡插上了+ sd卡目录(比如 /mnt)上面的容量显示真实sd卡的容量,设备库sd卡录像会停止), 2 = sd卡没插
typedef GCT_INT32 (*Fun_GLNK_Chk_Sdcard_Normal_CallBack)();

//返回当前网卡是不通的,上层可以根据这个结论做一些动作,比如重启wifi操作等(网络不通->网络通了,网络通了->网络不通,有变化了才会回调，不会重复回调同一个状态)
//pIframe 网卡名称,比如 wlan0
//bIsOk 网络是否中断或者恢复,GCT_FALSE = 网络不通 ,GCT_TRUE = 网络通了
typedef GCT_VOID (*Fun_GLNK_NetWork_Exp_CallBack)(const GCT_CHAR* pIframe,const GCT_BOOL bIsOk);

//固件md5校验 一般不用对接。除非浪涛这边要求
//pFirmAbsPath 固件所在的绝对路径
//pMd5AbsPath md5文件生成的绝对路径
typedef GCT_VOID (*Fun_GLNK_Firm_Md5_Auth_Callback)(const GCT_CHAR* pFirmAbsPath,const GCT_CHAR* pMd5AbsPath);

//gid 变更
typedef GCT_VOID (*Fun_GLNK_Gid_Change_Callback)();

//双向视频对讲(视频)
//bIsOpen , true = 打开，false = 关闭 关闭时不需要关注其他参数
//nVideoFmt 264 = H264 265 = H265
//nFrameRate 帧率
//nVideoH 分辨率 高
//nVideoW 分辨率 宽
//return 0 = 成功,1 = 占用中,其他 = 失败
typedef GCT_INT32 (*Fun_GLNK_DoubleVideo_Ctrl_Callback)(const GCT_BOOL bIsOpen,const GCT_INT32 nVideoFmt,const GCT_INT32 nFrameRate,const GCT_INT32 nVideoH,const GCT_INT32 nVideoW);

//双向视频对讲(音频)
//bIsOpen , true = 打开，false = 关闭 关闭时不需要关注其他参数
//return 0 = 成功,1 = 占用中,其他 = 失败
typedef GCT_INT32 (*Fun_GLNK_DoubleAudio_Ctrl_Callback)(const GCT_BOOL bIsOpen);

//双向视频对讲(音频数据)
//pData 
//nDataLen
//nTimeStampMs 时间戳 毫秒
typedef GCT_INT32 (*Fun_GLNK_DoubleAudio_Data_Callback)(const GCT_VOID* pData,const GCT_INT32 nDataLen,const GCT_INT32 nTimeStampMs,const GCT_INT32 nIndex);

//双向视频对讲(视频数据)
//pData 
//nDataLen
//nTimeStampMs 时间戳 毫秒
typedef GCT_INT32 (*Fun_GLNK_DoubleVideo_Data_Callback)(const GCT_VOID* pData,const GCT_INT32 nDataLen,const GCT_BOOL bIsIFrame,const GCT_INT32 nTimeStampMs,const GCT_INT32 nIndex);

//云存储开通套餐回调
//nCloudP 0 = 未开通 1 = 告警套餐 2 = 连续套餐
typedef GCT_VOID (*Fun_GLNK_Cloud_Packet_Callback)(const GCT_INT32 nCloudP);

/////////////////////////////////////////////////以下为回调注册接口////////////////////////////////
GCT_VOID gct_cb_reg_auth_pwd(Fun_GLNK_PwdAuthWithChannel_Callback funCallBack);
GCT_VOID gct_cb_reg_login(Fun_GLNK_login_Callback funCallBack);
GCT_VOID gct_cb_reg_videoswitch(Fun_GLNK_videoswitch_Callback funCallBack);
GCT_VOID gct_cb_reg_logout(Fun_GLNK_logout_Callback funCallBack);
GCT_VOID gct_cb_reg_streamtype_sw(Fun_GLNK_StreamTypeSw_Callback funCallBack);
GCT_VOID gct_cb_reg_svr_ts(Fun_GLNK_SvrTs_Callback funCallBack);
GCT_VOID gct_cb_reg_iframe(Fun_GLNK_CallForIFrame_CallBack funCallBack);
GCT_VOID gct_cb_reg_streaminfo(Fun_GLNK_StreamInfo_Callback funCallBack);
GCT_VOID gct_cb_reg_screenshots(Fun_GLNK_Screenshots_Callback funCallBack);
GCT_VOID gct_cb_reg_4g_iccid(Fun_GLNK_Get4G_ICCID_Callback funCallBack);
GCT_VOID gct_cb_reg_4g_imei(Fun_GLNK_Get4G_Imei_Callback funCallBack);
GCT_VOID gct_cb_reg_firmupdate_getversion(Fun_GLNK_GetVersionFirmware_Callback funCallBack);
GCT_VOID gct_cb_reg_firmupdate_req(Fun_GLNK_DeviceUpdateReq_Callback funCallBack);
GCT_VOID gct_cb_reg_firmupdate_body(Fun_GLNK_DeviceUpdateBody_Callback funCallBack);
GCT_VOID gct_cb_reg_firmupdate_start(Fun_GLNK_DeviceUpdateStart_Callback funCallBack);
GCT_VOID gct_cb_reg_reboot_dev(Fun_GLNK_DeviceReboot_Callback funCallBack);
GCT_VOID gct_cb_reg_time_syn(Fun_GLNK_TimeSyn_CallBack funCallBack);
GCT_VOID gct_cb_reg_talking(Fun_GLNK_Talking_Callback funCallBack);
GCT_VOID gct_cb_reg_talking_audio(Fun_GLNK_Talking_Audio_Callback funCallBack);
GCT_VOID gct_cb_reg_talking_video(Fun_GLNK_Talking_Video_Callback funCallBack);
GCT_VOID gct_cb_reg_trans_channel(Fun_GLNK_TransparentChannel_read_Callback funCallBack);
GCT_VOID gct_cb_reg_bind_state(Fun_GLNK_Bind_State_CallBack funCallBack);
GCT_VOID gct_cb_reg_sdcard_get(Fun_GLNK_GetStorageList_Callback funCallBack);
GCT_VOID gct_cb_reg_sdcard_format(Fun_GLNK_FormatStorage_Callback funCallBack);
GCT_VOID gct_cb_reg_ptz_op(Fun_GLNK_PTZCmd_Callback funCallBack);
GCT_VOID gct_cb_reg_factory_reset(Fun_GLNK_FactoryReset_Callback funCallBack);
GCT_VOID gct_cb_reg_wifi_search(Fun_GLNK_SearchWifi_Callback funCallBack);
GCT_VOID gct_cb_reg_wifi_config(Fun_GLNK_WifiConfig_Callback funCallBack);
GCT_VOID gct_cb_reg_dev_network_information(Fun_GLNK_networkInformation_CallBack funCallBack);
GCT_VOID gct_cb_reg_remount_sdcard_script(Fun_GLNK_RemountSdcard_CallBack funCallBack);
GCT_VOID gct_cb_reg_chk_sdcard_normal(Fun_GLNK_Chk_Sdcard_Normal_CallBack funCallBack);
GCT_VOID gct_cb_reg_network_exp(Fun_GLNK_NetWork_Exp_CallBack funCallBack);
GCT_VOID gct_cb_reg_firm_md5_auth(Fun_GLNK_Firm_Md5_Auth_Callback funCallBack);
GCT_VOID gct_cb_reg_trans_bigdata_req(Fun_GLNK_Cli_TransBigData_Req_CallBack funCallBack);
GCT_VOID gct_cb_reg_trans_bigdata_ing(Fun_GLNK_Cli_TransBigData_Ing_CallBack funCallBack);
GCT_VOID gct_cb_reg_trans_bigdata_end(Fun_GLNK_Cli_TransBigData_End_CallBack funCallBack);
GCT_VOID gct_cb_reg_gid_change(Fun_GLNK_Gid_Change_Callback funCallBack);
GCT_VOID gct_cb_reg_double_video_ctrl(Fun_GLNK_DoubleVideo_Ctrl_Callback funCallBack);
GCT_VOID gct_cb_reg_double_audio_ctrl(Fun_GLNK_DoubleAudio_Ctrl_Callback funCallBack);
GCT_VOID gct_cb_reg_double_video_data(Fun_GLNK_DoubleVideo_Data_Callback funCallBack);
GCT_VOID gct_cb_reg_double_audio_data(Fun_GLNK_DoubleAudio_Data_Callback funCallBack);
GCT_VOID gct_cb_reg_cloud_p(Fun_GLNK_Cloud_Packet_Callback funCallBack);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
