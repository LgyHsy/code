#include <sys/prctl.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_smart_md.h"
#include "anj_smart_provider.h"
#include "anj_smart_md_util.h"

#define TEST_IS_NIGHT 0
#define MD_CHECK_START_TIME (5 * 1000)
#define MD_CHECK_TIMES (300)

typedef struct _MD_MOD_OBJECT_S
{
    ANJ_MD_HANDLE mdHdl;
    int picWidth;
    int picHeight;
    int sensitivity;
    int threshold;
    int cell_thrd;
    int total_cell;
    unsigned long long tInitTime;
    unsigned long long tLastCheckTime;
    int enable;
} MD_MOD_OBJECT_S;

static MD_MOD_OBJECT_S g_md_obj[ANJ_CAMERA_MAX_NUMS];

static pthread_mutex_t s_stMdMutex = PTHREAD_MUTEX_INITIALIZER;

static int anj_md_set_block_cfg(MD_BLOCK_CFG *pBlockCfg, int cameraIndex)
{
    if (pBlockCfg->block_x != MD_MAX_W_DIV_NUM || pBlockCfg->block_y != MD_MAX_H_DIV_NUM)
    {
        ANJ_MDParamsIn_t md_param;
        md_param.sensitivity = g_md_obj[cameraIndex].sensitivity;
        md_param.size_percent_min = g_md_obj[cameraIndex].threshold;
        md_param.size_percent_max = 100;

        if (md_param.sensitivity < 1)
            md_param.sensitivity = 1;

        int xIndex, yIndex, regIndex = 0;
        for (yIndex = 0; yIndex < MD_MAX_H_DIV_NUM; yIndex++)
        {
            for (xIndex = 0; xIndex < MD_MAX_W_DIV_NUM; xIndex++)
            {
                int cfg_x = 0;
                int cfg_y = 0;

                cfg_x = xIndex * (pBlockCfg->block_x) / MD_MAX_W_DIV_NUM;
                cfg_y = yIndex * (pBlockCfg->block_y) / MD_MAX_H_DIV_NUM;

                md_param.enable = pBlockCfg->block_config[cfg_y][cfg_x];

                ANJ_MD_SetDetectRegion(g_md_obj[cameraIndex].mdHdl, regIndex, &md_param);
                regIndex++;
            }
        }
    }
    else
    {
        ANJ_MDParamsIn_t md_param;
        md_param.sensitivity = g_md_obj[cameraIndex].sensitivity;
        md_param.size_percent_min = g_md_obj[cameraIndex].threshold;
        md_param.size_percent_max = 100;

        int xIndex, yIndex, regIndex = 0;
        for (yIndex = 0; yIndex < pBlockCfg->block_y; yIndex++)
        {
            for (xIndex = 0; xIndex < pBlockCfg->block_x; xIndex++)
            {
                md_param.enable = pBlockCfg->block_config[yIndex][xIndex];
                ANJ_MD_SetDetectRegion(g_md_obj[cameraIndex].mdHdl, regIndex, &md_param);
                regIndex++;
            }
        }
    }

    ANJ_MD_PrintCfg(g_md_obj[cameraIndex].mdHdl);

    return 0;
}

