#include "anj_mw_media_common.h"

#include <unistd.h>

#define AWB_CACULATE_DST_MAX_VALUE (8191)
#define AWB_CACULATE_SET_MAX_VALUE (255)
/* UsrPreference 各档位 SDK 范围 [0, 256]，128 为中性 */
#define ISP_USR_PREFERENCE_MAX (256)
/* WDR 强度用 LTM contrast，对齐 MStar Strength 缓存到 0~255 */
#define ISP_LTM_WDR_STRENGTH_MAX (255)
#define ISP_SKIP_FRAME_MAX (2)
#define ISP_BASE_PARAM_MAX_CHN_16 (16)

/* 根据范围与默认值，计算实际设置的值（中值对应 default） */
static int TranferSettingValue(int dstmin, int dstmax, int dstvalue, int setmin, int setmax, int setvalue)
{
    int value_cfg;

    float tmp = ((float)setmax - (float)setmin) / 2;
    tmp += 0.5;

    int mid = (int)tmp;
    setvalue = anj_mw_check_value_by_default(setvalue, setmin, setmax, mid);

    if (setmax == mid)
    {
        value_cfg = dstvalue;
    }
    else if (setvalue > mid)
    {
        int value_setting_high = setvalue - mid;
        float rate = (float)value_setting_high / (float)(setmax - mid);
        float value_cfg_high = (float)(dstmax - dstvalue) * rate + 0.5;

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
    return value_cfg;
}

TS_S32 TS_Common_IspLoadIq(VI_PIPE ViPipe, char *filepath)
{
    char tuning_path[256] = {0};
    const char *filename = NULL;

    filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    snprintf(tuning_path, sizeof(tuning_path), "%.*stuning%s",
             (int)(filename - filepath), filepath, filename + 11);

    if (strstr(filepath, "bin"))
    {
        STCHECKRESULT(ISP_Set_CalibAttr_From_Bin_File(ViPipe, filepath));
        STCHECKRESULT(ISP_Set_TuningAttr_From_Bin_File(ViPipe, tuning_path));
    }
    else
    {
        STCHECKRESULT(ISP_Set_CalibAttr_From_Json_File(ViPipe, filepath));
        STCHECKRESULT(ISP_Set_TuningAttr_From_Json_File(ViPipe, tuning_path));
    }
    
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspIqStart(void)
{
#if _SUPPORT_IQTOOL_
    STCHECKRESULT(TS_MPI_ISP_StartTuningToolServer());
#endif
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspIqStop(void)
{
#if _SUPPORT_IQTOOL_
    STCHECKRESULT(TS_MPI_ISP_StopTuningToolServer());
#endif
    if (access("/mnt/nand/iqserver.flag", F_OK) == 0)
    {
        STCHECKRESULT(TS_MPI_ISP_StopTuningToolServer());
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspBrightnessSet(VI_PIPE ViPipe, int brightness)
{
    ISP_USR_PREFERENCE_S stPref = {0};
    TS_U32 newval;

    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    newval = (TS_U32)anj_mw_check_value_in_range(brightness, 0, ISP_USR_PREFERENCE_MAX);
    if (stPref.u32Brightness == newval)
    {
        return TS_SUCCESS;
    }
    stPref.u32Brightness = newval;
    STCHECKRESULT(TS_MPI_ISP_SetUsrPreference(ViPipe, &stPref));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspSharpnessSet(VI_PIPE ViPipe, int sharpness)
{
    ISP_USR_PREFERENCE_S stPref = {0};
    TS_U32 newval;

    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    newval = (TS_U32)anj_mw_check_value_in_range(sharpness, 0, ISP_USR_PREFERENCE_MAX);
    if (stPref.u32Sharpness == newval)
    {
        return TS_SUCCESS;
    }
    stPref.u32Sharpness = newval;
    STCHECKRESULT(TS_MPI_ISP_SetUsrPreference(ViPipe, &stPref));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspSaturationSet(VI_PIPE ViPipe, int saturation)
{
    ISP_USR_PREFERENCE_S stPref = {0};
    TS_U32 newval;

    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    newval = (TS_U32)anj_mw_check_value_in_range(saturation, 0, ISP_USR_PREFERENCE_MAX);
    if (stPref.u32Saturation == newval)
    {
        return TS_SUCCESS;
    }
    stPref.u32Saturation = newval;
    STCHECKRESULT(TS_MPI_ISP_SetUsrPreference(ViPipe, &stPref));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspContrastSet(VI_PIPE ViPipe, int contrast)
{
    ISP_USR_PREFERENCE_S stPref = {0};
    TS_U32 newval;

    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    newval = (TS_U32)anj_mw_check_value_in_range(contrast, 0, ISP_USR_PREFERENCE_MAX);
    if (stPref.u32Contrast == newval)
    {
        return TS_SUCCESS;
    }
    stPref.u32Contrast = newval;
    STCHECKRESULT(TS_MPI_ISP_SetUsrPreference(ViPipe, &stPref));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspFilckerSet(VI_PIPE ViPipe, int Hz)
{
    ISP_AEC_S stAecAttr = {0};

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));

    if (Hz == 50 || Hz == 60)
    {
        if (stAecAttr.enOpType == E_OP_TYPE_AUTO &&
            stAecAttr.bAntiFlickerEnable == 1 &&
            stAecAttr.u8AntiFlickerFrequency == (TS_U8)Hz)
        {
            return TS_SUCCESS;
        }
        stAecAttr.bAntiFlickerEnable = 1;
        stAecAttr.u8AntiFlickerFrequency = (TS_U8)Hz;
    }
    else if (Hz == 0)
    {
        if (stAecAttr.enOpType == E_OP_TYPE_AUTO &&
            stAecAttr.bAntiFlickerEnable == 0)
        {
            return TS_SUCCESS;
        }
        stAecAttr.bAntiFlickerEnable = 0;
    }
    else
    {
        __ERR("error flicker hz:%d\n", Hz);
        return TS_FAILED;
    }

    stAecAttr.enOpType = E_OP_TYPE_AUTO;
    STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspForceFlickerSet(VI_PIPE ViPipe, int forceFlicker)
{
    ISP_AEC_S stAecAttr = {0};

    if (forceFlicker != 0 && forceFlicker != 1)
    {
        __ERR("error force flicker:%d\n", forceFlicker);
        return TS_FAILED;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));
    if (stAecAttr.bForceFlicker == (TS_U8)forceFlicker)
    {
        __INFO("force flicker already set:%d\n", forceFlicker);
        return TS_SUCCESS;
    }
    stAecAttr.bForceFlicker = (TS_U8)forceFlicker;
    STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspShutterSet(VI_PIPE ViPipe, int minShutter, int maxShutter)
{
    ISP_AEC_S stAecAttr = {0};
    TS_U32 idx;
    TS_U32 validSize;

    if (minShutter < 0 || maxShutter < 0)
    {
        __ERR("error shutter min:%d max:%d\n", minShutter, maxShutter);
        return TS_FAILED;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));

    validSize = stAecAttr.stAutoParam.stAecExpRange.validSize;
    if (validSize == 0 || validSize > 10)
    {
        validSize = 1;
        stAecAttr.stAutoParam.stAecExpRange.validSize = 1;
    }
    for (idx = 0; idx < validSize; idx++)
    {
        if (minShutter > 0)
        {
            stAecAttr.stAutoParam.stAecExpRange.tab[idx].params.u32ExpTimeMin = minShutter;
        }
        if (maxShutter > 0)
        {
            stAecAttr.stAutoParam.stAecExpRange.tab[idx].params.u32ExpTimeMax = maxShutter;
        }
    }
    __INFO("set shutter min:%d max:%d validSize:%d\n", 
        minShutter, maxShutter, stAecAttr.stAutoParam.stAecExpRange.validSize);
    
    STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));

    return TS_SUCCESS;
}

TS_S32 TS_Common_IspMaxGainSet(VI_PIPE ViPipe, int sensorgain)
{
    ISP_AEC_S stAecAttr = {0};
    TS_U32 i;
    TS_U32 validSize;
    int bSet = 0;

    if (sensorgain <= 0)
    {
        return TS_SUCCESS;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));
    stAecAttr.enOpType = E_OP_TYPE_AUTO;

    validSize = stAecAttr.stAutoParam.stAecExpRange.validSize;
    if (validSize == 0 || validSize > 10)
    {
        validSize = 1;
        stAecAttr.stAutoParam.stAecExpRange.validSize = 1;
    }
    for (i = 0; i < validSize; i++)
    {
        if (stAecAttr.stAutoParam.stAecExpRange.tab[i].params.u32SenAgainMax != (TS_U32)sensorgain)
        {
            bSet = 1;
            stAecAttr.stAutoParam.stAecExpRange.tab[i].params.u32SenAgainMax = (TS_U32)sensorgain;
        }
        if (stAecAttr.stAutoParam.stAecExpRange.tab[i].params.u32SenAgainMax <
            stAecAttr.stAutoParam.stAecExpRange.tab[i].params.u32SenAgainMin)
        {
            __ERR("invalid again range min=%u max=%u\n",
                  stAecAttr.stAutoParam.stAecExpRange.tab[i].params.u32SenAgainMin,
                  stAecAttr.stAutoParam.stAecExpRange.tab[i].params.u32SenAgainMax);
            return TS_FAILED;
        }
    }

    validSize = stAecAttr.stAutoParam.stAecExpRangeHot.validSize;
    if (validSize > 0 && validSize <= 10)
    {
        for (i = 0; i < validSize; i++)
        {
            if (stAecAttr.stAutoParam.stAecExpRangeHot.tab[i].params.u32SenAgainMax != (TS_U32)sensorgain)
            {
                bSet = 1;
                stAecAttr.stAutoParam.stAecExpRangeHot.tab[i].params.u32SenAgainMax = (TS_U32)sensorgain;
            }
        }
    }

    if (stAecAttr.stManualParam.u32SenAgainMax != (TS_U32)sensorgain)
    {
        bSet = 1;
        stAecAttr.stManualParam.u32SenAgainMax = (TS_U32)sensorgain;
    }

    if (bSet)
    {
        STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspAWBSet(VI_PIPE ViPipe, int whitebalance)
{
    int enable = (whitebalance >> 24) & 0xff;
    int R = (whitebalance >> 16) & 0xff;
    int G = (whitebalance >> 8) & 0xff;
    int B = whitebalance & 0xff;
    ISP_AWB_S stAwbAttr = {0};
    ISP_WB_STATISTICS_S stWbStat = {0};
    TS_U32 baseR = 1024;
    TS_U32 baseG = 1024;
    TS_U32 baseB = 1024;
    TS_U16 newR;
    TS_U16 newG;
    TS_U16 newB;

    STCHECKRESULT(TS_MPI_ISP_Get_AwbAttr(ViPipe, &stAwbAttr));

    if (enable == 0)
    {
        if (stAwbAttr.bAwbEnable == 1 && stAwbAttr.enOpType == E_OP_TYPE_AUTO)
        {
            return TS_SUCCESS;
        }
        stAwbAttr.bAwbEnable = 1;
        stAwbAttr.enOpType = E_OP_TYPE_AUTO;
        STCHECKRESULT(TS_MPI_ISP_Set_AwbAttr(ViPipe, &stAwbAttr));
        return TS_SUCCESS;
    }

    /* QueryWBInfo 已废弃；手动基准优先用当前 manual gain，否则用 WB 统计灰世界估算 */
    if (stAwbAttr.enOpType == E_OP_TYPE_MANUAL &&
        (stAwbAttr.stManualParam.u16AwbRgain != 0 || stAwbAttr.stManualParam.u16AwbGgain != 0 ||
         stAwbAttr.stManualParam.u16AwbBgain != 0))
    {
        baseR = stAwbAttr.stManualParam.u16AwbRgain;
        baseG = stAwbAttr.stManualParam.u16AwbGgain;
        baseB = stAwbAttr.stManualParam.u16AwbBgain;
    }
    else
    {
        STCHECKRESULT(TS_MPI_ISP_GetWBStatistics(ViPipe, &stWbStat));
        baseG = 1024;
        if (stWbStat.u16GlobalR > 0)
        {
            baseR = (TS_U32)stWbStat.u16GlobalG * 1024U / stWbStat.u16GlobalR;
        }
        if (stWbStat.u16GlobalB > 0)
        {
            baseB = (TS_U32)stWbStat.u16GlobalG * 1024U / stWbStat.u16GlobalB;
        }
        if (baseR > AWB_CACULATE_DST_MAX_VALUE)
        {
            baseR = AWB_CACULATE_DST_MAX_VALUE;
        }
        if (baseB > AWB_CACULATE_DST_MAX_VALUE)
        {
            baseB = AWB_CACULATE_DST_MAX_VALUE;
        }
    }

    newR = (TS_U16)TranferSettingValue(0, AWB_CACULATE_DST_MAX_VALUE, (int)baseR, 0, AWB_CACULATE_SET_MAX_VALUE, R);
    newG = (TS_U16)TranferSettingValue(0, AWB_CACULATE_DST_MAX_VALUE, (int)baseG, 0, AWB_CACULATE_SET_MAX_VALUE, G);
    newB = (TS_U16)TranferSettingValue(0, AWB_CACULATE_DST_MAX_VALUE, (int)baseB, 0, AWB_CACULATE_SET_MAX_VALUE, B);
    if (stAwbAttr.bAwbEnable == 1 && stAwbAttr.enOpType == E_OP_TYPE_MANUAL &&
        stAwbAttr.stManualParam.u16AwbRgain == newR &&
        stAwbAttr.stManualParam.u16AwbGgain == newG &&
        stAwbAttr.stManualParam.u16AwbBgain == newB)
    {
        return TS_SUCCESS;
    }
    stAwbAttr.stManualParam.u16AwbRgain = newR;
    stAwbAttr.stManualParam.u16AwbGgain = newG;
    stAwbAttr.stManualParam.u16AwbBgain = newB;
    stAwbAttr.bAwbEnable = 1;
    stAwbAttr.enOpType = E_OP_TYPE_MANUAL;
    STCHECKRESULT(TS_MPI_ISP_Set_AwbAttr(ViPipe, &stAwbAttr));
    return TS_SUCCESS;
}

/*
 * 逆光补偿（产品 BLC）≈ 抬亮暗部主体。
 * 实测：WDR 关闭时 LTM MaxGain 无效果；AE 路径（同 HLC）有效。
 * 故用 enHighlightSupMode = ISP_LOWLIGHT_STRENGTHEN，并用 aeLowLevel 体现强度。
 * （与 HLC 的 HIGHLIGHT_SUPPRESS 互斥，同属 enHighlightSupMode。）
 */
TS_S32 TS_Common_IspBLCGet(VI_PIPE ViPipe, char *backlight)
{
    ISP_AEC_S stAecAttr = {0};
    char value = 0;
    int i;
    if (!backlight)
    {
        return TS_SUCCESS;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));
    if (stAecAttr.enHighlightSupMode == ISP_LOWLIGHT_STRENGTHEN)
    {
        /* 强度用暗区比例回读，便于上层缓存 */
        value = (char)anj_mw_check_value_in_range(
            (int)stAecAttr.stAutoParam.stAecLuxConvTab.tab[0].params.u32AeLowLevel * 255 / 100, 0, 255);
        if (value == 0)
        {
            value = 128;
        }
    }
    for (i = 0; i < ISP_BASE_PARAM_MAX_CHN_16; i++)
    {
        backlight[i] = value;
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspBLCSet(VI_PIPE ViPipe, int backlight)
{
    ISP_AEC_S stAecAttr = {0};
    ISP_AEC_LIGHT_MODE_E enMode = ISP_NORMAL_AE;
    TS_U32 lowLevel = 0;
    TS_U32 i;
    TS_U32 validSize;
    int level;
    int bSet = 0;

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));

    if (backlight > 0)
    {
        enMode = ISP_LOWLIGHT_STRENGTHEN;
        /* backlight 0~255 → aeLowLevel 0~100（暗区比例越大，越偏暗部曝光） */
        level = anj_mw_check_value_in_range(backlight, 0, 255);
        lowLevel = (TS_U32)(level * 100 / 255);
        if (lowLevel == 0)
        {
            lowLevel = 1;
        }
        if (stAecAttr.bAeHighlightSup != 0)
        {
            bSet = 1;
            stAecAttr.bAeHighlightSup = 0;
        }
    }
    else
    {
        enMode = ISP_NORMAL_AE;
        lowLevel = 0;
    }

    if (stAecAttr.enHighlightSupMode != enMode)
    {
        bSet = 1;
        stAecAttr.enHighlightSupMode = enMode;
    }

    validSize = stAecAttr.stAutoParam.stAecLuxConvTab.validSize;
    if (validSize == 0 || validSize > 10)
    {
        validSize = 1;
        stAecAttr.stAutoParam.stAecLuxConvTab.validSize = 1;
        bSet = 1;
    }
    for (i = 0; i < validSize; i++)
    {
        if (stAecAttr.stAutoParam.stAecLuxConvTab.tab[i].params.u32AeLowLevel != lowLevel)
        {
            bSet = 1;
            stAecAttr.stAutoParam.stAecLuxConvTab.tab[i].params.u32AeLowLevel = lowLevel;
        }
    }

    if (bSet)
    {
        STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));
    }
    return TS_SUCCESS;
}

/* HLC ≈ AE 高光抑制（aeHighlightSup / HIGHLIGHT_SUPPRESS） */
TS_S32 TS_Common_IspHLCSet(VI_PIPE ViPipe, int hlc)
{
    ISP_AEC_S stAecAttr = {0};

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));
    if (hlc)
    {
        if (stAecAttr.enHighlightSupMode == ISP_HIGHLIGHT_SUPPRESS &&
            stAecAttr.bAeHighlightSup == 1)
        {
            return TS_SUCCESS;
        }
        stAecAttr.enHighlightSupMode = ISP_HIGHLIGHT_SUPPRESS;
        stAecAttr.bAeHighlightSup = 1;
    }
    else
    {
        if (stAecAttr.enHighlightSupMode == ISP_NORMAL_AE &&
            stAecAttr.bAeHighlightSup == 0)
        {
            return TS_SUCCESS;
        }
        stAecAttr.enHighlightSupMode = ISP_NORMAL_AE;
        stAecAttr.bAeHighlightSup = 0;
    }
    STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));
    return TS_SUCCESS;
}

