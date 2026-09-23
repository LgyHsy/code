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
#include "onvif_event.h"
#include "http_cln.h"
#include "http_srv.h"
#include "onvif_pkt.h"
#include "onvif_utils.h"
#include "hqueue.h"
#include "xml_node.h"
#include "soap.h"
#ifdef PROFILE_G_SUPPORT
#include "onvif_recording.h"
#endif


/***************************************************************************************/
extern ONVIF_CFG 	g_onvif_cfg;
extern ONVIF_CLS	g_onvif_cls;

/***************************************************************************************/
void onvif_eua_init()
{
	log_print(HT_LOG_INFO, "onvif_eua_init START\n");
	g_onvif_cls.eua_fl = pps_ctx_fl_init(MAX_NUM_EUA, sizeof(EUA), TRUE);
	g_onvif_cls.eua_ul = pps_ctx_ul_init(g_onvif_cls.eua_fl, TRUE);
	log_print(HT_LOG_INFO, "onvif_eua_init OVER\n");

	return;
}

void onvif_eua_deinit()
{
	if (g_onvif_cls.eua_ul)
	{
		pps_ul_free(g_onvif_cls.eua_ul);
		g_onvif_cls.eua_ul = NULL;
	}
	
	if (g_onvif_cls.eua_fl)
	{
		pps_fl_free(g_onvif_cls.eua_fl);
		g_onvif_cls.eua_fl = NULL;
	}
}

EUA * onvif_get_idle_eua()
{
	EUA * p_eua = (EUA *)pps_fl_pop(g_onvif_cls.eua_fl);
	if (p_eua)
	{
		memset(p_eua, 0, sizeof(EUA));
		
		pps_ctx_ul_add(g_onvif_cls.eua_ul, p_eua);
	}

	return p_eua;
}

void onvif_set_idle_eua(EUA * p_eua)
{
	pps_ctx_ul_del(g_onvif_cls.eua_ul, p_eua);

	if (p_eua->pullmsg.used_flag)
	{
		HTTPCLN * p_cln = (HTTPCLN *)p_eua->pullmsg.http_cln;
		if (p_cln)
		{
            http_free_used_cln((HTTPSRV *)p_cln->http_srv, p_cln);
		}
	}

	if (p_eua->msg_list)
	{
		// clear notify messeage list
		while (h_list_get_number_of_nodes(p_eua->msg_list) > 0)
		{
			LINKED_NODE * p_node = h_list_get_from_front(p_eua->msg_list);
			onvif_free_NotificationMessage((NotificationMessageList *) p_node->p_data);
		
			h_list_remove_from_front(p_eua->msg_list);
		}

		h_list_free_container(p_eua->msg_list);
	}

	memset(p_eua, 0, sizeof(EUA));

	pps_fl_push(g_onvif_cls.eua_fl, p_eua);
}

void onvif_free_used_eua(EUA * p_eua)
{
	if (pps_safe_node(g_onvif_cls.eua_fl, p_eua) == FALSE)
		return;
	
	onvif_set_idle_eua(p_eua);
}

uint32 onvif_get_eua_index(EUA * p_eua)
{
	return pps_get_index(g_onvif_cls.eua_fl, p_eua);
}

EUA * onvif_get_eua_by_index(uint32 index)
{
	return (EUA *)pps_get_node_by_index(g_onvif_cls.eua_fl, index);
}

EUA * onvif_eua_lookup_start()
{
	return (EUA *)pps_lookup_start(g_onvif_cls.eua_ul);
}

EUA * onvif_eua_lookup_next(void * p_eua)
{
	return (EUA *)pps_lookup_next(g_onvif_cls.eua_ul, (EUA *)p_eua);
}

void onvif_eua_lookup_stop()
{
	pps_lookup_end(g_onvif_cls.eua_ul);
}

/***************************************************************************************/

/**
 * @brief
 *  Send notification messageage
 *
 */
BOOL onvif_put_NotificationMessage(NotificationMessageList * p_message)
{
	EUA * p_eua;

	p_eua = onvif_eua_lookup_start();
	while (p_eua)
	{
		if (p_eua->pollMode)
		{
			if (h_list_get_number_of_nodes(p_eua->msg_list) >= 20) // max 20 notify message
			{
				LINKED_NODE * p_node = h_list_get_from_front(p_eua->msg_list);
				onvif_free_NotificationMessage((NotificationMessageList *) p_node->p_data);
				
				h_list_remove_from_front(p_eua->msg_list);
			}

			p_message->refcnt++;

			h_list_add_at_back(p_eua->msg_list, (void *)p_message);
		}
		else
		{
			onvif_notify(p_eua, p_message);
		}

		p_eua = onvif_eua_lookup_next(p_eua);
	}
	onvif_eua_lookup_stop();

	onvif_free_NotificationMessage(p_message);

	return TRUE;
}

HQUEUE * onvif_xpath_parse(char * xpath)
{
	BOOL ret = TRUE;
	HQUEUE * queue = hqCreate(100, 128, 0);	

	int i = 0, fpush = 0;
	int quote_nums = 0;
	char fbuf[100];
	char buff[128];
	char * p = xpath; 

	while (*p != '\0')
	{
		switch (*p)
		{
		case ' ':
		case ':':
		case '=':
			if (i > 0)
			{
				buff[i] = '\0';
				hqBufPut(queue, buff);
				i = 0;
			}
			break;
			
		case '(':
			if (i > 0)
			{
				buff[i] = '\0';
				hqBufPut(queue, buff);
				i = 0;
			}
			
			fbuf[fpush++] = '(';
			break;

		case '/':
			if (*(p+1) == '/') p++;
			break;

		case '[':
			if (i > 0)
			{
				buff[i] = '\0';
				hqBufPut(queue, buff);
				i = 0;
			}

			fbuf[fpush++] = '[';
			break;

		case ']':
			if (fpush == 0)
			{
				return FALSE;
			}
			if (fbuf[--fpush] != '[')
			{
				ret = FALSE;
				break;
			}
			
			if (i > 0)
			{
				buff[i] = '\0';
				hqBufPut(queue, buff);
				i = 0;
			}
			break;

		case '"':
			if (quote_nums == 0)
			{
				quote_nums++;	
				i = 0;
			}
			else if (quote_nums == 1)
			{
				quote_nums = 0;

				if (i > 0)
				{
					buff[i] = '\0';
					hqBufPut(queue, buff);
					i = 0;
				}
				else
				{
					ret = FALSE;
					break;
				}
			}
			break;

		case ')':
			if (fpush == 0)
			{
				ret = FALSE;
				break;
			}
			if (fbuf[--fpush] != '(')
			{
				ret = FALSE;
				break;
			}

			if (i > 0)
			{
				buff[i] = '\0';
				hqBufPut(queue, buff);
				i = 0;
			}
			break;

		default:
			buff[i++] = *p;
			break;
		}

		p++;
	}

	if (ret == FALSE || fpush != 0)
	{
		hqDelete(queue);
		queue = NULL;
	}

	return queue;
}

ONVIF_RET onvif_check_filters(ONVIF_FILTER * p_filter)
{
	int i;

	for (i = 0; i < MAX_FILTER_NUMS; i++)
	{
		if (p_filter->TopicExpression[i][0] != '\0')
		{
			// todo : check if support the topic ...
			if (strlen(p_filter->TopicExpression[i]) <= 5)
			{
				return ONVIF_ERR_InvalidTopicExpressionFault;
			}
		}

		if (p_filter->MessageContent[i][0] != '\0')
		{
			// todo : check the message content is valid ...

			HQUEUE * queue = onvif_xpath_parse(p_filter->MessageContent[i]);
			if (NULL == queue)
			{
				return ONVIF_ERR_InvalidMessageContentExpressionFault;
			}
			hqDelete(queue);
		}
	}

	return ONVIF_OK;
}

/***************************************************************************************/

/**
 * @brief
 *  Subscrib Event Notification.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_ResourceUnknownFault
 */
