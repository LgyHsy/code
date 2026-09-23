#include "anj_mw_media_common.h"
#include "anj_mw_smart_pvd.h"
#include "anj_mw_smart_provider.h"
#include "ts_alg_mod8c.h"
#include "ts_alg_imgproc.h"
#include "ts_alg_rw.h"
#include "ts_rne_device.h"
#include "trp.h"
#include "ts_alg_log.h"
#include <stdint.h>

/* 与 module/alarm ALARM_PD_ID_* 对齐，避免 mw 依赖 alarm 头 */
#define ANJ_PVD_CLASS_HUMAN   (0)
#define ANJ_PVD_CLASS_BICYCLE (1)
#define ANJ_PVD_CLASS_CAR     (2)
#define ANJ_PVD_CLASS_MOTOR   (3)

static TS_VOID *s_pstPvdHandle = TS_NULL;
static TS_S32 s_s32PvdInitDone = 0;
static TS_VOID *s_pstRgbBufVirAddr = TS_NULL;
static TS_BOOL s_bRneOpened = TS_FALSE;
static TS_BOOL s_bRneMutexInited = TS_FALSE;
static ALG_IMAGE_S s_stPvdInputImage = {0};
static ALG_MODEL_INIT_S s_stPvdModelInitParam = {0};
static TS_BOOL s_bPvdModelLoaded = TS_FALSE;
static pthread_mutex_t s_stPvdMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_s32PvdEnable = 0;
static int s_s32LastMono = -1; /* -1: unset; 0: RGB; 1: MONO */

static int anj_mw_smart_pvd_ops_size_get(unsigned int *width, unsigned int *height)
{
    if ((width == NULL) || (height == NULL))
    {
        return -1;
    }

    *width = SMART_PVD_WIDTH;
    *height = SMART_PVD_HEIGHT;
    return 0;
}

static int anj_mw_smart_pvd_lab_to_class_id(TS_U32 u32Lab, int *pClassId)
{
    if (pClassId == TS_NULL)
    {
        return TS_FAILURE;
    }

    switch (u32Lab)
    {
    case ALG_MOD8C_DET_PEDESTRIAN:
        *pClassId = ANJ_PVD_CLASS_HUMAN;
        return TS_SUCCESS;
    case ALG_MOD8C_DET_CAR:
        *pClassId = ANJ_PVD_CLASS_CAR;
        return TS_SUCCESS;
    case ALG_MOD8C_DET_BICYCLE:
        *pClassId = ANJ_PVD_CLASS_BICYCLE;
        return TS_SUCCESS;
    case ALG_MOD8C_DET_EBIKE:
    case ALG_MOD8C_DET_ETRICYCLE:
        *pClassId = ANJ_PVD_CLASS_MOTOR;
        return TS_SUCCESS;
    default:
        /* PACKAGE/CAT/DOG 等丢弃 */
        return TS_FAILURE;
    }
}

/* 组未满则追加；已满则仅当新框面积严格更大时替换组内最小项 */
static void anj_mw_smart_pvd_keep_largest(AnjSmartBoxInfo *pstBoxes, int *pCnt, const AnjSmartBoxInfo *pstBox)
{
    int i = 0;
    int minIdx = 0;
    int minArea = 0;
    int area = 0;
    int quota = SMART_MAX_DETECT_RECT / 2;

    if (*pCnt < quota)
    {
        pstBoxes[*pCnt] = *pstBox;
        (*pCnt)++;
        return;
    }

    minArea = pstBoxes[0].width * pstBoxes[0].height;
    for (i = 1; i < *pCnt; i++)
    {
        area = pstBoxes[i].width * pstBoxes[i].height;
        if (area < minArea)
        {
            minArea = area;
            minIdx = i;
        }
    }

    if ((pstBox->width * pstBox->height) > minArea)
    {
        pstBoxes[minIdx] = *pstBox;
    }
}

