#include "anj_mw_media_common.h"
#include "anj_mw_media_video.h"
#include "anj_mw_comm.h"
#include "anj_mw_smart.h"
#include "anj_mw_smart_provider.h"
#include "mpi_cpm.h"
#include "mpi_cve.h"
#include "ts_comm_cve.h"
#include <math.h>

typedef struct
{
    CVE_HANDLE pHandle;
    TS_U32 blkSize;
    unsigned int dstW;
    unsigned int dstH;
    VB_POOL vpool;
    TS_S32 status; /* 0:idle 1:ready 2:exit */
    VB_BLK vblkPending;
    pthread_mutex_t mutex;
    anj_thread_s stSmartThread;
    unsigned long long lastProcessMs;
} cpm_param_s;

static anj_mw_media_scl_data s_pfnSmartYuvCb = TS_NULL;
static cpm_param_s *s_pstCpmParam = TS_NULL;

static void anj_mw_smart_cpm_vb_destroy(cpm_param_s *pCpmHandle)
{
    if (pCpmHandle == TS_NULL)
    {
        return;
    }

    if (pCpmHandle->vblkPending != VB_INVALID_HANDLE)
    {
        TS_MPI_VB_ReleaseBlock(pCpmHandle->vblkPending);
        pCpmHandle->vblkPending = VB_INVALID_HANDLE;
    }

    if (pCpmHandle->vpool != VB_INVALID_POOLID)
    {
        TS_MPI_VB_DestroyPool(pCpmHandle->vpool);
        pCpmHandle->vpool = VB_INVALID_POOLID;
    }

    pCpmHandle->dstW = 0;
    pCpmHandle->dstH = 0;
    pCpmHandle->blkSize = 0;
    pCpmHandle->status = 0;
}

static int anj_mw_smart_cpm_vb_ensure(unsigned int dst_w, unsigned int dst_h)
{
    cpm_param_s *pCpmHandle = s_pstCpmParam;
    VB_POOL_CONFIG_S stVbPoolCfg = {0};
    TS_S32 s32Ret;
    TS_U32 blkSize = 0;

    if ((pCpmHandle == TS_NULL) || (dst_w == 0) || (dst_h == 0))
    {
        return -1;
    }

    blkSize = dst_w * dst_h * 2;
    if ((pCpmHandle->vpool != VB_INVALID_POOLID) &&
        (pCpmHandle->dstW == dst_w) && (pCpmHandle->dstH == dst_h) &&
        (pCpmHandle->blkSize == blkSize))
    {
        return 0;
    }

    pthread_mutex_lock(&pCpmHandle->mutex);
    anj_mw_smart_cpm_vb_destroy(pCpmHandle);
    pthread_mutex_unlock(&pCpmHandle->mutex);

    stVbPoolCfg.u64BlkSize = blkSize;
    stVbPoolCfg.u32BlkCnt = 1;
    stVbPoolCfg.enRemapMode = VB_REMAP_MODE_CACHED;
    pCpmHandle->vpool = TS_MPI_VB_CreatePool(&stVbPoolCfg);
    if (pCpmHandle->vpool == VB_INVALID_POOLID)
    {
        __ERR("TS_MPI_VB_CreatePool failed, blkSize=%u\n", blkSize);
        return -1;
    }

    s32Ret = TS_MPI_VB_MmapPool(pCpmHandle->vpool);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VB_MmapPool failed!\n");
        TS_MPI_VB_DestroyPool(pCpmHandle->vpool);
        pCpmHandle->vpool = VB_INVALID_POOLID;
        return -1;
    }

    pCpmHandle->dstW = dst_w;
    pCpmHandle->dstH = dst_h;
    pCpmHandle->blkSize = blkSize;
    __INFO("smart cpm VB ready, %ux%u blkSize=%u poolId=%d\n",
           dst_w, dst_h, blkSize, pCpmHandle->vpool);
    return 0;
}

