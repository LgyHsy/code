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
#include "onvif.h"
#include "onvif_cm.h"
#include "onvif_cfg.h"
#include "xml_node.h"
#include "onvif_utils.h"
#include "onvif_pkt.h"
#include "soap_parser.h"

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;

/***************************************************************************************/
BOOL onvif_parse_device_information(XMLN * p_node, onvif_DeviceInformation * p_req)
{
	XMLN * p_Manufacturer;
	XMLN * p_Model;
	XMLN * p_FirmwareVersion;
	XMLN * p_SerialNumber;
	XMLN * p_HardwareId;

	p_Manufacturer = xml_node_soap_get(p_node, "Manufacturer");
	if (p_Manufacturer && p_Manufacturer->data)
	{
		strncpy(p_req->Manufacturer, p_Manufacturer->data, sizeof(p_req->Manufacturer)-1);
	}

	p_Model = xml_node_soap_get(p_node, "Model");
	if (p_Model && p_Model->data)
	{
		strncpy(p_req->Model, p_Model->data, sizeof(p_req->Model)-1);
	}
	
	p_FirmwareVersion = xml_node_soap_get(p_node, "FirmwareVersion");
	if (p_FirmwareVersion && p_FirmwareVersion->data)
	{
		strncpy(p_req->FirmwareVersion, p_FirmwareVersion->data, sizeof(p_req->FirmwareVersion)-1);
	}

	p_SerialNumber = xml_node_soap_get(p_node, "SerialNumber");
	if (p_SerialNumber && p_SerialNumber->data)
	{
		strncpy(p_req->SerialNumber, p_SerialNumber->data, sizeof(p_req->SerialNumber)-1);
	}

	p_HardwareId = xml_node_soap_get(p_node, "HardwareId");
	if (p_HardwareId && p_HardwareId->data)
	{
		strncpy(p_req->HardwareId, p_HardwareId->data, sizeof(p_req->HardwareId) - 1);
	}

	return TRUE;
}

BOOL onvif_parse_user(XMLN * p_node, onvif_User * p_req)
{
    XMLN * p_fixed;
	XMLN * p_username;
    XMLN * p_password;
    XMLN * p_userlevel;

    p_fixed = xml_node_soap_get(p_node, "fixed");
	if (p_fixed && p_fixed->data)
	{
		p_req->fixed = parse_Bool(p_fixed->data);
	}
	else
	{
        p_req->fixed = TRUE; // Fixed user, delete is not allowed
	}
	
	p_username = xml_node_soap_get(p_node, "username");
	if (p_username && p_username->data)
	{
		strncpy(p_req->Username, p_username->data, sizeof(p_req->Username)-1);
	}
	else
	{
		return FALSE;
	}

	p_password = xml_node_soap_get(p_node, "password");
	if (p_password && p_password->data)
	{
		strncpy(p_req->Password, p_password->data, sizeof(p_req->Password)-1);
	}

	p_userlevel = xml_node_soap_get(p_node, "userlevel");
	if (p_userlevel && p_userlevel->data)
	{
		p_req->UserLevel = onvif_StringToUserLevel(p_userlevel->data);
	}

	return TRUE;
}

BOOL onvif_parse_remote_user(XMLN * p_node, onvif_RemoteUser * p_req)
{
	XMLN * p_username;
    XMLN * p_password;
    XMLN * p_UseDerivedPassword;
	
	p_username = xml_node_soap_get(p_node, "username");
	if (p_username && p_username->data)
	{
		strncpy(p_req->Username, p_username->data, sizeof(p_req->Username)-1);
	}
	else
	{
		return FALSE;
	}

	p_password = xml_node_soap_get(p_node, "password");
	if (p_password && p_password->data)
	{
		strncpy(p_req->Password, p_password->data, sizeof(p_req->Password)-1);
	}

	p_UseDerivedPassword = xml_node_soap_get(p_node, "UseDerivedPassword");
	if (p_UseDerivedPassword && p_UseDerivedPassword->data)
	{
		p_req->UseDerivedPassword = parse_Bool(p_UseDerivedPassword->data);
	}

	return TRUE;
}

void onvif_parse_h264_options(XMLN * p_node, onvif_VideoEncoder2Configuration * p_req)
{
	XMLN * p_h264;
	XMLN * p_gov_length;
	XMLN * p_h264_profile;

	p_h264 = xml_node_soap_get(p_node, "h264");
	if (NULL == p_h264)
	{
		return;
	}
	
	p_gov_length = xml_node_soap_get(p_h264, "gov_length");
	if (p_gov_length && p_gov_length->data)
	{
	    p_req->GovLengthFlag = 1;
		p_req->GovLength = atoi(p_gov_length->data);
	}

	p_h264_profile = xml_node_soap_get(p_h264, "h264_profile");
	if (p_h264_profile && p_h264_profile->data)
	{
	    p_req->ProfileFlag = 1;
		strncpy(p_req->Profile, p_h264_profile->data, sizeof(p_req->Profile)-1);
	}
}

#ifdef MEDIA2_SUPPORT
void onvif_parse_h265_options(XMLN * p_node, onvif_VideoEncoder2Configuration * p_req)
{
	XMLN * p_h265;
	XMLN * p_gov_length;
	XMLN * p_h265_profile;

	p_h265 = xml_node_soap_get(p_node, "h265");
	if (NULL == p_h265)
	{
		return;
	}
	
	p_gov_length = xml_node_soap_get(p_h265, "gov_length");
	if (p_gov_length && p_gov_length->data)
	{
	    p_req->GovLengthFlag = 1;
		p_req->GovLength = atoi(p_gov_length->data);
	}

	p_h265_profile = xml_node_soap_get(p_h265, "h265_profile");
	if (p_h265_profile && p_h265_profile->data)
	{
	    p_req->ProfileFlag = 1;
		strncpy(p_req->Profile, p_h265_profile->data, sizeof(p_req->Profile)-1);
	}
}
#endif

void onvif_parse_mpeg4_options(XMLN * p_node, onvif_VideoEncoder2Configuration * p_req)
{
	XMLN * p_mpeg4;
	XMLN * p_gov_length;
	XMLN * p_mpeg4_profile;

	p_mpeg4 = xml_node_soap_get(p_node, "mpeg4");
	if (NULL == p_mpeg4)
	{
		return;
	}
	
	p_gov_length = xml_node_soap_get(p_mpeg4, "gov_length");
	if (p_gov_length && p_gov_length->data)
	{
	    p_req->GovLengthFlag = 1;
		p_req->GovLength = atoi(p_gov_length->data);
	}

	p_mpeg4_profile = xml_node_soap_get(p_mpeg4, "mpeg4_profile");
	if (p_mpeg4_profile && p_mpeg4_profile->data)
	{
	    p_req->ProfileFlag = 1;
		strncpy(p_req->Profile, p_mpeg4_profile->data, sizeof(p_req->Profile)-1);
	}
}

VideoSourceConfigurationList * onvif_parse_video_source_cfg(XMLN * p_node)
{
	int w = 0, h = 0;
	XMLN * p_width;
	XMLN * p_height;
	VideoSourceConfigurationList * p_v_src_cfg = NULL;
	
	p_width = xml_node_soap_get(p_node, "width");
	if (p_width && p_width->data)
	{
		w = atoi(p_width->data);
	}
	p_height = xml_node_soap_get(p_node, "height");
	if (p_height && p_height->data)
	{
		h = atoi(p_height->data);
	}
	if (w == 0 || h == 0)
	{
		return NULL;
	}
	p_v_src_cfg = onvif_find_VideoSourceConfiguration_by_size(g_onvif_cfg.v_src_cfg, w, h);
	if (p_v_src_cfg)
	{
		return p_v_src_cfg;
	}
	p_v_src_cfg = onvif_add_VideoSourceConfiguration(&g_onvif_cfg.v_src_cfg, w, h);
	if (p_v_src_cfg)
	{
		VideoSourceList * p_v_src = onvif_find_VideoSource_by_size(g_onvif_cfg.v_src, w, h);
		if (NULL == p_v_src)
		{
			p_v_src = onvif_add_VideoSource(&g_onvif_cfg.v_src, w, h, 0, 0, 0, 0);
		}
		if (p_v_src)
		{
			strcpy(p_v_src_cfg->Configuration.SourceToken, p_v_src->VideoSource.token);
			strcpy(p_v_src_cfg->Options.VideoSourceTokensAvailable[0], p_v_src->VideoSource.token);
			p_v_src_cfg->Options.sizeVideoSourceTokensAvailable = 1;
		}
	}
	return p_v_src_cfg;
}

VideoEncoder2ConfigurationList * onvif_parse_video_encoder_cfg(XMLN * p_node)
{
	XMLN * p_width;
	XMLN * p_height;
	XMLN * p_quality;
	XMLN * p_session_timeout;
	XMLN * p_framerate;
	XMLN * p_encoding_interval;
	XMLN * p_bitrate_limit;
	XMLN * p_encoding;
	VideoEncoder2ConfigurationList * p_v_enc_cfg;
	VideoEncoder2ConfigurationList v_enc_cfg;
	
	memset(&v_enc_cfg, 0, sizeof(v_enc_cfg));
	
	p_width = xml_node_soap_get(p_node, "width");
	if (p_width && p_width->data)
	{
		v_enc_cfg.Configuration.Resolution.Width = atoi(p_width->data);
	}

	p_height = xml_node_soap_get(p_node, "height");
	if (p_height && p_height->data)
	{
		v_enc_cfg.Configuration.Resolution.Height = atoi(p_height->data);
	}

	p_quality = xml_node_soap_get(p_node, "quality");
	if (p_quality && p_quality->data)
	{
		v_enc_cfg.Configuration.Quality = (float)atof(p_quality->data);
	}

	p_session_timeout = xml_node_soap_get(p_node, "session_timeout");
	if (p_session_timeout && p_session_timeout->data)
	{
		v_enc_cfg.Configuration.SessionTimeout = atoi(p_session_timeout->data);
	}

	v_enc_cfg.Configuration.RateControlFlag = 1;
	
	p_framerate = xml_node_soap_get(p_node, "framerate");
	if (p_framerate && p_framerate->data)
	{
		v_enc_cfg.Configuration.RateControl.FrameRateLimit = (float)atof(p_framerate->data);
	}

	p_encoding_interval = xml_node_soap_get(p_node, "encoding_interval");
	if (p_encoding_interval && p_encoding_interval->data)
	{
		v_enc_cfg.Configuration.RateControl.EncodingInterval = atoi(p_encoding_interval->data);
	}

	p_bitrate_limit = xml_node_soap_get(p_node, "bitrate_limit");
	if (p_bitrate_limit && p_bitrate_limit->data)
	{
		v_enc_cfg.Configuration.RateControl.BitrateLimit = atoi(p_bitrate_limit->data);
	}

	p_encoding = xml_node_soap_get(p_node, "encoding");
	if (p_encoding && p_encoding->data)
	{
		strncpy(v_enc_cfg.Configuration.Encoding, p_encoding->data, sizeof(v_enc_cfg.Configuration.Encoding)-1);
		v_enc_cfg.Configuration.VideoEncoding = onvif_StringToVideoEncoding(p_encoding->data);
	}

	if (strcasecmp(v_enc_cfg.Configuration.Encoding, "MPEG4") == 0 || strcasecmp(v_enc_cfg.Configuration.Encoding, "MP4V-ES") == 0)
	{
		strcpy(v_enc_cfg.Configuration.Encoding, "MP4V-ES");
		v_enc_cfg.Configuration.VideoEncoding = VideoEncoding_MPEG4;

		onvif_parse_mpeg4_options(p_node, &v_enc_cfg.Configuration);

		if (!v_enc_cfg.Configuration.ProfileFlag)
		{
			v_enc_cfg.Configuration.ProfileFlag = 1;
			strcpy(v_enc_cfg.Configuration.Profile, "SP");
		}

		if (!v_enc_cfg.Configuration.GovLengthFlag)
		{
			v_enc_cfg.Configuration.GovLengthFlag = 1;
			v_enc_cfg.Configuration.GovLength = 30;
		}
	}
	else if (strcasecmp(v_enc_cfg.Configuration.Encoding, "H264") == 0)
	{
		onvif_parse_h264_options(p_node, &v_enc_cfg.Configuration);

		if (!v_enc_cfg.Configuration.ProfileFlag)
		{
			v_enc_cfg.Configuration.ProfileFlag = 1;
			strcpy(v_enc_cfg.Configuration.Profile, "Main");
		}

		if (!v_enc_cfg.Configuration.GovLengthFlag)
		{
			v_enc_cfg.Configuration.GovLengthFlag = 1;
			v_enc_cfg.Configuration.GovLength = 30;
		}
	}
#ifdef MEDIA2_SUPPORT	
	else if (strcasecmp(v_enc_cfg.Configuration.Encoding, "H265") == 0)
	{
		onvif_parse_h265_options(p_node, &v_enc_cfg.Configuration);

		// onvif media 1 service don't support H265, force to H264
		v_enc_cfg.Configuration.VideoEncoding = VideoEncoding_H264;

		if (!v_enc_cfg.Configuration.ProfileFlag)
		{
			v_enc_cfg.Configuration.ProfileFlag = 1;
			strcpy(v_enc_cfg.Configuration.Profile, "Main");
		}

		if (!v_enc_cfg.Configuration.GovLengthFlag)
		{
			v_enc_cfg.Configuration.GovLengthFlag = 1;
			v_enc_cfg.Configuration.GovLength = 30;
		}
	}
#else
	else if (strcasecmp(v_enc_cfg.Configuration.Encoding, "H265") == 0)
	{
		// onvif media 1 service don't support H265, force to H264
		v_enc_cfg.Configuration.VideoEncoding = VideoEncoding_H264;

		if (!v_enc_cfg.Configuration.ProfileFlag)
		{
			v_enc_cfg.Configuration.ProfileFlag = 1;
			strcpy(v_enc_cfg.Configuration.Profile, "Main");
		}

		if (!v_enc_cfg.Configuration.GovLengthFlag)
		{
			v_enc_cfg.Configuration.GovLengthFlag = 1;
			v_enc_cfg.Configuration.GovLength = 30;
		}
	}
#endif
	else if (strcasecmp(v_enc_cfg.Configuration.Encoding, "JPEG"))
	{
		return NULL;
	}

	p_v_enc_cfg = onvif_find_VideoEncoder2Configuration_by_param(g_onvif_cfg.v_enc_cfg, &v_enc_cfg);
	if (p_v_enc_cfg)
	{
		return p_v_enc_cfg;
	}

	p_v_enc_cfg = onvif_add_VideoEncoder2Configuration(&g_onvif_cfg.v_enc_cfg, &v_enc_cfg);

	return p_v_enc_cfg;
}

