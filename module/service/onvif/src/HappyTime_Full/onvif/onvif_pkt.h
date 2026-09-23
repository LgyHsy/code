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

#ifndef __ONVIF_PKT_H__
#define __ONVIF_PKT_H__

#include "onvif.h"
#ifdef __cplusplus
extern "C" {
#endif

int build_err_rly_xml(char * p_buf, int mlen, const char * code, const char * subcode, const char * subcode_ex, const char * reason, const char * action);

int build_DeviceCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_DeviceServicesCapabilities_xml(char * p_buf, int mlen);
int build_EventsCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_EventsServicesCapabilities_xml(char * p_buf, int mlen);

#ifdef MEDIA_SUPPORT
int build_MediaCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_MediaServicesCapabilities_xml(char * p_buf, int mlen);
#endif // MEDIA_SUPPORT

#ifdef IMAGE_SUPPORT
int build_ImagingCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_ImagingServicesCapabilities_xml(char * p_buf, int mlen);
#endif // IMAGE_SUPPORT

#ifdef PTZ_SUPPORT
int build_PTZCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_PTZServicesCapabilities_xml(char * p_buf, int mlen);
#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS
int build_AnalyticsCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_AnalyticsServicesCapabilities_xml(char * p_buf, int mlen);
#endif // VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT
int build_RecordingCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_RecordingServicesCapabilities_xml(char * p_buf, int mlen);
int build_SearchCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_SearchServicesCapabilities_xml(char * p_buf, int mlen);
int build_ReplayCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_ReplayServicesCapabilities_xml(char * p_buf, int mlen);
#endif // PROFILE_G_SUPPORT

#ifdef DEVICEIO_SUPPORT
int build_DeviceIOCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_DeviceIOServicesCapabilities_xml(char * p_buf, int mlen);
#endif // DEVICEIO_SUPPORT

int build_ExtensionCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);

#ifdef RECEIVER_SUPPORT
int build_ReceiverCapabilities_xml(HTTPCLN * p_user, char * p_buf, int mlen);
int build_ReceiverServicesCapabilities_xml(char * p_buf, int mlen);
#endif // RECEIVER_SUPPORT

#ifdef MEDIA2_SUPPORT
int build_Media2ServicesCapabilities_xml(char * p_buf, int mlen);
#endif // MEDIA2_SUPPORT  

#ifdef PROFILE_C_SUPPORT
int build_AccessControlServicesCapabilities_xml(char * p_buf, int mlen);
int build_DoorControlServicesCapabilities_xml(char * p_buf, int mlen);
#endif // PROFILE_C_SUPPORT

#ifdef THERMAL_SUPPORT
int build_ThermalServicesCapabilities_xml(char * p_buf, int mlen);
#endif // THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT
int build_CredentialServicesCapabilities_xml(char * p_buf, int mlen);
#endif // CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
int build_AccessRulesServicesCapabilities_xml(char * p_buf, int mlen);
#endif // ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
int build_ScheduleServicesCapabilities_xml(char * p_buf, int mlen);
#endif // SCHEDULE_SUPPORT

#ifdef PROVISIONING_SUPPORT
int build_SourceCapabilities_xml(char * p_buf, int mlen, onvif_SourceCapabilities * p_res);
int build_ProvisioningServicesCapabilities_xml(char * p_buf, int mlen);
#endif // PROVISIONING_SUPPORT
int build_PlusServicesCapabilities_xml(char * p_buf, int mlen);

