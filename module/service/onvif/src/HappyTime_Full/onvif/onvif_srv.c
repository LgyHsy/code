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
#include "hxml.h"
#include "xml_node.h"
#include "onvif_probe.h"
#include "http.h"
#include "http_srv.h"
#include "http_parse.h"
#include "onvif_device.h"
#include "onvif.h"
#include "onvif_timer.h"
#include "onvif_srv.h"
#include "onvif_event.h"
#include "onvif_notify.h"
#include "onvif_cfg.h"
#include "soap.h"
#include "cgi.h"
#include "para.h"
#ifdef HTTP_GET
#undef HTTP_GET
#endif
#ifdef HTTP_PUT
#undef HTTP_PUT
#endif
#include "anj_http.h"
// #include "system_msg.h"
#ifdef PROFILE_G_SUPPORT
#include "onvif_recording.h"
#endif
#ifdef HTTPD
#include "httpd.h"
#endif

/***************************************************************************************/
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;
extern int g_irctl_fd;//onvif控制ircut口控制灯光
extern MediaStreamConfig gStreamCfg;
extern int g_onvif_expand;							//宇视私有协议入口 **
/***************************************************************************************/

void onvif_http_data_cb(HTTPCLN *p_cln, char *buff, int buflen, void *userdata)
{
	(void)userdata;
    if (p_cln && buff && buflen > 0)
    {
        if (RCV_UPGRADEFIRMWARE == p_cln->rcv_mode)
        {
            http_upgrade_trans_firmware(p_cln);
        }
    }
}

BOOL onvif_http_msg_cb(HTTPCLN * p_cln, HTTPMSG * p_msg, void * p_userdata)
{
	if (p_msg)
	{
		OIMSG msg;
		memset(&msg, 0, sizeof(OIMSG));
		msg.msg_src = ONVIF_MSG_SRC;
		msg.msg_dua = (char *)p_cln;
		msg.msg_buf = (char *)p_msg;

		if (hqBufPut(g_onvif_cls.msg_queue, (char *)&msg) == FALSE)
		{
			log_print(HT_LOG_ERR, "%s, send rx msg to main task failed!!!\r\n", __FUNCTION__);
			return  FALSE;
		}
	}
	else if (p_cln)
	{
		OIMSG msg;
		memset(&msg, 0, sizeof(OIMSG));
		msg.msg_src = ONVIF_DEL_UA_SRC;
		msg.msg_dua = (char *)p_cln;

		if (hqBufPut(g_onvif_cls.msg_queue, (char *)&msg) == FALSE)
		{
			log_print(HT_LOG_ERR, "%s, send rx msg to main task failed!!!\r\n", __FUNCTION__);
			return  FALSE;
		}
	}
	return TRUE;
}

void onvif_http_msg_handler(OIMSG *stm)
{
	HTTPMSG * p_msg = (HTTPMSG *)stm->msg_buf;
	HTTPCLN * p_cln = (HTTPCLN *)stm->msg_dua;
	char * post = p_msg->first_line.value_string;

	if (strstr(post, "SystemRestore"))// must be the same with onvif_StartSystemRestore
	{
		soap_SystemRestore(p_cln, p_msg);
	}
	else if (strstr(post, "/FirmwareUpgrade"))
	{
		soap_FirmwareUpgrade(p_cln, p_msg);
	}
	else if (strstr(post, "snapshot"))
	{
		soap_GetSnapshot(p_cln, p_msg);
	}
	else if (strstr(post, "SystemLog"))
	{
		soap_GetHttpSystemLog(p_cln, p_msg);
	}
	else if (strstr(post, "AccessLog"))
	{
		soap_GetHttpAccessLog(p_cln, p_msg);
	}
	else if (strstr(post, "SupportInfo"))
	{
		soap_GetSupportInfo(p_cln, p_msg);
	}
	else if (strstr(post, "SystemBackup"))
	{
		soap_GetSystemBackup(p_cln, p_msg);
	}
	else if (p_msg->ctt_type == CTT_XML)
	{
		soap_process_request(p_cln, p_msg);
	}
#ifdef HTTPD
	else
	{
		//log_print(HT_LOG_INFO, "ready task_manager\n");
		task_manager(stm);
	}
	return ;
#endif
}

void onvif_httpd_on_auth(char * username, BOOL * need_auth, BOOL * user_exist, char * password, void * userdata)
{
	MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
	*need_auth = (pStreamCfg && pStreamCfg->webConfig.onvif_auth) ? TRUE : FALSE;
	onvif_User * p_user = onvif_find_user(username);
	if (p_user)
	{
		*user_exist = TRUE;
		strcpy(password, p_user->Password);
	}
	else
	{
		*user_exist = FALSE;
	}
	return ;
}

