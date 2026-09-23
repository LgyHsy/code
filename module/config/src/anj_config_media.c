#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_str.h"
#include "anj_config.h"

#include "eventhub.h"

#include "anj_mw_media_video.h"
#include "anj_mw_media_isp.h"

#include "anj_video.h"
#include "anj_audio.h"
#include "anj_osd.h"
#include "anj_ispctl.h"
#include "anj_factory.h"
#include "anj_service.h"
#include "anj_record.h"

static char *osd_time_fmt_list[] =
    {
        "yyyy-mm-dd hh:mm:ss",
        "yyyy/mm/dd hh:mm:ss",
        "yy-mm-dd hh:mm:ss",
        "yy/mm/dd hh:mm:ss",
        "hh:mm:ss dd/mm/yyyy",
        "hh:mm:ss dd-mm-yyyy",
        "hh:mm:ss mm/dd/yyyy",
        "hh:mm:ss mm-dd-yyyy",
        "mm/dd/yyyy hh:mm:ss",
        "mm-dd-yyyy hh:mm:ss"};

int anj_config_image_flip_trans(int hflip, int vflip, int *newHflip, int *newVfilp)
{
    static int flip = -1;
    int iRet = 0;

    do
    {
        if (flip >= 0)
        {
            break;
        }

        iRet = anj_factory_image_flip_cfg_load(&flip);
        if (iRet == 0)
            __INFO("factory image flip:%d!\n", flip);
        else
            flip = 0;

    } while (0);

    if (flip == 0)
    {
        *newHflip = hflip;
        *newVfilp = vflip;
    }
    else
    {
        if (flip == 3 || flip == 1)
        {
            *newHflip = !hflip;
        }

        if (flip == 3 || flip == 2)
        {
            *newVfilp = !vflip;
        }
    }

    return 0;
}

int anj_config_meida_flip_set(unsigned char vflip, unsigned char hflip, int cameraIndex)
{
    int ret = 0;
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();
    pthread_rwlock_wrlock(rwlock);
    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    VideoConfig *pstVideoConfig = &pstMediaConfig->videoConfig[cameraIndex];
    if (pstVideoConfig->videoCapture.vflip != vflip || pstVideoConfig->videoCapture.hflip != hflip)
    {
        int new_hflip = 0;
        int new_vflip = 0;
        anj_config_image_flip_trans(hflip, vflip, &new_hflip, &new_vflip);
        ret = anj_mw_media_isp_flip_set(cameraIndex, new_hflip, new_vflip);
        if (ret == 0)
        {
            pstVideoConfig->videoCapture.vflip = vflip;
            pstVideoConfig->videoCapture.hflip = hflip;
        }
    }
    else
    {
        ret = 0;
    }
    pthread_rwlock_unlock(rwlock);
    return ret;
}

static int anj_config_video_fisheye_get(IXML_Node *pNode, FishEyeCfg *pCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "autocrop"))
        {
            pCfg->autocrop = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "center_ppm_x"))
        {
            pCfg->center_ppm_x = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "diameter_ppm"))
        {
            pCfg->diameter_ppm = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "center_ppm_y"))
        {
            pCfg->center_ppm_y = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

endFunc:
    return iRet;
}

int anj_config_video_capture_defalut(VideoCaptureCfg *pVideoCapture)
{
    if (NULL == pVideoCapture)
    {
        return -1;
    }

    memset(pVideoCapture, 0, sizeof(VideoCaptureCfg));

    pVideoCapture->wdr_value = 128;
    pVideoCapture->dfrog_flag = 0;
    pVideoCapture->dfrog_value = 128;

    pVideoCapture->shutterSetting.shutter_mode_day = 0;
    pVideoCapture->shutterSetting.shutter_mode_night = 0;
    pVideoCapture->shutterSetting.shutter_speed_day = 1000;
    pVideoCapture->shutterSetting.shutter_speed_night = 1000;

    pVideoCapture->whitebalance = (128 << 16) + (128 << 8) + 128;
    ;
    pVideoCapture->tnf = 128;
    pVideoCapture->snf = 128;
    pVideoCapture->HLC = 0;
    pVideoCapture->backlight = 0;

    pVideoCapture->isp_mode_color = 0;
    pVideoCapture->isp_mode_night = 0;
    pVideoCapture->videoEncodeMode = 0;
    pVideoCapture->led_mode = LED_PURE_INFRAED;
    pVideoCapture->ispadvmode = LED_IMAGE_NORMAL;
    pVideoCapture->light_off_sensitivity = 40;
    pVideoCapture->aov_mode = 2;
    pVideoCapture->aov_fps = 1;

    return 0;
}

int anj_config_video_capture_get(IXML_Node *pNode, VideoCaptureCfg *pVideoCaptureCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pVideoCaptureCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Brightness"))
        {
            pVideoCaptureCfg->brightness = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Contrast"))
        {
            pVideoCaptureCfg->contrast = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Saturation"))
        {
            pVideoCaptureCfg->saturation = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sharpness"))
        {
            pVideoCaptureCfg->sharpness = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DayNight"))
        {
            pVideoCaptureCfg->daynight = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "TVSystem"))
        {
            pVideoCaptureCfg->tvsystem = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "forct_antiflicker"))
        {
            pVideoCaptureCfg->forct_antiflicker = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "cropxpix"))
        {
            pVideoCaptureCfg->cropxpix = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "cropypix"))
        {
            pVideoCaptureCfg->cropypix = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "HFlip"))
        {
            pVideoCaptureCfg->hflip = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "VFlip"))
        {
            pVideoCaptureCfg->vflip = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "rotate"))
        {
            pVideoCaptureCfg->rotate = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "WB_RGB"))
        {
            pVideoCaptureCfg->whitebalance = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "BackLight"))
        {
            pVideoCaptureCfg->backlight = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "HLC"))
        {
            pVideoCaptureCfg->HLC = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "TNF"))
        {
            pVideoCaptureCfg->tnf = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "SNF"))
        {
            pVideoCaptureCfg->snf = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "IrcutMode"))
        {
            pVideoCaptureCfg->ircut_mode = (IRCutMode)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "IrcutSensitivity"))
        {
            pVideoCaptureCfg->ircut_sensitivity = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "IrcutOpenLedDelay"))
        {
            pVideoCaptureCfg->ircut_openled_delay = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "led_brightness_mode"))
        {
            pVideoCaptureCfg->led_brightness_mode = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "led_brightness_value"))
        {
            pVideoCaptureCfg->led_brightness_value = (unsigned char)Str2Num(tmpAttr->nodeValue);
            if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD && pVideoCaptureCfg->led_brightness_value > 5)
            {
                pVideoCaptureCfg->led_brightness_value = 5;
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "led_brightness_alarm"))
        {
            pVideoCaptureCfg->led_brightness_alarm = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "IrcutNightStartTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pVideoCaptureCfg->ircut_nighttime.startTime));
        }
        else if (!strcmp(tmpAttr->nodeName, "IrcutNightEndTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pVideoCaptureCfg->ircut_nighttime.endTime));
        }
        else if (!strcmp(tmpAttr->nodeName, "IrcutKeepColor"))
        {
            pVideoCaptureCfg->ircut_keepcolor = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "bManualGain"))
        {
            pVideoCaptureCfg->bManualGain = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "gainValue"))
        {
            pVideoCaptureCfg->gainValue = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "WDRMode"))
        {
            pVideoCaptureCfg->wdr_mode = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "WDRValue"))
        {
            pVideoCaptureCfg->wdr_value = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DfrogFlag"))
        {
            pVideoCaptureCfg->dfrog_flag = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DfrogValue"))
        {
            pVideoCaptureCfg->dfrog_value = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "WDRStartTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pVideoCaptureCfg->wdr_worktime.startTime));
        }
        else if (!strcmp(tmpAttr->nodeName, "WDREndTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pVideoCaptureCfg->wdr_worktime.endTime));
        }
        else if (!strcmp(tmpAttr->nodeName, "shutter_mode"))
        {
            pVideoCaptureCfg->shutterSetting.shutter_mode_day = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "shutter_mode_night"))
        {
            pVideoCaptureCfg->shutterSetting.shutter_mode_night = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "shutter_speed_day"))
        {
            pVideoCaptureCfg->shutterSetting.shutter_speed_day = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "shutter_speed_night"))
        {
            pVideoCaptureCfg->shutterSetting.shutter_speed_night = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "isp_mode_color"))
        {
            pVideoCaptureCfg->isp_mode_color = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "isp_mode_night"))
        {
            pVideoCaptureCfg->isp_mode_night = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "videoEncodeMode"))
        {
            pVideoCaptureCfg->videoEncodeMode = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "led_mode"))
        {
            pVideoCaptureCfg->led_mode = (LedMode)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ispadvmode"))
        {
            pVideoCaptureCfg->ispadvmode = (LedImageMode)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "light_off_sensitivity"))
        {
            pVideoCaptureCfg->light_off_sensitivity = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "face_exposure_sensitivity"))
        {
            pVideoCaptureCfg->face_exposure_sensitivity = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "aov_mode"))
        {
            pVideoCaptureCfg->aov_mode = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "aov_fps"))
        {
            pVideoCaptureCfg->aov_fps = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "led_open"))
        {
            pVideoCaptureCfg->open_light = (unsigned short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "led_close"))
        {
            pVideoCaptureCfg->close_light = (unsigned short)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    IXML_Node *tmpChild = NULL;
    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "FishEyeCfg"))
        {
            anj_config_video_fisheye_get(tmpChild, &(pVideoCaptureCfg->fishEyeCfg));
        }

        tmpChild = tmpChild->nextSibling;
    }

endFunc:
    return iRet;
}

