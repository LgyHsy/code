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

#ifndef ONVIF_EVENT_H
#define ONVIF_EVENT_H


/***************************************************************************************/
#define MAX_NUM_EUA			    20
#define MAX_FILTER_NUMS         10

#define EUA_CLIENT_TYPE_NORMAL  0
#define EUA_CLIENT_TYPE_WEB     1

/***************************************************************************************/

typedef struct _ONVIF_FILTER
{
	char    TopicExpression[MAX_FILTER_NUMS][256];
	char    MessageContent[MAX_FILTER_NUMS][256];
} ONVIF_FILTER;

typedef struct
{
	uint32  eua_idx;									// required
	
	int		Timeout;									// required, Maximum time to block until this method returns
	int 	MessageLimit;								// required, Upper limit for the number of messages to return at once. A server implementation may decide to return less messages
} tev_PullMessages_REQ;

typedef struct
{
	int     used_flag;
	time_t  req_time;
	void *  http_cln;
	tev_PullMessages_REQ req;
} PULLMESSAGE;


typedef struct _ONVIF_EVENT_AGENT
{
	uint32  FiltersFlag : 1;
	uint32  used        : 1;
	uint32  reserver    : 30;
	
	BOOL	pollMode;								    // ture - poll mode, false - notify mode
	
	char 	consumer_addr[256];

	char	host[32];
	char	url[256];

	int		port;
	int		https;
    int     clitype;

	char 	producter_addr[256];
	int  	init_term_time;
	
	time_t	subscibe_time;
	time_t	last_renew_time;

	ONVIF_FILTER Filters;
	PULLMESSAGE  pullmsg;

	LINKED_LIST* msg_list;                              // for poll mode, save notify message to list
} EUA;

typedef struct
{
	uint32  FiltersFlag                 : 1;
	uint32	InitialTerminationTimeFlag	: 1;			// Indicates whether the field InitialTerminationTime is valid
	uint32 	Reserved					: 30;

	char  	ConsumerReference[256];						// required
	int   	InitialTerminationTime;						// optional

	ONVIF_FILTER Filters;

	EUA   * p_eua;
} tev_Subscribe_REQ;

typedef struct
{
	uint32  eua_idx;									// required

	int     TerminationTimeType;                        // TerminationTime type, 0 - datetime, 1 - duration
	time_t 	TerminationTime;							// required
} tev_Renew_REQ;

typedef struct
{
	uint32  FiltersFlag                 : 1;
	uint32	InitialTerminationTimeFlag	: 1;			// Indicates whether the field InitialTerminationTime is valid
	uint32 	Reserved					: 30;
	
	ONVIF_FILTER Filters;
	
	int		InitialTerminationTime;						// optional, Initial termination time

	EUA   * p_eua;
} tev_CreatePullPointSubscription_REQ;



#ifdef __cplusplus
extern "C" {
#endif


/***************************************************************************************/
void 	  onvif_eua_init();
void 	  onvif_eua_deinit();
EUA     * onvif_get_idle_eua();
void 	  onvif_set_idle_eua(EUA * p_eua);
uint32 	  onvif_get_eua_index(EUA * p_eua);
EUA     * onvif_get_eua_by_index(uint32 index);
void      onvif_free_used_eua(EUA * p_eua);

ONVIF_RET onvif_create_web_eua(HTTPCLN * p_user, int *index);
BOOL      onvif_find_eua_by_client(HTTPCLN * p_user, int *find_index, int type);


BOOL 	  onvif_put_NotificationMessage(NotificationMessageList * p_message);

HQUEUE  * onvif_xpath_parse(char * xpath);

/***************************************************************************************/
ONVIF_RET onvif_tev_Subscribe(HTTPCLN * p_user, tev_Subscribe_REQ * p_req);
ONVIF_RET onvif_tev_Renew(tev_Renew_REQ * p_req);
ONVIF_RET onvif_tev_Unsubscribe(uint32 eua_idx);
ONVIF_RET onvif_tev_CreatePullPointSubscription(HTTPCLN * p_user, tev_CreatePullPointSubscription_REQ * p_req);
ONVIF_RET onvif_tev_SetSynchronizationPoint();
ONVIF_RET onvif_tev_PullMessages(tev_PullMessages_REQ * p_req);

/***************************************************************************************/
void      onvif_event_renew();
void      onvif_notify(EUA * p_eua, NotificationMessageList * p_message);
BOOL      onvif_event_filter(NotificationMessageList * p_message, EUA * p_eua);
BOOL      onvif_init_Message(onvif_Message * p_req, onvif_PropertyOperation op, const char * sname1, const char * svalue1, const char * sname2, const char * svalue2, const char *dname1, const char * dvalue1, const char * dname2, const char * dvalue2, const char *dname3, const char * dvalue3, const char * dname4, const char * dvalue4);
void      onvif_send_simulate_events(const char * topic);

NotificationMessageList * onvif_init_NotificationMessage(BOOL state);
NotificationMessageList * onvif_init_NotificationMessage1(BOOL state);
NotificationMessageList * onvif_init_NotificationMessage2();
NotificationMessageList * onvif_init_NotificationMessage3(const char * topic, onvif_PropertyOperation op, const char * sname1, const char * svalue1, const char * sname2, const char * svalue2, const char *dname1, const char * dvalue1, const char * dname2, const char * dvalue2);
NotificationMessageList * onvif_init_NotificationMessage4(const char * topic, onvif_PropertyOperation op, const char * sname1, const char * svalue1, const char * sname2, const char * svalue2, const char *dname1, const char * dvalue1, const char * dname2, const char * dvalue2, const char * dname3, const char * dvalue3, const char * dname4, const char * dvalue4);
NotificationMessageList * onvif_init_MotionDetect_Message(BOOL is_motion);


NotificationMessageList * onvif_init_NotificationMessage_Motion(BOOL state);
NotificationMessageList * onvif_init_NotificationMessage_IO(BOOL state);
NotificationMessageList * onvif_init_NotificationMessage_IOSTAR(BOOL state);

NotificationMessageList * onvif_init_NotificationMessage_PD(BOOL state);
NotificationMessageList * onvif_init_NotificationMessage_VEHICLE(BOOL state);
NotificationMessageList * onvif_init_NotificationMessage_FH(char *Type);


#ifdef __cplusplus
}
#endif


#endif


