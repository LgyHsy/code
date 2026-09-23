#include "anj_mw_media_common.h"
#include "anj_mw_media_video.h"
#include "anj_mw_media_isp.h"
#include "anj_mw_file.h"

#include <math.h>
#include <pthread.h>

#define ZOOM_TARCK_FAST_GAIN 12
#define ZOOM_TRACK_FAST_GAIN_DEBUG_FILE "/tmp/fast_gain"
#define ZOOM_TRACK_FAST_GAIN_MIN 8
#define ZOOM_TRACK_PX_PER_STEP 100
#define ZOOM_TRACK_MIN_STEP 8
#define ZOOM_TRACK_MAX_STEP 12
#define AIISP_ENTER_GAIN_TH 64.0f
#define AIISP_EXIT_GAIN_TH 48.0f
#define AIISP_EXIT_BV_TH (-160000)
#define AIISP_SWITCH_MAX_CNT (80)
#define ZOOM_DEFAULT_FPS 30
#define ZOOM_ALIGN 2

typedef struct
{
    TS_U16 x; /* 裁剪框左上角 X（相对 VI pipe，像素） */
    TS_U16 y; /* 裁剪框左上角 Y（相对 VI pipe，像素） */
    TS_U16 w; /* 裁剪框宽度（像素）；倍率 ≈ baseW / w */
    TS_U16 h; /* 裁剪框高度（像素）；通常按原图宽高比由 w 推导 */
} AnjTsZoomEntry;

static AnjTsZoomEntry s_zoomEntry[ISP_ZOOM_ENTRY_CNT] = {0};
static int s_zoomNum = 0; /* zoom 数量 */
static int s_from = 0; /* 开始运行的 zoom 索引 */
static int s_to = 0; /* 结束运行的 zoom 索引 */
/* Last crop successfully submitted to VPSS, never the next scheduled entry. */
static int s_cur = 0; /* 当前实际运行的 zoom 索引 */
static int s_next = 0; /* 下次运行的 zoom 索引 */
static int s_running = 0; /* 是否正在运行 zoom */
static unsigned int s_zoomGeneration = 0; /* zoom 运行代数，用于区分不同运行状态 */
static int s_zoomCamera = 0; /* 当前运行的相机索引 */
static int s_iZoomTrackFastGain = ZOOM_TARCK_FAST_GAIN;
static anj_thread_s s_stZoomThread = {0};
static pthread_mutex_t s_stZoomMutex = PTHREAD_MUTEX_INITIALIZER;
/* Serialize public software-zoom commands while the worker owns crop state. */
static pthread_mutex_t s_stZoomCmdMutex = PTHREAD_MUTEX_INITIALIZER;
static TS_U32 s_zoomOriginX = 0;
static TS_U32 s_zoomOriginY = 0;

static void anj_mw_zoom_base_size_get(int iCameraIdex, TS_U32 *pBaseW, TS_U32 *pBaseH, int *pFramePeriodUs)
{
    TS_Common_VideoAttr_t *pstVideoAttr = (TS_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    TS_Common_ViPipe_t *pstPipe = &pstVideoAttr->ViAttr.astViInfo[iCameraIdex].stPipeInfo;
    TS_U32 pipeW = pstPipe->width;
    TS_U32 pipeH = pstPipe->height;
    TS_FLOAT fps = pstPipe->frameRate;

    /* GrpCrop 坐标系 = VI pipe，不做编码口 origin 偏移 */
    s_zoomOriginX = 0;
    s_zoomOriginY = 0;

    *pBaseW = pipeW;
    *pBaseH = pipeH;
    if (fps < 1.0f)
    {
        fps = (TS_FLOAT)ZOOM_DEFAULT_FPS;
    }
    if (pFramePeriodUs)
    {
        *pFramePeriodUs = (int)(1000000.0f / fps);
        if (*pFramePeriodUs < 1000)
        {
            *pFramePeriodUs = 1000;
        }
    }
}

/* 与 mstar 一致：线性与 smoothstep 各半，端点严格 0/1 */
static float anj_mw_zoom_s_curve_progress(float t)
{
    float smooth;

    if (t <= 0.0f)
    {
        return 0.0f;
    }
    if (t >= 1.0f)
    {
        return 1.0f;
    }
    smooth = t * t * (3.0f - 2.0f * t);
    return (t + smooth) / 2.0f;
}

static void anj_mw_zoom_entry_clamp(AnjTsZoomEntry *pEntry, TS_U32 baseW, TS_U32 baseH)
{
    if (pEntry->w < ZOOM_ALIGN)
    {
        pEntry->w = ZOOM_ALIGN;
    }
    if (pEntry->h < ZOOM_ALIGN)
    {
        pEntry->h = ZOOM_ALIGN;
    }
    if ((TS_U32)pEntry->w > baseW)
    {
        pEntry->w = (TS_U16)baseW;
    }
    if ((TS_U32)pEntry->h > baseH)
    {
        pEntry->h = (TS_U16)baseH;
    }
    if ((TS_U32)pEntry->x + (TS_U32)pEntry->w > baseW)
    {
        pEntry->x = (TS_U16)(baseW - pEntry->w);
    }
    if ((TS_U32)pEntry->y + (TS_U32)pEntry->h > baseH)
    {
        pEntry->y = (TS_U16)(baseH - pEntry->h);
    }
}

/* 固定画面中心再对齐，避免逐档 ALIGN_DOWN 导致中心抖动 */
static void anj_mw_zoom_entry_set_wh_center(AnjTsZoomEntry *pEntry, TS_U32 baseW, TS_U32 baseH, TS_U32 w, TS_U32 h)
{
    int cx = (int)baseW / 2;
    int cy = (int)baseH / 2;
    int x = 0;
    int y = 0;

    w = ANJ_ALIGN_DOWN(w, ZOOM_ALIGN);
    h = ANJ_ALIGN_DOWN(h, ZOOM_ALIGN);
    if (w < ZOOM_ALIGN)
    {
        w = ZOOM_ALIGN;
    }
    if (h < ZOOM_ALIGN)
    {
        h = ZOOM_ALIGN;
    }
    if (w > baseW)
    {
        w = baseW;
    }
    if (h > baseH)
    {
        h = baseH;
    }

    x = ANJ_ALIGN_DOWN(cx - (int)w / 2, ZOOM_ALIGN);
    y = ANJ_ALIGN_DOWN(cy - (int)h / 2, ZOOM_ALIGN);
    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }

    pEntry->w = (TS_U16)w;
    pEntry->h = (TS_U16)h;
    pEntry->x = (TS_U16)x;
    pEntry->y = (TS_U16)y;
    anj_mw_zoom_entry_clamp(pEntry, baseW, baseH);
}

