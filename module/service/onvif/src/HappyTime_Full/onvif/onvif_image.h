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

#ifndef ONVIF_IMAGE_H
#define ONVIF_IMAGE_H

#include "sys_inc.h"
#include "onvif.h"
/***************************************************************************************/

typedef struct
{
	char	    VideoSourceToken[ONVIF_TOKEN_LEN];  // required
} img_GetImagingSettings_REQ;

typedef struct
{
	uint32      ForcePersistenceFlag	: 1;		// Indicates whether the field ForcePersistence is valid
	uint32      Reserved	 			: 31;
	
	char        VideoSourceToken[ONVIF_TOKEN_LEN];	// required		
	BOOL        ForcePersistence;					// optional, Flag that makes configuration persistent. Example: User wants the configuration to exist after reboot

	onvif_ImagingSettings	ImagingSettings;		// required
} img_SetImagingSettings_REQ;

typedef struct
{
	char	    VideoSourceToken[ONVIF_TOKEN_LEN];  // required, Reference token to the VideoSource for which the imaging parameter options are requested
} img_GetOptions_REQ;

typedef struct
{
    char        VideoSourceToken[ONVIF_TOKEN_LEN];  // required, Reference to the VideoSource
} img_GetMoveOptions_REQ;

typedef struct
{
    onvif_MoveOptions20 MoveOptions;	            // required, Valid ranges for the focus lens move options
} img_GetMoveOptions_RES;

typedef struct
{
	char		VideoSourceToken[ONVIF_TOKEN_LEN];	// required, Reference to the VideoSource for the requested move (focus) operation	

	onvif_FocusMove   Focus;                        // required, Content of the requested move (focus) operation
} img_Move_REQ;

typedef struct
{
    char        VideoSourceToken[ONVIF_TOKEN_LEN];  // required, Reference to the VideoSource
} img_Stop_REQ;

typedef struct
{
    char        VideoSourceToken[ONVIF_TOKEN_LEN];  // required, Reference to the VideoSource
} img_GetStatus_REQ;

typedef struct
{
    onvif_ImagingStatus Status;	                    // required, Requested imaging status
} img_GetStatus_RES;

typedef struct
{
	char        VideoSourceToken[ONVIF_TOKEN_LEN];  // required, Reference to the VideoSource
} img_GetPresets_REQ;

typedef struct
{
	char        VideoSourceToken[ONVIF_TOKEN_LEN];  // required, Reference to the VideoSource
} img_GetCurrentPreset_REQ;

typedef struct
{
	char        VideoSourceToken[ONVIF_TOKEN_LEN];  //required, , Reference to the VideoSource
	char        PresetToken[ONVIF_TOKEN_LEN];       //required, Reference token to the Imaging Preset to be applied to the specified Video Source
} img_SetCurrentPreset_REQ;


#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************/
ONVIF_RET onvif_img_SetImagingSettings(img_SetImagingSettings_REQ * p_req);
ONVIF_RET onvif_img_Move(img_Move_REQ * p_req);
ONVIF_RET onvif_img_Stop(img_Stop_REQ * p_req);
ONVIF_RET onvif_img_GetStatus(img_GetStatus_REQ * p_req, img_GetStatus_RES * p_res);
ONVIF_RET onvif_img_GetMoveOptions(img_GetMoveOptions_REQ * p_req, img_GetMoveOptions_RES * p_res);
ONVIF_RET onvif_img_SetCurrentPreset(img_SetCurrentPreset_REQ * p_req);

#ifdef __cplusplus
}
#endif


#endif





