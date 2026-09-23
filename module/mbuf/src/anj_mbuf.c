// ...existing code...
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_mw_media_video.h"
#include "anj_mbuf.h"
#include "zfifo.h"

#define ANJ_MBUF_LOG_FILE "/tmp/mbuf_log"
static anj_thread_s stLogThread;

static int s_stMbufInit = 0;

static ZFIFO *s_media_fifo[ANJ_MBUF_MAX_NUM] = {0};

static ANJ_MBUF_HANDLE s_hVideoWriterId[ANJ_MBUF_MAX_NUM];

static int anj_mbuf_check(int fifo_id)
{
    if (fifo_id < 0 || fifo_id >= ANJ_MBUF_MAX_NUM)
    {
        __ERR("param error. %d\n", fifo_id);
        return -1;
    }

    return 0;
}

static int anj_mbuf_create(int fifo_id, int size)
{
    if ((0 != anj_mbuf_check(fifo_id)) || (size <= 0))
    {
        __ERR("param error. %d, %d\n", fifo_id, size);
        return -1;
    }

    if (s_media_fifo[fifo_id] != NULL)
    {
        __ERR("Mbuf %d has been initialized.\n", fifo_id);
        return 0;
    }

    char name[10] = {0};
    sprintf(name, "stream%d", fifo_id);
    s_media_fifo[fifo_id] = zfifo_init(name, size);
    if (s_media_fifo[fifo_id] == NULL)
    {
        __ERR("zfifo_init %s failed.\n", name);
        return -1;
    }

    return 0;
}

static int anj_mbuf_destroy(int fifo_id)
{
    if (0 != anj_mbuf_check(fifo_id))
    {
        return -1;
    }

    if (s_media_fifo[fifo_id] == NULL)
    {
        return 0;
    }

    zfifo_uninit(s_media_fifo[fifo_id]);
    s_media_fifo[fifo_id] = NULL;

    return 0;
}

static ANJ_MBUF_HANDLE *anj_mbuf_add_writer(int fifo_id)
{
    if (0 != anj_mbuf_check(fifo_id))
    {
        return NULL;
    }
    ZFIFO *zfifo = s_media_fifo[fifo_id];
    /* 使用新的 writer open */
    ZFIFO_DESC *id = zfifo_open_writer(zfifo);
    if (id == NULL)
    {
        __ERR("zfifo_open_writer stream%d failed.\n", fifo_id);
        return NULL;
    }

    return (ANJ_MBUF_HANDLE *)id;
}

static void anj_mbuf_del_writer(ANJ_MBUF_HANDLE *writerid)
{
    ZFIFO_DESC *zdesc = (ZFIFO_DESC *)writerid;
    if (writerid == NULL)
    {
        __ERR("param error.\n");
        return;
    }

    zfifo_close(zdesc);
}

static ZFIFO_DESC *anj_mbuf_add_reader(int fifo_id, int bLastTime)
{
    if (0 != anj_mbuf_check(fifo_id))
    {
        return NULL;
    }

    ZFIFO *zfifo = s_media_fifo[fifo_id];
    ZFIFO_DESC *id = zfifo_open_reader(zfifo);
    if (id == NULL)
    {
        __ERR("zfifo_open_reader stream%d failed.\n", fifo_id);
        return NULL;
    }

    if (bLastTime)
    {
        zfifo_set_newest_frame(id);
    }
    else
    {
        zfifo_set_oldest_frame(id);
    }
    return (ZFIFO_DESC *)id;
}

static void anj_mbuf_del_reader(ZFIFO_DESC *readerid)
{
    ZFIFO_DESC *zdesc = (ZFIFO_DESC *)readerid;
    if (readerid == NULL)
    {
        __ERR("param error.\n");
        return;
    }
    zfifo_close(zdesc);
}

