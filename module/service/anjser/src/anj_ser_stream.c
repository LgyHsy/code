#include "anj_mw_comm.h"
#include "anj_mbuf.h"
#include "anj_ser.h"
#include "anj_ser_stream.h"

typedef enum
{
    AIOT_BULLET_CAMERA = 0,
    AIOT_DOME_CAMERA,
} AIOT_CAMERA_TYPE;

static ANJ_MBUF_POPER pstPoperArray[ANJ_CAMERA_MAX_NUMS][MAX_VENC_CHN];

static int anj_ser_stream_is_valid(ANJ_MBUF_POPER *hPoperHandle)
{
    for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        for (int j = 0; j < MAX_VENC_CHN; j++)
        {
            if (pstPoperArray[i][j].iPopId == hPoperHandle->iPopId)
            {
                ANJ_MBUF_POPER *pPoper = &pstPoperArray[i][j];
                if (pPoper->bOpen)
                {
                    return 1;
                }
                else
                {
                    return 0;
                }
            }
        }
    }
    return 0;
}

static int anj_ser_stream_cb(void *pHandle, media_frame_info_t *pFrameInfo)
{
    int iRet = 0;
    ANJ_MBUF_POPER *pPoper = (ANJ_MBUF_POPER *)pHandle;
    ANJ_CHK(((NULL != pPoper) && (NULL != pFrameInfo)), -1, "Invalid Input");
    if (!anj_ser_stream_is_valid(pPoper))
    {
        __ERR("{exit stream} invalid pPoper = 0x%p\n", pPoper);
        iRet = -1;
        goto endFunc;
    }

    if (anj_mw_file_exists(P2P_DEVICEBIND_FLAG) == 0)
    {
        goto endFunc;
    }

    int streamtype = 0;
    AIOT_CAMERA_TYPE camera_type = AIOT_BULLET_CAMERA;
    if (ANJ_CAMERA_MAX_NUMS > 1)
    {
        int channel_index = pPoper->iPopCh / ANJ_CAMERA_MAX_NUMS;
        streamtype = pPoper->iPopCh % ANJ_CAMERA_MAX_NUMS;

        camera_type = (channel_index == 0) ? AIOT_DOME_CAMERA : AIOT_BULLET_CAMERA;
    }
    else
    {
        streamtype = pPoper->iPopCh;
    }
    if (pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A)
    {
        int iskey = (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I) ? 1 : 0;
        anj_ser_push_video(camera_type, streamtype, iskey,
                           pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen, pFrameInfo->frameParam.frameTimeMs);
        anj_ser_cloud_push_video(camera_type, streamtype, iskey, pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen);
    }
    else
    {
        if (streamtype == 0)
        {
            anj_ser_push_audio(camera_type, pFrameInfo->frameBuf,
                            pFrameInfo->frameParam.frameLen, pFrameInfo->frameParam.frameTimeMs);
            anj_ser_cloud_push_audio(camera_type, pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen);
        }
    }

endFunc:
    return iRet;
}

void anj_ser_stream_create()
{
    for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        for (int j = 0; j < MAX_VENC_CHN; j++)
        {
            __INFO("========= Start stream nChannelNo:%d nStreamNo:%d\n", i, j);
            ANJ_MBUF_POPER *pPoper = &pstPoperArray[i][j];
            memset(pPoper, 0, sizeof(ANJ_MBUF_POPER));
            anj_mbuf_poper_create(i, j, pPoper, anj_ser_stream_cb, rand(), ANJ_MBUF_POPER_REQUEST_SER);
        }
    }
}

void anj_ser_stream_release()
{
    for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
    {
        for (int j = 0; j < MAX_VENC_CHN; j++)
        {
            ANJ_MBUF_POPER *pPoper = &pstPoperArray[i][j];
            // release must sweep all popers; one invalid entry should not abort the rest
            {
                if (!anj_ser_stream_is_valid(pPoper))
                {
                    memset(pPoper, 0, sizeof(ANJ_MBUF_POPER));
                    continue;
                }
                anj_mbuf_poper_release(pPoper);
                memset(pPoper, 0, sizeof(ANJ_MBUF_POPER));
            }
        }
    }
}