#ifdef AUDIO_SUPPORT

BOOL onvif_parse_audio_source(XMLN * p_node, onvif_AudioSource * p_req)
{
	XMLN * p_Channels;
	const char * p_token;
	
	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->token, p_token, sizeof(p_req->token)-1);
	}
	
	p_Channels = xml_node_soap_get(p_node, "Channels");
	if (p_Channels && p_Channels->data)
	{
		p_req->Channels = atoi(p_Channels->data);
	}

	return TRUE;
}

AudioSourceConfigurationList * onvif_parse_audio_source_cfg(XMLN * p_node)
{	
	AudioSourceConfigurationList * p_a_src_cfg = g_onvif_cfg.a_src_cfg;
	if (p_a_src_cfg)
	{
		return p_a_src_cfg;
	}

	p_a_src_cfg = onvif_add_AudioSourceConfiguration(&g_onvif_cfg.a_src_cfg);
	if (p_a_src_cfg)
	{
		AudioSourceList * p_a_src = g_onvif_cfg.a_src;
		if (NULL == p_a_src)
		{
			p_a_src = onvif_add_AudioSource(&g_onvif_cfg.a_src);
			if (p_a_src)
			{
				p_a_src->AudioSource.Channels = 2;
			}
		}
		if (p_a_src)
		{
			strcpy(p_a_src_cfg->Configuration.SourceToken, p_a_src->AudioSource.token);
		}
	}
	return p_a_src_cfg;
}

AudioEncoder2ConfigurationList * onvif_parse_audio_encoder_cfg(XMLN * p_node)
{
	XMLN * p_session_timeout;
	XMLN * p_sample_rate;
	XMLN * p_bitrate;
	XMLN * p_encoding;
	AudioEncoder2ConfigurationList * p_a_enc_cfg;
	AudioEncoder2ConfigurationList a_enc_cfg;
	
	memset(&a_enc_cfg, 0, sizeof(AudioEncoder2ConfigurationList));
	
	p_session_timeout = xml_node_soap_get(p_node, "session_timeout");
	if (p_session_timeout && p_session_timeout->data)
	{
		a_enc_cfg.Configuration.SessionTimeout = atoi(p_session_timeout->data);
	}

	p_sample_rate = xml_node_soap_get(p_node, "sample_rate");
	if (p_sample_rate && p_sample_rate->data)
	{
		a_enc_cfg.Configuration.SampleRate = atoi(p_sample_rate->data);
	}

	p_bitrate = xml_node_soap_get(p_node, "bitrate");
	if (p_bitrate && p_bitrate->data)
	{
		a_enc_cfg.Configuration.Bitrate = atoi(p_bitrate->data);
	}	

	p_encoding = xml_node_soap_get(p_node, "encoding");
	if (p_encoding && p_encoding->data)
	{
		strncpy(a_enc_cfg.Configuration.Encoding, p_encoding->data, sizeof(a_enc_cfg.Configuration.Encoding)-1);
		a_enc_cfg.Configuration.AudioEncoding = onvif_StringToAudioEncoding(p_encoding->data);
	}

#ifdef MEDIA2_SUPPORT
	if (strcasecmp(a_enc_cfg.Configuration.Encoding, "PCMU") == 0 || strcasecmp(a_enc_cfg.Configuration.Encoding, "G711") == 0)
	{
		strcpy(a_enc_cfg.Configuration.Encoding, "PCMU");
		a_enc_cfg.Configuration.AudioEncoding = AudioEncoding_G711;
	}
	else if (strcasecmp(a_enc_cfg.Configuration.Encoding, "AAC") == 0 || strcasecmp(a_enc_cfg.Configuration.Encoding, "MP4A-LATM") == 0)
	{
		strcpy(a_enc_cfg.Configuration.Encoding, "MP4A-LATM");
		a_enc_cfg.Configuration.AudioEncoding = AudioEncoding_AAC;
	}
#endif

	p_a_enc_cfg = onvif_find_AudioEncoder2Configuration_by_param(g_onvif_cfg.a_enc_cfg, &a_enc_cfg);
	if (p_a_enc_cfg)
	{
		return p_a_enc_cfg;
	}

	p_a_enc_cfg = onvif_add_AudioEncoder2Configuration(&g_onvif_cfg.a_enc_cfg, &a_enc_cfg);

	return p_a_enc_cfg;
}

#endif // end of AUDIO_SUPPORT

void onvif_parse_profile(XMLN * p_node)
{
	BOOL fixed = TRUE;
	XMLN * p_video_source;
	XMLN * p_video_encoder;
	XMLN * p_VideoSourceConfiguration;
	XMLN * p_VideoEncoderConfiguration;
#ifdef AUDIO_SUPPORT    
	XMLN * p_audio_source;
	XMLN * p_audio_encoder;
	XMLN * p_AudioSourceConfiguration;
	XMLN * p_AudioEncoderConfiguration;
	XMLN * p_AudioDecoderConfiguration;
#endif
	XMLN * p_stream_uri; 
	XMLN * p_Name;
	XMLN * p_MetadataConfiguration;
#ifdef VIDEO_ANALYTICS
	XMLN * p_VideoAnalyticsConfiguration;
#endif
#ifdef PTZ_SUPPORT
	XMLN * p_PTZConfiguration;
	XMLN * p_Presets;
	XMLN * p_PresetTours;
#endif
#ifdef DEVICEIO_SUPPORT
	XMLN * p_AudioOutputConfiguration;
#endif

	const char * p_token;
	const char * p_fixed;
	ONVIF_PROFILE * profile;

	p_token = xml_attr_get(p_node, "token");
	p_fixed = xml_attr_get(p_node, "fixed");
	if (p_fixed)
	{
		fixed = parse_Bool(p_fixed);
	}

	profile = onvif_add_profile(&g_onvif_cfg.profiles, fixed);
	if (NULL == profile)
	{
		return;
	}
	else if (p_token)
	{
		strncpy(profile->token, p_token, sizeof(profile->token)-1);
	}

	p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(profile->name, p_Name->data, sizeof(profile->name)-1);
	}
	
	p_video_source = xml_node_soap_get(p_node, "video_source");
	if (p_video_source)
	{
		profile->v_src_cfg = onvif_parse_video_source_cfg(p_video_source);
		if (profile->v_src_cfg)
		{
			profile->v_src_cfg->Configuration.UseCount++;
		}
	}

	p_video_encoder = xml_node_soap_get(p_node, "video_encoder");
	if (p_video_encoder)
	{
		profile->v_enc_cfg = onvif_parse_video_encoder_cfg(p_video_encoder);
		if (profile->v_enc_cfg)
		{
			profile->v_enc_cfg->Configuration.UseCount++;
		}
	}

	p_VideoSourceConfiguration = xml_node_soap_get(p_node, "VideoSourceConfiguration");
	if (p_VideoSourceConfiguration)
	{
		p_token = xml_attr_get(p_VideoSourceConfiguration, "token");
		if (p_token)
		{
			profile->v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_token);
			if (profile->v_src_cfg)
			{
				profile->v_src_cfg->Configuration.UseCount++;
			}
		}
	}

	p_VideoEncoderConfiguration = xml_node_soap_get(p_node, "VideoEncoderConfiguration");
	if (p_VideoEncoderConfiguration)
	{
		p_token = xml_attr_get(p_VideoEncoderConfiguration, "token");
		if (p_token)
		{
			profile->v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_token);
			if (profile->v_enc_cfg)
			{
				profile->v_enc_cfg->Configuration.UseCount++;
			}
		}
	}

#ifdef AUDIO_SUPPORT
	p_audio_source = xml_node_soap_get(p_node, "audio_source");
	if (p_audio_source)
	{
		if (NULL == g_onvif_cfg.a_src)
		{
			AudioSourceList * p_req = onvif_add_AudioSource(&g_onvif_cfg.a_src);
			if (p_req)
			{
				p_req->AudioSource.Channels = 2;
			}
		}

		profile->a_src_cfg = onvif_parse_audio_source_cfg(p_audio_source);
		if (profile->a_src_cfg)
		{
			profile->a_src_cfg->Configuration.UseCount++;
		}
	}
	
	p_audio_encoder = xml_node_soap_get(p_node, "audio_encoder");
	if (p_audio_encoder)
	{
		profile->a_enc_cfg = onvif_parse_audio_encoder_cfg(p_audio_encoder);
		if (profile->a_enc_cfg)
		{
			profile->a_enc_cfg->Configuration.UseCount++;
		}
	}

	p_AudioSourceConfiguration = xml_node_soap_get(p_node, "AudioSourceConfiguration");
	if (p_AudioSourceConfiguration)
	{
		p_token = xml_attr_get(p_AudioSourceConfiguration, "token");
		if (p_token)
		{
			profile->a_src_cfg = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_token);
			if (profile->a_src_cfg)
			{
				profile->a_src_cfg->Configuration.UseCount++;
			}
		}
	}

	p_AudioEncoderConfiguration = xml_node_soap_get(p_node, "AudioEncoderConfiguration");
	if (p_AudioEncoderConfiguration)
	{
		p_token = xml_attr_get(p_AudioEncoderConfiguration, "token");
		if (p_token)
		{
			profile->a_enc_cfg = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_token);
			if (profile->a_enc_cfg)
			{
				profile->a_enc_cfg->Configuration.UseCount++;
			}
		}
	}

	p_AudioDecoderConfiguration = xml_node_soap_get(p_node, "AudioDecoderConfiguration");
	if (p_AudioDecoderConfiguration)
	{
		p_token = xml_attr_get(p_AudioDecoderConfiguration, "token");
		if (p_token)
		{
			profile->a_dec_cfg = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_token);
			if (profile->a_dec_cfg)
			{
				profile->a_dec_cfg->Configuration.UseCount++;
			}
		}
	}
#endif

#ifdef VIDEO_ANALYTICS
	p_VideoAnalyticsConfiguration = xml_node_soap_get(p_node, "VideoAnalyticsConfiguration");
	if (p_VideoAnalyticsConfiguration)
	{
		p_token = xml_attr_get(p_VideoAnalyticsConfiguration, "token");
		if (p_token)
		{
			profile->va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_token);
			if (profile->va_cfg)
			{
				profile->va_cfg->Configuration.UseCount++;
			}
		}
	}
#endif

#ifdef PTZ_SUPPORT
	p_PTZConfiguration = xml_node_soap_get(p_node, "PTZConfiguration");
	if (p_PTZConfiguration)
	{
		p_token = xml_attr_get(p_PTZConfiguration, "token");
		if (p_token)
		{
			profile->ptz_cfg = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, p_token);
			if (profile->ptz_cfg)
			{
				profile->ptz_cfg->Configuration.UseCount++;
			}
		}
	}

	p_Presets = xml_node_soap_get(p_node, "Presets");
	while (p_Presets && soap_strcmp(p_Presets->name, "Presets") == 0)
	{
		PTZPresetList * p_req = onvif_add_PTZPreset(&profile->presets);
		if (p_req)
		{
			parse_PTZPreset(p_Presets, &p_req->PTZPreset);
		}
		p_Presets = p_Presets->next;
	}

	p_PresetTours = xml_node_soap_get(p_node, "PresetTours");
	while (p_PresetTours && soap_strcmp(p_PresetTours->name, "PresetTours") == 0)
	{
		PresetTourList * p_req = onvif_add_PresetTour(&profile->preset_tour);
		if (p_req)
		{
			parse_PresetTour(p_PresetTours, &p_req->PresetTour);
		}
		p_PresetTours = p_PresetTours->next;
	}
#endif

	p_MetadataConfiguration = xml_node_soap_get(p_node, "MetadataConfiguration");
	if (p_MetadataConfiguration)
	{
		p_token = xml_attr_get(p_MetadataConfiguration, "token");
		if (p_token)
		{
			profile->metadata_cfg = onvif_find_MetadataConfiguration(g_onvif_cfg.metadata_cfg, p_token);
			if (profile->metadata_cfg)
			{
				profile->metadata_cfg->Configuration.UseCount++;
			}
		}
	}

#ifdef DEVICEIO_SUPPORT
	p_AudioOutputConfiguration = xml_node_soap_get(p_node, "AudioOutputConfiguration");
	if (p_AudioOutputConfiguration)
	{
		p_token = xml_attr_get(p_AudioOutputConfiguration, "token");
		if (p_token)
		{
			profile->a_output_cfg = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_token);
			if (profile->a_output_cfg)
			{
				profile->a_output_cfg->Configuration.UseCount++;
			}
		}
	}
#endif

	p_stream_uri = xml_node_soap_get(p_node, "stream_uri");
	if (p_stream_uri && p_stream_uri->data && strlen(p_stream_uri->data) > 0)
	{
		const char * p_append_params = xml_attr_get(p_stream_uri, "append_params");
		if (p_append_params)
		{
			profile->append_params = atoi(p_append_params);
		}
		strncpy(profile->stream_uri, p_stream_uri->data, sizeof(profile->stream_uri)-1);
	}
	return;
}


void onvif_parse_event_cfg(XMLN * p_node)
{
	XMLN * p_renew_interval;
	XMLN * p_simulate_enable;

	p_renew_interval = xml_node_soap_get(p_node, "renew_interval");
	if (p_renew_interval && p_renew_interval->data)
	{
		g_onvif_cfg.evt_renew_time = atoi(p_renew_interval->data);
	}

	p_simulate_enable = xml_node_soap_get(p_node, "simulate_enable");
	if (p_simulate_enable && p_simulate_enable->data)
	{
		g_onvif_cfg.evt_sim_flag = atoi(p_simulate_enable->data);
	}
	return;
}