int build_Version_xml(char * p_buf, int mlen, onvif_Version * p_res);
int build_FloatRange_xml(char * p_buf, int mlen, onvif_FloatRange * p_res);
int build_IntList_xml(char * p_buf, int mlen, onvif_IntList * p_res);
int build_FloatList_xml(char * p_buf, int mlen, onvif_FloatList * p_res);
int build_VideoResolution_xml(char * p_buf, int mlen, onvif_VideoResolution * p_res);
int build_GetServiceCapabilities_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_NetworkInterface_xml(char * p_buf, int mlen, onvif_NetworkInterface * p_res);
int build_DeviceInformation_xml(char * p_buf, int mlen, onvif_DeviceInformation * p_res);
int build_SystemDateTime_xml(char * p_buf, int mlen, onvif_SystemDateTime * p_res);
int build_NetworkProtocol_xml(char * p_buf, int mlen, onvif_NetworkProtocol * p_res);
int build_DNSInformation_xml(char * p_buf, int mlen, onvif_DNSInformation * p_res);
int build_NTPInformation_xml(char * p_buf, int mlen, onvif_NTPInformation * p_res);
int build_NetworkGateway_xml(char * p_buf, int mlen, onvif_NetworkGateway * p_res);
int build_DynamicDNSInformation_xml(char * p_buf, int mlen, onvif_DynamicDNSInformation * p_res);
int build_ZeroConfiguration_xml(char * p_buf, int mlen, onvif_NetworkZeroConfiguration * p_res);
int build_Scope_xml(char * p_buf, int mlen, onvif_Scope * p_res);
int build_tds_GetDeviceInformation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetSystemUris_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetCapabilities_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetNetworkInterfaces_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetNetworkInterfaces_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SystemReboot_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetSystemFactoryDefault_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetSystemLog_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetSystemDateAndTime_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetSystemDateAndTime_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetServices_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_AddScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_RemoveScopes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetHostname_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetHostname_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetHostnameFromDHCP_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetNetworkProtocols_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetNetworkProtocols_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetNetworkDefaultGateway_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetNetworkDefaultGateway_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetDiscoveryMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetDiscoveryMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetDynamicDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetDynamicDNS_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetNTP_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetNTP_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetZeroConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetZeroConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetUsers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_CreateUsers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_DeleteUsers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetUser_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetRemoteUser_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetRemoteUser_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_StartFirmwareUpgrade_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_StartSystemRestore_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetWsdlUrl_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetEndpointReference_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetHashingAlgorithm_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

#ifdef DEVICEIO_SUPPORT
int build_tds_SetRelayOutputSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetRelayOutputState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetRelayOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // DEVICEIO_SUPPORT

#ifdef IPFILTER_SUPPORT	
int build_IPAddressFilter_xml(char * p_buf, int mlen, onvif_IPAddressFilter * p_res);
int build_tds_GetIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_AddIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_RemoveIPAddressFilter_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // IPFILTER_SUPPORT

#ifdef STORAGE_SUPPORT
int build_UserCredential_xml(char * p_buf, int mlen, onvif_UserCredential * p_res);
int build_StorageConfigurationData_xml(char * p_buf, int mlen, onvif_StorageConfigurationData * p_res);
int build_StorageConfiguration_xml(char * p_buf, int mlen, onvif_StorageConfiguration * p_res);
int build_tds_GetStorageConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_CreateStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_DeleteStorageConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // STORAGE_SUPPORT

#ifdef GEOLOCATION_SUPPORT
int build_tds_GetGeoLocation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_SetGeoLocation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_DeleteGeoLocation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // GEOLOCATION_SUPPORT

#ifdef DOT11_SUPPORT
int build_Dot11Configuration_xml(char * p_buf, int mlen, onvif_Dot11Configuration * p_res);
int build_Dot11AvailableNetworks_xml(char * p_buf, int mlen, onvif_Dot11AvailableNetworks * p_res);
int build_tds_GetDot11Capabilities_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_GetDot11Status_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tds_ScanAvailableDot11Networks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif

#ifdef PROFILE_Q_SUPPORT
int build_Base_EventProperties_xml(char * p_buf, int mlen);
#endif // PROFILE_Q_SUPPORT

int build_Imaging_EventProperties_xml(char * p_buf, int mlen);

#ifdef VIDEO_ANALYTICS
int build_Analytics_EventProperties_xml(char * p_buf, int mlen);
#endif

#ifdef DEVICEIO_SUPPORT
int build_DeviceIO_EventProperties_xml(char * p_buf, int mlen);
#endif // DEVICEIO_SUPPORT