int anj_md_cfg_set(int cameraIndex)
{
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pstMdAlarm = &pstAlarmCfg->normalAlarm.motionDetectAlarm[cameraIndex];
    pthread_mutex_lock(&s_stMdMutex);
    g_md_obj[cameraIndex].enable = pstMdAlarm->enable;
    /* 人形开则不支持 MD */
    if (pstAlarmCfg->aiAlarm.pdAlarm[cameraIndex].enable)
    {
        g_md_obj[cameraIndex].enable = 0;
    }
    if (TEST_IS_NIGHT && pstMdAlarm->dayNightSwitch)
    {
        g_md_obj[cameraIndex].sensitivity = pstMdAlarm->nightSensitivity;
        g_md_obj[cameraIndex].threshold = pstMdAlarm->nightAlarmThreshold;
    }
    else
    {
        g_md_obj[cameraIndex].sensitivity = pstMdAlarm->sensitivity;
        g_md_obj[cameraIndex].threshold = pstMdAlarm->alarmThreshold;
    }
    g_md_obj[cameraIndex].cell_thrd = g_md_obj[cameraIndex].threshold * 0.1;
    if (g_md_obj[cameraIndex].cell_thrd < 1)
    {
        g_md_obj[cameraIndex].cell_thrd = 1;
    }

    MD_BLOCK_CFG stMdBlockCfg = {0};
    stMdBlockCfg.block_x = pstMdAlarm->blockCount >> 16;
    stMdBlockCfg.block_y = pstMdAlarm->blockCount & 0xffff;
    if (stMdBlockCfg.block_x < 1)
        stMdBlockCfg.block_x = 1;
    if (stMdBlockCfg.block_x > MD_MAX_GRID_COL)
        stMdBlockCfg.block_x = MD_MAX_GRID_COL;
    if (stMdBlockCfg.block_y < 1)
        stMdBlockCfg.block_y = 1;
    if (stMdBlockCfg.block_y > MD_MAX_GRID_ROW)
        stMdBlockCfg.block_x = MD_MAX_GRID_ROW;
    memset(stMdBlockCfg.block_config, 0, MD_MAX_H_DIV_NUM * MD_MAX_W_DIV_NUM * sizeof(int));
    int nMaxY = (stMdBlockCfg.block_y < MD_MAX_H_DIV_NUM) ? stMdBlockCfg.block_y : MD_MAX_H_DIV_NUM;
    int nMaxX = (stMdBlockCfg.block_x < MD_MAX_W_DIV_NUM) ? stMdBlockCfg.block_x : MD_MAX_W_DIV_NUM;
    for (int yIndex = 0; yIndex < nMaxY; yIndex++)
    {
        // char tmpStr[64] = {0};
        for (int xIndex = 0; xIndex < nMaxX; xIndex++)
        {
            char cType = pstMdAlarm->blockCfg[yIndex * nMaxX + xIndex];
            if (cType == '1')
            {
                stMdBlockCfg.block_config[yIndex][xIndex] = 1;
            }
            else
            {
                stMdBlockCfg.block_config[yIndex][xIndex] = 0;
            }

            // tmpStr[xIndex] = cType;
        }

        // __INFO("%02d: %s\n", yIndex, tmpStr);
    }
    anj_md_set_block_cfg(&stMdBlockCfg, cameraIndex);
    pthread_mutex_unlock(&s_stMdMutex);
    return 0;
}

int anj_md_init(int pic_width, int pic_height)
{
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        memset(&g_md_obj[cameraIndex], 0, sizeof(MD_MOD_OBJECT_S));
        g_md_obj[cameraIndex].mdHdl = ANJ_MD_Create(pic_width, pic_height, MD_MAX_W_DIV_NUM, MD_MAX_H_DIV_NUM);
        if (g_md_obj[cameraIndex].mdHdl == NULL)
        {
            __ERR("ANJ_MD_Create error \n");
            return -1;
        }

        g_md_obj[cameraIndex].picWidth = pic_width;
        g_md_obj[cameraIndex].picHeight = pic_height;
        g_md_obj[cameraIndex].total_cell = ANJ_MD_GetTotalCellNum(g_md_obj[cameraIndex].mdHdl);
        g_md_obj[cameraIndex].tInitTime = anj_mw_get_cputime_ms(NULL);

        anj_md_cfg_set(cameraIndex);
    }

    __INFO("smart md init successful\n");
    return 0;
}

int anj_md_uninit()
{
    pthread_mutex_lock(&s_stMdMutex);
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (g_md_obj[cameraIndex].mdHdl)
        {
            ANJ_MD_Destroy(g_md_obj[cameraIndex].mdHdl);
            g_md_obj[cameraIndex].mdHdl = NULL;
        }
    }
    pthread_mutex_unlock(&s_stMdMutex);

    return 0;
}

