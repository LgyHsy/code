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
#include "onvif_media2.h"
#include "onvif_media.h"
#include "onvif_event.h"
#include "onvif_utils.h"
#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "anj_video.h"
#include "anj_record.h"
#include "anj_config.h"

#ifdef MEDIA2_SUPPORT

/************************************************************************************/
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_IDX g_onvif_idx;

extern int is_Y_Version;	//研创兴定制版本 ** 
extern unsigned int g_motion_mode;//MD和UMD的mode区分 1-umd,0-md
extern int g_stereo;
extern int isHBVersion;
/************************************************************************************
 *  	
 * Whenever a profile is created, deleted or one or more of its configurations are 
 * added or removed the following event should be generated.
 *
*************************************************************************************/
void onvif_MediaProfileChangedNotify(ONVIF_PROFILE * p_profile)
{
	NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Media/ProfileChanged", PropertyOperation_Changed, 
		"Token", p_profile->token, NULL, NULL, NULL, NULL, NULL, NULL);
	if (p_message)
	{
		onvif_put_NotificationMessage(p_message);
	}
}

/************************************************************************************
 *  	
 * Whenever a Configuration of a device changes the device should provide the 
 * following event. 
 * For the parameter Type pass the appropriate ConfigurationEnumeration value
 *
*************************************************************************************/
void onvif_MediaConfigurationChangedNotify(const char * token, const char * type)
{
	NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Media/ConfigurationChanged", PropertyOperation_Changed, 
		"Token", token, "Type", type, NULL, NULL, NULL, NULL);
	if (p_message)
	{
		onvif_put_NotificationMessage(p_message);
	}
}

