#ifndef __ANJ_HTTP_H__
#define __ANJ_HTTP_H__

#include "http_def.h"

void anj_http_web_file_init();

void anj_http_init(cb_func_http_response cbResponse, cb_func_http_sendfile cbGetFile);
void anj_http_uninit();

#endif
