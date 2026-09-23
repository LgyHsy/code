#include "anj_mw_media_common.h"
#if _SUPPORT_IQTOOL_
#include "mi_iqserver.h"
#endif
#include "mi_isp_cus3a_api.h"
#include "mi_isp_iq.h"
#include "mi_isp_awb.h"

#define ISP_BASE_PARAM_MAX_CHN_16 (16) // ISP基本参数比较前16通道

#define AWB_CACULATE_DST_MAX_VALUE (8191)
#define AWB_CACULATE_SET_MAX_VALUE (255)

unsigned int TransferCaptureValue(int input)
{
    input = anj_mw_check_value_in_range(input, 0, 255);
    float nvalue = ((float)input / 255.0) * 100;

    int output = (int)(nvalue + 0.5);
    output = anj_mw_check_value_in_range(output, 0, 100);

    return output;
}

// 根据范围与默认值，计算实际设置的值
// 下发到底层的默认值，对应应用设置下来的50
// 按default值分成2段来计算，保证set.value=50的时候落在default上面
int TranferSettingValue(int dstmin, int dstmax, int dstvalue, int setmin, int setmax, int setvalue)
{
    int value_cfg;

    float tmp = ((float)setmax - (float)setmin) / 2;
    tmp += 0.5;

    int mid = (int)tmp; // 计算设置的中值
    setvalue = anj_mw_check_value_by_default(setvalue, setmin, setmax, mid);

    if (setmax == mid) // 防止除0
    {
        value_cfg = dstvalue;
    }
    else if (setvalue > mid)
    {
        int value_setting_high = setvalue - mid;
        float rate = (float)value_setting_high / (float)(setmax - mid);
        float value_cfg_high = (float)(dstmax - dstvalue) * rate + 0.5;

        //      printf("%d %f %f\n", value_cfg_high, rate, value_cfg_high);

        value_cfg = (int)value_cfg_high + dstvalue;
    }
    else if (setvalue < mid)
    {
        int value_setting_low = mid - setvalue;
        float rate = (float)value_setting_low / (float)(mid - setmin);
        float value_cfg_low = (float)(dstvalue - dstmin) * rate + 0.5;

        value_cfg = dstvalue - (int)value_cfg_low;
    }
    else
    {
        value_cfg = dstvalue;
    }

    value_cfg = anj_mw_check_value_in_range(value_cfg, dstmin, dstmax);

    /*
        printf("dst: %d-%d-%d, set: %d-%d-%d --> %d\n",
            dstmin, dstmax, dstvalue,
            setmin, setmax, setvalue,
            value_cfg);
    */
    return value_cfg;
}