/* 2DNR ≈ Preference RawDenoise；相对 IQ 基准 ori * tnf / 128（同 MStar） */
TS_S32 TS_Common_ISP2DnrSet(VI_PIPE ViPipe, char *oriTnf, int tnf)
{
    ISP_USR_PREFERENCE_S stPref = {0};
    int oldvalue;
    int newvalue;
    if (!oriTnf)
    {
        return TS_SUCCESS;
    }

    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    oldvalue = (unsigned char)oriTnf[0];
    if (oldvalue <= 0)
    {
        oldvalue = (int)stPref.u32RawDenoise;
        if (oldvalue <= 0)
        {
            oldvalue = 128;
        }
    }
    newvalue = oldvalue * tnf / 128;
    newvalue = anj_mw_check_value_in_range(newvalue, 0, ISP_USR_PREFERENCE_MAX);
    if ((TS_U32)newvalue == stPref.u32RawDenoise)
    {
        return TS_SUCCESS;
    }
    stPref.u32RawDenoise = (TS_U32)newvalue;
    STCHECKRESULT(TS_MPI_ISP_SetUsrPreference(ViPipe, &stPref));
    return TS_SUCCESS;
}

/* 3DNR ≈ Preference YuvDenoise（无独立 TNR 用户旋钮时的近似）；相对缩放同 2DNR */
TS_S32 TS_Common_ISP3DnrSet(VI_PIPE ViPipe, char *oriSnf, int snf)
{
    ISP_USR_PREFERENCE_S stPref = {0};
    int oldvalue;
    int newvalue;
    if (!oriSnf)
    {
        return TS_SUCCESS;
    }

    STCHECKRESULT(TS_MPI_ISP_GetUsrPreference(ViPipe, &stPref));
    oldvalue = (unsigned char)oriSnf[0];
    if (oldvalue <= 0)
    {
        oldvalue = (int)stPref.u32YuvDenoise;
        if (oldvalue <= 0)
        {
            oldvalue = 128;
        }
    }
    newvalue = oldvalue * snf / 128;
    newvalue = anj_mw_check_value_in_range(newvalue, 0, ISP_USR_PREFERENCE_MAX);
    if ((TS_U32)newvalue == stPref.u32YuvDenoise)
    {
        return TS_SUCCESS;
    }
    stPref.u32YuvDenoise = (TS_U32)newvalue;
    STCHECKRESULT(TS_MPI_ISP_SetUsrPreference(ViPipe, &stPref));
    return TS_SUCCESS;
}

