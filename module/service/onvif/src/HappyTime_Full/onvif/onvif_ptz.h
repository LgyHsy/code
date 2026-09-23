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

#ifndef ONVIF_PTZ_H
#define ONVIF_PTZ_H

#include "sys_inc.h"
#include "onvif.h"
#include "para.h"
#include <stdint.h>

/* Legacy product types used by HappyTime_Full PTZ path */
#ifndef PRODUCT_TYPE_BASE
#define PRODUCT_TYPE_BASE 0
#endif
#ifndef PRODUCT_TYPE_MYE10
#define PRODUCT_TYPE_MYE10 0x199
#endif
#ifndef PRODUCT_TYPE_MYE40
#define PRODUCT_TYPE_MYE40 0x1a4
#endif
#ifndef PRODUCT_TYPE_MYE18
#define PRODUCT_TYPE_MYE18 0x1a6
#endif
#ifndef PRODUCT_TYPE_MYD20
#define PRODUCT_TYPE_MYD20 0x1a9
#endif
#ifndef PRODUCT_TYPE_MYE10Q
#define PRODUCT_TYPE_MYE10Q 0x1ad
#endif

/* Legacy PTZ command IDs */
#ifndef LENS_UP
#define LENS_UP       0x01
#define LENS_LEFT     0x02
#define LENS_RIGHT    0x03
#define LENS_DOWN     0x04
#define LENS_FAR      0x05
#define LENS_NEAR     0x06
#define LENS_FOCUSNEAR 0x07
#define LENS_FOCUSFAR  0x08
#define LENS_DIAPHRAGM_LARGE 0x09
#define LENS_DIAPHRAGM_SMALL 0x0a
#define LENS_PRESET_GOTO 0x0b
#define LENS_PRESET_SET  0x0c
#define LENS_PRESET_DEL  0x0d
#define LENS_LIGHT_ON    0x0e
#define LENS_LIGHT_OFF   0x0f
#define LENS_AUTO        0x10
#define LENS_STOP        0x11
#define WIPER_PWRON      0x12
#define WIPER_PWROFF     0x13
#define LENS_LEFT_UP     0x14
#define LENS_LEFT_DOWN   0x15
#define LENS_RIGHT_UP    0x16
#define LENS_RIGHT_DOWN  0x17
#endif


typedef struct
{
	uint32	TimeoutFlag 	: 1;				// Indicates whether the field Timeout is valid
	uint32  Reserved	 	: 31;
	
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile
	
	onvif_PTZSpeed	Velocity;					// required, A Velocity vector specifying the velocity of pan, tilt and zoom
	
	int		Timeout;							// optional, An optional Timeout parameter, unit is second
} ptz_ContinuousMove_REQ;

typedef struct 
{
	uint32	PanTiltFlag 	: 1;				// Indicates whether the field PanTilt is valid
	uint32	ZoomFlag 		: 1;				// Indicates whether the field Zoom is valid
	uint32  Reserved	 	: 30;
	
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile that indicate what should be stopped

	BOOL	PanTilt;							// optional, Set true when we want to stop ongoing pan and tilt movements.If PanTilt arguments are not present, this command stops these movements
	BOOL	Zoom;								// optional, Set true when we want to stop ongoing zoom movement.If Zoom arguments are not present, this command stops ongoing zoom movement
} ptz_Stop_REQ;

typedef struct
{
	uint32	SpeedFlag 		: 1;				// Indicates whether the field Speed is valid
	uint32  Reserved	 	: 31;
	
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile

	onvif_PTZVector	Position;					// required, A Position vector specifying the absolute target position
	onvif_PTZSpeed	Speed;						// optional, An optional Speed    
} ptz_AbsoluteMove_REQ;

typedef struct
{
	uint32	SpeedFlag 		: 1;				// Indicates whether the field Speed is valid
	uint32  Reserved	 	: 31;
	
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile

	onvif_PTZVector	Translation;				// required, A positional Translation relative to the current position
	onvif_PTZSpeed	Speed;						// optional, An optional Speed parameter
} ptz_RelativeMove_REQ;

typedef struct
{
	uint32	PresetTokenFlag : 1;				// Indicates whether the field PresetToken is valid
	uint32	PresetNameFlag 	: 1;				// Indicates whether the field PresetName is valid
	uint32  Reserved	 	: 30;
	
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile where the operation should take place
	char	PresetToken[ONVIF_TOKEN_LEN];		// optional, A requested preset token
	char    PresetName[ONVIF_NAME_LEN];			// optional, A requested preset name
} ptz_SetPreset_REQ;

typedef struct
{
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile where the operation should take place
	char	PresetToken[ONVIF_TOKEN_LEN];		// required, A requested preset token
} ptz_RemovePreset_REQ;

typedef struct
{
	uint32	SpeedFlag 		: 1;				// Indicates whether the field Speed is valid
	uint32  Reserved	 	: 31;
	
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile where the operation should take place
	char	PresetToken[ONVIF_TOKEN_LEN];		// required, A requested preset token

	onvif_PTZSpeed	Speed;						// optional, A requested speed.The speed parameter can only be specified when Speed Spaces are available for the PTZ Node
} ptz_GotoPreset_REQ;

typedef struct
{
	uint32	SpeedFlag 		: 1;				// Indicates whether the field Speed is valid
	uint32  Reserved	 	: 31;
	
	char	ProfileToken[ONVIF_TOKEN_LEN];		// required, A reference to the MediaProfile where the operation should take place

	onvif_PTZSpeed	Speed;						// optional, A requested speed.The speed parameter can only be specified when Speed Spaces are available for the PTZ Node
} ptz_GotoHomePosition_REQ;

