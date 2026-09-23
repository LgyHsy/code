#include "sys_inc.h"
#include "onvif_notify.h"
#include "onvif_extool.h"
#include "onvif_event.h"
// #include "media_cfg.h"
// #include "bufmgr.h"

#undef HTTP_GET
#undef HTTP_PUT
#include "http_def.h"
#include "webpost_handle.h"
#include "hapi_subs.h"
#include "unv_subs.h"
#include "onvif.h"
// #include "UNV_subs.h"
// #include "UNVpd_handle.h"
#ifdef CURL_SUPPORT
#include "curl/curl.h"
#endif

#include "cJSON.h"
#include "anj_mw_time.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "alarm_link.h"
// #include "media_cfg.h"

static int onvif_extract_xml_tag_local(const char *xml, const char *tag, char *out, size_t out_len)
{
	char open_tag[64] = {0};
	char close_tag[64] = {0};
	const char *start = NULL;
	const char *end = NULL;
	size_t len;

	if (!xml || !tag || !out || out_len == 0)
	{
		return -1;
	}

	snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
	snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

	start = strstr(xml, open_tag);
	if (!start)
	{
		return -1;
	}

	start += strlen(open_tag);
	end = strstr(start, close_tag);
	if (!end || end < start)
	{
		return -1;
	}

	len = (size_t)(end - start);
	if (len >= out_len)
	{
		len = out_len - 1;
	}

	memcpy(out, start, len);
	out[len] = '\0';
	return 0;
}

static int onvif_extract_xml_attr_value_local(const char *xml, const char *key, char *out, size_t out_len)
{
	const char *p;
	size_t key_len;

	if (!xml || !key || !out || out_len == 0)
	{
		return -1;
	}

	key_len = strlen(key);
	p = xml;
	while ((p = strstr(p, key)) != NULL)
	{
		const char *q = p + key_len;
		const char *e;
		size_t len;
		char quote;

		while (*q && isspace((unsigned char)(*q)))
		{
			q++;
		}
		if (*q != '=')
		{
			p += key_len;
			continue;
		}

		q++;
		while (*q && isspace((unsigned char)(*q)))
		{
			q++;
		}

		quote = *q;
		if (quote != '\'' && quote != '"')
		{
			p += key_len;
			continue;
		}

		q++;
		e = strchr(q, quote);
		if (!e)
		{
			return -1;
		}

		len = (size_t)(e - q);
		if (len >= out_len)
		{
			len = out_len - 1;
		}

		memcpy(out, q, len);
		out[len] = '\0';
		return 0;
	}

	return -1;
}

static int onvif_get_xml_value_local(char *xml, char *dst, const char *k)
{
	char value[256] = {0};

	if (!xml || !dst || !k)
	{
		return -1;
	}

	dst[0] = '\0';

	if (onvif_extract_xml_attr_value_local(xml, k, value, sizeof(value)) == 0)
	{
		strcpy(dst, value);
		return 0;
	}

	if (onvif_extract_xml_tag_local(xml, k, value, sizeof(value)) == 0)
	{
		strcpy(dst, value);
		return 0;
	}

	return -1;
}

static void onvif_alarm_msg_to_event_local(const ALARM_MSG_DATA *src, alarm_event_data *dst)
{
	memset(dst, 0, sizeof(*dst));
	dst->year = src->alarmtime.year;
	dst->month = src->alarmtime.month;
	dst->day = src->alarmtime.day;
	dst->hour = src->alarmtime.hour;
	dst->minute = src->alarmtime.minute;
	dst->second = src->alarmtime.second;
	dst->alarm_code = src->alarmcode;
	dst->alarm_flag = src->alarmflag;
	dst->alarm_level = src->alarmlevel;
	memcpy(dst->alarm_payload, src->alarmdata, sizeof(dst->alarm_payload));
	memcpy(dst->snap_path, src->snapfile, sizeof(dst->snap_path));
}


#define ONVIF_CALLOUT_STR  "callout"
#define ONVIF_THROUGH_STR  "through"
#define ONVIF_HANGUP_STR    "hangup"
#define ONVIF_TOHANGUP_STR "timeout_hangup"

