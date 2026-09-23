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

#include "sys_inc.h"
#include "onvif.h"
#include "onvif_media.h"
#include "onvif_util.h"

/***************************************************************************************/

extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;


/***************************************************************************************/
ONVIF_RET onvif_CreateProfile(CreateProfile_REQ * p_CreateProfile_req)
{
	ONVIF_PROFILE * p_profile = NULL;

	if (p_CreateProfile_req->token[0] != '\0')
	{
		p_profile = onvif_find_profile(p_CreateProfile_req->token);
		if (p_profile)
		{
			return ONVIF_ERR_PROFILE_EXISTS;
		}
	}
	
	p_profile = onvif_add_profile(FALSE);
	if (p_profile)
	{
		strcpy(p_profile->name, p_CreateProfile_req->name);
		if (p_CreateProfile_req->token[0] != '\0')
		{
			strcpy(p_profile->token, p_CreateProfile_req->token);
		}
		else
		{
			strcpy(p_CreateProfile_req->token, p_profile->token);
		}

		if (g_onvif_cfg.profiles)
		{
			strcpy(p_profile->stream_uri, g_onvif_cfg.profiles->stream_uri);
		}
		else
		{
			sprintf(p_profile->stream_uri, "rtsp://%s:%d/test.264", g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
		}		
	}
	else 
	{
		return ONVIF_ERR_MAX_NVT_PROFILES;
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_DeleteProfile(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (p_profile->fixed)
	{
		return ONVIF_ERR_DEL_FIX_PROFILE;
	}

	ONVIF_PROFILE * p_prev = g_onvif_cfg.profiles;
	if (p_profile == p_prev)
	{
		g_onvif_cfg.profiles = p_profile->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_profile)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next =  p_profile->next;
	}

	if (p_profile->video_src && p_profile->video_src->use_count > 0)
	{
		p_profile->video_src->use_count--;
	}
	
	if (p_profile->video_enc && p_profile->video_enc->use_count > 0)
	{
		p_profile->video_enc->use_count--;
	}

	if (p_profile->audio_src && p_profile->audio_src->use_count > 0)
	{
		p_profile->audio_src->use_count--;
	}

	if (p_profile->audio_enc && p_profile->audio_enc->use_count > 0)
	{
		p_profile->audio_enc->use_count--;
	}

	free(p_profile);
	
	return ONVIF_OK;
}

ONVIF_RET onvif_AddVideoSourceConfiguration(AddVideoSourceConfiguration_REQ * p_AddVideoSourceConfiguration_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_AddVideoSourceConfiguration_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	ONVIF_V_SRC * p_v_src = onvif_find_video_source(p_AddVideoSourceConfiguration_req->config_token);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	if (p_profile->video_src != p_v_src)
	{
		if (p_profile->video_src && p_profile->video_src->use_count > 0)
		{
			p_profile->video_src->use_count--;
		}
		
		p_v_src->use_count++;
		p_profile->video_src = p_v_src;
	}

	return ONVIF_OK;
}

ONVIF_RET onvif_AddVideoEncoderConfiguration(AddVideoEncoderConfiguration_REQ * p_AddVideoEncoderConfiguration_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_AddVideoEncoderConfiguration_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	ONVIF_V_ENC * p_v_enc = onvif_find_video_encoder(p_AddVideoEncoderConfiguration_req->config_token);
	if (NULL == p_v_enc)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	if (p_profile->video_enc != p_v_enc)
	{
		if (p_profile->video_enc && p_profile->video_enc->use_count > 0)
		{
			p_profile->video_enc->use_count--;
		}
		
		p_v_enc->use_count++;
		p_profile->video_enc = p_v_enc;
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_AddAudioSourceConfiguration(AddAudioSourceConfiguration_REQ * p_AddAudioSourceConfiguration_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_AddAudioSourceConfiguration_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	ONVIF_A_SRC * p_a_src = onvif_find_audio_source(p_AddAudioSourceConfiguration_req->config_token);
	if (NULL == p_a_src)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	if (p_profile->audio_src != p_a_src)
	{
		if (p_profile->audio_src && p_profile->audio_src->use_count > 0)
		{
			p_profile->audio_src->use_count--;
		}
		
		p_a_src->use_count++;
		p_profile->audio_src = p_a_src;
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_AddAudioEncoderConfiguration(AddAudioEncoderConfiguration_REQ * p_AddAudioEncoderConfiguration_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_AddAudioEncoderConfiguration_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	ONVIF_A_ENC * p_a_enc = onvif_find_audio_encoder(p_AddAudioEncoderConfiguration_req->config_token);
	if (NULL == p_a_enc)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	if (p_profile->audio_enc != p_a_enc)
	{
		if (p_profile->audio_enc && p_profile->audio_enc->use_count > 0)
		{
			p_profile->audio_enc->use_count--;
		}
		
		p_a_enc->use_count++;
		p_profile->audio_enc = p_a_enc;
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_RemoveVideoEncoderConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (p_profile->fixed)
	{
		return ONVIF_ERR_DEL_FIX_PROFILE;
	}

	if (p_profile->video_enc && p_profile->video_enc->use_count > 0)
	{
		p_profile->video_enc->use_count--;
	}
	
	p_profile->video_enc = NULL;

	return ONVIF_OK;
}

ONVIF_RET onvif_RemoveVideoSourceConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (p_profile->fixed)
	{
		return ONVIF_ERR_DEL_FIX_PROFILE;
	}

	if (p_profile->video_src && p_profile->video_src->use_count > 0)
	{
		p_profile->video_src->use_count--;
	}
	
	p_profile->video_src = NULL;

	return ONVIF_OK;
}


ONVIF_RET onvif_RemoveAudioEncoderConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (p_profile->fixed)
	{
		return ONVIF_ERR_DEL_FIX_PROFILE;
	}

	if (p_profile->audio_enc && p_profile->audio_enc->use_count > 0)
	{
		p_profile->audio_enc->use_count--;
	}
	
	p_profile->audio_enc = NULL;

	return ONVIF_OK;
}

ONVIF_RET onvif_RemoveAudioSourceConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	if (p_profile->fixed)
	{
		return ONVIF_ERR_DEL_FIX_PROFILE;
	}

	if (p_profile->audio_src && p_profile->audio_src->use_count > 0)
	{
		p_profile->audio_src->use_count--;
	}
	
	p_profile->audio_src = NULL;

	return ONVIF_OK;
}

ONVIF_RET onvif_AddPTZConfiguration(AddPTZConfiguration_REQ * p_AddPTZConfiguration_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(p_AddPTZConfiguration_req->profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	PTZ_NODE * p_ptz_node = onvif_find_ptz_cfg(p_AddPTZConfiguration_req->config_token);
	if (NULL == p_ptz_node)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	p_profile->ptz_node = p_ptz_node;

	return ONVIF_OK;
}

ONVIF_RET onvif_RemovePTZConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	p_profile->ptz_node = NULL;

	return ONVIF_OK;
}

/************************************************************************************
 *  	
 * Possible error:
 * 	ONVIF_ERR_CONFIG_MODIFY
 *	ONVIF_ERR_NO_CONFIG
 * 	ONVIF_ERR_CONFIGURATION_CONFLICT
 *
*************************************************************************************/
ONVIF_RET onvif_SetVideoEncoderConfiguration(SetVideoEncoderConfiguration_REQ * p_SetVideoEncoderConfiguration_req)
{
	ONVIF_V_ENC * p_v_enc = onvif_find_video_encoder(p_SetVideoEncoderConfiguration_req->token);
	if (NULL == p_v_enc)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	VideoEncode pCfg;
	memset(&pCfg, 0, sizeof(VideoEncode));
	MsgGetVideoEncodeConfig(&pCfg);

	VideoCapture videoCfg;
	memset(&videoCfg, 0, sizeof(VideoCapture));
	MsgGetVideoCaptureConfig(&videoCfg);
	
	int stream=0;



	if( 0 == strcmp(p_SetVideoEncoderConfiguration_req->token, "VideoEncodeMain")) //MainStream
	{
		stream = 0;
	}
	else if( 0 == strcmp(p_SetVideoEncoderConfiguration_req->token, "VideoEncodeSub"))//SubStream
	{
		stream = 1;
	}
	else if( 0 == strcmp(p_SetVideoEncoderConfiguration_req->token, "VideoEncodeThird"))//ThirdStream
	{
		stream = 2;
	}
	else
	{
		return ONVIF_ERR_NO_CONFIG;
	}
	
	int i = 0;
	
	for (i = 0; i < MAX_RES_NUMS; i++)
	{
		if (g_onvif_cfg.video_enc_cfg.resolution[i].w == p_SetVideoEncoderConfiguration_req->width && 
			g_onvif_cfg.video_enc_cfg.resolution[i].h == p_SetVideoEncoderConfiguration_req->height)
		{
			break;
		}
	}

	if (i == MAX_RES_NUMS)
	{
		return ONVIF_ERR_CONFIG_MODIFY;
	}

	if (p_SetVideoEncoderConfiguration_req->quality < g_onvif_cfg.video_enc_cfg.quality_min || 
		p_SetVideoEncoderConfiguration_req->quality > g_onvif_cfg.video_enc_cfg.quality_max)
	{
		return ONVIF_ERR_CONFIG_MODIFY;
	}
		
	RESOLUTION_ENTRY *pEntry = NULL;
	if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_H264)
	{
		pEntry = GetResolution(const_cast<char*>("H264"), p_SetVideoEncoderConfiguration_req->width, p_SetVideoEncoderConfiguration_req->height, videoCfg.tvsystem, stream);
	}
	else if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_H265)
	{
		pEntry = GetResolution(const_cast<char*>("H265"), p_SetVideoEncoderConfiguration_req->width, p_SetVideoEncoderConfiguration_req->height, videoCfg.tvsystem, stream);
	}
	else if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_JPEG)
	{
		pEntry = GetResolution(const_cast<char*>("MJPEG"), p_SetVideoEncoderConfiguration_req->width, p_SetVideoEncoderConfiguration_req->height, videoCfg.tvsystem, stream);
	}
	if( pEntry == NULL )
	{
		printf("Cannot get resolution for %d X %d", p_SetVideoEncoderConfiguration_req->width, p_SetVideoEncoderConfiguration_req->height);
		return ONVIF_ERR_CONFIG_MODIFY;
	}
	
	if( p_SetVideoEncoderConfiguration_req->framerate_limit >  pEntry->max_framerate )
		pCfg.encodeCfg[stream].frameRate = pEntry->max_framerate ;
	else if (p_SetVideoEncoderConfiguration_req->framerate_limit<  pEntry->min_framerate )
		pCfg.encodeCfg[stream].frameRate = pEntry->def_framerate;
	else
		pCfg.encodeCfg[stream].frameRate = p_SetVideoEncoderConfiguration_req->framerate_limit;
	
	pCfg.encodeCfg[stream].display_frameRate = pCfg.encodeCfg[stream].frameRate;

	if( p_SetVideoEncoderConfiguration_req->bitrate_limit >  pEntry->max_bitrate )
		pCfg.encodeCfg[stream].bitRate = pEntry->max_bitrate ;
	else if (p_SetVideoEncoderConfiguration_req->bitrate_limit<  pEntry->min_bitrate )
		pCfg.encodeCfg[stream].bitRate = pEntry->min_bitrate;
	else
		pCfg.encodeCfg[stream].bitRate = p_SetVideoEncoderConfiguration_req->bitrate_limit;
	
	pCfg.encodeCfg[stream].bitRateQuality=  VIDEO_QUALITY_CUSTOM;

	if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_H264)
	{
		strcpy(pCfg.encodeCfg[stream].encodeFormat.name,"H264");
	}
	else if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_H265)
	{
		strcpy(pCfg.encodeCfg[stream].encodeFormat.name,"H265");
	}
	else if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_JPEG)
	{
		strcpy(pCfg.encodeCfg[stream].encodeFormat.name,"MJPEG");
	}
	
	if (p_SetVideoEncoderConfiguration_req->gov_len > 0)
	{
		pCfg.encodeCfg[stream].initQuant = p_SetVideoEncoderConfiguration_req->gov_len;
	}
	else if (p_SetVideoEncoderConfiguration_req->framerate_limit > 0)
	{
		pCfg.encodeCfg[stream].initQuant = p_SetVideoEncoderConfiguration_req->framerate_limit * 4;
	}

	/*	if (p_SetVideoEncoderConfiguration_req->)
		strcpy(pCfg.encodeCfg[stream].bitRateControl.name,"CBR");
	else
		strcpy(pCfg.encodeCfg[stream].bitRateControl.name,"VBR");*/
	
	strcpy(pCfg.encodeCfg[stream].resolution.name, pEntry->res_name);
	
	int ret = MsgSetVideoEncodeConfig(&pCfg);
	if (ret != 0)
	{
		printf("MsgSetVideoEncodeConfig fail(%d)\n", ret);
	}
	else
	{
		printf("MsgSetVideoEncodeConfig success\n");
	}
	
	p_v_enc->width = p_SetVideoEncoderConfiguration_req->width;
	p_v_enc->height = p_SetVideoEncoderConfiguration_req->height;
	p_v_enc->quality = p_SetVideoEncoderConfiguration_req->quality;
	p_v_enc->session_timeout = p_SetVideoEncoderConfiguration_req->session_timeout;
	p_v_enc->encoding = p_SetVideoEncoderConfiguration_req->encoding;
	p_v_enc->gov_len = p_SetVideoEncoderConfiguration_req->gov_len;
	p_v_enc->profile = p_SetVideoEncoderConfiguration_req->profile;

	if (p_SetVideoEncoderConfiguration_req->framerate_limit != 0)
	{
		p_v_enc->framerate_limit = p_SetVideoEncoderConfiguration_req->framerate_limit;
	}

	if (p_SetVideoEncoderConfiguration_req->encoding_interval != 0)
	{
		p_v_enc->encoding_interval = p_SetVideoEncoderConfiguration_req->encoding_interval;
	}

	if (p_SetVideoEncoderConfiguration_req->bitrate_limit != 0)
	{
		p_v_enc->bitrate_limit = p_SetVideoEncoderConfiguration_req->bitrate_limit;
	}
	
	return ONVIF_OK;
}

