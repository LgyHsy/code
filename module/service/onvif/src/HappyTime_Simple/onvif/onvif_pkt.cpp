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

/***************************************************************************************/
#include "sys_inc.h"
#include "onvif.h"
#include "onvif_device.h"
#include "onvif_pkt.h"
#include "onvif_event.h"
#include "onvif_ptz.h"
#include "onvif_util.h"
#include "onvif_err.h"
#include "onvif_media.h"

#if __WIN32_OS__
#pragma warning(disable:4996)
#endif

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern int g_product_type;
extern int g_bWTDVersion;

extern void get_onvif_serial_number();

/***************************************************************************************/
char xml_hdr[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

char onvif_xmlns[] = 
	"<s:Envelope "
	"xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
	"xmlns:e=\"http://www.w3.org/2003/05/soap-encoding\" "
	"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
	"xmlns:wsa=\"http://www.w3.org/2005/08/addressing\" " 
	"xmlns:xmime=\"http://www.w3.org/2005/05/xmlmime\" "
	"xmlns:tns1=\"http://www.onvif.org/ver10/topics\" "
	"xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" "
	"xmlns:xop=\"http://www.w3.org/2004/08/xop/include\" " 
	"xmlns:tt=\"http://www.onvif.org/ver10/schema\" " 
	"xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" " 
	"xmlns:wstop=\"http://docs.oasis-open.org/wsn/t-1\" " 
	"xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" " 
	"xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" "
	"xmlns:trt2=\"http://www.onvif.org/ver20/media/wsdl\" "
	"xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
	"xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" "
	"xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" "
	"xmlns:ter=\"http://www.onvif.org/ver10/error\" >";

char soap_head[] = 
	"<s:Header>"
		"<wsa:Action>%s</wsa:Action>"
	"</s:Header>";		

char soap_body[] = 
	"<s:Body>";

char soap_tailer[] =
	"</s:Body></s:Envelope>";


/***************************************************************************************/

int build_err_rly_xml
(
char * p_buf, 
int mlen, 
const char * code, 
const char * subcode, 
const char * subcode_ex, 
const char * reason
)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<s:Fault><s:Code><s:Value>%s</s:Value>", code);
	offset += snprintf(p_buf+offset, mlen-offset, "<s:Subcode><s:Value>%s</s:Value>", subcode);
	if (NULL != subcode_ex)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<s:Subcode><s:Value>%s</s:Value></s:Subcode>", subcode_ex);
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</s:Subcode></s:Code>");
	offset += snprintf(p_buf+offset, mlen-offset, "<s:Reason><s:Text xml:lang=\"en\">%s</s:Text></s:Reason></s:Fault>", reason);

	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}


int build_GetDeviceInformation_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	get_onvif_serial_number();

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetDeviceInformationResponse>"
			"<tds:Manufacturer>%s</tds:Manufacturer>"
			"<tds:Model>%s</tds:Model>"
			"<tds:FirmwareVersion>%s</tds:FirmwareVersion>"
			"<tds:SerialNumber>%s</tds:SerialNumber>"
			"<tds:HardwareId>%s</tds:HardwareId>"
		"</tds:GetDeviceInformationResponse>", 
		g_onvif_cfg.manufacturer, g_onvif_cfg.model, 
		g_onvif_cfg.firmware_version, g_onvif_cfg.serial_number, g_onvif_cfg.hardware_id);
		
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

char video_source_config[] =
	"<tt:Name>%s</tt:Name>"
	"<tt:UseCount>%d</tt:UseCount>"
	"<tt:SourceToken>%s</tt:SourceToken>"
	"<tt:Bounds height=\"%d\" width=\"%d\" y=\"%d\" x=\"%d\"></tt:Bounds>";

char audio_source_config[] =
	"<tt:Name>%s</tt:Name>"
	"<tt:UseCount>%d</tt:UseCount>"
	"<tt:SourceToken>%s</tt:SourceToken>";


char audio_encoder_config[] =
	"<tt:Name>%s</tt:Name>"
	"<tt:UseCount>%d</tt:UseCount>"
	"<tt:Encoding>%s</tt:Encoding>"
	"<tt:Bitrate>%d</tt:Bitrate>"
	"<tt:SampleRate>%d</tt:SampleRate>"
	"<tt:Multicast>"
	"<tt:Address>"
	"<tt:Type>IPv4</tt:Type>"
	"<tt:IPv4Address>%s</tt:IPv4Address>"
	"</tt:Address>"
	"<tt:Port>80</tt:Port>"
	"<tt:TTL>64</tt:TTL>"
	"<tt:AutoStart>false</tt:AutoStart>"
	"</tt:Multicast>"
	"<tt:SessionTimeout>PT%dS</tt:SessionTimeout>"; 

int build_VideoEncoderConfiguration_xml_2(char * p_buf, int mlen, ONVIF_V_ENC  * video_encoder)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>"
		"<tt:UseCount>%d</tt:UseCount>"
		"<tt:Encoding>%s</tt:Encoding>"
		"<tt:Resolution>"
			"<tt:Width>%d</tt:Width>"
			"<tt:Height>%d</tt:Height>"
		"</tt:Resolution>"
		"<tt:Quality>%d</tt:Quality>"
		"<tt:RateControl>"
			"<tt:FrameRateLimit>%d</tt:FrameRateLimit>"
			"<tt:EncodingInterval>%d</tt:EncodingInterval>"
			"<tt:BitrateLimit>%d</tt:BitrateLimit>"
		"</tt:RateControl>", 
		video_encoder->name, video_encoder->use_count, onvif_get_video_encoding_str(video_encoder->encoding), 
		video_encoder->width, video_encoder->height, video_encoder->quality, video_encoder->framerate_limit,
		video_encoder->encoding_interval, video_encoder->bitrate_limit);

	/*offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:MPEG4>"
			"<tt:GovLength>0</tt:GovLength>"
			"<tt:Mpeg4Profile>SP</tt:Mpeg4Profile>"
		"</tt:MPEG4>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:H264>"
			"<tt:GovLength>%d</tt:GovLength>"
			"<tt:H264Profile>High</tt:H264Profile>"
		"</tt:H264>", 
		video_encoder->gov_len);*/
	/*if (VIDEO_ENCODING_H264 == video_encoder->encoding)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264><tt:GovLength>%d</tt:GovLength>"
			"<tt:H264Profile>Main</tt:H264Profile></tt:H264>", video_encoder->gov_len);
	}
	else if (VIDEO_ENCODING_H265 == video_encoder->encoding)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H265><tt:GovLength>%d</tt:GovLength>"
			"<tt:H265Profile>Main</tt:H265Profile></tt:H265>", video_encoder->gov_len);
	}
	else if (VIDEO_ENCODING_JPEG == video_encoder->encoding)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:MJPEG><tt:GovLength>%d</tt:GovLength>"
			"<tt:MJPEGProfile>Main</tt:MJPEGProfile></tt:MJPEG>", video_encoder->gov_len);
	}*/

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Multicast>"
			"<tt:Address>"
				"<tt:Type>IPv4</tt:Type>"
				"<tt:IPv4Address>%s</tt:IPv4Address>"
			"</tt:Address>"
			"<tt:Port>0</tt:Port>"
			"<tt:TTL>0</tt:TTL>"
			"<tt:AutoStart>false</tt:AutoStart>"
		"</tt:Multicast>"
		"<tt:SessionTimeout>PT%dS</tt:SessionTimeout>", 
		g_onvif_cfg.serv_ip, video_encoder->session_timeout);

	return offset;	  
}

int build_VideoEncoderConfiguration_xml(char * p_buf, int mlen, ONVIF_V_ENC  * video_encoder)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Name>%s</tt:Name>"
		"<tt:UseCount>%d</tt:UseCount>"
		"<tt:Encoding>H264</tt:Encoding>"
		"<tt:Resolution>"
			"<tt:Width>%d</tt:Width>"
			"<tt:Height>%d</tt:Height>"
		"</tt:Resolution>"
		"<tt:Quality>%d</tt:Quality>"
		"<tt:RateControl>"
			"<tt:FrameRateLimit>%d</tt:FrameRateLimit>"
			"<tt:EncodingInterval>%d</tt:EncodingInterval>"
			"<tt:BitrateLimit>%d</tt:BitrateLimit>"
		"</tt:RateControl>", 
		video_encoder->name, video_encoder->use_count,//, onvif_get_video_encoding_str(video_encoder->encoding), 
		video_encoder->width, video_encoder->height, video_encoder->quality, video_encoder->framerate_limit,
		video_encoder->encoding_interval, video_encoder->bitrate_limit);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:MPEG4>"
			"<tt:GovLength>0</tt:GovLength>"
			"<tt:Mpeg4Profile>SP</tt:Mpeg4Profile>"
		"</tt:MPEG4>");
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:H264>"
			"<tt:GovLength>%d</tt:GovLength>"
			"<tt:H264Profile>High</tt:H264Profile>"
		"</tt:H264>", video_encoder->gov_len);

	/*if (VIDEO_ENCODING_H264 == video_encoder->encoding)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264><tt:GovLength>%d</tt:GovLength>"
			"<tt:H264Profile>Main</tt:H264Profile></tt:H264>", video_encoder->gov_len);
	}
	else if (VIDEO_ENCODING_H265 == video_encoder->encoding)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H265><tt:GovLength>%d</tt:GovLength>"
			"<tt:H265Profile>Main</tt:H265Profile></tt:H265>", video_encoder->gov_len);
	}
	else if (VIDEO_ENCODING_JPEG == video_encoder->encoding)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:MJPEG><tt:GovLength>%d</tt:GovLength>"
			"<tt:MJPEGProfile>Main</tt:MJPEGProfile></tt:MJPEG>", video_encoder->gov_len);
	}*/

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:Multicast>"
			"<tt:Address>"
				"<tt:Type>IPv4</tt:Type>"
				"<tt:IPv4Address>%s</tt:IPv4Address>"
			"</tt:Address>"
			"<tt:Port>0</tt:Port>"
			"<tt:TTL>0</tt:TTL>"
			"<tt:AutoStart>false</tt:AutoStart>"
		"</tt:Multicast>"
		"<tt:SessionTimeout>PT%dS</tt:SessionTimeout>",
		g_onvif_cfg.serv_ip, video_encoder->session_timeout);

	return offset;    
}

int build_PTZCfg_xml(char * p_buf, int mlen, PTZ_NODE * p_node, const char * name_space)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, 
			"<%s:PTZConfiguration token=\"%s\">"
		"<tt:Name>%s</tt:Name>"
		"<tt:UseCount>1</tt:UseCount>"
		"<tt:NodeToken>%s</tt:NodeToken>", name_space, 
		p_node->config.token, p_node->config.name, p_node->config.token);//p_node->token);
		
	offset += snprintf(p_buf+offset, mlen-offset,  	
		"<tt:DefaultAbsolutePantTiltPositionSpace>"
			"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace"
		"</tt:DefaultAbsolutePantTiltPositionSpace>"
		"<tt:DefaultAbsoluteZoomPositionSpace>"
			"http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace"
		"</tt:DefaultAbsoluteZoomPositionSpace>"
		"<tt:DefaultRelativePanTiltTranslationSpace>"
			"http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace"
		"</tt:DefaultRelativePanTiltTranslationSpace>"
		"<tt:DefaultRelativeZoomTranslationSpace>"
			"http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace"
		"</tt:DefaultRelativeZoomTranslationSpace>"
		"<tt:DefaultContinuousPanTiltVelocitySpace>"
			 	"http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace"
		"</tt:DefaultContinuousPanTiltVelocitySpace>"
		"<tt:DefaultContinuousZoomVelocitySpace>"
			"http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace"
		"</tt:DefaultContinuousZoomVelocitySpace>");

	offset += snprintf(p_buf+offset, mlen-offset,  	    
		"<tt:DefaultPTZSpeed>"
			"<tt:PanTilt x=\"%0.1f\" y=\"%0.1f\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/GenericSpeedSpace\" />"
			"<tt:Zoom x=\"%0.1f\" space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/ZoomGenericSpeedSpace\" />"
		"</tt:DefaultPTZSpeed>", 
		p_node->config.def_pantilt_speed_x, p_node->config.def_pantilt_speed_y, p_node->config.def_zoom_speed);
		
	offset += snprintf(p_buf+offset, mlen-offset,  	 	
		"<tt:DefaultPTZTimeout>PT%dS</tt:DefaultPTZTimeout>", p_node->config.def_timeout);

	offset += snprintf(p_buf+offset, mlen-offset,  	 	
		"<tt:PanTiltLimits>"
			"<tt:Range>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
				"<tt:YRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:YRange>"
			"</tt:Range>"
		"</tt:PanTiltLimits>",
		p_node->config.pantilt_limits_x.min, p_node->config.pantilt_limits_x.max,
		p_node->config.pantilt_limits_y.min, p_node->config.pantilt_limits_y.max);

	offset += snprintf(p_buf+offset, mlen-offset,  	 	
		"<tt:ZoomLimits>"
			"<tt:Range>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:Range>"
		"</tt:ZoomLimits>",
		p_node->config.zoom_limits.min, p_node->config.zoom_limits.max);

	offset += snprintf(p_buf+offset, mlen-offset, "</%s:PTZConfiguration>", name_space);

	return offset;
}
    