int anj_mw_smart_pvd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                               AnjSmartInfo *pstAnjSmartInfo, AnjSmartPdAttr *pstAnjPdAttr)
{
    TS_S32 s32Ret = TS_SUCCESS;
    TS_U8 *pu8YImage = TS_NULL;
    TS_U8 *pu8UvImage = TS_NULL;
    ALG_MOD8C_RESULT_S stPvdResult = {0};
    AnjSmartBoxInfo stHumanBox[SMART_MAX_DETECT_RECT / 2];
    AnjSmartBoxInfo stOtherBox[SMART_MAX_DETECT_RECT / 2];
    int humanCnt = 0;
    int otherCnt = 0;
    int boxCnt = 0;

    (void)p_phy_addr;
    (void)len;

    if ((pstAnjSmartInfo == NULL) || (pstAnjPdAttr == NULL))
    {
        return TS_FAILURE;
    }

    s_s32PvdEnable = pstAnjPdAttr->enable;
    pstAnjSmartInfo->boxCnt = 0;
    if ((pstAnjPdAttr->enable == 0) || (p_vir_addr == NULL))
    {
        return TS_SUCCESS;
    }

    anj_mutex_lock(&s_stPvdMutex);
    if ((s_s32PvdInitDone == 0) || (s_pstPvdHandle == TS_NULL) || (s_stPvdInputImage.pData == TS_NULL))
    {
        anj_mutex_unlock(&s_stPvdMutex);
        return TS_SUCCESS;
    }

    {
        int daynight = 1;
        int is_mono = 0;

        if (TS_Common_IspDayNightGet(0, &daynight) == TS_SUCCESS)
        {
            is_mono = (daynight == 0) ? 1 : 0;
            if (s_s32LastMono != is_mono)
            {
                ALG_MOD8C_PARAM_S stPvdParam = {0};
                float thresh = pstAnjPdAttr->sensitivity;

                if (is_mono)
                {
                    thresh -= 0.2f; /* UI +20 */
                }
                if (thresh < 0.01f)
                {
                    thresh = 0.01f;
                }
                if (thresh > 0.6f)
                {
                    thresh = 0.6f;
                }
                stPvdParam.f32ConfThresh = thresh;
                stPvdParam.f32IouThresh = 0.45f;
                (void)TS_ALG_Mod8c_SetParam(s_pstPvdHandle, &stPvdParam);
                s_s32LastMono = is_mono;
                __INFO("pvd dn SetParam mono=%d thresh=%.3f\n", is_mono, thresh);
            }
        }
    }

    pu8YImage = (TS_U8 *)p_vir_addr;
    pu8UvImage = pu8YImage + SMART_PVD_WIDTH * SMART_PVD_HEIGHT;
    TS_ALG_YUV2RGB(pu8YImage, pu8UvImage, s_stPvdInputImage.pData,
                   SMART_PVD_WIDTH, SMART_PVD_HEIGHT, SMART_PVD_WIDTH, SMART_PVD_HEIGHT, ALG_RGB_TYPE_RGBA32);

    s32Ret = TS_MPI_TRP_RNE_MutexLockWithTimeout(2, 0);
    if (s32Ret != TS_SUCCESS)
    {
        anj_mutex_unlock(&s_stPvdMutex);
        return s32Ret;
    }
    s32Ret = TS_ALG_Mod8c_Process(s_pstPvdHandle, &s_stPvdInputImage, &stPvdResult);
    TS_MPI_TRP_RNE_MutexUnlock();
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_Mod8c_Process failed, s32Ret:%d\n", s32Ret);
        anj_mutex_unlock(&s_stPvdMutex);
        return s32Ret;
    }

    for (TS_U32 i = 0; i < stPvdResult.u32ObjNum; i++)
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int class_id = 0;
        AnjSmartBoxInfo stBox = {0};

        if (anj_mw_smart_pvd_lab_to_class_id(stPvdResult.astBox[i].u32Lab, &class_id) != TS_SUCCESS)
        {
            continue;
        }

        x = (int)(stPvdResult.astBox[i].f32Xmin * SMART_PVD_WIDTH);
        y = (int)(stPvdResult.astBox[i].f32Ymin * SMART_PVD_HEIGHT);
        width = (int)((stPvdResult.astBox[i].f32Xmax - stPvdResult.astBox[i].f32Xmin) * SMART_PVD_WIDTH);
        height = (int)((stPvdResult.astBox[i].f32Ymax - stPvdResult.astBox[i].f32Ymin) * SMART_PVD_HEIGHT);

        x = x * DEFAULT_SMART_WIDTH / SMART_PVD_WIDTH;
        y = y * DEFAULT_SMART_HEIGHT / SMART_PVD_HEIGHT;
        width = width * DEFAULT_SMART_WIDTH / SMART_PVD_WIDTH;
        height = height * DEFAULT_SMART_HEIGHT / SMART_PVD_HEIGHT;

        if ((width <= 0) || (height <= 0))
        {
            continue;
        }

        stBox.x = x;
        stBox.y = y;
        stBox.width = width;
        stBox.height = height;
        stBox.class_id = class_id;
        stBox.score = stPvdResult.astBox[i].f32Score;
        if (class_id == ANJ_PVD_CLASS_HUMAN)
        {
            anj_mw_smart_pvd_keep_largest(stHumanBox, &humanCnt, &stBox);
        }
        else
        {
            anj_mw_smart_pvd_keep_largest(stOtherBox, &otherCnt, &stBox);
        }
    }

    for (int i = 0; i < humanCnt; i++)
    {
        pstAnjSmartInfo->stBoxInfo[boxCnt] = stHumanBox[i];
        boxCnt++;
    }
    for (int i = 0; i < otherCnt; i++)
    {
        pstAnjSmartInfo->stBoxInfo[boxCnt] = stOtherBox[i];
        boxCnt++;
    }
    pstAnjSmartInfo->boxCnt = boxCnt;
    anj_mutex_unlock(&s_stPvdMutex);
    return TS_SUCCESS;
}

