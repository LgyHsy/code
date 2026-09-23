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

#include "onvif.h"
#include "onvif_recording.h"
#include "xml_node.h"
#include "onvif_utils.h"
#include "onvif_event.h"
#include "onvif_pkt.h"

#ifdef PROFILE_G_SUPPORT

/***************************************************************************************/
extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_IDX g_onvif_idx;

/***************************************************************************************/

/**
 * Whenever a track is created, a device shall provide the following event
 *
 */
void onvif_CreateTrackNotify(RecordingList * p_recording, TrackList * p_track)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:RecordingConfig/CreateTrack", PropertyOperation_Initialized, 
        "RecordingToken", p_recording->Recording.RecordingToken,
        "TrackToken", p_track->Track.TrackToken, 
        NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever a track is deleted, a device shall provide the following event
 *
 */
void onvif_DeleteTrackNotify(RecordingList * p_recording, TrackList * p_track)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:RecordingConfig/DeleteTrack", PropertyOperation_Deleted, 
        "RecordingToken", p_recording->Recording.RecordingToken,
        "TrackToken", p_track->Track.TrackToken, 
        NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever a recording is created, a device shall provide the following event
 *
 */
void onvif_CreateRecordingNotify(RecordingList * p_recording)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:RecordingConfig/CreateRecording", PropertyOperation_Initialized, 
        "RecordingToken", p_recording->Recording.RecordingToken,
        NULL, NULL, NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * Whenever a recording is deleted, a device shall provide the following event
 *
 */
void onvif_DeleteRecordingNotify(RecordingList * p_recording)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:RecordingConfig/DeleteRecording", PropertyOperation_Deleted, 
        "RecordingToken", p_recording->Recording.RecordingToken,
        NULL, NULL, NULL, NULL, NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * If the state field of the RecordingJobStateInformation structure changes, 
 * a device shall provide the following event
 *
 */
void onvif_RecordingJobStateNotify(RecordingJobList * p_recordingjob, onvif_PropertyOperation op)
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
		strcpy(p_msg->Topic, "tns1:RecordingConfig/JobState");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = op;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{		    
			strcpy(p_simpleitem->SimpleItem.Name, "RecordingJobToken");
			strcpy(p_simpleitem->SimpleItem.Value, p_recordingjob->RecordingJob.JobToken);
		}

        p_msg->Message.DataFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{		    
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, p_recordingjob->RecordingJob.JobConfiguration.Mode);
		}
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 1024;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Information");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
				onvif_RecordingJobStateInformation res;
				memset(&res, 0, sizeof(res));

		        memset(p_elementitem->ElementItem.Any, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;                
                
		        if (ONVIF_OK == onvif_trc_GetRecordingJobState(p_recordingjob->RecordingJob.JobToken, &res))
		        {
		            int offset = 0;
		            char * buff = p_elementitem->ElementItem.Any;
		            
		            offset += snprintf(buff+offset, buflen-offset, "<tt:RecordingJobStateInformation>");
		            offset += build_RecordingJobStateInformation_xml(buff+offset, buflen-offset, &res);
		            offset += snprintf(buff+offset, buflen-offset, "</tt:RecordingJobStateInformation>");
		        }
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

/**
 * If the configuration of a recording is changed, a device shall provide the following event
 *
 */
void onvif_RecordingConfigurationNotify(RecordingList * p_recording)
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
        strcpy(p_msg->Topic, "tns1:RecordingConfig/RecordingConfiguration");
        p_msg->Message.PropertyOperationFlag = 1;
        p_msg->Message.PropertyOperation = PropertyOperation_Changed;
        p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;

        p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
        if (p_simpleitem)
        {
            strcpy(p_simpleitem->SimpleItem.Name, "RecordingToken");
            strcpy(p_simpleitem->SimpleItem.Value, p_recording->Recording.RecordingToken);
        }

        p_msg->Message.DataFlag = 1;

        p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
        if (p_elementitem)
        {
            int buflen = 2048;
            
            strcpy(p_elementitem->ElementItem.Name, "Configuration");

            p_elementitem->ElementItem.Any = (char *)malloc(buflen);
            if (p_elementitem->ElementItem.Any)
            {
                int offset = 0;
                char * buff = p_elementitem->ElementItem.Any;

                memset(buff, 0, buflen);

                p_elementitem->ElementItem.AnyFlag = 1;

                offset += snprintf(buff+offset, buflen-offset, "<tt:RecordingConfiguration>");
                offset += build_RecordingConfiguration_xml(buff+offset, buflen-offset, &p_recording->Recording.Configuration);
                offset += snprintf(buff+offset, buflen-offset, "</tt:RecordingConfiguration>");
            }
        }

        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * If the configuration of a track is changed, a device shall provide the following event
 *
 */
void onvif_TrackConfigurationNotify(RecordingList * p_recording, TrackList * p_track)
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
		strcpy(p_msg->Topic, "tns1:RecordingConfig/TrackConfiguration");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "RecordingToken");
			strcpy(p_simpleitem->SimpleItem.Value, p_recording->Recording.RecordingToken);
		}

		p_simpleitem = onvif_add_SimpleItem(&p_message->NotificationMessage.Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "TrackToken");
			strcpy(p_simpleitem->SimpleItem.Value, p_track->Track.TrackToken);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 2048;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;
		        
	            offset += snprintf(buff+offset, buflen-offset, "<tt:TrackConfiguration>");
	            offset += build_TrackConfiguration_xml(buff+offset, buflen-offset, &p_track->Track.Configuration);
	            offset += snprintf(buff+offset, buflen-offset, "</tt:TrackConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

/**
 * If the configuration of a recording job is changed, a device shall provide the following event
 *
 */
void onvif_RecordingJobConfigurationNotify(RecordingJobList * p_recordingjob)
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
		strcpy(p_msg->Topic, "tns1:RecordingConfig/RecordingJobConfiguration");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL)+1;

        p_msg->Message.SourceFlag = 1;
        
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "RecordingJobToken");
			strcpy(p_simpleitem->SimpleItem.Value, p_recordingjob->RecordingJob.JobToken);
		}

		p_msg->Message.DataFlag = 1;
		
		p_elementitem = onvif_add_ElementItem(&p_msg->Message.Data.ElementItem);
		if (p_elementitem)
		{
		    int buflen = 2048;
		    
		    strcpy(p_elementitem->ElementItem.Name, "Configuration");
		    
		    p_elementitem->ElementItem.Any = (char *)malloc(buflen);
		    if (p_elementitem->ElementItem.Any)
		    {
		        int offset = 0;
		        char * buff = p_elementitem->ElementItem.Any;
		        
		        memset(buff, 0, buflen);
		        
		        p_elementitem->ElementItem.AnyFlag = 1;
		        
	            offset += snprintf(buff+offset, buflen-offset, 
	                "<tt:RecordingJobConfiguration ScheduleToken=\"%s\">",
	                p_recordingjob->RecordingJob.JobConfiguration.ScheduleToken);
	            offset += build_RecordingJobConfiguration_xml(buff+offset, buflen-offset,
	                        &p_recordingjob->RecordingJob.JobConfiguration);
	            offset += snprintf(buff+offset, buflen-offset, 
	                "</tt:RecordingJobConfiguration>");
		    }
		}

		onvif_put_NotificationMessage(p_message);
	}
}

/**
 * @brief
 *  Create a new recording
 *
 *  This method is optional. It shall be available if the 
 *  Recording/DynamicRecordings capability is TRUE.
 *
 *@return
 *  The possible return values:
 *  ONVIF_ERR_BadConfiguration,
 *  ONVIF_ERR_MaxRecordings,
 */
