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
#include "onvif_timer.h"

#ifdef SCHEDULE_SUPPORT
#include "onvif_schedule.h"
#endif

#ifdef PROFILE_G_SUPPORT
#include "onvif_recording.h"
#endif

/**************************************************************************************/

extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;

#define ONVIF_MAX_TIMER_NUMS    32

/**************************************************************************************/

void timer_init()
{
	g_onvif_cls.timer_fl = pps_ctx_fl_init(ONVIF_MAX_TIMER_NUMS, sizeof(TIMER_UA), TRUE);
	g_onvif_cls.timer_ul = pps_ctx_ul_init(g_onvif_cls.timer_fl, TRUE);
}

void timer_deinit()
{
	if (g_onvif_cls.timer_ul)
	{
		pps_ul_free(g_onvif_cls.timer_ul);
		g_onvif_cls.timer_ul = NULL;
	}

	if (g_onvif_cls.timer_fl)
	{
		pps_fl_free(g_onvif_cls.timer_fl);
		g_onvif_cls.timer_fl = NULL;
	}
}

int timer_add(uint32 interval, timer_func func, void * param, int mode)
{
	TIMER_UA * p_tua = (TIMER_UA *)pps_fl_pop(g_onvif_cls.timer_fl);
	if (p_tua)
	{
		memset(p_tua, 0, sizeof(TIMER_UA));

		p_tua->interval = interval;
		p_tua->func = func;
		p_tua->mode = mode;
		p_tua->param = param;
		p_tua->start = time(NULL);

		pps_ctx_ul_add(g_onvif_cls.timer_ul, p_tua);

		return pps_get_index(g_onvif_cls.timer_ul, p_tua);
	}

	return -1;
}

void timer_del(int idx)
{
	TIMER_UA * p_tua;

	if (idx < 0 || idx >= ONVIF_MAX_TIMER_NUMS)
	{
		return;
	}

	p_tua = (TIMER_UA *) pps_get_node_by_index(g_onvif_cls.timer_ul, idx);

	pps_ctx_ul_del(g_onvif_cls.timer_ul, p_tua);
	
	memset(p_tua, 0, sizeof(TIMER_UA));
	
	pps_fl_push(g_onvif_cls.timer_fl, p_tua);
}

void timer_task()
{
	time_t curtime = time(NULL);

	TIMER_UA * p_tua = (TIMER_UA *)pps_lookup_start(g_onvif_cls.timer_ul);
	while (p_tua)
	{
		if (curtime - p_tua->start >= p_tua->interval)
		{
			if (p_tua->func)
			{
				p_tua->func(p_tua->param);
			}

			if (p_tua->mode == TIMER_MODE_REPEAT)
			{
				p_tua->start = curtime;
			}
			else
			{
				TIMER_UA * p_next = (TIMER_UA *)pps_lookup_next(g_onvif_cls.timer_ul, p_tua);

				pps_ctx_ul_del_unlock(g_onvif_cls.timer_ul, p_tua);

				p_tua = p_next;
			}
		}

		p_tua = (TIMER_UA *)pps_lookup_next(g_onvif_cls.timer_ul, p_tua);
	}
	pps_lookup_end(g_onvif_cls.timer_ul);
}

/**************************************************************************************/

void onvif_timer()
{
	timer_task();

	onvif_event_renew();

#ifdef SCHEDULE_SUPPORT
	onvif_DoSchedule();
#endif

#ifdef PROFILE_G_SUPPORT
	onvif_SearchTimeoutCheck();
#endif
}


#if	__WINDOWS_OS__

#pragma comment(lib, "winmm.lib")

#ifdef _WIN64
void CALLBACK onvif_win_timer(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
#else
void CALLBACK onvif_win_timer(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
#endif
{
	OIMSG msg;
	memset(&msg, 0, sizeof(OIMSG));
	
	msg.msg_src = ONVIF_TIMER_SRC;
	
	hqBufPut(g_onvif_cls.msg_queue, (char *)&msg);
}

void onvif_timer_init()
{
	timer_init();

	g_onvif_cls.timer_id = timeSetEvent(1000, 0, onvif_win_timer, 0, TIME_PERIODIC);
}

void onvif_timer_deinit()
{
	timeKillEvent(g_onvif_cls.timer_id);

	timer_deinit();
}

#else

void * onvif_timer_task(void * argv)
{
	struct timeval tv;	
	OIMSG msg;
	memset(&msg, 0, sizeof(OIMSG));
	
	while (g_onvif_cls.sys_timer_run == 1)
	{
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		select(1, NULL, NULL, NULL, &tv);
		
		msg.msg_src = ONVIF_TIMER_SRC;
		
		hqBufPut(g_onvif_cls.msg_queue, (char *)&msg);
	}

	g_onvif_cls.timer_id = 0;
	
	log_print(HT_LOG_DBG, "onvif timer task exit\r\n");

	return NULL;
}

void onvif_timer_init()
{
	pthread_t tid;

	timer_init();

	g_onvif_cls.sys_timer_run = 1;

	tid = sys_os_create_thread((void *)onvif_timer_task, NULL);
	if (tid == 0)
	{
		log_print(HT_LOG_ERR, "%s, create onvif_timer_task failed\r\n", __FUNCTION__);
		return;
	}

	g_onvif_cls.timer_id = tid;

	log_print(HT_LOG_DBG, "create onvif timer thread sucessful\r\n");
}

void onvif_timer_deinit()
{
	g_onvif_cls.sys_timer_run = 0;

	while (g_onvif_cls.timer_id != 0)
	{
		usleep(10*1000);
	}

	timer_deinit();
}

#endif