/************************************************************************************
 *  	
 * Possible error:
 * 	ONVIF_ERR_CONFIG_MODIFY
 *	ONVIF_ERR_NO_CONFIG
 * 	ONVIF_ERR_CONFIGURATION_CONFLICT
 *
*************************************************************************************/
ONVIF_RET onvif_SetVideoSourceConfiguration(SetVideoSourceConfiguration_REQ * p_SetVideoSourceConfiguration_req)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(p_SetVideoSourceConfiguration_req->config_token);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_CONFIG;
	}
	
	p_v_src = onvif_find_video_source(p_SetVideoSourceConfiguration_req->source_token);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	if (p_SetVideoSourceConfiguration_req->x < g_onvif_cfg.video_src_cfg.x_min || 
		p_SetVideoSourceConfiguration_req->x > g_onvif_cfg.video_src_cfg.x_max ||
		p_SetVideoSourceConfiguration_req->y < g_onvif_cfg.video_src_cfg.y_min || 
		p_SetVideoSourceConfiguration_req->y > g_onvif_cfg.video_src_cfg.y_max ||
		p_SetVideoSourceConfiguration_req->width < g_onvif_cfg.video_src_cfg.w_min || 
		p_SetVideoSourceConfiguration_req->width > g_onvif_cfg.video_src_cfg.w_max ||
		p_SetVideoSourceConfiguration_req->height < g_onvif_cfg.video_src_cfg.h_min || 
		p_SetVideoSourceConfiguration_req->height > g_onvif_cfg.video_src_cfg.h_max)
	{
		return ONVIF_ERR_CONFIG_MODIFY;
	}

	p_v_src->x = p_SetVideoSourceConfiguration_req->x;
	p_v_src->y = p_SetVideoSourceConfiguration_req->y;
	p_v_src->width = p_SetVideoSourceConfiguration_req->width;
	p_v_src->height = p_SetVideoSourceConfiguration_req->height;
	// p_v_src->use_count = p_SetVideoSourceConfiguration_req->use_count;

	strcpy(p_v_src->name, p_SetVideoSourceConfiguration_req->name);

	return ONVIF_OK;
}