static int anj_mbuf_write_frame(ZFIFO_DESC *writerid, media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    ANJ_CHK((writerid != NULL) && (pFrameInfo != NULL), -1, "input invaild!\n");
    ANJ_CHK((pFrameInfo->frameParam.frameLen > 0), -1, "input invaild!\n");

    ANJ_MBUF_HEADER_T stMFrameHeader;
    ZFIFO_NODE node[ANJ_MBUF_MAX];
    node[ANJ_MBUF_HEADER].base = &stMFrameHeader;
    node[ANJ_MBUF_HEADER].len = sizeof(stMFrameHeader);

    memset(&stMFrameHeader, 0, sizeof(stMFrameHeader));
    stMFrameHeader.magicStart = MEDIA_MAGIC_START;
    stMFrameHeader.magicEnd = MEDIA_MAGIC_END;
    memcpy(&stMFrameHeader.frameParam, &pFrameInfo->frameParam, sizeof(stMFrameHeader.frameParam));

    node[ANJ_MBUF_DATA].base = pFrameInfo->frameBuf;
    node[ANJ_MBUF_DATA].len = pFrameInfo->frameParam.frameLen;

    /* zfifo_writev 会在写前检查是否会覆盖未读数据，若会覆盖则返回 -1 */
    iRet = zfifo_writev((ZFIFO_DESC *)writerid, node, ANJ_MBUF_MAX, (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I));
    if (iRet <= 0)
    {
        __ERR("zfifo_writev error. %d\n", iRet);
    }

endFunc:
    return iRet;
}

static int anj_mbuf_poper_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    ANJ_MBUF_POPER *pPoper = (ANJ_MBUF_POPER *)ctx;
    ANJ_CHK(((NULL != pPoper) && (NULL != bStart) && (NULL != pPoper->poperCb)), -1, "Invalid Input");

    __INFO("{start stream} (ch:%d)\n", pPoper->iPopCh);

    ANJ_MBUF_HANDLE *readerid = NULL;
    media_frame_info_t stReadFrameInfo = {0};
    while (*bStart && pPoper->bOpen)
    {
        ANJ_CHK(((0 != *bStart) && (0 != pPoper->bOpen)), -1, "not start");

        readerid = anj_mbuf_create_reader(pPoper->iPopCh, 1);
        if (readerid == NULL)
        {
            usleep(100 * 1000);
            continue;
        }
        __INFO("anj_mbuf_create_reader stream%d readerid:%p OK!\n", pPoper->iPopCh, readerid);

        int bFirstFrame = 1;
        int iLastVFrameIndex = 0;
        while (bStart && *bStart)
        {
            if (0 < anj_mbuf_read_frame(readerid, bFirstFrame, &stReadFrameInfo, 100))
            {
                // drop frame wait key
                if (stReadFrameInfo.frameParam.frameType != MEDIA_AFRAME_A)
                {
                    if ((iLastVFrameIndex != 0) && (stReadFrameInfo.frameParam.vframeIndex != iLastVFrameIndex + 1))
                    {
                        __INFO("DROP FRAME %d %d!\n", stReadFrameInfo.frameParam.vframeIndex, iLastVFrameIndex);
                        bFirstFrame = 1;
                    }
                    iLastVFrameIndex = stReadFrameInfo.frameParam.vframeIndex;
                    if (stReadFrameInfo.frameParam.frameType == MEDIA_VFRAME_I)
                    {
                        bFirstFrame = 0;
                    }
                }

                // first frame must I frame
                if (bFirstFrame == 0)
                {
                    pPoper->poperCb(pPoper, &stReadFrameInfo);
                }
                anj_mbuf_read_release(readerid, &stReadFrameInfo);
            }
        }

        anj_mbuf_destory_reader(readerid);
    }

endFunc:
    __ERR("{exit stream} (req:%d, chn:%d, start:%d, open:%d iRet:%d)\n",
          pPoper->iPopReq, pPoper->iPopCh, *bStart, pPoper->bOpen, iRet);
    return iRet;
}

