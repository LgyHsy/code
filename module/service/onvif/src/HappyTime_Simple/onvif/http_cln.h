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

#ifndef HTTP_CLN_H
#define HTTP_CLN_H

#include "http.h"


typedef struct http_req
{	
	int				cfd;
    
	unsigned int	port;
	char			host[256];
	char			url[256];

	char 			action[256];

	char			rcv_buf[2048];
	char *			dyn_recv_buf;
	int				rcv_dlen;
	int				hdr_len;
	int				ctt_len;
	char *			p_rbuf;				// --> rcv_buf or dyn_recv_buf
	int				mlen;				// = sizeof(rcv_buf) or size of dyn_recv_buf

	HTTPMSG *		rx_msg;
}HTTPREQ;

#ifdef __cplusplus
extern "C" {
#endif

BOOL http_onvif_trans(HTTPREQ * p_req, int timeout, const char * bufs, int len);

#ifdef __cplusplus
}
#endif

#endif