/************************************************************************************
 *
 * @brief
 *  Modifies a configuration. 
 *  The change may have immediate effect to running streams but the changes
 *  are not guaranteed to take effect unless the client restarts any affected
 *  stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetVideoEncoderConfiguration(tr2_SetVideoEncoderConfiguration_REQ * p_req)
{
	VideoEncoder2ConfigurationList * p_v_enc_cfg;
	VideoEncoder2ConfigurationOptionsList * p_option;

	log_print(HT_LOG_INFO, "tr2 SetVEC token[%s] name[%s] enc[%s] %dx%d Q=%.1f Gov=%d(flag=%d) fps=%.1f br=%d rc_flag=%d stereo=%d\n",
		p_req->Configuration.token,
		p_req->Configuration.Name,
		p_req->Configuration.Encoding,
		p_req->Configuration.Resolution.Width,
		p_req->Configuration.Resolution.Height,
		p_req->Configuration.Quality,
		p_req->Configuration.GovLength,
		p_req->Configuration.GovLengthFlag,
		p_req->Configuration.RateControl.FrameRateLimit,
		p_req->Configuration.RateControl.BitrateLimit,
		p_req->Configuration.RateControlFlag,
		g_stereo);

	p_v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_req->Configuration.token);
	if (NULL == p_v_enc_cfg)
	{
		log_print(HT_LOG_INFO, "tr2 SetVEC fail: NoConfig token[%s]\n", p_req->Configuration.token);
		return ONVIF_ERR_NoConfig;
	}

	p_option = onvif_find_VideoEncoder2ConfigurationOptions(p_v_enc_cfg->Options2, p_req->Configuration.Encoding);
	if (p_option)
	{
		if (p_req->Configuration.Quality < p_option->Options.QualityRange.Min || 
			p_req->Configuration.Quality > p_option->Options.QualityRange.Max )
		{
			log_print(HT_LOG_INFO, "tr2 SetVEC fail: Quality %.1f out of range [%d,%d] enc[%s]\n",
				p_req->Configuration.Quality,
				p_option->Options.QualityRange.Min,
				p_option->Options.QualityRange.Max,
				p_req->Configuration.Encoding);
			return ONVIF_ERR_ConfigModify;
		}
	}
	else
	{
		log_print(HT_LOG_INFO, "tr2 SetVEC: no Options2 for enc[%s], skip Quality check\n",
			p_req->Configuration.Encoding);
	}
	
	VideoEncode pCfg;
	memset(&pCfg, 0, sizeof(VideoEncode));
	memcpy(&pCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoEncode, sizeof(pCfg));

	VideoCaptureCfg videoCfg;
	memset(&videoCfg, 0, sizeof(VideoCaptureCfg));
	memcpy(&videoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoCapture, sizeof(videoCfg));

	int stream=0;
	if (g_stereo)
	{
		if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_1_1") || 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_2_1")) //MainStream
			stream = 0;
		else if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_1_2") || 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_2_2"))//SubStream
			stream = 1;
		else if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_1_3") || 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_2_3"))//ThirdStream
			stream = 2;
		else
		{
			log_print(HT_LOG_INFO, "tr2 SetVEC fail: unknown token[%s] stereo=%d\n",
				p_req->Configuration.token, g_stereo);
			return ONVIF_ERR_ConfigModify;
		}
	}
	else
	{
		if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_1")) //MainStream
			stream = 0;
		else if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_2"))//SubStream
			stream = 1;
		else if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_3"))//ThirdStream
			stream = 2;
		else
		{
			log_print(HT_LOG_INFO, "tr2 SetVEC fail: unknown token[%s] stereo=%d\n",
				p_req->Configuration.token, g_stereo);
			return ONVIF_ERR_ConfigModify;
		}
	}
	RESOLUTION_ENTRY *pEntry = NULL;
	if (strstr(p_req->Configuration.Encoding, "H264"))
	{
		pEntry = GetResolution("H264", p_req->Configuration.Resolution.Width, p_req->Configuration.Resolution.Height, videoCfg.tvsystem, stream);
	}
	else if (strstr(p_req->Configuration.Encoding, "H265"))
	{
		pEntry = GetResolution("H265", p_req->Configuration.Resolution.Width, p_req->Configuration.Resolution.Height, videoCfg.tvsystem, stream);
	}
	else if (strstr(p_req->Configuration.Encoding, "JPEG"))
	{
		pEntry = GetResolution("MJPEG", p_req->Configuration.Resolution.Width, p_req->Configuration.Resolution.Height, videoCfg.tvsystem, stream);
	}
	else
	{
		log_print(HT_LOG_INFO, "tr2 SetVEC fail: unsupported Encoding[%s]\n",
			p_req->Configuration.Encoding);
		return ONVIF_ERR_ConfigModify;
	}
	
	if( pEntry == NULL )
	{
		log_print(HT_LOG_INFO, "tr2 SetVEC fail: no resolution %dX%d stream=%d enc[%s] tv=%d\n",
			p_req->Configuration.Resolution.Width,
			p_req->Configuration.Resolution.Height,
			stream,
			p_req->Configuration.Encoding,
			videoCfg.tvsystem);
		return ONVIF_ERR_ConfigModify;
	}
	if( p_req->Configuration.RateControl.FrameRateLimit > pEntry->max_framerate )
		pCfg.encodeCfg[stream].frameRate = pEntry->max_framerate ;
	else if (p_req->Configuration.RateControl.FrameRateLimit< pEntry->min_framerate )
		pCfg.encodeCfg[stream].frameRate = pEntry->def_framerate;
	else
		pCfg.encodeCfg[stream].frameRate = p_req->Configuration.RateControl.FrameRateLimit;
	
	pCfg.encodeCfg[stream].display_frameRate = pCfg.encodeCfg[stream].frameRate;

	if( p_req->Configuration.RateControl.BitrateLimit > pEntry->max_bitrate )
		pCfg.encodeCfg[stream].bitRate = pEntry->max_bitrate ;
	else if (p_req->Configuration.RateControl.BitrateLimit < pEntry->min_bitrate )
		pCfg.encodeCfg[stream].bitRate = pEntry->min_bitrate;
	else
		pCfg.encodeCfg[stream].bitRate = p_req->Configuration.RateControl.BitrateLimit;
	
	pCfg.encodeCfg[stream].bitRateQuality=  VIDEO_QUALITY_CUSTOM;

	if (strstr(p_req->Configuration.Encoding, "H264"))
	{
		if (p_req->Configuration.GovLength < p_v_enc_cfg->Options.H264.GovLengthRange.Min || p_req->Configuration.GovLength > p_v_enc_cfg->Options.H264.GovLengthRange.Max)
		{
			log_print(HT_LOG_INFO, "tr2 SetVEC fail: H264 GovLength %d out of range [%d,%d] flag=%d\n",
				p_req->Configuration.GovLength,
				p_v_enc_cfg->Options.H264.GovLengthRange.Min,
				p_v_enc_cfg->Options.H264.GovLengthRange.Max,
				p_req->Configuration.GovLengthFlag);
			return ONVIF_ERR_ConfigModify;
		}

		if (p_req->Configuration.GovLength > 0)
		{
			pCfg.encodeCfg[stream].initQuant = p_req->Configuration.GovLength;
			if(isHBVersion > 0)
			{
				if((p_req->Configuration.RateControl.FrameRateLimit > 0) && (p_req->Configuration.GovLength <= p_req->Configuration.RateControl.FrameRateLimit))
					pCfg.encodeCfg[stream].initQuant = -1;
			}
		}
		else if (p_req->Configuration.RateControl.FrameRateLimit > 0)
			pCfg.encodeCfg[stream].initQuant = p_req->Configuration.RateControl.FrameRateLimit * 4;

		strcpy(pCfg.encodeCfg[stream].encodeFormat.name, "H264");
	}
	else if (strstr(p_req->Configuration.Encoding, "H265"))
	{
		if (p_req->Configuration.GovLength < p_v_enc_cfg->Options.H265.GovLengthRange.Min || p_req->Configuration.GovLength > p_v_enc_cfg->Options.H265.GovLengthRange.Max)
		{
			log_print(HT_LOG_INFO, "tr2 SetVEC fail: H265 GovLength %d out of range [%d,%d] flag=%d\n",
				p_req->Configuration.GovLength,
				p_v_enc_cfg->Options.H265.GovLengthRange.Min,
				p_v_enc_cfg->Options.H265.GovLengthRange.Max,
				p_req->Configuration.GovLengthFlag);
			return ONVIF_ERR_ConfigModify;
		}

		if (p_req->Configuration.GovLength > 0)
		{
			pCfg.encodeCfg[stream].initQuant = p_req->Configuration.GovLength;
			if(isHBVersion > 0)
			{
				if((p_req->Configuration.RateControl.FrameRateLimit > 0) && (p_req->Configuration.GovLength <= p_req->Configuration.RateControl.FrameRateLimit))
					pCfg.encodeCfg[stream].initQuant = -1;
			}
		}
		else if (p_req->Configuration.RateControl.FrameRateLimit > 0)
			pCfg.encodeCfg[stream].initQuant = p_req->Configuration.RateControl.FrameRateLimit * 4;

		strcpy(pCfg.encodeCfg[stream].encodeFormat.name, "H265");
	}
	else if (strstr(p_req->Configuration.Encoding, "JPEG"))
	{
		pCfg.encodeCfg[stream].initQuant = p_req->Configuration.RateControl.FrameRateLimit * 4;

		strcpy(pCfg.encodeCfg[stream].encodeFormat.name, "MJPEG");
	}
	
	strcpy(pCfg.encodeCfg[stream].resolution.name, pEntry->res_name);
	
	p_v_enc_cfg->Configuration.Resolution.Width = p_req->Configuration.Resolution.Width;
	p_v_enc_cfg->Configuration.Resolution.Height = p_req->Configuration.Resolution.Height;
	p_v_enc_cfg->Configuration.Quality = p_req->Configuration.Quality;
	strcpy(p_v_enc_cfg->Configuration.Encoding, p_req->Configuration.Encoding);
	strcpy(p_v_enc_cfg->Configuration.Name, p_req->Configuration.Name);

	if (strcasecmp(p_req->Configuration.Encoding, "JPEG") == 0)
	{
		p_v_enc_cfg->Configuration.VideoEncoding = VideoEncoding_JPEG;
	}
	else if (strcasecmp(p_req->Configuration.Encoding, "H264") == 0)
	{
		p_v_enc_cfg->Configuration.VideoEncoding = VideoEncoding_H264;
	}
	else if (strcasecmp(p_req->Configuration.Encoding, "H265") == 0)
	{
		// The onvif media 1 service does not support H265, 
		// here set the VideoEncoding of the onvif media 1 service field to H264
		p_v_enc_cfg->Configuration.VideoEncoding = VideoEncoding_H264;
	}

	if (p_req->Configuration.GovLengthFlag)
	{
		p_v_enc_cfg->Configuration.GovLength = p_req->Configuration.GovLength;
	}

	if (strcasecmp(p_req->Configuration.Encoding, "JPEG") == 0)
	{
		p_v_enc_cfg->Configuration.GovLengthFlag = 0;
	}
	else
	{
		p_v_enc_cfg->Configuration.GovLengthFlag = 1;
	}	

	if (p_req->Configuration.ProfileFlag)
	{
		strcpy(p_v_enc_cfg->Configuration.Profile, p_req->Configuration.Profile);
	}

	if (p_req->Configuration.RateControlFlag)
	{
		p_v_enc_cfg->Configuration.RateControl.FrameRateLimit = p_req->Configuration.RateControl.FrameRateLimit;		
		p_v_enc_cfg->Configuration.RateControl.BitrateLimit = p_req->Configuration.RateControl.BitrateLimit;

		if (p_req->Configuration.RateControl.ConstantBitRateFlag)
		{
			p_v_enc_cfg->Configuration.RateControl.ConstantBitRate = p_req->Configuration.RateControl.ConstantBitRate;
		}
	}
	if (p_req->Configuration.MulticastFlag)
	{
		memcpy(&p_v_enc_cfg->Configuration.Multicast, &p_req->Configuration.Multicast, sizeof(onvif_MulticastConfiguration));
	}
	onvif_MediaConfigurationChangedNotify(p_req->Configuration.token, "VideoEncoder");
	
	// todo : here add handler code ...
	int needSwitch = anj_config_video_encode_set(&pCfg, 0);
	if (needSwitch)
	{
		anj_video_encode_switch();
	}
	log_print(HT_LOG_INFO, "tr2 SetVEC ok stream=%d res[%s] fps=%d br=%d gop=%d\n",
		stream,
		pCfg.encodeCfg[stream].resolution.name,
		pCfg.encodeCfg[stream].frameRate,
		pCfg.encodeCfg[stream].bitRate,
		pCfg.encodeCfg[stream].initQuant);
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Returns the available parameters and their valid ranges to the client. 
 *  Any combination of the parameters obtained using a given media profile 
 *  and configuration shall be a valid input for the corresponding set 
 *  configuration command.
 *
 *  If a configuration token is provided, the device shall return the options
 *  compatible with that configuration. If a media profile token is specified, 
 *  the device shall return the options compatible with that media profile. 
 *  If both a media profile token and a configuration token are specified, the 
 *  device shall return the options compatible with both that media profile and 
 *  that configuration. If no tokens are specified, the options shall be 
 *  considered generic for the device.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_IncompatibleConfiguration
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_GetVideoEncoderConfigurationOptions(tr2_GetVideoEncoderConfigurationOptions_REQ * p_req, tr2_GetVideoEncoderConfigurationOptions_RES * p_res)
{
	log_print(HT_LOG_INFO, "onvif_tr2_GetVideoEncoderConfigurationOptions\n");
	ONVIF_PROFILE * p_profile = NULL;
	VideoEncoder2ConfigurationList * p_v_enc_cfg = NULL;
	VideoEncoder2ConfigurationOptionsList * p_option;

	if (p_req->GetConfiguration.ConfigurationTokenFlag)
	{
		p_v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_req->GetConfiguration.ConfigurationToken);
		if (NULL == p_v_enc_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (p_req->GetConfiguration.ProfileTokenFlag)
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->GetConfiguration.ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}
		p_v_enc_cfg = p_profile->v_enc_cfg;
	}

	if (NULL == p_v_enc_cfg)
	{
		p_v_enc_cfg = g_onvif_cfg.v_enc_cfg;
	}

	if (NULL == p_v_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_option = p_v_enc_cfg->Options2;

	while (p_option)
	{
		VideoEncoder2ConfigurationOptionsList * p_item = onvif_add_VideoEncoder2ConfigurationOptions(&p_res->Options);
		if (p_item)
		{
			memcpy(&p_item->Options, &p_option->Options, sizeof(onvif_VideoEncoder2ConfigurationOptions));
		}
		p_option = p_option->next;
	}
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Creates a new media profile. The media profile shall be created in the device.
 *
 *  A created profile shall be deletable and a device shall set the "fixed" 
 *  attribute to false in the returned Profile.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_MaxNVTProfiles
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_CreateProfile(tr2_CreateProfile_REQ * p_req, tr2_CreateProfile_RES * p_res)
{
	int i;
	ONVIF_PROFILE * p_profile = NULL;
	
	p_profile = onvif_add_profile(&g_onvif_cfg.profiles, TRUE);
	if (p_profile)
	{
		strcpy(p_profile->name, p_req->Name);		
		strcpy(p_res->Token, p_profile->token);

		// add configuration to the new profile
		for (i = 0; i < p_req->sizeConfiguration; i++)
		{
			if (strcasecmp(p_req->Configuration[i].Type, "all") == 0)
			{
				ONVIF_PROFILE * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_profile(g_onvif_cfg.profiles, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.profiles;
				}

				if (NULL == p_refer)
				{
					continue;
				}
			
				p_profile->v_src_cfg = p_refer->v_src_cfg;
				if (p_profile->v_src_cfg)
				{
					p_profile->v_src_cfg->Configuration.UseCount++;
				}
				
				p_profile->v_enc_cfg = p_refer->v_enc_cfg;
				if (p_profile->v_enc_cfg)
				{
					p_profile->v_enc_cfg->Configuration.UseCount++;
				}	
				
				p_profile->metadata_cfg = p_refer->metadata_cfg;
				if (p_profile->metadata_cfg)
				{
					p_profile->metadata_cfg->Configuration.UseCount++;
				}	
				
#ifdef AUDIO_SUPPORT				
				p_profile->a_src_cfg = p_refer->a_src_cfg;
				if (p_profile->a_src_cfg)
				{
					p_profile->a_src_cfg->Configuration.UseCount++;
				}
				
				p_profile->a_enc_cfg = p_refer->a_enc_cfg;
				if (p_profile->a_enc_cfg)
				{
					p_profile->a_enc_cfg->Configuration.UseCount++;
				}

				p_profile->a_dec_cfg = p_refer->a_dec_cfg;
				if (p_profile->a_dec_cfg)
				{
					p_profile->a_dec_cfg->Configuration.UseCount++;
				}
#endif		

#ifdef DEVICEIO_SUPPORT
				p_profile->a_output_cfg = p_refer->a_output_cfg;
				if (p_profile->a_output_cfg)
				{
					p_profile->a_output_cfg->Configuration.UseCount++;
				}
#endif

#ifdef VIDEO_ANALYTICS
				p_profile->va_cfg = p_refer->va_cfg;
				if (p_profile->va_cfg)
				{
					p_profile->va_cfg->Configuration.UseCount++;
				}
#endif

#ifdef PTZ_SUPPORT
				p_profile->ptz_cfg = p_refer->ptz_cfg;
				if (p_profile->ptz_cfg)
				{
					p_profile->ptz_cfg->Configuration.UseCount++;
				}
#endif
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "VideoSource") == 0)
			{
				VideoSourceConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.v_src_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->v_src_cfg = p_refer;
				p_profile->v_src_cfg->Configuration.UseCount++;
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "VideoEncoder") == 0)
			{
				VideoEncoder2ConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.v_enc_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->v_enc_cfg = p_refer;
				p_profile->v_enc_cfg->Configuration.UseCount++;
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "AudioSource") == 0)
			{
#ifdef AUDIO_SUPPORT
				AudioSourceConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.a_src_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->a_src_cfg = p_refer;
				p_profile->a_src_cfg->Configuration.UseCount++;
#endif
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "AudioEncoder") == 0)
			{
#ifdef AUDIO_SUPPORT
				AudioEncoder2ConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.a_enc_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->a_enc_cfg = p_refer;
				p_profile->a_enc_cfg->Configuration.UseCount++;
#endif			
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "AudioOutput") == 0)
			{
#ifdef DEVICEIO_SUPPORT
				AudioOutputConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.a_output_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->a_output_cfg = p_refer;
				p_profile->a_output_cfg->Configuration.UseCount++;
#endif			
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "AudioDecoder") == 0)
			{
#ifdef AUDIO_SUPPORT		
				AudioDecoderConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.a_dec_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->a_dec_cfg = p_refer;
				p_profile->a_dec_cfg->Configuration.UseCount++;
#endif	
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "Metadata") == 0)
			{
				MetadataConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_MetadataConfiguration(g_onvif_cfg.metadata_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.metadata_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->metadata_cfg = p_refer;
				p_profile->metadata_cfg->Configuration.UseCount++;
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "Analytics") == 0)
			{
#ifdef VIDEO_ANALYTICS
				VideoAnalyticsConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.va_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->va_cfg = p_refer;
				p_profile->va_cfg->Configuration.UseCount++;
#endif
			}
			else if (strcasecmp(p_req->Configuration[i].Type, "PTZ") == 0)
			{
#ifdef PTZ_SUPPORT
				PTZConfigurationList * p_refer = NULL;
				if (p_req->Configuration[i].TokenFlag)
				{
					p_refer = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, p_req->Configuration[i].Token);
				}
				else
				{
					p_refer = g_onvif_cfg.ptz_cfg;
				}
				if (NULL == p_refer)
				{
					continue;
				}
				p_profile->ptz_cfg = p_refer;
				p_profile->ptz_cfg->Configuration.UseCount++;
#endif			
			}
		}
		// setup the new profile stream uri
		if (g_onvif_cfg.profiles)
		{
			strcpy(p_profile->stream_uri, g_onvif_cfg.profiles->stream_uri);
		}
	}
	else 
	{
		return ONVIF_ERR_MaxNVTProfiles;
	}
	onvif_MediaProfileChangedNotify(p_profile);
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Deletes a profile.
 *
 *  A device signaling support for MultiTrackStreaming shall support deleting
 *  of virtual profiles via the command. Note that deleting a profile of a 
 *  virtual profile set may invalidate the virtual profile.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_DeletionOfFixedProfile
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_DeleteProfile(tr2_DeleteProfile_REQ * p_req)
{
	return ONVIF_ERR_ServiceNotSupported;
	trt_DeleteProfile_REQ req;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->Token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	onvif_MediaProfileChangedNotify(p_profile);

	strcpy(req.ProfileToken, p_req->Token);

	return onvif_trt_DeleteProfile(&req);
}

/************************************************************************************
 *
 * @brief
 *  Adds one or more configurations to an existing media profile. 
 *  If one of the configuration already exists in the media profile, 
 *  it will be replaced.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_AddConfiguration(tr2_AddConfiguration_REQ * p_req)
{
	int i;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_req->NameFlag)
	{
		strcpy(p_profile->name, p_req->Name);
	}

	// add configuration to the new profile
	for (i = 0; i < p_req->sizeConfiguration; i++)
	{
		if (strcasecmp(p_req->Configuration[i].Type, "all") == 0)
		{
			ONVIF_PROFILE * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_profile(g_onvif_cfg.profiles, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.profiles;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->v_src_cfg = p_refer->v_src_cfg;
			if (p_profile->v_src_cfg)
			{
				p_profile->v_src_cfg->Configuration.UseCount++;
			}

			p_profile->v_enc_cfg = p_refer->v_enc_cfg;
			if (p_profile->v_enc_cfg)
			{
				p_profile->v_enc_cfg->Configuration.UseCount++;
			}

			p_profile->metadata_cfg = p_refer->metadata_cfg;
			if (p_profile->metadata_cfg)
			{
				p_profile->metadata_cfg->Configuration.UseCount++;
			}

#ifdef AUDIO_SUPPORT				
			p_profile->a_src_cfg = p_refer->a_src_cfg;
			if (p_profile->a_src_cfg)
			{
				p_profile->a_src_cfg->Configuration.UseCount++;
			}

			p_profile->a_enc_cfg = p_refer->a_enc_cfg;
			if (p_profile->a_enc_cfg)
			{
				p_profile->a_enc_cfg->Configuration.UseCount++;
			}

			p_profile->a_dec_cfg = p_refer->a_dec_cfg;
			if (p_profile->a_dec_cfg)
			{
				p_profile->a_dec_cfg->Configuration.UseCount++;
			}
#endif		

#ifdef DEVICEIO_SUPPORT
			p_profile->a_output_cfg = p_refer->a_output_cfg;
			if (p_profile->a_output_cfg)
			{
				p_profile->a_output_cfg->Configuration.UseCount++;
			}
#endif

#ifdef VIDEO_ANALYTICS
			p_profile->va_cfg = p_refer->va_cfg;
			if (p_profile->va_cfg)
			{
				p_profile->va_cfg->Configuration.UseCount++;
			}
#endif

#ifdef PTZ_SUPPORT
			p_profile->ptz_cfg = p_refer->ptz_cfg;
			if (p_profile->ptz_cfg)
			{
				p_profile->ptz_cfg->Configuration.UseCount++;
			}
#endif
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "VideoSource") == 0)
		{
			VideoSourceConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.v_src_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->v_src_cfg = p_refer;
			p_profile->v_src_cfg->Configuration.UseCount++;
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "VideoEncoder") == 0)
		{
			VideoEncoder2ConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.v_enc_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->v_enc_cfg = p_refer;
			p_profile->v_enc_cfg->Configuration.UseCount++;
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioSource") == 0)
		{
#ifdef AUDIO_SUPPORT
			AudioSourceConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.a_src_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->a_src_cfg = p_refer;
			p_profile->a_src_cfg->Configuration.UseCount++;
#endif
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioEncoder") == 0)
		{
#ifdef AUDIO_SUPPORT
			AudioEncoder2ConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.a_enc_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->a_enc_cfg = p_refer;
			p_profile->a_enc_cfg->Configuration.UseCount++;
#endif			
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioOutput") == 0)
		{
#ifdef DEVICEIO_SUPPORT
			AudioOutputConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.a_output_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->a_output_cfg = p_refer;
			p_profile->a_output_cfg->Configuration.UseCount++;
#endif			
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioDecoder") == 0)
		{
#ifdef AUDIO_SUPPORT		
			AudioDecoderConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.a_dec_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->a_dec_cfg = p_refer;
			p_profile->a_dec_cfg->Configuration.UseCount++;
#endif			
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "Metadata") == 0)
		{
			MetadataConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_MetadataConfiguration(g_onvif_cfg.metadata_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.metadata_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->metadata_cfg = p_refer;
			p_profile->metadata_cfg->Configuration.UseCount++;
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "Analytics") == 0)
		{
#ifdef VIDEO_ANALYTICS
			VideoAnalyticsConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.va_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->va_cfg = p_refer;
			p_profile->va_cfg->Configuration.UseCount++;
#endif
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "PTZ") == 0)
		{
#ifdef PTZ_SUPPORT
			PTZConfigurationList * p_refer = NULL;
			if (p_req->Configuration[i].TokenFlag)
			{
				p_refer = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, p_req->Configuration[i].Token);
			}
			else
			{
				p_refer = g_onvif_cfg.ptz_cfg;
			}

			if (NULL == p_refer)
			{
				continue;
			}

			p_profile->ptz_cfg = p_refer;
			p_profile->ptz_cfg->Configuration.UseCount++;
#endif			
		}
	}

	onvif_MediaProfileChangedNotify(p_profile);

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes one or more configurations from an existing media profile. 
 *  Tokens appearing in the configuration list shall be ignored. 
 *  Presence of the "All" type shall result in an empty profile. 
 *  Removing a non existing configuration shall be ignored and not 
 *  result in an error.
 *  
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_RemoveConfiguration(tr2_RemoveConfiguration_REQ * p_req)
{
	int i;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	for (i = 0; i < p_req->sizeConfiguration; i++)
	{
		if (strcasecmp(p_req->Configuration[i].Type, "all") == 0)
		{
			if (p_profile->v_src_cfg && p_profile->v_src_cfg->Configuration.UseCount > 0)
			{
				p_profile->v_src_cfg->Configuration.UseCount--;
				p_profile->v_src_cfg = NULL;
			}

			if (p_profile->v_enc_cfg && p_profile->v_enc_cfg->Configuration.UseCount > 0)
			{
				p_profile->v_enc_cfg->Configuration.UseCount--;
				p_profile->v_enc_cfg = NULL;
			}

			if (p_profile->metadata_cfg && p_profile->metadata_cfg->Configuration.UseCount > 0)
			{
				p_profile->metadata_cfg->Configuration.UseCount--;
				p_profile->metadata_cfg = NULL;
			}
			
#ifdef AUDIO_SUPPORT	
			if (p_profile->a_src_cfg && p_profile->a_src_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_src_cfg->Configuration.UseCount--;
				p_profile->a_src_cfg = NULL;
			}

			if (p_profile->a_enc_cfg && p_profile->a_enc_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_enc_cfg->Configuration.UseCount--;
				p_profile->a_enc_cfg = NULL;
			}

			if (p_profile->a_dec_cfg && p_profile->a_dec_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_dec_cfg->Configuration.UseCount--;
				p_profile->a_dec_cfg = NULL;
			}
#endif		

#ifdef DEVICEIO_SUPPORT
			if (p_profile->a_output_cfg && p_profile->a_output_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_output_cfg->Configuration.UseCount--;
				p_profile->a_output_cfg = NULL;
			}
#endif

#ifdef VIDEO_ANALYTICS
			if (p_profile->va_cfg && p_profile->va_cfg->Configuration.UseCount > 0)
			{
				p_profile->va_cfg->Configuration.UseCount--;
				p_profile->va_cfg = NULL;
			}
#endif

#ifdef PTZ_SUPPORT
			if (p_profile->ptz_cfg && p_profile->ptz_cfg->Configuration.UseCount > 0)
			{
				p_profile->ptz_cfg->Configuration.UseCount--;
				p_profile->ptz_cfg = NULL;
			}
#endif
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "VideoSource") == 0)
		{
			if (p_profile->v_src_cfg && p_profile->v_src_cfg->Configuration.UseCount > 0)
			{
				p_profile->v_src_cfg->Configuration.UseCount--;
				p_profile->v_src_cfg = NULL;
			}
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "VideoEncoder") == 0)
		{
			if (p_profile->v_enc_cfg && p_profile->v_enc_cfg->Configuration.UseCount > 0)
			{
				p_profile->v_enc_cfg->Configuration.UseCount--;
				p_profile->v_enc_cfg = NULL;
			}
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioSource") == 0)
		{
#ifdef AUDIO_SUPPORT				
			if (p_profile->a_src_cfg && p_profile->a_src_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_src_cfg->Configuration.UseCount--;
				p_profile->a_src_cfg = NULL;
			}
#endif
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioEncoder") == 0)
		{
#ifdef AUDIO_SUPPORT
			if (p_profile->a_enc_cfg && p_profile->a_enc_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_enc_cfg->Configuration.UseCount--;
				p_profile->a_enc_cfg = NULL;
			}
#endif			
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioOutput") == 0)
		{
#ifdef DEVICEIO_SUPPORT
			if (p_profile->a_output_cfg && p_profile->a_output_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_output_cfg->Configuration.UseCount--;
				p_profile->a_output_cfg = NULL;
			}
#endif			
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "AudioDecoder") == 0)
		{
#ifdef AUDIO_SUPPORT		
		    if (p_profile->a_dec_cfg && p_profile->a_dec_cfg->Configuration.UseCount > 0)
			{
				p_profile->a_dec_cfg->Configuration.UseCount--;
				p_profile->a_dec_cfg = NULL;
			}
#endif		
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "Metadata") == 0)
		{
			if (p_profile->metadata_cfg && p_profile->metadata_cfg->Configuration.UseCount > 0)
			{
				p_profile->metadata_cfg->Configuration.UseCount--;
				p_profile->metadata_cfg = NULL;
			}
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "Analytics") == 0)
		{
#ifdef VIDEO_ANALYTICS
			if (p_profile->va_cfg && p_profile->va_cfg->Configuration.UseCount > 0)
			{
				p_profile->va_cfg->Configuration.UseCount--;
				p_profile->va_cfg = NULL;
			}
#endif
		}
		else if (strcasecmp(p_req->Configuration[i].Type, "PTZ") == 0)
		{
#ifdef PTZ_SUPPORT
			if (p_profile->ptz_cfg && p_profile->ptz_cfg->Configuration.UseCount > 0)
			{
				p_profile->ptz_cfg->Configuration.UseCount--;
				p_profile->ptz_cfg = NULL;
			}
#endif
		}
	}
	onvif_MediaProfileChangedNotify(p_profile);
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a configuration. 
 *  The change may have immediate effect to running streams but the changes
 *  are not guaranteed to take effect unless the client restarts any affected
 *  stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetVideoSourceConfiguration(tr2_SetVideoSourceConfiguration_REQ * p_req)
{
	ONVIF_RET ret;
	trt_SetVideoSourceConfiguration_REQ req;

	memcpy(&req.Configuration, &p_req->Configuration, sizeof(onvif_VideoSourceConfiguration));
	req.ForcePersistence = TRUE;

	ret = onvif_trt_SetVideoSourceConfiguration(&req);

	if (ONVIF_OK == ret)
	{
		onvif_MediaConfigurationChangedNotify(req.Configuration.token, "VideoSource");
	}

	// todo : here add handler code ...
	return ret;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a configuration. 
 *  The change may have immediate effect to running streams but the changes
 *  are not guaranteed to take effect unless the client restarts any affected
 *  stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetMetadataConfiguration(tr2_SetMetadataConfiguration_REQ * p_req)
{
    ONVIF_RET ret;
	trt_SetMetadataConfiguration_REQ req;

	memcpy(&req.Configuration, &p_req->Configuration, sizeof(onvif_MetadataConfiguration));
	req.ForcePersistence = TRUE;

	ret = onvif_trt_SetMetadataConfiguration(&req);

    if (ONVIF_OK == ret)
    {
	    onvif_MediaConfigurationChangedNotify(p_req->Configuration.token, "Metadata");
	}

    // todo : here add handler code ...

    
	return ret;
}

/************************************************************************************
 *
 * @brief
 *  Provides information on how many video encoders a device can instantiate
 *  concurrently for a VideoSourceConfiguration.
 *
 *  The Info response contains the following information:
 *  Total Total number of encoder instances independent of the codec,
 *  Codec Number of encoder instances for each supported codec .
 *
 *  A device shall guarantee to instantiate the indicated number of instances
 *  concurrently. If a device limits the number of instances of each particular
 *  video encoding type, the response shall contain information per video codec.
 *  For each video source, there shall be at least one video source configuration
 *  for which the GetVideoEncoderInstances shall return a Total greater than 0.
 *  The total sum of video encoder instances over all video source configurations 
 *  of a device shall not exceed the value signaled via MaximumNumberOfProfiles.
 *
 *  For example, if a device has two VideoSourceConfigurations and if the first 
 *  allows a total of two concurrent instances and the second allows only one 
 *  instance, this device shall allow creation of at least three media profiles.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_GetVideoEncoderInstances(tr2_GetVideoEncoderInstances_REQ * p_req, tr2_GetVideoEncoderInstances_RES * p_res)
{
	log_print(HT_LOG_INFO, "onvif_tr2_GetVideoEncoderInstances\n");
	int total = 0;
	ONVIF_PROFILE * p_profile;
	VideoSourceConfigurationList * p_v_src = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->ConfigurationToken);
	if (NULL == p_v_src)
	{
		log_print(HT_LOG_INFO, "onvif_tr2_GetVideoEncoderInstances ONVIF_ERR_NoConfig\n");
		return ONVIF_ERR_NoConfig;
	}

	// todo : modify the p_res ...
	if (1)
	{
		p_res->Info.sizeCodec = 2;
		strcpy(p_res->Info.Codec[0].Encoding, "H264");
		p_res->Info.Codec[0].Number = 1;
		strcpy(p_res->Info.Codec[1].Encoding, "H265");
		p_res->Info.Codec[1].Number = 1;
	}
	// get the stream nums
	p_profile = g_onvif_cfg.profiles;
	while (p_profile)
	{
		if (p_profile->v_src_cfg && strcmp(p_profile->v_src_cfg->Configuration.token, p_req->ConfigurationToken) == 0)
		{
			total++;
		}

		p_profile = p_profile->next;
	}

	p_res->Info.Total = total;
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Synchronization points allow clients to decode and correctly use all 
 *  data after the synchronization point.
 *  For example, if a video stream is configured with a large I-frame 
 *  distance and a client loses a single packet, the client does not 
 *  display video until the next I-frame is transmitted. In such cases, 
 *  the client can request a Synchronization Point which forces the device
 *  to add an I-frame as soon as possible. Clients can request Synchronization
 *  Points for profiles. The device shall add synchronization points for 
 *  all streams associated with this profile.
 *
 *  Similarly, a synchronization point is used to get an update on full PTZ
 *  or event status through the metadata stream.
 *
 *  If a video stream is associated with the profile, an I-frame shall be 
 *  added to this video stream. If an event stream is associated to the profile,
 *  the synchronization point request shall be handled as described in the 
 *  section Synchronization Point of the ONVIF Core Specification. If the 
 *  profile is configured for PTZ metadata, the PTZ position shall be repeated
 *  within the metadata stream.
 *
 *  A device shall support the request for an I-frame through the 
 *  SetSynchronizationPoint command if the RTSPStreaming capability is set.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoProfile
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetSynchronizationPoint(tr2_SetSynchronizationPoint_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	// todo : here add handler code ...
	if(p_req->ProfileToken != NULL)
	{
		log_print(HT_LOG_INFO, "ProfileToken = %s", p_req->ProfileToken);
		int streamno = 0;
		if(strstr(p_req->ProfileToken, "ProfileToken_1"))
			streamno = 0;
		else if(strstr(p_req->ProfileToken, "ProfileToken_2"))
			streamno = 1;
		
		log_print(HT_LOG_INFO, "add an I-Frame, streamno = %d", streamno);
		anj_video_request_idr(0, streamno); //插入I帧
	}


	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Changes the media profile structure relating to video source for 
 *  the specified video source mode. A device that indicates a capability
 *  of VideoSourceMode shall support this command. The behavior after 
 *  changing the mode is not defined in this specification.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoVideoSource
 *	ONVIF_ERR_NoVideoSourceMode
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetVideoSourceMode(tr2_SetVideoSourceMode_REQ * p_req, tr2_SetVideoSourceMode_RES * p_res)
{
    VideoSourceList * p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->VideoSourceToken);
    if (NULL == p_v_src)
    {
        return ONVIF_ERR_NoVideoSource;
    }

    if (strcmp(p_v_src->VideoSourceMode.token, p_req->VideoSourceModeToken))
    {
        return ONVIF_ERR_NoVideoSourceMode;
    }

    // todo : here add handler code ...
    

    return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  A Network client uses the GetSnapshotUri command to obtain a JPEG
 *  snapshot from the device. The returned URI shall remain valid indefinitely
 *  even if the profile parameters change. The URI can be used for acquiring
 *  one or more JPEG images through a HTTP GET operation.
 *
 *  The image encoding will always be JPEG regardless of the encoding setting 
 *  in the media profile. The JPEG settings (like resolution or quality) 
 *  should be taken from the profile if suitable. The provided image shall be
 *  updated automatically and independent from calls to GetSnapshotUri.
 *
 *  A device shall support this command when the SnapshotUri capability is 
 *  set to true.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_IncompleteConfiguration
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_GetSnapshotUri(HTTPCLN * p_user, tr2_GetSnapshotUri_REQ * p_req, tr2_GetSnapshotUri_RES * p_res)
{
	char sip[32];
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	// set the media uri

	onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);
	
	int stream = 1;
	char username[256] = "";
	char password[256] = "";
	
	if (!strcmp(p_req->ProfileToken, "ProfileToken_1"))
	{
		stream = 0;
	}
	else if (!strcmp(p_req->ProfileToken, "ProfileToken_2"))
	{
		stream = 1;
	}
	MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
	if ((pStreamCfg && pStreamCfg->webConfig.onvif_auth) || is_Y_Version)
	{
		UserConfig usrCfg;
		int j;
		SystemConfig *pSystemCfg = (SystemConfig *)getSystemConfig();
			
		if (pSystemCfg)
		{
			memcpy(&usrCfg, &pSystemCfg->userCfg, sizeof(usrCfg));
			for(j = 0; j < usrCfg.count; j ++)
			{
				if(!strcmp(usrCfg.accounts[j].group.groupName, "Administrator") && !strcmp(usrCfg.accounts[j].status, "Enable"))
				{
					strcpy(username, usrCfg.accounts[j].userName);
					our_md5_encode(password, (const unsigned char *)usrCfg.accounts[j].password, strlen(usrCfg.accounts[j].password));
					break;
				}
			}
		}
	}
	
	if (g_onvif_cfg.http_enable && !p_srv->https)
	{
		if (username[0] != '\0' && password[0] != '\0')
		{
			snprintf(p_res->Uri, sizeof(p_res->Uri),
				"http://%s:%d/cgi-bin/snapshot.cgi?stream=%d&amp;username=%.63s&amp;password=%.63s",
				sip, g_onvif_cls.http_port, stream, username, password);
		}
		else
		{
			snprintf(p_res->Uri, sizeof(p_res->Uri),
				"http://%s:%d/cgi-bin/snapshot.cgi?stream=%d",
				sip, g_onvif_cls.http_port, stream);
		}
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		if (username[0] != '\0' && password[0] != '\0')
		{
			snprintf(p_res->Uri, sizeof(p_res->Uri),
				"https://%s:%d/cgi-bin/snapshot.cgi?stream=%d&amp;username=%.63s&amp;password=%.63s",
				sip, g_onvif_cls.https_port, stream, username, password);
		}
		else
		{
			snprintf(p_res->Uri, sizeof(p_res->Uri),
				"https://%s:%d/cgi-bin/snapshot.cgi?stream=%d",
				sip, g_onvif_cls.https_port, stream);
		}
	}
#endif

	return ONVIF_OK;
}

int onvif_tr2_BuildStreamParams(tr2_GetStreamUri_REQ * p_req, ONVIF_PROFILE * p_profile, char * buff, int len)
{
    int offset = 0;
    
    if (strcasecmp(p_req->Protocol, "RtspUnicast") == 0)
    {
        offset += snprintf(buff+offset, len-offset, "&amp;t=%s", "unicast");
        offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "udp");
    }
    else if (strcasecmp(p_req->Protocol, "RtspMulticast") == 0)
    {
        offset += snprintf(buff+offset, len-offset, "&amp;t=%s", "multicast");
        offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "udp");
    }
    else if (strcasecmp(p_req->Protocol, "RTSP") == 0)
    {
        offset += snprintf(buff+offset, len-offset, "&amp;t=%s", "unicast");
        offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "rtsp");
    }
    else if (strcasecmp(p_req->Protocol, "RtspOverHttp") == 0)
    {
        offset += snprintf(buff+offset, len-offset, "&amp;t=%s", "unicast");
        offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "http");
    }

    /**
     * If the audio and video parameters have been set to the encoder 
     * in the onvif_tr2_SetVideoEncoderConfiguration function, 
     * then there is no need to pass the audio and video parameters 
     * to the rtsp server through the url.
     *
     **/