int anj_mw_smart_size_get(AnjSmartAttr *pstAnjSmartAttr, unsigned int *width, unsigned int *height)
{
    if ((pstAnjSmartAttr == NULL) || (width == NULL) || (height == NULL))
    {
        return -1;
    }

    if (anj_mw_smart_provider_size_get(pstAnjSmartAttr, width, height) != 0)
    {
        *width = DEFAULT_SMART_WIDTH;
        *height = DEFAULT_SMART_HEIGHT;
    }

    if (anj_mw_smart_cpm_vb_ensure(*width, *height) != 0)
    {
        __WARN("smart cpm VB ensure failed, size=%ux%u\n", *width, *height);
    }

    return 0;
}

int anj_mw_smart_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_provider_process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, pstAnjSmartAttr);
}

int anj_mw_smart_init(AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_provider_init(pstAnjSmartAttr);
}

int anj_mw_smart_uninit()
{
    return anj_mw_smart_provider_uninit();
}

float anj_mw_smart_set_sensitivity(float sensitivity)
{
    return anj_mw_smart_provider_set_sensitivity(sensitivity);
}

int anj_mw_smart_rect_map_pd_to_zoom(const DOUBLE_AREA_ENTRY *cur_area,
                                     int *box_x, int *box_y, int *box_w, int *box_h)
{
    (void)cur_area;

    /* TS_5326 smart 使用裁剪后区域，检测框已在当前画面坐标系，无需换算 */
    if (box_x == NULL || box_y == NULL || box_w == NULL || box_h == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }
    return 0;
}

/*
 * 把算法检测框从「当前数字变倍画面ZOOM」换算到「未变倍全图PD」坐标系。
 *
 * 背景：
 *   PD = Person Detection（人体检测）。SMART_PD_WIDTH/HEIGHT 是检测统一使用的
 *   参考分辨率（608x352）。本平台算法跑在数字变倍裁剪后的画面上，
 *   输出的框坐标仍按该参考分辨率给出，但含义是：整幅 608x352 对应的是裁剪区，
 *   而不是完整传感器画面。下游（PTZ 跟踪等）需要未变倍全图上的坐标，故做本换算。
 *
 * 入参：
 *   box_x/y/w/h : 算法检测到的目标框（左上角 x/y + 宽/高），坐标系为当前变倍画面
 *   cur_area    : 当前数字变倍状态（DOUBLE_AREA_ENTRY）,用以计算出裁剪区域的位置和大小
 *
 * 换算分三步（以水平方向为例，垂直同理）：
 *
 *   1. 计算裁剪区在未变倍全图上的位置和大小
 *   2. 将算法检出框映射到裁剪区局部坐标
 *   3. 将裁剪区局部框平移到未变倍全图（裁剪前画面）
 * 
 * 图例（中心 2 倍变倍，SMART_PD = 608x352）：
 *
 *   【未变倍全图】608 x 352                    【变倍画面 / 算法输入】608 x 352
 *   +--------------------------------------+   +--------------------------------------+
 *   |                 y=0                  |   |                 y=0                  |
 *   |     crop_x=152                       |   |                                      |
 *   |     |<------- crop_w=304 ------->|   |   |   1. 裁剪区被拉伸铺满整幅画面         |
 *   |     +------------------------+       |   |                                      |
 *   |     | 1. 裁剪区               |       |   |   +------+  算法检出框 in             |
 *   |     |  crop_y=88             |       |   |   | in   |  (100,50,80,40)            |
 *   |     |     +----+ 3. 输出框    |       |   |   +------+  ——2.→ 缩到裁剪区局部      |
 *   |     |     |out | (202,113,   |       |   |                                      |
 *   |     |     +----+  40,20)     |       |   |                                      |
 *   |     |           crop_h=176   |       |   |                                      |
 *   |     +------------------------+       |   |                                      |
 *   |                            x=608     |   |                            x=608     |
 *   +--------------------------------------+   +--------------------------------------+
 *              ^ 本函数输出（3.）                         ^ 本函数输入
 *
 * 出参：原地改写 box_*，变为未变倍全图坐标系下的检测框。
 * 与 anj_mw_smart_rect_map_pd_to_zoom（全图坐标 → 变倍画面坐标）互为逆运算。
 */