ONVIF_RET onvif_SetAudioSourceConfiguration(SetAudioSourceConfiguration_REQ * p_SetAudioSourceConfiguration_req)
{
	ONVIF_A_SRC * p_a_src = onvif_find_audio_source(p_SetAudioSourceConfiguration_req->config_token);
	if (NULL == p_a_src)
	{
		return ONVIF_ERR_NO_CONFIG;
	}
	
	p_a_src = onvif_find_audio_source(p_SetAudioSourceConfiguration_req->source_token);
	if (NULL == p_a_src)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	// p_v_src->use_count = p_SetVideoSourceConfiguration_req->use_count;

	strcpy(p_a_src->name, p_SetAudioSourceConfiguration_req->name);

	return ONVIF_OK;
}

ONVIF_RET onvif_SetAudioEncoderConfiguration(SetAudioEncoderConfiguration_REQ * P_SetAudioEncoderConfiguration_req)
{
	ONVIF_A_ENC * p_a_enc = onvif_find_audio_encoder(P_SetAudioEncoderConfiguration_req->token);
	if (NULL == p_a_enc)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	if (P_SetAudioEncoderConfiguration_req->sample_rate != 8  && 
		P_SetAudioEncoderConfiguration_req->sample_rate != 12 && 
		P_SetAudioEncoderConfiguration_req->sample_rate != 25 && 
		P_SetAudioEncoderConfiguration_req->sample_rate != 32 &&
		P_SetAudioEncoderConfiguration_req->sample_rate != 48)
	{
		return ONVIF_ERR_CONFIG_MODIFY;
	}

	p_a_enc->session_timeout = P_SetAudioEncoderConfiguration_req->session_timeout;
	p_a_enc->bitrate = P_SetAudioEncoderConfiguration_req->bitrate;
	p_a_enc->sample_rate = P_SetAudioEncoderConfiguration_req->sample_rate;
	p_a_enc->encoding = P_SetAudioEncoderConfiguration_req->encoding;

	return ONVIF_OK;
}





