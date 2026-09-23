#ifndef _GCT_APIV4_H__
#define _GCT_APIV4_H__
#include "gct_common.h"
#include <stdbool.h>
#include "gct_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////// 库全局 ////////////////////////
//初始化库
GCT_INT32 gct_apiv4_init(const gct_param gctparam);

//设置设备的能力集,json数据,注意,这个接口要在gct_apiv4_init之后调用
//pAbilityJson json数据
//nAbilityJsonLen 数据长度
GCT_VOID gct_apiv4_device_ability_v2(const GCT_CHAR* pAbilityJson,const GCT_UINT32 nAbilityJsonLen);
GCT_VOID gct_apiv4_device_ability_get_v2(GCT_CHAR* pAbilityJson,const GCT_UINT32 nAbilityJsonMaxLen);

//释放库
GCT_VOID gct_apiv4_release();

//释强制更新云存套餐
GCT_VOID gct_api_forceupdate_cloud();

//释放服务器资源(比如设备需要主动重启设备时调用,方便服务器及时知道设备离线了。调用reboot重启之前调用一下这个接口就行)
GCT_VOID gct_apiv4_server_release();

//重置服务器连接
GCT_VOID gct_apiv4_server_reset();

//获取服务器最后一次的保活时间,方便定位服务器是否正在运行中
GCT_INT64 gct_apiv4_server_get_lastalive_ms();

/*	获取goolink连接服务器状态值
 */
GCT_STATE_TO_SERVER gct_apiv4_get_state_to_server();

//设置固件升级目录和文件名
//pAbsPath: 目录(绝对路径),比如 "/tmp" ,注意:后尾不能加 "/",目录要先创建，保证是存在的
//pFileName: 固件文件名，不需要先创建,比如 "usr.sqsh4"
//bIsInTF 设置的目录是否在sd卡上面如果是，设备库会帮忙检测sd卡是否存在
GCT_VOID gct_apiv4_firmware_update_set_savepath(const GCT_CHAR* pAbsPath,const GCT_CHAR* pFileName,const GCT_BOOL bIsInTF);

//是否支持固件预下载(需要flash或者sd卡)
GCT_VOID gct_apiv4_firmware_update_set_preload(const GCT_BOOL bSupport);

GCT_VOID gct_apiv4_firmware_update_set_gid(const char* gid);

/*	更换网卡接口
 * 	pIFname : 网卡名称（字符串）
 * 	retutn : 0--成功   其他--失败		
 */
GCT_INT32 gct_apiv4_set_ifname(const GCT_CHAR* pIFname);

//获取网络是否通
//pPingOk 是否ping服务器OK,GCT_TRUE = 服务器通,GCT_FALSE = ping服务器不通
//pbConn 是否连接上服务器了,GCT_TRUE = 已连接,GCT_FALSE = 未连接
//注: pPingOk 和 pbConn 只要其中一个是GCT_TRUE,则表示设备端网络已通
GCT_VOID gct_apiv4_get_net_state(GCT_BOOL* pbPingOk,GCT_BOOL* pbConn);

//设备内部功能集(请咨询研发后再使用这个接口)
GCT_VOID gct_apiv4_device_ability(const GCT_DEV_ABILITY euGCT_DEV_ABILITY[200],const GCT_UINT32 nGCT_DEV_ABILITYSize);

//获取gid
GCT_VOID gct_apiv4_get_gid(GCT_CHAR* pGid);

AiotCloudType gct_apiv4_get_cloud_type();

//获取绑定主用户
GCT_VOID gct_apiv4_get_binduser(GCT_CHAR* pBindUser);

/////////////////////////////////// 实时流模块 ///////////////////////////////////
//设置实时流音视频可以使用内存，默认是1M，最低1M
//nSize  可用内存大小,单位为字节,比如2M填1024*1024*2
//注意这是所有通道一起共享的buffer size，每一个通道的session的buffer最大值会根据这个设置值除以通道数量
GCT_VOID gct_apiv4_stream_set_av_buffsize(const GCT_UINT32 nSize);


/// <summary>
/// 限制内存新方案,所有通道使用相同方案，必须在gct_apiv4_init后立即调用
/// </summary>
/// <param name="mainLimitBufSize">主码流限制大小,默认30</param>
/// <param name="subLimitSize">子码流限制大小,默认30</param>
/// <param name="audioLimitSize">音频限制大小,默认50</param>
/// <param name="iBufferLimitType">限制类型,默认1</param>
/// <param name="ch">通道号，默认-1表示所有</param>
/// <returns>成功返回0，负值表示失败</returns>
GCT_INT32 gct_apiv4_stream_set_avbuff_limitsize(const int mainLimitBufSize, const int subLimitSize,const int audioLimitSize, int iBufferLimitType,int ch);