#if 1
    if (p_profile->v_enc_cfg)
    {            
        offset += snprintf(buff+offset, len-offset, "&amp;ve=%s&amp;w=%d&amp;h=%d", 
            p_profile->v_enc_cfg->Configuration.Encoding,
            p_profile->v_enc_cfg->Configuration.Resolution.Width,
            p_profile->v_enc_cfg->Configuration.Resolution.Height);
        
    }

#ifdef AUDIO_SUPPORT
    if (p_profile->a_enc_cfg)
    {            
        offset += snprintf(buff+offset, len-offset, "&amp;ae=%s&amp;sr=%d", 
            p_profile->a_enc_cfg->Configuration.Encoding,
            p_profile->a_enc_cfg->Configuration.SampleRate * 1000);
        
    }

    if (p_profile->a_dec_cfg && p_profile->a_dec_cfg->Options2)
    {
        offset += snprintf(buff+offset, len-offset, "&amp;bce=%s", 
            p_profile->a_dec_cfg->Options2->Options.Encoding);
    }
#endif

#endif

    return offset;
}

/************************************************************************************
 *
 * @brief
 *  Requests a URI that can be used to initiate a live media stream using
 *  RTSP as the control protocol. The returned URI should remain valid 
 *  indefinitely even if the parameters of the profile are changed.
 *
 *  The following stream types are defined
 *  RtspUnicast RTSP streaming RTP via UDP Unicast.
 *  RtspMulticast RTSP streaming RTP via UDP Multicast.
 *  RTSP RTSP streaming RTP over TCP.
 *  RtspsUnicast Secure RTSP streaming SRTP via UDP Unicast.
 *  RtspsMulticast Secure RTSP streaming SRTP via UDP Multicast.
 *  RtspOverHttp Tunneling both the RTSP control channel and the RTP stream
 *  over HTTP or HTTPS.
 *
 *  For full compatibility with other ONVIF services a device shall not generate
 *  URIs longer than 128 octets.
 *
 *  A device that signals the RTSPStreaming capability shall support this command. 
 *  On a request for transport protocol RtspOverHttp a device shall return a URI
 *  that uses the same port as the web service. This enables seamless NAT traversal.
 *
 *  A device supporting MultiTrackStreaming shall support the retrieval of a 
 *  multitrack RTSP session URI by passing a virtual profile token.
 *  A device signaling support for SecureRTSPStreaming shall support streaming
 *  via SRTP.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_InvalidStreamSetup
 *	ONVIF_ERR_StreamConflict
 *	ONVIF_ERR_InvalidMulticastSettings
 *	ONVIF_ERR_IncompleteConfiguration
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_GetStreamUri(HTTPCLN * p_user, tr2_GetStreamUri_REQ * p_req, tr2_GetStreamUri_RES * p_res)
{
	int offset = 0;
	int len = sizeof(p_res->Uri);
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	
	char username[256]="";
	char password[256]="";
	get_username_and_password(username, password);
	
	int stream = 1;
	int base_len = 0;
	int max_cred_len = 0;
	char sip[32];
	onvif_get_service_ip_by_user(p_user, sip, sizeof(sip) - 1);
	base_len = snprintf(NULL, 0, "rtsp://%s:%d/stream%d?username=&amp;password=",
	                    sip, get_rtsp_port(), 2);
	if (base_len < 0)
		base_len = 0;
	if (base_len >= len)
		base_len = len - 1;
	max_cred_len = (len - base_len - 1) / 2;
	if (max_cred_len < 0)
		max_cred_len = 0;
	if (g_stereo)
	{
		if (strstr(p_req->ProfileToken, "ProfileToken_1_1") != NULL || strstr(p_req->ProfileToken, "ProfileToken_2_1") != NULL)
			stream = 0;
		else if (strstr(p_req->ProfileToken, "ProfileToken_1_2") != NULL || strstr(p_req->ProfileToken, "ProfileToken_2_2") != NULL)
			stream = 1;
		else if (strstr(p_req->ProfileToken, "ProfileToken_1_3") != NULL || strstr(p_req->ProfileToken, "ProfileToken_2_3") != NULL)
			stream = 2;
		// set the media uri
		if (strstr(p_req->ProfileToken, "ProfileToken_1_") != NULL)
			offset += snprintf(p_res->Uri, len, "rtsp://%.*s:%d/ch01/stream%d?username=%.*s&amp;password=%.*s",
			                   (int)(sizeof(sip) - 1), sip, get_rtsp_port(), stream,
			                   max_cred_len, username, max_cred_len, password);
		else
			offset += snprintf(p_res->Uri, len, "rtsp://%.*s:%d/ch02/stream%d?username=%.*s&amp;password=%.*s",
			                   (int)(sizeof(sip) - 1), sip, get_rtsp_port(), stream,
			                   max_cred_len, username, max_cred_len, password);
	}
	else
	{
		if (strstr(p_req->ProfileToken, "ProfileToken_1") != NULL)
			stream = 0;
		else if (strstr(p_req->ProfileToken, "ProfileToken_2") != NULL)
			stream = 1;
		else if (strstr(p_req->ProfileToken, "ProfileToken_3") != NULL)
			stream = 2;
		// set the media uri
		offset += snprintf(p_res->Uri, len, "rtsp://%.*s:%d/stream%d?username=%.*s&amp;password=%.*s",
		                   (int)(sizeof(sip) - 1), sip, get_rtsp_port(), stream,
		                   max_cred_len, username, max_cred_len, password);
	}

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Creates a new Mask for an existing VideoSourceConfiguration. 
 *  A device that signals support for Masks by the Mask capability shall
 *  support the creation of masks via this function as long as the number of
 *  existing masks does not exceed the value of MaxMasks for the given 
 *  VideoSourceConfiguration.
 *  
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_MaxMasks
 *	ONVIF_ERR_InvalidPolygon
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_CreateMask(tr2_CreateMask_REQ * p_req)
{
	MaskList * p_mask;
	VideoSourceConfigurationList * p_v_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->Mask.ConfigurationToken);
	if (NULL == p_v_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	p_mask = onvif_add_Mask(&g_onvif_cfg.mask, -1);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_MaxMasks;
	}

	if (p_req->Mask.Polygon.sizePoint <= 0)
	{
		return ONVIF_ERR_InvalidPolygon;
	}

	// return the token
	strcpy(p_req->Mask.token, p_mask->Mask.token);

	memcpy(&p_mask->Mask, &p_req->Mask, sizeof(onvif_Mask));

	// todo : here add handler code ... 
	int i_num = -1;
	if(p_req->Mask.token != NULL)
	{
		if(!strcmp(p_mask->Mask.token, "0"))
			i_num = 0;
		else if(!strcmp(p_mask->Mask.token, "1"))
			i_num = 1;
		else if(!strcmp(p_mask->Mask.token, "2"))
			i_num = 2;
		else if(!strcmp(p_mask->Mask.token, "3"))
			i_num = 3;
	}
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg, 0, sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));
	// 获取主子码流分辨率宽高
	int iIndex, imgW[2], imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex ++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);
	int mainX = 0, mainY = 0, mainW = 0, mainH = 0, subX = 0, subY = 0, subW = 0, subH = 0;

	float f0x = p_req->Mask.Polygon.Point[0].x;
	float f0y = p_req->Mask.Polygon.Point[0].y;
	float f1x = p_req->Mask.Polygon.Point[1].x;
	float f1y = p_req->Mask.Polygon.Point[1].y;
	float f2x = p_req->Mask.Polygon.Point[2].x;
	float f2y = p_req->Mask.Polygon.Point[2].y;
	float f3x = p_req->Mask.Polygon.Point[3].x;
	float f3y = p_req->Mask.Polygon.Point[3].y;

	int Point0x = (int)((f0x + 1) / 2 * imgW[0]);
	int Point1x = (int)((f1x + 1) / 2 * imgW[0]);
	int Point2x = (int)((f2x + 1) / 2 * imgW[0]);
	int Point3x = (int)((f3x + 1) / 2 * imgW[0]);
	int Point0y = (int)((1 - f0y) / 2 * imgH[0]);
	int Point1y = (int)((1 - f1y) / 2 * imgH[0]);
	int Point2y = (int)((1 - f2y) / 2 * imgH[0]);
	int Point3y = (int)((1 - f3y) / 2 * imgH[0]);

	Point0x = (Point0x < Point1x) ? Point0x : Point1x;
	Point0x = (Point0x < Point2x) ? Point0x : Point2x;
	Point0x = (Point0x < Point3x) ? Point0x : Point3x;
	Point0y = (Point0y < Point1y) ? Point0y : Point1y;
	Point0y = (Point0y < Point2y) ? Point0y : Point2y;
	Point0y = (Point0y < Point3y) ? Point0y : Point3y;
	mainX = Point0x;
	mainY = Point0y;
	subX = (int)(mainX * imgW[1] / imgW[0]);
	subY = (int)(mainY * imgH[1] / imgH[0]);
	
	Point0x = (Point0x > Point1x) ? Point0x : Point1x;
	Point0x = (Point0x > Point2x) ? Point0x : Point2x;
	Point0x = (Point0x > Point3x) ? Point0x : Point3x;
	Point0y = (Point0y > Point1y) ? Point0y : Point1y;
	Point0y = (Point0y > Point2y) ? Point0y : Point2y;
	Point0y = (Point0y > Point3y) ? Point0y : Point3y;
	mainW = Point0x - mainX;
	mainH = Point0y - mainY;
	subW = (int)(mainW * imgW[1] / imgW[0]);
	subH = (int)(mainH * imgH[1] / imgH[0]);
	int i = 0;
	if(!strcmp(p_req->Mask.ConfigurationToken, "VideoSourceConfigurationToken_1")) //VideoSourceConfigurationToken_1
	{
		for(i = 0; i < 4; i ++)
		{
			if((pCfg->mainStreamMaskList[i].xPos == 0)&&
				(pCfg->mainStreamMaskList[i].yPos == 0)&&
				(pCfg->mainStreamMaskList[i].width == 0)&&
				(pCfg->mainStreamMaskList[i].height == 0))
			break;
		}

		if((i_num >= 0) && (i_num < 4))
		{
			i = i_num;
		}
		pCfg->mainStreamMaskList[i].xPos = mainX;
		pCfg->mainStreamMaskList[i].yPos = mainY;
		pCfg->mainStreamMaskList[i].width = mainW;
		pCfg->mainStreamMaskList[i].height = mainH;

		pCfg->subStreamMaskList[i].xPos = subX;
		pCfg->subStreamMaskList[i].yPos = subY;
		pCfg->subStreamMaskList[i].width = subW;
		pCfg->subStreamMaskList[i].height = subH;

	}

	anj_config_video_mask_set(pCfg, 0);
	free(pVideoCfg);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Deletes a mask configuration.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_DeleteMask(tr2_DeleteMask_REQ * p_req)
{
	MaskList * p_prev;
	MaskList * p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_req->Token);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_NoConfig;
	}
	else
	{
		if (g_onvif_idx.mask_idx > 0)
		{
			g_onvif_idx.mask_idx --;
		}
	}
	// todo : here add handler code ...
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg, 0, sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);
	
	int token_int = -1;
	if(!strcmp(p_req->Token, "0"))
		token_int = 0;
	else if(!strcmp(p_req->Token, "1"))
		token_int = 1;
	else if(!strcmp(p_req->Token, "2"))
		token_int = 2;
	else if(!strcmp(p_req->Token, "3"))
		token_int = 3;
	
	if (token_int != -1)
	{
		onvif_del_Mask_token(token_int);
		memset(&pCfg->mainStreamMaskList[token_int], 0, sizeof(MASK_AREA_ENTRY));
		memset(&pCfg->subStreamMaskList[token_int], 0, sizeof(MASK_AREA_ENTRY));
	}
	else
	{
		memset(pCfg->mainStreamMaskList, 0, 4 * sizeof(MASK_AREA_ENTRY));
		memset(pCfg->subStreamMaskList, 0, 4 * sizeof(MASK_AREA_ENTRY));
	}
	
	anj_config_video_mask_set(pCfg, 0);
	free(pVideoCfg);

	p_prev = g_onvif_cfg.mask;
	if (p_mask == p_prev)
	{
		g_onvif_cfg.mask = p_mask->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_mask)
				break;
			else
				p_prev = p_prev->next;
		}
		p_prev->next = p_mask->next;
	}

	free(p_mask);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a mask configuration. Running streams using this configuration
 *  may be immediately updated according to the new settings.
 *
 *  A device signaling support for Mask via its capabilities support this
 *  command. It shall accept any combination of parameters returned by 
 *  GetMaskOptions. If necessary the device may adapt parameter values 
 *  for the Color and Polygon element without returning an error.
 *
 *  Note that for devices signaling SingleColorOnly all masks of the 
 *  associated VideoSource will be updated.
 *  
 *  Note: A device signaling RectangleOnly shall accept any polygon with
 *  four points. In case the four vertices are not defining an exact 
 *  rectangle the device may adjust the vertices.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_InvalidPolygon
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetMask(tr2_SetMask_REQ * p_req)
{
	MaskList * p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_req->Mask.token);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_req->Mask.ConfigurationToken[0] == '\0' || p_req->Mask.Polygon.sizePoint <= 0)
	{
		return ONVIF_ERR_InvalidArgVal;
	}
	// todo : here add handler code ...
	int i_num = -1;
	if(!strcmp(p_req->Mask.token, "0"))
		i_num = 0;
	else if(!strcmp(p_req->Mask.token, "1"))
		i_num = 1;
	else if(!strcmp(p_req->Mask.token, "2"))
		i_num = 2;
	else if(!strcmp(p_req->Mask.token, "3"))
		i_num = 3;

	if(i_num == -1)
	{
		log_print(HT_LOG_INFO, "ter:InvalidArgVal token\n");
		return ONVIF_ERR_InvalidArgVal;
	}
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg,0,sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));

	// 获取主子码流分辨率宽高
	int iIndex,imgW[2],imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);
	int mainX = 0, mainY = 0, mainW = 0, mainH = 0, subX = 0, subY = 0, subW = 0, subH = 0;

	float f0x = p_req->Mask.Polygon.Point[0].x;
	float f0y = p_req->Mask.Polygon.Point[0].y;
	float f1x = p_req->Mask.Polygon.Point[1].x;
	float f1y = p_req->Mask.Polygon.Point[1].y;
	float f2x = p_req->Mask.Polygon.Point[2].x;
	float f2y = p_req->Mask.Polygon.Point[2].y;
	float f3x = p_req->Mask.Polygon.Point[3].x;
	float f3y = p_req->Mask.Polygon.Point[3].y;

	int Point0x = (int)((f0x + 1) / 2 * imgW[0]);
	int Point1x = (int)((f1x + 1) / 2 * imgW[0]);
	int Point2x = (int)((f2x + 1) / 2 * imgW[0]);
	int Point3x = (int)((f3x + 1) / 2 * imgW[0]);
	int Point0y = (int)((1 - f0y) / 2 * imgH[0]);
	int Point1y = (int)((1 - f1y) / 2 * imgH[0]);
	int Point2y = (int)((1 - f2y) / 2 * imgH[0]);
	int Point3y = (int)((1 - f3y) / 2 * imgH[0]);

	Point0x = (Point0x < Point1x) ? Point0x : Point1x;
	Point0x = (Point0x < Point2x) ? Point0x : Point2x;
	Point0x = (Point0x < Point3x) ? Point0x : Point3x;
	Point0y = (Point0y < Point1y) ? Point0y : Point1y;
	Point0y = (Point0y < Point2y) ? Point0y : Point2y;
	Point0y = (Point0y < Point3y) ? Point0y : Point3y;
	mainX = Point0x;
	mainY = Point0y;
	subX = (int)(mainX * imgW[1] / imgW[0]);
	subY = (int)(mainY * imgH[1] / imgH[0]);
	
	Point0x = (Point0x > Point1x) ? Point0x : Point1x;
	Point0x = (Point0x > Point2x) ? Point0x : Point2x;
	Point0x = (Point0x > Point3x) ? Point0x : Point3x;
	Point0y = (Point0y > Point1y) ? Point0y : Point1y;
	Point0y = (Point0y > Point2y) ? Point0y : Point2y;
	Point0y = (Point0y > Point3y) ? Point0y : Point3y;
	mainW = Point0x - mainX;
	mainH = Point0y - mainY;
	subW = (int)(mainW * imgW[1] / imgW[0]);
	subH = (int)(mainH * imgH[1] / imgH[0]);
	if(!strcmp(p_req->Mask.ConfigurationToken, "VideoSourceConfigurationToken_1")) //MainStream
	{
		pCfg->mainStreamMaskList[i_num].xPos = mainX;
		pCfg->mainStreamMaskList[i_num].yPos = mainY;
		pCfg->mainStreamMaskList[i_num].width = mainW;
		pCfg->mainStreamMaskList[i_num].height = mainH;

		pCfg->subStreamMaskList[i_num].xPos = subX;
		pCfg->subStreamMaskList[i_num].yPos = subY;
		pCfg->subStreamMaskList[i_num].width = subW;
		pCfg->subStreamMaskList[i_num].height = subH;

		anj_config_video_mask_set(pCfg, 0);
	}
	memcpy(&p_mask->Mask, &p_req->Mask, sizeof(onvif_Mask));
	free(pVideoCfg);
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Starts multicast streaming using a specified media profile of a device. 
 *  Streaming continues until StopMulticastStreaming is called for the same
 *  Profile. The streaming shall be resumed after rebooting. It can be turned
 *  off using the StopMulticastStreaming method. The multicast address, port
 *  and TTL are configured in the VideoEncoderConfiguration, 
 *  AudioEncoderConfiguration and MetadataConfiguration respectively.
 *
 *  Multicast streaming may stop when the corresponding profile is deleted 
 *  or one of its Configurations is altered via one of the set configuration
 *  methods.
 *
 *  The implementation shall ensure that the RTP stream can be decoded without
 *  setting up an RTSP control connection. Especially in case of H.264 video, 
 *  the SPS/PPS header shall be sent inband.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_IncompleteConfiguration
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_StartMulticastStreaming(tr2_StartMulticastStreaming_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->v_enc_cfg == NULL ||
		p_profile->v_enc_cfg->Configuration.Multicast.Port <= 0 ||
		p_profile->v_enc_cfg->Configuration.Multicast.IPv4Address[0] == '\0')
	{
		return ONVIF_ERR_InvalidMulticastSettings;
	}

	p_profile->multicasting = TRUE;
	p_profile->v_enc_cfg->Configuration.Multicast.AutoStart = TRUE;

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Stops multicast streaming using a specified media profile of a device. 
 *  In case a device receives a StopMulticastStreaming request whose 
 *  corresponding multicast streaming is not started, the device should
 *  reply with successful StopMulticastStreamingResponse.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoProfile
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_StopMulticastStreaming(tr2_StopMulticastStreaming_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_profile->multicasting = FALSE;
	if (p_profile->v_enc_cfg)
	{
		p_profile->v_enc_cfg->Configuration.Multicast.AutoStart = FALSE;
	}

	return ONVIF_OK;
}

#ifdef AUDIO_SUPPORT

/************************************************************************************
 *
 * @brief
 *  Modifies a configuration. 
 *  The change may have immediate effect to running streams but the changes
 *  are not guaranteed to take effect unless the client restarts any affected
 *  stream.
 *
 * @returb
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetAudioEncoderConfiguration(tr2_SetAudioEncoderConfiguration_REQ * p_req)
{
	AudioEncoder2ConfigurationList * p_a_enc_cfg = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_req->Configuration.token);
	if (NULL == p_a_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_req->Configuration.SampleRate != 8  && 
		p_req->Configuration.SampleRate != 16 && 
		p_req->Configuration.SampleRate != 24 && 
		p_req->Configuration.SampleRate != 32 &&
		p_req->Configuration.SampleRate != 48 &&
		p_req->Configuration.SampleRate != 64)
	{
		return ONVIF_ERR_ConfigModify;
	}

	p_a_enc_cfg->Configuration.SessionTimeout = p_req->Configuration.SessionTimeout;
	p_a_enc_cfg->Configuration.Bitrate = p_req->Configuration.Bitrate;
	p_a_enc_cfg->Configuration.SampleRate = p_req->Configuration.SampleRate;
	strcpy(p_a_enc_cfg->Configuration.Name, p_req->Configuration.Name);
	strcpy(p_a_enc_cfg->Configuration.Encoding, p_req->Configuration.Encoding);

	if (strcasecmp(p_req->Configuration.Encoding, "G711"))
	{
		p_req->Configuration.AudioEncoding = AudioEncoding_G711;
	}
	else if (strcasecmp(p_req->Configuration.Encoding, "G711A"))
	{
		p_req->Configuration.AudioEncoding = AudioEncoding_G711A;
	}
	else if (strcasecmp(p_req->Configuration.Encoding, "AAC"))
	{
		p_req->Configuration.AudioEncoding = AudioEncoding_AAC;
	}

	memcpy(&p_a_enc_cfg->Configuration.Multicast, &p_req->Configuration.Multicast, sizeof(onvif_MulticastConfiguration));

	onvif_MediaConfigurationChangedNotify(p_req->Configuration.token, "AudioEncoder");

	// todo : here add handler code ...
	AudioConfig AudioCfg;
	memcpy(&AudioCfg, &((MediaConfig *)getMediaConfig())->audioConfig, sizeof(AudioCfg));

	if(AudioEncoding_AAC == onvif_StringToAudioEncoding(p_req->Configuration.Encoding))
	{
		AudioCfg.audioEncode.bitRate = 16000;
		AudioCfg.audioEncode.sampleRate = 16000;
		memset(AudioCfg.audioEncode.audioEncodeType.typeName, 0, 32);
		strncpy(AudioCfg.audioEncode.audioEncodeType.typeName, "AAC", AUDIO_ENCODE_TYPE_MAX_LEN);
	}
	else if(AudioEncoding_G711 == onvif_StringToAudioEncoding(p_req->Configuration.Encoding))
	{
		if (p_req->Configuration.SampleRate == 8 || p_req->Configuration.SampleRate == 16)
			AudioCfg.audioEncode.sampleRate = p_req->Configuration.SampleRate * 1000;
		else
			AudioCfg.audioEncode.sampleRate = 8 * 1000;
		
		AudioCfg.audioEncode.bitRate = 64 * 1000;
		memset(AudioCfg.audioEncode.audioEncodeType.typeName, 0, 32);
		strncpy(AudioCfg.audioEncode.audioEncodeType.typeName, "G.711", AUDIO_ENCODE_TYPE_MAX_LEN);
	}
	else if(AudioEncoding_G711A == onvif_StringToAudioEncoding(p_req->Configuration.Encoding))
	{
		if (p_req->Configuration.SampleRate == 8 || p_req->Configuration.SampleRate == 16)
			AudioCfg.audioEncode.sampleRate = p_req->Configuration.SampleRate * 1000;
		else
			AudioCfg.audioEncode.sampleRate = 8 * 1000;
		
		AudioCfg.audioEncode.bitRate = 64 * 1000;
		memset(AudioCfg.audioEncode.audioEncodeType.typeName, 0, 32);
		strncpy(AudioCfg.audioEncode.audioEncodeType.typeName, "G.711A", AUDIO_ENCODE_TYPE_MAX_LEN);
	}
	AudioCfg.audioEncode.enable = 1;
	anj_config_audio_set(&AudioCfg);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a configuration. 
 *  The change may have immediate effect to running streams but the changes
 *  are not guaranteed to take effect unless the client restarts any affected
 *  stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetAudioSourceConfiguration(tr2_SetAudioSourceConfiguration_REQ * p_req)
{
	ONVIF_RET ret;
	trt_SetAudioSourceConfiguration_REQ req;

	memcpy(&req.Configuration, &p_req->Configuration, sizeof(onvif_AudioSourceConfiguration));
	req.ForcePersistence = TRUE;

	ret = onvif_trt_SetAudioSourceConfiguration(&req);

	if (ONVIF_OK == ret)
	{
		onvif_MediaConfigurationChangedNotify(req.Configuration.token, "AudioSource");
	}

	// todo : here add handler code ...
	
	return ret;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a configuration. 
 *  The change may have immediate effect to running streams but the changes
 *  are not guaranteed to take effect unless the client restarts any affected
 *  stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetAudioDecoderConfiguration(tr2_SetAudioDecoderConfiguration_REQ * p_req)
{
	ONVIF_RET ret;
	trt_SetAudioDecoderConfiguration_REQ req;

	memcpy(&req.Configuration, &p_req->Configuration, sizeof(onvif_AudioDecoderConfiguration));
	req.ForcePersistence = TRUE;

	ret = onvif_trt_SetAudioDecoderConfiguration(&req);

	if (ONVIF_OK == ret)
	{
		onvif_MediaConfigurationChangedNotify(req.Configuration.token, "AudioDecoder");
	}

	// todo : here add handler code ...
	
	return ret;
}

#endif // end of AUDIO_SUPPORT

#ifdef DEVICEIO_SUPPORT

/************************************************************************************
 *
 * @brief
 *  Modifies a configuration. 
 *  The change may have immediate effect to running streams but the changes
 *  are not guaranteed to take effect unless the client restarts any affected
 *  stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_tr2_SetAudioOutputConfiguration(tr2_SetAudioOutputConfiguration_REQ * p_req)
{
	AudioOutputList * p_output;
	AudioOutputConfigurationList * p_cfg = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_req->Configuration.token);
	if (NULL == p_cfg)
	{
		return ONVIF_ERR_NoAudioOutput;
	}

	p_output = onvif_find_AudioOutput(g_onvif_cfg.a_output, p_req->Configuration.OutputToken);
	if (NULL == p_output)
	{
		return ONVIF_ERR_NoAudioOutput;
	}

	if (p_req->Configuration.OutputLevel < p_cfg->Options.OutputLevelRange.Min || p_req->Configuration.OutputLevel > p_cfg->Options.OutputLevelRange.Max)
	{
		return ONVIF_ERR_ConfigModify;
	}

	// todo : here add handler code ...


	strcpy(p_cfg->Configuration.Name, p_req->Configuration.Name);
	strcpy(p_cfg->Configuration.OutputToken, p_req->Configuration.OutputToken);
	if (p_req->Configuration.SendPrimacyFlag)
	{
		strcpy(p_cfg->Configuration.SendPrimacy, p_req->Configuration.SendPrimacy);
	}
	p_cfg->Configuration.OutputLevel = p_req->Configuration.OutputLevel;

	onvif_MediaConfigurationChangedNotify(p_req->Configuration.token, "AudioOutput");

	return ONVIF_OK;
}

#endif // end of DEVICEIO_SUPPORT

ONVIF_RET onvif_hbgk_ext_CreatePrivacyMask(ewsd_CreateMask_REQ * p_req)
{
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_CreatePrivacyMask start !!!!!!!!!!!!!!!!!!!!!!\n");
	MaskList * p_mask;
	VideoSourceConfigurationList * p_v_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->PrivacyMask.VideoSourceToken);
	if (NULL == p_v_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	p_mask = onvif_add_Mask(&g_onvif_cfg.mask, -1);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_MaxMasks;
	}

	if (p_req->PrivacyMask.Polygon.sizePoint <= 0)
	{
		return ONVIF_ERR_InvalidPolygon;
	}

	// return the token
	
	strcpy(p_req->PrivacyMask.token, p_mask->Mask.token);
	memcpy(&p_mask->Mask, &p_req->PrivacyMask, sizeof(onvif_Mask));
	// todo : here add handler code ... 
	
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg, 0, sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));
	// 获取主子码流分辨率宽高
	int iIndex, imgW[2], imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex ++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);
	int mainX = 0, mainY = 0, mainW = 0, mainH = 0, subX = 0, subY = 0, subW = 0, subH = 0;

	float f0x = p_req->PrivacyMask.Polygon.Point[0].x;
	float f0y = p_req->PrivacyMask.Polygon.Point[0].y;
	float f1x = p_req->PrivacyMask.Polygon.Point[1].x;
	float f1y = p_req->PrivacyMask.Polygon.Point[1].y;
	float f2x = p_req->PrivacyMask.Polygon.Point[2].x;
	float f2y = p_req->PrivacyMask.Polygon.Point[2].y;
	float f3x = p_req->PrivacyMask.Polygon.Point[3].x;
	float f3y = p_req->PrivacyMask.Polygon.Point[3].y;

	int Point0x = (int)((f0x + 1) / 2 * imgW[0]);
	int Point1x = (int)((f1x + 1) / 2 * imgW[0]);
	int Point2x = (int)((f2x + 1) / 2 * imgW[0]);
	int Point3x = (int)((f3x + 1) / 2 * imgW[0]);
	int Point0y = (int)((1 - f0y) / 2 * imgH[0]);
	int Point1y = (int)((1 - f1y) / 2 * imgH[0]);
	int Point2y = (int)((1 - f2y) / 2 * imgH[0]);
	int Point3y = (int)((1 - f3y) / 2 * imgH[0]);

	Point0x = (Point0x < Point1x) ? Point0x : Point1x;
	Point0x = (Point0x < Point2x) ? Point0x : Point2x;
	Point0x = (Point0x < Point3x) ? Point0x : Point3x;
	Point0y = (Point0y < Point1y) ? Point0y : Point1y;
	Point0y = (Point0y < Point2y) ? Point0y : Point2y;
	Point0y = (Point0y < Point3y) ? Point0y : Point3y;
	mainX = Point0x;
	mainY = Point0y;
	subX = (int)(mainX * imgW[1] / imgW[0]);
	subY = (int)(mainY * imgH[1] / imgH[0]);
	
	Point0x = (Point0x > Point1x) ? Point0x : Point1x;
	Point0x = (Point0x > Point2x) ? Point0x : Point2x;
	Point0x = (Point0x > Point3x) ? Point0x : Point3x;
	Point0y = (Point0y > Point1y) ? Point0y : Point1y;
	Point0y = (Point0y > Point2y) ? Point0y : Point2y;
	Point0y = (Point0y > Point3y) ? Point0y : Point3y;
	mainW = Point0x - mainX;
	mainH = Point0y - mainY;
	subW = (int)(mainW * imgW[1] / imgW[0]);
	subH = (int)(mainH * imgH[1] / imgH[0]);
	int i = 0;
	if(!strcmp(p_req->PrivacyMask.VideoSourceToken, "VideoSourceConfigurationToken_1")) //VideoSourceConfigurationToken_1
	{
		for(i = 0; i < 4; i ++)
		{
			if((pCfg->mainStreamMaskList[i].xPos == 0)&&
				(pCfg->mainStreamMaskList[i].yPos == 0)&&
				(pCfg->mainStreamMaskList[i].width == 0)&&
				(pCfg->mainStreamMaskList[i].height == 0))
			break;
		}
		
		if (i >= 4)
		{
			i = 3;
		}
		pCfg->mainStreamMaskList[i].xPos = mainX;
		pCfg->mainStreamMaskList[i].yPos = mainY;
		pCfg->mainStreamMaskList[i].width = mainW;
		pCfg->mainStreamMaskList[i].height = mainH;

		pCfg->subStreamMaskList[i].xPos = subX;
		pCfg->subStreamMaskList[i].yPos = subY;
		pCfg->subStreamMaskList[i].width = subW;
		pCfg->subStreamMaskList[i].height = subH;

	}

	anj_config_video_mask_set(pCfg, 0);
	free(pVideoCfg);
	
	//sprintf(p_req->Mask.token, "%d", i);
	
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_CreatePrivacyMask over !!!!!!!!!!!!!!!!!!!!!!\n");
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Deletes a mask configuration.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *
*************************************************************************************/
ONVIF_RET onvif_hbgk_ext_DeletePrivacyMask(ewsd_DeleteMask_REQ * p_req)
{
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_DeletePrivacyMask start !!!!!!!!!!!!!!!!!!!!!!\n");
	MaskList * p_prev;
	MaskList * p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_req->PrivacyMaskToken);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_NoConfig;
	}
	else
	{
		if (g_onvif_idx.mask_idx > 0)
			g_onvif_idx.mask_idx --;
	}
	
	// todo : here add handler code ...
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg, 0, sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);

	int token_int = -1;
	if(!strcmp(p_req->PrivacyMaskToken, "0"))
		token_int = 0;
	else if(!strcmp(p_req->PrivacyMaskToken, "1"))
		token_int = 1;
	else if(!strcmp(p_req->PrivacyMaskToken, "2"))
		token_int = 2;
	else if(!strcmp(p_req->PrivacyMaskToken, "3"))
		token_int = 3;
	
	if (token_int != -1)
	{
		onvif_del_Mask_token(token_int);
		memset(&pCfg->mainStreamMaskList[token_int], 0, sizeof(MASK_AREA_ENTRY));
		memset(&pCfg->subStreamMaskList[token_int], 0, sizeof(MASK_AREA_ENTRY));
	}
	else
	{
		memset(pCfg->mainStreamMaskList, 0, 4 * sizeof(MASK_AREA_ENTRY));
		memset(pCfg->subStreamMaskList, 0, 4 * sizeof(MASK_AREA_ENTRY));
	}
	
	anj_config_video_mask_set(pCfg, 0);
	free(pVideoCfg);


	p_prev = g_onvif_cfg.mask;
	if (p_mask == p_prev)
	{
		g_onvif_cfg.mask = p_mask->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_mask)
				break;
			else
				p_prev = p_prev->next;
		}
		p_prev->next = p_mask->next;
	}

	free(p_mask);
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_DeletePrivacyMask over !!!!!!!!!!!!!!!!!!!!!!\n");
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a mask configuration. Running streams using this configuration
 *  may be immediately updated according to the new settings.
 *
 *  A device signaling support for Mask via its capabilities support this
 *  command. It shall accept any combination of parameters returned by 
 *  GetMaskOptions. If necessary the device may adapt parameter values 
 *  for the Color and Polygon element without returning an error.
 *
 *  Note that for devices signaling SingleColorOnly all masks of the 
 *  associated VideoSource will be updated.
 *  
 *  Note: A device signaling RectangleOnly shall accept any polygon with
 *  four points. In case the four vertices are not defining an exact 
 *  rectangle the device may adjust the vertices.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_InvalidPolygon
 *