static int anj_mw_zoom_step_thread(void *ctx, int *bStart)
{
    int iCameraIdex = s_zoomCamera;
    int framePeriodUs = 1000000 / ZOOM_DEFAULT_FPS;
    TS_U32 baseW = 0;
    TS_U32 baseH = 0;

    (void)ctx;
    anj_mw_zoom_base_size_get(iCameraIdex, &baseW, &baseH, &framePeriodUs);

    while (bStart && *bStart)
    {
        AnjTsZoomEntry entry = {0};
        int cur = 0;
        int to = 0;
        int dir = 1;
        int running = 0;
        int validEntry = 0;
        unsigned int generation = 0;
        int ret = TS_FAILURE;

        /* Do not let a retarget replace the table while this entry is sent. */
        pthread_mutex_lock(&s_stZoomMutex);
        running = s_running;
        cur = s_next;
        to = s_to;
        dir = (s_to >= s_from) ? 1 : -1;
        generation = s_zoomGeneration;
        if (running && cur >= 0 && cur < s_zoomNum)
        {
            entry = s_zoomEntry[cur];
            validEntry = (entry.w != 0 && entry.h != 0);
        }

        if (!running || !validEntry)
        {
            if (running)
            {
                s_running = 0;
                __ERR("zoom worker invalid entry gen:%u entry:%d num:%d\n",
                      generation, cur, s_zoomNum);
            }
            pthread_mutex_unlock(&s_stZoomMutex);
            usleep(framePeriodUs);
            continue;
        }

        ret = TS_COMMON_VPSS_GrpCrop(iCameraIdex, entry.x, entry.y, entry.w, entry.h);
        if (ret != TS_SUCCESS)
        {
            if (generation == s_zoomGeneration)
            {
                s_running = 0;
            }
            pthread_mutex_unlock(&s_stZoomMutex);
            __ERR("zoom crop apply failed gen:%u entry:%d rect:%d,%d,%d,%d\n",
                  generation, cur, entry.x, entry.y, entry.w, entry.h);
            usleep(framePeriodUs);
            continue;
        }

        /* The crop call succeeded, so cur is the only state zoom_get may report. */
        if (generation == s_zoomGeneration && s_running)
        {
            s_cur = cur;
            if (cur == to)
            {
                s_running = 0;
            }
            else
            {
                s_next = cur + dir;
            }
        }
        pthread_mutex_unlock(&s_stZoomMutex);

        usleep(framePeriodUs);
    }

    return 0;
}

static void anj_mw_zoom_trajectory_stop(void)
{
    pthread_mutex_lock(&s_stZoomMutex);
    s_running = 0;
    s_zoomGeneration++;
    pthread_mutex_unlock(&s_stZoomMutex);
}

static void anj_mw_zoom_thread_stop(void)
{
    anj_mw_zoom_trajectory_stop();

    if (s_stZoomThread.start)
    {
        anj_thread_task_destroy(&s_stZoomThread, 0);
        memset(&s_stZoomThread, 0, sizeof(s_stZoomThread));
    }
}

static int anj_mw_zoom_thread_start(int iCameraIdex, int from, int to)
{
    int needStart = 0;
    AnjTsZoomEntry entry = {0};

    if (from < 0 || to < 0 || from >= s_zoomNum || to >= s_zoomNum)
    {
        __ERR("zoom index invalid from:%d to:%d num:%d\n", from, to, s_zoomNum);
        return -1;
    }

    pthread_mutex_lock(&s_stZoomMutex);
    s_zoomCamera = iCameraIdex;
    s_from = from;
    s_to = to;
    s_cur = from;
    s_next = from;
    s_running = (from != to);
    s_zoomGeneration++;
    entry = s_zoomEntry[from];
    needStart = (from != to && !s_stZoomThread.start);

    if (from == to)
    {
        if (TS_COMMON_VPSS_GrpCrop(iCameraIdex, entry.x, entry.y, entry.w, entry.h) != TS_SUCCESS)
        {
            pthread_mutex_unlock(&s_stZoomMutex);
            __ERR("zoom crop apply failed entry:%d rect:%d,%d,%d,%d\n",
                  from, entry.x, entry.y, entry.w, entry.h);
            return -1;
        }
        pthread_mutex_unlock(&s_stZoomMutex);
        return 0;
    }

    if (!needStart)
    {
        pthread_mutex_unlock(&s_stZoomMutex);
        return 0;
    }

    memset(&s_stZoomThread, 0, sizeof(s_stZoomThread));
    s_stZoomThread.bAutoDestroy = 0;
    strncpy(s_stZoomThread.iThreadName, "rm_zoom", sizeof(s_stZoomThread.iThreadName) - 1);
    s_stZoomThread.iThreadjob.ctx = NULL;
    s_stZoomThread.iThreadjob.func = anj_mw_zoom_step_thread;
    if (anj_thread_task_create(&s_stZoomThread) != 0)
    {
        __ERR("zoom thread create failed\n");
        s_running = 0;
        pthread_mutex_unlock(&s_stZoomMutex);
        memset(&s_stZoomThread, 0, sizeof(s_stZoomThread));
        return -1;
    }
    pthread_mutex_unlock(&s_stZoomMutex);
    return 0;
}

static int anj_mw_zoom_track_fast_gain_clamp(int gain)
{
    if (gain < ZOOM_TRACK_FAST_GAIN_MIN)
    {
        gain = ZOOM_TRACK_FAST_GAIN_MIN;
    }
    if (gain >= ISP_ZOOM_ENTRY_CNT)
    {
        gain = ISP_ZOOM_ENTRY_CNT - 1;
    }
    return gain;
}

