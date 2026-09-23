#ifndef __HAPI_HANDLE_H__
#define __HAPI_HANDLE_H__

#ifdef __cplusplus
extern "C"
{
#endif


#define HAPI_V1_HEAD    "/HAPI/V1.0"
#define HEAD_CHANNELS   "/Channels/"



/************************** 
*    ipc http cgi接口 统一处理url为/HAPI/的api命令
*    name:        http_hapi_handle
*    parameters:    
                pInst 
                szURLFullname    api的url
*                clientip    client ip
*    return:        http status code    
*    added on 2023-11-03
**************************/

int http_hapi_handle(void *pInst, const char *szMethod, const char *szURLFullname, const char* szMsgBody, const char *clientip, const char* host);


#ifdef __cplusplus
}
#endif


#endif
