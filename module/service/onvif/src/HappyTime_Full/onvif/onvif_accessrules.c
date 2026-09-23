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
#include "onvif_accessrules.h"
#include "onvif.h"
#include "onvif_event.h"


#ifdef ACCESS_RULES

/********************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_IDX g_onvif_idx;

/********************************************************************************/

/**
 * Whenever configuration data for an access profile is changed or an access 
 * profile is added, the device shall provide the following event
 *
 **/
void onvif_AccessProfileChangedNotify(AccessProfileList * p_req)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:Configuration/AccessProfile/Changed", PropertyOperation_Changed, 
        "AccessProfileToken", p_req->AccessProfile.token,
        NULL, NULL, NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever an access profile is removed, the device shall provide the 
 * following event
 *
 **/
void onvif_AccessProfileRemovedNotify(AccessProfileList * p_req)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:Configuration/AccessProfile/Removed", PropertyOperation_Deleted, 
        "AccessProfileToken", p_req->AccessProfile.token,
        NULL, NULL, NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * @brief
 *  Requests a list of AccessProfileInfo items matching the given tokens.
 *
 *  The device shall ignore tokens it cannot resolve and shall return an 
 *  empty list if there are no  items matching the specified tokens. 
 *  The device shall not return a fault in this case.
 *
 *  If the number of requested items is greater than MaxLimit, a TooManyItems
 *  fault shall be  returned.
 *
 * @return
 *  The possible return values:
 *	ONVIF_ERR_TooManyItems
 **/
