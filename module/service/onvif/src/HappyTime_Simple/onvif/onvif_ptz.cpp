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
#include "onvif_ptz.h"
#include "onvif_util.h"
#include "ptz.h"

extern ONVIF_CLS g_onvif_cls;
const float EPSINON = 0.00001;

BOOL ptz_GetStatus(ONVIF_PROFILE * p_profile, PTZ_STATUS * p_ptz_status)
{
	p_ptz_status->move_sta = PTZ_STA_IDLE;
	p_ptz_status->zoom_sta = PTZ_STA_IDLE;
	p_ptz_status->pantilt_pos_x = 0;
	p_ptz_status->pantilt_pos_y = 0;
	p_ptz_status->zoom_pos = 0;

	onvif_get_time_str(p_ptz_status->utc_time, sizeof(p_ptz_status->utc_time), 0);	
	
	return TRUE;
}

ONVIF_RET onvif_ContinuousMove(ContinuousMove_REQ * p_ContinuousMove_req)
{
	/*ONVIF_PROFILE * p_profile = onvif_find_profile(p_ContinuousMove_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	else if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}*/
	
	printf("->pantilt_velocity_x:%f, ->pantilt_velocity_y:%f, ->pantilt_velocity_z:%f\n",
	p_ContinuousMove_req->pantilt_velocity_x, p_ContinuousMove_req->pantilt_velocity_y, p_ContinuousMove_req->zoom_velocity);
	int x_speed = 0,y_speed = 0,z_speed = 0;
	/**/
	if((p_ContinuousMove_req->pantilt_velocity_x > 0.00000 && p_ContinuousMove_req->pantilt_velocity_x <= 1.00000)||
		(p_ContinuousMove_req->pantilt_velocity_x < 0.00000 && p_ContinuousMove_req->pantilt_velocity_x >= -1.00000))

	{
		x_speed = (int)(p_ContinuousMove_req->pantilt_velocity_x * 10 + (p_ContinuousMove_req->pantilt_velocity_x < 0.00000 ? -1 : 1));
	}
	else
	{
		x_speed = (int)p_ContinuousMove_req->pantilt_velocity_x;
	}

	if((p_ContinuousMove_req->pantilt_velocity_y > 0.00000 && p_ContinuousMove_req->pantilt_velocity_y <= 1.00000)||
		(p_ContinuousMove_req->pantilt_velocity_y < 0.00000 && p_ContinuousMove_req->pantilt_velocity_y >= -1.00000))

	{
		y_speed = (int)(p_ContinuousMove_req->pantilt_velocity_y * 10 + (p_ContinuousMove_req->pantilt_velocity_y < 0.00000 ? -1 : 1));
	}
	else
	{
		y_speed = (int)p_ContinuousMove_req->pantilt_velocity_y;
	}

	if((p_ContinuousMove_req->zoom_velocity > 0.00000 && p_ContinuousMove_req->zoom_velocity <= 1.00000)||
		(p_ContinuousMove_req->zoom_velocity < 0.00000 && p_ContinuousMove_req->zoom_velocity >= -1.00000))

	{
		z_speed = (int)(p_ContinuousMove_req->zoom_velocity * 10 + (p_ContinuousMove_req->zoom_velocity < 0.00000 ? -1 : 1));
	}
	else
	{
		z_speed = (int)p_ContinuousMove_req->zoom_velocity;
	}
	printf("x_speed:%d, y_speed:%d, z_speed:%d\n", x_speed, y_speed, z_speed);
	if(x_speed > 0)
	{
		if(y_speed > 0)
		{
			PtzCmdHandle(LENS_RIGHT_UP, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed < 0)
		{
			PtzCmdHandle(LENS_RIGHT_DOWN, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed >= -EPSINON && y_speed<= EPSINON)
		{
			PtzCmdHandle(LENS_RIGHT, abs(x_speed), 0, 0);
		}
	}
	if(x_speed < 0)
	{
		if(y_speed > 0)
		{
			PtzCmdHandle(LENS_LEFT_UP, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed < 0)
		{
			PtzCmdHandle(LENS_LEFT_DOWN, abs(x_speed), abs(y_speed), 0);
		}
		if(y_speed >= -EPSINON && y_speed<= EPSINON)
		{
			PtzCmdHandle(LENS_LEFT, abs(x_speed), 0, 0);
		}
	}
	if(x_speed >= -EPSINON && x_speed<= EPSINON)
	{
		if(y_speed > 0)
		{
			PtzCmdHandle(LENS_UP, 0, abs(y_speed), 0);
		}
		if(y_speed < 0)
		{
			PtzCmdHandle(LENS_DOWN, 0, abs(y_speed), 0);
		}
		if(y_speed >= -EPSINON && y_speed<= EPSINON)
		{
			if(z_speed >= -EPSINON && z_speed <= EPSINON)
			{
				PtzCmdHandle(LENS_STOP, 0, 0, 0);
			}
		}
	}
	if(z_speed > 0)
	{
		PtzCmdHandle(LENS_FAR, 0, 0, 0);
	}
	if(z_speed < 0)
	{
		PtzCmdHandle(LENS_NEAR, 0, 0, 0);
	}
	// do continuous move ... 
	
    return ONVIF_OK;
}

ONVIF_RET onvif_PTZ_Stop(PTZ_Stop_REQ * p_Stop_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_Stop_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	else if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}
	
	PtzCmdHandle(LENS_STOP,0,0,0);
	
	// stop PTZ moving ... 
	
    return ONVIF_OK;
}

ONVIF_RET onvif_AbsoluteMove(AbsoluteMove_REQ * p_AbsoluteMove_req)
{	
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_AbsoluteMove_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	else if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	if (p_AbsoluteMove_req->pantilt_pos_x > p_profile->ptz_node->abs_pantilt_x.max || 
		p_AbsoluteMove_req->pantilt_pos_y > p_profile->ptz_node->abs_pantilt_y.max || 
		p_AbsoluteMove_req->zoom_pos > p_profile->ptz_node->abs_zoom.max)
	{
		return ONVIF_ERR_INVALID_POSIION;
	}
	
	// do absolute move ...
	
    return ONVIF_OK;
}

ONVIF_RET onvif_RelativeMove(RelativeMove_REQ * p_RelativeMove_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_RelativeMove_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	else if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	// do relative move ...
	
	return ONVIF_OK;
}

ONVIF_RET onvif_SetPreset(SetPreset_REQ * p_SetPreset_req)
{
	PTZ_PRESET * p_preset = NULL;

	ONVIF_PROFILE * p_profile = onvif_find_profile(p_SetPreset_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	else if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}
	
	if (p_SetPreset_req->preset_token[0] != '\0')
	{
		p_preset = onvif_find_preset(p_SetPreset_req->profile_token, p_SetPreset_req->preset_token);
		if (NULL == p_preset)
		{
			return ONVIF_ERR_NO_TOKEN;
		}
	}
	else
	{
		p_preset = onvif_get_idle_preset(p_SetPreset_req->profile_token);
		if (NULL == p_preset)
		{
			return ONVIF_ERR_TOO_MANY_PRESETS;
		}
	}

	if (p_SetPreset_req->name[0] != '\0')
	{
		strcpy(p_preset->name, p_SetPreset_req->name);
	}
	else
	{
		sprintf(p_preset->name, "PRESET_%d", g_onvif_cls.preset_idx);
		strcpy(p_SetPreset_req->name, p_preset->name);
		g_onvif_cls.preset_idx++;
	}

	if (p_SetPreset_req->preset_token[0] != '\0')
	{
		strcpy(p_preset->token, p_SetPreset_req->preset_token);
	}
	else
	{
		sprintf(p_preset->token, "PRESET_%d", g_onvif_cls.preset_idx);
		strcpy(p_SetPreset_req->preset_token, p_preset->token);
		g_onvif_cls.preset_idx++;
	}

	// get PTZ current position
	p_preset->pantilt_pos_x = 0;
	p_preset->pantilt_pos_y = 0;
	p_preset->zoom_pos = 0;

	p_preset->used_flag = 1;
	
	
	if (1)
	{
		char presetToken[256]="";
	
		show_preset_list();
		
		add_preset(p_SetPreset_req->profile_token, p_SetPreset_req->name, p_SetPreset_req->preset_token, presetToken, 0); //not home

		show_preset_list();
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_RemovePreset(RemovePreset_REQ * p_RemovePreset_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_RemovePreset_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	PTZ_PRESET * p_preset = onvif_find_preset(p_RemovePreset_req->profile_token, p_RemovePreset_req->preset_token);
	if (NULL == p_preset)
	{
		return ONVIF_ERR_NO_TOKEN;
	}

	memset(p_preset, 0, sizeof(PTZ_PRESET));
	if (1)
	{
		show_preset_list();
		
		remove_preset(p_RemovePreset_req->profile_token, p_RemovePreset_req->preset_token);

		show_preset_list();
	}
	return ONVIF_OK;
}

ONVIF_RET onvif_GotoPreset(GotoPreset_REQ * p_GotoPreset_req)
{	
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_GotoPreset_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	PTZ_PRESET * p_preset = onvif_find_preset(p_GotoPreset_req->profile_token, p_GotoPreset_req->preset_token);
	if (NULL == p_preset)
	{
		return ONVIF_ERR_NO_TOKEN;
	}

	// goto preset ...
	if (1)
	{
		show_preset_list();

		int ret = goto_preset(p_GotoPreset_req->profile_token, p_GotoPreset_req->preset_token);

		if(ret >= 0)
			return ONVIF_OK;
		else
			return ONVIF_ERR_NO_TOKEN;
	}
	return ONVIF_OK;
}

ONVIF_RET onvif_GotoHomePosition(GotoHomePosition_REQ * p_GotoHomePosition_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_GotoHomePosition_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	// goto home position ...
	if (1)
	{
		int ret = goto_home_preset(p_GotoHomePosition_req->profile_token);
		
		show_preset_list();
		
		if(ret >= 0)
			return ONVIF_OK;
		else
			return ONVIF_ERR_NO_PTZ_PROFILE;
	}
	return ONVIF_OK;
}

ONVIF_RET onvif_SetHomePosition(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	if (p_profile->ptz_node->fixed_home_pos == 1)
	{
		return ONVIF_ERR_CANNOT_OVERWRITE_HOME;
	}
	// set home position ...
	if (1)
	{
		int ret = set_home_preset((char *)token);

		show_preset_list();

		if(ret >= 0)
			return ONVIF_OK;
		else
			return ONVIF_ERR_NO_PTZ_PROFILE;
	}
	return ONVIF_OK;
}