#ifdef PROFILE_G_SUPPORT
int build_ProfileG_EventProperties_xml(char * p_buf, int mlen);
#endif // end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT
int build_ProfileC_EventProperties_xml(char * p_buf, int mlen);
#endif // PROFILE_C_SUPPORT

#ifdef CREDENTIAL_SUPPORT
int build_Credential_EventProperties_xml(char * p_buf, int mlen);
#endif // CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
int build_AccessRules_EventProperties_xml(char * p_buf, int mlen);
#endif // ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
int build_Schedule_EventProperties_xml(char * p_buf, int mlen);
#endif // SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT
int build_Receiver_EventProperties_xml(char * p_buf, int mlen);
#endif // RECEIVER_SUPPORT

int build_SimpleItem_xml(char * p_buf, int mlen, onvif_SimpleItem * p_res);
int build_ElementItem_xml(char * p_buf, int mlen, onvif_ElementItem * p_res);
int build_NotificationMessage_xml(char * p_buf, int mlen, onvif_NotificationMessage * p_res);
int build_Notify_xml(char * p_buf, int mlen, const char * argv);
int build_tev_GetEventProperties_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tev_Subscribe_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tev_Unsubscribe_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tev_Renew_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tev_CreatePullPointSubscription_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tev_PullMessages_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tev_SetSynchronizationPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

#ifdef IMAGE_SUPPORT
int build_ImageSettings_xml(char * p_buf, int mlen, onvif_ImagingSettings * p_res);
int build_ImageOptions_xml(char * p_buf, int mlen, onvif_ImagingOptions * p_res);
int build_ImagePreset_xml(char * p_buf, int mlen, onvif_ImagingPreset * p_res);
int build_img_GetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_GetOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_SetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_GetMoveOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_Move_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_GetStatus_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_Stop_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_GetPresets_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_GetCurrentPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_img_SetCurrentPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // IMAGE_SUPPORT

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)
int build_MulticastConfiguration_xml(char * p_buf, int mlen, onvif_MulticastConfiguration * p_cfg);
int build_OSDConfiguration_xml(char * p_buf, int mlen, onvif_OSDConfiguration * p_res);
int build_VideoSourceMode_xml(char * p_buf, int mlen, onvif_VideoSourceMode * p_res);
int build_VideoSourceConfiguration_xml_by_profile(char * p_buf, int mlen, ONVIF_PROFILE * p_profile);
int build_VideoSourceConfiguration_xml(char * p_buf, int mlen, onvif_VideoSourceConfiguration * p_res);
int build_VideoSourceConfigurationOptions_xml(char * p_buf, int mlen, onvif_VideoSourceConfigurationOptions * p_res);
int build_VideoEncoder2Configuration_xml(char * p_buf, int mlen, onvif_VideoEncoder2Configuration * p_res);
int build_MetadataConfiguration_xml(char * p_buf, int mlen, onvif_MetadataConfiguration * p_res);
int build_MetadataConfigurationOptions_xml(char * p_buf, int mlen, onvif_MetadataConfigurationOptions * p_res);

#ifdef AUDIO_SUPPORT
int build_AudioSource_xml(char * p_buf, int mlen, onvif_AudioSource * p_res);
int build_AudioSourceConfiguration_xml(char * p_buf, int mlen, onvif_AudioSourceConfiguration * p_res);
int build_AudioEncoderConfiguration_xml(char * p_buf, int mlen, onvif_AudioEncoder2Configuration * p_res);
int build_AudioDecoderConfiguration_xml(char * p_buf, int mlen, onvif_AudioDecoderConfiguration * p_res);
int build_AudioSourceConfigurationOptions_xml(char * p_buf, int mlen);
int build_AudioEncoder2Configuration_xml(char * p_buf, int mlen, onvif_AudioEncoder2Configuration * p_res);
#endif // AUDIO_SUPPORT