int anj_config_video_encode_get(IXML_Node *pNode, VideoEncode *pVideoEncode)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pVideoEncode != NULL, -1, "input Invalid");

    IXML_Node *tmpChild = NULL;
    IXML_Node *tmpAttr = NULL;
    int curEncodeIdx = 0;

    for (int iIndex = 0; iIndex < MAX_VENC_CHN; iIndex++)
    {
        pVideoEncode->encodeCfg[iIndex].enable = iIndex < 2 ? 1 : 0;           // 用于支持早期无此选项
        pVideoEncode->encodeCfg[iIndex].bitRateQuality = VIDEO_QUALITY_CUSTOM; // 用于支持早期无此选项

        pVideoEncode->encodeCfg[iIndex].qp.qp_enable = 0;
        pVideoEncode->encodeCfg[iIndex].qp.qp_min = 30;
        pVideoEncode->encodeCfg[iIndex].qp.qp_max = 51;

        pVideoEncode->encodeCfg[iIndex].lbrConfig.lbr_enable = 0;
        pVideoEncode->encodeCfg[iIndex].lbrConfig.lbr_style = 1;
        pVideoEncode->encodeCfg[iIndex].lbrConfig.lbr_bitratemode = 0;
        pVideoEncode->encodeCfg[iIndex].lbrConfig.lbr_bitrate = 0;
        pVideoEncode->encodeCfg[iIndex].lbrConfig.lbr_motionlevel = 2;
        pVideoEncode->encodeCfg[iIndex].lbrConfig.lbr_noicelevel = 0;
    }
    pVideoEncode->twoLensCfg.nOptimumDistance = 6000;
    pVideoEncode->encode_mode = -1;
    pVideoEncode->noice_level = -1;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "EncodeConfig"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (MAX_VENC_CHN <= curEncodeIdx)
                    break;

                if (!strcmp(tmpAttr->nodeName, "Stream"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].streamID = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Enable"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Resolution"))
                {
                    memset(pVideoEncode->encodeCfg[curEncodeIdx].resolution.name, '\0', RESOLUTION_NAME_MAX_LEN);
                    StrCpy(pVideoEncode->encodeCfg[curEncodeIdx].resolution.name, RESOLUTION_NAME_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "EncodeFormat"))
                {
                    memset(pVideoEncode->encodeCfg[curEncodeIdx].encodeFormat.name, '\0', RESOLUTION_NAME_MAX_LEN);
                    StrCpy(pVideoEncode->encodeCfg[curEncodeIdx].encodeFormat.name, RESOLUTION_NAME_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "BitRateControl"))
                {
                    memset(pVideoEncode->encodeCfg[curEncodeIdx].bitRateControl.name, '\0', RESOLUTION_NAME_MAX_LEN);
                    StrCpy(pVideoEncode->encodeCfg[curEncodeIdx].bitRateControl.name, RESOLUTION_NAME_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Initquant"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].initQuant = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "BitRateQuality"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].bitRateQuality = (VideoQualityEnum)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "qp_enable"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].qp.qp_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "qp_min"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].qp.qp_min = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "qp_max"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].qp.qp_max = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "BitRate"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].bitRate = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "FrameRate"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].display_frameRate =
                        pVideoEncode->encodeCfg[curEncodeIdx].frameRate = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lbr_enable"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].lbrConfig.lbr_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lbr_style"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].lbrConfig.lbr_style = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lbr_bitratemode"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].lbrConfig.lbr_bitratemode = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lbr_bitrate"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].lbrConfig.lbr_bitrate = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lbr_motionlevel"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].lbrConfig.lbr_motionlevel = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lbr_noicelevel"))
                {
                    pVideoEncode->encodeCfg[curEncodeIdx].lbrConfig.lbr_noicelevel = Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }

            curEncodeIdx++;
        }
        else if (!strcmp(tmpChild->nodeName, "AdvanceEncodeConfig"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "EncodeProfile"))
                {
                    pVideoEncode->encode_profile = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "DisablePrivateData"))
                {
                    pVideoEncode->disable_private_data = Str2Num(tmpAttr->nodeValue);
                    pVideoEncode->disable_private_data = 1;
                }
                else if (!strcmp(tmpAttr->nodeName, "EncMode"))
                {
                    pVideoEncode->encode_mode = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "NoiceLevel"))
                {
                    pVideoEncode->noice_level = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "ssvcEnable"))
                {
                    pVideoEncode->ssvcEnable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "TwoLensWorkMode"))
                {
                    pVideoEncode->twoLensCfg.eTwoLensWorkMode = (TwoLensWorkMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "OptimumDistance"))
                {
                    pVideoEncode->twoLensCfg.nOptimumDistance = Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

endFunc:
    return iRet;
}

static int anj_config_jpeg_encode_get(IXML_Node *pNode, JpegEncodeCfg *pJpegCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pJpegCfg != NULL, -1, "input Invalid");
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pJpegCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Quality"))
        {
            pJpegCfg->quality = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

endFunc:
    return iRet;
}
int anj_config_overlay_get(IXML_Node *pNode, VideoOverlay *pVideoOverlay)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pVideoOverlay != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;
    IXML_Node *childNode = NULL;

    char title_utf8[TITLE_MAX_LEN];
    char title[TITLE_MAX_LEN];

    memset(title_utf8, 0, TITLE_MAX_LEN);
    memset(title, 0, TITLE_MAX_LEN);
    pVideoOverlay->titleOverlay.title_utf8[0] = 0;
    int bfound_real_transparency = 0;

    pVideoOverlay->style = AJ_OVERLAY_STYLE_TRANSPARENT_WHITEBLACK;
    pVideoOverlay->fontsize = 0;
    pVideoOverlay->bDsplayWeek = 1;
    pVideoOverlay->time24or12 = 0;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pVideoOverlay->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Transparency"))
        {
            pVideoOverlay->transparency = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RealTransparency"))
        {
            pVideoOverlay->real_transparency = (short)Str2Num(tmpAttr->nodeValue);
            bfound_real_transparency = 1;
        }
        else if (!strcmp(tmpAttr->nodeName, "time24or12"))
        {
            pVideoOverlay->time24or12 = (short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ovlayfps"))
        {
            pVideoOverlay->bOverlayFps = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Fontsize"))
        {
            pVideoOverlay->fontsize = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Week"))
        {
            pVideoOverlay->bDsplayWeek = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Style"))
        {
            pVideoOverlay->style = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    childNode = pNode->firstChild;
    while (childNode)
    {
        if (!strcmp(childNode->nodeName, "TimeOverlay"))
        {
            pVideoOverlay->timeOverlay.posType = POSITION_TYPE_BY_FOUR_CORNER;
            tmpAttr = childNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "PosX"))
                {
                    pVideoOverlay->timeOverlay.posX = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "PosY"))
                {
                    pVideoOverlay->timeOverlay.posY = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Format"))
                {
                    memset(pVideoOverlay->timeOverlay.timeFormat.format, '\0', TIME_FORMAT_MAX_LEN);
                    StrCpy(pVideoOverlay->timeOverlay.timeFormat.format, TIME_FORMAT_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "PosType"))
                {
                    pVideoOverlay->timeOverlay.posType = (Positiontype)Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }
        else if (!strcmp(childNode->nodeName, "TitleOverlay"))
        {
            pVideoOverlay->titleOverlay.posType = POSITION_TYPE_BY_FOUR_CORNER;
            pVideoOverlay->titleOverlay.titleType = TYPE_TYPE_BY_TEXT;
            tmpAttr = childNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "PosX"))
                {
                    pVideoOverlay->titleOverlay.posX = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "PosY"))
                {
                    pVideoOverlay->titleOverlay.posY = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Title"))
                {
                    if (tmpAttr->nodeValue)
                        hex2ascii(tmpAttr->nodeValue, strlen(tmpAttr->nodeValue), title, TITLE_MAX_LEN);
                }
                else if (!strcmp(tmpAttr->nodeName, "TitleUtf8"))
                {
                    if (tmpAttr->nodeValue)
                    {
                        StrCpy(pVideoOverlay->titleOverlay.title_utf8, TITLE_MAX_LEN, tmpAttr->nodeValue);
                        hex2ascii(tmpAttr->nodeValue, strlen(tmpAttr->nodeValue), title_utf8, TITLE_MAX_LEN);
                    }
                }
                else if (!strcmp(tmpAttr->nodeName, "PosType"))
                {
                    pVideoOverlay->titleOverlay.posType = (Positiontype)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "TitleType"))
                {
                    pVideoOverlay->titleOverlay.titleType = (Titletype)Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }
        childNode = childNode->nextSibling;
    }

    if (pVideoOverlay->style < 0 || pVideoOverlay->style > AJ_OVERLAY_STYLE_INVERSE_COLOR)
        pVideoOverlay->style = AJ_OVERLAY_STYLE_TRANSPARENT_WHITEBLACK;

    if (bfound_real_transparency == 0)
    {
        if (pVideoOverlay->style == AJ_OVERLAY_STYLE_BLACK_WHITE ||
            pVideoOverlay->style == AJ_OVERLAY_STYLE_WHITE_BLACK)
        {
            pVideoOverlay->real_transparency = 25;
        }
        else
        {
            pVideoOverlay->real_transparency = 0;
        }
    }

    // 以UTF8项目为准
    if (strlen(title) > 0 && strlen(title_utf8) == 0)
    {
        gb2312_to_utf8(title, pVideoOverlay->titleOverlay.title_utf8);
    }
    else if (strlen(title_utf8) > 0)
    {
        if (pVideoOverlay->titleOverlay.titleType == TYPE_TYPE_BY_TEXT)
        {
            strncpy(pVideoOverlay->titleOverlay.title_utf8, title_utf8, TITLE_MAX_LEN);
        }
    }

endFunc:
    return iRet;
}

int anj_config_user_overlay_get(IXML_Node *pNode, VideoUserOverlay *pCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;
    IXML_Node *childNode = NULL;

    int iIndex = 0;

    childNode = pNode->firstChild;
    while (childNode)
    {
        if (iIndex >= MAX_USER_OSD_NUM)
            break;

        UserOSD *p = &pCfg->data[iIndex];
        memset(p, 0, sizeof(UserOSD));
        p->fontsize = 0;
        p->linegap = 0;
        p->posType = POSITION_TYPE_BY_SCALE;
        p->style = AJ_OVERLAY_STYLE_TRANSPARENT_WHITEBLACK;
        char title_utf8[TITLE_MAX_LEN] = "";
        memset(title_utf8, 0, TITLE_MAX_LEN);

        if (!strcmp(childNode->nodeName, "UserOSD"))
        {
            tmpAttr = childNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "enable"))
                {
                    p->enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "pos_xscale"))
                {
                    p->pos_xscale = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "pos_yscale"))
                {
                    p->pos_yscale = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "PosType"))
                {
                    p->posType = (Positiontype)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "color_front"))
                {
                    p->color_front = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "color_back"))
                {
                    p->color_back = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "style"))
                {
                    p->style = (short)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "transparency"))
                {
                    p->transparency = (short)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Fontsize"))
                {
                    p->fontsize = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "linegap"))
                {
                    p->linegap = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "titleType"))
                {
                    p->titleType = (Titletype)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "TitleUtf8"))
                {
                    if (tmpAttr->nodeValue)
                    {
                        StrCpy(title_utf8, TITLE_MAX_LEN, tmpAttr->nodeValue);
                    }
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }

        if (p->titleType == TYPE_TYPE_BY_BMP) // TEXT OSD才转换
        {
            StrCpy(p->title_utf8, TITLE_MAX_LEN, title_utf8);
        }
        else
        {
            hex2ascii(title_utf8, strlen(title_utf8), p->title_utf8, TITLE_MAX_LEN);
        }

        childNode = childNode->nextSibling;
        iIndex++;
    }

