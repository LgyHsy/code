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

#ifndef ONVIF_ANALYTICS_H
#define ONVIF_ANALYTICS_H


/***************************************************************************************/

typedef struct
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, References an existing Video Analytics configuration. The list of available tokens can be obtained
															//	via the Media service GetVideoAnalyticsConfigurations method
} tan_GetSupportedRules_REQ;

typedef struct
{
	onvif_SupportedRules 	SupportedRules;					// required 
} tan_GetSupportedRules_RES;

typedef struct
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing VideoAnalyticsConfiguration

	ConfigList * Rule;									    // required
} tan_CreateRules_REQ;

typedef struct 
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing VideoAnalyticsConfiguration

	int 	sizeRuleName;
	char 	RuleName[10][ONVIF_NAME_LEN];					// required, References the specific rule to be deleted (e.g. "MyLineDetector"). 
} tan_DeleteRules_REQ;

typedef struct 
{
	char	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing VideoAnalyticsConfiguration
} tan_GetRules_REQ;

typedef struct 
{
	ConfigList * Rule;									    // optional
} tan_GetRules_RES;

typedef struct 
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing VideoAnalyticsConfiguration

	ConfigList * Rule;									    // required 
} tan_ModifyRules_REQ;

typedef struct 
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing VideoAnalyticsConfiguration

	ConfigList * AnalyticsModule;                           // required 
} tan_CreateAnalyticsModules_REQ;

typedef struct 
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing Video Analytics configuration

	int 	sizeAnalyticsModuleName;
	char 	AnalyticsModuleName[10][ONVIF_NAME_LEN];		//required, name of the AnalyticsModule to be deleted
} tan_DeleteAnalyticsModules_REQ;

typedef struct 
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing VideoAnalyticsConfiguration
} tan_GetAnalyticsModules_REQ;

typedef struct
{
	ConfigList * AnalyticsModule;                           // optional
} tan_GetAnalyticsModules_RES;

typedef struct 
{
	char 	ConfigurationToken[ONVIF_TOKEN_LEN];			// required, Reference to an existing VideoAnalyticsConfiguration
	
	ConfigList * AnalyticsModule;                           // required 
} tan_ModifyAnalyticsModules_REQ;

typedef struct 
{
	char    RuleType[100];	                                // optional, Reference to an SupportedRule Type returned from GetSupportedRules
	char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // required, Reference to an existing analytics configuration
} tan_GetRuleOptions_REQ;

typedef struct 
{
    char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // required, Reference to an existing VideoAnalyticsConfiguration
} tan_GetSupportedAnalyticsModules_REQ;

typedef struct
{
    char    Type[128];	                                    // required, Reference to an SupportedAnalyticsModule Type returned from GetSupportedAnalyticsModules
	char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // required, Reference to an existing AnalyticsConfiguration
} tan_GetAnalyticsModuleOptions_REQ;

typedef struct
{
    char    Type[128];                                      // optional, reference to an AnalyticsModule Type returned from GetSupportedAnalyticsModules
} tan_GetSupportedMetadata_REQ;

typedef struct
{
    int sizeAnalyticsModule;                                // sequence of elements <AnalyticsModule>
    onvif_MetadataInfo  AnalyticsModule[10];                // optional
} tan_GetSupportedMetadata_RES;


#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET onvif_tan_GetSupportedRules(tan_GetSupportedRules_REQ * p_req, tan_GetSupportedRules_RES * p_res);
ONVIF_RET onvif_tan_CreateRules(tan_CreateRules_REQ * p_req);
ONVIF_RET onvif_tan_DeleteRules(tan_DeleteRules_REQ * p_req);
ONVIF_RET onvif_tan_GetRules(tan_GetRules_REQ * p_req, tan_GetRules_RES * p_res);
ONVIF_RET onvif_tan_ModifyRules(tan_ModifyRules_REQ * p_req);
ONVIF_RET onvif_tan_CreateAnalyticsModules(tan_CreateAnalyticsModules_REQ * p_req);
ONVIF_RET onvif_tan_DeleteAnalyticsModules(tan_DeleteAnalyticsModules_REQ * p_req);
ONVIF_RET onvif_tan_GetAnalyticsModules(tan_GetAnalyticsModules_REQ * p_req, tan_GetAnalyticsModules_RES * p_res);
ONVIF_RET onvif_tan_ModifyAnalyticsModules(tan_ModifyAnalyticsModules_REQ * p_req);
ONVIF_RET onvif_tan_GetSupportedMetadata(tan_GetSupportedMetadata_REQ * p_req, tan_GetSupportedMetadata_RES * p_res);

#ifdef __cplusplus
}
#endif

#endif 	// end of ONVIF_ANALYTICS_H



