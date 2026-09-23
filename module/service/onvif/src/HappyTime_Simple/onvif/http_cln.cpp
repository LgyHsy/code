/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2010-2014, Happytimesoft Corporation, all rights reserved.
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
#include "http.h"
#include "http_cln.h"


BOOL http_tcp_tx(HTTPREQ * p_req, const char * p_data, int len)
{
	if (p_req->cfd <= 0)
		return FALSE;

	int slen = send(p_req->cfd, p_data, len, 0);
	if (slen != len)
	{
	    printf("http_tcp_tx::slen = %d, len = %d\r\n", slen, len);
		return FALSE;
    }
    
	return TRUE;
}

BOOL http_onvif_req(HTTPREQ * p_req, const char * p_xml, int len)
{
	char bufs[1024 * 4];
	if (len > 3072)
		return FALSE;

	int offset = 0;
	offset += sprintf(bufs+offset, "POST %s HTTP/1.1\r\n", p_req->url);
	offset += sprintf(bufs+offset, "Host: %s:%d\r\n", p_req->host, p_req->port);
	offset += sprintf(bufs+offset, "User-Agent: ltxd/1.0\r\n");
	offset += sprintf(bufs+offset, "Content-Type: application/soap+xml; charset=utf-8; action=\"%s\"\r\n", p_req->action);
	offset += sprintf(bufs+offset, "Content-Length: %d\r\n", len);
	offset += sprintf(bufs+offset, "Connection: close\r\n\r\n");

	memcpy(bufs+offset, p_xml, len);
	offset += len;
	bufs[offset] = '\0';

	return http_tcp_tx(p_req, bufs, offset);
}


BOOL http_onvif_trans(HTTPREQ * p_req, int timeout, const char * bufs, int len)
{
	p_req->cfd = tcp_connect_timeout(inet_addr(p_req->host), p_req->port, timeout);
	if (p_req->cfd <= 0)
	{
	    printf("http_onvif_trans::tcp_connect_timeout\n");
		return FALSE;
    }
    
	http_onvif_req(p_req, bufs, len);

	closesocket(p_req->cfd);
    
	return TRUE;
}



