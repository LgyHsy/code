#ifndef __CGI_HANDLE__H__
#define __CGI_HANDLE__H__

#if defined (__cplusplus)
extern "C" {
#endif


#define XML_HEAD        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
#define XML_CGI_FAULT   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<fault>\r\n<value>%s</value>\r\n<reason>%s</reason>\r\n</fault>\r\n"


// 获取cgi url中的参数的值, 获取失败时返回0
int cgi_get_value(const char *http_url, const char *name, char *buffer, int buflen);

// 判断uid或username,password是否有效, 缺少或无效返回0, 有效返回1
int cgi_is_valid_uid_or_username(const char *http_url, char *response, const char *url);

int cgi_settings_request(const char *urlpath, char (*url)[200], char *pResultBuf);

int login_request(const char *urlpath, char (*url)[200], char *resultBuf);

void split_path(const char *path, char (*url)[200]);


/************************** 
*	ipc http cgi接口 查找是否CGI接口
*	name:		http_cgi_find
*	parameters: http_url	cgi的url
*	return: 	0/1	
*	added on 2017-12-01
**************************/
int http_cgi_find(const char *http_url);

/************************** 
*	ipc http cgi接口 统一处理url为/cgi-bin/的cgi命令
*	name:		http_cgi_handle
*	parameters:	http_url	cgi的url
*				response	响应的报文
*               status_output status code
*	return:		http status code		
**************************/
int http_cgi_handle(const char *http_url,char *response, const char *ipstr);


int http_cgi_init_session_id();

void http_cgi_uninit_session_id();


#if defined (__cplusplus)
}
#endif



#endif