static void anj_mw_zoom_track_fast_gain_load(void)
{
    char *str = NULL;

    s_iZoomTrackFastGain = ZOOM_TARCK_FAST_GAIN;
    str = anj_mw_read_file_buffer(ZOOM_TRACK_FAST_GAIN_DEBUG_FILE);
    if (str)
    {
        s_iZoomTrackFastGain = anj_mw_zoom_track_fast_gain_clamp(atoi(str));
        anj_mw_free(str);
    }
}

int anj_mw_media_isp_brightness_set(int iCameraIdex, int brightness)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspBrightnessSet(ViPipe, brightness));
    return 0;
}

int anj_mw_media_isp_contrast_set(int iCameraIdex, int contrast)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspContrastSet(ViPipe, contrast));
    return 0;
}

int anj_mw_media_isp_saturation_get(int iCameraIdex, char *saturation)
{
    VI_PIPE ViPipe = iCameraIdex;
    ISP_USR_PREFERENCE_S stPref = {0};
    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    for (int i = 0; i < 16; i++)
    {
        saturation[i] = (char)stPref.u32Saturation;
    }
    return 0;
}

int anj_mw_media_isp_saturation_set(int iCameraIdex, char *oriSaturation, int saturation)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspSaturationSet(ViPipe, saturation));
    return 0;
}

int anj_mw_media_isp_sharpness_get(int iCameraIdex, char *sharpness0, char *sharpness1, char *sharpness2)
{
    VI_PIPE ViPipe = iCameraIdex;
    ISP_USR_PREFERENCE_S stPref = {0};
    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    for (int i = 0; i < 16; i++)
    {
        sharpness0[i] = (char)stPref.u32Sharpness;
        sharpness1[i] = (char)stPref.u32Sharpness;
        sharpness2[i] = (char)stPref.u32Sharpness;
    }
    return 0;
}

int anj_mw_media_isp_sharpness_set(int iCameraIdex, char *sharpness0, char *sharpness1, char *sharpness2, int sharpness)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspSharpnessSet(ViPipe, sharpness));
    return 0;
}

int anj_mw_media_isp_backlight_get(int iCameraIdex, char *backlight)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspBLCGet(ViPipe, backlight));
    return 0;
}

int anj_mw_media_isp_backlight_set(int iCameraIdex, char *oriBacklight, int backlight)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspBLCSet(ViPipe, backlight));
    return 0;
}

int anj_mw_media_isp_filcker_set(int iCameraIdex, int hz)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspFilckerSet(ViPipe, hz));
    return 0;
}

int anj_mw_media_isp_shutterus_set(int iCameraIdex, int minShutter, int maxShutter)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspShutterSet(ViPipe, minShutter, maxShutter));
    return 0;
}

int anj_mw_media_isp_gain_set(int iCameraIdex, int maxGain)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspMaxGainSet(ViPipe, maxGain));
    return 0;
}

int anj_mw_media_isp_rotate_set(int iCameraIdex, int rotate)
{
    (void)iCameraIdex;
    (void)rotate;
    return 0;
}

int anj_mw_media_isp_flip_set(int iCameraIdex, int hflip, int vflip)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_SensorMirrorSet(ViPipe, hflip, vflip));
    return 0;
}

int anj_mw_media_isp_awb_set(int iCameraIdex, int whitebalance)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspAWBSet(ViPipe, whitebalance));
    return 0;
}

int anj_mw_media_isp_wdr_value_get(int iCameraIdex, char *wdrvalue)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspWdrValueGet(ViPipe, wdrvalue));
    return 0;
}

int anj_mw_media_isp_wdr_value_set(int iCameraIdex, char *oriWdrValue, int wdrvalue)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspWdrValueSet(ViPipe, oriWdrValue, wdrvalue));
    return 0;
}

int anj_mw_media_isp_hlc_set(int iCameraIdex, int hlc, int brightness)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspHLCSet(ViPipe, hlc));
    return 0;
}

int anj_mw_media_isp_2dnr_get(int iCameraIdex, char *tnf)
{
    VI_PIPE ViPipe = iCameraIdex;
    ISP_USR_PREFERENCE_S stPref = {0};
    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    for (int i = 0; i < 16; i++)
    {
        tnf[i] = (char)stPref.u32RawDenoise;
    }
    return 0;
}

int anj_mw_media_isp_2dnr_set(int iCameraIdex, char *oriTnf, int tnf)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_ISP2DnrSet(ViPipe, oriTnf, tnf));
    return 0;
}

int anj_mw_media_isp_3dnr_get(int iCameraIdex, char *snf)
{
    VI_PIPE ViPipe = iCameraIdex;
    ISP_USR_PREFERENCE_S stPref = {0};
    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    for (int i = 0; i < 16; i++)
    {
        snf[i] = (char)stPref.u32YuvDenoise;
    }
    return 0;
}

int anj_mw_media_isp_3dnr_set(int iCameraIdex, char *oriSnf, int snf)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_ISP3DnrSet(ViPipe, oriSnf, snf));
    return 0;
}

int anj_mw_media_vpss_crop_set(int iCameraIdex, unsigned short cropx, unsigned short cropy)
{
    TS_Common_VideoAttr_t *pstVideoAttr = (TS_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    TS_U32 u32Width = 0;
    TS_U32 u32Height = 0;
    TS_S32 s32X = 0;
    TS_S32 s32Y = 0;

    if (pstVideoAttr == NULL)
    {
        return -1;
    }

    /* GrpCrop 坐标系 = VI pipe，与 zoom / video_vpss_crop 一致 */
    u32Width = pstVideoAttr->ViAttr.astViInfo[iCameraIdex].stPipeInfo.width;
    u32Height = pstVideoAttr->ViAttr.astViInfo[iCameraIdex].stPipeInfo.height;

    if (cropx >= u32Width || cropy >= u32Height)
    {
        __ERR("vpss crop invalid cropx:%u cropy:%u base:%ux%u\n", cropx, cropy, u32Width, u32Height);
        return -1;
    }

    /* 对齐 mstar scl_crop_set：左右/上下各裁一半 */
    s32X = cropx / 2;
    s32Y = cropy / 2;
    u32Width = u32Width - cropx;
    u32Height = u32Height - cropy;

    STCHECKRESULT(TS_COMMON_VPSS_GrpCrop(iCameraIdex, s32X, s32Y, u32Width, u32Height));
    return 0;
}

int anj_mw_media_scl_crop_set(int iCameraIdex, unsigned short cropx, unsigned short cropy)
{
    return anj_mw_media_vpss_crop_set(iCameraIdex, cropx, cropy);
}

int anj_mw_media_isp_init()
{
    return 0;
}

int anj_mw_media_isp_uninit()
{
    pthread_mutex_lock(&s_stZoomCmdMutex);
    anj_mw_zoom_thread_stop();
    pthread_mutex_unlock(&s_stZoomCmdMutex);
    STCHECKRESULT(TS_Common_IspIqStop());
    return 0;
}

int anj_mw_media_isp_param_init(int iCameraIdex)
{
    (void)iCameraIdex;
    return 0;
}

int anj_mw_media_isp_load(int iCameraIdex, char *filepath, int *bStart)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspLoadIq(ViPipe, filepath));
    return 0;
}

