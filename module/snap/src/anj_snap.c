#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mw_mutex.h"
#include "anj_mw_mem.h"
#include "anj_mw_file.h"
#include "anj_module.h"
#include "anj_snap.h"
#include "anj_snap_provider.h"
#include "anj_pri_cmd.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct AnjSnapReq
{
    int cam;    /* 摄像头索引 */
    int stream; /* 码流：0=主 1=子 */
    int sessionid;
    char filename[128];
    int quality;
    int width;
    int height;
    AreaStruct are;
    int abandoned;
    unsigned char *yuv_data;
    int yuv_len;
    int yuv_ready;
    struct AnjSnapReq *next;
} AnjSnapReq;

typedef struct
{
    AnjSnapReq *request_list;
    pthread_mutex_t lock;
    anj_thread_s worker;
    AnjSnapReq *encoding;
    unsigned char *soft_yuv_buf;
    int soft_yuv_cap;
    int soft_yuv_in_use;
    pthread_cond_t soft_cond;
} AnjSnapManager;

/*
 * 单目/双目共用一套抓拍管道（队列 + worker + 硬编 JPEG chn）。
 * 差异只在请求里的 cam：选哪路 YUV 送进该管道。
 */
static AnjSnapManager gstAnjSnapMng = {0};
static int s_stSnapInit = 0;

static void anj_snap_release_yuv(AnjSnapManager *manager, AnjSnapReq *req)
{
    if (manager == NULL || req == NULL || req->yuv_data == NULL)
    {
        return;
    }

    if (req->yuv_data == manager->soft_yuv_buf)
    {
        manager->soft_yuv_in_use = 0;
    }
    else
    {
        anj_mw_free(req->yuv_data);
    }
    req->yuv_data = NULL;
    req->yuv_len = 0;
    req->yuv_ready = 0;
}

static int anj_snap_worker_thread(void *ctx, int *bStart)
{
    AnjSnapManager *manager = (AnjSnapManager *)ctx;
    const anj_snap_provider_ops *ops = anj_snap_provider_get();

    while (bStart && *bStart)
    {
        AnjSnapReq *current = NULL;
        int ok = 0;
        int need_stop_yuv = 0;
        int cam = 0;

        anj_mutex_lock(&manager->lock);
        while (bStart && *bStart)
        {
            current = manager->request_list;
            if (current == NULL)
            {
                anj_condition_wait(&manager->soft_cond, &manager->lock, 50);
                continue;
            }
            /* 软编等 YUV；硬编无 start_yuv，直接处理 */
            if (ops && ops->start_yuv && !current->yuv_ready && !current->abandoned)
            {
                anj_condition_wait(&manager->soft_cond, &manager->lock, 50);
                continue;
            }
            break;
        }

        if (!(bStart && *bStart) || current == NULL)
        {
            anj_mutex_unlock(&manager->lock);
            continue;
        }

        manager->request_list = current->next;
        current->next = NULL;
        manager->encoding = current;
        cam = current->cam;
        anj_mutex_unlock(&manager->lock);

        if (!current->abandoned && ops && ops->capture)
        {
            int ret = ops->capture(cam, current->stream, current->quality, current->filename,
                                   &current->are, current->yuv_data, current->width, current->height);
            ok = (ret > 0);
            if (ok && current->abandoned)
            {
                remove(current->filename);
                ok = 0;
            }
        }

        anj_pri_cmd_jpg_reponse(ok, current->sessionid, current->filename);

        anj_mutex_lock(&manager->lock);
        manager->encoding = NULL;
        anj_snap_release_yuv(manager, current);
        anj_mw_free(current);
        if (manager->request_list == NULL)
        {
            need_stop_yuv = 1;
        }
        anj_condition_signal(&manager->soft_cond, 0);
        anj_mutex_unlock(&manager->lock);

        if (need_stop_yuv && ops && ops->stop_yuv)
        {
            ops->stop_yuv(cam);
        }
    }

    return 0;
}

