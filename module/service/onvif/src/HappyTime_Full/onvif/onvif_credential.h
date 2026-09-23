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

#ifndef ONVIF_CREDENTIAL_H
#define ONVIF_CREDENTIAL_H

#include "sys_inc.h"
#include "onvif_cm.h"
#include "onvif.h"


typedef struct 
{
	int     sizeToken;	                                // sequence of elements <Token>
	char    Token[CREDENTIAL_MAX_LIMIT][ONVIF_TOKEN_LEN];   // required, Tokens of CredentialInfo items to get
} tcr_GetCredentialInfo_REQ;

typedef struct 
{
	int     sizeCredentialInfo;	                        // sequence of elements <CredentialInfo>

	onvif_CredentialInfo CredentialInfo[CREDENTIAL_MAX_LIMIT];  // optional, List of CredentialInfo items
} tcr_GetCredentialInfo_RES;

typedef struct 
{
    uint32  LimitFlag           : 1;                    // Indicates whether the field Limit is valid
    uint32  StartReferenceFlag  : 1;                    // Indicates whether the field StartReference is valid
    uint32  Reserved            : 30;
    
	int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, less than one or higher 
	                                                    //  than what the device supports, the number of items is determined by the device
	char    StartReference[ONVIF_TOKEN_LEN];	        // optional, Start returning entries from this start reference. If not specified,
									                    //  entries shall start from the beginning of the dataset
} tcr_GetCredentialInfoList_REQ;
 
typedef struct 
{
    uint32  NextStartReferenceFlag  : 1;                // Indicates whether the field StartReference is valid
    uint32  Reserved                : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];        // optional, StartReference to use in next call to get the following items. If
									                    //  absent, no more items to get
    int     sizeCredentialInfo;                         // sequence of elements <CredentialInfo>

    onvif_CredentialInfo CredentialInfo[CREDENTIAL_MAX_LIMIT];  // optional, List of CredentialInfo items
} tcr_GetCredentialInfoList_RES;

typedef struct 
{
	int     sizeToken;	                                // sequence of elements <Token> 
	char    Token[CREDENTIAL_MAX_LIMIT][ONVIF_TOKEN_LEN];   // required, Token of Credentials to get
} tcr_GetCredentials_REQ;

typedef struct 
{
	int     sizeCredential;	                            // sequence of elements <Credential>
	
	onvif_Credential    Credential[CREDENTIAL_MAX_LIMIT];   // optional, List of Credential items
} tcr_GetCredentials_RES;

typedef struct 
{
    uint32  LimitFlag           : 1;                    // Indicates whether the field Limit is valid
    uint32  StartReferenceFlag  : 1;                    // Indicates whether the field StartReference is valid
    uint32  Reserved            : 30;
    
	int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, less than one or higher 
	                                                    //  than what the device supports, the number of items is determined by the device
	char    StartReference[ONVIF_TOKEN_LEN];	        // optional, Start returning entries from this start reference. If not specified,
									                    //  entries shall start from the beginning of the dataset
} tcr_GetCredentialList_REQ;

typedef struct 
{
    uint32  NextStartReferenceFlag  : 1;                // Indicates whether the field StartReference is valid
    uint32  Reserved                : 31;
    
	char    NextStartReference[ONVIF_TOKEN_LEN];	    // optional, StartReference to use in next call to get the following items. If
									                    //  absent, no more items to get
	int     sizeCredential;	                            // sequence of elements <Credential>
    onvif_Credential Credential[CREDENTIAL_MAX_LIMIT];  // optional, List of Credential items
} tcr_GetCredentialList_RES;

typedef struct 
{
	onvif_Credential        Credential;	                // required, The credential to create
	onvif_CredentialState   State;	                    // required, The state of the credential
} tcr_CreateCredential_REQ;

typedef struct 
{
	char    Token[ONVIF_TOKEN_LEN];	                    // required, The token of the created credential
} tcr_CreateCredential_RES;

typedef struct 
{
	onvif_Credential    Credential;	                    // required, Details of the credential
} tcr_ModifyCredential_REQ;