int build_GetProfiles_2_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * profile = g_onvif_cfg.profiles;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetProfilesResponse>");
	
	while (profile)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<trt2:Profiles fixed=\"%s\" token=\"%s\">"
			"<trt2:Name>%s</trt2:Name>"
			"<trt2:Configurations>",
			profile->fixed ? "true" : "false", profile->token, profile->name);

		if (profile->video_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<trt2:VideoSource token=\"%s\">", profile->video_src->token);
			offset += snprintf(p_buf+offset, mlen-offset, video_source_config, 
				profile->video_src->name, profile->video_src->use_count, profile->video_src->source_token, 
				profile->video_src->height, profile->video_src->width, 
				profile->video_src->y, profile->video_src->x);    
			offset += snprintf(p_buf+offset, mlen-offset, "</trt2:VideoSource>");
		}

		if (profile->audio_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<trt2:AudioSource token=\"%s\">", profile->audio_src->token);
			offset += snprintf(p_buf+offset, mlen-offset, audio_source_config, 
				profile->audio_src->name, profile->audio_src->use_count, profile->audio_src->source_token);
			offset += snprintf(p_buf+offset, mlen-offset, "</trt2:AudioSource>");
		}

		if (profile->video_enc)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<trt2:VideoEncoder token=\"%s\">", profile->video_enc->token);
			offset += build_VideoEncoderConfiguration_xml_2(p_buf+offset, mlen-offset, profile->video_enc);
			offset += snprintf(p_buf+offset, mlen-offset, "</trt2:VideoEncoder>");
		}

		if (profile->audio_enc)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<trt2:AudioEncoder token=\"%s\">", profile->audio_enc->token);
			offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config, 
				profile->audio_enc->name, profile->audio_enc->use_count, onvif_get_audio_encoding_str(profile->audio_enc->encoding), 
				profile->audio_enc->bitrate, profile->audio_enc->sample_rate, g_onvif_cfg.serv_ip, profile->audio_enc->session_timeout); 
			offset += snprintf(p_buf+offset, mlen-offset, "</trt2:AudioEncoder>");
		}

		if (profile->ptz_node)
		{
			offset += build_PTZCfg_xml(p_buf+offset, mlen-offset, profile->ptz_node, "trt2");
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Configurations>"
		"</trt2:Profiles>");

		profile = profile->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:GetProfilesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	//printf("\n%s\n", p_buf);

	return offset;
}

int build_GetProfiles_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * profile = g_onvif_cfg.profiles;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetProfilesResponse>");
	
	while (profile)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<trt:Profiles fixed=\"%s\" token=\"%s\"><tt:Name>%s</tt:Name>",
			profile->fixed ? "true" : "false", profile->token, profile->name);

		if (profile->video_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:VideoSourceConfiguration token=\"%s\">", 
				profile->video_src->token);
			offset += snprintf(p_buf+offset, mlen-offset, video_source_config,
				profile->video_src->name, profile->video_src->use_count, profile->video_src->source_token, 
				profile->video_src->height, profile->video_src->width, 
				profile->video_src->y, profile->video_src->x);    
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoSourceConfiguration>");
		}

		if (profile->audio_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:AudioSourceConfiguration token=\"%s\">", profile->audio_src->token);
			offset += snprintf(p_buf+offset, mlen-offset, audio_source_config,
				profile->audio_src->name, profile->audio_src->use_count, profile->audio_src->source_token);
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioSourceConfiguration>");
		}

		if (profile->video_enc)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoEncoderConfiguration token=\"%s\">", profile->video_enc->token);
			offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, profile->video_enc);
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoEncoderConfiguration>");
		}

		if (profile->audio_enc)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:AudioEncoderConfiguration token=\"%s\">", profile->audio_enc->token);
			offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config,
				profile->audio_enc->name, profile->audio_enc->use_count, onvif_get_audio_encoding_str(profile->audio_enc->encoding), 
				profile->audio_enc->bitrate, profile->audio_enc->sample_rate, g_onvif_cfg.serv_ip, 
				profile->audio_enc->session_timeout); 
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioEncoderConfiguration>");
		}

		if (profile->ptz_node)
		{
			offset += build_PTZCfg_xml(p_buf+offset, mlen-offset, profile->ptz_node, "tt");
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Profiles>");

		profile = profile->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetProfilesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	//printf("\n%s\n", p_buf);

	return offset;
}

int build_GetProfile_rly_xml(char * p_buf, int mlen, const char * token)
{
	ONVIF_PROFILE * profile = onvif_find_profile(token);
	if (NULL == profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetProfileResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:Profile fixed=\"%s\" token=\"%s\"><tt:Name>%s</tt:Name>",
		profile->fixed ? "true" : "false", profile->token, profile->name);

	if (profile->video_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:VideoSourceConfiguration token=\"%s\">", profile->video_src->token);
		offset += snprintf(p_buf+offset, mlen-offset, video_source_config, profile->video_src->name, 
			profile->video_src->use_count, profile->video_src->source_token, 
			profile->video_src->height, profile->video_src->width, 
			profile->video_src->y, profile->video_src->x);    
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoSourceConfiguration>");
	}

	if (profile->audio_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AudioSourceConfiguration token=\"%s\">", profile->audio_src->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_source_config, profile->audio_src->name, 
			profile->audio_src->use_count, profile->audio_src->source_token);    
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioSourceConfiguration>");
	}

	if (profile->video_enc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoEncoderConfiguration token=\"%s\">", profile->video_enc->token);
		offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, profile->video_enc);
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoEncoderConfiguration>");
	}

	if (profile->audio_enc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AudioEncoderConfiguration token=\"%s\">", profile->audio_enc->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config, profile->audio_enc->name, 
			profile->audio_enc->use_count, onvif_get_audio_encoding_str(profile->audio_enc->encoding), 
			profile->audio_enc->bitrate, profile->audio_enc->sample_rate, g_onvif_cfg.serv_ip, 
			profile->audio_enc->session_timeout); 
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioEncoderConfiguration>");
	}

	if (profile->ptz_node)
	{
		offset += build_PTZCfg_xml(p_buf+offset, mlen-offset, profile->ptz_node, "tt");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Profile>");


	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetProfileResponse>");            
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	//printf("\n%s\n", p_buf);

	return offset;
}

