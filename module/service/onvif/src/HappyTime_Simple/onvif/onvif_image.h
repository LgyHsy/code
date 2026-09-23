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

#ifndef ONVIF_IMAGE_H
#define ONVIF_IMAGE_H

#include "sys_inc.h"
#include "onvif.h"

/***************************************************************************************/
typedef struct
{
	char		source_token[ONVIF_TOKEN_LEN];	
	IMAGE_CFG	img_cfg;	
	BOOL		persistence;	
} SetImagingSettings_REQ;

typedef struct
{
	char		source_token[ONVIF_TOKEN_LEN];	
	int			absolute_pos;
	int			absolute_speed;
	int			relative_distance;
	int			relative_speed;
	int			continuous_speed;
} Move_REQ;

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************/
ONVIF_RET onvif_SetImagingSettings(SetImagingSettings_REQ * p_SetImagingSettings_req);
ONVIF_RET onvif_Move(Move_REQ * p_Move_req);

#ifdef __cplusplus
}
#endif


#endif





