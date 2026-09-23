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
#include "onvif_device.h"
#include "xml_node.h"
#include "onvif_event.h"
#include "onvif_util.h"

/***************************************************************************************/

ONVIF_CFG g_onvif_cfg;
OnvifOemStruct g_OemInfo;

ONVIF_CLS g_onvif_cls;
MediaStreamConfig gStreamCfg;
MediaConfig	gMediaCfg;
char g_SN[20] = {0};
char g_uuid[16] = {0};
int g_ExistWifi = 0;
WIFIConfig   g_wifiCfg;
WIFIApConfig g_wifiapCfg;
int g_product_type = PRODUCT_TYPE_BASE;

int g_bWTDVersion = 0;      //�������ص�T25G�ж�

/***************************************************************************************/
const char * onvif_get_video_encoding_str(E_VIDEO_ENCODING encoding)
{
	switch (encoding)
	{
	case VIDEO_ENCODING_JPEG:
		return "JPEG";

	case VIDEO_ENCODING_MPEG4:
		return "MPEG4";

	case VIDEO_ENCODING_H264:
		return "H264";

	case VIDEO_ENCODING_H265:
		return "H265";
	}

	return "H264";
}

const char * onvif_get_audio_encoding_str(E_AUDIO_ENCODING encoding)
{
	switch (encoding)
	{
	case AUDIO_ENCODING_G711:
		return "G711";

	case AUDIO_ENCODING_G726:
		return "G726";

	case AUDIO_ENCODING_AAC:
		return "AAC";

	case AUDIO_ENCODING_G711A:
		return "G711A";
	}

	return "G711";
}

#if 1
const char * onvif_get_h264_profile_str(E_H264_PROFILE profile)
{
	switch (profile)
	{
	case H264_PROFILE_Baseline:
		return "Baseline";

	case H264_PROFILE_Extended:
		return "Extended";

	case H264_PROFILE_High:
		return "High";

	case H264_PROFILE_Main:
		return "Main";
	}

	return "Baseline";
}

const char * onvif_get_mpeg4_profile_str(E_MPEG4_PROFILE profile)
{
	switch (profile)
	{
	case MPEG4_PROFILE_SP:
		return "SP";

	case MPEG4_PROFILE_ASP:
		return "ASP";    
	}

	return "SP";
}


const char * onvif_get_ptz_status_str(E_PTZ_STATUS ptz_status)
{
	if (PTZ_STA_IDLE == ptz_status)
	{
		return "IDLE";
	}
	else if (PTZ_STA_MOVING == ptz_status)
	{
		return "MOVING";
	}
	else
	{
		return "UNKNOWN";
	}
}


E_CAP_CATEGORY onvif_get_cap_category(const char * cateory)
{
	if (NULL == cateory)
	{
		return CAP_CATEGORY_INVALID;
	}
	
	if (strcasecmp(cateory, "Media") == 0)
	{
		return CAP_CATEGORY_MEDIA;
	}
	else if (strcasecmp(cateory, "Device") == 0)
	{
		return CAP_CATEGORY_DEVICE;
	}
	else if (strcasecmp(cateory, "Analytics") == 0)
	{
		return CAP_CATEGORY_ANALYTICS;
	}
	else if (strcasecmp(cateory, "Events") == 0)
	{
		return CAP_CATEGORY_EVENTS;
	}
	else if (strcasecmp(cateory, "Imaging") == 0)
	{
		return CAP_CATEGORY_IMAGE;
	}
	else if (strcasecmp(cateory, "PTZ") == 0)
	{
		return CAP_CATEGORY_PTZ;
	}
	else if (strcasecmp(cateory, "All") == 0)
	{
		return CAP_CATEGORY_ALL;
	}

	return CAP_CATEGORY_INVALID;
}
#endif

#if 1
ONVIF_V_SRC * onvif_find_video_source(const char * token)
{
	ONVIF_V_SRC * video_source = g_onvif_cfg.video_src;

	if (NULL == token)
	{
		return NULL;
	}
	while (video_source)
	{
		if (strcasecmp(token, video_source->token) == 0)
		{
			return video_source;
		}
		video_source = video_source->next;
	}
	return NULL;
}

ONVIF_A_SRC * onvif_find_audio_source(const char * token)
{
	ONVIF_A_SRC * audio_source = g_onvif_cfg.audio_src;

	if (NULL == token)
	{
		return NULL;
	}
	while (audio_source)
	{
		if (strcasecmp(token, audio_source->token) == 0)
		{
			return audio_source;
		}
		audio_source = audio_source->next;
	}
	return NULL;
}
#endif

#if 1
ONVIF_V_ENC * onvif_find_video_encoder(const char * token)
{
	ONVIF_V_ENC * video_encoder = g_onvif_cfg.video_enc;

	if (NULL == token)
	{
		return NULL;
	}
	while (video_encoder)
	{
		if (strcasecmp(token, video_encoder->token) == 0)
		{
			return video_encoder;
		}
		video_encoder = video_encoder->next;
	}
	return NULL;
}

ONVIF_A_ENC * onvif_find_audio_encoder(const char * token)
{
	ONVIF_A_ENC * audio_encoder = g_onvif_cfg.audio_enc;

	if (NULL == token)
	{
		return NULL;
	}
	while (audio_encoder)
	{
		if (strcasecmp(token, audio_encoder->token) == 0)
		{
			return audio_encoder;
		}
		audio_encoder = audio_encoder->next;
	}
	return NULL;
}



int onvif_get_video_encoding(const char * encoding)
{
	if (strcasecmp(encoding, "JPEG") == 0)
	{
		return VIDEO_ENCODING_JPEG;
	}
	else if (strcasecmp(encoding, "MPEG4") == 0)
	{
		return VIDEO_ENCODING_MPEG4;
	}
	else
	{
		return VIDEO_ENCODING_H264;
	}
}

int onvif_get_audio_encoding(const char * encoding)
{
	if (strcasecmp(encoding, "G726") == 0)
	{
		return AUDIO_ENCODING_G726;
	}
	else if (strcasecmp(encoding, "AAC") == 0)
	{
		return AUDIO_ENCODING_AAC;
	}
	else
	{
		return AUDIO_ENCODING_G711;
	}
}
#endif

#if 1
int onvif_get_h264_profile(const char * profile)
{
	if (strcasecmp(profile, "Main") == 0)
	{
		return H264_PROFILE_Main;
	}
	else if (strcasecmp(profile, "Extended") == 0)
	{
		return H264_PROFILE_Extended;
	}
	else if (strcasecmp(profile, "High") == 0)
	{
		return H264_PROFILE_High;
	}
	else
	{
		return H264_PROFILE_Baseline;
	}
}

int onvif_get_mpeg4_profile(const char * profile)
{
	if (strcasecmp(profile, "ASP") == 0)
	{
		return MPEG4_PROFILE_ASP;
	}
	else
	{
		return MPEG4_PROFILE_SP;
	}
}
#endif

#if 1
ONVIF_A_SRC * onvif_find_audio_source()
{
	return g_onvif_cfg.audio_src;
}

