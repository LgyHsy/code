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

#include "sys_inc.h"
#include "http_def.h"
#include "hxml.h"
#include "xml_node.h"
#include "onvif.h"
#include "http.h"
#include "http_srv.h"
#include "http_parse.h"
#include "soap.h"
#include "onvif_device.h"
#include "onvif_pkt.h"
#include "soap_parser.h"
#include "onvif_event.h"
#include "sha1.h"
#include "onvif_ptz.h"
#include "onvif_err.h"
#include "onvif_image.h"
#include "http_auth.h"
#include "base64.h"
#include "onvif_utils.h"
#include "onvif_probe.h"
#include "onvif_srv.h"
// #include "http_proc.h"
// #include "cgi_def.h"
#include "http_handle.h"
#include "http_upload.h"
#include "webpost_handle.h"

#define POST 6
#define PUT 9
#define GET 1
#define DELETE 10

#define HTTP_OK 0

#define HTTP_BODY_BUF_SIZE      (16 * 1024) // 16k（对齐老架构；Motion Areas 等 PUT body 可能较大）


typedef struct {
    char *saved_header;         // 只保存第一次的HTTP头
    int header_len;             // 头长度
} HttpUpgradeHeader;


pthread_mutex_t s_upgrade_mutex = PTHREAD_MUTEX_INITIALIZER;
static HttpUpgradeHeader sUpgradeHeader = {0};

static BOOL http_is_firmware_upgrade_path(const char *path)
{
    if (path == NULL)
    {
        return FALSE;
    }

    return (strstr(path, "IPC_FirmwareUpgrade") != NULL);
}

int http_response(void *pInst, const char *response_buf, int status)
{
	HTTPCLN * p_cln = (HTTPCLN *)pInst;
	int slen;
	int offset;
	int response_len = 0;
	char buff[1024] = {0};

	if (response_buf != NULL)
	{
		response_len = (int)strlen(response_buf);
	}

    if (1003 == status)
    {
        int fd = 0;
        fd = http_get_upgrade_fd();
        if (fd > 0 && fd == p_cln->cfd)
        {
            status = 202;
        }
    }

	if (200 == status || 1003 == status)
	{
		offset = sprintf(buff, 
			"HTTP/1.1 %d OK\r\n"
			"Server: Onvif Server %s\r\n"
			"Content-Type: text/xml; charset=utf-8 \r\n"
			"Content-Length: %d\r\n"
			"Connection: %s\r\n\r\n",
			200, ONVIF_VERSION_STRING, response_len, p_cln->keep_alive ? "keep-alive" : "close");
	}
	else if (202 == status)
	{
		offset = sprintf(buff, 
			"HTTP/1.1 202 Accepted\r\n"
			"Server: Onvif Server %s\r\n"
			"Access-Control-Allow-Origin: *\r\n"
			"Content-Type: text/plain; charset=utf-8 \r\n"
			"Content-Length: %d\r\n"
			"Connection: %s\r\n\r\n",
			ONVIF_VERSION_STRING, response_len, p_cln->keep_alive ? "keep-alive" : "close");        
	}
	else
	{
		offset = sprintf(buff, 
			"HTTP/1.1 %d\r\n"
			"Server: Onvif Server %s\r\n"
			"Content-Type: text/xml; charset=utf-8 \r\n"
			"Content-Length: %d\r\n"
			"Connection: %s\r\n\r\n",
			status, ONVIF_VERSION_STRING, response_len, p_cln->keep_alive ? "keep-alive" : "close");
	}

	slen = http_srv_cln_tx(p_cln, buff, offset);
	if (slen != offset)
		log_print(HT_LOG_ERR,  "%s, slen=%d, offset=%d\r\n", __FUNCTION__, slen, offset);
	
	if (response_buf != NULL)
	{
		slen = http_srv_cln_tx(p_cln, response_buf, response_len);
		if (slen != response_len)
			log_print(HT_LOG_ERR,  "%s, slen=%d, response_len=%d\r\n", __FUNCTION__, slen, response_len);
	}
	
	return 0;
}

int copy_file(void *pInst, void *pmsgt, const char *filename, const char *type)
{
	httpd_process_request(g_onvif_cls.httpd, (HTTPCLN *)pInst, (HTTPMSG *)pmsgt);
	return 0;
}

