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

#ifndef	__H_HTTP_H__
#define	__H_HTTP_H__

/***************************************************************************************/
typedef struct http_client
{
	int				cfd;
	unsigned int	rip;
	unsigned int	rport;

	unsigned int	guid;

	char			rcv_buf[2048];
	char *			dyn_recv_buf;
	int				rcv_dlen;
	int				hdr_len;
	int				ctt_len;
	char *			p_rbuf;				// --> rcv_buf or dyn_recv_buf
	int				mlen;				// = sizeof(rcv_buf) or size of dyn_recv_buf
}HTTPCLN;

typedef enum http_request_msg_type
{
	HTTP_MT_NULL = 0,
	HTTP_MT_GET,
	HTTP_MT_HEAD,
	HTTP_MT_MPOST,
	HTTP_MT_MSEARCH,
	HTTP_MT_NOTIFY,
	HTTP_MT_POST,
	HTTP_MT_SUBSCRIBE,
	HTTP_MT_UNSUBSCRIBE,
	HTTP_MT_PUT,
	HTTP_MT_DELETE
}HTTP_MT;

/***************************************************************************************/
typedef enum http_content_type
{
	CTT_NULL = 0,
	CTT_SDP,
	CTT_TXT,
	CTT_HTM,
	CTT_XML
}HTTPCTT;

typedef struct _http_msg_content
{
	unsigned int	msg_type;
	unsigned int	msg_sub_type;
	HDRV 			first_line;

	PPSN_CTX		hdr_ctx;
	PPSN_CTX		ctt_ctx;

	int				hdr_len;
	int				ctt_len;

	HTTPCTT			ctt_type;

	char *			msg_buf;
	int				buf_offset;

	unsigned long	remote_ip;
	unsigned short	remote_port;
	unsigned short	local_port;
}HTTPMSG;

/***************************************************************************************/
typedef struct http_srv_s
{
	int				r_flag;

	int				sfd;

	int				sport;
	unsigned int	saddr;

	unsigned int	guid;

	PPSN_CTX *		cln_fl;
	PPSN_CTX *		cln_ul;

	pthread_t		rx_tid;
}HTTPSRV;

/***************************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

BOOL 	is_http_msg(char * msg_buf);
void 	http_headl_parse(char * pline, int llen, HTTPMSG * p_msg);
int 	http_line_parse(char * p_buf, int max_len, char sep_char, PPSN_CTX * p_ctx);
int 	http_ctt_parse(HTTPMSG * p_msg);

int 	http_msg_parse(char * msg_buf,int msg_buf_len,HTTPMSG * msg);

int 	http_msg_parse_part1(char * p_buf,int buf_len,HTTPMSG * msg);
int 	http_msg_parse_part2(char * p_buf,int buf_len,HTTPMSG * msg);

HDRV  * find_http_headline(HTTPMSG * msg, const char * head);
char  * get_http_headline(HTTPMSG * msg, const char * head);

HDRV  * find_ctt_headline(HTTPMSG * msg, const char * head);
char  * get_http_ctt(HTTPMSG * msg);

/***************************************************************************************/
BOOL 	http_msg_buf_fl_init(int num);
void 	http_msg_buf_fl_deinit();

HTTPMSG * http_get_msg_buf();
void 	http_msg_ctx_init(HTTPMSG * msg);
void 	http_free_msg_buf(HTTPMSG * msg);
unsigned int http_idle_msg_buf_num();
HTTPMSG * http_get_msg_large_buf(int size);

/***************************************************************************************/
void 	free_http_msg(HTTPMSG * msg);
void 	free_http_msg_content(HTTPMSG * msg);
void 	free_http_msg_ctx(HTTPMSG * msg,int type);	//0:sip list; 1:sdp list;

/***************************************************************************************/
void	soap_process_request(HTTPCLN * p_user, HTTPMSG * rx_msg);

/***************************************************************************************/
int 	http_pkt_find_end(char * p_buf);
void 	http_commit_rx_msg(HTTPCLN * p_user, HTTPMSG * rx_msg);
BOOL 	http_tcp_rx(HTTPCLN * p_user);
int	 	http_tcp_listen_rx(HTTPSRV * p_srv);
void  * http_rx_thread(void * argv);

/***************************************************************************************/
int 	http_srv_net_init(HTTPSRV * p_srv);
int 	http_srv_init(HTTPSRV * p_srv, unsigned int saddr, unsigned short sport, int cln_num);
void 	http_srv_deinit(HTTPSRV * p_srv);

/***************************************************************************************/
unsigned long http_cln_index(HTTPSRV * p_srv, HTTPCLN * p_cln);
HTTPCLN * http_get_cln_by_index(HTTPSRV * p_srv, unsigned long index);
HTTPCLN * http_get_idle_cln(HTTPSRV * p_srv);
void 	http_free_used_cln(HTTPSRV * p_srv, HTTPCLN * p_cln);
HTTPCLN * rpc_find_cln(HTTPSRV * p_srv, unsigned int rip, unsigned short rport);

#ifdef __cplusplus
}
#endif

#endif	//	__H_HTTP_H__




