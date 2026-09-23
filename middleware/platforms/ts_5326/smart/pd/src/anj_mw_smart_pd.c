#include "anj_mw_media_common.h"
#include "anj_mw_smart_pd.h"
#include "anj_mw_smart_provider.h"
#include "ts_alg_body_detect3.h"
#include "ts_alg_imgproc.h"
#include "ts_alg_rw.h"
#include "ts_rne_device.h"
#include "trp.h"
#include "ts_alg_log.h"
#include <stdint.h>

static TS_VOID *s_pstPdHandle = TS_NULL;
static TS_S32 s_s32PdInitDone = 0;
static TS_VOID *s_pstRgbBufVirAddr = TS_NULL;
static TS_BOOL s_bRneOpened = TS_FALSE;
static TS_BOOL s_bRneMutexInited = TS_FALSE;
static ALG_IMAGE_S s_stPdInputImage = {0};
static ALG_MODEL_INIT_S s_stPdModelInitParam = {0};
static TS_BOOL s_bPdModelLoaded = TS_FALSE;
static pthread_mutex_t s_stPdMutex = PTHREAD_MUTEX_INITIALIZER;
static float s_fPdThreshold = 0.0f;
static int s_s32PdEnable = 0;

static int anj_mw_smart_pd_ops_size_get(unsigned int *width, unsigned int *height)
{
    if ((width == NULL) || (height == NULL))
    {
        return -1;
    }

    *width = DEFAULT_SMART_WIDTH;
    *height = DEFAULT_SMART_HEIGHT;
    return 0;
}

int anj_mw_smart_pd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartPdAttr *pstAnjPdAttr)
{
    TS_S32 s32Ret = TS_SUCCESS;
    TS_U8 *pu8YImage = TS_NULL;
    TS_U8 *pu8UvImage = TS_NULL;
    ALG_BODYDET3_RESULT_S stPdResult = {0};
    int boxCnt = 0;

    (void)p_phy_addr;
    (void)len;

    if ((pstAnjSmartInfo == NULL) || (pstAnjPdAttr == NULL))
    {
        return TS_FAILURE;
    }

    s_s32PdEnable = pstAnjPdAttr->enable;
    pstAnjSmartInfo->boxCnt = 0;
    if ((pstAnjPdAttr->enable == 0) || (p_vir_addr == NULL))
    {
        return TS_SUCCESS;
    }

    anj_mutex_lock(&s_stPdMutex);
    if ((s_s32PdInitDone == 0) || (s_pstPdHandle == TS_NULL) || (s_stPdInputImage.pData == TS_NULL))
    {
        anj_mutex_unlock(&s_stPdMutex);
        return TS_SUCCESS;
    }

    pu8YImage = (TS_U8 *)p_vir_addr;
    pu8UvImage = pu8YImage + DEFAULT_SMART_WIDTH * DEFAULT_SMART_HEIGHT;
    TS_ALG_YUV2RGB(pu8YImage, pu8UvImage, s_stPdInputImage.pData,
                   DEFAULT_SMART_WIDTH, DEFAULT_SMART_HEIGHT, DEFAULT_SMART_WIDTH, DEFAULT_SMART_HEIGHT, ALG_RGB_TYPE_RGBA32);

    s32Ret = TS_MPI_TRP_RNE_MutexLockWithTimeout(2, 0);
    if (s32Ret != TS_SUCCESS)
    {
        anj_mutex_unlock(&s_stPdMutex);
        return s32Ret;
    }
    s32Ret = TS_ALG_BodyDet3_Process(s_pstPdHandle, &s_stPdInputImage, &stPdResult);
    TS_MPI_TRP_RNE_MutexUnlock();
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_BodyDet3_Process failed, s32Ret:%d\n", s32Ret);
        anj_mutex_unlock(&s_stPdMutex);
        return s32Ret;
    }

    for (TS_U32 i = 0; i < stPdResult.u32BodyNum; i++)
    {
        int x = (int)(stPdResult.astBox[i].f32Xmin * DEFAULT_SMART_WIDTH);
        int y = (int)(stPdResult.astBox[i].f32Ymin * DEFAULT_SMART_HEIGHT);
        int width = (int)((stPdResult.astBox[i].f32Xmax - stPdResult.astBox[i].f32Xmin) * DEFAULT_SMART_WIDTH);
        int height = (int)((stPdResult.astBox[i].f32Ymax - stPdResult.astBox[i].f32Ymin) * DEFAULT_SMART_HEIGHT);

        if ((width <= 0) || (height <= 0))
        {
            continue;
        }
        if (pstAnjPdAttr->minRectFilter && (width * height < pstAnjPdAttr->minRectFilter))
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
        pstAnjSmartInfo->stBoxInfo[boxCnt].score = stPdResult.astBox[i].f32Score;
        boxCnt++;
    }

    pstAnjSmartInfo->boxCnt = boxCnt;
    anj_mutex_unlock(&s_stPdMutex);

    return TS_SUCCESS;
}