typedef struct 
{
	char    Token[ONVIF_TOKEN_LEN];	                    // required, The token of the credential to delete
} tcr_DeleteCredential_REQ;

typedef struct 
{
	char    Token[ONVIF_TOKEN_LEN];	                    // required, Token of Credential
} tcr_GetCredentialState_REQ;

typedef struct 
{
	onvif_CredentialState   State;	                    // required, State of the credential
} tcr_GetCredentialState_RES;

typedef struct 
{
    uint32  ReasonFlag  : 1;                            // Indicates whether the field ReasonFlag is valid
    uint32  Reserved    : 31;
    
	char    Token[ONVIF_TOKEN_LEN];	                    // required, The token of the credential
	char    Reason[200];	                            // optional, Reason for enabling the credential
} tcr_EnableCredential_REQ;

typedef struct 
{
    uint32  ReasonFlag  : 1;                            // Indicates whether the field ReasonFlag is valid
    uint32  Reserved    : 31;
    
	char    Token[ONVIF_TOKEN_LEN];	                    // required, The token of the credential
	char    Reason[200];	                            // optional, Reason for disabling the credential
} tcr_DisableCredential_REQ;

typedef struct 
{
    onvif_CredentialData    CredentialData;             // required, Details of the credential
} tcr_SetCredential_REQ;

typedef struct 
{
	char    CredentialToken[ONVIF_TOKEN_LEN];	        // required, Token of the Credential
} tcr_ResetAntipassbackViolation_REQ;

typedef struct 
{
	char    CredentialIdentifierTypeName[100];	        // required, Name of the credential identifier type
} tcr_GetSupportedFormatTypes_REQ;

typedef struct 
{
	int     sizeFormatTypeInfo;	                        // sequence of elements <FormatTypeInfo>

	onvif_CredentialIdentifierFormatTypeInfo FormatTypeInfo[CREDENTIAL_MAX_LIMIT];  // required, Identifier format types
} tcr_GetSupportedFormatTypes_RES;

typedef struct 
{
	char    CredentialToken[ONVIF_TOKEN_LEN];	        // required, Token of the Credential
} tcr_GetCredentialIdentifiers_REQ;

typedef struct 
{
	int     sizeCredentialIdentifier;	                // sequence of elements <CredentialIdentifier> 

	onvif_CredentialIdentifier CredentialIdentifier[CREDENTIAL_MAX_LIMIT];	// optional, Identifiers of the credential
} tcr_GetCredentialIdentifiers_RES;

typedef struct 
{
	char    CredentialToken[ONVIF_TOKEN_LEN];	        // required, Token of the Credential
	
	onvif_CredentialIdentifier CredentialIdentifier;	// required, Identifier of the credential
} tcr_SetCredentialIdentifier_REQ;

typedef struct 
{
	char    CredentialToken[ONVIF_TOKEN_LEN];	        // required, Token of the Credential
	char    CredentialIdentifierTypeName[100];          // required, Identifier type name of a credential
} tcr_DeleteCredentialIdentifier_REQ;

typedef struct 
{
	char    CredentialToken[ONVIF_TOKEN_LEN];	        // required, Token of the Credential
} tcr_GetCredentialAccessProfiles_REQ;

typedef struct 
{
	int     sizeCredentialAccessProfile;	            // sequence of elements <CredentialAccessProfile>
	
	onvif_CredentialAccessProfile CredentialAccessProfile[CREDENTIAL_MAX_LIMIT];    // optional, Access Profiles of the credential
} tcr_GetCredentialAccessProfiles_RES;

typedef struct 
{
	char    CredentialToken[ONVIF_TOKEN_LEN];	        // required, Token of the Credential
	int     sizeCredentialAccessProfile;	            // sequence of elements <CredentialAccessProfile>

	onvif_CredentialAccessProfile CredentialAccessProfile[CREDENTIAL_MAX_LIMIT];    // required, Access Profiles of the credential
} tcr_SetCredentialAccessProfiles_REQ;

