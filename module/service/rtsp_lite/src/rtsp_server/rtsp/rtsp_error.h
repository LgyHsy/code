#ifndef __RTSP_ERROR_H__
#define __RTSP_ERROR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtsp_status_code 
{
	RTSP_STATUS_IGNORE             	 ,			/* 自定义的枚举，用于过滤端口接收到不符合rtsp协议的内容 */
	RTSP_STATUS_CONTINUE             ,			/* 100 */
	RTSP_STATUS_OK                   ,			/* 200 */
	RTSP_STATUS_CREATED              ,			/* 201 */
	RTSP_STATUS_LOW_ON_STORAGE_SPACE ,			/* 250 */
	RTSP_STATUS_MULTIPLE_CHOICES     ,			/* 300 */
	RTSP_STATUS_MOVED_PERMANENTLY    ,			/* 301 */
	RTSP_STATUS_MOVED_TEMPORARILY    ,			/* 302 */
	RTSP_STATUS_SEE_OTHER            ,			/* 303 */
	RTSP_STATUS_NOT_MODIFIED         ,			/* 304 */
	RTSP_STATUS_USE_PROXY            ,			/* 305 */
	RTSP_STATUS_BAD_REQUEST          ,			/* 400 */
	RTSP_STATUS_UNAUTHORIZED         ,			/* 401 */
	RTSP_STATUS_PAYMENT_REQUIRED     ,			/* 402 */
	RTSP_STATUS_FORBIDDEN            ,			/* 403 */
	RTSP_STATUS_NOT_FOUND            ,			/* 404 */
	RTSP_STATUS_METHOD               ,			/* 405 */
	RTSP_STATUS_NOT_ACCEPTABLE       ,			/* 406 */
	RTSP_STATUS_PROXY_AUTH_REQUIRED  ,			/* 407 */
	RTSP_STATUS_REQ_TIME_OUT         ,			/* 408 */
	RTSP_STATUS_GONE                 ,			/* 410 */
	RTSP_STATUS_LENGTH_REQUIRED      ,			/* 411 */
	RTSP_STATUS_PRECONDITION_FAILED  ,			/* 412 */
	RTSP_STATUS_REQ_ENTITY_2LARGE    ,			/* 413 */
	RTSP_STATUS_REQ_URI_2LARGE       ,			/* 414 */
	RTSP_STATUS_UNSUPPORTED_MTYPE    ,			/* 415 */
	RTSP_STATUS_PARAM_NOT_UNDERSTOOD ,			/* 451 */
	RTSP_STATUS_CONFERENCE_NOT_FOUND ,			/* 452 */
	RTSP_STATUS_BANDWIDTH            ,			/* 453 */
	RTSP_STATUS_SESSION              ,			/* 454 */
	RTSP_STATUS_STATE                ,			/* 455 */
	RTSP_STATUS_INVALID_HEADER_FIELD ,			/* 456 */
	RTSP_STATUS_INVALID_RANGE        ,			/* 457 */
	RTSP_STATUS_RONLY_PARAMETER      ,			/* 458 */
	RTSP_STATUS_AGGREGATE            ,			/* 459 */
	RTSP_STATUS_ONLY_AGGREGATE       ,			/* 460 */
	RTSP_STATUS_TRANSPORT            ,			/* 461 */
	RTSP_STATUS_UNREACHABLE          ,			/* 462 */
	RTSP_STATUS_INTERNAL             ,			/* 500 */
	RTSP_STATUS_NOT_IMPLEMENTED      ,			/* 501 */
	RTSP_STATUS_BAD_GATEWAY          ,			/* 502 */
	RTSP_STATUS_SERVICE              ,			/* 503 */
	RTSP_STATUS_GATEWAY_TIME_OUT     ,			/* 504 */
	RTSP_STATUS_VERSION              ,			/* 505 */
	RTSP_STATUS_UNSUPPORTED_OPTION   ,			/* 551 */
} rtsp_status_code_e;
	
int rtsp_get_status_reply_message(rtsp_status_code_e rtsp_status_code, int cseq, char *rtsp_reply_message, int rtsp_reply_message_length);
const char *rtsp_get_status_msg(rtsp_status_code_e rtsp_status_code);

#ifdef __cplusplus
}
#endif

#endif /* __RTSP_ERROR_H__ */