int anj_mw_smart_pd_init(float threshold)
{
    TS_S32 s32Ret = TS_SUCCESS;
    TS_U32 u32RgbBufSize = DEFAULT_SMART_WIDTH * DEFAULT_SMART_HEIGHT * 4;

    (void)threshold;
    __INFO("smart body init start\n");

    anj_mutex_lock(&s_stPdMutex);
    if (s_s32PdInitDone == 1)
    {
        anj_mutex_unlock(&s_stPdMutex);
        return TS_SUCCESS;
    }

    TS_ALG_SetLogLevel(ALG_LOG_ERROR);
    s32Ret = TS_MPI_TRP_RNE_OpenDevice(NULL, NULL);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_TRP_RNE_OpenDevice failed, s32Ret:%d\n", s32Ret);
        goto exit_failed;
    }
    s_bRneOpened = TS_TRUE;

    s32Ret = TS_MPI_TRP_RNE_MutexInit();
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_TRP_RNE_MutexInit failed, s32Ret:%d\n", s32Ret);
        goto exit_failed;
    }
    s_bRneMutexInited = TS_TRUE;

    s_pstRgbBufVirAddr = TS_MPI_TRP_RNE_AllocLinearMemCached(u32RgbBufSize, 0);
    if (s_pstRgbBufVirAddr == TS_NULL)
    {
        __ERR("TS_MPI_TRP_RNE_AllocLinearMemCached failed, size:%u\n", u32RgbBufSize);
        s32Ret = TS_FAILURE;
        goto exit_failed;
    }

    memset(&s_stPdInputImage, 0, sizeof(ALG_IMAGE_S));
    s_stPdInputImage.s32C = 4;
    s_stPdInputImage.s32W = DEFAULT_SMART_WIDTH;
    s_stPdInputImage.s32H = DEFAULT_SMART_HEIGHT;
    s_stPdInputImage.pData = s_pstRgbBufVirAddr;
    s_stPdInputImage.u64DataPhyAddr = TS_MPI_TRP_RNE_VirtualToPhysicalAddress((TS_SIZE_T)s_pstRgbBufVirAddr);

    memset(&s_stPdModelInitParam, 0, sizeof(s_stPdModelInitParam));
    s32Ret = TS_ALG_LoadCfgWeightOnce(SMART_PD_CFG_PATH, SMART_PD_WEIGHT_PATH, &s_stPdModelInitParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_LoadCfgWeightOnce failed, cfg:%s weight:%s ret:%d\n",
              SMART_PD_CFG_PATH, SMART_PD_WEIGHT_PATH, s32Ret);
        goto exit_failed;
    }
    s_bPdModelLoaded = TS_TRUE;
    s_stPdModelInitParam.enImageType = ALG_IMAGE_TYPE_INT_HWC_RGB0;
    s_stPdModelInitParam.pSelfBuf = TS_NULL;
    s_stPdModelInitParam.bRneOff = 1;

    __INFO("TS_ALG_BodyDet3 version:%s cfg:%u weight:%u\n",
           TS_ALG_BodyDet3_GetVersion(),
           s_stPdModelInitParam.u32GraphSize,
           s_stPdModelInitParam.u32WeightSize);
    s32Ret = TS_ALG_BodyDet3_Init(&s_pstPdHandle, &s_stPdModelInitParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_BodyDet3_Init failed, s32Ret:%d\n", s32Ret);
        goto exit_failed;
    }

    s_fPdThreshold = threshold;
    s_s32PdInitDone = 1;
    anj_mutex_unlock(&s_stPdMutex);
    __INFO("smart body init successful\n");
    return TS_SUCCESS;