#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef MEDIA_SUPPORT
int build_VideoEncoderConfiguration_xml(char * p_buf, int mlen, onvif_VideoEncoder2Configuration * p_res);
int build_Profile_xml(char * p_buf, int mlen, ONVIF_PROFILE * p_profile);
int build_JpegOptions_xml(char * p_buf, int mlen, onvif_JpegOptions * p_options);
int build_Mpeg4Options_xml(char * p_buf, int mlen, onvif_Mpeg4Options * p_options);
int build_H264Options_xml(char * p_buf, int mlen, onvif_H264Options * p_options);
int build_BitrateRange_xml(char * p_buf, int mlen, onvif_IntRange * p_res);
int build_trt_GetProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_CreateProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_DeleteProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_AddVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_AddVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetStreamUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetSnapshotUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleVideoEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleVideoSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetSynchronizationPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetGuaranteedNumberOfVideoEncoderInstances_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetOSDs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_CreateOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_DeleteOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetOSDOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_StartMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_StopMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetMetadataConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleMetadataConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetMetadataConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_AddMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoSourceModes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetVideoSourceMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

#ifdef AUDIO_SUPPORT
int build_AudioDecoderConfigurationOptions_xml(char * p_buf, int mlen, onvif_AudioDecoderConfigurationOptions * p_res);
int build_trt_AddAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_AddAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleAudioSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleAudioEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_AddAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioDecoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioDecoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleAudioDecoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT
int build_trt_GetAudioOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_AddAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioOutputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleAudioOutputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAudioOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // DEVICEIO_SUPPORT

#ifdef PTZ_SUPPORT
int build_trt_AddPTZConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemovePTZConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS
int build_trt_GetVideoAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_AddVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_RemoveVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_SetVideoAnalyticsConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trt_GetCompatibleVideoAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // VIDEO_ANALYTICS

#endif // MEDIA_SUPPORT

#ifdef PTZ_SUPPORT
int build_PTZSpaces_xml(char * p_buf, int mlen, onvif_PTZSpaces * p_res);
int build_PTZNodeExtension_xml(char * p_buf, int mlen, onvif_PTZNodeExtension * p_res);
int build_PTZNode_xml(char * p_buf, int mlen, onvif_PTZNode * p_res);
int build_Vector_xml(char * p_buf, int mlen, onvif_Vector * p_res);
int build_Vector1D_xml(char * p_buf, int mlen, onvif_Vector1D * p_res);
int build_PTZVector_xml(char * p_buf, int mlen, onvif_PTZVector * p_res);
int build_PTZPresetTourPresetDetail_xml(char * p_buf, int mlen, onvif_PTZPresetTourPresetDetail * p_res);
int build_PTZSpeed_xml(char * p_buf, int mlen, onvif_PTZSpeed * p_res);
int build_PTZPreset_xml(char * p_buf, int mlen, onvif_PTZPreset * p_res);
int build_PTZPresetTourSpot_xml(char * p_buf, int mlen, onvif_PTZPresetTourSpot * p_res);
int build_PTZPresetTourStatus(char * p_buf, int mlen, onvif_PTZPresetTourStatus * p_res);
int build_PTZPresetTourStartingCondition(char * p_buf, int mlen, onvif_PTZPresetTourStartingCondition * p_res);
int build_PresetTour_xml(char * p_buf, int mlen, onvif_PresetTour * p_res);
int build_PresetTour_by_g_tour_ctx_xml(char * p_buf, int mlen, ptz_tour_t * p_res);

int build_IntRange_xml(char * p_buf, int mlen, onvif_IntRange * p_res);
int build_DurationRange_xml(char * p_buf, int mlen, onvif_DurationRange * p_res);
int build_PTZPresetTourStartingConditionOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourStartingConditionOptions * p_res);
int build_Space2DDescription_xml(char * p_buf, int mlen, onvif_Space2DDescription * p_res);
int build_Space1DDescription_xml(char * p_buf, int mlen, onvif_Space1DDescription * p_res);
int build_PTZPresetTourPresetDetailOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourPresetDetailOptions * p_res);
int build_PTZPresetTourSpotOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourSpotOptions * p_res);
int build_PTZPresetTourOptions_xml(char * p_buf, int mlen, onvif_PTZPresetTourOptions * p_res);
int build_PTZConfiguration_xml(char * p_buf, int mlen, onvif_PTZConfiguration * p_ptz_cfg);
int build_ptz_GetNodes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetNode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetCompatibleConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetStatus_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_ContinuousMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_Stop_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_AbsoluteMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_RelativeMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_SetPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetPresets_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_RemovePreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GotoPreset_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GotoHomePosition_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_SetHomePosition_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_SetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetPresetTours_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetPresetTours_new_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