TS_S32 TS_Common_ISPWeightSet(VI_PIPE ViPipe, int weight)
{
    ISP_AEC_S stAecAttr = {0};
    TS_U8 desired;

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));
    /* weight==0 平均，非 0 中心；表索引依赖 tuning（aeWeightTabSel [0:5]） */
    desired = (weight == 0) ? 0 : 1;
    if (stAecAttr.stAeWeightTab.bAeWeightTabSel == desired)
    {
        return TS_SUCCESS;
    }
    stAecAttr.stAeWeightTab.bAeWeightTabSel = desired;
    STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));
    return TS_SUCCESS;
}

/*
 * WDR 强度 ≈ MStar WDR Strength（blending/LTM 强度），不是 aeLmRatioMax（HDR 曝光比）。
 * TS：调 LTM.u32Contrast；上层 128 为基准，new = ori * wdr_value / 128。
 */
TS_S32 TS_Common_IspWdrValueGet(VI_PIPE ViPipe, char *wdr_value)
{
    ISP_LTM_S stLtm = {0};
    TS_U32 contrast;
    int i;
    if (!wdr_value)
    {
        return TS_SUCCESS;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_LtmAttr(ViPipe, &stLtm));
    contrast = stLtm.u32Contrast;
    if (contrast > ISP_LTM_WDR_STRENGTH_MAX)
    {
        contrast = ISP_LTM_WDR_STRENGTH_MAX;
    }
    for (i = 0; i < ISP_BASE_PARAM_MAX_CHN_16; i++)
    {
        wdr_value[i] = (char)contrast;
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspWdrValueSet(VI_PIPE ViPipe, char *oriWdrValue, int wdr_value)
{
    ISP_LTM_S stLtm = {0};
    int newvalue;
    int oldvalue;
    if (!wdr_value || !oriWdrValue)
    {
        return TS_SUCCESS;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_LtmAttr(ViPipe, &stLtm));
    oldvalue = (unsigned char)oriWdrValue[0];
    if (oldvalue <= 0)
    {
        oldvalue = (int)stLtm.u32Contrast;
        if (oldvalue <= 0)
        {
            oldvalue = 128;
        }
    }
    newvalue = oldvalue * wdr_value / 128;
    newvalue = anj_mw_check_value_in_range(newvalue, 0, ISP_LTM_WDR_STRENGTH_MAX);
    if ((TS_U32)newvalue == stLtm.u32Contrast)
    {
        return TS_SUCCESS;
    }

    stLtm.u32Contrast = (TS_U32)newvalue;
    stLtm.bLtmBypass = 0;
    STCHECKRESULT(TS_MPI_ISP_Set_LtmAttr(ViPipe, &stLtm));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspFpsSet(VI_PIPE ViPipe, int fps)
{
    ISP_PUB_ATTR_S stPubAttr = {0};
    int curFps;

    if (fps <= 0)
    {
        return TS_SUCCESS;
    }

    STCHECKRESULT(TS_MPI_ISP_GetPubAttr(ViPipe, &stPubAttr));
    /* OS05A20 2lane stagger 仅支持 WDR_SENSOR_FPS，避免编码帧率把 sensor 拉回 30 */
    if (stPubAttr.enWDRMode != WDR_MODE_NONE && fps > WDR_SENSOR_FPS)
    {
        fps = WDR_SENSOR_FPS;
    }
    curFps = (int)(stPubAttr.f32FrameRate + 0.5f);
    if (curFps == fps)
    {
        return TS_SUCCESS;
    }

    stPubAttr.f32FrameRate = (TS_FLOAT)fps;
    STCHECKRESULT(TS_MPI_ISP_SetPubAttr(ViPipe, &stPubAttr));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspDayNightSet(VI_PIPE ViPipe, int daynight)
{
    TS_U32 u32ParamIndex = (daynight == 1) ? 0 : 1;

    __ERR("isp set pipe:%d %s mode (paramIndex=%u)!\n", ViPipe,
          (daynight == 1) ? "day" : "night", u32ParamIndex);
    STCHECKRESULT(TS_MPI_ISP_SetParamIndex(ViPipe, u32ParamIndex));
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspDayNightGet(VI_PIPE ViPipe, int *daynight)
{
    TS_U32 u32ParamIndex = 0;

    if (daynight == NULL)
    {
        return TS_FAILED;
    }

    STCHECKRESULT(TS_MPI_ISP_GetParamIndex(ViPipe, &u32ParamIndex));
    *daynight = (u32ParamIndex == 0) ? 1 : 0;
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspAeTargetYGet(VI_PIPE ViPipe, unsigned int *u32Y)
{
    ISP_AEC_S stAecAttr = {0};
    TS_U32 i;
    TS_U32 validSize;

    if (u32Y == NULL)
    {
        return TS_FAILED;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));

    validSize = stAecAttr.stAutoParam.stAecLuxConvTab.validSize;
    if (validSize == 0 || validSize > 10)
    {
        validSize = 1;
    }

    for (i = 0; i < validSize; i++)
    {
        u32Y[i] = stAecAttr.stAutoParam.stAecLuxConvTab.tab[i].params.u32AeTarget;
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspAeTargetYSet(VI_PIPE ViPipe, unsigned int *u32Y)
{
    ISP_AEC_S stAecAttr = {0};
    TS_U32 i;
    TS_U32 validSize;
    int bSet = 0;
    if (u32Y == NULL)
    {
        return TS_FAILED;
    }

    STCHECKRESULT(TS_MPI_ISP_Get_AecAttr(ViPipe, &stAecAttr));

    validSize = stAecAttr.stAutoParam.stAecLuxConvTab.validSize;
    if (validSize == 0 || validSize > 10)
    {
        validSize = 1;
        stAecAttr.stAutoParam.stAecLuxConvTab.validSize = 1;
        bSet = 1;
    }
    for (i = 0; i < validSize; i++)
    {
        if (stAecAttr.stAutoParam.stAecLuxConvTab.tab[i].params.u32AeTarget != u32Y[i])
        {
            bSet = 1;
            stAecAttr.stAutoParam.stAecLuxConvTab.tab[i].params.u32AeTarget = u32Y[i];
        }
    }

    validSize = stAecAttr.stAutoParam.stAecLuxConvTabHlSup.validSize;
    if (validSize == 0 || validSize > 10)
    {
        validSize = 1;
        stAecAttr.stAutoParam.stAecLuxConvTabHlSup.validSize = 1;
        bSet = 1;
    }
    for (i = 0; i < validSize; i++)
    {
        if (stAecAttr.stAutoParam.stAecLuxConvTabHlSup.tab[i].params.u32AeTarget != u32Y[i])
        {
            bSet = 1;
            stAecAttr.stAutoParam.stAecLuxConvTabHlSup.tab[i].params.u32AeTarget = u32Y[i];
        }
    }

    if (bSet)
    {
        STCHECKRESULT(TS_MPI_ISP_Set_AecAttr(ViPipe, &stAecAttr));
    }
    return TS_SUCCESS;
}

TS_S32 TS_Common_IspSkipFrameSet(VI_PIPE ViPipe, int FrameCnt)
{
    ISP_GENERAL_TOP_S stTop = {0};
    FrameCnt = anj_mw_check_value_in_range(FrameCnt, 0, ISP_SKIP_FRAME_MAX);
    STCHECKRESULT(TS_MPI_ISP_GetGeneralTop(ViPipe, &stTop));
    stTop.u8SkipFrame = (TS_U8)FrameCnt;
    STCHECKRESULT(TS_MPI_ISP_SetGeneralTop(ViPipe, &stTop));
    return TS_SUCCESS;
}

TS_S32 TS_Common_SensorMirrorSet(VI_PIPE ViPipe, TS_BOOL bHFlip, TS_BOOL bVFlip)
{
    TS_S32 s32Ret = TS_SUCCESS;
    int times = 0;

    do
    {
        s32Ret = TS_MPI_VI_SetSensorMirrorFlip(ViPipe, !bHFlip, !bVFlip);
        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VI_SetSensorMirrorFlip %d %d %d failed, ret:%x!\n", ViPipe, !bHFlip, !bVFlip, s32Ret);
            break;
        }
        else
        {
            __INFO("TS_MPI_VI_SetSensorMirrorFlip %d %d %d OK\n", ViPipe, !bHFlip, !bVFlip);
            break;
        }
        times++;
        usleep(100 * 1000);
    } while (times < 10);

    return TS_SUCCESS;
}