BOOL onvif_parse_hostname_information(XMLN * p_node, onvif_HostnameInformation * p_req)
{
	XMLN * p_FromDHCP;
	XMLN * p_RebootNeeded;

	p_FromDHCP = xml_node_soap_get(p_node, "FromDHCP");
	if (p_FromDHCP && p_FromDHCP->data)
	{
		p_req->FromDHCP = parse_Bool(p_FromDHCP->data);
	}

	p_RebootNeeded = xml_node_soap_get(p_node, "RebootNeeded");
	if (p_RebootNeeded && p_RebootNeeded->data)
	{
		p_req->RebootNeeded = parse_Bool(p_RebootNeeded->data);
	}

	return TRUE;
}

BOOL onvif_parse_network(XMLN * p_node, ONVIF_NET * p_req)
{
	XMLN * p_DNSInformation;
	XMLN * p_NTPInformation;
	XMLN * p_HostnameInformation;
	XMLN * p_NetworkGateway;
	XMLN * p_DiscoveryMode;
	XMLN * p_DynamicDNSInformation;
	XMLN * p_ZeroConfiguration;

	if (ONVIF_OK == parse_NetworkProtocols(p_node, &p_req->NetworkProtocol))
	{
		p_req->NetworkProtocolFlag = 1;
	}

	p_DNSInformation = xml_node_soap_get(p_node, "DNSInformation");
	if (p_DNSInformation)
	{
		if (ONVIF_OK == parse_DNSInformation(p_DNSInformation, &p_req->DNSInformation))
		{
			p_req->DNSInformationFlag = 1;
		}
	}

	p_NTPInformation = xml_node_soap_get(p_node, "NTPInformation");
	if (p_NTPInformation)
	{
		if (ONVIF_OK == parse_NTPInformation(p_NTPInformation, &p_req->NTPInformation))
		{
			p_req->NTPInformationFlag = 1;
		}
	}

	p_HostnameInformation = xml_node_soap_get(p_node, "HostnameInformation");
	if (p_HostnameInformation)
	{
		p_req->HostnameInformationFlag = onvif_parse_hostname_information(p_HostnameInformation, &p_req->HostnameInformation);
	}

	p_NetworkGateway = xml_node_soap_get(p_node, "NetworkGateway");
	if (p_NetworkGateway)
	{
		if (ONVIF_OK == parse_NetworkGateway(p_NetworkGateway, &p_req->NetworkGateway))
		{
			p_req->NetworkGatewayFlag = 1;
		}
	}

	p_DiscoveryMode = xml_node_soap_get(p_node, "DiscoveryMode");
	if (p_DiscoveryMode && p_DiscoveryMode->data)
	{
		p_req->DiscoveryModeFlag = 1;
		p_req->DiscoveryMode = onvif_StringToDiscoveryMode(p_DiscoveryMode->data);
	}

	p_DynamicDNSInformation = xml_node_soap_get(p_node, "DynamicDNSInformation");
	if (p_DynamicDNSInformation)
	{
		if (ONVIF_OK == parse_DynamicDNSInformation(p_DynamicDNSInformation, &p_req->DynamicDNSInformation))
		{
			p_req->DynamicDNSInformationFlag = 1;
		}
	}

	p_ZeroConfiguration = xml_node_soap_get(p_node, "ZeroConfiguration");
	if (p_ZeroConfiguration)
	{
		if (ONVIF_OK == parse_NetworkZeroConfiguration(p_ZeroConfiguration, &p_req->ZeroConfiguration))
		{
			p_req->ZeroConfigurationFlag = 1;
		}
	}

	return TRUE;
}

BOOL onvif_parse_hashing_algorithm(XMLN * p_node)
{
	const char * p_md5;
	const char * p_sha256;

	p_md5 = xml_attr_get(p_node, "md5");
	if (p_md5 && atoi(p_md5))
	{
		g_onvif_cfg.md5_hashing = 1;
	}
	else
	{
		g_onvif_cfg.md5_hashing = 0;
	}

	p_sha256 = xml_attr_get(p_node, "sha256");
	if (p_sha256 && atoi(p_sha256))
	{
		g_onvif_cfg.sha256_hashing = 1;
	}
	else
	{
		g_onvif_cfg.sha256_hashing = 0;
	}

	if (!g_onvif_cfg.md5_hashing && !g_onvif_cfg.sha256_hashing)
	{
		g_onvif_cfg.md5_hashing = 1;
	}

	return TRUE;
}

BOOL onvif_parse_scope(XMLN * p_node)
{
	if (p_node->data)
	{
		onvif_add_scope(p_node->data, FALSE);
	}
	else
	{
		BOOL fixed = FALSE;
		char scope[128] = {'\0'};
		XMLN * p_ScopeDef;
		XMLN * p_ScopeItem;
		
		p_ScopeDef = xml_node_soap_get(p_node, "ScopeDef");
		if (p_ScopeDef && p_ScopeDef->data)
		{
			if (strcasecmp(p_ScopeDef->data, "Fixed") == 0)
			{
				fixed = TRUE;
			}
		}

		p_ScopeItem = xml_node_soap_get(p_node, "ScopeItem");
		if (p_ScopeItem && p_ScopeItem->data)
		{
			strncpy(scope, p_ScopeItem->data, sizeof(scope)-1);
		}

		if (scope[0] != '0')
		{
			onvif_add_scope(scope, fixed);
		}
	}

	return TRUE;
}

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

BOOL onvif_parse_video_source(XMLN * p_node, VideoSourceList * p_req)
{
	XMLN * p_VideoSourceModes;
#ifdef IMAGE_SUPPORT
	XMLN * p_ImagingSettings;
	XMLN * p_ImagingOptions;
	XMLN * p_CurrentPrestToken;
#endif    
#ifdef THERMAL_SUPPORT    
	XMLN * p_ThermalSupport;
	XMLN * p_ThermalConfiguration;
	XMLN * p_RadiometryConfiguration;
#endif

	parse_VideoSource(p_node, &p_req->VideoSource);

	p_VideoSourceModes = xml_node_soap_get(p_node, "VideoSourceModes");
	if (p_VideoSourceModes)
	{
		parse_VideoSourceMode(p_VideoSourceModes, &p_req->VideoSourceMode);
	}

#ifdef IMAGE_SUPPORT
	p_ImagingSettings = xml_node_soap_get(p_node, "ImagingSettings");
	if (p_ImagingSettings)
	{
		parse_ImagingSettings(p_ImagingSettings, &p_req->ImagingSettings);
	}

	p_ImagingOptions = xml_node_soap_get(p_node, "ImagingOptions");
	if (p_ImagingOptions)
	{
		parse_ImagingOptions(p_ImagingOptions, &p_req->ImagingOptions);
	}
	
	p_CurrentPrestToken = xml_node_soap_get(p_node, "CurrentPrestToken");
	if (p_CurrentPrestToken && p_CurrentPrestToken->data)
	{
		strncpy(p_req->CurrentPresetToken, p_CurrentPrestToken->data, sizeof(p_req->CurrentPresetToken)-1);
	}
#endif

#ifdef THERMAL_SUPPORT
	p_ThermalSupport = xml_node_soap_get(p_node, "ThermalSupport");
	if (p_ThermalSupport && p_ThermalSupport->data)
	{
		p_req->ThermalSupport = parse_Bool(p_ThermalSupport->data);
	}

	p_ThermalConfiguration = xml_node_soap_get(p_node, "ThermalConfiguration");
	if (p_ThermalConfiguration)
	{
		parse_ThermalConfiguration(p_ThermalConfiguration, &p_req->ThermalConfiguration);
	}

	p_RadiometryConfiguration = xml_node_soap_get(p_node, "RadiometryConfiguration");
	if (p_RadiometryConfiguration)
	{
		parse_RadiometryConfiguration(p_RadiometryConfiguration, &p_req->RadiometryConfiguration);
	}
#endif

	return TRUE;
}

BOOL onvif_parse_video_encoder_configuration(XMLN * p_node, onvif_VideoEncoder2Configuration * p_req)
{
	XMLN * p_EncodingInterval;
	XMLN * p_SessionTimeout;

	parse_VideoEncoder2Configuration(p_node, p_req);

	p_req->UseCount = 0;
	p_req->VideoEncoding = onvif_StringToVideoEncoding(p_req->Encoding);

	if (strcasecmp(p_req->Encoding, "MP4V-ES") == 0)
	{
		p_req->VideoEncoding = VideoEncoding_MPEG4;
	}

	p_EncodingInterval = xml_node_soap_get(p_node, "EncodingInterval");
	if (p_EncodingInterval && p_EncodingInterval->data)
	{
		p_req->RateControl.EncodingInterval = atoi(p_EncodingInterval->data);
	}

	p_SessionTimeout = xml_node_soap_get(p_node, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
	}

	return TRUE;
}

#ifdef AUDIO_SUPPORT

BOOL onvif_parse_audio_encoder_configuration(XMLN * p_node, onvif_AudioEncoder2Configuration * p_req)
{
	XMLN * p_SessionTimeout;

	parse_AudioEncoder2Configuration(p_node, p_req);

	p_req->UseCount = 0;
	p_req->AudioEncoding = onvif_StringToAudioEncoding(p_req->Encoding);

	if (strcasecmp(p_req->Encoding, "G711"))
	{
		p_req->AudioEncoding = AudioEncoding_G711;
	}
	else if (strcasecmp(p_req->Encoding, "G711A"))
	{
		p_req->AudioEncoding = AudioEncoding_G711A;
	}
	else if (strcasecmp(p_req->Encoding, "AAC"))
	{
		p_req->AudioEncoding = AudioEncoding_AAC;
	}

	p_SessionTimeout = xml_node_soap_get(p_node, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		parse_XSDDuration(p_SessionTimeout->data, &p_req->SessionTimeout);
	}

	return TRUE;
}

BOOL onvif_parse_audio_decoder_configuration(XMLN * p_node, AudioDecoderConfigurationList * p_req)
{
#ifdef MEDIA_SUPPORT
	XMLN * p_Options;
#endif
#ifdef MEDIA2_SUPPORT
	XMLN * p_Options2;
#endif

	parse_AudioDecoderConfiguration(p_node, &p_req->Configuration);

	p_req->Configuration.UseCount = 0;

#ifdef MEDIA_SUPPORT
	p_Options = xml_node_soap_get(p_node, "Options");
	if (p_Options)
	{
		parse_AudioDecoderConfigurationOptions(p_Options, &p_req->Options);
	}
#endif

#ifdef MEDIA2_SUPPORT
	p_Options2 = xml_node_soap_get(p_node, "Options2");
	while (p_Options2 && soap_strcmp(p_Options2->name, "Options2") == 0)
	{
		AudioEncoder2ConfigurationOptionsList * p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_req->Options2);
		if (p_option)
		{
			parse_AudioEncoder2ConfigurationOptions(p_Options2, &p_option->Options);
		}

		p_Options2 = p_Options2->next;
	}
#endif

	return TRUE;
}

#endif // AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT

BOOL onvif_parse_audio_output_configuration(XMLN * p_node, AudioOutputConfigurationList * p_req)
{
	XMLN * p_Options;

	parse_AudioOutputConfiguration(p_node, &p_req->Configuration);

	p_req->Configuration.UseCount = 0;

	p_Options = xml_node_soap_get(p_node, "Options");
	if (p_Options)
	{
		parse_AudioOutputConfigurationOptions(p_Options, &p_req->Options);
	}

	return TRUE;
}

#endif // DEVICEIO_SUPPORT

#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef DEVICEIO_SUPPORT

BOOL onvif_parse_relay_output(XMLN * p_node, RelayOutputList * p_req)
{
	XMLN * p_Options;

	parse_RelayOutput(p_node, &p_req->RelayOutput);

	p_Options = xml_node_soap_get(p_node, "Options");
	if (p_Options)
	{
		parse_RelayOutputOptions(p_Options, &p_req->Options);
	}

	return TRUE;
}

BOOL onvif_parse_digital_input(XMLN * p_node, DigitalInputList * p_req)
{
	XMLN * p_Options;

	parse_DigitalInput(p_node, &p_req->DigitalInput);

	p_Options = xml_node_soap_get(p_node, "Options");
	if (p_Options)
	{
		parse_DigitalInputConfigurationOptions(p_Options, &p_req->Options);
	}

	return TRUE;
}

BOOL onvif_parse_serial_port(XMLN * p_node, SerialPortList * p_req)
{
	XMLN * p_Configuration;
	XMLN * p_Options;
	const char * p_token;

	p_token = xml_attr_get(p_node, "token");
	if (p_token)
	{
		strncpy(p_req->SerialPort.token, p_token, sizeof(p_req->SerialPort.token)-1);
	}

	p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		parse_SerialPortConfiguration(p_Configuration, &p_req->Configuration);
	}

	p_Options = xml_node_soap_get(p_node, "Options");
	if (p_Options)
	{
		parse_SerialPortConfigurationOptions(p_Options, &p_req->Options);
	}

	return TRUE;
}

#endif // DEVICEIO_SUPPORT

#ifdef PROFILE_G_SUPPORT

BOOL onvif_parse_recording(XMLN * p_node, RecordingList * p_req)
{
	XMLN * p_EarliestRecording;
	XMLN * p_LatestRecording;
	XMLN * p_RecordingStatus;
	TrackList * p_track;

	parse_Recording(p_node, &p_req->Recording);

	p_EarliestRecording = xml_node_soap_get(p_node, "EarliestRecording");
	if (p_EarliestRecording && p_EarliestRecording->data)
	{
		parse_XSDDatetime(p_EarliestRecording->data, &p_req->EarliestRecording);
	}

	p_LatestRecording = xml_node_soap_get(p_node, "LatestRecording");
	if (p_LatestRecording && p_LatestRecording->data)
	{
		parse_XSDDatetime(p_LatestRecording->data, &p_req->LatestRecording);
	}

	p_RecordingStatus = xml_node_soap_get(p_node, "RecordingStatus");
	if (p_RecordingStatus && p_RecordingStatus->data)
	{
		p_req->RecordingStatus = onvif_StringToRecordingStatus(p_RecordingStatus->data);
	}

	p_track = p_req->Recording.Tracks;
	while (p_track)
	{
		p_track->EarliestRecording = p_req->EarliestRecording;
		p_track->LatestRecording = p_req->LatestRecording;

		p_track = p_track->next;
	}

	return TRUE;
}

