#include "anj_mw_media_common.h"
#include "anj_mw_media_video.h"
#include "anj_mw_media_isp.h"
#include "anj_mw_file.h"

#include <math.h>

#define ZOOM_TARCK_FAST_GAIN 12
#define ZOOM_TRACK_FAST_GAIN_DEBUG_FILE "/tmp/fast_gain"
#define ZOOM_TRACK_FAST_GAIN_MIN 8
#define ZOOM_TRACK_PX_PER_STEP 100 /* 平移/缩放共用：路程约每 100 像素一步 */
#define ZOOM_TRACK_MIN_STEP 8
#define ZOOM_TRACK_MAX_STEP 12

static int zoomNum = 0;
static int s_iZoomTrackFastGain = ZOOM_TARCK_FAST_GAIN;
static MI_ISP_ZoomEntry_t zoomEntry[ISP_ZOOM_ENTRY_CNT] = {0};

/* 平移/缩放共用 S 曲线，端点严格为 0 和 1 */
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

int anj_mw_media_isp_brightness_set(int iCameraIdex, int brightness)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspBrightnessSet(pIspAttr->IspDevId, pIspAttr->IspChnId, brightness));
    return 0;
}

int anj_mw_media_isp_contrast_set(int iCameraIdex, int contrast)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspContrastSet(pIspAttr->IspDevId, pIspAttr->IspChnId, contrast));
    return 0;
}

int anj_mw_media_isp_saturation_get(int iCameraIdex, char *saturation)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;
    MI_ISP_IQ_SaturationType_t data = {0};
    STCHECKRESULT(ST_Common_IspSaturationGet(pIspAttr->IspDevId, pIspAttr->IspChnId, &data));
    for (int i = 0; i < 16; i++)
    {
        saturation[i] = data.stAuto.stParaAPI[i].u8SatAllStr;
    }
    return 0;
}

int anj_mw_media_isp_saturation_set(int iCameraIdex, char *oriSaturation, int saturation)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspSaturationSet(pIspAttr->IspDevId, pIspAttr->IspChnId, oriSaturation, saturation));
    return 0;
}

int anj_mw_media_isp_sharpness_get(int iCameraIdex, char *sharpness0, char *sharpness1, char *sharpness2)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;
    MI_ISP_IQ_SharpnessType_t data = {0};
    STCHECKRESULT(ST_Common_IspSharpnessGet(pIspAttr->IspDevId, pIspAttr->IspChnId, &data));
    for (int i = 0; i < 16; i++)
    {
        sharpness0[i] = data.stAuto.stParaAPI[i].u8SharpnessUD[0];
        sharpness1[i] = data.stAuto.stParaAPI[i].u8SharpnessUD[1];
        sharpness2[i] = data.stAuto.stParaAPI[i].u8SharpnessUD[2];
    }

    return 0;
}

int anj_mw_media_isp_sharpness_set(int iCameraIdex, char *sharpness0, char *sharpness1, char *sharpness2, int sharpness)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspSharpnessSet(pIspAttr->IspDevId, pIspAttr->IspChnId, sharpness0, sharpness1, sharpness2, sharpness));
    return 0;
}

int anj_mw_media_isp_backlight_get(int iCameraIdex, char *backlight)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspBLCGet(pIspAttr->IspDevId, pIspAttr->IspChnId, backlight));
    return 0;
}

int anj_mw_media_isp_backlight_set(int iCameraIdex, char *oriBacklight, int backlight)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspBLCSet(pIspAttr->IspDevId, pIspAttr->IspChnId, oriBacklight, backlight));
    return 0;
}

int anj_mw_media_isp_filcker_set(int iCameraIdex, int hz)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspFilckerSet(pIspAttr->IspDevId, pIspAttr->IspChnId, hz));
    return 0;
}

int anj_mw_media_isp_shutterus_set(int iCameraIdex, int minShutter, int maxShutter)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspShutterSet(pIspAttr->IspDevId, pIspAttr->IspChnId, minShutter, maxShutter));
    return 0;
}

int anj_mw_media_isp_gain_set(int iCameraIdex, int maxGain)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspMaxGainSet(pIspAttr->IspDevId, pIspAttr->IspChnId, maxGain));
    return 0;
}

