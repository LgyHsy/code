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
#include "anj_mw_comm.h"
#include "anj_mw_net.h"
#include "para.h"
// #include "cgi_def.h"

/////////////////////////////////////////////
// #include "bufmgr.h"
#if ONVIF_ALL_NET												//1
#include "arp_get_mac.h" 
#endif
#ifdef CURL_SUPPORT
#include "curl/curl.h"
#endif
#include "cJSON.h"
#include <time.h>
#include "driver_interface.h"
// #include "utf8_s_t_gb2312.h"
// #include "sys_info.h"
#include "onvif_extool.h"
#include "protocol_queue.h"
#ifndef PROCESS_PRIO_WEB
#define PROCESS_PRIO_WEB 0
#endif

static int g_flag_tran = 0;

in_addr_t g_gateway_addr; //ip检测等使用
in_addr_t g_ip_addr;

#if ONVIF_ALL_NET
char NVR_IP[10][16]={{0}};																					//1
#endif
char g_SN[20] = {0}; 																						//1
char g_uuid[64] = {0}; 																						//1
char *group[] = {"Administrator", "Operator", "User", "Anonymous", "Extended"};								//1.

char g_HX_Authorization[128] = {"wtU2/EcgAxlmEQLLtYRtQMHJJhzRa/mJc7LpCQ4odLbBsh/6fVcfvaOiarp+KNc8lg1B7G4FzSKvzqa4rD9TpA=="};
char g_HX_stcd[64] = {"0160500003"};
char g_HX_URL[128] = {"http://10.52.1.25:8090/index/Fileuploads/putServerFile"};
int g_init_flag_hx = 0;
char msg_data[1024] = {0}; //汇讯回调 //1
int g_allnet_allocated = 0;//全网通已被设置标准 							//1
unsigned int  tLastSetTime[3] = {0, 0, 0};						//1
HttpsPrivateStruct  g_HttpsPrivateInfo; 						//1 https信息
OnvifOemStruct g_OemInfo; 										//1
AjOemStruct g_AJoemInfo;
int g_onvif_expand = 0;
ptz_tour_ctx_t g_tour_ctx = {0};
int isHBVersion = 0;
int g_hasIO = 0;
int g_isHxVersion = 0;
int g_isTTVersion = 0;
int g_bHxVersion = 0;
int g_http_alarm = 0;
int is_Y_Version = 0;
int g_ExistWifi = 0;
int g_hasSMART = 1; // SMART ENABLE
int g_hasSPD = 1; // SPD ENABLE
int g_hasSCAR = 1; // SCAR ENABLE

FRAME_BUFFER_MANAGER g_onvif_event_mgr; 						//1
int g_onvif_event_busy = 0; 									//1
char searchdomainname[MID_INFO_LENGTH] = "ipnc";

/////////////////////////////////////////////

//*********************************************************
#define NTP_PATH "/mnt/nand/ntpmsg"						//1

MediaConfig gMediaCfg;							//1
int g_product_type = 0;
int g_stereo = 0;
unsigned int g_motion_mode = 0;
int g_openeye_smw = 0;
int g_absolute_move = 0;
char g_tptz_tourtoken[16] = {0};
int g_tptz_start = 0;
char g_ifname[256] = {0};
pthread_mutex_t g_mclock = PTHREAD_MUTEX_INITIALIZER;
int g_yen_version_sdm = 0;
int g_yen_version_smd = 0;
int g_yen_version_ultramotion_clock = 0;
extern char g_SN[20];						//1
MediaStreamConfig gStreamCfg;							//1
extern int g_https_support;										//1
extern int g_Yen_Ys_old;//判断是否为延创兴对接宇视老版本						//1
extern int g_hasSMART;//有智能				//1
extern int g_hasSPD;//有人				//1
extern int g_hasSCAR;//有车				//1
extern int g_hasFACE;//有人脸				//1
extern int g_hasGATE;//有越界				//1
extern int g_hasREGION;//有区域			//1
extern int g_onvif_fastupgrad;//升级返回快速返回								//1
extern unsigned int g_link_endtime;//链接时间内，不报警推送						//1
extern unsigned int g_linking_mode;//链接触发								//1
extern int g_linking_mode_cdto_get_cap;									//1
extern int g_linking_mode_cdto_sub;										//1
extern unsigned int g_disable_osd;										//1
extern int g_onvif_expand;//宇视私有协议入口									//1
extern int g_plug_by_play;//宇视协议即插即用									//1
extern int g_snap_order;												//1
extern int g_hasIO;
extern int g_bHxVersion;
//----------------------other------------------------------ 
int GetVideoStreamResSize(RESOLUTION_ENTRY *pEntry, int nrescount, int stream_type)
{
	int i;
	int nCount = 0;
	for(i = 0; i < nrescount; i ++)
	{
		if( pEntry[i].stream_type == stream_type )
		{
			nCount++;
		}
	}
	return nCount;
}