int build_CreateProfile_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * profile = onvif_find_profile(argv);
	if (NULL == profile)
	{
		return -1;
	}

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:CreateProfileResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:Profile fixed=\"%s\" token=\"%s\"><tt:Name>%s</tt:Name>",
		profile->fixed ? "true" : "false", profile->token, profile->name);

	if (profile->video_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:VideoSourceConfiguration token=\"%s\">", profile->video_src->token);
		offset += snprintf(p_buf+offset, mlen-offset, video_source_config, profile->video_src->name, 
			profile->video_src->use_count, profile->video_src->source_token, 
			profile->video_src->height, profile->video_src->width, 
			profile->video_src->y, profile->video_src->x);    
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoSourceConfiguration>");	            
	}

	if (profile->audio_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AudioSourceConfiguration token=\"%s\">", profile->audio_src->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_source_config, profile->audio_src->name, 
			profile->audio_src->use_count, profile->audio_src->source_token);    
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioSourceConfiguration>");	            
	}

	if (profile->video_enc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoEncoderConfiguration token=\"%s\">", profile->video_enc->token);
		offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, profile->video_enc); 
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:VideoEncoderConfiguration>");	            
	}

	if (profile->audio_enc)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AudioEncoderConfiguration token=\"%s\">", profile->audio_enc->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config, profile->audio_enc->name, 
			profile->audio_enc->use_count, onvif_get_audio_encoding_str(profile->audio_enc->encoding), 
			profile->audio_enc->bitrate, profile->audio_enc->sample_rate, g_onvif_cfg.serv_ip, profile->audio_enc->session_timeout); 
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:AudioEncoderConfiguration>");	            
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Profile>");


	offset += snprintf(p_buf+offset, mlen-offset, "</trt:CreateProfileResponse>");            
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_DeleteProfile_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:DeleteProfileResponse></trt:DeleteProfileResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_AddVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddVideoSourceConfigurationResponse></trt:AddVideoSourceConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RemoveVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveVideoSourceConfigurationResponse></trt:RemoveVideoSourceConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_AddAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddAudioSourceConfigurationResponse></trt:AddAudioSourceConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RemoveAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveAudioSourceConfigurationResponse></trt:RemoveAudioSourceConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_AddVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddVideoEncoderConfigurationResponse></trt:AddVideoEncoderConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RemoveVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveVideoEncoderConfigurationResponse></trt:RemoveVideoEncoderConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_AddAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddAudioEncoderConfigurationResponse></trt:AddAudioEncoderConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RemoveAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemoveAudioEncoderConfigurationResponse></trt:RemoveAudioEncoderConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetStreamUri_rly_xml_2(char * p_buf, int mlen, const char * token)
{
	char username[256]="";
	char password[256]="";
	BOOL bNoAuth = TRUE;
	if(bNoAuth)
	{
		MediaStreamConfig streamCfg;
		UserConfig usrCfg;
		int j;
		
		if(!MsgGetMediaStreamConfig(&streamCfg))
		{			
			if(streamCfg.rtspConfig.rtsp_auth && !MsgGetUserConfig(&usrCfg))
			{
				for(j = 0; j < usrCfg.count; j++)
				{
					if(!strcmp(usrCfg.accounts[j].group.groupName, "Administrator") && !strcmp(usrCfg.accounts[j].status, "Enable"))
					{
						strcpy(username, usrCfg.accounts[j].userName);
						our_md5_encode(password, (const unsigned char *)usrCfg.accounts[j].password, strlen(usrCfg.accounts[j].password));
						
						printf("%s:%s:%s\n", username, usrCfg.accounts[j].password, password);
						break;
					}
				}
			}
		}
	}
	ONVIF_PROFILE * profile = onvif_find_profile(token);
	if (NULL == profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	int stream = 1;

	if (strstr(profile->token, "MainStream") != NULL)
		stream = 0;
	else if (strstr(profile->token, "SubStream") != NULL)
		stream = 1;
	else if (strstr(profile->token, "ThirdStream") != NULL)
		stream = 2;

	if (g_product_type == PRODUCT_TYPE_MYQ11
		|| g_product_type == PRODUCT_TYPE_MYQ11B
		|| g_product_type == PRODUCT_TYPE_MYQ12
		|| g_product_type == PRODUCT_TYPE_MYQ12D
		|| g_product_type == PRODUCT_TYPE_MYA12
		|| g_product_type == PRODUCT_TYPE_MYA20
		|| g_product_type == PRODUCT_TYPE_T30G
		|| g_product_type == PRODUCT_TYPE_MYV25
		|| g_product_type == PRODUCT_TYPE_MYA5
		|| access("/opt/ch/dual_sensor.flag", F_OK) == 0	
		|| g_product_type == PRODUCT_TYPE_MYQ10
		|| g_product_type == PRODUCT_TYPE_MYA25)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt2:GetStreamUriResponse>"
			"<trt2:Uri>rtsp://%s/ch02/stream%d?username=%s&amp;password=%s</trt2:Uri>"
		"</trt2:GetStreamUriResponse>",
		g_onvif_cls.local_ipstr, stream, username, password);
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt2:GetStreamUriResponse>"
			"<trt2:Uri>rtsp://%s:%d/stream%d?username=%s&amp;password=%s</trt2:Uri>"
		"</trt2:GetStreamUriResponse>",
		g_onvif_cls.local_ipstr, get_rtsp_port(), stream, username, password);
	}
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetStreamUri_rly_xml(char * p_buf, int mlen, const char * token)
{
	char username[256]="";
	char password[256]="";
	BOOL bNoAuth = TRUE;
	if( bNoAuth) 
	{
		MediaStreamConfig streamCfg;			
		UserConfig usrCfg;
		int j;
		
		if(!MsgGetMediaStreamConfig(&streamCfg))
		{			
			if(streamCfg.rtspConfig.rtsp_auth && !MsgGetUserConfig(&usrCfg))
			{
				for(j = 0; j < usrCfg.count; j++)
				{
					if(!strcmp(usrCfg.accounts[j].group.groupName, "Administrator") && !strcmp(usrCfg.accounts[j].status, "Enable"))
					{
						strcpy(username, usrCfg.accounts[j].userName);
						our_md5_encode(password, (const unsigned char *)usrCfg.accounts[j].password, strlen(usrCfg.accounts[j].password));
						
						printf("%s:%s:%s\n", username, usrCfg.accounts[j].password, password);
						break;
					}
				}
			}
		}
	}
	ONVIF_PROFILE * profile = onvif_find_profile(token);
	if (NULL == profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	int stream = 1;
	if (strstr(profile->token, "MainStream") != NULL)
		stream = 0;
	else if (strstr(profile->token, "SubStream") != NULL)
		stream = 1;
	else if (strstr(profile->token, "ThirdStream") != NULL)
		stream = 2;

	if (g_product_type == PRODUCT_TYPE_MYQ11
		|| g_product_type == PRODUCT_TYPE_MYQ11B
		|| g_product_type == PRODUCT_TYPE_MYQ12
		|| g_product_type == PRODUCT_TYPE_MYQ12D
		|| g_product_type == PRODUCT_TYPE_MYA12
		|| g_product_type == PRODUCT_TYPE_MYA20
		|| g_product_type == PRODUCT_TYPE_T30G
		|| g_product_type == PRODUCT_TYPE_MYV25
		|| g_product_type == PRODUCT_TYPE_MYQ10
        || g_product_type == PRODUCT_TYPE_MYA5 
        || access("/opt/ch/dual_sensor.flag", F_OK) == 0
		|| g_product_type == PRODUCT_TYPE_MYA25)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetStreamUriResponse>"
			"<trt:MediaUri>"
				"<tt:Uri>rtsp://%s/ch02/stream%d?username=%s&amp;password=%s</tt:Uri>"
				"<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
				"<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
				"<tt:Timeout>PT60S</tt:Timeout>"
			"</trt:MediaUri>"
		"</trt:GetStreamUriResponse>", g_onvif_cls.local_ipstr, stream, username, password);
	}
	else
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetStreamUriResponse>"
			"<trt:MediaUri>"
				"<tt:Uri>rtsp://%s:%d/stream%d?username=%s&amp;password=%s</tt:Uri>"
				"<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
				"<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
				"<tt:Timeout>PT60S</tt:Timeout>"
			"</trt:MediaUri>"
		"</trt:GetStreamUriResponse>", g_onvif_cls.local_ipstr, get_rtsp_port(), stream, username, password);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetSnapshotUri_rly_xml_2(char * p_buf, int mlen, const char * profile_token)
{
	char username[256]="";
	char password[256]="";
	BOOL bNoAuth = TRUE;
	if( bNoAuth) 
	{
		MediaStreamConfig streamCfg;			
		UserConfig usrCfg;
		int j;
		
		if(!MsgGetMediaStreamConfig(&streamCfg))
		{			
			if(streamCfg.rtspConfig.rtsp_auth && !MsgGetUserConfig(&usrCfg))
			{
				for(j = 0; j < usrCfg.count; j++)
				{
					if(!strcmp(usrCfg.accounts[j].group.groupName, "Administrator") && !strcmp(usrCfg.accounts[j].status, "Enable"))
					{
						strcpy(username, usrCfg.accounts[j].userName);
						our_md5_encode(password, (const unsigned char *)usrCfg.accounts[j].password, strlen(usrCfg.accounts[j].password));
						
						printf("%s:%s:%s\n", username, usrCfg.accounts[j].password, password);
						break;
					}
				}
			}
		}
	}
	ONVIF_PROFILE * p_profile = onvif_find_profile(profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	int stream = 1;
	if (strstr(p_profile->token, "MainStream") != NULL)
	{
		stream = 0;
	}
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt2:GetSnapshotUriResponse>"
			//"<trt2:MediaUri>"
				"<trt2:Uri>http://%s:%d/cgi-bin/snapshot.cgi?stream=%d?username=%s&amp;password=%s</trt2:Uri>"
				//"<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
				//"<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
				//"<tt:Timeout>PT5S</tt:Timeout>"
			//"</trt2:MediaUri>"
		"</trt2:GetSnapshotUriResponse>",
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port, stream, username, password);
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetSnapshotUri_rly_xml(char * p_buf, int mlen, const char * profile_token)
{
	char username[256]="";
	char password[256]="";
	BOOL bNoAuth = TRUE;
	if( bNoAuth) 
	{
		MediaStreamConfig streamCfg;			
		UserConfig usrCfg;
		int j;
		
		if(!MsgGetMediaStreamConfig(&streamCfg))
		{			
			if(streamCfg.rtspConfig.rtsp_auth && !MsgGetUserConfig(&usrCfg))
			{
				for(j = 0; j < usrCfg.count; j++)
				{
					if(!strcmp(usrCfg.accounts[j].group.groupName, "Administrator") && !strcmp(usrCfg.accounts[j].status, "Enable"))
					{
						strcpy(username, usrCfg.accounts[j].userName);
						our_md5_encode(password, (const unsigned char *)usrCfg.accounts[j].password, strlen(usrCfg.accounts[j].password));
						
						printf("%s:%s:%s\n", username, usrCfg.accounts[j].password, password);
						break;
					}
				}
			}
		}
	}
	ONVIF_PROFILE * p_profile = onvif_find_profile(profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	int stream = 1;
	if (strstr(p_profile->token, "MainStream") != NULL)
	{
		stream = 0;
	}
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<trt:GetSnapshotUriResponse>"
			"<trt:MediaUri>"
				"<tt:Uri>http://%s:%d/cgi-bin/snapshot.cgi?stream=%d?username=%s&amp;password=%s</tt:Uri>"
				"<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
				"<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
				"<tt:Timeout>PT5S</tt:Timeout>"
			"</trt:MediaUri>"
		"</trt:GetSnapshotUriResponse>",
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port, stream, username, password);
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}


char media_cap[] =     
	"<tt:Media>"
		"<tt:XAddr>http://%s:%d/onvif/media_service</tt:XAddr>"
		"<tt:StreamingCapabilities>"
			"<tt:RTPMulticast>true</tt:RTPMulticast>"
			"<tt:RTP_TCP>true</tt:RTP_TCP>"
			"<tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP>"
		"</tt:StreamingCapabilities>"
		"<tt:Extension>"
			"<tt:ProfileCapabilities>"
				"<tt:MaximumNumberOfProfiles>10</tt:MaximumNumberOfProfiles>"
			"</tt:ProfileCapabilities>"
		"</tt:Extension>"
	"</tt:Media>";

char device_cap[] = 
	"<tt:Device>"
	"<tt:XAddr>http://%s:%d/onvif/device_service</tt:XAddr>"
	"<tt:Network>"
		"<tt:IPFilter>false</tt:IPFilter>"
		"<tt:ZeroConfiguration>false</tt:ZeroConfiguration>"
		"<tt:IPVersion6>false</tt:IPVersion6>"
		"<tt:DynDNS>false</tt:DynDNS>"
	"</tt:Network>"
	"<tt:System>"
		"<tt:DiscoveryResolve>true</tt:DiscoveryResolve>"
		"<tt:DiscoveryBye>false</tt:DiscoveryBye>"
		"<tt:RemoteDiscovery>false</tt:RemoteDiscovery>"
		"<tt:SystemBackup>true</tt:SystemBackup>"
		"<tt:SystemLogging>true</tt:SystemLogging>"
		"<tt:FirmwareUpgrade>true</tt:FirmwareUpgrade>"
		"<tt:SupportedVersions>"
			"<tt:Major>2</tt:Major>"
			"<tt:Minor>42</tt:Minor>"
		"</tt:SupportedVersions>"
		"<tt:SupportedVersions>"
			"<tt:Major>2</tt:Major>"
			"<tt:Minor>20</tt:Minor>"
		"</tt:SupportedVersions>"
		"<tt:SupportedVersions>"
			"<tt:Major>2</tt:Major>"
			"<tt:Minor>10</tt:Minor>"
		"</tt:SupportedVersions>"
		"<tt:SupportedVersions>"
			"<tt:Major>2</tt:Major>"
			"<tt:Minor>0</tt:Minor>"
		"</tt:SupportedVersions>"
	"</tt:System>"
	"<tt:IO>"
		"<tt:InputConnectors>0</tt:InputConnectors>"
		"<tt:RelayOutputs>0</tt:RelayOutputs>"      
	"</tt:IO>"
	"<tt:Security>"
		"<tt:TLS1.1>false</tt:TLS1.1>"
		"<tt:TLS1.2>false</tt:TLS1.2>"
		"<tt:OnboardKeyGeneration>false</tt:OnboardKeyGeneration>"
		"<tt:AccessPolicyConfig>false</tt:AccessPolicyConfig>"
		"<tt:X.509Token>false</tt:X.509Token>"
		"<tt:SAMLToken>false</tt:SAMLToken>"
		"<tt:KerberosToken>false</tt:KerberosToken>"
		"<tt:RELToken>false</tt:RELToken>"
	"</tt:Security>"
	"</tt:Device>";

char event_cap[] = 
	"<tt:Events>"
	"<tt:XAddr>http://%s:%d/onvif/event_services</tt:XAddr>"
	"<tt:WSSubscriptionPolicySupport>false</tt:WSSubscriptionPolicySupport>"
	"<tt:WSPullPointSupport>false</tt:WSPullPointSupport>"
	"<tt:WSPausableSubscriptionManagerInterfaceSupport>false</tt:WSPausableSubscriptionManagerInterfaceSupport>"
	"</tt:Events>";

char ptz_cap[] = 
	"<tt:PTZ>"
		"<tt:XAddr>http://%s:%d/onvif/ptz_services</tt:XAddr>"
	"</tt:PTZ>";

char image_cap[] = 
	"<tt:Imaging>"
		"<tt:XAddr>http://%s:%d/onvif/image_services</tt:XAddr>"
	"</tt:Imaging>";

char analytics_cap[] = 
	"<tt:Analytics>"
		"<tt:XAddr>http://%s:%d/onvif/analytics_services</tt:XAddr>"
		"<tt:RuleSupport>false</tt:RuleSupport>"
		"<tt:AnalyticsModuleSupport>false</tt:AnalyticsModuleSupport>"
	"</tt:Analytics>";


int build_GetCapabilities_rly_xml(char * p_buf, int mlen, const char * argv)
{
	const int category = (const long)argv;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetCapabilitiesResponse><tds:Capabilities>");

	if (CAP_CATEGORY_MEDIA == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, media_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
	}
	else if (CAP_CATEGORY_DEVICE == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, device_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
	}
	else if (CAP_CATEGORY_ANALYTICS == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, analytics_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
	}
	else if (CAP_CATEGORY_EVENTS == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, event_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
	}
	else if (CAP_CATEGORY_IMAGE == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, image_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
	}
	else if (CAP_CATEGORY_PTZ == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, ptz_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
	}
	else
	{
		// offset += snprintf(p_buf+offset, mlen-offset, analytics_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	    
		offset += snprintf(p_buf+offset, mlen-offset, device_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
		offset += snprintf(p_buf+offset, mlen-offset, event_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
		offset += snprintf(p_buf+offset, mlen-offset, image_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
		offset += snprintf(p_buf+offset, mlen-offset, media_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	
		offset += snprintf(p_buf+offset, mlen-offset, ptz_cap, g_onvif_cls.local_ipstr, g_onvif_cls.local_port);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities></tds:GetCapabilitiesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetNetworkInterfaces_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_NET_INF * p_net_inf = g_onvif_cfg.network.interfaces;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNetworkInterfacesResponse>");
	
	while (p_net_inf)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tds:NetworkInterfaces token=\"%s\">"
			"<tt:Enabled>%s</tt:Enabled>"
			"<tt:Info>"
				"<tt:Name>%s</tt:Name>"
				"<tt:HwAddress>%s</tt:HwAddress>"
				//"<tt:MTU>%d</tt:MTU>"
			"</tt:Info>",
			p_net_inf->token, p_net_inf->enabled ? "true" : "false", 
			p_net_inf->name, p_net_inf->hwaddr);// p_net_inf->mtu);

		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:IPv4>"
				"<tt:Enabled>%s</tt:Enabled>"
				"<tt:Config>",
				p_net_inf->ipv4_enabled ? "true" : "false");

		if (p_net_inf->fromdhcp == FALSE)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:Manual>"
					"<tt:Address>%s</tt:Address>"
					"<tt:PrefixLength>%d</tt:PrefixLength>"
				"</tt:Manual>",
				p_net_inf->ipv4_addr, p_net_inf->prefix_len);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:FromDHCP>"
					"<tt:Address>%s</tt:Address>"
					"<tt:PrefixLength>%d</tt:PrefixLength>"
				"</tt:FromDHCP>",
				p_net_inf->ipv4_addr, p_net_inf->prefix_len);
		}
		
		offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:DHCP>%s</tt:DHCP>"
				"</tt:Config>"
			"</tt:IPv4>"
		"</tds:NetworkInterfaces>", 
		p_net_inf->fromdhcp ? "true" : "false");
		
		p_net_inf = p_net_inf->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetNetworkInterfacesResponse>");
		
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_SetNetworkInterfaces_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetNetworkInterfacesResponse>"
		"<tds:RebootNeeded>false</tds:RebootNeeded></tds:SetNetworkInterfacesResponse>");		
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

char video_sources[] = 
	"<tt:Framerate>%d</tt:Framerate>"
	"<tt:Resolution>"
	"<tt:Width>%d</tt:Width>"
	"<tt:Height>%d</tt:Height>"
	"</tt:Resolution>";

int build_GetVideoSources_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * video_source = g_onvif_cfg.video_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourcesResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:VideoSources token=\"%s\">", video_source->token);
	offset += snprintf(p_buf+offset, mlen-offset, video_sources, 25, video_source->width, video_source->height);	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:VideoSources>");

	/*while (video_source)
	{
		video_source = video_source->next;
		break;
	}*/
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourcesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioSources_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_A_SRC * audio_source = g_onvif_cfg.audio_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourcesResponse>");
	
	while (audio_source)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:AudioSources token=\"%s\">", audio_source->token);
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Channels>1</tt:Channels>");	
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:AudioSources>");

		audio_source = audio_source->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioSourcesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}


int build_GetVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * token)
{
	ONVIF_V_ENC * video_encoder = onvif_find_video_encoder(token);
	if (NULL == video_encoder)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoEncoderConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configuration token=\"%s\">", video_encoder->token);
	offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, video_encoder);	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configuration>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoEncoderConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * token)
{
	ONVIF_A_ENC * audio_encoder = onvif_find_audio_encoder(token);
	if (NULL == audio_encoder)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioEncoderConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configuration token=\"%s\">", audio_encoder->token);
	offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config, audio_encoder->name, 
			audio_encoder->use_count, onvif_get_audio_encoding_str(audio_encoder->encoding), 
			audio_encoder->bitrate, audio_encoder->sample_rate, g_onvif_cfg.serv_ip, audio_encoder->session_timeout); 
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configuration>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioEncoderConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_SetAudioEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetAudioDecoderConfigurationResponse></trt:SetAudioDecoderConfigurationResponse>");		    
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetVideoEncoderConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_ENC * video_encoder = g_onvif_cfg.video_enc;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetVideoEncoderConfigurationsResponse>");

	while (video_encoder)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Configurations token=\"%s\">", video_encoder->token);
		offset += build_VideoEncoderConfiguration_xml_2(p_buf+offset, mlen-offset, video_encoder);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Configurations>");

		video_encoder = video_encoder->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:GetVideoEncoderConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetVideoEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_ENC * video_encoder = g_onvif_cfg.video_enc;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoEncoderConfigurationsResponse>");

	while (video_encoder)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", video_encoder->token);
		offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, video_encoder);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		video_encoder = video_encoder->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoEncoderConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetCompatibleVideoEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	
	ONVIF_V_ENC * video_encoder = g_onvif_cfg.video_enc;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleVideoEncoderConfigurationsResponse>");

	while (video_encoder)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", video_encoder->token);
		offset += build_VideoEncoderConfiguration_xml(p_buf+offset, mlen-offset, video_encoder);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		video_encoder = video_encoder->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleVideoEncoderConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetAudioEncoderConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv)
{
	ONVIF_A_ENC * audio_encoder = g_onvif_cfg.audio_enc;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetAudioEncoderConfigurationsResponse>");

	while (audio_encoder)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Configurations token=\"%s\">", audio_encoder->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config, audio_encoder->name, 
				audio_encoder->use_count, onvif_get_audio_encoding_str(audio_encoder->encoding), 
				audio_encoder->bitrate, audio_encoder->sample_rate, g_onvif_cfg.serv_ip, audio_encoder->session_timeout);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Configurations>");
		
		audio_encoder = audio_encoder->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:GetAudioEncoderConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetAudioEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_A_ENC * audio_encoder = g_onvif_cfg.audio_enc;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioEncoderConfigurationsResponse>");

	while (audio_encoder)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", audio_encoder->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config, audio_encoder->name, 
				audio_encoder->use_count, onvif_get_audio_encoding_str(audio_encoder->encoding), 
				audio_encoder->bitrate, audio_encoder->sample_rate, g_onvif_cfg.serv_ip, audio_encoder->session_timeout);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");
		
		audio_encoder = audio_encoder->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioEncoderConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetCompatibleAudioEncoderConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	ONVIF_A_ENC * audio_encoder = g_onvif_cfg.audio_enc;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleAudioEncoderConfigurationsResponse>");

	while (audio_encoder)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", audio_encoder->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_encoder_config, audio_encoder->name, 
				audio_encoder->use_count, onvif_get_audio_encoding_str(audio_encoder->encoding), 
				audio_encoder->bitrate, audio_encoder->sample_rate, g_onvif_cfg.serv_ip, audio_encoder->session_timeout);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");
		
		audio_encoder = audio_encoder->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleAudioEncoderConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	return offset;
}

