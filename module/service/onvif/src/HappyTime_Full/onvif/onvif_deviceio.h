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

#ifndef ONVIF_DEVICEIO_H
#define ONVIF_DEVICEIO_H

#include "onvif.h"

typedef struct
{
	char 	VideoOutputToken[ONVIF_TOKEN_LEN];					// required, Token of the requested VideoOutput
} tmd_GetVideoOutputConfiguration_REQ;

typedef struct 
{
	onvif_VideoOutputConfiguration	VideoOutputConfiguration;	// required, Current configuration of the Video output
} tmd_GetVideoOutputConfiguration_RES;

typedef struct 
{
	onvif_VideoOutputConfiguration  Configuration;	            // required

	BOOL    ForcePersistence;	                                // required, The ForcePersistence element determines how configuration
							                                    //  changes shall be stored. If true, changes shall be persistent. 
							                                    //  If false, changes MAY revert to previous values after reboot
} tmd_SetVideoOutputConfiguration_REQ;

typedef struct
{
    char 	VideoOutputToken[ONVIF_TOKEN_LEN];					// required, Token of the Video Output whose options are requested
} tmd_GetVideoOutputConfigurationOptions_REQ;

typedef struct
{
    char    AudioOutputToken[ONVIF_TOKEN_LEN];                  // required, Token of the physical Audio output
} tmd_GetAudioOutputConfiguration_REQ;  

typedef struct
{
    onvif_AudioOutputConfiguration AudioOutputConfiguration;    // required, Current configuration of the Audio output
} tmd_GetAudioOutputConfiguration_RES; 

typedef struct 
{
	onvif_AudioOutputConfiguration  Configuration;	            // required, 
	
	BOOL    ForcePersistence;	                                // required, The ForcePersistence element determines how configuration changes shall be stored
} tmd_SetAudioOutputConfiguration_REQ;

typedef struct
{
    char    AudioOutputToken[ONVIF_TOKEN_LEN];                  // required, Token of the physical Audio Output whose options are requested
} tmd_GetAudioOutputConfigurationOptions_REQ;

typedef struct 
{
    uint32  RelayOutputTokenFlag : 1;
    uint32  Reserved             : 31;
    
	char    RelayOutputToken[ONVIF_TOKEN_LEN];	                // optional, Optional reference token to the relay for which the options are requested
} tmd_GetRelayOutputOptions_REQ;

typedef struct 
{
	onvif_RelayOutput   RelayOutput;	                        // required
} tmd_SetRelayOutputSettings_REQ;

typedef struct 
{
	char    RelayOutputToken[ONVIF_TOKEN_LEN];	                // required
	
	onvif_RelayLogicalState LogicalState;	                    // required 
} tmd_SetRelayOutputState_REQ;

typedef struct 
{
    uint32  TokenFlag   : 1;
    uint32  Reserved    : 31;
    
	char    Token[ONVIF_TOKEN_LEN];	                            // optional, 
} tmd_GetDigitalInputConfigurationOptions_REQ;

typedef struct
{
    DigitalInputList * DigitalInputs;
} tmd_SetDigitalInputConfigurations_REQ;

typedef struct 
{
	char    SerialPortToken[ONVIF_TOKEN_LEN];	                // required
} tmd_GetSerialPortConfiguration_REQ;

typedef struct 
{
	char    SerialPortToken[ONVIF_TOKEN_LEN];	                // required
} tmd_GetSerialPortConfigurationOptions_REQ;

typedef struct
{
    onvif_SerialPortConfiguration   SerialPortConfiguration;    // required, The parameters for configuring the serial port
    
	BOOL    ForcePersistance;	                                // required 
} tmd_SetSerialPortConfiguration_REQ;

typedef struct
{
    char    token[ONVIF_TOKEN_LEN];                             // required, The physical serial port reference to be used when this request is invoked
    
    onvif_SendReceiveSerialCommand  Command;
} tmd_SendReceiveSerialCommand_REQ;

typedef struct
{
    uint32  SerialDataFlag  : 1;
    uint32  Reserved        : 31;
    
    onvif_SerialData    SerialData;                             // optional, The serial port data
} tmd_SendReceiveSerialCommand_RES;


#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET onvif_tmd_GetRelayOutputs();
ONVIF_RET onvif_tmd_SetVideoOutputConfiguration(tmd_SetVideoOutputConfiguration_REQ * p_req);
ONVIF_RET onvif_tmd_SetAudioOutputConfiguration(tmd_SetAudioOutputConfiguration_REQ * p_req);
ONVIF_RET onvif_tmd_SetRelayOutputSettings(tmd_SetRelayOutputSettings_REQ * p_req);
ONVIF_RET onvif_tmd_SetRelayOutputState(tmd_SetRelayOutputState_REQ * p_req);
ONVIF_RET onvif_tmd_SetDigitalInputConfigurations(tmd_SetDigitalInputConfigurations_REQ * p_req);
ONVIF_RET onvif_tmd_SetSerialPortConfiguration(tmd_SetSerialPortConfiguration_REQ * p_req);
ONVIF_RET onvif_tmd_SendReceiveSerialCommand(tmd_SendReceiveSerialCommand_REQ * p_req, tmd_SendReceiveSerialCommand_RES * p_res);

#ifdef __cplusplus
}
#endif


#endif // end of ONVIF_DEVICEIO_H


