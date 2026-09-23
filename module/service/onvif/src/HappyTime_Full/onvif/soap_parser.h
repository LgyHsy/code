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
#ifdef VIDEO_ANALYTICS
#include "onvif_analytics.h"
#endif
#ifdef PROFILE_G_SUPPORT
#include "onvif_recording.h"
#endif
#ifdef PROFILE_C_SUPPORT
#include "onvif_doorcontrol.h"
#endif
#ifdef DEVICEIO_SUPPORT
#include "onvif_deviceio.h"
#endif
#ifdef MEDIA2_SUPPORT
#include "onvif_media2.h"
#endif
#ifdef THERMAL_SUPPORT
#include "onvif_thermal.h"
#endif
#ifdef CREDENTIAL_SUPPORT
#include "onvif_credential.h"
#endif
#ifdef ACCESS_RULES
#include "onvif_accessrules.h"
#endif
#ifdef SCHEDULE_SUPPORT
#include "onvif_schedule.h"
#endif
#ifdef RECEIVER_SUPPORT
#include "onvif_receiver.h"
#endif
#ifdef PROVISIONING_SUPPORT
#include "onvif_provisioning.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

BOOL      parse_Bool(const char * pdata);
BOOL      parse_XSDDatetime(const char * s, time_t * p);
BOOL      parse_XSDDuration(const char *s, int *a);
BOOL      parse_Vector(XMLN * p_node, onvif_Vector * p_req);
BOOL      parse_Vector1D(XMLN * p_node, onvif_Vector1D * p_req);
BOOL      parse_SystemDateTime(XMLN * p_node, onvif_SystemDateTime * p_req);
ONVIF_RET prase_User(XMLN * p_node, onvif_User * p_req);
ONVIF_RET parse_NetworkProtocols(XMLN * p_node, onvif_NetworkProtocol * p_req);
ONVIF_RET parse_DNSInformation(XMLN * p_node, onvif_DNSInformation * p_req);
ONVIF_RET parse_DynamicDNSInformation(XMLN * p_node, onvif_DynamicDNSInformation * p_req);
ONVIF_RET parse_NTPInformation(XMLN * p_node, onvif_NTPInformation * p_req);
ONVIF_RET parse_NetworkGateway(XMLN * p_node, onvif_NetworkGateway * p_req);
ONVIF_RET parse_NetworkZeroConfiguration(XMLN * p_node, onvif_NetworkZeroConfiguration * p_req);
ONVIF_RET parse_StreamSetup(XMLN * p_node, onvif_StreamSetup * p_req);
ONVIF_RET parse_tds_GetCapabilities(XMLN * p_node, tds_GetCapabilities_REQ * p_req);
ONVIF_RET parse_tds_GetServices(XMLN * p_node, tds_GetServices_REQ * p_req);
ONVIF_RET parse_tds_GetSystemLog(XMLN * p_node, tds_GetSystemLog_REQ * p_req);
ONVIF_RET parse_tds_SetSystemDateAndTime(XMLN * p_node, tds_SetSystemDateAndTime_REQ * p_req);
ONVIF_RET parse_tds_AddScopes(XMLN * p_node, tds_AddScopes_REQ * p_req);
ONVIF_RET parse_tds_SetScopes(XMLN * p_node, tds_SetScopes_REQ * p_req);
ONVIF_RET parse_tds_RemoveScopes(XMLN * p_node, tds_RemoveScopes_REQ * p_req);
ONVIF_RET parse_tds_SetHostname(XMLN * p_node, tds_SetHostname_REQ * p_req);
ONVIF_RET parse_tds_SetHostnameFromDHCP(XMLN * p_node, tds_SetHostnameFromDHCP_REQ * p_req);
ONVIF_RET parse_tds_SetDiscoveryMode(XMLN * p_node, tds_SetDiscoveryMode_REQ * p_req);
ONVIF_RET parse_tds_SetDNS(XMLN * p_node, tds_SetDNS_REQ * p_req);
ONVIF_RET parse_tds_SetDynamicDNS(XMLN * p_node, tds_SetDynamicDNS_REQ * p_req);
ONVIF_RET parse_tds_SetNTP(XMLN * p_node, tds_SetNTP_REQ * p_req);
ONVIF_RET parse_tds_SetZeroConfiguration(XMLN * p_node, tds_SetZeroConfiguration_REQ * p_req);
ONVIF_RET parse_tds_SetNetworkProtocols(XMLN * p_node, tds_SetNetworkProtocols_REQ * p_req);
ONVIF_RET parse_tds_SetNetworkDefaultGateway(XMLN * p_node, tds_SetNetworkDefaultGateway_REQ * p_req);
ONVIF_RET parse_tds_SetNetworkInterfaces(XMLN * p_node, tds_SetNetworkInterfaces_REQ * p_req);
ONVIF_RET parse_tds_SetSystemFactoryDefault(XMLN * p_node, tds_SetSystemFactoryDefault_REQ * p_req);
ONVIF_RET parse_tds_CreateUsers(XMLN * p_node, tds_CreateUsers_REQ * p_req);
ONVIF_RET parse_tds_DeleteUsers(XMLN * p_node, tds_DeleteUsers_REQ * p_req);
ONVIF_RET parse_tds_SetUser(XMLN * p_node, tds_SetUser_REQ * p_req);
ONVIF_RET parse_tds_SetRemoteUser(XMLN * p_node, tds_SetRemoteUser_REQ * p_req);
ONVIF_RET parse_tds_SetHashingAlgorithm(XMLN * p_node, tds_SetHashingAlgorithm_REQ * p_req);

#ifdef DEVICEIO_SUPPORT
ONVIF_RET parse_tds_SetRelayOutputSettings(XMLN * p_node, tmd_SetRelayOutputSettings_REQ * p_req);
#endif // DEVICEIO_SUPPORT

#ifdef IPFILTER_SUPPORT
BOOL      parse_PrefixedIPAddress(XMLN * p_node, onvif_PrefixedIPAddress * p_req);
ONVIF_RET parse_IPAddressFilter(XMLN * p_node, onvif_IPAddressFilter * p_req);
ONVIF_RET parse_tds_SetIPAddressFilter(XMLN * p_node, tds_SetIPAddressFilter_REQ * p_req);
ONVIF_RET parse_tds_AddIPAddressFilter(XMLN * p_node, tds_AddIPAddressFilter_REQ * p_req);
ONVIF_RET parse_tds_RemoveIPAddressFilter(XMLN * p_node, tds_RemoveIPAddressFilter_REQ * p_req);
#endif // IPFILTER_SUPPORT

#ifdef STORAGE_SUPPORT
ONVIF_RET parse_tds_GetStorageConfiguration(XMLN * p_node, tds_GetStorageConfiguration_REQ * p_req);
ONVIF_RET parse_tds_CreateStorageConfiguration(XMLN * p_node, tds_CreateStorageConfiguration_REQ * p_req);
ONVIF_RET parse_tds_SetStorageConfiguration(XMLN * p_node, tds_SetStorageConfiguration_REQ * p_req);
ONVIF_RET parse_tds_DeleteStorageConfiguration(XMLN * p_node, tds_DeleteStorageConfiguration_REQ * p_req);
#endif // STORAGE_SUPPORT