void * onvif_task(void * argv)
{
	OIMSG stm;
	while (1)
	{
		if (hqBufGet(g_onvif_cls.msg_queue, (char *)&stm))
		{
			HTTPCLN * p_cln = (HTTPCLN *)stm.msg_dua;
			switch (stm.msg_src)
			{
				case ONVIF_MSG_SRC:

					onvif_http_msg_handler(&stm);//ONVIF
					
					if (stm.msg_buf)
					{
						http_free_msg((HTTPMSG *)stm.msg_buf);
					}
					
					if (p_cln && !p_cln->keep_alive)
					{
						http_free_used_cln((HTTPSRV *)p_cln->http_srv, p_cln);
					}

					break;

				case ONVIF_DEL_UA_SRC:
					http_free_used_cln((HTTPSRV *)p_cln->http_srv, p_cln);
					break;
				
				case ONVIF_TIMER_SRC:
					onvif_timer();
					break;
				
				case ONVIF_EXIT:
					goto EXIT;
			}
		}
	}
	
EXIT:

	g_onvif_cls.tid_main = 0;
	return NULL;
}

void onvif_start_server()
{
	sys_buf_init(g_onvif_cfg.http_max_users);
	http_msg_buf_init(g_onvif_cfg.http_max_users);

	onvif_init();// net--	ip,	port

	log_print(HT_LOG_INFO, "Happytime onvif server version %s\n", ONVIF_VERSION_STRING);
	g_onvif_cls.msg_queue = hqCreate(g_onvif_cfg.http_max_users * 4, sizeof(OIMSG), HQ_GET_WAIT);	//消息队列生产
	if (g_onvif_cls.msg_queue == NULL)
	{
		log_print(HT_LOG_ERR, "%s, create task queue failed!!!\r\n", __FUNCTION__);
		return;
	}
	
	g_onvif_cls.tid_main = sys_os_create_thread((void *)onvif_task, NULL);	//onvif_task,	主循环取任务

	if (g_onvif_cfg.http_enable)
	{
		if (!http_srv_init(&g_onvif_cls.http_srv, NULL, g_onvif_cls.http_port, g_onvif_cfg.http_max_users, 0, NULL, NULL))
		{
			log_print(HT_LOG_ERR, "http server listen on http://%s:%u failed\n", g_onvif_cls.server_ip, g_onvif_cls.http_port);
		}
		else
		{
			http_set_msg_cb(&g_onvif_cls.http_srv, onvif_http_msg_cb, NULL);	//设置hqbufput回调到http_srv
			http_set_data_cb(&g_onvif_cls.http_srv, onvif_http_data_cb, NULL);
#ifdef IPFILTER_SUPPORT
			http_set_conn_cb(&g_onvif_cls.http_srv, onvif_http_conn_cb, NULL);
#endif
			log_print(HT_LOG_INFO, "Onvif server running at http://%s:%u (listen 0.0.0.0:%u)\n", g_onvif_cls.server_ip, g_onvif_cls.http_port, g_onvif_cls.http_port);
		}
	}
	
#ifdef HTTPS
	if (g_onvif_cfg.https_enable)
	{
		if (!http_srv_init(&g_onvif_cls.https_srv, NULL, g_onvif_cls.https_port, g_onvif_cfg.http_max_users, 1, g_onvif_cfg.cert_file, g_onvif_cfg.key_file))
		{
			log_print(HT_LOG_ERR, "https server listen on https://%s:%u failed\n", g_onvif_cfg.server_ip, g_onvif_cls.https_port);
		}
		else
		{
			http_set_msg_cb(&g_onvif_cls.https_srv, onvif_http_msg_cb, NULL);
#ifdef IPFILTER_SUPPORT
			http_set_conn_cb(&g_onvif_cls.https_srv, onvif_http_conn_cb, NULL);
#endif
			log_print(HT_LOG_INFO, "Onvif server running at https://%s:%u\n", g_onvif_cls.server_ip, g_onvif_cls.https_port);
		}
	}
#endif

#ifdef HTTPD
	g_onvif_cls.httpd = httpd_init();
	if (g_onvif_cls.httpd)
	{
		httpd_set_on_auth(g_onvif_cls.httpd, onvif_httpd_on_auth, NULL);
	}
#endif

	onvif_timer_init();
	onvif_start_discovery();

	onvif_event_mgr_init();
	sys_os_create_thread((void *)onvif_alarm_thr, NULL);	//报警发送

	return ;
}

void onvif_start()
{
	MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
	if (pStreamCfg == NULL || pStreamCfg->webConfig.enable_onvif <= 0)
	{
		log_print(HT_LOG_INFO, "onvif_start skipped: onvif=%d web=%d\n",
		       pStreamCfg ? pStreamCfg->webConfig.enable_onvif : 0,
		       pStreamCfg ? pStreamCfg->webConfig.enable_web : 0);
		return;
	}

	server_init_cfg();										/*初始化服务参数*/

	anj_http_init(http_response, copy_file);					/* CGI / UNV / HAPI + http处理 */

	onvif_init_def_cfg();

	onvif_init_cfg();										/*初始化官方接口参数*/

	onvif_start_server(); 							/*启动*/

	return;
}