float g_tptz_value_x;								//1
float g_tptz_value_y;								//1
float g_tptz_value_z;								//1


int thread_pool_block_num = 0;
int canSubscribe_pullmessages = 1;
char eventstatus[3] = "";

#define TOPIC_EXP_LENGTH 1024
char topicExpression[TOPIC_EXP_LENGTH];
//*********************************************************
pthread_mutex_t g_AlarmList_mutex = PTHREAD_MUTEX_INITIALIZER;
pAlarm_NVR_list g_Alarm_NVR_list = NULL;
pAlarm_NVR_list g_Onvif_Client_list = NULL;

pAlarm_NVR_list alarmlist_init(void)
{
	pAlarm_NVR_list node=(pAlarm_NVR_list)malloc(sizeof(Alarm_NVR_list));
	if(node==NULL)
	{
		log_print(HT_LOG_ERR, "node is NULL!\n");
		return NULL;
	}
	memset(node,0,sizeof(Alarm_NVR_list));
	node->next=node->prior=node;
	return node;
}

void alarmlist_insert(pAlarm_NVR_list node,pAlarm_NVR_list new_node, int bLock)
{	
	if(node == NULL)
		return ;

	if(bLock > 0)
		pthread_mutex_lock(&g_AlarmList_mutex);

	if(node == NULL || new_node == NULL)
	{
		if(bLock > 0)
			pthread_mutex_unlock(&g_AlarmList_mutex);		
		return;
	}

	new_node->prior=node->prior;
	node->prior->next=new_node;
	new_node->next=node;
	node->prior=new_node;

	if(bLock > 0)
		pthread_mutex_unlock(&g_AlarmList_mutex);	
}

void alarmlist_del(pAlarm_NVR_list head, pAlarm_NVR_list node, int bLock)
{
	if(head == NULL || node == NULL)
		return ;

	if(bLock > 0)
		pthread_mutex_lock(&g_AlarmList_mutex);

	if(node == NULL)
	{
		if(bLock > 0)	
			pthread_mutex_unlock(&g_AlarmList_mutex);		
		return;
	}

	pAlarm_NVR_list tmpnode = head->next; 

	while(tmpnode && tmpnode != head)
	{
		if(tmpnode == node)
			break;

		tmpnode = tmpnode->next;
	}

	if(tmpnode && tmpnode != head)
	{
		if(node->prior)
			node->prior->next=node->next;

		if(node->next)
			node->next->prior=node->prior;

		free(node);
	}
	
	if(bLock > 0)
		pthread_mutex_unlock(&g_AlarmList_mutex);		
}

int alarmlist_find(pAlarm_NVR_list head, pAlarm_NVR_list node, int bLock)
{
	int findnode = -1;

	if(head == NULL || node == NULL)
		return -1;

	if(bLock > 0)
		pthread_mutex_lock(&g_AlarmList_mutex);

	if(node == NULL)
	{
		if(bLock > 0)	
			pthread_mutex_unlock(&g_AlarmList_mutex);		
		return -1;
	}

	pAlarm_NVR_list tmpnode = head->next; 

	while(tmpnode && tmpnode != head)
	{
		if(tmpnode == node)
			break;

		tmpnode = tmpnode->next;
	}

	if(tmpnode && tmpnode != head)
	{
		findnode = 1;
	}
	
	if(bLock > 0)
		pthread_mutex_unlock(&g_AlarmList_mutex);

	return findnode;
}


pAlarm_NVR_list alarmlist_has_ip(pAlarm_NVR_list node,char *ipStr, int bLock) 
{
	pAlarm_NVR_list p;

	if(node == NULL)
		return NULL;

	if(bLock > 0)
		pthread_mutex_lock(&g_AlarmList_mutex);		
	
	if(node == NULL)
	{
		if(bLock > 0)	
			pthread_mutex_unlock(&g_AlarmList_mutex);		
		return NULL;
	}
	
	for(p = node->next;p!=node;p=p->next)
	{
		if(!strcmp(p->ipaddr,ipStr))
		{
			if(bLock > 0)
				pthread_mutex_unlock(&g_AlarmList_mutex);		
			
			return p;
		}
	}

	if(bLock > 0)
		pthread_mutex_unlock(&g_AlarmList_mutex);		
	
	return NULL;
}