ONVIF_RET onvif_tev_Subscribe(HTTPCLN * p_user, tev_Subscribe_REQ * p_req)
{
	EUA * p_eua;

	if (p_req->FiltersFlag)
	{
		ONVIF_RET ret = onvif_check_filters(&p_req->Filters);
		if (ONVIF_OK != ret)
		{
			return ret;
		}
	}

	p_eua = onvif_get_idle_eua();
	if (p_eua)
	{
		char sip[32];
		HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;

		if (p_req->InitialTerminationTimeFlag)
		{
			p_eua->init_term_time = p_req->InitialTerminationTime;
		}
		else
		{
			p_eua->init_term_time = g_onvif_cfg.evt_renew_time;
		}

		strcpy(p_eua->consumer_addr, p_req->ConsumerReference);

		onvif_get_service_ip_by_user(p_user, sip, sizeof(sip) - 1);

		onvif_parse_xaddr(p_eua->consumer_addr, p_eua->host, sizeof(p_eua->host), p_eua->url, sizeof(p_eua->url), &p_eua->port, &p_eua->https);

		if (g_onvif_cfg.http_enable && !p_srv->https)
		{
			sprintf(p_eua->producter_addr, "http://%s:%d/event_service/%u", sip, g_onvif_cls.http_port, onvif_get_eua_index(p_eua));
		}
#ifdef HTTPS
		else if (g_onvif_cfg.https_enable && p_srv->https)
		{
			sprintf(p_eua->producter_addr, "https://%s:%d/event_service/%u", sip, g_onvif_cls.https_port, onvif_get_eua_index(p_eua));
		}
#endif

		p_eua->subscibe_time = time(NULL);
		p_eua->last_renew_time = time(NULL);

		p_eua->used = 1;

		p_eua->FiltersFlag = p_req->FiltersFlag;
		memcpy(&p_eua->Filters, &p_req->Filters, sizeof(ONVIF_FILTER));

		p_req->p_eua = p_eua;
	}
	else
	{
		return ONVIF_ERR_ResourceUnknownFault;
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  An ONVIF compliant device shall support this command if it signals support
 *  for [WS-Base Notification] via the MaxNotificationProducers capability.
 *  The command shall at least support a Timeout of one minute. A device shall
 *  respond both parameters CurrentTime and TerminationTime as utc using 
 *  the 'Z' indicator.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_ResourceUnknownFault
 */
ONVIF_RET onvif_tev_Renew(tev_Renew_REQ * p_req)
{
	EUA * p_eua = onvif_get_eua_by_index(p_req->eua_idx);
	if (p_eua && p_eua->used)
	{
		p_eua->last_renew_time = time(NULL);
		
		return ONVIF_OK;
	}	

	return ONVIF_ERR_ResourceUnknownFault;
}

/**
 * @brief
 *  The device shall provide the following Unsubscribe command for all
 *  SubscriptionManager endpoints returned by the CreatePullPointSubscription
 *  command.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_ResourceUnknownFault
 */
ONVIF_RET onvif_tev_Unsubscribe(uint32 eua_idx)
{
	EUA * p_eua = onvif_get_eua_by_index(eua_idx);

	if (p_eua)
	{
	    HTTPCLN *cli = (HTTPCLN *)p_eua->pullmsg.http_cln;

        if (cli && cli->use_count > 1)
        {
            http_set_cln_use_count(cli, 1);
        }
    }

	if (p_eua && p_eua->used)
	{
		onvif_free_used_eua(p_eua);
		return ONVIF_OK;
	}	

	return ONVIF_ERR_ResourceUnknownFault;
}

/**
 * @brief
 *  An ONVIF compliant device shall provide the CreatePullPointSubscription command. 
 *  If no Filter element is specified the pullpoint shall notify all occurring 
 *  events to the client.
 *  
 *  By default the pull point keep alive is controlled via the PullMessages operation. 
 *  In this case, after a PullMessages response is returned, the subscription should
 *  be active for at least the timeout specified in the PullMessages request.
 *  
 *  A device shall support an absolute time value specified in utc as well as a 
 *  relative time value for the InitialTerminationTime parameter. A device shall 
 *  respond both parameters CurrentTime and TerminationTime as utc using the 'Z' indicator.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_ResourceUnknownFault
 */
ONVIF_RET onvif_tev_CreatePullPointSubscription(HTTPCLN * p_user, tev_CreatePullPointSubscription_REQ * p_req)
{
	EUA * p_eua;
	
	if (p_req->FiltersFlag)
	{
		ONVIF_RET ret = onvif_check_filters(&p_req->Filters);
		if (ONVIF_OK != ret)
		{
			return ret;
		}
	}

	p_eua = onvif_get_idle_eua();//拉去一个订阅节点
	if (p_eua)
	{
		char sip[32];
		HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;

		if (p_req->InitialTerminationTimeFlag)
		{
			p_eua->init_term_time = p_req->InitialTerminationTime;
		}
		else
		{
			p_eua->init_term_time = g_onvif_cfg.evt_renew_time;
		}

		onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);

		if (g_onvif_cfg.http_enable && !p_srv->https)
		{
			sprintf(p_eua->producter_addr, "http://%s:%d/event_service/%u", sip, g_onvif_cls.http_port, onvif_get_eua_index(p_eua));
		}
#ifdef HTTPS
		else if (g_onvif_cfg.https_enable && p_srv->https)
		{
			sprintf(p_eua->producter_addr, "https://%s:%d/event_service/%u", sip, g_onvif_cls.https_port, onvif_get_eua_index(p_eua));
		}
#endif

        p_eua->clitype = EUA_CLIENT_TYPE_NORMAL;
		p_eua->subscibe_time = time(NULL);
		p_eua->last_renew_time = time(NULL);

		p_eua->pollMode = TRUE;
		p_eua->used = 1;

		p_eua->msg_list = h_list_create(TRUE);

		p_eua->FiltersFlag = p_req->FiltersFlag;
		memcpy(&p_eua->Filters, &p_req->Filters, sizeof(ONVIF_FILTER));

		p_req->p_eua = p_eua;
	}
	else
	{
		return ONVIF_ERR_ResourceUnknownFault;
	}
	
	return ONVIF_OK;
}

/**
 * @brief
 *  When a client wants to synchronize its properties with the properties of 
 *  the device, it can request a synchronization point which repeats the 
 *  current status of all properties to which a client has subscribed.
 *
 *  The Synchronization Point is requested directly from the SubscriptionManager
 *  which was returned in either the SubscriptionResponse or in the 
 *  CreatePullPointSubscriptionResponse. The property update is transmitted via
 *  the notification transportation of the notification interface.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 */
ONVIF_RET onvif_tev_SetSynchronizationPoint()
{
	NotificationMessageList * p_message;

	/* Push a baseline property snapshot so pull-point clients can sync state. */
	p_message = onvif_init_NotificationMessage1(TRUE);
	if (p_message)
	{
		onvif_put_NotificationMessage(p_message);
	}

	p_message = onvif_init_NotificationMessage2();
	if (p_message)
	{
		onvif_put_NotificationMessage(p_message);
	}

	return ONVIF_OK;
}

/**
 * @brief
 *  The device shall provide the following PullMessages command for all 
 *  SubscriptionManager endpoints returned by the CreatePullPointSubscription 
 *  command.
 *
 *  The device shall support a Timeout of at least one minute. 
 *  The device shall not respond with a PullMessagesFaultResponse when the 
 *  MessageLimit is greater than the device supports. Instead, the device shall
 *  return up to the supported messages in the response.
 *
 *  The response behavior shall be one of three types:
 *  
 *  If there are one or more messages waiting (i.e., aggregated) when the request
 *  arrives, the device shall immediately respond with the waiting messages, 
 *  up to the MessageLimit. The device shall not discard unsent messages, but 
 *  shall await the next PullMessages request to send remaining messages.
 *
 *  If there are no messages waiting, and the device generates a message 
 *  (or multiple simultaneous messages) prior to reaching the Timeout, the device 
 *  shall immediately respond with the generated messages, up to the MessageLimit. 
 *  The device shall not wait for additional messages before returning the response.
 *
 *  If there are no messages waiting, and the device does not generate any message
 *  prior to reaching the Timeout, the device shall respond with zero messages. 
 *  The device shall not return a response with zero messages prior to reaching the 
 *  Timeout.
 *
 *  A device shall respond both parameters CurrentTime and TerminationTime as utc 
 *  using the 'Z' indicator.
 *
 *  After a seek operation the device shall return the messages in strict message 
 *  utc time order. Note that this requirement is not applicable to standard realtime
 *  message delivery where the delivery order may be affected by device internal 
 *  computations.
 *  
 *  A device should return an error (UnableToGetMessagesFault) when receiving a 
 *  PullMessages request for a subscription where a blocking PullMessage request 
 *  already exists.
 *
 * @return
 *  possible retrun value:
 *  ONVIF_OK
 *  ONVIF_ERR_ResourceUnknownFault
 */
ONVIF_RET onvif_tev_PullMessages(tev_PullMessages_REQ * p_req)
{
	EUA * p_eua = onvif_get_eua_by_index(p_req->eua_idx);
	if (p_eua && p_eua->used)
	{
		p_eua->last_renew_time = time(NULL);

		p_eua->pullmsg.used_flag = 1;
		p_eua->pullmsg.req_time = time(NULL);
		memcpy(&p_eua->pullmsg.req, p_req, sizeof(tev_PullMessages_REQ));

		return ONVIF_OK;
	}

	return ONVIF_ERR_ResourceUnknownFault;
}


BOOL onvif_find_eua_by_client(HTTPCLN * p_user, int *find_index, int type)
{
    if (NULL == p_user || NULL == find_index)
    {
        return FALSE;
    }

    int index = 0;
    int empty_client_index = -1;
    EUA *p_eua = NULL;

    for (index = 0; index < MAX_NUM_EUA; index++)
    {
        p_eua = (EUA *) onvif_get_eua_by_index(index);
        if (p_eua == NULL || p_eua->used == 0)
        {
            continue;
        }

        HTTPCLN *p_cli = (HTTPCLN *)p_eua->pullmsg.http_cln;

        if (NULL == p_cli)
        {
            if (-1 == empty_client_index)
            {
                empty_client_index = index;
            }
        }
        else if (p_cli->rip == p_user->rip && p_eua->clitype == type)
        {
            log_print(HT_LOG_INFO, "[onvif_full:%s, %5d] find web use eua! find index:%d, fd:%d, ip:%u, port:%u, address:%p\n",
                __func__, __LINE__, 
                index, p_cli->cfd, p_cli->rip, p_cli->rport, p_cli);

            *find_index = index;
            return TRUE;

        }
    }

    if (empty_client_index != -1)
    {
        log_print(HT_LOG_INFO, "[onvif_full:%s, %5d] find web use eua! use first empty eua:%d!\n", __func__, __LINE__, empty_client_index);
        *find_index = empty_client_index;
        return TRUE;
    }

    log_print(HT_LOG_INFO, "[onvif_full:%s, %5d] find web use eua! but not find!\n", __func__, __LINE__);
    return FALSE;
}


ONVIF_RET onvif_create_web_eua(HTTPCLN * p_user, int *index)
{
    if (NULL == p_user || NULL == index)
    {
        log_print(HT_LOG_INFO, "p_user or index is null!\n");
        return ONVIF_ERR_ResourceUnknownFault;
    }

    char sip[32] = {0};
    EUA * p_eua = NULL;
    p_eua = onvif_get_idle_eua();       //拉去一个订阅节点
    if (p_eua)
    {
        HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;

#if 0
        p_eua->init_term_time = g_onvif_cfg.evt_renew_time;
#else   // 取一个中间时间，不频繁释放重建，也不长时间保存
        // 老版onvif测试时，web停留在首页最长1分钟发送一次 POST /html/onvif/Events/pullSubmanager
        p_eua->init_term_time = 70;
#endif
        onvif_get_service_ip_by_user(p_user, sip, sizeof(sip)-1);

        *index = onvif_get_eua_index(p_eua);
        if (g_onvif_cfg.http_enable && !p_srv->https)
        {
            sprintf(p_eua->producter_addr, "http://%s:%d/event_service/%u", sip, g_onvif_cls.http_port, *index);
        }
#ifdef HTTPS
        else if (g_onvif_cfg.https_enable && p_srv->https)
        {
            sprintf(p_eua->producter_addr, "https://%s:%d/event_service/%u", sip, g_onvif_cls.https_port, *index);
        }
#endif
        p_eua->clitype = EUA_CLIENT_TYPE_WEB;

        p_eua->subscibe_time = time(NULL);
        p_eua->last_renew_time = time(NULL);

        p_eua->pollMode = TRUE;
        p_eua->used = 1;

        p_eua->msg_list = h_list_create(TRUE);

        p_eua->FiltersFlag = 0;
        memset(&p_eua->Filters, 0, sizeof(ONVIF_FILTER));
    }
    else
    {
        return ONVIF_ERR_ResourceUnknownFault;
    }

    return ONVIF_OK;
}

/**
 * @brief
 *  Is called by onvif_timer.
 *
 */
void onvif_event_renew()
{
	uint32 i;
	time_t cur_time = time(NULL);
	EUA * p_eua = NULL;
	int renew_time;

	// event agent renew handler
	
	for (i = 0; i < MAX_NUM_EUA; i++)
	{
		p_eua = (EUA *) onvif_get_eua_by_index(i);
		if (p_eua == NULL || p_eua->used == 0)
		{
			continue;
		}

		renew_time = (p_eua->init_term_time > g_onvif_cfg.evt_renew_time ? g_onvif_cfg.evt_renew_time : p_eua->init_term_time);

		/* timer check */
		if ((cur_time - p_eua->last_renew_time) >= renew_time)
		{
		    log_print(HT_LOG_INFO, "[onvif_full:%s, %5d] check event timeout:%d, free eua[%d] addr:%p\n", __func__, __LINE__, renew_time, i, p_eua);            
			onvif_free_used_eua(p_eua);
			continue;
		}
		else if (p_eua->pullmsg.used_flag)
		{
			int msgnums = h_list_get_number_of_nodes(p_eua->msg_list);

            if (p_eua->pullmsg.http_cln == NULL)
            {
                // 客户端已断开，清理消息
                while (h_list_get_number_of_nodes(p_eua->msg_list) > 0)
                {
                    LINKED_NODE * p_node = h_list_get_from_front(p_eua->msg_list);
                    onvif_free_NotificationMessage((NotificationMessageList *) p_node->p_data);
                    h_list_remove_from_front(p_eua->msg_list);
                }
                p_eua->pullmsg.used_flag = 0;

            }
            /* Faster event reporting *//* msgnums >= p_eua->pullmsg.req.MessageLimit */
			else if ((cur_time - p_eua->pullmsg.req_time + 2 > p_eua->pullmsg.req.Timeout || msgnums >= 1) && p_eua->pullmsg.http_cln != NULL)
			{
				HTTPCLN * p_cln = (HTTPCLN *)p_eua->pullmsg.http_cln;

                //log_print(HT_LOG_INFO, "[onvif_full:%s, %5d] check eua:%p cur_time:%ld, req time:%ld, %d! or trigger msg:%d! handle eua[%d], type:%d\n", 
                    //__func__, __LINE__, 
                    //p_eua, cur_time, p_eua->pullmsg.req_time, p_eua->pullmsg.req.Timeout, msgnums, i, p_eua->clitype);

                // 说明在http_srv中已经释放了这个客户端，这一次没有推送报警事件等到下一次一起推送
                if (p_cln->cfd <= 0)
                {
                    //log_print(HT_LOG_INFO, "[onvif_full:%s, %5d] eua[%d] client already free!\n", __func__, __LINE__, i);
                    http_free_used_cln((HTTPSRV *)p_cln->http_srv, p_cln);
                    p_eua->pullmsg.http_cln = NULL;
                    p_cln = NULL;
                }

                // client存在时才响应
				if (p_cln)
				{
                    //log_print(HT_LOG_INFO, "[onvif_full:%s, %5d] eua[%d] client:%p responese! fd:%d, port:%u, alive:%d!\n",
                        //__func__, __LINE__, i, p_cln, p_cln->cfd, p_cln->rport, p_cln->keep_alive);

    				soap_build_send_rly(p_cln, NULL, ONVIF_OK, build_tev_PullMessages_rly_xml, 
    					(char *)&p_eua->pullmsg.req, 
    					"http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesResponse", 
    					NULL);

                    /*
                        eua节点如果是web类型，则绑定的client统一在 check keep-alive处释放，
                        否则可能出现那边释放不完全时，client的fd已经设置为0，在这边发送失败。 
                    */

                    if (EUA_CLIENT_TYPE_WEB != p_eua->clitype)
                    {
        				http_free_used_cln((HTTPSRV *)p_cln->http_srv, p_cln);
        				p_eua->pullmsg.http_cln = NULL;
        				p_eua->pullmsg.used_flag = 0;
                    }
				}
			}
		}
	}
}


void onvif_notify(EUA * p_eua, NotificationMessageList * p_message)
{
	int len;
	int buflen = 10*1024;
	char * bufs;
	HTTPREQ	req;
	
	memset(&req, 0, sizeof(HTTPREQ));
	
	strcpy(req.host, p_eua->host);
	strcpy(req.url, p_eua->url);
	strcpy(req.action, "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify");
	req.port = p_eua->port;
	req.https = p_eua->https;

	if (!onvif_event_filter(p_message, p_eua))
	{
		return;
	}

	bufs = (char *)malloc(buflen);
	if (NULL == bufs)
	{
		return;
	}
	
	len = build_Notify_xml(bufs, buflen-1, (char *)p_message);

	http_onvif_trans(&req, 5000, bufs, len);

	free(bufs);
}

BOOL onvif_simpleitem_filter(SimpleItemList * p_item, char * name, char * value, BOOL flag)
{
	BOOL nameflag = FALSE;
	BOOL valueflag = FALSE;

	while (p_item)
	{
		if (strcmp(p_item->SimpleItem.Name, name) == 0)
		{
			nameflag = TRUE;
		}

		if (value[0] != '\0')
		{
			if (strcmp(p_item->SimpleItem.Value, value) == 0)
			{
				valueflag = TRUE;
			}

			if (flag == 1)
			{
				if (nameflag && valueflag)
				{
					return TRUE;
				}
			}
			else if (flag == 2)
			{
				if (nameflag || valueflag)
				{
					return TRUE;
				}
			}
		}
		else if (nameflag)
		{
			return TRUE;
		}

		p_item = p_item->next;
	}

	return FALSE;
}

BOOL onvif_elementitem_filter(ElementItemList * p_item, char * name)
{
	while (p_item)
	{
		if (strcmp(p_item->ElementItem.Name, name) == 0)
		{
			return TRUE;
		}

		p_item = p_item->next;
	}

	return FALSE;
}

BOOL onvif_message_content_filter_ex(HQUEUE  * queue, NotificationMessageList * p_message)
{
	int  flag = 0;
	int  itemflag = 0;
	char buff[128];	
	char name[128];
	char value[128] = {'\0'};
	onvif_Message * p_msg;

	if (hqBufGet(queue, buff) == FALSE) // prefix
	{
		return FALSE;
	}

	if (hqBufGet(queue, buff) == FALSE) // SimpleItem or ElementItem
	{
		return FALSE;
	}

	if (strcasecmp(buff, "simpleitem") == 0)
	{
		itemflag = 0;
	}
	else if (strcasecmp(buff, "ElementItem") == 0)
	{
		itemflag = 1;
	}
	else
	{
		return FALSE;
	}

	if (hqBufGet(queue, buff) == FALSE) // @Name
	{
		return FALSE;
	}
	if (strcasecmp(buff, "@name") != 0)
	{
		return FALSE;
	}

	if (hqBufGet(queue, buff) == FALSE) 
	{
		return FALSE;
	}
	strcpy(name, buff);

	if (hqBufPeek(queue, buff))
	{
		if (strcasecmp(buff, "and") == 0)
		{
			flag = 1;
		}
		else if (strcasecmp(buff, "or") == 0)
		{
			flag = 2;
		}
		else
		{
			goto finish;
		}

		hqBufGet(queue, buff);

		if (hqBufGet(queue, buff) == FALSE) 
		{
			return FALSE;
		}
		if (strcasecmp(buff, "@value") != 0)
		{
			return FALSE;
		}

		if (hqBufGet(queue, buff) == FALSE) 
		{
			return FALSE;
		}
		strcpy(value, buff);
	}

finish:

	p_msg = &p_message->NotificationMessage.Message;

	if (itemflag == 0)
	{
		if (p_msg->SourceFlag && onvif_simpleitem_filter(p_msg->Source.SimpleItem, name, value, flag))
		{
			return TRUE;
		}

		if (p_msg->KeyFlag && onvif_simpleitem_filter(p_msg->Key.SimpleItem, name, value, flag))
		{
			return TRUE;
		}

		if (p_msg->DataFlag && onvif_simpleitem_filter(p_msg->Data.SimpleItem, name, value, flag))
		{
			return TRUE;
		}
	}
	else if (itemflag == 1)
	{
		if (p_msg->SourceFlag && onvif_elementitem_filter(p_msg->Source.ElementItem, name))
		{
			return TRUE;
		}

		if (p_msg->KeyFlag && onvif_elementitem_filter(p_msg->Key.ElementItem, name))
		{
			return TRUE;
		}

		if (p_msg->DataFlag && onvif_elementitem_filter(p_msg->Data.ElementItem, name))
		{
			return TRUE;
		}
	}    

	return FALSE;
}

BOOL onvif_message_content_filter(char * filter, NotificationMessageList * p_message)
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
			BOOL ret1 = onvif_message_content_filter_ex(queue, p_message);
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

BOOL onvif_topic_filter(NotificationMessageList * p_message, char * topic)
{
	int len;

	len = (int)strlen(topic);

	if (topic[len-1] == '*')
	{
		topic[len-1] = '\0';

		if (memcmp(p_message->NotificationMessage.Topic, topic, strlen(topic)) == 0)
		{
			return TRUE;
		}
	}
	else if (len > 4 && topic[len-3] == '/' && topic[len-2] == '/' && topic[len-1] == '.')
	{
		topic[len-3] = '\0';

		if (memcmp(p_message->NotificationMessage.Topic, topic, strlen(topic)) == 0)
		{
			return TRUE;
		}
	}
	else if (strcmp(p_message->NotificationMessage.Topic, topic) == 0)
	{
		return TRUE;
	}

	return FALSE;
}

BOOL onvif_event_filter(NotificationMessageList * p_message, EUA * p_eua)
{
	int i;

	if (p_eua->FiltersFlag == 0)
	{
		return TRUE;
	}

	for (i = 0; i < MAX_FILTER_NUMS; i++)
	{
		if (p_eua->Filters.TopicExpression[i][0] != '\0')
		{
			char * p;
			char * tmp;
			char topic[256];

			// todo : check the message is required ... 

			tmp = p_eua->Filters.TopicExpression[i];
			p = strchr(tmp, '|');
			while (p)
			{
				memset(topic, 0, sizeof(topic));
				strncpy(topic, tmp, p-tmp);

				if (onvif_topic_filter(p_message, topic))
				{
					return TRUE;
				}

				tmp = p+1;
				p = strchr(tmp, '|');
			} 

			if (tmp)
			{
				strcpy(topic, tmp);

				if (onvif_topic_filter(p_message, topic))
				{
					return TRUE;
				}
			}
		}

		if (p_eua->Filters.MessageContent[i][0] != '\0')
		{
			// todo : check the message is required ... 

			if (onvif_message_content_filter(p_eua->Filters.MessageContent[i], p_message))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL onvif_init_Message(onvif_Message * p_req, onvif_PropertyOperation op, const char * sname1, const char * svalue1, const char * sname2, const char * svalue2, const char *dname1, const char * dvalue1, const char * dname2, const char * dvalue2, const char *dname3, const char * dvalue3, const char * dname4, const char * dvalue4)
{
	SimpleItemList * p_simpleitem;

	if (op != PropertyOperation_Invalid)
	{
		p_req->PropertyOperationFlag = 1;
		p_req->PropertyOperation = op;
	}

	p_req->UtcTime = time(NULL);

	if ((sname1 && svalue1) || (sname2 && svalue2))
	{
		p_req->SourceFlag = 1;

		if (sname1 && svalue1)
		{
			p_simpleitem = onvif_add_SimpleItem(&p_req->Source.SimpleItem);
			if (p_simpleitem)
			{	
				strcpy(p_simpleitem->SimpleItem.Name, sname1);
				strcpy(p_simpleitem->SimpleItem.Value, svalue1);
			}
		}

		if (sname2 && svalue2)
		{
			p_simpleitem = onvif_add_SimpleItem(&p_req->Source.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, sname2);
				strcpy(p_simpleitem->SimpleItem.Value, svalue2);
			}
		}
	}

	if ((dname1 && dvalue1) || (dname2 && dvalue2) || (dname3 && dvalue3) || (dname4 && dvalue4))
	{
		p_req->DataFlag = 1;

		if (dname1 && dvalue1)
		{
			p_simpleitem = onvif_add_SimpleItem(&p_req->Data.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, dname1);
				strcpy(p_simpleitem->SimpleItem.Value, dvalue1);
			}
		}

		if (dname2 && dvalue2)
		{
			p_simpleitem = onvif_add_SimpleItem(&p_req->Data.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, dname2);
				strcpy(p_simpleitem->SimpleItem.Value, dvalue2);
			}
		}

		if (dname3 && dvalue3)
		{
			p_simpleitem = onvif_add_SimpleItem(&p_req->Data.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, dname3);
				strcpy(p_simpleitem->SimpleItem.Value, dvalue3);
			}
		}

		if (dname4 && dvalue4)
		{
			p_simpleitem = onvif_add_SimpleItem(&p_req->Data.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, dname4);
				strcpy(p_simpleitem->SimpleItem.Value, dvalue4);
			}
		}
	}

	return TRUE;
}