/**推流接口，视频流
nChannelNo 		通道号,从0开始
nIsMainOrSub 	是否是主次码流,0=主码流 1= 次码流
nIsIFrame		是否是i帧 0 = 非i帧 1=i帧
pData			流数据
nDataLen		流数据长度
*/
GCT_VOID gct_apiv4_stream_push_video_stream(const         GCT_UINT32 nChannelNo,const GCT_UINT32 nIsMainOrSub,const GCT_UINT32 nIsIFrame,const GCT_VOID* pData,const GCT_UINT32 nDataLen,  GCT_UINT32 nTimestamp);


/*推流接口 音频流
nChannelNo 		通道号,从0开始
pData			流数据
nDataLen		流数据长度
*/
GCT_VOID gct_apiv4_stream_push_audio_stream(const GCT_UINT32 nChannelNo,const GCT_VOID* pData,const GCT_UINT32 nDataLen,  GCT_UINT32 nTimestamp);


GCT_UINT32  gct_apiv4_stream_get_video_buffullcnt(const          GCT_UINT32 nChannelNo,const GCT_UINT32 nIsMainOrSub);

//////////////////////// 透明通道 //////////////////////////////////////
/*	发送数据接口，透明通道发送数据
 * 	hConnectionID：	 	连接ID
 * 	pBuf：              写入的数据
 * 	nBufLen：			写入的数据长度，限制小于1480个字节
 * 	retutn : 			0--失败
 * 			   			1--成功           */
GCT_INT32 gct_apiv4_trans_channel_write(const         GCT_UINT32 nSessionId,const GCT_CHAR *pBuf,const GCT_UINT32 nBufLen);

//获取有多少个用户访问实时流
//return 大于0 有人访问,否则为无人访问
GCT_UINT32 gct_apiv4_get_video_access();


////////////////////////////////// sdcard 回放模块 //////////////////////////////

/*
pMountPath：mount的路径，库用于读写sd卡的数据
注： 中途有改变可以重新设置，没有改变不要调用
*/
GCT_VOID gct_apiv4_pb_set_path(const GCT_CHAR* pMountPath);

/////////////////////////////////	云存储模块 /////////////////////////////////

//可以分配给云存储最大内存大小,默认是1M,最低不少于1M(请留意根据设备的能力，一旦设置了，触发云存储或者有预录，内存会被一下子申请完)
GCT_VOID gct_apiv4_gtcloud_set_buff_size(const       GCT_UINT32 nChannelNo,const GCT_UINT32 nSize);

//需要预录多少秒,默认是不预录(不一定是很准的,库会根据实际设置的内存来衡量)
GCT_VOID gct_apiv4_gtcloud_set_prerecord(const       GCT_UINT32 nChannelNo,const GCT_UINT32 nPreRecordS);

//将 JSON 格式的属性数据异步增量写入云存储 properties 目录(本地缓存 + 定时/按需上传)
//pFullJson 完整全量属性 JSON 文本；nChannelNo 通道号（0 起，超出范围返回错误）
//return 0 成功，<0 失败
GCT_INT32 gct_apiv4_upload_properties_full(const GCT_CHAR* pFullJson, const GCT_UINT32 nChannelNo);
//pChangeJson 增量属性 JSON 文本；nChannelNo 通道号（0 起，超出范围返回错误）
//return 0 成功，<0 失败
GCT_INT32 gct_apiv4_upload_properties_change(const GCT_CHAR* pChangeJson, const GCT_UINT32 nChannelNo);

//设置属性上传总开关(默认关闭，需应用显式开启)
//bEnable TRUE 开启，FALSE 关闭
GCT_VOID gct_apiv4_upload_properties_set_enable(const GCT_BOOL bEnable);

//查询属性上传总开关当前状态
//return TRUE 已开启，FALSE 已关闭
GCT_BOOL gct_apiv4_upload_properties_is_enable(void);

//通过专用 API 设置强制同步标志，使下一次同步立即写入云端(忽略定时间隔)
//return 0 成功
GCT_INT32 gct_apiv4_upload_properties_force_sync(void);

//释放 properties 模块内存资源(进程退出时调用)
GCT_VOID gct_apiv4_upload_properties_release(void);

