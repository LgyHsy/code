#include <stdio.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "rtsp_error.h"

static const char * const g_rtsp_status_strings[] = {
	[RTSP_STATUS_IGNORE]                 ="Unknown",
	[RTSP_STATUS_CONTINUE]               ="100 Continue",
	[RTSP_STATUS_OK]                     ="200 OK",
	[RTSP_STATUS_CREATED]                ="201 Created",
	[RTSP_STATUS_LOW_ON_STORAGE_SPACE]   ="250 Low on Storage Space",
	[RTSP_STATUS_MULTIPLE_CHOICES]       ="300 Multiple Choices",
	[RTSP_STATUS_MOVED_PERMANENTLY]      ="301 Moved Permanently",
	[RTSP_STATUS_MOVED_TEMPORARILY]      ="302 Moved Temporarily",
	[RTSP_STATUS_SEE_OTHER]              ="303 See Other",
	[RTSP_STATUS_NOT_MODIFIED]           ="304 Not Modified",
	[RTSP_STATUS_USE_PROXY]              ="305 Use Proxy",
	[RTSP_STATUS_BAD_REQUEST]            ="400 Bad Request",
	[RTSP_STATUS_UNAUTHORIZED]           ="401 Unauthorized",
	[RTSP_STATUS_PAYMENT_REQUIRED]       ="402 Payment Required",
	[RTSP_STATUS_FORBIDDEN]              ="403 Forbidden",
	[RTSP_STATUS_NOT_FOUND]              ="404 Not Found",
	[RTSP_STATUS_METHOD]                 ="405 Method Not Allowed",
	[RTSP_STATUS_NOT_ACCEPTABLE]         ="406 Not Acceptable",
	[RTSP_STATUS_PROXY_AUTH_REQUIRED]    ="407 Proxy Authentication Required",
	[RTSP_STATUS_REQ_TIME_OUT]           ="408 Request Time-out",
	[RTSP_STATUS_GONE]                   ="410 Gone",
	[RTSP_STATUS_LENGTH_REQUIRED]        ="411 Length Required",
	[RTSP_STATUS_PRECONDITION_FAILED]    ="412 Precondition Failed",
	[RTSP_STATUS_REQ_ENTITY_2LARGE]      ="413 Request Entity Too Large",
	[RTSP_STATUS_REQ_URI_2LARGE]         ="414 Request URI Too Large",
	[RTSP_STATUS_UNSUPPORTED_MTYPE]      ="415 Unsupported Media Type",
	[RTSP_STATUS_PARAM_NOT_UNDERSTOOD]   ="451 Parameter Not Understood",
	[RTSP_STATUS_CONFERENCE_NOT_FOUND]   ="452 Conference Not Found",
	[RTSP_STATUS_BANDWIDTH]              ="453 Not Enough Bandwidth",
	[RTSP_STATUS_SESSION]                ="454 Session Not Found",
	[RTSP_STATUS_STATE]                  ="455 Method Not Valid in This State",
	[RTSP_STATUS_INVALID_HEADER_FIELD]   ="456 Header Field Not Valid for Resource",
	[RTSP_STATUS_INVALID_RANGE]          ="457 Invalid Range",
	[RTSP_STATUS_RONLY_PARAMETER]        ="458 Parameter Is Read-Only",
	[RTSP_STATUS_AGGREGATE]              ="459 Aggregate Operation no Allowed",
	[RTSP_STATUS_ONLY_AGGREGATE]         ="460 Only Aggregate Operation Allowed",
	[RTSP_STATUS_TRANSPORT]              ="461 Unsupported Transport",
	[RTSP_STATUS_UNREACHABLE]            ="462 Destination Unreachable",
	[RTSP_STATUS_INTERNAL]               ="500 Internal Server Error",
	[RTSP_STATUS_NOT_IMPLEMENTED]        ="501 Not Implemented",
	[RTSP_STATUS_BAD_GATEWAY]            ="502 Bad Gateway",
	[RTSP_STATUS_SERVICE]                ="503 Service Unavailable",
	[RTSP_STATUS_GATEWAY_TIME_OUT]       ="504 Gateway Time-out",
	[RTSP_STATUS_VERSION]                ="505 RTSP Version not Supported",
	[RTSP_STATUS_UNSUPPORTED_OPTION]     ="551 Option not supported",
};

int rtsp_get_status_reply_message(rtsp_status_code_e rtsp_status_code, int cseq,
		char *rtsp_reply_message, int rtsp_reply_message_length)
{
	if (rtsp_status_code >= ARRAY_SIZE(g_rtsp_status_strings))
	{
		__ERR("rtsp_status_code is invalid!(%lu)", (unsigned long)rtsp_status_code);
		return -1;
	}
	_NULL_POINTER_CHECK_(rtsp_reply_message, -1);
	if (rtsp_reply_message_length <= 0)
	{
		__ERR("rtsp_reply_message_length is invalid!(%lu)", (unsigned long)rtsp_reply_message_length);
		return -1;
	}

	int pos = 0;
	pos += snprintf(rtsp_reply_message + pos, rtsp_reply_message_length - pos,
		"RTSP/1.0 %s\r\n"
		"Cseq: %d\r\n",
		g_rtsp_status_strings[rtsp_status_code],
		cseq);
	if (rtsp_status_code == RTSP_STATUS_UNAUTHORIZED)
	{
		/*
		 * Hik NVR expects Basic-only with realm="/", matching hik_server-rtsp.
		 * Offering Digest (or realm RTSPD) makes NVR retry Digest and fail preview
		 * with 取流失败(0x1b01032).
		 */
		pos += snprintf(rtsp_reply_message + pos, rtsp_reply_message_length - pos,
				"WWW-Authenticate: Basic realm=\"/\"\r\n");
	}

	pos += snprintf(rtsp_reply_message + pos, rtsp_reply_message_length - pos, "\r\n");

	return 0;
}

const char *rtsp_get_status_msg(rtsp_status_code_e rtsp_status_code)
{
	if (rtsp_status_code >= ARRAY_SIZE(g_rtsp_status_strings))
	{
		__ERR("rtsp_status_code is invalid!(%lu)", (unsigned long)rtsp_status_code);
		return NULL;
	}
	return g_rtsp_status_strings[rtsp_status_code];
}
