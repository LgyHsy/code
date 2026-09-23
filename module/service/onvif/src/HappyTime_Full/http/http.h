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

#ifndef	_HTTP_H_
#define	_HTTP_H_

#include "sys_buf.h"
#include "ppstack.h"

#ifdef HTTPS
#include "openssl/ssl.h"
#endif

/***************************************************************************************/

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
} HTTP_MT;

typedef enum http_content_type
{
	CTT_NULL = 0,
	CTT_SDP,
	CTT_TXT,
	CTT_HTM,
	CTT_XML,
	CTT_BIN,
	CTT_JPG,
	CTT_RTSP_TUNNELLED,
	CTT_MULTIPART,
	CTT_FLV,
	CTT_FORM
} HTTPCTT;


typedef enum http_client_recv_mode
{
    RCV_NORMAL = 0,
    RCV_UPGRADEFIRMWARE,
}RCV_MODE_E;

/***************************************************************************************/

#define ctt_is_string(type)	(type == CTT_XML || type == CTT_HTM || type == CTT_TXT || type == CTT_SDP)


typedef struct _http_msg_content
{
	uint32	        msg_type;
	uint32	        msg_sub_type;
	HDRV            first_line;

	PPSN_CTX        hdr_ctx;
	PPSN_CTX        ctt_ctx;

	int             hdr_len;
	int             ctt_len;
	HTTPCTT         ctt_type;
	char            boundary[256];
	int             keep_alive;

	char          * msg_buf;
	int             buf_offset;

	uint32	        remote_ip;
	uint16	        remote_port;
} HTTPMSG;

/***************************************************************************************/

typedef struct http_client
{
	uint32          pass_through : 1;   // pass through received data flag
	uint32          https        : 1;   // https flag
	uint32          keep_alive   : 1;   // keep alive flag
	uint32          resv         : 29;

	SOCKET          cfd;                // client socket
	uint32	        rip;                // remote ip, network byte order
	uint32	        rport;              // remote port

	char            rcv_buf[MAX_BUF_SIZE + 4];      // static receiving buffer
	char          * dyn_recv_buf;       // dynamic receiving buffer
	int             rcv_dlen;           // received data length
	int             hdr_len;            // http header length
	int	            ctt_len;            // context  length
	HTTPCTT         ctt_type;           // context type
	char          * rbuf;               // pointer to rcv_buf or dyn_recv_buf
	int             mlen;				// sizeof(rcv_buf) or size of dyn_recv_buf
	RCV_MODE_E      rcv_mode;           // recv mode

	void          * http_srv;           // pointer to HTTPSRV
	int             use_count;          // use count
	uint32          last_time;          // last time

	void          * userdata;           // user data
	void          * userdata_mutex;     // user data mutex
	uint32          protocol;           // protocol, rtsp over http, websocket etc

#ifdef HTTPS
	SSL           * ssl;                // https SSL 
	void          * ssl_mutex;          // https SSL mutex
#endif	
} HTTPCLN;

typedef struct http_req
{
    uint32          need_auth : 1;      // need auth flag
    uint32          https     : 1;      // https flag
    uint32          resv      : 30;
    
	SOCKET          cfd;                // client socket    
	uint32	        port;               // server port
	char	        host[256];          // server host
	char	        url[256];           // the request url

	char 	        action[256];        // action
	char	        rcv_buf[MAX_BUF_SIZE + 4];      // static receiving buffer
	char          * dyn_recv_buf;       // dynamic receiving buffer
	int		        rcv_dlen;           // received data length
	int		        hdr_len;            // http header length            
	int		        ctt_len;            // context  length
	char            boundary[256];      // boundary, for CTT_MULTIPART
	char          * rbuf;               // pointer to rcv_buf or dyn_recv_buf
	int		        mlen;				// sizeof(rcv_buf) or size of dyn_recv_buf

	HTTPMSG       * rx_msg;             // rx message

	int  	        auth_mode;    		// 0 - baisc; 1 - digest
	HD_AUTH_INFO    auth_info;          // http auth information
	
#ifdef HTTPS
	SSL           * ssl;                // https SSL 
	void          * ssl_mutex;          // https SSL mutex
#endif
} HTTPREQ;

/***************************************************************************************/

/**
 * @brief http message callback 
 * 
 * If rx_msg is NULL, the callback should call the http_free_used_cln function to delete p_cln
 *
 * @param p_cln http client pointer
 * @param rx_msg http message
 * @param userdata user data
 *
 * @return If the callback is responsible for deleting rx_msg, 
 *         it returns TRUE, otherwise it returns FALSE
 */
typedef BOOL (*http_msg_cb)(HTTPCLN * p_cln, HTTPMSG * rx_msg, void * userdata);

/**
 * @brief HTTP raw data receiving callback
 *  only called when p_cln->pass_through is set to 1
 *  Used for RTSP over HTTP and RTSP over websocket        
 *
 *  @param p_cln http client pointer
 *  @param buff data buffer
 *  @param buflen data buffer length
 *
 */
typedef void (*http_data_cb)(HTTPCLN * p_cln, char * buff, int buflen, void * userdata);

/**
 * @brief http new connection callback 
 *
 *  @param p_srv http server pointer
 *  @param addr http client connection address, network byte order 
 *  @param port http client connection port, host byte order 
 *
 *  @return TRUE, accept the new connection, FALSE, reject the new connection 
 */
typedef BOOL (*http_conn_cb)(void * p_srv, uint32 addr, int port, void * userdata);

typedef struct http_srv_s
{
	uint32          r_flag  : 1;        // data receiving flag
	uint32          https   : 1;        // https flag
	uint32          resv    : 30;

	SOCKET          sfd;                // server socket

	char            host[128];          // local server address
	int             sport;              // server port
	uint32	        saddr;              // server address, network byte order
	uint32          max_cln_nums;       // max client number

	PPSN_CTX      * cln_fl;             // client free list
	PPSN_CTX      * cln_ul;             // client used list

	uint32          rx_num;             // data receiving thread numbers
	pthread_t     * rx_tid;             // data receiving thread id

	void          * mutex_cb;           // cabllback mutex
	http_msg_cb     msg_cb;             // http message callback
	http_data_cb    data_cb;            // http data callback
	http_conn_cb    conn_cb;            // http connection callback
	void          * msg_user;           // http message callback user data
	void          * data_user;          // http data callback user data
	void          * conn_user;          // http connection callback user data

#ifdef EPOLL
	int             ep_fd;              // epoll fd
	struct epoll_event * ep_events;     // epoll events
	int             ep_event_num;       // epoll event number    
#endif

#ifdef HTTPS
	char            cert_file[256];     // cert file name
	char            key_file[256];      // key file name
	SSL_CTX         * ssl_ctx;            // ssl context
#endif
} HTTPSRV;

#endif




