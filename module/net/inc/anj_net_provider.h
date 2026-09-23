#ifndef __ANJ_NET_PROVIDER_H__
#define __ANJ_NET_PROVIDER_H__

typedef struct
{
    int (*init)(void);
    int (*uninit)(void);
    void (*set_pause)(void);
    void (*clear_pause)(void);
} anj_net_4g_ops;

typedef struct
{
    int (*init)(void);
    int (*uninit)(void);
    int (*status_get)(void);
    void (*thread_set)(int status);
    int (*info_get)(const char *ifname, void *info);
    int (*ap_info_get)(void *info);
    void (*connect_mode_set)(int mode);
    int (*generate_wpa_config)(void *pstWifiCfg, int wpaCfgType);
    int (*start_wpa)(int iOverTime, void *pstWifiCfg);
} anj_net_wifi_ops;

typedef struct
{
    int (*init)(void);
    int (*uninit)(void);
    int (*recv_config)(void);
    int (*config_invalid)(void);
    int (*config_pwd_err)(void);
    int (*connect_fail)(void);
    int (*p2p_ok)(char *p2pidBuf);
} anj_net_ble_ops;

int anj_net_4g_provider_register(const anj_net_4g_ops *ops);
void anj_net_4g_provider_unregister(const anj_net_4g_ops *ops);
int anj_net_wifi_provider_register(const anj_net_wifi_ops *ops);
void anj_net_wifi_provider_unregister(const anj_net_wifi_ops *ops);
int anj_net_ble_provider_register(const anj_net_ble_ops *ops);
void anj_net_ble_provider_unregister(const anj_net_ble_ops *ops);

int anj_net_provider_4g_init(void);
int anj_net_provider_4g_uninit(void);
void anj_net_provider_4g_set_pause(void);
void anj_net_provider_4g_clear_pause(void);

int anj_net_provider_wifi_init(void);
int anj_net_provider_wifi_uninit(void);
int anj_net_provider_wifi_status_get(void);
void anj_net_provider_wifi_thread_set(int status);
int anj_net_provider_wifi_info_get(const char *ifname, void *info);
int anj_net_provider_wifi_ap_info_get(void *info);
void anj_net_provider_wifi_connect_mode_set(int mode);
int anj_net_provider_wifi_generate_wpa_config(void *pstWifiCfg, int wpaCfgType);
int anj_net_provider_wifi_start_wpa(int iOverTime, void *pstWifiCfg);

int anj_net_provider_ble_init(void);
int anj_net_provider_ble_uninit(void);
int anj_net_provider_ble_recv_config(void);
int anj_net_provider_ble_config_invalid(void);
int anj_net_provider_ble_config_pwd_err(void);
int anj_net_provider_ble_connect_fail(void);
int anj_net_provider_ble_p2p_ok(char *p2pidBuf);

#endif
