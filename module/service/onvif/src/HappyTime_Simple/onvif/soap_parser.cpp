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
#include "soap_parser.h"


/***************************************************************************************/

ONVIF_RET parse_SetVideoEncoderConfiguration(XMLN * p_node, SetVideoEncoderConfiguration_REQ * p_SetVideoEncoderConfiguration_req)
{
	XMLN * p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (!p_Configuration)
	{
		return ONVIF_ERR_MISSINGATTR;
	}

	const char * token = xml_attr_get(p_Configuration, "token");
	if (token)
	{
		strncpy(p_SetVideoEncoderConfiguration_req->token, token, sizeof(p_SetVideoEncoderConfiguration_req->token)-1);
	}

	XMLN * p_Name = xml_node_soap_get(p_Configuration, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_SetVideoEncoderConfiguration_req->name, p_Name->data, sizeof(p_SetVideoEncoderConfiguration_req->name)-1);
	}

	XMLN * p_UseCount = xml_node_soap_get(p_Configuration, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_SetVideoEncoderConfiguration_req->use_count = atoi(p_UseCount->data);
	}

	XMLN * p_Encoding = xml_node_soap_get(p_Configuration, "Encoding");
	if (p_Encoding && p_Encoding->data)
	{
		if (strcasecmp(p_Encoding->data, "H264") == 0)
		{
			p_SetVideoEncoderConfiguration_req->encoding = VIDEO_ENCODING_H264;
		}
		else if (strcasecmp(p_Encoding->data, "H265") == 0)
		{
			p_SetVideoEncoderConfiguration_req->encoding = VIDEO_ENCODING_H265;
		}
		else if (strcasecmp(p_Encoding->data, "JPEG") == 0)
		{
			p_SetVideoEncoderConfiguration_req->encoding = VIDEO_ENCODING_JPEG;
		}
		else if (strcasecmp(p_Encoding->data, "MPEG4") == 0)
		{
			p_SetVideoEncoderConfiguration_req->encoding = VIDEO_ENCODING_MPEG4;
		}
	}

	XMLN * p_Resolution = xml_node_soap_get(p_Configuration, "Resolution");
	if (p_Resolution)
	{
		XMLN * p_Width = xml_node_soap_get(p_Resolution, "Width");
		if (p_Width && p_Width->data)
		{
			p_SetVideoEncoderConfiguration_req->width = atoi(p_Width->data);
		}

		XMLN * p_Height = xml_node_soap_get(p_Resolution, "Height");
		if (p_Height && p_Height->data)
		{
			p_SetVideoEncoderConfiguration_req->height = atoi(p_Height->data);
		}
	}

	XMLN * p_Quality = xml_node_soap_get(p_Configuration, "Quality");
	if (p_Quality && p_Quality->data)
	{
		p_SetVideoEncoderConfiguration_req->quality = atoi(p_Quality->data);
	}

	XMLN * p_RateControl = xml_node_soap_get(p_Configuration, "RateControl");
	if (p_RateControl)
	{
		XMLN * p_FrameRateLimit = xml_node_soap_get(p_RateControl, "FrameRateLimit");
		if (p_FrameRateLimit && p_FrameRateLimit->data)
		{
			p_SetVideoEncoderConfiguration_req->framerate_limit = atoi(p_FrameRateLimit->data);
		}

		XMLN * p_EncodingInterval = xml_node_soap_get(p_RateControl, "EncodingInterval");
		if (p_EncodingInterval && p_EncodingInterval->data)
		{
			p_SetVideoEncoderConfiguration_req->encoding_interval = atoi(p_EncodingInterval->data);
		}

		XMLN * p_BitrateLimit = xml_node_soap_get(p_RateControl, "BitrateLimit");
		if (p_BitrateLimit && p_BitrateLimit->data)
		{
			p_SetVideoEncoderConfiguration_req->bitrate_limit = atoi(p_BitrateLimit->data);
		}
	}

	if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_H264)
	{
		XMLN * p_H264 = xml_node_soap_get(p_Configuration, "H264");
		if (p_H264)
		{
			XMLN * p_GovLength = xml_node_soap_get(p_H264, "GovLength");
			if (p_GovLength && p_GovLength->data)
			{
				p_SetVideoEncoderConfiguration_req->gov_len = atoi(p_GovLength->data);
			}

			XMLN * p_H264Profile = xml_node_soap_get(p_H264, "H264Profile");
			if (p_H264Profile && p_H264Profile->data)
			{
				if (strcasecmp(p_H264Profile->data, "Baseline") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H264_PROFILE_Baseline;
				}
				else if (strcasecmp(p_H264Profile->data, "Main") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H264_PROFILE_Main;
				}
				else if (strcasecmp(p_H264Profile->data, "High") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H264_PROFILE_High;
				}
				else if (strcasecmp(p_H264Profile->data, "Extended") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H264_PROFILE_Extended;
				}
			}
		}
	}
	else if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_H265)
	{
		XMLN * p_H265 = xml_node_soap_get(p_Configuration, "H265");
		if (p_H265)
		{
			XMLN * p_GovLength = xml_node_soap_get(p_H265, "GovLength");
			if (p_GovLength && p_GovLength->data)
			{
				p_SetVideoEncoderConfiguration_req->gov_len = atoi(p_GovLength->data);
			}

			XMLN * p_H265Profile = xml_node_soap_get(p_H265, "H265Profile");
			if (p_H265Profile && p_H265Profile->data)
			{
				if (strcasecmp(p_H265Profile->data, "Baseline") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H265_PROFILE_Baseline;
				}
				else if (strcasecmp(p_H265Profile->data, "Main") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H265_PROFILE_Main;
				}
				else if (strcasecmp(p_H265Profile->data, "High") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H265_PROFILE_High;
				}
				else if (strcasecmp(p_H265Profile->data, "Extended") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = H265_PROFILE_Extended;
				}
			}
		}
	}
	else if (p_SetVideoEncoderConfiguration_req->encoding == VIDEO_ENCODING_MPEG4)
	{
		XMLN * p_MPEG4 = xml_node_soap_get(p_Configuration, "MPEG4");
		if (p_MPEG4)
		{
			XMLN * p_GovLength = xml_node_soap_get(p_MPEG4, "GovLength");
			if (p_GovLength && p_GovLength->data)
			{
				p_SetVideoEncoderConfiguration_req->gov_len = atoi(p_GovLength->data);
			}

			XMLN * p_Mpeg4Profile = xml_node_soap_get(p_MPEG4, "Mpeg4Profile");
			if (p_Mpeg4Profile && p_Mpeg4Profile->data)
			{
				if (strcasecmp(p_Mpeg4Profile->data, "SP") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = MPEG4_PROFILE_SP;
				}
				else if (strcasecmp(p_Mpeg4Profile->data, "ASP") == 0)
				{
					p_SetVideoEncoderConfiguration_req->profile = MPEG4_PROFILE_ASP;
				}
			}
		}
	}	
	
	XMLN * p_SessionTimeout = xml_node_soap_get(p_Configuration, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		p_SetVideoEncoderConfiguration_req->session_timeout = atoi(p_SessionTimeout->data);
	}

	XMLN * p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		if (strcasecmp(p_ForcePersistence->data, "true") == 0)
		{
			p_SetVideoEncoderConfiguration_req->persistence = TRUE;
		}
	}
		
	return ONVIF_OK;
}