int anj_mw_media_isp_rotate_set(int iCameraIdex, int rotate)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspRotateSet(pIspAttr->IspDevId, pIspAttr->IspChnId, rotate));
    return 0;
}

int anj_mw_media_isp_flip_set(int iCameraIdex, int hflip, int vflip)
{
    MI_SNR_PADID eSnrPadId = E_MI_VIF_SNRPAD_ID_0;
    if (iCameraIdex == 0)
    {
        eSnrPadId = E_MI_VIF_SNRPAD_ID_0;
    }
    else
    {
        eSnrPadId = E_MI_VIF_SNRPAD_ID_2;
    }
    STCHECKRESULT(ST_Common_SensorMirrorSet(eSnrPadId, hflip, vflip));
    return 0;
}

int anj_mw_media_isp_awb_set(int iCameraIdex, int whitebalance)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspAWBSet(pIspAttr->IspDevId, pIspAttr->IspChnId, whitebalance));
    return 0;
}

int anj_mw_media_isp_wdr_value_get(int iCameraIdex, char *wdrvalue)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspWdrValueGet(pIspAttr->IspDevId, pIspAttr->IspChnId, wdrvalue));
    return 0;
}

int anj_mw_media_isp_wdr_value_set(int iCameraIdex, char *oriWdrValue, int wdrvalue)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspWdrValueSet(pIspAttr->IspDevId, pIspAttr->IspChnId, oriWdrValue, wdrvalue));
    return 0;
}

int anj_mw_media_isp_hlc_set(int iCameraIdex, int hlc, int brightness)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspHLCSet(pIspAttr->IspDevId, pIspAttr->IspChnId, hlc, brightness));
    return 0;
}

int anj_mw_media_isp_2dnr_get(int iCameraIdex, char *tnf)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_ISP2DnrGet(pIspAttr->IspDevId, pIspAttr->IspChnId, tnf));
    return 0;
}

int anj_mw_media_isp_2dnr_set(int iCameraIdex, char *oriTnf, int tnf)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_ISP2DnrSet(pIspAttr->IspDevId, pIspAttr->IspChnId, oriTnf, tnf));
    return 0;
}

int anj_mw_media_isp_3dnr_get(int iCameraIdex, char *snf)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_ISP3DnrGet(pIspAttr->IspDevId, pIspAttr->IspChnId, snf));
    return 0;
}

int anj_mw_media_isp_3dnr_set(int iCameraIdex, char *oriSnf, int snf)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_ISP3DnrSet(pIspAttr->IspDevId, pIspAttr->IspChnId, oriSnf, snf));
    return 0;
}

int anj_mw_media_scl_crop_set(int iCameraIdex, unsigned short cropx, unsigned short cropy)
{
    int iRet = 0;
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_SclAttr_t *pSclAttr = &pstVideoAttr[iCameraIdex].SclAttr[0];
    if (pSclAttr)
    {
        MI_SYS_WindowRect_t stOutCropInfo = {0};
        stOutCropInfo.u16X = cropx / 2;
        stOutCropInfo.u16Y = cropy / 2;
        stOutCropInfo.u16Width = pSclAttr->stSCLOutputSize.u16Width - cropx;
        stOutCropInfo.u16Height = pSclAttr->stSCLOutputSize.u16Height - cropy;
        for (int i = 0; i < MAX_SCL_PORT - 1; i++)
        {
            pSclAttr = &pstVideoAttr[iCameraIdex].SclAttr[i];
            iRet |= ST_Common_SclCropSet(pSclAttr, &stOutCropInfo);
        }
    }
    else
    {
        iRet = -1;
    }

    return iRet;
}

int anj_mw_media_isp_init()
{
    STCHECKRESULT(ST_Common_IspIqStart());
    return 0;
}

int anj_mw_media_isp_uninit()
{
    STCHECKRESULT(ST_Common_IspIqStop());
    return 0;
}

int anj_mw_media_isp_param_init(int iCameraIdex)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;
    STCHECKRESULT(ST_Common_IspParaInit(pIspAttr->IspChnId));
    return 0;
}