ONVIF_A_SRC * onvif_add_audio_source()
{
	ONVIF_A_SRC * p_a_src = (ONVIF_A_SRC *) malloc(sizeof(ONVIF_A_SRC));
	if (NULL == p_a_src)
	{
		return NULL;
	}

	memset(p_a_src, 0, sizeof(ONVIF_A_SRC));

	p_a_src->use_count = 2;

	//snprintf(p_a_src->name, ONVIF_NAME_LEN, "A_SRC_00%d", g_onvif_cls.a_src_idx);
    //snprintf(p_a_src->token, ONVIF_TOKEN_LEN, "A_SRC_00%d", g_onvif_cls.a_src_idx);
    //snprintf(p_a_src->source_token, ONVIF_TOKEN_LEN, "A_SRC_00%d", g_onvif_cls.a_src_idx);

    g_onvif_cls.a_src_idx++;

	strcpy(p_a_src->name,"AudioMainName");
	strcpy(p_a_src->token,"AudioMainToken");
	strcpy(p_a_src->source_token,"AudioMainSrcToken");
	
	ONVIF_A_SRC * p_tmp = g_onvif_cfg.audio_src;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.audio_src = p_a_src;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_a_src;
	}

	return p_a_src;
}

ONVIF_V_SRC * onvif_find_video_source(int w, int h)
{
	ONVIF_V_SRC * p_v_src = g_onvif_cfg.video_src;
	while (p_v_src)
	{
		if (p_v_src->width == w && p_v_src->height == h)
		{
			break;
		}

		p_v_src = p_v_src->next;
	}

	return p_v_src;
}

ONVIF_V_SRC * onvif_add_video_source(int w, int h, int iIndex)
{
	ONVIF_V_SRC * p_v_src = (ONVIF_V_SRC *) malloc(sizeof(ONVIF_V_SRC));
	if (NULL == p_v_src)
	{
		return NULL;
	}

	memset(p_v_src, 0, sizeof(ONVIF_V_SRC));

	p_v_src->x = 0;
	p_v_src->y = 0;
	p_v_src->width = w;
	p_v_src->height = h;

	p_v_src->use_count = 0;

	strcpy(p_v_src->name, "VideoSourceMain");
	strcpy(p_v_src->token, "VideoSourceMain");
	strcpy(p_v_src->source_token, "VideoSourceMain");
	
	//snprintf(p_v_src->name, ONVIF_NAME_LEN, "V_SRC_00%d", g_onvif_cls.v_src_idx);
	//snprintf(p_v_src->token, ONVIF_TOKEN_LEN, "V_SRC_00%d", g_onvif_cls.v_src_idx);
	//snprintf(p_v_src->source_token, ONVIF_TOKEN_LEN, "V_SRC_00%d", g_onvif_cls.v_src_idx);

	g_onvif_cls.v_src_idx++;

	ONVIF_V_SRC * p_tmp = g_onvif_cfg.video_src;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.video_src = p_v_src;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_v_src;
	}

	return p_v_src;
}
#endif

#if 1
ONVIF_A_ENC * onvif_find_audio_encoder(ONVIF_A_ENC * p_a_enc)
{
	ONVIF_A_ENC * p_a_enc_ = g_onvif_cfg.audio_enc;
	while (p_a_enc_)
	{
		if (p_a_enc_->session_timeout == p_a_enc->session_timeout &&
			p_a_enc_->sample_rate == p_a_enc->sample_rate &&
			p_a_enc_->bitrate == p_a_enc->bitrate &&
			p_a_enc_->encoding == p_a_enc->encoding)
		{
			break;
		}

		p_a_enc_ = p_a_enc_->next;
	}

	return p_a_enc_;
}

ONVIF_A_ENC * onvif_add_audio_encoder(ONVIF_A_ENC * p_a_enc)
{
	ONVIF_A_ENC * p_a_enc_ = (ONVIF_A_ENC *) malloc(sizeof(ONVIF_A_ENC));
	if (NULL == p_a_enc_)
	{
		return NULL;
	}

	memset(p_a_enc_, 0, sizeof(ONVIF_A_ENC));

	p_a_enc_->use_count = 2;
	p_a_enc_->session_timeout = p_a_enc->session_timeout;
	p_a_enc_->sample_rate = p_a_enc->sample_rate;
	p_a_enc_->bitrate = p_a_enc->bitrate;	
	p_a_enc_->encoding = p_a_enc->encoding;

	//snprintf(p_a_enc_->name, ONVIF_NAME_LEN, "A_ENC_00%d", g_onvif_cls.a_enc_idx);
	//snprintf(p_a_enc_->token, ONVIF_TOKEN_LEN, "A_ENC_00%d", g_onvif_cls.a_enc_idx);

	g_onvif_cls.a_enc_idx++;

	strcpy(p_a_enc_->name, "AudioMain");
	if (p_a_enc_->encoding == AUDIO_ENCODING_G711)
	{
		strcpy(p_a_enc_->token, "G711");
	}
	else if (p_a_enc_->encoding == AUDIO_ENCODING_AAC)
	{
		strcpy(p_a_enc_->token, "AAC");
	}
	else
	{
		strcpy(p_a_enc_->token, "G711A");
	}
	ONVIF_A_ENC * p_tmp = g_onvif_cfg.audio_enc;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.audio_enc = p_a_enc_;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_a_enc_;
	}

	return p_a_enc_;
}

ONVIF_V_ENC * onvif_find_video_encoder(ONVIF_V_ENC * p_v_enc)
{
	ONVIF_V_ENC * p_v_enc_ = g_onvif_cfg.video_enc;
	while (p_v_enc_)
	{
		if (p_v_enc_->width == p_v_enc->width && 
			p_v_enc_->height == p_v_enc->height && 
			p_v_enc_->quality == p_v_enc->quality && 
			p_v_enc_->session_timeout == p_v_enc->session_timeout && 
			p_v_enc_->framerate_limit == p_v_enc->framerate_limit && 
			p_v_enc_->encoding_interval == p_v_enc->encoding_interval && 
			p_v_enc_->bitrate_limit == p_v_enc->bitrate_limit && 
			p_v_enc_->encoding == p_v_enc->encoding_interval && 
			p_v_enc_->gov_len == p_v_enc->gov_len && 
			p_v_enc_->profile == p_v_enc->profile)
		{
			break;
		}

		p_v_enc_ = p_v_enc_->next;
	}

	return p_v_enc_;
}

ONVIF_V_ENC * onvif_add_video_encoder(ONVIF_V_ENC * p_v_enc)
{
	ONVIF_V_ENC * p_v_enc_ = (ONVIF_V_ENC *) malloc(sizeof(ONVIF_V_ENC));
	if (NULL == p_v_enc_)
	{
		return NULL;
	}

	memset(p_v_enc_, 0, sizeof(ONVIF_V_ENC));

	p_v_enc_->use_count = 0;
	p_v_enc_->width = p_v_enc->width;
	p_v_enc_->height = p_v_enc->height;
	p_v_enc_->quality = p_v_enc->quality;
	p_v_enc_->session_timeout = p_v_enc->session_timeout;
	p_v_enc_->framerate_limit = p_v_enc->framerate_limit;
	p_v_enc_->encoding_interval = p_v_enc->encoding_interval;
	p_v_enc_->bitrate_limit = p_v_enc->bitrate_limit;
	p_v_enc_->encoding = p_v_enc->encoding;
	p_v_enc_->gov_len = p_v_enc->gov_len;
	p_v_enc_->profile = p_v_enc->profile;

	//snprintf(p_v_enc_->name, ONVIF_NAME_LEN, "V_ENC_00%d", g_onvif_cls.v_enc_idx);
	//snprintf(p_v_enc_->token, ONVIF_TOKEN_LEN, "V_ENC_00%d", g_onvif_cls.v_enc_idx);

	
	if(g_onvif_cls.v_enc_idx == 0)
	{
		strcpy(p_v_enc_->name, "VideoEncodeMain");
		strcpy(p_v_enc_->token, "VideoEncodeMain");
	}
	else if(g_onvif_cls.v_enc_idx == 1)
	{
		strcpy(p_v_enc_->name,"VideoEncodeSub");
		strcpy(p_v_enc_->token,"VideoEncodeSub");
	}
	else 
	{
		strcpy(p_v_enc_->name,"VideoEncodeThird");
		strcpy(p_v_enc_->token,"VideoEncodeThird");
	}
	
	g_onvif_cls.v_enc_idx++;

	ONVIF_V_ENC * p_tmp = g_onvif_cfg.video_enc;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.video_enc = p_v_enc_;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_v_enc_;
	}

	return p_v_enc_;
}

