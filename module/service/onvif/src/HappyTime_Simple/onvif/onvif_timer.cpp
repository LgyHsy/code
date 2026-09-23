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
#include "onvif_timer.h"


/*******************************************************************/
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;

static int g_timer_cnt;

/*******************************************************************/
void onvif_timer()
{
	unsigned int i;
	time_t cur_time = time(NULL);
	EUA * p_eua = NULL;

	int max_renew_time;

	g_timer_cnt++;
	
	for (i=0; i<MAX_NUM_EUA; i++)
	{
		p_eua = (EUA *) onvif_get_eua_by_index(i);
		if (p_eua == NULL || p_eua->used == 0)
		{
			continue;
		}

		max_renew_time = (p_eua->init_term_time > g_onvif_cfg.evt_renew_time ? p_eua->init_term_time : g_onvif_cfg.evt_renew_time);
		
		/* timer check */
		if ((cur_time - p_eua->last_renew_time) > max_renew_time + max_renew_time/2)
		{
			onvif_free_used_eua(p_eua);
			continue;
		}

		if (g_onvif_cfg.evt_sim_flag && (g_timer_cnt % g_onvif_cfg.evt_sim_interval) == 0)
		{
			onvif_notify(p_eua);
		}
	}
}


#if	__WIN32_OS__

#pragma comment(lib, "winmm.lib")

void CALLBACK onvif_win_timer(UINT uID,UINT uMsg,DWORD dwUser,DWORD dw1,DWORD dw2)
{
	onvif_timer();
}

void onvif_timer_init()
{
	g_onvif_cls.timer_id = timeSetEvent(1000,0,onvif_win_timer,0,TIME_PERIODIC);
}

void onvif_timer_deinit()
{
	timeKillEvent(g_onvif_cls.timer_id);
}

#else

void * onvif_timer_task(void * argv)
{
	struct timeval tv;	
	
	while (g_onvif_cls.sys_timer_run == 1)
	{		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		select(1,NULL,NULL,NULL,&tv);
		
		onvif_timer();
	}

	g_onvif_cls.timer_id = 0;
	
	printf("onvif timer task exit\r\n");
	return NULL;
}

void onvif_timer_init()
{
	g_onvif_cls.sys_timer_run = 1;

	pthread_t tid = sys_os_create_thread((void *)onvif_timer_task, NULL);
	if (tid == 0)
	{
		printf("onvif_timer_init::pthread_create pp_timer_task\r\n");
		return;
	}

    g_onvif_cls.timer_id = (unsigned int)tid;

	printf("create onvif timer thread sucessful\r\n");
}

void onvif_timer_deinit()
{
	g_onvif_cls.sys_timer_run = 0;
	while (g_onvif_cls.timer_id != 0)
	{
		usleep(1000);
	}
}

#endif