typedef struct
{
	onvif_PTZConfiguration  PTZConfiguration;	// required, 

	BOOL	ForcePersistence;					// required, 	
} ptz_SetConfiguration_REQ;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];	    // required, Contains the token of an existing media profile the configurations shall be compatible with
} ptz_GetCompatibleConfigurations_REQ;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];      // required
} ptz_GetPresetTours_REQ;

typedef struct
{
	PresetTourList *  PresetTour;
} ptz_GetPresetTours_RES;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];      // required
	char    PresetTourToken[ONVIF_TOKEN_LEN];   // required
} ptz_GetPresetTour_REQ;

typedef struct
{
	onvif_PresetTour    PresetTour;
} ptz_GetPresetTour_RES;

typedef struct
{
	uint32	PresetTourTokenFlag : 1;	        // Indicates whether the field PresetTourToken is valid
	uint32  Reserved	 	    : 31;

	char    ProfileToken[ONVIF_TOKEN_LEN];      // required
	char    PresetTourToken[ONVIF_TOKEN_LEN];   // optional
} ptz_GetPresetTourOptions_REQ;

typedef struct
{
	onvif_PTZPresetTourOptions  Options;        // required
} ptz_GetPresetTourOptions_RES;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];      // required
} ptz_CreatePresetTour_REQ;

typedef struct
{
	char    PresetTourToken[ONVIF_TOKEN_LEN];   // required, 
} ptz_CreatePresetTour_RES;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];      // required

	onvif_PresetTour    PresetTour;             // required
} ptz_ModifyPresetTour_REQ;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];      // required
	char    PresetTourToken[ONVIF_TOKEN_LEN];   // required
	
	onvif_PTZPresetTourOperation Operation;     // required
} ptz_OperatePresetTour_REQ;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];      // required
	char    PresetTourToken[ONVIF_TOKEN_LEN];   // required
} ptz_RemovePresetTour_REQ;

typedef struct
{
	char    ProfileToken[ONVIF_TOKEN_LEN];      // required, 
	char    AuxiliaryData[64];                  // required, 
} ptz_SendAuxiliaryCommand_REQ;

typedef struct
{
	char    AuxiliaryResponse[256];             // required, 
} ptz_SendAuxiliaryCommand_RES;

typedef struct
{
	uint32	SpeedFlag       : 1;	            // Indicates whether the field Speed is valid
	uint32	AreaHeightFlag  : 1;	            // Indicates whether the field AreaHeight is valid
	uint32	AreaWidthFlag   : 1;	            // Indicates whether the field AreaWidth is valid
	uint32  Reserved        : 29;

	char    ProfileToken[ONVIF_TOKEN_LEN];	    // required, A reference to the MediaProfile

	onvif_GeoLocation   Target;	                // required, The geolocation of the target position
	onvif_PTZSpeed      Speed;	                // optional, An optional Speed

	float   AreaHeight;	                        // optional, An optional indication of the height of the target/area
	float   AreaWidth;	                        // optional, An optional indication of the width of the target/area
} ptz_GeoMove_REQ;


#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET onvif_ptz_GetStatus(onvif_PTZStatus * p_ptz_status);

ONVIF_RET onvif_ptz_GetStatus_old(ONVIF_PROFILE * p_profile, onvif_PTZStatus * p_ptz_status);

ONVIF_RET onvif_ptz_ContinuousMove(ptz_ContinuousMove_REQ * p_req);
ONVIF_RET onvif_ptz_Stop(ptz_Stop_REQ * p_req);
ONVIF_RET onvif_ptz_AbsoluteMove(ptz_AbsoluteMove_REQ * p_req);
ONVIF_RET onvif_ptz_RelativeMove(ptz_RelativeMove_REQ * p_req);
ONVIF_RET onvif_ptz_SetPreset(ptz_SetPreset_REQ * p_req);
ONVIF_RET onvif_ptz_RemovePreset(ptz_RemovePreset_REQ * p_req);
ONVIF_RET onvif_ptz_GotoPreset(ptz_GotoPreset_REQ * p_req);
ONVIF_RET onvif_ptz_GotoHomePosition(ptz_GotoHomePosition_REQ * p_req);
ONVIF_RET onvif_ptz_SetHomePosition(const char * token);
ONVIF_RET onvif_ptz_SetConfiguration(ptz_SetConfiguration_REQ * p_req);

ONVIF_RET onvif_ptz_GetPresetTourOptions(ptz_GetPresetTourOptions_REQ * p_req, ptz_GetPresetTourOptions_RES * p_res);
ONVIF_RET onvif_ptz_CreatePresetTour(ptz_CreatePresetTour_REQ * p_req, ptz_CreatePresetTour_RES * p_res);
ONVIF_RET onvif_ptz_ModifyPresetTour(ptz_ModifyPresetTour_REQ * p_req);
ONVIF_RET onvif_ptz_OperatePresetTour(ptz_OperatePresetTour_REQ * p_req);
ONVIF_RET onvif_ptz_RemovePresetTour(ptz_RemovePresetTour_REQ * p_req);
ONVIF_RET onvif_ptz_SendAuxiliaryCommand(ptz_SendAuxiliaryCommand_REQ * p_req, ptz_SendAuxiliaryCommand_RES * p_res);
ONVIF_RET onvif_ptz_GeoMove(ptz_GeoMove_REQ * p_req);

int ptz_tour_init(void);
int get_tour_cnt(void);
ptz_tour_t *find_ptz_tour(char *tour_token);


#ifdef __cplusplus
}
#endif


#endif