int anj_mw_media_isp_fps_set(int iCameraIdex, int fps)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspFpsSet(ViPipe, fps));
    return 0;
}

int anj_mw_media_isp_wdr_set(int iCameraIdex, int enable)
{
    /* 物理 WDR 由 anj_mw_media_video_encode_apply（uninit+init）按 wdr_enable 重建，此处不热切 */
    (void)iCameraIdex;
    (void)enable;
    return 0;
}

int anj_mw_media_isp_aeinfo_get(int iCameraIdex, AnjIspAeInfo *pstAnjIspAeInfo)
{
    VI_PIPE ViPipe = iCameraIdex;
    ISP_AEC_S stAecAttr = {0};
    ISP_PUB_ATTR_S stPubAttr = {0};
    ISP_AE_STATISTICS_S stAeStat = {0};
    TS_U32 u32Vts;
    TS_U32 u32Fps;
    TS_U32 u32Lines;
    TS_U32 u32TotalGain;
    TS_U32 u32Luma = 0;
    int matched_fps = 0;
    int gear_cnt = 10;
    TS_U32 gain_tab[10];
    int fps_tab[10];
    int i;

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));
    TS_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr);
    TS_MPI_ISP_GetAEStatistics(ViPipe, &stAeStat);

    u32TotalGain = stAecAttr.stManualParam.u32TotalGain[0];
    if (u32TotalGain == 0)
    {
        u32TotalGain = 1024;
    }
    /* TotalGain: 1024=1x */
    pstAnjIspAeInfo->curGain = (float)u32TotalGain / 1024.00f;

    u32Vts = stAecAttr.stManualParam.u32Vts;
    u32Lines = stAecAttr.stManualParam.u32ExpLineCount[0];
    u32Fps = (stPubAttr.f32FrameRate > 0.5f) ? (TS_U32)(stPubAttr.f32FrameRate + 0.5f) : 0;

    for (i = 0; i < gear_cnt; i++)
    {
        gain_tab[i] = stAecAttr.stAutoParam.stAecExpKneePoint.tab[i].params.u32TotalGain;
        fps_tab[i] = (int)stAecAttr.stAutoParam.stAecExpKneePoint.tab[i].params.u32FrameRate;
    }
    matched_fps = fps_tab[gear_cnt - 1];
    if (u32TotalGain <= gain_tab[0])
    {
        matched_fps = fps_tab[0];
    }
    else
    {
        for (i = 0; i < gear_cnt - 1; i++)
        {
            TS_U32 g_curr = gain_tab[i];
            TS_U32 g_next = gain_tab[i + 1];

            if (u32TotalGain > g_curr && u32TotalGain <= g_next)
            {
                matched_fps = fps_tab[i + 1];
                break;
            }
        }
    }
    if (matched_fps > (int)u32Fps)
    {
        matched_fps = (int)u32Fps;
    }
    u32Fps = (TS_U32)matched_fps;
    if (u32Vts > 0 && u32Fps > 0)
    {
        /* exp_us ≈ lines * 1e6 / (Vts * fps) */
        pstAnjIspAeInfo->expShutter =
            (int)((TS_U64)u32Lines * 1000000ULL / ((TS_U64)u32Vts * u32Fps));
    }
    else
    {
        pstAnjIspAeInfo->expShutter = (int)u32Lines;
    }

    for (i = 0; i < BAYER_PATTERN_NUM; i++)
    {
        u32Luma += stAeStat.u16Au16BEGlobalAvg[i];
    }
    pstAnjIspAeInfo->lumy = (int)(u32Luma / BAYER_PATTERN_NUM);
    /*
     * QueryExposureInfo 已废弃；TS 无等价收敛查询接口。
     * 默认按已收敛处理，避免软光敏关灯被 ae_stable 卡住。
     */
    pstAnjIspAeInfo->bIsStable = 1;
    pstAnjIspAeInfo->bvTarget = 0;
    pstAnjIspAeInfo->lv = 0;
    {
        TS_U32 validSize = stAecAttr.stAutoParam.stAecLuxConvTab.validSize;
        TS_U32 idx = 0;
        TS_U32 i;

        if (validSize == 0 || validSize > 10)
        {
            validSize = 1;
        }
        for (i = 0; i < validSize; i++)
        {
            if (u32TotalGain >= stAecAttr.stAutoParam.stAecLuxConvTab.tab[i].region)
            {
                idx = i;
            }
        }
        pstAnjIspAeInfo->sceneTarget =
            (int)stAecAttr.stAutoParam.stAecLuxConvTab.tab[idx].params.u32AeTarget;
    }
    return 0;
}

int anj_mw_media_isp_sensitive_get(AnjIspSensitiveInfo *pstSensitive, const AnjIspAeInfo *pstAeInfo)
{
    if (pstSensitive == NULL || pstAeInfo == NULL)
    {
        return -1;
    }

    pstSensitive->type = ANJ_ISP_SENSITIVE_GAIN;
    pstSensitive->value = (int)pstAeInfo->curGain;
    pstSensitive->ae_stable = pstAeInfo->bIsStable;
    return 0;
}

int anj_mw_media_isp_is_darker(int value, int threshold)
{
    return (value >= threshold) ? 1 : 0;
}

int anj_mw_media_isp_is_brighter(int value, int threshold)
{
    return (value <= threshold) ? 1 : 0;
}

int anj_mw_media_isp_aetarget_get(int iCameraIdex, unsigned int *u32Y)
{
    VI_PIPE ViPipe = iCameraIdex;

    STCHECKRESULT(TS_Common_IspAeTargetYGet(ViPipe, u32Y));
    return 0;
}