int anj_mw_smart_pvd_init(float threshold, int enable)
{
    TS_S32 s32Ret = TS_SUCCESS;
    TS_U32 u32RgbBufSize = SMART_PVD_WIDTH * SMART_PVD_HEIGHT * 4;
    ALG_MOD8C_PARAM_S stPvdParam = {0};

    __INFO("smart pvd init start\n");

    anj_mutex_lock(&s_stPvdMutex);
    s_s32PvdEnable = enable;
    if (enable == 0)
    {
        anj_mutex_unlock(&s_stPvdMutex);
        __INFO("smart pvd skip model load, enable=0\n");
        return TS_SUCCESS;
    }
    if (s_s32PvdInitDone == 1)
    {
        anj_mutex_unlock(&s_stPvdMutex);
        return TS_SUCCESS;
    }

    if (s_bPvdModelLoaded == TS_FALSE)
    {
        TS_ALG_SetLogLevel(ALG_LOG_ERROR);
        s32Ret = TS_MPI_TRP_RNE_OpenDevice(NULL, NULL);
        if (s32Ret == TS_SUCCESS)
        {
            s_bRneOpened = TS_TRUE;
        }
        else
        {
            __WARN("TS_MPI_TRP_RNE_OpenDevice ret:%d, try share\n", s32Ret);
            s_bRneOpened = TS_FALSE;
        }

        s32Ret = TS_MPI_TRP_RNE_MutexInit();
        if (s32Ret == TS_SUCCESS)
        {
            s_bRneMutexInited = TS_TRUE;
        }
        else
        {
            __WARN("TS_MPI_TRP_RNE_MutexInit ret:%d, try share\n", s32Ret);
            s_bRneMutexInited = TS_FALSE;
        }

        s_pstRgbBufVirAddr = TS_MPI_TRP_RNE_AllocLinearMemCached(u32RgbBufSize, 0);
        if (s_pstRgbBufVirAddr == TS_NULL)
        {
            __ERR("TS_MPI_TRP_RNE_AllocLinearMemCached failed, size:%u\n", u32RgbBufSize);
            s32Ret = TS_FAILURE;
            goto exit_failed;
        }

        memset(&s_stPvdInputImage, 0, sizeof(ALG_IMAGE_S));
        s_stPvdInputImage.s32C = 4;
        s_stPvdInputImage.s32W = SMART_PVD_WIDTH;
        s_stPvdInputImage.s32H = SMART_PVD_HEIGHT;
        s_stPvdInputImage.pData = s_pstRgbBufVirAddr;
        s_stPvdInputImage.u64DataPhyAddr = TS_MPI_TRP_RNE_VirtualToPhysicalAddress((TS_SIZE_T)s_pstRgbBufVirAddr);

        memset(&s_stPvdModelInitParam, 0, sizeof(s_stPvdModelInitParam));
        s32Ret = TS_ALG_LoadCfgWeightOnce(SMART_PVD_CFG_PATH, SMART_PVD_WEIGHT_PATH, &s_stPvdModelInitParam);
        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_ALG_LoadCfgWeightOnce failed, cfg:%s weight:%s ret:%d\n",
                  SMART_PVD_CFG_PATH, SMART_PVD_WEIGHT_PATH, s32Ret);
            goto exit_failed;
        }
        s_bPvdModelLoaded = TS_TRUE;
        s_stPvdModelInitParam.enImageType = ALG_IMAGE_TYPE_INT_HWC_RGB0;
        s_stPvdModelInitParam.pSelfBuf = TS_NULL;
        s_stPvdModelInitParam.bRneOff = 1;
    }

    __INFO("TS_ALG_Mod8c version:%s cfg:%u weight:%u\n",
           TS_ALG_Mod8c_GetVersion(),
           s_stPvdModelInitParam.u32GraphSize,
           s_stPvdModelInitParam.u32WeightSize);
    s32Ret = TS_ALG_Mod8c_Init(&s_pstPvdHandle, &s_stPvdModelInitParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_ALG_Mod8c_Init failed, s32Ret:%d\n", s32Ret);
        goto exit_failed;
    }

    stPvdParam.f32ConfThresh = threshold;
    stPvdParam.f32IouThresh = 0.45f;
    (void)TS_ALG_Mod8c_SetParam(s_pstPvdHandle, &stPvdParam);

    s_s32PvdInitDone = 1;
    anj_mutex_unlock(&s_stPvdMutex);
    __INFO("smart pvd init successful\n");
    return TS_SUCCESS;

