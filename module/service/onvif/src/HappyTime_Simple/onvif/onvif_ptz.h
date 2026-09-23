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

#ifndef ONVIF_PTZ_H
#define ONVIF_PTZ_H

#include "sys_inc.h"
#include "onvif.h"
#include "ptz.h"

typedef struct
{
	char	profile_token[ONVIF_TOKEN_LEN];
	
	float	pantilt_velocity_x;
	float	pantilt_velocity_y;
	float   zoom_velocity;
	
	int		timeout;	
} ContinuousMove_REQ;

typedef struct 
{
	char	profile_token[ONVIF_TOKEN_LEN];

	bool	stop_pantile;
	bool	stop_zoom;
} PTZ_Stop_REQ;

typedef struct
{
    char	profile_token[ONVIF_TOKEN_LEN];
    
    float   pantilt_pos_x;
    float   pantilt_pos_y;
    float   zoom_pos;

    float   pantilt_speed_x;
    float   pantilt_speed_y;
    float   zoom_speed;
} AbsoluteMove_REQ;

typedef struct
{
    char	profile_token[ONVIF_TOKEN_LEN];
    
    float   pantilt_pos_x;
    float   pantilt_pos_y;
    float   zoom_pos;

    float   pantilt_speed_x;
    float   pantilt_speed_y;
    float   zoom_speed;
} RelativeMove_REQ;

typedef struct
{
    char	profile_token[ONVIF_TOKEN_LEN];
    char	preset_token[ONVIF_TOKEN_LEN];
    char    name[ONVIF_NAME_LEN];
} SetPreset_REQ;

typedef struct
{
	char	profile_token[ONVIF_TOKEN_LEN];
	char	preset_token[ONVIF_TOKEN_LEN];
} RemovePreset_REQ;

typedef struct
{
	char	profile_token[ONVIF_TOKEN_LEN];
	char	preset_token[ONVIF_TOKEN_LEN];

	float   pantilt_speed_x;
    float   pantilt_speed_y;
    float   zoom_speed;
} GotoPreset_REQ;

typedef struct
{
    char	profile_token[ONVIF_TOKEN_LEN];

    float   pantilt_speed_x;
    float   pantilt_speed_y;
    float   zoom_speed;
} GotoHomePosition_REQ;


#ifdef __cplusplus
extern "C" {
#endif

BOOL ptz_GetStatus(ONVIF_PROFILE * p_profile, PTZ_STATUS * p_ptz_status);

ONVIF_RET onvif_ContinuousMove(ContinuousMove_REQ * p_ContinuousMove_req);
ONVIF_RET onvif_PTZ_Stop(PTZ_Stop_REQ * p_Stop_req);
ONVIF_RET onvif_AbsoluteMove(AbsoluteMove_REQ * p_AbsoluteMove_req);
ONVIF_RET onvif_RelativeMove(RelativeMove_REQ * p_RelativeMove_req);
ONVIF_RET onvif_SetPreset(SetPreset_REQ * p_SetPreset_req);
ONVIF_RET onvif_RemovePreset(RemovePreset_REQ * p_RemovePreset_req);
ONVIF_RET onvif_GotoPreset(GotoPreset_REQ * p_GotoPreset_req);
ONVIF_RET onvif_GotoHomePosition(GotoHomePosition_REQ * p_GotoHomePosition_req);
ONVIF_RET onvif_SetHomePosition(const char * token);

#ifdef __cplusplus
}
#endif


#endif