int anj_mw_media_isp_aetarget_set(int iCameraIdex, unsigned int *u32Y)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_IspAeTargetYSet(ViPipe, u32Y));
    return 0;
}

int anj_mw_media_isp_weight_set(int iCameraIdex, int weight)
{
    VI_PIPE ViPipe = iCameraIdex;
    STCHECKRESULT(TS_Common_ISPWeightSet(ViPipe, weight));
    return 0;
}

int anj_mw_media_isp_zoom_stop(int iCameraIdex)
{
    (void)iCameraIdex;
    pthread_mutex_lock(&s_stZoomCmdMutex);
    anj_mw_zoom_trajectory_stop();
    pthread_mutex_unlock(&s_stZoomCmdMutex);
    return 0;
}

int anj_mw_media_isp_zoom_center_init(int iCameraIdex, double max_multiple, int speed)
{
    TS_U32 baseW = 0;
    TS_U32 baseH = 0;
    TS_U32 targetW = 0;
    TS_U32 targetH = 0;
    int width = 0;
    int i = 0;
    int bMatchTarget = 0;

    if (max_multiple < 1.0)
    {
        max_multiple = 1.0;
    }
    if (speed <= 0)
    {
        speed = 102;
    }

    anj_mw_zoom_base_size_get(iCameraIdex, &baseW, &baseH, NULL);
    targetW = (TS_U32)round((double)baseW / max_multiple);
    targetH = (TS_U32)round((double)baseH / max_multiple);
    targetW = ANJ_ALIGN_DOWN(targetW, ZOOM_ALIGN);
    targetH = ANJ_ALIGN_DOWN(targetH, ZOOM_ALIGN);
    if (targetW < ZOOM_ALIGN)
    {
        targetW = ZOOM_ALIGN;
    }
    if (targetH < ZOOM_ALIGN)
    {
        targetH = ZOOM_ALIGN;
    }

    pthread_mutex_lock(&s_stZoomCmdMutex);
    anj_mw_zoom_trajectory_stop();
    pthread_mutex_lock(&s_stZoomMutex);
    memset(s_zoomEntry, 0, sizeof(s_zoomEntry));
    width = (int)baseW;
    anj_mw_zoom_entry_set_wh_center(&s_zoomEntry[0], baseW, baseH, baseW, baseH);

    for (i = 1; i < ISP_ZOOM_ENTRY_CNT; i++)
    {
        TS_U32 entryW = 0;
        TS_U32 entryH = 0;

        width = (width * 100) / speed;
        entryW = (TS_U32)ANJ_ALIGN_DOWN(width, ZOOM_ALIGN);
        if (0 == bMatchTarget && entryW <= targetW)
        {
            entryW = targetW;
            bMatchTarget = 1;
        }
        if (entryW < targetW)
        {
            break;
        }
        entryH = ANJ_ALIGN_DOWN(entryW * targetH / targetW, ZOOM_ALIGN);
        anj_mw_zoom_entry_set_wh_center(&s_zoomEntry[i], baseW, baseH, entryW, entryH);
    }
    if (i >= ISP_ZOOM_ENTRY_CNT)
    {
        pthread_mutex_unlock(&s_stZoomMutex);
        pthread_mutex_unlock(&s_stZoomCmdMutex);
        __ERR("how can speed %d too slow i %d\n", speed, i);
        return -1;
    }

    s_zoomNum = i;
    s_from = 0;
    s_to = 0;
    s_cur = 0;
    s_next = 0;
    s_running = 0;
    s_zoomGeneration++;
    pthread_mutex_unlock(&s_stZoomMutex);

    __INFO("zoom base %ux%u origin %u,%u max:%f target:%ux%u num:%d\n",
           baseW, baseH, s_zoomOriginX, s_zoomOriginY, max_multiple, targetW, targetH, s_zoomNum);
    for (i = 0; i < s_zoomNum; i++)
    {
        __DBG("i:%d zoomEntry xywh:%d %d %d %d\n", i,
              s_zoomEntry[i].x, s_zoomEntry[i].y, s_zoomEntry[i].w, s_zoomEntry[i].h);
    }
    pthread_mutex_unlock(&s_stZoomCmdMutex);
    return 0;
}

/*
 * 电子变倍“平移+变焦”轨迹初始化（move table）。
 *
 * 与 zoom_center_init / zoom_set（仅中心缩放）不同：本函数同时支持裁剪框
 * 位置(x/y)与倍率(w/h)变化，用于 AI 跟踪跟框、打断后的手动 move 变倍等场景。
 *
 * 流程概要：
 *  1) 停掉当前 zoom 工作线程，拿到 VI pipe 基准分辨率；
 *  2) 确定起点裁剪框（优先用上次真正落到 VPSS 的 rect，否则用 pCurArea 换算）；
 *  3) 按 pTargetArea 换算终点裁剪框，并做边界 clamp；
 *  4) 按“平移距离 + 宽度变化”估算步数，用 S 曲线插值生成 s_zoomEntry[]；
 *  5) 启动 zoom 线程从 entry[0] 跑到 entry[N-1]，逐帧提交 crop。
 *
 * @param iCameraIdex   摄像头通道号
 * @param max_multiple  允许的最大变倍倍率（上限钳位）
 * @param pCurArea      上层报告的当前 zoom 区域（归一化坐标/倍率）
 * @param pTargetArea   目标 zoom 区域（归一化坐标/倍率）
 * @return 0 成功或无需运动；负值失败
 *
 * DOUBLE_AREA_ENTRY 字段含义（归一化）：
 *  - xPos/yPos : 裁剪框左上角相对原图的比例 [0,1]
 *  - width/height : 放大倍数（裁剪宽=baseW/width，裁剪高=baseH/height）
 */
