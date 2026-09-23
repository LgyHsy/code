#ifndef __ANJ_BIND_H__
#define __ANJ_BIND_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define BIND_DATA_TEST_CHAR           ("T:")
#define BIND_DATA_WIFI_PASSWORD       ("P:")
#define BIND_DATA_WIFI_SSID           ("S:")
#define BIND_DATA_MATCH_CODE          ("C:")
#define BIND_DATA_USER_NAME           ("U:")
#define BIND_DATA_CLIENT_CODE         ("I:")
#define BIND_DATA_END_CHAR            (";")
#define BIND_DATA_BUTN_IN_GAP_CHAR    (',') //空格是间隔符
#define BIND_DATA_INT_NUM_CHAR        ('_') //下划线区别页数
#define BIND_DATA_INT_CHAR            (' ') //空格是间隔符
#define BIND_DATA_MAX_CHAR            (256)//最大字节数

typedef enum
{
    BIND_TYPE_NONE = 0,
    BIND_TYPE_AP,        /*AP绑定*/
    BIND_TYPE_ZXING,     /*二维码绑定*/
    BIND_TYPE_WAVE,      /*声波配对绑定*/
    BIND_TYPE_BLE,       /*蓝牙配对绑定*/
    BIND_TYPE_ONLINE,    /*有线绑定*/
    BIND_TYPE_SCAN_CODE, /*扫码绑定*/
} BIND_TYPE;

typedef struct
{
    BIND_TYPE bindType;
    char router_ssid[64];
    char router_passwd[64];
    char match_code[64];
    char owner_user[64];
    char client_code[64];
} IOTBindConfig;

int anj_bind_data_proc(const char *szInfo, int nLen, BIND_TYPE BindType);
void anj_bind_set(int status);
int anj_bind_get();
void anj_bind_cfg_clean();
void anj_bind_success();

IOTBindConfig *getBindInfo();

#ifdef __cplusplus
}
#endif

#endif
