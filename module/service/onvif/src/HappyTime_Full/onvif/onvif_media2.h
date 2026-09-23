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

#ifndef ONVIF_MEDIA2_H
#define ONVIF_MEDIA2_H

#include "onvif.h"
#include "onvif_cm.h"


#define TR2_MAX_TYPE            10
#define TR2_MAX_CONFIGURATION   10

typedef struct 
{
    uint32  ConfigurationTokenFlag  : 1;                    // Indicates whether the field ConfigurationToken is valid
    uint32  ProfileTokenFlag        : 1;                    // Indicates whether the field ProfileToken is valid
    uint32  Reserved                : 30;
    
    char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // optional, Token of the requested configuration
	char    ProfileToken[ONVIF_TOKEN_LEN];	                // optional, Contains the token of an existing media profile the configurations shall be compatible with
} tr2_GetConfiguration;

typedef struct 
{
	char    Name[ONVIF_NAME_LEN];	                        // required, friendly name of the profile to be created
    
	int     sizeConfiguration;
	onvif_ConfigurationRef  Configuration[TR2_MAX_CONFIGURATION];   // optional, Optional set of configurations to be assigned to the profile
} tr2_CreateProfile_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];                         // required, Token assigned by the device for the newly created profile
} tr2_CreateProfile_RES;

typedef struct 
{
    uint32  TokenFlag   : 1;                                // Indicates whether the field Token is valid
    uint32  Reserved    : 31;
    
	char    Token[ONVIF_TOKEN_LEN];							// optional, Optional token of the requested profile

	int     sizeType;	
	char    Type[TR2_MAX_TYPE][32];	                        // optional, The types shall be provided as defined by tr2:ConfigurationEnumeration
	                                                        //      All, VideoSource, VideoEncoder, AudioSource, AudioEncoder, AudioOutput,
                                        					//      AudioDecoder, Metadata, Analytics, PTZ
} tr2_GetProfiles_REQ;

typedef struct 
{
    uint32  NameFlag    : 1;                                // Indicates whether the field Name is valid
    uint32  Reserved    : 31;
    
	char    ProfileToken[ONVIF_TOKEN_LEN];	                // required, Reference to the profile where the configuration should be added
	char    Name[ONVIF_NAME_LEN];	                        // optional, Optional item. If present updates the Name property of the profile

	int     sizeConfiguration;	
	onvif_ConfigurationRef  Configuration[TR2_MAX_CONFIGURATION];   // optional, List of configurations to be added
} tr2_AddConfiguration_REQ;

typedef struct 
{
	char    ProfileToken[ONVIF_TOKEN_LEN];	                // required, This element contains a  reference to the media profile

	int     sizeConfiguration;
	onvif_ConfigurationRef  Configuration[TR2_MAX_CONFIGURATION];	// required, List of configurations to be removed
} tr2_RemoveConfiguration_REQ;

typedef struct 
{
	char    Token[ONVIF_TOKEN_LEN];	                        // required, This element contains a  reference to the profile that should be deleted
} tr2_DeleteProfile_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetVideoEncoderConfigurations_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetVideoSourceConfigurations_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioEncoderConfigurations_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioSourceConfigurations_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAnalyticsConfigurations_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetMetadataConfigurations_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioOutputConfigurations_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioDecoderConfigurations_REQ;

typedef struct
{
    onvif_VideoEncoder2Configuration    Configuration;      // Contains the modified video encoder configuration. The configuration shall exist in the device
} tr2_SetVideoEncoderConfiguration_REQ;

typedef struct 
{
	onvif_VideoSourceConfiguration      Configuration;	    // required, Contains the modified video source configuration. The configuration shall exist in the device
} tr2_SetVideoSourceConfiguration_REQ;

typedef struct 
{
	onvif_AudioEncoder2Configuration    Configuration;	    // required, Contains the modified audio encoder configuration. The configuration shall exist in the device
} tr2_SetAudioEncoderConfiguration_REQ;

typedef struct 
{
	onvif_AudioSourceConfiguration      Configuration;	    // required, Contains the modified audio source configuration. The configuration shall exist in the device
} tr2_SetAudioSourceConfiguration_REQ;

typedef struct 
{
	onvif_MetadataConfiguration Configuration;	            // required, Contains the modified metadata configuration. The configuration shall exist in the device
} tr2_SetMetadataConfiguration_REQ;

typedef struct 
{
	onvif_AudioOutputConfiguration  Configuration;	        // required, Contains the modified audio output configuration. The configuration shall exist in the device
} tr2_SetAudioOutputConfiguration_REQ;

