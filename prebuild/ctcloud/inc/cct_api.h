#ifndef _CCT_API_H__
#define _CCT_API_H__
#include "cct_common.h"
#include <stdbool.h>
#include "cct_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////// 库全局 ////////////////////////
//初始化库
CCT_INT32 cct_api_init(const cct_param cctparam);


//释放库
CCT_VOID cct_api_release();

//释强制更新云存套餐
CCT_VOID cct_api_forceupdate_cloud();

//释放服务器资源(比如设备需要主动重启设备时调用,方便服务器及时知道设备离线了。调用reboot重启之前调用一下这个接口就行)
CCT_VOID cct_api_server_release();

//重置服务器连接
CCT_VOID cct_api_server_reset();

//设置绑定用户
CCT_VOID cct_api_set_binduser(const CCT_CHAR* pBindUser);

//获取gid
CCT_VOID cct_api_get_gid(CCT_CHAR* pGid);

/////////////////////////////////// 实时流模块 ///////////////////////////////////

//设置实时流音视频可以使用内存，默认是2M，最低2M
//nSize  可用内存大小,单位为字节,比如2M填1024*1024*2
CCT_VOID cct_api_stream_set_av_buffsize(const CCT_UINT32 nSize);

/**推流接口，视频流
nChannelNo 		通道号,从0开始
nIsMainOrSub 	是否是主次码流,0=主码流 1= 次码流
nIsIFrame		是否是i帧 0 = 非i帧 1=i帧
pData			流数据
nDataLen		流数据长度
*/
CCT_VOID cct_api_stream_push_video_stream(const         CCT_UINT32 nChannelNo,const CCT_UINT32 nIsMainOrSub,const CCT_UINT32 nIsIFrame,const CCT_VOID* pData,const CCT_UINT32 nDataLen);

/*推流接口 音频流
nChannelNo 		通道号,从0开始
pData			流数据
nDataLen		流数据长度
*/
CCT_VOID cct_api_stream_push_audio_stream(const CCT_UINT32 nChannelNo,const CCT_VOID* pData,const CCT_UINT32 nDataLen);

/////////////////////////////////	云存储模块 /////////////////////////////////

//可以分配给云存储最大内存大小,默认是1M,最低不少于1M(请留意根据设备的能力，一旦设置了，触发云存储或者有预录，内存会被一下子申请完)
CCT_VOID cct_api_gtcloud_set_buff_size(const       CCT_UINT32 nChannelNo,const CCT_UINT32 nSize);

//需要预录多少秒,默认是不预录(不一定是很准的,库会根据实际设置的内存来衡量)
CCT_VOID cct_api_gtcloud_set_prerecord(const       CCT_UINT32 nChannelNo,const CCT_UINT32 nPreRecordS);

///////////////////////////////// 日志模块 //////////////////////////////////////
//是否需要打开日志开关，保存本地,如果需要打开，则先调用下面的cct_api_log_savepath 设置保存路径
// bOpen  false = 关闭 true = 打开
CCT_VOID cct_api_log_savelocal(const CCT_BOOL bOpen);

//日志保存的目录
CCT_VOID cct_api_log_savepath(const CCT_CHAR* pPath);

//实时日志控制
//bOpenDebug debug级别
//bOpenError error级别
CCT_VOID cct_api_log_switch(const CCT_BOOL bOpenDebug,const CCT_BOOL bOpenError);


//动态设置日志写文件
CCT_VOID cct_slog_openfile(const char* path);

//动态关闭日志写文件
CCT_VOID cct_slog_closeFile();

//devsdk执行slog字符串命令
CCT_VOID cct_slog_proc_cmd(const char* cmd);


///////////////////////////////// 工具类模块 //////////////////////////////////////
CCT_VOID* cct_api_buff_malloc_ext(const CCT_UINT32 nLen,const CCT_CHAR* pFunName,const CCT_UINT32 nLineNum);
#define cct_api_buff_malloc(nLen) cct_api_buff_malloc_ext(nLen,__func__,__LINE__)
CCT_VOID cct_api_buff_free(CCT_VOID* pBuff,const CCT_UINT32 nLen);

///////////////////////////////// 云存储文件解析 //////////////////////////////////////
CCT_INT32 cct_api_parse_owsp_file(const CCT_CHAR* pFileName);
CCT_INT32 cct_api_parse_config_file(const CCT_CHAR* pFileName, const CCT_CHAR* pGid);
CCT_INT32 cct_api_parse_owsp_by_iframe_config(const CCT_CHAR* pOwspFileName,
                                                 const CCT_CHAR* pConfigFileName,
                                                 const CCT_CHAR* pGid);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
