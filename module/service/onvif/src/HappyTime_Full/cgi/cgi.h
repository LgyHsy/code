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

#ifndef CGI_H
#define CGI_H

#include "sys_inc.h"
#include "http.h"


#ifdef __cplusplus
extern "C" {
#endif

int http_response(void *pInst, const char *response_buf, int status);

int copy_file(void *pInst, void *pmsgt, const char *filename, const char *type);

int soap_http_response(void *pInst, void *pmsgt, const char *pResponse, int status);

void * task_manager(OIMSG *stm);

int http_upgrade_trans_firmware(HTTPCLN * p_cln);

#ifdef __cplusplus
}
#endif

#endif