#endif // PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

BOOL onvif_parse_access_point(XMLN * p_node, AccessPointList * p_req)
{
	XMLN * p_AuthenticationProfileToken;
	XMLN * p_Enabled;

	parse_AccessPointInfo(p_node, &p_req->AccessPointInfo);

	p_AuthenticationProfileToken = xml_node_soap_get(p_node, "AuthenticationProfileToken");
	if (p_AuthenticationProfileToken && p_AuthenticationProfileToken->data)
	{
		strncpy(p_req->AuthenticationProfileToken, p_AuthenticationProfileToken->data, sizeof(p_req->AuthenticationProfileToken)-1);
	}

	p_Enabled = xml_node_soap_get(p_node, "Enabled");
	if (p_Enabled && p_Enabled->data)
	{
		p_req->Enabled = parse_Bool(p_Enabled->data);
	}

	return TRUE;
}

BOOL onvif_parse_door(XMLN * p_node, DoorList * p_req)
{
	XMLN * p_DoorState;

	parse_Door(p_node, &p_req->Door);

	p_DoorState = xml_node_soap_get(p_node, "DoorState");
	if (p_DoorState)
	{
		parse_DoorState(p_DoorState, &p_req->DoorState);
	}

	return TRUE;
}

#endif // PROFILE_C_SUPPORT

#ifdef CREDENTIAL_SUPPORT

BOOL onvif_parse_credential(XMLN * p_node, CredentialList * p_req)
{
	XMLN * p_State;

	parse_Credential(p_node, &p_req->Credential);

	p_State = xml_node_soap_get(p_node, "State");
	if (p_State)
	{
		parse_CredentialState(p_State, &p_req->State);
	}

	return TRUE;
}

#endif // CREDENTIAL_SUPPORT