typedef struct 
{
	char    CredentialToken[ONVIF_TOKEN_LEN];	        // required, Token of the Credential
	int     sizeAccessProfileToken;	                    // sequence of elements <CredentialAccessProfile>

	char    AccessProfileToken[CREDENTIAL_MAX_LIMIT][ONVIF_TOKEN_LEN];	// required, Tokens of Access Profiles
} tcr_DeleteCredentialAccessProfiles_REQ;

typedef struct 
{
    uint32  LimitFlag           : 1;                    // Indicates whether the field Limit is valid
    uint32  StartReferenceFlag  : 1;                    // Indicates whether the field StartReference is valid
    uint32  IdentifierTypeFlag  : 1;                    // Indicates whether the field IdentifierType is valid
    uint32  FormatTypeFlag      : 1;                    // Indicates whether the field FormatType is valid
    uint32  ValueFlag           : 1;                    // Indicates whether the field Value is valid
    uint32  Reserved            : 27;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, less than one or higher than what the device supports, the number of items is determined by the device
	char    StartReference[ONVIF_TOKEN_LEN];	        // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
	char    IdentifierType[128];	                    // optional, Get only whitelisted credential identifiers with the specified identifier type
	char    FormatType[128];	                        // optional, Get only whitelisted credential identifiers with the specified identifier format type
	char    Value[2048];	                            // optional, Get only whitelisted credential identifiers with the specified identifier value
} tcr_GetWhitelist_REQ;

typedef struct 
{
    uint32  NextStartReferenceFlag  : 1;                // Indicates whether the field NextStartReference is valid
    uint32  Reserved                : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];	    // optional, StartReference to use in next call to get the following items. If absent, no more items to get

    CredentialIdentifierItemList * Identifier;          // optional, The whitelisted credential identifiers matching the request criteria 
} tcr_GetWhitelist_RES;

typedef struct 
{
    CredentialIdentifierItemList * Identifier;          // required, The credential identifiers to be added to the whitelist
} tcr_AddToWhitelist_REQ;

typedef struct 
{
    CredentialIdentifierItemList * Identifier;          // required, The credential identifiers to be removed from the whitelist
} tcr_RemoveFromWhitelist_REQ;

typedef struct 
{
    uint32  LimitFlag           : 1;                    // Indicates whether the field Limit is valid
    uint32  StartReferenceFlag  : 1;                    // Indicates whether the field StartReference is valid
    uint32  IdentifierTypeFlag  : 1;                    // Indicates whether the field IdentifierType is valid
    uint32  FormatTypeFlag      : 1;                    // Indicates whether the field FormatType is valid
    uint32  ValueFlag           : 1;                    // Indicates whether the field Value is valid
    uint32  Reserved            : 27;
    
    int     Limit;	                                    // optional, Maximum number of entries to return. If not specified, less than one or higher than what the device supports, the number of items is determined by the device
	char    StartReference[ONVIF_TOKEN_LEN];	        // optional, Start returning entries from this start reference. If not specified, entries shall start from the beginning of the dataset
	char    IdentifierType[128];	                    // optional, Get only whitelisted credential identifiers with the specified identifier type
	char    FormatType[128];	                        // optional, Get only whitelisted credential identifiers with the specified identifier format type
	char    Value[2048];	                            // optional, Get only whitelisted credential identifiers with the specified identifier value
} tcr_GetBlacklist_REQ;

typedef struct 
{
    uint32  NextStartReferenceFlag  : 1;                // Indicates whether the field NextStartReference is valid
    uint32  Reserved                : 31;
    
    char    NextStartReference[ONVIF_TOKEN_LEN];	    // optional, StartReference to use in next call to get the following items. If absent, no more items to get

    CredentialIdentifierItemList * Identifier;          // optional, The blacklisted credential identifiers matching the request criteria   
} tcr_GetBlacklist_RES;

typedef struct 
{
    CredentialIdentifierItemList * Identifier;          // required, The credential identifiers to be added to the blacklist
} tcr_AddToBlacklist_REQ;