int anj_mw_media_isp_zoom_move_init(int iCameraIdex, double max_multiple, DOUBLE_AREA_ENTRY *pCurArea, DOUBLE_AREA_ENTRY *pTargetArea)
{
    TS_U32 baseW = 0;                 /* VI pipe 基准宽度（像素） */
    TS_U32 baseH = 0;                 /* VI pipe 基准高度（像素） */
    float run_multiple = 0.0f;        /* 目标变倍倍率，钳位到 [1, max_multiple] */
    AnjTsZoomEntry stCurRect = {0};   /* 起点裁剪框：像素坐标 x/y/w/h */
    AnjTsZoomEntry stTargetRect = {0};/* 终点裁剪框：像素坐标 x/y/w/h */
    float move_distance = 0.0f;       /* 起点到终点左上角的欧氏距离（像素） */
    float width_change = 0.0f;        /* 裁剪宽度变化量 |dw|（像素） */
    float path_len = 0.0f;            /* 等效路径长度 = 平移距离 + 宽度变化，用于估步数 */
    int stepCount = 0;                /* 规划的插值步数（含终点） */
    int x_diff = 0;                   /* 终点相对起点的 x 增量 */
    int y_diff = 0;                   /* 终点相对起点的 y 增量 */
    int width_diff = 0;               /* 终点相对起点的宽度增量（高度按宽高比推导） */
    int entryCount = 1;               /* 轨迹表实际有效条目数（去重后），至少含起点 */
    int i = 0;
    int haveAppliedRect = 0;          /* 1: 已从 worker 取到上次成功应用的 crop */

    if (pCurArea == NULL || pTargetArea == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&s_stZoomCmdMutex);
    /* 先冻结 worker，再采样其最后一次成功提交的 crop，避免边跑边改表 */
    anj_mw_zoom_trajectory_stop();
    anj_mw_zoom_base_size_get(iCameraIdex, &baseW, &baseH, NULL);
    /* 目标倍率取 width/height 较大者，并限制在合法区间 */
    run_multiple = fmax(pTargetArea->width, pTargetArea->height);
    run_multiple = fmax(1.0, fmin(max_multiple, run_multiple));

    /* 软件 zoom worker 持有权威的“上次真正落地”的裁剪状态，优先作起点 */
    pthread_mutex_lock(&s_stZoomMutex);
    if (s_cur >= 0 && s_cur < s_zoomNum && s_zoomEntry[s_cur].w != 0)
    {
        stCurRect = s_zoomEntry[s_cur];
        haveAppliedRect = 1;
    }
    pthread_mutex_unlock(&s_stZoomMutex);

    /* 尚无已应用裁剪时，用上层归一化区域换算成像素裁剪框 */
    if (!haveAppliedRect)
    {
        stCurRect.w = (TS_U16)ANJ_ALIGN_DOWN((TS_U32)round((double)baseW / pCurArea->width), ZOOM_ALIGN);
        stCurRect.x = (TS_U16)(baseW * pCurArea->xPos);
        stCurRect.y = (TS_U16)(baseH * pCurArea->yPos);
        stCurRect.h = (TS_U16)(stCurRect.w * baseH / baseW);
        stCurRect.w = (TS_U16)ANJ_ALIGN_DOWN(stCurRect.w, ZOOM_ALIGN);
        stCurRect.h = (TS_U16)ANJ_ALIGN_DOWN(stCurRect.h, ZOOM_ALIGN);
    }

    /* 目标区域：倍率 -> 裁剪宽高；xPos/yPos -> 像素左上角 */
    stTargetRect.w = (TS_U16)ANJ_ALIGN_DOWN((TS_U32)round((double)baseW / run_multiple), ZOOM_ALIGN);
    stTargetRect.x = (TS_U16)(baseW * pTargetArea->xPos);
    stTargetRect.y = (TS_U16)(baseH * pTargetArea->yPos);
    stTargetRect.h = (TS_U16)(stTargetRect.w * baseH / baseW);
    stTargetRect.w = (TS_U16)ANJ_ALIGN_DOWN(stTargetRect.w, ZOOM_ALIGN);
    stTargetRect.h = (TS_U16)ANJ_ALIGN_DOWN(stTargetRect.h, ZOOM_ALIGN);

    /* 防止目标框越出右/下边界 */
    if ((TS_U32)stTargetRect.x + (TS_U32)stTargetRect.w > baseW)
    {
        stTargetRect.x = (TS_U16)(baseW - stTargetRect.w);
    }
    if ((TS_U32)stTargetRect.y + (TS_U32)stTargetRect.h > baseH)
    {
        stTargetRect.y = (TS_U16)(baseH - stTargetRect.h);
    }
    anj_mw_zoom_entry_clamp(&stCurRect, baseW, baseH);
    anj_mw_zoom_entry_clamp(&stTargetRect, baseW, baseH);

    /* clamp 后起终点完全一致，无需重建轨迹 */
    if (stCurRect.x == stTargetRect.x &&
        stCurRect.y == stTargetRect.y &&
        stCurRect.w == stTargetRect.w &&
        stCurRect.h == stTargetRect.h)
    {
        __INFO("zoom move skip: cur rect equals target rect after clamp\n");
        pthread_mutex_unlock(&s_stZoomCmdMutex);
        return 0;
    }

    /* 路径越长步数越多：约每 ZOOM_TRACK_PX_PER_STEP 像素一步，再钳到 min/max/fast_gain */
    move_distance = sqrtf(powf((float)stTargetRect.x - (float)stCurRect.x, 2) +
                          powf((float)stTargetRect.y - (float)stCurRect.y, 2));
    width_change = fabsf((float)stTargetRect.w - (float)stCurRect.w);
    path_len = move_distance + width_change;
    stepCount = (int)(path_len / (float)ZOOM_TRACK_PX_PER_STEP + 0.5f);

    if (stepCount < ZOOM_TRACK_MIN_STEP)
    {
        stepCount = ZOOM_TRACK_MIN_STEP;
    }
    if (stepCount > ZOOM_TRACK_MAX_STEP)
    {
        stepCount = ZOOM_TRACK_MAX_STEP;
    }
    if (stepCount > s_iZoomTrackFastGain)
    {
        stepCount = s_iZoomTrackFastGain;
    }
    if (stepCount <= 0 || stepCount >= ISP_ZOOM_ENTRY_CNT)
    {
        __ERR("stepCount:%d invalid !\n", stepCount);
        pthread_mutex_unlock(&s_stZoomCmdMutex);
        return -1;
    }

    x_diff = (int)stTargetRect.x - (int)stCurRect.x;
    y_diff = (int)stTargetRect.y - (int)stCurRect.y;
    width_diff = (int)stTargetRect.w - (int)stCurRect.w;

    /* 重建轨迹表：entry[0]=起点，后续按 S 曲线插值，最后一步强制贴齐终点 */
    pthread_mutex_lock(&s_stZoomMutex);
    memset(s_zoomEntry, 0, sizeof(s_zoomEntry));
    s_zoomEntry[0].x = (TS_U16)ANJ_ALIGN_DOWN(stCurRect.x, ZOOM_ALIGN);
    s_zoomEntry[0].y = (TS_U16)ANJ_ALIGN_DOWN(stCurRect.y, ZOOM_ALIGN);
    s_zoomEntry[0].w = (TS_U16)ANJ_ALIGN_DOWN(stCurRect.w, ZOOM_ALIGN);
    s_zoomEntry[0].h = (TS_U16)ANJ_ALIGN_DOWN(stCurRect.h, ZOOM_ALIGN);

    for (i = 1; i <= stepCount; i++)
    {
        float t = (float)i / (float)stepCount;                 /* 线性进度 [0,1] */
        float progress = anj_mw_zoom_s_curve_progress(t);      /* S 曲线进度，两端慢中间快 */
        AnjTsZoomEntry nextRect = {0};                         /* 本步插值得到的裁剪框 */

        if (i == stepCount)
        {
            /* 最后一步直接写终点，避免浮点累积误差 */
            nextRect.x = (TS_U16)ANJ_ALIGN_DOWN(stTargetRect.x, ZOOM_ALIGN);
            nextRect.y = (TS_U16)ANJ_ALIGN_DOWN(stTargetRect.y, ZOOM_ALIGN);
            nextRect.w = (TS_U16)ANJ_ALIGN_DOWN(stTargetRect.w, ZOOM_ALIGN);
            nextRect.h = (TS_U16)ANJ_ALIGN_DOWN(stTargetRect.h, ZOOM_ALIGN);
        }
        else
        {
            int x = (int)stCurRect.x + (int)(x_diff * progress);
            int y = (int)stCurRect.y + (int)(y_diff * progress);
            int width = (int)stCurRect.w + (int)(width_diff * progress);
            int height = width * (int)baseH / (int)baseW;      /* 保持原图宽高比 */

            nextRect.x = (TS_U16)ANJ_ALIGN_DOWN(x, ZOOM_ALIGN);
            nextRect.y = (TS_U16)ANJ_ALIGN_DOWN(y, ZOOM_ALIGN);
            nextRect.w = (TS_U16)ANJ_ALIGN_DOWN(width, ZOOM_ALIGN);
            nextRect.h = (TS_U16)ANJ_ALIGN_DOWN(height, ZOOM_ALIGN);
        }

        /* 对齐后与上一条完全相同则跳过，避免无效重复帧 */
        if (s_zoomEntry[entryCount - 1].x == nextRect.x &&
            s_zoomEntry[entryCount - 1].y == nextRect.y &&
            s_zoomEntry[entryCount - 1].w == nextRect.w &&
            s_zoomEntry[entryCount - 1].h == nextRect.h)
        {
            continue;
        }

        s_zoomEntry[entryCount] = nextRect;
        entryCount++;
        if (entryCount >= ISP_ZOOM_ENTRY_CNT)
        {
            break;
        }
    }

    __DBG("zoom move stepCount:%d actual:%d path:%f xy:%f dw:%f\n",
           stepCount, entryCount - 1, path_len, move_distance, width_change);
    if (entryCount <= 1)
    {
        /* 去重后只剩起点，没有可执行运动 */
        __INFO("zoom move skip: no effective rect after align\n");
        pthread_mutex_unlock(&s_stZoomMutex);
        pthread_mutex_unlock(&s_stZoomCmdMutex);
        return 0;
    }

    s_zoomNum = entryCount;
    for (i = 0; i < s_zoomNum; i++)
    {
        __DBG("i:%d zoomEntry xywh:%d %d %d %d\n", i,
              s_zoomEntry[i].x, s_zoomEntry[i].y, s_zoomEntry[i].w, s_zoomEntry[i].h);
    }

    pthread_mutex_unlock(&s_stZoomMutex);
    /* 从 entry[0] 播放到最后一条，由 worker 按帧提交 VPSS crop */
    i = anj_mw_zoom_thread_start(iCameraIdex, 0, s_zoomNum - 1);
    pthread_mutex_unlock(&s_stZoomCmdMutex);
    return i;
}