//*********************************************************
void *onvif_allnet_timer_thr(void *arg)
{
	pthread_detach(pthread_self());

	unsigned int start_ms = GetCurrentTimeStamp();
	unsigned int now = 0;
    LANConfig *pLan = (LANConfig *)getNetWorkConfig();
	while (pLan->onvifAllnetEnable)
	{
		now = GetCurrentTimeStamp();

		unsigned int pasttime = now-start_ms;
		//	距离上次开启全网通的时间超过24小时, 关闭全网通
		if((now-start_ms) > TIMELEN_FOR_ONVIF_ALL_NET_CLOSE)
		{
			log_print(HT_LOG_INFO, "timer 24h close onvif_allnet\n");
			pLan->onvifAllnetEnable=0;
			break;
		}

		int day = -1;
		int hour = -1;
		int minute = -1;
		unsigned int time_run;	// in sec

		time_run = pasttime / 1000;
		minute	= (time_run / 60) % 60;
		hour	= (time_run / (60 * 60)) % 24;
		day		= time_run / (60 * 60 * 24);

		log_print(HT_LOG_INFO, "ONVIF ALL NET PAST %d seconds (%d day %d hour %d minute).", 
			time_run, day, hour, minute );
		sleep(60); //sleep 1 min
	}

	pthread_exit(0);
	return NULL;
}

//开始接收Audio数据函数
int start_recv_data(int c)
{
	int recv_size = 0;//收了多少个字节

	int fd = c;
	if( fd < 0 )
	{
		log_print(HT_LOG_ERR, "fd %d invalid.", fd);
		return -1;
	}

	int buflen = 320;
	char recvbuffer[320];
	struct timeval wait_time;
	int	maxFd = 0;
	int selectret = 0;
	fd_set	readSet;
	maxFd = c;

	while(g_flag_tran == 1)
	{
		FD_ZERO(&readSet);
		FD_SET(fd, &readSet);

		wait_time.tv_sec   = 1;
		wait_time.tv_usec  = 0;
		do
		{
			selectret = select(maxFd + 1, &readSet, NULL, NULL, &wait_time);
		}while(selectret < 0 && EINTR == errno );

		if( selectret == 0 )
		{
			log_print(HT_LOG_ERR, "select time out");
			continue;
		}
		else if( selectret <= 0 )
		{
			log_print(HT_LOG_ERR, "select < 0.");
			usleep(1000);
			continue;
		}

		int remainlen = buflen - recv_size;
		int retsize = recv(fd, recvbuffer+recv_size, remainlen, 0);
//		log_print(HT_LOG_ERR, "tcp receive data length %d, buffer size %d.", retsize, remainlen);
		if (retsize == remainlen)			
		{
			// todo
			// talk_playdata((char*)recvbuffer, buflen);
			recv_size = 0;
		}
		else if( retsize > remainlen )
		{
			log_print(HT_LOG_ERR, "error recvlen %d", retsize);
			recv_size = 0;			
		}
		else if( retsize > 0 && retsize < remainlen)
		{
			recv_size += retsize;
		}
		else if( retsize == 0 )
		{		
		}
		else
		{
			log_print(HT_LOG_ERR, "recv=%d, socket closed.", retsize);
		}
	}

	return 0;
}

void thread_fun(void* arg)
{
	pthread_detach(pthread_self());
	int c = (int)arg;
	//保存http应答头部信息
	char Http[2048] = "HTTP/1.1 200 OK\r\nAccept-Ranges: bytes\r\n\
	Content-Length: 13\r\nConnection: close\r\n\\r\n";
	
	//客户端要接收的内容，与头部中的Content-Length对应
	char buff[128] = "0\nOK\n";//发送的数据

	strcpy(Http, buff);
	char tmp[1024] = {0};//接受客户端请求
	recv(c, tmp, 1023, 0);
	
	if (strstr(tmp, "Audio_start") != NULL)
	{
		log_print(HT_LOG_INFO, "Play Audio_start\n");
		g_flag_tran = 1;
		send(c, Http, strlen(Http), 0);//向客户端应答
		start_recv_data(c);
	}
	else if(strstr(tmp, "Audio_end") != NULL)
 	{
 		log_print(HT_LOG_INFO, "Play Audio_end\n");
 		g_flag_tran = 0;
		send(c, Http, strlen(Http), 0);
	}
	close(c);//每个客户端只接收一次就关闭
	pthread_exit(0);
}