static void anj_snap_mgr_init(void)
{
    AnjSnapManager *manager = &gstAnjSnapMng;

    anj_mutex_create(&manager->lock, 0);
    anj_condition_create(&manager->soft_cond);
    manager->request_list = NULL;
    manager->encoding = NULL;
    manager->soft_yuv_buf = NULL;
    manager->soft_yuv_cap = 0;
    manager->soft_yuv_in_use = 0;

    manager->worker.bAutoDestroy = 0;
    strncpy(manager->worker.iThreadName, "rm_jpg", sizeof(manager->worker.iThreadName) - 1);
    manager->worker.iThreadjob.ctx = manager;
    manager->worker.iThreadjob.func = anj_snap_worker_thread;
    anj_thread_task_create(&manager->worker);
}

static void anj_snap_mgr_uninit(void)
{
    AnjSnapManager *manager = &gstAnjSnapMng;

    if (manager->worker.start)
    {
        anj_condition_signal(&manager->soft_cond, 1);
        anj_thread_task_destroy(&manager->worker, 0);
        memset(&manager->worker, 0, sizeof(manager->worker));
    }

    anj_mutex_lock(&manager->lock);
    AnjSnapReq *current = manager->request_list;
    while (current)
    {
        AnjSnapReq *next = current->next;
        anj_snap_release_yuv(manager, current);
        anj_mw_free(current);
        current = next;
    }
    manager->request_list = NULL;
    if (manager->encoding)
    {
        anj_snap_release_yuv(manager, manager->encoding);
        anj_mw_free(manager->encoding);
        manager->encoding = NULL;
    }
    if (manager->soft_yuv_buf)
    {
        anj_mw_free(manager->soft_yuv_buf);
        manager->soft_yuv_buf = NULL;
        manager->soft_yuv_cap = 0;
    }
    anj_mutex_unlock(&manager->lock);
    anj_condition_destroy(&manager->soft_cond);
    anj_mutex_destroy(&manager->lock);
}

static void anj_snap_discard(const char *filename)
{
    AnjSnapManager *manager = &gstAnjSnapMng;

    if (filename == NULL || filename[0] == '\0')
    {
        return;
    }

    remove(filename);

    anj_mutex_lock(&manager->lock);
    if (manager->encoding != NULL && strcmp(manager->encoding->filename, filename) == 0)
    {
        manager->encoding->abandoned = 1;
        anj_mutex_unlock(&manager->lock);
        return;
    }
    AnjSnapReq *prev = NULL;
    AnjSnapReq *cur = manager->request_list;
    while (cur != NULL)
    {
        AnjSnapReq *next = cur->next;
        if (strcmp(cur->filename, filename) == 0)
        {
            if (prev)
            {
                prev->next = next;
            }
            else
            {
                manager->request_list = next;
            }
            anj_snap_release_yuv(manager, cur);
            anj_condition_signal(&manager->soft_cond, 0);
            anj_mw_free(cur);
            break;
        }
        prev = cur;
        cur = next;
    }
    anj_mutex_unlock(&manager->lock);
}

int anj_snap_wait_complete(const char *filename, int timeout_ms)
{
    if (filename == NULL)
    {
        return -1;
    }

    if (0 == wait_for_jpeg_complete(filename, timeout_ms))
    {
        return 0;
    }

    anj_snap_discard(filename);
    return -1;
}