extern FRAME_BUFFER_MANAGER g_onvif_event_mgr;
extern int g_onvif_event_busy;

extern char msg_data[1024]; //汇讯回调 //1
extern char g_HX_Authorization[128];
extern char g_HX_stcd[64];
extern char g_HX_URL[128];
extern int g_init_flag_hx;

extern int g_yen_version_sdm;//宇视老款NVR							//1
extern int g_yen_version_smd;//宇视新款NVR							//1
extern pthread_mutex_t g_mclock;
extern int g_yen_version_ultramotion_clock;//宇视NVR人型clock
extern int g_onvif_expand;
extern int g_bHxVersion;
extern int g_http_alarm;
extern int is_Y_Version;

int g_yen_version_car_clock = 0;
int g_yen_version_pd_clock = 0;
int g_YS_old_version = 0;
int g_Yen_Ys_old = 0;

extern BOOL onvif_build_notify_alarm_motion_message(int alarm_level, char *alarm_data);

#define ALARM_CODE_VIDEO_PD ALARM_CODE_VIDEO_AI
#define ALARM_CODE_VIDEO_PD_FINISH ALARM_CODE_VIDEO_AI_FINISH

unsigned int receive_data(void *buffer, size_t size, size_t nmemb, unsigned char *date)
{
	unsigned int realsize = size * nmemb;
	memset(msg_data, 0, 1024);
	strcpy(msg_data, buffer);
	return realsize;
}

int Send_pic_to_http(char* filepath)
{
#ifdef CURL_SUPPORT
	unsigned char *respBodyData = malloc(1024);
	memset(respBodyData, 0, 1024);
	struct timeval tv;
	gettimeofday(&tv, 0);
	struct tm *ptm = SystemLocalTime(&tv.tv_sec);
	char time[128] = {0};
	sprintf(&time, "%04d-%02d-%02d %02d:%02d:%02d",
					ptm->tm_year + 1900,
					ptm->tm_mon + 1,
					ptm->tm_mday,
					ptm->tm_hour,
					ptm->tm_min,
					ptm->tm_sec);
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl) 
	{
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_URL, g_HX_URL);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
		struct curl_slist *headers = NULL;
		char Authorization_msg[128] = {0};
		sprintf(Authorization_msg, "Authorization: %s", g_HX_Authorization);
		headers = curl_slist_append(headers, Authorization_msg);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_mime *mime;
		curl_mimepart *part;
		mime = curl_mime_init(curl);
		
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "stcd");
		curl_mime_data(part, g_HX_stcd, CURL_ZERO_TERMINATED);
		
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "file_format");
		curl_mime_data(part, "jpg", CURL_ZERO_TERMINATED);
		
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "file_time");
		curl_mime_data(part, &time, CURL_ZERO_TERMINATED);
		
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "file");
		curl_mime_filedata(part, filepath);
		
		curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
		//请求超时时长（秒）
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
		//设置连接超时时长（秒）
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, respBodyData);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			log_print(HT_LOG_ERR, "curl_easy_perform() failed，error code is:%s\n", curl_easy_strerror(res));
		}
		else
		{
			log_print(HT_LOG_INFO, "curl_easy_perform() success\n");
		}
		log_print(HT_LOG_INFO, "g_HX_Authorization:%s\n", g_HX_Authorization);
		log_print(HT_LOG_INFO, "g_HX_stcd:%s\n", g_HX_stcd);
		log_print(HT_LOG_INFO, "g_HX_URL:%s\n", g_HX_URL);
		curl_mime_free(mime);
	}
	free(respBodyData);
	curl_easy_cleanup(curl);
#endif
	return 0;
}

