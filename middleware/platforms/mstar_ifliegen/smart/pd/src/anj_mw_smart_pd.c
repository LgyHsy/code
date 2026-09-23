#include "anj_mw_media_common.h"
#include "anj_mw_smart_pd.h"
#include "anj_mw_smart_provider.h"
#include "sgs_det_api.h"
// #include "sdk_option.h"

static void *pstPdHandle = NULL;
static int s_stPdInitDone = 0;
static DetParams_t s_stDetParams = {0};
static pthread_mutex_t s_stPdMutex = PTHREAD_MUTEX_INITIALIZER;
int anj_mw_smart_pd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartPdAttr *pstAnjPdAttr)
{
    MI_S32 iRet = MI_SUCCESS;
    anj_mutex_lock(&s_stPdMutex);
    if (pstAnjPdAttr->enable && pstPdHandle && s_stPdInitDone)
    {
        int boxCnt = 0;
        DetBox_t bboxes[MAX_DET_OBJECT] = {0};
        MI_S32 num_bboxes = 0;
        DetInput_t algo_input = {0}; // buffer info for det model
        algo_input.p_vir_addr = p_vir_addr;
        algo_input.phy_addr = p_phy_addr;
        algo_input.buf_size = len;

        iRet = ALGO_DET_Run(pstPdHandle, &algo_input, bboxes, &num_bboxes);
        if (iRet != MI_SUCCESS)
        {
            __INFO("ALGO_DET_Run failed! error code = %d\n", iRet);
        }

        if (iRet == MI_SUCCESS && num_bboxes > 0)
        {
            for (int i_box = 0; i_box < num_bboxes; i_box++)
            {
                __DBG("[Track Result] class:%d, x:%d, y:%d, w:%d, h:%d, score:%f\n",
                      bboxes[i_box].class_id,
                      bboxes[i_box].x, bboxes[i_box].y,
                      bboxes[i_box].width, bboxes[i_box].height,
                      bboxes[i_box].score);

                if (bboxes[i_box].class_id != 0)
                {
                    continue;
                }
                if (pstAnjPdAttr->minRectFilter)
                {
                    if (bboxes[i_box].width * bboxes[i_box].height < pstAnjPdAttr->minRectFilter)
                    {
                        continue;
                    }
                }
                if (boxCnt >= SMART_MAX_DETECT_RECT)
                {
                    break;
                }
                pstAnjSmartInfo->stBoxInfo[boxCnt].x = bboxes[i_box].x;
                pstAnjSmartInfo->stBoxInfo[boxCnt].y = bboxes[i_box].y;
                pstAnjSmartInfo->stBoxInfo[boxCnt].width = bboxes[i_box].width;
                pstAnjSmartInfo->stBoxInfo[boxCnt].height = bboxes[i_box].height;
                pstAnjSmartInfo->stBoxInfo[boxCnt].class_id = bboxes[i_box].class_id;
                pstAnjSmartInfo->stBoxInfo[boxCnt].score = bboxes[i_box].score;
                boxCnt++;
            }
        }
        pstAnjSmartInfo->boxCnt = boxCnt;
    }
    anj_mutex_unlock(&s_stPdMutex);
    return iRet;
}

int anj_mw_smart_pd_init(float threshold)
{
    __INFO("smart pd threshold:%f\n", threshold);
    MI_S32 s32Ret = MI_SUCCESS;
    if (pstPdHandle == NULL)
    {
        s32Ret = ALGO_DET_CreateHandle(&pstPdHandle);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ALGO_DET_CreateHandle failed, s32Ret:%d \n", s32Ret);
            return s32Ret;
        }
        DetInit_t stPdInfo = {0};
        strncpy((char *)stPdInfo.ipu_firmware_path, SMART_PD_IPU_PATH, MAX_DET_STRLEN);
        strncpy((char *)stPdInfo.model_path, SMART_PD_MODEL_PATH, MAX_DET_STRLEN);
        stPdInfo.create_device = true;
        stPdInfo.destroy_device = true;
        stPdInfo.model_buffer = NULL;
        stPdInfo.model_buffer_len = 0;

        s32Ret = ALGO_DET_InitHandle(pstPdHandle, &stPdInfo);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ALGO_DET_InitHandle failed, s32Ret:%d \n", s32Ret);
            return s32Ret;
        }

        if (threshold > 0.6)
        {
            __INFO("pd algo threshold max 0.6!\n");
            threshold = 0.6;
        }

        s_stDetParams.conf_threshold = threshold;
        s_stDetParams.disp_width = SMART_PD_WIDTH;
        s_stDetParams.disp_height = SMART_PD_HEIGHT;
        s_stDetParams.ignore_static_objects = false;
        s_stDetParams.static_sensitive = threshold;
        s_stDetParams.stable_bbox = true;
        s_stDetParams.stable_sensitive = threshold;
        s_stDetParams.ignore_frame_number = 0;
        s_stDetParams.strict_mode = false;

        s32Ret = ALGO_DET_SetParams(pstPdHandle, &s_stDetParams);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ALGO_DET_SetParams failed, s32Ret:%d \n", s32Ret);
            return s32Ret;
        }

        s_stPdInitDone = 1;

        __INFO("smart pd init successful\n");
    }

    return s32Ret;
}

float anj_mw_smart_pd_set_sensitivity(float threshold)
{
    MI_S32 s32Ret = MI_SUCCESS;
    if (threshold > 0.6)
    {
        __INFO("pd algo threshold max 0.6!\n");
        threshold = 0.6;
    }

    anj_mutex_lock(&s_stPdMutex);
    if (pstPdHandle == NULL || s_stPdInitDone == 0)
    {
        anj_mutex_unlock(&s_stPdMutex);
        return s_stDetParams.stable_sensitive;
    }

    float old_threshold = s_stDetParams.stable_sensitive;
    if (s_stDetParams.static_sensitive != threshold || s_stDetParams.stable_sensitive != threshold)
    {
        s_stDetParams.stable_sensitive = threshold;
        s_stDetParams.static_sensitive = threshold;
    }

    s32Ret = ALGO_DET_SetParams(pstPdHandle, &s_stDetParams);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("ALGO_DET_SetParams failed, s32Ret:%d \n", s32Ret);
        s_stDetParams.stable_sensitive = old_threshold;
        s_stDetParams.static_sensitive = threshold;
    }
    anj_mutex_unlock(&s_stPdMutex);

    return s_stDetParams.stable_sensitive;
}

int anj_mw_smart_pd_uninit()
{
    MI_S32 s32Ret = MI_SUCCESS;

    anj_mutex_lock(&s_stPdMutex);
    if (pstPdHandle)
    {
        ALGO_DET_DeinitHandle(pstPdHandle);
        ALGO_DET_ReleaseHandle(pstPdHandle);
        pstPdHandle = NULL;
    }
    s_stPdInitDone = 0;
    anj_mutex_unlock(&s_stPdMutex);

    return s32Ret;
}

static int anj_mw_smart_pd_ops_init(AnjSmartAttr *pstAnjSmartAttr)
{
    int iRet = anj_mw_smart_pd_init(pstAnjSmartAttr->stAnjPdAttr.sensitivity);
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

static const anj_mw_smart_provider_ops s_stMwSmartProviderOps = {
    .provider_name = "mstar_ifliegen_pd",
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
