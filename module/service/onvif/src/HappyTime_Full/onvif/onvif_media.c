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
#include "onvif_media.h"
#include "onvif_utils.h"
#include "onvif_pkt.h"
#include "onvif_event.h"
#include "anj_mw_crypt.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_record.h"
#include "anj_config.h"

#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#define ONVIF_SNAPSHOT_QUALITY_DEFAULT 70

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_IDX g_onvif_idx;
extern unsigned int tLastSetTime[3];//1
extern int g_onvif_expand;
extern int g_stereo;
extern int isHBVersion;
extern int is_Y_Version;
/***************************************************************************************/

#ifdef MEDIA_SUPPORT

/************************************************************************************
 *  	
 * Whenever a change in the profiles of a device supporting the media service occurs 
 * the device should provide the following event. The Profile change could be caused 
 * by Creation or Deletion of a Profile or by Adding or Removing a Configuration to 
 * or from a Profile 
 *
*************************************************************************************/
void onvif_ProfileChangedNotify(ONVIF_PROFILE * p_req, onvif_PropertyOperation op)
{
	SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
	
	p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/Profile");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = op;
		p_msg->Message.UtcTime = time(NULL)+1;

		p_msg->Message.SourceFlag = 1;
		
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
			int buflen = 1024*10;
			
			strcpy(p_elementitem->ElementItem.Name, "Configuration");
			
			p_elementitem->ElementItem.Any = (char *)malloc(buflen);
			if (p_elementitem->ElementItem.Any)
			{
				int offset = 0;
				char * buff = p_elementitem->ElementItem.Any;
				
				memset(buff, 0, buflen);
				
				p_elementitem->ElementItem.AnyFlag = 1;

				offset += snprintf(buff+offset, buflen-offset, "<tt:Profile token=\"%s\" fixed=\"%s\">", p_req->token, p_req->fixed ? "true" : "false");
				offset += build_Profile_xml(buff+offset, buflen-offset, p_req);
				offset += snprintf(buff+offset, buflen-offset, "</tt:Profile>");
			}
		}

		onvif_put_NotificationMessage(p_message);
	}
}

/************************************************************************************
 *  	
 * Whenever a VideoEncoderConfiguration of a device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_VideoEncoderConfigurationChangedNotify(onvif_VideoEncoder2Configuration * p_req)
{
	SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;

	p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		p_msg = &p_message->NotificationMessage;

		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/VideoEncoderConfiguration");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}
		p_msg->Message.DataFlag = 1;
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		
		if (p_elementitem)
		{
			int buflen = 1024*4;
			strcpy(p_elementitem->ElementItem.Name, "Configuration");
			p_elementitem->ElementItem.Any = (char *)malloc(buflen);
			
			if (p_elementitem->ElementItem.Any)
			{
				int offset = 0;
				char * buff = p_elementitem->ElementItem.Any;
				memset(buff, 0, buflen);
				p_elementitem->ElementItem.AnyFlag = 1;

				offset += snprintf(buff+offset, buflen-offset, "<tt:VideoEncoderConfiguration token=\"%s\">", p_req->token);
				offset += build_VideoEncoderConfiguration_xml(buff+offset, buflen-offset, p_req);
				offset += snprintf(buff+offset, buflen-offset, "</tt:VideoEncoderConfiguration>");
			}
		}
		onvif_put_NotificationMessage(p_message);
	}
	return;
}

/************************************************************************************
 *  	
 * Whenever a VideoSourceConfiguration of a device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_VideoSourceConfigurationChangedNotify(onvif_VideoSourceConfiguration * p_req)
{
	SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;

	p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		p_msg = &p_message->NotificationMessage;

		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/VideoSourceConfiguration/MediaService");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL) + 1;

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
			int buflen = 1024*4;
			strcpy(p_elementitem->ElementItem.Name, "Configuration");
			p_elementitem->ElementItem.Any = (char *)malloc(buflen);
			if (p_elementitem->ElementItem.Any)
			{
				int offset = 0;
				char * buff = p_elementitem->ElementItem.Any;
				memset(buff, 0, buflen);
				p_elementitem->ElementItem.AnyFlag = 1;

				offset += snprintf(buff+offset, buflen-offset, "<tt:VideoSourceConfiguration token=\"%s\">", p_req->token);
				offset += build_VideoSourceConfiguration_xml(buff+offset, buflen-offset, p_req);
				offset += snprintf(buff+offset, buflen-offset, "</tt:VideoSourceConfiguration>");
			}
		}
		onvif_put_NotificationMessage(p_message);
	}
	return;
}

/************************************************************************************
 *  	
 * Whenever a MetadataConfiguration of a device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_MetadataConfigurationChangedNotify(onvif_MetadataConfiguration * p_req)
{
    SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
    
    p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/MetadataConfiguration");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024*4;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;

		        offset += snprintf(buff+offset, buflen-offset, 
		            "<tt:MetadataConfiguration token=\"%s\" "
                        "CompressionType=\"%s\" GeoLocation=\"%s\" ShapePolygon=\"%s\">\r\n", 
                    p_req->token,
                    p_req->CompressionType,
                    p_req->GeoLocation ? "true" : "false",
                    p_req->ShapePolygon ? "true" : "false");
	            offset += build_MetadataConfiguration_xml(buff+offset, buflen-offset, p_req);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:MetadataConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

#ifdef DEVICEIO_SUPPORT

/************************************************************************************
 *  	
 * Whenever a VideoOutputConfiguration of a device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_VideoOutputConfigurationChangedNotify(onvif_VideoOutputConfiguration * p_req)
{
    SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
    
    p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/VideoOutputConfiguration/MediaService");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024*4;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;

		        offset += snprintf(buff+offset, buflen-offset, 
                	"<tt:VideoOutputConfiguration token=\"%s\">", 
                	p_req->token);
	            offset += build_VideoOutputConfiguration_xml(buff+offset, buflen-offset, p_req);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:VideoOutputConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

/************************************************************************************
 *  	
 * Whenever a AudioOutputConfiguration of a device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_AudioOutputConfigurationChangedNotify(onvif_AudioOutputConfiguration * p_req)
{
    SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
    
    p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/AudioOutputConfiguration/MediaService");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024*4;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;

		        offset += snprintf(buff+offset, buflen-offset, 
                    "<tt:AudioOutputConfiguration token=\"%s\">",
                    p_req->token);
	            offset += build_AudioOutputConfiguration_xml(buff+offset, buflen-offset, p_req);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:AudioOutputConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

#endif // DEVICEIO_SUPPORT

#ifdef AUDIO_SUPPORT

/************************************************************************************
 *  	
 * Whenever an AudioEncoderConfiguration of a device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_AudioEncoderConfigurationChangedNotify(onvif_AudioEncoder2Configuration * p_req)
{
    SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
    
    p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/AudioEncoderConfiguration");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024*4;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;

		        offset += snprintf(buff+offset, buflen-offset, 
                	"<tt:AudioEncoderConfiguration token=\"%s\">", 
                	p_req->token);
	            offset += build_AudioEncoderConfiguration_xml(buff+offset, buflen-offset, p_req);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:AudioEncoderConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

/************************************************************************************
 *  	
 * Whenever a AudioSourceConfiguration of a device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_AudioSourceConfigurationChangedNotify(onvif_AudioSourceConfiguration * p_req)
{
    SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
    
    p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/AudioSourceConfiguration/MediaService");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024*4;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;

		        offset += snprintf(buff+offset, buflen-offset, 
                    "<tt:AudioSourceConfiguration token=\"%s\">",
                    p_req->token);
	            offset += build_AudioSourceConfiguration_xml(buff+offset, buflen-offset, p_req);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:AudioSourceConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

#endif // AUDIO_SUPPORT

#ifdef PTZ_SUPPORT

/************************************************************************************
 *  	
 * Whenever a PTZConfiguration of a PTZ capable device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_PTZConfigurationChangedNotify(onvif_PTZConfiguration * p_req)
{
    SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
    
    p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/PTZConfiguration");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024*4;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;

		        offset += snprintf(buff+offset, buflen-offset, 
                    "<tt:PTZConfiguration token=\"%s\" "
                        "MoveRamp=\"%d\" PresetRamp=\"%d\" PresetTourRamp=\"%d\">", 
                    p_req->token, 
                    p_req->MoveRamp, 
                    p_req->PresetRamp, 
                    p_req->PresetTourRamp);
	            offset += build_PTZConfiguration_xml(buff+offset, buflen-offset, p_req);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:PTZConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

#endif // PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

/************************************************************************************
 *  	
 * Whenever a VideoAnalyticsConfiguration of device changes the device should 
 * provide the following event
 *
*************************************************************************************/
void onvif_VideoAnalyticsConfigurationChangedNotify(onvif_VideoAnalyticsConfiguration * p_req)
{
    SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	NotificationMessageList * p_message;
	onvif_NotificationMessage * p_msg;
    
    p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
	    p_msg = &p_message->NotificationMessage;
	    
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Configuration/VideoAnalyticsConfiguration");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Token");
			strcpy(p_simpleitem->SimpleItem.Value, p_req->token);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024*4;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;

		        offset += snprintf(buff+offset, buflen-offset, 
                    "<tt:VideoAnalyticsConfiguration token=\"%s\">",
                    p_req->token);
	            offset += build_VideoAnalyticsConfiguration_xml(buff+offset, buflen-offset, p_req);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:VideoAnalyticsConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