int build_GetVideoSourceConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * video_source = g_onvif_cfg.video_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetVideoSourceConfigurationsResponse>");

	while (video_source)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Configurations token=\"%s\">", video_source->token);
		offset += snprintf(p_buf+offset, mlen-offset, video_source_config, video_source->name, 
			video_source->use_count, video_source->source_token, video_source->height, 
			video_source->width, video_source->y, video_source->x);	
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Configurations>");

		video_source = video_source->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:GetVideoSourceConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetVideoSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * video_source = g_onvif_cfg.video_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourceConfigurationsResponse>");

	while (video_source)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", video_source->token);
		offset += snprintf(p_buf+offset, mlen-offset, video_source_config, video_source->name, 
			video_source->use_count, video_source->source_token, video_source->height, 
			video_source->width, video_source->y, video_source->x);	
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		video_source = video_source->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourceConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * token)
{
	ONVIF_V_SRC * video_source = onvif_find_video_source(token);
	if (NULL == video_source)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourceConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configuration token=\"%s\">", video_source->token);
	offset += snprintf(p_buf+offset, mlen-offset, video_source_config, video_source->name, 
		video_source->use_count, video_source->source_token, video_source->height, 
		video_source->width, video_source->y, video_source->x);	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configuration>");	    
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourceConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_SetVideoSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetVideoSourceConfigurationResponse></trt:SetVideoSourceConfigurationResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetVideoSourceConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * p_v_src = NULL;
	
	GetVideoSourceConfigurationOptions_REQ * p_GetVideoSourceConfigurationOptions_req = (GetVideoSourceConfigurationOptions_REQ *)argv;
	if (p_GetVideoSourceConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetVideoSourceConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}
		p_v_src = p_profile->video_src;
	}

	if (p_GetVideoSourceConfigurationOptions_req->config_token[0] != '\0')
	{
		p_v_src = onvif_find_video_source(p_GetVideoSourceConfigurationOptions_req->config_token);
		if (NULL == p_v_src)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetVideoSourceConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:BoundsRange>"
			"<tt:XRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:XRange>"
			"<tt:YRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:YRange>"
			"<tt:WidthRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:WidthRange>"
			"<tt:HeightRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:HeightRange>"
		"</tt:BoundsRange>", 
		g_onvif_cfg.video_src_cfg.x_min, g_onvif_cfg.video_src_cfg.x_max,
		g_onvif_cfg.video_src_cfg.y_min, g_onvif_cfg.video_src_cfg.y_max,
		g_onvif_cfg.video_src_cfg.w_min, g_onvif_cfg.video_src_cfg.w_max,
		g_onvif_cfg.video_src_cfg.h_min, g_onvif_cfg.video_src_cfg.h_max);

	if (p_v_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoSourceTokensAvailable>%s</tt:VideoSourceTokensAvailable>", p_v_src->token);
	}
	else
	{
		p_v_src = g_onvif_cfg.video_src;
		while (p_v_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:VideoSourceTokensAvailable>%s</tt:VideoSourceTokensAvailable>",
				p_v_src->token);
			
			p_v_src = p_v_src->next;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:GetVideoSourceConfigurationOptionsResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetVideoSourceConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * p_v_src = NULL;
	
	GetVideoSourceConfigurationOptions_REQ * p_GetVideoSourceConfigurationOptions_req = (GetVideoSourceConfigurationOptions_REQ *)argv;
	if (p_GetVideoSourceConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetVideoSourceConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}
		p_v_src = p_profile->video_src;
	}

	if (p_GetVideoSourceConfigurationOptions_req->config_token[0] != '\0')
	{
		p_v_src = onvif_find_video_source(p_GetVideoSourceConfigurationOptions_req->config_token);
		if (NULL == p_v_src)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoSourceConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:BoundsRange>"
			"<tt:XRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:XRange>"
			"<tt:YRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:YRange>"
			"<tt:WidthRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:WidthRange>"
			"<tt:HeightRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:HeightRange>"
		"</tt:BoundsRange>", 
		g_onvif_cfg.video_src_cfg.x_min, g_onvif_cfg.video_src_cfg.x_max,
		g_onvif_cfg.video_src_cfg.y_min, g_onvif_cfg.video_src_cfg.y_max,
		g_onvif_cfg.video_src_cfg.w_min, g_onvif_cfg.video_src_cfg.w_max,
		g_onvif_cfg.video_src_cfg.h_min, g_onvif_cfg.video_src_cfg.h_max);

	if (p_v_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:VideoSourceTokensAvailable>%s</tt:VideoSourceTokensAvailable>", p_v_src->token);
	}
	else
	{
		p_v_src = g_onvif_cfg.video_src;
		while (p_v_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, 
				"<tt:VideoSourceTokensAvailable>%s</tt:VideoSourceTokensAvailable>",
				p_v_src->token);
			
			p_v_src = p_v_src->next;
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetVideoSourceConfigurationOptionsResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetCompatibleVideoSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	ONVIF_V_SRC * video_source = g_onvif_cfg.video_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleVideoSourceConfigurationsResponse>");

	while (video_source)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", video_source->token);
		offset += snprintf(p_buf+offset, mlen-offset, video_source_config,
			video_source->name, video_source->use_count, video_source->source_token, video_source->height, 
			video_source->width, video_source->y, video_source->x);	
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		video_source = video_source->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleVideoSourceConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioSourceConfigurations_rly_xml_2(char * p_buf, int mlen, const char * argv)
{
	ONVIF_A_SRC * audio_source = g_onvif_cfg.audio_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetAudioSourceConfigurationsResponse>");

	while (audio_source)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Configurations token=\"%s\">", audio_source->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_source_config,
			audio_source->name, audio_source->use_count, audio_source->source_token);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Configurations>");

		audio_source = audio_source->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:GetAudioSourceConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_A_SRC * audio_source = g_onvif_cfg.audio_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourceConfigurationsResponse>");

	while (audio_source)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", audio_source->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_source_config,
			audio_source->name, audio_source->use_count, audio_source->source_token);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		audio_source = audio_source->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioSourceConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetCompatibleAudioSourceConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}

	ONVIF_A_SRC * audio_source = g_onvif_cfg.audio_src;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetCompatibleAudioSourceConfigurationsResponse>");

	while (audio_source)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configurations token=\"%s\">", audio_source->token);
		offset += snprintf(p_buf+offset, mlen-offset, audio_source_config,
			audio_source->name, audio_source->use_count, audio_source->source_token);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configurations>");

		audio_source = audio_source->next;
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetCompatibleAudioSourceConfigurationsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioSourceConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv)
{
	ONVIF_A_SRC * p_a_src = NULL;
	
	GetAudioSourceConfigurationOptions_REQ * p_GetAudioSourceConfigurationOptions_req = (GetAudioSourceConfigurationOptions_REQ *)argv;
	if (p_GetAudioSourceConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetAudioSourceConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}

		p_a_src = p_profile->audio_src;
	}

	if (p_GetAudioSourceConfigurationOptions_req->config_token[0] != '\0')
	{
		p_a_src = onvif_find_audio_source(p_GetAudioSourceConfigurationOptions_req->config_token);
		if (NULL == p_a_src)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetAudioSourceConfigurationOptionsResponse><trt2:Options>");

	if (p_a_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:InputTokensAvailable>%s</tt:InputTokensAvailable>", p_a_src->token);
	}
	else
	{
		p_a_src = g_onvif_cfg.audio_src;
		
		while (p_a_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:InputTokensAvailable>%s</tt:InputTokensAvailable>", p_a_src->token);
			
			p_a_src = p_a_src->next;
		}
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Options></trt2:GetAudioSourceConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioSourceConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_A_SRC * p_a_src = NULL;
	
	GetAudioSourceConfigurationOptions_REQ * p_GetAudioSourceConfigurationOptions_req = (GetAudioSourceConfigurationOptions_REQ *)argv;
	if (p_GetAudioSourceConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetAudioSourceConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}

		p_a_src = p_profile->audio_src;
	}

	if (p_GetAudioSourceConfigurationOptions_req->config_token[0] != '\0')
	{
		p_a_src = onvif_find_audio_source(p_GetAudioSourceConfigurationOptions_req->config_token);
		if (NULL == p_a_src)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourceConfigurationOptionsResponse><trt:Options>");

	if (p_a_src)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:InputTokensAvailable>%s</tt:InputTokensAvailable>", p_a_src->token);
	}
	else
	{
		p_a_src = g_onvif_cfg.audio_src;
		
		while (p_a_src)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:InputTokensAvailable>%s</tt:InputTokensAvailable>", p_a_src->token);
			
			p_a_src = p_a_src->next;
		}
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options></trt:GetAudioSourceConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * token)
{
	ONVIF_A_SRC * audio_source = onvif_find_audio_source(token);
	if (NULL == audio_source)
	{
		return ONVIF_ERR_NO_CONFIG;
	}

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioSourceConfigurationResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:Configuration token=\"%s\">", audio_source->token);
	offset += snprintf(p_buf+offset, mlen-offset, audio_source_config,
		audio_source->name, audio_source->use_count, audio_source->source_token);
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Configuration>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetAudioSourceConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_SetAudioSourceConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetAudioSourceConfigurationResponse></trt:SetAudioSourceConfigurationResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetVideoEncoderConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv)
{
	GetVideoEncoderConfigurationOptions_REQ * p_GetVideoEncoderConfigurationOptions_req = (GetVideoEncoderConfigurationOptions_REQ *) argv;
	int req_stream = 0;
	if (p_GetVideoEncoderConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetVideoEncoderConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}
	}

	if (p_GetVideoEncoderConfigurationOptions_req->config_token[0] != '\0')
	{
		ONVIF_V_ENC * p_v_enc = onvif_find_video_encoder(p_GetVideoEncoderConfigurationOptions_req->config_token);
		if (NULL == p_v_enc)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
		else
		{
			if (!strcmp(p_v_enc->token, "VideoEncodeMain"))
				req_stream = 0;
			else if (!strcmp(p_v_enc->token, "VideoEncodeSub"))
				req_stream = 1;
			else if (!strcmp(p_v_enc->token, "VideoEncodeThird"))
				req_stream = 2;
		}
	}
	
	int i = 0;
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetVideoEncoderConfigurationOptionsResponse>");

	if (g_onvif_cfg.video_enc_cfg.h264_sup)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>");
		// H264 options
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Encoding>H264</tt:Encoding>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:QualityRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:QualityRange>",
			g_onvif_cfg.video_enc_cfg.quality_min, g_onvif_cfg.video_enc_cfg.quality_max);
		for (i = 0; i < MAX_RES_NUMS; i++)
		{
			if (g_onvif_cfg.video_enc_cfg.resolution[i].channle != req_stream || strncmp(g_onvif_cfg.video_enc_cfg.resolution[i].encoding, "H264", strlen("H264"))  || g_onvif_cfg.video_enc_cfg.resolution[i].w == 0 || g_onvif_cfg.video_enc_cfg.resolution[i].h == 0)
				continue;
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ResolutionsAvailable><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height>"
			"<tt:FrameRateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:FrameRateRange>"
			"<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange></tt:ResolutionsAvailable>",
				g_onvif_cfg.video_enc_cfg.resolution[i].w, g_onvif_cfg.video_enc_cfg.resolution[i].h,
				g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_min, g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_max,
				g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange>"
		"<tt:GovLengthRange>%d-%d</tt:GovLengthRange>"
		"<tt:FrameRatesSupported>%d-%d</tt:FrameRatesSupported>"
		"<tt:ProfilesSupported>Main</tt:ProfilesSupported>"
		"<tt:ConstantBitRateSupported>true</tt:ConstantBitRateSupported>",
			g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max,
			g_onvif_cfg.video_enc_cfg.gov_length_min, g_onvif_cfg.video_enc_cfg.gov_length_max,
			g_onvif_cfg.video_enc_cfg.frame_sup_min, g_onvif_cfg.video_enc_cfg.frame_sup_max);
			
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Options>");
	}

	if (g_onvif_cfg.video_enc_cfg.h265_sup)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>");
		// H265 options
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Encoding>H265</tt:Encoding>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:QualityRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:QualityRange>",
			g_onvif_cfg.video_enc_cfg.quality_min, g_onvif_cfg.video_enc_cfg.quality_max);
		for (i = 0; i < MAX_RES_NUMS; i++)
		{
			if (g_onvif_cfg.video_enc_cfg.resolution[i].channle != req_stream || strncmp(g_onvif_cfg.video_enc_cfg.resolution[i].encoding, "H265", strlen("H265"))  || g_onvif_cfg.video_enc_cfg.resolution[i].w == 0 || g_onvif_cfg.video_enc_cfg.resolution[i].h == 0)
				continue;
			
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ResolutionsAvailable><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height>"
			"<tt:FrameRateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:FrameRateRange>"
			"<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange></tt:ResolutionsAvailable>",
				g_onvif_cfg.video_enc_cfg.resolution[i].w, g_onvif_cfg.video_enc_cfg.resolution[i].h,
				g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_min, g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_max,
				g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange>"
		"<tt:GovLengthRange>%d-%d</tt:GovLengthRange>"
		"<tt:FrameRatesSupported>%d-%d</tt:FrameRatesSupported>"
		"<tt:ProfilesSupported>Main</tt:ProfilesSupported>"
		"<tt:ConstantBitRateSupported>true</tt:ConstantBitRateSupported>",
			g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max,
			g_onvif_cfg.video_enc_cfg.gov_length_min, g_onvif_cfg.video_enc_cfg.gov_length_max,
			g_onvif_cfg.video_enc_cfg.frame_sup_min, g_onvif_cfg.video_enc_cfg.frame_sup_max);
			
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Options>");
	}

	if (g_onvif_cfg.video_enc_cfg.mjpg_sup)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>");
		// MJPEG options
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Encoding>JPEG</tt:Encoding>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:QualityRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:QualityRange>",
			g_onvif_cfg.video_enc_cfg.quality_min, g_onvif_cfg.video_enc_cfg.quality_max);
		for (i = 0; i < MAX_RES_NUMS; i++)
		{
			if (g_onvif_cfg.video_enc_cfg.resolution[i].channle != req_stream || strncmp(g_onvif_cfg.video_enc_cfg.resolution[i].encoding, "MJPEG", strlen("MJPEG"))  || g_onvif_cfg.video_enc_cfg.resolution[i].w == 0 || g_onvif_cfg.video_enc_cfg.resolution[i].h == 0)
				continue;
			
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ResolutionsAvailable><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height>"
			"<tt:FrameRateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:FrameRateRange>"
			"<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange></tt:ResolutionsAvailable>",
				g_onvif_cfg.video_enc_cfg.resolution[i].w, g_onvif_cfg.video_enc_cfg.resolution[i].h,
				g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_min, g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_max,
				g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange>"
		"<tt:GovLengthRange>%d-%d</tt:GovLengthRange>"
		"<tt:FrameRatesSupported>%d-%d</tt:FrameRatesSupported>"
		"<tt:ProfilesSupported>Main</tt:ProfilesSupported>"
		"<tt:ConstantBitRateSupported>true</tt:ConstantBitRateSupported>",
			g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max,
			g_onvif_cfg.video_enc_cfg.gov_length_min, g_onvif_cfg.video_enc_cfg.gov_length_max,
			g_onvif_cfg.video_enc_cfg.frame_sup_min, g_onvif_cfg.video_enc_cfg.frame_sup_max);
			
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Options>");
	}
	
	if (0)//g_onvif_cfg.video_enc_cfg.h265p_sup)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>");
		// H265+ options
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:Encoding>H265+</tt:Encoding>");
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:QualityRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:QualityRange>",
			g_onvif_cfg.video_enc_cfg.quality_min, g_onvif_cfg.video_enc_cfg.quality_max);
		for (i = 0; i < MAX_RES_NUMS; i++)
		{
			if (g_onvif_cfg.video_enc_cfg.resolution[i].channle != req_stream || strncmp(g_onvif_cfg.video_enc_cfg.resolution[i].encoding, "H265+", strlen("H265+"))  || g_onvif_cfg.video_enc_cfg.resolution[i].w == 0 || g_onvif_cfg.video_enc_cfg.resolution[i].h == 0)
				continue;
			
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ResolutionsAvailable><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height>"
			"<tt:FrameRateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:FrameRateRange>"
			"<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange></tt:ResolutionsAvailable>",
				g_onvif_cfg.video_enc_cfg.resolution[i].w, g_onvif_cfg.video_enc_cfg.resolution[i].h,
				g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_min, g_onvif_cfg.video_enc_cfg.resolution[i].frame_rate_max,
				g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:BitrateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:BitrateRange>"
		"<tt:GovLengthRange>%d-%d</tt:GovLengthRange>"
		"<tt:FrameRatesSupported>%d-%d</tt:FrameRatesSupported>"
		"<tt:ProfilesSupported>Main</tt:ProfilesSupported>"
		"<tt:ConstantBitRateSupported>true</tt:ConstantBitRateSupported>",
			g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_min, g_onvif_cfg.video_enc_cfg.resolution[i].bit_rate_range_max,
			g_onvif_cfg.video_enc_cfg.gov_length_min, g_onvif_cfg.video_enc_cfg.gov_length_max,
			g_onvif_cfg.video_enc_cfg.frame_sup_min, g_onvif_cfg.video_enc_cfg.frame_sup_max);
			
		offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Options>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:GetVideoEncoderConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetVideoEncoderConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv)
{
	GetVideoEncoderConfigurationOptions_REQ * p_GetVideoEncoderConfigurationOptions_req = (GetVideoEncoderConfigurationOptions_REQ *) argv;
	int req_stream = 0;
	if (p_GetVideoEncoderConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetVideoEncoderConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}
	}

	if (p_GetVideoEncoderConfigurationOptions_req->config_token[0] != '\0')
	{
		ONVIF_V_ENC * p_v_enc = onvif_find_video_encoder(p_GetVideoEncoderConfigurationOptions_req->config_token);
		if (NULL == p_v_enc)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
		else
		{
			if (!strcmp(p_v_enc->token, "VideoEncodeMain"))
				req_stream = 0;
			else if (!strcmp(p_v_enc->token, "VideoEncodeSub"))
				req_stream = 1;
			else if (!strcmp(p_v_enc->token, "VideoEncodeThird"))
				req_stream = 2;
		}
	}
	
	int i = 0;
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetVideoEncoderConfigurationOptionsResponse><trt:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:QualityRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:QualityRange>",
		g_onvif_cfg.video_enc_cfg.quality_min, g_onvif_cfg.video_enc_cfg.quality_max);
	
	if (g_onvif_cfg.video_enc_cfg.h264_sup)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:H264>");
		// H264 options
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:QualityRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:QualityRange>",
			g_onvif_cfg.video_enc_cfg.quality_min, g_onvif_cfg.video_enc_cfg.quality_max);
		for (i = 0; i < MAX_RES_NUMS; i++)
		{
			if (g_onvif_cfg.video_enc_cfg.resolution[i].channle != req_stream || strncmp(g_onvif_cfg.video_enc_cfg.resolution[i].encoding, "H264", strlen("H264"))  || g_onvif_cfg.video_enc_cfg.resolution[i].w == 0 || g_onvif_cfg.video_enc_cfg.resolution[i].h == 0)
				continue;
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ResolutionsAvailable><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height></tt:ResolutionsAvailable>",
				g_onvif_cfg.video_enc_cfg.resolution[i].w, g_onvif_cfg.video_enc_cfg.resolution[i].h);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:GovLengthRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:GovLengthRange>"
		"<tt:FrameRateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:FrameRateRange>"
		"<tt:EncodingIntervalRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:EncodingIntervalRange>",
		g_onvif_cfg.video_enc_cfg.h264_opt.gov_length_min, g_onvif_cfg.video_enc_cfg.h264_opt.gov_length_max, 
		g_onvif_cfg.video_enc_cfg.h264_opt.frame_rate_min, g_onvif_cfg.video_enc_cfg.h264_opt.frame_rate_max,
		g_onvif_cfg.video_enc_cfg.h264_opt.encoding_interval_min, g_onvif_cfg.video_enc_cfg.h264_opt.encoding_interval_max);
			
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:H264>");
	}

	if (g_onvif_cfg.video_enc_cfg.mjpg_sup)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:JPEG>");
		// MJPEG options
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:QualityRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:QualityRange>",
			g_onvif_cfg.video_enc_cfg.quality_min, g_onvif_cfg.video_enc_cfg.quality_max);
		for (i = 0; i < MAX_RES_NUMS; i++)
		{
			if (g_onvif_cfg.video_enc_cfg.resolution[i].channle != req_stream || strncmp(g_onvif_cfg.video_enc_cfg.resolution[i].encoding, "MJPEG", strlen("MJPEG"))  || g_onvif_cfg.video_enc_cfg.resolution[i].w == 0 || g_onvif_cfg.video_enc_cfg.resolution[i].h == 0)
				continue;
			
			offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:ResolutionsAvailable><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height></tt:ResolutionsAvailable>",
				g_onvif_cfg.video_enc_cfg.resolution[i].w, g_onvif_cfg.video_enc_cfg.resolution[i].h);
		}	
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:FrameRateRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:FrameRateRange>"
		"<tt:EncodingIntervalRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:EncodingIntervalRange>",
		g_onvif_cfg.video_enc_cfg.jpeg_opt.frame_rate_min, g_onvif_cfg.video_enc_cfg.jpeg_opt.frame_rate_max,
		g_onvif_cfg.video_enc_cfg.jpeg_opt.encoding_interval_min, g_onvif_cfg.video_enc_cfg.jpeg_opt.encoding_interval_max);
			
		offset += snprintf(p_buf+offset, mlen-offset, "</tt:JPEG>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options></trt:GetVideoEncoderConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioEncoderConfigurationOptions_rly_xml_2(char * p_buf, int mlen, const char * argv)
{    
	GetAudioEncoderConfigurationOptions_REQ * p_GetAudioEncoderConfigurationOptions_req = (GetAudioEncoderConfigurationOptions_REQ *) argv;

	if (p_GetAudioEncoderConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetAudioEncoderConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}
	}

	if (p_GetAudioEncoderConfigurationOptions_req->config_token[0] != '\0')
	{
		ONVIF_A_ENC * p_a_enc = onvif_find_audio_encoder(p_GetAudioEncoderConfigurationOptions_req->config_token);
		if (NULL == p_a_enc)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:GetAudioEncoderConfigurationOptionsResponse><trt2:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>"
		"<tt:Encoding>G711</tt:Encoding>"
		"<tt:BitrateList><tt:Items>64</tt:Items><tt:Items>80</tt:Items></tt:BitrateList>"
		"<tt:SampleRateList><tt:Items>8</tt:Items><tt:Items>12</tt:Items>"
		"<tt:Items>25</tt:Items><tt:Items>32</tt:Items><tt:Items>48</tt:Items>"
		"</tt:SampleRateList></trt2:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>"
		"<tt:Encoding>G726</tt:Encoding>"
		"<tt:BitrateList><tt:Items>64</tt:Items><tt:Items>80</tt:Items></tt:BitrateList>"
		"<tt:SampleRateList><tt:Items>8</tt:Items><tt:Items>12</tt:Items>"
		"<tt:Items>25</tt:Items><tt:Items>32</tt:Items><tt:Items>48</tt:Items>"
		"</tt:SampleRateList></trt2:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "<trt2:Options>"
		"<tt:Encoding>AAC</tt:Encoding>"
		"<tt:BitrateList><tt:Items>64</tt:Items><tt:Items>80</tt:Items></tt:BitrateList>"
		"<tt:SampleRateList><tt:Items>8</tt:Items><tt:Items>12</tt:Items>"
		"<tt:Items>25</tt:Items><tt:Items>32</tt:Items><tt:Items>48</tt:Items>"
		"</tt:SampleRateList></trt2:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trt2:Options></trt2:GetAudioEncoderConfigurationOptionsResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetAudioEncoderConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv)
{    
	GetAudioEncoderConfigurationOptions_REQ * p_GetAudioEncoderConfigurationOptions_req = (GetAudioEncoderConfigurationOptions_REQ *) argv;

	if (p_GetAudioEncoderConfigurationOptions_req->profile_token[0] != '\0')
	{
		ONVIF_PROFILE * p_profile = onvif_find_profile(p_GetAudioEncoderConfigurationOptions_req->profile_token);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NO_PROFILE;
		}
	}

	if (p_GetAudioEncoderConfigurationOptions_req->config_token[0] != '\0')
	{
		ONVIF_A_ENC * p_a_enc = onvif_find_audio_encoder(p_GetAudioEncoderConfigurationOptions_req->config_token);
		if (NULL == p_a_enc)
		{
			return ONVIF_ERR_NO_CONFIG;
		}
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetAudioEncoderConfigurationOptionsResponse><trt:Options>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Options>"
		"<tt:Encoding>G711</tt:Encoding>"
		"<tt:BitrateList><tt:Items>64</tt:Items><tt:Items>80</tt:Items></tt:BitrateList>"
		"<tt:SampleRateList><tt:Items>8</tt:Items><tt:Items>12</tt:Items>"
		"<tt:Items>25</tt:Items><tt:Items>32</tt:Items><tt:Items>48</tt:Items>"
		"</tt:SampleRateList></tt:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Options>"
		"<tt:Encoding>G726</tt:Encoding>"
		"<tt:BitrateList><tt:Items>64</tt:Items><tt:Items>80</tt:Items></tt:BitrateList>"
		"<tt:SampleRateList><tt:Items>8</tt:Items><tt:Items>12</tt:Items>"
		"<tt:Items>25</tt:Items><tt:Items>32</tt:Items><tt:Items>48</tt:Items>"
		"</tt:SampleRateList></tt:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Options>"
		"<tt:Encoding>AAC</tt:Encoding>"
		"<tt:BitrateList><tt:Items>64</tt:Items><tt:Items>80</tt:Items></tt:BitrateList>"
		"<tt:SampleRateList><tt:Items>8</tt:Items><tt:Items>12</tt:Items>"
		"<tt:Items>25</tt:Items><tt:Items>32</tt:Items><tt:Items>48</tt:Items>"
		"</tt:SampleRateList></tt:Options>");
	offset += snprintf(p_buf+offset, mlen-offset, "</trt:Options></trt:GetAudioEncoderConfigurationOptionsResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_SystemReboot_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:SystemRebootResponse><tds:Message>Rebooting</tds:Message></tds:SystemRebootResponse>");	    
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_SetSystemFactoryDefault_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetSystemFactoryDefaultResponse></tds:SetSystemFactoryDefaultResponse>");	    
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetSystemLog_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int type = (long)argv; /* 0:System Log, 1:Access Log */

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetSystemLogResponse><tds:SystemLog>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:String>Test %s Log</tt:String>", type == 1 ? "Access" : "System");
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:SystemLog></tds:GetSystemLogResponse>");	    
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_SetVideoEncoderConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:SetVideoEncoderConfigurationResponse></trt:SetVideoEncoderConfigurationResponse>");		    
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetSystemDateAndTime_rly_xml(char * p_buf, int mlen, const char * argv)
{
	time_t nowtime;
	struct tm *gtime;

	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	time(&nowtime);
	gtime = gmtime(&nowtime);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetSystemDateAndTimeResponse><tds:SystemDateAndTime>"
			"<tt:DateTimeType>%s</tt:DateTimeType>"
			"<tt:DaylightSavings>%s</tt:DaylightSavings>",
			g_onvif_cfg.datetime_ntp ? "NTP" : "Manual", 
			g_onvif_cfg.daylightsavings ? "true" : "false");

	if (g_onvif_cfg.timezone[0] != '\0')
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:TimeZone>"
				"<tt:TZ>%s</tt:TZ>"
			"</tt:TimeZone>",
			g_onvif_cfg.timezone);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:UTCDateTime>"
				"<tt:Time>"
					"<tt:Hour>%d</tt:Hour>"
					"<tt:Minute>%d</tt:Minute>"
					"<tt:Second>%d</tt:Second>"
				"</tt:Time>"
				"<tt:Date>"
					"<tt:Year>%d</tt:Year>"
					"<tt:Month>%d</tt:Month>"
					"<tt:Day>%d</tt:Day>"
				"</tt:Date>"
			"</tt:UTCDateTime>"	
		"</tds:SystemDateAndTime>"
		"</tds:GetSystemDateAndTimeResponse>",
		gtime->tm_hour, gtime->tm_min, gtime->tm_sec, gtime->tm_year + 1900, gtime->tm_mon + 1, gtime->tm_mday);
		
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}


