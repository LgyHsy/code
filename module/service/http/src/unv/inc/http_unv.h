#ifndef __HTTP_UNV_H__
#define __HTTP_UNV_H__

#include "http_def.h"

typedef struct
{
    int unv_enable;             // 宇视协议使能
    int smart_support;          // 宇视smart支持
    int plug_by_play_support;   // 即插即用支持
    int private_probe_support;  // 私有探测支持

    int search_status;          // 宇视协议搜索状态 0:关闭 1:打开
}unv_info_t;

int unv_motion_detect_mode_get();
void unv_motion_detect_mode_set(int mode);

int unv_smart_support_get();
int unv_enable_get();

int unv_handle_put_request(void* pInst, const char* http_url, const char *pMsgBody, const char* msgbuf, cb_func_http_response pCbResponse);

int unv_handle_get_request(const char *http_url, char **pResponseBuffer);

int http_unv_init();

void http_unv_uninit();

#endif