*************************************************************************************/
ONVIF_RET onvif_hbgk_ext_SetPrivacyMask(ewsd_SetMask_REQ * p_req)
{
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_SetPrivacyMask start !!!!!!!!!!!!!!!!!!!!!!\n");
	MaskList * p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_req->PrivacyMask.token);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_req->PrivacyMask.VideoSourceToken[0] == '\0' || p_req->PrivacyMask.Polygon.sizePoint <= 0)
	{
		return ONVIF_ERR_InvalidArgVal;
	}
	
	//onvif_renew_Masks();
	// todo : here add handler code ...
	int i_num = -1;
	if(!strcmp(p_req->PrivacyMask.token, "0"))
		i_num = 0;
	else if(!strcmp(p_req->PrivacyMask.token, "1"))
		i_num = 1;
	else if(!strcmp(p_req->PrivacyMask.token, "2"))
		i_num = 2;
	else if(!strcmp(p_req->PrivacyMask.token, "3"))
		i_num = 3;
	
	if(i_num == -1)
	{
		log_print(HT_LOG_INFO, "ter:InvalidArgVal token\n");
		return ONVIF_ERR_InvalidArgVal;
	}
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg,0,sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));

	// 获取主子码流分辨率宽高
	int iIndex,imgW[2],imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);
	int mainX = 0, mainY = 0, mainW = 0, mainH = 0, subX = 0, subY = 0, subW = 0, subH = 0;

	float f0x = p_req->PrivacyMask.Polygon.Point[0].x;
	float f0y = p_req->PrivacyMask.Polygon.Point[0].y;
	float f1x = p_req->PrivacyMask.Polygon.Point[1].x;
	float f1y = p_req->PrivacyMask.Polygon.Point[1].y;
	float f2x = p_req->PrivacyMask.Polygon.Point[2].x;
	float f2y = p_req->PrivacyMask.Polygon.Point[2].y;
	float f3x = p_req->PrivacyMask.Polygon.Point[3].x;
	float f3y = p_req->PrivacyMask.Polygon.Point[3].y;

	int Point0x = (int)((f0x + 1) / 2 * imgW[0]);
	int Point1x = (int)((f1x + 1) / 2 * imgW[0]);
	int Point2x = (int)((f2x + 1) / 2 * imgW[0]);
	int Point3x = (int)((f3x + 1) / 2 * imgW[0]);
	int Point0y = (int)((1 - f0y) / 2 * imgH[0]);
	int Point1y = (int)((1 - f1y) / 2 * imgH[0]);
	int Point2y = (int)((1 - f2y) / 2 * imgH[0]);
	int Point3y = (int)((1 - f3y) / 2 * imgH[0]);

	Point0x = (Point0x < Point1x) ? Point0x : Point1x;
	Point0x = (Point0x < Point2x) ? Point0x : Point2x;
	Point0x = (Point0x < Point3x) ? Point0x : Point3x;
	Point0y = (Point0y < Point1y) ? Point0y : Point1y;
	Point0y = (Point0y < Point2y) ? Point0y : Point2y;
	Point0y = (Point0y < Point3y) ? Point0y : Point3y;
	mainX = Point0x;
	mainY = Point0y;
	subX = (int)(mainX * imgW[1] / imgW[0]);
	subY = (int)(mainY * imgH[1] / imgH[0]);
	
	Point0x = (Point0x > Point1x) ? Point0x : Point1x;
	Point0x = (Point0x > Point2x) ? Point0x : Point2x;
	Point0x = (Point0x > Point3x) ? Point0x : Point3x;
	Point0y = (Point0y > Point1y) ? Point0y : Point1y;
	Point0y = (Point0y > Point2y) ? Point0y : Point2y;
	Point0y = (Point0y > Point3y) ? Point0y : Point3y;
	mainW = Point0x - mainX;
	mainH = Point0y - mainY;
	subW = (int)(mainW * imgW[1] / imgW[0]);
	subH = (int)(mainH * imgH[1] / imgH[0]);
	
	pCfg->mainStreamMaskList[i_num].xPos = mainX;
	pCfg->mainStreamMaskList[i_num].yPos = mainY;
	pCfg->mainStreamMaskList[i_num].width = mainW;
	pCfg->mainStreamMaskList[i_num].height = mainH;

	pCfg->subStreamMaskList[i_num].xPos = subX;
	pCfg->subStreamMaskList[i_num].yPos = subY;
	pCfg->subStreamMaskList[i_num].width = subW;
	pCfg->subStreamMaskList[i_num].height = subH;

	anj_config_video_mask_set(pCfg, 0);
	
	memcpy(&p_mask->Mask, &p_req->PrivacyMask, sizeof(onvif_Mask));
	
	free(pVideoCfg);
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_SetPrivacyMask over !!!!!!!!!!!!!!!!!!!!!!\n");
	return ONVIF_OK;
}