ONVIF_RET onvif_tar_GetAccessProfileInfo(tar_GetAccessProfileInfo_REQ * p_req, tar_GetAccessProfileInfo_RES * p_res)
{
    int i;
    int idx;
    AccessProfileList * p_tmp;
    
    for (i = 0; i< p_req->sizeToken; i++)
    {
        p_tmp = onvif_find_AccessProfile(g_onvif_cfg.access_rules, p_req->Token[i]);
        if (p_tmp)
        {
            idx = p_res->sizeAccessProfileInfo;

            p_res->AccessProfileInfo[idx].DescriptionFlag = p_tmp->AccessProfile.DescriptionFlag;
            
            strcpy(p_res->AccessProfileInfo[idx].token, p_tmp->AccessProfile.token);
            strcpy(p_res->AccessProfileInfo[idx].Name, p_tmp->AccessProfile.Name);
            strcpy(p_res->AccessProfileInfo[idx].Description, p_tmp->AccessProfile.Description);            
            
            p_res->sizeAccessProfileInfo++;
            if (p_res->sizeAccessProfileInfo >= ARRAY_SIZE(p_res->AccessProfileInfo))
            {
                break;
            }
        }
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all AccessProfileInfo items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data
 *  is returned and more  data is available. The reference shall be valid
 *  for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 * The possible return values:
 *	ONVIF_ERR_InvalidStartReference
 **/
ONVIF_RET onvif_tar_GetAccessProfileInfoList(tar_GetAccessProfileInfoList_REQ * p_req, tar_GetAccessProfileInfoList_RES * p_res)
{
    int idx;
    int nums = 0;
    AccessProfileList * p_tmp = g_onvif_cfg.access_rules;
    
    if (p_req->StartReferenceFlag)
    {
        p_tmp = onvif_find_AccessProfile(g_onvif_cfg.access_rules, p_req->StartReference);
        if (NULL == p_tmp)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_tmp)
    {
        idx = p_res->sizeAccessProfileInfo;

        p_res->AccessProfileInfo[idx].DescriptionFlag = p_tmp->AccessProfile.DescriptionFlag;
        
        strcpy(p_res->AccessProfileInfo[idx].token, p_tmp->AccessProfile.token);
        strcpy(p_res->AccessProfileInfo[idx].Name, p_tmp->AccessProfile.Name);
        strcpy(p_res->AccessProfileInfo[idx].Description, p_tmp->AccessProfile.Description);  
            
        p_res->sizeAccessProfileInfo++;
        
        p_tmp = p_tmp->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
        else if (nums >= ARRAY_SIZE(p_res->AccessProfileInfo))
        {
            break;
        }
    }

    if (p_tmp)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_tmp->AccessProfile.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of AccessProfile items matching the given tokens.
 *
 *  The device shall ignore tokens it cannot resolve and shall return
 *  an empty list if there are no items matching the specified tokens. 
 *  The device shall not return a fault in this case.
 *
 *  If the number of requested items is greater than MaxLimit, a 
 *  TooManyItems fault shall be returned.
 *
 * @return
 *  The possible return values:
 *	ONVIF_ERR_TooManyItems
 **/
ONVIF_RET onvif_tar_GetAccessProfiles(tar_GetAccessProfiles_REQ * p_req, tar_GetAccessProfiles_RES * p_res)
{
    int i;
    int idx;
    AccessProfileList * p_tmp;
    
    for (i = 0; i< p_req->sizeToken; i++)
    {
        p_tmp = onvif_find_AccessProfile(g_onvif_cfg.access_rules, p_req->Token[i]);
        if (p_tmp)
        {
            idx = p_res->sizeAccessProfile;

            memcpy(&p_res->AccessProfile[idx], &p_tmp->AccessProfile, sizeof(onvif_AccessProfile));
            
            p_res->sizeAccessProfile++;
            if (p_res->sizeAccessProfile >= ARRAY_SIZE(p_res->AccessProfile))
            {
                break;
            }
        }
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all AccessProfile items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data
 *  is returned and more  data is available. The reference shall be valid
 *  for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 * @return
 *  The possible return values:
 *	ONVIF_ERR_InvalidStartReference
 **/
ONVIF_RET onvif_tar_GetAccessProfileList(tar_GetAccessProfileList_REQ * p_req, tar_GetAccessProfileList_RES * p_res)
{
    int idx;
    int nums = 0;
    AccessProfileList * p_tmp = g_onvif_cfg.access_rules;
    
    if (p_req->StartReferenceFlag)
    {
        p_tmp = onvif_find_AccessProfile(g_onvif_cfg.access_rules, p_req->StartReference);
        if (NULL == p_tmp)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_tmp)
    {
        idx = p_res->sizeAccessProfile;

        memcpy(&p_res->AccessProfile[idx], &p_tmp->AccessProfile, sizeof(onvif_AccessProfile));
        
        p_res->sizeAccessProfile++;
        
        p_tmp = p_tmp->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
        else if (nums >= ARRAY_SIZE(p_res->AccessProfile))
        {
            break;
        }
    }

    if (p_tmp)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_tmp->AccessProfile.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Creates the specified access profile in the device.
 *
 *  The token field of the AccessProfile structure shall be empty and the 
 *  device shall allocate a  token for the access profile. The allocated 
 *  token shall be returned in the response.
 *
 *  If the client sends any value in the token field, the device shall 
 *  return InvalidArgVal as a generic fault code.
 *
 *  If several access policies in one access profile are specifying different
 *  schedules for the same access point, then it will result in a union of
 *  the schedules.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_MaxAccessProfiles
 *  ONVIF_ERR_MaxAccessPoliciesPerAccessProfile
 *  ONVIF_ERR_MultipleSchedulesPerAccessPointSupported
 *  ONVIF_ERR_ReferenceNotFound
 **/
ONVIF_RET onvif_tar_CreateAccessProfile(tar_CreateAccessProfile_REQ * p_req, tar_CreateAccessProfile_RES * p_res)
{
    AccessProfileList * p_tmp;
    
    // parameter check
    if (strlen(p_req->AccessProfile.token) > 0)
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    // add new AccessProfile
    p_tmp = onvif_add_AccessProfile(&g_onvif_cfg.access_rules);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_MaxAccessProfiles;
    }

    strcpy(p_req->AccessProfile.token, p_tmp->AccessProfile.token);
    strcpy(p_res->Token, p_tmp->AccessProfile.token);
    
    memcpy(&p_tmp->AccessProfile, &p_req->AccessProfile, sizeof(onvif_AccessProfile));
    
    // todo : here add handler code ...

    // send event notify
    onvif_AccessProfileChangedNotify(p_tmp);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies the specified access profile.
 *  
 *  The token of the access profile to modify is specified in the token 
 *  field of the AccessProfile structure and shall not be empty. 
 *  All other fields in the structure shall overwrite the fields in 
 *  the specified access profile.
 *
 *  If several access policies specifying different schedules for the same 
 *  access point will result in a union of the schedules.
 *
 *  If no token was specified in the request, the device shall return
 *  InvalidArgs as a generic fault code.
 *
 * @return
 *  The possible return values:
 *  InvalidArgs
 *  ONVIF_ERR_NotFound
 *  ONVIF_ERR_MaxAccessPoliciesPerAccessProfile
 *  ONVIF_ERR_MultipleSchedulesPerAccessPointSupported
 *  ONVIF_ERR_ReferenceNotFound
 **/
ONVIF_RET onvif_tar_ModifyAccessProfile(tar_ModifyAccessProfile_REQ * p_req)
{
    AccessProfileList * p_tmp = onvif_find_AccessProfile(g_onvif_cfg.access_rules, p_req->AccessProfile.token);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }
    
    // parameter check

    // todo : here add handler code ...
    
    // modify the credential
    memcpy(&p_tmp->AccessProfile, &p_req->AccessProfile, sizeof(onvif_AccessProfile));

    // send event notify
    onvif_AccessProfileChangedNotify(p_tmp);
            
    return ONVIF_OK;
}

/**
 * @brief
 *  Delete the specified access profile.
 *
 *  If the access profile is deleted, all access policies associated with
 *  the access profile will also be deleted.
 *
 *  If it is associated with one or more entities some devices may not be 
 *  able to delete the access profile, and consequently a ReferenceInUse 
 *  fault shall be generated.
 *  
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 *  ONVIF_ERR_ReferenceInUse
 **/
ONVIF_RET onvif_tar_DeleteAccessProfile(tar_DeleteAccessProfile_REQ * p_req)
{
    AccessProfileList * p_tmp = onvif_find_AccessProfile(g_onvif_cfg.access_rules, p_req->Token);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ...

    // send event notify
    onvif_AccessProfileRemovedNotify(p_tmp);
        
    // delete the credential
    onvif_free_AccessProfile(&g_onvif_cfg.access_rules, p_tmp);

    return ONVIF_OK;
}

/**
 * @brief
 *  Synchronize an access profile in a client with the device.
 *  
 *  If an access profile with the specified token does not exist in the device,
 *  the access profile is created. If an access profile with the specified 
 *  token exists, then the access profile is modified.
 *
 *  A call to this method takes an access profile structure as input parameter.
 *  The token field of the access profile must not be empty.
 *
 *  A device that signals support for the ClientSuppliedTokenSupported capability
 *  shall implement this command.
 *
 *  If no token was specified in the request, the device shall return InvalidArgs
 *  as a generic fault code.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidArgs
 *  ONVIF_ERR_MaxAccessProfiles
 **/
ONVIF_RET onvif_tar_SetAccessProfile(tar_SetAccessProfile_REQ * p_req)
{
    AccessProfileList * p_tmp;
    
    if (p_req->AccessProfile.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgs;
    }
    
    p_tmp = onvif_find_AccessProfile(g_onvif_cfg.access_rules, p_req->AccessProfile.token);
    if (NULL == p_tmp)
    {
        // add new AccessProfile
        p_tmp = onvif_add_AccessProfile(&g_onvif_cfg.access_rules);
        if (NULL == p_tmp)
        {
            return ONVIF_ERR_MaxAccessProfiles;
        }

        memcpy(&p_tmp->AccessProfile, &p_req->AccessProfile, sizeof(onvif_AccessProfile));
        
        // todo : here add handler code ...

        // send event notify
        onvif_AccessProfileChangedNotify(p_tmp);
    }
    else
    {
        // parameter check

        // todo : here add handler code ...
        
        // modify the credential
        memcpy(&p_tmp->AccessProfile, &p_req->AccessProfile, sizeof(onvif_AccessProfile));

        // send event notify
        onvif_AccessProfileChangedNotify(p_tmp);
    }

    return ONVIF_OK;
}

#endif // end of ACCESS_RULES



