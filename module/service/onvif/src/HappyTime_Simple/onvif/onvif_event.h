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

#ifndef ONVIF_EVENT_H
#define ONVIF_EVENT_H


/***************************************************************************************/
#define MAX_NUM_EUA			200


/***************************************************************************************/
typedef struct _ONVIF_EVENT_AGENT
{
	BOOL	used;
	
	char 	consumer_addr[256];

	char	host[32];
	char	url[256];

	int		port;
	
	char 	producter_addr[256];
	int  	init_term_time;
	
	time_t	subscibe_time;
	time_t	last_renew_time;
	time_t	last_ntf_time;
} EUA;


typedef struct
{
	char  	consumer_addr[256];
	int   	init_term_time;

	EUA   * p_eua;
} Subscribe_REQ;

typedef struct
{
	int   	term_time;
	char  	refer_addr[256];
} Renew_REQ;


#ifdef __cplusplus
extern "C" {
#endif


/***************************************************************************************/
void 	  onvif_eua_init();
void 	  onvif_eua_deinit();
EUA     * onvif_get_idle_eua();
void 	  onvif_set_idle_eua(EUA * p_eua);
uint32 	  onvif_get_eua_index(EUA * p_eua);
EUA     * onvif_get_eua_by_index(unsigned int index);
EUA     * onvif_eua_lookup_by_addr(const char * addr);
void      onvif_free_used_eua(EUA * p_eua);

/***************************************************************************************/
ONVIF_RET onvif_Subscribe(Subscribe_REQ * p_Subscribe_req);
ONVIF_RET onvif_Renew(Renew_REQ * p_Renew_req);
ONVIF_RET onvif_Unsubscribe(const char * addr);

/***************************************************************************************/
void      onvif_notify(EUA * p_eua);


#ifdef __cplusplus
}
#endif


#endif