typedef struct 
{
	onvif_AudioDecoderConfiguration Configuration;	        // required, Contains the modified audio decoder configuration. The configuration shall exist in the device
} tr2_SetAudioDecoderConfiguration_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetVideoSourceConfigurationOptions_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetVideoEncoderConfigurationOptions_REQ;

typedef struct
{
    VideoEncoder2ConfigurationOptionsList * Options;
} tr2_GetVideoEncoderConfigurationOptions_RES;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioSourceConfigurationOptions_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioEncoderConfigurationOptions_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetMetadataConfigurationOptions_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioOutputConfigurationOptions_REQ;

typedef struct 
{
	tr2_GetConfiguration    GetConfiguration;
} tr2_GetAudioDecoderConfigurationOptions_REQ;

typedef struct 
{
	char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // required, Token of the video source configuration
} tr2_GetVideoEncoderInstances_REQ;

typedef struct 
{
	onvif_EncoderInstanceInfo   Info;	                    // required
} tr2_GetVideoEncoderInstances_RES;

typedef struct 
{
	char    Protocol[32];									// required, Defines the network protocol
	                                                        //  RtspUnicast -- RTSP straming RTP as UDP Unicast
	                                                        //  RtspMulticast -- RTSP straming RTP as UDP Multicast
	                                                        //  RTSP -- RTSP straming RTP over TCP
	                                                        //  RtspOverHttp -- Tunneling both the RTSP control channel and the RTP stream over HTTP or HTTPS
	char    ProfileToken[ONVIF_TOKEN_LEN];	                // required, The ProfileToken element indicates the media profile to use and will define the configuration of the content of the stream
} tr2_GetStreamUri_REQ;

typedef struct
{
    char    Uri[ONVIF_URI_LEN];                             // required, 
} tr2_GetStreamUri_RES;

typedef struct 
{
	char    ProfileToken[ONVIF_TOKEN_LEN];	                // required, Contains a Profile reference for which a Synchronization Point is requested
} tr2_SetSynchronizationPoint_REQ;

typedef struct
{
    char    VideoSourceToken[ONVIF_TOKEN_LEN];	            // required
} tr2_GetVideoSourceModes_REQ;

typedef struct
{
    char    VideoSourceToken[ONVIF_TOKEN_LEN];	            // required
	char    VideoSourceModeToken[ONVIF_TOKEN_LEN];	        // required
} tr2_SetVideoSourceMode_REQ;

typedef struct
{
    BOOL    Reboot;                                         // required, The response contains information about rebooting after returning response. 
                                                            //  When Reboot is set true, a device will reboot automatically after setting mode
} tr2_SetVideoSourceMode_RES;

typedef struct
{
    char    ProfileToken[ONVIF_TOKEN_LEN];                  // required
} tr2_GetSnapshotUri_REQ;

typedef struct
{
    char    Uri[256];                                       // required
} tr2_GetSnapshotUri_RES;

typedef struct
{
    uint32  OSDTokenFlag            : 1;                    // Indicates whether the field OSDToken is valid
    uint32  ConfigurationTokenFlag  : 1;                    // Indicates whether the field ConfigurationToken is valid
    uint32  Reserved                : 30;
    
    char    OSDToken[ONVIF_TOKEN_LEN];	                    // optional, The GetOSDs command fetches the OSD configuration if the OSD token is known
	char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // optional, Token of the Video Source Configuration, which has OSDs associated with are requested. 
	                                                        //  If token not exist, request all available OSDs
} tr2_GetOSDs_REQ;

typedef struct
{
    onvif_Mask  Mask;	                                    // required, Contain the initial mask configuration for create
} tr2_CreateMask_REQ;

typedef struct
{
    char    Token[ONVIF_TOKEN_LEN];	                        // required, Returns Token of the newly created Mask
} tr2_CreateMask_RES;

typedef struct
{
    uint32  TokenFlag               : 1;                    // Indicates whether the field Token is valid
    uint32  ConfigurationTokenFlag  : 1;                    // Indicates whether the field ConfigurationToken is valid
    uint32  Reserved                : 30;
    
    char    Token[ONVIF_TOKEN_LEN];	                        // optional, Optional mask token of an existing mask
	char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // optional, Optional token of a Video Source Configuration
} tr2_GetMasks_REQ;

typedef struct
{
    MaskList * Masks;                                       // optional, List of Mask configurations
} tr2_GetMasks_RES;

typedef struct
{
    onvif_Mask  Mask;	                                    // required, Mask to be updated
} tr2_SetMask_REQ;

typedef struct 
{
	char    Token[ONVIF_TOKEN_LEN];	                        // required, This element contains a reference to the Mask configuration that should be deleted
} tr2_DeleteMask_REQ;