int anj_mw_smart_rect_map_zoom_to_pd(const DOUBLE_AREA_ENTRY *cur_area,
                                     int *box_x, int *box_y, int *box_w, int *box_h)
{
    DOUBLE_AREA_ENTRY area = {0.0, 0.0, 1.0, 1.0};
    double crop_x, crop_y;

    if (box_x == NULL || box_y == NULL || box_w == NULL || box_h == NULL)
    {
        __ERR("input invalid!\n");
        return -1;
    }
    if (cur_area != NULL)
    {
        area = *cur_area;
    }
    /* 无效/未变倍：视为全图，不做换算 */
    if (area.width < 1.0 || area.height < 1.0)
    {
        area.xPos = 0.0;
        area.yPos = 0.0;
        area.width = 1.0;
        area.height = 1.0;
    }

    /* 1. 裁剪区左上角（未变倍全图像素） */
    crop_x = area.xPos * (double)SMART_PD_WIDTH;
    crop_y = area.yPos * (double)SMART_PD_HEIGHT;
    /* 2. 检出框缩到裁剪区局部 + 3. 加上裁剪区偏移 → 未变倍全图坐标 */
    *box_x = (int)round((double)(*box_x) / area.width + crop_x);
    *box_y = (int)round((double)(*box_y) / area.height + crop_y);
    *box_w = (int)round((double)(*box_w) / area.width);
    *box_h = (int)round((double)(*box_h) / area.height);
    /* 钳到未变倍全图 SMART_PD_* 范围，避免贴边取整溢出 */
    if (*box_w < 1)
        *box_w = 1;
    if (*box_h < 1)
        *box_h = 1;
    if (*box_x < 0)
        *box_x = 0;
    if (*box_y < 0)
        *box_y = 0;
    if (*box_x + *box_w > SMART_PD_WIDTH)
        *box_x = SMART_PD_WIDTH - *box_w;
    if (*box_y + *box_h > SMART_PD_HEIGHT)
        *box_y = SMART_PD_HEIGHT - *box_h;
    if (*box_x < 0)
    {
        *box_w += *box_x;
        *box_x = 0;
    }
    if (*box_y < 0)
    {
        *box_h += *box_y;
        *box_y = 0;
    }
    return 0;
}

static int anj_mw_smart_thread(void *ctx, int *bStart)
{
    cpm_param_s *pCpmHandle = (cpm_param_s *)ctx;
    if (pCpmHandle == TS_NULL)
    {
        return 0;
    }

    while (*bStart)
    {
        VB_BLK vblk = VB_INVALID_HANDLE;
        TS_U64 u64PhyAddr = 0;
        TS_VOID *pVirAddr = TS_NULL;

        pthread_mutex_lock(&pCpmHandle->mutex);
        if ((pCpmHandle->status == 1) && (pCpmHandle->vblkPending != VB_INVALID_HANDLE))
        {
            vblk = pCpmHandle->vblkPending;
            pCpmHandle->vblkPending = VB_INVALID_HANDLE;
            pCpmHandle->status = 0;
        }
        pthread_mutex_unlock(&pCpmHandle->mutex);

        if (vblk == VB_INVALID_HANDLE)
        {
            usleep(10 * 1000);
            continue;
        }

        u64PhyAddr = TS_MPI_VB_Handle2PhysAddr(vblk);
        TS_MPI_VB_GetBlockVirAddr(pCpmHandle->vpool, u64PhyAddr, &pVirAddr);
        if (s_pfnSmartYuvCb != TS_NULL)
        {
            s_pfnSmartYuvCb(0, pVirAddr, u64PhyAddr, pCpmHandle->blkSize, NULL);
        }
        TS_MPI_VB_ReleaseBlock(vblk);
    }

    return 0;
}