int anj_mw_media_isp_load(int iCameraIdex, char *filepath, int *bStart)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;
    while (bStart && *bStart)
    {
        if (0 == anj_mw_media_isp_param_init(iCameraIdex))
        {
            break;
        }
        usleep(500 * 1000);
    }
    if (!bStart || !*bStart)
    {
        return -1;
    }
    STCHECKRESULT(ST_Common_IspLoadIq(pIspAttr->IspChnId, filepath));
    return 0;
}

int anj_mw_media_isp_fps_set(int iCameraIdex, int fps)
{
    MI_SNR_PADID eSnrPadId = E_MI_VIF_SNRPAD_ID_0;
    if (iCameraIdex == 0)
    {
        eSnrPadId = E_MI_VIF_SNRPAD_ID_0;
    }
    else
    {
        eSnrPadId = E_MI_VIF_SNRPAD_ID_2;
    }

    int curFps = 0;
    STCHECKRESULT(ST_Common_SensorFpsGet(eSnrPadId, &curFps));
    if (curFps != fps)
    {
        STCHECKRESULT(ST_Common_SensorFpsSet(eSnrPadId, fps));
    }
    return 0;
}

int anj_mw_media_isp_wdr_set(int iCameraIdex, int enable)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;
    STCHECKRESULT(ST_Common_IspWdrEnableSet(pIspAttr->IspDevId, pIspAttr->IspChnId, enable));
    return 0;
}

int anj_mw_media_isp_aeinfo_get(int iCameraIdex, AnjIspAeInfo *pstAnjIspAeInfo)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;
    ST_Common_IspAeInfoGet(pIspAttr->IspDevId, pIspAttr->IspChnId, &pIspAttr->IspAeInfo);
    MI_U32 u32SensorGain = pIspAttr->IspAeInfo.stExpoValueLong.u32SensorGain;
    MI_U32 u32ISPGain = pIspAttr->IspAeInfo.stExpoValueLong.u32ISPGain;
    pstAnjIspAeInfo->curGain = (float)u32SensorGain / 1024.00 * (float)u32ISPGain / 1024;
    pstAnjIspAeInfo->bvTarget = pIspAttr->IspAeInfo.s32BV;
    pstAnjIspAeInfo->expShutter = pIspAttr->IspAeInfo.stExpoValueLong.u32US;
    pstAnjIspAeInfo->lumy = pIspAttr->IspAeInfo.stHistWeightY.u32LumY;
    pstAnjIspAeInfo->lv = pIspAttr->IspAeInfo.u32LVx10;
    pstAnjIspAeInfo->bIsStable = pIspAttr->IspAeInfo.bIsStable;
    pstAnjIspAeInfo->sceneTarget = pIspAttr->IspAeInfo.u32SceneTarget;
    return 0;
}

int anj_mw_media_isp_sensitive_get(AnjIspSensitiveInfo *pstSensitive, const AnjIspAeInfo *pstAeInfo)
{
    if (pstSensitive == NULL || pstAeInfo == NULL)
    {
        return -1;
    }

    pstSensitive->type = ANJ_ISP_SENSITIVE_BV;
    pstSensitive->value = pstAeInfo->bvTarget;
    pstSensitive->ae_stable = pstAeInfo->bIsStable;
    return 0;
}

int anj_mw_media_isp_is_darker(int value, int threshold)
{
    return (value < threshold) ? 1 : 0;
}

int anj_mw_media_isp_is_brighter(int value, int threshold)
{
    return (value > threshold) ? 1 : 0;
}

int anj_mw_media_isp_aetarget_get(int iCameraIdex, unsigned int *u32Y)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspAeTargetYGet(pIspAttr->IspDevId, pIspAttr->IspChnId, u32Y));
    return 0;
}

int anj_mw_media_isp_aetarget_set(int iCameraIdex, unsigned int *u32Y)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_IspAeTargetYSet(pIspAttr->IspDevId, pIspAttr->IspChnId, u32Y));
    return 0;
}

int anj_mw_media_isp_weight_set(int iCameraIdex, int weight)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    STCHECKRESULT(ST_Common_ISPWeightSet(pIspAttr->IspDevId, pIspAttr->IspChnId, weight));
    return 0;
}

int anj_mw_media_isp_zoom_stop(int iCameraIdex)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;
    ST_Common_IspZoomStop(pIspAttr->IspDevId, pIspAttr->IspChnId);
    return 0;
}