int soap_http_response(void *pInst, void *pmsgt, const char *pResponse, int status)
{
	if( NULL == pInst)
		return 0;

	if(NULL == pResponse)
		return 0;

	return http_response(pInst, pResponse, status);
}

void get_remote_ip(HTTPCLN * p_cln, char *remote_ip)
{
	struct sockaddr_in struAddr;
	socklen_t AddrLen = sizeof(struct sockaddr_in);
	int s32Rtn = getpeername(p_cln->cfd, (struct sockaddr *)&struAddr, (socklen_t*)&AddrLen);
	if(s32Rtn < 0)
		log_print(HT_LOG_ERR, "getpeername failed\r\n");
	
	strcpy(remote_ip, inet_ntoa(struAddr.sin_addr));
	
	return ;
}

int get_http_body_msg(HTTPCLN * p_cln, char *body_buf, HTTPMSG * p_msg)
{
	/*
	 * HTTPMSG 异步入队后，大包会走 dyn_recv_buf，收完即 free 并把 p_cln->rbuf 置空。
	 * 此处绝不能再 strstr(p_cln->rbuf,...)，否则 NVR 下发 Motion Areas 等大 PUT 必段错误。
	 * body 以 msg_buf / http_get_ctt 为准（与老 OnvifServer_full/cgi.c 一致）。
	 */
	const char *p_body = NULL;
	int total_hdr;

	(void)p_cln;
	if (body_buf == NULL || p_msg == NULL || p_msg->msg_buf == NULL
		|| p_msg->hdr_len < 0 || p_msg->ctt_len < 0 || p_msg->ctt_len >= HTTP_BODY_BUF_SIZE)
	{
		log_print(HT_LOG_ERR, "pmsg error! hdr_len:%d, ctt_len:%d\n",
			p_msg ? p_msg->hdr_len : -1, p_msg ? p_msg->ctt_len : -1);
		return -1;
	}

	if (p_msg->ctt_len == 0)
	{
		body_buf[0] = '\0';
		return 0;
	}

	p_body = http_get_ctt(p_msg);
	if (p_body == NULL)
	{
		total_hdr = (int)strlen(p_msg->msg_buf) + 2 + p_msg->hdr_len;
		if (total_hdr <= 0)
		{
			log_print(HT_LOG_ERR, "invalid total_hdr:%d, hdr_len:%d\n", total_hdr, p_msg->hdr_len);
			return -1;
		}
		p_body = p_msg->msg_buf + total_hdr;
	}

	memcpy(body_buf, p_body, p_msg->ctt_len);
	body_buf[p_msg->ctt_len] = '\0';
	return 0;
}

int http_post_handler(HTTPCLN * p_cln, HTTPMSG * p_msg)
{
    int iRet = 0;
	//log_print(HT_LOG_INFO, "p_msg->msg_buf:%s\n", p_msg->msg_buf);
	char *body_buf = (char *)malloc(HTTP_BODY_BUF_SIZE);
	if (NULL == body_buf)
	{
        log_print(HT_LOG_ERR, "body_buf malloc failed!\n");
        return -1;
	}
	memset(body_buf, 0, HTTP_BODY_BUF_SIZE);
	//log_print(HT_LOG_INFO, "p_msg->hdr_len:%d, p_msg->ctt_len:%d\n", p_msg->hdr_len, p_msg->ctt_len);
	iRet = get_http_body_msg(p_cln, body_buf, p_msg);
	if (iRet != 0)
	{
        log_print(HT_LOG_ERR, "Client recv buf don't HTTP!\n");
        free(body_buf);
        return -1;
	}

	//log_print(HT_LOG_INFO, "body_buf:%s\n", body_buf);
	//log_print(HT_LOG_INFO, "p_cln->rcv_buf:%s\n", p_cln->rcv_buf);
	
	char host[256] = {0};
	char remote_ip[64] = {0};
	get_remote_ip(p_cln, remote_ip);
	snprintf(host, sizeof(host), "%s:%d", g_onvif_cls.server_ip, g_onvif_cls.http_port);

	char uripath[128] = {0};
	get_url_http(p_msg->first_line.value_string, uripath);
	log_print(HT_LOG_INFO, "uripath:%s\n", uripath);

	iRet = http_post_proc((void *)p_cln, (void *)p_msg, uripath, p_msg->msg_buf, body_buf, remote_ip, host, http_response);

	free(body_buf);

	return 0;
}