#endif // VIDEO_ANALYTICS

#endif // MEDIA_SUPPORT

/************************************************************************************
 *  	
 * @brief
 *  Get snapshot JPEG image data.
 *
 * @param buff data buffer
 * @param rlen [in, out], [in] the buff size, [out] the image data size
 * @param profile_token profile token
 * 
 * @return
 *  ONVIF_OK
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_ServiceNotSupported
 *
*************************************************************************************/
ONVIF_RET onvif_trt_GetSnapshot(char **buff, int * rlen, char * profile_token)
{
	log_print(HT_LOG_INFO, "onvif_GetSnapshot, stream:%s\r\n", profile_token);
	FILE * fp;
	int len;
	int stream = 1;
	char filename[128] = {0};
	char localPath[128] = {0};
	char filePath[320];
	
	strcpy(localPath,"/tmp");
	
	if(!strcmp(profile_token, "0"))
		stream = 0;
	else if(!strcmp(profile_token, "1"))
		stream = 1;
	else
		return ONVIF_ERR_ServiceNotSupported;
	
	struct timeval tv;
	gettimeofday(&tv, NULL);
	sprintf(filename, "snapshot_stream%d_%x.jpg", stream, (unsigned int)tv.tv_sec);
	
	anj_snap_jpg(0, stream, ONVIF_SNAPSHOT_QUALITY_DEFAULT, localPath, filename, NULL);

	sprintf(filePath, "%s/%s", localPath, filename);

	/* anj_snap_jpg 异步落盘：只 access 会读到半截（无 EOI），必须等 JPEG 写完 */
	if (anj_snap_wait_complete(filePath, 3000) != 0)
	{
		log_print(HT_LOG_ERR, "onvif GetSnapshot wait complete failed: %s\n", filePath);
		return ONVIF_ERR_ServiceNotSupported;
	}

	fp = fopen(filePath, "rb");
	if (NULL == fp)
	{
		return ONVIF_ERR_ServiceNotSupported;
	}

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if (len <= 0)
	{
		fclose(fp);
		remove(filePath);
		return ONVIF_ERR_ServiceNotSupported;
	}

	*buff = (char *)malloc(len);
	if (NULL == *buff)
	{
		fclose(fp);
		remove(filePath);
		return ONVIF_ERR_ServiceNotSupported;
	}

	fseek(fp, 0, SEEK_SET);
	*rlen = (int)fread(*buff, 1, len, fp);
	fclose(fp);
	remove(filePath);

	if (*rlen != len)
	{
		free(*buff);
		*buff = NULL;
		*rlen = 0;
		return ONVIF_ERR_ServiceNotSupported;
	}

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brif
 *  Modifies an OSD configuration. 
 *  Running streams using this configuration may be immediately updated 
 *  according to the new settings.
 *
 *  A device shall accept any combination of parameters returned by 
 *  GetOSDOptions. If necessary the device may adapt parameter values 
 *  for FontColor, FontSize, and BackgroundColor elements without 
 *  returning an error.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigModify
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetOSD(trt_SetOSD_REQ * p_req)
{
	OSDConfigurationList * p_osd = onvif_find_OSDConfiguration(g_onvif_cfg.OSDs, p_req->OSD.token);
	if (NULL == p_osd)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	char *content_format = NULL;
	// todo : here add handler code ...
	log_print(HT_LOG_INFO, "p_req->OSD.token:%s\n", p_req->OSD.token);

	if (&p_req->OSD.TextString)
	{
		if (!strcmp(p_req->OSD.token, "OSDConfigurationToken_1"))//title
		{
			content_format = p_req->OSD.TextString.PlainText;
			if(content_format!=NULL)
			{
				char sztmp[128] = {0};
				int iIndex = 0;
				for( iIndex = 0; iIndex < strlen(content_format) && iIndex < 42; iIndex++)
				{
					sprintf(sztmp + strlen(sztmp), "%02x ", content_format[iIndex]);
				}
			}
		}
		else if (!strcmp(p_req->OSD.token, "OSDConfigurationToken_2"))//time
		{
			if( p_req->OSD.TextString.DateFormat == NULL || strlen(p_req->OSD.TextString.DateFormat ) == 0)
				content_format = "yyyy-MM-dd";
			else if(strcmp("MM/dd/yyyy", p_req->OSD.TextString.DateFormat)
					&& strcmp("dd/MM/yyyy", p_req->OSD.TextString.DateFormat)
					&& strcmp("yyyy/MM/dd", p_req->OSD.TextString.DateFormat)
					&& strcmp("yyyy-MM-dd", p_req->OSD.TextString.DateFormat)
					&& strcmp("yy/MM/dd", p_req->OSD.TextString.DateFormat)
					&& strcmp("yy/MM/dd", p_req->OSD.TextString.DateFormat)
					&& strcmp("dd-MM-yyyy", p_req->OSD.TextString.DateFormat)
					&& strcmp("MM-dd-yyyy", p_req->OSD.TextString.DateFormat)
					)
			{
				log_print(HT_LOG_INFO, "InvalidDateFormat, default yyyy-MM-dd\n");
				content_format = "yyyy-MM-dd";
			}
			else
			{
				content_format = p_req->OSD.TextString.DateFormat;
			}
		}
	}
	
	float pos_x = 0;
	float pos_y = 0;
	char *pos_type = NULL;
	if(&p_req->OSD.Position != NULL)
	{
		if(&p_req->OSD.Position.Pos != NULL)
		{
			pos_x = p_req->OSD.Position.Pos.x;
			pos_y = p_req->OSD.Position.Pos.y;
		}
		if(&p_req->OSD.Position.Type != NULL)
			pos_type = (char *)onvif_OSDPosTypeToString(p_req->OSD.Position.Type);
	}
	set_osd(p_req->OSD.token, content_format, pos_type, pos_x, pos_y);
	
	memcpy(&p_osd->OSD, &p_req->OSD, sizeof(onvif_OSDConfiguration));
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Creates a new OSD configuration with specified values and also make 
 *  the association between the new OSD and an existing VideoSourceConfiguration
 *  identified by the VideoSourceConfigurationToken. Any value required by 
 *  a device for a new OSD configuration that is optional and not present in
 *  the CreateOSD message may be adapted to the appropriate value by the device.
 *  The OSD shall be created in the device and shall be persistent 
 *  (remain after reboot). A device that indicates OSD capability shall support
 *  the creation of OSD as long as the number of existing OSDs does not exceed
 *  the value of MaximumNumberOfOSDs in GetOSDOptions.
 *
 *  When creating a OSDTextConfiguration, if the IsPersistentText attribute
 *  is missing, device shall assume IsPersistentText attribute as true.
 *
 *  A created OSD shall be deletable.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_MaxOSDs
 *
*************************************************************************************/
ONVIF_RET onvif_trt_CreateOSD(trt_CreateOSD_REQ * p_req)
{
	char *content_format = NULL;
	char *token = p_req->OSD.token;
	if (p_req->OSD.TextStringFlag)
	{
		if(p_req->OSD.TextString.Type == OSDTextType_Plain)
		{
			strcpy(p_req->OSD.token, "OSDConfigurationToken_1");
			content_format = p_req->OSD.TextString.PlainText;
		}
		else if(p_req->OSD.TextString.Type == OSDTextType_DateAndTime)
		{
			strcpy(p_req->OSD.token, "OSDConfigurationToken_2");
			content_format = p_req->OSD.TextString.DateFormat;
		}
		else
		{
			return ONVIF_ERR_InvalidArgVal;
		}
	}
	
	float pos_x = 0;
	float pos_y = 0;
	char * pos_type = NULL;
	if(&p_req->OSD.Position != NULL)
	{
		if(&p_req->OSD.Position.Pos != NULL)
		{
			pos_x = p_req->OSD.Position.Pos.x;
			pos_y = p_req->OSD.Position.Pos.y;
		}
		if(&p_req->OSD.Position.Type != NULL)
			pos_type = (char *)onvif_OSDPosTypeToString(p_req->OSD.Position.Type);
	}
	set_osd(token, content_format, pos_type, pos_x, pos_y);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Deletes an OSD. This change shall always be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 *
*************************************************************************************/
ONVIF_RET onvif_trt_DeleteOSD(trt_DeleteOSD_REQ * p_req)
{
	// todo
	// if(!strcmp(p_req->OSDToken, "OSDConfigurationToken_1"))
	// 	MsgSetModuleConfig(0, CMD_SET_MEDIA_VIDEO_OSD, 
	// 	"<Overlay Enable=\"1\" Transparency=\"0\"><TitleOverlay PosType=\"0\" PosX=\"2\" PosY=\"2\" Title=\"\" /></Overlay>");
	
	// else if(!strcmp(p_req->OSDToken, "OSDConfigurationToken_2"))
	// 	MsgSetModuleConfig(0, CMD_SET_MEDIA_VIDEO_OSD, 
	// 	"<Overlay Enable=\"1\" Transparency=\"0\"><TimeOverlay PosType=\"0\" PosX=\"2\" PosY=\"2\" Format=\"\" /></Overlay>");
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Deletes a profile. This change shall always be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_DeletionOfFixedProfile
 *
*************************************************************************************/
ONVIF_RET onvif_trt_DeleteProfile(trt_DeleteProfile_REQ * p_req)
{
	return ONVIF_ERR_ServiceNotSupported;
	ONVIF_PROFILE * p_prev;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->fixed)
	{
		return ONVIF_ERR_DeletionOfFixedProfile;
	}

	p_prev = g_onvif_cfg.profiles;
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

		p_prev->next = p_profile->next;
	}

#ifdef MEDIA_SUPPORT
	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Deleted);
#endif

	if (p_profile->v_src_cfg && p_profile->v_src_cfg->Configuration.UseCount > 0)
	{
		p_profile->v_src_cfg->Configuration.UseCount--;
	}
	
	if (p_profile->v_enc_cfg && p_profile->v_enc_cfg->Configuration.UseCount > 0)
	{
		p_profile->v_enc_cfg->Configuration.UseCount--;
	}

#ifdef AUDIO_SUPPORT
	if (p_profile->a_src_cfg && p_profile->a_src_cfg->Configuration.UseCount > 0)
	{
		p_profile->a_src_cfg->Configuration.UseCount--;
	}

	if (p_profile->a_enc_cfg && p_profile->a_enc_cfg->Configuration.UseCount > 0)
	{
		p_profile->a_enc_cfg->Configuration.UseCount--;
	}
#endif

#ifdef PTZ_SUPPORT
	if (p_profile->ptz_cfg && p_profile->ptz_cfg->Configuration.UseCount > 0)
	{
		p_profile->ptz_cfg->Configuration.UseCount--;
	}
#endif

	if (p_profile->multicasting)
	{
		p_profile->multicasting = FALSE;
		if (p_profile->v_enc_cfg)
		{
			p_profile->v_enc_cfg->Configuration.Multicast.AutoStart = FALSE;
		}
	}

	free(p_profile);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a video source configuration. 
 *  The ForcePersistence flag indicates if the changes shall remain after 
 *  reboot of the device. Running streams using this configuration may be 
 *  immediately updated according to the new settings. The changes are not 
 *  guaranteed to take effect unless the client requests a new stream URI 
 *  and restarts any affected stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetVideoSourceConfiguration(trt_SetVideoSourceConfiguration_REQ * p_req)
{
	VideoSourceList * p_v_src;
	VideoSourceConfigurationList * p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->Configuration.token);
	if (NULL == p_v_src_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	p_v_src = onvif_find_VideoSource(g_onvif_cfg.v_src, p_req->Configuration.SourceToken);
	if (NULL == p_v_src)
	{
		return ONVIF_ERR_NoConfig;
	}
	if(!((p_req->Configuration.Bounds.width >= 320 && p_req->Configuration.Bounds.width <= 1920) 
		&& (p_req->Configuration.Bounds.height >= 192 && p_req->Configuration.Bounds.height <= 1080)) 
		&& (g_onvif_expand == 0))
	{
		return ONVIF_ERR_InvalidArgVal;
	}
	if (/*p_req->Configuration.Bounds.x < g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.XRange.Min || */
		p_req->Configuration.Bounds.x > p_v_src_cfg->Options.BoundsRange.XRange.Max ||
		/*p_req->Configuration.Bounds.y < g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.YRange.Min || */
		p_req->Configuration.Bounds.y > p_v_src_cfg->Options.BoundsRange.YRange.Max ||
		/*p_req->Configuration.Bounds.width < g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.WidthRange.Min || */
		p_req->Configuration.Bounds.width > p_v_src_cfg->Options.BoundsRange.WidthRange.Max ||
		/*p_req->Configuration.Bounds.height < g_onvif_cfg.VideoSourceConfigurationOptions.BoundsRange.HeightRange.Min || */
		p_req->Configuration.Bounds.height > p_v_src_cfg->Options.BoundsRange.HeightRange.Max)
	{
		return ONVIF_ERR_ConfigModify;
	}

	//p_v_src_cfg->Configuration.Bounds.x = p_req->Configuration.Bounds.x;
	//p_v_src_cfg->Configuration.Bounds.y = p_req->Configuration.Bounds.y;
	//p_v_src_cfg->Configuration.Bounds.width = p_req->Configuration.Bounds.width;
	//p_v_src_cfg->Configuration.Bounds.height = p_req->Configuration.Bounds.height;

	//strcpy(p_v_src_cfg->Configuration.Name, p_req->Configuration.Name);
	//strcpy(p_v_src_cfg->Configuration.SourceToken, p_req->Configuration.SourceToken);

	if (p_req->Configuration.ExtensionFlag)
	{
		if (g_onvif_expand)
		{
			if(p_req->Configuration.Extension.RotateFlag)
			{
				int rotate_mode = -1;
				int rotate_value = -1;
				VideoCaptureCfg *pCfg = (VideoCaptureCfg *)malloc(sizeof(VideoCaptureCfg));//free n
				memset(pCfg, 0, sizeof(VideoCaptureCfg));
				memcpy(pCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoCapture, sizeof(*pCfg));
				rotate_mode = (p_req->Configuration.Extension.Rotate.Mode);
				if (rotate_mode == 1){
					rotate_value = (p_req->Configuration.Extension.Rotate.Degree);
					if (rotate_value == 90){
						pCfg->hflip = 1;
						pCfg->vflip = 0;
					}
					else if (rotate_value == 180){
						pCfg->hflip = 0;
						pCfg->vflip = 1;
					}
					else if (rotate_value == 270){
						pCfg->hflip = 1;
						pCfg->vflip = 1;
					}
				}
				else if(rotate_mode == 0){
					pCfg->hflip = 0;
					pCfg->vflip = 0;
				}
				anj_config_video_capture_set(pCfg, 0);
				if ( NULL != pCfg ){
					free(pCfg);
					pCfg = NULL;
				}
			}
		}
		memcpy(&p_v_src_cfg->Configuration.Extension, &p_req->Configuration.Extension, sizeof(onvif_VideoSourceConfigurationExtension));
	}

#ifdef MEDIA_SUPPORT
	onvif_VideoSourceConfigurationChangedNotify(&p_v_src_cfg->Configuration);
#endif

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a metadata configuration. 
 *  The ForcePersistence flag indicates if the changes shall remain after 
 *  reboot of the device. Changes in the Multicast settings shall always be 
 *  persistent. Running streams using this configuration may be updated
 *  immediately according to the new settings. The changes are not guaranteed 
 *  to take effect unless the client requests a new stream URI and restarts
 *  any affected streams.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigModify
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetMetadataConfiguration(trt_SetMetadataConfiguration_REQ * p_req)
{
	MetadataConfigurationList * p_cfg = onvif_find_MetadataConfiguration(g_onvif_cfg.metadata_cfg, p_req->Configuration.token);
	if (NULL == p_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_req->Configuration.SessionTimeout <= 0)
	{
		return ONVIF_ERR_ConfigModify;
	}

    strcpy(p_cfg->Configuration.Name, p_req->Configuration.Name);
    p_cfg->Configuration.SessionTimeout = p_req->Configuration.SessionTimeout;

    p_cfg->Configuration.AnalyticsFlag = p_req->Configuration.AnalyticsFlag;
    p_cfg->Configuration.Analytics = p_req->Configuration.Analytics;

    p_cfg->Configuration.PTZStatusFlag = p_req->Configuration.PTZStatusFlag;
    memcpy(&p_cfg->Configuration.PTZStatus, &p_req->Configuration.PTZStatus, sizeof(onvif_PTZFilter));

    p_cfg->Configuration.EventsFlag = p_req->Configuration.EventsFlag;
    memcpy(&p_cfg->Configuration.Events, &p_req->Configuration.Events, sizeof(onvif_EventSubscription));

    memcpy(&p_cfg->Configuration.Multicast, &p_req->Configuration.Multicast, sizeof(onvif_MulticastConfiguration));

#ifdef MEDIA_SUPPORT
	onvif_MetadataConfigurationChangedNotify(&p_cfg->Configuration);
#endif

	return ONVIF_OK;
}

#ifdef AUDIO_SUPPORT

/************************************************************************************
 *
 * @brief
 *  Modifies an audio source configuration. 
 *  The ForcePersistence flag indicates if the changes shall remain after
 *  reboot of the device. Running streams using this configuration may be 
 *  immediately updated according to the new settings, but the changes are 
 *  not guaranteed to take effect unless the client requests a new stream 
 *  URI and restarts any affected stream. If the new settings invalidate 
 *  any parameters already negotiated using RTSP, for example by changing 
 *  codec type, the device must not apply these settings to existing streams.
 *  Instead it must either continue to stream using the old settings or stop 
 *  sending data on the affected streams.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigModify
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetAudioSourceConfiguration(trt_SetAudioSourceConfiguration_REQ * p_req)
{
	AudioSourceList * p_a_src;
	AudioSourceConfigurationList * p_a_src_cfg = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_req->Configuration.token);
	if (NULL == p_a_src_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	
	p_a_src = onvif_find_AudioSource(g_onvif_cfg.a_src, p_req->Configuration.SourceToken);
	if (NULL == p_a_src)
	{
		return ONVIF_ERR_NoConfig;
	}

	strcpy(p_a_src_cfg->Configuration.Name, p_req->Configuration.Name);

#ifdef MEDIA_SUPPORT
    onvif_AudioSourceConfigurationChangedNotify(&p_a_src_cfg->Configuration);
#endif

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies an audio decoder configuration. 
 *  The ForcePersistence flag indicates if the changes shall remain after 
 *  reboot of the device.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigModify
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetAudioDecoderConfiguration(trt_SetAudioDecoderConfiguration_REQ * p_req)
{
    AudioDecoderConfigurationList * p_a_dec_cfg = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_req->Configuration.token);
	if (NULL == p_a_dec_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	// todo : add set audio decoder code ...

    strcpy(p_a_dec_cfg->Configuration.Name, p_req->Configuration.Name);
    
	return ONVIF_OK;
}

#endif // end of AUDIO_SUPPORT

#endif // end of defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)

#ifdef MEDIA_SUPPORT

/************************************************************************************
 *
 * @brief
 *  Creates a new empty media profile.
 *  The media profile shall be created in the device and shall be persistent
 *  (remain after reboot). A device shall support the creation of media profiles
 *  as long as the number of existing profiles does not exceed the capability 
 *  value MaximumNumberOfProfiles.
 *
 *  A created profile shall be deletable and a device shall set the "fixed" 
 *  attribute to false in the returned Profile.
 *
 *  Optionally the token identifier can be defined by the client. In this case 
 *  a device shall support at least a token length of 12 characters and characters
 *  "A-Z" | "a-z" | "0-9" | "-.".
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_ProfileExists
 *	ONVIF_ERR_MaxNVTProfiles
 *
*************************************************************************************/
ONVIF_RET onvif_trt_CreateProfile(trt_CreateProfile_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = NULL;

	if (p_req->TokenFlag && p_req->Token[0] != '\0')
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->Token);
		if (p_profile)
		{
			return ONVIF_ERR_ProfileExists;
		}
	}
	
	p_profile = onvif_add_profile(&g_onvif_cfg.profiles, TRUE);
	if (p_profile)
	{
		strcpy(p_profile->name, p_req->Name);
		
		if (p_req->TokenFlag && p_req->Token[0] != '\0')
		{
			strcpy(p_profile->token, p_req->Token);
		}
		else
		{
			strcpy(p_req->Token, p_profile->token);
		}
	}
	else 
	{
		return ONVIF_ERR_MaxNVTProfiles;
	}

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Initialized);
	
	return ONVIF_OK;
}