int anj_mw_media_isp_zoom_center_init(int iCameraIdex, double max_multiple, int speed)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    MI_SNR_PlaneInfo_t stSnrPlane0Info;
    memset(&stSnrPlane0Info, 0x0, sizeof(MI_SNR_PlaneInfo_t));
    STCHECKRESULT(MI_SNR_GetPlaneInfo(pstVideoAttr[iCameraIdex].SnrPadId, 0, &stSnrPlane0Info));
    MI_SYS_PixelFormat_e eSensorPixFormat =
        (MI_SYS_PixelFormat_e)RGB_BAYER_PIXEL(stSnrPlane0Info.ePixPrecision, stSnrPlane0Info.eBayerId);
    ST_Common_SclAttr_t *pSclAttr = &pstVideoAttr[iCameraIdex].SclAttr[0];
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    MI_SYS_WindowRect_t stTargetRect = {0};

    ANJ_SIZE_S tmpSize;
    tmpSize.u32Width = round(pSclAttr->stSCLOutputSize.u16Width / max_multiple);
    tmpSize.u32Height = round(pSclAttr->stSCLOutputSize.u16Height / max_multiple);
    stTargetRect.u16X = (pSclAttr->stSCLOutputSize.u16Width - tmpSize.u32Width) / 2;
    stTargetRect.u16Y = (pSclAttr->stSCLOutputSize.u16Height - tmpSize.u32Height) / 2;
    stTargetRect.u16Width = tmpSize.u32Width;
    stTargetRect.u16Height = tmpSize.u32Height;

    int i = 0;
    int bMatchTarget = 0;
    int width = pSclAttr->stSCLOutputSize.u16Width;
    zoomEntry[0].u8ZoomSensorId = 0;
    zoomEntry[0].stCropWin.u16Width = pSclAttr->stSCLOutputSize.u16Width;
    zoomEntry[0].stCropWin.u16Height = pSclAttr->stSCLOutputSize.u16Height;
    zoomEntry[0].stCropWin.u16X = 0;
    zoomEntry[0].stCropWin.u16Y = 0;
    zoomEntry[0].stSensorRes.u16Width = pSclAttr->stSCLOutputSize.u16Width;
    zoomEntry[0].stSensorRes.u16Height = pSclAttr->stSCLOutputSize.u16Height;
    zoomEntry[0].eSensorPixFormat = eSensorPixFormat;
    for (i = 1; i < ISP_ZOOM_ENTRY_CNT; i++)
    {
        width = ((width * 100) / speed);
        zoomEntry[i].u8ZoomSensorId = 0;
        zoomEntry[i].stSensorRes.u16Width = pSclAttr->stSCLOutputSize.u16Width;
        zoomEntry[i].stSensorRes.u16Height = pSclAttr->stSCLOutputSize.u16Height;
        zoomEntry[i].eSensorPixFormat = eSensorPixFormat;
        zoomEntry[i].stCropWin.u16Width = ANJ_ALIGN_DOWN(width, 2);
        if (0 == bMatchTarget && zoomEntry[i].stCropWin.u16Width <= stTargetRect.u16Width)
        {
            zoomEntry[i].stCropWin.u16Width = stTargetRect.u16Width;
            bMatchTarget = 1;
        }
        if (zoomEntry[i].stCropWin.u16Width < stTargetRect.u16Width)
        {
            break;
        }
        int height = zoomEntry[i].stCropWin.u16Width * stTargetRect.u16Height / stTargetRect.u16Width;
        zoomEntry[i].stCropWin.u16Height = ANJ_ALIGN_DOWN(height, 2);
        int x = (pSclAttr->stSCLOutputSize.u16Width - zoomEntry[i].stCropWin.u16Width) / 2;
        zoomEntry[i].stCropWin.u16X = ANJ_ALIGN_DOWN(x, 2);
        int y = (pSclAttr->stSCLOutputSize.u16Height - zoomEntry[i].stCropWin.u16Height) / 2;
        zoomEntry[i].stCropWin.u16Y = ANJ_ALIGN_DOWN(y, 2);
    }
    if (i >= ISP_ZOOM_ENTRY_CNT)
    {
        __ERR("how can speed %d too slow i %d", speed, i);
        return -1;
    }

    zoomNum = i;
    MI_ISP_ZoomTable_t zoomTable = {0};
    zoomTable.pVirTableAddr = zoomEntry;
    zoomTable.u32EntryNum = zoomNum;
    zoomTable.u16SCLOutputPortMask = 1 << (MAX_SCL_PORT - 1);

    STCHECKRESULT(ST_Common_IspZoomTableLoad(pIspAttr->IspDevId, pIspAttr->IspChnId, &zoomTable));
    for (int i = 0; i < zoomNum; i++)
    {
        __ERR("i:%d zoomEntry xywh:%d %d %d %d\n", i,
              zoomEntry[i].stCropWin.u16X, zoomEntry[i].stCropWin.u16Y, zoomEntry[i].stCropWin.u16Width, zoomEntry[i].stCropWin.u16Height);
    }
    return 0;
}