/* 读取：浅拷贝（返回指向 zfifo 内部缓冲区的指针），并把 frameParam 从 header 解析到 pFrameInfo 中
   必须在使用完数据后调用 anj_mbuf_read_release 来推进 reader 指针（释放对内部数据的占用）。
*/
int anj_mbuf_read_frame(ANJ_MBUF_HANDLE readerid, int bKeyFrame, media_frame_info_t *pFrameInfo, int iTimeouts)
{
    int iRet = -1;
    ANJ_CHK((0 != s_stMbufInit), -1, "not init");
    ANJ_CHK((readerid != NULL) && (pFrameInfo != NULL), -1, "input invaild!\n");

    ANJ_MBUF_HEADER_T *pstHeader = NULL;
    ZFIFO_DESC *zfifo_desc = (ZFIFO_DESC *)readerid;

    struct timeval tv_start, tv_now;
    long remaining = iTimeouts; /* ms */
    gettimeofday(&tv_start, NULL);

    while (1)
    {
        /* 首先 peek header，获取 frameLen / frameType */
        ZFIFO_NODE header_node[1];
        header_node[ANJ_MBUF_HEADER].len = sizeof(ANJ_MBUF_HEADER_T);
        header_node[ANJ_MBUF_HEADER].base = NULL;

        int ret = zfifo_peek(zfifo_desc, header_node, 1, remaining);
        if (ret < 0)
        {
            __ERR("zfifo_peek header error. %d\n", ret);
            goto endFunc;
        }
        else if (ret == 0)
        {
            /* timeout or no data */
            iRet = 0;
            goto endFunc;
        }

        pstHeader = (ANJ_MBUF_HEADER_T *)header_node[ANJ_MBUF_HEADER].base;
        if ((pstHeader == NULL) ||
            (pstHeader->magicStart != MEDIA_MAGIC_START) || (pstHeader->magicEnd != MEDIA_MAGIC_END))
        {
            __ERR("zfifo read error.%p\n", pstHeader);
            iRet = -1;
            goto endFunc;
        }

        if (pstHeader->frameParam.frameLen <= 0)
        {
            __ERR("anj_mbuf_read_frame: invalid frameLen=%d\n", pstHeader->frameParam.frameLen);
            iRet = -1;
            goto endFunc;
        }
        int frameLen = pstHeader->frameParam.frameLen;
        int frameType = pstHeader->frameParam.frameType;

        /* 若要求只取 I 帧且当前不是 I 帧，则跳过该帧（推进 reader 指针），继续查找 */
        if ((bKeyFrame == 1) && (frameType != MEDIA_VFRAME_I))
        {
            int skip_n = zfifo_read_release(zfifo_desc, ANJ_MBUF_MAX); /* 跳过 header+data */
            if (skip_n <= 0)
            {
                __ERR("zfifo read_release skip failed.%d\n", skip_n);
                iRet = -1;
                goto endFunc;
            }

            if (iTimeouts > 0)
            {
                gettimeofday(&tv_now, NULL);
                long elapsed = (tv_now.tv_sec - tv_start.tv_sec) * 1000 + (tv_now.tv_usec - tv_start.tv_usec) / 1000;
                remaining = iTimeouts - elapsed;
                if (remaining <= 0)
                {
                    iRet = 0;
                    goto endFunc;
                }
            }
            /* 继续循环查找下一个帧 */
            continue;
        }

        /* 否则要返回当前帧：再次 peek header+data 获得 data 指针（浅拷贝） */
        ZFIFO_NODE node[ANJ_MBUF_MAX];
        node[ANJ_MBUF_HEADER].len = sizeof(ANJ_MBUF_HEADER_T);
        node[ANJ_MBUF_HEADER].base = NULL;
        node[ANJ_MBUF_DATA].len = frameLen;
        node[ANJ_MBUF_DATA].base = NULL;

        ret = zfifo_peek(zfifo_desc, node, ANJ_MBUF_MAX, remaining);
        if (ret < 0)
        {
            __ERR("zfifo_peek hdr+data error. %d\n", ret);
            goto endFunc;
        }
        else if (ret == 0)
        {
            iRet = 0;
            goto endFunc;
        }

        /* 设置输出：浅拷贝 frameBuf 指向内部 buffer，caller 必须 调用 anj_mbuf_read_release */
        pstHeader = (ANJ_MBUF_HEADER_T *)node[ANJ_MBUF_HEADER].base;
        if ((pstHeader == NULL) ||
            (pstHeader->magicStart != MEDIA_MAGIC_START) || (pstHeader->magicEnd != MEDIA_MAGIC_END))
        {
            __ERR("zfifo read error.%p\n", pstHeader);
            iRet = -1;
            goto endFunc;
        }

        memcpy(&pFrameInfo->frameParam, &pstHeader->frameParam, sizeof(pFrameInfo->frameParam));
        pFrameInfo->frameBuf = (unsigned char *)node[ANJ_MBUF_DATA].base;

        iRet = ret;
        goto endFunc;
    }

endFunc:
    return iRet;
}

/* read release: 推进 reader 指针（对应之前读到的数据），并返回推进字节数（或 -1 错误） */
int anj_mbuf_read_release(ANJ_MBUF_HANDLE readerid, media_frame_info_t *pFrameInfo)
{
    ZFIFO_DESC *zfifo_desc = (ZFIFO_DESC *)readerid;
    if (s_stMbufInit == 0 || zfifo_desc == NULL)
        return -1;

    /* 传入 iovcnt = ANJ_MBUF_MAX（header + data） */
    int n = zfifo_read_release(zfifo_desc, ANJ_MBUF_MAX);
    return n;
}