MI_S32 ST_Common_IspInit(MI_ISP_DEV IspDevId)
{
    MI_ISP_DevAttr_t stCreateDevAttr;
    memset(&stCreateDevAttr, 0x0, sizeof(MI_ISP_DevAttr_t));

    stCreateDevAttr.u32DevStitchMask = E_MI_ISP_DEVICEMASK_ID0;

    STCHECKRESULT(MI_ISP_CreateDevice(IspDevId, &stCreateDevAttr));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspUnInit(MI_ISP_DEV IspDevId)
{
    STCHECKRESULT(MI_ISP_DestoryDevice(IspDevId));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspStart(ST_Common_IspAttr_t *pstIspAttr, MI_SYS_WindowSize_t *pSnrRes)
{
    if (NULL == pstIspAttr)
    {
        __ERR("pstIspAttr is NULL\n");
        return MI_FAILED;
    }
    MI_ISP_ChannelAttr_t stIspChnAttr;
    memset(&stIspChnAttr, 0x0, sizeof(MI_ISP_ChannelAttr_t));
    stIspChnAttr.u32SensorBindId = pstIspAttr->u32SensorBindId;
    stIspChnAttr.u32Sync3AType = E_MI_ISP_SYNC3A_NONE;
    STCHECKRESULT(MI_ISP_CreateChannel(pstIspAttr->IspDevId, pstIspAttr->IspChnId, &stIspChnAttr));

    MI_ISP_ChnParam_t stIspChnParam;
    memset(&stIspChnParam, 0x0, sizeof(MI_ISP_ChnParam_t));
    stIspChnParam.bFlip = 0;
    stIspChnParam.bMirror = 0;
    stIspChnParam.e3DNRLevel = E_MI_ISP_3DNR_LEVEL2;
    stIspChnParam.eHDRType = E_MI_ISP_HDR_TYPE_OFF;
    stIspChnParam.eRot = E_MI_SYS_ROTATE_NONE;
    STCHECKRESULT(MI_ISP_SetChnParam(pstIspAttr->IspDevId, pstIspAttr->IspChnId, &stIspChnParam));

    STCHECKRESULT(MI_ISP_StartChannel(pstIspAttr->IspDevId, pstIspAttr->IspChnId));

    MI_ISP_OutPortParam_t stIspOutputParam;
    memset(&stIspOutputParam, 0x0, sizeof(MI_ISP_OutPortParam_t));
    memcpy(&stIspOutputParam.stCropRect, pSnrRes, sizeof(MI_SYS_WindowRect_t));
    stIspOutputParam.ePixelFormat = pstIspAttr->ePixelFormat;
    STCHECKRESULT(MI_ISP_SetOutputPortParam(pstIspAttr->IspDevId, pstIspAttr->IspChnId, pstIspAttr->IspOutPortId, &stIspOutputParam));

    STCHECKRESULT(MI_ISP_EnableOutputPort(pstIspAttr->IspDevId, pstIspAttr->IspChnId, pstIspAttr->IspOutPortId));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspStop(ST_Common_IspAttr_t *pstIspAttr)
{
    if (NULL == pstIspAttr)
    {
        __ERR("pstIspAttr is NULL\n");
        return MI_FAILED;
    }

    STCHECKRESULT(MI_ISP_DisableOutputPort(pstIspAttr->IspDevId, pstIspAttr->IspChnId, pstIspAttr->IspOutPortId));

    STCHECKRESULT(MI_ISP_StopChannel(pstIspAttr->IspDevId, pstIspAttr->IspChnId));

    STCHECKRESULT(MI_ISP_DestroyChannel(pstIspAttr->IspDevId, pstIspAttr->IspChnId));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspParaInit(MI_ISP_CHANNEL IspChnId)
{
    MI_ISP_DEV IspDevId = MI_ISP_DEV0;
    MI_ISP_IQ_ParamInitInfoType_t data = {0};

    STCHECKRESULT(MI_ISP_IQ_GetParaInitStatus(IspDevId, IspChnId, &data));

    if (data.stParaAPI.bFlag)
    {
        return MI_SUCCESS;
    }

    return MI_FAILED;
}

MI_S32 ST_Common_IspLoadIq(MI_ISP_CHANNEL IspChnId, char *filepath)
{
    MI_ISP_DEV IspDevId = MI_ISP_DEV0;

    STCHECKRESULT(MI_ISP_ApiCmdLoadBinFile(IspDevId, IspChnId, filepath, 1234));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspIqStart()
{
    MI_S32 s32Ret = 0;
#if _SUPPORT_IQTOOL_
    /* open IQSERVER*/
    s32Ret = MI_IQSERVER_Open();
    if (MI_IQSERVER_OK != s32Ret)
    {
        __ERR("open iq err %x\n", s32Ret);
    }
#endif
    return s32Ret;
}

MI_S32 ST_Common_IspIqStop()
{
    MI_S32 s32Ret = 0;
#if _SUPPORT_IQTOOL_
    /* close IQSERVER*/
    s32Ret = MI_IQSERVER_Close();
    if (MI_IQSERVER_OK != s32Ret)
    {
        __ERR("open iq err %x\n", s32Ret);
    }
#endif
    return s32Ret;
}

void ST_Common_IspAeInfoGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_AE_ExpoInfoType_t *pstAeData)
{
    MI_ISP_AE_QueryExposureInfo(DevId, Channel, pstAeData);
}

void ST_Common_IspDetectionSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int D2NThd, int N2DThd)
{
    MI_ISP_IQ_DaynightDetectionType_t data;
    memset(&data, 0, sizeof(MI_ISP_IQ_DaynightDetectionType_t));
    data.bEnable = 1;
    data.s32D2N_BvThd = D2NThd;
    data.u32N2D_VsbLtScoreThd = N2DThd;
    MI_ISP_IQ_SetDayNightDetection(DevId, Channel, &data);
}

MI_S32 ST_Common_IspBrightnessSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int brightness)
{
    MI_ISP_IQ_BrightnessType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetBrightness(DevId, Channel, &data));

    if (data.bEnable != E_SS_IQ_TRUE)
    {
        return MI_SUCCESS;
    }

    if (brightness == 128)
    {
        if (data.enOpType != E_SS_IQ_OP_TYP_AUTO)
        {
            data.enOpType = E_SS_IQ_OP_TYP_AUTO;
        }
    }
    else
    {
        MI_U32 u32Lev = TransferCaptureValue(brightness);
        if ((data.enOpType == E_SS_IQ_OP_TYP_MANUAL) && (data.stManual.stParaAPI.u32Lev == u32Lev))
        {
            return MI_SUCCESS;
        }

        __ERR("brightness=%d, u32Lev=%u currrent u32Lev=%u\n", brightness, u32Lev, data.stManual.stParaAPI.u32Lev);

        data.bEnable = E_SS_IQ_TRUE;
        data.enOpType = E_SS_IQ_OP_TYP_MANUAL;
        data.stManual.stParaAPI.u32Lev = u32Lev;
    }

    STCHECKRESULT(MI_ISP_IQ_SetBrightness(DevId, Channel, &data));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspSharpnessGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_IQ_SharpnessType_t *data)
{
    STCHECKRESULT(MI_ISP_IQ_GetSharpness(DevId, Channel, data));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspSharpnessSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel,
                                 char *sharpness0, char *sharpness1, char *sharpness2, int sharpness)
{
    int i = 0;

    MI_ISP_IQ_SharpnessType_t data = {0};
    MI_ISP_IQ_GetSharpness(DevId, Channel, &data);

    for (i = 0; i < MI_ISP_AUTO_NUM; i++)
    {
        MI_U8 nSetValue0 = (MI_U8)((float)sharpness0[i] * (float)sharpness / 128.0);
        MI_U8 nSetValue1 = (MI_U8)((float)sharpness1[i] * (float)sharpness / 128.0);
        MI_U8 nSetValue2 = (MI_U8)((float)sharpness2[i] * (float)sharpness / 128.0);

        if (data.stAuto.stParaAPI[i].u8SharpnessUD[0] != nSetValue0)
        {
            __ERR("0: %d: %u != %u\n", i, data.stAuto.stParaAPI[i].u8SharpnessUD[0], nSetValue0);
            break;
        }

        if (data.stAuto.stParaAPI[i].u8SharpnessUD[1] != nSetValue1)
        {
            __ERR("1: %d: %u != %u\n", i, data.stAuto.stParaAPI[i].u8SharpnessUD[1], nSetValue1);
            break;
        }

        if (data.stAuto.stParaAPI[i].u8SharpnessUD[2] != nSetValue2)
        {
            __ERR("1: %d: %u != %u\n", i, data.stAuto.stParaAPI[i].u8SharpnessUD[2], nSetValue2);
            break;
        }
    }

    if (MI_ISP_AUTO_NUM == i)
    {
        return MI_SUCCESS;
    }

    __ERR("new sharpness=%d\n", sharpness);

    for (i = 0; i < MI_ISP_AUTO_NUM; i++)
    {
        MI_U8 nSetValue0 = (MI_U8)((float)sharpness0[i] * (float)sharpness / 128.0);
        MI_U8 nSetValue1 = (MI_U8)((float)sharpness1[i] * (float)sharpness / 128.0);
        MI_U8 nSetValue2 = (MI_U8)((float)sharpness2[i] * (float)sharpness / 128.0);

        data.stAuto.stParaAPI[i].u8SharpnessUD[0] = nSetValue0;
        data.stAuto.stParaAPI[i].u8SharpnessUD[1] = nSetValue1;
        data.stAuto.stParaAPI[i].u8SharpnessUD[2] = nSetValue2;
    }

    STCHECKRESULT(MI_ISP_IQ_SetSharpness(DevId, Channel, &data));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspSaturationGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_IQ_SaturationType_t *data)
{
    STCHECKRESULT(MI_ISP_IQ_GetSaturation(DevId, Channel, data));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspSaturationSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriSaturation, int saturation)
{
    int i = 0;
    MI_ISP_IQ_SaturationType_t data = {0};
    MI_ISP_IQ_GetSaturation(DevId, Channel, &data);

    for (i = 0; i < MI_ISP_AUTO_NUM; i++)
    {
        if (data.stAuto.stParaAPI[i].u8SatAllStr != oriSaturation[i] * saturation / 128)
        {
            break;
        }
    }

    if (MI_ISP_AUTO_NUM == i)
    {
        return 0;
    }

    __ERR("new saturation=%d\n", saturation);

    for (i = 0; i < MI_ISP_AUTO_NUM; i++)
    {
        data.stAuto.stParaAPI[i].u8SatAllStr = oriSaturation[i] * saturation / 128;
    }

    STCHECKRESULT(MI_ISP_IQ_SetSaturation(DevId, Channel, &data));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspContrastSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int contrast)
{
    MI_ISP_IQ_ContrastType_t data;
    MI_ISP_IQ_GetContrast(DevId, Channel, &data);

    if (data.bEnable != E_SS_IQ_TRUE)
    {
        return MI_SUCCESS;
    }

    if (contrast == 128)
    {
        if (data.enOpType != E_SS_IQ_OP_TYP_AUTO)
        {
            data.enOpType = E_SS_IQ_OP_TYP_AUTO;
        }
    }
    else
    {
        MI_U32 u32Lev = TransferCaptureValue(contrast);
        if ((data.enOpType == E_SS_IQ_OP_TYP_MANUAL) && (data.stManual.stParaAPI.u32Lev == u32Lev))
        {
            return MI_SUCCESS;
        }

        __ERR("contrast=%d, u32Lev=%u currrent u32Lev=%u\n", contrast, u32Lev, data.stManual.stParaAPI.u32Lev);

        data.bEnable = E_SS_IQ_TRUE;
        data.enOpType = E_SS_IQ_OP_TYP_MANUAL;
        data.stManual.stParaAPI.u32Lev = u32Lev;
    }

    STCHECKRESULT(MI_ISP_IQ_SetContrast(DevId, Channel, &data));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspFilckerSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int Hz)
{
    return MI_SUCCESS;
    MI_ISP_AE_FlickerExType_t data;
    MI_ISP_AE_GetFlickerEx(DevId, Channel, &data);

    if (Hz == 60)
    {
        if (data.eFlickerType == E_SS_AE_FLICKER_TYPE_DETECT_60HZ && data.bEnable == E_SS_AE_TRUE)
        {
            return MI_SUCCESS;
        }

        data.eFlickerType = E_SS_AE_FLICKER_TYPE_DETECT_60HZ;
        data.bEnable = E_SS_AE_TRUE;
    }
    else if (Hz == 50)
    {
        if (data.eFlickerType == E_SS_AE_FLICKER_TYPE_DETECT_50HZ && data.bEnable == E_SS_AE_TRUE)
        {
            return MI_SUCCESS;
        }

        data.eFlickerType = E_SS_AE_FLICKER_TYPE_DETECT_50HZ;
        data.bEnable = E_SS_AE_TRUE;
    }
    else if (Hz == 0)
    {
        if (data.bEnable == E_SS_AE_FALSE)
        {
            return MI_SUCCESS;
        }

        data.bEnable = E_SS_AE_FALSE;
    }
    else
    {
        __ERR("error flicker hz:%d\n", Hz);
        return -1;
    }

    STCHECKRESULT(MI_ISP_AE_SetFlickerEx(DevId, Channel, &data));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspShutterSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int minShutter, int maxShutter)
{
    MI_ISP_AE_ExpoLimitType_t data = {0};
    STCHECKRESULT(MI_ISP_AE_GetExposureLimit(DevId, Channel, &data));

    int bSet = 0;
    if (minShutter && (minShutter != data.u32MinShutterUS))
    {
        bSet = 1;
        data.u32MinShutterUS = minShutter;
    }
    if (maxShutter && (maxShutter != data.u32MaxShutterUS))
    {
        bSet = 1;
        data.u32MaxShutterUS = maxShutter;
        data.u32MaxSensorGain = 1024;
    }

    if (bSet)
    {
        STCHECKRESULT(MI_ISP_AE_SetExposureLimit(DevId, Channel, &data));
        __INFO("MI_ISP_AE_SetExposureLimit u32MinShutterUS=%u u32MaxShutterUS:%u success\n",
               data.u32MinShutterUS, data.u32MaxShutterUS);
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspMaxGainSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int sensorgain)
{
    MI_ISP_AE_ExpoLimitType_t data = {0};
    STCHECKRESULT(MI_ISP_AE_GetExposureLimit(DevId, Channel, &data));

    if (data.u32MaxSensorGain == (MI_U32)sensorgain)
    {
        return MI_SUCCESS;
    }

    data.u32MaxSensorGain = sensorgain;

    STCHECKRESULT(MI_ISP_AE_SetExposureLimit(DevId, Channel, &data));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspAWBSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int whitebalance)
{

    MI_ISP_AWB_AttrType_t data;
    MI_ISP_AWB_GetAttr(DevId, Channel, &data);

    int enable = (whitebalance >> 24) & 0xff;
    int R = (whitebalance >> 16) & 0xff;
    int G = (whitebalance >> 8) & 0xff;
    int B = (whitebalance) & 0xff;

    MI_ISP_AWB_ModeType_e eOpType;
    eOpType = (enable != 0) ? E_SS_AWB_MODE_MANUAL : E_SS_AWB_MODE_AUTO;

    MI_ISP_AWB_QueryInfoType_t AwbValue = {0};
    MI_ISP_AWB_QueryInfo(DevId, Channel, &AwbValue);

    MI_U16 u16Rgain;  // RW, Multiplier for R  color channel, Range: [0, 0x2000]
    MI_U16 u16Grgain; // RW, Multiplier for Gr color channel, Range: [0, 0x2000]
    MI_U16 u16Gbgain; // RW, Multiplier for Gb color channel, Range: [0, 0x2000]
    MI_U16 u16Bgain;  // RW, Multiplier for B  color channel, Range: [0, 0x2000]

    int dstmin = 0;
    int dstmax = AWB_CACULATE_DST_MAX_VALUE;
    int setmin = 0;
    int setmax = AWB_CACULATE_SET_MAX_VALUE;

    int dstvalue = AwbValue.u16Rgain;
    int setvalue = R;
    u16Rgain = TranferSettingValue(dstmin, dstmax, dstvalue, setmin, setmax, setvalue);

    dstvalue = AwbValue.u16Grgain;
    setvalue = G;
    u16Grgain = TranferSettingValue(dstmin, dstmax, dstvalue, setmin, setmax, setvalue);

    dstvalue = AwbValue.u16Gbgain;
    setvalue = G;
    u16Gbgain = TranferSettingValue(dstmin, dstmax, dstvalue, setmin, setmax, setvalue);

    dstvalue = AwbValue.u16Bgain;
    setvalue = B;
    u16Bgain = TranferSettingValue(dstmin, dstmax, dstvalue, setmin, setmax, setvalue);

    if (data.eOpType == eOpType)
    {
        if (eOpType == E_SS_AWB_MODE_MANUAL)
        {
            if (data.stManualParaAPI.u16Rgain == u16Rgain && data.stManualParaAPI.u16Grgain == u16Grgain && data.stManualParaAPI.u16Gbgain == u16Gbgain && data.stManualParaAPI.u16Bgain == u16Bgain)
            {
                return 0;
            }
        }
        else if (eOpType == E_SS_AWB_MODE_AUTO)
        {
            return 0;
        }
    }

    MI_ISP_AWB_AttrType_t AwbFull = {0};
    STCHECKRESULT(MI_ISP_AWB_GetAttr(DevId, Channel, &AwbFull));

    data.eOpType = eOpType;

    if (eOpType == E_SS_AWB_MODE_MANUAL)
    {
        data.stManualParaAPI.u16Rgain = u16Rgain;
        data.stManualParaAPI.u16Grgain = u16Grgain;
        data.stManualParaAPI.u16Gbgain = u16Gbgain;
        data.stManualParaAPI.u16Bgain = u16Bgain;

        __ERR("Ready to set MWB [%u %u %u %u] from [%u %u %u]\n",
              u16Rgain, u16Grgain, u16Gbgain, u16Bgain,
              R, G, B);
    }
    else if (eOpType == E_SS_AWB_MODE_AUTO)
    {
        memcpy(&AwbFull.stAutoParaAPI, &data.stAutoParaAPI, sizeof(data.stAutoParaAPI));
    }

    STCHECKRESULT(MI_ISP_AWB_SetAttr(DevId, Channel, &data));
    __ERR("MI_ISP_AWB_SetAttr eOpType=%d success\n", data.eOpType);

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspBLCGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *backlight)
{
    int i = 0;
    MI_ISP_IQ_WdrType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetWdr(DevId, Channel, &data));
    for (i = 0; i < ISP_BASE_PARAM_MAX_CHN_16; i++)
    {
        backlight[i] = data.stAuto.stParaAPI[i].u8GlobalDarkToneEnhance;
    }
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspBLCSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriBacklight, int backlight)
{
    int i = 0;
    int chgflag = 0;

    MI_ISP_IQ_WdrType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetWdr(DevId, Channel, &data));

    char oldvalue = 0;
    char newvalue = 0;
    if (backlight)
    {
        for (i = 0; i < ISP_BASE_PARAM_MAX_CHN_16; i++)
        {
            oldvalue = data.stAuto.stParaAPI[i].u8GlobalDarkToneEnhance;
            newvalue = 15 * backlight / 255;
            if (oldvalue != newvalue)
            {
                chgflag = 1;
                data.stAuto.stParaAPI[i].u8GlobalDarkToneEnhance = newvalue;
            }
        }
    }
    else
    {
        for (i = 0; i < ISP_BASE_PARAM_MAX_CHN_16; i++)
        {
            if (data.stAuto.stParaAPI[i].u8GlobalDarkToneEnhance != oriBacklight[i])
            {
                chgflag = 1;
                data.stAuto.stParaAPI[i].u8GlobalDarkToneEnhance = oriBacklight[i];
            }
        }
    }

    if (1 == chgflag)
    {
        STCHECKRESULT(MI_ISP_IQ_SetWdr(DevId, Channel, &data));
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspHLCSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int hlc, int brightness)
{
    int i = 0;

    MI_ISP_AE_StrategyType_t data = {0};
    STCHECKRESULT(MI_ISP_AE_GetStrategy(DevId, Channel, &data));

    if (hlc)
    {
        if ((data.u32Weighting == 1024) && (data.u32AutoStrength == 1024) &&
            (data.u32AutoSensitivity == 1024) && (data.stLowerOffset.u32Y[0] == (MI_U32)(470 * hlc / 255)))
        {
            return MI_SUCCESS;
        }

        data.eAEStrategyMode = E_SS_AE_STRATEGY_AUTO;
        data.u32Weighting = 1024;
        data.u32AutoStrength = 1024;
        data.u32AutoSensitivity = 1024;

        for (i = 0; i < ISP_BASE_PARAM_MAX_CHN_16; i++)
        {
            data.stLowerOffset.u32Y[i] = 470 * hlc / 255;
        }
    }
    else
    {
        if ((data.u32Weighting == 128) && (data.u32AutoStrength == 0) && (data.u32AutoSensitivity == 0))
        {
            return MI_SUCCESS;
        }

        data.eAEStrategyMode = E_SS_AE_STRATEGY_AUTO;
        data.u32Weighting = 128;
        data.u32AutoStrength = 0;
        data.u32AutoSensitivity = 0;

        for (i = 0; i < ISP_BASE_PARAM_MAX_CHN_16; i++)
        {
            data.stLowerOffset.u32Y[i] = 0;
        }
    }

    STCHECKRESULT(MI_ISP_AE_SetStrategy(DevId, Channel, &data));

    // MI_ISP_IQ_BrightnessType_t data_bh = {0};
    // STCHECKRESULT(MI_ISP_IQ_GetBrightness(DevId, Channel, &data_bh));
    // if (data_bh.bEnable != E_SS_IQ_TRUE)
    // {
    //     return MI_SUCCESS;
    // }

    // if (brightness == 128 && hlc == 0)
    // {
    //     if (data_bh.enOpType != E_SS_IQ_OP_TYP_AUTO)
    //     {
    //         data_bh.enOpType = E_SS_IQ_OP_TYP_AUTO;
    //     }
    // }
    // else
    // {
    //     if (hlc == 0)
    //     {
    //         return MI_SUCCESS;
    //     }

    //     MI_U32 u32Lev = brightness * (1 - hlc / 256.0);

    //     u32Lev = TransferCaptureValue(u32Lev);
    //     if ((data_bh.enOpType == E_SS_IQ_OP_TYP_MANUAL) && (data_bh.stManual.stParaAPI.u32Lev == u32Lev))
    //     {
    //         return MI_SUCCESS;
    //     }
    //     __INFO("u32Lev = [%d]\n", u32Lev);

    //     data_bh.bEnable = E_SS_IQ_TRUE;
    //     data_bh.enOpType = E_SS_IQ_OP_TYP_MANUAL;
    //     data_bh.stManual.stParaAPI.u32Lev = u32Lev;
    // }

    // STCHECKRESULT(MI_ISP_IQ_SetBrightness(DevId, Channel, &data_bh));

    return MI_SUCCESS;
}

MI_S32 ST_Common_ISP2DnrGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *tnf)
{
    int i = 0;
    MI_ISP_IQ_NrDespikeType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetNrDeSpike(DevId, Channel, &data));
    for (i = 0; i < 16; i++)
    {
        tnf[i] = data.stAuto.stParaAPI[i].u8BlendRatio;
    }
    return MI_SUCCESS;
}

