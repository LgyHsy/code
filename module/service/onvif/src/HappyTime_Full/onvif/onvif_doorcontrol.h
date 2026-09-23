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

#ifndef ONVIF_DOORCONTROL_H
#define ONVIF_DOORCONTROL_H

#include "sys_inc.h"
#include "onvif_cm.h"


typedef struct
{
    uint32  LimitFlag           : 1;
    uint32  StartReferenceFlag  : 1;
    uint32  Reserved            : 30;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, less than one or higher than what the device supports, the number of items is determined by the device
	char    StartReference[256];	                    // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
} tac_GetAccessPointList_REQ;

typedef struct
{
    uint32  NextStartReferenceFlag : 1;
    uint32  Reserved               : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];        // optional, StartReference to use in next call to get the following items. If absent, no more items to get
	
	AccessPointList * AccessPoint;	                    // optional, List of AccessPoint items
} tac_GetAccessPointList_RES;

typedef struct
{
    char    token[ACCESS_CTRL_MAX_LIMIT][ONVIF_TOKEN_LEN];  // Tokens of AccessPoint items to get
} tac_GetAccessPoints_REQ;

typedef struct
{
    onvif_AccessPointInfo   AccessPoint;                // required
    char    AuthenticationProfileToken[ONVIF_TOKEN_LEN];// optional
} tac_CreateAccessPoint_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // required
} tac_CreateAccessPoint_RES;

typedef struct
{
    onvif_AccessPointInfo   AccessPoint;                // required
    char    AuthenticationProfileToken[ONVIF_TOKEN_LEN];// optional
} tac_SetAccessPoint_REQ;

typedef struct
{
    onvif_AccessPointInfo   AccessPoint;                // required
    char    AuthenticationProfileToken[ONVIF_TOKEN_LEN];// optional
} tac_ModifyAccessPoint_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // required, Token of AccessPoint item to delete
} tac_DeleteAccessPoint_REQ;

typedef struct
{
    uint32  LimitFlag           : 1;
    uint32  StartReferenceFlag  : 1;
    uint32  Reserved            : 30;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, less than one or higher than what the device supports, the number of items is determined by the device
	char    StartReference[256];	                    // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
} tac_GetAccessPointInfoList_REQ;

typedef struct
{
    uint32  NextStartReferenceFlag : 1;
    uint32  Reserved               : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];        // optional, StartReference to use in next call to get the following items. If absent, no more items to get
	
	AccessPointList * AccessPointInfo;	                // optional, List of AccessPointInfo items
} tac_GetAccessPointInfoList_RES;

typedef struct
{
    char    token[ACCESS_CTRL_MAX_LIMIT][ONVIF_TOKEN_LEN];  // Tokens of AccessPointInfo items to get     
} tac_GetAccessPointInfo_REQ;

typedef struct
{
	AccessPointList * AccessPointInfo;	                // List of AccessPointInfo items
} tac_GetAccessPointInfo_RES;

typedef struct
{
    uint32  LimitFlag           : 1;
    uint32  StartReferenceFlag  : 1;
    uint32  Reserved            : 30;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, less than one or higher than what the device supports, the number of items is determined by the device
	char    StartReference[256];	                    // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
} tac_GetAreaList_REQ;

typedef struct
{
    uint32  NextStartReferenceFlag : 1;
    uint32  Reserved               : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];        // optional, StartReference to use in next call to get the following items. If absent, no more items to get
	
	AreaList * Area;	                                // optional, List of Area items
} tac_GetAreaList_RES;

typedef struct
{
    char    token[ACCESS_CTRL_MAX_LIMIT][ONVIF_TOKEN_LEN];  // Tokens of Area items to get
} tac_GetAreas_REQ;

typedef struct
{
    onvif_AreaInfo   Area;                              // required
} tac_CreateArea_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // required
} tac_CreateArea_RES;

typedef struct
{
    onvif_AreaInfo   Area;                              // required
} tac_SetArea_REQ;

typedef struct
{
    onvif_AreaInfo   Area;                              // required
} tac_ModifyArea_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // required, Token of Area item to delete
} tac_DeleteArea_REQ;

