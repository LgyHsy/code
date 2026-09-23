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

#include "onvif_doorcontrol.h"
#include "onvif.h"
#include "onvif_event.h"
#include "onvif_timer.h"

#ifdef PROFILE_C_SUPPORT

extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_IDX g_onvif_idx;

/***************************************************************************************/

/**
 * Whenever important configuration data for an access point is changed or an access point
 * is added, the device shall provide the following event
 *
 */
void onvif_AccessPointChangedNotify(AccessPointList * p_ap)
{
    if (NULL == p_ap)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/AccessPoint/Changed", PropertyOperation_Changed, 
		"AccessPointToken", p_ap->AccessPointInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever an access point is removed, the device shall provide the following event
 *
 */
void onvif_AccessPointRemovedNotify(AccessPointList * p_ap)
{
    if (NULL == p_ap)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/AccessPoint/Removed", PropertyOperation_Deleted, 
		"AccessPointToken", p_ap->AccessPointInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever configuration data for an area is changed or an area is added, 
 * the device shall provide the following event
 *
 */
void onvif_AreaChangedNotify(AreaList * p_area)
{
    if (NULL == p_area)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/Area/Changed", PropertyOperation_Changed, 
		"AreaToken", p_area->AreaInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever an area is removed, the device shall provide the following event
 *
 */
void onvif_AreaRemovedNotify(AreaList * p_area)
{
    if (NULL == p_area)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/Area/Removed", PropertyOperation_Deleted, 
		"AreaToken", p_area->AreaInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever configuration data for a door is changed or a door is added, 
 * the device shall provide the following event
 *
 */
void onvif_DoorChangedNotify(DoorList * p_door)
{
    if (NULL == p_door)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/Door/Changed", PropertyOperation_Changed, 
		"DoorToken", p_door->Door.DoorInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever a door is removed, the device shall provide the following event
 *
 */
void onvif_DoorRemovedNotify(DoorList * p_door)
{
    if (NULL == p_door)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Configuration/Door/Removed", PropertyOperation_Deleted, 
		"DoorToken", p_door->Door.DoorInfo.token, NULL, NULL,
		NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * The device that signals support for the DisableAccessPoint capability for 
 * a particular access point instance shall provide the following event whenever 
 * the state, enabled or disabled, of this access point is changed
 *
 */
void onvif_AccessPointStateEnabledChangedNotify(AccessPointList * p_accesspoint)
{
    if (NULL == p_accesspoint)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:AccessPoint/State/Enabled", PropertyOperation_Changed, 
		"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
		"State", p_accesspoint->Enabled ? "true" : "false", NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever a door mode is changed, the device shall provide the following event
 *
 */
void onvif_DoorStateDoorModeChangedNotify(DoorList * p_door)
{
    if (NULL == p_door)
    {
        return;
    }
    
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Door/State/DoorMode", PropertyOperation_Changed, 
		"DoorToken", p_door->Door.DoorInfo.token, NULL, NULL,
		"State", onvif_DoorModeToString(p_door->DoorState.DoorMode), NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

void onvif_DoorStataChanged(void * argv)
{
    DoorList * p_door = (DoorList *)argv;

    p_door->DoorState.DoorMode = DoorMode_Locked;
    
    onvif_DoorStateDoorModeChangedNotify(p_door);
}

/***************************************************************************************/

/**
 * @brief
 *  Requests a list of all AccessPoint items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data
 *  is returned and more data is available.
 *  The reference shall be valid for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 *  A device that signals support for the AccessPointManagementSupported 
 *  capability shall implement this command.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tac_GetAccessPointList(tac_GetAccessPointList_REQ * p_req, tac_GetAccessPointList_RES * p_res)
{
    int nums = 0;
    AccessPointList * p_accesspoint = g_onvif_cfg.access_points;
    
    if (p_req->StartReferenceFlag)
    {
        p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->StartReference);
        if (NULL == p_accesspoint)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_accesspoint)
    {
        AccessPointList * p_accesspoint1 = onvif_add_AccessPoint(&p_res->AccessPoint);
        if (p_accesspoint1)
        {
            memcpy(&p_accesspoint1->AccessPointInfo, &p_accesspoint->AccessPointInfo, sizeof(onvif_AccessPointInfo));
        }
        
        p_accesspoint = p_accesspoint->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_accesspoint)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_accesspoint->AccessPointInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Creates the specified access point in the device.
 *
 *  The token field of the AccessPoint structure shall be empty and the 
 *  device shall allocate a token for the access point. The allocated 
 *  token shall be returned in the response.
 *
 *  If the client sends any value in the token field, the device shall
 *  return InvalidArgVal as a generic fault code.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxAccessPoints or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tac_CreateAccessPoint(tac_CreateAccessPoint_REQ * p_req, tac_CreateAccessPoint_RES * p_res)
{
    int nums = 0;
    AccessPointList * p_ap = g_onvif_cfg.access_points;

    if (p_req->AccessPoint.token[0] != '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }
    
    while (p_ap)
    {
        nums++;
        p_ap = p_ap->next;
    }

    if (nums >= g_onvif_cfg.Capabilities.accesscontrol.MaxAccessPoints)
    {
        return ONVIF_ERR_MaxAccessPoints; 
    }
    
    p_ap = onvif_add_AccessPoint(&g_onvif_cfg.access_points);
    if (p_ap)
    {
        strcpy(p_req->AccessPoint.token, p_ap->AccessPointInfo.token);
        strcpy(p_res->Token, p_ap->AccessPointInfo.token);
        
        memcpy(&p_ap->AccessPointInfo, &p_req->AccessPoint, sizeof(onvif_AccessPointInfo));
        strcpy(p_ap->AuthenticationProfileToken, p_req->AuthenticationProfileToken);

        onvif_AccessPointChangedNotify(p_ap);
    }
    else
    {
        return ONVIF_ERR_MaxAccessPoints;
    }

    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Synchronize an access point in a client with the device.
 *
 *  If an access point with the specified token does not exist in the device, 
 *  the access point is created. If an access point with the specified token
 *  exists, then the access point is modified.
 *
 *  A call to this method takes an AccessPoint structure as input parameter. 
 *  The token field of the AccessPoint structure shall not be empty.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxAccessPoints or ONVIF_ERR_ReferenceNotFound or 
 *  ONVIF_ERR_ClientSuppliedTokenSupported
 */
ONVIF_RET onvif_tac_SetAccessPoint(tac_SetAccessPoint_REQ * p_req)
{
    AccessPointList * p_ap;
    
    if (p_req->AccessPoint.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_ap = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->AccessPoint.token);
    if (p_ap)
    {
        // update
        memcpy(&p_ap->AccessPointInfo, &p_req->AccessPoint, sizeof(onvif_AccessPointInfo));
        strcpy(p_ap->AuthenticationProfileToken, p_req->AuthenticationProfileToken);
    }
    else // add new
    {
        int nums = 0;
        p_ap = g_onvif_cfg.access_points;
        while (p_ap)
        {
            nums++;
            p_ap = p_ap->next;
        }

        if (nums >= g_onvif_cfg.Capabilities.accesscontrol.MaxAccessPoints)
        {
            return ONVIF_ERR_MaxAccessPoints; 
        }
        
        p_ap = onvif_add_AccessPoint(&g_onvif_cfg.access_points);
        if (p_ap)
        {
            memcpy(&p_ap->AccessPointInfo, &p_req->AccessPoint, sizeof(onvif_AccessPointInfo));
            strcpy(p_ap->AuthenticationProfileToken, p_req->AuthenticationProfileToken);
        }
    }

    // todo : here add handler code ...


    onvif_AccessPointChangedNotify(p_ap);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies the specified access point.
 *
 *  The token of the access point to modify is specified in the token field
 *  of the AccessPoint structure and shall not be empty. All other fields 
 *  in the structure shall overwrite the fields in the specified access point.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tac_ModifyAccessPoint(tac_ModifyAccessPoint_REQ * p_req)
{
    AccessPointList * p_ap;
    
    if (p_req->AccessPoint.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_ap = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->AccessPoint.token);
    if (p_ap)
    {
        memcpy(&p_ap->AccessPointInfo, &p_req->AccessPoint, sizeof(onvif_AccessPointInfo));
        strcpy(p_ap->AuthenticationProfileToken, p_req->AuthenticationProfileToken);
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ...

    
    onvif_AccessPointChangedNotify(p_ap);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes the specified access point.
 *
 *  If it is associated with one or more entities some devices may not be
 *  able to delete the access point, and consequently a ReferenceInUse fault
 *  shall be generated.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tac_DeleteAccessPoint(tac_DeleteAccessPoint_REQ * p_req)
{
    AccessPointList * p_ap;
    
    if (p_req->Token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgs;
    }

    p_ap = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->Token);
    if (p_ap)
    {
        // todo : here add handler code ...

        
        onvif_AccessPointRemovedNotify(p_ap);
        
        onvif_free_AccessPoint(&g_onvif_cfg.access_points, p_ap);
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all AccessPointInfo items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data 
 *  is returned and more data is available.
 *  The reference shall be valid for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tac_GetAccessPointInfoList(tac_GetAccessPointInfoList_REQ * p_req, tac_GetAccessPointInfoList_RES * p_res)
{
    int nums = 0;
    AccessPointList * p_accesspoint = g_onvif_cfg.access_points;
    
    if (p_req->StartReferenceFlag)
    {
        p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->StartReference);
        if (NULL == p_accesspoint)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_accesspoint)
    {
        AccessPointList * p_accesspoint1 = onvif_add_AccessPoint(&p_res->AccessPointInfo);
        if (p_accesspoint1)
        {
            memcpy(&p_accesspoint1->AccessPointInfo, &p_accesspoint->AccessPointInfo, sizeof(onvif_AccessPointInfo));
        }
        
        p_accesspoint = p_accesspoint->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_accesspoint)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_accesspoint->AccessPointInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Enabling an access point.
 *
 *  A device that signals support for DisableAccessPoint capability for
 *  a particular access point instance shall implement this command.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ONVIF_ERR_NotSupported
 */
ONVIF_RET onvif_tac_EnableAccessPoint(tac_EnableAccessPoint_REQ * p_req)
{
    AccessPointList * p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->Token);
    if (NULL == p_accesspoint)
    {
        return ONVIF_ERR_NotFound;
    }

    p_accesspoint->Enabled = TRUE;

    // send event topic tns1:AccessPoint/State/Enabled notify
    onvif_AccessPointStateEnabledChangedNotify(p_accesspoint);	

    return ONVIF_OK;
}

/**
 * @brief
 *  Disabling an access point.
 *
 *  A device that signals support for the DisableAccessPoint capability for 
 *  a particular access point instance shall implement this command.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ONVIF_ERR_NotSupported
 */
ONVIF_RET onvif_tac_DisableAccessPoint(tac_DisableAccessPoint_REQ * p_req)
{
    AccessPointList * p_accesspoint = onvif_find_AccessPoint(g_onvif_cfg.access_points, p_req->Token);
    if (NULL == p_accesspoint)
    {
        return ONVIF_ERR_NotFound;
    }

    p_accesspoint->Enabled = FALSE;

    // send event topic tns1:AccessPoint/State/Enabled notify
    onvif_AccessPointStateEnabledChangedNotify(p_accesspoint);	
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all Area items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data 
 *  is returned and more data is available.
 *  The reference shall be valid for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter
 *
 *  A device that signals support for the AreaManagementSupported capability
 *  shall implement this command.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tac_GetAreaList(tac_GetAreaList_REQ * p_req, tac_GetAreaList_RES * p_res)
{
    int nums = 0;
    AreaList * p_area = g_onvif_cfg.areas;
    
    if (p_req->StartReferenceFlag)
    {
        p_area = onvif_find_Area(g_onvif_cfg.areas, p_req->StartReference);
        if (NULL == p_area)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_area)
    {
        AreaList * p_area1 = onvif_add_Area(&p_res->Area);
        if (p_area1)
        {
            memcpy(&p_area1->AreaInfo, &p_area->AreaInfo, sizeof(onvif_AreaInfo));
        }
        
        p_area = p_area->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_area)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_area->AreaInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Creates the specified area in the device.
 *
 *  The token field of the Area structure shall be empty and the device 
 *  shall allocate a token for the area. The allocated token shall be
 *  returned in the response.
 *
 *  If the client sends any value in the token field, the device shall
 *  return InvalidArgVal as a generic fault code.
 *
 *  A device that signals support for the AreaManagementSupported 
 *  capability shall implement this command.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxAreas or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tac_CreateArea(tac_CreateArea_REQ * p_req, tac_CreateArea_RES * p_res)
{
    int nums = 0;
    AreaList * p_area = g_onvif_cfg.areas;

    if (p_req->Area.token[0] != '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }
    
    while (p_area)
    {
        nums++;
        p_area = p_area->next;
    }

    if (nums >= g_onvif_cfg.Capabilities.accesscontrol.MaxAreas)
    {
        return ONVIF_ERR_MaxAreas; 
    }
    
    p_area = onvif_add_Area(&g_onvif_cfg.areas);
    if (p_area)
    {
        strcpy(p_req->Area.token, p_area->AreaInfo.token);
        strcpy(p_res->Token, p_area->AreaInfo.token);
        
        memcpy(&p_area->AreaInfo, &p_req->Area, sizeof(onvif_AreaInfo));

        onvif_AreaChangedNotify(p_area);
    }
    else
    {
        return ONVIF_ERR_MaxAreas;
    }

    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Synchronize an area in a client with the device.
 *
 *  If an area with the specified token does not exist in the device, 
 *  the area is created. If an area with the specified token exists, 
 *  then the area is modified.
 *
 *  A call to this method takes an Area structure as input parameter. 
 *  The token field of the Area structure shall not be empty.
 *
 *  A device that signals support for the ClientSuppliedTokenSupported 
 *  capability shall implement this command.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxAreas or ONVIF_ERR_ReferenceNotFound or 
 *  ONVIF_ERR_ClientSuppliedTokenSupported
 */
ONVIF_RET onvif_tac_SetArea(tac_SetArea_REQ * p_req)
{
    AreaList * p_area;
    
    if (p_req->Area.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_area = onvif_find_Area(g_onvif_cfg.areas, p_req->Area.token);
    if (p_area)
    {
        // update
        memcpy(&p_area->AreaInfo, &p_req->Area, sizeof(onvif_AreaInfo));
    }
    else // add new
    {
        int nums = 0;
        p_area = g_onvif_cfg.areas;
        while (p_area)
        {
            nums++;
            p_area = p_area->next;
        }

        if (nums >= g_onvif_cfg.Capabilities.accesscontrol.MaxAreas)
        {
            return ONVIF_ERR_MaxAreas; 
        }
        
        p_area = onvif_add_Area(&g_onvif_cfg.areas);
        if (p_area)
        {
            memcpy(&p_area->AreaInfo, &p_req->Area, sizeof(onvif_AreaInfo));
        }
    }

    // todo : here add handler code ...


    onvif_AreaChangedNotify(p_area);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies the specified area.
 *
 *  The token of the area to modify is specified in the token field of the 
 *  Area structure and shall not be empty. All other fields in the 
 *  structure shall overwrite the fields in the specified area.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 *  A device that signals support for the AreaManagementSupported capability
 *  shall implement this command.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tac_ModifyArea(tac_ModifyArea_REQ * p_req)
{
    AreaList * p_area;
    
    if (p_req->Area.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_area = onvif_find_Area(g_onvif_cfg.areas, p_req->Area.token);
    if (p_area)
    {
        memcpy(&p_area->AreaInfo, &p_req->Area, sizeof(onvif_AreaInfo));
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ...

    
    onvif_AreaChangedNotify(p_area);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes the specified area.
 *
 *  If it is associated with one or more entities some devices may not be 
 *  able to delete the area, and consequently a ReferenceInUse fault shall
 *  be generated.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 *  A device that signals support for the AreaManagementSupported capability
 *  shall implement this command.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tac_DeleteArea(tac_DeleteArea_REQ * p_req)
{
    AreaList * p_area;
    
    if (p_req->Token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgs;
    }

    p_area = onvif_find_Area(g_onvif_cfg.areas, p_req->Token);
    if (p_area)
    {
        // todo : here add handler code ...

        
        onvif_AreaRemovedNotify(p_area);
        
        onvif_free_Area(&g_onvif_cfg.areas, p_area);
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all AreaInfo items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data
 *  is returned and more data is available.
 *  The reference shall be valid for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tac_GetAreaInfoList(tac_GetAreaInfoList_REQ * p_req, tac_GetAreaInfoList_RES * p_res)
{
    int nums = 0;
    AreaList * p_area = g_onvif_cfg.areas;
    
    if (p_req->StartReferenceFlag)
    {
        p_area = onvif_find_Area(g_onvif_cfg.areas, p_req->StartReference);
        if (NULL == p_area)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_area)
    {
        AreaList * p_info = onvif_add_Area(&p_res->AreaInfo);
        if (p_info)
        {
            memcpy(&p_info->AreaInfo, &p_area->AreaInfo, sizeof(onvif_AreaInfo));
        }
        
        p_area = p_area->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_area)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_area->AreaInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all Door items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data 
 *  is returned and more data is available. The reference shall be valid 
 *  for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *  
 *  A device that signals support for the DoorManagementSupported capability 
 *  shall implement this command.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tdc_GetDoorList(tdc_GetDoorList_REQ * p_req, tdc_GetDoorList_RES * p_res)
{
    int nums = 0;
    DoorList * p_door = g_onvif_cfg.doors;
    
    if (p_req->StartReferenceFlag)
    {
        p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->StartReference);
        if (NULL == p_door)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_door)
    {
        DoorList * p_info = onvif_add_Door(&p_res->Door);
        if (p_info)
        {
            memcpy(&p_info->Door, &p_door->Door, sizeof(onvif_Door));
        }
        
        p_door = p_door->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_door)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_door->Door.DoorInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Creates the specified door in the device.
 *
 *  The token field of the Door structure shall be empty and the device 
 *  shall allocate a token for the door. The allocated token shall be 
 *  returned in the response.
 *
 *  If the client sends any value in the token field, the device shall
 *  return InvalidArgVal as a generic fault code.
 *
 *  A device that signals support for the DoorManagementSupported 
 *  capability shall implement this command.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxDoors or ONVIF_ERR_ReferenceNotFound
 */
ONVIF_RET onvif_tdc_CreateDoor(tdc_CreateDoor_REQ * p_req, tdc_CreateDoor_RES * p_res)
{
    int nums = 0;
    DoorList * p_door = g_onvif_cfg.doors;

    if (p_req->Door.DoorInfo.token[0] != '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }
    
    while (p_door)
    {
        nums++;
        p_door = p_door->next;
    }

    if (nums >= g_onvif_cfg.Capabilities.doorcontrol.MaxDoors)
    {
        return ONVIF_ERR_MaxDoors; 
    }
    
    p_door = onvif_add_Door(&g_onvif_cfg.doors);
    if (p_door)
    {
        strcpy(p_req->Door.DoorInfo.token, p_door->Door.DoorInfo.token);
        strcpy(p_res->Token, p_door->Door.DoorInfo.token);
        
        memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));

        onvif_DoorChangedNotify(p_door);
    }
    else
    {
        return ONVIF_ERR_MaxDoors;
    }

    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Synchronize a door in a client with the device.
 *
 *  If a door with the specified token does not exist in the device, 
 *  the door is created. If a door with the specified token exists, 
 *  then the door is modified.
 *
 *  A call to this method takes a door structure as input parameter.
 *  The token field of the Door structure shall not be empty.
 *
 *  A device that signals support for the ClientSuppliedTokenSupported 
 *  capability shall implement this command.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_MaxDoors or ONVIF_ERR_ReferenceNotFound or 
 *  ONVIF_ERR_ClientSuppliedTokenSupported
 */
ONVIF_RET onvif_tdc_SetDoor(tdc_SetDoor_REQ * p_req)
{
    DoorList * p_door;
    
    if (p_req->Door.DoorInfo.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Door.DoorInfo.token);
    if (p_door)
    {
        // update
        memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));
    }
    else // add new
    {
        int nums = 0;
        p_door = g_onvif_cfg.doors;
        while (p_door)
        {
            nums++;
            p_door = p_door->next;
        }

        if (nums >= g_onvif_cfg.Capabilities.doorcontrol.MaxDoors)
        {
            return ONVIF_ERR_MaxDoors; 
        }
        
        p_door = onvif_add_Door(&g_onvif_cfg.doors);
        if (p_door)
        {
            memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));
        }
    }

    // todo : here add handler code ...


    onvif_DoorChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies the specified door.
 *
 *  The token of the door to modify is specified in the token field of the
 *  Door structure and shall not be empty. All other fields in the structure
 *  shall overwrite the fields in the specified door.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 *  A device that signals support for the DoorManagementSupported 
 *  capability shall implement this command.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound
 */
ONVIF_RET onvif_tdc_ModifyDoor(tdc_ModifyDoor_REQ * p_req)
{
    DoorList * p_door;
    
    if (p_req->Door.DoorInfo.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Door.DoorInfo.token);
    if (p_door)
    {
        memcpy(&p_door->Door, &p_req->Door, sizeof(onvif_Door));
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ...

    
    onvif_DoorChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes the specified door.
 *
 *  If it is associated with one or more entities some devices may not be
 *  able to delete the door, and consequently a ReferenceInUse fault shall
 *  be generated.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 *  A device that signals support for the DoorManagementSupported capability
 *  shall implement this command.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_ReferenceInUse
 */
ONVIF_RET onvif_tdc_DeleteDoor(tdc_DeleteDoor_REQ * p_req)
{
    DoorList * p_door;
    
    if (p_req->Token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgs;
    }

    p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (p_door)
    {
        // todo : here add handler code ...

        
        onvif_DoorRemovedNotify(p_door);
        
        onvif_free_Door(&g_onvif_cfg.doors, p_door);
    }
    else
    {
        return ONVIF_ERR_NotFound;
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all DoorInfo items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data
 *  is returned and more data is available. The reference shall be valid 
 *  for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 * @param p_req the request parameter
 * @param p_res the response parameter
 * @return ONVIF_OK or ONVIF_ERR_InvalidStartReference
 */
ONVIF_RET onvif_tdc_GetDoorInfoList(tdc_GetDoorInfoList_REQ * p_req, tdc_GetDoorInfoList_RES * p_res)
{
    int nums = 0;
    DoorList * p_door = g_onvif_cfg.doors;
    
    if (p_req->StartReferenceFlag)
    {
        p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->StartReference);
        if (NULL == p_door)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_door)
    {
        DoorInfoList * p_info = onvif_add_DoorInfo(&p_res->DoorInfo);
        if (p_info)
        {
            memcpy(&p_info->DoorInfo, &p_door->Door.DoorInfo, sizeof(onvif_DoorInfo));
        }
        
        p_door = p_door->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_door)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_door->Door.DoorInfo.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows momentarily accessing a door. It invokes the
 *  functionality typically used when a card holder presents a card to 
 *  a card reader at the door and is granted access.
 *
 *  The DoorMode shall change to Accessed state.
 *
 *  The door shall remain accessible for the defined time. When the time
 *  span elapses, the DoorMode shall change back to its previous state.
 *
 *  If the request cannot be fulfilled, a Failure fault shall be returned.
 *
 *  A device that signals support for Access capability for a particular 
 *  Door instance shall support this method.
 *
 *  The device shall take the best effort approach for parameters not 
 *  supported, it must fallback to preconfigured time or limit the time to 
 *  the closest supported time if the specified time is out of range.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_AccessDoor(tdc_AccessDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Access == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do access door operation ...
    p_door->DoorState.DoorMode = DoorMode_Accessed;
    
    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);

    timer_add(3, onvif_DoorStataChanged, p_door, TIMER_MODE_SINGLE);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows locking a door. The door mode shall change to 
 *  Locked state.
 *
 *  A device that signals support for Lock capability for a particular 
 *  Door instance shall support this command.
 *
 *  If the request cannot be fulfilled, a Failure fault shall be returned.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockDoor(tdc_LockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Lock == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Locked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows unlocking a door. The door mode shall change to 
 *  Unlocked state.
 *
 *  A device that signals support for Unlock capability for a particular 
 *  Door instance support this command.
 *
 *  If the request cannot be fulfilled, a Failure fault shall be returned.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_UnlockDoor(tdc_UnlockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Unlock == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Unlocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation is used for securely locking a door. 
 *  A call to this method shall change door mode state to DoubleLocked.
 *
 *  A device that signals support for DoubleLock capability for a 
 *  particular Door instance shall support this command. Otherwise this 
 *  method can be performed as a standard Lock operation.
 *
 *  If the door has an extra lock that shall be locked as well.
 *
 *  If the request cannot be fulfilled, a Failure fault shall be returned.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_DoubleLockDoor(tdc_DoubleLockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.DoubleLock == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_DoubleLocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows blocking a door and preventing momentary access.
 *  The door mode shall change to Blocked state.
 *
 *  A device that signals support for Block capability for a particular 
 *  Door instance shall support this command.
 *
 *  If the request cannot be fulfilled, a Failure fault shall be returned.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_BlockDoor(tdc_BlockDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.Block == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Blocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows locking and preventing other actions until a 
 *  LockDownRelease command is invoked. 
 *  The DoorMode shall change to LockedDown state.
 *  
 *  The device shall ignore other door control commands until a 
 *  LockDownRelease command is performed.
 *
 *  A device that signals support for LockDown capability for a 
 *  particular Door instance shall support this command.
 *
 *  If a device supports DoubleLock capability for a particular Door 
 *  instance, that operation may be engaged as well.
 *
 *  If the request cannot be fulfilled, a Failure fault shall be returned.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockDownDoor(tdc_LockDownDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockDown == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_LockedDown;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows releasing the LockedDown state of a door. 
 *  The door mode shall change back to its previous/next state. 
 *  It is not defined what the previous/next state shall be, but 
 *  typically the Locked state. A device that signals support for 
 *  LockDown capability for a particular Door instance shall support
 *  this command.
 *
 *  This method shall only succeed if the current door mode is LockedDown.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockDownReleaseDoor(tdc_LockDownReleaseDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockDown == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Locked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows unlocking a door and preventing other actions
 *  until LockOpenRelease method is invoked. 
 *  The door mode shall change to LockedOpen state. 
 *
 *  The device shall ignore other door control commands until a 
 *  LockOpenRelease command is performed.
 *
 *  A device that signals support for LockOpen capability for a particular
 *  Door instance shall support this command.
 *
 *  If the request cannot be fulfilled, a Failure fault shall be returned.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockOpenDoor(tdc_LockOpenDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockOpen == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_LockedOpen;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  This operation allows releasing the LockedOpen state of a door. 
 *  The door mode shall change state from the LockedOpen state back 
 *  to its previous/next state. It is not defined what the previous/next
 *  state shall be, but typically the Unlocked state. A device that 
 *  signals support for LockOpen capability for a particular Door 
 *  instance shall support this command.
 *
 *  This method shall only succeed if the current DoorMode is LockedOpen.
 *
 * @param p_req the request parameter
 * @return ONVIF_OK or ONVIF_ERR_NotFound or ONVIF_ERR_Failure
 */
ONVIF_RET onvif_tdc_LockOpenReleaseDoor(tdc_LockOpenReleaseDoor_REQ * p_req)
{
    DoorList * p_door = onvif_find_Door(g_onvif_cfg.doors, p_req->Token);
    if (NULL == p_door)
    {
        return ONVIF_ERR_NotFound;
    }

    if (p_door->Door.DoorInfo.Capabilities.LockOpen == FALSE)
    {
        return ONVIF_ERR_Failure;
    }
    
    // todo : do lock door operation ...
    p_door->DoorState.DoorMode = DoorMode_Unlocked;

    // todo : if the door state changed, send event notify
    onvif_DoorStateDoorModeChangedNotify(p_door);
    
    return ONVIF_OK;
}

#endif // PROFILE_C_SUPPORT



