#include "anj_mw_media_common.h"
#include "anj_mw_smart_fd.h"
#include "anj_mw_smart_provider.h"
#include "ts_alg_face_detect.h"
#include "ts_alg_imgproc.h"
#include "ts_alg_rw.h"
#include "ts_rne_device.h"
#include "trp.h"
#include "ts_alg_log.h"
#include <stdint.h>

static TS_VOID *s_pstFdHandle = TS_NULL;
static TS_S32 s_s32FdInitDone = 0;
static TS_VOID *s_pstRgbBufVirAddr = TS_NULL;
static TS_BOOL s_bRneOpened = TS_FALSE;
static TS_BOOL s_bRneMutexInited = TS_FALSE;
static ALG_IMAGE_S s_stFdInputImage = {0};
static ALG_MODEL_INIT_S s_stFdModelInitParam = {0};
static TS_BOOL s_bFdModelLoaded = TS_FALSE;
static pthread_mutex_t s_stFdMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_s32FdEnable = 0;

static int anj_mw_smart_fd_ops_size_get(unsigned int *width, unsigned int *height)
{
    if ((width == NULL) || (height == NULL))
    {
        return -1;
    }

    *width = SMART_FD_WIDTH;
    *height = SMART_FD_HEIGHT;
    return 0;
}

int anj_mw_smart_fd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                            AnjSmartInfo *pstAnjSmartInfo, AnjSmartFdAttr *pstAnjFdAttr)
{
    TS_S32 s32Ret = TS_SUCCESS;
    TS_U8 *pu8YImage = TS_NULL;
    TS_U8 *pu8UvImage = TS_NULL;
    ALG_FACE_DETECT_RESULT_S stFdResult = {0};
    int boxCnt = 0;

    (void)p_phy_addr;
    (void)len;

    if ((pstAnjSmartInfo == NULL) || (pstAnjFdAttr == NULL))
    {
        return TS_FAILURE;
    }

    s_s32FdEnable = pstAnjFdAttr->enable;
    pstAnjSmartInfo->boxCnt = 0;
    if ((pstAnjFdAttr->enable == 0) || (p_vir_addr == NULL))
    {
        return TS_SUCCESS;
    }

    anj_mutex_lock(&s_stFdMutex);
    if ((s_s32FdInitDone == 0) || (s_pstFdHandle == TS_NULL) || (s_stFdInputImage.pData == TS_NULL))
    {
        anj_mutex_unlock(&s_stFdMutex);
        return TS_SUCCESS;
    }

    pu8YImage = (TS_U8 *)p_vir_addr;
    pu8UvImage = pu8YImage + SMART_FD_WIDTH * SMART_FD_HEIGHT;
    TS_ALG_YUV2RGB(pu8YImage, pu8UvImage, s_stFdInputImage.pData,
                   SMART_FD_WIDTH, SMART_FD_HEIGHT, SMART_FD_WIDTH, SMART_FD_HEIGHT, ALG_RGB_TYPE_RGBA32);

    s32Ret = TS_MPI_TRP_RNE_MutexLockWithTimeout(2, 0);
    if (s32Ret != TS_SUCCESS)
    {
        anj_mutex_unlock(&s_stFdMutex);
        return s32Ret;
    }
    s32Ret = TS_ALG_FaceDetect_Process(s_pstFdHandle, &s_stFdInputImage, &stFdResult);
    TS_MPI_TRP_RNE_MutexUnlock();
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_FaceDetect_Process failed, s32Ret:%d\n", s32Ret);
        anj_mutex_unlock(&s_stFdMutex);
        return s32Ret;
    }

    for (TS_U32 i = 0; i < stFdResult.u32FaceNum; i++)
    {
        int x = (int)(stFdResult.astBox[i].f32Xmin * SMART_FD_WIDTH);
        int y = (int)(stFdResult.astBox[i].f32Ymin * SMART_FD_HEIGHT);
        int width = (int)((stFdResult.astBox[i].f32Xmax - stFdResult.astBox[i].f32Xmin) * SMART_FD_WIDTH);
        int height = (int)((stFdResult.astBox[i].f32Ymax - stFdResult.astBox[i].f32Ymin) * SMART_FD_HEIGHT);

        x = x * DEFAULT_SMART_WIDTH / SMART_FD_WIDTH;
        y = y * DEFAULT_SMART_HEIGHT / SMART_FD_HEIGHT;
        width = width * DEFAULT_SMART_WIDTH / SMART_FD_WIDTH;
        height = height * DEFAULT_SMART_HEIGHT / SMART_FD_HEIGHT;

        if ((width <= 0) || (height <= 0))
        {
            continue;
        }
        if (boxCnt >= SMART_MAX_DETECT_RECT)
        {
            break;
        }

        pstAnjSmartInfo->stBoxInfo[boxCnt].x = x;
        pstAnjSmartInfo->stBoxInfo[boxCnt].y = y;
        pstAnjSmartInfo->stBoxInfo[boxCnt].width = width;
        pstAnjSmartInfo->stBoxInfo[boxCnt].height = height;
        pstAnjSmartInfo->stBoxInfo[boxCnt].class_id = 0;
        pstAnjSmartInfo->stBoxInfo[boxCnt].score = stFdResult.astBox[i].f32Score;
        boxCnt++;
    }

    pstAnjSmartInfo->boxCnt = boxCnt;
    anj_mutex_unlock(&s_stFdMutex);
    return TS_SUCCESS;
}