#endif

#if 1
BOOL onvif_is_scope_exist(const char * scope)
{
	for (int i = 0; i < MAX_SCOPE_NUMS; i++)
	{
		if (strcmp(scope, g_onvif_cfg.scopes[i].scope) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;	
}

ONVIF_SCOPE * onvif_find_scope(const char * scope)
{
	for (int i = 0; i < MAX_SCOPE_NUMS; i++)
	{
		if (strcmp(g_onvif_cfg.scopes[i].scope, scope) == 0)
		{
			return &g_onvif_cfg.scopes[i];
		}
	}

	return NULL;
}

ONVIF_SCOPE * onvif_get_idle_scope()
{
	for (int i = 0; i < MAX_SCOPE_NUMS; i++)
	{
		if (g_onvif_cfg.scopes[i].scope[0] == '\0')
		{
			return &g_onvif_cfg.scopes[i];
		}
	}

	return NULL;
}

int onvif_add_scope(const char * scope, BOOL fixed)
{
	if (onvif_is_scope_exist(scope) == TRUE)
	{
		return -1;
	}

	ONVIF_SCOPE * p_scope = onvif_get_idle_scope();
	if (p_scope)
	{
		p_scope->fixed = fixed;
		strncpy(p_scope->scope, scope, sizeof(p_scope->scope)-1);
		return 0;
	}

	return -2;
}

ONVIF_RET onvif_add_scopes(ONVIF_SCOPE * p_scope, int scope_max)
{
	for (int i = 0; i < scope_max; i++)
	{
		if (p_scope[i].scope[0] == '\0')
		{
			break;
		}

		if (onvif_is_scope_exist(p_scope[i].scope))
		{
			continue;
		}
		
		ONVIF_SCOPE * p_item = onvif_get_idle_scope();
		if (p_item)
		{
			p_item->fixed = p_scope[i].fixed;
			strcpy(p_item->scope, p_scope[i].scope);
		}
		else
		{
			return ONVIF_ERR_TOO_MANY_SCOPES;
		}
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_set_scopes(ONVIF_SCOPE * p_scope, int scope_max)
{
	ONVIF_SCOPE * p_item = NULL;
	
	for (int i = 0; i < scope_max; i++)
	{
		if (p_scope[i].scope[0] == '\0')
		{
			break;
		}
		
		p_item = onvif_find_scope(p_scope[i].scope);
		if (p_item && p_item->fixed)
		{
			return ONVIF_ERR_SCOPE_OVERWRITE;
		}
	}
	
	for (int i = 0; i < MAX_SCOPE_NUMS; i++)
	{
		if (g_onvif_cfg.scopes[i].fixed == FALSE && g_onvif_cfg.scopes[i].scope[0] != '\0')
		{
			memset(&g_onvif_cfg.scopes[i], 0, sizeof(ONVIF_SCOPE));
		}
	}
	
	return onvif_add_scopes(p_scope, scope_max);
}

ONVIF_RET onvif_remove_scopes(ONVIF_SCOPE * p_scope, int scope_max)
{
	ONVIF_SCOPE * p_item = NULL;
	
	for (int i = 0; i < scope_max; i++)
	{
		if (p_scope[i].scope[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_scope(p_scope[i].scope);
		if (NULL == p_item)
		{
			return ONVIF_ERR_NO_SCOPE;
		}
		else if (p_item->fixed)
		{
			return ONVIF_ERR_FIXED_SCOPE;
		}
	}	

	for (int i = 0; i < scope_max; i++)
	{
		if (p_scope[i].scope[0] == '\0')
		{
			break;
		}

		p_item = onvif_find_scope(p_scope[i].scope);
		if (NULL != p_item && p_item->fixed == FALSE)
		{
			memset(p_item, 0, sizeof(ONVIF_SCOPE));
		}
	}	

	return ONVIF_OK;
}
#endif

#if 1
PTZ_NODE * onvif_find_ptz_node(const char * token)
{
	PTZ_NODE * p_node = g_onvif_cfg.ptznodes;
	while (p_node)
	{
		if (strcmp(p_node->token, token) == 0)
		{
			break;
		}

		p_node = p_node->next;
	}

	return p_node;
}

PTZ_NODE * onvif_find_ptz_cfg(const char * token)
{
	PTZ_NODE * p_node = g_onvif_cfg.ptznodes;
	while (p_node)
	{
		if (strcmp(p_node->config.token, token) == 0)
		{
			break;
		}

		p_node = p_node->next;
	}

	return p_node;
}


PTZ_NODE * onvif_add_ptz_node()
{
	PTZ_NODE * p_node = (PTZ_NODE *) malloc(sizeof(PTZ_NODE));
	if (NULL == p_node)
	{
		return NULL;
	}

	memset(p_node, 0, sizeof(PTZ_NODE));

	snprintf(p_node->name, ONVIF_NAME_LEN, "NODE_00%d", g_onvif_cls.ptznode_idx);
	snprintf(p_node->token, ONVIF_TOKEN_LEN, "NODE_00%d", g_onvif_cls.ptznode_idx);
	snprintf(p_node->config.name, ONVIF_NAME_LEN, "PTZ_00%d", g_onvif_cls.ptznode_idx);
	snprintf(p_node->config.token, ONVIF_TOKEN_LEN, "PTZ_00%d", g_onvif_cls.ptznode_idx);

	g_onvif_cls.ptznode_idx++;

	PTZ_NODE * p_tmp = g_onvif_cfg.ptznodes;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.ptznodes = p_node;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_node;
	}

	return p_node;
}

void onvif_parse_ptz_node()
{
	PTZ_NODE * p_ptz_node = onvif_add_ptz_node();
	if (NULL == p_ptz_node)
	{
		printf("NULL == p_ptz_node\n");
		return;
	}
	
	p_ptz_node->fixed_home_pos = 0;
	if (1)
	{
		p_ptz_node->abs_pantilt_space = 1;
		p_ptz_node->abs_pantilt_x.min = -1;
		p_ptz_node->abs_pantilt_x.max = 1;
		p_ptz_node->abs_pantilt_y.min = -1;
		p_ptz_node->abs_pantilt_y.max = 1;
		
		p_ptz_node->abs_zoom_space = 1;
		p_ptz_node->abs_zoom.min = 0;
		p_ptz_node->abs_zoom.max = 1;
	}
	if (1)
	{
		p_ptz_node->rel_pantilt_space = 1;
		p_ptz_node->rel_pantilt_x.min = -1;
		p_ptz_node->rel_pantilt_x.max = 1;
		p_ptz_node->rel_pantilt_y.min = -1;
		p_ptz_node->rel_pantilt_y.max = 1;
		
		p_ptz_node->rel_zoom_space = 1;
		p_ptz_node->rel_zoom.min = -1;
		p_ptz_node->rel_zoom.max = 1;
	}
	if (1)
	{
		p_ptz_node->con_pantilt_space = 1;
		p_ptz_node->con_pantilt_x.min = -1;
		p_ptz_node->con_pantilt_x.max = 1;
		p_ptz_node->con_pantilt_y.min = -1;
		p_ptz_node->con_pantilt_y.max = 1;
		
		p_ptz_node->con_zoom_space = 1;
		p_ptz_node->con_zoom.min = -1;
		p_ptz_node->con_zoom.max = 1;
	}
	if (1)
	{
		p_ptz_node->pantile_speed_space = 1;
		p_ptz_node->pantile_speed.min = 0;
		p_ptz_node->pantile_speed.max = 1;
		
		p_ptz_node->zoom_speed_space = 1;
		p_ptz_node->zoom_speed.min = 0;
		p_ptz_node->zoom_speed.max = 1;
	}

	if (1)
	{
		p_ptz_node->config.def_pantilt_speed_x = 0.5;
		p_ptz_node->config.def_pantilt_speed_y = 0.5;
		p_ptz_node->config.def_zoom_speed = 0.5;

		p_ptz_node->config.def_timeout = 5;
		p_ptz_node->config.pantilt_limits_x.min = -1;
		p_ptz_node->config.pantilt_limits_x.max = 1;
		p_ptz_node->config.pantilt_limits_y.min = -1;
		p_ptz_node->config.pantilt_limits_y.max = 1;
		p_ptz_node->config.zoom_limits.min = 0;
		p_ptz_node->config.zoom_limits.max = 1;
		p_ptz_node->config.timeout_range.min = 1;
		p_ptz_node->config.timeout_range.max = 60;
	}
}

PTZ_PRESET * onvif_find_preset(const char * profile_token, const char  * preset_token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(profile_token);
	if (NULL == p_profile || NULL == p_profile->ptz_node)
	{
		return NULL;
	}

	int i;

	for (i = 0; i < PTZ_MAX_PRESETS; i++)
	{
		if (strcmp(preset_token, p_profile->ptz_node->config.presets[i].token) == 0)
		{
			break;
		}
	}

	if (i == PTZ_MAX_PRESETS)
	{
		return NULL;
	}

	return &p_profile->ptz_node->config.presets[i];
}

PTZ_PRESET * onvif_get_idle_preset(const char * profile_token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(profile_token);
	if (NULL == p_profile || NULL == p_profile->ptz_node)
	{
		return NULL;
	}

	int i;

	for (i = 0; i < PTZ_MAX_PRESETS; i++)
	{
		if (p_profile->ptz_node->config.presets[i].used_flag == 0)
		{
			break;
		}
	}

	if (i == PTZ_MAX_PRESETS)
	{
		return NULL;
	}

	return &p_profile->ptz_node->config.presets[i];
}
#endif
#if 1
ONVIF_V_SRC * onvif_parse_video_source(int iIndex)
{
	int w, h;
	OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].resolution.name, 
		gMediaCfg.videoConfig[0].videoCapture.tvsystem, &w, &h);
	
	ONVIF_V_SRC * p_v_src = NULL;
	
	if (g_onvif_cfg.merge_src == 1)
	{
		p_v_src = onvif_find_video_source(w, h);
		if (p_v_src)
		{
			return p_v_src;
		}
	}

	p_v_src = onvif_add_video_source(w, h, iIndex);

	return p_v_src;
}

ONVIF_V_ENC * onvif_parse_video_encoder(int iIndex)
{
	int width, height;
	OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].resolution.name, 
		gMediaCfg.videoConfig[0].videoCapture.tvsystem, &width, &height);//��ȡ��ǰ�ֱ��ʵĳ���
	
	ONVIF_V_ENC v_enc;
	memset(&v_enc, 0, sizeof(ONVIF_V_ENC));
	
	v_enc.width = width;
	v_enc.height = height;

	VideoQualityEnum quality = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].bitRateQuality;//��ǰ����
	float VEquality = 0;
	if(quality == VIDEO_QUALITY_WORSER)
		VEquality = 20;
	else if(quality == VIDEO_QUALITY_WORSE)
		VEquality = 40;
	else if(quality == VIDEO_QUALITY_NORMAL)
		VEquality = 60;
	else if(quality == VIDEO_QUALITY_GOOD)
		VEquality = 80;
	else if(quality == VIDEO_QUALITY_BEST)
		VEquality = 100;
	else if(quality == VIDEO_QUALITY_CUSTOM)
		VEquality = 50;
	else 
		VEquality = 0;
	v_enc.quality = VEquality;
	
	v_enc.session_timeout = 720000;
	
	v_enc.framerate_limit = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].display_frameRate;
	
	v_enc.encoding_interval = 1;
	
	v_enc.bitrate_limit = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].bitRate;
	if(v_enc.bitrate_limit > 10*1024)
		v_enc.bitrate_limit = 0;
	
	if(strstr(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].encodeFormat.name, "MJPEG"))
		v_enc.encoding = VIDEO_ENCODING_JPEG; //JPEG = 0, MPEG4 = 1, H264 = 2, H265 = 3;
	else if(strstr(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].encodeFormat.name, "H265"))
		v_enc.encoding = VIDEO_ENCODING_H265;
	else 
		v_enc.encoding = VIDEO_ENCODING_H264;

	v_enc.gov_len = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].initQuant;
	
	if (gMediaCfg.videoConfig[0].videoEncode.encode_profile == 0)
	{
		v_enc.profile = 3;
	}
	else if (gMediaCfg.videoConfig[0].videoEncode.encode_profile == 1)
	{
		v_enc.profile = 1;
	}
	else if (gMediaCfg.videoConfig[0].videoEncode.encode_profile == 2)
	{
		v_enc.profile = 0;
	}
	else
	{
		v_enc.profile = 3;
	}

	ONVIF_V_ENC * p_v_enc = onvif_find_video_encoder(&v_enc);
	if (p_v_enc)
	{
		return p_v_enc;
	}

	p_v_enc = onvif_add_video_encoder(&v_enc);

	return p_v_enc;
}