int build_ptz_GetPresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GetPresetTourOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_CreatePresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_ModifyPresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_OperatePresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_RemovePresetTour_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_SendAuxiliaryCommand_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_ptz_GeoMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS
int build_Config_xml(char * p_buf, int mlen, onvif_Config * p_res);
int build_AnalyticsEngineConfiguration_xml(char * p_buf, int mlen, onvif_AnalyticsEngineConfiguration * p_res);
int build_RuleEngineConfiguration_xml(char * p_buf, int mlen, onvif_RuleEngineConfiguration * p_res);
int build_VideoAnalyticsConfiguration_xml(char * p_buf, int mlen, onvif_VideoAnalyticsConfiguration * p_res);
int build_ItemListDescription_xml(char * p_buf, int mlen, onvif_ItemListDescription * p_res);
int build_ConfigDescription_Messages_xml(char * p_buf, int mlen, onvif_ConfigDescription_Messages * p_res);
int build_SupportedRules_xml(char * p_buf, int mlen, onvif_SupportedRules * p_res);
int build_SupportedAnalyticsModules_xml(char * p_buf, int mlen, onvif_SupportedAnalyticsModules * p_res);
int build_tan_GetSupportedRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_CreateRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_DeleteRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_GetRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_ModifyRules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_CreateAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_DeleteAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_GetAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_ModifyAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_GetRuleOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_GetSupportedAnalyticsModules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_GetAnalyticsModuleOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tan_GetSupportedMetadata_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT
int build_RecordingSourceInformation_xml(char * p_buf, int mlen, onvif_RecordingSourceInformation * p_res);
int build_TrackInformation_xml(char * p_buf, int mlen, onvif_TrackInformation * p_res);
int build_RecordingInformation_xml(char * p_buf, int mlen, onvif_RecordingInformation * p_res);
int build_TrackAttributes_xml(char * p_buf, int mlen, onvif_TrackAttributes * p_res);
int build_MediaAttributes_xml(char * p_buf, int mlen, onvif_MediaAttributes * p_res);
int build_FindEventResult_xml(char * p_buf, int mlen, onvif_FindEventResult * p_res);
int build_FindPTZPositionResult_xml(char * p_buf, int mlen, onvif_FindPTZPositionResult * p_res);
int build_tse_GetRecordingInformation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_GetRecordingSummary_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_GetMediaAttributes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_FindRecordings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_GetRecordingSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_FindEvents_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_GetEventSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_FindMetadata_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_FindMetadataResult_xml(char * p_buf, int mlen, onvif_FindMetadataResult * p_res);
int build_tse_GetMetadataSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_FindPTZPosition_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_GetPTZPositionSearchResults_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_EndSearch_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tse_GetSearchState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_CreateRecording_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_DeleteRecording_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_RecordingConfiguration_xml(char * p_buf, int mlen, onvif_RecordingConfiguration * p_res);
int build_TrackConfiguration_xml(char * p_buf, int mlen, onvif_TrackConfiguration * p_res);
int build_Recording_xml(char * p_buf, int mlen, onvif_Recording * p_res);
int build_RecordingJobConfiguration_xml(char * p_buf, int mlen, onvif_RecordingJobConfiguration * p_res);
int build_RecordingJob_xml(char * p_buf, int mlen, onvif_RecordingJob * p_res);
int build_RecordingJobStateInformation_xml(char * p_buf, int mlen, onvif_RecordingJobStateInformation * p_res);
int build_trc_GetRecordings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_SetRecordingConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_GetRecordingConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_CreateTrack_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_DeleteTrack_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_GetTrackConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_SetTrackConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_CreateRecordingJob_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_DeleteRecordingJob_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_GetRecordingJobs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_SetRecordingJobConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_GetRecordingJobConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_SetRecordingJobMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_GetRecordingJobState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_GetRecordingOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_ExportRecordedData_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_StopExportRecordedData_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trc_GetExportRecordedDataState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trp_GetReplayUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trp_GetReplayConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trp_SetReplayConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT
int build_AccessPointInfo_xml(char * p_buf, int mlen, onvif_AccessPointInfo * p_res);
int build_DoorInfo_xml(char * p_buf, int mlen, onvif_DoorInfo * p_res);
int build_AreaInfo_xml(char * p_buf, int mlen, onvif_AreaInfo * p_res);
int build_Door_xml(char * p_buf, int mlen, onvif_Door * p_res);
int build_DoorState_xml(char * p_buf, int mlen, onvif_DoorState * p_res);
int build_tac_GetAccessPointList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAccessPoints_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_CreateAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_SetAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_ModifyAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_DeleteAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAccessPointInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAccessPointInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAreaList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAreas_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_CreateArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_SetArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_ModifyArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_DeleteArea_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAreaInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAreaInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_GetAccessPointState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_EnableAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tac_DisableAccessPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_GetDoorList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_GetDoors_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_CreateDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_SetDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_ModifyDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_DeleteDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_GetDoorInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_GetDoorInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_GetDoorState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_AccessDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_LockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_UnlockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_DoubleLockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_BlockDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_LockDownDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_LockDownReleaseDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_LockOpenDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tdc_LockOpenReleaseDoor_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT
int build_PaneLayout_xml(char * p_buf, int mlen, onvif_PaneLayout * p_PaneLayout);
int build_Layout_xml(char * p_buf, int mlen, onvif_Layout * p_Layout);
int build_VideoOutput_xml(char * p_buf, int mlen, onvif_VideoOutput * p_VideoOutput);
int build_VideoOutputConfiguration_xml(char * p_buf, int mlen, onvif_VideoOutputConfiguration * p_res);
int build_AudioOutputConfiguration_xml(char * p_buf, int mlen, onvif_AudioOutputConfiguration * p_res);
int build_AudioOutputConfigurationOptions_xml(char * p_buf, int mlen, onvif_AudioOutputConfigurationOptions * p_res);
int build_RelayOutput_xml(char * p_buf, int mlen, onvif_RelayOutput * p_res);
int build_RelayOutputOptions_xml(char * p_buf, int mlen, onvif_RelayOutputOptions * p_res);
int build_DigitalInputConfigurationOptions_xml(char * p_buf, int mlen, onvif_DigitalInputConfigurationOptions * p_res);
int build_SerialPortConfiguration_xml(char * p_buf, int mlen, onvif_SerialPortConfiguration * p_res);
int build_SerialPortConfigurationOptions_xml(char * p_buf, int mlen, onvif_SerialPortConfigurationOptions * p_res);
int build_ParityBitList_xml(char * p_buf, int mlen, onvif_ParityBitList * p_res);
int build_tmd_GetVideoOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetVideoSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetVideoOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_SetVideoOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetVideoOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetAudioOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetAudioOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetRelayOutputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetRelayOutputOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_SetRelayOutputSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_SetRelayOutputState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetDigitalInputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetDigitalInputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_SetDigitalInputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetSerialPorts_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetSerialPortConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetSerialPortConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_SetSerialPortConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_SendReceiveSerialCommand_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tmd_GetAudioSources_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // DEVICEIO_SUPPORT