/************************************************************************************
 * 
 * @brief
 *  Adds a VideoSourceConfiguration to an existing media profile. 
 *  If such a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddVideoSourceConfiguration(trt_AddVideoSourceConfiguration_REQ * p_req)
{
	VideoSourceConfigurationList * p_v_src_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_v_src_cfg = onvif_find_VideoSourceConfiguration(g_onvif_cfg.v_src_cfg, p_req->ConfigurationToken);
	if (NULL == p_v_src_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_profile->v_src_cfg != p_v_src_cfg)
	{
		if (p_profile->v_src_cfg && p_profile->v_src_cfg->Configuration.UseCount > 0)
		{
			p_profile->v_src_cfg->Configuration.UseCount --;
		}
		
		p_v_src_cfg->Configuration.UseCount ++;
		
		p_profile->v_src_cfg = p_v_src_cfg;
	}

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Adds a VideoEncoderConfiguration to an existing media profile. 
 *  If a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent.
 *
 *  A device shall support adding a compatible VideoEncoderconfiguration 
 *  to a Profile containing a VideoSourceConfiguration and shall support 
 *  streaming video data of such a Profile.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddVideoEncoderConfiguration(trt_AddVideoEncoderConfiguration_REQ * p_req)
{
	VideoEncoder2ConfigurationList * p_v_enc_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_req->ConfigurationToken);
	if (NULL == p_v_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_profile->v_enc_cfg != p_v_enc_cfg)
	{
		if (p_profile->v_enc_cfg && p_profile->v_enc_cfg->Configuration.UseCount > 0)
		{
			p_profile->v_enc_cfg->Configuration.UseCount --;
		}
		
		p_v_enc_cfg->Configuration.UseCount ++;
		
		p_profile->v_enc_cfg = p_v_enc_cfg;
	}

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes a VideoEncoderConfiguration from an existing media profile. 
 *  If the media profile does not contain a VideoEncoderConfiguration, 
 *  the operation has no effect.The removal shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveVideoEncoderConfiguration(trt_RemoveVideoEncoderConfiguration_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->v_enc_cfg && p_profile->v_enc_cfg->Configuration.UseCount > 0)
	{
		p_profile->v_enc_cfg->Configuration.UseCount--;
	}
	
	p_profile->v_enc_cfg = NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes a VideoSourceConfiguration from an existing media profile. 
 *  If the media profile does not contain a VideoSourceConfiguration, 
 *  the operation has no effect.The removal shall be persistent.
 *
 *  Video source configurations should only be removed after removing a 
 *  VideoEncoderConfiguration from the media profile.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_NoConfig
 *	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveVideoSourceConfiguration(trt_RemoveVideoSourceConfiguration_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->v_enc_cfg)
	{
		return ONVIF_ERR_ConfigurationConflict;
	}

	if (p_profile->v_src_cfg && p_profile->v_src_cfg->Configuration.UseCount > 0)
	{
		p_profile->v_src_cfg->Configuration.UseCount --;
	}
	
	p_profile->v_src_cfg = NULL;

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies a video encoder configuration. 
 *  The ForcePersistence flag indicates if the changes shall remain after 
 *  reboot of the device. Changes in the Multicast settings shall always 
 *  be persistent. Running streams using this configuration may be immediately
 *  updated according to the new settings, but the changes are not guaranteed 
 *  to take effect unless the client requests a new stream URI and restarts 
 *  any affected stream. If the new settings invalidate any parameters already 
 *  negotiated using RTSP, for example by changing codec type, the device must
 *  not apply these settings to existing streams. Instead it must either 
 *  continue to stream using the old settings or stop sending data on the 
 *  affected streams.
 *
 *  A device shall accept any combination of parameters that it returned in the 
 *  GetVideoEncoderConfigurationOptionsResponse. If necessary the device may 
 *  adapt parameter values for Quality and RateControl elements without returning
 *  an error. A device shall adapt an out of range BitrateLimit instead of 
 *  returning a fault
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_ConfigModify
 *	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetVideoEncoderConfiguration(trt_SetVideoEncoderConfiguration_REQ * p_req)
{
	VideoEncoder2ConfigurationList * p_v_enc_cfg;
	
	p_v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_req->Configuration.token);
	if (NULL == p_v_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}
	if (p_req->Configuration.Quality < p_v_enc_cfg->Options.QualityRange.Min || p_req->Configuration.Quality > p_v_enc_cfg->Options.QualityRange.Max )
	{
		return ONVIF_ERR_ConfigModify;
	}
	
	VideoEncode pCfg;
	memset(&pCfg, 0, sizeof(VideoEncode));
	memcpy(&pCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoEncode, sizeof(pCfg));

	VideoCaptureCfg videoCfg;
	memset(&videoCfg, 0, sizeof(VideoCaptureCfg));
	MediaConfig *pTmpVideoCaptureCfg = (MediaConfig *)getMediaConfig();
	if (pTmpVideoCaptureCfg != NULL)
	{
		memcpy(&videoCfg, &pTmpVideoCaptureCfg->videoConfig[0].videoCapture,
		       sizeof(VideoCaptureCfg));
	}

	int stream=0;
	if (g_stereo)
	{
		if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_1_1") ||  0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_2_1")) //MainStream
			stream = 0;
		else if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_1_2") ||  0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_2_2"))//SubStream
			stream = 1;
		else if( 0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_1_3") ||  0 == strcmp(p_req->Configuration.token, "VideoEncoderConfigurationToken_2_3"))//ThirdStream
			stream = 2;
		else
			return ONVIF_ERR_ConfigModify;
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
			return ONVIF_ERR_ConfigModify;
	}
	

	unsigned int  tNowTime = GetCurrentTimeStamp();
	if( tNowTime - tLastSetTime[stream] < 5 * 1000 )
	{
		log_print(HT_LOG_ERR, "Set interval too short, please more than 5s\n");
		return ONVIF_ERR_ConfigModify;
	}
	RESOLUTION_ENTRY *pEntry = NULL;
	if (p_req->Configuration.Encoding == VideoEncoding_H264)
	{
		pEntry = GetResolution("H264", p_req->Configuration.Resolution.Width, p_req->Configuration.Resolution.Height, videoCfg.tvsystem, stream);
	}
	else if (p_req->Configuration.Encoding == VideoEncoding_JPEG)
	{
		pEntry = GetResolution("MJPEG", p_req->Configuration.Resolution.Width, p_req->Configuration.Resolution.Height, videoCfg.tvsystem, stream);
	}
	
	if( pEntry == NULL )
	{
		log_print(HT_LOG_INFO, "Cannot get resolution for %d X %d", p_req->Configuration.Resolution.Width, p_req->Configuration.Resolution.Height);
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
	
	if (VideoEncoding_H264 == p_req->Configuration.Encoding)
	{
		if (p_req->Configuration.H264.GovLength < p_v_enc_cfg->Options.H264.GovLengthRange.Min || p_req->Configuration.H264.GovLength > p_v_enc_cfg->Options.H264.GovLengthRange.Max)
			return ONVIF_ERR_ConfigModify;

		if (p_req->Configuration.H264.GovLength > 0)
		{
			pCfg.encodeCfg[stream].initQuant = p_req->Configuration.H264.GovLength;
			if(isHBVersion > 0)
			{
				if((p_req->Configuration.RateControl.FrameRateLimit > 0) && (p_req->Configuration.H264.GovLength <= p_req->Configuration.RateControl.FrameRateLimit))
					pCfg.encodeCfg[stream].initQuant = -1;
			}
		}
		else if (p_req->Configuration.RateControl.FrameRateLimit > 0)
			pCfg.encodeCfg[stream].initQuant = p_req->Configuration.RateControl.FrameRateLimit * 4;

		strcpy(pCfg.encodeCfg[stream].encodeFormat.name, "H264");
	}
	else if (VideoEncoding_JPEG == p_req->Configuration.Encoding)
	{
		pCfg.encodeCfg[stream].initQuant = p_req->Configuration.RateControl.FrameRateLimit * 4;

		strcpy(pCfg.encodeCfg[stream].encodeFormat.name, "MJPEG");
	}
	
	strcpy(pCfg.encodeCfg[stream].resolution.name, pEntry->res_name);
	
	p_v_enc_cfg->Configuration.Resolution.Width = p_req->Configuration.Resolution.Width;
	p_v_enc_cfg->Configuration.Resolution.Height = p_req->Configuration.Resolution.Height;
	p_v_enc_cfg->Configuration.Quality = (float)p_req->Configuration.Quality;
	p_v_enc_cfg->Configuration.SessionTimeout = p_req->Configuration.SessionTimeout;
	p_v_enc_cfg->Configuration.VideoEncoding = p_req->Configuration.Encoding;

	if (VideoEncoding_H264 == p_req->Configuration.Encoding)
	{
		strcpy(p_v_enc_cfg->Configuration.Encoding, "H264");
		p_v_enc_cfg->Configuration.GovLength = p_req->Configuration.H264.GovLength;
		strcpy(p_v_enc_cfg->Configuration.Profile, onvif_H264ProfileToString(p_req->Configuration.H264.H264Profile));
	}	
	else if (VideoEncoding_JPEG == p_req->Configuration.Encoding)
	{
		strcpy(p_v_enc_cfg->Configuration.Encoding, "JPEG");
	}

	if (p_req->Configuration.RateControlFlag)
	{
		p_v_enc_cfg->Configuration.RateControl.FrameRateLimit = (float)p_req->Configuration.RateControl.FrameRateLimit;
		p_v_enc_cfg->Configuration.RateControl.EncodingInterval = p_req->Configuration.RateControl.EncodingInterval;
		p_v_enc_cfg->Configuration.RateControl.BitrateLimit = p_req->Configuration.RateControl.BitrateLimit;
	}

	memcpy(&p_v_enc_cfg->Configuration.Multicast, &p_req->Configuration.Multicast, sizeof(onvif_MulticastConfiguration));

	onvif_VideoEncoderConfigurationChangedNotify(&p_v_enc_cfg->Configuration);
	
	// todo : here add handler code ...
	
	int needSwitch = anj_config_video_encode_set(&pCfg, 0);
	if (needSwitch)
	{
		anj_video_encode_switch();
	}
	tLastSetTime[stream] = tNowTime;
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Returns the available parameters and their valid ranges to the client. 
 *  Any combination of the parameters obtained using a given media profile
 *  and video encoder configuration shall be a valid input for the 
 *  SetVideoEncoderConfiguration command.
 *
 *  If a video encoder configuration token is provided, the device shall 
 *  return the options compatible with that configuration. If a media profile
 *  token is specified, the device shall return the options compatible with
 *  that media profile. If both a media profile token and a video encoder 
 *  configuration token are specified, the device shall return the options 
 *  compatible with both that media profile and that configuration. 
 *  If no tokens are specified, the options shall be considered generic for 
 *  the device.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_NoConfig
 *
*************************************************************************************/
ONVIF_RET onvif_trt_GetVideoEncoderConfigurationOptions(trt_GetVideoEncoderConfigurationOptions_REQ * p_req, trt_GetVideoEncoderConfigurationOptions_RES * p_res)
{
	ONVIF_PROFILE * p_profile = NULL;
	VideoEncoder2ConfigurationList * p_v_enc_cfg = NULL;

	if (p_req->ProfileTokenFlag && p_req->ProfileToken[0] != '\0')
	{
		p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
		if (NULL == p_profile)
		{
			return ONVIF_ERR_NoProfile;
		}

		p_v_enc_cfg = p_profile->v_enc_cfg;
	}

	if (p_req->ConfigurationTokenFlag && p_req->ConfigurationToken[0] != '\0')
	{
		p_v_enc_cfg = onvif_find_VideoEncoder2Configuration(g_onvif_cfg.v_enc_cfg, p_req->ConfigurationToken);
		if (NULL == p_v_enc_cfg)
		{
			return ONVIF_ERR_NoConfig;
		}
	}

	if (NULL == p_v_enc_cfg)
	{
		p_v_enc_cfg = g_onvif_cfg.v_enc_cfg;
	}

	if (NULL == p_v_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	// todo : Fill p_res ...

	memcpy(&p_res->Options, &p_v_enc_cfg->Options, sizeof(onvif_VideoEncoderConfigurationOptions));

	return ONVIF_OK;
}

int onvif_trt_BuildStreamParams(trt_GetStreamUri_REQ * p_req, ONVIF_PROFILE * p_profile, char * buff, int len)
{
	int offset = 0;

	if (StreamType_RTP_Unicast == p_req->StreamSetup.Stream)
	{
		offset += snprintf(buff+offset, len-offset, "&amp;t=%s", "unicast");
	}
	else if (StreamType_RTP_Multicast == p_req->StreamSetup.Stream)
	{
		offset += snprintf(buff+offset, len-offset, "&amp;t=%s", "multicast");
	}

	if (TransportProtocol_UDP == p_req->StreamSetup.Transport.Protocol)
	{
		offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "udp");
	}
	else if (TransportProtocol_TCP == p_req->StreamSetup.Transport.Protocol)
	{
		offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "tcp");
	}
	else if (TransportProtocol_RTSP == p_req->StreamSetup.Transport.Protocol)
	{
		offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "rtsp");
	}
	else if (TransportProtocol_HTTP == p_req->StreamSetup.Transport.Protocol)
	{
		offset += snprintf(buff+offset, len-offset, "&amp;p=%s", "http");
	}

	/**
	 * If the audio and video parameters have been set to the encoder 
	 * in the onvif_trt_SetVideoEncoderConfiguration function, 
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

	if (p_profile->a_dec_cfg)
	{
		char encoding[8];

		if (p_profile->a_dec_cfg->Options.AACDecOptionsFlag)
		{
			strcpy(encoding, "AAC");
		}
		else if (p_profile->a_dec_cfg->Options.G726DecOptionsFlag)
		{
			strcpy(encoding, "G726");
		}
		else
		{
			strcpy(encoding, "G711");
		}

		offset += snprintf(buff+offset, len-offset, "&amp;bce=%s", encoding);
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
 *  indefinitely even if the profile is changed. The InvalidAfterConnect,
 *  InvalidAfterReboot and Timeout Parameter should be set accordingly 
 *  (InvalidAfterConnect=false, InvalidAfterReboot=false, timeout=PT0S).
 *
 *  If a multicast stream is requested at least one of VideoEncoderConfiguration, 
 *  AudioEncoderConfiguration and MetadataConfiguration shall have a valid 
 *  multicast setting.
 *  
 *  For full compatibility with other ONVIF services a device should not 
 *  generate Uris longer than 128 octets.
 *
 *  On a request for transport protocol http a device shall return a url that 
 *  uses the same port as the web service. This enables seamless NAT traversal.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *	ONVIF_ERR_InvalidStreamSetup
 *	ONVIF_ERR_StreamConflict
 *	ONVIF_ERR_IncompleteConfiguration
 *	ONVIF_ERR_InvalidMulticastSettings
 *
*************************************************************************************/
ONVIF_RET onvif_trt_GetStreamUri(HTTPCLN * p_user, trt_GetStreamUri_REQ * p_req, trt_GetStreamUri_RES * p_res)
{
	log_print(HT_LOG_INFO, "onvif_trt_GetStreamUri\n");
	int offset = 0;
	int len = sizeof(p_res->MediaUri.Uri);
	int base_len = 0;
	int max_cred_len = 0;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	
	char username[256]="";
	char password[256]="";
	get_username_and_password(username, password);
	
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
		int stream = 1;
		if (strstr(p_req->ProfileToken, "ProfileToken_1_1") != NULL || strstr(p_req->ProfileToken, "ProfileToken_2_1") != NULL)
			stream = 0;
		else if (strstr(p_req->ProfileToken, "ProfileToken_1_2") != NULL || strstr(p_req->ProfileToken, "ProfileToken_2_2") != NULL)
			stream = 1;
		else if (strstr(p_req->ProfileToken, "ProfileToken_1_3") != NULL || strstr(p_req->ProfileToken, "ProfileToken_2_3") != NULL)
			stream = 2;
		// set the media uri
		if (strstr(p_req->ProfileToken, "ProfileToken_1_") != NULL)
			offset += snprintf(p_res->MediaUri.Uri, len, "rtsp://%.*s:%d/ch01/stream%d?username=%.*s&amp;password=%.*s",
			                   (int)(sizeof(sip) - 1), sip, get_rtsp_port(), stream,
			                   max_cred_len, username, max_cred_len, password);
		else
			offset += snprintf(p_res->MediaUri.Uri, len, "rtsp://%.*s:%d/ch02/stream%d?username=%.*s&amp;password=%.*s",
			                   (int)(sizeof(sip) - 1), sip, get_rtsp_port(), stream,
			                   max_cred_len, username, max_cred_len, password);
	}
	else
	{
		int stream = 1;
		if (strstr(p_req->ProfileToken, "ProfileToken_1") != NULL)
			stream = 0;
		else if (strstr(p_req->ProfileToken, "ProfileToken_2") != NULL)
			stream = 1;
		else if (strstr(p_req->ProfileToken, "ProfileToken_3") != NULL)
			stream = 2;
	
		// set the media uri
		offset += snprintf(p_res->MediaUri.Uri, len, "rtsp://%.*s:%d/stream%d?username=%.*s&amp;password=%.*s",
		                   (int)(sizeof(sip) - 1), sip, get_rtsp_port(), stream,
		                   max_cred_len, username, max_cred_len, password);
	}
	p_res->MediaUri.InvalidAfterConnect = FALSE;
	p_res->MediaUri.InvalidAfterReboot = FALSE;
	p_res->MediaUri.Timeout = 60;

	return ONVIF_OK;
}