#if 1
ONVIF_RET onvif_tpl_CreatePrivacyMask(tpl_CreateMask_REQ * p_req)
{
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_CreatePrivacyMask start !!!!!!!!!!!!!!!!!!!!!!\n");
	MaskList * p_mask;
	VideoSourceConfigurationList * p_v_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->PrivacyMask.VideoSourceConfigurationToken);
	if (NULL == p_v_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	p_mask = onvif_add_Mask(&g_onvif_cfg.mask, -1);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_MaxMasks;
	}

	if (p_req->PrivacyMask.Polygon.sizePoint <= 0)
	{
		return ONVIF_ERR_InvalidPolygon;
	}

	// return the token
	
	strcpy(p_req->PrivacyMask.token, p_mask->Mask.token);
	memcpy(&p_mask->Mask, &p_req->PrivacyMask, sizeof(onvif_Mask));
	// todo : here add handler code ... 
	
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg, 0, sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));
	// 获取主子码流分辨率宽高
	int iIndex, imgW[2], imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex ++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);
	int mainX = 0, mainY = 0, mainW = 0, mainH = 0, subX = 0, subY = 0, subW = 0, subH = 0;

	float f0x = p_req->PrivacyMask.Polygon.Point[0].x;
	float f0y = p_req->PrivacyMask.Polygon.Point[0].y;
	float f1x = p_req->PrivacyMask.Polygon.Point[1].x;
	float f1y = p_req->PrivacyMask.Polygon.Point[1].y;
	float f2x = p_req->PrivacyMask.Polygon.Point[2].x;
	float f2y = p_req->PrivacyMask.Polygon.Point[2].y;
	float f3x = p_req->PrivacyMask.Polygon.Point[3].x;
	float f3y = p_req->PrivacyMask.Polygon.Point[3].y;

	int Point0x = (int)((f0x + 1) / 2 * imgW[0]);
	int Point1x = (int)((f1x + 1) / 2 * imgW[0]);
	int Point2x = (int)((f2x + 1) / 2 * imgW[0]);
	int Point3x = (int)((f3x + 1) / 2 * imgW[0]);
	int Point0y = (int)((1 - f0y) / 2 * imgH[0]);
	int Point1y = (int)((1 - f1y) / 2 * imgH[0]);
	int Point2y = (int)((1 - f2y) / 2 * imgH[0]);
	int Point3y = (int)((1 - f3y) / 2 * imgH[0]);

	Point0x = (Point0x < Point1x) ? Point0x : Point1x;
	Point0x = (Point0x < Point2x) ? Point0x : Point2x;
	Point0x = (Point0x < Point3x) ? Point0x : Point3x;
	Point0y = (Point0y < Point1y) ? Point0y : Point1y;
	Point0y = (Point0y < Point2y) ? Point0y : Point2y;
	Point0y = (Point0y < Point3y) ? Point0y : Point3y;
	mainX = Point0x;
	mainY = Point0y;
	subX = (int)(mainX * imgW[1] / imgW[0]);
	subY = (int)(mainY * imgH[1] / imgH[0]);
	
	Point0x = (Point0x > Point1x) ? Point0x : Point1x;
	Point0x = (Point0x > Point2x) ? Point0x : Point2x;
	Point0x = (Point0x > Point3x) ? Point0x : Point3x;
	Point0y = (Point0y > Point1y) ? Point0y : Point1y;
	Point0y = (Point0y > Point2y) ? Point0y : Point2y;
	Point0y = (Point0y > Point3y) ? Point0y : Point3y;
	mainW = Point0x - mainX;
	mainH = Point0y - mainY;
	subW = (int)(mainW * imgW[1] / imgW[0]);
	subH = (int)(mainH * imgH[1] / imgH[0]);
	int i = 0;
	if(!strcmp(p_req->PrivacyMask.VideoSourceConfigurationToken, "VideoSourceConfigurationToken_1")) //VideoSourceConfigurationToken_1
	{
		for(i = 0; i < 4; i ++)
		{
			if((pCfg->mainStreamMaskList[i].xPos == 0)&&
				(pCfg->mainStreamMaskList[i].yPos == 0)&&
				(pCfg->mainStreamMaskList[i].width == 0)&&
				(pCfg->mainStreamMaskList[i].height == 0))
			break;
		}
		
		if (i >= 4)
		{
			i = 3;
		}
		pCfg->mainStreamMaskList[i].xPos = mainX;
		pCfg->mainStreamMaskList[i].yPos = mainY;
		pCfg->mainStreamMaskList[i].width = mainW;
		pCfg->mainStreamMaskList[i].height = mainH;

		pCfg->subStreamMaskList[i].xPos = subX;
		pCfg->subStreamMaskList[i].yPos = subY;
		pCfg->subStreamMaskList[i].width = subW;
		pCfg->subStreamMaskList[i].height = subH;

	}

	anj_config_video_mask_set(pCfg, 0);
	free(pVideoCfg);
	
	//sprintf(p_req->Mask.token, "%d", i);
	
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_CreatePrivacyMask over !!!!!!!!!!!!!!!!!!!!!!\n");
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Deletes a mask configuration.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *
*************************************************************************************/
ONVIF_RET onvif_tpl_DeletePrivacyMask(tpl_DeleteMask_REQ * p_req)
{
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_DeletePrivacyMask start !!!!!!!!!!!!!!!!!!!!!!\n");
	MaskList * p_prev;
	MaskList * p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_req->PrivacyMaskToken);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_NoConfig;
	}
	else
	{
		if (g_onvif_idx.mask_idx > 0)
			g_onvif_idx.mask_idx --;
	}
	
	// todo : here add handler code ...
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg, 0, sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);

	int token_int = -1;
	if(!strcmp(p_req->PrivacyMaskToken, "0"))
		token_int = 0;
	else if(!strcmp(p_req->PrivacyMaskToken, "1"))
		token_int = 1;
	else if(!strcmp(p_req->PrivacyMaskToken, "2"))
		token_int = 2;
	else if(!strcmp(p_req->PrivacyMaskToken, "3"))
		token_int = 3;
	
	if (token_int != -1)
	{
		onvif_del_Mask_token(token_int);
		memset(&pCfg->mainStreamMaskList[token_int], 0, sizeof(MASK_AREA_ENTRY));
		memset(&pCfg->subStreamMaskList[token_int], 0, sizeof(MASK_AREA_ENTRY));
	}
	else
	{
		memset(pCfg->mainStreamMaskList, 0, 4 * sizeof(MASK_AREA_ENTRY));
		memset(pCfg->subStreamMaskList, 0, 4 * sizeof(MASK_AREA_ENTRY));
	}
	
	anj_config_video_mask_set(pCfg, 0);
	free(pVideoCfg);


	p_prev = g_onvif_cfg.mask;
	if (p_mask == p_prev)
	{
		g_onvif_cfg.mask = p_mask->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_mask)
				break;
			else
				p_prev = p_prev->next;
		}
		p_prev->next = p_mask->next;
	}

	free(p_mask);
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_DeletePrivacyMask over !!!!!!!!!!!!!!!!!!!!!!\n");
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a mask configuration. Running streams using this configuration
 *  may be immediately updated according to the new settings.
 *
 *  A device signaling support for Mask via its capabilities support this
 *  command. It shall accept any combination of parameters returned by 
 *  GetMaskOptions. If necessary the device may adapt parameter values 
 *  for the Color and Polygon element without returning an error.
 *
 *  Note that for devices signaling SingleColorOnly all masks of the 
 *  associated VideoSource will be updated.
 *  
 *  Note: A device signaling RectangleOnly shall accept any polygon with
 *  four points. In case the four vertices are not defining an exact 
 *  rectangle the device may adjust the vertices.
 *
 * @return
 *  Possible error:
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_InvalidPolygon
 *