typedef struct
{
    uint32  LimitFlag           : 1;
    uint32  StartReferenceFlag  : 1;
    uint32  Reserved            : 30;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, or higher than what the device supports, the number of items shall be determined by the device
	char    StartReference[256];	                    // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
} tac_GetAreaInfoList_REQ;

typedef struct
{
    uint32  NextStartReferenceFlag : 1;
    uint32  Reserved               : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];        // optional, StartReference to use in next call to get the following items. If absent, no more items to get
	
	AreaList * AreaInfo;                                // optional, List of AreaInfo items
} tac_GetAreaInfoList_RES;

typedef struct
{
    char    token[ACCESS_CTRL_MAX_LIMIT][ONVIF_TOKEN_LEN];  // Tokens of DoorInfo items to get 
} tac_GetAreaInfo_REQ;

typedef struct
{
	AreaList * AreaInfo;                                // List of AreaInfo items
} tac_GetAreaInfo_RES;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // required, Token of AccessPoint instance to get AccessPointState for
} tac_GetAccessPointState_REQ;

typedef struct
{
    BOOL    Enabled;                                    // required, Indicates that the AccessPoint is enabled. By default this field value shall be True, if the DisableAccessPoint capabilities is not supported
} tac_GetAccessPointState_RES;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // required, Token of the AccessPoint instance to enable
} tac_EnableAccessPoint_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // required, Token of the AccessPoint instance to disable
} tac_DisableAccessPoint_REQ;

typedef struct 
{
    uint32  LimitFlag           : 1;
    uint32  StartReferenceFlag  : 1;
    uint32  Reserved            : 30;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, or higher than what the device supports, the number of items shall be determined by the device
	char    StartReference[256];	                    // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
} tdc_GetDoorList_REQ;

typedef struct 
{
    uint32  NextStartReferenceFlag : 1;
    uint32  Reserved               : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];        // optional, StartReference to use in next call to get the following items. If absent, no more items to get
	
	DoorList * Door;                                    // optional, List of Door items
} tdc_GetDoorList_RES;

typedef struct
{
    char    token[DOOR_CTRL_MAX_LIMIT][ONVIF_TOKEN_LEN];// Tokens of Door items to get
} tdc_GetDoors_REQ;

typedef struct
{
	DoorList * Door;                                    // List of Door items
} tdc_GetDoors_RES;

typedef struct
{
    onvif_Door  Door;                                   // Door item to create
} tdc_CreateDoor_REQ;

typedef struct
{
	char    Token[ONVIF_TOKEN_LEN];                     // Token of created Door item
} tdc_CreateDoor_RES;

typedef struct
{
    onvif_Door  Door;                                   // The Door item to create or modify
} tdc_SetDoor_REQ;

typedef struct
{
    onvif_Door  Door;                                   // The details of the door
} tdc_ModifyDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                     // The Token of the door to delete
} tdc_DeleteDoor_REQ;

typedef struct
{
    uint32  LimitFlag           : 1;
    uint32  StartReferenceFlag  : 1;
    uint32  Reserved            : 30;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, or higher than what the device supports, the number of items shall be determined by the device
	char    StartReference[256];	                    // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
} tdc_GetDoorInfoList_REQ;

typedef struct
{
    uint32  NextStartReferenceFlag : 1;
    uint32  Reserved               : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];        // optional, StartReference to use in next call to get the following items. If absent, no more items to get
	
	DoorInfoList * DoorInfo;                            // optional, List of DoorInfo items
} tdc_GetDoorInfoList_RES;

typedef struct
{
    char    token[DOOR_CTRL_MAX_LIMIT][ONVIF_TOKEN_LEN];// Tokens of DoorInfo items to get 
} tdc_GetDoorInfo_REQ;

typedef struct
{
	DoorInfoList * DoorInfo;                            // List of DoorInfo items
} tdc_GetDoorInfo_RES;

typedef struct 
{
	char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to get the state for
} tdc_GetDoorState_REQ;

typedef struct 
{
    uint32  UseExtendedTimeFlag : 1;
    uint32  AccessTimeFlag      : 1;
    uint32  OpenTooLongTimeFlag : 1;
    uint32  PreAlarmTimeFlag    : 1;
    uint32  Reserved            : 28;
    
	char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
	BOOL    UseExtendedTime;	                        // optional, Indicates that the configured extended time should be used
	int     AccessTime;	                                // optional, overrides AccessTime if specified
	int     OpenTooLongTime;	                        // optional, overrides OpenTooLongTime if specified (DOTL)
	int     PreAlarmTime;	                            // optional, overrides PreAlarmTime if specified
} tdc_AccessDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_LockDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_UnlockDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_DoubleLockDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_BlockDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_LockDownDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_LockDownReleaseDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_LockOpenDoor_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of the Door instance to control
} tdc_LockOpenReleaseDoor_REQ;


