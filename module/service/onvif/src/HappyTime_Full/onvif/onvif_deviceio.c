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

#ifdef DEVICEIO_SUPPORT

#include "sys_inc.h"
#include "onvif_deviceio.h"
#include "onvif_event.h"
#include "onvif_timer.h"
#include "anj_mw_hwctrl.h"

extern ONVIF_CFG g_onvif_cfg;

/***************************************************************************************/

/**
 * @brief
 *  A device that signals support for digital inputs in its capabilities shall
 *  provide the following event whenever one of its input state changes
 *
 **/
void onvif_DigitalInputStateNotify(DigitalInputList * p_req)
{
    NotificationMessageList * p_message = onvif_init_NotificationMessage3(
        "tns1:Device/Trigger/DigitalInput", PropertyOperation_Changed, 
        "InputToken", p_req->DigitalInput.token, NULL, NULL, 
        "LogicalState", 
        (p_req->DigitalInput.IdleState == DigitalIdleState_closed) ? "true" : "false", 
        NULL, NULL);
    if (p_message)
    {
        onvif_put_NotificationMessage(p_message);
    }
}

/**
 * @brief
 *  A device that signals RelayOutputs in its capabilities should provide
 *  the Trigger event whenever its relay output state is changed
 *
 **/
void onvif_RelayOutputStateNotify(RelayOutputList * p_req, int state)
{
	NotificationMessageList * p_message = onvif_init_NotificationMessage3(
		"tns1:Device/Trigger/Relay", (onvif_PropertyOperation)state, 
		"RelayToken", p_req->RelayOutput.token, NULL, NULL, 
		"LogicalState", 
		onvif_RelayLogicalStateToString(p_req->RelayLogicalState), NULL, NULL);
	if (p_message)
	{
		onvif_put_NotificationMessage(p_message);
	}
}

/**
 * @brief
 *  Change relay logical state and send event notify
 *
 **/
void onvif_RelayLogicalStateChanged(void * argv)
{
    RelayOutputList * p_output = (RelayOutputList *)argv;

    p_output->RelayLogicalState = RelayLogicalState_inactive;

    // send notify message    
    onvif_RelayOutputStateNotify(p_output, PropertyOperation_Changed);
}

/**
 * @brief
 *  Gets a list of all available relay outputs and their settings.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 **/