int build_SetSystemDateAndTime_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetSystemDateAndTimeResponse></tds:SetSystemDateAndTimeResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

char device_capabilities[] = 
	"<tds:Capabilities>"
		"<tds:Network IPFilter=\"false\" ZeroConfiguration=\"false\" "
			"IPVersion6=\"false\" DynDNS=\"true\" Dot11Configuration=\"false\" "
			"HostnameFromDHCP=\"false\" NTP=\"1\" />"
		"<tds:Security TLS1.0=\"false\" TLS1.1=\"false\" TLS1.2=\"false\" "
			"OnboardKeyGeneration=\"false\" AccessPolicyConfig=\"false\" DefaultAccessPolicy=\"false\" "
			"Dot1X=\"false\" RemoteUserHandling=\"false\" X.509Token=\"false\" SAMLToken=\"false\" "
			"KerberosToken=\"false\" UsernameToken=\"true\" HttpDigest=\"true\" RELToken=\"false\" />"	
		"<tds:System DiscoveryResolve=\"true\" DiscoveryBye=\"false\" "
			"RemoteDiscovery=\"false\" SystemBackup=\"true\" SystemLogging=\"true\" "
			"FirmwareUpgrade=\"true\" HttpFirmwareUpgrade=\"true\" HttpSystemBackup=\"true\" "
			"HttpSystemLogging=\"true\" HttpSupportInformation=\"true\" />"			
	"</tds:Capabilities>";	