typedef struct 
{
	char    ConfigurationToken[ONVIF_TOKEN_LEN];            // required, Video Source Configuration Token that specifies an existing video source configuration that the options shall be compatible with
} tr2_GetMaskOptions_REQ;

typedef struct 
{
	onvif_MaskOptions   Options;	                        // required, 
} tr2_GetMaskOptions_RES;

typedef struct
{
    char    ProfileToken[ONVIF_TOKEN_LEN];                  // required, 
} tr2_StartMulticastStreaming_REQ;

typedef struct
{
    char    ProfileToken[ONVIF_TOKEN_LEN];                  // required, 
} tr2_StopMulticastStreaming_REQ;

#if 1

typedef struct 
{
	char    PrivacyMaskToken[ONVIF_TOKEN_LEN];	                        // required, This element contains a reference to the Mask configuration that should be deleted
} ewsd_DeleteMask_REQ;

typedef struct
{
	uint32  TokenFlag               : 1;                    // Indicates whether the field Token is valid
	uint32  VideoSourceTokenFlag: 1;
	uint32  Reserved                : 30;

	char    Token[ONVIF_TOKEN_LEN];	                        // optional, Optional mask token of an existing mask
	char    VideoSourceToken[ONVIF_TOKEN_LEN];
} ewsd_GetMasks_REQ;

typedef struct 
{
	uint32  ColorFlag : 1;                              // Indicates whether the field Color is valid
	uint32  Reserved  : 31;

	char VideoSourceToken[ONVIF_TOKEN_LEN];

	onvif_Polygon   Polygon;

	char    Type[64];	                                // required, 
	                                                    //  Color - The masked area is colored with color defined by the Color field
								                        //  Pixelated - The masked area is filled in mosaic style to hide details
								                        //  Blurred - The masked area is low pass filtered to hide details
	onvif_Color Color;	                                // optional, Color of the masked area

	BOOL    Enabled;	                                // required, If set the mask will cover the image, otherwise it will be fully transparent
	char token[ONVIF_TOKEN_LEN];
	
} ewsd_onvif_Mask;

typedef struct
{
	BOOL PrivacyMaskEnable;
	ewsd_onvif_Mask  PrivacyMask;
} ewsd_CreateMask_REQ;

typedef struct
{
	BOOL PrivacyMaskEnable;
	ewsd_onvif_Mask  PrivacyMask;
} ewsd_SetMask_REQ;


typedef struct
{
	uint32  TokenFlag               : 1;                    // Indicates whether the field Token is valid
	uint32  ConfigurationTokenFlag  : 1;                    // Indicates whether the field ConfigurationToken is valid
	uint32  Reserved                : 30;

	char    Token[ONVIF_TOKEN_LEN];	                        // optional, Optional mask token of an existing mask
	char    ConfigurationToken[ONVIF_TOKEN_LEN];	        // optional, Optional token of a Video Source Configuration
} tpl_GetMasks_REQ;

typedef struct 
{
	char    PrivacyMaskToken[ONVIF_TOKEN_LEN];	                        // required, This element contains a reference to the Mask configuration that should be deleted
} tpl_DeleteMask_REQ;

typedef struct 
{
	uint32  ColorFlag : 1;                              // Indicates whether the field Color is valid
	uint32  Reserved  : 31;

	char VideoSourceConfigurationToken[ONVIF_TOKEN_LEN];

	onvif_Polygon   Polygon;

	char    Type[64];	                                // required, 
	                                                    //  Color - The masked area is colored with color defined by the Color field
								                        //  Pixelated - The masked area is filled in mosaic style to hide details
								                        //  Blurred - The masked area is low pass filtered to hide details
	onvif_Color Color;	                                // optional, Color of the masked area

	BOOL    Enabled;	                                // required, If set the mask will cover the image, otherwise it will be fully transparent
	char token[ONVIF_TOKEN_LEN];
	
} tpl_onvif_Mask;

typedef struct
{
	BOOL PrivacyMaskEnable;
	tpl_onvif_Mask  PrivacyMask;
} tpl_CreateMask_REQ;

typedef struct
{
	BOOL PrivacyMaskEnable;
	tpl_onvif_Mask  PrivacyMask;
} tpl_SetMask_REQ;

typedef struct
{
	uint32  CommandDataFlag : 1;
	uint32  ProfileTokenFlag : 1;
	uint32  TokenFlag : 1;
	BOOL CommandData;
	char ProfileToken[ONVIF_TOKEN_LEN];
	char Token[ONVIF_TOKEN_LEN];
} tpl_onvif_Command;

typedef struct
{
	tpl_onvif_Command  SendCommand;
} tpl_SendCommand_REQ;

