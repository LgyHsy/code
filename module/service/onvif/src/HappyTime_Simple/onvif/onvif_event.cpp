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

#include "sys_inc.h"
#include "onvif.h"
#include "onvif_event.h"
#include "http_cln.h"
#include "onvif_pkt.h"


extern ONVIF_CFG 	g_onvif_cfg;
extern ONVIF_CLS	g_onvif_cls;


void onvif_eua_init()
{
	g_onvif_cls.eua_fl = pps_ctx_fl_init(MAX_NUM_EUA, sizeof(EUA), TRUE);
	g_onvif_cls.eua_ul = pps_ctx_ul_init(g_onvif_cls.eua_fl, TRUE);
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
	EUA * p_eua = (EUA *)ppstack_fl_pop(g_onvif_cls.eua_fl);
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

	memset(p_eua, 0, sizeof(EUA));
	
	ppstack_fl_push(g_onvif_cls.eua_fl, p_eua);
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

EUA * onvif_get_eua_by_index(unsigned int index)
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

EUA * onvif_eua_lookup_by_addr(const char * addr)
{
	EUA * p_eua = onvif_eua_lookup_start();
	while (p_eua)
	{
		if (strcmp(p_eua->producter_addr, addr) == 0)
			break;
			
		p_eua = onvif_eua_lookup_next((void *)p_eua);
	}
	onvif_eua_lookup_stop();

	return p_eua;
}


int onvif_parse_xaddr(const char * pdata, char * host, char * url, int * port)
{
    const char *p1, *p2;
    int len = strlen(pdata);

    *port = 80;
    
    if (len > 7) // skip "http://"
    {
        p1 = strchr(pdata+7, ':');
        if (p1)
        {
            strncpy(host, pdata+7, p1-pdata-7);
            
            char buff[100];
            memset(buff, 0, 100);
            
            p2 = strchr(p1, '/');
            if (p2)
            {
                strncpy(url, p2, len - (p2 - pdata));
                
                len = p2 - p1 - 1;
                strncpy(buff, p1+1, len);                
            }
            else
            {
                len = len - (p1 - pdata);
                strncpy(buff, p1+1, len);
            }  

            *port = atoi(buff);
        }
        else
        {
            p2 = strchr(pdata+7, '/');
            if (p2)
            {
                strncpy(url, p2, len - (p2 - pdata));
                
                len = p2 - pdata - 7;
                strncpy(host, pdata+7, len);
            }
            else
            {
                len = len - 7;
                strncpy(host, pdata+7, len);
            }
        }
    }

    return 1;
}


ONVIF_RET onvif_Subscribe(Subscribe_REQ * p_Subscribe_req)
{
	EUA * p_eua = onvif_get_idle_eua();
	if (p_eua)
	{
		p_eua->init_term_time = p_Subscribe_req->init_term_time;
		strcpy(p_eua->consumer_addr, p_Subscribe_req->consumer_addr);

		onvif_parse_xaddr(p_eua->consumer_addr, p_eua->host, p_eua->url, &p_eua->port);

		sprintf(p_eua->producter_addr, "http://%s:%d/event_service/%u", g_onvif_cls.local_ipstr, g_onvif_cfg.serv_port, onvif_get_eua_index(p_eua));
		p_eua->subscibe_time = time(NULL);
		p_eua->last_renew_time = time(NULL);
		
		p_eua->used = 1;

		p_Subscribe_req->p_eua = p_eua;
		return ONVIF_OK;
	}
	
	return ONVIF_OK;
}

ONVIF_RET onvif_Renew(Renew_REQ * p_Renew_req)
{
	EUA * p_eua = onvif_eua_lookup_by_addr(p_Renew_req->refer_addr);
	if (p_eua)
	{
		p_eua->last_renew_time = time(NULL);
		
		return ONVIF_OK;
	}	

	return ONVIF_ERR_RESOURCE_UNKNOWN_FAULT;
}

ONVIF_RET onvif_Unsubscribe(const char * addr)
{
	EUA * p_eua = onvif_eua_lookup_by_addr(addr);
	if (p_eua)
	{
		onvif_free_used_eua(p_eua);

		return ONVIF_OK;
	}	

	return ONVIF_ERR_RESOURCE_UNKNOWN_FAULT;
}


void onvif_notify(EUA * p_eua)
{
	char bufs[4*1024];
	
	HTTPREQ	req;
	memset(&req, 0, sizeof(HTTPREQ));
	
	strcpy(req.host, p_eua->host);
	strcpy(req.url, p_eua->url);
	strcpy(req.action, "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify");
	req.port = p_eua->port;

	int len = build_Notify_xml(bufs, sizeof(bufs), (char *)p_eua);

	http_onvif_trans(&req, 5000, bufs, len);
}