ONVIF_A_SRC * onvif_parse_audio_source()
{	
	ONVIF_A_SRC * p_a_src = onvif_find_audio_source();
	if (p_a_src)
	{
		return p_a_src;
	}

	p_a_src = onvif_add_audio_source();

	return p_a_src;
}

ONVIF_A_ENC * onvif_parse_audio_encoder(int iIndex)
{
	ONVIF_A_ENC a_enc;
	memset(&a_enc, 0, sizeof(ONVIF_A_ENC));

	a_enc.session_timeout = 10;
	a_enc.sample_rate = gMediaCfg.audioConfig.audioEncode.sampleRate;
	a_enc.bitrate = gMediaCfg.audioConfig.audioEncode.bitRate;

	
	AudioType_e myType = (AudioType_e)GetAudioEncoderType(gMediaCfg.audioConfig.audioEncode.audioEncodeType.typeName);
	switch(myType)
	{
		case AudioType_PCMU:
			a_enc.encoding = AUDIO_ENCODING_G711;
			break;
		case AudioType_AAC_LC:
			a_enc.encoding = AUDIO_ENCODING_AAC;
			break;
		case AudioType_PCMA:
			a_enc.encoding = AUDIO_ENCODING_G711A;
			break;
		default:
			break;
	}

	ONVIF_A_ENC * p_a_enc = onvif_find_audio_encoder(&a_enc);
	if (p_a_enc)
	{
		return p_a_enc;
	}

	p_a_enc = onvif_add_audio_encoder(&a_enc);

	return p_a_enc;
}
#endif
#if 1
ONVIF_NET_INF * onvif_add_net_interface()
{
	ONVIF_NET_INF * p_net_inf = (ONVIF_NET_INF *) malloc(sizeof(ONVIF_NET_INF));
	if (NULL == p_net_inf)
	{
		return NULL;
	}

	memset(p_net_inf, 0, sizeof(ONVIF_NET_INF));

	snprintf(p_net_inf->token, ONVIF_TOKEN_LEN, "eth%d", g_onvif_cls.netinf_idx);
	snprintf(p_net_inf->name, ONVIF_NAME_LEN, "eth%d", g_onvif_cls.netinf_idx);

	g_onvif_cls.netinf_idx++;

	ONVIF_NET_INF * p_tmp = g_onvif_cfg.network.interfaces;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.network.interfaces = p_net_inf;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;

		p_tmp->next = p_net_inf;
	}

	return p_net_inf;
}