int anj_mw_media_isp_zoom_set(int iCameraIdex, double cur_multiple, double run_multiple)
{
    TS_U32 baseW = 0;
    TS_U32 baseH = 0;
    TS_U32 curW = 0;
    TS_U32 runW = 0;
    int curIndex = -1;
    int runIndex = -1;
    int i = 0;

    pthread_mutex_lock(&s_stZoomCmdMutex);
    anj_mw_zoom_trajectory_stop();
    if (s_zoomNum <= 0)
    {
        __ERR("zoom table empty, call center_init first\n");
        pthread_mutex_unlock(&s_stZoomCmdMutex);
        return -1;
    }

    if (cur_multiple < 1.0)
    {
        cur_multiple = 1.0;
    }
    if (run_multiple < 1.0)
    {
        run_multiple = 1.0;
    }

    anj_mw_zoom_base_size_get(iCameraIdex, &baseW, &baseH, NULL);
    curW = (TS_U32)round((double)baseW / cur_multiple);
    runW = (TS_U32)round((double)baseW / run_multiple);

    pthread_mutex_lock(&s_stZoomMutex);
    if (s_cur >= 0 && s_cur < s_zoomNum)
    {
        curIndex = s_cur;
    }
    pthread_mutex_unlock(&s_stZoomMutex);

    for (i = 0; i < s_zoomNum; i++)
    {
        if (curIndex == -1 && s_zoomEntry[i].w <= curW)
        {
            curIndex = i;
        }
        if (runIndex == -1 && s_zoomEntry[i].w <= runW)
        {
            runIndex = i;
        }
        if (runIndex > 0 && curIndex > 0)
        {
            break;
        }
    }

    if (curIndex < 0)
    {
        curIndex = s_zoomNum - 1;
    }
    if (runIndex < 0)
    {
        runIndex = s_zoomNum - 1;
    }

    __INFO("cur_multiple:%f run_multiple:%f\n", cur_multiple, run_multiple);
    __INFO("curIndex:%d runIndex:%d\n", curIndex, runIndex);

    i = anj_mw_zoom_thread_start(iCameraIdex, curIndex, runIndex);
    pthread_mutex_unlock(&s_stZoomCmdMutex);
    return i;
}

