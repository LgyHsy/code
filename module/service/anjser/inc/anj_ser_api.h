#ifndef __ANJ_SER_API_H__
#define __ANJ_SER_API_H__

enum
{
    SER_RESPONSE_SDCARD = 0,
    SER_RESPONSE_ABILITY,
    SER_RESPONSE_WIFILIST,
    SER_RESPONSE_PTZ_PRESET,
    SER_RESPONSE_PTZ_ADVANCE_STATE,
    SER_RESPONSE_4G_INIT_DONE,
    SER_RESPONSE_BUIT,
};

typedef struct P2pLoginState_s
{
    int logined;
    char devid[128];

    int enable;
    int p2ptype;
    int cloudstorage;
} P2pLoginState_t;

typedef struct
{
    int cloud_ready;
    char uid[64];
    char secret[64];
    P2pLoginState_t stP2pLoginState;
    char report_dn[64];
    char bindRecvBuffer[1024];
} anj_ser_info;

#define LOCAL_CFG_PATH "/mnt/nand" // 配置文件保存,文件很小，要求保存到flash,重启设备不能删除。
#define SDCARD_PATH "/tmp"         // sd卡挂载目录,按照实际填，不清楚及时跟浪涛咨询
#define LOCAL_PATH_LOG "/tmp/"     // 设备端日志保存路径,一般是sd卡目录
#define P2P_ID_FILE_NAME "/mnt/nand/p2p.id"
#define P2P_DEVICEBIND_FLAG "/mnt/nand/device.bind.flag"
#define P2P_RESET_FLAG "/mnt/nand/reset_button_press.flag"

anj_ser_info *getSerInfo(void);

void anj_ser_cloud_binduser_check(char *pstBindUser);
void anj_ser_cloud_push_alarm(int channel, int eventype, int buploadcloud);
void anj_ser_cloud_push_video(int camera_type, int streamtype, int iskey, unsigned char *frameBuf, unsigned int frameLen);
void anj_ser_cloud_push_audio(int camera_type, unsigned char *frameBuf, unsigned int frameLen);

#endif