exit_failed:
    if (s_pstPdHandle != TS_NULL)
    {
        TS_ALG_BodyDet3_Exit(s_pstPdHandle);
        s_pstPdHandle = TS_NULL;
    }
    if (s_bPdModelLoaded == TS_TRUE)
    {
        TS_ALG_FreeCfgWeightOnce(&s_stPdModelInitParam);
        memset(&s_stPdModelInitParam, 0, sizeof(s_stPdModelInitParam));
        s_bPdModelLoaded = TS_FALSE;
    }
    if (s_pstRgbBufVirAddr != TS_NULL)
    {
        TS_MPI_TRP_RNE_FreeLinearMemCached(s_pstRgbBufVirAddr);
        s_pstRgbBufVirAddr = TS_NULL;
    }
    memset(&s_stPdInputImage, 0, sizeof(ALG_IMAGE_S));
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
    s_s32PdInitDone = 0;
    anj_mutex_unlock(&s_stPdMutex);
    return s32Ret;
}

int anj_mw_smart_pd_uninit()
{
    anj_mutex_lock(&s_stPdMutex);
    if (s_pstPdHandle != TS_NULL)
    {
        TS_ALG_BodyDet3_Exit(s_pstPdHandle);
        s_pstPdHandle = TS_NULL;
    }
    if (s_bPdModelLoaded == TS_TRUE)
    {
        TS_ALG_FreeCfgWeightOnce(&s_stPdModelInitParam);
        memset(&s_stPdModelInitParam, 0, sizeof(s_stPdModelInitParam));
        s_bPdModelLoaded = TS_FALSE;
    }
    if (s_pstRgbBufVirAddr != TS_NULL)
    {
        TS_MPI_TRP_RNE_FreeLinearMemCached(s_pstRgbBufVirAddr);
        s_pstRgbBufVirAddr = TS_NULL;
    }
    memset(&s_stPdInputImage, 0, sizeof(ALG_IMAGE_S));
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
    s_s32PdInitDone = 0;
    s_s32PdEnable = 0;
    s_fPdThreshold = 0.0f;
    anj_mutex_unlock(&s_stPdMutex);
    return TS_SUCCESS;
}

float anj_mw_smart_pd_set_sensitivity(float threshold)
{
    s_fPdThreshold = threshold;
    __INFO("ts body detect does not support runtime sensitivity update\n");
    return s_fPdThreshold;
}

static int anj_mw_smart_pd_ops_init(AnjSmartAttr *pstAnjSmartAttr)
{
    int iRet = 0;
    if (pstAnjSmartAttr == NULL)
    {
        return TS_FAILURE;
    }

    s_s32PdEnable = pstAnjSmartAttr->stAnjPdAttr.enable;
    if (s_s32PdEnable == 0)
    {
        __INFO("smart pd skip model load, enable=0\n");
        return TS_SUCCESS;
    }
    iRet = anj_mw_smart_pd_init(pstAnjSmartAttr->stAnjPdAttr.sensitivity);
    if (iRet != 0)
    {
        anj_mw_smart_pd_uninit();
    }

    return iRet;
}

static int anj_mw_smart_pd_ops_uninit(void)
{
    return anj_mw_smart_pd_uninit();
}

static float anj_mw_smart_pd_ops_set_sensitivity(float sensitivity)
{
    return anj_mw_smart_pd_set_sensitivity(sensitivity);
}

static int anj_mw_smart_pd_ops_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                       AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_pd_process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, &pstAnjSmartAttr->stAnjPdAttr);
}

static const anj_mw_smart_provider_ops s_stMwSmartProviderOps = {
    .provider_name = "ts_5326_pd",
    .type = SMART_TYPE_PD,
    .provider_priority = 100,
    .init = anj_mw_smart_pd_ops_init,
    .uninit = anj_mw_smart_pd_ops_uninit,
    .set_sensitivity = anj_mw_smart_pd_ops_set_sensitivity,
    .process = anj_mw_smart_pd_ops_process,
    .size_get = anj_mw_smart_pd_ops_size_get,
};

ANJ_LINK_KEEP(anj_keep_mw_smart_pd_provider);

__attribute__((constructor)) static void anj_mw_smart_provider_register_constructor(void)
{
    anj_mw_smart_provider_register(&s_stMwSmartProviderOps);
}

__attribute__((destructor)) static void anj_mw_smart_provider_unregister_constructor(void)
{
    anj_mw_smart_provider_unregister(&s_stMwSmartProviderOps);
}