void onvif_parse_cfg(char * xml_buff, int rlen)
{
	XMLN * p_node;
	XMLN * p_server_ip;
	XMLN * p_server_port;
	XMLN * p_http_enable;
	XMLN * p_http_port;
	
#ifdef HTTPS
	XMLN * p_https_enable;
	XMLN * p_https_port;
	XMLN * p_cert_file;
	XMLN * p_key_file;
#endif

	XMLN * p_http_max_users;
	XMLN * p_need_auth;
	XMLN * p_log_enable;
	XMLN * p_log_level;
	XMLN * p_EndpointReference;
	
#ifdef PROFILE_Q_SUPPORT
	XMLN * p_device_state;
#endif

	XMLN * p_information;
	XMLN * p_user;
	XMLN * p_RemoteUser;
	XMLN * p_profile;
	XMLN * p_scope;
	XMLN * p_event;
	XMLN * p_SystemDateTime;
	XMLN * p_network;
	XMLN * p_HashingAlgorithm;
	
#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)
	XMLN * p_VideoSources;
	XMLN * p_VideoSourceConfigurations;
	XMLN * p_VideoEncoderConfigurations;
#ifdef AUDIO_SUPPORT
	XMLN * p_AudioSources;
	XMLN * p_AudioSourceConfigurations;
	XMLN * p_AudioEncoderConfigurations;
	XMLN * p_AudioDecoderConfigurations;
#endif

#ifdef VIDEO_ANALYTICS
	XMLN * p_VideoAnalyticsConfigurations;
#endif

#ifdef DEVICEIO_SUPPORT
	XMLN * p_AudioOutputConfigurations;
#endif

	XMLN * p_OSDConfigurations;
	XMLN * p_MetadataConfigurations;
#ifdef MEDIA2_SUPPORT
	XMLN * p_Masks;
#endif
#endif

#ifdef PTZ_SUPPORT
	XMLN * p_PTZNodes;
	XMLN * p_PTZConfigurations;
#endif

#ifdef DEVICEIO_SUPPORT
	XMLN * p_VideoOutputs;
	XMLN * p_VideoOutputConfigurations;
	XMLN * p_AudioOutputs;
	XMLN * p_RelayOutputs;
	XMLN * p_DigitalInputs;
	XMLN * p_SerialPorts;
#endif

#ifdef IPFILTER_SUPPORT
	XMLN * p_IPAddressFilter;
#endif

#ifdef PROFILE_G_SUPPORT
	XMLN * p_Recordings;
	XMLN * p_RecordingJobs;
	XMLN * p_replay_session_timeout;
#endif

#ifdef PROFILE_C_SUPPORT
	XMLN * p_AccessPoints;
	XMLN * p_Doors;
	XMLN * p_Areas;
#endif

#ifdef CREDENTIAL_SUPPORT
	XMLN * p_Credentials;
	XMLN * p_CredentialWhiltlists;
	XMLN * p_CredentialBlacklists;
#endif

#ifdef ACCESS_RULES
	XMLN * p_AccessProfiles;
#endif

#ifdef SCHEDULE_SUPPORT
	XMLN * p_Schedules;
	XMLN * p_SpecialDayGroups;
#endif

#ifdef RECEIVER_SUPPORT
	XMLN * p_Receivers;
#endif

	p_node = xxx_hxml_parse(xml_buff, rlen);
	if (NULL == p_node)
	{
		return;
	}

	p_server_ip = xml_node_soap_get(p_node, "server_ip");
	if (p_server_ip && p_server_ip->data)
	{
		strncpy(g_onvif_cfg.server_ip, p_server_ip->data, sizeof(g_onvif_cfg.server_ip)-1);
	}

	// Compatible with older configuration file	
	p_server_port = xml_node_soap_get(p_node, "server_port");
	if (p_server_port && p_server_port->data)
	{
		g_onvif_cfg.http_port = atoi(p_server_port->data);
	}

	p_http_enable = xml_node_soap_get(p_node, "http_enable");
	if (p_http_enable && p_http_enable->data)
	{
		g_onvif_cfg.http_enable = atoi(p_http_enable->data);
	}
	
	p_http_port = xml_node_soap_get(p_node, "http_port");
	if (p_http_port && p_http_port->data)
	{
		g_onvif_cfg.http_port = atoi(p_http_port->data);
	}

#ifdef HTTPS
	p_https_enable = xml_node_soap_get(p_node, "https_enable");
	if (p_https_enable && p_https_enable->data)
	{
		g_onvif_cfg.https_enable = atoi(p_https_enable->data);
	}

	p_https_port = xml_node_soap_get(p_node, "https_port");
	if (p_https_port && p_https_port->data)
	{
		g_onvif_cfg.https_port = atoi(p_https_port->data);
	}
	
	p_cert_file = xml_node_soap_get(p_node, "cert_file");
	if (p_cert_file && p_cert_file->data)
	{
		strncpy(g_onvif_cfg.cert_file, p_cert_file->data, sizeof(g_onvif_cfg.cert_file)-1);
	}

	p_key_file = xml_node_soap_get(p_node, "key_file");
	if (p_key_file && p_key_file->data)
	{
		strncpy(g_onvif_cfg.key_file, p_key_file->data, sizeof(g_onvif_cfg.key_file)-1);
	}
#endif

	p_http_max_users = xml_node_soap_get(p_node, "http_max_users");
	if (p_http_max_users && p_http_max_users->data)
	{
		g_onvif_cfg.http_max_users = atoi(p_http_max_users->data);
	}
	
	p_need_auth = xml_node_soap_get(p_node, "need_auth");
	if (p_need_auth && p_need_auth->data)
	{
		g_onvif_cfg.need_auth = atoi(p_need_auth->data);
	}

	p_log_enable = xml_node_soap_get(p_node, "log_enable");
	if (p_log_enable && p_log_enable->data)
	{
		g_onvif_cfg.log_enable = atoi(p_log_enable->data);
	}

	p_log_level = xml_node_soap_get(p_node, "log_level");
	if (p_log_level && p_log_level->data)
	{
		g_onvif_cfg.log_level = atoi(p_log_level->data);
	}

	p_EndpointReference = xml_node_soap_get(p_node, "EndpointReference");
	if (p_EndpointReference && p_EndpointReference->data)
	{
		strncpy(g_onvif_cfg.EndpointReference, p_EndpointReference->data, sizeof(g_onvif_cfg.EndpointReference)-1);
	}

#ifdef PROFILE_Q_SUPPORT
	p_device_state = xml_node_soap_get(p_node, "device_state");
	if (p_device_state && p_device_state->data)
	{
		g_onvif_cfg.device_state = atoi(p_device_state->data);
	}
#endif
	
	p_information = xml_node_soap_get(p_node, "information");
	if (p_information)
	{
		g_onvif_cfg.DeviceInformationFlag = onvif_parse_device_information(p_information, &g_onvif_cfg.DeviceInformation);
	}

	p_user = xml_node_soap_get(p_node, "user");
	while (p_user && strcmp(p_user->name, "user") == 0)
	{
		onvif_User user;
		memset(&user, 0, sizeof(user));
		
		if (onvif_parse_user(p_user, &user))
		{
			onvif_add_user(&user);
			g_onvif_cfg.UsersFlag = 1;
		}

		p_user = p_user->next;
	}

	p_RemoteUser = xml_node_soap_get(p_node, "RemoteUser");
	if (p_RemoteUser)
	{
		onvif_parse_remote_user(p_RemoteUser, &g_onvif_cfg.RemoteUser);
	}

	p_SystemDateTime = xml_node_soap_get(p_node, "SystemDateTime");
	if (p_SystemDateTime)
	{
		g_onvif_cfg.SystemDateTimeFlag = parse_SystemDateTime(p_SystemDateTime, &g_onvif_cfg.SystemDateTime);
	}

	p_network = xml_node_soap_get(p_node, "network");
	if (p_network)
	{
	    onvif_parse_network(p_network, &g_onvif_cfg.network);
	}

	p_HashingAlgorithm = xml_node_soap_get(p_node, "HashingAlgorithm");
	if (p_HashingAlgorithm)
	{
	    onvif_parse_hashing_algorithm(p_HashingAlgorithm);
	}

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

	p_VideoSources = xml_node_soap_get(p_node, "VideoSources");
	while (p_VideoSources && soap_strcmp(p_VideoSources->name, "VideoSources") == 0)
	{
		VideoSourceList * p_req = onvif_add_VideoSource(&g_onvif_cfg.v_src, 1280, 720, 0, 0, 0, 0);
		if (p_req)
		{
			onvif_parse_video_source(p_VideoSources, p_req);
		}

		p_VideoSources = p_VideoSources->next;
	}

	p_VideoSourceConfigurations = xml_node_soap_get(p_node, "VideoSourceConfigurations");
	while (p_VideoSourceConfigurations && soap_strcmp(p_VideoSourceConfigurations->name, "VideoSourceConfigurations") == 0)
	{
		VideoSourceConfigurationList * p_req = onvif_add_VideoSourceConfiguration(&g_onvif_cfg.v_src_cfg, 1280, 720);
		if (p_req)
		{
			parse_VideoSourceConfiguration(p_VideoSourceConfigurations, &p_req->Configuration);
			p_req->Configuration.UseCount = 0;
			p_req->Options.sizeVideoSourceTokensAvailable = 1;
			strcpy(p_req->Options.VideoSourceTokensAvailable[0], p_req->Configuration.SourceToken);
		}
		p_VideoSourceConfigurations = p_VideoSourceConfigurations->next;
	}

	p_VideoEncoderConfigurations = xml_node_soap_get(p_node, "VideoEncoderConfigurations");
	while (p_VideoEncoderConfigurations && soap_strcmp(p_VideoEncoderConfigurations->name, "VideoEncoderConfigurations") == 0)
	{
		VideoEncoder2ConfigurationList * p_req = onvif_add_VideoEncoder2Configuration(&g_onvif_cfg.v_enc_cfg, NULL);
		if (p_req)
		{
			onvif_parse_video_encoder_configuration(p_VideoEncoderConfigurations, &p_req->Configuration);
		}
		p_VideoEncoderConfigurations = p_VideoEncoderConfigurations->next;
	}

#ifdef AUDIO_SUPPORT

	p_AudioSources = xml_node_soap_get(p_node, "AudioSources");
	while (p_AudioSources && soap_strcmp(p_AudioSources->name, "AudioSources") == 0)
	{
		AudioSourceList * p_req = onvif_add_AudioSource(&g_onvif_cfg.a_src);
		if (p_req)
		{
			onvif_parse_audio_source(p_AudioSources, &p_req->AudioSource);
		}
		p_AudioSources = p_AudioSources->next;
	}

	p_AudioSourceConfigurations = xml_node_soap_get(p_node, "AudioSourceConfigurations");
	while (p_AudioSourceConfigurations && soap_strcmp(p_AudioSourceConfigurations->name, "AudioSourceConfigurations") == 0)
	{
		AudioSourceConfigurationList * p_req = onvif_add_AudioSourceConfiguration(&g_onvif_cfg.a_src_cfg);
		if (p_req)
		{
			parse_AudioSourceConfiguration(p_AudioSourceConfigurations, &p_req->Configuration);
			p_req->Configuration.UseCount = 0;
		}
		p_AudioSourceConfigurations = p_AudioSourceConfigurations->next;
	}

	p_AudioEncoderConfigurations = xml_node_soap_get(p_node, "AudioEncoderConfigurations");
	while (p_AudioEncoderConfigurations && soap_strcmp(p_AudioEncoderConfigurations->name, "AudioEncoderConfigurations") == 0)
	{
		AudioEncoder2ConfigurationList * p_req = onvif_add_AudioEncoder2Configuration(&g_onvif_cfg.a_enc_cfg, NULL);
		if (p_req)
		{
			onvif_parse_audio_encoder_configuration(p_AudioEncoderConfigurations, &p_req->Configuration);
		}
		p_AudioEncoderConfigurations = p_AudioEncoderConfigurations->next;
	}

	p_AudioDecoderConfigurations = xml_node_soap_get(p_node, "AudioDecoderConfigurations");
	while (p_AudioDecoderConfigurations && soap_strcmp(p_AudioDecoderConfigurations->name, "AudioDecoderConfigurations") == 0)
	{
		AudioDecoderConfigurationList * p_req = onvif_add_AudioDecoderConfiguration(&g_onvif_cfg.a_dec_cfg);
		if (p_req)
		{
			onvif_parse_audio_decoder_configuration(p_AudioDecoderConfigurations, p_req);
		}
		p_AudioDecoderConfigurations = p_AudioDecoderConfigurations->next;
	}
#endif

	p_OSDConfigurations = xml_node_soap_get(p_node, "OSDConfigurations");
	while (p_OSDConfigurations && soap_strcmp(p_OSDConfigurations->name, "OSDConfigurations") == 0)
	{
		OSDConfigurationList * p_req = onvif_add_OSDConfiguration(&g_onvif_cfg.OSDs);
		if (p_req)
		{
			parse_OSDConfiguration(p_OSDConfigurations, &p_req->OSD);
		}
		p_OSDConfigurations = p_OSDConfigurations->next;
	}

	p_MetadataConfigurations = xml_node_soap_get(p_node, "MetadataConfigurations");
	while (p_MetadataConfigurations && soap_strcmp(p_MetadataConfigurations->name, "MetadataConfigurations") == 0)
	{
		MetadataConfigurationList * p_req = onvif_add_MetadataConfiguration(&g_onvif_cfg.metadata_cfg);
		if (p_req)
		{
			parse_MetadataConfiguration(p_MetadataConfigurations, &p_req->Configuration);
			p_req->Configuration.UseCount = 0;
		}
		p_MetadataConfigurations = p_MetadataConfigurations->next;
	}

#ifdef MEDIA2_SUPPORT

	p_Masks = xml_node_soap_get(p_node, "Masks");
	while (p_Masks && soap_strcmp(p_Masks->name, "Masks") == 0)
	{
		MaskList * p_req = onvif_add_Mask(&g_onvif_cfg.mask, -1);
		if (p_req)
		{
			parse_Mask(p_Masks, &p_req->Mask);
		}
		p_Masks = p_Masks->next;
	}
	
#endif // MEDIA2_SUPPORT

#ifdef VIDEO_ANALYTICS

	p_VideoAnalyticsConfigurations = xml_node_soap_get(p_node, "VideoAnalyticsConfigurations");
	while (p_VideoAnalyticsConfigurations && soap_strcmp(p_VideoAnalyticsConfigurations->name, "VideoAnalyticsConfigurations") == 0)
	{
		VideoAnalyticsConfigurationList * p_req = onvif_add_VideoAnalyticsConfiguration(&g_onvif_cfg.va_cfg);
		if (p_req)
		{
			parse_VideoAnalyticsConfiguration(p_VideoAnalyticsConfigurations, &p_req->Configuration);
			p_req->Configuration.UseCount = 0;
		}
		p_VideoAnalyticsConfigurations = p_VideoAnalyticsConfigurations->next;
	}
	
#endif

#ifdef DEVICEIO_SUPPORT

	p_AudioOutputConfigurations = xml_node_soap_get(p_node, "AudioOutputConfigurations");
	while (p_AudioOutputConfigurations && soap_strcmp(p_AudioOutputConfigurations->name, "AudioOutputConfigurations") == 0)
	{
		AudioOutputConfigurationList * p_req = onvif_add_AudioOutputConfiguration(&g_onvif_cfg.a_output_cfg);
		if (p_req)
		{
			onvif_parse_audio_output_configuration(p_AudioOutputConfigurations, p_req);
		}
		p_AudioOutputConfigurations = p_AudioOutputConfigurations->next;
	}
	
#endif // DEVICEIO_SUPPORT

#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef PTZ_SUPPORT

	p_PTZNodes = xml_node_soap_get(p_node, "PTZNodes");
	while (p_PTZNodes && soap_strcmp(p_PTZNodes->name, "PTZNodes") == 0)
	{
		PTZNodeList * p_req = onvif_add_PTZNode(&g_onvif_cfg.ptz_node);
		if (p_req)
		{
			parse_PTZNode(p_PTZNodes, &p_req->PTZNode);
		}
		p_PTZNodes = p_PTZNodes->next;
	}

	p_PTZConfigurations = xml_node_soap_get(p_node, "PTZConfigurations");
	while (p_PTZConfigurations && soap_strcmp(p_PTZConfigurations->name, "PTZConfigurations") == 0)
	{
		PTZConfigurationList * p_req = onvif_add_PTZConfiguration(&g_onvif_cfg.ptz_cfg);
		if (p_req)
		{
			parse_PTZConfiguration(p_PTZConfigurations, &p_req->Configuration);
			p_req->Configuration.UseCount = 0;
		}
		p_PTZConfigurations = p_PTZConfigurations->next;
	}
	
#endif // PTZ_SUPPORT

#ifdef DEVICEIO_SUPPORT

	p_VideoOutputs = xml_node_soap_get(p_node, "VideoOutputs");
	while (p_VideoOutputs && soap_strcmp(p_VideoOutputs->name, "VideoOutputs") == 0)
	{
		VideoOutputList * p_req = onvif_add_VideoOutput(&g_onvif_cfg.v_output);
		if (p_req)
		{
			parse_VideoOutput(p_VideoOutputs, &p_req->VideoOutput);
		}
		p_VideoOutputs = p_VideoOutputs->next;
	}

	p_VideoOutputConfigurations = xml_node_soap_get(p_node, "VideoOutputConfigurations");
	while (p_VideoOutputConfigurations && soap_strcmp(p_VideoOutputConfigurations->name, "VideoOutputConfigurations") == 0)
	{
		VideoOutputConfigurationList * p_req = onvif_add_VideoOutputConfiguration(&g_onvif_cfg.v_output_cfg);
		if (p_req)
		{
			parse_VideoOutputConfiguration(p_VideoOutputConfigurations, &p_req->Configuration);
			p_req->Configuration.UseCount = 0;
		}
		p_VideoOutputConfigurations = p_VideoOutputConfigurations->next;
	}

	p_AudioOutputs = xml_node_soap_get(p_node, "AudioOutputs");
	while (p_AudioOutputs && soap_strcmp(p_AudioOutputs->name, "AudioOutputs") == 0)
	{
		AudioOutputList * p_req = onvif_add_AudioOutput(&g_onvif_cfg.a_output);
		if (p_req)
		{
			parse_AudioOutput(p_AudioOutputs, &p_req->AudioOutput);
		}
		p_AudioOutputs = p_AudioOutputs->next;
	}

	p_RelayOutputs = xml_node_soap_get(p_node, "RelayOutputs");
	while (p_RelayOutputs && soap_strcmp(p_RelayOutputs->name, "RelayOutputs") == 0)
	{
		RelayOutputList * p_req = onvif_add_RelayOutput(&g_onvif_cfg.relay_output);
		if (p_req)
		{
			onvif_parse_relay_output(p_RelayOutputs, p_req);
		}
		p_RelayOutputs = p_RelayOutputs->next;
	}

	p_DigitalInputs = xml_node_soap_get(p_node, "DigitalInputs");
	while (p_DigitalInputs && soap_strcmp(p_DigitalInputs->name, "DigitalInputs") == 0)
	{
		DigitalInputList * p_req = onvif_add_DigitalInput(&g_onvif_cfg.digit_input);
		if (p_req)
		{
			onvif_parse_digital_input(p_DigitalInputs, p_req);
		}
		p_DigitalInputs = p_DigitalInputs->next;
	}

	p_SerialPorts = xml_node_soap_get(p_node, "SerialPorts");
	while (p_SerialPorts && soap_strcmp(p_SerialPorts->name, "SerialPorts") == 0)
	{
		SerialPortList * p_req = onvif_add_SerialPort(&g_onvif_cfg.serial_port);
		if (p_req)
		{
			onvif_parse_serial_port(p_SerialPorts, p_req);
		}
		p_SerialPorts = p_SerialPorts->next;
	}
	
#endif // DEVICEIO_SUPPORT

	p_profile = xml_node_soap_get(p_node, "profile");
	while (p_profile && strcmp(p_profile->name, "profile") == 0)
	{
		onvif_parse_profile(p_profile);
		p_profile = p_profile->next;
	}

#ifdef PROFILE_G_SUPPORT
	p_Recordings = xml_node_soap_get(p_node, "Recordings");
	while (p_Recordings && strcmp(p_Recordings->name, "Recordings") == 0)
	{
		RecordingList * p_req = onvif_add_Recording(&g_onvif_cfg.recordings);
		if (p_req)
		{
			p_req->Recording.RecordingToken[0] = '\0';
			onvif_parse_recording(p_Recordings, p_req);
		}
		p_Recordings = p_Recordings->next;
	}
	p_RecordingJobs = xml_node_soap_get(p_node, "RecordingJobs");
	while (p_RecordingJobs && strcmp(p_RecordingJobs->name, "RecordingJobs") == 0)
	{
		RecordingJobList * p_req = onvif_add_RecordingJob(&g_onvif_cfg.recording_jobs);
		if (p_req)
		{
			parse_RecordingJob(p_RecordingJobs, &p_req->RecordingJob);
		}
		p_RecordingJobs = p_RecordingJobs->next;
	}
	p_replay_session_timeout = xml_node_soap_get(p_node, "replay_session_timeout");
	if (p_replay_session_timeout && p_replay_session_timeout->data)
	{
		g_onvif_cfg.replay_session_timeout = atoi(p_replay_session_timeout->data);
	}
#endif // PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT
	p_Doors = xml_node_soap_get(p_node, "Doors");
	while (p_Doors && strcmp(p_Doors->name, "Doors") == 0)
	{
		DoorList * p_req = onvif_add_Door(&g_onvif_cfg.doors);
		if (p_req)
		{
			onvif_parse_door(p_Doors, p_req);
		}
		p_Doors = p_Doors->next;
	}
	p_Areas = xml_node_soap_get(p_node, "Areas");
	while (p_Areas && strcmp(p_Areas->name, "Areas") == 0)
	{
		AreaList * p_req = onvif_add_Area(&g_onvif_cfg.areas);
		if (p_req)
		{
			parse_AreaInfo(p_Areas, &p_req->AreaInfo);
		}
		p_Areas = p_Areas->next;
	}
	p_AccessPoints = xml_node_soap_get(p_node, "AccessPoints");
	while (p_AccessPoints && strcmp(p_AccessPoints->name, "AccessPoints") == 0)
	{
		AccessPointList * p_req = onvif_add_AccessPoint(&g_onvif_cfg.access_points);
		if (p_req)
		{
			onvif_parse_access_point(p_AccessPoints, p_req);
		}
		p_AccessPoints = p_AccessPoints->next;
	}
#endif // PROFILE_C_SUPPORT

#ifdef CREDENTIAL_SUPPORT
	p_Credentials = xml_node_soap_get(p_node, "Credentials");
	while (p_Credentials && strcmp(p_Credentials->name, "Credentials") == 0)
	{
		CredentialList * p_req = onvif_add_Credential(&g_onvif_cfg.credential);
		if (p_req)
		{
			onvif_parse_credential(p_Credentials, p_req);
		}
		p_Credentials = p_Credentials->next;
	}
	p_CredentialWhiltlists = xml_node_soap_get(p_node, "CredentialWhiltlists");
	if (p_CredentialWhiltlists)
	{
		parse_CredentialIdentifierItemList(p_CredentialWhiltlists, &g_onvif_cfg.whiltlist);
	}

	p_CredentialBlacklists = xml_node_soap_get(p_node, "CredentialBlacklists");
	if (p_CredentialBlacklists)
	{
		parse_CredentialIdentifierItemList(p_CredentialWhiltlists, &g_onvif_cfg.blacklist);
	}
#endif // CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
	p_AccessProfiles = xml_node_soap_get(p_node, "AccessProfiles");
	while (p_AccessProfiles && strcmp(p_AccessProfiles->name, "AccessProfiles") == 0)
	{
		AccessProfileList * p_req = onvif_add_AccessProfile(&g_onvif_cfg.access_rules);
		if (p_req)
		{
			parse_AccessProfile(p_AccessProfiles, &p_req->AccessProfile);
		}
		p_AccessProfiles = p_AccessProfiles->next;
	}
#endif // ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
	p_Schedules = xml_node_soap_get(p_node, "Schedules");
	while (p_Schedules && strcmp(p_Schedules->name, "Schedules") == 0)
	{
		ScheduleList * p_req = onvif_add_Schedule(&g_onvif_cfg.schedule);
		if (p_req)
		{
			parse_Schedule(p_Schedules, &p_req->Schedule);
		}
		p_Schedules = p_Schedules->next;
	}
	p_SpecialDayGroups = xml_node_soap_get(p_node, "SpecialDayGroups");
	while (p_SpecialDayGroups && strcmp(p_SpecialDayGroups->name, "SpecialDayGroups") == 0)
	{
		SpecialDayGroupList * p_req = onvif_add_SpecialDayGroup(&g_onvif_cfg.specialdaygroup);
		if (p_req)
		{
			parse_SpecialDayGroup(p_SpecialDayGroups, &p_req->SpecialDayGroup);
		}
		p_SpecialDayGroups = p_SpecialDayGroups->next;
	}
#endif // SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT
	p_Receivers = xml_node_soap_get(p_node, "Receivers");
	while (p_Receivers && strcmp(p_Receivers->name, "Receivers") == 0)
	{
		ReceiverList * p_req = onvif_add_Receiver(&g_onvif_cfg.receiver);
		if (p_req)
		{
			parse_Receiver(p_Receivers, &p_req->Receiver);
		}
		p_Receivers = p_Receivers->next;
	}
#endif // RECEIVER_SUPPORT

#ifdef IPFILTER_SUPPORT
	p_IPAddressFilter = xml_node_soap_get(p_node, "IPAddressFilter");
	if (p_IPAddressFilter)
	{
		parse_IPAddressFilter(p_IPAddressFilter, &g_onvif_cfg.ipaddr_filter);
	}
#endif // IPFILTER_SUPPORT
	p_scope = xml_node_soap_get(p_node, "scope");
	while (p_scope && strcmp(p_scope->name, "scope") == 0)
	{
		onvif_parse_scope(p_scope);
		g_onvif_cfg.ScopesFlag = 1;
		p_scope = p_scope->next;
	}
	p_event = xml_node_soap_get(p_node, "event");
	if (p_event)
	{
		onvif_parse_event_cfg(p_event);
	}
	xml_node_del(p_node);
}

