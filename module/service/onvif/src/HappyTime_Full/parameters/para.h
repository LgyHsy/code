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

#ifndef PARA_H
#define PARA_H
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <termios.h>
#include <linux/soundcard.h>
#include <poll.h>
#include <fcntl.h>
#include <netdb.h>

#include "curl/curl.h"
#include "cJSON.h"
// #include "debug_util.h"
// #include "msg_def.h"
// #include "bufmgr.h"
// #include "tps_msg.h"
// #include "msg_util.h"
// #include "func_util.h"
// #include "system_msg.h"
// #include "sys_info.h"
// #include "aux_msg.h"
// #include "net_config.h"
// #include "misc_util.h"
// #include "media_inf.h"
// #include "media_msg.h"
// #include "gpio_util.h"

// #include "data_struct.h"
// #include "check_util.h"
// #include "bufmgr.h"

#include "sys_inc.h"
#include "driver_interface.h"
// #include "config.h"
// #include "record_msg.h"
// #include "replay_util.h"
// #include "check_util.h"
// #include "cgi_def.h"

#include "anj_mw_time.h"
#include "anj_config_media.h"
#include "anj_config_alarm.h"
#include "alarm_link.h"

#include "anj_config.h"
#include "anj_sysmng.h"


#ifdef HTTPS
#include "openssl/ssl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
//////////////////////////////////////////////////////////////////////////
#define ONVIF_ALL_NET 1	//全网通开关，1 开启， 0 关闭						//1
#define DEFAULT_HOST_NAME "IPNC"								//1
#define DEFAULT_ONVIF_NAME "ONVIF_ICAMERA"						//1
#define DEFAULT_ONVIF_NONE "NONE"								//1

#define GUANG_IP "255.255.255.255"    //---> 你所在局域网的广播地址//0
#define GUANG_PORT 3707					//0

#define TIMELEN_FOR_ONVIF_ALL_NET_CLOSE (24*3600 * 1000)		//1

typedef struct
{
	int socket;
	char *eventMgrAddr;
}SOCKET_EVENT_MGR;

#define MAX_ALARM_DATA 220//128 //32->128 modified by 20090403
#define MAX_SNAP_FILE_PATH_LEN 96
typedef struct 
{
	SYSTEM_TIME	alarmtime;
	int         alarmcode; // AjAlarmCode
	int 		alarmflag;
	int			alarmlevel;
	char 		alarmdata[MAX_ALARM_DATA];
	char		snapfile[MAX_SNAP_FILE_PATH_LEN];
} ALARM_MSG_DATA;

typedef struct alarm_NVR_list
{
	int type;//type 1:通过pullmessage响应告警,2:通过订阅后再onvif_alarm_thr线程直接发送告警
	int  binit;
	char ipaddr[16];
	char strTimeMotion[128];
	char strTimeState[128];
	char strTimeAlarmIn[128];
	char strTimeIO_InSTATE[128];	
	char strTimeCall[128];	
	ALARM_MSG_DATA alarmData;
	int type1HasData;
	SOCKET_EVENT_MGR eventMgr;
	struct alarm_NVR_list *next,*prior;
}Alarm_NVR_list,*pAlarm_NVR_list;

#ifndef MAX_TOUR_CNT
#define MAX_TOUR_CNT 8
#endif

typedef struct tour_spot_s
{
	int stay_time;
	char preset_token[32];
	int speed;
}tour_spot_t;

typedef struct  tour_spot_entry_s
{
	tour_spot_t tour_spot;
	struct tour_spot_entry_s *next;
}tour_spot_entry_t;

typedef struct ptz_tour_s
{
    tour_spot_entry_t *p_spot_head;
    tour_spot_entry_t *p_spot_last;
    int spot_cnt;
    int free;
    char token[32];
    int index;
    int active;
} ptz_tour_t;

typedef struct ptz_tour_ctx_s
{
    ptz_tour_t ptz_tours[MAX_TOUR_CNT];
    int b_stop_tour;
    int b_tour_running;
    pthread_t tour_thrd_id;
} ptz_tour_ctx_t;

//////////////////////////////////////////////////////////////////////////
extern int g_ExistWifi;
extern int g_product_type; //型号参数
extern int g_stereo;
extern unsigned int g_motion_mode;
extern int g_openeye_smw;
extern int g_absolute_move;
extern char g_tptz_tourtoken[16];
extern int g_tptz_start;
extern char g_ifname[256];
extern pthread_mutex_t g_mclock;
extern int g_yen_version_sdm;
extern int g_yen_version_smd;
extern int g_yen_version_ultramotion_clock;
extern WIFIApConfig g_wifiapCfg;
extern MediaStreamConfig gStreamCfg;

extern MediaConfig	gMediaCfg;

int start_recv_data(int c);
void thread_fun(void* arg);
void *recv_cgi_audio(void *arg);

void server_init_cfg();


void *onvif_allnet_timer_thr(void *arg); 							//1
char *get_my_ifname_static(void); 									//1

pAlarm_NVR_list alarmlist_init(void);
void alarmlist_insert(pAlarm_NVR_list node,pAlarm_NVR_list new_node, int bLock);
void alarmlist_del(pAlarm_NVR_list head, pAlarm_NVR_list node, int bLock);
pAlarm_NVR_list alarmlist_has_ip(pAlarm_NVR_list node,char *ipStr, int bLock);
int GetVideoStreamResSize(RESOLUTION_ENTRY *pEntry, int nrescount, int stream_type);

#ifdef __cplusplus
}
#endif

#endif


