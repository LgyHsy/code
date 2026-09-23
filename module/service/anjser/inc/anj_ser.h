#ifndef __ANJ_SER_H__
#define __ANJ_SER_H__

#include "anj_ser_api.h"
#include "anj_ser_provider.h"

#define AIOT_P2PID_FILE_NAME		"/mnt/nand/aiot.id"
#define DANALE_P2PID_FILE_NAME      "/mnt/nand/danale.conf"
#define GOOLINK_P2PID_FILE_NAME     "/mnt/nand/goolinkid.txt"
#define SKYWORTH_P2PID_FILE_NAME    "/mnt/nand/skyworth.txt"
#define AC18PLUS_NVR_P2PID_FILENAME "/mnt/nand/chuangweiid.txt"
#define ISMART_P2PID_FILE_NAME      "/mnt/nand/ismart.txt"
#define TUTK_P2PID_FILE_NAME        "/mnt/nand/tutkuid.txt"
#define CW_P2PID_FILE_NAME          SKYWORTH_P2PID_FILE_NAME
#define TUYA_P2PID_FILE_NAME        "/mnt/nand/tuya.id"
#define TUYA_MAIN_DEV_PID_FILENAME  "/mnt/nand/tuyamainpid.txt" //涂鸦主设备PID
#define TUYA_SUB_DEV_PID_FILENAME   "/mnt/nand/tuyasubpid.txt"  //涂鸦子设备PID
#define EYE_PLUS_SERIAL_ID_FILE     "/mnt/nand/eyeplus_sid.txt"
#define QINIU_FILE_NAME             "/mnt/nand//qnlinking.conf"
#define TENTCENT_IOT_IPC_P2PID_FILENAME "/mnt/nand/tencentiot.txt"
#define DOT_IPC_P2PID_FILENAME      "/mnt/nand/dotid.txt"       //视洞平台
#define AGORA_P2PID_FILENAME        "/mnt/nand/agora_id.txt"    //妙月平台

#define TMP_P2P_ID_FILE_NAME        "/tmp/p2pid.txt"
#define TMP_P2P_STATUS_FILE_NAME    "/tmp/p2pstatus"

void anj_ser_reponse(int func, void *data);

int anj_ser_bind(char *user_name, char *client_code);
void anj_ser_unbind();

void anj_ser_reset_conn(int eNetStatus);

void anj_ser_push_video(int camera_type, int streamtype, int iskey,
                        unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs);
void anj_ser_push_audio(int camera_type, unsigned char *frameBuf, int frameLen, unsigned long long frameTimeMs);

int anj_ser_alarm_handle(int chn, int code, int sub_code, char *pdata);

void anj_ser_simple_unbind();
void anj_ser_remove_unbind_device();
int anj_ser_bind_status_get();

#endif