endFunc:
    return iRet;
}
int anj_config_video_mask_get(IXML_Node *pNode, VideoMaskConfig *pVideoMask)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pVideoMask != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;
    IXML_Node *childNode = NULL;

    childNode = pNode->firstChild;
    while (childNode)
    {
        if (!strcmp(childNode->nodeName, "MainStream"))
        {
            tmpAttr = childNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "Area1PosX"))
                {
                    pVideoMask->mainStreamMaskList[0].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area1PosY"))
                {
                    pVideoMask->mainStreamMaskList[0].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area1Width"))
                {
                    pVideoMask->mainStreamMaskList[0].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area1Height"))
                {
                    pVideoMask->mainStreamMaskList[0].height = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2PosX"))
                {
                    pVideoMask->mainStreamMaskList[1].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2PosY"))
                {
                    pVideoMask->mainStreamMaskList[1].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2Width"))
                {
                    pVideoMask->mainStreamMaskList[1].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2Height"))
                {
                    pVideoMask->mainStreamMaskList[1].height = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3PosX"))
                {
                    pVideoMask->mainStreamMaskList[2].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3PosY"))
                {
                    pVideoMask->mainStreamMaskList[2].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3Width"))
                {
                    pVideoMask->mainStreamMaskList[2].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3Height"))
                {
                    pVideoMask->mainStreamMaskList[2].height = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4PosX"))
                {
                    pVideoMask->mainStreamMaskList[3].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4PosY"))
                {
                    pVideoMask->mainStreamMaskList[3].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4Width"))
                {
                    pVideoMask->mainStreamMaskList[3].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4Height"))
                {
                    pVideoMask->mainStreamMaskList[3].height = Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }
        }
        else if (!strcmp(childNode->nodeName, "SubStream"))
        {
            tmpAttr = childNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "Area1PosX"))
                {
                    pVideoMask->subStreamMaskList[0].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area1PosY"))
                {
                    pVideoMask->subStreamMaskList[0].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area1Width"))
                {
                    pVideoMask->subStreamMaskList[0].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area1Height"))
                {
                    pVideoMask->subStreamMaskList[0].height = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2PosX"))
                {
                    pVideoMask->subStreamMaskList[1].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2PosY"))
                {
                    pVideoMask->subStreamMaskList[1].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2Width"))
                {
                    pVideoMask->subStreamMaskList[1].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area2Height"))
                {
                    pVideoMask->subStreamMaskList[1].height = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3PosX"))
                {
                    pVideoMask->subStreamMaskList[2].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3PosY"))
                {
                    pVideoMask->subStreamMaskList[2].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3Width"))
                {
                    pVideoMask->subStreamMaskList[2].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area3Height"))
                {
                    pVideoMask->subStreamMaskList[2].height = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4PosX"))
                {
                    pVideoMask->subStreamMaskList[3].xPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4PosY"))
                {
                    pVideoMask->subStreamMaskList[3].yPos = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4Width"))
                {
                    pVideoMask->subStreamMaskList[3].width = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "Area4Height"))
                {
                    pVideoMask->subStreamMaskList[3].height = Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }
        }
        childNode = childNode->nextSibling;
    }

endFunc:
    return iRet;
}

static int anj_config_video_roi_get(IXML_Node *pNode, VideoROI *pCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area1PosX"))
        {
            pCfg->roi[0].xPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area1PosY"))
        {
            pCfg->roi[0].yPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area1Width"))
        {
            pCfg->roi[0].width = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area1Height"))
        {
            pCfg->roi[0].height = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area2PosX"))
        {
            pCfg->roi[1].xPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area2PosY"))
        {
            pCfg->roi[1].yPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area2Width"))
        {
            pCfg->roi[1].width = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area2Height"))
        {
            pCfg->roi[1].height = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area3PosX"))
        {
            pCfg->roi[2].xPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area3PosY"))
        {
            pCfg->roi[2].yPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area3Width"))
        {
            pCfg->roi[2].width = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area3Height"))
        {
            pCfg->roi[2].height = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area4PosX"))
        {
            pCfg->roi[3].xPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area4PosY"))
        {
            pCfg->roi[3].yPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area4Width"))
        {
            pCfg->roi[3].width = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Area4Height"))
        {
            pCfg->roi[3].height = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

endFunc:
    return iRet;
}

static int anj_config_video_yuv_get(IXML_Node *pNode, YuvEncodeCfg *pCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Resolution"))
        {
            memset(pCfg->resolution.name, '\0', RESOLUTION_NAME_MAX_LEN);
            StrCpy(pCfg->resolution.name, RESOLUTION_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FrameRate"))
        {
            pCfg->frameRate = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Format"))
        {
            pCfg->format = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

endFunc:
    return iRet;
}

static int anj_config_video_get(IXML_Node *pNode, VideoConfig *pVideoCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pVideoCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpChild = pNode;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "Capture"))
        {
            anj_config_video_capture_defalut(&(pVideoCfg->videoCapture));
            anj_config_video_capture_get(tmpChild, &(pVideoCfg->videoCapture));
        }
        else if (!strcmp(tmpChild->nodeName, "Encode"))
        {
            anj_config_video_encode_get(tmpChild, &(pVideoCfg->videoEncode));
        }
        else if (!strcmp(tmpChild->nodeName, "JpegConfig"))
        {
            anj_config_jpeg_encode_get(tmpChild, &(pVideoCfg->jpegCfg));
        }
        else if (!strcmp(tmpChild->nodeName, "Overlay"))
        {
            anj_config_overlay_get(tmpChild, &(pVideoCfg->overlay));
        }
        else if (!strcmp(tmpChild->nodeName, "UserOverlay"))
        {
            anj_config_user_overlay_get(tmpChild, &(pVideoCfg->useroverlay));
        }
        else if (!strcmp(tmpChild->nodeName, "Mask"))
        {
            anj_config_video_mask_get(tmpChild, &(pVideoCfg->videoMask));
        }
        else if (!strcmp(tmpChild->nodeName, "ROI"))
        {
            anj_config_video_roi_get(tmpChild, &(pVideoCfg->roiCfg));
        }
        else if (!strcmp(tmpChild->nodeName, "YUV"))
        {
            anj_config_video_yuv_get(tmpChild, &(pVideoCfg->yuvCfg));
        }
        /*
        else if(!strcmp(tmpChild->nodeName, "Decode"))
        {
            tmpAttr = tmpChild->firstAttr;
            while(tmpAttr)
            {
                if(!strcmp(tmpAttr->nodeName, "Enable"))
                {
                    pVideoCfg->videoDecode.enable = Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }
        */

        tmpChild = tmpChild->nextSibling;
    }

endFunc:
    return iRet;
}
static int anj_config_audio_capture_get(IXML_Node *pNode, AudioCapture *pCaptureCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pCaptureCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;

    if (pCaptureCfg->channels != 1 && pCaptureCfg->channels != 2)
    {
        pCaptureCfg->channels = 1;
    }

    if (pCaptureCfg->samplerate != 8000 && pCaptureCfg->samplerate != 16000 && pCaptureCfg->samplerate != 32000 && pCaptureCfg->samplerate != 48000)
    {
        pCaptureCfg->samplerate = 8000;
    }

    pCaptureCfg->bitspersample = 16;
    pCaptureCfg->amplify = 1;
    pCaptureCfg->aec_enable = 0;
    pCaptureCfg->ra_answer = 0;
    pCaptureCfg->volume_play = 100;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Volume"))
        {
            pCaptureCfg->volume_capture = (short)Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "VolumePlay"))
        {
            pCaptureCfg->volume_play = (short)Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "Channels"))
        {
            pCaptureCfg->channels = Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "BitsPerSample"))
        {
            pCaptureCfg->bitspersample = Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "SampleRate"))
        {
            pCaptureCfg->samplerate = Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "amplify"))
        {
            pCaptureCfg->amplify = Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "ra_answer"))
        {
            pCaptureCfg->ra_answer = Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "aec_enable"))
        {
            pCaptureCfg->aec_enable = Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "mute_ptz_turn"))
        {
            pCaptureCfg->mute_ptz_turn = (short)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }
endFunc:
    return iRet;
}

static int anj_config_audio_encode_get(IXML_Node *pNode, AudioEncode *pEncodeCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pEncodeCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pEncodeCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "SampleRate"))
        {
            pEncodeCfg->sampleRate = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncodeType"))
        {
            memset(pEncodeCfg->audioEncodeType.typeName, '\0', AUDIO_ENCODE_TYPE_MAX_LEN);
            StrCpy(pEncodeCfg->audioEncodeType.typeName, AUDIO_ENCODE_TYPE_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "BitRate"))
        {
            pEncodeCfg->bitRate = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

endFunc:
    return iRet;
}

static int anj_config_audio_get(IXML_Node *pNode, AudioConfig *pAudioCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pAudioCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpChild = NULL;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "Capture"))
        {
            anj_config_audio_capture_get(tmpChild, &(pAudioCfg->audioCapture));
        }
        else if (!strcmp(tmpChild->nodeName, "Encode"))
        {
            anj_config_audio_encode_get(tmpChild, &(pAudioCfg->audioEncode));
        }
        /*
        else if(!strcmp(tmpChild->nodeName, "Decode"))
        {
            tmpAttr = tmpChild->firstAttr;
            while(tmpAttr)
            {
                if(!strcmp(tmpAttr->nodeName, "Enable"))
                {
                    pAudioCfg->audioDecode.enable = Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }

        }
        */

        tmpChild = tmpChild->nextSibling;
    }

endFunc:
    return iRet;
}

static void anj_config_media_overlay_check(VideoOverlay *pOverlay)
{
    if (pOverlay->fontsize < 0 || pOverlay->fontsize > 2)
    {
        pOverlay->fontsize = 0;
    }

    TimeOverlay *pTimeOverlay = &pOverlay->timeOverlay;

    if (pTimeOverlay->posType == POSITION_TYPE_BY_FOUR_CORNER)
    {
        if (pTimeOverlay->posX > 2)
        {
            pTimeOverlay->posX = 0;
        }
        if (pTimeOverlay->posY > 2)
        {
            pTimeOverlay->posY = 0;
        }
    }
    else if (pTimeOverlay->posType == POSITION_TYPE_BY_SCALE)
    {
        if (pTimeOverlay->posX > 100 || pTimeOverlay->posX < 0)
        {
            pTimeOverlay->posX = 0;
        }
        if (pTimeOverlay->posY > 100 || pTimeOverlay->posY < 0)
        {
            pTimeOverlay->posY = 0;
        }
    }

    TitleOverlay *pTitleOverlay = &pOverlay->titleOverlay;
    if (pTitleOverlay->posType == POSITION_TYPE_BY_FOUR_CORNER)
    {
        if (pTitleOverlay->posX > 2)
        {
            pTitleOverlay->posX = 0;
        }
        if (pTitleOverlay->posY > 2)
        {
            pTitleOverlay->posY = 0;
        }
    }
    else if (pTitleOverlay->posType == POSITION_TYPE_BY_SCALE)
    {
        if (pTitleOverlay->posX > 100 || pTitleOverlay->posX < 0)
        {
            pTitleOverlay->posX = 0;
        }
        if (pTitleOverlay->posY > 100 || pTitleOverlay->posY < 0)
        {
            pTitleOverlay->posY = 0;
        }
    }
}

static int anj_config_media_check(MediaConfig *pMediaCfg)
{
    int iRet = 0;
    ANJ_CHK(pMediaCfg != NULL, -1, "input Invalid");
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        VideoCaptureCfg *pVideoCaptureCfg = &pMediaCfg->videoConfig[cameraIndex].videoCapture;
        pVideoCaptureCfg->wdr_value = CHECK_IN_RANGE(pVideoCaptureCfg->wdr_value, 0, 255) ? pVideoCaptureCfg->wdr_value : 0;
        pVideoCaptureCfg->dfrog_flag = CHECK_IN_RANGE(pVideoCaptureCfg->dfrog_flag, 0, 1) ? pVideoCaptureCfg->dfrog_flag : 0;
        pVideoCaptureCfg->dfrog_value = CHECK_IN_RANGE(pVideoCaptureCfg->dfrog_value, 0, 255) ? pVideoCaptureCfg->dfrog_value : 0;
        pVideoCaptureCfg->shutterSetting.shutter_mode_day =
            CHECK_IN_RANGE(pVideoCaptureCfg->shutterSetting.shutter_mode_day, 0, 1) ? pVideoCaptureCfg->shutterSetting.shutter_mode_day : 0;
        pVideoCaptureCfg->shutterSetting.shutter_mode_night =
            CHECK_IN_RANGE(pVideoCaptureCfg->shutterSetting.shutter_mode_night, 0, 1) ? pVideoCaptureCfg->shutterSetting.shutter_mode_night : 0;
        pVideoCaptureCfg->shutterSetting.shutter_speed_day =
            CHECK_IN_RANGE(pVideoCaptureCfg->shutterSetting.shutter_speed_day, 10, 10000) ? pVideoCaptureCfg->shutterSetting.shutter_speed_day : 10;
        pVideoCaptureCfg->shutterSetting.shutter_speed_night =
            CHECK_IN_RANGE(pVideoCaptureCfg->shutterSetting.shutter_speed_night, 10, 10000) ? pVideoCaptureCfg->shutterSetting.shutter_speed_night : 10;
        pVideoCaptureCfg->ircut_keepcolor = 0;

        VideoEncode *pVideoEncode = &pMediaCfg->videoConfig[cameraIndex].videoEncode;
        if (pVideoEncode->encode_mode != 0 && pVideoEncode->encode_mode != 4)
        {
            pVideoEncode->encode_mode = 4;
        }
        pVideoEncode->noice_level = CHECK_IN_RANGE(pVideoEncode->noice_level, 0, 10) ? pVideoEncode->noice_level : 6;
    }

endFunc:
    return iRet;
}

static int anj_config_video_default(VideoConfig *pVideoCfg)
{
    int iRet = 0;
    ANJ_CHK(pVideoCfg != NULL, -1, "input Invalid");

    VideoCaptureCfg *pVideoCaptureCfg = &pVideoCfg->videoCapture;
    pVideoCaptureCfg->wdr_value = 128;
    pVideoCaptureCfg->dfrog_value = 128;
    pVideoCaptureCfg->shutterSetting.shutter_speed_day = 1000;
    pVideoCaptureCfg->shutterSetting.shutter_speed_night = 1000;
    pVideoCaptureCfg->whitebalance = (128 << 16) + (128 << 8) + 128;
    pVideoCaptureCfg->tnf = 128;
    pVideoCaptureCfg->snf = 128;
    pVideoCaptureCfg->led_mode = LED_PURE_INFRAED;
    pVideoCaptureCfg->ispadvmode = LED_IMAGE_NORMAL;
    pVideoCaptureCfg->light_off_sensitivity = 40;
    pVideoCaptureCfg->aov_mode = 2;
    pVideoCaptureCfg->aov_fps = 1;

    pVideoCaptureCfg->ircut_mode = IRCUT_Mode_Passive;
    pVideoCaptureCfg->ircut_sensitivity = 50;
    pVideoCaptureCfg->led_brightness_value = 10;
    pVideoCaptureCfg->ircut_nighttime.startTime.hour = 18;
    pVideoCaptureCfg->ircut_nighttime.endTime.hour = 8;

endFunc:
    return iRet;
}

static int anj_config_audio_default(AudioConfig *pAudioConfig)
{
    int iRet = 0;
    ANJ_CHK(pAudioConfig != NULL, -1, "input Invalid");

endFunc:
    return iRet;
}

char *anj_config_audio_encode_conver_xml(AudioEncode *pCfg)
{
    int maxSize = 512;
    char *pe;
    char *pb;
    char *buf;
    char escapeBuf[512];
    const char *encodeType = pCfg->audioEncodeType.typeName;

    /* G.711 等价于能力表中的 G.711U，统一输出以便 Web 匹配 CodeList */
    if (strcasecmp(encodeType, "G.711") == 0)
    {
        encodeType = "G.711U";
    }
    else if (strcasecmp(encodeType, "G711A") == 0 || strcasecmp(encodeType, "PCMA") == 0)
    {
        encodeType = "G.711A";
    }

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<Encode\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pCfg->enable);
    pb += snprintf(pb, pe - pb, "SampleRate=\"%d\"\r\n", pCfg->sampleRate);
    pb += snprintf(pb, pe - pb, "EncodeType=\"%s\"\r\n", copy_with_escape(escapeBuf, (char *)encodeType));
    pb += snprintf(pb, pe - pb, "BitRate=\"%d\"\r\n", pCfg->bitRate);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_audio_capture_conver_xml(AudioCapture *pCfg)
{
    int maxSize = 512;
    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<Capture\r\n");
    pb += snprintf(pb, pe - pb, "Channels=\"%d\"\r\n", pCfg->channels);
    pb += snprintf(pb, pe - pb, "BitsPerSample=\"%d\"\r\n", pCfg->bitspersample);
    pb += snprintf(pb, pe - pb, "SampleRate=\"%d\"\r\n", pCfg->samplerate);
    pb += snprintf(pb, pe - pb, "Volume=\"%d\"\r\n", pCfg->volume_capture);
    pb += snprintf(pb, pe - pb, "VolumePlay=\"%d\"\r\n", pCfg->volume_play);
    pb += snprintf(pb, pe - pb, "amplify=\"%d\"\r\n", pCfg->amplify);
    pb += snprintf(pb, pe - pb, "ra_answer=\"%d\"\r\n", pCfg->ra_answer);
    pb += snprintf(pb, pe - pb, "aec_enable=\"%d\"\r\n", pCfg->aec_enable);
    pb += snprintf(pb, pe - pb, "mute_ptz_turn=\"%d\"\r\n", pCfg->mute_ptz_turn);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_audio_conver_xml(AudioConfig *pAudioCfg)
{
    int initSize = 100;
    char *tmp = NULL;
    int incrSize = 0;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<Audio>\r\n");

    curPos = pb - buf;
    tmp = anj_config_audio_capture_conver_xml(&(pAudioCfg->audioCapture));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_audio_encode_conver_xml(&(pAudioCfg->audioEncode));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</Audio>\r\n");

    return buf;
}

char *anj_config_video_fisheye_conver_xml(FishEyeCfg *pCfg)
{
    int maxSize = 256;

    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<FishEyeCfg\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pCfg->enable);
    pb += snprintf(pb, pe - pb, "autocrop=\"%d\"\r\n", pCfg->autocrop);
    pb += snprintf(pb, pe - pb, "diameter_ppm=\"%d\"\r\n", pCfg->diameter_ppm);
    pb += snprintf(pb, pe - pb, "center_ppm_x=\"%d\"\r\n", pCfg->center_ppm_x);
    pb += snprintf(pb, pe - pb, "center_ppm_y=\"%d\"\r\n", pCfg->center_ppm_y);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_video_capture_conver_xml(VideoCaptureCfg *pCfg)
{
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize = 0;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<Capture\r\n");
    pb += snprintf(pb, pe - pb, "Brightness=\"%d\"\r\n", pCfg->brightness);
    pb += snprintf(pb, pe - pb, "Contrast=\"%d\"\r\n", pCfg->contrast);
    pb += snprintf(pb, pe - pb, "Saturation=\"%d\"\r\n", pCfg->saturation);
    pb += snprintf(pb, pe - pb, "Sharpness=\"%d\"\r\n", pCfg->sharpness);
    pb += snprintf(pb, pe - pb, "DayNight=\"%d\"\r\n", pCfg->daynight);

    pb += snprintf(pb, pe - pb, "TVSystem=\"%u\"\r\n", pCfg->tvsystem);
    pb += snprintf(pb, pe - pb, "forct_antiflicker=\"%u\"\r\n", pCfg->forct_antiflicker);
    pb += snprintf(pb, pe - pb, "cropxpix=\"%u\"\r\n", pCfg->cropxpix);
    pb += snprintf(pb, pe - pb, "cropypix=\"%u\"\r\n", pCfg->cropypix);

    pb += snprintf(pb, pe - pb, "HFlip=\"%d\"\r\n", pCfg->hflip);
    pb += snprintf(pb, pe - pb, "VFlip=\"%d\"\r\n", pCfg->vflip);
    pb += snprintf(pb, pe - pb, "rotate=\"%d\"\r\n", pCfg->rotate);

    pb += snprintf(pb, pe - pb, "WB_RGB=\"%d\"\r\n", pCfg->whitebalance);
    pb += snprintf(pb, pe - pb, "BackLight=\"%d\"\r\n", pCfg->backlight);
    pb += snprintf(pb, pe - pb, "HLC=\"%d\"\r\n", pCfg->HLC);
    pb += snprintf(pb, pe - pb, "TNF=\"%d\"\r\n", pCfg->tnf);
    pb += snprintf(pb, pe - pb, "SNF=\"%d\"\r\n", pCfg->snf);

    pb += snprintf(pb, pe - pb, "IrcutMode=\"%d\"\r\n", pCfg->ircut_mode);
    pb += snprintf(pb, pe - pb, "IrcutSensitivity=\"%u\"\r\n", pCfg->ircut_sensitivity);
    pb += snprintf(pb, pe - pb, "IrcutOpenLedDelay=\"%u\"\r\n", pCfg->ircut_openled_delay);
    pb += snprintf(pb, pe - pb, "led_brightness_mode=\"%u\"\r\n", pCfg->led_brightness_mode);
    pb += snprintf(pb, pe - pb, "led_brightness_value=\"%u\"\r\n", pCfg->led_brightness_value);
    pb += snprintf(pb, pe - pb, "led_brightness_alarm=\"%u\"\r\n", pCfg->led_brightness_alarm);
    pb += snprintf(pb, pe - pb, "IrcutNightStartTime=\"%02d:%02d:%02d\"\r\n",
                   pCfg->ircut_nighttime.startTime.hour,
                   pCfg->ircut_nighttime.startTime.minute,
                   pCfg->ircut_nighttime.startTime.sec);
    pb += snprintf(pb, pe - pb, "IrcutNightEndTime=\"%02d:%02d:%02d\"\r\n",
                   pCfg->ircut_nighttime.endTime.hour,
                   pCfg->ircut_nighttime.endTime.minute,
                   pCfg->ircut_nighttime.endTime.sec);
    pb += snprintf(pb, pe - pb, "IrcutKeepColor=\"%d\"\r\n", pCfg->ircut_keepcolor);
    pb += snprintf(pb, pe - pb, "led_mode=\"%d\"\r\n", pCfg->led_mode);     // cham 20171030
    pb += snprintf(pb, pe - pb, "ispadvmode=\"%d\"\r\n", pCfg->ispadvmode); // cham 20171030

    pb += snprintf(pb, pe - pb, "bManualGain=\"%d\"\r\n", pCfg->bManualGain);
    pb += snprintf(pb, pe - pb, "gainValue=\"%d\"\r\n", pCfg->gainValue);

    pb += snprintf(pb, pe - pb, "WDRMode=\"%d\"\r\n", pCfg->wdr_mode);

    pb += snprintf(pb, pe - pb, "WDRValue=\"%d\"\r\n", pCfg->wdr_value);
    pb += snprintf(pb, pe - pb, "DfrogFlag=\"%d\"\r\n", pCfg->dfrog_flag);
    pb += snprintf(pb, pe - pb, "DfrogValue=\"%d\"\r\n", pCfg->dfrog_value);

    pb += snprintf(pb, pe - pb, "WDRStartTime=\"%02d:%02d:%02d\"\r\n",
                   pCfg->wdr_worktime.startTime.hour,
                   pCfg->wdr_worktime.startTime.minute,
                   pCfg->wdr_worktime.startTime.sec);
    pb += snprintf(pb, pe - pb, "WDREndTime=\"%02d:%02d:%02d\"\r\n",
                   pCfg->wdr_worktime.endTime.hour,
                   pCfg->wdr_worktime.endTime.minute,
                   pCfg->wdr_worktime.endTime.sec);

    pb += snprintf(pb, pe - pb, "shutter_mode=\"%d\"\r\n", pCfg->shutterSetting.shutter_mode_day);
    pb += snprintf(pb, pe - pb, "shutter_mode_night=\"%d\"\r\n", pCfg->shutterSetting.shutter_mode_night);
    pb += snprintf(pb, pe - pb, "shutter_speed_day=\"%d\"\r\n", pCfg->shutterSetting.shutter_speed_day);
    pb += snprintf(pb, pe - pb, "shutter_speed_night=\"%d\"\r\n", pCfg->shutterSetting.shutter_speed_night);

    pb += snprintf(pb, pe - pb, "isp_mode_color=\"%d\"\r\n", pCfg->isp_mode_color);
    pb += snprintf(pb, pe - pb, "isp_mode_night=\"%d\"\r\n", pCfg->isp_mode_night);
    pb += snprintf(pb, pe - pb, "videoEncodeMode=\"%d\"\r\n", pCfg->videoEncodeMode);

    pb += snprintf(pb, pe - pb, "aov_mode=\"%d\"\r\n", pCfg->aov_mode);
    pb += snprintf(pb, pe - pb, "aov_fps=\"%d\"\r\n", pCfg->aov_fps);

    pb += snprintf(pb, pe - pb, "light_off_sensitivity=\"%u\"\r\n", pCfg->light_off_sensitivity);
    pb += snprintf(pb, pe - pb, "led_open=\"%u\"\r\n", pCfg->open_light);
    pb += snprintf(pb, pe - pb, "led_close=\"%u\"\r\n", pCfg->close_light);
    pb += snprintf(pb, pe - pb, "face_exposure_sensitivity=\"%u\">\r\n", pCfg->face_exposure_sensitivity);

    curPos = pb - buf;
    tmp = anj_config_video_fisheye_conver_xml(&(pCfg->fishEyeCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</Capture>\r\n");

    return buf;
}

char *anj_config_video_encode_conver_xml(VideoEncode *pCfg)
{
    int maxSize = 2000;
    char escapeBuf[2000];

    char *pe;
    char *pb;
    char *buf;
    int i;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<Encode>\r\n");
    for (i = 0; i < MAX_VENC_CHN; i++)
    {
        pb += snprintf(pb, pe - pb, "<EncodeConfig\r\n");
        pb += snprintf(pb, pe - pb, "Stream=\"%d\"\r\n", pCfg->encodeCfg[i].streamID);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pCfg->encodeCfg[i].enable);
        pb += snprintf(pb, pe - pb, "Resolution=\"%s\"\r\n", copy_with_escape(escapeBuf, pCfg->encodeCfg[i].resolution.name));
        pb += snprintf(pb, pe - pb, "EncodeFormat=\"%s\"\r\n", copy_with_escape(escapeBuf, pCfg->encodeCfg[i].encodeFormat.name));
        pb += snprintf(pb, pe - pb, "BitRateControl=\"%s\"\r\n", copy_with_escape(escapeBuf, pCfg->encodeCfg[i].bitRateControl.name));
        pb += snprintf(pb, pe - pb, "Initquant=\"%d\"\r\n", pCfg->encodeCfg[i].initQuant);
        pb += snprintf(pb, pe - pb, "BitRateQuality=\"%d\"\r\n", pCfg->encodeCfg[i].bitRateQuality);
        pb += snprintf(pb, pe - pb, "qp_enable=\"%d\"\r\n", pCfg->encodeCfg[i].qp.qp_enable);
        pb += snprintf(pb, pe - pb, "qp_min=\"%d\"\r\n", pCfg->encodeCfg[i].qp.qp_min);
        pb += snprintf(pb, pe - pb, "qp_max=\"%d\"\r\n", pCfg->encodeCfg[i].qp.qp_max);
        pb += snprintf(pb, pe - pb, "BitRate=\"%d\"\r\n", pCfg->encodeCfg[i].bitRate);
        pb += snprintf(pb, pe - pb, "FrameRate=\"%d\"\r\n", pCfg->encodeCfg[i].display_frameRate);

        pb += snprintf(pb, pe - pb, "lbr_enable=\"%d\"\r\n", pCfg->encodeCfg[i].lbrConfig.lbr_enable);
        pb += snprintf(pb, pe - pb, "lbr_style=\"%d\"\r\n", pCfg->encodeCfg[i].lbrConfig.lbr_style);
        pb += snprintf(pb, pe - pb, "lbr_bitratemode=\"%d\"\r\n", pCfg->encodeCfg[i].lbrConfig.lbr_bitratemode);
        pb += snprintf(pb, pe - pb, "lbr_bitrate=\"%d\"\r\n", pCfg->encodeCfg[i].lbrConfig.lbr_bitrate);
        pb += snprintf(pb, pe - pb, "lbr_motionlevel=\"%d\"\r\n", pCfg->encodeCfg[i].lbrConfig.lbr_motionlevel);
        pb += snprintf(pb, pe - pb, "lbr_noicelevel=\"%d\"\r\n", pCfg->encodeCfg[i].lbrConfig.lbr_noicelevel);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "<AdvanceEncodeConfig\r\n");
    pb += snprintf(pb, pe - pb, "EncodeProfile=\"%d\"\r\n", pCfg->encode_profile);

    pb += snprintf(pb, pe - pb, "DisablePrivateData=\"%d\"\r\n", pCfg->disable_private_data);

    pb += snprintf(pb, pe - pb, "EncMode=\"%d\"\r\n", pCfg->encode_mode);
    pb += snprintf(pb, pe - pb, "NoiceLevel=\"%d\"\r\n", pCfg->noice_level);
    pb += snprintf(pb, pe - pb, "ssvcEnable=\"%d\"\r\n", pCfg->ssvcEnable);
    pb += snprintf(pb, pe - pb, "TwoLensWorkMode=\"%d\"\r\n", pCfg->twoLensCfg.eTwoLensWorkMode);
    pb += snprintf(pb, pe - pb, "OptimumDistance=\"%d\"\r\n", pCfg->twoLensCfg.nOptimumDistance);

    pb += snprintf(pb, pe - pb, "/>\r\n");

    // added end
    pb += snprintf(pb, pe - pb, "</Encode>\r\n");
    return buf;
}

char *anj_config_jpeg_conver_xml(JpegEncodeCfg *pCfg)
{
    int maxSize = 2000;

    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<JpegConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pCfg->enable);
    pb += snprintf(pb, pe - pb, "Quality=\"%d\"\r\n", pCfg->quality);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_video_mask_conver_xml(VideoMaskConfig *pCfg)
{
    int maxSize = 2000;

    char *pe;
    char *pb;
    char *buf;
    int i;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<Mask>\r\n<MainStream \r\n");
    for (i = 0; i < MAX_VIDEO_MASK_AREA; i++)
    {
        pb += snprintf(pb, pe - pb, "Area%dPosX=\"%d\" ", i + 1, pCfg->mainStreamMaskList[i].xPos);
        pb += snprintf(pb, pe - pb, "Area%dPosY=\"%d\" ", i + 1, pCfg->mainStreamMaskList[i].yPos);
        pb += snprintf(pb, pe - pb, "Area%dWidth=\"%d\" ", i + 1, pCfg->mainStreamMaskList[i].width);
        pb += snprintf(pb, pe - pb, "Area%dHeight=\"%d\" ", i + 1, pCfg->mainStreamMaskList[i].height);
        pb += snprintf(pb, pe - pb, "\r\n");
    }
    pb += snprintf(pb, pe - pb, "/>\r\n<SubStream \r\n");
    for (i = 0; i < MAX_VIDEO_MASK_AREA; i++)
    {
        pb += snprintf(pb, pe - pb, "Area%dPosX=\"%d\" ", i + 1, pCfg->subStreamMaskList[i].xPos);
        pb += snprintf(pb, pe - pb, "Area%dPosY=\"%d\" ", i + 1, pCfg->subStreamMaskList[i].yPos);
        pb += snprintf(pb, pe - pb, "Area%dWidth=\"%d\" ", i + 1, pCfg->subStreamMaskList[i].width);
        pb += snprintf(pb, pe - pb, "Area%dHeight=\"%d\" ", i + 1, pCfg->subStreamMaskList[i].height);
        pb += snprintf(pb, pe - pb, "\r\n");
    }
    pb += snprintf(pb, pe - pb, "/>\r\n</Mask>\r\n");

    return buf;
}

char *anj_config_video_roi_conver_xml(VideoROI *pCfg)
{
    int maxSize = 2000;

    char *pe;
    char *pb;
    char *buf;
    int i;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<ROI Enable=\"%d\"\r\n", pCfg->enable);
    for (i = 0; i < MAX_VIDEO_MASK_AREA; i++)
    {
        pb += snprintf(pb, pe - pb, "Area%dPosX=\"%d\" ", i + 1, pCfg->roi[i].xPos);
        pb += snprintf(pb, pe - pb, "Area%dPosY=\"%d\" ", i + 1, pCfg->roi[i].yPos);
        pb += snprintf(pb, pe - pb, "Area%dWidth=\"%d\" ", i + 1, pCfg->roi[i].width);
        pb += snprintf(pb, pe - pb, "Area%dHeight=\"%d\" ", i + 1, pCfg->roi[i].height);
        pb += snprintf(pb, pe - pb, "\r\n");
    }
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_video_yuv_conver_xml(YuvEncodeCfg *pCfg)
{
    int maxSize = 2000;
    char escapeBuf[512];

    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<YUV Enable=\"%d\"\r\n", pCfg->enable);
    pb += snprintf(pb, pe - pb, "Resolution=\"%s\"\r\n", copy_with_escape(escapeBuf, pCfg->resolution.name));
    pb += snprintf(pb, pe - pb, "FrameRate=\"%d\"\r\n", pCfg->frameRate);
    pb += snprintf(pb, pe - pb, "Format=\"%d\"\r\n", pCfg->format);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_user_overlay_conver_xml(VideoUserOverlay *pCfg)
{
    int maxSize = 2000;

    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<UserOverlay>\r\n");

    for (int iIndex = 0; iIndex < MAX_USER_OSD_NUM; iIndex++)
    {
        UserOSD *p = &pCfg->data[iIndex];
        pb += snprintf(pb, pe - pb, "<UserOSD\r\n");
        pb += snprintf(pb, pe - pb, "enable=\"%d\"\r\n", p->enable);
        pb += snprintf(pb, pe - pb, "PosType=\"%d\"\r\n", p->posType);
        pb += snprintf(pb, pe - pb, "pos_xscale=\"%d\"\r\n", p->pos_xscale);
        pb += snprintf(pb, pe - pb, "pos_yscale=\"%d\"\r\n", p->pos_yscale);
        pb += snprintf(pb, pe - pb, "color_front=\"%d\"\r\n", p->color_front);
        pb += snprintf(pb, pe - pb, "color_back=\"%d\"\r\n", p->color_back);
        pb += snprintf(pb, pe - pb, "style=\"%d\"\r\n", p->style);
        pb += snprintf(pb, pe - pb, "transparency=\"%d\"\r\n", p->transparency);
        pb += snprintf(pb, pe - pb, "Fontsize=\"%d\"\r\n", p->fontsize);
        pb += snprintf(pb, pe - pb, "linegap=\"%d\"\r\n", p->linegap);
        pb += snprintf(pb, pe - pb, "titleType=\"%d\"\r\n", p->titleType);

        char title_hex[TITLE_MAX_LEN];
        memset(title_hex, 0, TITLE_MAX_LEN);
        hexdataTohexStr(p->title_utf8, strlen(p->title_utf8), title_hex, TITLE_MAX_LEN);

        if (p->titleType == TYPE_TYPE_BY_TEXT)
            pb += snprintf(pb, pe - pb, "TitleUtf8=\"%s\"\r\n", title_hex);
        else
            pb += snprintf(pb, pe - pb, "TitleUtf8=\"%s\"\r\n", p->title_utf8);

        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</UserOverlay>\r\n");
    return buf;
}

char *anj_config_overlay_conver_xml(VideoOverlay *pCfg)
{
    int maxSize = 2000;
    char escapeBuf[2000];

    char *pe;
    char *pb;
    char *buf;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<Overlay\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pCfg->enable);
    pb += snprintf(pb, pe - pb, "Transparency=\"%d\"\r\n", pCfg->transparency);
    pb += snprintf(pb, pe - pb, "RealTransparency=\"%d\"\r\n", pCfg->real_transparency);
    pb += snprintf(pb, pe - pb, "time24or12=\"%d\"\r\n", pCfg->time24or12);
    pb += snprintf(pb, pe - pb, "ovlayfps=\"%d\"\r\n", pCfg->bOverlayFps);
    pb += snprintf(pb, pe - pb, "Week=\"%d\"\r\n", pCfg->bDsplayWeek);
    pb += snprintf(pb, pe - pb, "Fontsize=\"%d\"\r\n", pCfg->fontsize);
    pb += snprintf(pb, pe - pb, "Style=\"%d\"\r\n", pCfg->style);
    pb += snprintf(pb, pe - pb, ">\r\n");

    pb += snprintf(pb, pe - pb, "<TimeOverlay\r\n");
    pb += snprintf(pb, pe - pb, "PosX=\"%d\"\r\n", pCfg->timeOverlay.posX);
    pb += snprintf(pb, pe - pb, "PosY=\"%d\"\r\n", pCfg->timeOverlay.posY);
    pb += snprintf(pb, pe - pb, "Format=\"%s\"\r\n", copy_with_escape(escapeBuf, pCfg->timeOverlay.timeFormat.format));
    pb += snprintf(pb, pe - pb, "PosType=\"%d\"\r\n", pCfg->timeOverlay.posType);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    pb += snprintf(pb, pe - pb, "<TitleOverlay\r\n");
    pb += snprintf(pb, pe - pb, "PosX=\"%d\"\r\n", pCfg->titleOverlay.posX);
    pb += snprintf(pb, pe - pb, "PosY=\"%d\"\r\n", pCfg->titleOverlay.posY);
    pb += snprintf(pb, pe - pb, "PosType=\"%d\"\r\n", pCfg->titleOverlay.posType);
    pb += snprintf(pb, pe - pb, "TitleType=\"%d\"\r\n", pCfg->titleOverlay.titleType);
#if 0   
    pb += snprintf(pb, pe-pb, "Title=\"%s\"\r\n", copy_with_escape(escapeBuf,pVideoCfg->overlay.titleOverlay.title));
#else
    if (pCfg->titleOverlay.titleType == TYPE_TYPE_BY_TEXT)
    {
        char title_hex[TITLE_MAX_LEN];
        memset(title_hex, 0, TITLE_MAX_LEN);
        hexdataTohexStr(pCfg->titleOverlay.title_utf8, strlen(pCfg->titleOverlay.title_utf8), title_hex, TITLE_MAX_LEN);

        pb += snprintf(pb, pe - pb, "TitleUtf8=\"%s\"\r\n", title_hex);
        char title[TITLE_MAX_LEN];
        utf8_to_gb2312(pCfg->titleOverlay.title_utf8, title);

        memset(title_hex, 0, TITLE_MAX_LEN);
        hexdataTohexStr(title, strlen(title), title_hex, TITLE_MAX_LEN);

        pb += snprintf(pb, pe - pb, "Title=\"%s\"\r\n", title_hex);
    }
    else
    {
        pb += snprintf(pb, pe - pb, "TitleUtf8=\"%s\"\r\n", pCfg->titleOverlay.title_utf8);
    }
#endif
    pb += snprintf(pb, pe - pb, "/>\r\n");
    pb += snprintf(pb, pe - pb, "</Overlay>\r\n");

    return buf;
}

char *anj_config_video_conver_xml(VideoConfig *pVideoCfgArray, int camera_index, int bMsg)
{
    int initSize = 100;
    char *tmp = NULL;
    int incrSize = 0;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<Video>\r\n");
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }
        VideoConfig *pVideoCfg = &pVideoCfgArray[cameraIndex];
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "<Camera id=\"%d\">\r\n", cameraIndex);

        curPos = pb - buf;
        tmp = anj_config_video_capture_conver_xml(&(pVideoCfg->videoCapture));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_video_encode_conver_xml(&(pVideoCfg->videoEncode));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_jpeg_conver_xml(&(pVideoCfg->jpegCfg));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_overlay_conver_xml(&(pVideoCfg->overlay));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_video_mask_conver_xml(&(pVideoCfg->videoMask));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_video_roi_conver_xml(&(pVideoCfg->roiCfg));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_user_overlay_conver_xml(&(pVideoCfg->useroverlay));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_video_yuv_conver_xml(&(pVideoCfg->yuvCfg));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "</Camera>\r\n");
    }
    pb += snprintf(pb, pe - pb, "</Video>\r\n");

    return buf;
}

char *anj_config_media_conver_xml(MediaConfig *pMediaCfg, int camera_index, int bMsg)
{
    int initSize = 100;
    char *tmp = NULL;
    int incrSize = 0;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<MediaConfig>\r\n");

    curPos = pb - buf;
    tmp = anj_config_video_conver_xml(pMediaCfg->videoConfig, camera_index, bMsg);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_audio_conver_xml(&(pMediaCfg->audioConfig));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</MediaConfig>\r\n");

    return buf;
}

void ShowMediaConfig(MediaConfig *pMediaCfg)
{
    char szPrintLine[512];

    sprintf(szPrintLine, "|%10s|%10s|%10s|%10s|%10s|%10s|"
                         "%10s|%10s|"
                         "%10s|%10s|%10s|%10s|%10s|%10s|",
            "stream", "enable", "format", "resolution", "bitrate", "framerate",
            "bitratectl", "initquant",
            "lbr_enable", "lbr_style", "lbr_bitmod", "lbr_bitrat", "lbr_motlvl", "lbr_noice");

    //__INFO(szPrintLine);
    printf("%s\n", szPrintLine);

    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        for (int iIndex = 0; iIndex < MAX_VENC_CHN; iIndex++)
        {
            VideoEncodeCfg *pEncode = &pMediaCfg->videoConfig[cameraIndex].videoEncode.encodeCfg[iIndex];
            sprintf(szPrintLine, "|%10d|%10d|%10s|%10s|%10d|%10d|"
                                 "%10s|%10d|"
                                 "%10d|%10d|%10d|%10d|%10d|%10d|",
                    iIndex,
                    pEncode->enable,
                    pEncode->encodeFormat.name,
                    pEncode->resolution.name,
                    pEncode->bitRate,
                    pEncode->frameRate,
                    pEncode->bitRateControl.name,
                    pEncode->initQuant,
                    pEncode->lbrConfig.lbr_enable,
                    pEncode->lbrConfig.lbr_style,
                    pEncode->lbrConfig.lbr_bitratemode,
                    pEncode->lbrConfig.lbr_bitrate,
                    pEncode->lbrConfig.lbr_motionlevel,
                    pEncode->lbrConfig.lbr_noicelevel);
            //__INFO(szPrintLine);
            printf("%s\n", szPrintLine);
        }
    }

    sprintf(szPrintLine, "|%10s|%10s|%10s|%10s|%10s"
                         "|%10s|%10s|%10s|%10s|%10s|\n",
            "audenable", "audiocode", "sampleRate", "bitrate", "channels",
            "volumn_c", "volumn_p", "amplify", "ra_answer", "aec_enable");
    //__INFO(szPrintLine);
    printf("%s\n", szPrintLine);
    sprintf(szPrintLine, "|%10d|%10s|%10d|%10d|%10d"
                         "|%10d|%10d|%10d|%10d|%10d|\n",
            pMediaCfg->audioConfig.audioEncode.enable,
            pMediaCfg->audioConfig.audioEncode.audioEncodeType.typeName,
            pMediaCfg->audioConfig.audioEncode.sampleRate,
            pMediaCfg->audioConfig.audioEncode.bitRate,
            pMediaCfg->audioConfig.audioCapture.channels,
            pMediaCfg->audioConfig.audioCapture.volume_capture,
            pMediaCfg->audioConfig.audioCapture.volume_play,
            pMediaCfg->audioConfig.audioCapture.amplify,
            pMediaCfg->audioConfig.audioCapture.ra_answer,
            pMediaCfg->audioConfig.audioCapture.aec_enable);
    //__INFO(szPrintLine);
    printf("%s\n", szPrintLine);

    sprintf(szPrintLine, "|%10s|%10s|%10s|%10s|%10s|"
                         "%10s|%10s|"
                         "%10s|%10s|%10s|%10s|%10s|%10s|",
            "profile", "wdr", "wdrvalue", "dfrogflag", "dfrogvalue",
            "encodemode", "noicelevel",
            "S-mode-D", "S-mode-NT", "S-D", "S-NT", "isp_color", "isp_night");
    //__INFO(szPrintLine);
    printf("%s\n", szPrintLine);

    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        sprintf(szPrintLine, "|%10d|%10d|%10d|%10d|%10d|"
                             "%10d|%10d|"
                             "%10d|%10d|%10d|%10d|%10d|%10d|",
                pMediaCfg->videoConfig[cameraIndex].videoEncode.encode_profile,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.wdr_mode,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.wdr_value,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.dfrog_flag,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.dfrog_value,
                pMediaCfg->videoConfig[cameraIndex].videoEncode.encode_mode,
                pMediaCfg->videoConfig[cameraIndex].videoEncode.noice_level,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.shutterSetting.shutter_mode_day,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.shutterSetting.shutter_mode_night,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.shutterSetting.shutter_speed_day,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.shutterSetting.shutter_speed_night,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.isp_mode_color,
                pMediaCfg->videoConfig[cameraIndex].videoCapture.isp_mode_night);

        //__INFO(szPrintLine);
        printf("%s\n", szPrintLine);

        VideoOverlay *pOverlayCfg = &pMediaCfg->videoConfig[cameraIndex].overlay;

        sprintf(szPrintLine, "system time type %d, value[%d %d]",
                pOverlayCfg->timeOverlay.posType, pOverlayCfg->timeOverlay.posX, pOverlayCfg->timeOverlay.posY);
        //__INFO(szPrintLine);
        printf("%s\n", szPrintLine);

        sprintf(szPrintLine, "system title type %d, value[%d %d]",
                pOverlayCfg->titleOverlay.posType, pOverlayCfg->titleOverlay.posX, pOverlayCfg->titleOverlay.posY);
        //__INFO(szPrintLine);
        printf("%s\n", szPrintLine);
    }
}

int anj_config_media_default(MediaConfig *pMediaCfg)
{
    int iRet = 0;
    ANJ_CHK(pMediaCfg != NULL, -1, "input Invalid");
    memset(pMediaCfg, 0, sizeof(MediaConfig));

    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        VideoConfig *pVideoCfg = &pMediaCfg->videoConfig[cameraIndex];
        anj_config_video_default(pVideoCfg);
    }
    AudioConfig *pAudioCfg = &pMediaCfg->audioConfig;
    anj_config_audio_default(pAudioCfg);

endFunc:
    return iRet;
}

int anj_config_media_get(IXML_Node *pNode, MediaConfig *pMediaCfg)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pMediaCfg != NULL, -1, "input Invalid");

    IXML_Node *tmpChild = pNode->firstChild;
    while (tmpChild != NULL)
    {
        if (!strcmp(tmpChild->nodeName, "Video"))
        {
            IXML_Node *pChildNode = tmpChild->firstChild;
            int camera_index = -1;
            while (pChildNode)
            {
                if (!strcmp(pChildNode->nodeName, "Camera"))
                {
                    IXML_Node *tmpAttr = pChildNode->firstAttr;
                    while (tmpAttr)
                    {
                        if (!strcmp(tmpAttr->nodeName, "id"))
                        {
                            camera_index = Str2Num(tmpAttr->nodeValue);
                        }
                        tmpAttr = tmpAttr->nextSibling;
                    }

                    anj_config_video_get(pChildNode->firstChild, &pMediaCfg->videoConfig[camera_index]);
                }
                pChildNode = pChildNode->nextSibling;
            }
            if (camera_index < 0)
            {
                for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
                {
                    anj_config_video_get(tmpChild->firstChild, &pMediaCfg->videoConfig[i]);
                }
            }
        }
        else if (!strcmp(tmpChild->nodeName, "Audio"))
        {
            anj_config_audio_get(tmpChild, &(pMediaCfg->audioConfig));
        }

        tmpChild = tmpChild->nextSibling;
    }

    anj_config_media_check(pMediaCfg);
    ShowMediaConfig(pMediaCfg);

endFunc:

    return iRet;
}

int anj_config_media_save(MediaConfig *pMediaCfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_media_conver_xml(pMediaCfg, -1, 0);
    iRet = anj_config_save_node(pDataXml, "<MediaConfig>", "</MediaConfig>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_media_set(MediaConfig *pstMediaConfig)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    if (memcmp(mediaCfg, pstMediaConfig, sizeof(MediaConfig)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(mediaCfg, pstMediaConfig, sizeof(MediaConfig));
        anj_config_media_save(pstMediaConfig);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_video_set(VideoConfig *pstVideoConfigArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    VideoConfig *videoConfigArray = mediaCfg->videoConfig;
    if (memcmp(videoConfigArray, pstVideoConfigArray, sizeof(VideoConfig) * ANJ_CAMERA_MAX_NUMS))
    {
        // todo ...
        memcpy(videoConfigArray, pstVideoConfigArray, sizeof(VideoConfig) * ANJ_CAMERA_MAX_NUMS);
        anj_config_media_save(mediaCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_video_capture_set(VideoCaptureCfg *pstVideoCapture, int cameraIndex)
{
    int iRet = 0;
    int needWdrRestart = 0;
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *videoCapture = &mediaCfg->videoConfig[cameraIndex].videoCapture;
    if (pstVideoCapture->led_mode != videoCapture->led_mode)
    {
        if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE)
        {
            pstVideoCapture->led_mode = LED_PURE_WHITE;
        }
        else if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_RED)
        {
            pstVideoCapture->led_mode = LED_PURE_INFRAED;
        }
    }
    if (memcmp(videoCapture, pstVideoCapture, sizeof(VideoCaptureCfg)))
    {
        if (pstVideoCapture->wdr_mode != videoCapture->wdr_mode)
        {
            if (VIDEO_WDR_MODE_HDR == pstVideoCapture->wdr_mode ||
                VIDEO_WDR_MODE_HDR == videoCapture->wdr_mode)
            {
                if (anj_video_restart_is_busy())
                {
                    pthread_rwlock_unlock(rwlock);
                    __ERR("video restart is running, reject wdr mode switch\n");
                    return -1;
                }
                needWdrRestart = 1;
            }
        }

        iRet = anj_ispctl_update_base_param(pstVideoCapture, cameraIndex);
        __INFO("set cameraIndex:%d video capture ret:%d!\n", cameraIndex, iRet);
        if (0 == iRet)
        {
            int ispctlSet = 0;
            if (pstVideoCapture->ircut_mode != videoCapture->ircut_mode ||
                pstVideoCapture->led_mode != videoCapture->led_mode)
            {
                ispctlSet = 1;
            }
            memcpy(videoCapture, pstVideoCapture, sizeof(VideoCaptureCfg));
            if (ispctlSet)
                anj_ispctl_config_set();
            iRet = anj_config_media_save(mediaCfg);
        }
        else
        {
            needWdrRestart = 0;
        }
    }
    pthread_rwlock_unlock(rwlock);

    if (0 == iRet && needWdrRestart)
    {
        anj_video_encode_switch();
    }

    return iRet;
}

int anj_config_overlay_set(VideoOverlay *pstVideoOverlay, int cameraIndex)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    VideoOverlay *videoOverlay = &mediaCfg->videoConfig[cameraIndex].overlay;
    if (memcmp(videoOverlay, pstVideoOverlay, sizeof(VideoOverlay)))
    {
        anj_config_media_overlay_check(pstVideoOverlay);
        memcpy(videoOverlay, pstVideoOverlay, sizeof(VideoOverlay));
        anj_config_media_save(mediaCfg);
        anj_osd_update_config();
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_user_overlay_set(VideoUserOverlay *pstVideoUserOverlay, int cameraIndex)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    VideoUserOverlay *videoUserOverlay = &mediaCfg->videoConfig[cameraIndex].useroverlay;
    if (memcmp(videoUserOverlay, pstVideoUserOverlay, sizeof(VideoUserOverlay)))
    {
        memcpy(videoUserOverlay, pstVideoUserOverlay, sizeof(VideoUserOverlay));
        anj_config_media_save(mediaCfg);
        anj_osd_update_config();
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_video_mask_set(VideoMaskConfig *pstVideoMask, int cameraIndex)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    VideoMaskConfig *videoMask = &mediaCfg->videoConfig[cameraIndex].videoMask;
    if (memcmp(videoMask, pstVideoMask, sizeof(VideoMaskConfig)))
    {
        __INFO("video mask change!\n");
        memcpy(videoMask, pstVideoMask, sizeof(VideoMaskConfig));
        if (0 == anj_osd_cover_set())
        {
            EventResult event_result = {0};
            PtzCmdParse stPtzCmdParse = {0};
            if (anj_osd_lens_cover_get())
            {
                strncpy(stPtzCmdParse.ptzCmd, "LensCoverOn", sizeof(stPtzCmdParse.ptzCmd) - 1);
            }
            else
            {
                strncpy(stPtzCmdParse.ptzCmd, "LensCoverOff", sizeof(stPtzCmdParse.ptzCmd) - 1);
            }
            //镜头遮挡 电机转动到最下方
            eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);
            anj_config_media_save(mediaCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_video_roi_set(VideoROI *pstVideoRoi, int cameraIndex)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    VideoROI *videoRoi = &mediaCfg->videoConfig[cameraIndex].roiCfg;
    if (memcmp(videoRoi, pstVideoRoi, sizeof(VideoROI)))
    {
        // todo ...
        memcpy(videoRoi, pstVideoRoi, sizeof(VideoROI));
        anj_config_media_save(mediaCfg);
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_video_yuv_set(YuvEncodeCfg *pstVideoYuv, int cameraIndex)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    YuvEncodeCfg *videoYuv = &mediaCfg->videoConfig[cameraIndex].yuvCfg;
    if (memcmp(videoYuv, pstVideoYuv, sizeof(YuvEncodeCfg)))
    {
        // SSTAR_359G使用：运行中只保存配置，等到重新初始化子码流scl时生效
        memcpy(videoYuv, pstVideoYuv, sizeof(YuvEncodeCfg));
        anj_config_media_save(mediaCfg);
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_video_encode_set(VideoEncode *pstVideoEncode, int cameraIndex)
{
    __WARN("video encode set stage 1 \n");

    int iNeedSwitch = 0; /* 分辨率/编码/RC/enable/twoLens/升帧率 → encode_switch */
    int iNeedSet = 0;    /* 仅 bitrate/降帧率/gop/qp/profile 热设 */

    for (int stream_no = 0; stream_no < MAX_VENC_CHN; stream_no++)
    {
        if (pstVideoEncode->encodeCfg[stream_no].display_frameRate !=
            pstVideoEncode->encodeCfg[stream_no].frameRate)
        {
            __INFO("sync display fps camera:%d stream:%d %d -> %d\n",
                   cameraIndex, stream_no,
                   pstVideoEncode->encodeCfg[stream_no].display_frameRate,
                   pstVideoEncode->encodeCfg[stream_no].frameRate);
            pstVideoEncode->encodeCfg[stream_no].display_frameRate =
                pstVideoEncode->encodeCfg[stream_no].frameRate;
        }
    }

    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    VideoEncode *videoEncode = &mediaCfg->videoConfig[cameraIndex].videoEncode;
    if (memcmp(videoEncode, pstVideoEncode, sizeof(VideoEncode)))
    {
        int stream_no = 0;
        do
        {
            for (stream_no = 0; stream_no < MAX_VENC_CHN; stream_no++)
            {
                VideoEncodeCfg *pstVideoEncodeCfg = &videoEncode->encodeCfg[stream_no];
                VideoEncodeCfg *pstNewVideoEncodeCfg = &pstVideoEncode->encodeCfg[stream_no];
                if (pstVideoEncodeCfg->enable != pstNewVideoEncodeCfg->enable ||
                    strcmp(pstVideoEncodeCfg->encodeFormat.name, pstNewVideoEncodeCfg->encodeFormat.name) != 0 ||
                    strcmp(pstVideoEncodeCfg->resolution.name, pstNewVideoEncodeCfg->resolution.name) != 0 ||
                    strcmp(pstVideoEncodeCfg->bitRateControl.name, pstNewVideoEncodeCfg->bitRateControl.name) != 0 ||
                    videoEncode->twoLensCfg.eTwoLensWorkMode != pstVideoEncode->twoLensCfg.eTwoLensWorkMode)
                {
                    __INFO("enable:%d %d  name:%s %s  res:%s %s rc:%s %s work:%d %d\n",
                           pstVideoEncodeCfg->enable, pstNewVideoEncodeCfg->enable,
                           pstVideoEncodeCfg->encodeFormat.name, pstNewVideoEncodeCfg->encodeFormat.name,
                           pstVideoEncodeCfg->resolution.name, pstNewVideoEncodeCfg->resolution.name,
                           pstVideoEncodeCfg->bitRateControl.name, pstNewVideoEncodeCfg->bitRateControl.name,
                           videoEncode->twoLensCfg.eTwoLensWorkMode, pstVideoEncode->twoLensCfg.eTwoLensWorkMode);
                    iNeedSwitch = 1;
                    break;
                }

                /* 升帧率需 switch（重建时序/缓冲）；降帧率可热设 */
                if (pstNewVideoEncodeCfg->frameRate > pstVideoEncodeCfg->frameRate)
                {
                    __INFO("fps up %d -> %d, need switch\n",
                           pstVideoEncodeCfg->frameRate, pstNewVideoEncodeCfg->frameRate);
                    iNeedSwitch = 1;
                    break;
                }

                if (pstNewVideoEncodeCfg->frameRate < pstVideoEncodeCfg->frameRate ||
                    pstVideoEncodeCfg->initQuant != pstNewVideoEncodeCfg->initQuant ||
                    pstVideoEncodeCfg->bitRate != pstNewVideoEncodeCfg->bitRate ||
                    pstVideoEncodeCfg->qp.qp_enable != pstNewVideoEncodeCfg->qp.qp_enable ||
                    pstVideoEncodeCfg->qp.qp_max != pstNewVideoEncodeCfg->qp.qp_max ||
                    pstVideoEncodeCfg->qp.qp_min != pstNewVideoEncodeCfg->qp.qp_min ||
                    videoEncode->encode_profile != pstVideoEncode->encode_profile)
                {
                    iNeedSet = 1;
                }
            }

            __WARN("video encode set stage 2 \n");
        } while (0);

        __WARN("video config change! NeedSet:%d, NeedSwitch:%d\n", iNeedSet, iNeedSwitch);

        /* switch 路径会 attr_init 全量刷新，不必再单独 set_config */
        if (1 == iNeedSet && 0 == iNeedSwitch)
        {
            int i = 0;
            AnjVencConfig stTmpVenCfg[MAX_VENC_CHN] = {0};
            for (i = 0; i < MAX_VENC_CHN; i++)
            {
                stTmpVenCfg[i].fps = pstVideoEncode->encodeCfg[i].frameRate;
                stTmpVenCfg[i].profile = pstVideoEncode->encode_profile;
                stTmpVenCfg[i].gop = pstVideoEncode->encodeCfg[i].initQuant;
                stTmpVenCfg[i].bitrate = pstVideoEncode->encodeCfg[i].bitRate;
                if (strcmp(pstVideoEncode->encodeCfg[i].bitRateControl.name, "CBR") == 0)
                {
                    stTmpVenCfg[i].rcMode = ANJ_VIDEO_CBR;
                }
                else if (strcmp(pstVideoEncode->encodeCfg[i].bitRateControl.name, "VBR") == 0)
                {
                    stTmpVenCfg[i].rcMode = ANJ_VIDEO_VBR;
                }
                else if (strcmp(pstVideoEncode->encodeCfg[i].bitRateControl.name, "AVBR") == 0)
                {
                    stTmpVenCfg[i].rcMode = ANJ_VIDEO_AVBR;
                }
                else if (strcmp(pstVideoEncode->encodeCfg[i].bitRateControl.name, "FIXQP") == 0)
                {
                    stTmpVenCfg[i].rcMode = ANJ_VIDEO_FIXQP;
                }
                stTmpVenCfg[i].qpenable = pstVideoEncode->encodeCfg[i].qp.qp_enable;
                stTmpVenCfg[i].maxqp = pstVideoEncode->encodeCfg[i].qp.qp_max;
                stTmpVenCfg[i].minqp = pstVideoEncode->encodeCfg[i].qp.qp_min;
            }

            anj_video_set_config((void *)&stTmpVenCfg);
        }

        memcpy(videoEncode, pstVideoEncode, sizeof(VideoEncode));
        anj_config_media_save(mediaCfg);
    }
    pthread_rwlock_unlock(rwlock);

    __WARN("video encode set stage 3 \n");

    return iNeedSwitch;
}

int anj_config_jpeg_encode_set(JpegEncodeCfg *pstVideoJpeg, int cameraIndex)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    JpegEncodeCfg *videoJpeg = &mediaCfg->videoConfig[cameraIndex].jpegCfg;
    if (memcmp(videoJpeg, pstVideoJpeg, sizeof(JpegEncodeCfg)))
    {
        // 直接保存配置，不需要其他响应
        memcpy(videoJpeg, pstVideoJpeg, sizeof(JpegEncodeCfg));
        anj_config_media_save(mediaCfg);
    }
    pthread_rwlock_unlock(rwlock);
    return 0;
}

int anj_config_audio_set(AudioConfig *pstAudioConfig)
{
    int iNeedRestart = 0;
    int iAudioTypeChange = 0;
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *audioConfig = &mediaCfg->audioConfig;
    if (memcmp(audioConfig, pstAudioConfig, sizeof(AudioConfig)))
    {
        // todo ...
        iNeedRestart = 1;
        if (strcmp(pstAudioConfig->audioEncode.audioEncodeType.typeName, 
            audioConfig->audioEncode.audioEncodeType.typeName) != 0)
        {
            iAudioTypeChange = 1;
        }

        memcpy(audioConfig, pstAudioConfig, sizeof(AudioConfig));
        anj_config_media_save(mediaCfg);
    }
    pthread_rwlock_unlock(rwlock);

    if (1 == iNeedRestart)
    {
        anj_audio_restart();
        anj_record_restart();
    }

    if (1 == iAudioTypeChange)
    {
        anj_service_audio_enc_change();
    }

    return 0;
}

int anj_config_audio_capture_set(AudioCapture *pstAudioCapture)
{
    int iRet = 0;
    int iNeedRestart = 0;

    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    AudioCapture *audioCapture = &mediaCfg->audioConfig.audioCapture;
    if (memcmp(audioCapture, pstAudioCapture, sizeof(AudioCapture)))
    {
        do
        {
            if (pstAudioCapture->channels != audioCapture->channels || pstAudioCapture->bitspersample != audioCapture->bitspersample || pstAudioCapture->samplerate != audioCapture->samplerate || pstAudioCapture->aec_enable != audioCapture->aec_enable)
            {
                iNeedRestart = 1;
                break;
            }

            if (pstAudioCapture->volume_play != audioCapture->volume_play)
            {
                iRet = anj_audio_ao_volume_set(pstAudioCapture->volume_play);
                if (iRet)
                {
                    break;
                }
            }

            if (pstAudioCapture->volume_capture != audioCapture->volume_capture || pstAudioCapture->amplify != audioCapture->amplify)
            {
                iRet = anj_audio_ai_volume_set(pstAudioCapture->volume_capture, pstAudioCapture->amplify);
                if (iRet)
                {
                    break;
                }
            }

            if (pstAudioCapture->ra_answer != audioCapture->ra_answer || pstAudioCapture->mute_ptz_turn != audioCapture->mute_ptz_turn)
            {
                ; // 只保存配置
            }

        } while (0);

        if (0 == iRet)
        {
            memcpy(audioCapture, pstAudioCapture, sizeof(AudioCapture));
            anj_config_media_save(mediaCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);

    // 保存完配置解锁后再重启音频
    if (1 == iNeedRestart)
    {
        anj_audio_restart();
        anj_record_restart();
    }

    return iRet;
}

int anj_config_audio_encode_set(AudioEncode *pstAudioEncode)
{
    int iRet = 0;
    int iNeedRestart = 0;
    int iAudioTypeChange = 0;
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    MediaConfig *mediaCfg = (MediaConfig *)getMediaConfig();
    AudioEncode *audioEncode = &mediaCfg->audioConfig.audioEncode;
    if (memcmp(audioEncode, pstAudioEncode, sizeof(AudioEncode)))
    {
        do
        {
            if (strcasecmp(pstAudioEncode->audioEncodeType.typeName, audioEncode->audioEncodeType.typeName) != 0)
            {
                iNeedRestart = 1;
                iAudioTypeChange = 1;
                break;
            }

            if (pstAudioEncode->enable != audioEncode->enable || pstAudioEncode->sampleRate != audioEncode->sampleRate)
            {
                iNeedRestart = 1;
                break;
            }

            if (pstAudioEncode->bitRate != audioEncode->bitRate)
            {
                ; // 只保存配置
            }

        } while (0);

        if (0 == iRet)
        {
            memcpy(audioEncode, pstAudioEncode, sizeof(AudioEncode));
            anj_config_media_save(mediaCfg);
        }
    }
    pthread_rwlock_unlock(rwlock);

    if (1 == iNeedRestart)
    {
        anj_audio_restart();
        anj_record_restart();
    }

    if (1 == iAudioTypeChange)
    {
        anj_service_audio_enc_change();
    }

    return iRet;
}

int anj_config_audio_capture_get_by_xml(AudioCapture *pCaptureCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Capture");
    if (pNodelist != NULL)
    {
        anj_config_audio_capture_get(pNodelist->nodeItem, pCaptureCfg);

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_audio_encode_get_by_xml(AudioEncode *pEncodeCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Encode");
    if (pNodelist != NULL)
    {
        anj_config_audio_encode_get(pNodelist->nodeItem, pEncodeCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_audio_get_by_xml(AudioConfig *pAudioCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Audio");
    if (pNodelist != NULL)
    {
        anj_config_audio_get(pNodelist->nodeItem, pAudioCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_jpeg_encode_get_by_xml(JpegEncodeCfg *pJpegCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "JpegConfig");
    if (pNodelist != NULL)
    {
        anj_config_jpeg_encode_get(pNodelist->nodeItem, pJpegCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_video_capture_get_by_xml(VideoCaptureCfg *pVideoCapture, char *xmlBuf, int MsgSrc)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Capture");
    if (pNodelist != NULL)
    {
        // MsgSrc=0(MSG_SRC_PRI) 才使用默认值，其他通道不使用默认值，直接覆盖
        if (MsgSrc == 0)
        {
            anj_config_video_capture_defalut(pVideoCapture);
        }
        anj_config_video_capture_get(pNodelist->nodeItem, pVideoCapture);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_video_encode_get_by_xml(VideoEncode *pVideoEncode, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Encode");
    if (pNodelist != NULL)
    {
        anj_config_video_encode_get(pNodelist->nodeItem, pVideoEncode);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_video_mask_get_by_xml(VideoMaskConfig *pVideoMask, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Mask");
    if (pNodelist != NULL)
    {
        anj_config_video_mask_get(pNodelist->nodeItem, pVideoMask);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_overlay_get_by_xml(VideoOverlay *pVideoOverlay, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Overlay");
    if (pNodelist != NULL)
    {
        anj_config_overlay_get(pNodelist->nodeItem, pVideoOverlay);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_video_roi_get_by_xml(VideoROI *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "ROI");
    if (pNodelist != NULL)
    {
        anj_config_video_roi_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_user_overlay_get_by_xml(VideoUserOverlay *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "UserOverlay");
    if (pNodelist != NULL)
    {
        anj_config_user_overlay_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_video_yuv_get_by_xml(YuvEncodeCfg *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "YUV");
    if (pNodelist != NULL)
    {
        anj_config_video_yuv_get(pNodelist->nodeItem, pCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_video_get_by_xml(VideoConfig *pVideoCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Video");
    if (pNodelist != NULL)
    {
        anj_config_video_get(pNodelist->nodeItem, pVideoCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_media_get_by_xml(MediaConfig *pMediaCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MediaConfig");
    if (pNodelist != NULL)
    {
        anj_config_media_get(pNodelist->nodeItem, pMediaCfg);
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        ixmlDocument_free(pDocNode);
        return -1;
    }

    return 0;
}

int anj_config_media_load(MediaConfig *pMediaCfg)
{
    return anj_config_load("MediaConfig", pMediaCfg, CONFIG_FILE_PATH);
}

char *anj_config_media_time_list_get(int index)
{
    return osd_time_fmt_list[index];
}

int anj_config_audio_param_get(media_codec_type_e *audio_type, int *samplerate, int *bitspersample, int *channels)
{
    int iRet = 0;
    ANJ_CHK((audio_type != NULL) && (samplerate != NULL) && (bitspersample != NULL) && (channels != NULL), -1, "input Invalid");

    MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
    AudioConfig *pstAudioConfig = &pstMediaConfig->audioConfig;
    if (!pstAudioConfig->audioEncode.enable)
    {
        iRet = -1;
    }
    else
    {
        *samplerate = pstAudioConfig->audioCapture.samplerate;
        *bitspersample = pstAudioConfig->audioCapture.bitspersample;
        *channels = pstAudioConfig->audioCapture.channels;
        *audio_type = audio_encode_type_get(pstAudioConfig->audioEncode.audioEncodeType.typeName);
    }
endFunc:
    return iRet;
}