ONVIF_RET onvif_trt_GetStreamUri_old(HTTPCLN * p_user, trt_GetStreamUri_REQ * p_req, trt_GetStreamUri_RES * p_res)
{
	log_print(HT_LOG_INFO, "onvif_trt_GetStreamUri\n");
	int offset = 0;
	int len = sizeof(p_res->MediaUri.Uri);
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	// set the media uri
	if (p_profile->stream_uri[0] == '\0')
	{
		/*If the <stream_uri> node value under the <profile> node in the configuration file is not set, the default rtsp url is generated*/
		char sip[32];
		onvif_get_service_ip_by_user(p_user, sip, sizeof(sip) - 1);
		if (p_req->StreamSetup.Transport.Protocol == TransportProtocol_HTTP)
		{
			offset += snprintf(p_res->MediaUri.Uri, len, "http://%s/%s", sip, RTSP_URL_SUFFIX);
		}
		else
		{
			offset += snprintf(p_res->MediaUri.Uri, len, "rtsp://%s/%s", sip, RTSP_URL_SUFFIX);
		}
		
		onvif_trt_BuildStreamParams(p_req, p_profile, p_res->MediaUri.Uri + offset, len - offset);
	}
	else
	{
		/** If the <stream_uri> node value under the <profile> node in the * configuration file is set, the rtsp url is used*/
		offset += snprintf(p_res->MediaUri.Uri, len, "%s", p_profile->stream_uri);
		if (p_profile->append_params)
		{
			onvif_trt_BuildStreamParams(p_req, p_profile, p_res->MediaUri.Uri+offset, len-offset);
		}
	}
	p_res->MediaUri.InvalidAfterConnect = FALSE;
	p_res->MediaUri.InvalidAfterReboot = FALSE;
	p_res->MediaUri.Timeout = 60;

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Starts multicast streaming using a specified media profile of a device. 
 *  Streaming continues until StopMulticastStreaming is called for the same Profile. 
 *  The streaming shall continue after a reboot of the device until a 
 *  StopMulticastStreaming request is received. The multicast address, port and TTL 
 *  are configured in the VideoEncoderConfiguration, AudioEncoderConfiguration
 *  and MetadataConfiguration respectively.
 *
 *  Multicast streaming may stop when the corresponding profile is deleted 
 *  or one of its Configurations is altered via one of the set configuration 
 *  methods.
 *
 *  The implementation shall ensure that the RTP stream can be decoded without
 *  setting up an RTSP control connection. 
 *  Especially in case of H.264 video, the SPS/PPS header shall be sent inband.
 *
 * @returnn
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_IncompleteConfiguration
 *
*************************************************************************************/
ONVIF_RET onvif_trt_StartMulticastStreaming(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, token);
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
 *  Stop multicast streaming using a specified media profile of a device. 
 *  In case that a device receives the StopMulticastStreaming request whose 
 *  corresponding multicast streaming is not started, the device should 
 *  reply with successful StopMulticastStreamingResponse.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_IncompleteConfiguration
 *
*************************************************************************************/
ONVIF_RET onvif_trt_StopMulticastStreaming(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, token);
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

/************************************************************************************
 *
 * @brief
 *  adds a Metadata configuration to an existing media profile. 
 *  If a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent.
 *
 *  Adding a MetadataConfiguration to a Profile means that streams using 
 *  that profile contain metadata. Metadata can consist of events, PTZ status, 
 *  and/or video analytics data.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddMetadataConfiguration(trt_AddMetadataConfiguration_REQ * p_req)
{
	MetadataConfigurationList * p_metadata_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_metadata_cfg = onvif_find_MetadataConfiguration(g_onvif_cfg.metadata_cfg, p_req->ConfigurationToken);
	if (NULL == p_metadata_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

    if (p_profile->metadata_cfg != p_metadata_cfg)
	{
		if (p_profile->metadata_cfg && p_profile->metadata_cfg->Configuration.UseCount > 0)
		{
			p_profile->metadata_cfg->Configuration.UseCount--;
		}
		
		p_metadata_cfg->Configuration.UseCount++;
		
		p_profile->metadata_cfg = p_metadata_cfg;
	}

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes a MetadataConfiguration from an existing media profile. 
 *  If the media profile does not contain a MetadataConfiguration, 
 *  the operation has no effect. The removal shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveMetadataConfiguration(const char * profile_token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, profile_token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

    if (p_profile->metadata_cfg && p_profile->metadata_cfg->Configuration.UseCount > 0)
	{
		p_profile->metadata_cfg->Configuration.UseCount--;
	}
	
	p_profile->metadata_cfg = NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Changes the media profile structure relating to video source for the
 *  specified video source mode. A device that indicates a capability of 
 *  VideoSourceMode shall support this command. The behavior after changing 
 *  the mode is not defined in this specification.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoVideoSource
 * 	ONVIF_ERR_NoVideoSourceMode
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetVideoSourceMode(trt_SetVideoSourceMode_REQ * p_req, trt_SetVideoSourceMode_RES * p_res)
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

	// todo : handler set video source mode ...
	p_res->Reboot = FALSE;

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Obtain a JPEG snhapshot from the device. 
 *  The returned URI shall remain valid indefinitely even if the profile 
 *  is changed. The ValidUntilConnect, ValidUntilReboot and Timeout 
 *  Parameter shall be set accordingly 
 *  (ValidUntilConnect=false, ValidUntilReboot=false, timeout=PT0S).
 *  The URI can be used for acquiring a JPEG image through a HTTP GET operation.
 *
 *  The image encoding will always be JPEG regardless of the encoding setting 
 *  in the media profile. The JPEG settings (like resolution or quality) should
 *  be taken from the profile if suitable. The provided image shall be updated 
 *  automatically and independent from calls to GetSnapshotUri.
 *
 *  A device supporting the media service should support this command. 
 *  A device shall support this command when the SnapshotUri capability is 
 *  set to true.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_IncompleteConfiguration
 *
*************************************************************************************/
ONVIF_RET onvif_trt_GetSnapshotUri(HTTPCLN * p_user, trt_GetSnapshotUri_REQ * p_req, trt_GetSnapshotUri_RES * p_res)
{
	char sip[32];
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	
	log_print(HT_LOG_INFO, "p_req->ProfileToken:%s\n", p_req->ProfileToken);
	

	onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);
	
	int stream = 1;
	char username[256] = "";
	char password[256] = "";
	// set the media uri
	if (g_stereo)
	{
		if (!strcmp(p_req->ProfileToken, "ProfileToken_1_1") || !strcmp(p_req->ProfileToken, "ProfileToken_2_1"))
			stream = 0;
		else if (!strcmp(p_req->ProfileToken, "ProfileToken_1_2") || !strcmp(p_req->ProfileToken, "ProfileToken_2_2"))
			stream = 1;
	}
	else
	{
		if (!strcmp(p_req->ProfileToken, "ProfileToken_1"))
			stream = 0;
		else if (!strcmp(p_req->ProfileToken, "ProfileToken_2"))
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
			snprintf(p_res->MediaUri.Uri, sizeof(p_res->MediaUri.Uri),
				"http://%s:%d/cgi-bin/snapshot.cgi?stream=%d&amp;username=%.63s&amp;password=%.63s",
				sip, g_onvif_cls.http_port, stream, username, password);
		}
		else
		{
			snprintf(p_res->MediaUri.Uri, sizeof(p_res->MediaUri.Uri),
				"http://%s:%d/cgi-bin/snapshot.cgi?stream=%d",
				sip, g_onvif_cls.http_port, stream);
		}
	}