///////////////////////////////// 设备使用,清理配置日志文件等 ///////////
//重置设备(恢复出厂)
GCT_VOID gct_apiv4_reset_device();
//解绑设备
GCT_VOID gct_apiv4_unbind_device();

GCT_VOID gct_apiv4_remove_unbind_device();

GCT_INT32 gct_apiv4_get_bind_state();



///////////////////////////////// 日志模块 //////////////////////////////////////
//是否需要打开日志开关，保存本地,如果需要打开，则先调用下面的gct_apiv4_log_savepath 设置保存路径
// bOpen  false = 关闭 true = 打开
GCT_VOID gct_apiv4_log_savelocal(const GCT_BOOL bOpen);

//日志保存的目录
GCT_VOID gct_apiv4_log_savepath(const GCT_CHAR* pPath);

//实时日志控制
//bOpenDebug debug级别
//bOpenError error级别
GCT_VOID gct_apiv4_log_switch(const GCT_BOOL bOpenDebug,const GCT_BOOL bOpenError);


//动态设置日志写文件
GCT_VOID gct_slog_openfile(const char* path);

//动态关闭日志写文件
GCT_VOID gct_slog_closeFile();

//devsdk执行slog字符串命令
GCT_VOID gct_slog_proc_cmd(const char* cmd);


///////////////////////////////// 工具类模块 //////////////////////////////////////
GCT_VOID* gct_apiv4_buff_malloc_ext(const GCT_UINT32 nLen,const GCT_CHAR* pFunName,const GCT_UINT32 nLineNum);
#define gct_apiv4_buff_malloc(nLen) gct_apiv4_buff_malloc_ext(nLen,__func__,__LINE__)
GCT_VOID gct_apiv4_buff_free(GCT_VOID* pBuff,const GCT_UINT32 nLen);

//设置有线网卡(库会帮忙检查这个网卡是否通,回调请注册 gct_cb_reg_network_exp)
GCT_VOID gct_apiv4_network_exp_set_iframe_wired(const GCT_CHAR* pIframe);
//设置无线网卡(库会帮忙检查这个网卡是否通,回调请注册 gct_cb_reg_network_exp)
GCT_VOID gct_apiv4_network_exp_set_iframe_wireless(const GCT_CHAR* pIframe);

//记录重启的缘由,比如APP发送的指令等
//nReason [1,1000] 为设备库内部使用,设备库上层用户请用 > 1000的值
GCT_BOOL gct_apiv4_reboot_reason_record(const GCT_UINT32 nReason);

//根据网卡获取ip地址
//pIfname 网卡名称，比如"eth0","wlan0"
//pOutIp ip地址，比如定义 GCT_CHAR szOutIp[256] = {0};
//return GCT_TRUE = 拿到ip地址了，否则失败
GCT_BOOL gct_apiv4_getLocalip(const GCT_CHAR* pIfname,GCT_CHAR *pOutIp);

/////其他
//NVR使能,多少个通道则调用多次
//nChannelNo 通道号
//bEnable 使能 true = 打开 false = 关闭
GCT_VOID gct_apiv4_nvr_enable(const GCT_UINT32 nChannelNo,const GCT_BOOL bEnable);

GCT_INT32  gct_apiv4_add_fw_conn(const GCT_CHAR *pIp, const GCT_UINT32 nPort);


//清空所有DNS解析，用于4G卡或者基站切换后重新解析
GCT_VOID gct_apiv4_dns_clearall();
//清空单个DOMAIN DNS解析
GCT_VOID gct_apiv4_dns_clear(const GCT_CHAR* pDomain);


GCT_VOID gct_apiv4_set_aov_flag();
GCT_VOID gct_apiv4_set_4g_flag();
GCT_VOID gct_apiv4_get_my_area_node( GCT_CHAR *pWebUrl,  GCT_CHAR *pIp,  GCT_UINT32 *nPort);
	GCT_UINT32	   gct_apiv4_get_session_cnt();


///////////////////////////////// 云存储文件解析 //////////////////////////////////////
GCT_INT32 gct_api_parse_owsp_file(const GCT_CHAR* pFileName);
GCT_INT32 gct_api_parse_config_file(const GCT_CHAR* pFileName, const GCT_CHAR* pGid);
GCT_INT32 gct_api_parse_owsp_by_iframe_config(const GCT_CHAR* pOwspFileName,
                                                 const GCT_CHAR* pConfigFileName,
                                                 const GCT_CHAR* pGid);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