ONVIF_NET_INF * onvif_find_net_inf(const char * token)
{
	ONVIF_NET_INF * p_net_inf = g_onvif_cfg.network.interfaces;

	while (p_net_inf)
	{
		if (strcmp(p_net_inf->token, token) == 0)
		{
			break;
		}
		p_net_inf = p_net_inf->next;
	}
	return p_net_inf;
}

void onvif_init_net_interfaces()
{
#if __LINUX_OS__

	int socket_fd;
	struct ifreq *ifr;
	struct ifreq ifreq;
	struct ifconf conf;
	char buff[BUFSIZ];
	int num;

	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

	conf.ifc_len = BUFSIZ;
	conf.ifc_buf = buff;

	ioctl(socket_fd, SIOCGIFCONF, &conf);

	num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;

	for (int i = 0; i < num; i++)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);
		if (ifr->ifr_addr.sa_family != AF_INET)
		{
			ifr++;
			continue;
		}
		ioctl(socket_fd, SIOCGIFFLAGS, ifr);
		if ((ifr->ifr_flags & IFF_LOOPBACK) == 0)
		{
			ONVIF_NET_INF * p_net_inf = onvif_add_net_interface();
			if (NULL == p_net_inf)
			{
				break;
			}
			p_net_inf->mtu = ifr->ifr_mtu;
			p_net_inf->ipv4_enabled = 1;
			p_net_inf->fromdhcp = 0;
			p_net_inf->prefix_len = 24;
			p_net_inf->enabled = TRUE;
			strcpy(p_net_inf->ipv4_addr, inet_ntoa(sin->sin_addr));
			strncpy(p_net_inf->name, ifr->ifr_name, sizeof(p_net_inf->name)-1);
			strncpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
			if (ioctl(socket_fd, SIOCGIFHWADDR, &ifreq) < 0) 
			{
				ifr++;
				continue;
			}
			snprintf(p_net_inf->hwaddr, sizeof(p_net_inf->hwaddr), "%02X:%02X:%02X:%02X:%02X:%02X", 
				(unsigned char)ifreq.ifr_hwaddr.sa_data[0], (unsigned char)ifreq.ifr_hwaddr.sa_data[1], (unsigned char)ifreq.ifr_hwaddr.sa_data[2],
				(unsigned char)ifreq.ifr_hwaddr.sa_data[3], (unsigned char)ifreq.ifr_hwaddr.sa_data[4], (unsigned char)ifreq.ifr_hwaddr.sa_data[5]);
		}
		ifr++;
	}
	closesocket(socket_fd);
	
#elif __WIN32_OS__
		IP_ADAPTER_ADDRESSES addr[16], *paddr;
		DWORD len = sizeof(addr);
		
		if (NO_ERROR == GetAdaptersAddresses(AF_INET, 0, 0, addr, &len) && len >= sizeof(IP_ADAPTER_ADDRESSES))
		{
			paddr = addr;
			while (paddr)
			{
				if (paddr->IfType & IF_TYPE_SOFTWARE_LOOPBACK)
				{
					paddr = paddr->Next;
					continue;
				}
				ONVIF_NET_INF * p_net_inf = onvif_add_net_interface();
				if (NULL == p_net_inf)
				{
					return;
				}
				sprintf(p_net_inf->hwaddr, "%02X:%02X:%02X:%02X:%02X:%02X", paddr->PhysicalAddress[0], paddr->PhysicalAddress[1],
					paddr->PhysicalAddress[2], paddr->PhysicalAddress[3], paddr->PhysicalAddress[4], paddr->PhysicalAddress[5]);
	
				p_net_inf->mtu = paddr->Mtu;
				p_net_inf->ipv4_enabled = (paddr->Flags & IP_ADAPTER_IPV4_ENABLED);
				p_net_inf->fromdhcp = (paddr->Flags & IP_ADAPTER_DHCP_ENABLED);
	
				IP_ADAPTER_UNICAST_ADDRESS * p_ipaddr = paddr->FirstUnicastAddress;
				while (p_ipaddr)
				{
					if (paddr->FirstUnicastAddress->Address.lpSockaddr->sa_family == AF_INET)
					{
						struct sockaddr_in * p_inaddr = (struct sockaddr_in *)paddr->FirstUnicastAddress->Address.lpSockaddr;
	
						strcpy(p_net_inf->ipv4_addr, inet_ntoa(p_inaddr->sin_addr));
						p_net_inf->prefix_len = 24;
						break;
					}
					p_ipaddr = p_ipaddr->Next;
				}
				p_net_inf->enabled = TRUE;
				paddr = paddr->Next;
			}
		}
#endif
}

void onvif_init_net()
{
	printf("onvif_init_net\n");
	// init host name

	g_onvif_cfg.network.hostname_fromdhcp = 0;
	g_onvif_cfg.network.set_hostname_need_reboot = 1;
	gethostname(g_onvif_cfg.network.hostname, sizeof(g_onvif_cfg.network.hostname));

	// init dns setting
	g_onvif_cfg.network.dns_fromdhcp = 0;
	
	char _dns[LARGE_INFO_LENGTH];
	NET_IPV4 ip;
	ip.int32 = net_get_dns();
	sprintf(_dns, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]);
	
	strcpy(g_onvif_cfg.network.dns_server[0], _dns);

	// init ntp settting
	g_onvif_cfg.network.ntp_fromdhcp = 0;
	strcpy(g_onvif_cfg.network.ntp_server[0], "time.windows.com");

	// init network protocol
	g_onvif_cfg.network.http_support = 1;
	g_onvif_cfg.network.https_support = 0;
	g_onvif_cfg.network.rtsp_support = 1;
	g_onvif_cfg.network.http_enable = 1;
	g_onvif_cfg.network.https_enable = 0;
	g_onvif_cfg.network.rtsp_enable = 1;
	g_onvif_cfg.network.http_port[0] = 80;
	g_onvif_cfg.network.https_port[0] = 443;
	g_onvif_cfg.network.rtsp_port[0] = 554;

	// init default gateway
	char _GatewayAddress[LARGE_INFO_LENGTH];
	NET_IPV4 ip_g;
	ip_g.int32 = net_get_gateway();
	sprintf(_GatewayAddress, "%d.%d.%d.%d", ip_g.str[0], ip_g.str[1], ip_g.str[2], ip_g.str[3]); 
	strcpy(g_onvif_cfg.network.gateway[0], _GatewayAddress);

	// init network interface
	onvif_init_net_interfaces();
	
}
#endif
#if 1
ONVIF_PROFILE * onvif_find_profile(const char * token)
{
	ONVIF_PROFILE * profile = g_onvif_cfg.profiles;
	if (NULL == token)
	{
		return NULL;
	}
	while (profile)
	{
		if (strcasecmp(token, profile->token) == 0)
		{
			return profile;
		}
		profile = profile->next;
	}
	return NULL;
}

