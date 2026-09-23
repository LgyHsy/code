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

#ifndef ONVIF_MEDIA_H
#define ONVIF_MEDIA_H


typedef struct
{
	char name[ONVIF_NAME_LEN];
	char token[ONVIF_TOKEN_LEN];
} CreateProfile_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} AddVideoSourceConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} AddVideoEncoderConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} AddAudioSourceConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} AddAudioEncoderConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} AddPTZConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	int  stream_type;	/* 0: RTP-Unicast, 1: RTP-Multicast */
	int  protocol;		/* 0: UDP 1: TCP 2: RTSP 3: HTTP */
} GetStreamUri_REQ;

typedef struct
{
	char token[32];
	char name[32];
	
	int  use_count;
	
    int  width;
    int  height;
    
	int  quality;
    int  session_timeout;
    int  framerate_limit;
	int  encoding_interval;
	int  bitrate_limit;

	E_VIDEO_ENCODING encoding;
	
	int  gov_len;
	int  profile;

	BOOL persistence;
} SetVideoEncoderConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} GetVideoSourceConfigurationOptions_REQ;

typedef struct
{
	char config_token[ONVIF_TOKEN_LEN];
	char name[ONVIF_NAME_LEN];
	char source_token[ONVIF_TOKEN_LEN];

	int  use_count;
	int  persistence;
	
	// Bounds   
    int  width;
    int  height;
    int  x;
    int  y;
} SetVideoSourceConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} GetVideoEncoderConfigurationOptions_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} GetAudioSourceConfigurationOptions_REQ;

typedef struct
{
	char config_token[ONVIF_TOKEN_LEN];
	char name[ONVIF_NAME_LEN];
	char source_token[ONVIF_TOKEN_LEN];

	int  use_count;
	int  persistence;
} SetAudioSourceConfiguration_REQ;

typedef struct
{
	char profile_token[ONVIF_TOKEN_LEN];
	char config_token[ONVIF_TOKEN_LEN];
} GetAudioEncoderConfigurationOptions_REQ;

typedef struct
{
	char token[32];
	char name[32];
	
	int  use_count;
	int  session_timeout;
	int  sample_rate;
	int  bitrate;

	E_AUDIO_ENCODING encoding;

	BOOL persistence;
} SetAudioEncoderConfiguration_REQ;



#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET onvif_CreateProfile(CreateProfile_REQ * p_CreateProfile_req);
ONVIF_RET onvif_DeleteProfile(const char * token);
ONVIF_RET onvif_AddVideoSourceConfiguration(AddVideoSourceConfiguration_REQ * p_AddVideoSourceConfiguration_req);
ONVIF_RET onvif_AddVideoEncoderConfiguration(AddVideoEncoderConfiguration_REQ * p_AddVideoEncoderConfiguration_req);
ONVIF_RET onvif_AddAudioSourceConfiguration(AddAudioSourceConfiguration_REQ * p_AddAudioSourceConfiguration_req);
ONVIF_RET onvif_AddAudioEncoderConfiguration(AddAudioEncoderConfiguration_REQ * p_AddAudioEncoderConfiguration_req);
ONVIF_RET onvif_AddPTZConfiguration(AddPTZConfiguration_REQ * p_AddPTZConfiguration_req);
ONVIF_RET onvif_RemoveVideoEncoderConfiguration(const char * token);
ONVIF_RET onvif_RemoveVideoSourceConfiguration(const char * token);
ONVIF_RET onvif_RemoveAudioEncoderConfiguration(const char * token);
ONVIF_RET onvif_RemoveAudioSourceConfiguration(const char * token);
ONVIF_RET onvif_RemovePTZConfiguration(const char * token);
ONVIF_RET onvif_SetVideoEncoderConfiguration(SetVideoEncoderConfiguration_REQ * p_SetVideoEncoderConfiguration_req);
ONVIF_RET onvif_SetVideoSourceConfiguration(SetVideoSourceConfiguration_REQ * p_SetVideoSourceConfiguration_req);
ONVIF_RET onvif_SetAudioSourceConfiguration(SetAudioSourceConfiguration_REQ * p_SetAudioSourceConfiguration_req);
ONVIF_RET onvif_SetAudioEncoderConfiguration(SetAudioEncoderConfiguration_REQ * P_SetAudioEncoderConfiguration_req);


#ifdef __cplusplus
}
#endif


#endif