exit_failed:
    if (s_pstPvdHandle != TS_NULL)
    {
        TS_ALG_Mod8c_Exit(s_pstPvdHandle);
        s_pstPvdHandle = TS_NULL;
    }
    if (s_bPvdModelLoaded == TS_TRUE)
    {
        TS_ALG_FreeCfgWeightOnce(&s_stPvdModelInitParam);
        memset(&s_stPvdModelInitParam, 0, sizeof(s_stPvdModelInitParam));
        s_bPvdModelLoaded = TS_FALSE;
    }
    if (s_pstRgbBufVirAddr != TS_NULL)
    {
        TS_MPI_TRP_RNE_FreeLinearMemCached(s_pstRgbBufVirAddr);
        s_pstRgbBufVirAddr = TS_NULL;
    }
    memset(&s_stPvdInputImage, 0, sizeof(ALG_IMAGE_S));
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
    s_s32PvdInitDone = 0;
    s_s32LastMono = -1;
    anj_mutex_unlock(&s_stPvdMutex);
    return s32Ret;
}

int anj_mw_smart_pvd_uninit(void)
{
    anj_mutex_lock(&s_stPvdMutex);
    if (s_pstPvdHandle != TS_NULL)
    {
        TS_ALG_Mod8c_Exit(s_pstPvdHandle);
        s_pstPvdHandle = TS_NULL;
    }
    s_s32PvdInitDone = 0;
    s_s32PvdEnable = 0;
    s_s32LastMono = -1;
    anj_mutex_unlock(&s_stPvdMutex);
    return TS_SUCCESS;
}

static const anj_mw_smart_provider_ops s_stMwSmartProviderOps = {
    .provider_name = "ts_5326_pvd",
    .type = SMART_TYPE_PVD,
    .provider_priority = 100,
    .size_get = anj_mw_smart_pvd_ops_size_get,
};

ANJ_LINK_KEEP(anj_keep_mw_smart_pvd_provider);

__attribute__((constructor)) static void anj_mw_smart_provider_register_constructor(void)
{
    anj_mw_smart_provider_register(&s_stMwSmartProviderOps);
}

__attribute__((destructor)) static void anj_mw_smart_provider_unregister_constructor(void)
{
    anj_mw_smart_provider_unregister(&s_stMwSmartProviderOps);

    anj_mutex_lock(&s_stPvdMutex);
    if (s_pstPvdHandle != TS_NULL)
    {
        TS_ALG_Mod8c_Exit(s_pstPvdHandle);
        s_pstPvdHandle = TS_NULL;
    }
    if (s_bPvdModelLoaded == TS_TRUE)
    {
        TS_ALG_FreeCfgWeightOnce(&s_stPvdModelInitParam);
        memset(&s_stPvdModelInitParam, 0, sizeof(s_stPvdModelInitParam));
        s_bPvdModelLoaded = TS_FALSE;
    }
    if (s_pstRgbBufVirAddr != TS_NULL)
    {
        TS_MPI_TRP_RNE_FreeLinearMemCached(s_pstRgbBufVirAddr);
        s_pstRgbBufVirAddr = TS_NULL;
    }
    memset(&s_stPvdInputImage, 0, sizeof(ALG_IMAGE_S));
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
    s_s32PvdInitDone = 0;
    s_s32PvdEnable = 0;
    s_s32LastMono = -1;
    anj_mutex_unlock(&s_stPvdMutex);
}
