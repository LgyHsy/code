/******************************************************************************
Copyright (C), 2010-2012, Skyworth Digital Tech. Co., Ltd.

FileName: skyapi_wireless_net.h 

Description: - set the wireless net 

modification history
--------------------
 ******************************************************************************/


#ifndef SKYAPI_SOUND_WAVE_H_
#define SKYAPI_SOUND_WAVE_H_

/*****************/
/* Include Files */
/*****************/

#include "sk_def.h"

#ifndef u8
typedef unsigned char			u8;
#endif
#ifndef s8
typedef char			s8;
#endif
#ifndef u16
typedef unsigned short int		u16;
#endif
#ifndef s16
typedef short int		s16;
#endif
#ifndef u32
typedef unsigned int			u32;
#endif
#ifndef s32
typedef int				s32;
#endif


//声波获取ssid和psd的状态
typedef enum sky_api_wireless_soundwave_status_e
{
	SK_WIRELESS_SOUNDWAVE_STATUS_FAILED 				= 0,		//成功获取ssid和psd
	SK_WIRELESS_SOUNDWAVE_STATUS_SUCCESS 			= 1,   	//获取ssid和psd失败
	SK_WIRELESS_SOUNDWAVE_STATUS_GETTING 		= 2,		//正在获取音频和解析
}sky_api_wireless_soundwave_status_t;

//声波配网的ssid、psd和状态
typedef struct sky_wireless_soundwave_callback_data_s
{
	//char dev[WIRELESS_DEV_LEN];
	char ssid[64];//no use 
	char pwd[64];//no use

	char conf[512];
	sky_api_wireless_soundwave_status_t status;
}sky_wireless_soundwave_callback_data_t;


/***********************************************************/
/** 
* @param   p_data   声波获取数据的回调函数的参数
*
* @return SK_SUCCESS 成功
* @brief - 声波获取数据的回调函数格式，状态变化时的通知函数
************************************************************/
typedef sk_status_code_t (*skyapi_wireless_soundwave_callback_pfn_t)(const sky_wireless_soundwave_callback_data_t * const p_data);


/***********************************************************/
/** 
* @param   
*
* @return SK_SUCCESS 成功
* @brief - 开始声波获取ssid和psd
************************************************************/

sk_status_code_t skyapi_wireless_soundwave_start(void);

/***********************************************************/
/** 
* @param   
*
* @return SK_SUCCESS 成功
* @brief - 停止声波获取ssid和psd
************************************************************/
sk_status_code_t skyapi_wireless_soundwave_stop(void);

/***********************************************************/
/** 
* @param   
*
* @return 1: buffer已满 0:buffer未满
* @brief - 查看声音buffer是否已满，如果已满，不注入声音数据。
************************************************************/
s32 skyapi_wireless_is_soundwave_full(void);

/***********************************************************/
/** 
* @param  audio   [in]  声音数据
* @param  size   [in]  声音数据大小
*
* @return 1: buffer已满 0:buffer未满
* @brief - 查看声音buffer是否已满，如果已满，不注入声音数据。
************************************************************/
sk_status_code_t skyapi_wireless_soundwave_voicein(void *audio, size_t size);

/***********************************************************/
/** 
* @param   p_fun   回调函数，如果为NULL，那么相当unregister 
*
* @return 原先的回调函数
* @brief - 设置回调函数
************************************************************/
skyapi_wireless_soundwave_callback_pfn_t skyapi_wireless_soundwave_register_callback(skyapi_wireless_soundwave_callback_pfn_t p_fun);

#endif
