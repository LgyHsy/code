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

#ifndef HTTP_SRV_H
#define HTTP_SRV_H

#include "sys_inc.h"
#include "http.h"
#include "http_parse.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum 
{
    HTTP_UPGARDE_NONE = 0,
    HTTP_UPGARDE_START,
    HTTP_UPGARDE_RECV_HEADER,
    HTTP_UPGARDE_TRANS_DATA,
    HTTP_UPGARDE_COMPELET
}HttpUpgradeStat;


/***************************************************************************************/
HT_API BOOL     http_srv_init(HTTPSRV * p_srv, const char * saddr, uint16 sport, int cln_num, BOOL https, const char * cert_file, const char * key_file);
HT_API void     http_srv_deinit(HTTPSRV * p_srv);
HT_API void     http_set_msg_cb(HTTPSRV * p_srv, http_msg_cb cb, void * p_userdata);
HT_API void     http_set_data_cb(HTTPSRV * p_srv, http_data_cb cb, void * p_userdata);
HT_API void     http_set_conn_cb(HTTPSRV * p_srv, http_conn_cb cb, void * p_userdata);
HT_API int      http_srv_cln_tx(HTTPCLN * p_user, const char * p_data, int len);

/***************************************************************************************/
HT_API uint32   http_cln_index(HTTPSRV * p_srv, HTTPCLN * p_cln);
HT_API HTTPCLN* http_get_cln_by_index(HTTPSRV * p_srv, unsigned long index);
HT_API HTTPCLN* http_get_idle_cln(HTTPSRV * p_srv);
HT_API int      http_free_used_cln(HTTPSRV * p_srv, HTTPCLN * p_cln);
HT_API int      http_set_cln_use_count(HTTPCLN * p_cln, int cnt);

/***************************************************************************************/
void http_set_upgrade_info(int fd, HttpUpgradeStat status);
void http_reset_upgrade_info();
int http_get_upgrade_fd();
HttpUpgradeStat http_get_upgrade_status();

#ifdef __cplusplus
}
#endif

#endif


