#include "anj_mw_media_common.h"
#include "anj_mw_smart_pd.h"
#include "anj_mw_smart_provider.h"
#include "sstar_dynamic_static_detect_api.h"
// #include "sdk_option.h"

static void *pstPdHandle = NULL;
static int s_stPdInitDone = 0;
static pthread_mutex_t s_stPdMutex = PTHREAD_MUTEX_INITIALIZER;
static float s_fPdThreshold = 0.0f;
int anj_mw_smart_pd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartPdAttr *pstAnjPdAttr)
{
    MI_S32 iRet = MI_SUCCESS;
    anj_mutex_lock(&s_stPdMutex);
    if (pstAnjPdAttr->enable && pstPdHandle && s_stPdInitDone)
    {
        int boxCnt = 0;
        DtBox_t bboxes[MAX_DT_OBJECT] = {0};
        MI_S32 num_bboxes = 0;
        ALGO_Input_t algo_input = {0}; // buffer info for det model
        algo_input.p_vir_addr = p_vir_addr;
        algo_input.phy_addr = p_phy_addr;
        algo_input.buf_size = len;

        iRet = ALGO_DT_Detect(pstPdHandle, &algo_input, bboxes, &num_bboxes);
        if (iRet != MI_SUCCESS)
        {
            __INFO("ALGO_DT_Detect failed! error code = %d\n", iRet);
        }

        iRet = ALGO_DT_Track(pstPdHandle, bboxes, &num_bboxes);
        if (iRet != MI_SUCCESS)
        {
            __INFO("ALGO_DT_GetTrackResult! error code = %d\n", iRet);
        }

        if (iRet == MI_SUCCESS && num_bboxes > 0)
        {
            for (int i_box = 0; i_box < num_bboxes; i_box++)
            {
                __DBG("[Track Result] track_id:%llu, class:%d, x:%d, y:%d, w:%d, h:%d, score:%f, moving:%d\n",
                       bboxes[i_box].track_id, bboxes[i_box].class_id,
                       bboxes[i_box].x, bboxes[i_box].y,
                       bboxes[i_box].width, bboxes[i_box].height,
                       bboxes[i_box].score, bboxes[i_box].moving);

                if (((pstAnjPdAttr->mdFilter == 0) && ((bboxes[i_box].class_id == 0) || (bboxes[i_box].class_id > 0 && bboxes[i_box].moving == 1))) ||
                    ((pstAnjPdAttr->mdFilter == 1) && (bboxes[i_box].moving == 1)) )
                {
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
        s32Ret = ALGO_DT_CreateHandle(&pstPdHandle);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ALGO_DT_CreateHandle failed, s32Ret:%d \n", s32Ret);
            return s32Ret;
        }
        DtInfo_t stPdInfo = {0};
        strncpy((char *)stPdInfo.ipu_firmware_path, SMART_PD_IPU_PATH, MAX_DT_STRLEN);
        strncpy((char *)stPdInfo.model, SMART_PD_MODEL_PATH, MAX_DT_STRLEN);
        stPdInfo.conf_threshold = threshold;
        stPdInfo.quality_threshold = threshold;
        stPdInfo.interval = 1;
        stPdInfo.disp_size.width = SMART_PD_WIDTH;
        stPdInfo.disp_size.height = SMART_PD_HEIGHT;
        stPdInfo.capture_mode = E_QUALITY_FIRST;
        stPdInfo.track_params.ignore_static_objects = false;
        stPdInfo.track_params.stable_bbox = false;
        stPdInfo.track_params.stable_sensitive = threshold;
        stPdInfo.track_params.use_algo_dt_detect = true;

        s32Ret = ALGO_DT_InitHandle(pstPdHandle, &stPdInfo);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ALGO_DT_InitHandle failed, s32Ret:%d \n", s32Ret);
            return s32Ret;
        }

        s32Ret = ALGO_DT_SetThreshold(pstPdHandle, threshold);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ALGO_DT_SetThreshold failed, s32Ret:%d \n", s32Ret);
            return s32Ret;
        }

        s32Ret = ALGO_DT_SetTrack(pstPdHandle, stPdInfo.track_params);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ALGO_DT_SetTrack failed, s32Ret:%d \n", s32Ret);
            return s32Ret;
        }
        s_fPdThreshold = threshold;
        s_stPdInitDone = 1;

        __INFO("smart pd init successful\n");
    }

    return s32Ret;
}

float anj_mw_smart_pd_set_sensitivity(float threshold)
{
    MI_S32 s32Ret = MI_SUCCESS;
    DtInfo_t stPdInfo = {0};
    float old_threshold = s_fPdThreshold;

    anj_mutex_lock(&s_stPdMutex);
    if (pstPdHandle == NULL || s_stPdInitDone == 0)
    {
        anj_mutex_unlock(&s_stPdMutex);
        return s_fPdThreshold;
    }

    s32Ret = ALGO_DT_SetThreshold(pstPdHandle, threshold);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("ALGO_DT_SetThreshold failed, s32Ret:%d \n", s32Ret);
        anj_mutex_unlock(&s_stPdMutex);
        return old_threshold;
    }

    stPdInfo.track_params.ignore_static_objects = false;
    stPdInfo.track_params.stable_bbox = false;
    stPdInfo.track_params.stable_sensitive = threshold;
    stPdInfo.track_params.use_algo_dt_detect = true;
    s32Ret = ALGO_DT_SetTrack(pstPdHandle, stPdInfo.track_params);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("ALGO_DT_SetTrack failed, s32Ret:%d \n", s32Ret);
        anj_mutex_unlock(&s_stPdMutex);
        return old_threshold;
    }
    s_fPdThreshold = threshold;
    anj_mutex_unlock(&s_stPdMutex);

    return s_fPdThreshold;
}

int anj_mw_smart_pd_uninit()
{
    MI_S32 s32Ret = MI_SUCCESS;

    anj_mutex_lock(&s_stPdMutex);
    if (pstPdHandle)
    {
        ALGO_DT_DeinitHandle(pstPdHandle);
        ALGO_DT_ReleaseHandle(pstPdHandle);
        pstPdHandle = NULL;
    }
    s_stPdInitDone = 0;
    s_fPdThreshold = 0.0f;
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
    .provider_name = "mstar_i6c_pd",
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