ONVIF_PROFILE * onvif_add_profile(BOOL fixed)
{
	ONVIF_PROFILE * p_profile = (ONVIF_PROFILE *) malloc(sizeof(ONVIF_PROFILE));
	if (NULL == p_profile)
	{
		return NULL;
	}

	memset(p_profile, 0, sizeof(ONVIF_PROFILE));

	p_profile->fixed = fixed;

	//snprintf(p_profile->name, ONVIF_NAME_LEN, "PROFILE_00%d", g_onvif_cls.profile_idx);
	//snprintf(p_profile->token, ONVIF_TOKEN_LEN, "PROFILE_00%d", g_onvif_cls.profile_idx);


	if(g_onvif_cls.profile_idx == 0)
	{
		strcpy(p_profile->name, "MainStream");
		strcpy(p_profile->token, "MainStream");
	}
	else if(g_onvif_cls.profile_idx == 1)
	{
		strcpy(p_profile->name,"SubStream");
		strcpy(p_profile->token,"SubStream");
	}
	else 
	{
		strcpy(p_profile->name,"ThirdStream");
		strcpy(p_profile->token,"ThirdStream");
	}
	
	g_onvif_cls.profile_idx ++;

	ONVIF_PROFILE * p_tmp = g_onvif_cfg.profiles;
	if (NULL == p_tmp)
	{
		g_onvif_cfg.profiles = p_profile;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_profile;
	}

	return p_profile;
}

void onvif_parse_profile(int iIndex)
{
	ONVIF_PROFILE * profile = onvif_add_profile(TRUE);
	if (NULL == profile)
	{
		return;
	}
	profile->fixed = 0;
	profile->video_enc = onvif_parse_video_encoder(iIndex);
	profile->video_enc->use_count++;
		
	profile->video_src = onvif_parse_video_source(iIndex);
	profile->video_src->use_count++;

	profile->audio_src = onvif_parse_audio_source();
	profile->audio_src->use_count++;
	
	profile->audio_enc = onvif_parse_audio_encoder(iIndex);
	profile->audio_enc->use_count++;

	
	if (strlen(g_onvif_cfg.serv_ip) > 0)
	{
		sprintf(profile->stream_uri, "rtsp://%s/test.264", g_onvif_cfg.serv_ip);
	}
	else
	{
		sprintf(profile->stream_uri, "rtsp://%s/test.264", onvif_get_local_ip());
	}
}