#ifdef MEDIA2_SUPPORT
int build_ColorspaceRange_xml(char * p_buf, int mlen, onvif_ColorspaceRange * p_res);
int build_ColorOptions_xml(char * p_buf, int mlen, onvif_ColorOptions * p_res);
int build_OSDColorOptions_xml(char * p_buf, int mlen, onvif_OSDColorOptions * p_res);
int build_OSDTextOptions_xml(char * p_buf, int mlen, onvif_OSDTextOptions * p_res);
int build_OSDImgOptions_xml(char * p_buf, int mlen, onvif_OSDImgOptions * p_res);
int build_Polygon_xml(char * p_buf, int mlen, onvif_Polygon * p_res);
int build_Color_xml(char * p_buf, int mlen, onvif_Color * p_res);
int build_Mask_xml(char * p_buf, int mlen, onvif_Mask * p_res);
int build_VideoEncoder2ConfigurationOptions_xml(char * p_buf, int mlen, onvif_VideoEncoder2ConfigurationOptions * p_res);
int build_AudioEncoder2ConfigurationOptions_xml(char * p_buf, int mlen, onvif_AudioEncoder2ConfigurationOptions * p_res);
int build_tr2_GetVideoEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetVideoEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_CreateProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_DeleteProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_AddConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_RemoveConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetVideoSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetMetadataConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetVideoSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetMetadataConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetAudioSourceConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetVideoSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetMetadataConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetVideoEncoderInstances_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetStreamUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetSynchronizationPoint_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetVideoSourceModes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetVideoSourceMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetSnapshotUri_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetOSDOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetOSDs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_CreateOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_DeleteOSD_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_CreateMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_DeleteMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetMasks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetMaskOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_StartMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_StopMulticastStreaming_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

