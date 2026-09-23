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
#include "http.h"
#ifndef ONVIF_API_H
#define ONVIF_API_H


#define ONVIF_VERSION_STRING    "V10.7"

#define DEFAULT_CONFIG_FILE     "/tmp/onvif.cfg"
#define RUNTIME_CONFIG_FILE     "/tmp/onvifrun.cfg"

#define ONVIF_LOG_FILE          "/tmp/onvifserver.log"

#ifdef __cplusplus
extern "C" {
#endif

void   onvif_start_server();
void   onvif_start();
void   onvif_stop();

void * onvif_task(void * argv);
void   onvif_free_device();

BOOL   onvif_http_msg_cb(HTTPCLN * p_cln, HTTPMSG * p_msg, void * p_userdata);
void   onvif_http_data_cb(HTTPCLN *p_cln, char *buff, int buflen, void *userdata);

void   server_init_cfg();


#ifdef __cplusplus
}
#endif

#endif


