/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include "sys_inc.h"
#include "onvif_image.h"
#include "anj_ispctl.h"
#ifdef IMAGE_SUPPORT

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern int g_onvif_expand;
extern int is_Y_Version;
/***************************************************************************************/

/**
 * @brief
 *  Sets the imaging settings for a video source on a device.
 *  A device indicating support for the AdaptablePreset capability shall 
 *  apply the same settings to the current active preset
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 *  ONVIF_ERR_SettingsInvalid
 */
ONVIF_RET onvif_img_SetImagingSettings(img_SetImagingSettings_REQ * p_req)
{
	onvif_ImagingSettings * p_setting;
	onvif_ImagingOptions * p_option;
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

	p_setting = &p_v_src->ImagingSettings;
	p_option = &p_v_src->ImagingOptions;

	/* valid param value */

	if (p_req->ImagingSettings.BacklightCompensationFlag && p_req->ImagingSettings.BacklightCompensation.LevelFlag && 
		(p_req->ImagingSettings.BacklightCompensation.Level - p_option->BacklightCompensation.Level.Min < -FPP || 
		 p_req->ImagingSettings.BacklightCompensation.Level - p_option->BacklightCompensation.Level.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.BrightnessFlag && 
		(p_req->ImagingSettings.Brightness - p_option->Brightness.Min < -FPP || 
		 p_req->ImagingSettings.Brightness - p_option->Brightness.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ColorSaturationFlag && 
		(p_req->ImagingSettings.ColorSaturation - p_option->ColorSaturation.Min < -FPP || 
		 p_req->ImagingSettings.ColorSaturation - p_option->ColorSaturation.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ContrastFlag && 
		(p_req->ImagingSettings.Contrast - p_option->Contrast.Min < -FPP || 
		 p_req->ImagingSettings.Contrast - p_option->Contrast.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	/*	
	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MinExposureTimeFlag && 
		(p_req->ImagingSettings.Exposure.MinExposureTime - p_option->Exposure.MinExposureTime.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MinExposureTime - p_option->Exposure.MinExposureTime.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MaxExposureTimeFlag && 
		(p_req->ImagingSettings.Exposure.MaxExposureTime - p_option->Exposure.MaxExposureTime.Min < -FPP|| 
		 p_req->ImagingSettings.Exposure.MaxExposureTime - p_option->Exposure.MaxExposureTime.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}
	*/

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MinGainFlag && 
		(p_req->ImagingSettings.Exposure.MinGain - p_option->Exposure.MinGain.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MinGain - p_option->Exposure.MinGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MaxGainFlag && 
		(p_req->ImagingSettings.Exposure.MaxGain - p_option->Exposure.MaxGain.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MaxGain - p_option->Exposure.MaxGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MinIrisFlag && 
		(p_req->ImagingSettings.Exposure.MinIris - p_option->Exposure.MinIris.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MinIris - p_option->Exposure.MinIris.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.MaxIrisFlag && 
		(p_req->ImagingSettings.Exposure.MaxIris - p_option->Exposure.MaxIris.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.MaxIris - p_option->Exposure.MaxIris.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	/*
	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.GainFlag && 
		(p_req->ImagingSettings.Exposure.Gain - p_option->Exposure.Gain.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.Gain - p_option->Exposure.Gain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}
	*/

	if (p_req->ImagingSettings.ExposureFlag && p_req->ImagingSettings.Exposure.IrisFlag && 
		(p_req->ImagingSettings.Exposure.Iris - p_option->Exposure.Iris.Min < -FPP || 
		 p_req->ImagingSettings.Exposure.Iris - p_option->Exposure.Iris.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}
	
	if (p_req->ImagingSettings.SharpnessFlag && 
		(p_req->ImagingSettings.Sharpness - p_option->Sharpness.Min < -FPP || 
		 p_req->ImagingSettings.Sharpness - p_option->Sharpness.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.WideDynamicRangeFlag && p_req->ImagingSettings.WideDynamicRange.LevelFlag && 
		(p_req->ImagingSettings.WideDynamicRange.Level - p_option->WideDynamicRange.Level.Min < -FPP || 
		 p_req->ImagingSettings.WideDynamicRange.Level - p_option->WideDynamicRange.Level.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.WhiteBalanceFlag && p_req->ImagingSettings.WhiteBalance.CrGainFlag && 
		(p_req->ImagingSettings.WhiteBalance.CrGain - p_option->WhiteBalance.YrGain.Min < -FPP || 
		 p_req->ImagingSettings.WhiteBalance.CrGain - p_option->WhiteBalance.YrGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}

	if (p_req->ImagingSettings.WhiteBalanceFlag && p_req->ImagingSettings.WhiteBalance.CbGainFlag && 
		(p_req->ImagingSettings.WhiteBalance.CbGain - p_option->WhiteBalance.YbGain.Min < -FPP || 
		 p_req->ImagingSettings.WhiteBalance.CbGain - p_option->WhiteBalance.YbGain.Max > FPP))
	{
		return ONVIF_ERR_SettingsInvalid;
	}
	
	// todo : add the image setting code ...
	
	if (1)
	{
		VideoCaptureCfg pVideoCap;
		memset(&pVideoCap, 0, sizeof(VideoCaptureCfg));
		MediaConfig *pTmpMediaCfg = (MediaConfig *)getMediaConfig();
		memcpy(&pVideoCap, &pTmpMediaCfg->videoConfig[0].videoCapture, sizeof(VideoCaptureCfg));

		pVideoCap.brightness = p_req->ImagingSettings.Brightness;
		pVideoCap.saturation = p_req->ImagingSettings.ColorSaturation;
		pVideoCap.contrast	= p_req->ImagingSettings.Contrast;
		pVideoCap.sharpness	= p_req->ImagingSettings.Sharpness;

		if (1)
		{
			if (p_req->ImagingSettings.IrCutFilter == IrCutFilterMode_AUTO)
			{
				pVideoCap.ircut_keepcolor = 0;
				pVideoCap.ircut_mode = IRCUT_Mode_Active; // 使用软光敏自动控制
			}
			else
			{
				if(p_req->ImagingSettings.IrCutFilter == IrCutFilterMode_OFF)//OFF黑夜
				{
					pVideoCap.ircut_keepcolor = 0;
					pVideoCap.ircut_mode = IRCUT_Mode_Manual;
				}
				else//OFF白天 IrCutFilterMode_ON
				{
					pVideoCap.ircut_keepcolor = 1;
					pVideoCap.ircut_mode = IRCUT_Mode_Manual;
				}
			}
		}

		if (0)
		{
			//p_setting->Focus.AutoFocusMode = p_req->ImagingSettings.Focus.AutoFocusMode;
			//p_setting->Focus.DefaultSpeed = p_req->ImagingSettings.Focus.DefaultSpeed;
			//p_setting->Focus.NearLimit = p_req->ImagingSettings.Focus.NearLimit;
			//p_setting->Focus.FarLimit = p_req->ImagingSettings.Focus.FarLimit;
		}

		if (p_req->ImagingSettings.WideDynamicRangeFlag)
		{
			/*设置宽动态WDR*/
			if(p_req->ImagingSettings.WideDynamicRange.Mode >= 0)
			{
				pVideoCap.wdr_mode = (p_req->ImagingSettings.WideDynamicRange.Mode > 0) ? 1 : 0;
#if !PLATFORM_DM365
				if((p_req->ImagingSettings.WideDynamicRange.Level >= 0))
					pVideoCap.wdr_value = (p_req->ImagingSettings.WideDynamicRange.Level > 255) ? 255 : p_req->ImagingSettings.WideDynamicRange.Level;
#endif		
			}
		}
		if (p_req->ImagingSettings.BacklightCompensationFlag)
		{
			/*设置逆光补偿*/
			pVideoCap.backlight = p_req->ImagingSettings.BacklightCompensation.Level;
		}
	
		if (p_req->ImagingSettings.WhiteBalanceFlag)
		{
			if (p_req->ImagingSettings.WhiteBalance.Mode == WhiteBalanceMode_AUTO)
			{
				pVideoCap.whitebalance &= ~(0xff<<24);
			}
			else
			{
				float CrGain_tmp = 0;
				float CbGain_tmp = 0;
				if(p_req->ImagingSettings.WhiteBalance.CrGain < 0)
					CrGain_tmp = 0;
				else if(p_req->ImagingSettings.WhiteBalance.CrGain > 255)
					CrGain_tmp = 255;
				else
					CrGain_tmp = p_req->ImagingSettings.WhiteBalance.CrGain;
				
				if(p_req->ImagingSettings.WhiteBalance.CbGain < 0)
					CbGain_tmp = 0;
				else if(p_req->ImagingSettings.WhiteBalance.CbGain > 255)
					CbGain_tmp = 255;
				else
					CbGain_tmp = p_req->ImagingSettings.WhiteBalance.CbGain;
				
				pVideoCap.whitebalance &= ~(0xff << 24 | 0xff << 16 | 0xff);
				pVideoCap.whitebalance |= (1 << 24);
				pVideoCap.whitebalance |= ((unsigned char)CrGain_tmp << 16);
				pVideoCap.whitebalance |= ((unsigned char)CbGain_tmp);
			}
		}
		if (p_req->ImagingSettings.ExposureFlag)
		{
			pVideoCap.shutterSetting.shutter_mode_day = p_req->ImagingSettings.Exposure.Mode;
			if (p_req->ImagingSettings.Exposure.ExposureTimeFlag)
			{
				if(p_req->ImagingSettings.Exposure.Mode != 0 && (p_req->ImagingSettings.Exposure.ExposureTime >= 10 && p_req->ImagingSettings.Exposure.ExposureTime <= 10000))
					pVideoCap.shutterSetting.shutter_speed_day = p_req->ImagingSettings.Exposure.ExposureTime;
			}
		}
		if (p_req->ImagingSettings.ExtensionFlag)
		{
			if (p_req->ImagingSettings.Extension.Extension202Flag)
			{
				if (p_req->ImagingSettings.Extension.Extension.Extension203Flag)
				{
					if (p_req->ImagingSettings.Extension.Extension.Extension.DefoggingFlag)
					{
						pVideoCap.dfrog_flag = p_req->ImagingSettings.Extension.Extension.Extension.Defogging.Mode != 0 ? 1 : 0;
						if(pVideoCap.dfrog_flag != 0)
							pVideoCap.dfrog_value = (int)(p_req->ImagingSettings.Extension.Extension.Extension.Defogging.Level*255);
					}
					if (p_req->ImagingSettings.Extension.Extension.Extension.NoiseReductionFlag)
					{
						pVideoCap.tnf = p_req->ImagingSettings.Extension.Extension.Extension.NoiseReduction.Level*255;
					}
				}
			}
		}
		
		anj_config_video_capture_set(&pVideoCap, 0);
		if (pVideoCap.ircut_mode == IRCUT_Mode_Manual)
		{
			anj_ispctl_ircut_manual_ctrl(pVideoCap.ircut_keepcolor, 0);
		}
	}

	// save image setting

	if (p_req->ImagingSettings.BacklightCompensationFlag)
	{
		p_setting->BacklightCompensation.Mode = p_req->ImagingSettings.BacklightCompensation.Mode;
		
		if (p_req->ImagingSettings.BacklightCompensation.LevelFlag)
		{
			p_setting->BacklightCompensation.Level = p_req->ImagingSettings.BacklightCompensation.Level;
		}
	}

	if (p_req->ImagingSettings.BrightnessFlag)
	{
		p_setting->Brightness = p_req->ImagingSettings.Brightness;
	}

	if (p_req->ImagingSettings.ColorSaturationFlag)
	{
		p_setting->ColorSaturation = p_req->ImagingSettings.ColorSaturation;
	}

	if (p_req->ImagingSettings.ContrastFlag)
	{
		p_setting->Contrast = p_req->ImagingSettings.Contrast;
	}

	if (p_req->ImagingSettings.SharpnessFlag)
	{
		p_setting->Sharpness = p_req->ImagingSettings.Sharpness;
	}

	if (p_req->ImagingSettings.IrCutFilterFlag)
	{
		if(access("/opt/ch/onvif_set_image_ircolors", F_OK) == 0)
		{
			log_print(HT_LOG_INFO, "mamual control ircut mode control led!\n");
		}
		else if ((access("/opt/ch/onvif_set_image_ircolors", F_OK) == 0) || is_Y_Version)
		{
			if (is_Y_Version)
			{
				VideoCaptureCfg pVideoCap;
				memset(&pVideoCap, 0, sizeof(VideoCaptureCfg));
				MediaConfig *pTmpMediaCfg = (MediaConfig *)getMediaConfig();
				memcpy(&pVideoCap, &pTmpMediaCfg->videoConfig[0].videoCapture, sizeof(VideoCaptureCfg));
				log_print(HT_LOG_INFO, "is_Y_Version != NULL\n");
				if (p_req->ImagingSettings.IrCutFilter == 0)
				{
					pVideoCap.ircut_mode = IRCUT_Mode_Manual;
					anj_config_video_capture_set(&pVideoCap, 0);
				}
				else if(p_req->ImagingSettings.IrCutFilter == 1)
				{
					pVideoCap.ircut_mode = IRCUT_Mode_Manual;
					anj_config_video_capture_set(&pVideoCap, 0);
				}
				else if(p_req->ImagingSettings.IrCutFilter == 2)
				{
					pVideoCap.ircut_mode = IRCUT_Mode_Active;
					pVideoCap.ircut_keepcolor = 0;
					anj_config_video_capture_set(&pVideoCap, 0);
				}
				
				
			}
			else
			{
				VideoCaptureCfg pVideoCap;
				memset(&pVideoCap, 0, sizeof(VideoCaptureCfg));
				MediaConfig *pTmpMediaCfg = (MediaConfig *)getMediaConfig();
				memcpy(&pVideoCap, &pTmpMediaCfg->videoConfig[0].videoCapture, sizeof(VideoCaptureCfg));
				if (p_req->ImagingSettings.IrCutFilter == 0)
				{
					pVideoCap.ircut_mode = IRCUT_Mode_Manual;
					pVideoCap.led_mode = 1;
					pVideoCap.ircut_keepcolor = IRCUT_Mode_Manual;
					anj_config_video_capture_set(&pVideoCap, 0);
				}
				else if(p_req->ImagingSettings.IrCutFilter == 1)
				{
					pVideoCap.ircut_mode = IRCUT_Mode_Manual;
					pVideoCap.led_mode = 0;
					pVideoCap.ircut_keepcolor = 0;
					anj_config_video_capture_set(&pVideoCap, 0);
				}
				else if(p_req->ImagingSettings.IrCutFilter == 2)
				{
					pVideoCap.ircut_mode = IRCUT_Mode_Passive;
					pVideoCap.ircut_keepcolor = 0;
					anj_config_video_capture_set(&pVideoCap, 0);
				}
			}
		}
		if (g_onvif_expand)
		{
			VideoCaptureCfg pVideoCap;
			memset(&pVideoCap, 0, sizeof(VideoCaptureCfg));
			MediaConfig *pTmpMediaCfg = (MediaConfig *)getMediaConfig();
			memcpy(&pVideoCap, &pTmpMediaCfg->videoConfig[0].videoCapture, sizeof(VideoCaptureCfg));
			if (p_req->ImagingSettings.IrCutFilter == 0)
			{
				system("rm /tmp/g_onvif_expand_ircut_off");
				system("touch /tmp/g_onvif_expand_ircut_on");
				pVideoCap.ircut_mode = IRCUT_Mode_Manual;
				anj_config_video_capture_set(&pVideoCap, 0);
				// MsgSetIRCUTControl(1); // todo
			}
			else if(p_req->ImagingSettings.IrCutFilter == 1)
			{
				system("rm /tmp/g_onvif_expand_ircut_on");
				system("touch /tmp/g_onvif_expand_ircut_off");
				pVideoCap.ircut_mode = IRCUT_Mode_Manual;
				anj_config_video_capture_set(&pVideoCap, 0);
				// MsgSetIRCUTControl(0); // todo
			}
			else if(p_req->ImagingSettings.IrCutFilter == 2)
			{
				system("rm /tmp/g_onvif_expand_ircut_on");
				system("rm /tmp/g_onvif_expand_ircut_off");
				pVideoCap.ircut_mode = IRCUT_Mode_Active;
				pVideoCap.ircut_keepcolor = 0;
				anj_config_video_capture_set(&pVideoCap, 0);
			}
		}
		p_setting->IrCutFilter = p_req->ImagingSettings.IrCutFilter;
	}
	
	if (p_req->ImagingSettings.ExposureFlag)
	{
		p_setting->Exposure.Mode = p_req->ImagingSettings.Exposure.Mode;

		if (p_req->ImagingSettings.Exposure.PriorityFlag)
		{
			p_setting->Exposure.Priority = p_req->ImagingSettings.Exposure.Priority;
		}

		if (p_req->ImagingSettings.Exposure.MinExposureTimeFlag)
		{
			p_setting->Exposure.MinExposureTime = p_req->ImagingSettings.Exposure.MinExposureTime;
		}

		if (p_req->ImagingSettings.Exposure.MaxExposureTimeFlag)
		{
			p_setting->Exposure.MaxExposureTime = p_req->ImagingSettings.Exposure.MaxExposureTime;
		}

		if (p_req->ImagingSettings.Exposure.MinGainFlag)
		{
			p_setting->Exposure.MinGain = p_req->ImagingSettings.Exposure.MinGain;
		}

		if (p_req->ImagingSettings.Exposure.MaxGainFlag)
		{
			p_setting->Exposure.MaxGain = p_req->ImagingSettings.Exposure.MaxGain;
		}

		if (p_req->ImagingSettings.Exposure.MinIrisFlag)
		{
			p_setting->Exposure.MinIris = p_req->ImagingSettings.Exposure.MinIris;
		}

		if (p_req->ImagingSettings.Exposure.MaxIrisFlag)
		{
			p_setting->Exposure.MaxIris = p_req->ImagingSettings.Exposure.MaxIris;
		}

		if (p_req->ImagingSettings.Exposure.ExposureTimeFlag)
		{
			p_setting->Exposure.ExposureTime = p_req->ImagingSettings.Exposure.ExposureTime;
		}

		if (p_req->ImagingSettings.Exposure.GainFlag)
		{
			p_setting->Exposure.Gain = p_req->ImagingSettings.Exposure.Gain;
		}

		if (p_req->ImagingSettings.Exposure.IrisFlag)
		{
			p_setting->Exposure.Iris = p_req->ImagingSettings.Exposure.Iris;
		}
	}

	if (p_req->ImagingSettings.FocusFlag)
	{
		p_setting->Focus.AutoFocusMode = p_req->ImagingSettings.Focus.AutoFocusMode;

		if (p_req->ImagingSettings.Focus.DefaultSpeedFlag)
		{
			p_setting->Focus.DefaultSpeed = p_req->ImagingSettings.Focus.DefaultSpeed;
		}

		if (p_req->ImagingSettings.Focus.NearLimitFlag)
		{
			p_setting->Focus.NearLimit = p_req->ImagingSettings.Focus.NearLimit;
		}

		if (p_req->ImagingSettings.Focus.FarLimitFlag)
		{
			p_setting->Focus.FarLimit = p_req->ImagingSettings.Focus.FarLimit;
		}
	}
	
	if (p_req->ImagingSettings.WideDynamicRangeFlag)
	{
		p_setting->WideDynamicRange.Mode = p_req->ImagingSettings.WideDynamicRange.Mode;

		if (p_req->ImagingSettings.WideDynamicRange.LevelFlag)
		{
			p_setting->WideDynamicRange.Level = p_req->ImagingSettings.WideDynamicRange.Level;
		}
	}

	if (p_req->ImagingSettings.WhiteBalanceFlag)
	{
		p_setting->WhiteBalance.Mode = p_req->ImagingSettings.WhiteBalance.Mode;

		if (p_req->ImagingSettings.WhiteBalance.CrGainFlag)
		{
			p_setting->WhiteBalance.CrGain = p_req->ImagingSettings.WhiteBalance.CrGain;
		}

		if (p_req->ImagingSettings.WhiteBalance.CbGainFlag)
		{
			p_setting->WhiteBalance.CbGain = p_req->ImagingSettings.WhiteBalance.CbGain;
		}
	}

	if (p_req->ImagingSettings.ExtensionFlag)
	{
		if (p_req->ImagingSettings.Extension.Extension202Flag)
		{
			if (p_req->ImagingSettings.Extension.Extension.Extension203Flag)
			{
				if (p_req->ImagingSettings.Extension.Extension.Extension.DefoggingFlag)
				{
					strcpy(p_setting->Extension.Extension.Extension.Defogging.Mode, p_req->ImagingSettings.Extension.Extension.Extension.Defogging.Mode);
					p_setting->Extension.Extension.Extension.Defogging.Level = p_req->ImagingSettings.Extension.Extension.Extension.Defogging.Level;
				}
				if (p_req->ImagingSettings.Extension.Extension.Extension.NoiseReductionFlag)
				{
					p_setting->Extension.Extension.Extension.NoiseReduction.Level = p_req->ImagingSettings.Extension.Extension.Extension.NoiseReduction.Level;
				}
			}
		}
	}
	return ONVIF_OK;
}

/**
 * @brief
 *  Moves the focus lens in an absolute, a relative or in a continuous manner 
 *  from its current position. The speed argument is optional for absolute 
 *  and relative control, but required for continuous. If no speed argument 
 *  is used, the default speed is used. Focus adjustments through this operation 
 *  will turn off the autofocus. A device with support for remote focus control
 *  should support absolute, relative or continuous control through the Move operation.
 *
 *  At least one focus control capability is required for this operation to be functional.
 *
 *  The move operation contains the following commands:
 *  Absolute - Requires position parameter and optionally takes a speed argument. 
 *  A unitless type is used by default for focus positioning and speed. 
 *  Relative - Requires distance parameter and optionally takes a speed argument. 
 *  Negative distance means negative direction.
 *  Continuous - Requires a speed argument. Negative speed argument means negative 
 *  direction.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_Move(img_Move_REQ * p_req)
{
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

	if (p_req->Focus.AbsoluteFlag || p_req->Focus.RelativeFlag)
	{
		return ONVIF_ERR_NotSupported;
	}

	if (p_req->Focus.ContinuousFlag)
	{
		// check the parameter range 
		if (p_req->Focus.Continuous.Speed < 1.0f || p_req->Focus.Continuous.Speed > 5.0f)
		{
			return ONVIF_ERR_SettingsInvalid;
		}
	}
	// todo : add move code ...
	if(p_req->Focus.ContinuousFlag)
	{
		if(p_req->Focus.Continuous.Speed > 0)
		{
			// PtzCmdHandle(LENS_FOCUSFAR,0,0,0);// todo
		}
		if(p_req->Focus.Continuous.Speed < 0)
		{
			// PtzCmdHandle(LENS_FOCUSNEAR,0,0,0); // todo
		}
	}

	return ONVIF_OK;
}

/**
 * @brief
 *  Stops all ongoing focus movements of the lense.
 *  The operation will not affect ongoing autofocus operation.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_Stop(img_Stop_REQ * p_req)
{
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoSource;
	}

	// todo : add stop move code ...
	// PtzCmdHandle(LENS_STOP, 0, 0, 0);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Requests the current imaging status from the device.
 *
 * @return
 *  possible return value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_GetStatus(img_GetStatus_REQ * p_req, img_GetStatus_RES * p_res)
{
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoVideoSource;
	}

	// todo : add get imaging status code ...

	p_res->Status.FocusStatusFlag = 1;
	p_res->Status.FocusStatus.Position = 0.0;
	p_res->Status.FocusStatus.MoveStatus = MoveStatus_IDLE;

	return ONVIF_OK;
}

/**
 * @brief
 *  Retrieves the focus lens move options to be used in the move command.
 *
 *  The response to the command shall include all supported Move Operations. 
 *  If focus move is not supported at all, the reponse shall be empty.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 *  ONVIF_ERR_NoImagingForSource
 */
ONVIF_RET onvif_img_GetMoveOptions(img_GetMoveOptions_REQ * p_req, img_GetMoveOptions_RES * p_res)
{
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoVideoSource;
	}

	// todo : add get imaging move options code ...

	p_res->MoveOptions.ContinuousFlag = 1;
	p_res->MoveOptions.Continuous.Speed.Min = 1.0f;
	p_res->MoveOptions.Continuous.Speed.Max = 10.0f;

	return ONVIF_OK;
}

/**
 * @brief
 *  Request a given Imaging Preset to be applied to the specified video source.
 *
 *  A device indicating the ImagingPresets capability shall support this command.
 *
 *  Imaging Presets are defined by the Manufacturer, and offered as a tool to 
 *  simplify Imaging Settings adjustments for specific scene patterns. When the 
 *  new Imaging Preset is applied by SetCurrentPreset, as a response, the device 
 *  shall adjust the video source settings to match those values defined by the 
 *  specified Imaging Preset.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoSource
 */
ONVIF_RET onvif_img_SetCurrentPreset(img_SetCurrentPreset_REQ * p_req)
{
	VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoVideoSource;
	}

	strcpy(p_v_src->CurrentPresetToken, p_req->PresetToken);

	// todo : here add handler code ...

	return ONVIF_OK;
}

#endif // IMAGE_SUPPORT