char media_capabilities[] = 
	"<trt:Capabilities SnapshotUri=\"true\" Rotation=\"true\" VideoSourceMode=\"true\" OSD=\"false\">"
		"<trt:ProfileCapabilities MaximumNumberOfProfiles=\"10\" />"
		"<trt:StreamingCapabilities RTPMulticast=\"true\" RTP_TCP=\"true\" RTP_RTSP_TCP=\"true\" "
			"NonAggregateControl=\"false\" NoRTSPStreaming=\"false\" />"
	"</trt:Capabilities>";

char event_capabilities[] = 
	"<tev:Capabilities WSSubscriptionPolicySupport=\"false\" WSPullPointSupport=\"false\" "
		"WSPausableSubscriptionManagerInterfaceSupport=\"false\" MaxNotificationProducers=\"10\" "
		"MaxPullPoints=\"10\" PersistentNotificationStorage=\"false\" />";

char ptz_capabilities[] = 
	"<tptz:Capabilities EFlip=\"false\" Reverse=\"false\" GetCompatibleConfigurations=\"true\" />";

char image_capabilities[] =
	"<timg:Capabilities ImageStabilization=\"false\" />";

int build_GetServices_rly_xml(char * p_buf, int mlen, const char * argv)
{
	BOOL bflag = (long)argv;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetServicesResponse>");

	// device manager
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Service>"
		"<tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>"
		"<tds:XAddr>http://%s:%d/onvif/device_service</tds:XAddr>", 
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	
	if (bflag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
		offset += snprintf(p_buf+offset, mlen-offset, device_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
		"<tt:Major>%d</tt:Major>"
		"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>"
		"</tds:Service>", 2, 42);

	// media 
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Service>"
		"<tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>"
		"<tds:XAddr>http://%s:%d/onvif/media_service</tds:XAddr>", 
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	
	if (bflag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
		offset += snprintf(p_buf+offset, mlen-offset, media_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
		"<tt:Major>%d</tt:Major>"
		"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>"
		"</tds:Service>", 2, 42);

	// media 2
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Service>"
		"<tds:Namespace>http://www.onvif.org/ver20/media/wsdl</tds:Namespace>"
		"<tds:XAddr>http://%s:%d/onvif/media_service2</tds:XAddr>", 
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	
	if (bflag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
		offset += snprintf(p_buf+offset, mlen-offset, media_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
		"<tt:Major>%d</tt:Major>"
		"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>"
		"</tds:Service>", 2, 42);
	
	// event 
	offset += snprintf(p_buf+offset, mlen-offset,
		"<tds:Service>"
		"<tds:Namespace>http://www.onvif.org/ver10/events/wsdl</tds:Namespace>"
		"<tds:XAddr>http://%s:%d/onvif/event_service</tds:XAddr>", 
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	
	if (bflag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
		offset += snprintf(p_buf+offset, mlen-offset, event_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
		"<tt:Major>%d</tt:Major>"
		"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>"
		"</tds:Service>", 2, 42);

	// ptz
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Service>"
		"<tds:Namespace>http://www.onvif.org/ver20/ptz/wsdl</tds:Namespace>"
		"<tds:XAddr>http://%s:%d/onvif/ptz_service</tds:XAddr>", 
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	
	if (bflag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
		offset += snprintf(p_buf+offset, mlen-offset, ptz_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
		"<tt:Major>%d</tt:Major>"
		"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>"
		"</tds:Service>", 2, 42);

	// image
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Service>"
		"<tds:Namespace>http://www.onvif.org/ver20/imaging/wsdl</tds:Namespace>"
		"<tds:XAddr>http://%s:%d/onvif/image_service</tds:XAddr>", 
		g_onvif_cls.local_ipstr, g_onvif_cls.local_port);	
	if (bflag)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:Capabilities>");
		offset += snprintf(p_buf+offset, mlen-offset, image_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:Capabilities>");
	}    
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:Version>"
		"<tt:Major>%d</tt:Major>"
		"<tt:Minor>%d</tt:Minor>"
		"</tds:Version>"
		"</tds:Service>", 2, 42);

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetServicesResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetScopes_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetScopesResponse>");

	for (int i = 0; i < MAX_SCOPE_NUMS; i++)
	{
		if (g_onvif_cfg.scopes[i].scope[0] != '\0')
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tds:Scopes><tt:ScopeDef>%s</tt:ScopeDef>"
				"<tt:ScopeItem>%s</tt:ScopeItem></tds:Scopes>", 
				g_onvif_cfg.scopes[i].fixed ? "Fixed" : "Configurable", g_onvif_cfg.scopes[i].scope);
		}
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetScopesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_AddScopes_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:AddScopesResponse></tds:AddScopesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetScopes_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetScopesResponse></tds:SetScopesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RemoveScopes_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_SCOPE * p_scope = (ONVIF_SCOPE *)argv;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:RemoveScopesResponse>");

	for (int i = 0; i < MAX_SCOPE_NUMS; i++)
	{
		if (p_scope[i].scope[0] == '\0')
		{
			break;
		}

		offset += snprintf(p_buf+offset, mlen-offset, "<tt:ScopeItem>%s</tt:ScopeItem>", p_scope[i].scope);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:RemoveScopesResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetHostname_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetHostnameResponse>"
			"<tds:HostnameInformation>"
			"<tt:FromDHCP>%s</tt:FromDHCP>"
			"<tt:Name>%s</tt:Name>"
			"</tds:HostnameInformation>"
		"</tds:GetHostnameResponse>",
			g_onvif_cfg.network.hostname_fromdhcp ? "true" : "false",
			g_onvif_cfg.network.hostname);

	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetHostname_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetHostnameResponse></tds:SetHostnameResponse>");

	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetNetworkProtocols_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int i = 0;
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNetworkProtocolsResponse>");

	if (g_onvif_cfg.network.http_support)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:NetworkProtocols>"
			"<tt:Name>HTTP</tt:Name><tt:Enabled>%s</tt:Enabled>", g_onvif_cfg.network.http_enable ? "true" : "false");

		for (i = 0; i < MAX_SERVER_PORT; i++)
		{
			if (g_onvif_cfg.network.http_port[i] == 0)
			{
				continue;
			}
			
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Port>%d</tt:Port>", g_onvif_cfg.network.http_port[i]);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkProtocols>");
	}

	if (g_onvif_cfg.network.https_support)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:NetworkProtocols>"
			"<tt:Name>HTTPS</tt:Name><tt:Enabled>%s</tt:Enabled>", g_onvif_cfg.network.https_enable ? "true" : "false");

		for (i = 0; i < MAX_SERVER_PORT; i++)
		{
			if (g_onvif_cfg.network.https_port[i] == 0)
			{
				continue;
			}
			
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Port>%d</tt:Port>", g_onvif_cfg.network.https_port[i]);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkProtocols>");
	}

	if (g_onvif_cfg.network.rtsp_support)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:NetworkProtocols>"
			"<tt:Name>RTSP</tt:Name><tt:Enabled>%s</tt:Enabled>", g_onvif_cfg.network.rtsp_enable ? "true" : "false");

		for (i = 0; i < MAX_SERVER_PORT; i++)
		{
			if (g_onvif_cfg.network.rtsp_port[i] == 0)
			{
				continue;
			}
			
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:Port>%d</tt:Port>", g_onvif_cfg.network.rtsp_port[i]);
		}

		offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkProtocols>");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetNetworkProtocolsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetNetworkProtocols_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetNetworkProtocolsResponse></tds:SetNetworkProtocolsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetNetworkDefaultGateway_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNetworkDefaultGatewayResponse><tds:NetworkGateway>");

	for (int i = 0; i < MAX_GATEWAY; i++)
	{
		if (g_onvif_cfg.network.gateway[i][0] != '\0')
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:IPv4Address>%s</tt:IPv4Address>", g_onvif_cfg.network.gateway[i]);
		}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:NetworkGateway></tds:GetNetworkDefaultGatewayResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetNetworkDefaultGateway_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetNetworkDefaultGatewayResponse></tds:SetNetworkDefaultGatewayResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetDiscoveryMode_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetDiscoveryModeResponse>"
		"<tds:DiscoveryMode>%s</tds:DiscoveryMode>"
		"</tds:GetDiscoveryModeResponse>", g_onvif_cfg.discoverable ? "Discoverable" : "NonDiscoverable");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetDiscoveryMode_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetDiscoveryModeResponse></tds:SetDiscoveryModeResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetDNS_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetDNSResponse><tds:DNSInformation>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:FromDHCP>%s</tt:FromDHCP>", g_onvif_cfg.network.dns_fromdhcp ? "true" : "false");

	int i;
	
	for (i = 0; i < MAX_SEARCHDOMAIN; i++)
	{
		if (g_onvif_cfg.network.dns_searchdomain[i][0] == '\0')
		{
			continue;
		}

		offset += snprintf(p_buf+offset, mlen-offset, "<tt:SearchDomain>%s</tt:SearchDomain>", g_onvif_cfg.network.dns_searchdomain[i]);
	}
	
	for (i = 0; i < MAX_DNS_SERVER; i++)
	{
		if (g_onvif_cfg.network.dns_server[i][0] == '\0')
		{
			continue;
		}
		
		if (g_onvif_cfg.network.dns_fromdhcp)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DNSFromDHCP>");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DNSManual>");
		}

		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>IPv4</tt:Type><tt:IPv4Address>%s</tt:IPv4Address>",
			g_onvif_cfg.network.dns_server[i]);

		if (g_onvif_cfg.network.dns_fromdhcp)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:DNSFromDHCP>");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:DNSManual>");
		}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:DNSInformation></tds:GetDNSResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetDNS_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetDNSResponse></tds:SetDNSResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetNTP_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetNTPResponse><tds:NTPInformation>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:FromDHCP>%s</tt:FromDHCP>", g_onvif_cfg.network.ntp_fromdhcp ? "true" : "false");

	for (int i = 0; i < MAX_NTP_SERVER; i++)
	{
		if (g_onvif_cfg.network.ntp_server[i][0] == '\0')
		{
			continue;
		}
		
		if (g_onvif_cfg.network.ntp_fromdhcp)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:NTPFromDHCP>");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:NTPManual>");
		}

		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Type>IPv4</tt:Type>");
		if (is_ip_address(g_onvif_cfg.network.ntp_server[i]))
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:IPv4Address>%s</tt:IPv4Address>", g_onvif_cfg.network.ntp_server[i]);
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "<tt:DNSname>%s</tt:DNSname>", g_onvif_cfg.network.ntp_server[i]);
		}

		if (g_onvif_cfg.network.ntp_fromdhcp)
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:NTPFromDHCP>");
		}
		else
		{
			offset += snprintf(p_buf+offset, mlen-offset, "</tt:NTPManual>");
		}
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tds:NTPInformation></tds:GetNTPResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetNTP_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tds:SetNTPResponse></tds:SetNTPResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_GetServiceCapabilities_rly_xml(char * p_buf, int mlen, const char * argv)
{
	const int category = (const long)argv;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);

	if (CAP_CATEGORY_EVENTS == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://www.onvif.org/ver10/events/wsdl/EventPortType/GetServiceCapabilitiesResponse");
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	

	if (CAP_CATEGORY_DEVICE == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tds:GetServiceCapabilitiesResponse>");
		offset += snprintf(p_buf+offset, mlen-offset, device_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tds:GetServiceCapabilitiesResponse>");
	}
	else if (CAP_CATEGORY_MEDIA == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<trt:GetServiceCapabilitiesResponse>");
		offset += snprintf(p_buf+offset, mlen-offset, media_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</trt:GetServiceCapabilitiesResponse>");
	}
	else if (CAP_CATEGORY_EVENTS == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tev:GetServiceCapabilitiesResponse>");
		offset += snprintf(p_buf+offset, mlen-offset, event_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tev:GetServiceCapabilitiesResponse>");
	}
	else if (CAP_CATEGORY_PTZ == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetServiceCapabilitiesResponse>");
		offset += snprintf(p_buf+offset, mlen-offset, ptz_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetServiceCapabilitiesResponse>");
	}
	else if (CAP_CATEGORY_IMAGE == category)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetServiceCapabilitiesResponse>");
		offset += snprintf(p_buf+offset, mlen-offset, image_capabilities);
		offset += snprintf(p_buf+offset, mlen-offset, "</timg:GetServiceCapabilitiesResponse>");
	}	
		
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_GetEventProperties_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset,soap_head, "http://www.onvif.org/ver10/events/wsdl/EventPortType/GetEventPropertiesResponse");
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tev:GetEventPropertiesResponse>"
			"<tev:TopicNamespaceLocation>"
				"http://www.onvif.org/onvif/ver10/topics/topicns.xml"
			"</tev:TopicNamespaceLocation>"
			"<wsnt:FixedTopicSet>true</wsnt:FixedTopicSet>"

			"<wstop:TopicSet xmlns=\"\">"
				"<tns1:RuleEngine>"
					"<CellMotionDetector>"
					"<Motion wstop:topic=\"true\">"
					"<tt:MessageDescription IsProperty=\"true\">"
					"<tt:Source>"
					"<tt:SimpleItemDescription Name=\"VideoSourceConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"VideoAnalyticsConfigurationToken\" Type=\"tt:ReferenceToken\"/>"
					"<tt:SimpleItemDescription Name=\"Rule\" Type=\"xs:string\"/>"
					"</tt:Source>"
					"<tt:Data>"
					"<tt:SimpleItemDescription Name=\"IsMotion\" Type=\"xs:boolean\"/>"
					"</tt:Data>"
					"</tt:MessageDescription>"
					"</Motion>"
					"</CellMotionDetector>"
				"</tns1:RuleEngine>"
			"</wstop:TopicSet>"

			"<wsnt:TopicExpressionDialect>"
				"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet"
			"</wsnt:TopicExpressionDialect>"
			"<wsnt:TopicExpressionDialect>"
				"http://docs.oasis-open.org/wsnt/t-1/TopicExpression/ConcreteSet"
			"</wsnt:TopicExpressionDialect>"
			"<wsnt:TopicExpressionDialect>"
				"http://docs.oasis-open.org/wsn/t-1/TopicExpression/Concrete"
			"</wsnt:TopicExpressionDialect>"	
			"<tev:MessageContentFilterDialect>"
				"http://www.onvif.org/ver10/tev/messageContentFilter/ItemFilter"
			"</tev:MessageContentFilterDialect>"
			"<tev:MessageContentSchemaLocation>"
				"http://www.onvif.org/onvif/ver10/schema/onvif.xsd"
			"</tev:MessageContentSchemaLocation>"
		"</tev:GetEventPropertiesResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


char subscibe[] = 
	"<wsnt:SubscribeResponse>"
		"<wsnt:SubscriptionReference>"
			"<wsa:Address>%s</wsa:Address>"
		"</wsnt:SubscriptionReference>"
		"<wsnt:CurrentTime>%s</wsnt:CurrentTime>"
		"<wsnt:TerminationTime>%s</wsnt:TerminationTime>"
	"</wsnt:SubscribeResponse>";

int build_Subscribe_rly_xml(char * p_buf, int mlen, const char * argv)
{
	EUA * p_eua = (EUA *) argv;
	if (NULL == p_eua)
	{
		return -1;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://docs.oasis-open.org/wsn/bw-2/NotificationProducer/SubscribeResponse");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	char cur_time[100], term_time[100];

	onvif_get_time_str(cur_time, sizeof(cur_time), 0);	
	
	if (g_onvif_cfg.evt_renew_time > p_eua->init_term_time)
	{
		onvif_get_time_str(term_time, sizeof(term_time), g_onvif_cfg.evt_renew_time);
	}
	else
	{
		onvif_get_time_str(term_time, sizeof(term_time), p_eua->init_term_time);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, subscibe, p_eua->producter_addr, cur_time, term_time);

	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_Unsubscribe_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://docs.oasis-open.org/wsn/bw-2/SubscriptionManager/UnsubscribeResponse");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<wsnt:UnsubscribeResponse></wsnt:UnsubscribeResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_Renew_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://docs.oasis-open.org/wsn/bw-2/SubscriptionManager/RenewResponse");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	char cur_time[100], term_time[100];

	onvif_get_time_str(cur_time, sizeof(cur_time), 0);
	onvif_get_time_str(term_time, sizeof(term_time), 60);
		
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<wsnt:RenewResponse>"			
	        "<wsnt:TerminationTime>%s</wsnt:TerminationTime>"
	        "<wsnt:CurrentTime>%s</wsnt:CurrentTime>"
		"</wsnt:RenewResponse>", term_time, cur_time);
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_SetSynchronizationPoint_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);

	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/SetSynchronizationPointResponse");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);	
		
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tev:SetSynchronizationPointResponse>"
		"</tev:SetSynchronizationPointResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_Notify_xml(char * p_buf, int mlen, const char * argv)
{
	EUA * p_eua = (EUA *) argv;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify");
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	time_t nowtime;
	struct tm *gtime;	
	char cur_time[100];

	time(&nowtime);
	gtime = gmtime(&nowtime);

	snprintf(cur_time, sizeof(cur_time), "%04d-%02d-%02dT%02d:%02d:%02dZ", 		 
		gtime->tm_year+1900, gtime->tm_mon+1, gtime->tm_mday,
		gtime->tm_hour, gtime->tm_min, gtime->tm_sec);
		
	offset += snprintf(p_buf+offset, mlen-offset, 
		"<wsnt:Notify>"
			"<wsnt:NotificationMessage>"
					"<wsnt:SubscriptionReference>"
						"<wsa:Address>%s</wsa:Address>"
				"</wsnt:SubscriptionReference>"
				"<wsnt:Topic Dialect=\"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet\">"
					"tns1:RuleEngine/CellMotionDetector/Motion"
				"</wsnt:Topic>"
				"<wsnt:ProducerReference>"
					"<wsa:Address>%s</wsa:Address>"
				"</wsnt:ProducerReference>"	
				"<wsnt:Message>"
					"<tt:Message UtcTime=\"%s\" PropertyOperation=\"Changed\">"
					"<tt:Source>"
						"<tt:SimpleItem Name=\"VideoSourceConfigurationToken\" Value=\"11\"/>"
						"<tt:SimpleItem Name=\"VideoAnalyticsConfigurationToken\" Value=\"12\"/>"
						"<tt:SimpleItem Name=\"Rule\" Value=\"TestRule\"/>"
					"</tt:Source>"
					"<tt:Data>"
						"<tt:SimpleItem Name=\"IsMotion\" Value=\"true\"/>"
					"</tt:Data>"
					"</tt:Message>"
				"</wsnt:Message>"
			"</wsnt:NotificationMessage>"
		"</wsnt:Notify>", 
		p_eua->consumer_addr, p_eua->producter_addr, cur_time);
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetWsdlUrl_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tds:GetWsdlUrlResponse>"
			"<tds:WsdlUrl>http://www.onvif.org/</tds:WsdlUrl>"
		"</tds:GetWsdlUrlResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_PTZNode_xml(char * p_buf, int mlen, PTZ_NODE * p_node)
{
	int offset = 0;
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:PTZNode token=\"%s\" FixedHomePosition=\"%s\">",
		p_node->token, p_node->fixed_home_pos ? "true" : "false");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Name>%s</tt:Name>", p_node->name);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:SupportedPTZSpaces>");

	if (p_node->abs_pantilt_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AbsolutePanTiltPositionSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
				"<tt:YRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:YRange>"
			"</tt:AbsolutePanTiltPositionSpace>", 
			p_node->abs_pantilt_x.min, p_node->abs_pantilt_x.max,
			p_node->abs_pantilt_y.min, p_node->abs_pantilt_y.max);
	}

	if (p_node->abs_zoom_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AbsoluteZoomPositionSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:AbsoluteZoomPositionSpace>", 
			p_node->abs_zoom.min, p_node->abs_zoom.max);
	}

	if (p_node->rel_pantilt_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:RelativePanTiltTranslationSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
				"<tt:YRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:YRange>"
			"</tt:RelativePanTiltTranslationSpace>",
			p_node->rel_pantilt_x.min, p_node->rel_pantilt_x.max,
			p_node->rel_pantilt_y.min, p_node->rel_pantilt_y.max);
	}

	if (p_node->rel_zoom_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 			
			"<tt:RelativeZoomTranslationSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:RelativeZoomTranslationSpace>", 
			p_node->rel_zoom.min, p_node->rel_zoom.max);	
	}

	if (p_node->con_pantilt_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 	
			"<tt:ContinuousPanTiltVelocitySpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
				"<tt:YRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:YRange>"
			"</tt:ContinuousPanTiltVelocitySpace>",
			p_node->con_pantilt_x.min, p_node->con_pantilt_x.max,
			p_node->con_pantilt_y.min, p_node->con_pantilt_y.max);	
	}

	if (p_node->con_zoom_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 	
			"<tt:ContinuousZoomVelocitySpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:ContinuousZoomVelocitySpace>",
			p_node->con_zoom.min, p_node->con_zoom.max);	
	}

	if (p_node->pantile_speed_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 	
			"<tt:PanTiltSpeedSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/GenericSpeedSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:PanTiltSpeedSpace>",  
			p_node->pantile_speed.min, p_node->pantile_speed.max);	
	}

	if (p_node->zoom_speed_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 			
			"<tt:ZoomSpeedSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/ZoomGenericSpeedSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:ZoomSpeedSpace>",
			p_node->zoom_speed.min, p_node->zoom_speed.max);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:SupportedPTZSpaces>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaximumNumberOfPresets>%d</tt:MaximumNumberOfPresets>", PTZ_MAX_PRESETS);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:HomeSupported>true</tt:HomeSupported>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZNode>"); 

	return offset;
}

