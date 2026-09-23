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
#include "onvif_credential.h"
#include "onvif.h"
#include "onvif_event.h"


#ifdef CREDENTIAL_SUPPORT

/********************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_IDX g_onvif_idx;

/********************************************************************************/

/**
 * Whenever a credential is removed, the device shall provide the following event
 *
 **/
void onvif_CredentialRemovedNotify(CredentialList * p_req)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:Configuration/Credential/Removed", PropertyOperation_Deleted, 
        "CredentialToken", p_req->Credential.token,
        NULL, NULL, NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever the credential state (enabled or disabled) is changed, the device 
 * shall provide the following event
 *
 **/
void onvif_CredentialStateNotify(CredentialList * p_req, BOOL ClientUpdated)
{
    NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    SimpleItemList * p_simpleitem;
	    onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Credential/State/Enabled");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{		    
			strcpy(p_simpleitem->SimpleItem.Name, "CredentialToken");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->Credential.token);
		}

		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{		    
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->State.Enabled ? "true" : "false");
		}

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{		    
			strcpy(p_simpleitem->SimpleItem.Name, "Reason");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->State.Reason);
		}

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{		    
			strcpy(p_simpleitem->SimpleItem.Name, "ClientUpdated");
			strcpy(p_simpleitem->SimpleItem.Value, ClientUpdated ? "true" : "false");
		}

		onvif_put_NotificationMessage(p_message);
	}
}

/**
 * Whenever configuration data for a credential (including credential 
 * identifiers and credential access profiles) is changed, or if a credential 
 * is added, the device shall provide the following event
 *
 **/
void onvif_CredentialChangedNotify(CredentialList * p_req)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:Configuration/Credential/Changed", PropertyOperation_Changed, 
        "CredentialToken", p_req->Credential.token,
        NULL, NULL, NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * The device shall provide the following event whenever there is any
 *  anti-passback violation
 *
 **/