int anj_mw_smart_fd_init(float threshold, int enable)
{
    TS_S32 s32Ret = TS_SUCCESS;
    TS_U32 u32RgbBufSize = SMART_FD_WIDTH * SMART_FD_HEIGHT * 4;
    ALG_FACE_DETECT_PARAM_S stFdParam = {0};

    __INFO("smart face init start\n");

    anj_mutex_lock(&s_stFdMutex);
    s_s32FdEnable = enable;
    if (enable == 0)
    {
        anj_mutex_unlock(&s_stFdMutex);
        __INFO("smart face skip model load, enable=0\n");
        return TS_SUCCESS;
    }
    if (s_s32FdInitDone == 1)
    {
        anj_mutex_unlock(&s_stFdMutex);
        return TS_SUCCESS;
    }

    TS_ALG_SetLogLevel(ALG_LOG_ERROR);
    s32Ret = TS_MPI_TRP_RNE_OpenDevice(NULL, NULL);
    if (s32Ret == TS_SUCCESS)
    {
        s_bRneOpened = TS_TRUE;
    }
    else
    {
        /* PD 可能已打开 RNE，继续使用共享设备 */
        __WARN("TS_MPI_TRP_RNE_OpenDevice ret:%d, try share with pd\n", s32Ret);
        s_bRneOpened = TS_FALSE;
    }

    s32Ret = TS_MPI_TRP_RNE_MutexInit();
    if (s32Ret == TS_SUCCESS)
    {
        s_bRneMutexInited = TS_TRUE;
    }
    else
    {
        __WARN("TS_MPI_TRP_RNE_MutexInit ret:%d, try share with pd\n", s32Ret);
        s_bRneMutexInited = TS_FALSE;
    }

    s_pstRgbBufVirAddr = TS_MPI_TRP_RNE_AllocLinearMemCached(u32RgbBufSize, 0);
    if (s_pstRgbBufVirAddr == TS_NULL)
    {
        __ERR("TS_MPI_TRP_RNE_AllocLinearMemCached failed, size:%u\n", u32RgbBufSize);
        s32Ret = TS_FAILURE;
        goto exit_failed;
    }

    memset(&s_stFdInputImage, 0, sizeof(ALG_IMAGE_S));
    s_stFdInputImage.s32C = 4;
    s_stFdInputImage.s32W = SMART_FD_WIDTH;
    s_stFdInputImage.s32H = SMART_FD_HEIGHT;
    s_stFdInputImage.pData = s_pstRgbBufVirAddr;
    s_stFdInputImage.u64DataPhyAddr = TS_MPI_TRP_RNE_VirtualToPhysicalAddress((TS_SIZE_T)s_pstRgbBufVirAddr);

    memset(&s_stFdModelInitParam, 0, sizeof(s_stFdModelInitParam));
    s32Ret = TS_ALG_LoadCfgWeightOnce(SMART_FD_CFG_PATH, SMART_FD_WEIGHT_PATH, &s_stFdModelInitParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_LoadCfgWeightOnce failed, cfg:%s weight:%s ret:%d\n",
              SMART_FD_CFG_PATH, SMART_FD_WEIGHT_PATH, s32Ret);
        goto exit_failed;
    }
    s_bFdModelLoaded = TS_TRUE;
    s_stFdModelInitParam.enImageType = ALG_IMAGE_TYPE_INT_HWC_RGB0;
    s_stFdModelInitParam.pSelfBuf = TS_NULL;
    s_stFdModelInitParam.bRneOff = 1;

    __INFO("TS_ALG_FaceDetect version:%s cfg:%u weight:%u\n",
           TS_ALG_FaceDetect_GetVersion(),
           s_stFdModelInitParam.u32GraphSize,
           s_stFdModelInitParam.u32WeightSize);
    s32Ret = TS_ALG_FaceDetect_Init(&s_pstFdHandle, &s_stFdModelInitParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_FaceDetect_Init failed, s32Ret:%d\n", s32Ret);
        goto exit_failed;
    }

    stFdParam.f32Thresh = threshold;
    (void)TS_ALG_FaceDetect_SetParam(s_pstFdHandle, &stFdParam);

    s_s32FdInitDone = 1;
    anj_mutex_unlock(&s_stFdMutex);
    __INFO("smart face init successful\n");
    return TS_SUCCESS;