#ifdef AUDIO_SUPPORT
int build_tr2_GetAudioEncoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetAudioSourceConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetAudioSourceConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetAudioEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetAudioEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetAudioDecoderConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetAudioDecoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetAudioDecoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT
int build_tr2_GetAudioOutputConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_GetAudioOutputConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tr2_SetAudioOutputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // DEVICEIO_SUPPORT

#ifdef VIDEO_ANALYTICS
int build_tr2_GetAnalyticsConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // VIDEO_ANALYTICS

#endif // MEDIA2_SUPPORT

#ifdef THERMAL_SUPPORT
int build_ColorPalette_xml(char * p_buf, int mlen, onvif_ColorPalette * p_res);
int build_NUCTable_xml(char * p_buf, int mlen, onvif_NUCTable * p_res);
int build_ThermalConfiguration_xml(char * p_buf, int mlen, onvif_ThermalConfiguration * p_res);
int build_RadiometryGlobalParameters_xml(char * p_buf, int mlen, onvif_RadiometryGlobalParameters * p_res);
int build_RadiometryConfiguration_xml(char * p_buf, int mlen, onvif_RadiometryConfiguration * p_res);
int build_RadiometryGlobalParameterOptions_xml(char * p_buf, int mlen, onvif_RadiometryGlobalParameterOptions * p_res);
int build_tth_GetConfigurations_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tth_GetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tth_SetConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tth_GetConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tth_GetRadiometryConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tth_SetRadiometryConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tth_GetRadiometryConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT
int build_CredentialInfo_xml(char * p_buf, int mlen, onvif_CredentialInfo * p_res);
int build_CredentialIdentifierType_xml(char * p_buf, int mlen, onvif_CredentialIdentifierType * p_res);
int build_CredentialIdentifier_xml(char * p_buf, int mlen, onvif_CredentialIdentifier * p_res);
int build_CredentialIdentifierItem_xml(char * p_buf, int mlen, onvif_CredentialIdentifierItem * p_res);
int build_CredentialAccessProfile_xml(char * p_buf, int mlen, onvif_CredentialAccessProfile * p_res);
int build_Credential_xml(char * p_buf, int mlen, onvif_Credential * p_res);
int build_CredentialIdentifierFormatTypeInfo_xml(char * p_buf, int mlen, onvif_CredentialIdentifierFormatTypeInfo * p_res);
int build_CredentialState_xml(char * p_buf, int mlen, onvif_CredentialState * p_res);
int build_tcr_GetCredentialInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetCredentialInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetCredentials_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetCredentialList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_CreateCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_ModifyCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_DeleteCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetCredentialState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_EnableCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_DisableCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_SetCredential_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_ResetAntipassbackViolation_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetSupportedFormatTypes_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetCredentialIdentifiers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_SetCredentialIdentifier_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_DeleteCredentialIdentifier_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetCredentialAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_SetCredentialAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_DeleteCredentialAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_AddToWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_RemoveFromWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_DeleteWhitelist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_GetBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_AddToBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_RemoveFromBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tcr_DeleteBlacklist_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
int build_AccessProfileInfo_xml(char * p_buf, int mlen, onvif_AccessProfileInfo * p_res);
int build_AccessPolicy_xml(char * p_buf, int mlen, onvif_AccessPolicy * p_res);
int build_AccessProfile_xml(char * p_buf, int mlen, onvif_AccessProfile * p_res);
int build_tar_GetAccessProfileInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tar_GetAccessProfileInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tar_GetAccessProfiles_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tar_GetAccessProfileList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tar_CreateAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tar_ModifyAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tar_DeleteAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tar_SetAccessProfile_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
int build_ScheduleInfo_xml(char * p_buf, int mlen, onvif_ScheduleInfo * p_res);
int build_TimePeriod_xml(char * p_buf, int mlen, onvif_TimePeriod * p_res);
int build_SpecialDaysSchedule_xml(char * p_buf, int mlen, onvif_SpecialDaysSchedule * p_res);
int build_Schedule_xml(char * p_buf, int mlen, onvif_Schedule * p_res);
int build_SpecialDayGroupInfo_xml(char * p_buf, int mlen, onvif_SpecialDayGroupInfo * p_res);
int build_SpecialDayGroup_xml(char * p_buf, int mlen, onvif_SpecialDayGroup * p_res);
int build_tsc_GetScheduleInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetScheduleInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetSchedules_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetScheduleList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_CreateSchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_ModifySchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_DeleteSchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetSpecialDayGroupInfo_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetSpecialDayGroupInfoList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetSpecialDayGroups_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetSpecialDayGroupList_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_CreateSpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_ModifySpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_DeleteSpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_GetScheduleState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_SetSchedule_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tsc_SetSpecialDayGroup_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // CHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT
int build_StreamSetup_xml(char * p_buf, int mlen, onvif_StreamSetup * p_res);
int build_ReceiverConfiguration_xml(char * p_buf, int mlen, onvif_ReceiverConfiguration * p_res);
int build_Receiver_xml(char * p_buf, int mlen, onvif_Receiver * p_res);
int build_trv_GetReceivers_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trv_GetReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trv_CreateReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trv_DeleteReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trv_ConfigureReceiver_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trv_SetReceiverMode_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_trv_GetReceiverState_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT
int build_tpv_PanMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpv_TiltMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpv_ZoomMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpv_RollMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpv_FocusMove_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpv_Stop_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpv_GetUsage_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
#endif // PROVISIONING_SUPPORT

int build_tpl_PrivacyMask_xml(char * p_buf, int mlen, onvif_Mask * p_res);
int build_tpl_GetPrivacyMaskOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_GetPrivacyMasks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_DeletePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_SetPrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_CreatePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

int build_tpl_ServicesCapabilities_xml(char * p_buf, int mlen);
int build_tpl_SetVideoEncoderConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_GetImagingOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_GetDigitalInputs_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_SetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_GetImagingSettings_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_GetVideoEncoderConfigurationOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_SetDigitalInputConfiguration_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_SendCommand_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_tpl_SetImagingSettingDefault_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

int build_hbgk_ext_PrivacyMask_xml(char * p_buf, int mlen, onvif_Mask * p_res);
int build_hbgk_ext_GetPrivacyMaskOptions_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_hbgk_ext_GetPrivacyMasks_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_hbgk_ext_DeletePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_hbgk_ext_SetPrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);
int build_hbgk_ext_CreatePrivacyMask_rly_xml(HTTPCLN * p_user, char * p_buf, int mlen, const char * argv);

#ifdef __cplusplus
}
#endif

#endif 