*************************************************************************************/
ONVIF_RET onvif_tpl_SetPrivacyMask(tpl_SetMask_REQ * p_req)
{
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_SetPrivacyMask start !!!!!!!!!!!!!!!!!!!!!!\n");
	MaskList * p_mask = onvif_find_Mask(g_onvif_cfg.mask, p_req->PrivacyMask.token);
	if (NULL == p_mask)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_req->PrivacyMask.VideoSourceConfigurationToken[0] == '\0' || p_req->PrivacyMask.Polygon.sizePoint <= 0)
	{
		return ONVIF_ERR_InvalidArgVal;
	}
	
	//onvif_renew_Masks();
	// todo : here add handler code ...
	int i_num = -1;
	if(!strcmp(p_req->PrivacyMask.token, "0"))
		i_num = 0;
	else if(!strcmp(p_req->PrivacyMask.token, "1"))
		i_num = 1;
	else if(!strcmp(p_req->PrivacyMask.token, "2"))
		i_num = 2;
	else if(!strcmp(p_req->PrivacyMask.token, "3"))
		i_num = 3;
	
	if(i_num == -1)
	{
		log_print(HT_LOG_INFO, "ter:InvalidArgVal token\n");
		return ONVIF_ERR_InvalidArgVal;
	}
	VideoConfig *pVideoCfg = (VideoConfig *)malloc(sizeof(VideoConfig));
	memset(pVideoCfg,0,sizeof(VideoConfig));
	memcpy(pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(*pVideoCfg));

	// 获取主子码流分辨率宽高
	int iIndex,imgW[2],imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);
	int mainX = 0, mainY = 0, mainW = 0, mainH = 0, subX = 0, subY = 0, subW = 0, subH = 0;

	float f0x = p_req->PrivacyMask.Polygon.Point[0].x;
	float f0y = p_req->PrivacyMask.Polygon.Point[0].y;
	float f1x = p_req->PrivacyMask.Polygon.Point[1].x;
	float f1y = p_req->PrivacyMask.Polygon.Point[1].y;
	float f2x = p_req->PrivacyMask.Polygon.Point[2].x;
	float f2y = p_req->PrivacyMask.Polygon.Point[2].y;
	float f3x = p_req->PrivacyMask.Polygon.Point[3].x;
	float f3y = p_req->PrivacyMask.Polygon.Point[3].y;

	int Point0x = (int)((f0x + 1) / 2 * imgW[0]);
	int Point1x = (int)((f1x + 1) / 2 * imgW[0]);
	int Point2x = (int)((f2x + 1) / 2 * imgW[0]);
	int Point3x = (int)((f3x + 1) / 2 * imgW[0]);
	int Point0y = (int)((1 - f0y) / 2 * imgH[0]);
	int Point1y = (int)((1 - f1y) / 2 * imgH[0]);
	int Point2y = (int)((1 - f2y) / 2 * imgH[0]);
	int Point3y = (int)((1 - f3y) / 2 * imgH[0]);

	Point0x = (Point0x < Point1x) ? Point0x : Point1x;
	Point0x = (Point0x < Point2x) ? Point0x : Point2x;
	Point0x = (Point0x < Point3x) ? Point0x : Point3x;
	Point0y = (Point0y < Point1y) ? Point0y : Point1y;
	Point0y = (Point0y < Point2y) ? Point0y : Point2y;
	Point0y = (Point0y < Point3y) ? Point0y : Point3y;
	mainX = Point0x;
	mainY = Point0y;
	subX = (int)(mainX * imgW[1] / imgW[0]);
	subY = (int)(mainY * imgH[1] / imgH[0]);
	
	Point0x = (Point0x > Point1x) ? Point0x : Point1x;
	Point0x = (Point0x > Point2x) ? Point0x : Point2x;
	Point0x = (Point0x > Point3x) ? Point0x : Point3x;
	Point0y = (Point0y > Point1y) ? Point0y : Point1y;
	Point0y = (Point0y > Point2y) ? Point0y : Point2y;
	Point0y = (Point0y > Point3y) ? Point0y : Point3y;
	mainW = Point0x - mainX;
	mainH = Point0y - mainY;
	subW = (int)(mainW * imgW[1] / imgW[0]);
	subH = (int)(mainH * imgH[1] / imgH[0]);
	
	pCfg->mainStreamMaskList[i_num].xPos = mainX;
	pCfg->mainStreamMaskList[i_num].yPos = mainY;
	pCfg->mainStreamMaskList[i_num].width = mainW;
	pCfg->mainStreamMaskList[i_num].height = mainH;

	pCfg->subStreamMaskList[i_num].xPos = subX;
	pCfg->subStreamMaskList[i_num].yPos = subY;
	pCfg->subStreamMaskList[i_num].width = subW;
	pCfg->subStreamMaskList[i_num].height = subH;

	anj_config_video_mask_set(pCfg, 0);
	
	memcpy(&p_mask->Mask, &p_req->PrivacyMask, sizeof(onvif_Mask));
	
	free(pVideoCfg);
	log_print(HT_LOG_INFO, "onvif_hbgk_ext_SetPrivacyMask over !!!!!!!!!!!!!!!!!!!!!!\n");
	return ONVIF_OK;
}
#endif
ONVIF_RET onvif_tpl_SendCommand(tpl_SendCommand_REQ * p_req)
{
	log_print(HT_LOG_INFO, "onvif_tpl_SendCommand\n");
	g_motion_mode = 0;
	if(p_req->SendCommand.ProfileTokenFlag && p_req->SendCommand.TokenFlag && p_req->SendCommand.CommandDataFlag)
	{
		if (!strcmp(p_req->SendCommand.Token, "motion_enable_plan"))
		{
			MotionDetectAlarm md_alarm;
			memset(&md_alarm, 0, sizeof(MotionDetectAlarm));
			memcpy(&md_alarm, &((AlarmConfig *)getAlarmConfig())->normalAlarm.motionDetectAlarm[0], sizeof(md_alarm));
			MotionDetectAlarm md_alarm_old;
			memset(&md_alarm_old, 0, sizeof(MotionDetectAlarm));
			memcpy(&md_alarm_old, &((AlarmConfig *)getAlarmConfig())->normalAlarm.motionDetectAlarm[0], sizeof(md_alarm_old));
			
			log_print(HT_LOG_INFO, "set __tpl__SendCommand motion_enable_plan\n");
			if (p_req->SendCommand.CommandData == TRUE)
			{
				md_alarm.enable = 1;
				log_print(HT_LOG_INFO, "set __tpl__SendCommand true\n");
			}
			else
			{
				md_alarm.enable = 0;
				log_print(HT_LOG_INFO, "set __tpl__SendCommand flase\n");
			}
			
			if (memcmp(&md_alarm_old, &md_alarm, sizeof(MotionDetectAlarm)) != 0)
			{
				anj_config_alarm_motion_set(&md_alarm);
			}
		}
	}
	return ONVIF_OK;
}

