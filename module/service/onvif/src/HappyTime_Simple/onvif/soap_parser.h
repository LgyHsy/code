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

#ifndef _SOAP_PARSER_H_
#define _SOAP_PARSER_H_

/***************************************************************************************/
#include "xml_node.h"
#include "onvif.h"
#include "onvif_ptz.h"
#include "onvif_device.h"
#include "onvif_media.h"
#include "onvif_event.h"
#include "onvif_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************/
ONVIF_RET parse_AddScopes(XMLN * p_AddScopes, ONVIF_SCOPE * p_scope, int scope_max);
ONVIF_RET parse_SetScopes(XMLN * p_AddScopes, ONVIF_SCOPE * p_scope, int scope_max);
ONVIF_RET parse_SetDiscoveryMode(XMLN * p_SetDiscoveryMode, SetDiscoveryMode_REQ * p_SetDiscoveryMode_req);
ONVIF_RET parse_Subscribe(XMLN * p_node, Subscribe_REQ * p_Subscribe_req);
ONVIF_RET parse_Renew(XMLN * p_node, Renew_REQ * p_Renew_req);
ONVIF_RET parse_ContinuousMove(XMLN * p_node, ContinuousMove_REQ * p_ContinuousMove_req);
ONVIF_RET parse_PTZ_Stop(XMLN * p_node, PTZ_Stop_REQ * p_Stop_req);
ONVIF_RET parse_AbsoluteMove(XMLN * p_node, AbsoluteMove_REQ * p_AbsoluteMove_req);
ONVIF_RET parse_RelativeMove(XMLN * p_node, RelativeMove_REQ * p_RelativeMove_req);
ONVIF_RET parse_SetPreset(XMLN * p_node, SetPreset_REQ * p_SetPreset_req);
ONVIF_RET parse_RemovePreset(XMLN * p_node, RemovePreset_REQ * p_RemovePreset_req);
ONVIF_RET parse_GotoPreset(XMLN * p_node, GotoPreset_REQ * p_GotoPreset_req);
ONVIF_RET parse_GotoHomePosition(XMLN * p_node, GotoHomePosition_REQ * p_GotoHomePosition_req);
ONVIF_RET parse_SetDNS(XMLN * p_node, SetDNS_REQ * p_SetDNS_req);
ONVIF_RET parse_SetNTP(XMLN * p_node, SetNTP_REQ * p_SetNTP_req);
ONVIF_RET parse_SetNetworkProtocols(XMLN * p_node, SetNetworkProtocols_REQ * p_SetNetworkProtocols_req);
ONVIF_RET parse_SetNetworkDefaultGateway(XMLN * p_node, SetNetworkDefaultGateway_REQ * p_SetNetworkDefaultGateway_req);
ONVIF_RET parse_SetSystemDateAndTime(XMLN * p_node, SetSystemDateAndTime_REQ * p_SetSystemDateAndTime_req);
ONVIF_RET parse_CreateProfile(XMLN * p_node, CreateProfile_REQ * p_CreateProfile_req);
ONVIF_RET parse_AddVideoSourceConfiguration(XMLN * p_node, AddVideoSourceConfiguration_REQ * p_AddVideoSourceConfiguration_req);
ONVIF_RET parse_AddVideoEncoderConfiguration(XMLN * p_node, AddVideoEncoderConfiguration_REQ * p_AddVideoEncoderConfiguration_req);
ONVIF_RET parse_AddAudioSourceConfiguration(XMLN * p_node, AddAudioSourceConfiguration_REQ * p_AddAudioSourceConfiguration_req);
ONVIF_RET parse_AddAudioEncoderConfiguration(XMLN * p_node, AddAudioEncoderConfiguration_REQ * p_AddAudioEncoderConfiguration_req);
ONVIF_RET parse_AddPTZConfiguration(XMLN * p_node, AddPTZConfiguration_REQ * p_AddPTZConfiguration_req);
ONVIF_RET parse_GetStreamUri(XMLN * p_node, GetStreamUri_REQ * p_GetStreamUri_req);
ONVIF_RET parse_SetNetworkInterfaces(XMLN * p_node, SetNetworkInterfaces_REQ * p_SetNetworkInterfaces_req);
ONVIF_RET parse_GetVideoSourceConfigurationOptions(XMLN * p_node, GetVideoSourceConfigurationOptions_REQ * p_GetVideoSourceConfigurationOptions_req);
ONVIF_RET parse_SetVideoSourceConfiguration(XMLN * p_node, SetVideoSourceConfiguration_REQ * p_SetVideoSourceConfiguration_req);
ONVIF_RET parse_GetVideoEncoderConfigurationOptions(XMLN * p_node, GetVideoEncoderConfigurationOptions_REQ * p_GetVideoEncoderConfigurationOptions_req);
ONVIF_RET parse_SetVideoEncoderConfiguration(XMLN * p_SetVideoEncoderConfiguration, SetVideoEncoderConfiguration_REQ * p_SetVideoEncoderConfiguration_req);
ONVIF_RET parse_GetAudioSourceConfigurationOptions(XMLN * p_node, GetAudioSourceConfigurationOptions_REQ * p_GetAudioSourceConfigurationOptions_req);
ONVIF_RET parse_SetAudioSourceConfiguration(XMLN * p_node, SetAudioSourceConfiguration_REQ * p_SetAudioSourceConfiguration_req);
ONVIF_RET parse_GetAudioEncoderConfigurationOptions(XMLN * p_node, GetAudioEncoderConfigurationOptions_REQ * p_GetAudioEncoderConfigurationOptions_req);
ONVIF_RET parse_SetAudioEncoderConfiguration(XMLN * p_node, SetAudioEncoderConfiguration_REQ * p_SetAudioEncoderConfiguration_req);
ONVIF_RET parse_SetImagingSettings(XMLN * p_node, SetImagingSettings_REQ * p_SetImagingSettings_req);
ONVIF_RET parse_Move(XMLN * p_node, Move_REQ * p_Move_req);


#ifdef __cplusplus
}
#endif

#endif


