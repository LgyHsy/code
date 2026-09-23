/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2010-2014, Happytimesoft Corporation, all rights reserved.
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

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;


/***************************************************************************************/
ONVIF_RET onvif_SetImagingSettings(SetImagingSettings_REQ * p_SetImagingSettings_req)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(p_SetImagingSettings_req->source_token);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_SOURCE;
	}

	if (p_SetImagingSettings_req->img_cfg.Brightness < g_onvif_cfg.img_opt.Brightness_min || 
		p_SetImagingSettings_req->img_cfg.Brightness > g_onvif_cfg.img_opt.Brightness_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}

	if (p_SetImagingSettings_req->img_cfg.ColorSaturation < g_onvif_cfg.img_opt.ColorSaturation_min || 
		p_SetImagingSettings_req->img_cfg.ColorSaturation > g_onvif_cfg.img_opt.ColorSaturation_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}

	if (p_SetImagingSettings_req->img_cfg.Contrast < g_onvif_cfg.img_opt.Contrast_min|| 
		p_SetImagingSettings_req->img_cfg.Contrast > g_onvif_cfg.img_opt.Contrast_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}

	if (p_SetImagingSettings_req->img_cfg.MinExposureTime < g_onvif_cfg.img_opt.MinExposureTime_min || 
		p_SetImagingSettings_req->img_cfg.MinExposureTime > g_onvif_cfg.img_opt.MinExposureTime_max )
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}

	if (p_SetImagingSettings_req->img_cfg.MaxExposureTime < g_onvif_cfg.img_opt.MaxExposureTime_min || 
		p_SetImagingSettings_req->img_cfg.MaxExposureTime > g_onvif_cfg.img_opt.MaxExposureTime_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}

	if (p_SetImagingSettings_req->img_cfg.MinGain < g_onvif_cfg.img_opt.MinGain_min || 
		p_SetImagingSettings_req->img_cfg.MinGain > g_onvif_cfg.img_opt.MinGain_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}

	if (p_SetImagingSettings_req->img_cfg.MaxGain < g_onvif_cfg.img_opt.MaxGain_min || 
		p_SetImagingSettings_req->img_cfg.MaxGain > g_onvif_cfg.img_opt.MaxGain_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}
	
	if (p_SetImagingSettings_req->img_cfg.Sharpness < g_onvif_cfg.img_opt.Sharpness_min || 
		p_SetImagingSettings_req->img_cfg.Sharpness > g_onvif_cfg.img_opt.Sharpness_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}

	if (p_SetImagingSettings_req->img_cfg.WideDynamicRange_Level < g_onvif_cfg.img_opt.WideDynamicRange_Level_min || 
		p_SetImagingSettings_req->img_cfg.WideDynamicRange_Level > g_onvif_cfg.img_opt.WideDynamicRange_Level_max)
	{
		return ONVIF_ERR_SETTINGS_INVALID;
	}
	
	// save the settings

	memcpy(&g_onvif_cfg.img_cfg, &p_SetImagingSettings_req->img_cfg, sizeof(IMAGE_CFG));
	
	return ONVIF_OK;
}

ONVIF_RET onvif_Move(Move_REQ * p_Move_req)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(p_Move_req->source_token);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_SOURCE;
	}
	
	return ONVIF_ERR_NO_IMAGEING_FOR_SOURCE;
}