typedef struct
{
	uint32  VideoSourceTokenFlag : 1;
	char VideoSourceToken[ONVIF_TOKEN_LEN];
} tpl_SetImagingSettingDefault_REQ;

#endif

#ifdef __cplusplus
extern "C" {
#endif

void      onvif_MediaConfigurationChangedNotify(const char * token, const char * type);

ONVIF_RET onvif_tr2_SetVideoEncoderConfiguration(tr2_SetVideoEncoderConfiguration_REQ * p_req);
ONVIF_RET onvif_tr2_GetVideoEncoderConfigurationOptions(tr2_GetVideoEncoderConfigurationOptions_REQ * p_req, tr2_GetVideoEncoderConfigurationOptions_RES * p_res);
ONVIF_RET onvif_tr2_CreateProfile(tr2_CreateProfile_REQ * p_req, tr2_CreateProfile_RES * p_res);
ONVIF_RET onvif_tr2_DeleteProfile(tr2_DeleteProfile_REQ * p_req);
ONVIF_RET onvif_tr2_AddConfiguration(tr2_AddConfiguration_REQ * p_req);
ONVIF_RET onvif_tr2_RemoveConfiguration(tr2_RemoveConfiguration_REQ * p_req);
ONVIF_RET onvif_tr2_SetVideoSourceConfiguration(tr2_SetVideoSourceConfiguration_REQ * p_req);
ONVIF_RET onvif_tr2_SetMetadataConfiguration(tr2_SetMetadataConfiguration_REQ * p_req);
ONVIF_RET onvif_tr2_GetVideoEncoderInstances(tr2_GetVideoEncoderInstances_REQ * p_req, tr2_GetVideoEncoderInstances_RES * p_res);
ONVIF_RET onvif_tr2_SetSynchronizationPoint(tr2_SetSynchronizationPoint_REQ * p_req);
ONVIF_RET onvif_tr2_SetVideoSourceMode(tr2_SetVideoSourceMode_REQ * p_req, tr2_SetVideoSourceMode_RES * p_res);
ONVIF_RET onvif_tr2_GetSnapshotUri(HTTPCLN * p_user, tr2_GetSnapshotUri_REQ * p_req, tr2_GetSnapshotUri_RES * p_res);
ONVIF_RET onvif_tr2_GetStreamUri(HTTPCLN * p_user, tr2_GetStreamUri_REQ * p_req, tr2_GetStreamUri_RES * p_res);
ONVIF_RET onvif_tr2_CreateMask(tr2_CreateMask_REQ * p_req);
ONVIF_RET onvif_tr2_DeleteMask(tr2_DeleteMask_REQ * p_req);
ONVIF_RET onvif_tr2_SetMask(tr2_SetMask_REQ * p_req);
ONVIF_RET onvif_tr2_StartMulticastStreaming(tr2_StartMulticastStreaming_REQ * p_req);
ONVIF_RET onvif_tr2_StopMulticastStreaming(tr2_StopMulticastStreaming_REQ * p_req);

#ifdef AUDIO_SUPPORT
ONVIF_RET onvif_tr2_SetAudioEncoderConfiguration(tr2_SetAudioEncoderConfiguration_REQ * p_req);
ONVIF_RET onvif_tr2_SetAudioSourceConfiguration(tr2_SetAudioSourceConfiguration_REQ * p_req);
ONVIF_RET onvif_tr2_SetAudioDecoderConfiguration(tr2_SetAudioDecoderConfiguration_REQ * p_req);
#endif

#ifdef DEVICEIO_SUPPORT
ONVIF_RET onvif_tr2_SetAudioOutputConfiguration(tr2_SetAudioOutputConfiguration_REQ * p_req);
#endif

ONVIF_RET onvif_hbgk_ext_CreatePrivacyMask(ewsd_CreateMask_REQ * p_req);
ONVIF_RET onvif_hbgk_ext_DeletePrivacyMask(ewsd_DeleteMask_REQ * p_req);
ONVIF_RET onvif_hbgk_ext_SetPrivacyMask(ewsd_SetMask_REQ * p_req);

ONVIF_RET onvif_tpl_CreatePrivacyMask(tpl_CreateMask_REQ * p_req);
ONVIF_RET onvif_tpl_DeletePrivacyMask(tpl_DeleteMask_REQ * p_req);
ONVIF_RET onvif_tpl_SetPrivacyMask(tpl_SetMask_REQ * p_req);
ONVIF_RET onvif_tpl_SendCommand(tpl_SendCommand_REQ * p_req);
ONVIF_RET onvif_tpl_SetImagingSettingDefault(tpl_SetImagingSettingDefault_REQ * p_req);

#ifdef __cplusplus
}
#endif

#endif // ONVIF_MEDIA2_H