static TS_S32 anj_mw_smart_cve_resize(CVE_HANDLE pHandle, VIDEO_FRAME_INFO_S *pFrm,
                                      TS_U64 phyAddr_dest, unsigned int dst_w, unsigned int dst_h)
{
    TS_S32 s32Ret = TS_SUCCESS;

    CVE_TASK_ATTR_S pstTask;
    TS_S32 src_w = pFrm->stVFrame.u32Width;
    TS_S32 src_h = pFrm->stVFrame.u32Height;
    memset(&pstTask, 0, sizeof(CVE_TASK_ATTR_S));

    pstTask.u8DmaInputNum = 1;
    pstTask.pstSrc[0].u32Stride = src_w;
    pstTask.pstSrc[0].u32Width = src_w;
    pstTask.pstSrc[0].u32Height = src_h;
    pstTask.pstSrc[0].enType = TS_CVE_IMAGE_TYPE_YUV420SP;

    pstTask.u8DmaInputNum = 1;
    pstTask.pstDst[0].u32Stride = dst_w;
    pstTask.pstDst[0].u32Width = dst_w;
    pstTask.pstDst[0].u32Height = dst_h;
    pstTask.pstDst[0].enType = TS_CVE_IMAGE_TYPE_YUV420SP;

    pstTask.pstCascade.enCascadeType = TS_CVE_RESIZE_MODE;
    pstTask.pstCascade.stOperatorCtrl.pstResizeCtrl.u32SubWidth = src_w;
    pstTask.pstCascade.stOperatorCtrl.pstResizeCtrl.u32SubHeight = src_h;

    pstTask.pstSrc[0].u64PhyAddr[0] = pFrm->stVFrame.u64PhyAddr[0];
    pstTask.pstDst[0].u64PhyAddr[0] = phyAddr_dest;

    s32Ret = TS_MPI_TRP_CVE_AddTask(pHandle, &pstTask);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("cve add task error\n");
        return TS_FAILURE;
    }

    CVE_RESULT_S cveResult;

    s32Ret = TS_MPI_TRP_CVE_EndJob(pHandle, &cveResult);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("cve end job error\n");
        return TS_FAILURE;
    }

    return s32Ret;
}

static TS_S32 anj_mw_smart_cpm_init(TS_VOID **pHandle)
{
    cpm_param_s *pCpmHandle = TS_NULL;
    TS_S32 s32Ret;

    pCpmHandle = (cpm_param_s *)calloc(1, sizeof(cpm_param_s));
    if (NULL == pCpmHandle)
    {
        __ERR("malloc failed \n");
        return TS_FAILURE;
    }

    /* VB 延后到 anj_mw_smart_size_get(attr) 再建 */
    pCpmHandle->vpool = VB_INVALID_POOLID;
    pCpmHandle->vblkPending = VB_INVALID_HANDLE;
    pCpmHandle->dstW = 0;
    pCpmHandle->dstH = 0;
    pCpmHandle->blkSize = 0;

    s32Ret = TS_MPI_TRP_CVE_Init();
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_TRP_CVE_Init failed, s32Ret=%d\n", s32Ret);
        free(pCpmHandle);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_TRP_CVE_BeginJob(&pCpmHandle->pHandle);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_TRP_CVE_BeginJob failed, s32Ret=%d\n", s32Ret);
        TS_MPI_TRP_CVE_Exit();
        free(pCpmHandle);
        return TS_FAILURE;
    }

    pCpmHandle->status = 0;
    pthread_mutex_init(&pCpmHandle->mutex, TS_NULL);
    memset(&pCpmHandle->stSmartThread, 0, sizeof(anj_thread_s));
    pCpmHandle->stSmartThread.bAutoDestroy = 0;
    strncpy(pCpmHandle->stSmartThread.iThreadName, "smart_cpm", sizeof(pCpmHandle->stSmartThread.iThreadName) - 1);
    pCpmHandle->stSmartThread.iThreadjob.ctx = pCpmHandle;
    pCpmHandle->stSmartThread.iThreadjob.func = anj_mw_smart_thread;
    s32Ret = anj_thread_task_create(&pCpmHandle->stSmartThread);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("smart cpm thread create failed, s32Ret=%d\n", s32Ret);
        TS_MPI_TRP_CVE_Exit();
        pthread_mutex_destroy(&pCpmHandle->mutex);
        free(pCpmHandle);
        return TS_FAILURE;
    }

    s_pstCpmParam = pCpmHandle;
    *pHandle = pCpmHandle;
    return TS_SUCCESS;
}