int anj_mw_media_isp_zoom_move_init(int iCameraIdex, double max_multiple, DOUBLE_AREA_ENTRY *pCurArea, DOUBLE_AREA_ENTRY *pTargetArea)
{
    __INFO("zoom cur_area xy:%f %f multiple:%f\n", pCurArea->xPos, pCurArea->yPos, pCurArea->width);
    __INFO("zoom target xy:%f %f multiple:%f\n", pTargetArea->xPos, pTargetArea->yPos, pTargetArea->width);
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    MI_SNR_PlaneInfo_t stSnrPlane0Info;
    memset(&stSnrPlane0Info, 0x0, sizeof(MI_SNR_PlaneInfo_t));
    STCHECKRESULT(MI_SNR_GetPlaneInfo(pstVideoAttr[iCameraIdex].SnrPadId, 0, &stSnrPlane0Info));
    MI_SYS_PixelFormat_e eSensorPixFormat =
        (MI_SYS_PixelFormat_e)RGB_BAYER_PIXEL(stSnrPlane0Info.ePixPrecision, stSnrPlane0Info.eBayerId);
    ST_Common_SclAttr_t *pSclAttr = &pstVideoAttr[iCameraIdex].SclAttr[0];
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    float run_multiple = fmax(pTargetArea->width, pTargetArea->height);
    // 限制范围
    run_multiple = fmax(1.0, fmin(max_multiple, run_multiple));

    ANJ_SIZE_S tmpSize;
    MI_SYS_WindowRect_t stCurRect = {0};
    tmpSize.u32Width = round(pSclAttr->stSCLOutputSize.u16Width / pCurArea->width);
    stCurRect.u16X = pSclAttr->stSCLOutputSize.u16Width * pCurArea->xPos;
    stCurRect.u16Y = pSclAttr->stSCLOutputSize.u16Height * pCurArea->yPos;
    stCurRect.u16Width = ANJ_ALIGN_DOWN(tmpSize.u32Width, 2);
    stCurRect.u16Height = tmpSize.u32Width * pSclAttr->stSCLOutputSize.u16Height / pSclAttr->stSCLOutputSize.u16Width;

    MI_SYS_WindowRect_t stTargetRect = {0};
    tmpSize.u32Width = round(pSclAttr->stSCLOutputSize.u16Width / run_multiple);
    stTargetRect.u16X = pSclAttr->stSCLOutputSize.u16Width * pTargetArea->xPos;
    stTargetRect.u16Y = pSclAttr->stSCLOutputSize.u16Height * pTargetArea->yPos;
    stTargetRect.u16Width = ANJ_ALIGN_DOWN(tmpSize.u32Width, 2);
    stTargetRect.u16Height = tmpSize.u32Width * pSclAttr->stSCLOutputSize.u16Height / pSclAttr->stSCLOutputSize.u16Width;
    // 边界保护
    if (stTargetRect.u16X < 0)
        stTargetRect.u16X = 0;
    if (stTargetRect.u16Y < 0)
        stTargetRect.u16Y = 0;
    if (stTargetRect.u16X + stTargetRect.u16Width > pSclAttr->stSCLOutputSize.u16Width)
        stTargetRect.u16X = pSclAttr->stSCLOutputSize.u16Width - stTargetRect.u16Width;
    if (stTargetRect.u16Y + stTargetRect.u16Height > pSclAttr->stSCLOutputSize.u16Height)
        stTargetRect.u16Y = pSclAttr->stSCLOutputSize.u16Height - stTargetRect.u16Height;

    /*
     * track 目标打到边界时，归一化坐标可能在上层看起来变化很大，
     * 但量化到实际 crop window 后已经和当前位置一致。
     * 这时继续下发一整张重复的 zoom table 没有意义，且底层 ISP 对此较敏感。
     */
    if (stCurRect.u16X == stTargetRect.u16X &&
        stCurRect.u16Y == stTargetRect.u16Y &&
        stCurRect.u16Width == stTargetRect.u16Width &&
        stCurRect.u16Height == stTargetRect.u16Height)
    {
        __INFO("zoom move skip: cur rect equals target rect after clamp\n");
        return 0;
    }

    float move_distance = sqrtf(powf(stTargetRect.u16X - stCurRect.u16X, 2) +
                                powf(stTargetRect.u16Y - stCurRect.u16Y, 2));
    float width_change = fabsf((float)stTargetRect.u16Width - (float)stCurRect.u16Width);
    float path_len = move_distance + width_change;
    int stepCount = (int)(path_len / (float)ZOOM_TRACK_PX_PER_STEP + 0.5f);

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
        return -1;
    }

    int x_diff = stTargetRect.u16X - stCurRect.u16X;
    int y_diff = stTargetRect.u16Y - stCurRect.u16Y;
    int width_diff = stTargetRect.u16Width - stCurRect.u16Width;
    int entryCount = 1;

    zoomEntry[0].u8ZoomSensorId = 0;
    zoomEntry[0].stSensorRes.u16Width = pSclAttr->stSCLOutputSize.u16Width;
    zoomEntry[0].stSensorRes.u16Height = pSclAttr->stSCLOutputSize.u16Height;
    zoomEntry[0].eSensorPixFormat = eSensorPixFormat;
    zoomEntry[0].stCropWin.u16X = ANJ_ALIGN_DOWN(stCurRect.u16X, 2);
    zoomEntry[0].stCropWin.u16Y = ANJ_ALIGN_DOWN(stCurRect.u16Y, 2);
    zoomEntry[0].stCropWin.u16Width = ANJ_ALIGN_DOWN(stCurRect.u16Width, 2);
    zoomEntry[0].stCropWin.u16Height = ANJ_ALIGN_DOWN(stCurRect.u16Height, 2);

    for (int i = 1; i <= stepCount; i++)
    {
        float t = (float)i / (float)stepCount;
        float progress = anj_mw_zoom_s_curve_progress(t);
        MI_SYS_WindowRect_t nextRect = {0};

        if (i == stepCount)
        {
            nextRect.u16X = ANJ_ALIGN_DOWN(stTargetRect.u16X, 2);
            nextRect.u16Y = ANJ_ALIGN_DOWN(stTargetRect.u16Y, 2);
            nextRect.u16Width = ANJ_ALIGN_DOWN(stTargetRect.u16Width, 2);
            nextRect.u16Height = ANJ_ALIGN_DOWN(stTargetRect.u16Height, 2);
        }
        else
        {
            int x = stCurRect.u16X + x_diff * progress;
            int y = stCurRect.u16Y + y_diff * progress;
            int width = stCurRect.u16Width + width_diff * progress;
            int height = width * pSclAttr->stSCLOutputSize.u16Height / pSclAttr->stSCLOutputSize.u16Width;
            nextRect.u16X = ANJ_ALIGN_DOWN(x, 2);
            nextRect.u16Y = ANJ_ALIGN_DOWN(y, 2);
            nextRect.u16Width = ANJ_ALIGN_DOWN(width, 2);
            nextRect.u16Height = ANJ_ALIGN_DOWN(height, 2);
        }

        MI_SYS_WindowRect_t *pPrevRect = &zoomEntry[entryCount - 1].stCropWin;
        if (pPrevRect->u16X == nextRect.u16X &&
            pPrevRect->u16Y == nextRect.u16Y &&
            pPrevRect->u16Width == nextRect.u16Width &&
            pPrevRect->u16Height == nextRect.u16Height)
        {
            continue;
        }

        zoomEntry[entryCount].u8ZoomSensorId = 0;
        zoomEntry[entryCount].stSensorRes.u16Width = pSclAttr->stSCLOutputSize.u16Width;
        zoomEntry[entryCount].stSensorRes.u16Height = pSclAttr->stSCLOutputSize.u16Height;
        zoomEntry[entryCount].eSensorPixFormat = eSensorPixFormat;
        zoomEntry[entryCount].stCropWin = nextRect;
        entryCount++;
    }

    __INFO("zoom move stepCount:%d actual:%d path:%f xy:%f dw:%f\n",
           stepCount, entryCount - 1, path_len, move_distance, width_change);
    if (entryCount <= 1)
    {
        __INFO("zoom move skip: no effective rect after align\n");
        return 0;
    }
    zoomNum = entryCount;
    MI_ISP_ZoomTable_t zoomTable = {0};
    zoomTable.pVirTableAddr = zoomEntry;
    zoomTable.u32EntryNum = zoomNum;
    /* 打印本次生成的 zoomEntry 以供调试 */
    for (int i = 0; i < zoomNum; i++)
    {
        __ERR("i:%d zoomEntry xywh:%d %d %d %d\n", i,
              zoomEntry[i].stCropWin.u16X, zoomEntry[i].stCropWin.u16Y, zoomEntry[i].stCropWin.u16Width, zoomEntry[i].stCropWin.u16Height);
    }

    zoomTable.u16SCLOutputPortMask = 1 << (MAX_SCL_PORT - 1);
    STCHECKRESULT(ST_Common_IspZoomTableLoad(pIspAttr->IspDevId, pIspAttr->IspChnId, &zoomTable));
    MI_ISP_ZoomAttr_t stZoomAttr = {0};
    stZoomAttr.u32FromEntryIndex = 0;
    stZoomAttr.u32ToEntryIndex = zoomNum - 1;
    STCHECKRESULT(ST_Common_IspZoomStart(pIspAttr->IspDevId, pIspAttr->IspChnId, &stZoomAttr));

    return 0;
}