int anj_mbuf_video_write_frame(int mbufId, media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    ANJ_CHK((0 != s_stMbufInit), -1, "not init");
    if (s_hVideoWriterId[mbufId] && (0 == anj_mbuf_check(mbufId)))
    {
        iRet = anj_mbuf_write_frame((ZFIFO_DESC *)s_hVideoWriterId[mbufId], pFrameInfo);
    }
    else
    {
        __ERR("Invalid Write Id:%d\n", mbufId);
    }
endFunc:
    return iRet;
}

int anj_mbuf_audio_write_frame(media_frame_info_t *pFrameInfo)
{
    int iRet = -1;
    ANJ_CHK((0 != s_stMbufInit), -1, "not init");
    for (int i = 0; i < ANJ_MBUF_MAX_NUM; i++)
    {
        if (s_hVideoWriterId[i] && (0 == anj_mbuf_check(i)))
        {
            iRet = anj_mbuf_write_frame((ZFIFO_DESC *)s_hVideoWriterId[i], pFrameInfo);
        }
    }

endFunc:
    return iRet;
}

ANJ_MBUF_HANDLE *anj_mbuf_create_reader(int mbufId, int bLastTime)
{
    if (0 == s_stMbufInit || 0 != anj_mbuf_check(mbufId))
    {
        __ERR("mbufId:%d invaild!\n", mbufId);
        return NULL;
    }
    return (ANJ_MBUF_HANDLE *)anj_mbuf_add_reader(mbufId, bLastTime);
}

int anj_mbuf_destory_reader(ANJ_MBUF_HANDLE *readerid)
{
    if (s_stMbufInit)
    {
        anj_mbuf_del_reader((ZFIFO_DESC *)readerid);
    }
    return 0;
}

static int anj_mbuf_print_info_thread(void *ctx, int *bStart)
{
    (void)ctx;
    int max_waste_bytes[ANJ_MBUF_MAX_NUM] = {0};

    // 打印线程信息
    __INFO("mbuf info thread start. name: %s, tid: %lu\n",
           stLogThread.iThreadName, (unsigned long)pthread_self());

    while (bStart && *bStart)
    {
        if (!anj_mw_file_exists(ANJ_MBUF_LOG_FILE))
        {
            sleep(1);
            continue;
        }

        for (int i = 0; i < ANJ_MBUF_MAX_NUM; i++)
        {
            const char *name = "unknown";
            int size = 0;
            int remain_percent = 0;
            unsigned int waste_events = 0;
            int last_waste_bytes = 0;

            if (s_media_fifo[i] == NULL)
            {
                continue;
            }

            if (zfifo_get_info(s_media_fifo[i], &remain_percent) != 0)
            {
                remain_percent = 0;
            }
            name = s_media_fifo[i]->name ? s_media_fifo[i]->name : "unknown";
            size = s_media_fifo[i]->buf_size;
            waste_events = s_media_fifo[i]->wrap_waste_events;
            last_waste_bytes = s_media_fifo[i]->last_wrap_waste_bytes;

            if (last_waste_bytes > max_waste_bytes[i])
            {
                max_waste_bytes[i] = last_waste_bytes;
            }

            __INFO("mbuf info: name=%s size=%8d remain=%3d%% waste(ev:%u,last:%d,max:%d)\n",
                   name,
                   size,
                   remain_percent,
                   waste_events,
                   last_waste_bytes,
                   max_waste_bytes[i]);
        }

        usleep(1000 * 1000);
    }

    return 0;
}