void onvif_init_profile()
{
	int iIndex = 0;
	
	if(MsgGetMediaConfig(&gMediaCfg) != 0)
	{
		printf("MsgGetMediaConfig error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
	}
	
	int third = (gMediaCfg.videoConfig[0].videoEncode.encodeCfg[2].enable);
	
	for( iIndex = 0; iIndex < 2 + third; iIndex ++)
	{
		onvif_parse_profile(iIndex);
	}

	onvif_parse_ptz_node();
	
	ONVIF_PROFILE * profile = g_onvif_cfg.profiles;
	while (profile)
	{
		profile->ptz_node = g_onvif_cfg.ptznodes;
		
		profile = profile->next;
	}
}

#endif
in_addr_t g_gateway_addr;
in_addr_t g_ip_addr;

void *check_ipaddr_thr(void *arg)
{
	__ERR("!!!enter check_ipaddr_thr!!!\n");
	pthread_detach(pthread_self()); 

	in_addr_t gateway_addr;
	in_addr_t ip_addr;

	char ifname[256]={0};

	while(1)
	{
		if (Check_Link_Status(WIRE_INTERFACE_NAME))
		{
			strcpy(ifname,WIRE_INTERFACE_NAME);
		}
		else
		{
			if(Check_Link_Status(net_get_wireless_name()))
				strcpy(ifname,net_get_wireless_name());
			else
				strcpy(ifname,WIRE_INTERFACE_NAME);
		}

		ip_addr      = net_get_ifaddr(ifname);
		gateway_addr = net_get_gateway();
		__ERR("ifname = %s, ip = 0x%x, gateway = 0x%x\n", ifname, ip_addr, gateway_addr);
		if((ip_addr != g_ip_addr && ip_addr != 0xFFFFFFFF) || (gateway_addr != g_gateway_addr && gateway_addr != 0xFFFFFFFF))
		{
			__ERR("ip_addr != g_ip_addr\n");
			break;
		}
		sleep(5);
	}

	__ERR("restart webserver_easy\n");
	exit(0);
	
	return NULL;
}

void *wait_ipaddr_thr(void *arg)
{
	__ERR("!!!enter wait_ipaddr_thr!!!\n");
	pthread_detach(pthread_self()); 

	in_addr_t gateway_addr;
	in_addr_t ip_addr;

	char gateway_addr_str[256]={0};
	char ip_addr_str[256]={0};

	int loopcount=0;
	char ifname[256]={0};

	while(1)
	{
		if((loopcount%10) == 0)
		{
			__ERR("wait ip address ready...\n");
		}
		
		if (Check_Link_Status(WIRE_INTERFACE_NAME)){
			strcpy(ifname,WIRE_INTERFACE_NAME);
		}
		else{
			if(Check_Link_Status(net_get_wireless_name()))
				strcpy(ifname,net_get_wireless_name());
			else
				strcpy(ifname,WIRE_INTERFACE_NAME);
		}

		ip_addr	   = net_get_ifaddr(ifname);
		gateway_addr = net_get_gateway();
		__ERR("ifname = %s, ip = 0x%x, gateway = 0x%x\n", ifname, ip_addr, gateway_addr);

		if(ip_addr != INADDR_ANY && ip_addr != 0xFFFFFFFF
					&& gateway_addr != INADDR_ANY && gateway_addr != 0xFFFFFFFF)
		{
			get_ip_str(ip_addr, ip_addr_str, 256);
			get_ip_str(gateway_addr, gateway_addr_str, 256);
			__ERR("ready! ip = 0x%x, %s, gateway = %s\n", ip_addr, ip_addr_str, gateway_addr_str);
			break;
		}
		loopcount++;
		sleep(1);
	}

	__ERR("restart webserver_easy\n");
	exit(0);
	
	return NULL;
}

void get_onvif_serial_number()
{
	char macaddr[MACH_ADDR_LENGTH];
	get_my_macaddr(macaddr);
	sprintf(g_uuid, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

	if(strlen(macaddr) > 0)
		sprintf(g_onvif_cfg.serial_number, "%s", g_uuid);
	else
		sprintf(g_onvif_cfg.serial_number, "%s", g_SN);

}

void onvif_init_cfg()
{
	printf("onvif_init_cfg\n");
	
	GetOnvifOemInfo(&g_OemInfo);
	g_onvif_cfg.discoverable = 1;
	if (1)
	{
		g_onvif_cfg.need_auth = 0;
		MediaStreamConfig pMediaStreamCfg;
		memset(&pMediaStreamCfg, 0, sizeof(MediaStreamConfig));
		MsgGetMediaStreamConfig(&pMediaStreamCfg);
		if (pMediaStreamCfg.webConfig.onvif_auth == 1)
			g_onvif_cfg.need_auth = 1;
	}
	g_onvif_cfg.serv_port = gStreamCfg.webConfig.webPort;
	
	g_onvif_cfg.evt_sim_interval = 60;
	g_onvif_cfg.evt_sim_flag = 1;
	g_onvif_cfg.evt_renew_time = 60;

	g_onvif_cfg.datetime_ntp = 0;
	g_onvif_cfg.daylightsavings = false;
//////////////////////////////////ip+port
	if (0)/*��ȡip*/
	{
		LANConfig lanCfg;
		MsgGetNetworkLANConfig(&lanCfg);
		memset(g_onvif_cfg.serv_ip, 0, 32);
		strcpy(g_onvif_cfg.serv_ip, lanCfg.IPAddress);
	}
	if (1)
	{
		in_addr_t gateway_addr;
		in_addr_t ip_addr;

		char gateway_addr_str[256];
		char ip_addr_str[256];

		int loopcount=0;
		char ifname[256];
		while(1)
		{
			if((loopcount % 10) == 0)
			{
				printf("wait ip address ready...\n");
			}

			if(loopcount ++ == 60)
				break;
			
			if (Check_Link_Status(WIRE_INTERFACE_NAME))
			{
				strcpy(ifname, WIRE_INTERFACE_NAME);
			}
			else
			{
				if(Check_Link_Status(net_get_wireless_name()))
					strcpy(ifname, net_get_wireless_name());
				else
					strcpy(ifname, WIRE_INTERFACE_NAME);
			}

			ip_addr = net_get_ifaddr(ifname);
			gateway_addr = net_get_gateway();
			
			g_ip_addr = ip_addr;
			g_gateway_addr = gateway_addr;
			
			printf("ifname = %s, ip = 0x%x, gateway = 0x%x\n", ifname, ip_addr, gateway_addr);
			if(ip_addr != INADDR_ANY && ip_addr != 0xFFFFFFFF && gateway_addr != INADDR_ANY && gateway_addr != 0xFFFFFFFF)
			{
				get_ip_str(ip_addr, ip_addr_str, 256);
				get_ip_str(gateway_addr, gateway_addr_str, 256);
				
				memset(g_onvif_cfg.serv_ip, 0, 32);
				strncpy(g_onvif_cfg.serv_ip, ip_addr_str, 32);
				printf("ip = 0x%x, %s, gateway = %s\n", ip_addr, ip_addr_str, gateway_addr_str);
				break;
			}
			usleep(1000*1000);
			//get_ip_str(ip_addr, ip_addr_str, 256);
			//get_ip_str(gateway_addr, gateway_addr_str, 256);

			//break;
		}
		
		printf("g_onvif_cfg.serv_ip:%s\n", g_onvif_cfg.serv_ip);
		if(1)
		{
			//��ʱ
			pthread_t msg_thread;
			if(pthread_create(&msg_thread, NULL, check_ipaddr_thr, NULL) != 0)
				__ERR("Error: check_ipaddr_thr thread create failed");
			else
				__ERR("create check_ipaddr_thr ok!\n");
		}
		if(loopcount >= 60)
		{
			get_ip_str(ip_addr, ip_addr_str, 256);
			get_ip_str(gateway_addr, gateway_addr_str, 256);
			memset(g_onvif_cfg.serv_ip, 0, 32);
			strncpy(g_onvif_cfg.serv_ip, ip_addr_str, 32);
			//��ʱ
			pthread_t msg_thread;
			if(pthread_create(&msg_thread, NULL, wait_ipaddr_thr, NULL) != 0)
				__ERR("Error: wait_ipaddr_thr thread create failed");
			else
				__ERR("create wait_ipaddr_thr ok!\n");
		}
	}

	
	if (g_onvif_cfg.serv_port > 0 && g_onvif_cfg.serv_port < 65535) 
	{
		g_onvif_cls.local_port = g_onvif_cfg.serv_port;
	}
	else
	{
		g_onvif_cls.local_port = 80;
	}

	if (strlen(g_onvif_cfg.serv_ip) > 0)/*��һ��ipv4���ж�*/
	{
		strcpy(g_onvif_cls.local_ipstr, g_onvif_cfg.serv_ip);
	}
	else 
	{
		const char * ip = onvif_get_local_ip();
		if (ip)
		{
			strcpy(g_onvif_cls.local_ipstr, ip);
			strcpy(g_onvif_cfg.serv_ip, ip);
		}
	}
//////////////////////////////////username+password
	UserConfig pUserCfg;
	MsgGetUserConfig(&pUserCfg);
	memset(g_onvif_cfg.auth_user, 0, 32);
	memset(g_onvif_cfg.auth_pass, 0, 32);
	
	strcpy(g_onvif_cfg.auth_user, pUserCfg.accounts[0].userName);
	strcpy(g_onvif_cfg.auth_pass, pUserCfg.accounts[0].password);
///////////////////////////////////device information

	strcpy(g_onvif_cfg.manufacturer, DEFAULT_ONVIF_NONE);

	if (1)
	{
		FILE *fp = fopen("/etc/filesys.ver","rb");
		char filebuf[LARGE_INFO_LENGTH]="";
		char *ptr=NULL;
		if(fp)
		{
			int ret = 0;
			ret = fread(filebuf, 1, LARGE_INFO_LENGTH, fp);
			fclose(fp);

			if(ret > 0)
			{
				filebuf[ret]='\0';
				ptr = strstr(filebuf," ");
				if(ptr)
					strcpy(g_onvif_cfg.firmware_version, ptr + 1);
				else
					strcpy(g_onvif_cfg.firmware_version, filebuf);
			}
		}
	}
	
	SYSTEM_VERSION_DATA versionInfo;
	int ret = MsgGetSystemVersionInfo(&versionInfo);
	if(ret == 0)
	{
		char *ptr=strstr(versionInfo.fsVersion, " ");
		if(ptr)
			*ptr = '\0';
		ptr = strstr(versionInfo.fsVersion, "_V");
		if(ptr)
			*ptr = '\0';

		strcpy((char *)g_onvif_cfg.model, versionInfo.fsVersion);
	}
	else
	{
		strcpy((char *)g_onvif_cfg.model, DEFAULT_ONVIF_NAME);
	}

	unsigned char sn[256];
	if(ReadEncriptData(sn, sizeof(sn)) >= 0)
		sprintf(g_SN, "%02X%02X%02X%02X%02X%02X%02X%02X", sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7]);
	
	char macaddr[MACH_ADDR_LENGTH];
	get_my_macaddr(macaddr);
	sprintf(g_uuid, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

	if(strlen(macaddr) > 0)
		sprintf(g_onvif_cfg.serial_number, "%s", g_uuid);
	else
		sprintf(g_onvif_cfg.serial_number, "%s", g_SN);
	
	sprintf(g_onvif_cfg.hardware_id, "1419d68a-1dd2-11b2-a105-%s", g_uuid);

//////////////////////////////////time
	onvif_get_timezone(g_onvif_cfg.timezone, sizeof(g_onvif_cfg.timezone));
//////////////////////////////////scope

	strcpy(g_onvif_cfg.scopes[0].scope, "onvif://www.onvif.org/type/ptz");
	strcpy(g_onvif_cfg.scopes[1].scope, "onvif://www.onvif.org/type/video_encoder");
	strcpy(g_onvif_cfg.scopes[2].scope, "onvif://www.onvif.org/type/audio_encoder");
	strcpy(g_onvif_cfg.scopes[3].scope, "onvif://www.onvif.org/name/ONVIF_ICAMERA");
	strcpy(g_onvif_cfg.scopes[4].scope, "onvif://www.onvif.org/Profile/Streaming");
	
	char szDefaultHardware[32] = {0};
	if(strlen(g_OemInfo.model) != 0)
	{
		strcpy(szDefaultHardware, g_OemInfo.model);
	}
	else
	{
		GetDeviceTypeStr(szDefaultHardware);
	}
	sprintf(g_onvif_cfg.scopes[5].scope, "onvif://www.onvif.org/hardware/%s", szDefaultHardware);
	strcpy(g_onvif_cfg.scopes[6].scope, "onvif://www.onvif.org/manufacturer/NONE");
	strcpy(g_onvif_cfg.scopes[7].scope, "onvif://www.onvif.org/location/china");
	strcpy(g_onvif_cfg.scopes[8].scope, "onvif://www.onvif.org/location/Shenzhen");
	//strcpy(g_onvif_cfg.scopes[9].scope, "onvif://www.onvif.org/Profile/T");
//////////////////////////////////profile
	onvif_init_profile();
	//ONVIF_PROFILE * profile2 = onvif_add_profile(TRUE);
//////////////////////////////////video_source_config

	g_onvif_cfg.video_src_cfg.x_min = 0;
	g_onvif_cfg.video_src_cfg.x_max = 3840;
	g_onvif_cfg.video_src_cfg.y_min = 0;
	g_onvif_cfg.video_src_cfg.y_max = 2160;
	g_onvif_cfg.video_src_cfg.w_min = 0;
	g_onvif_cfg.video_src_cfg.w_max = 3840;
	g_onvif_cfg.video_src_cfg.h_min = 0;
	g_onvif_cfg.video_src_cfg.h_max = 2160;

//////////////////////////////////video_encoder_config
	int i;
	//����֧���ж�//�ֱ�������//
	RESOLUTION_ENTRY *pEntry = NULL;
	int nrescount = GetVideoResArray(&pEntry);
	int width, height;
	g_onvif_cfg.video_enc_cfg.resolution_num = nrescount;

	for( i = 0; i < nrescount; i++)
	{
		if( strncmp(pEntry[i].codec_name, "H264", strlen("H264")) == 0 )
			g_onvif_cfg.video_enc_cfg.h264_sup = EXIST;
		/*else if( strncmp(pEntry[i].codec_name, "H265+", strlen("H265+")) == 0 )
			g_onvif_cfg.video_enc_cfg.h265p_sup = EXIST;*/
		else if( strncmp(pEntry[i].codec_name, "H265", strlen("H265")) == 0 )
			g_onvif_cfg.video_enc_cfg.h265_sup = EXIST;
		else if( strncmp(pEntry[i].codec_name, "MJPEG", strlen("MJPEG")) == 0 )
			g_onvif_cfg.video_enc_cfg.mjpg_sup = EXIST;

	}
	
	for( i = 0; i < nrescount; i++)
	{
		if (pEntry[i].stream_type == 0)
			OnvifGetVideoSize(pEntry[i].res_name, 0, &width, &height);
		else
			OnvifGetVideoSize(pEntry[i].res_name, 1, &width, &height);
		
		strcpy(g_onvif_cfg.video_enc_cfg.resolution[i].encoding, pEntry[i].codec_name);
		g_onvif_cfg.video_enc_cfg.resolution[i].channle = pEntry[i].stream_type;
		g_onvif_cfg.video_enc_cfg.resolution[i].w = width;
		g_onvif_cfg.video_enc_cfg.resolution[i].h = height;
		g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_min = pEntry[i].min_framerate;
		g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_max = pEntry[i].max_framerate;
		g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min = pEntry[i].min_bitrate;
		g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max = pEntry[i].max_bitrate;
	}
	
	g_onvif_cfg.video_enc_cfg.quality_min = 0;
	g_onvif_cfg.video_enc_cfg.quality_max = 100;
	g_onvif_cfg.video_enc_cfg.gov_length_min = 1;
	g_onvif_cfg.video_enc_cfg.gov_length_max = 200;
	g_onvif_cfg.video_enc_cfg.frame_sup_min = 1;
	g_onvif_cfg.video_enc_cfg.frame_sup_max = 30;
//////////////////////////////////audio_source_config
	
//////////////////////////////////image
	// image setting
	g_onvif_cfg.img_cfg.BacklightCompensation_Mode = 0;
	g_onvif_cfg.img_cfg.Brightness = 50;
	g_onvif_cfg.img_cfg.ColorSaturation = 50;
	g_onvif_cfg.img_cfg.Contrast = 50;
	g_onvif_cfg.img_cfg.Exposure_Mode = 0;
	g_onvif_cfg.img_cfg.MinExposureTime = 10;
	g_onvif_cfg.img_cfg.MaxExposureTime = 40000;
	g_onvif_cfg.img_cfg.MinGain = 0;
	g_onvif_cfg.img_cfg.MaxGain = 100;
	g_onvif_cfg.img_cfg.IrCutFilter_Mode = 2;
	g_onvif_cfg.img_cfg.Sharpness = 50;
	g_onvif_cfg.img_cfg.WideDynamicRange_Mode = 0;
	g_onvif_cfg.img_cfg.WhiteBalance_Mode = 0;
	g_onvif_cfg.img_cfg.WideDynamicRange_Level = 50;

	// image config options
	g_onvif_cfg.img_opt.BacklightCompensation_OFF = 1;
	g_onvif_cfg.img_opt.BacklightCompensation_ON = 1;
	g_onvif_cfg.img_opt.Brightness_min = 0;
	g_onvif_cfg.img_opt.Brightness_max = 100;
	g_onvif_cfg.img_opt.ColorSaturation_min = 0;
	g_onvif_cfg.img_opt.ColorSaturation_max = 100;
	g_onvif_cfg.img_opt.Contrast_min = 0;
	g_onvif_cfg.img_opt.Contrast_max = 100;
	g_onvif_cfg.img_opt.Exposure_AUTO = 1;
	g_onvif_cfg.img_opt.Exposure_MANUAL = 0;
	g_onvif_cfg.img_opt.MinExposureTime_min = 10;
	g_onvif_cfg.img_opt.MinExposureTime_max = 10;
	g_onvif_cfg.img_opt.MaxExposureTime_min = 10;
	g_onvif_cfg.img_opt.MaxExposureTime_max = 320000;
	g_onvif_cfg.img_opt.MinGain_min = 0;
	g_onvif_cfg.img_opt.MinGain_max = 0;
	g_onvif_cfg.img_opt.MaxGain_min = 0;
	g_onvif_cfg.img_opt.MaxGain_max = 100;
	g_onvif_cfg.img_opt.IrCutFilter_ON = 1;
	g_onvif_cfg.img_opt.IrCutFilter_OFF = 1;
	g_onvif_cfg.img_opt.IrCutFilter_AUTO = 1;
	g_onvif_cfg.img_opt.Sharpness_min = 0;
	g_onvif_cfg.img_opt.Sharpness_max = 100;
	g_onvif_cfg.img_opt.WideDynamicRange_OFF = 1;
	g_onvif_cfg.img_opt.WideDynamicRange_ON = 1;
	g_onvif_cfg.img_opt.WideDynamicRange_Level_min = 0;
	g_onvif_cfg.img_opt.WideDynamicRange_Level_max = 100;
	g_onvif_cfg.img_opt.WhiteBalance_AUTO = 1;
	g_onvif_cfg.img_opt.WhiteBalance_MANUAL = 0;

//////////////////////////////////ptz
	g_onvif_cfg.ptz_enable = 1;
	
//////////////////////////////////
	return ;
}

void onvif_init()/*��ʼ����������*/
{
	g_product_type = product_type_read();

    char szCustomName[32] = {0};
	read_file_to_string("/etc/flag.customize", szCustomName, 32);
	if(strstr(szCustomName, "_WTD") != NULL)
	{
		g_bWTDVersion = 1;
	}

	if(MsgGetMediaStreamConfig(&gStreamCfg))
	{	
		printf("MsgGetMediaStreamConfig failed!\n");
		exit(0);
	}
	if(MsgGetMediaConfig(&gMediaCfg) != 0)
	{
		printf("MsgGetMediaConfig failed!\n");
		exit(0);
	}
	memset(&g_onvif_cfg, 0, sizeof(ONVIF_CFG));
	memset(&g_onvif_cls, 0, sizeof(ONVIF_CLS));
	
	onvif_init_cfg();//���ó�ʼ��

	onvif_init_net();//�����ʼ��
	
	//onvif_eua_init();//�¼���ʼ��
	
	return ;
}