void anj_md_process(void *p_vir_addr, int len, int cameraIndex, MD_RESULT_S *pstMdResult)
{
    int iRet = 0;
    unsigned long long tNowTime = anj_mw_get_cputime_ms(NULL);
    // 编码刚启动， 亮度变化较大， 前5s不检测
    if (tNowTime - g_md_obj[cameraIndex].tInitTime <= MD_CHECK_START_TIME)
    {
        return;
    }

    // 300毫秒以上做一次侦测
    if (tNowTime - g_md_obj[cameraIndex].tLastCheckTime <= MD_CHECK_TIMES)
    {
        return;
    }

    pthread_mutex_lock(&s_stMdMutex);
    if (g_md_obj[cameraIndex].enable && g_md_obj[cameraIndex].mdHdl)
    {
        g_md_obj[cameraIndex].tLastCheckTime = anj_mw_get_cputime_ms(NULL);
        iRet = ANJ_MD_Detect(g_md_obj[cameraIndex].mdHdl, (const uint8_t *)p_vir_addr, len, tNowTime);
#if 0
        __ERR("CellNum=%d,TotalNum=%d, cell_thrd=%d, luma_sensitivity=%d, MaxPixLumaDiff=%d, MaxCellLumaDiff=%d, pass_time=%d\n", 
            iRet, g_md_obj[cameraIndex].total_cell, g_md_obj[cameraIndex].cell_thrd,
            g_md_obj[cameraIndex].sensitivity, ANJ_MD_GetMaxPixLumaDiff(g_md_obj[cameraIndex].mdHdl), 
            ANJ_MD_GetMaxCellLumaDiff(g_md_obj[cameraIndex].mdHdl));
#endif
        ANJ_MOTION_REGION_S *pMdRegions = NULL;
        int mdRegionCnt = ANJ_MD_GetMotionRegion(g_md_obj[cameraIndex].mdHdl, &pMdRegions);

        memset(pstMdResult, 0, sizeof(MD_RESULT_S));
        pstMdResult->md_cell_num = iRet;
        pstMdResult->region_cnt = mdRegionCnt;
        for (int i = 0; i < mdRegionCnt; i++)
        {
            MOTION_REGION_S *pDstRegion = &pstMdResult->md_regions[i];
            ANJ_MOTION_REGION_S *pSrcRegion = &pMdRegions[i];
            pDstRegion->lt_x = pSrcRegion->lt_x;
            pDstRegion->lt_y = pSrcRegion->lt_y;
            pDstRegion->rb_x = pSrcRegion->rb_x;
            pDstRegion->rb_y = pSrcRegion->rb_y;
            pDstRegion->motion_cell = pSrcRegion->motion_cell;
            pDstRegion->row = pSrcRegion->row;
            pDstRegion->col = pSrcRegion->col;
        }

        if (pMdRegions)
        {
            free(pMdRegions);
            pMdRegions = NULL;
        }
    }
    pthread_mutex_unlock(&s_stMdMutex);
}

static smart_mask_e anj_smart_md_mask_get(void)
{
    return SMART_SET_MASK(SMART_NULL_MASK, SMART_MOTION_MASK);
}

static int anj_smart_md_provider_init(int pic_width, int pic_height)
{
    return anj_md_init(pic_width, pic_height);
}

static int anj_smart_md_provider_uninit(void)
{
    return anj_md_uninit();
}

static int anj_smart_md_provider_process(void *p_vir_addr, int len, int cameraIndex, MD_RESULT_S *pstMdResult)
{
    anj_md_process(p_vir_addr, len, cameraIndex, pstMdResult);
    return 0;
}

static const anj_smart_md_ops s_stSmartMdOps = {
    .provider_name = "md",
    .provider_priority = 100,
    .mask_get = anj_smart_md_mask_get,
    .init = anj_smart_md_provider_init,
    .uninit = anj_smart_md_provider_uninit,
    .process = anj_smart_md_provider_process,
};

ANJ_LINK_KEEP(anj_keep_smart_md_provider);

__attribute__((constructor)) static void anj_smart_md_provider_register_constructor(void)
{
    anj_smart_md_provider_register(&s_stSmartMdOps);
}

__attribute__((destructor)) static void anj_smart_md_provider_unregister_constructor(void)
{
    anj_smart_md_provider_unregister(&s_stSmartMdOps);
}