int anj_mbuf_init(void *pAnjVideoCfg)
{
    int iRet = -1;
    ANJ_CHK((0 == s_stMbufInit), -1, "has been init");
    ANJ_CHK((pAnjVideoCfg != NULL), -1, "input invaild!\n");

    int iMbufSize = 0;
    AnjVideoConfig *pstAnjVideoCfg = (AnjVideoConfig *)pAnjVideoCfg;
    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        AnjVencConfig *pstVencCfg = &pstAnjVideoCfg->stVencCfg[i];
        if (pstVencCfg->enable && (pstVencCfg->encodeType != MEDIA_CODEC_VIDEO_JPG))
        {
            ANJ_CHK_FUNC(anj_mbuf_check(pstVencCfg->chn), 0, "mbuf id invalid!\n");

            iMbufSize = (i == ANJ_MBUF_VSTREAM_MAIN) ? (ANJ_CAMERA_VIDEO_MAX_SIZE * ANJ_CAMERA_PRE_RECORD_TIMES) : ANJ_CAMERA_VIDEO_SUB_MAX_SIZE;
            __INFO("mbufId:%d iMbufSize %d.\n", pstVencCfg->chn, iMbufSize);
            ANJ_CHK_FUNC(anj_mbuf_create(pstVencCfg->chn, iMbufSize), 0, "anj_mbuf_create failed!\n");

            if (s_hVideoWriterId[pstVencCfg->chn] == NULL)
            {
                s_hVideoWriterId[pstVencCfg->chn] = anj_mbuf_add_writer(pstVencCfg->chn);
                ANJ_CHK((s_hVideoWriterId[pstVencCfg->chn] != NULL), -1, "Create mbuf failed!\n");
            }
            else
            {
                __ERR("s_hVideoWriterId %d has been initialized.\n", pstVencCfg->chn);
            }
        }
    }

    // 创建线程打印log日志
    stLogThread.bAutoDestroy = 1;
    snprintf(stLogThread.iThreadName, sizeof(stLogThread.iThreadName), "mbuf_print_info");
    stLogThread.iThreadjob.ctx = NULL;
    stLogThread.iThreadjob.func = anj_mbuf_print_info_thread;
    iRet = anj_thread_task_create(&stLogThread);
    s_stMbufInit = 1;

endFunc:
    if (iRet)
    {
        anj_mbuf_uninit(pAnjVideoCfg);
    }

    return iRet;
}

int anj_mbuf_uninit(void *pAnjVideoCfg)
{
    if (0 == s_stMbufInit)
    {
        __ERR("not init\n");
        return -1;
    }
    AnjVideoConfig *pstAnjVideoCfg = (AnjVideoConfig *)pAnjVideoCfg;
    anj_thread_task_destroy(&stLogThread, 0);

    for (int i = 0; i < MAX_VENC_CHN; i++)
    {
        AnjVencConfig *pstVencCfg = &pstAnjVideoCfg->stVencCfg[i];
        if (anj_mbuf_check(pstVencCfg->chn) != 0)
        {
            continue;
        }
        if (s_hVideoWriterId[pstVencCfg->chn])
        {
            anj_mbuf_del_writer(s_hVideoWriterId[pstVencCfg->chn]);
            anj_mbuf_destroy(pstVencCfg->chn);
            s_hVideoWriterId[pstVencCfg->chn] = NULL;
        }
    }
    s_stMbufInit = 0;
    return 0;
}

int anj_mbuf_poper_create(int camera, int chn, ANJ_MBUF_POPER *pPoper, poper_cb cb, int iPopId, ANJ_MBUF_POPER_REQUEST_E requester)
{
    int iRet = 0;
    ANJ_CHK((0 != s_stMbufInit), -1, "not init");
    ANJ_CHK(((NULL != pPoper) && (NULL != cb)), -1, "Invalid Input");
    ANJ_CHK(((ANJ_CAMERA_MAX_NUMS > camera) && (MAX_VENC_CHN > chn)), -1, "Invalid Input");
    int videoNum = camera * ANJ_CAMERA_MAX_NUMS + chn;
    memset(pPoper, 0, sizeof(ANJ_MBUF_POPER));
    pPoper->bOpen = 1;
    pPoper->poperCb = cb;
    pPoper->iPopCh = videoNum;
    pPoper->iPopId = iPopId;

    pPoper->stThread.bAutoDestroy = 0;
    snprintf(pPoper->stThread.iThreadName, sizeof(pPoper->stThread.iThreadName),
             "stream_%d_%d", requester, pPoper->iPopCh);
    pPoper->stThread.iThreadjob.ctx = (void *)pPoper;
    pPoper->stThread.iThreadjob.func = anj_mbuf_poper_thread;
    iRet = anj_thread_task_create(&pPoper->stThread);
endFunc:
    return iRet;
}

void anj_mbuf_poper_release(ANJ_MBUF_POPER *pPoper)
{
    if (s_stMbufInit && NULL != pPoper)
    {
        pPoper->bOpen = 0;
        anj_thread_task_destroy(&pPoper->stThread, 0);
    }
}