static TS_VOID anj_mw_smart_cpm_exit(TS_VOID *pHandle)
{
    if (TS_NULL != pHandle)
    {
        cpm_param_s *pCpmHandle = (cpm_param_s *)pHandle;
        anj_thread_task_destroy(&pCpmHandle->stSmartThread, 0);
        pCpmHandle->status = 2;
        pthread_mutex_lock(&pCpmHandle->mutex);
        anj_mw_smart_cpm_vb_destroy(pCpmHandle);
        pthread_mutex_unlock(&pCpmHandle->mutex);
        pthread_mutex_destroy(&pCpmHandle->mutex);
        if (s_pstCpmParam == pCpmHandle)
        {
            s_pstCpmParam = TS_NULL;
        }
        TS_MPI_TRP_CVE_Exit();
        free(pHandle);
    }
}

static TS_S32 anj_mw_smart_cpm_process(TS_VOID *pHandle, TS_VOID **in, TS_VOID **out)
{
    cpm_param_s *pCpmHandle = (cpm_param_s *)pHandle;
    VIDEO_FRAME_INFO_S *bufinfo = (VIDEO_FRAME_INFO_S *)in[0];
    TS_U64 u64PhyAddr = 0;
    TS_S32 s32Ret = TS_SUCCESS;
    VB_BLK vblk = VB_INVALID_HANDLE;
    unsigned int fps = DEFAULT_SMART_FPS;
    unsigned long long nowMs = 0;
    unsigned long long intervalMs = 0;

    if ((pCpmHandle == TS_NULL) || (bufinfo == TS_NULL))
    {
        return TS_FAILURE;
    }

    if (bufinfo)
    {
        TS_MPI_VB_DupBlock_UID(bufinfo->u32PoolId, bufinfo->stVFrame.u64PhyAddr[0], VB_UID_CPM);
        memcpy(out[0], in[0], sizeof(VIDEO_FRAME_INFO_S));
    }

    if (fps == 0)
    {
        fps = 1;
    }
    intervalMs = 1000ULL / fps;
    nowMs = anj_mw_get_cputime_ms(NULL);
    if ((pCpmHandle->lastProcessMs != 0) && (nowMs - pCpmHandle->lastProcessMs < intervalMs))
    {
        return TS_SUCCESS;
    }

    if ((pCpmHandle->vpool == VB_INVALID_POOLID) || (pCpmHandle->blkSize == 0))
    {
        return TS_SUCCESS;
    }

    pthread_mutex_lock(&pCpmHandle->mutex);
    if (pCpmHandle->status != 0)
    {
        pthread_mutex_unlock(&pCpmHandle->mutex);
        return TS_SUCCESS;
    }
    pthread_mutex_unlock(&pCpmHandle->mutex);

    vblk = TS_MPI_VB_GetBlock(pCpmHandle->vpool, pCpmHandle->blkSize, TS_NULL);
    if (VB_INVALID_HANDLE != vblk)
    {
        u64PhyAddr = TS_MPI_VB_Handle2PhysAddr(vblk);
        s32Ret = anj_mw_smart_cve_resize(pCpmHandle->pHandle, bufinfo, u64PhyAddr,
                                         pCpmHandle->dstW, pCpmHandle->dstH);
        if (s32Ret == TS_SUCCESS)
        {
            pthread_mutex_lock(&pCpmHandle->mutex);
            pCpmHandle->vblkPending = vblk;
            pCpmHandle->status = 1;
            pCpmHandle->lastProcessMs = anj_mw_get_cputime_ms(NULL);
            pthread_mutex_unlock(&pCpmHandle->mutex);
            vblk = VB_INVALID_HANDLE;
        }
        if (vblk != VB_INVALID_HANDLE)
        {
            TS_MPI_VB_ReleaseBlock(vblk);
        }
    }

    return TS_SUCCESS;
}

static TS_S32 anj_mw_smart_cpm_set_param(TS_VOID *pHandle, TS_VOID *pParam)
{
    (void)pHandle;
    (void)pParam;
    return TS_SUCCESS;
}