exit_failed:
    if (s_pstFdHandle != TS_NULL)
    {
        TS_ALG_FaceDetect_Exit(s_pstFdHandle);
        s_pstFdHandle = TS_NULL;
    }
    if (s_bFdModelLoaded == TS_TRUE)
    {
        TS_ALG_FreeCfgWeightOnce(&s_stFdModelInitParam);
        memset(&s_stFdModelInitParam, 0, sizeof(s_stFdModelInitParam));
        s_bFdModelLoaded = TS_FALSE;
    }
    if (s_pstRgbBufVirAddr != TS_NULL)
    {
        TS_MPI_TRP_RNE_FreeLinearMemCached(s_pstRgbBufVirAddr);
        s_pstRgbBufVirAddr = TS_NULL;
    }
    memset(&s_stFdInputImage, 0, sizeof(ALG_IMAGE_S));
    if (s_bRneMutexInited == TS_TRUE)
    {
        TS_MPI_TRP_RNE_MutexDeInit();
        s_bRneMutexInited = TS_FALSE;
    }
    if (s_bRneOpened == TS_TRUE)
    {
        TS_MPI_TRP_RNE_CloseDevice();
        s_bRneOpened = TS_FALSE;
    }
    s_s32FdInitDone = 0;
    anj_mutex_unlock(&s_stFdMutex);
    return s32Ret;
}

int anj_mw_smart_fd_uninit(void)
{
    anj_mutex_lock(&s_stFdMutex);
    if (s_pstFdHandle != TS_NULL)
    {
        TS_ALG_FaceDetect_Exit(s_pstFdHandle);
        s_pstFdHandle = TS_NULL;
    }
    if (s_bFdModelLoaded == TS_TRUE)
    {
        TS_ALG_FreeCfgWeightOnce(&s_stFdModelInitParam);
        memset(&s_stFdModelInitParam, 0, sizeof(s_stFdModelInitParam));
        s_bFdModelLoaded = TS_FALSE;
    }
    if (s_pstRgbBufVirAddr != TS_NULL)
    {
        TS_MPI_TRP_RNE_FreeLinearMemCached(s_pstRgbBufVirAddr);
        s_pstRgbBufVirAddr = TS_NULL;
    }
    memset(&s_stFdInputImage, 0, sizeof(ALG_IMAGE_S));
    if (s_bRneMutexInited == TS_TRUE)
    {
        TS_MPI_TRP_RNE_MutexDeInit();
        s_bRneMutexInited = TS_FALSE;
    }
    if (s_bRneOpened == TS_TRUE)
    {
        TS_MPI_TRP_RNE_CloseDevice();
        s_bRneOpened = TS_FALSE;
    }
    s_s32FdInitDone = 0;
    s_s32FdEnable = 0;
    anj_mutex_unlock(&s_stFdMutex);
    return TS_SUCCESS;
}

static const anj_mw_smart_provider_ops s_stMwSmartProviderOps = {
    .provider_name = "ts_5326_fd",
    .type = SMART_TYPE_FD,
    .provider_priority = 100,
    .size_get = anj_mw_smart_fd_ops_size_get,
};

ANJ_LINK_KEEP(anj_keep_mw_smart_fd_provider);

__attribute__((constructor)) static void anj_mw_smart_provider_register_constructor(void)
{
    anj_mw_smart_provider_register(&s_stMwSmartProviderOps);
}

__attribute__((destructor)) static void anj_mw_smart_provider_unregister_constructor(void)
{
    anj_mw_smart_provider_unregister(&s_stMwSmartProviderOps);
}
