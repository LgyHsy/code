#ifndef __ANJ_WIFI_H__
#define __ANJ_WIFI_H__

#include "iwlib.h"

enum
{
	WIFI_TYPE_RT3070 = 0,
	WIFI_TYPE_MT7601 = 1,
	WIFI_TYPE_MT7612 = 2,
	WIFI_TYPE_ATBM603X = 3,
	WIFI_TYPE_SSW101X = 4,
	WIFI_TYPE_SSW105X = 5,
	WIFI_TYPE_AIC8800 = 6,
	WIFI_TYPE_TXW901 = 7,
};

int anj_wifi_init(void);
int anj_wifi_uninit(void);

void anj_wifi_destory();

void anj_wifi_status_set(int status);
int anj_wifi_status_get();

void anj_wifi_thread_set(int status);

void anj_wifi_connect_mode_set(int mode);

int anj_wifi_info_get(const char *ifname, void *info);
int anj_wifi_ap_info_get(void *info);

int anj_wifi_generate_wpa_config(WIFIConfig *pstWifiCfg, int wpaCfgType);
int anj_wifi_start_wpa(int iOverTime, WIFIConfig *pstWifiCfg);

#endif