ONVIF_RET parse_SetSystemDateAndTime(XMLN * p_node, SetSystemDateAndTime_REQ * p_SetSystemDateAndTime_req)
{
    XMLN * p_DateTimeType = xml_node_soap_get(p_node, "DateTimeType");
    if (!p_DateTimeType || !p_DateTimeType->data)
    {
        return ONVIF_ERR_MISSINGATTR;
    }
    p_SetSystemDateAndTime_req->type = (strcasecmp(p_DateTimeType->data, "NTP") ? 0 : 1);

    XMLN * p_DaylightSavings = xml_node_soap_get(p_node, "DaylightSavings");
    if (!p_DaylightSavings || !p_DaylightSavings->data)
    {
        return ONVIF_ERR_MISSINGATTR;
    }
    p_SetSystemDateAndTime_req->DaylightSavings = (strcasecmp(p_DaylightSavings->data, "true") ? false : true);

    XMLN * p_TimeZone = xml_node_soap_get(p_node, "TimeZone");
    if (p_TimeZone)
    {
        XMLN * p_TZ = xml_node_soap_get(p_TimeZone, "TZ");
		if (p_TZ && p_TZ->data)
		{
			strncpy(p_SetSystemDateAndTime_req->TZ, p_TZ->data, sizeof(p_SetSystemDateAndTime_req->TZ)-1);
		}		
    }    

    XMLN * p_UTCDateTime = xml_node_soap_get(p_node, "UTCDateTime");
    if (p_UTCDateTime)
    {
        XMLN * p_Time = xml_node_soap_get(p_UTCDateTime, "Time");
	    if (!p_Time)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }

	    XMLN * p_Hour = xml_node_soap_get(p_Time, "Hour");
	    if (!p_Hour || !p_Hour->data)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }
	    p_SetSystemDateAndTime_req->hour = atoi(p_Hour->data);

	    XMLN * p_Minute = xml_node_soap_get(p_Time, "Minute");
	    if (!p_Minute || !p_Minute->data)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }
	    p_SetSystemDateAndTime_req->minute = atoi(p_Minute->data);

	    XMLN * p_Second = xml_node_soap_get(p_Time, "Second");
	    if (!p_Second || !p_Second->data)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }
	    p_SetSystemDateAndTime_req->second = atoi(p_Second->data);

	    XMLN * p_Date = xml_node_soap_get(p_UTCDateTime, "Date");
	    if (!p_Date)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }

	    XMLN * p_Year = xml_node_soap_get(p_Date, "Year");
	    if (!p_Year || !p_Year->data)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }
	    p_SetSystemDateAndTime_req->year = atoi(p_Year->data);

	    XMLN * p_Month = xml_node_soap_get(p_Date, "Month");
	    if (!p_Month || !p_Month->data)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }
	    p_SetSystemDateAndTime_req->month = atoi(p_Month->data);

	    XMLN * p_Day = xml_node_soap_get(p_Date, "Day");
	    if (!p_Day || !p_Day->data)
	    {
	        return ONVIF_ERR_MISSINGATTR;
	    }
	    p_SetSystemDateAndTime_req->day = atoi(p_Day->data);
    }    

    return ONVIF_OK;
}