void anj_snap_on_yuv(int iCameraIdex, void *p_vir_addr, unsigned long long p_phy_addr, int len, void *param)
{
    (void)p_phy_addr;
    (void)len;
    (void)param;

    int align_h;
    int yuv_len;
    AnjSnapManager *manager = &gstAnjSnapMng;

    if (s_stSnapInit == 0 || p_vir_addr == NULL)
        return;

    anj_mutex_lock(&manager->lock);
    AnjSnapReq *current = manager->request_list;
    /* 只吃当前请求指定源的 YUV */
    if (!current || current->cam != iCameraIdex || current->yuv_ready ||
        current->abandoned || manager->soft_yuv_in_use)
    {
        anj_mutex_unlock(&manager->lock);
        return;
    }

    align_h = ANJ_ALIGN_UP(current->height, 32);
    yuv_len = current->width * align_h * 3 / 2;
    if (yuv_len <= 0)
    {
        anj_mutex_unlock(&manager->lock);
        return;
    }
    if (manager->soft_yuv_buf == NULL || manager->soft_yuv_cap < yuv_len)
    {
        unsigned char *buf = (unsigned char *)anj_mw_malloc(yuv_len);
        if (buf == NULL)
        {
            __ERR("snap yuv malloc failed size:%d\n", yuv_len);
            anj_mutex_unlock(&manager->lock);
            return;
        }
        if (manager->soft_yuv_buf)
        {
            anj_mw_free(manager->soft_yuv_buf);
        }
        manager->soft_yuv_buf = buf;
        manager->soft_yuv_cap = yuv_len;
    }
    memcpy(manager->soft_yuv_buf, p_vir_addr, yuv_len);
    current->yuv_data = manager->soft_yuv_buf;
    current->yuv_len = yuv_len;
    current->yuv_ready = 1;
    manager->soft_yuv_in_use = 1;
    anj_condition_signal(&manager->soft_cond, 0);
    anj_mutex_unlock(&manager->lock);
}

int anj_snap_jpg(int cam, int stream, int quality, char *filePath, char *fileName, AreaStruct *are)
{
    int iRet = 0;
    const anj_snap_provider_ops *ops = anj_snap_provider_get();
    AnjSnapManager *manager = &gstAnjSnapMng;

    if (s_stSnapInit == 0 || ops == NULL || ops->capture == NULL)
    {
        return -1;
    }
    if (cam < 0 || cam >= ANJ_CAMERA_MAX_NUMS || stream < 0 || stream >= MAX_VENC_CHN ||
        filePath == NULL || fileName == NULL)
    {
        return -1;
    }

    /* 软编只有一路 YUV（子码流源），主码流等请求回落到子码流，文件名不变 */
    if (ops->start_yuv != NULL && stream != 1)
    {
        __INFO("soft snap remap stream %d -> 1\n", stream);
        stream = 1;
    }

    AnjSnapReq *new_req = anj_mw_malloc(sizeof(AnjSnapReq));
    if (new_req == NULL)
    {
        __ERR("malloc snap request failed\n");
        return -1;
    }
    memset(new_req, 0, sizeof(AnjSnapReq));
    new_req->cam = cam;
    new_req->stream = stream;
    new_req->quality = quality;
    snprintf(new_req->filename, sizeof(new_req->filename), "%s/%s", filePath, fileName);

    if (ops->prepare)
    {
        AreaStruct tmp = {0};
        int has_are = (are != NULL);
        if (has_are)
        {
            tmp = *are;
        }
        if (ops->prepare(cam, stream, &new_req->width, &new_req->height, &tmp, has_are) != 0)
        {
            anj_mw_free(new_req);
            return -1;
        }
        new_req->are = tmp;
    }

    anj_mutex_lock(&manager->lock);
    if (!manager->request_list)
    {
        manager->request_list = new_req;
    }
    else
    {
        AnjSnapReq *tail = manager->request_list;
        while (tail->next)
            tail = tail->next;
        tail->next = new_req;
    }
    anj_mutex_unlock(&manager->lock);

    if (ops->start_yuv)
    {
        iRet = ops->start_yuv(cam);
    }
    anj_condition_signal(&manager->soft_cond, 0);
    return iRet;
}

static int anj_snap_init(void)
{
    if (s_stSnapInit)
    {
        return 0;
    }
    anj_snap_provider_init();
    anj_snap_mgr_init();
    s_stSnapInit = 1;
    return 0;
}

static int anj_snap_uninit(void)
{
    if (s_stSnapInit == 0)
    {
        return 0;
    }
    anj_snap_mgr_uninit();
    anj_snap_provider_uninit();
    s_stSnapInit = 0;
    return 0;
}

REGISTER_MODULE(anj_snap, MODULE_PRIORITY_SNAP);