/**
 * Generate notify message, just for test
 *
 */
NotificationMessageList * onvif_init_NotificationMessage(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:RuleEngine/MotionRegionDetector/Motion");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Initialized;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoSource");

			if (g_onvif_cfg.v_src)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.v_src->VideoSource.token);
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceToken_1");
			}
		}

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "RuleName");
			strcpy(p_simpleitem->SimpleItem.Value, "MyMotionRegionDetector");
		}

		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
		}
	}

	return p_message;
}

NotificationMessageList * onvif_init_NotificationMessage_IOSTAR(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Device/Trigger/DigitalInput");
		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "InputToken");
			strcpy(p_simpleitem->SimpleItem.Value, "Input1");
		}
		
		p_msg->Message.DataFlag = 1;
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "LogicalState");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
		}
	}
	
	return p_message;
}

NotificationMessageList * onvif_init_NotificationMessage_IO(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:Device/Trigger/tnshik:AlarmIn");
		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "AlarmInToken");
			strcpy(p_simpleitem->SimpleItem.Value, "Input1");
		}
		
		p_msg->Message.DataFlag = 1;
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
		}
	}
	
	return p_message;
}

NotificationMessageList * onvif_init_notification_motion_alarm_message(int alarm_type, int alarm_level, char *alarm_data)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:RuleEngine/CellMotionDetector/Motion");
		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoSourceConfigurationToken");
			strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceMain");
		}
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoAnalyticsConfigurationToken");
			strcpy(p_simpleitem->SimpleItem.Value, "VideoAnalyticsToken");
		}
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Rule");
			strcpy(p_simpleitem->SimpleItem.Value, "MyMotionDetectorRule");
		}
		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
            if (alarm_type == 1)
			{
                strcpy(p_simpleitem->SimpleItem.Name, "IsVideoPd");
			}
			else if (alarm_type == 2)
			{
                strcpy(p_simpleitem->SimpleItem.Name, "IsVideoAi");
                p_msg->AlarmDataLevel = (char *)malloc(4);
                if (p_msg->AlarmDataLevel != NULL)
                {
                    memset(p_msg->AlarmDataLevel, 0, 4);
                    sprintf(p_msg->AlarmDataLevel, "%d", alarm_level);
                }
			}
			else if (alarm_type == 3)
			{
                strcpy(p_simpleitem->SimpleItem.Name, alarm_data);
			}
		    else
		    {
			    strcpy(p_simpleitem->SimpleItem.Name, "IsMotion");
			}

			strcpy(p_simpleitem->SimpleItem.Value, "true");
		}
	}
	
	return p_message;
}