int http_put_handler(HTTPCLN * p_cln, HTTPMSG * p_msg)
{
    int iRet = 0;
	//log_print(HT_LOG_INFO, "p_msg->msg_buf:%s\n", p_msg->msg_buf);
	char *body_buf = (char *)malloc(HTTP_BODY_BUF_SIZE);
	if (NULL == body_buf)
	{
        log_print(HT_LOG_ERR, "body_buf malloc failed!\n");
        return -1;
	}

	memset(body_buf, 0, HTTP_BODY_BUF_SIZE);
	
	iRet = get_http_body_msg(p_cln, body_buf, p_msg);
	if (iRet != 0)
	{
        log_print(HT_LOG_ERR, "Client recv buf don't HTTP!\n");
        free(body_buf);
        return -1;
	}

	//log_print(HT_LOG_INFO, "body_buf:%s\n", body_buf);
	//log_print(HT_LOG_INFO, "p_cln->rcv_buf:%s\n", p_cln->rcv_buf);
	
	char host[256] = {0};
	char remote_ip[64] = {0};
	get_remote_ip(p_cln, remote_ip);
	snprintf(host, sizeof(host), "%s:%d", g_onvif_cls.server_ip, g_onvif_cls.http_port);
	
	char uripath[128] = {0};
	get_url_http(p_msg->first_line.value_string, uripath);
	log_print(HT_LOG_INFO, "uripath:%s\n", uripath);
	
	char firstline[128] = {0};
	if (p_msg->msg_sub_type == DELETE)
		sprintf(firstline, "DELETE %s", p_msg->first_line.value_string);
	else
		sprintf(firstline, "PUT %s", p_msg->first_line.value_string);
	
	iRet = http_put_proc((void *)p_cln, uripath, firstline, body_buf, remote_ip, host, http_response);

	free(body_buf);

	return 0;
}

int http_get_handler(HTTPCLN * p_cln, HTTPMSG * p_msg)
{
	int iRet = 0;
	char host[256] = {0};
	char remote_ip[64] = {0};
	get_remote_ip(p_cln, remote_ip);
	sprintf(host, "%s:%d", g_onvif_cls.server_ip, g_onvif_cls.http_port);

	char uripath[128] = {0};
	get_url_http(p_msg->first_line.value_string, uripath);

	log_print(HT_LOG_DBG, "uripath:%s, remote_ip:%s, host:%s\n", uripath, remote_ip, host);

	iRet = http_get_proc((void*)p_cln, (void*)p_msg, uripath, remote_ip, host);
	return iRet;
}


void http_upgrade_start(HTTPCLN *p_cln)
{
    int fd = p_cln->cfd;
    http_set_upgrade_info(fd, HTTP_UPGARDE_START);

    pthread_mutex_lock(&s_upgrade_mutex);
    sUpgradeHeader.header_len = 0;
    if (sUpgradeHeader.saved_header != NULL)
    {
        free(sUpgradeHeader.saved_header);
        sUpgradeHeader.saved_header = NULL;
    }
    pthread_mutex_unlock(&s_upgrade_mutex);
}