ONVIF_RET parse_AddScopes(XMLN * p_AddScopes, ONVIF_SCOPE * p_scope, int scope_max)
{
	int i = 0;
	
	XMLN * p_ScopeItem = xml_node_soap_get(p_AddScopes, "ScopeItem");
	while (p_ScopeItem)
	{
		if (i < scope_max)
		{
			p_scope[i].fixed = false;
			strncpy(p_scope[i].scope, p_ScopeItem->data, sizeof(p_scope[i].scope)-1);

			++i;
		}
		else
		{
			return ONVIF_ERR_TOO_MANY_SCOPES;
		}
		
		p_ScopeItem = p_ScopeItem->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_SetScopes(XMLN * p_AddScopes, ONVIF_SCOPE * p_scope, int scope_max)
{
	int i = 0;
	
	XMLN * p_Scopes = xml_node_soap_get(p_AddScopes, "Scopes");
	while (p_Scopes)
	{
		if (i < scope_max)
		{
			p_scope[i].fixed = false;
			strncpy(p_scope[i].scope, p_Scopes->data, sizeof(p_scope[i].scope)-1);

			++i;
		}
		else
		{
			return ONVIF_ERR_TOO_MANY_SCOPES;
		}
		
		p_Scopes = p_Scopes->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_SetDiscoveryMode(XMLN * p_node, SetDiscoveryMode_REQ * p_SetDiscoveryMode_req)
{
	XMLN * p_DiscoveryMode = xml_node_soap_get(p_node, "DiscoveryMode");
	if (p_DiscoveryMode && p_DiscoveryMode->data)
	{
		if (strcasecmp(p_DiscoveryMode->data, "NonDiscoverable") == 0)
		{
			p_SetDiscoveryMode_req->discoverable = FALSE;
		}
		else if (strcasecmp(p_DiscoveryMode->data, "Discoverable") == 0)
		{
			p_SetDiscoveryMode_req->discoverable = TRUE;
		}		
	}

	return ONVIF_OK;
}


ONVIF_RET parse_Subscribe(XMLN * p_node, Subscribe_REQ * p_Subscribe_req)
{
	assert(p_node);

	XMLN * p_ConsumerReference = xml_node_soap_get(p_node, "ConsumerReference");
	if (NULL == p_ConsumerReference)
	{
		return ONVIF_ERR_MISSINGATTR;		
	}

	XMLN * p_Address = xml_node_soap_get(p_ConsumerReference, "Address");
	if (p_Address && p_Address->data)
	{
		strncpy(p_Subscribe_req->consumer_addr, p_Address->data, sizeof(p_Subscribe_req->consumer_addr)-1);
	}	

	XMLN * p_InitialTerminationTime = xml_node_soap_get(p_node, "InitialTerminationTime");
	if (p_InitialTerminationTime && p_InitialTerminationTime->data)
	{
		p_Subscribe_req->init_term_time = atoi(p_InitialTerminationTime->data+2);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_Renew(XMLN * p_node, Renew_REQ * p_Renew_req)
{
	assert(p_node);

	XMLN * p_TerminationTime = xml_node_soap_get(p_node, "TerminationTime");
	if (p_TerminationTime && p_TerminationTime->data)
	{
		p_Renew_req->term_time = atoi(p_TerminationTime->data+2);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_AddPTZConfiguration(XMLN * p_node, AddPTZConfiguration_REQ * p_AddPTZConfiguration_req)
{
	assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_AddPTZConfiguration_req->profile_token, p_ProfileToken->data, sizeof(p_AddPTZConfiguration_req->profile_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_AddPTZConfiguration_req->config_token, p_ConfigurationToken->data, sizeof(p_AddPTZConfiguration_req->config_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_ContinuousMove(XMLN * p_node, ContinuousMove_REQ * p_ContinuousMove_req)
{
	assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_ContinuousMove_req->profile_token, p_ProfileToken->data, sizeof(p_ContinuousMove_req->profile_token)-1);
	}
	
	XMLN * p_Velocity = xml_node_soap_get(p_node, "Velocity");
	if (p_Velocity)
	{	
		XMLN * p_PanTilt = xml_node_soap_get(p_Velocity, "PanTilt");
		if (p_PanTilt)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_ContinuousMove_req->pantilt_velocity_x = atof(p_x);
			}

			const char * p_y = xml_attr_get(p_PanTilt, "y");
			if (p_y)
			{
				p_ContinuousMove_req->pantilt_velocity_y = atof(p_y);
			}
		}

		XMLN * p_Zoom = xml_node_soap_get(p_Velocity, "Zoom");
		if (p_Zoom)
		{	
			const char * p_x = xml_attr_get(p_Zoom, "x");
			if (p_x)
			{
				p_ContinuousMove_req->zoom_velocity = atof(p_x);
			}
		}
	}

/*	XMLN * p_Timeout = xml_node_soap_get(p_node, "Timeout");
	if (p_Timeout && p_Timeout->data)
	{
		p_ContinuousMove_req->timeout = atoi(p_Timeout->data+2);
	}*/
	
	return ONVIF_OK;
}

ONVIF_RET parse_PTZ_Stop(XMLN * p_node, PTZ_Stop_REQ * p_Stop_req)
{
	assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_Stop_req->profile_token, p_ProfileToken->data, sizeof(p_Stop_req->profile_token)-1);
	}

	p_Stop_req->stop_pantile = true;
	p_Stop_req->stop_zoom = true;
	
	XMLN * p_PanTilt = xml_node_soap_get(p_node, "PanTilt");
	if (p_PanTilt && p_PanTilt->data)
	{
		if (strcasecmp(p_PanTilt->data, "false") == 0)
		{
			p_Stop_req->stop_pantile = false;
		}
	}

	XMLN * p_Zoom = xml_node_soap_get(p_node, "Zoom");
	if (p_Zoom && p_Zoom->data)
	{
		if (strcasecmp(p_Zoom->data, "false") == 0)
		{
			p_Stop_req->stop_zoom = false;
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_AbsoluteMove(XMLN * p_node, AbsoluteMove_REQ * p_AbsoluteMove_req)
{
    assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_AbsoluteMove_req->profile_token, p_ProfileToken->data, sizeof(p_AbsoluteMove_req->profile_token)-1);
	}

    XMLN * p_Position = xml_node_soap_get(p_node, "Position");
	if (p_Position)
	{	
	    XMLN * p_PanTilt = xml_node_soap_get(p_Position, "PanTilt");
		if (p_PanTilt)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_AbsoluteMove_req->pantilt_pos_x = atof(p_x);
			}

			const char * p_y = xml_attr_get(p_PanTilt, "x");
			if (p_y)
			{
				p_AbsoluteMove_req->pantilt_pos_y = atof(p_y);
			}
		}

		XMLN * p_Zoom = xml_node_soap_get(p_Position, "Zoom");
		if (p_Zoom)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_AbsoluteMove_req->zoom_pos = atof(p_x);
			}
		}
	}

	XMLN * p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
	    XMLN * p_PanTilt = xml_node_soap_get(p_Speed, "PanTilt");
		if (p_PanTilt)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_AbsoluteMove_req->pantilt_speed_x = atof(p_x);
			}

			const char * p_y = xml_attr_get(p_PanTilt, "x");
			if (p_y)
			{
				p_AbsoluteMove_req->pantilt_speed_y = atof(p_y);
			}
		}

		XMLN * p_Zoom = xml_node_soap_get(p_Speed, "Zoom");
		if (p_Zoom)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_AbsoluteMove_req->zoom_speed = atof(p_x);
			}
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_RelativeMove(XMLN * p_node, RelativeMove_REQ * p_RelativeMove_req)
{
    assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_RelativeMove_req->profile_token, p_ProfileToken->data, sizeof(p_RelativeMove_req->profile_token)-1);
	}

    XMLN * p_Translation = xml_node_soap_get(p_node, "Translation");
	if (p_Translation)
	{	
	    XMLN * p_PanTilt = xml_node_soap_get(p_Translation, "PanTilt");
		if (p_PanTilt)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_RelativeMove_req->pantilt_pos_x = atof(p_x);
			}

			const char * p_y = xml_attr_get(p_PanTilt, "x");
			if (p_y)
			{
				p_RelativeMove_req->pantilt_pos_y = atof(p_y);
			}
		}

		XMLN * p_Zoom = xml_node_soap_get(p_Translation, "Zoom");
		if (p_Zoom)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_RelativeMove_req->zoom_pos = atof(p_x);
			}
		}
	}

	XMLN * p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
	    XMLN * p_PanTilt = xml_node_soap_get(p_Speed, "PanTilt");
		if (p_PanTilt)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_RelativeMove_req->pantilt_speed_x = atof(p_x);
			}

			const char * p_y = xml_attr_get(p_PanTilt, "x");
			if (p_y)
			{
				p_RelativeMove_req->pantilt_speed_y = atof(p_y);
			}
		}

		XMLN * p_Zoom = xml_node_soap_get(p_Speed, "Zoom");
		if (p_Zoom)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_RelativeMove_req->zoom_speed = atof(p_x);
			}
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_SetPreset(XMLN * p_node, SetPreset_REQ * p_SetPreset_req)
{
    assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_SetPreset_req->profile_token, p_ProfileToken->data, sizeof(p_SetPreset_req->profile_token)-1);
	}

	XMLN * p_PresetName = xml_node_soap_get(p_node, "PresetName");
	if (p_PresetName && p_PresetName->data)
	{
		strncpy(p_SetPreset_req->name, p_PresetName->data, sizeof(p_SetPreset_req->name)-1);
	}

	XMLN * p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
	if (p_PresetToken && p_PresetToken->data)
	{
		strncpy(p_SetPreset_req->preset_token, p_PresetToken->data, sizeof(p_SetPreset_req->preset_token)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_RemovePreset(XMLN * p_node, RemovePreset_REQ * p_RemovePreset_req)
{
	assert(p_node);

    XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_RemovePreset_req->profile_token, p_ProfileToken->data, sizeof(p_RemovePreset_req->profile_token)-1);
	}

	XMLN * p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
	if (p_PresetToken && p_PresetToken->data)
	{
		strncpy(p_RemovePreset_req->preset_token, p_PresetToken->data, sizeof(p_RemovePreset_req->preset_token)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_GotoPreset(XMLN * p_node, GotoPreset_REQ * p_GotoPreset_req)
{
	assert(p_node);

    XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_GotoPreset_req->profile_token, p_ProfileToken->data, sizeof(p_GotoPreset_req->profile_token)-1);
	}

	XMLN * p_PresetToken = xml_node_soap_get(p_node, "PresetToken");
	if (p_PresetToken && p_PresetToken->data)
	{
		strncpy(p_GotoPreset_req->preset_token, p_PresetToken->data, sizeof(p_GotoPreset_req->preset_token)-1);
	}

	XMLN * p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
	    XMLN * p_PanTilt = xml_node_soap_get(p_Speed, "PanTilt");
		if (p_PanTilt)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_GotoPreset_req->pantilt_speed_x = atof(p_x);
			}

			const char * p_y = xml_attr_get(p_PanTilt, "x");
			if (p_y)
			{
				p_GotoPreset_req->pantilt_speed_y = atof(p_y);
			}
		}

		XMLN * p_Zoom = xml_node_soap_get(p_Speed, "Zoom");
		if (p_Zoom)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_GotoPreset_req->zoom_speed = atof(p_x);
			}
		}
	}

	return ONVIF_OK;
}