NotificationMessageList * onvif_init_NotificationMessage_Motion(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:RuleEngine/CellMotionDetector/Motion");
		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoSourceConfigurationToken");
			strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceMain");
		}
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoAnalyticsConfigurationToken");
			strcpy(p_simpleitem->SimpleItem.Value, "VideoAnalyticsToken");
		}
		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Rule");
			strcpy(p_simpleitem->SimpleItem.Value, "MyMotionDetectorRule");
		}
		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "IsMotion");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
		}
	}
	
	return p_message;
}

NotificationMessageList * onvif_init_NotificationMessage_motion(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
#ifdef DEVICEIO_SUPPORT
		strcpy(p_msg->Topic, "tns1:RuleEngine/CellMotionDetector/Motion");
#else
		strcpy(p_msg->Topic, "tns1:VideoSource/ImageTooBlurry/ImagingService");
#endif		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Initialized;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
#ifdef DEVICEIO_SUPPORT 
			strcpy(p_simpleitem->SimpleItem.Name, "InputToken");

			if (g_onvif_cfg.digit_input)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.digit_input->DigitalInput.token);
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "DigitalInputToken_1");
			}
#else
			strcpy(p_simpleitem->SimpleItem.Name, "Source");

			if (g_onvif_cfg.v_src)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.v_src->VideoSource.token);
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceToken_1");
			}