void onvif_CredentialApbViolationNotify(CredentialList * p_req, BOOL ClientUpdated)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:Credential/State/ApbViolation", PropertyOperation_Changed, 
        "CredentialToken", p_req->Credential.token, NULL, NULL, 
        "ApbViolation", p_req->State.AntipassbackState.AntipassbackViolated ? "true" : "false",
        "ClientUpdated", ClientUpdated ? "true" : "false");
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * @brief
 *  Requests a list of CredentialInfo items matching the given tokens.
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
ONVIF_RET onvif_tcr_GetCredentialInfo(tcr_GetCredentialInfo_REQ * p_req, tcr_GetCredentialInfo_RES * p_res)
{
    int i;
    int idx;
    CredentialList * p_tmp;
    
    for (i = 0; i< p_req->sizeToken; i++)
    {
        p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->Token[i]);
        if (p_tmp)
        {
            idx = p_res->sizeCredentialInfo;

            p_res->CredentialInfo[idx].DescriptionFlag = p_tmp->Credential.DescriptionFlag;
            p_res->CredentialInfo[idx].ValidFromFlag = p_tmp->Credential.ValidFromFlag;            
            p_res->CredentialInfo[idx].ValidToFlag = p_tmp->Credential.ValidToFlag;            

            strcpy(p_res->CredentialInfo[idx].ValidFrom, p_tmp->Credential.ValidFrom);
            strcpy(p_res->CredentialInfo[idx].ValidTo, p_tmp->Credential.ValidTo);
            strcpy(p_res->CredentialInfo[idx].token, p_tmp->Credential.token);
            strcpy(p_res->CredentialInfo[idx].Description, p_tmp->Credential.Description);
            strcpy(p_res->CredentialInfo[idx].CredentialHolderReference, p_tmp->Credential.CredentialHolderReference);
            
            p_res->sizeCredentialInfo++;
            if (p_res->sizeCredentialInfo >= ARRAY_SIZE(p_res->CredentialInfo))
            {
                break;
            }
        }
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all CredentialInfo items provided by the device.
 *  
 *  A call to this method shall return a StartReference when not all data 
 *  is returned and more data is available. The reference shall be valid 
 *  for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 * @return
 *  The possible return values:
 *	ONVIF_ERR_InvalidStartReference
 **/
ONVIF_RET onvif_tcr_GetCredentialInfoList(tcr_GetCredentialInfoList_REQ * p_req, tcr_GetCredentialInfoList_RES * p_res)
{
    int idx;
    int nums = 0;
    CredentialList * p_tmp = g_onvif_cfg.credential;
    
    if (p_req->StartReferenceFlag)
    {
        p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->StartReference);
        if (NULL == p_tmp)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_tmp)
    {
        idx = p_res->sizeCredentialInfo;

        p_res->CredentialInfo[idx].DescriptionFlag = p_tmp->Credential.DescriptionFlag;
        p_res->CredentialInfo[idx].ValidFromFlag = p_tmp->Credential.ValidFromFlag;
        p_res->CredentialInfo[idx].ValidToFlag = p_tmp->Credential.ValidToFlag;

        strcpy(p_res->CredentialInfo[idx].ValidFrom, p_tmp->Credential.ValidFrom);
        strcpy(p_res->CredentialInfo[idx].ValidTo, p_tmp->Credential.ValidTo);
        strcpy(p_res->CredentialInfo[idx].token, p_tmp->Credential.token);
        strcpy(p_res->CredentialInfo[idx].Description, p_tmp->Credential.Description);
        strcpy(p_res->CredentialInfo[idx].CredentialHolderReference, p_tmp->Credential.CredentialHolderReference);
            
        p_res->sizeCredentialInfo++;

        p_tmp = p_tmp->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
        else if (nums >= ARRAY_SIZE(p_res->CredentialInfo))
        {
            break;
        }
    }

    if (p_tmp)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_tmp->Credential.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of Credential items matching the given tokens.
 *
 *  The device shall ignore tokens it cannot resolve and shall return an 
 *  empty list if there are no items matching the specified tokens. 
 *  The device shall not return a fault in this case.
 *
 *  If the number of requested items is greater than MaxLimit, a 
 *  TooManyItems fault shall be returned.
 *
 * @return
 *  The possible return values:
 *	ONVIF_ERR_TooManyItems
 **/
ONVIF_RET onvif_tcr_GetCredentials(tcr_GetCredentials_REQ * p_req, tcr_GetCredentials_RES * p_res)
{
    int i;
    int idx;
    CredentialList * p_tmp;
    
    for (i = 0; i< p_req->sizeToken; i++)
    {
        p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->Token[i]);
        if (p_tmp)
        {
            idx = p_res->sizeCredential;

            memcpy(&p_res->Credential[idx], &p_tmp->Credential, sizeof(onvif_Credential));
            
            p_res->sizeCredential++;
            if (p_res->sizeCredential >= ARRAY_SIZE(p_res->Credential))
            {
                break;
            }
        }
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all Credential items provided by the device.
 *
 *  A call to this method shall return a StartReference when not all data
 *  is returned and more data is available. The reference shall be valid 
 *  for retrieving the next set of data.
 *
 *  The number of items returned shall not be greater than the Limit parameter.
 *
 * @return
 *  The possible return values:
 *	ONVIF_ERR_InvalidStartReference
 **/
ONVIF_RET onvif_tcr_GetCredentialList(tcr_GetCredentialList_REQ * p_req, tcr_GetCredentialList_RES * p_res)
{
    int idx;
    int nums = 0;
    CredentialList * p_tmp = g_onvif_cfg.credential;
    
    if (p_req->StartReferenceFlag)
    {
        p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->StartReference);
        if (NULL == p_tmp)
        {
            return ONVIF_ERR_InvalidStartReference;
        }
    }

    while (p_tmp)
    {
        idx = p_res->sizeCredential;

        memcpy(&p_res->Credential[idx], &p_tmp->Credential, sizeof(onvif_Credential));
        
        p_res->sizeCredential++;

        p_tmp = p_tmp->next;

        nums++;
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
        else if (nums >= ARRAY_SIZE(p_res->Credential))
        {
            break;
        }
    }

    if (p_tmp)
    {
        p_res->NextStartReferenceFlag = 1;
        strcpy(p_res->NextStartReference, p_tmp->Credential.token);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Creates the specified credential in the device.
 *
 *  A call to this method takes a credential structure and a credential
 *  state structure as input parameters. The credential state can be created
 *  in disabled or enabled state.
 *
 *  The token field of the Credential structure shall be empty and the device
 *  shall allocate a token for the credential. The allocated token shall be 
 *  returned in the response.
 *
 *  If the client sends any value in the token field, the device shall return
 *  InvalidArgVal as a generic fault code.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_MaxAccessProfilesPerCredential
 *  ONVIF_ERR_CredentialValiditySupported
 *  ONVIF_ERR_CredentialAccessProfileValiditySupported
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_DuplicatedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 *  ONVIF_ERR_InvalidIdentifierValue
 *  ONVIF_ERR_DuplicatedIdentifierValue
 *  ONVIF_ERR_ReferenceNotFound
 *  ONVIF_ERR_ExemptFromAuthenticationSupported
 *  ONVIF_ERR_MaxCredentials
 **/
ONVIF_RET onvif_tcr_CreateCredential(tcr_CreateCredential_REQ * p_req, tcr_CreateCredential_RES * p_res)
{
    uint32 i;
    CredentialList * p_tmp;
    
    // parameter check
    if (strlen(p_req->Credential.token) > 0)
    {
        return ONVIF_ERR_InvalidArgVal;
    }

    // add new credential
    p_tmp = onvif_add_Credential(&g_onvif_cfg.credential);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_MaxCredentials;
    }

    strcpy(p_req->Credential.token, p_tmp->Credential.token);
    strcpy(p_res->Token, p_tmp->Credential.token);
    
    memcpy(&p_tmp->Credential, &p_req->Credential, sizeof(onvif_Credential));
    memcpy(&p_tmp->State, &p_req->State, sizeof(onvif_CredentialState));

    for (i = 0; i < p_req->Credential.sizeCredentialIdentifier; i++)
    {
        p_tmp->Credential.CredentialIdentifier[i].Used = 1;
    }

    for (i = 0; i < p_req->Credential.sizeCredentialAccessProfile; i++)
    {
        p_tmp->Credential.CredentialAccessProfile[i].Used = 1;
    }

    for (i = 0; i < p_req->Credential.sizeAttribute; i++)
    {
        p_tmp->Credential.Attribute[i].Used = 1;
    }
    
    // todo : here add handler code ...

    // send event notify
    onvif_CredentialChangedNotify(p_tmp);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies the specified credential.
 *
 *  The token of the credential to modify is specified in the token field of 
 *  the Credential structure and shall not be empty. All other fields in the
 *  structure shall overwrite the fields in the specified credential.
 *
 *  When an existing credential is modified, the state is not modified explicitly. 
 *  The only way for a client to change the state of a credential is to 
 *  explicitly call the EnableCredential, DisableCredential or ResetAntipassback
 *  command.
 *
 *  All existing credential identifiers and credential access profiles are 
 *  removed and replaced with the specified entities.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 *  ONVIF_ERR_CredentialValiditySupported
 *  ONVIF_ERR_CredentialAccessProfileValiditySupported
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_DuplicatedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 *  ONVIF_ERR_InvalidIdentifierValue
 *  ONVIF_ERR_DuplicatedIdentifierValue
 *  ONVIF_ERR_ReferenceNotFound
 *  ONVIF_ERR_ExemptFromAuthenticationSupported
 **/
ONVIF_RET onvif_tcr_ModifyCredential(tcr_ModifyCredential_REQ * p_req)
{
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->Credential.token);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }
    
    // parameter check

    // todo : here add handler code ...
    
    // modify the credential
    memcpy(&p_tmp->Credential, &p_req->Credential, sizeof(onvif_Credential));

    // send event notify
    onvif_CredentialChangedNotify(p_tmp);
            
    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes the specified credential.
 *
 *  If it is associated with one or more entities some devices may not be 
 *  able to delete the credential, and consequently a ReferenceInUse fault
 *  shall be generated.
 *
 *  If no token was specified in the request, the device shall return
 *  InvalidArgs as a generic fault code.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 *  ONVIF_ERR_ReferenceInUse
 **/
ONVIF_RET onvif_tcr_DeleteCredential(tcr_DeleteCredential_REQ * p_req)
{
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->Token);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ...

    // send event notify
    onvif_CredentialRemovedNotify(p_tmp);
        
    // delete the credential
    onvif_free_Credential(&g_onvif_cfg.credential, p_tmp);

    return ONVIF_OK;
}

/**
 * @brief
 *  Returns the state for the specified credential.
 *
 *  If the capability ResetAntipassbackSupported is set to true, then the 
 *  device shall supply the anti-passback state in the returned credential
 *  state structure.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_GetCredentialState(tcr_GetCredentialState_REQ * p_req, tcr_GetCredentialState_RES * p_res)
{
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->Token);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    memcpy(&p_res->State, &p_tmp->State, sizeof(onvif_CredentialState));
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Enable a credential.
 * 
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_EnableCredential(tcr_EnableCredential_REQ * p_req)
{
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->Token);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ... 

    // endable the credentail
    
    p_tmp->State.Enabled = TRUE;
    if (p_req->ReasonFlag)
    {
        p_tmp->State.ReasonFlag = 1;
        snprintf(p_tmp->State.Reason, sizeof(p_tmp->State.Reason), "%.*s",
                 (int)(sizeof(p_tmp->State.Reason) - 1), p_req->Reason);
    }

    // send event notify
    onvif_CredentialStateNotify(p_tmp, TRUE);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Disable a credential.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_DisableCredential(tcr_DisableCredential_REQ * p_req)
{
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->Token);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ... 

    // disable the credentail
    
    p_tmp->State.Enabled = FALSE;
    if (p_req->ReasonFlag)
    {
        p_tmp->State.ReasonFlag = 1;
        snprintf(p_tmp->State.Reason, sizeof(p_tmp->State.Reason), "%.*s",
                 (int)(sizeof(p_tmp->State.Reason) - 1), p_req->Reason);
    }

    // send event notify
    onvif_CredentialStateNotify(p_tmp, TRUE);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Synchronize a credential in a client with the device.
 *
 *  A call to this method takes a credential structure and a credential 
 *  state structure as input parameters. The token field of the credential 
 *  must not be empty
 *
 *  If a credential with the specified token does not exist in the device, 
 *  the credential is created. The credential state can be created in 
 *  disabled or enabled state.
 *
 *  If a credential with the specified token exists in the device, then the
 *  credential is modified. The credential state will be set according to 
 *  the specified state (this behavior differs from the ModifyCredential 
 *  command). All existing credential identifiers and credential access 
 *  profiles are removed and replaced with the specified entities.
 *
 *  A device that signals support for the ClientSuppliedTokenSupported 
 *  capability shall implement this command.
 *
 *  If no token was specified in the request, the device shall return 
 *  InvalidArgs as a generic fault code.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_ClientSuppliedTokenSupported
 *  ONVIF_ERR_MaxCredentials
 *  ONVIF_ERR_MaxAccessProfilesPerCredential
 *  ONVIF_ERR_CredentialValiditySupported
 *  ONVIF_ERR_CredentialAccessProfileValiditySupported
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_DuplicatedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 *  ONVIF_ERR_DuplicatedIdentifierValue
 *  ONVIF_ERR_ReferenceNotFound
 **/
ONVIF_RET onvif_tcr_SetCredential(tcr_SetCredential_REQ * p_req)
{
	uint32 i;
    
    if (p_req->CredentialData.Credential.token[0] == '\0')
    {
        return ONVIF_ERR_InvalidArgs;
    }
    
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialData.Credential.token);
    if (NULL == p_tmp)
    {
        // add new credential
        p_tmp = onvif_add_Credential(&g_onvif_cfg.credential);
        if (NULL == p_tmp)
        {
            return ONVIF_ERR_MaxCredentials;
        }

        memcpy(&p_tmp->Credential, &p_req->CredentialData.Credential, sizeof(onvif_Credential));
        memcpy(&p_tmp->State, &p_req->CredentialData.CredentialState, sizeof(onvif_CredentialState));

        for (i = 0; i < p_req->CredentialData.Credential.sizeCredentialIdentifier; i++)
        {
            p_tmp->Credential.CredentialIdentifier[i].Used = 1;
        }

        for (i = 0; i < p_req->CredentialData.Credential.sizeCredentialAccessProfile; i++)
        {
            p_tmp->Credential.CredentialAccessProfile[i].Used = 1;
        }

        for (i = 0; i < p_req->CredentialData.Credential.sizeAttribute; i++)
        {
            p_tmp->Credential.Attribute[i].Used = 1;
        }
        
        // todo : here add handler code ...

        // send event notify
        onvif_CredentialChangedNotify(p_tmp);
    }
    else
    {
        memcpy(&p_tmp->Credential, &p_req->CredentialData.Credential, sizeof(onvif_Credential));
        memcpy(&p_tmp->State, &p_req->CredentialData.CredentialState, sizeof(onvif_CredentialState));

        for (i = 0; i < p_req->CredentialData.Credential.sizeCredentialIdentifier; i++)
        {
            p_tmp->Credential.CredentialIdentifier[i].Used = 1;
        }

        for (i = 0; i < p_req->CredentialData.Credential.sizeCredentialAccessProfile; i++)
        {
            p_tmp->Credential.CredentialAccessProfile[i].Used = 1;
        }

        for (i = 0; i < p_req->CredentialData.Credential.sizeAttribute; i++)
        {
            p_tmp->Credential.Attribute[i].Used = 1;
        }
        
        // todo : here add handler code ...

        // send event notify    
        onvif_CredentialStateNotify(p_tmp, TRUE);

        onvif_CredentialChangedNotify(p_tmp);
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Reset anti-passback violations for a specified credential.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_ResetAntipassbackViolation(tcr_ResetAntipassbackViolation_REQ * p_req)
{
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialToken);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    // todo : here add handler code ... 

    // reset AntipassbackViolation

    if (p_tmp->State.AntipassbackStateFlag)
    {
        p_tmp->State.AntipassbackState.AntipassbackViolated = FALSE;
    }

    // send event notify
    onvif_CredentialApbViolationNotify(p_tmp, TRUE);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Returns all the supported format types of a specified identifier type
 *  that is supported by the device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_GetSupportedFormatTypes(tcr_GetSupportedFormatTypes_REQ * p_req, tcr_GetSupportedFormatTypes_RES * p_res)
{
    p_res->sizeFormatTypeInfo = 2;
    strcpy(p_res->FormatTypeInfo[0].FormatType, "GUID");
    strcpy(p_res->FormatTypeInfo[0].Description, "test");
    strcpy(p_res->FormatTypeInfo[1].FormatType, "CHUID");
    strcpy(p_res->FormatTypeInfo[1].Description, "test1");

    //strcpy(p_res->FormatTypeInfo[0].FormatType, "USER_PASSWORD");
    //strcpy(p_res->FormatTypeInfo[0].FormatType, "WIEGAND26");
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Returns all the credential identifiers for a credential.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_GetCredentialIdentifiers(tcr_GetCredentialIdentifiers_REQ * p_req, tcr_GetCredentialIdentifiers_RES * p_res)
{
    int i;
    int idx = 0;
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialToken);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    p_res->sizeCredentialIdentifier = p_tmp->Credential.sizeCredentialIdentifier;

    for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialIdentifier); i++)
    {
        if (!p_tmp->Credential.CredentialIdentifier[i].Used)
        {
            continue;
        }

        memcpy(&p_res->CredentialIdentifier[idx++], &p_tmp->Credential.CredentialIdentifier[i], sizeof(onvif_CredentialIdentifier));
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Creates or updates a credential identifier for a credential.
 *
 *  If the type of specified credential identifier already exists, the current
 *  credential identifier of that type is replaced. Otherwise the credential 
 *  identifier is added.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 *  ONVIF_ERR_InvalidIdentifierValue
 *  ONVIF_ERR_ExemptFromAuthenticationSupported
 **/
ONVIF_RET onvif_tcr_SetCredentialIdentifier(tcr_SetCredentialIdentifier_REQ * p_req)
{
    int i;
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialToken);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialIdentifier); i++)
    {
        if (!p_tmp->Credential.CredentialIdentifier[i].Used)
        {
            continue;
        }
        
        if (strcmp(p_tmp->Credential.CredentialIdentifier[i].Type.Name, p_req->CredentialIdentifier.Type.Name) == 0)
        {
            // update the credential identifier
            memcpy(&p_tmp->Credential.CredentialIdentifier[i], &p_req->CredentialIdentifier, sizeof(onvif_CredentialIdentifier));
			p_tmp->Credential.CredentialIdentifier[i].Used = TRUE; 

            // send event notify
            onvif_CredentialChangedNotify(p_tmp);

            // todo : here add handler code ...

            return ONVIF_OK;
        }
    }

    for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialIdentifier); i++)
    {
        if (p_tmp->Credential.CredentialIdentifier[i].Used)
        {
            continue;
        }
        
        // add the credential identifier
        p_tmp->Credential.sizeCredentialIdentifier++;               
        memcpy(&p_tmp->Credential.CredentialIdentifier[i], &p_req->CredentialIdentifier, sizeof(onvif_CredentialIdentifier));

        p_tmp->Credential.CredentialIdentifier[i].Used = TRUE; 

        // send event notify
        onvif_CredentialChangedNotify(p_tmp);
            
        // todo : here add handler code ...     

        break;
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  deletes all the identifier values for the specified type. However, 
 *  if the identifier type name doesn't exist in the device, it will be 
 *  silently ignored without any response.
 *
 *  Note that each credential needs at least one identifier and an attempt
 *  to delete the last identifier will result in a fault.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 *  ONVIF_ERR_MinIdentifiersPerCredential
 **/
ONVIF_RET onvif_tcr_DeleteCredentialIdentifier(tcr_DeleteCredentialIdentifier_REQ * p_req)
{
    int i;
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialToken);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialIdentifier); i++)
    {
        if (!p_tmp->Credential.CredentialIdentifier[i].Used)
        {
            continue;
        }
        
        if (strcmp(p_tmp->Credential.CredentialIdentifier[i].Type.Name, p_req->CredentialIdentifierTypeName) == 0)
        {
            // delete the credential identifier
            
            if (p_tmp->Credential.sizeCredentialIdentifier > 1)
            {
                p_tmp->Credential.sizeCredentialIdentifier--;
                p_tmp->Credential.CredentialIdentifier[i].Used = FALSE;

                // send event notify
                onvif_CredentialChangedNotify(p_tmp);
            }
            else
            {
                return ONVIF_ERR_MinIdentifiersPerCredential;
            }

            // todo : here add handler code ...

            
        }
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Returns all the credential access profiles for a credential.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_GetCredentialAccessProfiles(tcr_GetCredentialAccessProfiles_REQ * p_req, tcr_GetCredentialAccessProfiles_RES * p_res)
{
    int i;
    int idx = 0;
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialToken);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    p_res->sizeCredentialAccessProfile = p_tmp->Credential.sizeCredentialAccessProfile;

    for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialAccessProfile); i++)
    {
        if (!p_tmp->Credential.CredentialAccessProfile[i].Used)
        {
            continue;
        }

        memcpy(&p_res->CredentialAccessProfile[idx++], &p_tmp->Credential.CredentialAccessProfile[i], sizeof(onvif_CredentialAccessProfile));
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Adds or updates the credential access profiles for a credential.
 *
 *  The device shall update the credential access profile if the access
 *  profile token in the specified credential access profile matches. 
 *  Otherwise the credential access profile is added.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 *  ONVIF_ERR_MaxAccessProfilesPerCredential
 *  ONVIF_ERR_CredentialAccessProfileValiditySupported
 *  ONVIF_ERR_ReferenceNotFound
 **/
ONVIF_RET onvif_tcr_SetCredentialAccessProfiles(tcr_SetCredentialAccessProfiles_REQ * p_req)
{
    int i, j;
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialToken);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    for (j = 0; j < p_req->sizeCredentialAccessProfile; j++)
    {
        for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialAccessProfile); i++)
        {
            if (!p_tmp->Credential.CredentialAccessProfile[i].Used)
            {
                continue;
            }
            
            if (strcmp(p_tmp->Credential.CredentialAccessProfile[i].AccessProfileToken, p_req->CredentialAccessProfile[j].AccessProfileToken) == 0)
            {
                // update the credential Access Profile
                memcpy(&p_tmp->Credential.CredentialAccessProfile[i], &p_req->CredentialAccessProfile[j], sizeof(onvif_CredentialAccessProfile));
                p_tmp->Credential.CredentialAccessProfile[i].Used = 1;

                // todo : here add handler code ...

                // send event notify
                onvif_CredentialChangedNotify(p_tmp);
            
                return ONVIF_OK;
            }
        }

        for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialAccessProfile); i++)
        {
            if (p_tmp->Credential.CredentialAccessProfile[i].Used)
            {
                continue;
            }
            
            // add the credential identifier
            if (p_tmp->Credential.sizeCredentialAccessProfile < ARRAY_SIZE(p_tmp->Credential.CredentialAccessProfile))
            {
                p_tmp->Credential.sizeCredentialAccessProfile++;                        
                memcpy(&p_tmp->Credential.CredentialAccessProfile[i], &p_req->CredentialAccessProfile[j], sizeof(onvif_CredentialAccessProfile));
                p_tmp->Credential.CredentialAccessProfile[i].Used = TRUE;
                
                // todo : here add handler code ...

                // send event notify
                onvif_CredentialChangedNotify(p_tmp);
            }
            else
            {
                
            }            
            
            break;
        }
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes credential access profiles for the specified credential token.
 *
 *  However, if no matching credential access profiles are found, the 
 *  corresponding access profile tokens are silently ignored without any 
 *  response.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotFound
 **/