ONVIF_RET parse_GotoHomePosition(XMLN * p_node, GotoHomePosition_REQ * p_GotoHomePosition_req)
{
    assert(p_node);

    XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_GotoHomePosition_req->profile_token, p_ProfileToken->data, sizeof(p_GotoHomePosition_req->profile_token)-1);
	}

	XMLN * p_Speed = xml_node_soap_get(p_node, "Speed");
	if (p_Speed)
	{	
	    XMLN * p_PanTilt = xml_node_soap_get(p_Speed, "PanTilt");
		if (p_PanTilt)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_GotoHomePosition_req->pantilt_speed_x = atof(p_x);
			}

			const char * p_y = xml_attr_get(p_PanTilt, "x");
			if (p_y)
			{
				p_GotoHomePosition_req->pantilt_speed_y = atof(p_y);
			}
		}

		XMLN * p_Zoom = xml_node_soap_get(p_Speed, "Zoom");
		if (p_Zoom)
		{	
			const char * p_x = xml_attr_get(p_PanTilt, "x");
			if (p_x)
			{
				p_GotoHomePosition_req->zoom_speed = atof(p_x);
			}
		}
	}

	return ONVIF_OK;
}


ONVIF_RET parse_SetDNS(XMLN * p_node, SetDNS_REQ * p_SetDNS_req)
{
	assert(p_node);

	XMLN * p_FromDHCP = xml_node_soap_get(p_node, "FromDHCP");
	if (p_FromDHCP && p_FromDHCP->data)
	{
		if (strcasecmp(p_FromDHCP->data, "true") == 0)
		{
			p_SetDNS_req->fromdhcp = true;
		}
	}

	int i = 0;
	
	XMLN * p_SearchDomain = xml_node_soap_get(p_node, "SearchDomain");
	while (p_SearchDomain && soap_strcmp(p_SearchDomain->name, "SearchDomain") == 0)
	{
		if (p_SearchDomain->data && i < MAX_SEARCHDOMAIN)
		{
			strncpy(p_SetDNS_req->dns_searchdomain[i], p_SearchDomain->data, sizeof(p_SetDNS_req->dns_searchdomain[i])-1);
			++i;
		}

		p_SearchDomain = p_SearchDomain->next;
	}

	i = 0;
	
	XMLN * p_DNSManual = xml_node_soap_get(p_node, "DNSManual");
	while (p_DNSManual && soap_strcmp(p_DNSManual->name, "DNSManual") == 0)
	{
		XMLN * p_Type = xml_node_soap_get(p_DNSManual, "Type");
		if (p_Type && p_Type->data)
		{
			if (strcasecmp(p_Type->data, "IPv4") != 0) // only support ipv4
			{
				continue;
			}
		}

		XMLN * p_IPv4Address = xml_node_soap_get(p_DNSManual, "IPv4Address");
		if (p_IPv4Address && p_IPv4Address->data)
		{
			if (is_ip_address(p_IPv4Address->data) == FALSE)
			{
				return ONVIF_ERR_INVALID_IPV4_ADDR;
			}
			else if (i < MAX_DNS_SERVER)
			{
				strncpy(p_SetDNS_req->dns_server[i], p_IPv4Address->data, sizeof(p_SetDNS_req->dns_server[i])-1);
				++i;
			}
		}
		
		p_DNSManual = p_DNSManual->next;
	}

	return ONVIF_OK;	
}

ONVIF_RET parse_SetNTP(XMLN * p_node, SetNTP_REQ * p_SetNTP_req)
{
	assert(p_node);

	XMLN * p_FromDHCP = xml_node_soap_get(p_node, "FromDHCP");
	if (p_FromDHCP && p_FromDHCP->data)
	{
		if (strcasecmp(p_FromDHCP->data, "true") == 0)
		{
			p_SetNTP_req->fromdhcp = true;
		}
	}

	int i = 0;
	
	XMLN * p_NTPManual = xml_node_soap_get(p_node, "NTPManual");
	while (p_NTPManual && soap_strcmp(p_NTPManual->name, "NTPManual") == 0)
	{
		XMLN * p_Type = xml_node_soap_get(p_NTPManual, "Type");
		if (p_Type && p_Type->data)
		{
			if (strcasecmp(p_Type->data, "IPv4") != 0) // only support ipv4
			{
				continue;
			}
		}

		XMLN * p_IPv4Address = xml_node_soap_get(p_NTPManual, "IPv4Address");
		if (p_IPv4Address && p_IPv4Address->data)
		{
			if (is_ip_address(p_IPv4Address->data) == FALSE)
			{
				return ONVIF_ERR_INVALID_IPV4_ADDR;
			}
			else if (i < MAX_NTP_SERVER)
			{
				strncpy(p_SetNTP_req->ntp_server[i], p_IPv4Address->data, sizeof(p_SetNTP_req->ntp_server[i])-1);
				++i;
			}
		}

		XMLN * p_DNSname = xml_node_soap_get(p_NTPManual, "DNSname");
		if (p_DNSname && p_DNSname->data)
		{
			if (i < MAX_NTP_SERVER)
			{
				strncpy(p_SetNTP_req->ntp_server[i], p_DNSname->data, sizeof(p_SetNTP_req->ntp_server[i])-1);
				++i;
			}
		}
		
		p_NTPManual = p_NTPManual->next;
	}

	return ONVIF_OK;
}