#ifdef HTTPS
	else if (g_onvif_cfg.https_enable && p_srv->https)
	{
		if (username[0] != '\0' && password[0] != '\0')
		{
			snprintf(p_res->MediaUri.Uri, sizeof(p_res->MediaUri.Uri),
				"https://%s:%d/cgi-bin/snapshot.cgi?stream=%d&amp;username=%.63s&amp;password=%.63s",
				sip, g_onvif_cls.https_port, stream, username, password);
		}
		else
		{
			snprintf(p_res->MediaUri.Uri, sizeof(p_res->MediaUri.Uri),
				"https://%s:%d/cgi-bin/snapshot.cgi?stream=%d",
				sip, g_onvif_cls.https_port, stream);
		}
	}
#endif
	log_print(HT_LOG_INFO, "GetSnapshotUri:%s\n", p_res->MediaUri.Uri);
	p_res->MediaUri.InvalidAfterConnect = FALSE;
	p_res->MediaUri.InvalidAfterReboot = FALSE;
	p_res->MediaUri.Timeout = 60;

	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Synchronization points allow clients to decode and correctly use all data 
 *  after the synchronization point.
 *
 *  For example, if a video stream is configured with a large I-frame distance 
 *  and a client loses a single packet, the client does not display video until 
 *  the next I-frame is transmitted. In such cases, the client can request a
 *  Synchronization Point which enforces the device to add an I-frame as soon as 
 *  possible. Clients can request Synchronization Points for profiles.The device 
 *  shall add synchronization points for all streams associated with this profile.
 *
 *  Similarly, a synchronization point is used to get an update on full PTZ or 
 *  event status through the metadata stream.
 *
 *  If a video stream is associated with the profile, an I-frame shall be added 
 *  to this video stream.
 *  If a PTZ metadata stream is associated to the profile, the PTZ position 
 *  shall be repeated within the metadata stream.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetSynchronizationPoint(trt_SetSynchronizationPoint_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}
	
	// todo : here add handler code ...
	int streamno = 0;
	if(strstr(p_req->ProfileToken, "ProfileToken_1"))
		streamno = 0;
	else if(strstr(p_req->ProfileToken, "ProfileToken_2"))
		streamno = 1;

	log_print(HT_LOG_INFO, "add an I-Frame,streamno=%d", streamno);
	anj_video_request_idr(0, streamno); // request IDR frame
	return ONVIF_OK;
}