ONVIF_RET onvif_trc_CreateRecording(trc_CreateRecording_REQ * p_req)
{
	TrackList * p_track;	
	RecordingList * p_recording = onvif_add_Recording(&g_onvif_cfg.recordings);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_MaxRecordings;
	}

	memcpy(&p_recording->Recording.Configuration, &p_req->RecordingConfiguration, sizeof(onvif_RecordingConfiguration));
	
	strcpy(p_req->RecordingToken, p_recording->Recording.RecordingToken);

	// send CreateRecording event
    onvif_CreateRecordingNotify(p_recording);
    
	// create three tracks
	p_track = onvif_add_Track(&p_recording->Recording.Tracks);
	if (p_track)
	{
		strcpy(p_track->Track.TrackToken, "VIDEO001");
		p_track->Track.Configuration.TrackType = TrackType_Video;

        // send CreateTrack event
		onvif_CreateTrackNotify(p_recording, p_track);
	}
	
	p_track = onvif_add_Track(&p_recording->Recording.Tracks);
	if (p_track)
	{
		strcpy(p_track->Track.TrackToken, "AUDIO001");
		p_track->Track.Configuration.TrackType = TrackType_Audio;

		// send CreateTrack event
		onvif_CreateTrackNotify(p_recording, p_track);
	}
	
	p_track = onvif_add_Track(&p_recording->Recording.Tracks);
	if (p_track)
	{
		strcpy(p_track->Track.TrackToken, "META001");
		p_track->Track.Configuration.TrackType = TrackType_Metadata;

		// send CreateTrack event
		onvif_CreateTrackNotify(p_recording, p_track);
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  delete a recording object. Whenever a recording is deleted, 
 *  the device shall delete all the tracks that are part of the 
 *  recording, and it shall delete all the Recording Jobs that 
 *  record into the recording. For each deleted recording job, 
 *  the device shall also delete all the receiver objects associated 
 *  with the recording jobthat are automatically created using 
 *  the AutoCreateReceiver field of the recording job configuration 
 *  structure and are not used in any other recording job.
 *
 *  This method is optional. It shall be available if the 
 *  Recording/DynamicRecordings capability is TRUE.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecording,
 *  ONVIF_ERR_CannotDelete,
 */
ONVIF_RET onvif_trc_DeleteRecording(const char * p_RecordingToken)
{
	TrackList	* p_track;	
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}

    // todo : add delete recording code ...

    p_track = p_recording->Recording.Tracks;
    while (p_track)
    {
        // send DeleteTrack event
    	onvif_DeleteTrackNotify(p_recording, p_track);

    	p_track = p_track->next;
	}

    // send DeleteRecording event
    onvif_DeleteRecordingNotify(p_recording);	

	onvif_free_Recording(&g_onvif_cfg.recordings, p_recording);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Change the configuration of a recording
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_BadConfiguration,
 *  ONVIF_ERR_NoRecording,
 */
ONVIF_RET onvif_trc_SetRecordingConfiguration(trc_SetRecordingConfiguration_REQ * p_req)
{
    RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->RecordingToken);
    if (NULL == p_recording)
    {
        return ONVIF_ERR_NoRecording;
    }

    // todo : add set recording configuration code ...

    memcpy(&p_recording->Recording.Configuration, &p_req->RecordingConfiguration, sizeof(onvif_RecordingConfiguration));

    // send RecordingConfiguration event

    onvif_RecordingConfigurationNotify(p_recording);

    return ONVIF_OK;
}

/**
 * @brief
 *  Create a new track within a recording if the method GetRecordingOptions 
 *  signals spare tracks for the recording. For a track to be created the 
 *  SpareXXX (where XXX is the track type) needs to be set.
 *
 *  This method is optional. It shall be available if the 
 *  Recording/DynamicTracks capability is TRUE.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_BadConfiguration,
 *  ONVIF_ERR_NoRecording,
 *  ONVIF_ERR_MaxTracks
 */
ONVIF_RET onvif_trc_CreateTrack(trc_CreateTrack_REQ * p_req)
{
	TrackList * p_track;	
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}
	
	p_track = onvif_add_Track(&p_recording->Recording.Tracks);
	if (NULL == p_track)
	{
		return ONVIF_ERR_MaxTracks;
	}

	memcpy(&p_track->Track.Configuration, &p_req->TrackConfiguration, sizeof(onvif_TrackConfiguration));
	strcpy(p_req->TrackToken, p_track->Track.TrackToken);

	// send CreateTrack event    
	onvif_CreateTrackNotify(p_recording, p_track);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Remove a track from a recording. All the data in the track shall be deleted.
 *
 *  This method is optional. It shall be available if the 
 *  Recording/DynamicTracks capability is TRUE.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecording,
 *  ONVIF_ERR_NoTrack,
 *  ONVIF_ERR_CannotDelete
 */
ONVIF_RET onvif_trc_DeleteTrack(trc_DeleteTrack_REQ * p_req)
{
	TrackList * p_track;	
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}

	p_track = onvif_find_Track(p_recording->Recording.Tracks, p_req->TrackToken);
	if (NULL == p_track)
	{
		return ONVIF_ERR_NoTrack;
	}

	// todo : add delete track code ...


    // send DeleteTrack event    
	onvif_DeleteTrackNotify(p_recording, p_track);
	
	onvif_free_Track(&p_recording->Recording.Tracks, p_track);
	
	return ONVIF_OK;
}

/**
 * @brief 
 *  change the configuration of a track. TrackType shall be 
 *  ignored by the device as it can't be changed
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecording,
 *  ONVIF_ERR_NoTrack,
 *  ONVIF_ERR_BadConfiguration
 */
ONVIF_RET onvif_trc_SetTrackConfiguration(trc_SetTrackConfiguration_REQ * p_req)
{
	TrackList * p_track;
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}

	p_track = onvif_find_Track(p_recording->Recording.Tracks, p_req->TrackToken);
	if (NULL == p_track)
	{
		return ONVIF_ERR_NoTrack;
	}

	// todo : add set track configuration code ...

	memcpy(&p_track->Track.Configuration, &p_req->TrackConfiguration, sizeof(onvif_TrackConfiguration));

    // send TrackConfiguration event
    
	onvif_TrackConfigurationNotify(p_recording, p_track);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Create a new recording job. A device shall support adding a 
 *  RecordingJob to a recording for which it signals Spare jobs 
 *  via GetRecordingOptions.
 *
 *  A device should reject a configuration that neither includes 
 *  a source with a source token nor AutoCreateReceiver set to true.
 *
 *  If the configuration doesn not include any tracks a device should 
 *  assign all tracks of the corresponding recording.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_MaxRecordingJobs,
 *  ONVIF_ERR_BadConfiguration,
 *  ONVIF_ERR_MaxReceivers
 *  ONVIF_ERR_NoRecording
 */