#endif			
		}

		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
#ifdef DEVICEIO_SUPPORT 
			strcpy(p_simpleitem->SimpleItem.Name, "LogicalState");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
#else
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
#endif			
		}
	}
	
	return p_message;
}

NotificationMessageList * onvif_init_NotificationMessage_PD(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:UserAlarm/IVA/HumanShapeDetect");
		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoSourceConfigurationToken");
			strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceMain");
		}
		
		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
		}
	}
	
	return p_message;
}

NotificationMessageList * onvif_init_NotificationMessage_VEHICLE(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:UserAlarm/IVA/VehicleShapeDetect");
		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoSourceConfigurationToken");
			strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceMain");
		}
		
		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
		}
	}
	
	return p_message;
}

NotificationMessageList * onvif_init_NotificationMessage_FH(char *Type)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:UserAlarm/Call/Alarm");
		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoSourceConfigurationToken");
			strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceMain");
		}
		
		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Action");
			strcpy(p_simpleitem->SimpleItem.Value, Type);
		}
	}
	
	return p_message;
}

/**
 * Generate notify message, just for test
 *
 */
NotificationMessageList * onvif_init_NotificationMessage1(BOOL state)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
#ifdef DEVICEIO_SUPPORT
		strcpy(p_msg->Topic, "tns1:Device/Trigger/DigitalInput");
#else
		strcpy(p_msg->Topic, "tns1:VideoSource/ImageTooBlurry/ImagingService");