#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET onvif_tac_GetAccessPointList(tac_GetAccessPointList_REQ * p_req, tac_GetAccessPointList_RES * p_res);
ONVIF_RET onvif_tac_CreateAccessPoint(tac_CreateAccessPoint_REQ * p_req, tac_CreateAccessPoint_RES * p_res);
ONVIF_RET onvif_tac_SetAccessPoint(tac_SetAccessPoint_REQ * p_req);
ONVIF_RET onvif_tac_ModifyAccessPoint(tac_ModifyAccessPoint_REQ * p_req);
ONVIF_RET onvif_tac_DeleteAccessPoint(tac_DeleteAccessPoint_REQ * p_req);
ONVIF_RET onvif_tac_GetAccessPointInfoList(tac_GetAccessPointInfoList_REQ * p_req, tac_GetAccessPointInfoList_RES * p_res);
ONVIF_RET onvif_tac_GetAreaList(tac_GetAreaList_REQ * p_req, tac_GetAreaList_RES * p_res);
ONVIF_RET onvif_tac_CreateArea(tac_CreateArea_REQ * p_req, tac_CreateArea_RES * p_res);
ONVIF_RET onvif_tac_SetArea(tac_SetArea_REQ * p_req);
ONVIF_RET onvif_tac_ModifyArea(tac_ModifyArea_REQ * p_req);
ONVIF_RET onvif_tac_DeleteArea(tac_DeleteArea_REQ * p_req);
ONVIF_RET onvif_tac_GetAreaInfoList(tac_GetAreaInfoList_REQ * p_req, tac_GetAreaInfoList_RES * p_res);
ONVIF_RET onvif_tac_EnableAccessPoint(tac_EnableAccessPoint_REQ * p_req);
ONVIF_RET onvif_tac_DisableAccessPoint(tac_DisableAccessPoint_REQ * p_req);

ONVIF_RET onvif_tdc_GetDoorList(tdc_GetDoorList_REQ * p_req, tdc_GetDoorList_RES * p_res);
ONVIF_RET onvif_tdc_CreateDoor(tdc_CreateDoor_REQ * p_req, tdc_CreateDoor_RES * p_res);
ONVIF_RET onvif_tdc_SetDoor(tdc_SetDoor_REQ * p_req);
ONVIF_RET onvif_tdc_ModifyDoor(tdc_ModifyDoor_REQ * p_req);
ONVIF_RET onvif_tdc_DeleteDoor(tdc_DeleteDoor_REQ * p_req);
ONVIF_RET onvif_tdc_GetDoorInfoList(tdc_GetDoorInfoList_REQ * p_req, tdc_GetDoorInfoList_RES * p_res);
ONVIF_RET onvif_tdc_AccessDoor(tdc_AccessDoor_REQ * p_req);
ONVIF_RET onvif_tdc_LockDoor(tdc_LockDoor_REQ * p_req);
ONVIF_RET onvif_tdc_UnlockDoor(tdc_UnlockDoor_REQ * p_req);
ONVIF_RET onvif_tdc_DoubleLockDoor(tdc_DoubleLockDoor_REQ * p_req);
ONVIF_RET onvif_tdc_BlockDoor(tdc_BlockDoor_REQ * p_req);
ONVIF_RET onvif_tdc_LockDownDoor(tdc_LockDownDoor_REQ * p_req);
ONVIF_RET onvif_tdc_LockDownReleaseDoor(tdc_LockDownReleaseDoor_REQ * p_req);
ONVIF_RET onvif_tdc_LockOpenDoor(tdc_LockOpenDoor_REQ * p_req);
ONVIF_RET onvif_tdc_LockOpenReleaseDoor(tdc_LockOpenReleaseDoor_REQ * p_req);

#ifdef __cplusplus
}
#endif

#endif // ONVIF_DOORCONTROL_H

