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

#ifndef ONVIF_PROVISIONING_H
#define ONVIF_PROVISIONING_H

#include "onvif_cm.h"

typedef struct
{
    uint32  TimeoutFlag : 1;                    // Indicates whether the field Timeout is valid
    uint32  Reserved    : 31;
    
	char    VideoSource[100];	                // required, The video source associated with the provisioning.
	
	onvif_PanDirection Direction;	            // required, The direction for PanMove to move the device, "left" or "right"
	
	int     Timeout;	                        // optional, Operation timeout, if less than default timeout
} tpv_PanMove_REQ;

typedef struct 
{
    uint32  TimeoutFlag : 1;                    // Indicates whether the field Timeout is valid
    uint32  Reserved    : 31;
    
	char    VideoSource[100];	                // required, The video source associated with the provisioning
	
	onvif_TiltDirection Direction;	            // required, "up" or "down"

	int     Timeout;                            // optional, Operation timeout, if less than default timeout
} tpv_TiltMove_REQ;

typedef struct
{
    uint32  TimeoutFlag : 1;                    // Indicates whether the field Timeout is valid
    uint32  Reserved    : 31;
    
	char    VideoSource[100];	                // required, The video source associated with the provisioning
	
	onvif_ZoomDirection Direction;              // required, "wide" or "telephoto"
	
	int     Timeout;                            // optional, Operation timeout, if less than default timeout
} tpv_ZoomMove_REQ;

typedef struct
{
    uint32  TimeoutFlag : 1;                    // Indicates whether the field Timeout is valid
    uint32  Reserved    : 31;
    
	char    VideoSource[100];	                // required, The video source associated with the provisioning
	
	onvif_RollDirection Direction;              // required, "clockwise", "counterclockwise", or "auto"
	
	int     Timeout;                            // optional, Operation timeout, if less than default timeout
} tpv_RollMove_REQ;

typedef struct
{
    uint32  TimeoutFlag : 1;                    // Indicates whether the field Timeout is valid
    uint32  Reserved    : 31;
    
	char    VideoSource[100];	                // required, The video source associated with the provisioning
	
	onvif_FocusDirection Direction;             // required, "near", "far", or "auto"
	
	int     Timeout;                            // optional, Operation timeout, if less than default timeout
} tpv_FocusMove_REQ;

typedef struct 
{
	char    VideoSource[100];                   // required, The video source associated with the provisioning
} tpv_Stop_REQ;

typedef struct 
{
	char    VideoSource[100];                   // required, The video source associated with the provisioning
} tpv_GetUsage_REQ;

typedef struct 
{
	onvif_Usage Usage;                          // required, The set of lifetime usage values for the provisioning associated with the video source
} tpv_GetUsage_RES;


#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET onvif_tpv_PanMove(tpv_PanMove_REQ * p_req);
ONVIF_RET onvif_tpv_TiltMove(tpv_TiltMove_REQ * p_req);
ONVIF_RET onvif_tpv_ZoomMove(tpv_ZoomMove_REQ * p_req);
ONVIF_RET onvif_tpv_RollMove(tpv_RollMove_REQ * p_req);
ONVIF_RET onvif_tpv_FocusMove(tpv_FocusMove_REQ * p_req);
ONVIF_RET onvif_tpv_Stop(tpv_Stop_REQ * p_req);
ONVIF_RET onvif_tpv_GetUsage(tpv_GetUsage_REQ * p_req, tpv_GetUsage_RES * p_res);

#ifdef __cplusplus
}
#endif

#endif