ONVIF_RET onvif_tcr_DeleteCredentialAccessProfiles(tcr_DeleteCredentialAccessProfiles_REQ * p_req)
{
    int i, j;
    CredentialList * p_tmp = onvif_find_Credential(g_onvif_cfg.credential, p_req->CredentialToken);
    if (NULL == p_tmp)
    {
        return ONVIF_ERR_NotFound;
    }

    for (j = 0; j < p_req->sizeAccessProfileToken; j++)
    {
        for (i = 0; i < ARRAY_SIZE(p_tmp->Credential.CredentialAccessProfile); i++)
        {
            if (!p_tmp->Credential.CredentialAccessProfile[i].Used)
            {
                continue;
            }
            
            if (strcmp(p_tmp->Credential.CredentialAccessProfile[i].AccessProfileToken, p_req->AccessProfileToken[j]) == 0)
            {
                // delete the credential Access Profile
                
                p_tmp->Credential.sizeCredentialAccessProfile--;
                p_tmp->Credential.CredentialAccessProfile[i].Used = FALSE;

                // todo : here add handler code ...

                // send event notify
                onvif_CredentialChangedNotify(p_tmp);
            }
        }
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all whitelisted credential identifiers in the device.
 *
 *  A device with capability MaxWhitelistedItems greater than zero, shall 
 *  implement this command.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidStartReference
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 **/
ONVIF_RET onvif_tcr_GetWhitelist(tcr_GetWhitelist_REQ * p_req, tcr_GetWhitelist_RES * p_res)
{
    int idx = 0;
    int nums = 0;
    int start  = 0;
    CredentialIdentifierItemList * p_tmp = g_onvif_cfg.whiltlist;
    CredentialIdentifierItemList * p_item;
    
    if (p_req->StartReferenceFlag)
    {
        start = atoi(p_req->StartReference);

        while (p_tmp && idx++ < start)
        {
            p_tmp = p_tmp->next;
        }
    }

    while (p_tmp)
    {
        idx++;

        if (p_req->IdentifierTypeFlag)
        {
            if (strcmp(p_req->IdentifierType, p_tmp->Item.Type.Name) != 0)
            {
                p_tmp = p_tmp->next;
                continue;
            }
        }
        
        if (p_req->FormatTypeFlag)
        {
            if (strcmp(p_req->FormatType, p_tmp->Item.Type.FormatType) != 0)
            {
                p_tmp = p_tmp->next;
                continue;
            }
        }

        if (p_req->ValueFlag)
        {
            if (strcmp(p_req->Value, p_tmp->Item.Value) != 0)
            {
                p_tmp = p_tmp->next;
                continue;
            }
        }
        
        p_item = onvif_add_CredentialIdentifierItem(&p_res->Identifier);
        if (p_item)
        {
            memcpy(&p_item->Item, &p_tmp->Item, sizeof(onvif_CredentialIdentifierItem));
        }
        
        p_tmp = p_tmp->next;

        nums++;
        
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_tmp)
    {
        p_res->NextStartReferenceFlag = 1;
        sprintf(p_res->NextStartReference, "%d", idx);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Adds the specified credential identifiers to the whitelist.
 *
 *  A device with capability MaxWhitelistedItems greater than zero, shall
 *  implement this command.
 *
 *  If a specified whitelist item also is blacklisted, the item shall be
 *  removed from the blacklist.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_MaxWhitelistedItems
 *  ONVIF_ERR_TooManyItems
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 **/
ONVIF_RET onvif_tcr_AddToWhitelist(tcr_AddToWhitelist_REQ * p_req)
{
    CredentialIdentifierItemList * p_tmp = g_onvif_cfg.whiltlist;
    while (p_tmp && p_tmp->next)
    {
        p_tmp = p_tmp->next;
    }

    if (p_tmp)
    {
        p_tmp->next = p_req->Identifier;
    }
    else
    {
        g_onvif_cfg.whiltlist = p_req->Identifier;
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Removes the specified credential identifiers from the whitelist.
 *
 *  A device with capability MaxWhitelistedItems greater than zero, 
 *  shall implement this command.
 *
 *  This command is idempotent and is safe to repeat even if the 
 *  specified whitelist items do not exist.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_TooManyItems
 **/
ONVIF_RET onvif_tcr_RemoveFromWhitelist(tcr_RemoveFromWhitelist_REQ * p_req)
{
    CredentialIdentifierItemList * p_tmp = p_req->Identifier;
    while (p_tmp)
    {
        onvif_free_CredentialIdentifierItem(&g_onvif_cfg.whiltlist, p_tmp);

        p_tmp = p_tmp->next;
    }

    onvif_free_CredentialIdentifierItems(&p_req->Identifier);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes all credential identifiers from the whitelist.
 *
 *  A device with capability MaxWhitelistedItems greater than zero, 
 *  shall implement this command.
 *
 *  This command is idempotent and is safe to repeat even if the 
 *  whitelist already is empty.
 *  
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tcr_DeleteWhitelist()
{
    onvif_free_CredentialIdentifierItems(&g_onvif_cfg.whiltlist);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Requests a list of all blacklisted credential identifiers in the device.
 *
 *  A device with capability MaxBlacklistedItems greater than zero, shall 
 *  implement this command.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidStartReference
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 **/
ONVIF_RET onvif_tcr_GetBlacklist(tcr_GetBlacklist_REQ * p_req, tcr_GetBlacklist_RES * p_res)
{
    int idx = 0;
    int nums = 0;
    int start  = 0;
    CredentialIdentifierItemList * p_tmp = g_onvif_cfg.blacklist;
    CredentialIdentifierItemList * p_item;
    
    if (p_req->StartReferenceFlag)
    {
        start = atoi(p_req->StartReference);

        while (p_tmp && idx++ < start)
        {
            p_tmp = p_tmp->next;
        }
    }

    while (p_tmp)
    {
        idx++;

        if (p_req->IdentifierTypeFlag)
        {
            if (strcmp(p_req->IdentifierType, p_tmp->Item.Type.Name) != 0)
            {
                p_tmp = p_tmp->next;
                continue;
            }
        }
        
        if (p_req->FormatTypeFlag)
        {
            if (strcmp(p_req->FormatType, p_tmp->Item.Type.FormatType) != 0)
            {
                p_tmp = p_tmp->next;
                continue;
            }
        }

        if (p_req->ValueFlag)
        {
            if (strcmp(p_req->Value, p_tmp->Item.Value) != 0)
            {
                p_tmp = p_tmp->next;
                continue;
            }
        }
        
        p_item = onvif_add_CredentialIdentifierItem(&p_res->Identifier);
        if (p_item)
        {
            memcpy(&p_item->Item, &p_tmp->Item, sizeof(onvif_CredentialIdentifierItem));
        }
        
        p_tmp = p_tmp->next;

        nums++;
        
        if (p_req->LimitFlag && nums >= p_req->Limit)
        {
            break;
        }
    }

    if (p_tmp)
    {
        p_res->NextStartReferenceFlag = 1;
        sprintf(p_res->NextStartReference, "%d", idx);
    }    
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Adds the specified credential identifiers to the blacklist.
 *
 *  A device with capability MaxBlacklistedItems greater than zero, 
 *  shall implement this command.
 *
 *  If a specified blacklist item also is whitelisted, the item shall
 *  be removed from the whitelist.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_MaxBlacklistedItems
 *  ONVIF_ERR_TooManyItems
 *  ONVIF_ERR_SupportedIdentifierType
 *  ONVIF_ERR_InvalidFormatType
 **/
ONVIF_RET onvif_tcr_AddToBlacklist(tcr_AddToBlacklist_REQ * p_req)
{
    CredentialIdentifierItemList * p_tmp = g_onvif_cfg.blacklist;
    while (p_tmp && p_tmp->next)
    {
        p_tmp = p_tmp->next;
    }

    if (p_tmp)
    {
        p_tmp->next = p_req->Identifier;
    }
    else
    {
        g_onvif_cfg.whiltlist = p_req->Identifier;
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Removes the specified credential identifiers from the blacklist.
 *
 *  A device with capability MaxBlacklistedItems greater than zero, 
 *  shall implement this command.
 *
 *  This command is idempotent and is safe to repeat even if the 
 *  specified blacklist items do not exist.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_TooManyItems
 **/
ONVIF_RET onvif_tcr_RemoveFromBlacklist(tcr_RemoveFromBlacklist_REQ * p_req)
{
    CredentialIdentifierItemList * p_tmp = p_req->Identifier;
    while (p_tmp)
    {
        onvif_free_CredentialIdentifierItem(&g_onvif_cfg.blacklist, p_tmp);

        p_tmp = p_tmp->next;
    }
    
    onvif_free_CredentialIdentifierItems(&p_req->Identifier);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Deletes all credential identifiers from the blacklist.
 *
 *  A device with capability MaxBlacklistedItems greater than zero, 
 *  shall implement this command.
 *
 *  This command is idempotent and is safe to repeat even if the 
 *  blacklist already is empty.
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tcr_DeleteBlacklist()
{
    onvif_free_CredentialIdentifierItems(&g_onvif_cfg.blacklist);
    
    return ONVIF_OK;
}

#endif // end of  CREDENTAIL_SUPPORT