int build_GetNodes_rly_xml(char * p_buf, int mlen, const char * argv)
{
	PTZ_NODE * p_node = g_onvif_cfg.ptznodes;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_head, "http://www.onvif.org/ver20/ptz/wsdl/GetNodesResponse");
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetNodesResponse>");

	while (p_node)
	{
		offset += build_PTZNode_xml(p_buf+offset, mlen-offset, p_node);

		p_node = p_node->next;
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetNodesResponse>");	
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_GetNode_rly_xml(char * p_buf, int mlen, const char * argv)
{
	PTZ_NODE * p_node = onvif_find_ptz_node(argv);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NO_ENTITY;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetNodeResponse>");

	offset += build_PTZNode_xml(p_buf+offset, mlen-offset, p_node);
	   
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetNodeResponse>");	
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetConfigurations_rly_xml(char * p_buf, int mlen, const char * argv)
{
	PTZ_NODE * p_node = g_onvif_cfg.ptznodes;
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetConfigurationsResponse>");

	while (p_node)
	{
		offset += build_PTZCfg_xml(p_buf+offset, mlen-offset, p_node, "tptz");

		p_node = p_node->next;
	}	
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetConfigurationsResponse>");	

	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	PTZ_NODE * p_node = onvif_find_ptz_cfg(argv);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NO_CONFIG;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetConfigurationResponse>");
	
	offset += build_PTZCfg_xml(p_buf+offset, mlen-offset, p_node, "tptz");
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetConfigurationResponse>");	

	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_AddPTZConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:AddPTZConfigurationResponse></trt:AddPTZConfigurationResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RemovePTZConfiguration_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<trt:RemovePTZConfigurationResponse></trt:RemovePTZConfigurationResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetConfigurationOptions_rly_xml(char * p_buf, int mlen, const char * argv)
{
	PTZ_NODE * p_node = onvif_find_ptz_cfg(argv);
	if (NULL == p_node)
	{
		return ONVIF_ERR_NO_CONFIG;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetConfigurationOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:PTZConfigurationOptions>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Spaces>");

	if (p_node->abs_pantilt_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AbsolutePanTiltPositionSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
				"<tt:YRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:YRange>"
			"</tt:AbsolutePanTiltPositionSpace>", 
			p_node->abs_pantilt_x.min, p_node->abs_pantilt_x.max,
			p_node->abs_pantilt_y.min, p_node->abs_pantilt_y.max);
	}

	if (p_node->abs_zoom_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:AbsoluteZoomPositionSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:AbsoluteZoomPositionSpace>", 
			p_node->abs_zoom.min, p_node->abs_zoom.max);
	}

	if (p_node->rel_pantilt_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tt:RelativePanTiltTranslationSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
				"<tt:YRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:YRange>"
			"</tt:RelativePanTiltTranslationSpace>",
			p_node->rel_pantilt_x.min, p_node->rel_pantilt_x.max,
			p_node->rel_pantilt_y.min, p_node->rel_pantilt_y.max);
	}

	if (p_node->rel_zoom_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 			
			"<tt:RelativeZoomTranslationSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:RelativeZoomTranslationSpace>", 
			p_node->rel_zoom.min, p_node->rel_zoom.max);	
	}

	if (p_node->con_pantilt_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 	
			"<tt:ContinuousPanTiltVelocitySpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
				"<tt:YRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:YRange>"
			"</tt:ContinuousPanTiltVelocitySpace>",
			p_node->con_pantilt_x.min, p_node->con_pantilt_x.max,
			p_node->con_pantilt_y.min, p_node->con_pantilt_y.max);	
	}

	if (p_node->con_zoom_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 	
			"<tt:ContinuousZoomVelocitySpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:ContinuousZoomVelocitySpace>",
			p_node->con_zoom.min, p_node->con_zoom.max);	
	}

	if (p_node->pantile_speed_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 	
			"<tt:PanTiltSpeedSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/GenericSpeedSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:PanTiltSpeedSpace>",  
			p_node->pantile_speed.min, p_node->pantile_speed.max);	
	}

	if (p_node->zoom_speed_space)
	{
		offset += snprintf(p_buf+offset, mlen-offset, 			
			"<tt:ZoomSpeedSpace>"
				"<tt:URI>http://www.onvif.org/ver10/tptz/ZoomSpaces/ZoomGenericSpeedSpace</tt:URI>"
				"<tt:XRange>"
					"<tt:Min>%0.1f</tt:Min>"
					"<tt:Max>%0.1f</tt:Max>"
				"</tt:XRange>"
			"</tt:ZoomSpeedSpace>",
			p_node->zoom_speed.min, p_node->zoom_speed.max);
	}
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Spaces>");

	offset += snprintf(p_buf+offset, mlen-offset, 
		"<tt:PTZTimeout>"
			"<tt:Min>PT%0.0fS</tt:Min>"
			"<tt:Max>PT%0.0fS</tt:Max>"
		"</tt:PTZTimeout>", 
		p_node->config.timeout_range.min, p_node->config.timeout_range.max);

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZConfigurationOptions>");	
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetConfigurationOptionsResponse>");	

	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_PTZ_GetStatus_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	else if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	PTZ_STATUS ptz_status;
	memset(&ptz_status, 0, sizeof(PTZ_STATUS));

	if (FALSE == ptz_GetStatus(p_profile, &ptz_status))
	{
		return -1;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetStatusResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:PTZStatus>");

	offset += snprintf(p_buf+offset, mlen-offset, 	
		"<tt:Position>"
			"<tt:PanTilt x=\"%0.1f\" y=\"%0.1f\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\" />"
			"<tt:Zoom x=\"%0.1f\" space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace\" />"
		"</tt:Position>"
		"<tt:MoveStatus>"
			"<tt:PanTilt>%s</tt:PanTilt>"
			"<tt:Zoom>%s</tt:Zoom>"
		"</tt:MoveStatus>",
		ptz_status.pantilt_pos_x, ptz_status.pantilt_pos_y, ptz_status.zoom_pos,
		onvif_get_ptz_status_str(ptz_status.move_sta), onvif_get_ptz_status_str(ptz_status.zoom_sta));

	if (strlen(ptz_status.error) > 0)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Error>%s</tt:Error>", ptz_status.error);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:UtcTime>%s</tt:UtcTime>", ptz_status.utc_time);
	
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:PTZStatus>");
	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetStatusResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}