static TS_S32 anj_mw_smart_cpm_get_param(TS_VOID *pHandle, TS_VOID *pParam)
{
    (void)pHandle;
    (void)pParam;
    return TS_SUCCESS;
}

static TS_S32 anj_mw_smart_cpm_get_result(TS_VOID *pHandle, TS_VOID *pResult)
{
    (void)pHandle;
    (void)pResult;
    return TS_SUCCESS;
}

static TS_S32 anj_mw_smart_cpm_release_result(TS_VOID *pHandle, TS_VOID *pResult)
{
    (void)pHandle;
    (void)pResult;
    return TS_SUCCESS;
}

int anj_mw_smart_cpm_start(int cameraIndex, int vencChn, unsigned int width, unsigned int height)
{
    CPM_GRP_ATTR_S stCpmGrpAttr = {0};
    CPM_Handle_S stCPMHandle = {0};

    if (cameraIndex != 0)
    {
        return TS_SUCCESS;
    }
    if ((width == 0) || (height == 0))
    {
        return TS_FAILURE;
    }

    stCpmGrpAttr.u32GrpId = cameraIndex;
    stCpmGrpAttr.u32PipeNum = 1;
    stCpmGrpAttr.u32ChnNum = 1;
    stCpmGrpAttr.u32Interval = 1;
    stCpmGrpAttr.bSyncPipe = TS_FALSE;
    stCpmGrpAttr.stChnAttr[0].u32Width = width;
    stCpmGrpAttr.stChnAttr[0].u32Height = height;
    stCpmGrpAttr.stChnAttr[0].enPixelFormat = PIXEL_FORMAT_NV_12;

    stCPMHandle.s32PipeNum = 1;
    stCPMHandle.stPipeAttr[0].u32Width = width;
    stCPMHandle.stPipeAttr[0].u32Height = height;
    stCPMHandle.stPipeAttr[0].enPixelFormat = PIXEL_FORMAT_NV_12;
    stCPMHandle.init = anj_mw_smart_cpm_init;
    stCPMHandle.exit = anj_mw_smart_cpm_exit;
    stCPMHandle.process = anj_mw_smart_cpm_process;
    stCPMHandle.set_param = anj_mw_smart_cpm_set_param;
    stCPMHandle.get_param = anj_mw_smart_cpm_get_param;
    stCPMHandle.get_result = anj_mw_smart_cpm_get_result;
    stCPMHandle.release_result = anj_mw_smart_cpm_release_result;

    STCHECKRESULT(TS_MPI_CPM_CreateGrp(cameraIndex, &stCpmGrpAttr));
    STCHECKRESULT(TS_MPI_CPM_Register(cameraIndex, &stCPMHandle));
    STCHECKRESULT(TS_MPI_CPM_StartGrp(cameraIndex));

    STCHECKRESULT(TS_COMMON_VPSS_Bind_CPM((VPSS_GRP)cameraIndex, MAX_VPSS_CHN - 1, cameraIndex, 0));
    STCHECKRESULT(TS_COMMON_CPM_Bind_VENC(cameraIndex, 0, (VENC_CHN)vencChn));

    return TS_SUCCESS;
}

int anj_mw_smart_cpm_stop(int cameraIndex, int vencChn)
{
    if (cameraIndex != 0)
    {
        return TS_SUCCESS;
    }

    TS_COMMON_CPM_UnBind_VENC(cameraIndex, 0, vencChn);
    TS_COMMON_VPSS_UnBind_CPM((VPSS_GRP)cameraIndex, MAX_VPSS_CHN - 1, cameraIndex, 0);
    TS_MPI_CPM_StopGrp(cameraIndex);
    TS_MPI_CPM_Unregister(cameraIndex);
    TS_MPI_CPM_DestroyGrp(cameraIndex);

    return TS_SUCCESS;
}

int anj_mw_smart_register_yuv_cb(void *pfnYuvCb)
{
    s_pfnSmartYuvCb = (anj_mw_media_scl_data)pfnYuvCb;
    return TS_SUCCESS;
}