int anj_mw_media_isp_zoom_set(int iCameraIdex, double cur_multiple, double run_multiple)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_SclAttr_t *pSclAttr = &pstVideoAttr[iCameraIdex].SclAttr[0];
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    MI_SYS_WindowRect_t stTargetRect = {0};
    MI_SYS_WindowRect_t stCurRect = {0};

    ANJ_SIZE_S tmpSize;
    tmpSize.u32Width = round(pSclAttr->stSCLOutputSize.u16Width / cur_multiple);
    tmpSize.u32Height = round(pSclAttr->stSCLOutputSize.u16Height / cur_multiple);

    stCurRect.u16X = (pSclAttr->stSCLOutputSize.u16Width - tmpSize.u32Width) / 2;
    stCurRect.u16Y = (pSclAttr->stSCLOutputSize.u16Height - tmpSize.u32Height) / 2;
    stCurRect.u16Width = tmpSize.u32Width;
    stCurRect.u16Height = tmpSize.u32Height;

    tmpSize.u32Width = round(pSclAttr->stSCLOutputSize.u16Width / run_multiple);
    tmpSize.u32Height = round(pSclAttr->stSCLOutputSize.u16Height / run_multiple);
    stTargetRect.u16X = (pSclAttr->stSCLOutputSize.u16Width - tmpSize.u32Width) / 2;
    stTargetRect.u16Y = (pSclAttr->stSCLOutputSize.u16Height - tmpSize.u32Height) / 2;
    stTargetRect.u16Width = tmpSize.u32Width;
    stTargetRect.u16Height = tmpSize.u32Height;

    int curIndex = -1;
    int runIndex = -1;
    for (int i = 0; i < zoomNum; i++)
    {
        if (curIndex == -1 && zoomEntry[i].stCropWin.u16Width <= stCurRect.u16Width)
        {
            curIndex = i;
        }
        if (runIndex == -1 && zoomEntry[i].stCropWin.u16Width <= stTargetRect.u16Width)
        {
            runIndex = i;
        }
        if (runIndex > 0 && curIndex > 0)
        {
            break;
        }
    }
    __INFO("cur_multiple:%f run_multiple:%f\n", cur_multiple, run_multiple);
    __INFO("curIndex:%d runIndex:%d\n", curIndex, runIndex);

    MI_ISP_ZoomAttr_t stZoomAttr = {0};
    stZoomAttr.u32FromEntryIndex = curIndex;
    stZoomAttr.u32ToEntryIndex = runIndex;
    STCHECKRESULT(ST_Common_IspZoomStart(pIspAttr->IspDevId, pIspAttr->IspChnId, &stZoomAttr));
    return 0;
}