ONVIF_RET onvif_tpl_SetImagingSettingDefault(tpl_SetImagingSettingDefault_REQ * p_req)
{
	log_print(HT_LOG_INFO, "__tpl__SetImagingSettingDefault\n");
	char szSrcFile[128] = {0};
	GlobalConfig *pConfig = (GlobalConfig*)malloc(sizeof(GlobalConfig));
	if( anj_config_get_default(szSrcFile, sizeof(szSrcFile), pConfig) == 0 )
	{
		VideoConfig videoConfig;
		memcpy(&videoConfig, &((MediaConfig *)getMediaConfig())->videoConfig[0], sizeof(videoConfig));
		VideoCaptureCfg *pCaptureCfg = (VideoCaptureCfg *)&videoConfig.videoCapture;

		float brightness = (float)(pConfig->mediaCfg.videoConfig[0].videoCapture.brightness);
		float saturation = (float)(pConfig->mediaCfg.videoConfig[0].videoCapture.saturation);
		float contrast = (float)(pConfig->mediaCfg.videoConfig[0].videoCapture.contrast);
		float sharpness = (float)(pConfig->mediaCfg.videoConfig[0].videoCapture.sharpness);
		float NoiseReduction_level = (float)(pConfig->mediaCfg.videoConfig[0].videoCapture.tnf)/255;
		
		int ExposureMode = pConfig->mediaCfg.videoConfig[0].videoCapture.shutterSetting.shutter_mode_day != 0?1:0;
		int Exposure_time = pConfig->mediaCfg.videoConfig[0].videoCapture.shutterSetting.shutter_speed_day;
		int IrCutFilterMode_t = pConfig->mediaCfg.videoConfig[0].videoCapture.ircut_mode;
		
		int WhiteBalance_Mode = 0;
		if((pConfig->mediaCfg.videoConfig[0].videoCapture.whitebalance>>24)&0xff)
			WhiteBalance_Mode = 1;
		else
			WhiteBalance_Mode = 0;
		int WhiteBalance_CrGain = (pConfig->mediaCfg.videoConfig[0].videoCapture.whitebalance>>16)&0xff;
		int WhiteBalance_CbGain = (pConfig->mediaCfg.videoConfig[0].videoCapture.whitebalance)&0xff;
		
		int Defog_mode = pConfig->mediaCfg.videoConfig[0].videoCapture.dfrog_flag;
		float Defog_value = (float)(pConfig->mediaCfg.videoConfig[0].videoCapture.dfrog_value);

		if (1)
		{
			pCaptureCfg->brightness = brightness;
			pCaptureCfg->contrast	= contrast;
			pCaptureCfg->sharpness	= sharpness;
			pCaptureCfg->saturation = saturation;

			pCaptureCfg->tnf = (int)(NoiseReduction_level*255);

			pCaptureCfg->shutterSetting.shutter_mode_day = pCaptureCfg->shutterSetting.shutter_mode_night = ExposureMode != 0?1:0;
			if(ExposureMode != 0 && (Exposure_time >= 10 && Exposure_time <= 10000))
				pCaptureCfg->shutterSetting.shutter_speed_day = pCaptureCfg->shutterSetting.shutter_speed_night = Exposure_time;

			if(IrCutFilterMode_t >= 0)
			{
				if (access("/opt/ch/onvif_control_ircut", F_OK) == F_OK)
				{
					if(IrCutFilterMode_t == 2)//AUTO
					{
						pCaptureCfg->ircut_keepcolor = 0;
						pCaptureCfg->ircut_mode = IRCUT_Mode_Passive;
					}
					else
					{
						pCaptureCfg->ircut_mode = IRCUT_Mode_Manual;
						if(IrCutFilterMode_t == 1)//OFF黑夜
						{
							pCaptureCfg->ircut_keepcolor = 0;
							//MsgSetIRCUTControl(0);
					
						}
						else//OFF白天
						{
							pCaptureCfg->ircut_keepcolor = 1;
							//MsgSetIRCUTControl(1);
						}
					}
				}
				else
					log_print(HT_LOG_INFO, "/opt/ch/onvif_control_ircut nothingness\n");
			}

			if( WhiteBalance_Mode == 0)		
			{	
				pCaptureCfg->whitebalance &= ~(0xff<<24);
			}
			else
			{
				//enable=(whitebalance>>24)&0xff; R=(whitebalance>>16)&0xff; G=(whitebalance>>8)&0xff; B=(whitebalance)&0xff
				if(WhiteBalance_CrGain < 0)
					WhiteBalance_CrGain = 0;
				else if(WhiteBalance_CrGain > 255)
					WhiteBalance_CrGain = 255;
				
				if(WhiteBalance_CbGain < 0)
					WhiteBalance_CbGain = 0;
				else if(WhiteBalance_CbGain > 255)
					WhiteBalance_CbGain = 255;

				pCaptureCfg->whitebalance &= ~(0xff<<24 | 0xff<<16 | 0xff);
				pCaptureCfg->whitebalance |= (1<<24);
				pCaptureCfg->whitebalance |= ((unsigned char)WhiteBalance_CrGain<<16);
				pCaptureCfg->whitebalance |= ((unsigned char)WhiteBalance_CbGain);
			}
			
			/* 设置去雾 */
			pCaptureCfg->dfrog_flag = Defog_mode != 0 ? 1 : 0;
			if(pCaptureCfg->dfrog_flag != 0)
				pCaptureCfg->dfrog_value = (int)(Defog_value * 255);
		}
		anj_config_video_capture_set(pCaptureCfg, 0);
	}

	
	return ONVIF_OK;
}

#endif // MEDIA2_SUPPORT