ONVIF_RET onvif_trc_CreateRecordingJob(trc_CreateRecordingJob_REQ  * p_req)
{
	uint32 i = 0;
	RecordingList * p_recording;	
	RecordingJobList * p_tmp;
	RecordingJobList * p_recordingjob;

	p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->JobConfiguration.RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}
	
	p_recordingjob = onvif_add_RecordingJob(&g_onvif_cfg.recording_jobs);
	if (NULL == p_recordingjob)
	{
		return ONVIF_ERR_MaxRecordingJobs;
	}

    memcpy(&p_recordingjob->RecordingJob.JobConfiguration, &p_req->JobConfiguration, sizeof(onvif_RecordingJobConfiguration));
	
	strcpy(p_req->JobToken, p_recordingjob->RecordingJob.JobToken);
	
	// auto create recording source
	for (i = 0; i < p_req->JobConfiguration.sizeSource; i++)
	{
		if (p_req->JobConfiguration.Source[i].AutoCreateReceiverFlag && p_req->JobConfiguration.Source[i].AutoCreateReceiver)
		{
			p_req->JobConfiguration.Source[i].SourceTokenFlag = 1;
			p_req->JobConfiguration.Source[i].SourceToken.TypeFlag = 1;
			strcpy(p_req->JobConfiguration.Source[i].SourceToken.Type, "http://www.onvif.org/ver10/schema/Profile");

			if (g_onvif_cfg.profiles)
			{
				strcpy(p_req->JobConfiguration.Source[i].SourceToken.Token, g_onvif_cfg.profiles->token);
			}
		}
		else if (p_req->JobConfiguration.Source[i].SourceTokenFlag)
		{
			if (strcmp(p_req->JobConfiguration.Source[i].SourceToken.Type, "http://www.onvif.org/ver10/schema/Profile") == 0)
			{
				if (p_req->JobConfiguration.Source[i].SourceToken.Token[0] == '\0')
				{
					if (g_onvif_cfg.profiles)
					{
						strcpy(p_req->JobConfiguration.Source[i].SourceToken.Token, g_onvif_cfg.profiles->token);
					}
				}
			}
		}
	}

    // Adjust the status according to priority
    
    if (strcmp(p_recordingjob->RecordingJob.JobConfiguration.Mode, "Active") == 0)
    {
        // If the Mode is Active, determine whether to start recording based on priority
        
        p_tmp = g_onvif_cfg.recording_jobs;
        while (p_tmp)
        {
            if (p_tmp != p_recordingjob)
            {
                if (strcmp(p_tmp->RecordingJob.JobConfiguration.Mode, "Active") == 0)
                {
                    if (p_tmp->RecordingJob.JobConfiguration.Priority <= p_recordingjob->RecordingJob.JobConfiguration.Priority)
                    {
                        strcpy(p_tmp->RecordingJob.JobConfiguration.Mode, "Idle");

                        onvif_RecordingJobStateNotify(p_tmp, PropertyOperation_Changed);
                    }
                    else
                    {
                        strcpy(p_recordingjob->RecordingJob.JobConfiguration.Mode, "Idle");
                    }
                }
            }
            
            p_tmp = p_tmp->next;
        }
    }
    else
    {
        // todo : if the Mode is Idle, it need to stop recording
        
    }

	// if job state changed, send job state event notify
	onvif_RecordingJobStateNotify(p_recordingjob, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Removes a recording job. It shall also implicitly delete all 
 *  the receiver objects associated with the recording job that 
 *  are automatically created using the AutoCreateReceiver field 
 *  of the recording job configuration structure and are not used 
 *  in any other recording job.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecordingJob
 */
ONVIF_RET onvif_trc_DeleteRecordingJob(const char * p_JobToken)
{
    RecordingJobList * p_highPriority = NULL;
	RecordingJobList * p_recordingjob = onvif_find_RecordingJob(g_onvif_cfg.recording_jobs, p_JobToken);
	if (NULL == p_recordingjob)
	{
		return ONVIF_ERR_NoRecordingJob;
	}

	// todo : add delete recording job code ...


    // Adjust the status according to priority
    if (strcmp(p_recordingjob->RecordingJob.JobConfiguration.Mode, "Active") == 0)
    {
        RecordingJobList * p_tmp = g_onvif_cfg.recording_jobs;
        while (p_tmp)
        {
            if (p_tmp != p_recordingjob)
            {
                if (NULL == p_highPriority)
                {
                    p_highPriority = p_tmp;
                }
                else if (p_highPriority->RecordingJob.JobConfiguration.Priority <= 
                    p_tmp->RecordingJob.JobConfiguration.Priority)
                {
                    p_highPriority = p_tmp;
                }
            }
            
            p_tmp = p_tmp->next;
        }

        if (p_highPriority)
        {
            strcpy(p_highPriority->RecordingJob.JobConfiguration.Mode, "Active");

            // todo : start recording ...
            
            onvif_RecordingJobStateNotify(p_highPriority, PropertyOperation_Changed);
        }
    }

	onvif_free_RecordingJob(&g_onvif_cfg.recording_jobs, p_recordingjob);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Change the configuration for a recording job. A device shall 
 *  reject a request that tries to modify the RecordingToken.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecordingJob
 *  ONVIF_ERR_BadConfiguration
 *  ONVIF_ERR_MaxReceivers
 */
ONVIF_RET onvif_trc_SetRecordingJobConfiguration(trc_SetRecordingJobConfiguration_REQ * p_req)
{
	uint32 i;
	
	RecordingList * p_recording;
	RecordingJobList * p_recordingjob = onvif_find_RecordingJob(g_onvif_cfg.recording_jobs, p_req->JobToken);
	if (NULL == p_recordingjob)
	{
		return ONVIF_ERR_NoRecordingJob;
	}

	p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->JobConfiguration.RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_BadConfiguration;
	}

	// auto create recording source
	for (i = 0; i < p_req->JobConfiguration.sizeSource; i++)
	{
		if (p_req->JobConfiguration.Source[i].AutoCreateReceiverFlag && 
		    p_req->JobConfiguration.Source[i].AutoCreateReceiver)
		{
			p_req->JobConfiguration.Source[i].SourceTokenFlag = 1;
			p_req->JobConfiguration.Source[i].SourceToken.TypeFlag = 1;
			strcpy(p_req->JobConfiguration.Source[i].SourceToken.Type, "http://www.onvif.org/ver10/schema/Profile");

			if (g_onvif_cfg.profiles)
			{
				strcpy(p_req->JobConfiguration.Source[i].SourceToken.Token, g_onvif_cfg.profiles->token);
			}
		}
	}
	
	memcpy(&p_recordingjob->RecordingJob.JobConfiguration, &p_req->JobConfiguration, sizeof(onvif_RecordingJobConfiguration));

	// send RecordingJobConfiguration event

    onvif_RecordingJobConfigurationNotify(p_recordingjob);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Change the mode of the recording job. Using this method shall be 
 *  equivalent to retrieving the recording job configuration, 
 *  and writing it back with a different mode.
 *
 *  Note that the state of a recording job will only become active 
 *  if the recording job has the highest priority of all active jobs 
 *  of a recording.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecordingJob
 *  ONVIF_ERR_BadMode
 */
ONVIF_RET onvif_trc_SetRecordingJobMode(trc_SetRecordingJobMode_REQ * p_req)
{
	RecordingJobList * p_recordingjob = onvif_find_RecordingJob(g_onvif_cfg.recording_jobs, p_req->JobToken);
	if (NULL == p_recordingjob)
	{
		return ONVIF_ERR_NoRecordingJob;
	}

	if (strcmp(p_req->Mode, "Idle") && strcmp(p_req->Mode, "Active"))
	{
		return ONVIF_ERR_BadMode;
	}

    if (strcmp(p_req->Mode, p_recordingjob->RecordingJob.JobConfiguration.Mode) == 0)
    {
        return ONVIF_OK;
    }

	// todo : add set recording job mode code ...

	strcpy(p_recordingjob->RecordingJob.JobConfiguration.Mode, p_req->Mode);

    // If the Mode is Active, determine whether to start recording based on priority
    
    // if job state changed, send job state event notify
	onvif_RecordingJobStateNotify(p_recordingjob, PropertyOperation_Changed);
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Returns the state of a recording job. It includes an aggregated state, 
 *  and state for each track of the recording job. The RecordingJogState 
 *  may change due to
 *  calls that effect the RecordingJobMode, e.g. SetRecordingJobMode,
 *  internal recording engine state changes,
 *  changes in the recorded local media profile or
 *  changes to the RTSP connection defined by the associated Receiver.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecordingJob
 */
ONVIF_RET onvif_trc_GetRecordingJobState(const char * p_JobToken, onvif_RecordingJobStateInformation * p_res)
{
	uint32 i, j;
	RecordingJobList * p_recordingjob = onvif_find_RecordingJob(g_onvif_cfg.recording_jobs, p_JobToken);
	if (NULL == p_recordingjob)
	{
		return ONVIF_ERR_NoRecordingJob;
	}

	strcpy(p_res->RecordingToken, p_recordingjob->RecordingJob.JobConfiguration.RecordingToken);
	strcpy(p_res->State, p_recordingjob->RecordingJob.JobConfiguration.Mode);

	p_res->sizeSources = p_recordingjob->RecordingJob.JobConfiguration.sizeSource;
	
	for (i = 0; i < p_recordingjob->RecordingJob.JobConfiguration.sizeSource; i++)
	{
		p_res->Sources[i].SourceToken.TypeFlag = 1;
		strcpy(p_res->Sources[i].SourceToken.Token, p_recordingjob->RecordingJob.JobConfiguration.Source[i].SourceToken.Token);
		strcpy(p_res->Sources[i].SourceToken.Type, p_recordingjob->RecordingJob.JobConfiguration.Source[i].SourceToken.Type);

		strcpy(p_res->Sources[i].State, p_recordingjob->RecordingJob.JobConfiguration.Mode);

		p_res->Sources[i].sizeTrack = p_recordingjob->RecordingJob.JobConfiguration.Source[i].sizeTracks;
		
		for (j = 0; j < p_recordingjob->RecordingJob.JobConfiguration.Source[i].sizeTracks; j++)
		{
			strcpy(p_res->Sources[i].Track[j].SourceTag, p_recordingjob->RecordingJob.JobConfiguration.Source[i].Tracks[j].SourceTag);
			strcpy(p_res->Sources[i].Track[j].Destination, p_recordingjob->RecordingJob.JobConfiguration.Source[i].Tracks[j].Destination);
			strcpy(p_res->Sources[i].Track[j].State, p_recordingjob->RecordingJob.JobConfiguration.Mode);
		}
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  Returns information for a recording identified by the RecordingToken. 
 *  The information includes the number of additional tracks as well as 
 *  recording jobs that can be configured.
 *  This method shall be supported if the Options support is signaled 
 *  via the capabilities.
 *  Note that this information is not static and is only guaranteed to 
 *  be valid until the next modification of any recording jobs or tracks.
 *  The track options shall be supported if the device signals support 
 *  for dynamic tracks.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecording
 */
ONVIF_RET onvif_trc_GetRecordingOptions(const char * p_RecordingToken, onvif_RecordingOptions * p_res)
{
	int video, audio, meta;
	ONVIF_PROFILE * p_profile;
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}

	video = onvif_get_track_nums_by_type(p_recording->Recording.Tracks, TrackType_Video);
	audio = onvif_get_track_nums_by_type(p_recording->Recording.Tracks, TrackType_Audio);
	meta  = onvif_get_track_nums_by_type(p_recording->Recording.Tracks, TrackType_Metadata);

	p_res->Track.SpareVideoFlag = 1;
	p_res->Track.SpareVideo = 2 - video;	// max support two video track
	p_res->Track.SpareAudioFlag = 1;
	p_res->Track.SpareAudio = 2 - audio;	// max support two audio track
	p_res->Track.SpareMetadataFlag = 1;
	p_res->Track.SpareMetadata = 1 - meta;	// max support one metadata track
	p_res->Track.SpareTotalFlag = 1;
	p_res->Track.SpareTotal = 2 - video + 2 - audio + 1 - meta;
	
	p_res->Job.SpareFlag = 1;
	p_res->Job.Spare = 2;
	p_res->Job.CompatibleSourcesFlag = 1;

	p_profile = g_onvif_cfg.profiles;
	while (p_profile)
	{
	    strcat(p_res->Job.CompatibleSources, p_profile->token);
	    strcat(p_res->Job.CompatibleSources, " ");

		p_profile = p_profile->next;
	}
	
	return ONVIF_OK;
}

/**
 * @brief 
 *  Exports the selected recordings to the given storage target
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_BadConfiguration
 */
ONVIF_RET onvif_trc_ExportRecordedData(trc_ExportRecordedData_REQ * p_req, trc_ExportRecordedData_RES * p_res)
{
    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief 
 *  Stops the ExportRecordedData operation that is started before. 
 *  The response message lists the status of the exported files.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_BadConfiguration
 */
ONVIF_RET onvif_trc_StopExportRecordedData(trc_StopExportRecordedData_REQ * p_req, trc_StopExportRecordedData_RES * p_res)
{
    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Returns the status of export operations. This interface allows 
 *  client to poll the status information from the device.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_BadConfiguration
 */
ONVIF_RET onvif_trc_GetExportRecordedDataState(trc_GetExportRecordedDataState_REQ * p_req, trc_GetExportRecordedDataState_RES * p_res)
{
    // todo : here add handler code ...
    
    return ONVIF_OK;
}

/**
 * @brief 
 *  Get a summary description of all recorded data
 *
 * @return 
 *  The possible return values:
 *	ONVIF_OK
 */
ONVIF_RET onvif_tse_GetRecordingSummary(tse_GetRecordingSummary_RES * p_summary)
{
    int cnt = 0;
    RecordingList * p_recording;
    
    p_recording = g_onvif_cfg.recordings;
    while (p_recording)
    {
        if (p_summary->Summary.DataFrom == 0 || 
            p_summary->Summary.DataFrom > p_recording->EarliestRecording)
        {
            p_summary->Summary.DataFrom = p_recording->EarliestRecording;
        }

        if (p_summary->Summary.DataUntil == 0 || 
            p_summary->Summary.DataUntil < p_recording->LatestRecording)
        {
            p_summary->Summary.DataUntil = p_recording->LatestRecording;
        }
        
        cnt++;
        
        p_recording = p_recording->next;
    }
    
	p_summary->Summary.NumberRecordings = cnt;

	return ONVIF_OK;
}

/**
 * @brief
 *  Returns information about a single Recording specified by a RecordingToken
 *
 * @return 
 *  The possible return values:
 *  ONVIF_ERR_InvalidToken
 */
ONVIF_RET onvif_tse_GetRecordingInformation(tse_GetRecordingInformation_REQ * p_req, tse_GetRecordingInformation_RES * p_res)
{
	TrackList * p_track;
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_InvalidToken;
	}

	// todo : fill the recording information ...
	strcpy(p_res->RecordingInformation.RecordingToken, p_recording->Recording.RecordingToken);
	memcpy(&p_res->RecordingInformation.Source, &p_recording->Recording.Configuration.Source, sizeof(onvif_RecordingSourceInformation));
	p_res->RecordingInformation.EarliestRecordingFlag = 1;
	p_res->RecordingInformation.EarliestRecording = p_recording->EarliestRecording;
	p_res->RecordingInformation.LatestRecordingFlag = 1;
	p_res->RecordingInformation.LatestRecording = p_recording->LatestRecording;

	strcpy(p_res->RecordingInformation.Content, p_recording->Recording.Configuration.Content);

	p_track = p_recording->Recording.Tracks;
	while (p_track)
	{
		int index = p_res->RecordingInformation.sizeTrack;
		
		strcpy(p_res->RecordingInformation.Track[index].TrackToken, p_track->Track.TrackToken);
		p_res->RecordingInformation.Track[index].TrackType = p_track->Track.Configuration.TrackType;
		strcpy(p_res->RecordingInformation.Track[index].Description, p_track->Track.Configuration.Description);
		p_res->RecordingInformation.Track[index].DataFrom = p_track->EarliestRecording;
		p_res->RecordingInformation.Track[index].DataTo = p_track->LatestRecording;

		p_track = p_track->next;
		p_res->RecordingInformation.sizeTrack++;

		if (p_res->RecordingInformation.sizeTrack >= 5)
		{
			break;
		}
	}

	p_res->RecordingInformation.RecordingStatus = p_recording->RecordingStatus;
	
	return ONVIF_OK;
}

void onvif_fillMediaAttributes(RecordingList * p_recording, onvif_MediaAttributes * p_res)
{
    int i;
    TrackList * p_track;
    
    strcpy(p_res->RecordingToken, p_recording->Recording.RecordingToken);

	p_res->From = p_recording->EarliestRecording;
	p_res->Until = p_recording->LatestRecording;

	p_track = p_recording->Recording.Tracks;
	while (p_track)
	{
		i = p_res->sizeTrackAttributes;
		
		strcpy(p_res->TrackAttributes[i].TrackInformation.TrackToken, p_track->Track.TrackToken);
		p_res->TrackAttributes[i].TrackInformation.TrackType = p_track->Track.Configuration.TrackType;
		strcpy(p_res->TrackAttributes[i].TrackInformation.Description, p_track->Track.Configuration.Description);
		p_res->TrackAttributes[i].TrackInformation.DataFrom = p_recording->EarliestRecording;
		p_res->TrackAttributes[i].TrackInformation.DataTo = p_recording->LatestRecording;

		// todo : fill video, audio, metadata information ...

		if (p_track->Track.Configuration.TrackType == TrackType_Metadata)
		{
		    p_res->TrackAttributes[i].MetadataAttributesFlag = 1;
		    p_res->TrackAttributes[i].MetadataAttributes.PtzSpacesFlag = 1;
		    p_res->TrackAttributes[i].MetadataAttributes.CanContainPTZ = 1;
		    p_res->TrackAttributes[i].MetadataAttributes.CanContainAnalytics = 1;
		    p_res->TrackAttributes[i].MetadataAttributes.CanContainNotifications = 1;
		    strcpy(p_res->TrackAttributes[i].MetadataAttributes.PtzSpaces, 
		        "http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace "
		        "http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace");
		}
		
		p_track = p_track->next;

		p_res->sizeTrackAttributes++;
		if (p_res->sizeTrackAttributes >= ARRAY_SIZE(p_res->TrackAttributes))
		{
			break;
		}
	}
}

/**
 * @brief 
 *  Returns a set of media attributes for all tracks of the 
 *  specified recordings at a specified point in time
 *
 * @return 
 *  The possible return values:
 *	ONVIF_ERR_InvalidToken
 *	ONVIF_ERR_NoRecording
 */
ONVIF_RET onvif_tse_GetMediaAttributes(tse_GetMediaAttributes_REQ * p_req, tse_GetMediaAttributes_RES * p_res)
{
	int i, idx;	
	RecordingList * p_recording;

	if (p_req->sizeRecordingTokens > 0)
	{
	    // A list of references to the recordings to query
	    
    	for (i = 0; i < p_req->sizeRecordingTokens; i++)
    	{
    		p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->RecordingTokens[i]);
    		if (NULL == p_recording)
    		{
    			return ONVIF_ERR_NoRecording;
    		}

            idx = p_res->sizeMediaAttributes;
            
            onvif_fillMediaAttributes(p_recording, &p_res->MediaAttributes[idx]);

            p_res->sizeMediaAttributes++;
            if (p_res->sizeMediaAttributes >= ARRAY_SIZE(p_res->MediaAttributes))
            {
                break;
            }
    	}
	}
	else
	{
	    // If no recording tokens are provided all recordings should be queried

	    p_recording = g_onvif_cfg.recordings;
	    while (p_recording)
	    {
	        if (p_req->Time < p_recording->EarliestRecording || 
	            p_req->Time > p_recording->LatestRecording)
	        {
	            p_recording = p_recording->next;
	            continue;
	        }

	        idx = p_res->sizeMediaAttributes;
            
            onvif_fillMediaAttributes(p_recording, &p_res->MediaAttributes[idx]);

            p_res->sizeMediaAttributes++;
            if (p_res->sizeMediaAttributes >= ARRAY_SIZE(p_res->MediaAttributes))
            {
                break;
            }

	        p_recording = p_recording->next;
	    }
	}
	
	return ONVIF_OK;
}

/*****************************************************************************/

BOOL onvif_AddSearch(SEARCH_SUA * p_sua)
{
    BOOL ret = FALSE;
    
    if (NULL == g_onvif_cls.search_list)
    {
        g_onvif_cls.search_list = h_list_create(TRUE);
    }

    if (g_onvif_cls.search_list)
    {
        ret = h_list_add_at_back(g_onvif_cls.search_list, p_sua);
    }

    return ret;
}

SEARCH_SUA * onvif_FindSearch(const char * token)
{
    SEARCH_SUA * p_sua = NULL;
    SEARCH_SUA * p_tmp = NULL;
    
    LINKED_NODE * p_node = h_list_lookup_start(g_onvif_cls.search_list);
    while (p_node)
    {
        p_tmp = (SEARCH_SUA *)p_node->p_data;
        if (p_tmp && strcmp(p_tmp->token, token) == 0)
        {
            p_sua = p_tmp;
            break;
        }
        
        p_node = h_list_lookup_next(g_onvif_cls.search_list, p_node);
    }
    h_list_lookup_end(g_onvif_cls.search_list);

    return p_sua;
}

void onvif_FreeSearch(SEARCH_SUA * p_sua)
{
    if (p_sua->req)
    {
        free(p_sua->req);
    }
    
    h_list_remove_data(g_onvif_cls.search_list, p_sua);

    free(p_sua);
}

void onvif_FreeSearchs()
{
    SEARCH_SUA * p_sua = NULL;
    
    LINKED_NODE * p_node = h_list_lookup_start(g_onvif_cls.search_list);
    while (p_node)
    {
        p_sua = (SEARCH_SUA *)p_node->p_data;
        if (p_sua && p_sua->req)
        {
            free(p_sua->req);
        }
        
        p_node = h_list_lookup_next(g_onvif_cls.search_list, p_node);
    }
    h_list_lookup_end(g_onvif_cls.search_list);

    h_list_free_container(g_onvif_cls.search_list);
    g_onvif_cls.search_list = NULL;
}

/**
 * @brief
 *  is called from onvif_timer
 */
void onvif_SearchTimeoutCheck()
{
    time_t curtime = time(NULL);
    SEARCH_SUA * p_sua = NULL;
    
    LINKED_NODE * p_node = h_list_lookup_start(g_onvif_cls.search_list);
    while (p_node)
    {
        int KeepAliveTime = 0;
        
        p_sua = (SEARCH_SUA *)p_node->p_data;
        if (p_sua && p_sua->req)
        {
            switch (p_sua->type)
            {
            case SEARCH_TYPE_EVENTS:
                KeepAliveTime = ((tse_FindEvents_REQ *)p_sua->req)->KeepAliveTime;
                break;

            case SEARCH_TYPE_RECORDING:
                KeepAliveTime = ((tse_FindRecordings_REQ *)p_sua->req)->KeepAliveTime;
                break;

            case SEARCH_TYPE_PTZPOS:
                KeepAliveTime = ((tse_FindPTZPosition_REQ *)p_sua->req)->KeepAliveTime;
                break;

            case SEARCH_TYPE_METADATA:
                KeepAliveTime = ((tse_FindMetadata_REQ *)p_sua->req)->KeepAliveTime;
                break;    
            }

            if (KeepAliveTime > 0 && curtime - p_sua->reqtime >= KeepAliveTime)
            {
                LINKED_NODE * p_next = h_list_lookup_next(g_onvif_cls.search_list, p_node);

                free(p_sua->req);
                
                h_list_remove(g_onvif_cls.search_list, p_node);

                free(p_sua);

                p_node = p_next;
            }
        }
        
        p_node = h_list_lookup_next(g_onvif_cls.search_list, p_node);
    }
    h_list_lookup_end(g_onvif_cls.search_list);
}

BOOL onvif_MetadataStreamFilterEx(HQUEUE * queue, onvif_FindMetadataResult * p_item)
{
	char buff[128];	
	char name[128];
	char value[128] = {'\0'};

	if (hqBufGet(queue, buff) == FALSE)
	{
		return FALSE;
	}

	if (strcasecmp(buff, "track") != 0)
	{
		return FALSE;
	}
	
	if (hqBufGet(queue, buff) == FALSE) 
	{
		return FALSE;
	}
	strcpy(name, buff);

    if (hqBufGet(queue, buff) == FALSE) 
	{
		return FALSE;
	}
	strcpy(value, buff);

    if (strcasecmp(name, "tracktoken") == 0)
    {
        if (strcmp(p_item->TrackToken, value) == 0)
        {
            return TRUE;
        }
    }
    
	return FALSE;
}

BOOL onvif_MetadataStreamFilter(char * filter, onvif_FindMetadataResult * p_item)
{
    BOOL notflag = FALSE;
    char buff[128];
    BOOL ret = FALSE;
    int  flag = 0;
        
    HQUEUE * queue = onvif_xpath_parse(filter);
    if (NULL == queue)
    {
        return FALSE;
    }

    while (!hqBufIsEmpty(queue))
    {
        hqBufGet(queue, buff);

        if (strcmp(buff, "not") == 0)
        {
            notflag = TRUE;            
        }
        else if (strcmp(buff, "boolean") == 0)
        {
            BOOL ret1 = onvif_MetadataStreamFilterEx(queue, p_item);
			if (notflag)
			{
				ret1 = !ret1;
				notflag = FALSE;
			}

			if (flag == 1)
			{
			    ret = ret && ret1;
			}
			else if (flag == 2)
			{
			    ret = ret || ret1;
			}
			else
			{
			    ret = ret1;
			}
        }
		else if (strcmp(buff, "and") == 0)
		{
			flag = 1;
		}
		else if (strcmp(buff, "or") == 0)
		{
			flag = 2;
		}
    }
    
    hqDelete(queue);
    
    return ret;
}

/*****************************************************************************/

/**
 * @brief 
 *  starts a search session, looking for recordings that match the 
 *  scope (See 5.2.4) defined in the request. Results from the search 
 *  are acquired using the GetRecordingSearchResults request, 
 *  specifying the search token returned from this request
 *  
 *  The device shall continue searching until one of the following occurs:
 *  The total number of matches has been found, defined by the MaxMatches parameter
 *  The session has been ended by a client EndSearch request
 *  The session has been ended because KeepAliveTime since the last 
 *  request related to this session has expired.
 *
 *  The order of the results is undefined, to allow the device to return 
 *  results in any order they are found. This operation is mandatory to 
 *  support for a device implementing the recording search service.
 *  For the KeepAliveTime a device shall support at least values up to 
 *  ten seconds. A device may adapt larger values.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidToken
 *  ONVIF_ERR_InvalidSource
 *  ONVIF_ERR_ResourceProblem
 */
ONVIF_RET onvif_tse_FindRecordings(tse_FindRecordings_REQ * p_req, tse_FindRecordings_RES * p_res)
{
    SEARCH_SUA * p_sua = (SEARCH_SUA *)malloc(sizeof(SEARCH_SUA));
    if (p_sua)
    {
        p_sua->req = malloc(sizeof(tse_FindRecordings_REQ));
        if (p_sua->req)
        {
            memcpy(p_sua->req, p_req, sizeof(tse_FindRecordings_REQ));
        }

        p_sua->type = SEARCH_TYPE_RECORDING;
        p_sua->reqtime = time(NULL);
        sprintf(p_sua->token, "SearchToken_%u", ++g_onvif_idx.search_idx);
        strcpy(p_res->SearchToken, p_sua->token);
        
        if (!onvif_AddSearch(p_sua))
        {
            if (p_sua->req)
            {
                free(p_sua->req);
            }
            
            free(p_sua);
        }
    }
    
	return ONVIF_OK;
}

/**
 * @brief
 *  acquires the results from a recording search session previously 
 *  initiated by a FindRecordings operation. The response shall not 
 *  include results already returned in previous requests for the
 *  same session. If MaxResults is specified, the response shall not 
 *  contain more than MaxResults results. The number of results relates 
 *  to the number of recordings. For viewing individual recorded data 
 *  for a signal track use the FindEvents method.
 *
 * @return 
 *  The possible return values:
 *  ONVIF_ERR_InvalidToken
 */
ONVIF_RET onvif_tse_GetRecordingSearchResults(tse_GetRecordingSearchResults_REQ * p_req, tse_GetRecordingSearchResults_RES * p_res)
{
    int count = 0;
    TrackList * p_track;
    RecordingList * p_recording;
    RecordingInformationList * p_recinf;
    SEARCH_SUA * p_sua = onvif_FindSearch(p_req->SearchToken);
    if (NULL == p_sua)
    {
        return ONVIF_ERR_InvalidToken;
    }

    if (p_sua->type != SEARCH_TYPE_RECORDING)
    {
        return ONVIF_ERR_InvalidToken;
    }
    
    p_res->ResultList.SearchState = SearchState_Completed;

    // the following code is just for example ...
    
    p_recording = g_onvif_cfg.recordings;
    while (p_recording)
    {
        if (p_req->MaxResultsFlag && count >= p_req->MaxResults)
        {
            break;
        }

        // add a recording to the result
        p_recinf = onvif_add_RecordingInformation(&p_res->ResultList.RecordInformation);
        if (p_recinf)
        {
            // todo : fill the recording information ...
        	strcpy(p_recinf->RecordingInformation.RecordingToken, p_recording->Recording.RecordingToken);
        	memcpy(&p_recinf->RecordingInformation.Source, &p_recording->Recording.Configuration.Source, sizeof(onvif_RecordingSourceInformation));
        	p_recinf->RecordingInformation.EarliestRecordingFlag = 1;
        	p_recinf->RecordingInformation.EarliestRecording = p_recording->EarliestRecording;
        	p_recinf->RecordingInformation.LatestRecordingFlag = 1;
        	p_recinf->RecordingInformation.LatestRecording = p_recording->LatestRecording;
        	p_recinf->RecordingInformation.RecordingStatus = p_recording->RecordingStatus;

        	strcpy(p_recinf->RecordingInformation.Content, p_recording->Recording.Configuration.Content);

        	p_track = p_recording->Recording.Tracks;
        	while (p_track)
        	{
        		int index = p_recinf->RecordingInformation.sizeTrack;
        		
        		strcpy(p_recinf->RecordingInformation.Track[index].TrackToken, p_track->Track.TrackToken);
        		p_recinf->RecordingInformation.Track[index].TrackType = p_track->Track.Configuration.TrackType;
        		strcpy(p_recinf->RecordingInformation.Track[index].Description, p_track->Track.Configuration.Description);
        		p_recinf->RecordingInformation.Track[index].DataFrom = p_track->EarliestRecording;
        		p_recinf->RecordingInformation.Track[index].DataTo = p_track->LatestRecording;

        		p_track = p_track->next;
        		p_recinf->RecordingInformation.sizeTrack++;

        		if (p_recinf->RecordingInformation.sizeTrack >= ARRAY_SIZE(p_recinf->RecordingInformation.Track))
        		{
        			break;
        		}
        	}
        }

        p_recording = p_recording->next;
    }
    
	return ONVIF_OK;
}

/**
 * @brief
 *  starts a search session, looking for events in the scope (See 5.2.4) 
 *  that match the search filter defined in the request. Events are recording 
 *  events (see 5.2.2) and other events that are available in the track.
 *  Results from the search are acquired using the GetEventSearchResults request, 
 *  specifying the search token returned from this request.
 *
 *  The device shall continue searching until one of the following occurs:
 *  The entire time range from StartPoint to EndPoint has been searched through.
 *  The total number of matches has been found, defined by the MaxMatches parameter.
 *  The session has been ended by a client EndSearch request.
 *  The session has been ended because KeepAliveTime since the last request 
 *  related to this session has expired.
 *
 *  Results shall be ordered by time, ascending in case of forward search, 
 *  or descending in case of backward search. This operation is mandatory 
 *  to support for a device implementing the recording search service. 
 *  Although the values of property events refer to the forward direction, 
 *  they shall be reported identically in reverse search mode.
 *
 *  For the KeepAliveTime a device shall support at least values up to 
 *  ten seconds. A device may adapt larger values.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidFilterFault
 *  ONVIF_ERR_ResourceProblem
 */
ONVIF_RET onvif_tse_FindEvents(tse_FindEvents_REQ * p_req, tse_FindEvents_RES * p_res)
{
    SEARCH_SUA * p_sua = (SEARCH_SUA *)malloc(sizeof(SEARCH_SUA));
    if (p_sua)
    {
        p_sua->req = malloc(sizeof(tse_FindEvents_REQ));
        if (p_sua->req)
        {
            memcpy(p_sua->req, p_req, sizeof(tse_FindEvents_REQ));
        }

        p_sua->type = SEARCH_TYPE_EVENTS;
        p_sua->reqtime = time(NULL);
        sprintf(p_sua->token, "SearchToken_%u", ++g_onvif_idx.search_idx);
        strcpy(p_res->SearchToken, p_sua->token);
        
        if (!onvif_AddSearch(p_sua))
        {
            if (p_sua->req)
            {
                free(p_sua->req);
            }
            
            free(p_sua);
        }
    }
    
	return ONVIF_OK;
}

/**
 * @brief
 *  acquires the results from a recording event search session 
 *  previously initiated by a FindEvents operation. The response 
 *  shall not include results already returned in previous requests 
 *  for the same session. If MaxResults is specified, the response 
 *  shall not contain more than MaxResults results.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidToken
 */
ONVIF_RET onvif_tse_GetEventSearchResults(tse_GetEventSearchResults_REQ * p_req, tse_GetEventSearchResults_RES * p_res)
{
    SEARCH_SUA * p_sua = onvif_FindSearch(p_req->SearchToken);
    if (NULL == p_sua)
    {
        return ONVIF_ERR_InvalidToken;
    }

    if (p_sua->type != SEARCH_TYPE_EVENTS)
    {
        return ONVIF_ERR_InvalidToken;
    }

    // todo : fill the result ...

    if (g_onvif_cfg.recordings)
    {
        int count = 0;
        BOOL state = TRUE;
        BOOL recording = TRUE;
        tse_FindEvents_REQ * p_findreq = (tse_FindEvents_REQ *) p_sua->req;
        time_t st, et;

        st = g_onvif_cfg.recordings->EarliestRecording;
        et = g_onvif_cfg.recordings->LatestRecording;
        
        if (p_findreq->EndPointFlag)
        {
            if (p_findreq->EndPoint < p_findreq->StartPoint)
            {
                // backword search
                
                recording = FALSE;

                st = g_onvif_cfg.recordings->LatestRecording;
                et = g_onvif_cfg.recordings->EarliestRecording;

                if (p_findreq->StartPoint < g_onvif_cfg.recordings->LatestRecording)
                {
                    st = p_findreq->StartPoint;
                }

                if (p_findreq->EndPoint > g_onvif_cfg.recordings->EarliestRecording)
                {
                    et = p_findreq->EndPoint;
                }
            }
            else
            {
                // forward search
                
                if (p_findreq->StartPoint > g_onvif_cfg.recordings->EarliestRecording)
                {
                    st = p_findreq->StartPoint;
                }

                if (p_findreq->EndPoint < g_onvif_cfg.recordings->LatestRecording)
                {
                    et = p_findreq->EndPoint;
                }
            }
        }
        else
        {
            // forward search
            
            if (p_findreq->StartPoint > g_onvif_cfg.recordings->EarliestRecording)
            {
                st = p_findreq->StartPoint;
            }
        }

        state = p_findreq->IncludeStartState;

        // todo : just for test ...
        
        FindEventResultList * p_item = onvif_add_FindEventResult(&p_res->ResultList.Result);
        if (p_item)
        {
            strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
            p_item->Result.Time = st;
            p_item->Result.Event.TopicFlag = 1;
            strcpy(p_item->Result.Event.Topic.Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSe");
            strcpy(p_item->Result.Event.Topic.Topic, "tns1:RecordingHistory/Recording/State");
            p_item->Result.StartStateEvent = state;

            onvif_init_Message(&p_item->Result.Event.Message, PropertyOperation_Changed,
                "RecordingToken", g_onvif_cfg.recordings->Recording.RecordingToken,
                NULL, NULL, "IsRecording", recording ? "true" : "false", NULL, NULL,
                NULL, NULL, NULL, NULL);

            p_item->Result.Event.Message.UtcTime = st;
            count++;
        }

        if (p_findreq->MaxMatchesFlag == 0 || count < p_findreq->MaxMatches)
        {
            TrackList * p_track = g_onvif_cfg.recordings->Recording.Tracks;
            while (p_track)
            {
                p_item = onvif_add_FindEventResult(&p_res->ResultList.Result);
                if (p_item)
                {
                    strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
                    strcpy(p_item->Result.TrackToken, p_track->Track.TrackToken);
                    p_item->Result.Time = st;
                    p_item->Result.Event.TopicFlag = 1;
                    strcpy(p_item->Result.Event.Topic.Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSe");
                    strcpy(p_item->Result.Event.Topic.Topic, "tns1:RecordingHistory/Track/State");
                    p_item->Result.StartStateEvent = state;

                    onvif_init_Message(&p_item->Result.Event.Message, PropertyOperation_Changed,
                        "RecordingToken", g_onvif_cfg.recordings->Recording.RecordingToken,
                        "Track", p_track->Track.TrackToken, 
                        "IsDataPresent", recording ? "true" : "false", NULL, NULL,
                        NULL, NULL, NULL, NULL);

                    p_item->Result.Event.Message.UtcTime = st; 
                    count++;
                }

                if (p_findreq->MaxMatchesFlag && count >= p_findreq->MaxMatches)
                {
                    break;
                }
            
                p_track = p_track->next;
            }
        }

        if (!p_findreq->IncludeStartState || 
            (p_findreq->EndPointFlag && p_findreq->EndPoint < p_findreq->StartPoint))
        {
            recording = !recording;
        
            if (p_findreq->MaxMatchesFlag == 0 || count < p_findreq->MaxMatches)
            {
                p_item = onvif_add_FindEventResult(&p_res->ResultList.Result);
                if (p_item)
                {
                    strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
                    p_item->Result.Time = et;
                    p_item->Result.Event.TopicFlag = 1;
                    strcpy(p_item->Result.Event.Topic.Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSe");
                    strcpy(p_item->Result.Event.Topic.Topic, "tns1:RecordingHistory/Recording/State");
                    p_item->Result.StartStateEvent = state;

                    onvif_init_Message(&p_item->Result.Event.Message, PropertyOperation_Changed,
                        "RecordingToken", g_onvif_cfg.recordings->Recording.RecordingToken,
                        NULL, NULL, "IsRecording", recording ? "true" : "false", NULL, NULL,
                        NULL, NULL, NULL, NULL);

                    p_item->Result.Event.Message.UtcTime = et;
                    count++;
                }
            }

            if (p_findreq->MaxMatchesFlag == 0 || count < p_findreq->MaxMatches)
            {
                TrackList * p_track = g_onvif_cfg.recordings->Recording.Tracks;
                while (p_track)
                {
                    p_item = onvif_add_FindEventResult(&p_res->ResultList.Result);
                    if (p_item)
                    {
                        strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
                        strcpy(p_item->Result.TrackToken, p_track->Track.TrackToken);
                        p_item->Result.Time = et;
                        p_item->Result.Event.TopicFlag = 1;
                        strcpy(p_item->Result.Event.Topic.Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSe");
                        strcpy(p_item->Result.Event.Topic.Topic, "tns1:RecordingHistory/Track/State");
                        p_item->Result.StartStateEvent = state;

                        onvif_init_Message(&p_item->Result.Event.Message, PropertyOperation_Changed,
                            "RecordingToken", g_onvif_cfg.recordings->Recording.RecordingToken,
                            "Track", p_track->Track.TrackToken, 
                            "IsDataPresent", recording ? "true" : "false", NULL, NULL,
                            NULL, NULL, NULL, NULL);

                        p_item->Result.Event.Message.UtcTime = et; 
                        count++;
                    }

                    if (p_findreq->MaxMatchesFlag && count >= p_findreq->MaxMatches)
                    {
                        break;
                    }
                
                    p_track = p_track->next;
                }
            }
        }
    }
    
    p_res->ResultList.SearchState = SearchState_Completed;
    
	return ONVIF_OK;
}

/**
 * @brief
 *  starts a search session, looking for metadata in the scope (See 5.2.4) 
 *  that matches the search filter defined in the request. Results 
 *  from the search are acquired using the GetMetadataSearchResults request, 
 *  specifying the search token returned from this request.
 *
 *  The device shall continue searching until one of the following occurs:
 *  The entire time range from StartPoint to EndPoint has been searched through.
 *  The total number of matches has been found, defined by the MaxMatches parameter.
 *  The session has been ended by a client EndSearch request.
 *  The session has been ended because KeepAliveTime since the last request 
 *  related to this session has expired.
 *
 *  This operation is mandatory to support if the MetaDataSearch capability 
 *  is set to true in the SearchCapabilities structure return by the 
 *  GetCapabilities command in the Device service.
 *
 *  For the KeepAliveTime a device shall support at least values up to ten seconds. 
 *  A device may adapt larger values.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_ResourceProblem
 */
ONVIF_RET onvif_tse_FindMetadata(tse_FindMetadata_REQ * p_req, tse_FindMetadata_RES * p_res)
{
    SEARCH_SUA * p_sua = (SEARCH_SUA *)malloc(sizeof(SEARCH_SUA));
    if (p_sua)
    {
        p_sua->req = malloc(sizeof(tse_FindMetadata_REQ));
        if (p_sua->req)
        {
            memcpy(p_sua->req, p_req, sizeof(tse_FindMetadata_REQ));
        }

        p_sua->type = SEARCH_TYPE_METADATA;
        p_sua->reqtime = time(NULL);
        sprintf(p_sua->token, "SearchToken_%u", ++g_onvif_idx.search_idx);
        strcpy(p_res->SearchToken, p_sua->token);
        
        if (!onvif_AddSearch(p_sua))
        {
            if (p_sua->req)
            {
                free(p_sua->req);
            }
            
            free(p_sua);
        }
    }
    
    return ONVIF_OK;
}

/**
 * @brief
 *  acquires the results from a recording search session previously 
 *  initiated by a FindMetadata operation. The response shall not 
 *  include results already returned in previous requests for the same
 *  session. If MaxResults is specified, the response shall not contain 
 *  more than MaxResults results.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidToken
 */
ONVIF_RET onvif_tse_GetMetadataSearchResults(tse_GetMetadataSearchResults_REQ * p_req, tse_GetMetadataSearchResults_RES * p_res)
{
    SEARCH_SUA * p_sua = onvif_FindSearch(p_req->SearchToken);
    if (NULL == p_sua)
    {
        return ONVIF_ERR_InvalidToken;
    }

    if (p_sua->type != SEARCH_TYPE_METADATA)
    {
        return ONVIF_ERR_InvalidToken;
    }

    // todo : fill the result ...

    if (g_onvif_cfg.recordings)
    {
        int count = 0;
        tse_FindMetadata_REQ * p_findreq = (tse_FindMetadata_REQ *) p_sua->req;
        time_t st, et;

        st = g_onvif_cfg.recordings->EarliestRecording;
        et = g_onvif_cfg.recordings->LatestRecording;
        
        if (p_findreq->EndPointFlag)
        {
            if (p_findreq->EndPoint < p_findreq->StartPoint)
            {
                // backword search

                st = g_onvif_cfg.recordings->LatestRecording;
                et = g_onvif_cfg.recordings->EarliestRecording;

                if (p_findreq->StartPoint < g_onvif_cfg.recordings->LatestRecording)
                {
                    st = p_findreq->StartPoint;
                }

                if (p_findreq->EndPoint > g_onvif_cfg.recordings->EarliestRecording)
                {
                    et = p_findreq->EndPoint;
                }
            }
            else
            {
                // forward search
                
                if (p_findreq->StartPoint > g_onvif_cfg.recordings->EarliestRecording)
                {
                    st = p_findreq->StartPoint;
                }

                if (p_findreq->EndPoint < g_onvif_cfg.recordings->LatestRecording)
                {
                    et = p_findreq->EndPoint;
                }
            }
        }
        else
        {
            // forward search
            
            if (p_findreq->StartPoint > g_onvif_cfg.recordings->EarliestRecording)
            {
                st = p_findreq->StartPoint;
            }
        }

        // todo : just for test ...

        FindMetadataResultList * p_item;

        if (p_findreq->MaxMatchesFlag == 0 || count < p_findreq->MaxMatches)
        {
            TrackList * p_track = g_onvif_cfg.recordings->Recording.Tracks;
            while (p_track)
            {
                if (p_track->Track.Configuration.TrackType == TrackType_Metadata)
                {
                    p_item = onvif_add_FindMetadataResult(&p_res->ResultList.Result);
                    if (p_item)
                    {
                        strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
                        strcpy(p_item->Result.TrackToken, p_track->Track.TrackToken);
                        p_item->Result.Time = st;

                        if (p_findreq->MetadataFilter.MetadataStreamFilter[0] != '\0')
                        {
                            if (!onvif_MetadataStreamFilter(p_findreq->MetadataFilter.MetadataStreamFilter, &p_item->Result))
                            {
                                onvif_free_FindMetadataResult(&p_res->ResultList.Result, p_item);
                                
                                p_track = p_track->next;
                                continue;
                            }
                        }
                        
                        count++;
                    }

                    if (p_findreq->MaxMatchesFlag && count >= p_findreq->MaxMatches)
                    {
                        break;
                    }
                }
            
                p_track = p_track->next;
            }
        }

        if (p_findreq->MaxMatchesFlag == 0 || count < p_findreq->MaxMatches)
        {
            TrackList * p_track = g_onvif_cfg.recordings->Recording.Tracks;
            while (p_track)
            {
                if (p_track->Track.Configuration.TrackType == TrackType_Metadata)
                {
                    p_item = onvif_add_FindMetadataResult(&p_res->ResultList.Result);
                    if (p_item)
                    {
                        strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
                        strcpy(p_item->Result.TrackToken, p_track->Track.TrackToken);
                        p_item->Result.Time = et;

                        if (p_findreq->MetadataFilter.MetadataStreamFilter[0] != '\0')
                        {
                            if (!onvif_MetadataStreamFilter(p_findreq->MetadataFilter.MetadataStreamFilter, &p_item->Result))
                            {
                                onvif_free_FindMetadataResult(&p_res->ResultList.Result, p_item);
                                
                                p_track = p_track->next;
                                continue;
                            }
                        }
                        
                        count++;
                    }

                    if (p_findreq->MaxMatchesFlag && count >= p_findreq->MaxMatches)
                    {
                        break;
                    }
                }
            
                p_track = p_track->next;
            }
        }
    }
    
    p_res->ResultList.SearchState = SearchState_Completed;
    
	return ONVIF_OK;
}

/**
 * @brief 
 *  starts a search session, looking for ptz positions in the scope (See 5.2.4) 
 *  that matches the search filter defined in the request. Results from the 
 *  search are acquired using the GetPTZPositionSearchResults request, 
 *  specifying the search token returned from this request.
 *
 *  The device shall continue searching until one of the following occurs:
 *  The entire time range from StartPoint to EndPoint has been searched through.
 *  The total number of matches has been found, defined by the MaxMatches parameter.
 *  The session has been ended by a client EndSearch request.
 *  The session has been ended because KeepAliveTime since the last request
 *  related to this session has expired.
 *
 *  This operation is mandatory to support whenever CanContainPTZ is true
 *  for any metadata track in any recording on the device.
 *
 *  For the KeepAliveTime a device shall support at least values up to 
 *  ten seconds. A device may adapt larger values.
 *
 *  A device shall only match the search criteria against PTZ status updates 
 *  available between the time interval given in the search, i.e. the device 
 *  shall not locate the PTZ position at the start of the search interval.
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NotImplemented
 *  ONVIF_ERR_ResourceProblem
 */
ONVIF_RET onvif_tse_FindPTZPosition(tse_FindPTZPosition_REQ * p_req, tse_FindPTZPosition_RES * p_res)
{
    SEARCH_SUA * p_sua = (SEARCH_SUA *)malloc(sizeof(SEARCH_SUA));
    if (p_sua)
    {
        p_sua->req = malloc(sizeof(tse_FindPTZPosition_REQ));
        if (p_sua->req)
        {
            memcpy(p_sua->req, p_req, sizeof(tse_FindPTZPosition_REQ));
        }

        p_sua->type = SEARCH_TYPE_PTZPOS;
        p_sua->reqtime = time(NULL);
        sprintf(p_sua->token, "SearchToken_%u", ++g_onvif_idx.search_idx);
        strcpy(p_res->SearchToken, p_sua->token);
        
        if (!onvif_AddSearch(p_sua))
        {
            if (p_sua->req)
            {
                free(p_sua->req);
            }
            
            free(p_sua);
        }
    }
    
    return ONVIF_OK;
}

/**
 * @brief 
 *  acquires the results from a PTZ position search session previously 
 *  initiated by a FindPTZPosition operation. The response shall not 
 *  include results already returned in previous requests for the same 
 *  session. If MaxResults is specified, the response shall not 
 *  contain more than MaxResults results.
 *
 * @return 
 *  The possible return values:
 *  ONVIF_ERR_InvalidToken
 */
ONVIF_RET onvif_tse_GetPTZPositionSearchResults(tse_GetPTZPositionSearchResults_REQ * p_req, tse_GetPTZPositionSearchResults_RES * p_res)
{
    SEARCH_SUA * p_sua = onvif_FindSearch(p_req->SearchToken);
    if (NULL == p_sua)
    {
        return ONVIF_ERR_InvalidToken;
    }

    if (p_sua->type != SEARCH_TYPE_PTZPOS)
    {
        return ONVIF_ERR_InvalidToken;
    }

    // todo : fill the result ...

    if (g_onvif_cfg.recordings)
    {
        int count = 0;
        tse_FindPTZPosition_REQ * p_findreq = (tse_FindPTZPosition_REQ *) p_sua->req;
        time_t st, et;

        st = g_onvif_cfg.recordings->EarliestRecording;
        et = g_onvif_cfg.recordings->LatestRecording;
        
        if (p_findreq->EndPointFlag)
        {
            if (p_findreq->EndPoint < p_findreq->StartPoint)
            {
                // backword search

                st = g_onvif_cfg.recordings->LatestRecording;
                et = g_onvif_cfg.recordings->EarliestRecording;

                if (p_findreq->StartPoint < g_onvif_cfg.recordings->LatestRecording)
                {
                    st = p_findreq->StartPoint;
                }

                if (p_findreq->EndPoint > g_onvif_cfg.recordings->EarliestRecording)
                {
                    et = p_findreq->EndPoint;
                }
            }
            else
            {
                // forward search
                
                if (p_findreq->StartPoint > g_onvif_cfg.recordings->EarliestRecording)
                {
                    st = p_findreq->StartPoint;
                }

                if (p_findreq->EndPoint < g_onvif_cfg.recordings->LatestRecording)
                {
                    et = p_findreq->EndPoint;
                }
            }
        }
        else
        {
            // forward search
            
            if (p_findreq->StartPoint > g_onvif_cfg.recordings->EarliestRecording)
            {
                st = p_findreq->StartPoint;
            }
        }

        // todo : just for test ...

        FindPTZPositionResultList * p_item;

        if (p_findreq->MaxMatchesFlag == 0 || count < p_findreq->MaxMatches)
        {
            TrackList * p_track = g_onvif_cfg.recordings->Recording.Tracks;
            while (p_track)
            {
                if (p_track->Track.Configuration.TrackType == TrackType_Metadata)
                {
                    p_item = onvif_add_FindPTZPositionResult(&p_res->ResultList.Result);
                    if (p_item)
                    {
                        strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
                        strcpy(p_item->Result.TrackToken, p_track->Track.TrackToken);
                        p_item->Result.Time = st;
                        p_item->Result.Position.PanTiltFlag = 1;
                        p_item->Result.Position.PanTilt.x = 0;
                        p_item->Result.Position.PanTilt.y = 0;
                        p_item->Result.Position.ZoomFlag = 1;
                        p_item->Result.Position.Zoom.x = 0;
                        count++;
                    }

                    if (p_findreq->MaxMatchesFlag && count >= p_findreq->MaxMatches)
                    {
                        break;
                    }
                }
            
                p_track = p_track->next;
            }
        }

        if (p_findreq->MaxMatchesFlag == 0 || count < p_findreq->MaxMatches)
        {
            TrackList * p_track = g_onvif_cfg.recordings->Recording.Tracks;
            while (p_track)
            {
                if (p_track->Track.Configuration.TrackType == TrackType_Metadata)
                {
                    p_item = onvif_add_FindPTZPositionResult(&p_res->ResultList.Result);
                    if (p_item)
                    {
                        strcpy(p_item->Result.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
                        strcpy(p_item->Result.TrackToken, p_track->Track.TrackToken);
                        p_item->Result.Time = et;
                        p_item->Result.Position.PanTiltFlag = 1;
                        p_item->Result.Position.PanTilt.x = 0;
                        p_item->Result.Position.PanTilt.y = 0;
                        p_item->Result.Position.ZoomFlag = 1;
                        p_item->Result.Position.Zoom.x = 0;
                        count++;
                    }

                    if (p_findreq->MaxMatchesFlag && count >= p_findreq->MaxMatches)
                    {
                        break;
                    }
                }
            
                p_track = p_track->next;
            }
        }
    }
    
    p_res->ResultList.SearchState = SearchState_Completed;
    
	return ONVIF_OK;
}

/**
 * @brief 
 *  stops an ongoing search session, causing any blocking result 
 *  request to return and the SearchToken to become invalid.
 *  If the search was interrupted before completion, the point 
 *  in time that the search had reached shall be returned. 
 *  If the search had not yet begun, the StartPoint shall be returned. 
 *  Note that an error  message will occur if the search session 
 *  has been already completed before this request. If the search was
 *  completed the original EndPoint supplied by the Find operation 
 *  shall be returned. When issuing EndSearch on a FindRecordings 
 *  request the EndPoint is undefined and shall not be used since 
 *  the FindRecordings request doesn't have StartPoint/EndPoint.
 *
 * @return 
 *  The possible return values:
 *	ONVIF_ERR_InvalidToken
 */
ONVIF_RET onvif_tse_EndSearch(tse_EndSearch_REQ * p_req, tse_EndSearch_RES * p_res)
{
    SEARCH_SUA * p_sua = onvif_FindSearch(p_req->SearchToken);
    if (NULL == p_sua)
    {
        return ONVIF_ERR_InvalidToken;
    }

    if (SEARCH_TYPE_EVENTS == p_sua->type)
    {
        tse_FindEvents_REQ * req = (tse_FindEvents_REQ *) p_sua->req;

        p_res->Endpoint = req->EndPoint;
    }
    else if (SEARCH_TYPE_RECORDING == p_sua->type)
    {
        p_res->Endpoint = time(NULL);
    }
    else if (SEARCH_TYPE_PTZPOS == p_sua->type)
    {
        tse_FindPTZPosition_REQ * req = (tse_FindPTZPosition_REQ *) p_sua->req;

        p_res->Endpoint = req->EndPoint;
    }
    else if (SEARCH_TYPE_METADATA == p_sua->type)
    {
        tse_FindMetadata_REQ * req = (tse_FindMetadata_REQ *) p_sua->req;

        p_res->Endpoint = req->EndPoint;
    }

    onvif_FreeSearch(p_sua);
    
	return ONVIF_OK;
}

/**
 * @return
 *  The possible return values:
 *  ONVIF_ERR_InvalidToken
 */
ONVIF_RET onvif_tse_GetSearchState(tse_GetSearchState_REQ * p_req, tse_GetSearchState_RES * p_res)
{
    SEARCH_SUA * p_sua = onvif_FindSearch(p_req->SearchToken);
    if (NULL == p_sua)
    {
        return ONVIF_ERR_InvalidToken;
    }

    // todo : set the search state ...

	p_res->State = SearchState_Completed;
    
	return ONVIF_OK;
}

/**
 * @brief
 *  requests a URI that can be used to initiate playback of a recorded 
 *  stream using RTSP as the control protocol
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_NoRecording
 *  ONVIF_ERR_InvalidStreamSetup
 *  ONVIF_ERR_StreamConflict
 */
ONVIF_RET onvif_trp_GetReplayUri(HTTPCLN * p_user, trp_GetReplayUri_REQ * p_req, trp_GetReplayUri_RES * p_res)
{
    char sip[32];
	RecordingList * p_recording = onvif_find_Recording(g_onvif_cfg.recordings, p_req->RecordingToken);
	if (NULL == p_recording)
	{
		return ONVIF_ERR_NoRecording;
	}

    onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);
    
	if (p_req->StreamSetup.Transport.Protocol == TransportProtocol_HTTP)
    {
        snprintf(p_res->Uri, sizeof(p_res->Uri), 
            "http://%s/replay?earliest=%llu&amp;latest=%llu", 
            sip, 
            (long long unsigned int)p_recording->EarliestRecording,
            (long long unsigned int)p_recording->LatestRecording);
    }
    else
    {
        snprintf(p_res->Uri, sizeof(p_res->Uri), 
            "rtsp://%s/replay?earliest=%llu&amp;latest=%llu", 
            sip, 
            (long long unsigned int)p_recording->EarliestRecording,
            (long long unsigned int)p_recording->LatestRecording);
    }

	return ONVIF_OK;
}

/**
 * @brief
 *   returns the current configuration of the replay service
 *
 * @return
 *  The possible return values:
 *  ONVIF_OK
 */
ONVIF_RET onvif_trp_GetReplayConfiguration(trp_GetReplayConfiguration_RES * p_res)
{
	p_res->SessionTimeout = g_onvif_cfg.replay_session_timeout;

	return ONVIF_OK;
}

/**
 * @brief
 *  changes the configuration of the replay service
 *
 * @return
 *  The possible return values:
 *  ONVIF_ERR_ConfigModify
 */
ONVIF_RET onvif_trp_SetReplayConfiguration(trp_SetReplayConfiguration_REQ * p_req)
{
    g_onvif_cfg.replay_session_timeout = p_req->SessionTimeout;
    
	return ONVIF_OK;
}

#endif // end of PROFILE_G_SUPPORT