int build_ContinuousMove_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:ContinuousMoveResponse></tptz:ContinuousMoveResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_PTZ_Stop_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:StopResponse></tptz:StopResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_AbsoluteMove_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:AbsoluteMoveResponse></tptz:AbsoluteMoveResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RelativeMove_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:RelativeMoveResponse></tptz:RelativeMoveResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetPreset_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, 
	    "<tptz:SetPresetResponse><tptz:PresetToken>%s</tptz:PresetToken></tptz:SetPresetResponse>", argv);	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetPresets_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(argv);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NO_PROFILE;
	}
	else if (NULL == p_profile->ptz_node)
	{
		return ONVIF_ERR_NO_PTZ_PROFILE;
	}

	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);

	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetPresetsResponse>");

	for (int i = 0; i < PTZ_MAX_PRESETS; i++)
	{
		if (p_profile->ptz_node->config.presets[i].used_flag == 0)
		{
			continue;
		}

		offset += snprintf(p_buf+offset, mlen-offset, 
			"<tptz:Preset token=\"%s\">"
			"<tt:Name>%s</tt:Name>"
			"<tt:PTZPosition>"
			"<tt:PanTilt x=\"%0.1f\" y=\"%0.1f\" space=\"\">"
			"</tt:PanTilt>"
			"<tt:Zoom x=\"%0.1f\" space=\"\">"
			"</tt:Zoom>"
			"</tt:PTZPosition>"
			"</tptz:Preset>",
			p_profile->ptz_node->config.presets[i].token,
			p_profile->ptz_node->config.presets[i].name,
			p_profile->ptz_node->config.presets[i].pantilt_pos_x,
			p_profile->ptz_node->config.presets[i].pantilt_pos_y,
			p_profile->ptz_node->config.presets[i].zoom_pos);
	}

	offset += snprintf(p_buf+offset, mlen-offset, "</tptz:GetPresetsResponse>");
	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_RemovePreset_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:RemovePresetResponse></tptz:RemovePresetResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GotoPreset_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GotoPresetResponse></tptz:GotoPresetResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GotoHomePosition_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GotoHomePositionResponse></tptz:GotoHomePositionResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetHomePosition_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:SetHomePositionResponse></tptz:SetHomePositionResponse>");	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetGuaranteedNumberOfVideoEncoderInstances_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<tptz:GetGuaranteedNumberOfVideoEncoderInstancesResponse>"
		"<trt:TotalNumber>%d</trt:TotalNumber><trt:JPEG>%d</trt:JPEG><trt:H264>%d</trt:H264><trt:MPEG4>%d</trt:MPEG4>"
		"</tptz:GetGuaranteedNumberOfVideoEncoderInstancesResponse>", 6, 1, 5, 2);	
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetImagingSettings_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(argv);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_SOURCE;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetImagingSettingsResponse><timg:ImagingSettings>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:BacklightCompensation><tt:Mode>%s</tt:Mode></tt:BacklightCompensation>",
		g_onvif_cfg.img_cfg.BacklightCompensation_Mode ? "ON" : "OFF");
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Brightness>%d</tt:Brightness><tt:ColorSaturation>%d</tt:ColorSaturation>"
		"<tt:Contrast>%d</tt:Contrast>", g_onvif_cfg.img_cfg.Brightness, g_onvif_cfg.img_cfg.ColorSaturation, g_onvif_cfg.img_cfg.Contrast);
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Exposure><tt:Mode>%s</tt:Mode>"
		"<tt:MinExposureTime>%d</tt:MinExposureTime><tt:MaxExposureTime>%d</tt:MaxExposureTime>"
		"<tt:MinGain>%d</tt:MinGain><tt:MaxGain>%d</tt:MaxGain></tt:Exposure>",
		g_onvif_cfg.img_cfg.Exposure_Mode ? "MANUAL" : "AUTO", g_onvif_cfg.img_cfg.MinExposureTime,
		g_onvif_cfg.img_cfg.MaxExposureTime, g_onvif_cfg.img_cfg.MinGain, g_onvif_cfg.img_cfg.MaxGain);
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilter>%s</tt:IrCutFilter><tt:Sharpness>%d</tt:Sharpness>", 
		g_onvif_cfg.img_cfg.IrCutFilter_Mode == 1 ? "ON" : (g_onvif_cfg.img_cfg.IrCutFilter_Mode == 2 ? "AUTO" : "OFF"),
		g_onvif_cfg.img_cfg.Sharpness);
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:WideDynamicRange><tt:Mode>%s</tt:Mode><tt:Level>%d</tt:Level></tt:WideDynamicRange>", 
		g_onvif_cfg.img_cfg.WideDynamicRange_Mode ? "ON" : "OFF", g_onvif_cfg.img_cfg.WideDynamicRange_Level);	
		
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:WhiteBalance><tt:Mode>%s</tt:Mode></tt:WhiteBalance>", 
		g_onvif_cfg.img_cfg.WhiteBalance_Mode ? "MANUAL" : "AUTO");	
		
	offset += snprintf(p_buf+offset, mlen-offset, "</timg:ImagingSettings></timg:GetImagingSettingsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_GetOptions_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(argv);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_SOURCE;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetOptionsResponse><timg:ImagingOptions>");

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:BacklightCompensation>");
	if (g_onvif_cfg.img_opt.BacklightCompensation_OFF)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>OFF</tt:Mode>");
	}
	if (g_onvif_cfg.img_opt.BacklightCompensation_ON)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>ON</tt:Mode>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:BacklightCompensation>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Brightness><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:Brightness>",
		g_onvif_cfg.img_opt.Brightness_min, g_onvif_cfg.img_opt.Brightness_max);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:ColorSaturation><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:ColorSaturation>",
		g_onvif_cfg.img_opt.ColorSaturation_min, g_onvif_cfg.img_opt.ColorSaturation_max);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Contrast><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:Contrast>",
		g_onvif_cfg.img_opt.Contrast_min, g_onvif_cfg.img_opt.Contrast_max);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Exposure>");
	if (g_onvif_cfg.img_opt.Exposure_AUTO)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>AUTO</tt:Mode>");
	}
	if (g_onvif_cfg.img_opt.Exposure_MANUAL)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>Manual</tt:Mode>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:MinExposureTime><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:MinExposureTime>",
		g_onvif_cfg.img_opt.MinExposureTime_min, g_onvif_cfg.img_opt.MinExposureTime_max);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaxExposureTime><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:MaxExposureTime>",
		g_onvif_cfg.img_opt.MaxExposureTime_min, g_onvif_cfg.img_opt.MaxExposureTime_max);	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:MinGain><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:MinGain>",
		g_onvif_cfg.img_opt.MinGain_min, g_onvif_cfg.img_opt.MinGain_max);
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:MaxGain><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:MaxGain>",
		g_onvif_cfg.img_opt.MaxGain_min, g_onvif_cfg.img_opt.MaxGain_max);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:Exposure>");
	
	if (g_onvif_cfg.img_opt.IrCutFilter_ON)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilterModes>ON</tt:IrCutFilterModes>");
	}
	if (g_onvif_cfg.img_opt.IrCutFilter_OFF)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilterModes>OFF</tt:IrCutFilterModes>");
	}
	if (g_onvif_cfg.img_opt.IrCutFilter_AUTO)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:IrCutFilterModes>AUTO</tt:IrCutFilterModes>");
	}

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Sharpness><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:Sharpness>",
		g_onvif_cfg.img_opt.Sharpness_min, g_onvif_cfg.img_opt.Sharpness_max);

	offset += snprintf(p_buf+offset, mlen-offset, "<tt:WideDynamicRange>");
	if (g_onvif_cfg.img_opt.WideDynamicRange_ON)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>ON</tt:Mode>");
	}
	if (g_onvif_cfg.img_opt.WideDynamicRange_OFF)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>OFF</tt:Mode>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:Level><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:Level>",
		g_onvif_cfg.img_opt.WideDynamicRange_Level_min, g_onvif_cfg.img_opt.WideDynamicRange_Level_max);
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:WideDynamicRange>");
	
	offset += snprintf(p_buf+offset, mlen-offset, "<tt:WhiteBalance>");
	if (g_onvif_cfg.img_opt.WhiteBalance_AUTO)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>AUTO</tt:Mode>");
	}
	if (g_onvif_cfg.img_opt.WhiteBalance_MANUAL)
	{
		offset += snprintf(p_buf+offset, mlen-offset, "<tt:Mode>Manual</tt:Mode>");
	}
	offset += snprintf(p_buf+offset, mlen-offset, "</tt:WhiteBalance>");

	offset += snprintf(p_buf+offset, mlen-offset, "</timg:ImagingOptions></timg:GetOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);

	return offset;
}

int build_SetImagingSettings_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:SetImagingSettingsResponse></timg:SetImagingSettingsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_GetMoveOptions_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(argv);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_SOURCE;
	}
	
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:GetMoveOptionsResponse><timg:MoveOptions>"
		"</timg:MoveOptions></timg:GetMoveOptionsResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_Move_rly_xml(char * p_buf, int mlen, const char * argv)
{
	int offset = snprintf(p_buf, mlen, xml_hdr);
	
	offset += snprintf(p_buf+offset, mlen-offset, onvif_xmlns);
	offset += snprintf(p_buf+offset, mlen-offset, soap_body);
	offset += snprintf(p_buf+offset, mlen-offset, "<timg:MoveResponse></timg:MoveResponse>");
	offset += snprintf(p_buf+offset, mlen-offset, soap_tailer);
	
	return offset;
}

int build_IMG_GetStatus_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(argv);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_SOURCE;
	}

	return ONVIF_ERR_NO_IMAGEING_FOR_SOURCE;
}

int build_IMG_Stop_rly_xml(char * p_buf, int mlen, const char * argv)
{
	ONVIF_V_SRC * p_v_src = onvif_find_video_source(argv);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NO_SOURCE;
	}

	return ONVIF_ERR_NO_IMAGEING_FOR_SOURCE;
}