void *recv_cgi_audio(void *arg)
{
	// todo
	// OpenTalk(WEB_PROCESS_KEY);//进程号打开对接通知
	/*AVTECH定制对接,起888端口接收*/
	log_print(HT_LOG_INFO, "recv_cgi_audio\n");
	pthread_detach(pthread_self());
	//1.
	int sockfd  = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		log_print(HT_LOG_ERR, "socket error: %s\r\n", strerror(errno));
		return NULL;
	}
	//2.
	struct sockaddr_in caddr;//ipv4地址结构
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(888);
	addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	int ret = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
	if(ret < 0){
		log_print(HT_LOG_ERR, "bind error: %s\r\n", strerror(errno));
		close(sockfd);
		return NULL;
	}
	//3.监听
	ret = listen(sockfd, 5);
	if(ret < 0){
		log_print(HT_LOG_ERR, "listen error: %s\r\n", strerror(errno));
		close(sockfd);
		return NULL;
	}
	
	socklen_t len = sizeof(caddr);
	int clientfd;
	while(1)
	{
		// accept阻塞等待
		clientfd = accept(sockfd, (struct sockaddr*)&caddr, &len);
		if (clientfd < 0) 
			log_print(HT_LOG_ERR, "accept perror: %s\r\n", strerror(errno));
		log_print(HT_LOG_INFO, "accept(ip:%s,port:%d) c = %d\n", 
			inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port), clientfd);
		log_print(HT_LOG_INFO, "tcp request +1\n");
		//pthread_t id;
		//pthread_create(&id,NULL,thread_fun,(void*)clientfd);
		{
			pthread_t tid = 0;
			anj_thread_create(&tid, 0, "audio_cli",
							  (void *(*)(void *))thread_fun,
							  (void*)clientfd, 1);
		}
	}
	//循环读取data
	pthread_exit(0);
}

void server_init_cfg()
{
	setpriority(PRIO_PROCESS, 0, PROCESS_PRIO_WEB);

	// 获取OEM信息
	anj_config_oem_onvif_get(&g_OemInfo);
	anj_config_oem_get(&g_AJoemInfo);

	// 获取媒体流配置
	MediaStreamConfig *pTmpMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
	if (pTmpMediaStreamConfig == NULL)
	{
		log_print(HT_LOG_ERR, "getMediaStreamConfig failed!\n");
		return;
	}
	memcpy(&gStreamCfg, pTmpMediaStreamConfig, sizeof(gStreamCfg));
	log_print(HT_LOG_INFO, "rtsp_auth=%d, onvif_auth=%d, videoport=%d, "
		   "rtpoverrtsp=%d, ptzport=%d, webport=%d\n",
			gStreamCfg.rtspConfig.rtsp_auth,
			gStreamCfg.webConfig.onvif_auth,
			gStreamCfg.rtspConfig.videoPort,
			gStreamCfg.rtspConfig.rtpoverrtsp,
			gStreamCfg.commConfig.ptzPort,
			gStreamCfg.webConfig.webPort);
	if ((gStreamCfg.rtspConfig.videoPort <= 0) || 
	    (gStreamCfg.rtspConfig.videoPort > 65535))
	{
		log_print(HT_LOG_ERR, "invalid videoPort: %d\n", 
			gStreamCfg.rtspConfig.videoPort);
		return;
	}
	if ((gStreamCfg.webConfig.webPort <= 0) || 
	    (gStreamCfg.webConfig.webPort > 65535))
	{
		log_print(HT_LOG_ERR, "invalid webPort: %d\n", 
			gStreamCfg.webConfig.webPort);
		return;
	}
	
	// 获取媒体配置
	MediaConfig *pTmpMediaConfig = (MediaConfig *)getMediaConfig();
	if (pTmpMediaConfig == NULL)
	{
		log_print(HT_LOG_ERR, "getMediaConfig failed!\n");
		return;
	}
	memcpy(&gMediaCfg, pTmpMediaConfig, sizeof(gMediaCfg));

	// 获取网络配置
	LANConfig *pTmpLanConfig = (LANConfig *)getNetWorkConfig();
	if (pTmpLanConfig == NULL)
	{
		log_print(HT_LOG_ERR, "getNetWorkConfig failed!\n");
		return;
	}
	// 判断是否有无线网卡
	g_ExistWifi = is_network_device_exist(net_get_wireless_name());
	log_print(HT_LOG_INFO, "DHCP ENABLE:%d, ONVIF ALL NET: %d\n",
		pTmpLanConfig->dhcpEnable, pTmpLanConfig->onvifAllnetEnable);

	return ;
}