#ifdef GEOLOCATION_SUPPORT
ONVIF_RET parse_tds_SetGeoLocation(XMLN * p_node, tds_SetGeoLocation_REQ * p_req);
ONVIF_RET parse_tds_DeleteGeoLocation(XMLN * p_node, tds_DeleteGeoLocation_REQ * p_req);
#endif // GEOLOCATION_SUPPORT

#ifdef DOT11_SUPPORT
BOOL      parse_Dot11Configuration(XMLN * p_node, onvif_Dot11Configuration * p_req);
ONVIF_RET parse_tds_GetDot11Status(XMLN * p_node, tds_GetDot11Status_REQ * p_req);
ONVIF_RET parse_tds_ScanAvailableDot11Networks(XMLN * p_node, tds_ScanAvailableDot11Networks_REQ * p_req);
#endif // DOT11_SUPPORT

ONVIF_RET parse_Filter(XMLN * p_node, ONVIF_FILTER * p_req);
ONVIF_RET parse_tev_Subscribe(XMLN * p_node, tev_Subscribe_REQ * p_req);
ONVIF_RET parse_tev_Renew(XMLN * p_node, tev_Renew_REQ * p_req);
ONVIF_RET parse_tev_CreatePullPointSubscription(XMLN * p_node, tev_CreatePullPointSubscription_REQ * p_req);
ONVIF_RET parse_tev_PullMessages(XMLN * p_node, tev_PullMessages_REQ * p_req);

#ifdef IMAGE_SUPPORT
ONVIF_RET parse_ImagingSettings(XMLN * p_node, onvif_ImagingSettings * p_req);
ONVIF_RET parse_ImagingOptions(XMLN * p_node, onvif_ImagingOptions * p_req);
ONVIF_RET parse_img_GetImagingSettings(XMLN * p_node, img_GetImagingSettings_REQ * p_req);
ONVIF_RET parse_img_SetImagingSettings(XMLN * p_node, img_SetImagingSettings_REQ * p_req);
ONVIF_RET parse_img_GetOptions(XMLN * p_node, img_GetOptions_REQ * p_req);
ONVIF_RET parse_img_GetMoveOptions(XMLN * p_node, img_GetMoveOptions_REQ * p_req);
ONVIF_RET parse_img_Move(XMLN * p_node, img_Move_REQ * p_req);
ONVIF_RET parse_img_GetStatus(XMLN * p_node, img_GetStatus_REQ * p_req);
ONVIF_RET parse_img_Stop(XMLN * p_node, img_Stop_REQ * p_req);
ONVIF_RET parse_img_GetPresets(XMLN * p_node, img_GetPresets_REQ * p_req);
ONVIF_RET parse_img_GetCurrentPreset(XMLN * p_node, img_GetCurrentPreset_REQ * p_req);
ONVIF_RET parse_img_SetCurrentPreset(XMLN * p_node, img_SetCurrentPreset_REQ * p_req);
#endif // IMAGE_SUPPORT

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)
ONVIF_RET parse_MulticastConfiguration(XMLN * p_node, onvif_MulticastConfiguration * p_req);
ONVIF_RET parse_OSDColor(XMLN * p_node, onvif_OSDColor * p_req);
ONVIF_RET parse_OSDConfiguration(XMLN * p_node, onvif_OSDConfiguration * p_req);
ONVIF_RET parse_VideoSource(XMLN * p_node, onvif_VideoSource * p_req);
ONVIF_RET parse_VideoSourceMode(XMLN * p_node, onvif_VideoSourceMode * p_req);
ONVIF_RET parse_VideoSourceConfiguration(XMLN * p_node, onvif_VideoSourceConfiguration * p_req);
ONVIF_RET parse_VideoEncoder2Configuration(XMLN * p_node, onvif_VideoEncoder2Configuration * p_req);
ONVIF_RET parse_MetadataConfiguration(XMLN * p_node, onvif_MetadataConfiguration * p_req);
ONVIF_RET parse_trt_SetOSD(XMLN * p_node, trt_SetOSD_REQ * p_req);
ONVIF_RET parse_trt_CreateOSD(XMLN * p_node, trt_CreateOSD_REQ * p_req);
ONVIF_RET parse_trt_DeleteOSD(XMLN * p_node, trt_DeleteOSD_REQ * p_req);

#ifdef AUDIO_SUPPORT
ONVIF_RET parse_AudioSourceConfiguration(XMLN * p_node, onvif_AudioSourceConfiguration * p_req);
ONVIF_RET parse_AudioDecoderConfiguration(XMLN * p_node, onvif_AudioDecoderConfiguration * p_req);
ONVIF_RET parse_AudioEncoder2Configuration(XMLN * p_node, onvif_AudioEncoder2Configuration * p_req);
#endif // AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT
ONVIF_RET parse_AudioOutputConfiguration(XMLN * p_node, onvif_AudioOutputConfiguration * p_req);
ONVIF_RET parse_trt_GetAudioOutputConfigurationOptions(XMLN * p_node, trt_GetAudioOutputConfigurationOptions_REQ * p_req);
#endif // DEVICEIO_SUPPORT

#ifdef VIDEO_ANALYTICS
ONVIF_RET parse_AnalyticsEngineConfiguration(XMLN * p_node, onvif_AnalyticsEngineConfiguration * p_req);
ONVIF_RET parse_RuleEngineConfiguration(XMLN * p_node, onvif_RuleEngineConfiguration * p_req);
ONVIF_RET parse_VideoAnalyticsConfiguration(XMLN * p_node, onvif_VideoAnalyticsConfiguration * p_req);
#endif // VIDEO_ANALYTICS