typedef struct 
{
    CredentialIdentifierItemList * Identifier;          // required, The credential identifiers to be removed from the blacklist  
} tcr_RemoveFromBlacklist_REQ;


#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET onvif_tcr_GetCredentialInfo(tcr_GetCredentialInfo_REQ * p_req, tcr_GetCredentialInfo_RES * p_res);
ONVIF_RET onvif_tcr_GetCredentialInfoList(tcr_GetCredentialInfoList_REQ * p_req, tcr_GetCredentialInfoList_RES * p_res);
ONVIF_RET onvif_tcr_GetCredentials(tcr_GetCredentials_REQ * p_req, tcr_GetCredentials_RES * p_res);
ONVIF_RET onvif_tcr_GetCredentialList(tcr_GetCredentialList_REQ * p_req, tcr_GetCredentialList_RES * p_res);
ONVIF_RET onvif_tcr_CreateCredential(tcr_CreateCredential_REQ * p_req, tcr_CreateCredential_RES * p_res);
ONVIF_RET onvif_tcr_ModifyCredential(tcr_ModifyCredential_REQ * p_req);
ONVIF_RET onvif_tcr_DeleteCredential(tcr_DeleteCredential_REQ * p_req);
ONVIF_RET onvif_tcr_GetCredentialState(tcr_GetCredentialState_REQ * p_req, tcr_GetCredentialState_RES * p_res);
ONVIF_RET onvif_tcr_EnableCredential(tcr_EnableCredential_REQ * p_req);
ONVIF_RET onvif_tcr_DisableCredential(tcr_DisableCredential_REQ * p_req);
ONVIF_RET onvif_tcr_SetCredential(tcr_SetCredential_REQ * p_req);
ONVIF_RET onvif_tcr_ResetAntipassbackViolation(tcr_ResetAntipassbackViolation_REQ * p_req);
ONVIF_RET onvif_tcr_GetSupportedFormatTypes(tcr_GetSupportedFormatTypes_REQ * p_req, tcr_GetSupportedFormatTypes_RES * p_res);
ONVIF_RET onvif_tcr_GetCredentialIdentifiers(tcr_GetCredentialIdentifiers_REQ * p_req, tcr_GetCredentialIdentifiers_RES * p_res);
ONVIF_RET onvif_tcr_SetCredentialIdentifier(tcr_SetCredentialIdentifier_REQ * p_req);
ONVIF_RET onvif_tcr_DeleteCredentialIdentifier(tcr_DeleteCredentialIdentifier_REQ * p_req);
ONVIF_RET onvif_tcr_GetCredentialAccessProfiles(tcr_GetCredentialAccessProfiles_REQ * p_req, tcr_GetCredentialAccessProfiles_RES * p_res);
ONVIF_RET onvif_tcr_SetCredentialAccessProfiles(tcr_SetCredentialAccessProfiles_REQ * p_req);
ONVIF_RET onvif_tcr_DeleteCredentialAccessProfiles(tcr_DeleteCredentialAccessProfiles_REQ * p_req);
ONVIF_RET onvif_tcr_GetWhitelist(tcr_GetWhitelist_REQ * p_req, tcr_GetWhitelist_RES * p_res);
ONVIF_RET onvif_tcr_AddToWhitelist(tcr_AddToWhitelist_REQ * p_req);
ONVIF_RET onvif_tcr_RemoveFromWhitelist(tcr_RemoveFromWhitelist_REQ * p_req);
ONVIF_RET onvif_tcr_DeleteWhitelist();
ONVIF_RET onvif_tcr_GetBlacklist(tcr_GetBlacklist_REQ * p_req, tcr_GetBlacklist_RES * p_res);
ONVIF_RET onvif_tcr_AddToBlacklist(tcr_AddToBlacklist_REQ * p_req);
ONVIF_RET onvif_tcr_RemoveFromBlacklist(tcr_RemoveFromBlacklist_REQ * p_req);
ONVIF_RET onvif_tcr_DeleteBlacklist();

#ifdef __cplusplus
}
#endif

#endif // ONVIF_CREDENTIAL_H



