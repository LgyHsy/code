#ifndef __ANJ_PRI_H__
#define __ANJ_PRI_H__

#include "ixml.h"
#include "protocol_queue.h"
#include "project_option.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_USER_LOGIN_COUNT 100

#define SOCKETFLAG_NOTUSED -1
#define SOCKETFLAG_ALL -2

#define USERSTATUS_NOUSER 0
#define USERSTATUS_CONNECT 1

#define MAX_USER_SESSION 40
#define USERINFO_QUEUE_MAX_NUM 200
#define ANJPRI_QUEUE_MAX_NUM 100

#define PROTOCOL_LEAD_CODE 0x51589158

typedef struct
{
    int user_status;
    int sockfd;
    char session[MAX_USER_SESSION];
    int bCloseSession;
    int ip;
    FRAME_BUFFER_MANAGER bufMgr;
    struct timeval last_send_time;

    unsigned short m_packseq;
    int m_last_keyframe_timeoff;

    int pb_download;
    int sensor_id;

    int clientversion;
    void *pHandle[MAX_VENC_CHN];
} User_Information;

typedef struct
{
    int sock;
    pthread_mutex_t sock_mutex;
    FRAME_BUFFER_MANAGER bufMgr;
    User_Information stUserInfo[MAX_USER_LOGIN_COUNT];
} anj_pri_info;

typedef struct
{
    unsigned long frame_timestamp;    // 此帧对应的时间戳，用于音视频同步，一帧中的不同包时间戳相同
    unsigned long keyframe_timestamp; // 如果是非I帧，记录其前一I帧的timestamp，如果解码器没有收到前面那个I帧，所有非I帧丢掉丢包不解码
    unsigned short pack_seq;          // 包序号0-65535，到最大后从0开始
    unsigned short payload_size;      // 此包中包含有效数据的长度
    unsigned char pack_type;          // 0x01第一包，0x10最后一包, 0x11第一包也是最后一包，0x00中间包
    unsigned char frame_type;         // 帧类型1：I帧，0：非I帧
    unsigned char stream_type;        // 0: video, 1: audio，2：发送报告，3：接收报告，4：打洞包
    unsigned char stream_index;
    unsigned long frame_index; // added by XXX 20100722, index for one stream_type;
} MEDIA_DATA_HEADER;

char *anj_pri_xml_name_get(int lognum);

void anj_pri_file_stop_proc(int msgcode, int lognum, IXML_Document *pDoc);

void *getPriInfo(void);

int anj_pri_init(void);
int anj_pri_uninit(void);

#ifdef __cplusplus
}
#endif

#endif
