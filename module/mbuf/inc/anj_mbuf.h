#ifndef __ANJ_MBUF_H__
#define __ANJ_MBUF_H__

#include "zfifo.h"
#include "media_util.h"
#include "anj_mw_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ANJ_MBUF_HEADER = 0,
    ANJ_MBUF_DATA = 1,
    ANJ_MBUF_MAX,
} ANJ_MBUF_TYPE_E;

typedef enum {
    ANJ_MBUF_VSTREAM_MAIN = 0,
    ANJ_MBUF_VSTREAM_SUB = 1,
    ANJ_MBUF_VSTREAM_BUTT,
} ANJ_MBUF_VSTREAM_E;

typedef enum {
    ANJ_MBUF_POPER_REQUEST_SER = 0,
    ANJ_MBUF_POPER_REQUEST_RECORD,
    ANJ_MBUF_POPER_REQUEST_RTSP,
    ANJ_MBUF_POPER_REQUEST_GB28181,
} ANJ_MBUF_POPER_REQUEST_E;

#define ANJ_CAMERA_PRE_RECORD_TIMES     (4)         // pre record times(second)
#define ANJ_MBUF_MAX_NUM                ((ANJ_CAMERA_MAX_NUMS * ANJ_MBUF_VSTREAM_BUTT) + 1)

#define MEDIA_MAGIC_START (0x5a5a5a5a)
#define MEDIA_MAGIC_END (0xa5a5a5a5)

typedef void* ANJ_MBUF_HANDLE;

/*! Media buffer frame header */
typedef struct {
    unsigned int magicStart;         /* MEDIA_MAGIC_START */
    media_frame_param_t frameParam;
    unsigned int magicEnd;         /* MEDIA_MAGIC_END */
} ANJ_MBUF_HEADER_T;

typedef int (*poper_cb)(void *pPoper, media_frame_info_t *pFrameInfo);

typedef struct
{
    int bOpen;
    anj_thread_s stThread;
    pthread_mutex_t iPopMutex;
    int iPopReq;
    int iPopCh;
    int iPopId;
    poper_cb poperCb;
} ANJ_MBUF_POPER;

int anj_mbuf_read_frame(ANJ_MBUF_HANDLE readerid, int bKeyFrame, media_frame_info_t *pFrameInfo, int iTimeouts);
int anj_mbuf_read_release(ANJ_MBUF_HANDLE readerid, media_frame_info_t *pFrameInfo);

int anj_mbuf_video_write_frame(int mbufId, media_frame_info_t *pFrameInfo);

int anj_mbuf_audio_write_frame(media_frame_info_t *pFrameInfo);

ANJ_MBUF_HANDLE* anj_mbuf_create_reader(int mbufId, int bLastTime);

int anj_mbuf_destory_reader(ANJ_MBUF_HANDLE* readerid);

int anj_mbuf_init(void *pstAnjVideoCfg);

int anj_mbuf_uninit(void *pstAnjVideoCfg);

int anj_mbuf_poper_create(int camera, int chn, ANJ_MBUF_POPER *pPoper, poper_cb cb, int iPopId, ANJ_MBUF_POPER_REQUEST_E requester);
void anj_mbuf_poper_release(ANJ_MBUF_POPER *pPoper);

#ifdef __cplusplus
};
#endif
#endif