ONVIF_RET onvif_tmd_GetRelayOutputs()
{
    RelayOutputList * p_output = g_onvif_cfg.relay_output;
    while (p_output)
    {
        onvif_RelayOutputStateNotify(p_output, PropertyOperation_Initialized);

        p_output = p_output->next;
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies a video output configuration.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoVideoOutput
 *  ONVIF_ERR_ConfigModify
 **/
ONVIF_RET onvif_tmd_SetVideoOutputConfiguration(tmd_SetVideoOutputConfiguration_REQ * p_req)
{
    VideoOutputList * p_output;
    VideoOutputConfigurationList * p_cfg = onvif_find_VideoOutputConfiguration(g_onvif_cfg.v_output_cfg, p_req->Configuration.token);
    if (NULL == p_cfg)
    {
        return ONVIF_ERR_NoVideoOutput;
    }

    p_output = onvif_find_VideoOutput(g_onvif_cfg.v_output, p_req->Configuration.OutputToken);
    if (NULL == p_output)
    {
        return ONVIF_ERR_NoVideoOutput;
    }

    // todo : here add handler code ...

    
    strcpy(p_cfg->Configuration.Name, p_req->Configuration.Name);
    strcpy(p_cfg->Configuration.OutputToken, p_req->Configuration.OutputToken);
    
    return ONVIF_OK;
}

/**
 * @brief
 *  Modifies a audio output configuration.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoAudioOutput
 *  ONVIF_ERR_ConfigModify
 **/
ONVIF_RET onvif_tmd_SetAudioOutputConfiguration(tmd_SetAudioOutputConfiguration_REQ * p_req)
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

	return ONVIF_OK;
}

/**
 * @brief
 *  Sets the settings of a relay output.
 *
 *  The relay can work in two relay modes:
 *  Bistable - After setting the state, the relay remains in this state.
 *  Monostable - After setting the state, the relay returns to its idle 
 *  state after the specified time.
 *
 *  The physical idle state of a relay output can be configured by setting
 *  the IdleState to 'open' or 'closed' (inversion of the relay behaviour).
 *
 *  Idle State 'open' means that the relay is open when the relay state is 
 *  set to 'inactive' through the trigger command and closed when the state
 *  is set to 'active' through the same command.
 *
 *  Idle State 'closed' means, that the relay is closed when the relay state
 *  is set to 'inactive' through the trigger command and open when the state 
 *  is set to 'active' through the same command.
 *
 *  The Duration parameter of the Properties field "DelayTime" describes the
 *  time after which the relay returns to its idle state if it is in monostable
 *  mode. If the relay is set to bistable mode the value of the parameter shall
 *  be ignored.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_RelayToken
 *  ONVIF_ERR_ModeError
 **/
ONVIF_RET onvif_tmd_SetRelayOutputSettings(tmd_SetRelayOutputSettings_REQ * p_req)
{
	RelayOutputList * p_output = onvif_find_RelayOutput(g_onvif_cfg.relay_output, p_req->RelayOutput.token);
	if (NULL == p_output)
	{
		return ONVIF_ERR_RelayToken;
	}

	// todo : here add handler code ...
	if (1)
	{
		int position = -1;
		if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_1"))
			position = 0;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_2"))
			position = 1;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_3"))
			position = 2;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_4"))
			position = 3;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_5"))
			position = 4;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_6"))
			position = 5;
		if (position != -1)
		{
			OutPutAlarm OutputAlm;
			AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();
			if (!pAlarmCfg)
			{
				return ONVIF_OK;
			}
			memcpy(&OutputAlm, &pAlarmCfg->normalAlarm.outputAlarm, sizeof(OutputAlm));

			int delaytime = p_req->RelayOutput.Properties.DelayTime;
			int idlestate = p_req->RelayOutput.Properties.IdleState;
			if(delaytime > 60000)
				delaytime = 60000;
			if(delaytime < 0)
				delaytime = 0;
			
			OutputChannel *poutputChannel;
			poutputChannel = &(OutputAlm.outputChannels[position]);
			if(idlestate == 0)//常闭
				strcpy(poutputChannel->triggerType.name, "LOW");
			else//常开
				strcpy(poutputChannel->triggerType.name, "HIGH");

			poutputChannel->duration = delaytime/1000;
			
			anj_config_alarm_output_set(&OutputAlm);
		}
		
	}
	memcpy(&p_output->RelayOutput.Properties, &p_req->RelayOutput.Properties, sizeof(onvif_RelayOutputSettings));

	return ONVIF_OK;
}

/**
 * @brief
 *  Triggers a relay output
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_RelayToken
 **/
ONVIF_RET onvif_tmd_SetRelayOutputState(tmd_SetRelayOutputState_REQ * p_req)
{
	RelayOutputList * p_output = onvif_find_RelayOutput(g_onvif_cfg.relay_output, p_req->RelayOutputToken);
	if (NULL == p_output)
	{
		return ONVIF_ERR_RelayToken;
	}

	// todo : here add handler code ...
	if (1)
	{
		int ret;
		int logicalstate = -1;
		int position = -1;
		if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_1"))
			position = 0;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_2"))
			position = 1;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_3"))
			position = 2;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_4"))
			position = 3;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_5"))
			position = 4;
		else if (!strcmp(p_output->RelayOutput.token, "RelayOutputToken_6"))
			position = 5;

		if (p_output->RelayLogicalState == 0)
			logicalstate = 1;
		else
			logicalstate = 0;
		
		if (position != -1)
		{
			ret = anj_mw_hwctrl_alarmout_chn_status_set(position + 1, logicalstate);
			if(ret == -1)
				log_print(HT_LOG_INFO, "WriteIooutByChannel failed !!!\n");
			else
				log_print(HT_LOG_INFO, "WriteIooutByChannel %d channel:%d status:%d \n", ret, position, logicalstate);
		}
	}
	
	if (p_output->RelayLogicalState != p_req->LogicalState)
	{
		p_output->RelayLogicalState = p_req->LogicalState;

		// send notify message    
		onvif_RelayOutputStateNotify(p_output, PropertyOperation_Changed);

		if (p_output->RelayLogicalState == RelayLogicalState_active)
		{
			timer_add(5, onvif_RelayLogicalStateChanged, p_output, TIMER_MODE_SINGLE);
		}
	}

	return ONVIF_OK;
}

/**
 * @brief
 *  Modifies existing digital input configurations. 
 *  When applying multiple configuration settings, the expected behaviour
 *  is to configure all or none. If one of the provided configurations is
 *  invalid, the expected behaviour of the device is to apply none of the
 *  configurations and an indication in the return fault which digital
 *  input configuration has not been accepted.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_NoConfig
 *  ONVIF_ERR_SettingsInvalid
 **/
ONVIF_RET onvif_tmd_SetDigitalInputConfigurations(tmd_SetDigitalInputConfigurations_REQ * p_req)
{
    DigitalInputList * p_input = p_req->DigitalInputs;

    // todo : here add handler code ...

    
    while (p_input)
    {
        DigitalInputList * p_tmp = onvif_find_DigitalInput(g_onvif_cfg.digit_input, p_input->DigitalInput.token);
        if (p_tmp)
        {
            if (p_input->DigitalInput.IdleStateFlag)
            {
                if (p_tmp->DigitalInput.IdleState != p_input->DigitalInput.IdleState)
                {
                    p_tmp->DigitalInput.IdleState = p_input->DigitalInput.IdleState;

                    // send notify message    
                    onvif_DigitalInputStateNotify(p_tmp);
                }
            }
        }
        
        p_input = p_input->next;
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Sets the setting of serial port.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_ConfigModify
 *  ONVIF_ERR_InvalidSerialPort
 **/
ONVIF_RET onvif_tmd_SetSerialPortConfiguration(tmd_SetSerialPortConfiguration_REQ * p_req)
{
    SerialPortList * p_port = onvif_find_SerialPort_by_ConfigurationToken(g_onvif_cfg.serial_port, p_req->SerialPortConfiguration.token);
    if (NULL == p_port)
    {
        return ONVIF_ERR_InvalidSerialPort;
    }

    // todo : here add handler code ...


    memcpy(&p_port->Configuration, &p_req->SerialPortConfiguration, sizeof(onvif_SerialPortConfiguration));

    return ONVIF_OK;
}

/**
 * @brief
 *  Transmit/receive generic controlling data to/from a serial device that is
 *  connected to the serial port of the device.
 *
 *  This operation can be used for the following purposes.
 *  Transmitting arbitrary data to the connected serial device
 *  Receiving data from the connected serial device
 *  Transmitting arbitrary data to the connected serial device and then receiving
 *  its response data
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_InvalidSerialPort
 *  ONVIF_ERR_DataLengthOver
 *  ONVIF_ERR_DelimiterNotSupported
 **/
ONVIF_RET onvif_tmd_SendReceiveSerialCommand(tmd_SendReceiveSerialCommand_REQ * p_req, tmd_SendReceiveSerialCommand_RES * p_res)
{
    SerialPortList * p_port = onvif_find_SerialPort_by_ConfigurationToken(g_onvif_cfg.serial_port, p_req->token);
    if (NULL == p_port)
    {
        return ONVIF_ERR_InvalidSerialPort;
    }
    
    // todo : here add handler code ...

    
    return ONVIF_OK;
}

#endif // DEVICEIO_SUPPORT



