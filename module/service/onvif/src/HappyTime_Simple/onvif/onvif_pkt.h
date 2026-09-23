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

#ifndef __ONVIF_PKT_H__
#define __ONVIF_PKT_H__


#ifdef __cplusplus
extern "C" {
#endif

int build_err_rly_xml(char * p_buf, int mlen, const char * code, const char * subcode, const char * subcode_ex, const char * reason);

int build_GetDeviceInformation_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetProfiles_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetProfiles_2_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetProfile_rly_xml(char * p_buf, int mlen, const char * argv);
int build_CreateProfile_rly_xml(char * p_buf, int mlen, const char * argv);
int build_DeleteProfile_rly_xml(char * p_buf, int mlen, const char * argv);
int build_AddVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RemoveVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_AddAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RemoveAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_AddVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RemoveVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_AddAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RemoveAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetStreamUri_rly_xml(char * p_buf, int mlen, const char * profile_token);
int build_GetStreamUri_rly_xml_2(char * p_buf, int mlen, const char * token);

int build_GetCapabilities_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetNetworkInterfaces_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetNetworkInterfaces_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetVideoEncoderConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_GetVideoEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetCompatibleVideoEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetVideoSourceConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_GetVideoSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetVideoSourceConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_GetVideoSourceConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetVideoEncoderConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetVideoEncoderConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_SystemReboot_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetSystemFactoryDefault_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetSystemLog_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetVideoSources_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * token);
int build_GetSystemDateAndTime_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetSystemDateAndTime_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetServices_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetAudioSources_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetAudioEncoderConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_GetAudioEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetAudioSourceConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_GetAudioSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * token);
int build_GetAudioEncoderConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_GetAudioEncoderConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetSnapshotUri_rly_xml_2(char * p_buf, int mlen, const char * profile_token);
int build_GetSnapshotUri_rly_xml(char * p_buf, int mlen, const char * profile_token);
int build_GetVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * token);
int build_GetAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * token);
int build_GetCompatibleVideoSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * token);
int build_GetScopes_rly_xml(char * p_buf, int mlen, const char * argv);
int build_AddScopes_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetScopes_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RemoveScopes_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetHostname_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetHostname_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetNetworkProtocols_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetNetworkProtocols_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetNetworkDefaultGateway_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetNetworkDefaultGateway_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetDiscoveryMode_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetDiscoveryMode_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetDNS_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetDNS_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetNTP_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetNTP_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetServiceCapabilities_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetEventProperties_rly_xml(char * p_buf, int mlen, const char * argv);
int build_Subscribe_rly_xml(char * p_buf, int mlen, const char * argv);
int build_Unsubscribe_rly_xml(char * p_buf, int mlen, const char * argv);
int build_Renew_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetSynchronizationPoint_rly_xml(char * p_buf, int mlen, const char * argv);
int build_Notify_xml(char * p_buf, int mlen, const char * argv);
int build_GetWsdlUrl_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetNodes_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetNode_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_AddPTZConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RemovePTZConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv);
int build_PTZ_GetStatus_rly_xml(char * p_buf, int mlen, const char * argv);
int build_ContinuousMove_rly_xml(char * p_buf, int mlen, const char * argv);
int build_PTZ_Stop_rly_xml(char * p_buf, int mlen, const char * argv);
int build_AbsoluteMove_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RelativeMove_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetPreset_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetPresets_rly_xml(char * p_buf, int mlen, const char * argv);
int build_RemovePreset_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GotoPreset_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GotoHomePosition_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetHomePosition_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetGuaranteedNumberOfVideoEncoderInstances_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetCompatibleAudioSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetAudioSourceConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv);
int build_GetAudioSourceConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetCompatibleAudioEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetImagingSettings_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetOptions_rly_xml(char * p_buf, int mlen, const char * argv);
int build_SetImagingSettings_rly_xml(char * p_buf, int mlen, const char * argv);
int build_GetMoveOptions_rly_xml(char * p_buf, int mlen, const char * argv);
int build_Move_rly_xml(char * p_buf, int mlen, const char * argv);
int build_IMG_GetStatus_rly_xml(char * p_buf, int mlen, const char * argv);
int build_IMG_Stop_rly_xml(char * p_buf, int mlen, const char * argv);


#ifdef __cplusplus
}
#endif

#endif 