#endif		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Initialized;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
#ifdef DEVICEIO_SUPPORT 
			strcpy(p_simpleitem->SimpleItem.Name, "InputToken");

			if (g_onvif_cfg.digit_input)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.digit_input->DigitalInput.token);
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "DigitalInputToken_1");
			}
#else
			strcpy(p_simpleitem->SimpleItem.Name, "Source");

			if (g_onvif_cfg.v_src)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.v_src->VideoSource.token);
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceToken_1");
			}
#endif			
		}

		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
#ifdef DEVICEIO_SUPPORT 
			strcpy(p_simpleitem->SimpleItem.Name, "LogicalState");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
#else
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, state ? "true" : "false");
#endif			
		}
	}
	
	return p_message;
}

/**
 * Generate notify message, just for test
 *
 */
NotificationMessageList * onvif_init_NotificationMessage2()
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
		
		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
#ifdef DEVICEIO_SUPPORT        
		strcpy(p_msg->Topic, "tns1:Device/Trigger/Relay");
#else
		strcpy(p_msg->Topic, "tns1:VideoSource/ImageTooDark/ImagingService");
#endif		
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Initialized;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
#ifdef DEVICEIO_SUPPORT 
			strcpy(p_simpleitem->SimpleItem.Name, "RelayToken");

			if (g_onvif_cfg.relay_output)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.relay_output->RelayOutput.token);
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "RelayOutputToken_1");
			}
#else
			strcpy(p_simpleitem->SimpleItem.Name, "Source");

			if (g_onvif_cfg.v_src)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.v_src->VideoSource.token);
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "VideoSourceToken_1");
			}
#endif
		}

		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{	
#ifdef DEVICEIO_SUPPORT 
			strcpy(p_simpleitem->SimpleItem.Name, "LogicalState");

			if (g_onvif_cfg.relay_output)
			{
				strcpy(p_simpleitem->SimpleItem.Value, onvif_RelayLogicalStateToString(g_onvif_cfg.relay_output->RelayLogicalState));
			}
			else
			{
				strcpy(p_simpleitem->SimpleItem.Value, "inactive");
			}
#else
			strcpy(p_simpleitem->SimpleItem.Name, "State");
			strcpy(p_simpleitem->SimpleItem.Value, "true");
#endif
		}
	}
	
	return p_message;
}

/**
 * Generate notify message, just for test
 *
 */
NotificationMessageList * onvif_init_NotificationMessage3(const char * topic, onvif_PropertyOperation op, const char * sname1, const char * svalue1, const char * sname2, const char * svalue2, const char *dname1, const char * dvalue1, const char * dname2, const char * dvalue2)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;

		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, topic);

		onvif_init_Message(&p_msg->Message, op, sname1, svalue1, sname2, svalue2, 
			dname1, dvalue1, dname2, dvalue2, NULL, NULL, NULL, NULL);
	}

	return p_message;
}

/**
 * Generate notify message, just for test
 *
 */
NotificationMessageList * onvif_init_NotificationMessage4(const char * topic, onvif_PropertyOperation op, const char * sname1, const char * svalue1, const char * sname2, const char * svalue2, const char *dname1, const char * dvalue1, const char * dname2, const char * dvalue2, const char * dname3, const char * dvalue3, const char * dname4, const char * dvalue4)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;

		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, topic);

		onvif_init_Message(&p_msg->Message, op, sname1, svalue1, sname2, svalue2, 
			dname1, dvalue1, dname2, dvalue2, dname3, dvalue3, dname4, dvalue4);
	}
	
	return p_message;
}

/**
* @brief
*  Initiate a motion detection event
*
* @param is_motion The parameter is_motion is equal to TRUE, 
*  indicating that a motion detection event is detected, 
*  and is_motion is equal to FALSE, 
*  indicating that the motion detection event is stopped
*/
NotificationMessageList * onvif_init_MotionDetect_Message(BOOL is_motion)
{
	NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
	if (p_message)
	{
		SimpleItemList * p_simpleitem;
		onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;

		strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
		strcpy(p_msg->Topic, "tns1:RuleEngine/CellMotionDetector/Motion");
		p_msg->Message.PropertyOperationFlag = 1;
		p_msg->Message.PropertyOperation = PropertyOperation_Changed;
		p_msg->Message.UtcTime = time(NULL);

		p_msg->Message.SourceFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoSourceConfigurationToken");

			if (g_onvif_cfg.v_src_cfg)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.v_src_cfg->Configuration.token);
			}
		}

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "VideoAnalyticsConfigurationToken");

#ifdef VIDEO_ANALYTICS
			if (g_onvif_cfg.va_cfg)
			{
				strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.va_cfg->Configuration.token);
			}
#endif
		}

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "Rule");
			strcpy(p_simpleitem->SimpleItem.Value, "MyCellMotionDetector");
		}

		p_msg->Message.DataFlag = 1;

		p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
		if (p_simpleitem)
		{
			strcpy(p_simpleitem->SimpleItem.Name, "IsMotion");
			sprintf(p_simpleitem->SimpleItem.Value, "%s", is_motion ? "true" : "false");
		}
	}

	return p_message;
}

void onvif_send_simulate_events(const char * topic)
{
	// for ODTT test case IMAGING-4-1-1-V18.06, IMAGING-4-1-2-V18.06, 
	//  IMAGING-4-1-3-V18.06, IMAGING-4-1-4-V18.06, IMAGING-4-1-5-V18.06
	if (soap_strcmp(topic, "VideoSource/ImageTooBlurry/ImagingService")== 0 ||
		soap_strcmp(topic, "VideoSource/ImageTooDark/ImagingService")== 0 || 
		soap_strcmp(topic, "VideoSource/ImageTooBright/ImagingService")== 0 || 
		soap_strcmp(topic, "VideoSource/GlobalSceneChange/ImagingService")== 0 ||
		soap_strcmp(topic, "VideoSource/MotionAlarm")== 0)
	{
		char token[ONVIF_TOKEN_LEN];
		NotificationMessageList * p_message;

		if (g_onvif_cfg.v_src)
		{
			strcpy(token, g_onvif_cfg.v_src->VideoSource.token);
		}
		else
		{
			strcpy(token, "VideoSourceToken_1");
		}
			
		p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
						"Source", token, NULL, NULL, "State", "true", NULL, NULL);
		if (p_message)
		{
			onvif_put_NotificationMessage(p_message);
		}
	}
	else if (soap_strcmp(topic, "RuleEngine/MotionRegionDetector/Motion") == 0)
	{
		char token[ONVIF_TOKEN_LEN];
		NotificationMessageList * p_message;

		if (g_onvif_cfg.v_src)
		{
			strcpy(token, g_onvif_cfg.v_src->VideoSource.token);
		}
		else
		{
			strcpy(token, "VideoSourceToken_1");
		}
			
		p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
						"VideoSource", token, "RuleName", "TestMotionRegion", "State", "true", 
						NULL, NULL);
		if (p_message)
		{
			onvif_put_NotificationMessage(p_message);
		}
	}
	else if (soap_strcmp(topic, "AccessPoint/State/Enabled") == 0)
	{
#ifdef PROFILE_C_SUPPORT
		AccessPointList * p_access_points = g_onvif_cfg.access_points;
		while (p_access_points)
		{
			NotificationMessageList * p_message;

			p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
							"AccessPointToken", p_access_points->AccessPointInfo.token, NULL, NULL, 
							"State", "true", NULL, NULL);
			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}

			p_access_points = p_access_points->next;
		}
#endif
	}
	else if (soap_strcmp(topic, "Configuration/Area/Changed") == 0 || soap_strcmp(topic, "Configuration/Area/Removed") == 0)
	{
#ifdef PROFILE_C_SUPPORT
		AreaList * p_area = g_onvif_cfg.areas;
		while (p_area)
		{
			NotificationMessageList * p_message;

			if (soap_strcmp(topic, "Configuration/Area/Removed") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AreaToken", "testtoken", NULL, NULL, NULL, NULL, NULL, NULL);
			}
			else
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AreaToken", p_area->AreaInfo.token, NULL, NULL, NULL, NULL, NULL, NULL);
			}

			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}

			if (soap_strcmp(topic, "Configuration/Area/Removed") == 0)
			{
				break;
			}

			p_area = p_area->next;
		}