ONVIF_RET parse_SetNetworkProtocols(XMLN * p_node, SetNetworkProtocols_REQ * p_SetNetworkProtocols_req)
{
	assert(p_node);

	char name[32];
	bool enable;
	int  port[MAX_SERVER_PORT];
	
	XMLN * p_NetworkProtocols = xml_node_soap_get(p_node, "NetworkProtocols");
	while (p_NetworkProtocols && strcasecmp(p_NetworkProtocols->name, "NetworkProtocols") == 0)
	{
		enable = false;
		memset(name, 0, sizeof(name));
		memset(port, 0, sizeof(int)*MAX_SERVER_PORT);
		
		XMLN * p_Name = xml_node_soap_get(p_NetworkProtocols, "Name");
		if (p_Name && p_Name->data)
		{
			strncpy(name, p_Name->data, sizeof(name)-1);
		}

		XMLN * p_Enabled = xml_node_soap_get(p_NetworkProtocols, "Enabled");
		if (p_Enabled && p_Enabled->data)
		{
			if (strcasecmp(p_Enabled->data, "true") == 0)
			{
				enable = true;
			}
		}

		int i = 0;
		
		XMLN * p_Port = xml_node_soap_get(p_NetworkProtocols, "Port");
		while (p_Port && p_Port->data && strcasecmp(p_Port->name, "Port") == 0)
		{
			if (i < MAX_SERVER_PORT)
			{
				port[i++] = atoi(p_Port->data);
			}
			
			p_Port = p_Port->next;
		}

		if (strcasecmp(name, "HTTP") == 0)
		{
			p_SetNetworkProtocols_req->http_flag = 1;
			p_SetNetworkProtocols_req->http_enable = enable;
			memcpy(p_SetNetworkProtocols_req->http_port, port, sizeof(int)*MAX_SERVER_PORT);
		}
		else if (strcasecmp(name, "HTTPS") == 0)
		{
			p_SetNetworkProtocols_req->https_flag = 1;
			p_SetNetworkProtocols_req->https_enable = enable;
			memcpy(p_SetNetworkProtocols_req->https_port, port, sizeof(int)*MAX_SERVER_PORT);
		}
		else if (strcasecmp(name, "RTSP") == 0)
		{
			p_SetNetworkProtocols_req->rtsp_flag = 1;
			p_SetNetworkProtocols_req->rtsp_enable = enable;
			memcpy(p_SetNetworkProtocols_req->rtsp_port, port, sizeof(int)*MAX_SERVER_PORT);
		}
		else
		{
			return ONVIF_ERR_SERVICE_NOT_SUPPORT;
		}

		p_NetworkProtocols = p_NetworkProtocols->next;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_SetNetworkDefaultGateway(XMLN * p_node, SetNetworkDefaultGateway_REQ * p_SetNetworkDefaultGateway_req)
{
	assert(p_node);

	int i = 0;
	
	XMLN * p_IPv4Address = xml_node_soap_get(p_node, "IPv4Address");
	while (p_IPv4Address && p_IPv4Address->data && strcasecmp(p_IPv4Address->name, "IPv4Address") == 0)
	{
		if (is_ip_address(p_IPv4Address->data) == FALSE)
		{
			return ONVIF_ERR_INVALID_IPV4_ADDR;
		}

		if (i < MAX_GATEWAY)
		{
			strncpy(p_SetNetworkDefaultGateway_req->gateway[i++], p_IPv4Address->data, sizeof(p_SetNetworkDefaultGateway_req->gateway[0])-1);
		}

		p_IPv4Address = p_IPv4Address->next;
	}

	return ONVIF_OK;
}


ONVIF_RET parse_CreateProfile(XMLN * p_node, CreateProfile_REQ * p_CreateProfile_req)
{
	assert(p_node);

	XMLN * p_Name = xml_node_soap_get(p_node, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_CreateProfile_req->name, p_Name->data, sizeof(p_CreateProfile_req->name)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	XMLN * p_Token = xml_node_soap_get(p_node, "Token");
	if (p_Token && p_Token->data)
	{
		strncpy(p_CreateProfile_req->token, p_Token->data, sizeof(p_CreateProfile_req->token)-1);
	}

	return ONVIF_OK;
}

ONVIF_RET parse_AddVideoSourceConfiguration(XMLN * p_node, AddVideoSourceConfiguration_REQ * p_AddVideoSourceConfiguration_req)
{
	assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_AddVideoSourceConfiguration_req->profile_token, p_ProfileToken->data, sizeof(p_AddVideoSourceConfiguration_req->profile_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_AddVideoSourceConfiguration_req->config_token, p_ConfigurationToken->data, sizeof(p_AddVideoSourceConfiguration_req->config_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_AddVideoEncoderConfiguration(XMLN * p_node, AddVideoEncoderConfiguration_REQ * p_AddVideoEncoderConfiguration_req)
{
	assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_AddVideoEncoderConfiguration_req->profile_token, p_ProfileToken->data, sizeof(p_AddVideoEncoderConfiguration_req->profile_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_AddVideoEncoderConfiguration_req->config_token, p_ConfigurationToken->data, sizeof(p_AddVideoEncoderConfiguration_req->config_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	return ONVIF_OK;
}


ONVIF_RET parse_AddAudioSourceConfiguration(XMLN * p_node, AddAudioSourceConfiguration_REQ * p_AddAudioSourceConfiguration_req)
{
	assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_AddAudioSourceConfiguration_req->profile_token, p_ProfileToken->data, sizeof(p_AddAudioSourceConfiguration_req->profile_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_AddAudioSourceConfiguration_req->config_token, p_ConfigurationToken->data, sizeof(p_AddAudioSourceConfiguration_req->config_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_AddAudioEncoderConfiguration(XMLN * p_node, AddAudioEncoderConfiguration_REQ * p_AddAudioEncoderConfiguration_req)
{
	assert(p_node);

	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_AddAudioEncoderConfiguration_req->profile_token, p_ProfileToken->data, sizeof(p_AddAudioEncoderConfiguration_req->profile_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_AddAudioEncoderConfiguration_req->config_token, p_ConfigurationToken->data, sizeof(p_AddAudioEncoderConfiguration_req->config_token)-1);
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_GetStreamUri(XMLN * p_node, GetStreamUri_REQ * p_GetStreamUri_req)
{
	XMLN * p_StreamSetup = xml_node_soap_get(p_node, "StreamSetup");
	if (p_StreamSetup)
	{
		XMLN * p_Stream = xml_node_soap_get(p_StreamSetup, "Stream");
		if (p_Stream && p_Stream->data)
		{
			if (strcasecmp(p_Stream->data, "RTP-Unicast") == 0)
			{
				p_GetStreamUri_req->stream_type = 0;
			}
			else if (strcasecmp(p_Stream->data, "RTP-Multicast") == 0)
			{
				p_GetStreamUri_req->stream_type = 1;
			}
		}

		XMLN * p_Transport = xml_node_soap_get(p_StreamSetup, "Transport");
		if (p_Transport)
		{
			XMLN * p_Protocol = xml_node_soap_get(p_Transport, "Protocol");
			if (p_Protocol && p_Protocol->data)
			{
				if (strcasecmp(p_Protocol->data, "UDP") == 0)
				{
					p_GetStreamUri_req->protocol = 0;
				}
				else if (strcasecmp(p_Protocol->data, "TCP") == 0)
				{
					p_GetStreamUri_req->protocol = 1;
				}
				else if (strcasecmp(p_Protocol->data, "RTSP") == 0)
				{
					p_GetStreamUri_req->protocol = 2;
				}
				else if (strcasecmp(p_Protocol->data, "HTTP") == 0)
				{
					p_GetStreamUri_req->protocol = 3;
				}
			}
		}
		
	}
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_GetStreamUri_req->profile_token, p_ProfileToken->data, sizeof(p_GetStreamUri_req->profile_token)-1);
	}
	
	return ONVIF_OK;
}


ONVIF_RET parse_SetNetworkInterfaces(XMLN * p_node, SetNetworkInterfaces_REQ * p_SetNetworkInterfaces_req)
{
    XMLN * p_InterfaceToken = xml_node_soap_get(p_node, "InterfaceToken");
	if (p_InterfaceToken && p_InterfaceToken->data)
	{
	    strncpy(p_SetNetworkInterfaces_req->token, p_InterfaceToken->data, sizeof(p_SetNetworkInterfaces_req->token)-1);
	}
	else
	{
	    return ONVIF_ERR_MISSINGATTR;
	}

	XMLN * p_NetworkInterface = xml_node_soap_get(p_node, "NetworkInterface");
	if (p_NetworkInterface)
	{
	    p_SetNetworkInterfaces_req->enabled = TRUE;
	    
	    XMLN * p_Enabled = xml_node_soap_get(p_NetworkInterface, "Enabled");
	    if (p_Enabled && p_Enabled->data)
	    {
	        p_SetNetworkInterfaces_req->enabled = (strcasecmp(p_Enabled->data, "true") == 0 ? TRUE : FALSE);
	    }

	    XMLN * p_MTU = xml_node_soap_get(p_NetworkInterface, "MTU");
	    if (p_MTU && p_MTU->data)
	    {
	        p_SetNetworkInterfaces_req->mtu = atoi(p_MTU->data);
	    }

	    XMLN * p_IPv4 = xml_node_soap_get(p_NetworkInterface, "IPv4");
	    if (p_IPv4)
	    {
	        p_SetNetworkInterfaces_req->ipv4_enabled = TRUE;
	        
	        XMLN * p_Enabled = xml_node_soap_get(p_IPv4, "Enabled");
	        if (p_Enabled && p_Enabled->data)
    	    {
    	        p_SetNetworkInterfaces_req->ipv4_enabled = (strcasecmp(p_Enabled->data, "false") == 0 ? FALSE : TRUE);
    	    }

    	    XMLN * p_DHCP = xml_node_soap_get(p_IPv4, "DHCP");
	        if (p_DHCP && p_DHCP->data)
	        {
	            p_SetNetworkInterfaces_req->fromdhcp = (strcasecmp(p_DHCP->data, "true") == 0 ? TRUE : FALSE);
	        }

	        if (p_SetNetworkInterfaces_req->fromdhcp == FALSE)
	        {
	            XMLN * p_Manual = xml_node_soap_get(p_IPv4, "Manual");
	            if (p_Manual)
	            {
	                XMLN * p_Address = xml_node_soap_get(p_Manual, "Address");
	                if (p_Address && p_Address->data)
	                {
	                    strncpy(p_SetNetworkInterfaces_req->ipv4_addr, p_Address->data, sizeof(p_SetNetworkInterfaces_req->ipv4_addr)-1);
	                }

	                XMLN * p_PrefixLength = xml_node_soap_get(p_Manual, "PrefixLength");
	                if (p_PrefixLength && p_PrefixLength->data)
	                {
	                    p_SetNetworkInterfaces_req->prefix_len = atoi(p_PrefixLength->data);
	                }
	            }
	        }
	    }
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_GetVideoSourceConfigurationOptions(XMLN * p_node, GetVideoSourceConfigurationOptions_REQ * p_GetVideoSourceConfigurationOptions_req)
{
	assert(p_node);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_GetVideoSourceConfigurationOptions_req->config_token, p_ConfigurationToken->data, sizeof(p_GetVideoSourceConfigurationOptions_req->config_token)-1);
	}
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_GetVideoSourceConfigurationOptions_req->profile_token, p_ProfileToken->data, sizeof(p_GetVideoSourceConfigurationOptions_req->profile_token)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_SetVideoSourceConfiguration(XMLN * p_node, SetVideoSourceConfiguration_REQ * p_SetVideoSourceConfiguration_req)
{
	assert(p_node);

	XMLN * p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		const char * token = xml_attr_get(p_Configuration, "token");
		if (token)
		{
			strncpy(p_SetVideoSourceConfiguration_req->config_token, token, sizeof(p_SetVideoSourceConfiguration_req->config_token)-1);
		}
		else
		{
			return ONVIF_ERR_MISSINGATTR;
		}
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}

	XMLN * p_Name = xml_node_soap_get(p_Configuration, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_SetVideoSourceConfiguration_req->name, p_Name->data, sizeof(p_SetVideoSourceConfiguration_req->name)-1);
	}

	XMLN * p_UseCount = xml_node_soap_get(p_Configuration, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_SetVideoSourceConfiguration_req->use_count = atoi(p_UseCount->data);
	}

	XMLN * p_SourceToken = xml_node_soap_get(p_Configuration, "SourceToken");
	if (p_SourceToken && p_SourceToken->data)
	{
		strncpy(p_SetVideoSourceConfiguration_req->source_token, p_SourceToken->data, sizeof(p_SetVideoSourceConfiguration_req->source_token)-1);
	}

	XMLN * p_Bounds = xml_node_soap_get(p_Configuration, "Bounds");
	if (p_Bounds)
	{
		const char * x = xml_attr_get(p_Bounds, "x");
		if (x)
		{
			p_SetVideoSourceConfiguration_req->x = atoi(x);
		}

		const char * y = xml_attr_get(p_Bounds, "y");
		if (y)
		{
			p_SetVideoSourceConfiguration_req->y = atoi(y);
		}

		const char * width = xml_attr_get(p_Bounds, "width");
		if (width)
		{
			p_SetVideoSourceConfiguration_req->width = atoi(width);
		}

		const char * height = xml_attr_get(p_Bounds, "height");
		if (height)
		{
			p_SetVideoSourceConfiguration_req->height = atoi(height);
		}
	}

	XMLN * p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		if (strcasecmp(p_ForcePersistence->data, "true") == 0)
		{
			p_SetVideoSourceConfiguration_req->persistence = 1;
		}
	}	
	
	return ONVIF_OK;
}

ONVIF_RET parse_GetVideoEncoderConfigurationOptions(XMLN * p_node, GetVideoEncoderConfigurationOptions_REQ * p_GetVideoEncoderConfigurationOptions_req)
{
	assert(p_node);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_GetVideoEncoderConfigurationOptions_req->config_token, p_ConfigurationToken->data, sizeof(p_GetVideoEncoderConfigurationOptions_req->config_token)-1);
	}
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_GetVideoEncoderConfigurationOptions_req->profile_token, p_ProfileToken->data, sizeof(p_GetVideoEncoderConfigurationOptions_req->profile_token)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_GetAudioSourceConfigurationOptions(XMLN * p_node, GetAudioSourceConfigurationOptions_REQ * p_GetAudioSourceConfigurationOptions_req)
{
	assert(p_node);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_GetAudioSourceConfigurationOptions_req->config_token, p_ConfigurationToken->data, sizeof(p_GetAudioSourceConfigurationOptions_req->config_token)-1);
	}
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_GetAudioSourceConfigurationOptions_req->profile_token, p_ProfileToken->data, sizeof(p_GetAudioSourceConfigurationOptions_req->profile_token)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_SetAudioSourceConfiguration(XMLN * p_node, SetAudioSourceConfiguration_REQ * p_SetAudioSourceConfiguration_req)
{
	assert(p_node);

	XMLN * p_Configuration = xml_node_soap_get(p_node, "Configuration");
	if (p_Configuration)
	{
		const char * token = xml_attr_get(p_Configuration, "token");
		if (token)
		{
			strncpy(p_SetAudioSourceConfiguration_req->config_token, token, sizeof(p_SetAudioSourceConfiguration_req->config_token)-1);
		}
		else
		{
			return ONVIF_ERR_MISSINGATTR;
		}
	}
	else
	{
		return ONVIF_ERR_MISSINGATTR;
	}

	XMLN * p_Name = xml_node_soap_get(p_Configuration, "Name");
	if (p_Name && p_Name->data)
	{
		strncpy(p_SetAudioSourceConfiguration_req->name, p_Name->data, sizeof(p_SetAudioSourceConfiguration_req->name)-1);
	}

	XMLN * p_UseCount = xml_node_soap_get(p_Configuration, "UseCount");
	if (p_UseCount && p_UseCount->data)
	{
		p_SetAudioSourceConfiguration_req->use_count = atoi(p_UseCount->data);
	}

	XMLN * p_SourceToken = xml_node_soap_get(p_Configuration, "SourceToken");
	if (p_SourceToken && p_SourceToken->data)
	{
		strncpy(p_SetAudioSourceConfiguration_req->source_token, p_SourceToken->data, sizeof(p_SetAudioSourceConfiguration_req->source_token)-1);
	}

	XMLN * p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		if (strcasecmp(p_ForcePersistence->data, "true") == 0)
		{
			p_SetAudioSourceConfiguration_req->persistence = 1;
		}
	}	
	
	return ONVIF_OK;
}

ONVIF_RET parse_GetAudioEncoderConfigurationOptions(XMLN * p_node, GetAudioEncoderConfigurationOptions_REQ * p_GetAudioEncoderConfigurationOptions_req)
{
	assert(p_node);

	XMLN * p_ConfigurationToken = xml_node_soap_get(p_node, "ConfigurationToken");
	if (p_ConfigurationToken && p_ConfigurationToken->data)
	{
		strncpy(p_GetAudioEncoderConfigurationOptions_req->config_token, p_ConfigurationToken->data, sizeof(p_GetAudioEncoderConfigurationOptions_req->config_token)-1);
	}
	
	XMLN * p_ProfileToken = xml_node_soap_get(p_node, "ProfileToken");
	if (p_ProfileToken && p_ProfileToken->data)
	{
		strncpy(p_GetAudioEncoderConfigurationOptions_req->profile_token, p_ProfileToken->data, sizeof(p_GetAudioEncoderConfigurationOptions_req->profile_token)-1);
	}
	
	return ONVIF_OK;
}

ONVIF_RET parse_SetAudioEncoderConfiguration(XMLN * p_node, SetAudioEncoderConfiguration_REQ * p_SetAudioEncoderConfiguration_req)
{
	XMLN * p_Configuration = xml_node_soap_get(p_node, "Configuration");
    if (!p_Configuration)
    {
        return ONVIF_ERR_MISSINGATTR;
    }
    
    const char * token = xml_attr_get(p_Configuration, "token");
    if (token)
    {
        strncpy(p_SetAudioEncoderConfiguration_req->token, token, sizeof(p_SetAudioEncoderConfiguration_req->token)-1);
    }    

    XMLN * p_Name = xml_node_soap_get(p_Configuration, "Name");
    if (p_Name && p_Name->data)
    {
        strncpy(p_SetAudioEncoderConfiguration_req->name, p_Name->data, sizeof(p_SetAudioEncoderConfiguration_req->name)-1);
    }

    XMLN * p_UseCount = xml_node_soap_get(p_Configuration, "UseCount");
    if (p_UseCount && p_UseCount->data)
    {
        p_SetAudioEncoderConfiguration_req->use_count = atoi(p_UseCount->data);
    }

    XMLN * p_Encoding = xml_node_soap_get(p_Configuration, "Encoding");
    if (p_Encoding && p_Encoding->data)
    {
        if (strcasecmp(p_Encoding->data, "G711") == 0)
        {
        	p_SetAudioEncoderConfiguration_req->encoding = AUDIO_ENCODING_G711;
        }
        else if (strcasecmp(p_Encoding->data, "G726") == 0)
        {
        	p_SetAudioEncoderConfiguration_req->encoding = AUDIO_ENCODING_G726;
        }
        else if (strcasecmp(p_Encoding->data, "AAC") == 0)
        {
        	p_SetAudioEncoderConfiguration_req->encoding = AUDIO_ENCODING_AAC;
        }
    }    

    XMLN * p_Bitrate = xml_node_soap_get(p_Configuration, "Bitrate");
    if (p_Bitrate && p_Bitrate->data)
    {
        p_SetAudioEncoderConfiguration_req->bitrate = atoi(p_Bitrate->data);
    }

    XMLN * p_SampleRate = xml_node_soap_get(p_Configuration, "SampleRate");
    if (p_SampleRate && p_SampleRate->data)
    {
        p_SetAudioEncoderConfiguration_req->sample_rate = atoi(p_SampleRate->data);
    }    
	
	XMLN * p_SessionTimeout = xml_node_soap_get(p_Configuration, "SessionTimeout");
	if (p_SessionTimeout && p_SessionTimeout->data)
	{
		p_SetAudioEncoderConfiguration_req->session_timeout = atoi(p_SessionTimeout->data);
	}

	XMLN * p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
	if (p_ForcePersistence && p_ForcePersistence->data)
	{
		if (strcasecmp(p_ForcePersistence->data, "true") == 0)
		{
			p_SetAudioEncoderConfiguration_req->persistence = TRUE;
		}
	}
    	
    return ONVIF_OK;
}

ONVIF_RET parse_SetImagingSettings(XMLN * p_node, SetImagingSettings_REQ * p_SetImagingSettings_req)
{
	XMLN * p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_SetImagingSettings_req->source_token, p_VideoSourceToken->data, sizeof(p_SetImagingSettings_req->source_token)-1);
    }
    else
    {
    	return ONVIF_ERR_MISSINGATTR;
    }

    XMLN * p_ImagingSettings = xml_node_soap_get(p_node, "ImagingSettings");
    if (NULL == p_ImagingSettings)
    {
    	return ONVIF_ERR_MISSINGATTR;
    }

    XMLN * p_BacklightCompensation = xml_node_soap_get(p_ImagingSettings, "BacklightCompensation");
    if (p_BacklightCompensation)
    {
    	XMLN * p_Mode = xml_node_soap_get(p_BacklightCompensation, "Mode");
    	if (p_Mode && p_Mode->data)
    	{
    		if (strcasecmp(p_Mode->data, "OFF") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.BacklightCompensation_Mode = 0;
    		}
    		else if (strcasecmp(p_Mode->data, "ON") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.BacklightCompensation_Mode = 1;
    		}
    		else
			{
				return ONVIF_ERR_SETTINGS_INVALID;
			}
    	}
    }

    XMLN * p_Brightness = xml_node_soap_get(p_ImagingSettings, "Brightness");
    if (p_Brightness && p_Brightness->data)
    {
    	p_SetImagingSettings_req->img_cfg.Brightness = atoi(p_Brightness->data);
    }

    XMLN * p_ColorSaturation = xml_node_soap_get(p_ImagingSettings, "ColorSaturation");
    if (p_ColorSaturation && p_ColorSaturation->data)
    {
    	p_SetImagingSettings_req->img_cfg.ColorSaturation = atoi(p_ColorSaturation->data);
    }

    XMLN * p_Contrast = xml_node_soap_get(p_ImagingSettings, "Contrast");
    if (p_Contrast && p_Contrast->data)
    {
    	p_SetImagingSettings_req->img_cfg.Contrast= atoi(p_Contrast->data);
    }

    XMLN * p_Exposure = xml_node_soap_get(p_ImagingSettings, "Exposure");
    if (p_Exposure)
    {
    	XMLN * p_Mode = xml_node_soap_get(p_Exposure, "Mode");
    	if (p_Mode && p_Mode->data)
    	{
    		if (strcasecmp(p_Mode->data, "AUTO") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.Exposure_Mode = 0;
    		}
    		else if (strcasecmp(p_Mode->data, "Manual") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.Exposure_Mode = 1;
    		}
    		else
			{
				return ONVIF_ERR_SETTINGS_INVALID;
			}
    	}

    	XMLN * p_MinExposureTime = xml_node_soap_get(p_Exposure, "MinExposureTime");
    	if (p_MinExposureTime && p_MinExposureTime->data)
    	{
    		p_SetImagingSettings_req->img_cfg.MinExposureTime = atoi(p_MinExposureTime->data);
    	}

    	XMLN * p_MaxExposureTime = xml_node_soap_get(p_Exposure, "MaxExposureTime");
    	if (p_MaxExposureTime && p_MaxExposureTime->data)
    	{
    		p_SetImagingSettings_req->img_cfg.MaxExposureTime= atoi(p_MaxExposureTime->data);
    	}

    	XMLN * p_MinGain = xml_node_soap_get(p_Exposure, "MinGain");
    	if (p_MinGain && p_MinGain->data)
    	{
    		p_SetImagingSettings_req->img_cfg.MinGain = atoi(p_MinGain->data);
    	}

    	XMLN * p_MaxGain = xml_node_soap_get(p_Exposure, "MaxGain");
    	if (p_MaxGain && p_MaxGain->data)
    	{
    		p_SetImagingSettings_req->img_cfg.MaxGain = atoi(p_MaxGain->data);
    	}
    }

    XMLN * p_IrCutFilter = xml_node_soap_get(p_ImagingSettings, "IrCutFilter");
    if (p_IrCutFilter && p_IrCutFilter->data)
    {
		if (strcasecmp(p_IrCutFilter->data, "OFF") == 0)
		{
			p_SetImagingSettings_req->img_cfg.IrCutFilter_Mode = 0;
		}
		else if (strcasecmp(p_IrCutFilter->data, "ON") == 0)
		{
			p_SetImagingSettings_req->img_cfg.IrCutFilter_Mode = 1;
		}
		else if (strcasecmp(p_IrCutFilter->data, "AUTO") == 0)
		{
			p_SetImagingSettings_req->img_cfg.IrCutFilter_Mode = 2;
		}
		else
		{
			return ONVIF_ERR_SETTINGS_INVALID;
		}
    }

    XMLN * p_Sharpness = xml_node_soap_get(p_ImagingSettings, "Sharpness");
    if (p_Sharpness && p_Sharpness->data)
    {
    	p_SetImagingSettings_req->img_cfg.Sharpness = atoi(p_Sharpness->data);
    }

    XMLN * p_WideDynamicRange = xml_node_soap_get(p_ImagingSettings, "WideDynamicRange");
    if (p_WideDynamicRange)
    {
    	XMLN * p_Mode = xml_node_soap_get(p_WideDynamicRange, "Mode");
    	if (p_Mode && p_Mode->data)
    	{
    		if (strcasecmp(p_Mode->data, "OFF") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.WideDynamicRange_Mode = 0;
    		}
    		else if (strcasecmp(p_Mode->data, "ON") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.WideDynamicRange_Mode = 1;
    		}
    		else
    		{
    			return ONVIF_ERR_SETTINGS_INVALID;
    		}
    	}

    	XMLN * p_Level = xml_node_soap_get(p_WideDynamicRange, "Level");
    	if (p_Level && p_Level->data)
    	{
    		p_SetImagingSettings_req->img_cfg.WideDynamicRange_Level = atoi(p_Level->data);
    	}
    }

    XMLN * p_WhiteBalance = xml_node_soap_get(p_ImagingSettings, "WhiteBalance");
    if (p_WhiteBalance)
    {
    	XMLN * p_Mode = xml_node_soap_get(p_WhiteBalance, "Mode");
    	if (p_Mode && p_Mode->data)
    	{
    		if (strcasecmp(p_Mode->data, "AUTO") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.WhiteBalance_Mode = 0;
    		}
    		else if (strcasecmp(p_Mode->data, "Manual") == 0)
    		{
    			p_SetImagingSettings_req->img_cfg.WhiteBalance_Mode = 1;
    		}
    		else
    		{
    			return ONVIF_ERR_SETTINGS_INVALID;
    		}
    	}
    }

    XMLN * p_ForcePersistence = xml_node_soap_get(p_node, "ForcePersistence");
    if (p_ForcePersistence && p_ForcePersistence->data)
    {
    	if (strcasecmp(p_ForcePersistence->data, "true") == 0)
		{
			p_SetImagingSettings_req->persistence = TRUE;
		}
    }
    
	return ONVIF_OK;
}

ONVIF_RET parse_Move(XMLN * p_node, Move_REQ * p_Move_req)
{
	XMLN * p_VideoSourceToken = xml_node_soap_get(p_node, "VideoSourceToken");
    if (p_VideoSourceToken && p_VideoSourceToken->data)
    {
        strncpy(p_Move_req->source_token, p_VideoSourceToken->data, sizeof(p_Move_req->source_token)-1);
    }
    else
    {
    	return ONVIF_ERR_MISSINGATTR;
    }

    XMLN * p_Focus = xml_node_soap_get(p_node, "Focus");
    if (NULL == p_Focus)
    {
    	return ONVIF_ERR_MISSINGATTR;
    }

    XMLN * p_Absolute = xml_node_soap_get(p_node, "Absolute");
    if (p_Absolute)
    {
    	XMLN * p_Position = xml_node_soap_get(p_Absolute, "Position");
    	if (p_Position && p_Position->data)
    	{
    		p_Move_req->absolute_pos = atoi(p_Position->data);
    	}

    	XMLN * p_Speed = xml_node_soap_get(p_Absolute, "Speed");
    	if (p_Speed && p_Speed->data)
    	{
    		p_Move_req->absolute_speed = atoi(p_Speed->data);
    	}
    }

    
	XMLN * p_Relative = xml_node_soap_get(p_node, "Relative");
	if (p_Relative)
	{
		XMLN * p_Distance = xml_node_soap_get(p_Relative, "Distance");
		if (p_Distance && p_Distance->data)
		{
			p_Move_req->relative_distance = atoi(p_Distance->data);
		}

		XMLN * p_Speed = xml_node_soap_get(p_Relative, "Speed");
		if (p_Speed && p_Speed->data)
		{
			p_Move_req->relative_speed = atoi(p_Speed->data);
		}
	}

	XMLN * p_Continuous = xml_node_soap_get(p_node, "Continuous");
	if (p_Continuous)
	{
		XMLN * p_Speed = xml_node_soap_get(p_Continuous, "Speed");
		if (p_Speed && p_Speed->data)
		{
			p_Move_req->continuous_speed = atoi(p_Speed->data);
		}
	}

	return ONVIF_OK;
}



