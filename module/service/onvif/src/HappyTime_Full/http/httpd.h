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

#ifndef HTTPD_H
#define HTTPD_H

#include "sys_inc.h"
#include "http.h"
#include "linked_list.h"


/**************************************************************************************/

typedef struct
{
    char    vpath[64];
    char    filename[64];
} vpath_file;

typedef struct
{
	const char * func_name;
	const char * mime_type;
    
	int (*output)(HTTPCLN * p_user, char *buff, int buflen);
} func_handler;

typedef struct  
{
	const char * pattern;
	const char * mime_type;
	int          auth;
} mime_handler;

typedef void (*httpd_on_auth)(char * username, BOOL * need_auth, BOOL * user_exist, char * password, void * userdata);

typedef struct
{
    char            root_path[256];
    HD_AUTH_INFO    auth_info;
    LINKED_LIST *   vpath_list;

    void *          userdata;
    httpd_on_auth   on_auth;
} HTTPD_CLASS;

#ifdef __cplusplus
extern "C" {
#endif

HTTPD_CLASS *   httpd_init();
void            httpd_deinit(HTTPD_CLASS * httpd);
void            httpd_process_request(HTTPD_CLASS * httpd, HTTPCLN * p_user, HTTPMSG * rx_msg);
BOOL            httpd_add_vpath_file(HTTPD_CLASS * httpd, const char * vpath, const char * filename);
void            httpd_remove_vpath_file(HTTPD_CLASS * httpd, const char * vpath);
void            httpd_set_root_path(HTTPD_CLASS * httpd, const char * path);
void            httpd_set_on_auth(HTTPD_CLASS * httpd, httpd_on_auth on_auth, void * userdata);
BOOL            httpd_auth_handler(HTTPD_CLASS * httpd, HTTPCLN * p_user, HTTPMSG * rx_msg, char * method);
void            httpd_send_authenticate(HTTPD_CLASS * httpd, HTTPCLN * p_user);

#ifdef __cplusplus
}
#endif

#endif