void onvif_free_device()
{
	onvif_free_NetworkInterfaces(&g_onvif_cfg.network.interfaces);
	onvif_free_VideoSources(&g_onvif_cfg.v_src);
	onvif_free_VideoSourceConfigurations(&g_onvif_cfg.v_src_cfg);
	onvif_free_VideoEncoder2Configurations(&g_onvif_cfg.v_enc_cfg);
#ifdef AUDIO_SUPPORT
	onvif_free_AudioSources(&g_onvif_cfg.a_src);
	onvif_free_AudioSourceConfigurations(&g_onvif_cfg.a_src_cfg);
	onvif_free_AudioEncoder2Configurations(&g_onvif_cfg.a_enc_cfg);
	onvif_free_AudioDecoderConfigurations(&g_onvif_cfg.a_dec_cfg);
#endif 
	onvif_free_profiles(&g_onvif_cfg.profiles);
	onvif_free_OSDConfigurations(&g_onvif_cfg.OSDs);
	onvif_free_MetadataConfigurations(&g_onvif_cfg.metadata_cfg);

#ifdef MEDIA2_SUPPORT
	onvif_free_Masks(&g_onvif_cfg.mask);
#endif 

#ifdef PTZ_SUPPORT
	onvif_free_PTZNodes(&g_onvif_cfg.ptz_node);
	onvif_free_PTZConfigurations(&g_onvif_cfg.ptz_cfg);
#endif

#ifdef VIDEO_ANALYTICS
	onvif_free_VideoAnalyticsConfigurations(&g_onvif_cfg.va_cfg);
#endif

#ifdef PROFILE_G_SUPPORT
	onvif_free_Recordings(&g_onvif_cfg.recordings);
	onvif_free_RecordingJobs(&g_onvif_cfg.recording_jobs);
#endif

#ifdef PROFILE_C_SUPPORT
	onvif_free_AccessPoints(&g_onvif_cfg.access_points);
	onvif_free_Doors(&g_onvif_cfg.doors);
	onvif_free_Areas(&g_onvif_cfg.areas);
#endif

#ifdef DEVICEIO_SUPPORT
	onvif_free_VideoOutputs(&g_onvif_cfg.v_output);
	onvif_free_VideoOutputConfigurations(&g_onvif_cfg.v_output_cfg);
	onvif_free_AudioOutputs(&g_onvif_cfg.a_output);
	onvif_free_AudioOutputConfigurations(&g_onvif_cfg.a_output_cfg);
	onvif_free_RelayOutputs(&g_onvif_cfg.relay_output);
	onvif_free_DigitalInputs(&g_onvif_cfg.digit_input);
	onvif_free_SerialPorts(&g_onvif_cfg.serial_port);
#endif

#ifdef CREDENTIAL_SUPPORT
	onvif_free_Credentials(&g_onvif_cfg.credential);
#endif

#ifdef ACCESS_RULES
	onvif_free_AccessProfiles(&g_onvif_cfg.access_rules);
#endif

#ifdef SCHEDULE_SUPPORT
	onvif_free_Schedules(&g_onvif_cfg.schedule);
	onvif_free_SpecialDayGroups(&g_onvif_cfg.specialdaygroup);
#endif

#ifdef RECEIVER_SUPPORT
	onvif_free_Receivers(&g_onvif_cfg.receiver);
#endif

	return ;
}

void onvif_stop()
{
	if (1) //关闭onvif_task
	{
		OIMSG stm;
		memset(&stm, 0, sizeof(stm));
		stm.msg_src = ONVIF_EXIT;
		hqBufPut(g_onvif_cls.msg_queue, (char *)&stm);
	}
	
	while (g_onvif_cls.tid_main)
	{
		usleep(10*1000);
	}
	
	onvif_bye();
	onvif_stop_discovery();
	onvif_timer_deinit();
	
	http_srv_deinit(&g_onvif_cls.http_srv);
	
#ifdef HTTPS
	http_srv_deinit(&g_onvif_cls.https_srv);
#endif

	anj_http_uninit();

	hqDelete(g_onvif_cls.msg_queue);
	//onvif_save_cfg(RUNTIME_CONFIG_FILE);
	
	onvif_eua_deinit();
	
#ifdef PROFILE_G_SUPPORT
	onvif_FreeSearchs();
#endif

#ifdef HTTPD
	if (g_onvif_cls.httpd)
	{
		httpd_deinit(g_onvif_cls.httpd);
		g_onvif_cls.httpd = NULL;
	}
#endif

	onvif_free_device();
	sys_buf_deinit();
	http_msg_buf_deinit();
	log_close();
	
	return ;
}