int Init_HX_http_config()
{
	int fd = -1;
	const char* filePath = "/mnt/nand/HX_http_config.xml";
	fd = open(filePath, 'r');
	if (fd == -1) {
		log_print(HT_LOG_ERR, "Open failed: %s!", filePath);
		return -1;
	}
	char *file_msg = malloc(1024);
	int offset = read(fd, file_msg, 1024);
	if(fd != -1)
		close(fd);
	if(offset <= 0)
	{
		if (file_msg != NULL)
		{
			free(file_msg);
			file_msg = NULL;
		}
		return -1;
	}
	memset(g_HX_Authorization, 0, 128);
	memset(g_HX_stcd, 0, 64);
	memset(g_HX_URL, 0, 128);
	onvif_get_xml_value_local(file_msg, g_HX_Authorization, "Authorization");
	onvif_get_xml_value_local(file_msg, g_HX_stcd, "stcd");
	onvif_get_xml_value_local(file_msg, g_HX_URL, "URL");
	if (file_msg != NULL)
	{
		free(file_msg);
		file_msg = NULL;
	}
	return 0;
}

int HX_http_alarm()
{
	//保存图片
	int time_s = 0;
	struct timeval tv;
	gettimeofday(&tv, 0);
	char jpegFile[128] = {0};
	char jpegPath[256] = {0};
	struct tm ptm_val;
	SystemLocalTime(&ptm_val);
	struct tm *ptm = &ptm_val;
	sprintf(jpegFile, "%04d%02d%02d02%d02%d02%d.jpg",
					ptm->tm_year + 1900,
					ptm->tm_mon + 1,
					ptm->tm_mday,
					ptm->tm_hour,
					ptm->tm_min,
					ptm->tm_sec);
	anj_snap_jpg(0, 1, 80, "/tmp", jpegFile, NULL);
	sprintf(jpegPath, "/tmp/%s", jpegFile);
	while (1)
	{
		if(access(jpegPath, F_OK) == F_OK)
			break;
		if (time_s > 3000)
		{
			char rm_cmd[512] = {0};
			sprintf(rm_cmd, "rm %s", jpegPath);
			system(rm_cmd);
			return -1;
		}
		time_s++;
		usleep(1000);
	}
	if (access("/tmp/init_http_config", F_OK) == 0)
	{
		g_init_flag_hx = 0;
		system("rm /tmp/init_http_config");
	}
	if(access("/mnt/nand/HX_http_config.xml", F_OK) == 0 && g_init_flag_hx == 0)
	{
		//加载内容到
		int ret = Init_HX_http_config();
		if (ret == -1){
			usleep(10*1000);
			return -1;
		}
		g_init_flag_hx = 1;
	}
	log_print(HT_LOG_INFO, "Send_pic_to_http start\n");
	int ret = Send_pic_to_http(jpegPath);
	if (ret == 0)
	{
		//cJSON *root	= cJSON_Parse(msg_data);
		//int code 	= cJSON_GetObjectItem(root, "code")->valueint;
		//char *msg 	= cJSON_GetObjectItem(root, "msg")->valuestring;
		//cJSON_Delete(root);
	}
	char rm_cmd[512] = {0};
	sprintf(rm_cmd, "rm %s", jpegPath);
	system(rm_cmd);
	return 0;
}

int msg_send_onvifevent(FRAME_ENTRY *frame, ALARM_MSG_DATA *msg)
{
	if(frame == NULL || msg == NULL)
		return -1;

	log_print(HT_LOG_DBG, "Time:%04d-%02d-%02d %02d:%02d:%02d, "
		"alarmcode:%d, alarmflag:%d, alarmlevel:%d \n", 
		msg->alarmtime.year, msg->alarmtime.month, msg->alarmtime.day,
		msg->alarmtime.hour, msg->alarmtime.minute, msg->alarmtime.second,
		msg->alarmcode, msg->alarmflag, msg->alarmlevel);

	int nFrameLen = sizeof(ALARM_MSG_DATA);
	frame->pFrame = (char *)malloc(nFrameLen);
	if(frame->pFrame == NULL)
	{
		log_print(HT_LOG_ERR, "no memory!!!\n");
		return -1;
	}
	
	memcpy(frame->pFrame, msg, nFrameLen);
	frame->nFrameLen = nFrameLen;
	frame->nFlag = 0;
	
	while(g_onvif_event_busy != 0)
		usleep(10*1000);

	g_onvif_event_busy = 1;
	frame_mgr_push(&g_onvif_event_mgr, frame);
	g_onvif_event_busy = 0;

	return 0;
}