#endif
	}
	else if (soap_strcmp(topic, "Configuration/AccessPoint/Changed") == 0 ||
	    soap_strcmp(topic, "Configuration/AccessPoint/Removed") == 0)
	{
#ifdef PROFILE_C_SUPPORT
		AccessPointList * p_access_points = g_onvif_cfg.access_points;
		while (p_access_points)
		{
			NotificationMessageList * p_message;

			if (soap_strcmp(topic, "Configuration/AccessPoint/Removed") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AccessPointToken", "testtoken", NULL, NULL, NULL, NULL, NULL, NULL);
			}
			else
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AccessPointToken", p_access_points->AccessPointInfo.token, NULL, 
								NULL, NULL, NULL, NULL, NULL);
			}

			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}

			if (soap_strcmp(topic, "Configuration/AccessPoint/Removed") == 0)
			{
				break;
			}

			p_access_points = p_access_points->next;
		}
#endif
	}    	    
	else if (soap_strcmp(topic, "AccessControl/AccessGranted/Anonymous") == 0 || 
		soap_strcmp(topic, "AccessControl/AccessTaken/Anonymous") == 0 || 
		soap_strcmp(topic, "AccessControl/AccessNotTaken/Anonymous") == 0 || 
		soap_strcmp(topic, "AccessControl/Denied/Anonymous") == 0 || 
		soap_strcmp(topic, "AccessControl/Duress") == 0)
	{
#ifdef PROFILE_C_SUPPORT
		AccessPointList * p_accesspoint = g_onvif_cfg.access_points;
		if (p_accesspoint)
		{
			NotificationMessageList * p_message;

			if (soap_strcmp(topic, "AccessControl/Denied/Anonymous") == 0 || 
				soap_strcmp(topic, "AccessControl/Duress") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								 "AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								 "Reason", "CredentialNotEnabled", NULL, NULL);
			}
			else
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								 "AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								 NULL, NULL, NULL, NULL);
			}

			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}
	    }
#endif    	        
	}
	else if (soap_strcmp(topic, "AccessControl/AccessGranted/Credential") == 0 ||
		soap_strcmp(topic, "AccessControl/AccessGranted/Identifier") == 0 ||
		soap_strcmp(topic, "AccessControl/AccessTaken/Credential") == 0 ||
		soap_strcmp(topic, "AccessControl/AccessTaken/Identifier") == 0 ||
		soap_strcmp(topic, "AccessControl/AccessNotTaken/Credential") == 0 ||
		soap_strcmp(topic, "AccessControl/AccessNotTaken/Identifier") == 0 ||
		soap_strcmp(topic, "AccessControl/Denied/Credential") == 0 ||
		soap_strcmp(topic, "AccessControl/Denied/Identifier") == 0 ||
		soap_strcmp(topic, "AccessControl/Denied/CredentialNotFound") == 0 ||
		soap_strcmp(topic, "AccessControl/Denied/CredentialNotFound/Card") == 0)
	{
#if (defined(PROFILE_C_SUPPORT) && defined(CREDENTIAL_SUPPORT))
		AccessPointList * p_accesspoint = g_onvif_cfg.access_points;
		while (p_accesspoint)
		{
			BOOL next = 0;
			NotificationMessageList * p_message;

			if (soap_strcmp(topic, "AccessControl/Denied/Credential") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								"CredentialToken", g_onvif_cfg.credential->Credential.token, 
								"Reason", "CredentialNotActive");
			}
			else if (soap_strcmp(topic, "AccessControl/Denied/Identifier") == 0)
			{
				p_message = onvif_init_NotificationMessage4(topic, PropertyOperation_Invalid, 
								"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								"IdentifierType", "pt:Card", "FormatType", "GUID", 
								"IdentifierValue", "1A2B3C4D", "Reason", "CredentialNotActive");
			}
			else if (soap_strcmp(topic, "AccessControl/Denied/CredentialNotFound") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								"IdentifierType", "pt:Card", "IdentifierValue", "1A2B3C4D");
			}
			else if (soap_strcmp(topic, "AccessControl/Denied/CredentialNotFound/Card") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								"Card", "123234534534534", NULL, NULL);
			}
			else if (soap_strcmp(topic, "AccessControl/AccessGranted/Identifier") == 0 || 
				soap_strcmp(topic, "AccessControl/AccessTaken/Identifier") == 0 || 
				soap_strcmp(topic, "AccessControl/AccessNotTaken/Identifier") == 0)
			{
				next = 1;
				p_message = onvif_init_NotificationMessage4(topic, PropertyOperation_Invalid, 
								"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								"IdentifierType", "pt:Card", "FormatType", "GUID", "IdentifierValue", 
								"1A2B3C4D", NULL, NULL);
			}
			else
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"AccessPointToken", p_accesspoint->AccessPointInfo.token, NULL, NULL, 
								"CredentialToken", g_onvif_cfg.credential->Credential.token, NULL, NULL);
			}

			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}

			if (next)
			{
				p_accesspoint = p_accesspoint->next;
			}
			else
			{
				break;
			}
		}
#endif    	        
	}
	else if (soap_strcmp(topic, "Configuration/Door/Changed") == 0 || soap_strcmp(topic, "Configuration/Door/Removed") == 0)
	{
#ifdef PROFILE_C_SUPPORT
		DoorList * p_door = g_onvif_cfg.doors;
		while (p_door)
		{
			NotificationMessageList * p_message;

			if (soap_strcmp(topic, "Configuration/Door/Removed") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"DoorToken", "testtoken", NULL, NULL, NULL, NULL, NULL, NULL);
			}
			else
			{
				p_message = onvif_init_NotificationMessage3(topic, PropertyOperation_Initialized, 
								"DoorToken", p_door->Door.DoorInfo.token, NULL, NULL, NULL, NULL, 
								NULL, NULL);
			}

			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}

			if (soap_strcmp(topic, "Configuration/Door/Removed") == 0)
			{
				break;
			}

			p_door = p_door->next;
		}
