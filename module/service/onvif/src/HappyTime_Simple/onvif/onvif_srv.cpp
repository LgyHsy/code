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
#include "hxml.h"
#include "xml_node.h"
#include "onvif_probe.h"
#include "http.h"
#include "onvif_device.h"
#include "onvif.h"
#include "onvif_timer.h"
#include "onvif_api.h"


static HTTPSRV hsrv;
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;

void onvif_traning()
{
	if (strcmp(g_onvif_cls.local_ipstr, g_onvif_cfg.serv_ip) != 0)
	{
		usleep(2000 * 1000);
		onvif_stop();
		usleep(1000 * 1000);
		onvif_start();
		
		/*http_srv_deinit(&hsrv);
		memset(g_onvif_cls.local_ipstr, 0, 32);
		strcpy(g_onvif_cls.local_ipstr, lanCfg.IPAddress);
		usleep(1000 * 2000);
		http_srv_init(&hsrv, inet_addr(g_onvif_cls.local_ipstr), g_onvif_cls.local_port, 16);*/
	}
	
	return ;
}

void onvif_start()
{
	printf("Happytime onvif server version 2.6\r\n");
	
	onvif_init();

	printf("Onvif server running at %s:%d\r\n", g_onvif_cls.local_ipstr, g_onvif_cls.local_port);

	sys_buf_init();
	
	http_msg_buf_fl_init(16);
	
	http_srv_init(&hsrv, inet_addr(g_onvif_cls.local_ipstr), g_onvif_cls.local_port, 16);

	onvif_timer_init();

	onvif_start_discovery();
	
	return ;
}

void onvif_stop()
{
	onvif_stop_discovery();

	onvif_timer_deinit();

	http_srv_deinit(&hsrv);
	
	http_msg_buf_fl_deinit();
	
	sys_buf_deinit();
	
	return ;
}