MI_S32 ST_Common_ISP2DnrSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriTnf, int tnf)
{
    int i = 0;
    int chgflag = 0;

    MI_ISP_IQ_NrDespikeType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetNrDeSpike(DevId, Channel, &data));

    char oldvalue = 0;
    char newvalue = 0;
    for (i = 0; i < 16; i++)
    {
        oldvalue = oriTnf[i];
        newvalue = oldvalue * tnf / 128;
        if (oldvalue != newvalue)
        {
            chgflag = 1;
            data.stAuto.stParaAPI[i].u8BlendRatio = newvalue;
        }
    }

    if (1 == chgflag)
    {
        STCHECKRESULT(MI_ISP_IQ_SetNrDeSpike(DevId, Channel, &data));
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_ISP3DnrGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *snf)
{
    int i = 0;
    MI_ISP_IQ_Nr3dType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetNr3d(DevId, Channel, &data));
    for (i = 0; i < 16; i++)
    {
        snf[i] = data.stAuto.stParaAPI[i].u16MdGain;
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_ISP3DnrSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriSnf, int snf)
{
    int i = 0;
    int chgflag = 0;

    MI_ISP_IQ_Nr3dType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetNr3d(DevId, Channel, &data));

    char oldvalue = 0;
    char newvalue = 0;
    for (i = 0; i < 16; i++)
    {
        oldvalue = oriSnf[i];
        newvalue = oldvalue * snf / 128;
        if (oldvalue != newvalue)
        {
            chgflag = 1;
            data.stAuto.stParaAPI[i].u16MdGain = newvalue;
        }
    }

    if (1 == chgflag)
    {
        STCHECKRESULT(MI_ISP_IQ_SetNr3d(DevId, Channel, &data));
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_ISPWeightSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int weight)
{
    MI_ISP_AE_WinWeightType_t data = {0};
    STCHECKRESULT(MI_ISP_AE_GetWinWgt(DevId, Channel, &data));

    if (weight == 0)
    {
        if (data.eTypeID == E_SS_AE_WEIGHT_AVERAGE)
        {
            return MI_SUCCESS;
        }

        data.eTypeID = E_SS_AE_WEIGHT_AVERAGE;
    }
    else
    {
        if (data.eTypeID == E_SS_AE_WEIGHT_CENTER)
        {
            return MI_SUCCESS;
        }

        data.eTypeID = E_SS_AE_WEIGHT_CENTER;
    }

    STCHECKRESULT(MI_ISP_AE_SetWinWgt(DevId, Channel, &data));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspWdrValueGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *wdr_value)
{
    int i = 0;
    MI_ISP_IQ_WdrType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetWdr(DevId, Channel, &data));
    for (i = 0; i < 16; i++)
    {
        wdr_value[i] = data.stAuto.stParaAPI[i].u8Strength;
    }
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspWdrValueSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriWdrValue, int wdr_value)
{
    int i = 0;
    int chgflag = 0;

    MI_ISP_IQ_WdrType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetWdr(DevId, Channel, &data));
    if (wdr_value)
    {
        char oldvalue = 0;
        char newvalue = 0;
        for (i = 0; i < 16; i++)
        {
            oldvalue = oriWdrValue[i];
            newvalue = oldvalue * wdr_value / 128;
            if (oldvalue != newvalue)
            {
                chgflag = 1;
                data.stAuto.stParaAPI[i].u8Strength = newvalue;
            }
        }
    }

    if (1 == chgflag)
    {
        STCHECKRESULT(MI_ISP_IQ_SetWdr(DevId, Channel, &data));
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspWdrEnableSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int enable)
{
    MI_ISP_IQ_WdrType_t data = {0};
    STCHECKRESULT(MI_ISP_IQ_GetWdr(DevId, Channel, &data));
    if (enable)
    {
        data.bEnable = E_SS_IQ_TRUE;
    }
    else
    {
        data.bEnable = E_SS_IQ_FALSE;
    }

    STCHECKRESULT(MI_ISP_IQ_SetWdr(DevId, Channel, &data));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspRotateSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int rotate)
{
    MI_ISP_ChnParam_t data = {0};
    STCHECKRESULT(MI_ISP_GetChnParam(DevId, Channel, &data));

    if (rotate > 0)
    {
        if (data.eRot == E_MI_SYS_ROTATE_90)
        {
            return MI_SUCCESS;
        }
        data.eRot = E_MI_SYS_ROTATE_90;
    }
    else
    {
        if (data.eRot == E_MI_SYS_ROTATE_NONE)
        {
            return MI_SUCCESS;
        }
        data.eRot = E_MI_SYS_ROTATE_NONE;
    }

    STCHECKRESULT(MI_ISP_SetChnParam(DevId, Channel, &data));

    return MI_SUCCESS;
}