#endif
	}
	else if (soap_strcmp(topic, "Door/State/DoorMode") == 0 || 
		soap_strcmp(topic, "Door/State/DoorPhysicalState") == 0 ||
		soap_strcmp(topic, "Door/State/LockPhysicalState") == 0 ||
		soap_strcmp(topic, "Door/State/DoubleLockPhysicalState") == 0 ||
		soap_strcmp(topic, "Door/State/DoorAlarm") == 0 ||
		soap_strcmp(topic, "Door/State/DoorTamper") == 0 ||
		soap_strcmp(topic, "Door/State/DoorFault") == 0)
	{
#ifdef PROFILE_C_SUPPORT
		DoorList * p_door = g_onvif_cfg.doors;
		while (p_door)
		{
			static int DoorPhysicalState = 1;
			static int LockPhysicalState = 1;
			static int DoubleLockPhysicalState = 1;
			static int DoorAlarm = 1;
			static int DoorTamper = 1;
			static int DoorFault = 1;
			const char * state = "";
			onvif_PropertyOperation op = PropertyOperation_Initialized;
			NotificationMessageList * p_message = NULL;

			if (soap_strcmp(topic, "Door/State/DoorMode") == 0)
			{
				state = onvif_DoorModeToString(p_door->DoorState.DoorMode);
			}
			else if (soap_strcmp(topic, "Door/State/DoorPhysicalState") == 0)
			{
				if (DoorPhysicalState)
				{
					DoorPhysicalState = 0;
					op = PropertyOperation_Changed;
				}

				state = onvif_DoorPhysicalStateToString(p_door->DoorState.DoorPhysicalState);
			}
			else if (soap_strcmp(topic, "Door/State/LockPhysicalState") == 0)
			{
				if (LockPhysicalState)
				{
					LockPhysicalState = 0;
					op = PropertyOperation_Changed;
				}

				state = onvif_LockPhysicalStateToString(p_door->DoorState.LockPhysicalState);
			}
			else if (soap_strcmp(topic, "Door/State/DoubleLockPhysicalState") == 0)
			{
				if (DoubleLockPhysicalState)
				{
					DoubleLockPhysicalState = 0;
					op = PropertyOperation_Changed;
				}

				state = onvif_LockPhysicalStateToString(p_door->DoorState.DoubleLockPhysicalState);
			}
			else if (soap_strcmp(topic, "Door/State/DoorAlarm") == 0)
			{
				if (DoorAlarm)
				{
					DoorAlarm = 0;
					op = PropertyOperation_Changed;
				}

				state = onvif_DoorAlarmStateToString(p_door->DoorState.Alarm);
			}
			else if (soap_strcmp(topic, "Door/State/DoorTamper") == 0)
			{
				if (DoorTamper)
				{
					DoorTamper = 0;
					op = PropertyOperation_Changed;
				}

				state = onvif_DoorTamperStateToString(p_door->DoorState.Tamper.State);
			}
			else if (soap_strcmp(topic, "Door/State/DoorFault") == 0)
			{
				if (DoorFault)
				{
					DoorFault = 0;
					op = PropertyOperation_Changed;
				}

				state = onvif_DoorFaultStateToString(p_door->DoorState.Fault.State);
			}

			if (soap_strcmp(topic, "Door/State/DoorFault") == 0)
			{
				p_message = onvif_init_NotificationMessage3(topic, op, "DoorToken", 
								p_door->Door.DoorInfo.token, NULL, NULL, "State", state, 
								"Reason", p_door->DoorState.Fault.Reason);
			}
			else
			{
				p_message = onvif_init_NotificationMessage3(topic, op, "DoorToken", 
								p_door->Door.DoorInfo.token, NULL, NULL, "State", state, 
								NULL, NULL);
			}

			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}

			if (op == PropertyOperation_Changed)
			{
				break;
			}
			p_door = p_door->next;
		}
#endif
	}    
	else if (soap_strcmp(topic, "Schedule/State/Active") == 0)
	{
#ifdef SCHEDULE_SUPPORT
		ScheduleList * p_schedule = g_onvif_cfg.schedule;
		while (p_schedule)
		{
			onvif_PropertyOperation op = PropertyOperation_Initialized;
			NotificationMessageList * p_message = NULL;

			if (p_schedule->ScheduleState.Active)
			{
				op = PropertyOperation_Changed;
			}
			else
			{
				op = PropertyOperation_Initialized;
			}

			p_message = onvif_init_NotificationMessage3(topic, op, 
							"ScheduleToken", p_schedule->Schedule.token, 
							"Name", p_schedule->Schedule.Name, 
							"Active", p_schedule->ScheduleState.Active ? "true" : "false", 
							"SpecialDay", p_schedule->ScheduleState.SpecialDay ? "true" : "false");
			if (p_message)
			{
				onvif_put_NotificationMessage(p_message);
			}

			p_schedule = p_schedule->next;
		}
#endif
	}
	else if (soap_strcmp(topic, "RecordingConfig/JobState") == 0)
	{
#ifdef PROFILE_G_SUPPORT
		RecordingJobList * p_recordingjob = g_onvif_cfg.recording_jobs;
		while (p_recordingjob)
		{
			onvif_RecordingJobStateNotify(p_recordingjob, PropertyOperation_Initialized);

			p_recordingjob = p_recordingjob->next;
		}
#endif
	}
	else if (soap_strcmp(topic, "RuleEngine/CountAggregation/Counter") == 0)
	{
#ifdef VIDEO_ANALYTICS
		NotificationMessageList * p_message = onvif_add_NotificationMessage(NULL);
		if (p_message)
		{
			SimpleItemList * p_simpleitem;
			onvif_NotificationMessage * p_msg = &p_message->NotificationMessage;
			
			strcpy(p_msg->Dialect, "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");
			strcpy(p_msg->Topic, "tns1:RuleEngine/CountAggregation/Counter");
			p_msg->Message.PropertyOperationFlag = 1;
			p_msg->Message.PropertyOperation = PropertyOperation_Initialized;
			p_msg->Message.UtcTime = time(NULL);

			p_msg->Message.SourceFlag = 1;

			p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "VideoSource");

				if (g_onvif_cfg.v_src_cfg)
				{
					strcpy(p_simpleitem->SimpleItem.Value, g_onvif_cfg.v_src_cfg->Configuration.token);
				}
			}

			p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Source.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Rule");

				if (g_onvif_cfg.va_cfg)
				{
					ConfigList * p_config = onvif_find_Config_by_type(g_onvif_cfg.va_cfg->Configuration.RuleEngineConfiguration.Rule, "tt:LineCounting");
					if (p_config)
					{
						strcpy(p_simpleitem->SimpleItem.Value, p_config->Config.Name);
					}
				}
			}

			p_msg->Message.DataFlag = 1;

			p_simpleitem = onvif_add_SimpleItem(&p_msg->Message.Data.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Count");
				strcpy(p_simpleitem->SimpleItem.Value, "1");
			}

			onvif_put_NotificationMessage(p_message);
		}
#endif
	}
	else
	{
		NotificationMessageList * p_message = onvif_init_NotificationMessage1(TRUE);
		if (p_message)
		{
			onvif_put_NotificationMessage(p_message);
		}

		p_message = onvif_init_NotificationMessage2();
		if (p_message)
		{
			onvif_put_NotificationMessage(p_message);
		}
	}
}

BOOL onvif_build_notify_alarm_motion_message(int alarm_level, char *alarm_data)
{
    char alarm_type = 0;        // 0-motion 1-PD 2-AI 3-custom
    EUA *p_eua = NULL;
    NotificationMessageList *p_message = NULL;

    p_eua = onvif_eua_lookup_start();
    while(p_eua)
    {
        // 根据不同的level和data生成不同的报警xml message
        if (p_message != NULL)
        {
            onvif_free_NotificationMessage(p_message);
        }

        alarm_type = 0;
        if (p_eua->clitype == EUA_CLIENT_TYPE_WEB && 
            alarm_level <= ALARM_AI_VEHICLE_BICYCLE &&
            alarm_data != NULL && (strstr(alarm_data, "Human") ||strstr(alarm_data, "human")) )
        {
            alarm_type = 1;
        }
        else if (p_eua->clitype == EUA_CLIENT_TYPE_WEB && 
            alarm_level > ALARM_AI_PD && alarm_level < ALARM_AI_MAX)
        {
            alarm_type = 2;
        }
        else if (p_eua->clitype == EUA_CLIENT_TYPE_WEB && 
            alarm_data != NULL && strlen(alarm_data) > 0 && (!strstr(alarm_data, "motion alarm")))
        {
            alarm_type = 3;
        }
        else
        {
            alarm_type = 0;
        }

        p_message = onvif_init_notification_motion_alarm_message(alarm_type, alarm_level, alarm_data);

        // 根据节点类型处理 message
        if (p_eua->pollMode)
        {
            if (h_list_get_number_of_nodes(p_eua->msg_list) >= 20) // max 20 notify message
            {
                LINKED_NODE * p_node = h_list_get_from_front(p_eua->msg_list);
                onvif_free_NotificationMessage((NotificationMessageList *) p_node->p_data);

                h_list_remove_from_front(p_eua->msg_list);
            }

            p_message->refcnt++;
            h_list_add_at_back(p_eua->msg_list, (void *)p_message);
        }
        else
        {
            onvif_notify(p_eua, p_message);
        }

        p_eua = onvif_eua_lookup_next(p_eua);
    }

    onvif_eua_lookup_stop();
    onvif_free_NotificationMessage(p_message);

    return TRUE;
}