#ifdef VIDEO_ANALYTICS

/************************************************************************************
 *
 * @brief
 *  adds a VideoAnalytics configuration to an existing media profile. 
 *  If a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent.
 *
 *  Adding a VideoAnalyticsConfiguration to a media profile means that 
 *  streams using that media profile can contain video analytics data 
 *  (in the metadata) as defined by the submitted configuration reference.
 *
 *  A profile containing only a video analytics configuration but no video 
 *  source configuration is incomplete. Therefore, a client should first 
 *  add a video source configuration to a profile before adding a video 
 *  analytics configuration. The device can deny adding of a video analytics 
 *  configuration before a video source configuration. 
 *
 * @return
 *  Possible error:
 *  ONVIF_ERR_NoProfile
 *  ONVIF_ERR_NoConfig
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddVideoAnalyticsConfiguration(trt_AddVideoAnalyticsConfiguration_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->ConfigurationToken);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_profile->va_cfg != p_va_cfg)
	{
		if (p_profile->va_cfg && p_profile->va_cfg->Configuration.UseCount > 0)
		{
			p_profile->va_cfg->Configuration.UseCount--;
		}
		
		p_va_cfg->Configuration.UseCount++;
		
		p_profile->va_cfg = p_va_cfg;
	}

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	// todo : add video analytics configuration code ...
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes a VideoAnalyticsConfiguration from an existing media profile. 
 *  If the media profile does not contain a VideoAnalyticsConfiguration, 
 *  the operation has no effect. The removal shall be persistent.
 *
 * @return
 *  Possible error:
 *  ONVIF_ERR_NoProfile
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveVideoAnalyticsConfiguration(trt_RemoveVideoAnalyticsConfiguration_REQ * p_req)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->va_cfg && p_profile->va_cfg->Configuration.UseCount > 0)
	{
		p_profile->va_cfg->Configuration.UseCount--;
	}
	
	p_profile->va_cfg = NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	// todo : remove video analytics configuration code ...
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  A video analytics configuration is modified using this command. 
 *  The ForcePersistence flag indicates if the changes shall remain after 
 *  reboot of the device or not. Running streams using this configuration
 *  shall be immediately updated according to the new settings. Otherwise
 *  inconsistencies can occur between the scene description processed by 
 *  the rule engine and the notifications produced by analytics engine and
 *  rule engine which reference the very same video analytics configuration
 *  token.
 *
 * @return
 *  Possible error:
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_ConfigModify
 *  ONVIF_ERR_ConfigurationConflict 
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetVideoAnalyticsConfiguration(trt_SetVideoAnalyticsConfiguration_REQ * p_req)
{
	VideoAnalyticsConfigurationList * p_va_cfg;

	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, p_req->Configuration.token);
	if (NULL == p_va_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	// check the configuration parameters ...

	// save the video analytics configuration 
	strcpy(p_va_cfg->Configuration.Name, p_req->Configuration.Name);
	
	onvif_free_Configs(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
	onvif_free_Configs(&p_va_cfg->Configuration.RuleEngineConfiguration.Rule);

	p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule = p_req->Configuration.AnalyticsEngineConfiguration.AnalyticsModule;
	p_va_cfg->Configuration.RuleEngineConfiguration.Rule = p_req->Configuration.RuleEngineConfiguration.Rule;

    onvif_VideoAnalyticsConfigurationChangedNotify(&p_va_cfg->Configuration);
    
	// todo : set video analytics configuration code ...

	
	return ONVIF_OK;
}

#endif // VIDEO_ANALYTICS

#ifdef AUDIO_SUPPORT

/************************************************************************************
 *
 * @brief
 *  Adds an AudioSourceConfiguration to an existing media profile. 
 *  If a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddAudioSourceConfiguration(trt_AddAudioSourceConfiguration_REQ * p_req)
{
	AudioSourceConfigurationList * p_a_src_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_a_src_cfg = onvif_find_AudioSourceConfiguration(g_onvif_cfg.a_src_cfg, p_req->ConfigurationToken);
	if (NULL == p_a_src_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_profile->a_src_cfg != p_a_src_cfg)
	{
		if (p_profile->a_src_cfg && p_profile->a_src_cfg->Configuration.UseCount > 0)
		{
			p_profile->a_src_cfg->Configuration.UseCount--;
		}
		
		p_a_src_cfg->Configuration.UseCount++;
		
		p_profile->a_src_cfg = p_a_src_cfg;
	}

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Adds an AudioEncoderConfiguration to an existing media profile. 
 *  If a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent.
 *
 *  A device shall support adding a compatible AudioEncoderConfiguration 
 *  to a Profile containing an AudioSourceConfiguration and shall support 
 *  streaming audio data of such a Profile.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddAudioEncoderConfiguration(trt_AddAudioEncoderConfiguration_REQ * p_req)
{
    AudioEncoder2ConfigurationList * p_a_enc_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_a_enc_cfg = onvif_find_AudioEncoder2Configuration(g_onvif_cfg.a_enc_cfg, p_req->ConfigurationToken);
	if (NULL == p_a_enc_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_profile->a_enc_cfg != p_a_enc_cfg)
	{
		if (p_profile->a_enc_cfg && p_profile->a_enc_cfg->Configuration.UseCount > 0)
		{
			p_profile->a_enc_cfg->Configuration.UseCount--;
		}
		
		p_a_enc_cfg->Configuration.UseCount++;
		
		p_profile->a_enc_cfg = p_a_enc_cfg;
	}

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes an AudioEncoderConfiguration from an existing media profile. 
 *  If the media profile does not contain an AudioEncoderConfiguration, 
 *  the operation has no effect. The removal shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveAudioEncoderConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->a_enc_cfg && p_profile->a_enc_cfg->Configuration.UseCount > 0)
	{
		p_profile->a_enc_cfg->Configuration.UseCount--;
	}
	
	p_profile->a_enc_cfg = NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes an AudioSourceConfiguration from an existing media profile. 
 *  If the media profile does not contain an AudioSourceConfiguration, 
 *  the operation has no effect. The removal shall be persistent.
 *
 *  Audio source configurations should only be removed after removing an 
 *  AudioEncoderConfiguration from the media profile.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveAudioSourceConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

    if (p_profile->a_enc_cfg)
    {
        return ONVIF_ERR_ConfigurationConflict;
    }
    
	if (p_profile->a_src_cfg && p_profile->a_src_cfg->Configuration.UseCount > 0)
	{
		p_profile->a_src_cfg->Configuration.UseCount--;
	}
	
	p_profile->a_src_cfg= NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Modifies an audio encoder configuration. 
 *  The ForcePersistence flag indicates if the changes shall remain after 
 *  reboot of the device. Changes in the Multicast settings shall always be 
 *  persistent. Running streams using this configuration may be immediately 
 *  updated according to the new settings. The changes are not guaranteed to
 *  take effect unless the client requests a new stream URI and restarts any
 *  affected streams.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigModify
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_SetAudioEncoderConfiguration(trt_SetAudioEncoderConfiguration_REQ * p_req)
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
		p_req->Configuration.SampleRate != 48)
	{
		return ONVIF_ERR_ConfigModify;
	}

	p_a_enc_cfg->Configuration.SessionTimeout = p_req->Configuration.SessionTimeout;
	p_a_enc_cfg->Configuration.Bitrate = p_req->Configuration.Bitrate;
	p_a_enc_cfg->Configuration.SampleRate = p_req->Configuration.SampleRate;
	p_a_enc_cfg->Configuration.AudioEncoding = p_req->Configuration.Encoding;

	if (AudioEncoding_G711 == p_req->Configuration.Encoding)
	{
		strcpy(p_a_enc_cfg->Configuration.Encoding, "G711");
	}
	else if (AudioEncoding_G711A == p_req->Configuration.Encoding)
	{
		strcpy(p_a_enc_cfg->Configuration.Encoding, "G711A");
	}
	else if (AudioEncoding_AAC == p_req->Configuration.Encoding)
	{
		strcpy(p_a_enc_cfg->Configuration.Encoding, "AAC");
	}

	memcpy(&p_a_enc_cfg->Configuration.Multicast, &p_req->Configuration.Multicast, sizeof(onvif_MulticastConfiguration));

	onvif_AudioEncoderConfigurationChangedNotify(&p_a_enc_cfg->Configuration);
	// todo : add set audio encoder code ...
	AudioConfig AudioCfg;
	memcpy(&AudioCfg, &((MediaConfig *)getMediaConfig())->audioConfig, sizeof(AudioCfg));


	if(AudioEncoding_AAC == p_req->Configuration.Encoding)
	{
		if (((p_req->Configuration.Bitrate == 8000) || (p_req->Configuration.Bitrate == 16000)) && (p_req->Configuration.SampleRate == 64000))
		{
			AudioCfg.audioEncode.bitRate = p_req->Configuration.Bitrate;
			AudioCfg.audioEncode.sampleRate = p_req->Configuration.SampleRate;
		}
		strncpy(AudioCfg.audioEncode.audioEncodeType.typeName, "ACC", AUDIO_ENCODE_TYPE_MAX_LEN);
	}
	else if(AudioEncoding_G711 == p_req->Configuration.Encoding)
	{
		if (p_req->Configuration.Bitrate == 16000 && p_req->Configuration.SampleRate == 16000)
		{
			AudioCfg.audioEncode.bitRate = 16000;
			AudioCfg.audioEncode.sampleRate = 16000;
		}
		
		strncpy(AudioCfg.audioEncode.audioEncodeType.typeName, "G.711", AUDIO_ENCODE_TYPE_MAX_LEN);
	}
	else if(AudioEncoding_G711A == p_req->Configuration.Encoding)
	{
		if (p_req->Configuration.Bitrate == 16000 && p_req->Configuration.SampleRate == 16000)
		{
			AudioCfg.audioEncode.bitRate = 16000;
			AudioCfg.audioEncode.sampleRate = 16000;
		}
		
		strncpy(AudioCfg.audioEncode.audioEncodeType.typeName, "G.711A", AUDIO_ENCODE_TYPE_MAX_LEN);
	}
	AudioCfg.audioEncode.enable = 1;
	
	anj_config_audio_set(&AudioCfg);
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Adds an AudioDecoderConfiguration to an existing media profile. 
 *  If a configuration exists in the media profile, it shall be replaced. 
 *  The change shall be persistent. 
 *
 *  An device that signals support for Audio outputs via its Device IO 
 *  AudioOutputs capability shall support the addition of an audio decoder 
 *  configuration to a profile
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddAudioDecoderConfiguration(trt_AddAudioDecoderConfiguration_REQ * p_req)
{
    AudioDecoderConfigurationList * p_a_dec_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_a_dec_cfg = onvif_find_AudioDecoderConfiguration(g_onvif_cfg.a_dec_cfg, p_req->ConfigurationToken);
	if (NULL == p_a_dec_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_profile->a_dec_cfg != p_a_dec_cfg)
	{
		if (p_profile->a_dec_cfg && p_profile->a_dec_cfg->Configuration.UseCount > 0)
		{
			p_profile->a_dec_cfg->Configuration.UseCount--;
		}
		
		p_a_dec_cfg->Configuration.UseCount++;
		
		p_profile->a_dec_cfg = p_a_dec_cfg;
	}

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes an AudioDecoderConfiguration from an existing media profile. 
 *  If the media profile does not contain an AudioDecoderConfiguration, 
 *  the operation has no effect. The removal shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveAudioDecoderConfiguration(trt_RemoveAudioDecoderConfiguration_REQ * p_req)
{
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->a_dec_cfg && p_profile->a_dec_cfg->Configuration.UseCount > 0)
	{
		p_profile->a_dec_cfg->Configuration.UseCount--;
	}
	
	p_profile->a_dec_cfg = NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

#endif // end of AUDIO_SUPPORT

#ifdef PTZ_SUPPORT

/************************************************************************************
 *
 * @brief
 *  Adds a PTZConfiguration to an existing media profile. 
 *  If a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent.
 *
 *  Adding a PTZConfiguration to a media profile means that streams using 
 *  that media profile can contain PTZ status (in the metadata), and that 
 *  the media profile can be used for controlling PTZ movement.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddPTZConfiguration(trt_AddPTZConfiguration_REQ * p_req)
{
    PTZConfigurationList * p_ptz_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_ptz_cfg = onvif_find_PTZConfiguration(g_onvif_cfg.ptz_cfg, p_req->ConfigurationToken);
	if (NULL == p_ptz_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

    if (p_profile->ptz_cfg != p_ptz_cfg)
	{
		if (p_profile->ptz_cfg && p_profile->ptz_cfg->Configuration.UseCount > 0)
		{
			p_profile->ptz_cfg->Configuration.UseCount--;
		}
		
		p_ptz_cfg->Configuration.UseCount++;
		
		p_profile->ptz_cfg = p_ptz_cfg;
	}

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  removes a PTZConfiguration from an existing media profile. 
 *  If the media profile does not contain a PTZConfiguration, 
 *  the operation has no effect. The removal shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemovePTZConfiguration(const char * token)
{
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, token);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

    if (p_profile->ptz_cfg && p_profile->ptz_cfg->Configuration.UseCount > 0)
	{
		p_profile->ptz_cfg->Configuration.UseCount--;
	}
	
	p_profile->ptz_cfg = NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

#endif // PTZ_SUPPORT

#ifdef DEVICEIO_SUPPORT

/************************************************************************************
 *
 * @brief
 *  adds an AudioOutputConfiguration to an existing media profile. 
 *  If a configuration exists in the media profile, it will be replaced. 
 *  The change shall be persistent. 
 *
 *  An device that signals support for Audio outputs via its Device IO 
 *  AudioOutputs capability shall support the addition of an audio output
 *  configuration to a profile.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_AddAudioOutputConfiguration(trt_AddAudioOutputConfiguration_REQ * p_req)
{
    AudioOutputConfigurationList * p_a_output_cfg;
	ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	p_a_output_cfg = onvif_find_AudioOutputConfiguration(g_onvif_cfg.a_output_cfg, p_req->ConfigurationToken);
	if (NULL == p_a_output_cfg)
	{
		return ONVIF_ERR_NoConfig;
	}

	if (p_profile->a_output_cfg != p_a_output_cfg)
	{
		if (p_profile->a_output_cfg && p_profile->a_output_cfg->Configuration.UseCount > 0)
		{
			p_profile->a_output_cfg->Configuration.UseCount--;
		}
		
		p_a_output_cfg->Configuration.UseCount++;
		
		p_profile->a_output_cfg = p_a_output_cfg;
	}

	onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/************************************************************************************
 *
 * @brief
 *  Removes an AudioOutputConfiguration from an existing media profile. 
 *  If the media profile does not contain an AudioOutputConfiguration, 
 *  the operation has no effect. The removal shall be persistent.
 *
 * @return
 *  Possible error:
 * 	ONVIF_ERR_NoProfile
 * 	ONVIF_ERR_NoConfig
 * 	ONVIF_ERR_ConfigurationConflict
 *
*************************************************************************************/
ONVIF_RET onvif_trt_RemoveAudioOutputConfiguration(trt_RemoveAudioOutputConfiguration_REQ * p_req)
{
    ONVIF_PROFILE * p_profile = onvif_find_profile(g_onvif_cfg.profiles, p_req->ProfileToken);
	if (NULL == p_profile)
	{
		return ONVIF_ERR_NoProfile;
	}

	if (p_profile->a_output_cfg && p_profile->a_output_cfg->Configuration.UseCount > 0)
	{
		p_profile->a_output_cfg->Configuration.UseCount--;
	}
	
	p_profile->a_output_cfg = NULL;

    onvif_ProfileChangedNotify(p_profile, PropertyOperation_Changed);
    
	return ONVIF_OK;
}

#endif // end of DEVICEIO_SUPPORT

#endif // end of MEDIA_SUPPORT
ONVIF_RET onvif_tpl_SetVideoEncoderConfiguration(trt_SetVideoEncoderConfiguration_REQ * p_req)
{
	return ONVIF_OK;
}