int http_upgrade_trans_firmware(HTTPCLN * p_cln)
{
    int isFileEnd = -1;
    char md5_str[64] = {0};
    const char *web_upgrade_post = "POST /IPC_FirmwareUpgrade";
    FormDataBoundary *pstFirmwareBoundary = http_get_form_firmware_boundary();

    pthread_mutex_lock(&s_upgrade_mutex);

    // 第一次调用：保存http header
    if (0 == sUpgradeHeader.header_len) 
    {
        if (sUpgradeHeader.saved_header != NULL)
        {
            free(sUpgradeHeader.saved_header);
        }

        sUpgradeHeader.header_len = p_cln->rcv_dlen;
        sUpgradeHeader.saved_header = (char *)malloc(sUpgradeHeader.header_len);
        if (NULL == sUpgradeHeader.saved_header) 
        {
            log_print(HT_LOG_INFO, "saved_header malloc failed!\n");
            pthread_mutex_unlock(&s_upgrade_mutex);
            return -1;
        }
        memcpy(sUpgradeHeader.saved_header, p_cln->rbuf, sUpgradeHeader.header_len);

        pthread_mutex_unlock(&s_upgrade_mutex);

        http_set_upgrade_info(p_cln->cfd, HTTP_UPGARDE_RECV_HEADER);
        return 0;
    }

    int buf_len = p_cln->rcv_dlen;
    char *total_data = NULL;
    int total_len = 0;

    // 第二次调用：拼接第一次的http header + 第二次的http data 然后传输
    if (NULL != sUpgradeHeader.saved_header)
    {
        total_len = sUpgradeHeader.header_len + p_cln->rcv_dlen;
        buf_len = total_len;

        total_data = (char *) malloc(total_len);
        if (NULL == total_data)
        {
            log_print(HT_LOG_INFO, "total_data malloc failed!\n");
            pthread_mutex_unlock(&s_upgrade_mutex);
            return -1;
        }
        
        memcpy(total_data, sUpgradeHeader.saved_header, sUpgradeHeader.header_len);
        memcpy(total_data + sUpgradeHeader.header_len, p_cln->rbuf, p_cln->rcv_dlen);

        free(sUpgradeHeader.saved_header);
        sUpgradeHeader.saved_header = NULL;
    }

    pthread_mutex_unlock(&s_upgrade_mutex);

    if (total_data != NULL && total_len != 0)
    {
        isFileEnd = http_recv_form_file(p_cln, web_upgrade_post,
                                pstFirmwareBoundary, md5_str,
                                p_cln->cfd, total_data, total_len, &buf_len);

        http_set_upgrade_info(p_cln->cfd, HTTP_UPGARDE_TRANS_DATA);

        free(total_data);
        total_data = NULL;
    }
    else
    {
        isFileEnd = http_recv_form_file(p_cln, web_upgrade_post,
                                pstFirmwareBoundary, md5_str,
                                p_cln->cfd, p_cln->rbuf, p_cln->mlen, &buf_len);
    }

    if (1 == isFileEnd)
    {
        http_set_upgrade_info(p_cln->cfd, HTTP_UPGARDE_COMPELET);
        if(0 == pstFirmwareBoundary->fileSize)
        {
            pstFirmwareBoundary->fileSize = pstFirmwareBoundary->recvSize;
        }

        if (1 == p_cln->pass_through)
        {
            p_cln->pass_through = 0;

            if (p_cln->dyn_recv_buf != NULL)
            {
                free(p_cln->dyn_recv_buf);
                p_cln->dyn_recv_buf = NULL;
            }
            p_cln->rbuf = NULL;
        }
    }

    return 0;
}


void * task_manager(OIMSG *stm)
{
	log_print(HT_LOG_DBG, "task_manager\n");

	HTTPMSG * p_msg = (HTTPMSG *)stm->msg_buf;
	HTTPCLN * p_cln = (HTTPCLN *)stm->msg_dua;

    if (NULL == p_msg || NULL == p_cln)
    {
    	log_print(HT_LOG_ERR, "p_msg or p_cln is NULL\n");
    	return NULL;
    }
	
	if (p_msg->msg_sub_type == POST)//POST
	{
		log_print(HT_LOG_DBG, "POST %s\n", p_msg->first_line.value_string);

        if (strstr(p_msg->first_line.value_string, "setFirmwareUpgradeFreeMemory"))
        {
            http_upgrade_start(p_cln);
        }

        if (http_is_firmware_upgrade_path(p_msg->first_line.value_string))
        {
            http_upgrade_trans_firmware(p_cln);
        }
        else
        {
            http_post_handler(p_cln, p_msg);
        }

	}
	else if (p_msg->msg_sub_type == GET)//GET
	{
		log_print(HT_LOG_DBG, "GET\n");
		http_get_handler(p_cln, p_msg);

	}
	else if (p_msg->msg_sub_type == PUT || p_msg->msg_sub_type == DELETE)//PUT
	{
		log_print(HT_LOG_DBG, "PUT\n");
		http_put_handler(p_cln, p_msg);
	}

	return NULL;
}