int anj_mw_media_isp_zoom_get(int iCameraIdex, DOUBLE_AREA_ENTRY *cur_area, double *pRunPercent)
{
    ST_Common_VideoAttr_t *pstVideoAttr = (ST_Common_VideoAttr_t *)anj_mw_media_video_attr_get();
    ST_Common_SclAttr_t *pSclAttr = &pstVideoAttr[iCameraIdex].SclAttr[0];
    ST_Common_IspAttr_t *pIspAttr = &pstVideoAttr[iCameraIdex].IspAttr;

    MI_ISP_ZoomAttr_t stZoomAttr = {0};
    ST_Common_IspZoomCurGet(pIspAttr->IspDevId, pIspAttr->IspChnId, &stZoomAttr);
    if (abs(stZoomAttr.u32ToEntryIndex - stZoomAttr.u32FromEntryIndex) > 0)
    {
        // __INFO("zoom get: u32FromEntryIndex:%d u32ToEntryIndex:%d u32CurEntryIndex:%d\n",
        //        stZoomAttr.u32FromEntryIndex, stZoomAttr.u32ToEntryIndex, stZoomAttr.u32CurEntryIndex);
        double multiple =
            (double)pSclAttr->stSCLOutputSize.u16Width / (double)zoomEntry[stZoomAttr.u32CurEntryIndex].stCropWin.u16Width;
        cur_area->xPos =
            (double)zoomEntry[stZoomAttr.u32CurEntryIndex].stCropWin.u16X / (double)pSclAttr->stSCLOutputSize.u16Width;
        cur_area->yPos =
            (double)zoomEntry[stZoomAttr.u32CurEntryIndex].stCropWin.u16Y / (double)pSclAttr->stSCLOutputSize.u16Height;
        cur_area->width = multiple;
        cur_area->height = multiple;
        if (pRunPercent)
        {
            *pRunPercent =
                (double)abs(stZoomAttr.u32CurEntryIndex - stZoomAttr.u32FromEntryIndex) /
                (double)abs(stZoomAttr.u32ToEntryIndex - stZoomAttr.u32FromEntryIndex);
            // __INFO("run:%f\n", *pRunPercent);
        }
        // __INFO("zoom cur_area xy:%f %f multiple:%f\n", cur_area->xPos, cur_area->yPos, cur_area->width);
        return 0;
    }

    return -1;
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

void anj_mw_media_isp_zoom_track_init()
{
    char *str = NULL;

    s_iZoomTrackFastGain = ZOOM_TARCK_FAST_GAIN;
    str = anj_mw_read_file_buffer(ZOOM_TRACK_FAST_GAIN_DEBUG_FILE);
    if (str)
    {
        s_iZoomTrackFastGain = anj_mw_zoom_track_fast_gain_clamp(atoi(str));
        anj_mw_free(str);
    }
    __INFO("zoom track max step:%d (file:%s)\n",
           s_iZoomTrackFastGain, ZOOM_TRACK_FAST_GAIN_DEBUG_FILE);
}

int anj_mw_media_isp_skip_frame_set(int iCameraIdx, int iFrameCnt)
{
    (void)iCameraIdx;
    (void)iFrameCnt;
    return 0;
}

int anj_mw_media_isp_ai_start(int iCameraIdx, int bEnable)
{
    (void)iCameraIdx;
    (void)bEnable;
    return 0;
}

int anj_mw_media_isp_ai_update(int iCameraIdx, float curGain, int bv, int white_pwm,
                               int *enabled, int *hold_ticks, int *switch_cnt, int is_mono)
{
    (void)iCameraIdx;
    (void)curGain;
    (void)bv;
    (void)white_pwm;
    (void)enabled;
    (void)hold_ticks;
    (void)switch_cnt;
    (void)is_mono;
    return -1;
}