int anj_mw_media_isp_zoom_get(int iCameraIdex, DOUBLE_AREA_ENTRY *cur_area, double *pRunPercent)
{
    TS_U32 baseW = 0;
    TS_U32 baseH = 0;
    int running = 0;
    int cur = 0;
    int from = 0;
    int to = 0;
    AnjTsZoomEntry entry = {0};

    if (cur_area == NULL)
    {
        return -1;
    }

    anj_mw_zoom_base_size_get(iCameraIdex, &baseW, &baseH, NULL);

    pthread_mutex_lock(&s_stZoomMutex);
    running = s_running;
    cur = s_cur;
    from = s_from;
    to = s_to;
    if (cur >= 0 && cur < s_zoomNum)
    {
        entry = s_zoomEntry[cur];
    }
    else
    {
        pthread_mutex_unlock(&s_stZoomMutex);
        return -1;
    }
    pthread_mutex_unlock(&s_stZoomMutex);

    if (entry.w == 0 || baseW == 0)
    {
        return -1;
    }

    cur_area->xPos = (double)entry.x / (double)baseW;
    cur_area->yPos = (double)entry.y / (double)baseH;
    cur_area->width = (double)baseW / (double)entry.w;
    cur_area->height = cur_area->width;
    if (pRunPercent)
    {
        if (running && abs(to - from) > 0)
        {
            *pRunPercent = (double)abs(cur - from) / (double)abs(to - from);
        }
        else
        {
            *pRunPercent = 1.0;
        }
    }
    return 0;
}

void anj_mw_media_isp_zoom_track_init(void)
{
    anj_mw_zoom_track_fast_gain_load();
    __INFO("zoom track max step:%d (file:%s)\n", s_iZoomTrackFastGain, ZOOM_TRACK_FAST_GAIN_DEBUG_FILE);
}

int anj_mw_media_isp_ai_start(int iCameraIdx, int bEnable)
{
    VI_PIPE ViPipe = iCameraIdx;
    VI_AIISP_ATTR_S stAttr = {0};
    TS_S32 s32Ret;
    int model_id;
    int model_type;

    s32Ret = TS_MPI_VI_AIISP_GetAttr(ViPipe, &stAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __WARN("TS_MPI_VI_AIISP_GetAttr %#x\n", (unsigned int)s32Ret);
        return -1;
    }

    /* 开=RFR；关=AIMVD（与 VI 启动默认一致，非 NONE） */
    if (bEnable)
    {
        model_type = VI_AIISP_MODEL_TYPE_RFR;
        model_id = 2;
    }
    else
    {
        model_type = VI_AIISP_MODEL_TYPE_AIMVD;
        model_id = 129;
    }
    stAttr.bModelAutoSwitch = TS_FALSE;
    s32Ret = TS_MPI_VI_AIISP_SetAttr(ViPipe, &stAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __WARN("TS_MPI_VI_AIISP_SetAttr %#x\n", (unsigned int)s32Ret);
        return -1;
    }

    s32Ret = TS_MPI_VI_AIISP_SetManualModel(ViPipe, model_type, model_id);
    if (s32Ret != TS_SUCCESS)
    {
        __WARN("TS_MPI_VI_AIISP_SetManualModel %#x\n", (unsigned int)s32Ret);
        return -1;
    }
    if (model_type == VI_AIISP_MODEL_TYPE_RFR)
    {
        sleep(1);
        STCHECKRESULT(TS_Common_IspFpsSet(ViPipe, AIISP_MAX_FPS));
    }
    else
    {
        sleep(1);
        STCHECKRESULT(TS_Common_IspFpsSet(ViPipe, DEFAULT_SENSOR_FPS));
    }
    __INFO("aiisp enable=%d model_type=%d, model_id=%d\n", bEnable, model_type, model_id);
    return 0;
}

/* 同向累计、反向清零。冷却期内只计数；满 MAX 返回目标态，否则返回当前 enabled。 */
static int anj_mw_media_isp_ai_switch_cnt_update(int want, int enabled, int *hold_ticks, int *switch_cnt)
{
    if (want == enabled)
    {
        *switch_cnt = 0;
    }
    else if (want)
    {
        if (*switch_cnt < 0)
        {
            *switch_cnt = 0;
        }
        (*switch_cnt)++;
    }
    else
    {
        if (*switch_cnt > 0)
        {
            *switch_cnt = 0;
        }
        (*switch_cnt)--;
    }

    if (*hold_ticks > 0)
    {
        return enabled;
    }
    if (*switch_cnt < AIISP_SWITCH_MAX_CNT && *switch_cnt > -AIISP_SWITCH_MAX_CNT)
    {
        return enabled;
    }

    *switch_cnt = 0;
    return want;
}

int anj_mw_media_isp_ai_update(int iCameraIdx, float curGain, int bv, int white_pwm,
                               int *enabled, int *hold_ticks, int *switch_cnt, int is_mono)
{
    int want;
    int next;

    (void)bv;
    (void)white_pwm;
    if (enabled == NULL || hold_ticks == NULL || switch_cnt == NULL)
    {
        return -1;
    }

    /* 红外黑白：保持 AIMVD。已在 RFR 则本调用立刻退出，不等防抖。 */
    if (is_mono)
    {
        want = 0;
        if (*enabled)
        {
            *switch_cnt = -AIISP_SWITCH_MAX_CNT;
            *hold_ticks = 0;
        }
    }
    else if (curGain > AIISP_ENTER_GAIN_TH)
    {
        want = 1; // enable aiisp RFR
    }
    else if (curGain < AIISP_EXIT_GAIN_TH)
    {
        want = 0; // disable aiisp AIMVD
    }
    else
    {
        want = *enabled; // keep current state
    }

    next = anj_mw_media_isp_ai_switch_cnt_update(want, *enabled, hold_ticks, switch_cnt);
    if (next == *enabled)
    {
        return 0;
    }
    if (anj_mw_media_isp_ai_start(iCameraIdx, next) != 0)
    {
        return -1;
    }
    *enabled = next;
    return 0;
}