void onvif_event_mgr_init(void)
{
	static int s_inited = 0;

	if (s_inited)
		return;

	frame_mgr_init(&g_onvif_event_mgr, 30);
	s_inited = 1;
}

int onvif_alarm_event_handle(const ALARM_MSG_DATA *alarm)
{
	int ret = 0;
	FRAME_ENTRY frame;
	ALARM_MSG_DATA alarmDataMem;
	ALARM_MSG_DATA *alarmData;

	if (alarm == NULL)
		return -1;

	onvif_event_mgr_init();

	{//HAPI
		alarm_event_data ev;
		onvif_alarm_msg_to_event_local(alarm, &ev);
		hapi_alarm_event_notify(&ev);
	}

	memset(&alarmDataMem, 0, sizeof(alarmDataMem));
	alarmData = &alarmDataMem;
	memcpy(alarmData, alarm, sizeof(alarmDataMem));

	if(alarmData->alarmcode == ALARM_CODE_MOTION_DETECT || 
	   alarmData->alarmcode == ALARM_CODE_MOTION_DETECT_DISAPPEAR || 
	   alarmData->alarmcode == ALARM_CODE_VIDEO_PD  || 
	   alarmData->alarmcode == ALARM_CODE_VIDEO_PD_FINISH || 
	   alarmData->alarmcode == ALARM_CODE_GPIO3_HIGH2LOW || 
	   alarmData->alarmcode == ALARM_CODE_GPIO3_LOW2HIGH || 
	   alarmData->alarmcode == ALARM_CODE_IO_ALARM || 
	   alarmData->alarmcode == ALARM_CODE_IO_ALARM_FINISH || 
	   alarmData->alarmcode == ALARM_CODE_CALL_CALLOUT || 
	   alarmData->alarmcode == ALARM_CODE_CALL_THROUGH || 
	   alarmData->alarmcode == ALARM_CODE_CALL_HANGUP || 
	   alarmData->alarmcode == ALARM_CODE_CALL_TOHANGUP || 
	   alarmData->alarmcode == ALARM_CODE_VIDEO_COVERD)//csj 20190402
	{
		if(alarmData->alarmcode == ALARM_CODE_MOTION_DETECT)
		{
			if (g_bHxVersion && g_http_alarm)
			{
				log_print(HT_LOG_INFO, "alarm start\n");
				int ret_pic = HX_http_alarm();
				if (ret_pic == -1)
				{
					log_print(HT_LOG_ERR, "HX_http_alarm error\n");
				}
				log_print(HT_LOG_INFO, "alarm over\n");
			}
			alarmData->alarmcode = ALARM_CODE_MOTION_TRUE;		
		}
		else if(alarmData->alarmcode == ALARM_CODE_MOTION_DETECT_DISAPPEAR)
		{
			alarmData->alarmcode = ALARM_CODE_MOTION_FALSE; 	
		}
		else if (alarmData->alarmcode == ALARM_CODE_VIDEO_PD)
		{
			if(is_Y_Version || g_onvif_expand)
			{
				//log_print(HT_LOG_INFO, "RRRTTT alarmData->alarmlevel:%d\n", alarmData->alarmlevel);
				if(alarmData->alarmlevel == ALARM_AI_PD)
				{
					alarmData->alarmcode = ALARM_CODE_PD_TRUE;
				}
				else if((alarmData->alarmlevel == ALARM_AI_VEHICLE_MOTO) || 
				        (alarmData->alarmlevel == ALARM_AI_VEHICLE_BICYCLE))
				{
					alarmData->alarmcode = ALARM_CODE_NON_VEHICEL_TRUE;
				}
				else if((alarmData->alarmlevel == ALARM_AI_VEHICLE_CAR) || 
				        (alarmData->alarmlevel == ALARM_AI_VEHICLE_ELECTRICBICYCLE))
				{
					alarmData->alarmcode = ALARM_CODE_VEHICEL_TRUE;
				}
				else if(alarmData->alarmlevel == ALARM_AI_LPR)//车牌
				{
					alarmData->alarmcode = ALARM_CODE_LRP_TRUE;
				}
				else if(alarmData->alarmlevel == ALARM_AI_FACEDETECT)//人脸
				{
					alarmData->alarmcode = ALARM_CODE_FACE_TRUE;
				}
				else if(alarmData->alarmlevel == ALARM_AI_FIRE)//火焰
				{
					alarmData->alarmcode = ALARM_CODE_FIRE_TRUE;
				}
				else if(alarmData->alarmlevel == ALARM_AI_VIDEO_REGION_DETECT_ENTER)//进入区域
				{
					alarmData->alarmcode = ALARM_CODE_REGION_DETECT_ENTER_TRUE;
				}
				else if(alarmData->alarmlevel == ALARM_AI_VIDEO_REGION_DETECT_LEAVE)//离开区域
				{
					alarmData->alarmcode = ALARM_CODE_REGION_DETECT_LEAVE_TRUE;
				}
				else if(alarmData->alarmlevel == ALARM_AI_VIDEO_REGION_DETECT_STAY)//区域入侵/区域逗留
				{
					alarmData->alarmcode = ALARM_CODE_REGION_DETECT_STAY_TRUE;
				}
				else if(alarmData->alarmlevel == ALARM_AI_VIDEO_GATE)//越界
				{
					alarmData->alarmcode = ALARM_CODE_GATE_TRUE;
				}
				else
					alarmData->alarmcode = ALARM_CODE_MOTION_TRUE;
			}
			else 
				alarmData->alarmcode = ALARM_CODE_MOTION_TRUE;
		}
		else if (alarmData->alarmcode == ALARM_CODE_VIDEO_PD_FINISH)
		{
			if(is_Y_Version || g_onvif_expand)
			{
				// log_print(HT_LOG_INFO, "RRRTTT alarmData->alarmlevel:%d\n", alarmData->alarmlevel);
				if(alarmData->alarmlevel == ALARM_AI_PD)
				{
					alarmData->alarmcode = ALARM_CODE_PD_FALSE;
				}
				else if((alarmData->alarmlevel == ALARM_AI_VEHICLE_MOTO) || 
				        (alarmData->alarmlevel == ALARM_AI_VEHICLE_BICYCLE))
				{
					alarmData->alarmcode = ALARM_CODE_NON_VEHICEL_FALSE;
				}
				else if((alarmData->alarmlevel == ALARM_AI_VEHICLE_CAR) || 
				        (alarmData->alarmlevel == ALARM_AI_VEHICLE_ELECTRICBICYCLE))
				{
					alarmData->alarmcode = ALARM_CODE_VEHICEL_FALSE;
				}
				else
				{
					alarmData->alarmcode = ALARM_CODE_MOTION_FALSE;
				}
			}
			else
				alarmData->alarmcode = ALARM_CODE_MOTION_FALSE;
		}			
		else if (alarmData->alarmcode == ALARM_CODE_IO_ALARM_FINISH)
		{
			alarmData->alarmcode = ALARM_CODE_IO_FALSE;
		}
		else if (alarmData->alarmcode == ALARM_CODE_IO_ALARM)
		{
			alarmData->alarmcode = ALARM_CODE_IO_TRUE;
		}
		else if (alarmData->alarmcode == ALARM_CODE_CALL_CALLOUT)//csj 20190402
		{
			alarmData->alarmcode = ALARM_CODE_OCALL_CALLOUT;
		}
		else if (alarmData->alarmcode == ALARM_CODE_CALL_THROUGH)
		{
			alarmData->alarmcode = ALARM_CODE_OCALL_THROUGH;
		}					
		else if (alarmData->alarmcode == ALARM_CODE_CALL_HANGUP)
		{
			alarmData->alarmcode = ALARM_CODE_OCALL_HANGUP;
		}
		else if (alarmData->alarmcode == ALARM_CODE_CALL_TOHANGUP)
		{
			alarmData->alarmcode = ALARM_CODE_OCALL_TOHANGUP;
		}

		ret = msg_send_onvifevent(&frame, alarmData);
		if(ret != 0)
			return -1;

		//YCX版本, 如果是人形报警, 除了推送人形报警以外, 再推送移动侦测报警
		if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_PD_TRUE || 
		   alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_PD_FALSE ||
		   alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_VEHICEL_TRUE ||
		   alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_VEHICEL_FALSE ||
		   alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_NON_VEHICEL_TRUE ||
		   alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_NON_VEHICEL_FALSE)
		{
			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_PD_TRUE)
				alarmData->alarmcode = ALARM_CODE_MOTION_TRUE;

			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_PD_FALSE)
				alarmData->alarmcode = ALARM_CODE_MOTION_FALSE;

			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_VEHICEL_TRUE)
				alarmData->alarmcode = ALARM_CODE_MOTION_TRUE;

			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_VEHICEL_FALSE)
				alarmData->alarmcode = ALARM_CODE_MOTION_FALSE;

			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_NON_VEHICEL_TRUE)
				alarmData->alarmcode = ALARM_CODE_MOTION_TRUE;

			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_NON_VEHICEL_FALSE)
				alarmData->alarmcode = ALARM_CODE_MOTION_FALSE;

			ret = msg_send_onvifevent(&frame, alarmData);
			if(ret != 0)
				return -1;
		}
		
		//模仿海康,如果是移动侦测事件,额外多推送一条status事件
		if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_MOTION_TRUE || 
		   alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_MOTION_FALSE)
		{
			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_MOTION_TRUE)
				alarmData->alarmcode = ALARM_EVENT_STATE_TRUE;
			else if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_MOTION_FALSE)
				alarmData->alarmcode = ALARM_EVENT_STATE_FALSE;
			
			ret = msg_send_onvifevent(&frame, alarmData);
			if(ret != 0)
				return -1;
		}	
			
		//模仿海康,如果是IO输入事件,额外多推送一条status事件
		//csj 20190909 
		if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_IO_TRUE || 
		   alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_IO_FALSE)
		{	
			if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_IO_TRUE)
				alarmData->alarmcode = ALARM_IO_INSTATE_TRUE;
			else if(alarmData->alarmcode == (AjAlarmCode)ALARM_CODE_IO_FALSE)
				alarmData->alarmcode = ALARM_IO_INSTATE_FALSE;
			
			ret = msg_send_onvifevent(&frame, alarmData);
			if(ret != 0)
				return -1;
		}
	}

	return 0;
}