#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef MEDIA_SUPPORT
ONVIF_RET parse_VideoEncoderConfiguration(XMLN * p_node, onvif_VideoEncoderConfiguration * p_req);
ONVIF_RET parse_AudioEncoderConfiguration(XMLN * p_node, onvif_AudioEncoderConfiguration * p_req);
ONVIF_RET parse_AudioDecoderConfigurationOptions(XMLN * p_node, onvif_AudioDecoderConfigurationOptions * p_req);
ONVIF_RET parse_trt_SetVideoEncoderConfiguration(XMLN * p_node, trt_SetVideoEncoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_SetSynchronizationPoint(XMLN * p_node, trt_SetSynchronizationPoint_REQ * p_req);
ONVIF_RET parse_trt_GetProfile(XMLN * p_node, trt_GetProfile_REQ * p_req);
ONVIF_RET parse_trt_CreateProfile(XMLN * p_node, trt_CreateProfile_REQ * p_req);
ONVIF_RET parse_trt_DeleteProfile(XMLN * p_node, trt_DeleteProfile_REQ * p_req);
ONVIF_RET parse_trt_AddVideoSourceConfiguration(XMLN * p_node, trt_AddVideoSourceConfiguration_REQ * p_req);
ONVIF_RET parse_trt_RemoveVideoSourceConfiguration(XMLN * p_node, trt_RemoveVideoSourceConfiguration_REQ * p_req);
ONVIF_RET parse_trt_AddVideoEncoderConfiguration(XMLN * p_node, trt_AddVideoEncoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_RemoveVideoEncoderConfiguration(XMLN * p_node, trt_RemoveVideoEncoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetStreamUri(XMLN * p_node, trt_GetStreamUri_REQ * p_req);
ONVIF_RET parse_trt_GetSnapshotUri(XMLN * p_node, trt_GetSnapshotUri_REQ * p_req);
ONVIF_RET parse_trt_GetVideoSourceConfigurationOptions(XMLN * p_node, trt_GetVideoSourceConfigurationOptions_REQ * p_req);
ONVIF_RET parse_trt_SetVideoSourceConfiguration(XMLN * p_node, trt_SetVideoSourceConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetVideoEncoderConfigurationOptions(XMLN * p_node, trt_GetVideoEncoderConfigurationOptions_REQ * p_req);
ONVIF_RET parse_trt_GetOSDs(XMLN * p_node, trt_GetOSDs_REQ * p_req);
ONVIF_RET parse_trt_GetOSD(XMLN * p_node, trt_GetOSD_REQ * p_req);
ONVIF_RET parse_trt_SetMetadataConfiguration(XMLN * p_node, trt_SetMetadataConfiguration_REQ * p_req);
ONVIF_RET parse_trt_AddMetadataConfiguration(XMLN * p_node, trt_AddMetadataConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetMetadataConfigurationOptions(XMLN * p_node, trt_GetMetadataConfigurationOptions_REQ * p_req);
ONVIF_RET parse_trt_GetVideoSourceModes(XMLN * p_node, trt_GetVideoSourceModes_REQ * p_req);
ONVIF_RET parse_trt_SetVideoSourceMode(XMLN * p_node, trt_SetVideoSourceMode_REQ * p_req);

#ifdef AUDIO_SUPPORT
ONVIF_RET parse_trt_AddAudioSourceConfiguration(XMLN * p_node, trt_AddAudioSourceConfiguration_REQ * p_req);
ONVIF_RET parse_trt_AddAudioEncoderConfiguration(XMLN * p_node, trt_AddAudioEncoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetAudioSourceConfigurationOptions(XMLN * p_node, trt_GetAudioSourceConfigurationOptions_REQ * p_req);
ONVIF_RET parse_trt_SetAudioSourceConfiguration(XMLN * p_node, trt_SetAudioSourceConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetAudioEncoderConfigurationOptions(XMLN * p_node, trt_GetAudioEncoderConfigurationOptions_REQ * p_req);
ONVIF_RET parse_trt_SetAudioEncoderConfiguration(XMLN * p_node, trt_SetAudioEncoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_AddAudioDecoderConfiguration(XMLN * p_node, trt_AddAudioDecoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_RemoveAudioDecoderConfiguration(XMLN * p_node, trt_RemoveAudioDecoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_SetAudioDecoderConfiguration(XMLN * p_node, trt_SetAudioDecoderConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetAudioDecoderConfigurationOptions(XMLN * p_node, trt_GetAudioDecoderConfigurationOptions_REQ * p_req);
#endif // AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT
ONVIF_RET parse_trt_GetAudioOutputConfiguration(XMLN * p_node, trt_GetAudioOutputConfiguration_REQ * p_req);
ONVIF_RET parse_trt_SetAudioOutputConfiguration(XMLN * p_node, tmd_SetAudioOutputConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetCompatibleAudioOutputConfigurations(XMLN * p_node, trt_GetCompatibleAudioOutputConfigurations_REQ * p_req);
ONVIF_RET parse_trt_AddAudioOutputConfiguration(XMLN * p_node, trt_AddAudioOutputConfiguration_REQ * p_req);
ONVIF_RET parse_trt_RemoveAudioOutputConfiguration(XMLN * p_node, trt_RemoveAudioOutputConfiguration_REQ * p_req);
#endif // DEVICEIO_SUPPORT

#ifdef PTZ_SUPPORT
ONVIF_RET parse_trt_AddPTZConfiguration(XMLN * p_node, trt_AddPTZConfiguration_REQ * p_req);
#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS
ONVIF_RET parse_VideoAnalyticsConfiguration(XMLN * p_node, onvif_VideoAnalyticsConfiguration * p_req);
ONVIF_RET parse_trt_AddVideoAnalyticsConfiguration(XMLN * p_node, trt_AddVideoAnalyticsConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetVideoAnalyticsConfiguration(XMLN * p_node, trt_GetVideoAnalyticsConfiguration_REQ * p_req);
ONVIF_RET parse_trt_RemoveVideoAnalyticsConfiguration(XMLN * p_node, trt_RemoveVideoAnalyticsConfiguration_REQ * p_req);
ONVIF_RET parse_trt_SetVideoAnalyticsConfiguration(XMLN * p_node, trt_SetVideoAnalyticsConfiguration_REQ * p_req);
ONVIF_RET parse_trt_GetCompatibleVideoAnalyticsConfigurations(XMLN * p_node, trt_GetCompatibleVideoAnalyticsConfigurations_REQ * p_req);
#endif // VIDEO_ANALYTICS

#endif // MEDIA_SUPPORT

#ifdef PTZ_SUPPORT
ONVIF_RET parse_PTZNode(XMLN * p_node, onvif_PTZNode * p_req);
ONVIF_RET parse_PTZConfiguration(XMLN * p_node, onvif_PTZConfiguration * p_req);
BOOL      parse_PTZSpeed(XMLN * p_node, onvif_PTZSpeed * p_req);
BOOL      parse_PTZVector(XMLN * p_node, onvif_PTZVector * p_req);
ONVIF_RET parse_PTZPreset(XMLN * p_node, onvif_PTZPreset * p_req);
BOOL      parse_PTZPresetTourPresetDetail(XMLN * p_node, onvif_PTZPresetTourPresetDetail * p_req);
BOOL      parse_PTZPresetTourSpot(XMLN * p_node, onvif_PTZPresetTourSpot * p_req);
BOOL      parse_PTZPresetTourStatus(XMLN * p_node, onvif_PTZPresetTourStatus * p_req);
BOOL      parse_PTZPresetTourStartingCondition(XMLN * p_node, onvif_PTZPresetTourStartingCondition * p_req);
BOOL      parse_PresetTour(XMLN * p_node, onvif_PresetTour * p_req);
ONVIF_RET parse_ptz_GetCompatibleConfigurations(XMLN * p_node, ptz_GetCompatibleConfigurations_REQ * p_req);
ONVIF_RET parse_ptz_SetConfiguration(XMLN * p_node, ptz_SetConfiguration_REQ * p_req);
ONVIF_RET parse_ptz_ContinuousMove(XMLN * p_node, ptz_ContinuousMove_REQ * p_req);
ONVIF_RET parse_ptz_Stop(XMLN * p_node, ptz_Stop_REQ * p_req);
ONVIF_RET parse_ptz_AbsoluteMove(XMLN * p_node, ptz_AbsoluteMove_REQ * p_req);
ONVIF_RET parse_ptz_RelativeMove(XMLN * p_node, ptz_RelativeMove_REQ * p_req);
ONVIF_RET parse_ptz_SetPreset(XMLN * p_node, ptz_SetPreset_REQ * p_req);
ONVIF_RET parse_ptz_RemovePreset(XMLN * p_node, ptz_RemovePreset_REQ * p_req);
ONVIF_RET parse_ptz_GotoPreset(XMLN * p_node, ptz_GotoPreset_REQ * p_req);
ONVIF_RET parse_ptz_GotoHomePosition(XMLN * p_node, ptz_GotoHomePosition_REQ * p_req);
ONVIF_RET parse_ptz_GetPresetTours(XMLN * p_node, ptz_GetPresetTours_REQ * p_req);
ONVIF_RET parse_ptz_GetPresetTour(XMLN * p_node, ptz_GetPresetTour_REQ * p_req);
ONVIF_RET parse_ptz_GetPresetTourOptions(XMLN * p_node, ptz_GetPresetTourOptions_REQ * p_req);
ONVIF_RET parse_ptz_CreatePresetTour(XMLN * p_node, ptz_CreatePresetTour_REQ * p_req);
ONVIF_RET parse_ptz_ModifyPresetTour(XMLN * p_node, ptz_ModifyPresetTour_REQ * p_req);
ONVIF_RET parse_ptz_OperatePresetTour(XMLN * p_node, ptz_OperatePresetTour_REQ * p_req);
ONVIF_RET parse_ptz_RemovePresetTour(XMLN * p_node, ptz_RemovePresetTour_REQ * p_req);
ONVIF_RET parse_ptz_SendAuxiliaryCommand(XMLN * p_node, ptz_SendAuxiliaryCommand_REQ * p_req);
ONVIF_RET parse_ptz_GeoMove(XMLN * p_node, ptz_GeoMove_REQ * p_req);
#endif // PTZ_SUPPORT

#ifdef PROFILE_G_SUPPORT
ONVIF_RET parse_RecordingConfiguration(XMLN * p_node, onvif_RecordingConfiguration * p_req);
ONVIF_RET parse_TrackConfiguration(XMLN * p_node, onvif_TrackConfiguration * p_req);
ONVIF_RET parse_Track(XMLN * p_node, onvif_Track * p_req);
ONVIF_RET parse_Recording(XMLN * p_node, onvif_Recording * p_req);
ONVIF_RET parse_JobConfiguration(XMLN * p_node, onvif_RecordingJobConfiguration * p_req);
ONVIF_RET parse_RecordingJob(XMLN * p_node, onvif_RecordingJob * p_req);
ONVIF_RET parse_SearchScope(XMLN * p_node, onvif_SearchScope * p_req);
ONVIF_RET parse_trc_CreateRecording(XMLN * p_node, trc_CreateRecording_REQ * p_req);
ONVIF_RET parse_trc_SetRecordingConfiguration(XMLN * p_node, trc_SetRecordingConfiguration_REQ * p_req);
ONVIF_RET parse_trc_CreateTrack(XMLN * p_node, trc_CreateTrack_REQ * p_req);
ONVIF_RET parse_trc_DeleteTrack(XMLN * p_node, trc_DeleteTrack_REQ * p_req);
ONVIF_RET parse_trc_GetTrackConfiguration(XMLN * p_node, trc_GetTrackConfiguration_REQ * p_req);
ONVIF_RET parse_trc_SetTrackConfiguration(XMLN * p_node, trc_SetTrackConfiguration_REQ * p_req);
ONVIF_RET parse_trc_CreateRecordingJob(XMLN * p_node, trc_CreateRecordingJob_REQ * p_req);
ONVIF_RET parse_trc_SetRecordingJobConfiguration(XMLN * p_node, trc_SetRecordingJobConfiguration_REQ * p_req);
ONVIF_RET parse_trc_SetRecordingJobMode(XMLN * p_node, trc_SetRecordingJobMode_REQ * p_req);
ONVIF_RET parse_trc_ExportRecordedData(XMLN * p_node, trc_ExportRecordedData_REQ * p_req);
ONVIF_RET parse_trc_StopExportRecordedData(XMLN * p_node, trc_StopExportRecordedData_REQ * p_req);
ONVIF_RET parse_trc_GetExportRecordedDataState(XMLN * p_node, trc_GetExportRecordedDataState_REQ * p_req);
ONVIF_RET parse_tse_GetRecordingInformation(XMLN * p_node, tse_GetRecordingInformation_REQ * p_req);
ONVIF_RET parse_tse_GetMediaAttributes(XMLN * p_node, tse_GetMediaAttributes_REQ * p_req);
ONVIF_RET parse_tse_FindRecordings(XMLN * p_node, tse_FindRecordings_REQ * p_req);
ONVIF_RET parse_tse_GetRecordingSearchResults(XMLN * p_node, tse_GetRecordingSearchResults_REQ * p_req);
ONVIF_RET parse_tse_FindEvents(XMLN * p_node, tse_FindEvents_REQ * p_req);
ONVIF_RET parse_tse_GetEventSearchResults(XMLN * p_node, tse_GetEventSearchResults_REQ * p_req);
ONVIF_RET parse_tse_FindMetadata(XMLN * p_node, tse_FindMetadata_REQ * p_req);
ONVIF_RET parse_tse_GetMetadataSearchResults(XMLN * p_node, tse_GetMetadataSearchResults_REQ * p_req);
ONVIF_RET parse_tse_EndSearch(XMLN * p_node, tse_EndSearch_REQ * p_req);
ONVIF_RET parse_tse_GetSearchState(XMLN * p_node, tse_GetSearchState_REQ * p_req);

#ifdef PTZ_SUPPORT
BOOL      parse_PTZPositionFilter(XMLN * p_node, onvif_PTZPositionFilter * p_req);
ONVIF_RET parse_tse_FindPTZPosition(XMLN * p_node, tse_FindPTZPosition_REQ * p_req);
ONVIF_RET parse_tse_GetPTZPositionSearchResults(XMLN * p_node, tse_GetPTZPositionSearchResults_REQ * p_req);
#endif

ONVIF_RET parse_trp_GetReplayUri(XMLN * p_node, trp_GetReplayUri_REQ * p_req);
ONVIF_RET parse_trp_SetReplayConfiguration(XMLN * p_node, trp_SetReplayConfiguration_REQ * p_req);
#endif	// PROFILE_G_SUPPORT

#ifdef VIDEO_ANALYTICS
ONVIF_RET parse_SimpleItem(XMLN * p_node, onvif_SimpleItem * p_req);
ONVIF_RET parse_ElementItem(XMLN * p_node, onvif_ElementItem * p_req);
ONVIF_RET parse_Config(XMLN * p_node, onvif_Config * p_req);
ONVIF_RET parse_tan_GetSupportedRules(XMLN * p_node, tan_GetSupportedRules_REQ * p_req);
ONVIF_RET parse_tan_CreateRules(XMLN * p_node, tan_CreateRules_REQ * p_req);
ONVIF_RET parse_tan_DeleteRules(XMLN * p_node, tan_DeleteRules_REQ * p_req);
ONVIF_RET parse_tan_GetRules(XMLN * p_node, tan_GetRules_REQ * p_req);
ONVIF_RET parse_tan_ModifyRules(XMLN * p_node, tan_ModifyRules_REQ * p_req);
ONVIF_RET parse_tan_CreateAnalyticsModules(XMLN * p_node, tan_CreateAnalyticsModules_REQ * p_req);
ONVIF_RET parse_tan_DeleteAnalyticsModules(XMLN * p_node, tan_DeleteAnalyticsModules_REQ * p_req);
ONVIF_RET parse_tan_GetAnalyticsModules(XMLN * p_node, tan_GetAnalyticsModules_REQ * p_req);
ONVIF_RET parse_tan_ModifyAnalyticsModules(XMLN * p_node, tan_ModifyAnalyticsModules_REQ * p_req);
ONVIF_RET parse_tan_GetRuleOptions(XMLN * p_node, tan_GetRuleOptions_REQ * p_req);
ONVIF_RET parse_tan_GetSupportedAnalyticsModules(XMLN * p_node, tan_GetSupportedAnalyticsModules_REQ * p_req);
ONVIF_RET parse_tan_GetAnalyticsModuleOptions(XMLN * p_node, tan_GetAnalyticsModuleOptions_REQ * p_req);
ONVIF_RET parse_tan_GetSupportedMetadata(XMLN * p_node, tan_GetSupportedMetadata_REQ * p_req);
#endif	// VIDEO_ANALYTICS

#ifdef PROFILE_C_SUPPORT
ONVIF_RET parse_DoorCapabilities(XMLN * p_node, onvif_DoorCapabilities * p_req);
ONVIF_RET parse_AccessPointCapabilities(XMLN * p_node, onvif_AccessPointCapabilities * p_req);
ONVIF_RET parse_AccessPointInfo(XMLN * p_node, onvif_AccessPointInfo * p_req);
ONVIF_RET parse_AreaInfo(XMLN * p_node, onvif_AreaInfo * p_req);
ONVIF_RET parse_DoorInfo(XMLN * p_node, onvif_DoorInfo * p_req);
ONVIF_RET parse_Timings(XMLN * p_node, onvif_Timings * p_req);
ONVIF_RET parse_Door(XMLN * p_node, onvif_Door * p_req);
ONVIF_RET parse_DoorState(XMLN * p_node, onvif_DoorState * p_req);
ONVIF_RET parse_tac_GetAccessPointList(XMLN * p_node, tac_GetAccessPointList_REQ * p_req);
ONVIF_RET parse_tac_GetAccessPoints(XMLN * p_node, tac_GetAccessPoints_REQ * p_req);
ONVIF_RET parse_tac_CreateAccessPoint(XMLN * p_node, tac_CreateAccessPoint_REQ * p_req);
ONVIF_RET parse_tac_SetAccessPoint(XMLN * p_node, tac_SetAccessPoint_REQ * p_req);
ONVIF_RET parse_tac_ModifyAccessPoint(XMLN * p_node, tac_ModifyAccessPoint_REQ * p_req);
ONVIF_RET parse_tac_DeleteAccessPoint(XMLN * p_node, tac_DeleteAccessPoint_REQ * p_req);
ONVIF_RET parse_tac_GetAccessPointInfoList(XMLN * p_node, tac_GetAccessPointInfoList_REQ * p_req);
ONVIF_RET parse_tac_GetAccessPointInfo(XMLN * p_node, tac_GetAccessPointInfo_REQ * p_req);
ONVIF_RET parse_tac_GetAreaList(XMLN * p_node, tac_GetAreaList_REQ * p_req);
ONVIF_RET parse_tac_GetAreas(XMLN * p_node, tac_GetAreas_REQ * p_req);
ONVIF_RET parse_tac_CreateArea(XMLN * p_node, tac_CreateArea_REQ * p_req);
ONVIF_RET parse_tac_SetArea(XMLN * p_node, tac_SetArea_REQ * p_req);
ONVIF_RET parse_tac_ModifyArea(XMLN * p_node, tac_ModifyArea_REQ * p_req);
ONVIF_RET parse_tac_DeleteArea(XMLN * p_node, tac_DeleteArea_REQ * p_req);
ONVIF_RET parse_tac_GetAreaInfoList(XMLN * p_node, tac_GetAreaInfoList_REQ * p_req);
ONVIF_RET parse_tac_GetAreaInfo(XMLN * p_node, tac_GetAreaInfo_REQ * p_req);
ONVIF_RET parse_tac_GetAccessPointState(XMLN * p_node, tac_GetAccessPointState_REQ * p_req);
ONVIF_RET parse_tac_EnableAccessPoint(XMLN * p_node, tac_EnableAccessPoint_REQ * p_req);
ONVIF_RET parse_tac_DisableAccessPoint(XMLN * p_node, tac_DisableAccessPoint_REQ * p_req);
ONVIF_RET parse_tdc_GetDoorList(XMLN * p_node, tdc_GetDoorList_REQ * p_req);
ONVIF_RET parse_tdc_GetDoors(XMLN * p_node, tdc_GetDoors_REQ * p_req);
ONVIF_RET parse_tdc_CreateDoor(XMLN * p_node, tdc_CreateDoor_REQ * p_req);
ONVIF_RET parse_tdc_SetDoor(XMLN * p_node, tdc_SetDoor_REQ * p_req);
ONVIF_RET parse_tdc_ModifyDoor(XMLN * p_node, tdc_ModifyDoor_REQ * p_req);
ONVIF_RET parse_tdc_DeleteDoor(XMLN * p_node, tdc_DeleteDoor_REQ * p_req);
ONVIF_RET parse_tdc_GetDoorInfoList(XMLN * p_node, tdc_GetDoorInfoList_REQ * p_req);
ONVIF_RET parse_tdc_GetDoorInfo(XMLN * p_node, tdc_GetDoorInfo_REQ * p_req);
ONVIF_RET parse_tdc_GetDoorState(XMLN * p_node, tdc_GetDoorState_REQ * p_req);
ONVIF_RET parse_tdc_AccessDoor(XMLN * p_node, tdc_AccessDoor_REQ * p_req);
ONVIF_RET parse_tdc_LockDoor(XMLN * p_node, tdc_LockDoor_REQ * p_req);
ONVIF_RET parse_tdc_UnlockDoor(XMLN * p_node, tdc_UnlockDoor_REQ * p_req);
ONVIF_RET parse_tdc_DoubleLockDoor(XMLN * p_node, tdc_DoubleLockDoor_REQ * p_req);
ONVIF_RET parse_tdc_BlockDoor(XMLN * p_node, tdc_BlockDoor_REQ * p_req);
ONVIF_RET parse_tdc_LockDownDoor(XMLN * p_node, tdc_LockDownDoor_REQ * p_req);
ONVIF_RET parse_tdc_LockDownReleaseDoor(XMLN * p_node, tdc_LockDownReleaseDoor_REQ * p_req);
ONVIF_RET parse_tdc_LockOpenDoor(XMLN * p_node, tdc_LockOpenDoor_REQ * p_req);
ONVIF_RET parse_tdc_LockOpenReleaseDoor(XMLN * p_node, tdc_LockOpenReleaseDoor_REQ * p_req);
#endif  // PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT
ONVIF_RET parse_VideoOutput(XMLN * p_node, onvif_VideoOutput * p_req);
ONVIF_RET parse_VideoOutputConfiguration(XMLN * p_node, onvif_VideoOutputConfiguration * p_req);
ONVIF_RET parse_AudioOutput(XMLN * p_node, onvif_AudioOutput * p_req);
ONVIF_RET parse_AudioOutputConfigurationOptions(XMLN * p_node, onvif_AudioOutputConfigurationOptions * p_req);
ONVIF_RET parse_RelayOutputSettings(XMLN * p_node, onvif_RelayOutputSettings * p_req);
ONVIF_RET parse_RelayOutput(XMLN * p_node, onvif_RelayOutput * p_req);
ONVIF_RET parse_RelayOutputOptions(XMLN * p_node, onvif_RelayOutputOptions * p_req);
ONVIF_RET parse_DigitalInput(XMLN * p_node, onvif_DigitalInput * p_req);
ONVIF_RET parse_DigitalInputConfigurationOptions(XMLN * p_node, onvif_DigitalInputConfigurationOptions * p_req);
ONVIF_RET parse_SerialPortConfiguration(XMLN * p_node, onvif_SerialPortConfiguration * p_req);
ONVIF_RET parse_SerialPortConfigurationOptions(XMLN * p_node, onvif_SerialPortConfigurationOptions * p_req);
ONVIF_RET parse_tmd_SetRelayOutputState(XMLN * p_node, tmd_SetRelayOutputState_REQ * p_req);
ONVIF_RET parse_tmd_GetVideoOutputConfiguration(XMLN * p_node, tmd_GetVideoOutputConfiguration_REQ * p_req);
ONVIF_RET parse_tmd_SetVideoOutputConfiguration(XMLN * p_node, tmd_SetVideoOutputConfiguration_REQ * p_req);
ONVIF_RET parse_tmd_GetVideoOutputConfigurationOptions(XMLN * p_node, tmd_GetVideoOutputConfigurationOptions_REQ * p_req);
ONVIF_RET parse_tmd_GetAudioOutputConfiguration(XMLN * p_node, tmd_GetAudioOutputConfiguration_REQ * p_req);
ONVIF_RET parse_tmd_GetAudioOutputConfigurationOptions(XMLN * p_node, tmd_GetAudioOutputConfigurationOptions_REQ * p_req);
ONVIF_RET parse_tmd_GetRelayOutputOptions(XMLN * p_node, tmd_GetRelayOutputOptions_REQ * p_req);
ONVIF_RET parse_tmd_SetRelayOutputSettings(XMLN * p_node, tmd_SetRelayOutputSettings_REQ * p_req);
ONVIF_RET parse_tmd_GetDigitalInputConfigurationOptions(XMLN * p_node, tmd_GetDigitalInputConfigurationOptions_REQ * p_req);
ONVIF_RET parse_tmd_SetDigitalInputConfigurations(XMLN * p_node, tmd_SetDigitalInputConfigurations_REQ * p_req);
ONVIF_RET parse_tmd_GetSerialPortConfiguration(XMLN * p_node, tmd_GetSerialPortConfiguration_REQ * p_req);
ONVIF_RET parse_tmd_GetSerialPortConfigurationOptions(XMLN * p_node, tmd_GetSerialPortConfigurationOptions_REQ * p_req);
ONVIF_RET parse_tmd_SetSerialPortConfiguration(XMLN * p_node, tmd_SetSerialPortConfiguration_REQ * p_req);
ONVIF_RET parse_tmd_SendReceiveSerialCommand(XMLN * p_node, tmd_SendReceiveSerialCommand_REQ * p_req);
#endif // DEVICEIO_SUPPORT

#ifdef MEDIA2_SUPPORT
ONVIF_RET parse_ConfigurationRef(XMLN * p_node, onvif_ConfigurationRef * p_req);
BOOL      parse_Polygon(XMLN * p_node, onvif_Polygon * p_req);
BOOL      parse_Color(XMLN * p_node, onvif_Color * p_req);
ONVIF_RET parse_Mask(XMLN * p_node, onvif_Mask * p_req);
ONVIF_RET parse_tr2_GetConfiguration(XMLN * p_node, tr2_GetConfiguration * p_req);
ONVIF_RET parse_tr2_SetVideoEncoderConfiguration(XMLN * p_node, tr2_SetVideoEncoderConfiguration_REQ * p_req);
ONVIF_RET parse_tr2_CreateProfile(XMLN * p_node, tr2_CreateProfile_REQ * p_req);
ONVIF_RET parse_tr2_GetProfiles(XMLN * p_node, tr2_GetProfiles_REQ * p_req);
ONVIF_RET parse_tr2_DeleteProfile(XMLN * p_node, tr2_DeleteProfile_REQ * p_req);
ONVIF_RET parse_tr2_AddConfiguration(XMLN * p_node, tr2_AddConfiguration_REQ * p_req);
ONVIF_RET parse_tr2_RemoveConfiguration(XMLN * p_node, tr2_RemoveConfiguration_REQ * p_req);
ONVIF_RET parse_tr2_SetVideoSourceConfiguration(XMLN * p_node, tr2_SetVideoSourceConfiguration_REQ * p_req);
ONVIF_RET parse_tr2_SetMetadataConfiguration(XMLN * p_node, tr2_SetMetadataConfiguration_REQ * p_req);
ONVIF_RET parse_tr2_GetVideoEncoderInstances(XMLN * p_node, tr2_GetVideoEncoderInstances_REQ * p_req);
ONVIF_RET parse_tr2_GetStreamUri(XMLN * p_node, tr2_GetStreamUri_REQ * p_req);
ONVIF_RET parse_tr2_SetSynchronizationPoint(XMLN * p_node, tr2_SetSynchronizationPoint_REQ * p_req);
ONVIF_RET parse_tr2_GetVideoSourceModes(XMLN * p_node, tr2_GetVideoSourceModes_REQ * p_req);
ONVIF_RET parse_tr2_SetVideoSourceMode(XMLN * p_node, tr2_SetVideoSourceMode_REQ * p_req);
ONVIF_RET parse_tr2_GetSnapshotUri(XMLN * p_node, tr2_GetSnapshotUri_REQ * p_req);
ONVIF_RET parse_tr2_GetOSDs(XMLN * p_node, tr2_GetOSDs_REQ * p_req);
ONVIF_RET parse_tr2_CreateMask(XMLN * p_node, tr2_CreateMask_REQ * p_req);
ONVIF_RET parse_tr2_DeleteMask(XMLN * p_node, tr2_DeleteMask_REQ * p_req);
ONVIF_RET parse_tr2_GetMasks(XMLN * p_node, tr2_GetMasks_REQ * p_req);
ONVIF_RET parse_tr2_SetMask(XMLN * p_node, tr2_SetMask_REQ * p_req);
ONVIF_RET parse_tr2_StartMulticastStreaming(XMLN * p_node, tr2_StartMulticastStreaming_REQ * p_req);
ONVIF_RET parse_tr2_StopMulticastStreaming(XMLN * p_node, tr2_StopMulticastStreaming_REQ * p_req);

#ifdef DEVICEIO_SUPPORT
ONVIF_RET parse_tr2_GetAudioOutputConfigurations(XMLN * p_node, tr2_GetAudioOutputConfigurations_REQ * p_req);
ONVIF_RET parse_tr2_SetAudioOutputConfiguration(XMLN * p_node, tr2_SetAudioOutputConfiguration_REQ * p_req);
#endif // DEVICEIO_SUPPORT

#ifdef AUDIO_SUPPORT
ONVIF_RET parse_AudioEncoder2ConfigurationOptions(XMLN * p_node, onvif_AudioEncoder2ConfigurationOptions * p_req);
ONVIF_RET parse_tr2_SetAudioSourceConfiguration(XMLN * p_node, tr2_SetAudioSourceConfiguration_REQ * p_req);
ONVIF_RET parse_tr2_SetAudioEncoderConfiguration(XMLN * p_node, tr2_SetAudioEncoderConfiguration_REQ * p_req);
ONVIF_RET parse_tr2_SetAudioDecoderConfiguration(XMLN * p_node, tr2_SetAudioDecoderConfiguration_REQ * p_req);
#endif // AUDIO_SUPPORT

#endif // MEDIA2_SUPPORT

#ifdef THERMAL_SUPPORT
ONVIF_RET parse_ColorPalette(XMLN * p_node, onvif_ColorPalette * p_req);
ONVIF_RET parse_NUCTable(XMLN * p_node, onvif_NUCTable * p_req);
ONVIF_RET parse_Cooler(XMLN * p_node, onvif_Cooler * p_req);
ONVIF_RET parse_ThermalConfiguration(XMLN * p_node, onvif_ThermalConfiguration * p_req);
ONVIF_RET parse_RadiometryGlobalParameters(XMLN * p_node, onvif_RadiometryGlobalParameters * p_req);
ONVIF_RET parse_RadiometryConfiguration(XMLN * p_node, onvif_RadiometryConfiguration * p_req);
ONVIF_RET parse_tth_GetConfiguration(XMLN * p_node, tth_GetConfiguration_REQ * p_req);
ONVIF_RET parse_tth_SetConfiguration(XMLN * p_node, tth_SetConfiguration_REQ * p_req);
ONVIF_RET parse_tth_GetConfigurationOptions(XMLN * p_node, tth_GetConfigurationOptions_REQ * p_req);
ONVIF_RET parse_tth_GetRadiometryConfiguration(XMLN * p_node, tth_GetRadiometryConfiguration_REQ * p_req);
ONVIF_RET parse_tth_SetRadiometryConfiguration(XMLN * p_node, tth_SetRadiometryConfiguration_REQ * p_req);
ONVIF_RET parse_tth_GetRadiometryConfigurationOptions(XMLN * p_node, tth_GetRadiometryConfigurationOptions_REQ * p_req);
#endif // THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT
ONVIF_RET parse_CredentialIdentifierType(XMLN * p_node, onvif_CredentialIdentifierType * p_req);
ONVIF_RET parse_CredentialIdentifier(XMLN * p_node, onvif_CredentialIdentifier * p_req);
ONVIF_RET parse_CredentialAccessProfile(XMLN * p_node, onvif_CredentialAccessProfile * p_req);
ONVIF_RET parse_Attribute(XMLN * p_node, onvif_Attribute * p_req);
ONVIF_RET parse_Credential(XMLN * p_node, onvif_Credential * p_req);
ONVIF_RET parse_CredentialState(XMLN * p_node, onvif_CredentialState * p_req);
ONVIF_RET parse_CredentialIdentifierItemList(XMLN * p_node, CredentialIdentifierItemList ** p_req);
ONVIF_RET parse_tcr_GetCredentialInfo(XMLN * p_node, tcr_GetCredentialInfo_REQ * p_req);
ONVIF_RET parse_tcr_GetCredentialInfoList(XMLN * p_node, tcr_GetCredentialInfoList_REQ * p_req);
ONVIF_RET parse_tcr_GetCredentials(XMLN * p_node, tcr_GetCredentials_REQ * p_req);
ONVIF_RET parse_tcr_GetCredentialList(XMLN * p_node, tcr_GetCredentialList_REQ * p_req);
ONVIF_RET parse_tcr_CreateCredential(XMLN * p_node, tcr_CreateCredential_REQ * p_req);
ONVIF_RET parse_tcr_ModifyCredential(XMLN * p_node, tcr_ModifyCredential_REQ * p_req);
ONVIF_RET parse_tcr_DeleteCredential(XMLN * p_node, tcr_DeleteCredential_REQ * p_req);
ONVIF_RET parse_tcr_GetCredentialState(XMLN * p_node, tcr_GetCredentialState_REQ * p_req);
ONVIF_RET parse_tcr_EnableCredential(XMLN * p_node, tcr_EnableCredential_REQ * p_req);
ONVIF_RET parse_tcr_DisableCredential(XMLN * p_node, tcr_DisableCredential_REQ * p_req);
ONVIF_RET parse_tcr_SetCredential(XMLN * p_node, tcr_SetCredential_REQ * p_req);
ONVIF_RET parse_tcr_ResetAntipassbackViolation(XMLN * p_node, tcr_ResetAntipassbackViolation_REQ * p_req);
ONVIF_RET parse_tcr_GetSupportedFormatTypes(XMLN * p_node, tcr_GetSupportedFormatTypes_REQ * p_req);
ONVIF_RET parse_tcr_GetCredentialIdentifiers(XMLN * p_node, tcr_GetCredentialIdentifiers_REQ * p_req);
ONVIF_RET parse_tcr_SetCredentialIdentifier(XMLN * p_node, tcr_SetCredentialIdentifier_REQ * p_req);
ONVIF_RET parse_tcr_DeleteCredentialIdentifier(XMLN * p_node, tcr_DeleteCredentialIdentifier_REQ * p_req);
ONVIF_RET parse_tcr_GetCredentialAccessProfiles(XMLN * p_node, tcr_GetCredentialAccessProfiles_REQ * p_req);
ONVIF_RET parse_tcr_SetCredentialAccessProfiles(XMLN * p_node, tcr_SetCredentialAccessProfiles_REQ * p_req);
ONVIF_RET parse_tcr_DeleteCredentialAccessProfiles(XMLN * p_node, tcr_DeleteCredentialAccessProfiles_REQ * p_req);
ONVIF_RET parse_tcr_GetWhitelist(XMLN * p_node, tcr_GetWhitelist_REQ * p_req);
ONVIF_RET parse_tcr_AddToWhitelist(XMLN * p_node, tcr_AddToWhitelist_REQ * p_req);
ONVIF_RET parse_tcr_RemoveFromWhitelist(XMLN * p_node, tcr_RemoveFromWhitelist_REQ * p_req);
ONVIF_RET parse_tcr_GetBlacklist(XMLN * p_node, tcr_GetBlacklist_REQ * p_req);
ONVIF_RET parse_tcr_AddToBlacklist(XMLN * p_node, tcr_AddToBlacklist_REQ * p_req);
ONVIF_RET parse_tcr_RemoveFromBlacklist(XMLN * p_node, tcr_RemoveFromBlacklist_REQ * p_req);
#endif // CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
ONVIF_RET parse_AccessPolicy(XMLN * p_node, onvif_AccessPolicy * p_req);
ONVIF_RET parse_AccessProfile(XMLN * p_node, onvif_AccessProfile * p_req);
ONVIF_RET parse_tar_GetAccessProfileInfo(XMLN * p_node, tar_GetAccessProfileInfo_REQ * p_req);
ONVIF_RET parse_tar_GetAccessProfileInfoList(XMLN * p_node, tar_GetAccessProfileInfoList_REQ * p_req);
ONVIF_RET parse_tar_GetAccessProfiles(XMLN * p_node, tar_GetAccessProfiles_REQ * p_req);
ONVIF_RET parse_tar_GetAccessProfileList(XMLN * p_node, tar_GetAccessProfileList_REQ * p_req);
ONVIF_RET parse_tar_CreateAccessProfile(XMLN * p_node, tar_CreateAccessProfile_REQ * p_req);
ONVIF_RET parse_tar_ModifyAccessProfile(XMLN * p_node, tar_ModifyAccessProfile_REQ * p_req);
ONVIF_RET parse_tar_DeleteAccessProfile(XMLN * p_node, tar_DeleteAccessProfile_REQ * p_req);
ONVIF_RET parse_tar_SetAccessProfile(XMLN * p_node, tar_SetAccessProfile_REQ * p_req);
#endif // ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
ONVIF_RET parse_TimePeriod(XMLN * p_node, onvif_TimePeriod * p_req);
ONVIF_RET parse_SpecialDaysSchedule(XMLN * p_node, onvif_SpecialDaysSchedule * p_req);
ONVIF_RET parse_Schedule(XMLN * p_node, onvif_Schedule * p_req);
ONVIF_RET parse_SpecialDayGroup(XMLN * p_node, onvif_SpecialDayGroup * p_req);
ONVIF_RET parse_tsc_GetScheduleInfo(XMLN * p_node, tsc_GetScheduleInfo_REQ * p_req);
ONVIF_RET parse_tsc_GetScheduleInfoList(XMLN * p_node, tsc_GetScheduleInfoList_REQ * p_req);
ONVIF_RET parse_tsc_GetSchedules(XMLN * p_node, tsc_GetSchedules_REQ * p_req);
ONVIF_RET parse_tsc_GetScheduleList(XMLN * p_node, tsc_GetScheduleList_REQ * p_req);
ONVIF_RET parse_tsc_CreateSchedule(XMLN * p_node, tsc_CreateSchedule_REQ * p_req);
ONVIF_RET parse_tsc_ModifySchedule(XMLN * p_node, tsc_ModifySchedule_REQ * p_req);
ONVIF_RET parse_tsc_DeleteSchedule(XMLN * p_node, tsc_DeleteSchedule_REQ * p_req);
ONVIF_RET parse_tsc_GetSpecialDayGroupInfo(XMLN * p_node, tsc_GetSpecialDayGroupInfo_REQ * p_req);
ONVIF_RET parse_tsc_GetSpecialDayGroupInfoList(XMLN * p_node, tsc_GetSpecialDayGroupInfoList_REQ * p_req);
ONVIF_RET parse_tsc_GetSpecialDayGroups(XMLN * p_node, tsc_GetSpecialDayGroups_REQ * p_req);
ONVIF_RET parse_tsc_GetSpecialDayGroupList(XMLN * p_node, tsc_GetSpecialDayGroupList_REQ * p_req);
ONVIF_RET parse_tsc_CreateSpecialDayGroup(XMLN * p_node, tsc_CreateSpecialDayGroup_REQ * p_req);
ONVIF_RET parse_tsc_ModifySpecialDayGroup(XMLN * p_node, tsc_ModifySpecialDayGroup_REQ * p_req);
ONVIF_RET parse_tsc_DeleteSpecialDayGroup(XMLN * p_node, tsc_DeleteSpecialDayGroup_REQ * p_req);
ONVIF_RET parse_tsc_GetScheduleState(XMLN * p_node, tsc_GetScheduleState_REQ * p_req);
ONVIF_RET parse_tsc_SetSchedule(XMLN * p_node, tsc_SetSchedule_REQ * p_req);
ONVIF_RET parse_tsc_SetSpecialDayGroup(XMLN * p_node, tsc_SetSpecialDayGroup_REQ * p_req);
#endif // SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT
ONVIF_RET parse_ReceiverConfiguration(XMLN * p_node, onvif_ReceiverConfiguration * p_req);
ONVIF_RET parse_Receiver(XMLN * p_node, onvif_Receiver * p_req);
ONVIF_RET parse_trv_GetReceiver(XMLN * p_node, trv_GetReceiver_REQ * p_req);
ONVIF_RET parse_trv_CreateReceiver(XMLN * p_node, trv_CreateReceiver_REQ * p_req);
ONVIF_RET parse_trv_DeleteReceiver(XMLN * p_node, trv_DeleteReceiver_REQ * p_req);
ONVIF_RET parse_trv_ConfigureReceiver(XMLN * p_node, trv_ConfigureReceiver_REQ * p_req);
ONVIF_RET parse_trv_SetReceiverMode(XMLN * p_node, trv_SetReceiverMode_REQ * p_req);
ONVIF_RET parse_trv_GetReceiverState(XMLN * p_node, trv_GetReceiverState_REQ * p_req);
#endif // RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT
ONVIF_RET parse_tpv_PanMove(XMLN * p_node, tpv_PanMove_REQ * p_req);
ONVIF_RET parse_tpv_TiltMove(XMLN * p_node, tpv_TiltMove_REQ * p_req);
ONVIF_RET parse_tpv_ZoomMove(XMLN * p_node, tpv_ZoomMove_REQ * p_req);
ONVIF_RET parse_tpv_RollMove(XMLN * p_node, tpv_RollMove_REQ * p_req);
ONVIF_RET parse_tpv_FocusMove(XMLN * p_node, tpv_FocusMove_REQ * p_req);
ONVIF_RET parse_tpv_Stop(XMLN * p_node, tpv_Stop_REQ * p_req);
ONVIF_RET parse_tpv_GetUsage(XMLN * p_node, tpv_GetUsage_REQ * p_req);
#endif // PROVISIONING_SUPPORT


ONVIF_RET parse_PrivacyMask(XMLN * p_node, onvif_Mask * p_req);

ONVIF_RET parse_hbgk_ext_PrivacyMask(XMLN * p_node, ewsd_onvif_Mask * p_req);
ONVIF_RET parse_hbgk_ext_CreatePrivacyMask(XMLN * p_node, ewsd_CreateMask_REQ * p_req);
ONVIF_RET parse_hbgk_ext_DeletePrivacyMask(XMLN * p_node, ewsd_DeleteMask_REQ * p_req);
ONVIF_RET parse_hbgk_ext_GetPrivacyMasks(XMLN * p_node, ewsd_GetMasks_REQ * p_req);
ONVIF_RET parse_hbgk_ext_SetPrivacyMask(XMLN * p_node, ewsd_SetMask_REQ * p_req);

ONVIF_RET parse_tpl_PrivacyMask(XMLN * p_node, tpl_onvif_Mask * p_req);
ONVIF_RET parse_tpl_CreatePrivacyMask(XMLN * p_node, tpl_CreateMask_REQ * p_req);
ONVIF_RET parse_tpl_DeletePrivacyMask(XMLN * p_node, tpl_DeleteMask_REQ * p_req);
ONVIF_RET parse_tpl_GetPrivacyMasks(XMLN * p_node, tpl_GetMasks_REQ * p_req);
ONVIF_RET parse_tpl_SetPrivacyMask(XMLN * p_node, tpl_SetMask_REQ * p_req);

ONVIF_RET parse_tpl_Command(XMLN * p_node, tpl_onvif_Command * p_req);
ONVIF_RET parse_tpl_SendCommand(XMLN * p_node, tpl_SendCommand_REQ * p_req);
ONVIF_RET parse_tpl_SetImagingSettingDefault(XMLN * p_node, tpl_SetImagingSettingDefault_REQ * p_req);

#ifdef __cplusplus
}
#endif

#endif