// daynight -> DAY_MODE:1 NIGHT_MODE:2
MI_S32 ST_Common_IspDayNightSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int daynight)
{
    __ERR("isp set dev:%d chn:%d %s mode!\n", DevId, Channel, (daynight == 1) ? "day" : "night");

    MI_ISP_IQ_ColorToGrayType_t data = {0};

    if (1 == daynight)
    {
        data.bEnable = E_SS_IQ_FALSE;
    }
    else
    {
        data.bEnable = E_SS_IQ_TRUE;
    }

    STCHECKRESULT(MI_ISP_IQ_SetColorToGray(DevId, Channel, &data));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspShutterusGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *maxshutterus, unsigned int *minshutterus)
{
    MI_ISP_AE_ExpoLimitType_t data = {0};
    STCHECKRESULT(MI_ISP_AE_GetExposureLimit(DevId, Channel, &data));

    *maxshutterus = data.u32MaxShutterUS;
    *minshutterus = data.u32MinShutterUS;

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspShutterusSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int maxshutterus, unsigned int minshutterus)
{
    MI_ISP_AE_ExpoLimitType_t data = {0};
    STCHECKRESULT(MI_ISP_AE_GetExposureLimit(DevId, Channel, &data));

    data.u32MaxShutterUS = maxshutterus;
    data.u32MinShutterUS = minshutterus;

    STCHECKRESULT(MI_ISP_AE_SetExposureLimit(DevId, Channel, &data));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspAeTargetYGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32Y)
{
    MI_ISP_AE_IntpLutType_t ae_target = {0};
    STCHECKRESULT(MI_ISP_AE_GetTarget(DevId, Channel, &ae_target));

    memcpy(u32Y, &ae_target.u32Y, sizeof(ae_target.u32Y));

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspAeTargetYSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32Y)
{
    MI_ISP_AE_IntpLutType_t ae_target = {0};
    STCHECKRESULT(MI_ISP_AE_GetTarget(DevId, Channel, &ae_target));

    if (memcmp(&ae_target.u32Y, u32Y, sizeof(ae_target.u32Y)))
    {
        memcpy(&ae_target.u32Y, u32Y, sizeof(ae_target.u32Y));

        STCHECKRESULT(MI_ISP_AE_SetTarget(DevId, Channel, &ae_target));
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspConvergeSpeedGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32SpeedY)
{
    MI_ISP_AE_ConvConditonType_t ae_data = {0};
    STCHECKRESULT(MI_ISP_AE_GetConverge(DevId, Channel, &ae_data));

    u32SpeedY[0] = ae_data.stConvSpeed.u32SpeedY[0];
    u32SpeedY[1] = ae_data.stConvSpeed.u32SpeedY[1];
    u32SpeedY[2] = ae_data.stConvSpeed.u32SpeedY[2];
    u32SpeedY[3] = ae_data.stConvSpeed.u32SpeedY[3];

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspConvergeSpeedSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32SpeedY)
{
    MI_ISP_AE_ConvConditonType_t ae_data = {0};
    STCHECKRESULT(MI_ISP_AE_GetConverge(DevId, Channel, &ae_data));
    if (data.stConvSpeed.u32SpeedY[0] != u32SpeedY[0] ||
        data.stConvSpeed.u32SpeedY[1] != u32SpeedY[1] ||
        data.stConvSpeed.u32SpeedY[2] != u32SpeedY[2] ||
        data.stConvSpeed.u32SpeedY[3] != u32SpeedY[3])
    {
        data.stConvSpeed.u32SpeedY[0] = u32SpeedY[0];
        data.stConvSpeed.u32SpeedY[1] = u32SpeedY[1];
        data.stConvSpeed.u32SpeedY[2] = u32SpeedY[2];
        data.stConvSpeed.u32SpeedY[3] = u32SpeedY[3];
        STCHECKRESULT(MI_ISP_AE_SetConverge(DevId, Channel, &ae_data));
    }

    return MI_SUCCESS;
}

MI_S32 ST_Common_IspZoomStop(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel)
{
    STCHECKRESULT(MI_ISP_StopPortZoom(DevId, Channel));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspZoomStart(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_ZoomAttr_t *pstZoomAttr)
{
    STCHECKRESULT(MI_ISP_StartPortZoom(DevId, Channel, pstZoomAttr));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspZoomTableLoad(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_ZoomTable_t *pZoomTable)
{
    STCHECKRESULT(MI_ISP_LoadPortZoomTable(DevId, Channel, pZoomTable));
    return MI_SUCCESS;
}

MI_S32 ST_Common_IspZoomCurGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_ZoomAttr_t *pstZoomAttr)
{
    STCHECKRESULT(MI_ISP_GetPortCurZoomAttr(DevId, Channel, pstZoomAttr));
    return MI_SUCCESS;
}
