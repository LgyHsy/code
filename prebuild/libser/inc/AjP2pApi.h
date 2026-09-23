#ifndef _AJ_P2P_API_H_
#define _AJ_P2P_API_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
	AJ_P2P_TYPE_NOTDEFINED = 0,
	AJ_P2P_TYPE_DANALE = 1,
	AJ_P2P_TYPE_ANKO = 2,
	AJ_P2P_TYPE_GOOLINK = 3,
	AJ_P2P_TYPE_YUECAM = 4,
	AJ_P2P_TYPE_QQCONNECT = 5,
	AJ_P2P_TYPE_TUTK = 6,
	AJ_P2P_TYPE_EYEPLUS = 7,
	AJ_P2P_TYPE_DOORBELL = 8,
	AJ_P2P_TYPE_CW_NVR = 9,
	AJ_P2P_TYPE_CW_IPCNORMAL = 10,
	AJ_P2P_TYPE_CW_IPCCONSUME = 11,
	AJ_P2P_TYPE_AC18PRO_NVR = 12,
	AJ_P2P_TYPE_AC18PRO_IPCNORMAL = 13,
	AJ_P2P_TYPE_AC18PRO_IPCCONSUME = 14,
	AJ_P2P_TYPE_AC18PRO_CMCC4G = 15,
	AJ_P2P_TYPE_HAIER = 16,
	AJ_P2P_TYPE_TENCENTIOT = 17,
	AJ_P2P_TYPE_AIOT = 18,
} AjP2pType;

#define AJ_P2P_SUCC 0
#define AJ_P2P_ERROR -1

typedef void (*aj_p2p_log_cb)(const char *fmt, ...);

/*************************************************
Function:         aj_load_p2pid_callback
Description:      The callbcack of aj p2p load P2PID data to buffer
buff:             Pointer to the buffer to be written
size:             Size in bytes of buffer to be written.
Return:           The total number of data successfully load is returned.
*************************************************/
typedef int (*aj_load_p2pid_callback)(char *buff, int size);

/*************************************************
Function:         aj_save_p2pid_callback
Description:      The callbcack of aj p2p save the buffer of P2PID data
buff:             Pointer to the buffer to be save
size:             Size in bytes of buffer to be save.
Return:           The total number of data successfully save is returned.
*************************************************/
typedef int (*aj_save_p2pid_callback)(const char *buff, int size, const char *p2pid);

/*************************************************
Function:         aj_getok_p2pid_callback
Description:      The callbcack of aj p2p get get p2pid OK message
使用场合：没有P2PID的情况下，lib自动从server拉取P2PID。拉取成功时，调用本回调通知设备
Return:           .
*************************************************/
typedef void (*aj_getok_p2pid_callback)();

/*************************************************
Function:         aj_p2pid_regetnotify_callback
Description:      The callbcack of aj p2p get re get p2pid message
使用场合：P2PID在server上发现被多个序列号使用，server通知设备重新取P2PID。LIB在清空P2PID后再调用回调。
回调函数中，需要设备停止P2P各项服务，重新读取P2PID，直到读取内容合法，再启动P2P服务
Return:           .
*************************************************/
typedef void (*aj_p2pid_regetnotify_callback)();

/*************************************************
Function:         aj_settime_callback
Description:      The callbcack of aj p2p set time
Return:
*************************************************/
typedef void (*aj_settime_callback)(int gmt_seconds);

/*设置P2P ID服务器地址*/
void AjP2pApiSetServer(
	const char *pServerAddr,
	unsigned short nPortGetP2pid,
	unsigned short nPortHb);

int AjP2pApiInit(
	AjP2pType p2ptype,			 // P2P类型
	int bGetP2PIDFromServer,	 // 没有ID时是否从服务器取ID
	int bSyncTimeFromServer,	 // 从服务器同步时间
	const char *p_szSerialNo,	 // 序列号，最长31位
	const char *p_szDeviceType,	 // 设备型号，最长31位
	const char *p_szCustomName,	 // 定制商名称,最长31位
	const char *p_szVersion,	 // 版本号，V开头
	const char *p_szProductCode, // product code,主要用于标识danale系统内的类型
	const char *p_szUUID		 // 芯片唯一标识字符串
);

// 用于NVR子通道也需要license的情况
int AjP2pApiInit_ex(
	AjP2pType p2ptype,			 // P2P类型
	int bGetP2PIDFromServer,	 // 没有ID时是否从服务器取ID
	int bSyncTimeFromServer,	 // 从服务器同步时间
	const char *p_szSerialNo,	 // 序列号，最长31位
	const char *p_szDeviceType,	 // 设备型号，最长31位
	const char *p_szCustomName,	 // 定制商名称,最长31位
	const char *p_szVersion,	 // 版本号，V开头
	const char *p_szProductCode, // product code,主要用于标识danale系统内的类型
	const char *p_szUUID,		 // 芯片唯一标识字符串
	unsigned int total_channels);

int AjP2pApiSetCallback(
	aj_load_p2pid_callback cbload,
	aj_save_p2pid_callback cbsave,
	aj_getok_p2pid_callback cbgetok,
	aj_p2pid_regetnotify_callback cbreget,
	aj_settime_callback cbsettime,
	aj_p2p_log_cb cblog);

int AjP2pApiStart();
void AjP2pApiExit();

/*计算序列号的云授权码*/
int GetAuthCode(const char *sn, char *buffer, unsigned int buflen);

// 对于需要校验授权码的摄像机，需要校验授权码
void AjP2pSetAuthCode(const char *p_szAuthCode);

// aj_load_p2pid_callback读取到的P2PID FILE有可能是P2PID加密过的内容，需要用P2P LIB解开后才能获得P2PID，例如danale
void AjP2pSetP2pID(const char *p_szP2pID);
// 设置登录状态，登录OK的情况下才向server发送心跳。用于统计在线数据

void AjP2pSetLoginStatus(int bStatus);

// 设置云注册状态，只要有一次云上传的动作，就认为用户已经注册云服务。用于统计在线数据
void AjP2pSetCloudStatus(int bStatus);

// 停止P2P ID获取线程，用于手动上传ID或者其他方式获取到ID后，停止从云服务器拉取ID
void AjP2pStopGetP2pIDFromCloud();

#ifdef __cplusplus
}
#endif

#endif