void onvif_load_cfg(const char * filename)
{
	int len;
	int rlen;
	FILE * fp;
	char * xml_buff;

	// read config file
	
	fp = fopen(filename, "r");
	if (NULL == fp)
	{
		log_print(HT_LOG_ERR, "%s, open file(%s) failed\r\n", __FUNCTION__, filename);
		return;
	}
	
	fseek(fp, 0, SEEK_END);
	
	len = ftell(fp);
	if (len <= 0)
	{
		fclose(fp);
		return;
	}
	fseek(fp, 0, SEEK_SET);
	
	xml_buff = (char *) malloc(len + 1);
	if (NULL == xml_buff)
	{
		fclose(fp);
		return;
	}

	rlen = (int)fread(xml_buff, 1, len, fp);
	if (rlen > 0)
	{
		xml_buff[rlen] = '\0';
		onvif_parse_cfg(xml_buff, rlen);
	}
	else
	{
		log_print(HT_LOG_ERR, "%s, rlen = %d, len=%d\r\n", __FUNCTION__, rlen, len);
	}
	
	fclose(fp);
	free(xml_buff);
	
	return;
}

BOOL onvif_save_head(FILE * fp)
{
	char buf[128];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, 
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"<config>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_tail(FILE * fp)
{
	char buf[128];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, 
		"</config>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_server(FILE * fp)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, 
		"<server_ip>%s</server_ip>\r\n"
		"<http_enable>%d</http_enable>\r\n"
		"<http_port>%d</http_port>\r\n",
		g_onvif_cfg.server_ip,
		g_onvif_cfg.http_enable,
		g_onvif_cfg.http_port);

#ifdef HTTPS    
	offset += snprintf(buf+offset, len-offset, 
		"<https_enable>%d</https_enable>\r\n"
		"<https_port>%d</https_port>\r\n"
		"<cert_file>%s</cert_file>\r\n"
		"<key_file>%s</key_file>\r\n",
		g_onvif_cfg.https_enable,
		g_onvif_cfg.https_port,
		g_onvif_cfg.cert_file,
		g_onvif_cfg.key_file);
#endif

	offset += snprintf(buf+offset, len-offset, 
		"<http_max_users>%d</http_max_users>\r\n",
		g_onvif_cfg.http_max_users);

	offset += snprintf(buf+offset, len-offset, 
		"<need_auth>%d</need_auth>\r\n",
		g_onvif_cfg.need_auth);    

	offset += snprintf(buf+offset, len-offset, 
		"<log_enable>%d</log_enable>\r\n",
		g_onvif_cfg.log_enable); 

	offset += snprintf(buf+offset, len-offset, 
		"<log_level>%d</log_level>\r\n",
		g_onvif_cfg.log_level); 

	offset += snprintf(buf+offset, len-offset, 
		"<EndpointReference>%s</EndpointReference>\r\n",
		g_onvif_cfg.EndpointReference); 

#ifdef PROFILE_Q_SUPPORT
	offset += snprintf(buf+offset, len-offset, 
		"<device_state>%d</device_state>\r\n",
		g_onvif_cfg.device_state); 
#endif

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_device_information(FILE * fp)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<information>\r\n");
	offset += build_DeviceInformation_xml(buf+offset, len-offset, &g_onvif_cfg.DeviceInformation);
	offset += snprintf(buf+offset, len-offset, "</information>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_user(FILE * fp)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] == '\0')
		{
			continue;
		}

		offset += snprintf(buf+offset, len-offset, 
			"<user>\r\n"
			"<fixed>%s</fixed>\r\n"
			"<username>%s</username>\r\n"
			"<password>%s</password>\r\n"
			"<userlevel>%s</userlevel>\r\n"
			"</user>\r\n",
			g_onvif_cfg.users[i].fixed ? "TRUE" : "FALSE",
			g_onvif_cfg.users[i].Username,
			g_onvif_cfg.users[i].Password,
			onvif_UserLevelToString(g_onvif_cfg.users[i].UserLevel));
	}

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_remote_user(FILE * fp)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, 
		"<RemoteUser>\r\n"
		"<Username>%s</Username>\r\n"
		"<Password>%s</Password>\r\n"
		"<UseDerivedPassword>%s</UseDerivedPassword>\r\n"
		"</RemoteUser>\r\n",
		g_onvif_cfg.RemoteUser.Username,
		g_onvif_cfg.RemoteUser.Password,
		g_onvif_cfg.RemoteUser.UseDerivedPassword ? "TRUE" : "FALSE");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_scope(FILE * fp)
{
	char buf[100*160];
	int len = sizeof(buf);
	int offset = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (g_onvif_cfg.scopes[i].ScopeItem[0] == '\0')
		{
		    continue;
		}

		offset += snprintf(buf+offset, len-offset, "<scope>\r\n");
		offset += build_Scope_xml(buf+offset, len-offset, &g_onvif_cfg.scopes[i]);
		offset += snprintf(buf+offset, len-offset, "</scope>\r\n");
	}

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_event_cfg(FILE * fp)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, 
		"<event>\r\n"
			"<renew_interval>%d</renew_interval>\r\n"	
			"<simulate_enable>%d</simulate_enable>\r\n"
		"</event>\r\n",
		g_onvif_cfg.evt_renew_time,
		g_onvif_cfg.evt_sim_flag);

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_system_datetime(FILE * fp)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<SystemDateTime>\r\n");
	offset += build_SystemDateTime_xml(buf+offset, len-offset, &g_onvif_cfg.SystemDateTime);
	offset += snprintf(buf+offset, len-offset, "</SystemDateTime>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_network(FILE * fp)
{
	char buf[10000];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<network>\r\n");

	offset += build_NetworkProtocol_xml(buf+offset, len-offset, &g_onvif_cfg.network.NetworkProtocol);

	offset += snprintf(buf+offset, len-offset, "<DNSInformation>\r\n");
	offset += build_DNSInformation_xml(buf+offset, len-offset, &g_onvif_cfg.network.DNSInformation);
	offset += snprintf(buf+offset, len-offset, "</DNSInformation>\r\n");

	offset += snprintf(buf+offset, len-offset, "<NTPInformation>\r\n");
	offset += build_NTPInformation_xml(buf+offset, len-offset, &g_onvif_cfg.network.NTPInformation);
	offset += snprintf(buf+offset, len-offset, "</NTPInformation>\r\n");

	offset += snprintf(buf+offset, len-offset, "<HostnameInformation>\r\n");
	offset += snprintf(buf+offset, len-offset, 
		"<FromDHCP>%s</FromDHCP>\r\n"
		"<RebootNeeded>%s</RebootNeeded>\r\n",
		g_onvif_cfg.network.HostnameInformation.FromDHCP ? "TRUE" : "FALSE",
		g_onvif_cfg.network.HostnameInformation.RebootNeeded ? "TRUE" : "FALSE");
	offset += snprintf(buf+offset, len-offset, "</HostnameInformation>\r\n");

	offset += snprintf(buf+offset, len-offset, "<NetworkGateway>\r\n");
	offset += build_NetworkGateway_xml(buf+offset, len-offset, &g_onvif_cfg.network.NetworkGateway);
	offset += snprintf(buf+offset, len-offset, "</NetworkGateway>\r\n");

	offset += snprintf(buf+offset, len-offset, 
		"<DiscoveryMode>%s</DiscoveryMode>\r\n",
		onvif_DiscoveryModeToString(g_onvif_cfg.network.DiscoveryMode));

	offset += snprintf(buf+offset, len-offset, "<ZeroConfiguration>\r\n");
	offset += build_ZeroConfiguration_xml(buf+offset, len-offset, &g_onvif_cfg.network.ZeroConfiguration);
	offset += snprintf(buf+offset, len-offset, "</ZeroConfiguration>\r\n");

	offset += snprintf(buf+offset, len-offset, "<DynamicDNSInformation>\r\n");
	offset += build_DynamicDNSInformation_xml(buf+offset, len-offset, &g_onvif_cfg.network.DynamicDNSInformation);
	offset += snprintf(buf+offset, len-offset, "</DynamicDNSInformation>\r\n");

	offset += snprintf(buf+offset, len-offset, "</network>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_hashing_algorithm(FILE * fp)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<HashingAlgorithm md5=\"%d\" sha256=\"%d\" />\r\n",g_onvif_cfg.md5_hashing, g_onvif_cfg.sha256_hashing);

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

#ifdef IMAGE_SUPPORT

BOOL onvif_save_imaging_settings(FILE * fp, onvif_ImagingSettings * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<ImagingSettings>\r\n");
	offset += build_ImageSettings_xml(buf+offset, len-offset, p_req);
	offset += snprintf(buf+offset, len-offset, "</ImagingSettings>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_imaging_options(FILE * fp, onvif_ImagingOptions * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<ImagingOptions>\r\n");
	offset += build_ImageOptions_xml(buf+offset, len-offset, p_req);
	offset += snprintf(buf+offset, len-offset, "</ImagingOptions>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

#endif

#ifdef THERMAL_SUPPORT

BOOL onvif_save_thermal_configuration(FILE * fp, onvif_ThermalConfiguration * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<tth:ThermalConfiguration>\r\n");
	offset += build_ThermalConfiguration_xml(buf+offset, len-offset, p_req);
	offset += snprintf(buf+offset, len-offset, "</tth:ThermalConfiguration>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;  
}

BOOL onvif_save_radiometry_configuration(FILE * fp, onvif_RadiometryConfiguration * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<tth:RadiometryConfiguration>\r\n");
	offset += build_RadiometryConfiguration_xml(buf+offset, len-offset, p_req);
	offset += snprintf(buf+offset, len-offset, "</tth:RadiometryConfiguration>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE; 
}

#endif // end of THERMAL_SUPPORT

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

BOOL onvif_save_video_source_mode(FILE * fp, onvif_VideoSourceMode * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<VideoSourceModes token=\"%s\" Enabled=\"%s\">\r\n",p_req->token, p_req->Enabled ? "true" : "false");

	offset += build_VideoSourceMode_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</VideoSourceModes>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_video_source(FILE * fp, VideoSourceList * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, 
		"<VideoSources token=\"%s\">\r\n"
			"<Framerate>%0.1f</Framerate>\r\n"
			"<Resolution>\r\n"
				"<Width>%d</Width>\r\n"
				"<Height>%d</Height>\r\n"
			"</Resolution>\r\n", 
		p_req->VideoSource.token, 
		p_req->VideoSource.Framerate, 
		p_req->VideoSource.Resolution.Width, 
		p_req->VideoSource.Resolution.Height); 

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	if (!onvif_save_video_source_mode(fp, &p_req->VideoSourceMode))
	{
		log_print(HT_LOG_ERR, "%s, onvif_save_video_source_mode failed\r\n", __FUNCTION__);
		return FALSE;
	}

#ifdef IMAGE_SUPPORT
	if (!onvif_save_imaging_settings(fp, &p_req->ImagingSettings))
	{
		log_print(HT_LOG_ERR, "%s, onvif_save_imaging_settings failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_imaging_options(fp, &p_req->ImagingOptions))
	{
		log_print(HT_LOG_ERR, "%s, onvif_save_imaging_options failed\r\n", __FUNCTION__);
		return FALSE;
	}

	offset = snprintf(buf, len, "<CurrentPresetToken>%s</CurrentPresetToken>\r\n",p_req->CurrentPresetToken);

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
#endif

#ifdef THERMAL_SUPPORT
	offset = snprintf(buf, len, "<ThermalSupport>%s</ThermalSupport>\r\n", p_req->ThermalSupport ? "true" : "false");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	if (!onvif_save_thermal_configuration(fp, &p_req->ThermalConfiguration))
	{
		log_print(HT_LOG_ERR, "%s, onvif_save_thermal_configuration failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_radiometry_configuration(fp, &p_req->RadiometryConfiguration))
	{
		log_print(HT_LOG_ERR, "%s, onvif_save_radiometry_configuration failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

	offset = snprintf(buf, len, "</VideoSources>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_video_sources(FILE * fp)
{
	VideoSourceList * p_req = g_onvif_cfg.v_src;
	while (p_req)
	{
		if (!onvif_save_video_source(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_video_source failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_video_source_configuration(FILE * fp, onvif_VideoSourceConfiguration * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<VideoSourceConfigurations token=\"%s\">\r\n", p_req->token);
		
	offset += build_VideoSourceConfiguration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</VideoSourceConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_video_source_configurations(FILE * fp)
{
	VideoSourceConfigurationList * p_req = g_onvif_cfg.v_src_cfg;
	while (p_req)
	{
		if (!onvif_save_video_source_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_video_source_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_video_encoder_configuration(FILE * fp, onvif_VideoEncoder2Configuration * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<VideoEncoderConfigurations token=\"%s\"", p_req->token);

	if (p_req->GovLengthFlag)
	{
		offset += snprintf(buf+offset, len-offset, " GovLength=\"%d\"", p_req->GovLength);
	}

	if (p_req->AnchorFrameDistanceFlag)
	{
		offset += snprintf(buf+offset, len-offset, " AnchorFrameDistance=\"%d\"", p_req->AnchorFrameDistance);
	}

	if (p_req->ProfileFlag)
	{
		offset += snprintf(buf+offset, len-offset, " Profile=\"%s\"", p_req->Profile);
	}

	offset += snprintf(buf+offset, len-offset, " GuaranteedFrameRate=\"%s\">\r\n",p_req->GuaranteedFrameRate ? "true" : "false");

	offset += build_VideoEncoder2Configuration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "<tt:EncodingInterval>%d</tt:EncodingInterval>\r\n", p_req->RateControl.EncodingInterval);

	offset += snprintf(buf+offset, len-offset, "<tt:SessionTimeout>PT%dS</tt:SessionTimeout>\r\n", p_req->SessionTimeout);
		
	offset += snprintf(buf+offset, len-offset, "</VideoEncoderConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_video_encoder_configurations(FILE * fp)
{
	VideoEncoder2ConfigurationList * p_req = g_onvif_cfg.v_enc_cfg;
	while (p_req)
	{
		if (!onvif_save_video_encoder_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_video_encoder_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#ifdef AUDIO_SUPPORT

BOOL onvif_save_audio_source(FILE * fp, onvif_AudioSource * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += build_AudioSource_xml(buf+offset, len-offset, p_req);

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_audio_sources(FILE * fp)
{
	AudioSourceList * p_req = g_onvif_cfg.a_src;
	while (p_req)
	{
		if (!onvif_save_audio_source(fp, &p_req->AudioSource))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_audio_source failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_audio_source_configuration(FILE * fp, onvif_AudioSourceConfiguration * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<AudioSourceConfigurations token=\"%s\">\r\n", p_req->token);

	offset += build_AudioSourceConfiguration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</AudioSourceConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;    
}

BOOL onvif_save_audio_source_configurations(FILE * fp)
{
	AudioSourceConfigurationList * p_req = g_onvif_cfg.a_src_cfg;
	while (p_req)
	{
		if (!onvif_save_audio_source_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_audio_source_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_audio_encoder_configuration(FILE * fp, onvif_AudioEncoder2Configuration * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<AudioEncoderConfigurations token=\"%s\">\r\n", p_req->token);
		
	offset += build_AudioEncoder2Configuration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "<tt:SessionTimeout>PT%dS</tt:SessionTimeout>\r\n", p_req->SessionTimeout);
		
	offset += snprintf(buf+offset, len-offset, "</AudioEncoderConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_audio_encoder_configurations(FILE * fp)
{
	AudioEncoder2ConfigurationList * p_req = g_onvif_cfg.a_enc_cfg;
	while (p_req)
	{
		if (!onvif_save_audio_encoder_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_audio_encoder_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_audio_decoder_configuration(FILE * fp, AudioDecoderConfigurationList * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;
#ifdef MEDIA2_SUPPORT    
	AudioEncoder2ConfigurationOptionsList * p_option;
#endif

	offset += snprintf(buf+offset, len-offset, "<AudioDecoderConfigurations token=\"%s\">\r\n", p_req->Configuration.token);

	offset += build_AudioDecoderConfiguration_xml(buf+offset, len-offset, &p_req->Configuration);

#ifdef MEDIA_SUPPORT
	offset += snprintf(buf+offset, len-offset, "<Options>\r\n");
	offset += build_AudioDecoderConfigurationOptions_xml(buf+offset, len-offset, &p_req->Options);
	offset += snprintf(buf+offset, len-offset, "</Options>\r\n");
#endif

#ifdef MEDIA2_SUPPORT
	p_option = p_req->Options2;
	while (p_option)
	{
		offset += snprintf(buf+offset, len-offset, "<Options2>\r\n");        
		offset += build_AudioEncoder2ConfigurationOptions_xml(buf+offset, len-offset, &p_option->Options);
		offset += snprintf(buf+offset, len-offset, "</Options2>\r\n");

		p_option = p_option->next;
	}
#endif
	offset += snprintf(buf+offset, len-offset, "</AudioDecoderConfigurations>\r\n");
	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_audio_decoder_configurations(FILE * fp)
{
	AudioDecoderConfigurationList * p_req = g_onvif_cfg.a_dec_cfg;
	while (p_req)
	{
		if (!onvif_save_audio_decoder_configuration(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_audio_decoder_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // end of AUDIO_SUPPORT

BOOL onvif_save_osd_configuration(FILE * fp, onvif_OSDConfiguration * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<OSDConfigurations token=\"%s\">\r\n", p_req->token);
		
	offset += build_OSDConfiguration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</OSDConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_osd_configurations(FILE * fp)
{
	OSDConfigurationList * p_req = g_onvif_cfg.OSDs;
	while (p_req)
	{
		if (!onvif_save_osd_configuration(fp, &p_req->OSD))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_osd_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_metadata_configuration(FILE * fp, onvif_MetadataConfiguration * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset,
		"<MetadataConfigurations token=\"%s\" "
			"CompressionType=\"%s\" GeoLocation=\"%s\" ShapePolygon=\"%s\">\r\n", 
			p_req->token,
			p_req->CompressionType,
			p_req->GeoLocation ? "true" : "false",
			p_req->ShapePolygon ? "true" : "false");
		
	offset += build_MetadataConfiguration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</MetadataConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_metadata_configurations(FILE * fp)
{
	MetadataConfigurationList * p_req = g_onvif_cfg.metadata_cfg;
	while (p_req)
	{
		if (!onvif_save_metadata_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_metadata_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#ifdef MEDIA2_SUPPORT

BOOL onvif_save_mask(FILE * fp, onvif_Mask * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<Masks token=\"%s\">\r\n", p_req->token);
		
	offset += build_Mask_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</Masks>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_masks(FILE * fp)
{
	MaskList * p_req = g_onvif_cfg.mask;
	while (p_req)
	{
		if (!onvif_save_mask(fp, &p_req->Mask))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_mask failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // end of MEDIA2_SUPPORT

#ifdef VIDEO_ANALYTICS

BOOL onvif_save_video_analytics_configuration(FILE * fp, onvif_VideoAnalyticsConfiguration * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<VideoAnalyticsConfigurations token=\"%s\">\r\n", p_req->token);
		
	offset += build_VideoAnalyticsConfiguration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</VideoAnalyticsConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_video_analytics_configurations(FILE * fp)
{
	VideoAnalyticsConfigurationList * p_req = g_onvif_cfg.va_cfg;
	while (p_req)
	{
		if (!onvif_save_video_analytics_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_video_analytics_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}
 
#endif // end of VIDEO_ANALYTICS

#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef PTZ_SUPPORT

BOOL onvif_save_ptz_node(FILE * fp, onvif_PTZNode * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<PTZNodes token=\"%s\">\r\n", p_req->token);
		
	offset += build_PTZNode_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</PTZNodes>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_ptz_nodes(FILE * fp)
{
	PTZNodeList * p_req = g_onvif_cfg.ptz_node;
	while (p_req)
	{
		if (!onvif_save_ptz_node(fp, &p_req->PTZNode))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_ptz_node failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_ptz_configuration(FILE * fp, onvif_PTZConfiguration * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, 
		"<PTZConfigurations token=\"%s\" "
			"MoveRamp=\"%d\" PresetRamp=\"%d\" PresetTourRamp=\"%d\">\r\n", 
		p_req->token, 
		p_req->MoveRamp,
		p_req->PresetRamp, 
		p_req->PresetTourRamp);
		
	offset += build_PTZConfiguration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</PTZConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_ptz_configurations(FILE * fp)
{
	PTZConfigurationList * p_req = g_onvif_cfg.ptz_cfg;
	while (p_req)
	{
		if (!onvif_save_ptz_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_ptz_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // end of PTZ_SUPPORT

BOOL onvif_save_profile(FILE * fp, ONVIF_PROFILE * p_req)
{
	log_print(HT_LOG_INFO, "||||||||||||||||||\n");
	log_print(HT_LOG_INFO, "onvif_save_profile START\n");
	char buf[40960];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<profile token=\"%s\" fixed=\"%s\">\r\n",p_req->token, p_req->fixed ? "true" : "false");

	offset += snprintf(buf+offset, len-offset, 
		"<Name>%s</Name>\r\n"
		"<stream_uri append_params=\"%d\">%s</stream_uri>\r\n",
		p_req->name,
		p_req->append_params,
		p_req->stream_uri);

	if (p_req->v_src_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<VideoSourceConfiguration token=\"%s\"></VideoSourceConfiguration>\r\n", p_req->v_src_cfg->Configuration.token);
	}

#ifdef AUDIO_SUPPORT
	if (p_req->a_src_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<AudioSourceConfiguration token=\"%s\"></AudioSourceConfiguration>\r\n",p_req->a_src_cfg->Configuration.token);
	}
#endif

	if (p_req->v_enc_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<VideoEncoderConfiguration token=\"%s\"></VideoEncoderConfiguration>\r\n", p_req->v_enc_cfg->Configuration.token);
	}

#ifdef AUDIO_SUPPORT
	if (p_req->a_enc_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<AudioEncoderConfiguration token=\"%s\"></AudioEncoderConfiguration>\r\n", p_req->a_enc_cfg->Configuration.token);
	}
#endif

#ifdef VIDEO_ANALYTICS
	if (p_req->va_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<VideoAnalyticsConfiguration token=\"%s\"></VideoAnalyticsConfiguration>\r\n", p_req->va_cfg->Configuration.token);
	}
#endif

#ifdef PTZ_SUPPORT
	if (p_req->ptz_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<PTZConfiguration token=\"%s\"></PTZConfiguration>\r\n", p_req->ptz_cfg->Configuration.token);
	}

	if (p_req->presets)
	{
		PTZPresetList * p_preset = p_req->presets;
		while (p_preset)
		{
			offset += snprintf(buf+offset, len-offset, "<Presets token=\"%s\">\r\n", p_preset->PTZPreset.token);
			offset += build_PTZPreset_xml(buf+offset, len-offset, &p_preset->PTZPreset);
			offset += snprintf(buf+offset, len-offset, "</Presets>\r\n");

			p_preset = p_preset->next;
		}
	}

	if (p_req->preset_tour)
	{
		PresetTourList * p_preset_tour = p_req->preset_tour;
		while (p_preset_tour)
		{
			offset += snprintf(buf+offset, len-offset, "<PresetTours token=\"%s\">\r\n", p_preset_tour->PresetTour.token);
			
			offset += build_PresetTour_xml(buf+offset, len-offset, &p_preset_tour->PresetTour);
			
			offset += snprintf(buf+offset, len-offset, "</PresetTours>\r\n");

			p_preset_tour = p_preset_tour->next;
		}
	}
#endif

	if (p_req->metadata_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<MetadataConfiguration token=\"%s\"></MetadataConfiguration>\r\n", p_req->metadata_cfg->Configuration.token);
	}

#ifdef DEVICEIO_SUPPORT
	if (p_req->a_output_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<AudioOutputConfiguration token=\"%s\"></AudioOutputConfiguration>\r\n", p_req->a_output_cfg->Configuration.token);
	}
#endif

#ifdef AUDIO_SUPPORT
	if (p_req->a_dec_cfg)
	{
		offset += snprintf(buf+offset, len-offset, "<AudioDecoderConfiguration token=\"%s\"></AudioDecoderConfiguration>\r\n", p_req->a_dec_cfg->Configuration.token);
	}
#endif

	offset += snprintf(buf+offset, len-offset, "</profile>\r\n");

	log_print(HT_LOG_INFO, "onvif_save_profile OVER\n");
	log_print(HT_LOG_INFO, "||||||||||||||||||\n");
	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_profiles(FILE * fp)/*�˳�����*/
{
	ONVIF_PROFILE * p_req = g_onvif_cfg.profiles;
	while (p_req)
	{
		if (!onvif_save_profile(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_profile failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#ifdef PROFILE_G_SUPPORT

BOOL onvif_save_recording(FILE * fp, RecordingList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	char EarliestRecording[64];
	char LatestRecording[64];
	onvif_get_time_str_s(EarliestRecording, sizeof(EarliestRecording), p_req->EarliestRecording, 0);
	onvif_get_time_str_s(LatestRecording, sizeof(LatestRecording), p_req->LatestRecording, 0);
		
	offset += snprintf(buf+offset, len-offset, "<Recordings>\r\n");
	offset += build_Recording_xml(buf+offset, len-offset, &p_req->Recording);
	offset += snprintf(buf+offset, len-offset, 
		"<EarliestRecording>%s</EarliestRecording>\r\n"
		"<LatestRecording>%s</LatestRecording>\r\n"
		"<RecordingStatus>%s</RecordingStatus>\r\n",
		EarliestRecording,
		LatestRecording,
		onvif_RecordingStatusToString(p_req->RecordingStatus));
	offset += snprintf(buf+offset, len-offset, "</Recordings>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_recordings(FILE * fp)
{
	RecordingList * p_req = g_onvif_cfg.recordings;
	while (p_req)
	{
		if (!onvif_save_recording(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_recording failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_recording_job(FILE * fp, onvif_RecordingJob * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<RecordingJobs>\r\n");
	offset += build_RecordingJob_xml(buf+offset, len-offset, p_req);
	offset += snprintf(buf+offset, len-offset, "</RecordingJobs>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_recording_jobs(FILE * fp)
{
	RecordingJobList * p_req = g_onvif_cfg.recording_jobs;
	while (p_req)
	{
		if (!onvif_save_recording_job(fp, &p_req->RecordingJob))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_recording_job failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_replay_session_timeout(FILE * fp)
{
	char buf[200];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<replay_session_timeout>%d</replay_session_timeout>\r\n",g_onvif_cfg.replay_session_timeout);

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

#endif // end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

BOOL onvif_save_access_point(FILE * fp, AccessPointList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<AccessPoints token=\"%s\">\r\n", p_req->AccessPointInfo.token);

	offset += build_AccessPointInfo_xml(buf+offset, len-offset, &p_req->AccessPointInfo);

	offset += snprintf(buf+offset, len-offset, "<tac:AuthenticationProfileToken>%s</tac:AuthenticationProfileToken>\r\n",p_req->AuthenticationProfileToken);

	offset += snprintf(buf+offset, len-offset, "<Enabled>%s</Enabled>\r\n",p_req->Enabled ? "true" : "false");  

	offset += snprintf(buf+offset, len-offset, "</AccessPoints>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_access_points(FILE * fp)
{
	AccessPointList * p_req = g_onvif_cfg.access_points;
	while (p_req)
	{
		if (!onvif_save_access_point(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_access_point failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_door(FILE * fp, DoorList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<Doors token=\"%s\">\r\n", p_req->Door.DoorInfo.token);

	offset += build_Door_xml(buf+offset, len-offset, &p_req->Door);

	offset += snprintf(buf+offset, len-offset, "<DoorState>\r\n");
	offset += build_DoorState_xml(buf+offset, len-offset, &p_req->DoorState);
	offset += snprintf(buf+offset, len-offset, "</DoorState>\r\n");

	offset += snprintf(buf+offset, len-offset, "</Doors>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_doors(FILE * fp)
{
	DoorList * p_req = g_onvif_cfg.doors;
	while (p_req)
	{
		if (!onvif_save_door(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_door failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_area(FILE * fp, onvif_AreaInfo * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<Areas token=\"%s\">\r\n", p_req->token);
	offset += build_AreaInfo_xml(buf+offset, len-offset, p_req);
	offset += snprintf(buf+offset, len-offset, "</Areas>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_areas(FILE * fp)
{
	AreaList * p_req = g_onvif_cfg.areas;
	while (p_req)
	{
		if (!onvif_save_area(fp, &p_req->AreaInfo))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_area failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT

BOOL onvif_save_video_output(FILE * fp, onvif_VideoOutput * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<VideoOutputs token=\"%s\">\r\n",p_req->token);

	offset += build_VideoOutput_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</VideoOutputs>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_video_outputs(FILE * fp)
{
	VideoOutputList * p_req = g_onvif_cfg.v_output;
	while (p_req)
	{
		if (!onvif_save_video_output(fp, &p_req->VideoOutput))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_video_output failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_video_output_configuration(FILE * fp, onvif_VideoOutputConfiguration * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<VideoOutputConfigurations token=\"%s\">\r\n",p_req->token);

	offset += build_VideoOutputConfiguration_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</VideoOutputConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
			return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_video_output_configurations(FILE * fp)
{
	VideoOutputConfigurationList * p_req = g_onvif_cfg.v_output_cfg;
	while (p_req)
	{
		if (!onvif_save_video_output_configuration(fp, &p_req->Configuration))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_video_output_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_audio_output(FILE * fp, onvif_AudioOutput * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<AudioOutputs token=\"%s\"></AudioOutputs>\r\n", p_req->token);

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_audio_outputs(FILE * fp)
{
	AudioOutputList * p_req = g_onvif_cfg.a_output;
	while (p_req)
	{
		if (!onvif_save_audio_output(fp, &p_req->AudioOutput))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_audio_output failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_audio_output_configuration(FILE * fp, AudioOutputConfigurationList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<AudioOutputConfigurations token=\"%s\">\r\n", p_req->Configuration.token);

	offset += build_AudioOutputConfiguration_xml(buf+offset, len-offset, &p_req->Configuration);

	offset += snprintf(buf+offset, len-offset, "<Options>\r\n");
	offset += build_AudioOutputConfigurationOptions_xml(buf+offset, len-offset, &p_req->Options);
	offset += snprintf(buf+offset, len-offset, "</Options>\r\n");

	offset += snprintf(buf+offset, len-offset, "</AudioOutputConfigurations>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_audio_output_configurations(FILE * fp)
{
	AudioOutputConfigurationList * p_req = g_onvif_cfg.a_output_cfg;

	while (p_req)
	{
		if (!onvif_save_audio_output_configuration(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_audio_output_configuration failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_relay_output(FILE * fp, RelayOutputList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<RelayOutputs token=\"%s\">\r\n", p_req->RelayOutput.token);

	offset += build_RelayOutput_xml(buf+offset, len-offset, &p_req->RelayOutput);

	offset += snprintf(buf+offset, len-offset, "<Options token=\"%s\">\r\n", p_req->Options.token);
	offset += build_RelayOutputOptions_xml(buf+offset, len-offset, &p_req->Options);
	offset += snprintf(buf+offset, len-offset, "</Options>\r\n");

	offset += snprintf(buf+offset, len-offset, "</RelayOutputs>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_relay_outputs(FILE * fp)
{
	RelayOutputList * p_req = g_onvif_cfg.relay_output;
	while (p_req)
	{
		if (!onvif_save_relay_output(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_relay_output failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_digital_input(FILE * fp, DigitalInputList * p_req)
{
	char buf[1024];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<DigitalInputs token=\"%s\" IdleState=\"%s\">\r\n", 
		p_req->DigitalInput.token, onvif_DigitalIdleStateToString(p_req->DigitalInput.IdleState));

	offset += snprintf(buf+offset, len-offset, "<Options>\r\n");
	offset += build_DigitalInputConfigurationOptions_xml(buf+offset, len-offset, &p_req->Options);
	offset += snprintf(buf+offset, len-offset, "</Options>\r\n");

	offset += snprintf(buf+offset, len-offset, "</DigitalInputs>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_digital_inputs(FILE * fp)
{
	DigitalInputList * p_req = g_onvif_cfg.digit_input;
	while (p_req)
	{
		if (!onvif_save_digital_input(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_digital_input failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_serial_port(FILE * fp, SerialPortList * p_req)
{
	char buf[2048];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<SerialPorts token=\"%s\">\r\n", p_req->SerialPort.token);

	offset += snprintf(buf+offset, len-offset, "<Configuration token=\"%s\" type=\"%s\">\r\n",
		p_req->Configuration.token, onvif_SerialPortTypeToString(p_req->Configuration.type));
	offset += build_SerialPortConfiguration_xml(buf+offset, len-offset, &p_req->Configuration);
	offset += snprintf(buf+offset, len-offset, "</Configuration>\r\n");

	offset += snprintf(buf+offset, len-offset, "<Options token=\"%s\">\r\n", p_req->Options.token);
	offset += build_SerialPortConfigurationOptions_xml(buf+offset, len-offset, &p_req->Options);
	offset += snprintf(buf+offset, len-offset, "</Options>\r\n");

	offset += snprintf(buf+offset, len-offset, "</SerialPorts>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_serial_ports(FILE * fp)
{
	SerialPortList * p_req = g_onvif_cfg.serial_port;
	while (p_req)
	{
		if (!onvif_save_serial_port(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_serial_port failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // end of DEVICEIO_SUPPORT

#ifdef CREDENTIAL_SUPPORT

BOOL onvif_save_credential(FILE * fp, CredentialList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<Credentials token=\"%s\">\r\n", p_req->Credential.token);

	offset += build_Credential_xml(buf+offset, len-offset, &p_req->Credential);

	offset += snprintf(buf+offset, len-offset, "<State>\r\n");
	offset += build_CredentialState_xml(buf+offset, len-offset, &p_req->State);	
	offset += snprintf(buf+offset, len-offset, "</State>\r\n");

	offset += snprintf(buf+offset, len-offset, "</Credentials>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_credentials(FILE * fp)
{
	CredentialList * p_req = g_onvif_cfg.credential;
	while (p_req)
	{
		if (!onvif_save_credential(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_credential failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_credential_whiltlists(FILE * fp)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;
	CredentialIdentifierItemList * p_req = g_onvif_cfg.whiltlist;

	offset += snprintf(buf+offset, len-offset, "<CredentialWhiltlists>\r\n");

	while (p_req)
	{
		offset += snprintf(buf+offset, len-offset, "<Identifier>\r\n");
		offset += build_CredentialIdentifierItem_xml(buf+offset, len-offset, &p_req->Item);
		offset += snprintf(buf+offset, len-offset, "</Identifier>\r\n");

		p_req = p_req->next;
	}
	
	offset += snprintf(buf+offset, len-offset, "</CredentialWhiltlists>\r\n");
	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_credential_blacklists(FILE * fp)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;
	CredentialIdentifierItemList * p_req = g_onvif_cfg.whiltlist;

	offset += snprintf(buf+offset, len-offset, "<CredentialBlacklists>\r\n");

	while (p_req)
	{
		offset += snprintf(buf+offset, len-offset, "<Identifier>\r\n");
		offset += build_CredentialIdentifierItem_xml(buf+offset, len-offset, &p_req->Item);
		offset += snprintf(buf+offset, len-offset, "</Identifier>\r\n");

		p_req = p_req->next;
	}

	offset += snprintf(buf+offset, len-offset, "</CredentialBlacklists>\r\n");
	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

BOOL onvif_save_access_rule(FILE * fp, onvif_AccessProfile * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<AccessProfiles token=\"%s\">\r\n", p_req->token);

	offset += build_AccessProfile_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</AccessProfiles>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL onvif_save_access_rules(FILE * fp)
{
	AccessProfileList * p_req = g_onvif_cfg.access_rules;
	while (p_req)
	{
		if (!onvif_save_access_rule(fp, &p_req->AccessProfile))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_access_rule failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

BOOL onvif_save_schedule(FILE * fp, ScheduleList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<Schedules token=\"%s\">\r\n", p_req->Schedule.token);

	offset += build_Schedule_xml(buf+offset, len-offset, &p_req->Schedule);

	offset += snprintf(buf+offset, len-offset, "</Schedules>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL onvif_save_schedules(FILE * fp)
{
	ScheduleList * p_req = g_onvif_cfg.schedule;
	while (p_req)
	{
		if (!onvif_save_schedule(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_schedule failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

BOOL onvif_save_specialday_group(FILE * fp, onvif_SpecialDayGroup * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<SpecialDayGroups token=\"%s\">", p_req->token);

	offset += build_SpecialDayGroup_xml(buf+offset, len-offset, p_req);

	offset += snprintf(buf+offset, len-offset, "</SpecialDayGroups>");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE;    
}

BOOL onvif_save_specialday_groups(FILE * fp)
{
	SpecialDayGroupList * p_req = g_onvif_cfg.specialdaygroup;
	while (p_req)
	{
		if (!onvif_save_specialday_group(fp, &p_req->SpecialDayGroup))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_specialday_group failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

BOOL onvif_save_receiver(FILE * fp, ReceiverList * p_req)
{
	char buf[4096];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<Receivers>\r\n");
	offset += build_Receiver_xml(buf+offset, len-offset, &p_req->Receiver);
	offset += snprintf(buf+offset, len-offset, "</Receivers>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}

	return TRUE; 
}

BOOL onvif_save_receivers(FILE * fp)
{
	ReceiverList * p_req = g_onvif_cfg.receiver;
	while (p_req)
	{
		if (!onvif_save_receiver(fp, p_req))
		{
			log_print(HT_LOG_ERR, "%s, onvif_save_receiver failed\r\n", __FUNCTION__);
			return FALSE;
		}
		p_req = p_req->next;
	}
	return TRUE;
}

#endif // RECEIVER_SUPPORT

#ifdef IPFILTER_SUPPORT

BOOL onvif_save_ipaddress_filter(FILE * fp)
{
	char buf[1024];
	int len = sizeof(buf);
	int offset = 0;

	offset += snprintf(buf+offset, len-offset, "<IPAddressFilter>\r\n");
	offset += build_IPAddressFilter_xml(buf+offset, len-offset, &g_onvif_cfg.ipaddr_filter);    
	offset += snprintf(buf+offset, len-offset, "</IPAddressFilter>\r\n");

	if (offset != fwrite(buf, 1, offset, fp))
	{
		return FALSE;
	}
	return TRUE;
}

#endif // end of IPFILTER_SUPPORT

BOOL onvif_save_cfg(const char * filename)
{
	FILE * fp;

	fp = fopen(filename, "w+");
	if (NULL == fp)
	{
		log_print(HT_LOG_ERR, "%s, open file(%s) failed\r\n", __FUNCTION__, filename);
		return FALSE;
	}

	onvif_save_head(fp);

	if (!onvif_save_server(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_server failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_device_information(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_device_information failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_user(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_user failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_remote_user(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_remote_user failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_system_datetime(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_system_datetime failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_network(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_network failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_hashing_algorithm(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_hashing_algorithm failed\r\n", __FUNCTION__);
		return FALSE;
	}

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

	if (!onvif_save_video_sources(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_video_sources failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_video_source_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_video_source_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_video_encoder_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_video_encoder_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

#ifdef AUDIO_SUPPORT
	if (!onvif_save_audio_sources(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_audio_sources failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_audio_source_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_audio_source_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_audio_encoder_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_audio_encoder_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_audio_decoder_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_audio_decoder_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

	if (!onvif_save_osd_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_osd_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_metadata_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_metadata_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

#ifdef MEDIA2_SUPPORT
	if (!onvif_save_masks(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_masks failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef VIDEO_ANALYTICS
	if (!onvif_save_video_analytics_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_video_analytics_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#endif // #if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef PTZ_SUPPORT
	if (!onvif_save_ptz_nodes(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_ptz_nodes failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_ptz_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_ptz_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef DEVICEIO_SUPPORT
	if (!onvif_save_video_outputs(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_video_outputs failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_video_output_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_video_output_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_audio_outputs(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_audio_outputs failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_audio_output_configurations(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_audio_output_configurations failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_relay_outputs(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_relay_outputs failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_digital_inputs(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_digital_inputs failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_serial_ports(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_serial_ports failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

	if (!onvif_save_profiles(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_profiles failed\r\n", __FUNCTION__);
		return FALSE;
	}

#ifdef PROFILE_G_SUPPORT
	if (!onvif_save_recordings(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_recordings failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_recording_jobs(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_recording_jobs failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_replay_session_timeout(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_replay_session_timeout failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef PROFILE_C_SUPPORT
	if (!onvif_save_doors(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_doors failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_areas(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_areas failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_access_points(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_access_points failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef CREDENTIAL_SUPPORT
	if (!onvif_save_credentials(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_credentials failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_credential_whiltlists(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_credential_whiltlists failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_credential_blacklists(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_credential_blacklists failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef ACCESS_RULES
	if (!onvif_save_access_rules(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_access_rules failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef SCHEDULE_SUPPORT
	if (!onvif_save_schedules(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_schedules failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_specialday_groups(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_specialday_groups failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef RECEIVER_SUPPORT
	if (!onvif_save_receivers(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_receivers failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

#ifdef IPFILTER_SUPPORT
	if (!onvif_save_ipaddress_filter(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_ipaddress_filter failed\r\n", __FUNCTION__);
		return FALSE;
	}
#endif

	if (!onvif_save_scope(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_scope failed\r\n", __FUNCTION__);
		return FALSE;
	}

	if (!onvif_save_event_cfg(fp))
	{
		fclose(fp);
		log_print(HT_LOG_ERR, "%s, onvif_save_event_cfg failed\r\n", __FUNCTION__);
		return FALSE;
	}

	onvif_save_tail(fp);

	fclose(fp);

	return TRUE;
}



