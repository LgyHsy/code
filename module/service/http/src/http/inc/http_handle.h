#ifndef __HTTP_HANDLE_H__
#define __HTTP_HANDLE_H__

#include "http_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    int smart_motion;       // 智能
    int human_enable;       // 人形
    int car_enable;         // 车型
    int video_gate;         // 越界
    int region_enable;      // 区域
    int face_detect;        // 人脸检测
    int tamper_enable;      // 遮挡报警
    int io_alarm;           // io报警
    int auto_track;         // 自动跟踪
}http_alarm_ability_t;

TestWebSiteStruct *get_test_website_info();

AudioFileList *http_audio_file_list_get();

http_alarm_ability_t *http_alarm_ability_get();

char* http_login_path_get(void);

int http_response_cb(void *pInst, const char *pResponse, int status);

int http_send_file_cb(void *pInst, void *pmsgt, const char *szFileName, const char *type);

int http_snapshot_request(void *pInst, void *pmsgt, const char* http_url, const char* ipRemote);

int http_get_proc(void *pInst, void *pmsgt, const char* http_url, const char* ipRemote, const char* host);

int http_put_proc(void *pInst, 
	                const char *szURLFullname,
	                const char *szMsgBuffer,
	                const char* szMsgBody, 
	                const char *clientip,
	                const char* host,
	                cb_func_http_response pCbResponse);

int http_post_proc(void *pInst, 
                        void *pmsgt,
                        const char *szURLFullname,
                        const char *szMsgBuffer, 
                        const char* szMsgBody, 
                        const char *clientip, 
                        const char* host, 
                        cb_func_http_response pCbResponse);

int http_handle_init(cb_func_http_response cbResponse, cb_func_http_sendfile cbGetFile);

const char *get_oem_mp3_path();
const char *get_oem_app_path();
const char *get_oem_logo_path();

void http_query_record_set(int flag);
int  http_query_record_get();

void http_get_ip_addr_port(char *ip_addr, int ip_len, int *port);

#ifdef __cplusplus
}
#endif


#endif