void *onvif_alarm_thr(void *arg)
{
	log_print(HT_LOG_INFO, "enter onvif_alarm_thr\n");
	pthread_detach(pthread_self());
	prctl(PR_SET_NAME, __func__);

	FRAME_ENTRY frame;
	ALARM_MSG_DATA *alarm_data;

	while(1)
	{
		if(frame_mgr_pop(&g_onvif_event_mgr, &frame) > 0)
		{
			alarm_data = (ALARM_MSG_DATA *)frame.pFrame;
			if (unv_subscribe_map_size_get() > 0)//人1，汽车2，摩托车3，客车货车4，自行车5
			{
				if (g_onvif_expand && g_Yen_Ys_old == 0 && (g_yen_version_smd == 1 && g_yen_version_sdm == 0))
				{
					if(
						((alarm_data->alarmlevel == (int )ALARM_AI_VEHICLE_CAR || alarm_data->alarmlevel == (int )ALARM_AI_VEHICLE_ELECTRICBICYCLE) && alarm_data->alarmcode == (int )ALARM_CODE_VEHICEL_TRUE)//机动车2,4;64
						|| ((alarm_data->alarmlevel == (int )ALARM_AI_VEHICLE_MOTO || alarm_data->alarmlevel == (int )ALARM_AI_VEHICLE_BICYCLE) && alarm_data->alarmcode == (int )ALARM_CODE_NON_VEHICEL_TRUE)//非机动车3,5;66
						|| (alarm_data->alarmlevel == (int )ALARM_AI_PD && alarm_data->alarmcode == (int )ALARM_CODE_PD_TRUE)//人1;62
						|| (alarm_data->alarmlevel == (int )ALARM_AI_FIRE && alarm_data->alarmcode == (int )ALARM_CODE_FIRE_TRUE)//火焰8;78
						|| (alarm_data->alarmlevel == (int )ALARM_AI_FACEDETECT && alarm_data->alarmcode == (int )ALARM_CODE_FACE_TRUE)//人脸9;68
						|| (alarm_data->alarmlevel == (int )ALARM_AI_VIDEO_GATE && alarm_data->alarmcode == (int )ALARM_CODE_GATE_TRUE)//越界7;70
						|| (alarm_data->alarmlevel == (int )ALARM_AI_VIDEO_REGION_DETECT_ENTER && alarm_data->alarmcode == (int )ALARM_CODE_REGION_DETECT_ENTER_TRUE)//进入区域10;72
						|| (alarm_data->alarmlevel == (int )ALARM_AI_VIDEO_REGION_DETECT_LEAVE && alarm_data->alarmcode == (int )ALARM_CODE_REGION_DETECT_LEAVE_TRUE)//离开区域11;74
						|| (alarm_data->alarmlevel == (int )ALARM_AI_VIDEO_REGION_DETECT_STAY && alarm_data->alarmcode == (int )ALARM_CODE_REGION_DETECT_STAY_TRUE)//区域逗留12;76
						)
					{
						log_print(HT_LOG_INFO, "Send_notify_to_listnode1\n");
						log_print(HT_LOG_INFO, "alarm_data->alarmlevel:%d\n", alarm_data->alarmlevel);
						log_print(HT_LOG_INFO, "alarm_data->alarmcode:%d\n", alarm_data->alarmcode);
						log_print(HT_LOG_INFO, "alarm_data->snapfile:%s\n", alarm_data->snapfile);
						pthread_mutex_lock(&g_mclock);    //加锁
						{
							alarm_event_data ev;
							onvif_alarm_msg_to_event_local(alarm_data, &ev);
							unv_notify_alarm_event_snap_to_sublist(&ev, 1, alarm_data->snapfile);
						}
						pthread_mutex_unlock(&g_mclock);    //加锁
					}
				}
				else if((( alarm_data->alarmlevel == (int )ALARM_AI_VEHICLE_CAR || alarm_data->alarmlevel == (int )ALARM_AI_VEHICLE_ELECTRICBICYCLE ) && alarm_data->alarmcode == (int )ALARM_CODE_VEHICEL_TRUE) && (is_Y_Version || g_YS_old_version))//延创兴对接宇视老版本NVR，只支持人车
				{
					log_print(HT_LOG_INFO, "Send_notify_to_listnode2\n");
					pthread_mutex_lock(&g_mclock);    //加锁
					{
						alarm_event_data ev;
						onvif_alarm_msg_to_event_local(alarm_data, &ev);
						unv_notify_alarm_event_to_sublist(&ev, 1);
					}
					pthread_mutex_unlock(&g_mclock);    //加锁
				}
			}
			log_print(HT_LOG_DBG, "pop up an event, alarm_data->alarmcode:%d\n", alarm_data->alarmcode);
			
			if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_MOTION_TRUE)
			{
				if (unv_subscribe_map_size_get() > 0 && (g_yen_version_car_clock == 1 || g_yen_version_pd_clock == 1) && g_yen_version_smd == 1 && g_yen_version_ultramotion_clock == 1)//链接数大于0 & 车使能         & 人使能 & smd模式 & Set_AMD_Rule 设置
					continue;

    #if 0
				NotificationMessageList *p_message = onvif_init_NotificationMessage_Motion(TRUE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
    #else
                onvif_build_notify_alarm_motion_message(alarm_data->alarmlevel, alarm_data->alarmdata);
    #endif
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_MOTION_FALSE)
			{
				if (unv_subscribe_map_size_get() > 0 && (g_yen_version_car_clock == 1 || g_yen_version_pd_clock == 1) && g_yen_version_smd == 1 && g_yen_version_ultramotion_clock == 1)
					continue;
				NotificationMessageList *p_message = onvif_init_NotificationMessage_Motion(FALSE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_PD_TRUE  && alarm_data->alarmlevel == 1)
			{
				if (g_yen_version_smd == 1)
				{
					continue;
				}
				NotificationMessageList *p_message = onvif_init_NotificationMessage_PD(TRUE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_PD_FALSE && alarm_data->alarmlevel == 1)
			{
				if (g_yen_version_smd == 1)
				{
					continue;
				}
				NotificationMessageList *p_message = onvif_init_NotificationMessage_PD(FALSE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_NON_VEHICEL_TRUE && (alarm_data->alarmlevel == 3 || alarm_data->alarmlevel == 5))
			{
				if (g_yen_version_smd == 1)
				{
					continue;
				}
				NotificationMessageList *p_message = onvif_init_NotificationMessage_PD(TRUE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_NON_VEHICEL_FALSE && (alarm_data->alarmlevel == 3 || alarm_data->alarmlevel == 5))
			{
				if (g_yen_version_smd == 1)
				{
					continue;
				}
				NotificationMessageList *p_message = onvif_init_NotificationMessage_PD(FALSE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_VEHICEL_TRUE)
			{
				if (g_yen_version_smd == 1 || g_yen_version_sdm == 1)
				{
					continue;
				}
				NotificationMessageList *p_message = onvif_init_NotificationMessage_VEHICLE(TRUE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_VEHICEL_FALSE)
			{
				if (g_yen_version_smd == 1 || g_yen_version_sdm == 1)
				{
					continue;
				}
				NotificationMessageList *p_message = onvif_init_NotificationMessage_VEHICLE(FALSE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_IO_TRUE)
			{
				//log_print(HT_LOG_INFO, "ALARM_CODE_IO_TRUE\n");
				NotificationMessageList *p_message = onvif_init_NotificationMessage_IO(TRUE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_IO_FALSE)
			{
				//log_print(HT_LOG_INFO, "ALARM_CODE_IO_FALSE\n");
				NotificationMessageList *p_message = onvif_init_NotificationMessage_IO(FALSE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_IO_INSTATE_TRUE)
			{
				//log_print(HT_LOG_INFO, "ALARM_IO_INSTATE_TRUE\n");
				NotificationMessageList *p_message = onvif_init_NotificationMessage_IOSTAR(TRUE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_IO_INSTATE_FALSE)
			{
				//log_print(HT_LOG_INFO, "ALARM_IO_INSTATE_FALSE\n");
				NotificationMessageList *p_message = onvif_init_NotificationMessage_IOSTAR(FALSE);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			else if(alarm_data->alarmcode >= (AjAlarmCode)ALARM_CODE_OCALL_CALLOUT && alarm_data->alarmcode <= (AjAlarmCode)ALARM_CODE_OCALL_TOHANGUP)//csj 20190402
			{
				char Type[20];

				if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_OCALL_CALLOUT)
					strcpy(Type, ONVIF_CALLOUT_STR);
				else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_OCALL_THROUGH)
					strcpy(Type, ONVIF_THROUGH_STR);
				else if(alarm_data->alarmcode == (AjAlarmCode)ALARM_CODE_OCALL_HANGUP)
					strcpy(Type, ONVIF_HANGUP_STR);
				else
					strcpy(Type, ONVIF_TOHANGUP_STR);
				
				NotificationMessageList *p_message = onvif_init_NotificationMessage_FH(Type);
				if (p_message)
					onvif_put_NotificationMessage(p_message);
			}
			
			
			free(frame.pFrame);
			usleep(40*1000);
		}
		
		usleep(10*1000);
	}

	pthread_exit(NULL);
	return NULL;
}












