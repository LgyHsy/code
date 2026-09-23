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
#include "http.h"
#include "http_srv.h"
#include "http_parse.h"
// #include "check_util.h"
#include "anj_mw_time.h"
#include "anj_mw_mutex.h"
#include "anj_mw_thread.h"
#include "anj_config_oem.h"

#ifdef HTTPD
#include "httpd.h"
#endif


/***************************************************************************************/

#define HTTP_LISTEN_WAIT_MAX_NUMS   64
#define HTTP_SOCKETS        60
#define KEEPALIVE_TIMEOUT   5   // 5 second timeout
#define KEEPALIVE_ERROR_RANGE   2   //2s误差
#define KEEPALIVE_UPGRADE_TIMEOUT   30

#define SERVER_CERT_FILE "./https.crt"
#define SERVER_PRIVATE_KEY_FILE "./https.key"
#define SERVER_CERT_FILE_UPLOAD "/mnt/nand/https.crt"
#define SERVER_PRIVATE_KEY_FILE_UPLOAD "/mnt/nand/https.key"
#define SERVER_PRIVATE_KEY_PWD_UPLOAD "/mnt/nand/https.pwd"
#define SERVER_PRIVATE_KEY_PASS "5MFc62xs67"//"xNwu8iRCt4"
extern HttpsPrivateStruct  g_HttpsPrivateInfo; 						//1 https信息

typedef struct
{
    int fd;
    HttpUpgradeStat stat;
}HttpUpgradeInfo;

typedef struct
{
    uint32    idx;
    HTTPSRV * srv;
} HttpThreadParam;

/***************************************************************************************/

static pthread_mutex_t sHttpUpgradeMutex = PTHREAD_MUTEX_INITIALIZER;
static HttpUpgradeInfo sHttpUpgradeInfo = {0};

static uint32 onvif_get_uptime_sec(void)
{
    return (uint32)(anj_mw_get_cputime_ms(NULL) / 1000ULL);
}

void http_set_upgrade_info(int fd, HttpUpgradeStat status)
{
    pthread_mutex_lock(&sHttpUpgradeMutex);
    sHttpUpgradeInfo.fd = fd;
    sHttpUpgradeInfo.stat = status;
    pthread_mutex_unlock(&sHttpUpgradeMutex);
}

void http_reset_upgrade_info()
{
    pthread_mutex_lock(&sHttpUpgradeMutex);
    sHttpUpgradeInfo.fd = 0;
    sHttpUpgradeInfo.stat = HTTP_UPGARDE_NONE;
    pthread_mutex_unlock(&sHttpUpgradeMutex);
}

int http_get_upgrade_fd()
{
    pthread_mutex_lock(&sHttpUpgradeMutex);
    int fd = sHttpUpgradeInfo.fd;
    pthread_mutex_unlock(&sHttpUpgradeMutex);

    return fd;
}

HttpUpgradeStat http_get_upgrade_status()
{
    pthread_mutex_lock(&sHttpUpgradeMutex);
    HttpUpgradeStat status = (int)sHttpUpgradeInfo.stat;
    pthread_mutex_unlock(&sHttpUpgradeMutex);

    return status;
}

#define HTTP_UPGRADE_TRANS_WAIT_TIMES   60
int http_wait_recv_upgrade_header()
{
    int iRetryTimes = 0;
    HttpUpgradeStat status = HTTP_UPGARDE_NONE;
    status = http_get_upgrade_status();
    if (HTTP_UPGARDE_NONE == status)
    {
        return TRUE;
    }
    
    while(iRetryTimes++ < HTTP_UPGRADE_TRANS_WAIT_TIMES)
    {
        status = http_get_upgrade_status();
        if (status != HTTP_UPGARDE_RECV_HEADER)
        {
            usleep(20 * 1000);
            continue;
        }
        else
        {
            //log_print(HT_LOG_INFO, "[%s, %d] wait recv http head times:%d!\n", __func__, __LINE__, iRetryTimes);
            break;
        }
    }

    if (iRetryTimes > HTTP_UPGRADE_TRANS_WAIT_TIMES)
    {
        log_print(HT_LOG_INFO, "[%s, %d] wait recv http head too many times:%d!\n", __func__, __LINE__, iRetryTimes);
        return FALSE;
    }

    return TRUE;
}


#ifdef HTTPS

#include "openssl/err.h"

#if __WINDOWS_OS__
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")
#endif


/* initialize SSL server and create context */
SSL_CTX * http_init_ssl_ctx()
{
	const SSL_METHOD * method;
	SSL_CTX * ctx;

	SSLeay_add_ssl_algorithms();		/* load & register all cryptos, etc. */
	SSL_load_error_strings();			/* load all error messages */
	method = SSLv23_server_method();	/* create new server-method instance */
	ctx = SSL_CTX_new(method);			/* create new context from method */
	if (ctx == NULL)
	{
		log_print(HT_LOG_ERR,  "%s, SSL_CTX_new failed\r\n", __FUNCTION__);
	}
	
	return ctx;
}

/* LoadCertificates - load from files */
void http_load_certificates(SSL_CTX * ctx, const char * cert_file, const char * key_file)
{
	if (1)
	{
		int has_upload = 0;
		int has_https_pwd = 0;
		char httpsPrivateKeyPass_in[32] = {0};
		FILE *fp_https = NULL;
		fp_https = fopen(SERVER_PRIVATE_KEY_PWD_UPLOAD, "r");
		if(fp_https != NULL)
		{
			fread(g_HttpsPrivateInfo.httpsPrivateKeyPass, 1, sizeof(g_HttpsPrivateInfo.httpsPrivateKeyPass) - 1, fp_https);
			fclose(fp_https);
		}
		else
		{
			strcpy(g_HttpsPrivateInfo.httpsPrivateKeyPass, "");
		}
		if(access(SERVER_PRIVATE_KEY_PWD_UPLOAD, F_OK)==0)
			has_https_pwd = 1;
		
		//把最后的换行符去掉
		int i = 0;
		if((strlen(g_HttpsPrivateInfo.httpsPrivateKeyPass) != 0) && (has_https_pwd == 1))
		{
			for(i=0;i<(strlen(g_HttpsPrivateInfo.httpsPrivateKeyPass)-1);i++)
			{
				if(g_HttpsPrivateInfo.httpsPrivateKeyPass[i]=='\n') 
					break;
				httpsPrivateKeyPass_in[i] = g_HttpsPrivateInfo.httpsPrivateKeyPass[i];
			}
		}
		else
		{
			strcpy(httpsPrivateKeyPass_in,"");
		}
		if(has_upload == 0)
		{
			SSL_CTX_set_default_passwd_cb_userdata(ctx, (void*)"");
		}
		else
		{
			if(has_https_pwd)
			{
				if(httpsPrivateKeyPass_in != NULL)
					SSL_CTX_set_default_passwd_cb_userdata(ctx, (void*)httpsPrivateKeyPass_in);
				else
					SSL_CTX_set_default_passwd_cb_userdata(ctx, (void*)SERVER_PRIVATE_KEY_PASS);
			}
			else
			{
				SSL_CTX_set_default_passwd_cb_userdata(ctx, (void*)"");
			}
		}
	}
	/* set the local certificate from CertFile */
	if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0)
	{
		log_print(HT_LOG_ERR,  "%s, SSL_CTX_use_certificate_file failed\r\n", __FUNCTION__);
		return;
	}
	
	/* set the private key from KeyFile (may be the same as CertFile) */
	if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
	{
		log_print(HT_LOG_ERR,  "%s, SSL_CTX_use_PrivateKey_file failed\r\n", __FUNCTION__);
		return;
	}
	
	/* verify private key */
	if (!SSL_CTX_check_private_key(ctx))
	{
		log_print(HT_LOG_ERR,  "%s, Private key does not match the public certificate\r\n", __FUNCTION__);
		return;
	}
}

#endif // end of HTTPS

/***************************************************************************************/

BOOL http_commit_rx_msg(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
	BOOL ret = FALSE;
	HTTPSRV * p_srv = (HTTPSRV *)p_user->http_srv;

	anj_mutex_lock((pthread_mutex_t *)p_srv->mutex_cb);

	if (p_srv->msg_cb)
	{
		ret = p_srv->msg_cb(p_user, rx_msg, p_srv->msg_user); //onvif_http_msg_cb 回调
	}

	anj_mutex_unlock((pthread_mutex_t *)p_srv->mutex_cb);

	return ret;
}

/*
 * The message previously received by the client may not have been processed yet and 
 *  needs to be sent to the main task queue for deletion
 */
void http_commit_free_cln(HTTPSRV * p_srv, HTTPCLN * p_cln)
{
	if (p_cln->cfd > 0)
	{
#ifdef EPOLL	
		epoll_ctl(p_srv->ep_fd, EPOLL_CTL_DEL, p_cln->cfd, NULL);
#endif

		closesocket(p_cln->cfd);
		p_cln->cfd = 0;
	}
	
	anj_mutex_lock((pthread_mutex_t *)p_srv->mutex_cb);

	if (p_srv->msg_cb)
	{
		p_srv->msg_cb(p_cln, NULL, p_srv->msg_user); //onvif_http_msg_cb 回调
	}

	anj_mutex_unlock((pthread_mutex_t *)p_srv->mutex_cb);
}

void http_commit_rx_data(HTTPCLN * p_user, char * buff, int buflen)
{
	HTTPSRV * p_srv = (HTTPSRV *)p_user->http_srv;
	if (NULL == p_srv)
	{
		return;
	}

	anj_mutex_lock((pthread_mutex_t *)p_srv->mutex_cb);

	if (p_srv->data_cb)
	{
		p_srv->data_cb(p_user, buff, buflen, p_srv->data_user);
	}

	anj_mutex_unlock((pthread_mutex_t *)p_srv->mutex_cb);
}

BOOL http_commit_connection(HTTPSRV * p_srv, uint32 addr, int port)
{
	BOOL ret = TRUE;

	anj_mutex_lock((pthread_mutex_t *)p_srv->mutex_cb);

	if (p_srv->conn_cb)
	{
		ret = p_srv->conn_cb(p_srv, addr, port, p_srv->conn_user);
	}

	anj_mutex_unlock((pthread_mutex_t *)p_srv->mutex_cb);

	return ret;
}

BOOL http_data_rx(HTTPCLN * p_user)
{
    BOOL bRet = TRUE;
	int rlen = 0;
	HTTPMSG * rx_msg = NULL;

	if (p_user->rbuf == NULL)
	{
		p_user->rbuf = p_user->rcv_buf;
		p_user->mlen = sizeof(p_user->rcv_buf) - 4;
		p_user->rcv_dlen = 0;
		p_user->ctt_len = 0;
		p_user->hdr_len = 0;
	}


#ifdef HTTPS
	if (p_user->https)
	{
		anj_mutex_lock((pthread_mutex_t *)p_user->ssl_mutex);
		if (p_user->ssl)
		{
			rlen = SSL_read(p_user->ssl, p_user->rbuf+p_user->rcv_dlen, p_user->mlen-p_user->rcv_dlen);
		}
		anj_mutex_unlock((pthread_mutex_t *)p_user->ssl_mutex);
	}
	else 
#endif

	rlen = recv(p_user->cfd, p_user->rbuf + p_user->rcv_dlen, p_user->mlen - p_user->rcv_dlen, 0);

	if (rlen <= 0)
	{
		/* rlen==0: peer closed normally; rlen<0: real recv error */
		if (rlen == 0)
			log_print(HT_LOG_DBG,  "%s, recv return = %d, dlen[%d], mlen[%d]\r\n", __FUNCTION__, rlen, p_user->rcv_dlen, p_user->mlen);
		else
			log_print(HT_LOG_WARN,  "%s, recv return = %d, dlen[%d], mlen[%d]\r\n", __FUNCTION__, rlen, p_user->rcv_dlen, p_user->mlen);
		return FALSE;
	}

	p_user->rcv_dlen += rlen;
	p_user->rbuf[p_user->rcv_dlen] = '\0';
	p_user->last_time = onvif_get_uptime_sec();

	//log_print(HT_LOG_INFO, "p_user->pass_through:%d\n", p_user->pass_through);
	if (p_user->pass_through)
	{
		http_commit_rx_data(p_user, p_user->rbuf, p_user->rcv_dlen);

		p_user->hdr_len = 0;
		p_user->ctt_len = 0;
		p_user->rcv_dlen = 0;

        // 因为msg传递是异步的，要等http header保存完后，再用32k的buf传数据。 透传完一次data后，使用增大后的buf：dyn_recv_buf
        if (p_user->dyn_recv_buf != NULL)
        {
            memset(p_user->dyn_recv_buf, 0, CLIENT_TRANS_DATA_BUF_SIZE);
            p_user->rbuf = p_user->dyn_recv_buf;
        }
        else
        {
            p_user->rbuf = NULL;
        }

        if (NULL == p_user->dyn_recv_buf && CLIENT_TRANS_DATA_BUF_SIZE != p_user->mlen
            && CTT_FORM == p_user->ctt_type && RCV_UPGRADEFIRMWARE == p_user->rcv_mode)
        {
            p_user->dyn_recv_buf = (char *) malloc (CLIENT_TRANS_DATA_BUF_SIZE);
            if (NULL == p_user->dyn_recv_buf)
            {
                log_print(HT_LOG_INFO, "dyn_recv_buf malloc failed!\n");
                return FALSE;
            }

            p_user->rbuf = p_user->dyn_recv_buf;
            p_user->mlen = CLIENT_TRANS_DATA_BUF_SIZE;
        }

		return TRUE;
	}

	if (p_user->rcv_dlen < 16)
	{
		return TRUE;
	}

	if (http_is_http_msg(p_user->rbuf) == FALSE)//判断不是http返回
	{
		return FALSE;
	}

	//log_print(HT_LOG_INFO, "p_user->hdr_len:%d\n", p_user->hdr_len);
	if (p_user->hdr_len == 0) // http header length
	{
		int parse_len;
		int http_pkt_len;
		
		http_pkt_len = http_pkt_find_end(p_user->rbuf);
		if (http_pkt_len == 0)
		{
			return TRUE;
		}
		p_user->hdr_len = http_pkt_len;

		rx_msg = http_get_msg_buf(http_pkt_len + 1);
		if (rx_msg == NULL)
		{
			log_print(HT_LOG_ERR,  "%s, get_msg_buf ret null!!!\r\n", __FUNCTION__);
			return FALSE;
		}

		memcpy(rx_msg->msg_buf, p_user->rbuf, http_pkt_len);
		rx_msg->msg_buf[http_pkt_len] = '\0';
		
		log_print(HT_LOG_DBG,  "RX << %s\r\n", rx_msg->msg_buf);

		parse_len = http_msg_parse_part1(rx_msg->msg_buf, http_pkt_len, rx_msg);
		if (parse_len != http_pkt_len)
		{
			log_print(HT_LOG_ERR,  "%s, http_msg_parse_part1=%d, http_pkt_len=%d!!!\r\n", __FUNCTION__, parse_len, http_pkt_len);
			http_free_msg(rx_msg);
			return FALSE;
		}
		
		p_user->ctt_len = rx_msg->ctt_len;
		p_user->ctt_type = rx_msg->ctt_type;
		p_user->keep_alive = rx_msg->keep_alive;

		if (CTT_RTSP_TUNNELLED == p_user->ctt_type)
		{
			if (http_commit_rx_msg(p_user, rx_msg) == FALSE)
			{
				http_free_msg(rx_msg);
				return FALSE;
			}

			if (p_user->rcv_dlen - http_pkt_len > 0 && p_user->pass_through)
			{
				http_commit_rx_data(p_user, p_user->rbuf+p_user->hdr_len, p_user->rcv_dlen-http_pkt_len);
			}

			p_user->hdr_len = 0;
			p_user->ctt_len = 0;
			p_user->rbuf = 0;
			p_user->rcv_dlen = 0;
			
			return TRUE;
		}
	}

	if (p_user->ctt_len > 0 && (p_user->ctt_len + p_user->hdr_len) > p_user->mlen) //大于接收大小
	{
	    if (CTT_FORM == p_user->ctt_type)       //如果是表单
	    {
	        //先直接传递
            bRet = http_commit_rx_msg(p_user, rx_msg);
            if (FALSE == bRet)
            {
    			http_free_msg(rx_msg);
    			return FALSE;
            }

            // 在这里阻塞是防止 msg还未解析完，又recv到数据，会覆盖client的rcv_buf内容
            http_wait_recv_upgrade_header(p_user);
   
            // 重置缓冲区指针，准备接收后续数据
            p_user->pass_through = 1;
            p_user->rbuf = p_user->rcv_buf;
            p_user->mlen = sizeof(p_user->rcv_buf);
            p_user->rcv_dlen = 0;
            p_user->hdr_len = 0;
            p_user->ctt_len = 0;
            p_user->rcv_mode = RCV_UPGRADEFIRMWARE;
            
            return TRUE;
	    }
	    else        //如果不是文件，继续走之前的逻辑
	    {
    		if (p_user->dyn_recv_buf)
    		{
    			log_print(HT_LOG_INFO,  "%s, dyn_recv_buf=%p, mlen=%d!!!\r\n", __FUNCTION__, p_user->dyn_recv_buf, p_user->mlen);
    			free(p_user->dyn_recv_buf);
    		}

    		p_user->dyn_recv_buf = (char *)malloc(p_user->ctt_len + p_user->hdr_len + 1);
    		if (NULL == p_user->dyn_recv_buf)
    		{
    			http_free_msg(rx_msg);

    			log_print(HT_LOG_INFO,  "%s, malloc failed\r\n", __FUNCTION__);
    			return FALSE;
    		}

    		memcpy(p_user->dyn_recv_buf, p_user->rcv_buf, p_user->rcv_dlen);

    		p_user->rbuf = p_user->dyn_recv_buf;
    		p_user->mlen = p_user->ctt_len + p_user->hdr_len;

    		http_free_msg(rx_msg);
    		return TRUE;
		}
	}

	if (p_user->rcv_dlen >= (p_user->ctt_len + p_user->hdr_len)) // received data length //context length // http header length
	{
	    if (p_user->rcv_mode != RCV_NORMAL)
	    {
            p_user->rcv_mode = RCV_NORMAL;
	    }

		if (rx_msg == NULL)
		{
			int parse_len;
			int nlen;

			nlen = p_user->ctt_len + p_user->hdr_len;

			rx_msg = http_get_msg_buf(nlen + 1);
			if (rx_msg == NULL)
			{
			    log_print(HT_LOG_INFO, "[%s, %5d] rx_msg malloc failed!\n", __func__, __LINE__);
				log_print(HT_LOG_ERR,  "%s, get msg buf failed\r\n", __FUNCTION__);
				return FALSE;
			}	

			memcpy(rx_msg->msg_buf, p_user->rbuf, p_user->hdr_len);
			rx_msg->msg_buf[p_user->hdr_len] = '\0';
			
			parse_len = http_msg_parse_part1(rx_msg->msg_buf, p_user->hdr_len, rx_msg);
			if (parse_len != p_user->hdr_len)
			{
				log_print(HT_LOG_ERR,  "%s, http_msg_parse_part1=%d, hdr_len=%d!!!\r\n", __FUNCTION__, parse_len, p_user->hdr_len);
				http_free_msg(rx_msg);
				return FALSE;
			}
		}

		if (p_user->ctt_len > 0) //context length
		{
			int parse_len;

			memcpy(rx_msg->msg_buf + p_user->hdr_len, p_user->rbuf + p_user->hdr_len, p_user->ctt_len);
			rx_msg->msg_buf[p_user->hdr_len + p_user->ctt_len] = '\0';

			if (ctt_is_string(rx_msg->ctt_type))
			{
				log_print(HT_LOG_DBG,  "%s\r\n\r\n", rx_msg->msg_buf+p_user->hdr_len);
			}

			parse_len = http_msg_parse_part2(rx_msg->msg_buf+p_user->hdr_len, p_user->ctt_len, rx_msg);
			if (parse_len != p_user->ctt_len)
			{
				log_print(HT_LOG_WARN,  "%s, http_msg_parse_part2=%d, ctt_len=%d!!!\r\n", __FUNCTION__, parse_len, p_user->ctt_len);
			}
		}
		
		p_user->rcv_dlen -= p_user->hdr_len + p_user->ctt_len;

		if (p_user->dyn_recv_buf == NULL)// dynamic receiving buffer
		{
			if (p_user->rcv_dlen > 0)
			{
				memmove(p_user->rcv_buf, p_user->rcv_buf + p_user->hdr_len + p_user->ctt_len, p_user->rcv_dlen);
				p_user->rcv_buf[p_user->rcv_dlen] = '\0';
			}
			
			p_user->rbuf = p_user->rcv_buf;
			p_user->mlen = sizeof(p_user->rcv_buf) - 4;
			p_user->hdr_len = 0;
			p_user->ctt_len = 0;
		}
		else
		{
			free(p_user->dyn_recv_buf);
			p_user->dyn_recv_buf = NULL;
			p_user->hdr_len = 0;
			p_user->ctt_len = 0;
			p_user->rbuf = 0;
			p_user->rcv_dlen = 0;
		}

		if (http_commit_rx_msg(p_user, rx_msg) == FALSE)
		{
			http_free_msg(rx_msg);
			return FALSE;
		}
	}
	else if (rx_msg)
	{
		http_free_msg(rx_msg);
	}

	return TRUE;
}

void http_srv_rx(HTTPSRV * p_srv, HTTPCLN * p_cln)
{
#ifdef HTTPS
	if (p_cln->https)
	{
		do 
		{
			if (http_data_rx(p_cln) == FALSE)
			{
				http_commit_free_cln(p_srv, p_cln);
				break;
			}
			anj_mutex_lock((pthread_mutex_t *)p_cln->ssl_mutex);
			if (p_cln->ssl && SSL_pending(p_cln->ssl) > 0)
			{
				anj_mutex_unlock((pthread_mutex_t *)p_cln->ssl_mutex);
				continue;
			}
			anj_mutex_unlock((pthread_mutex_t *)p_cln->ssl_mutex);
			break;
		} while (1);
	}
	else
#endif

	if (http_data_rx(p_cln) == FALSE)
	{
		http_commit_free_cln(p_srv, p_cln);
	}
	return;
}

int http_listen_rx(HTTPSRV * p_srv)
{
	SOCKET cfd;
	struct sockaddr_in caddr;
	socklen_t size;
	HTTPCLN * p_cln;
	int len = 1024 * 1024;
#ifdef HTTPS
	SSL * ssl = NULL;
#endif
#ifdef EPOLL
	uint64 e_dat;
	struct epoll_event event;
#endif

	size = sizeof(struct sockaddr_in);
	cfd = accept(p_srv->sfd, (struct sockaddr *)&caddr, &size);
	if (cfd <= 0)
	{
		log_print(HT_LOG_ERR,  "%s, accept, cfd(%d), %s\r\n", __FUNCTION__, cfd, strerror(errno));
		return -1;
	}

#ifdef HTTPS
	if (p_srv->https)
	{
		ssl = SSL_new(p_srv->ssl_ctx);
		if (NULL == ssl)
		{
			log_print(HT_LOG_ERR,  "%s, SSL_new failed, %s\r\n", __FUNCTION__, ERR_reason_error_string(ERR_get_error()));
			goto err;
		}

		SSL_set_fd(ssl, (int)cfd);

		if (-1 == SSL_accept(ssl))
		{
			log_print(HT_LOG_ERR,  "%s, SSL_accept failed, %s\r\n", __FUNCTION__, ERR_reason_error_string(ERR_get_error()));
			goto err;
		}
	}
#endif

	if (!http_commit_connection(p_srv, caddr.sin_addr.s_addr, ntohs(caddr.sin_port)))
	{
		log_print(HT_LOG_INFO,  "%s, http reject connection!\r\n", __FUNCTION__);
		goto err;
	}

	p_cln = http_get_idle_cln(p_srv);
	if (NULL == p_cln)
	{
		log_print(HT_LOG_ERR,  "%s, http_get_idle_cln::ret null!!!\r\n", __FUNCTION__);
		goto err;
	}
	
	if (setsockopt(cfd, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN,  "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	
	if (setsockopt(cfd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN,  "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	
	p_cln->cfd = cfd;
	p_cln->rip = caddr.sin_addr.s_addr;
	p_cln->rport = ntohs(caddr.sin_port);

	p_cln->http_srv = p_srv;
	p_cln->last_time = onvif_get_uptime_sec();

#ifdef HTTPS
	if (p_srv->https)
	{
		p_cln->https = 1;
		p_cln->ssl = ssl;
		p_cln->ssl_mutex = malloc(sizeof(pthread_mutex_t));
		if (p_cln->ssl_mutex) anj_mutex_create((pthread_mutex_t *)p_cln->ssl_mutex, 0);
	}
#endif

	pps_ctx_ul_add(p_srv->cln_ul, p_cln);

	log_print(HT_LOG_DBG,  "http user over tcp from[0x%08x,%u]\r\n", p_cln->rip, p_cln->rport);

#ifdef EPOLL
	e_dat = http_cln_index(p_srv, p_cln);
	e_dat = e_dat << 32;
	e_dat = e_dat | cfd;

	event.events = EPOLLIN;
	event.data.u64 = e_dat;
	epoll_ctl(p_srv->ep_fd, EPOLL_CTL_ADD, cfd, &event);
#endif

	return 0;

err:

#ifdef HTTPS
	if (ssl)
	{
		SSL_free(ssl);
	}
#endif

	closesocket(cfd);

	return -1;
}

void * http_rx_thread(void * argv)
{
	HttpThreadParam * param = (HttpThreadParam *)argv;
	uint32 idx = param->idx;
	HTTPSRV * p_srv = param->srv;

	free(argv);
	
	if (p_srv == NULL)
	{
		return NULL;
	}

	while (p_srv->r_flag == 1)
	{
#ifdef EPOLL

		int i, fd, nfds;
		uint32 cur_time = onvif_get_uptime_sec();
		HTTPCLN * p_cln;

		nfds = epoll_wait(p_srv->ep_fd, p_srv->ep_events, p_srv->ep_event_num, 100);

		for (i = 0; i < nfds; i ++)
		{
			if (p_srv->ep_events[i].events & EPOLLIN)
			{
				fd = (int)(p_srv->ep_events[i].data.u64);
				if ((p_srv->ep_events[i].data.u64 & ((uint64)1 << 63)) != 0)
				{
					http_listen_rx(p_srv);
				}
				else
				{
					uint32 u_index = p_srv->ep_events[i].data.u64 >> 32;
					p_cln = http_get_cln_by_index(p_srv, u_index);

					if (NULL == p_cln)
					{
						break;
					}

					if (p_cln->cfd > 0 && p_cln->cfd == fd)
					{
						http_srv_rx(p_srv, p_cln);
					}
					else
					{
						log_print(HT_LOG_WARN,  "%s, event fd[%d] not match user fd[%d]!!!\r\n", __FUNCTION__, fd, p_cln->cfd);
					}
				}
			}
		}

        int upgrade_fd = 0;
        int upgrade_flag = 0;
        int timeout = KEEPALIVE_TIMEOUT;
		for (i = 0; i < (int)p_srv->max_cln_nums; i++)
		{
			p_cln = http_get_cln_by_index(p_srv, i);
			if (p_cln && p_cln->cfd > 0)
			{
			    upgrade_flag = 0;
                if (p_cln->keep_alive)
                {
                    upgrade_fd = http_get_upgrade_fd();
                    if (upgrade_fd > 0 && upgrade_fd == p_cln->cfd)
                    {
                        upgrade_flag = 1;
                        timeout = KEEPALIVE_UPGRADE_TIMEOUT;
                    }
                    else
                    {
                        timeout = KEEPALIVE_TIMEOUT;
                    }

                    if (cur_time > p_cln->last_time + timeout)
                    {
    					//log_print(HT_LOG_INFO, "[%s, %5d] client free! index:%d, fd:%d, port:%u, use_count:%d, timeout:%d, address:%p, upgrade_fd:%d flag:%d!\n", 
    					    //__func__, __LINE__, i, p_cln->cfd, p_cln->rport, p_cln->use_count, timeout, p_cln, upgrade_fd, upgrade_flag);

    					http_commit_free_cln(p_srv, p_cln);
    					if (1 == upgrade_flag)
    					{
                            http_reset_upgrade_info();
    					}
                    }
                }

			}
		}
		
#else
		int sret;
		int max_fd = 0;
		uint32 i, s, e;
		fd_set fdr;
		HTTPCLN * p_cln;
		struct timeval tv;
		uint32 cur_time = onvif_get_uptime_sec();

		FD_ZERO(&fdr);

		if (idx == 0)
		{	
			FD_SET(p_srv->sfd, &fdr);
			max_fd = (int)p_srv->sfd;
		}

		s = idx * HTTP_SOCKETS;
		e = (idx + 1) * HTTP_SOCKETS;

		e = e > p_srv->max_cln_nums ? p_srv->max_cln_nums : e;

		for (i = s; i < e; i++)
		{
			p_cln = http_get_cln_by_index(p_srv, i);
			if (p_cln && p_cln->cfd > 0)
			{
				if (p_cln->keep_alive && cur_time - p_cln->last_time > KEEPALIVE_TIMEOUT)
				{
					http_commit_free_cln(p_srv, p_cln);
				}
				else
				{
					FD_SET(p_cln->cfd, &fdr);
					max_fd = (int)(((int)p_cln->cfd > max_fd)? p_cln->cfd : max_fd);
				}
			}
		}

		if (max_fd == 0)
		{
			usleep(100*1000);
			continue;
		}
		
		tv.tv_sec = 0;
		tv.tv_usec = 100 * 1000;
		
		sret = select(max_fd + 1, &fdr, NULL, NULL, &tv);
		if (sret == 0)
		{
			continue;
		}
		else if (sret < 0)
		{
			usleep(1000);

			log_print(HT_LOG_ERR,  "%s, select err, max fd[%d], sret[%d], [%d,%s]\r\n", __FUNCTION__, max_fd, sret, errno, strerror(errno));
			continue;
		}

		if (FD_ISSET(p_srv->sfd, &fdr))
		{
			http_listen_rx(p_srv);
		}

		for (i = s; i < e; i++)
		{
			p_cln = http_get_cln_by_index(p_srv, i);

			if (p_cln && p_cln->cfd > 0 && FD_ISSET(p_cln->cfd, &fdr))
			{
				http_srv_rx(p_srv, p_cln);
			}
		}

#endif
	}

	p_srv->rx_tid[idx] = 0;
	
	log_print(HT_LOG_DBG,  "%s, exit\r\n", __FUNCTION__);

	return NULL;
}

int http_srv_net_init(HTTPSRV * p_srv)
{
	int reuse = 1;
	struct sockaddr_in addr;

	p_srv->sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (p_srv->sfd <= 0)
	{
		log_print(HT_LOG_ERR,  "%s, socket err[%s]!!!\r\n", __FUNCTION__, strerror(errno));
		return -1;
	}
	
	setsockopt(p_srv->sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = p_srv->saddr;
	addr.sin_port = htons(p_srv->sport);
	
	if (bind(p_srv->sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		log_print(HT_LOG_ERR,  "%s, bind tcp socket fail,err[%s]!!!\n", __FUNCTION__, strerror(errno));
		log_print(HT_LOG_INFO, "Bind %s:%d failed\r\n", get_ip_str_(p_srv->saddr), p_srv->sport);
		closesocket(p_srv->sfd);
		p_srv->sfd = 0;
		return -1;
	}

	if (listen(p_srv->sfd, HTTP_LISTEN_WAIT_MAX_NUMS) < 0)
	{
		log_print(HT_LOG_ERR,  "%s, listen tcp socket fail,err[%s]!!!\r\n", __FUNCTION__, strerror(errno));
		closesocket(p_srv->sfd);
		p_srv->sfd = 0;
		return -1;
	}
	
#ifdef EPOLL
	uint64 e_dat = p_srv->sfd;
	e_dat |= ((uint64)1 << 63);

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.u64 = e_dat;
	epoll_ctl(p_srv->ep_fd, EPOLL_CTL_ADD, p_srv->sfd, &event);
#endif
	
	return 0;
}

HT_API BOOL http_srv_init(HTTPSRV * p_srv, const char * saddr, uint16 sport, int cln_num, BOOL https, const char * cert_file, const char * key_file)
{
	uint32 i;

	memset(p_srv, 0, sizeof(HTTPSRV));

	if (saddr && saddr[0] != '\0')
	{
		strncpy(p_srv->host, saddr, sizeof(p_srv->host)-1);
		p_srv->saddr = get_address_by_name(saddr);
	}
	else
	{
		strcpy(p_srv->host, get_local_ip());
		p_srv->saddr = 0;   // listen on all interfaces
	}

	p_srv->sport = sport;
	p_srv->https = https;
	p_srv->max_cln_nums = cln_num;

	p_srv->cln_fl = pps_ctx_fl_init(cln_num, sizeof(HTTPCLN), TRUE);
	if (p_srv->cln_fl == NULL)
	{
		return FALSE;
	}

	p_srv->cln_ul = pps_ctx_ul_init(p_srv->cln_fl, TRUE);
	if (p_srv->cln_ul == NULL)
	{
		goto FAILED;
	}

#ifdef EPOLL
	p_srv->ep_event_num = cln_num + 16;

	p_srv->ep_fd = epoll_create(p_srv->ep_event_num);
	if (p_srv->ep_fd < 0)
	{
		log_print(HT_LOG_ERR,  "%s, epoll_create failed\r\n", __FUNCTION__);
		goto FAILED;
	}

	p_srv->ep_events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * p_srv->ep_event_num);
	if (p_srv->ep_events == NULL)
	{
		log_print(HT_LOG_ERR,  "%s, malloc failed\r\n", __FUNCTION__);
		goto FAILED;
	}
#endif

#ifdef HTTPS
	if (p_srv->https)
	{
		if (cert_file)
		{
			strncpy(p_srv->cert_file, cert_file, sizeof(p_srv->cert_file)-1);
		}

		if (key_file)
		{
			strncpy(p_srv->key_file, key_file, sizeof(p_srv->key_file)-1);
		}

		/* initialize SSL */
		p_srv->ssl_ctx = http_init_ssl_ctx();   
		if (NULL == p_srv->ssl_ctx)
		{
			log_print(HT_LOG_ERR,  "%s, http_init_ssl_ctx failed\r\n", __FUNCTION__);
			goto FAILED;
		}

		/* load certs */
		http_load_certificates(p_srv->ssl_ctx, p_srv->cert_file, p_srv->key_file);
	}
#endif

	if (http_srv_net_init(p_srv) != 0)
	{
		goto FAILED;
	}

	p_srv->mutex_cb = malloc(sizeof(pthread_mutex_t));
	if (p_srv->mutex_cb) anj_mutex_create((pthread_mutex_t *)p_srv->mutex_cb, 0);

	p_srv->r_flag = 1;

#ifdef EPOLL
	p_srv->rx_num = 1;
#else
	p_srv->rx_num = p_srv->max_cln_nums / HTTP_SOCKETS + 1;
#endif

	p_srv->rx_tid = (pthread_t *) malloc(sizeof(pthread_t) * p_srv->rx_num);

	for (i = 0; i < p_srv->rx_num; i ++)
	{
		HttpThreadParam * param = (HttpThreadParam *) malloc(sizeof(HttpThreadParam));

		param->idx = i;
		param->srv = p_srv;

		anj_thread_create(&p_srv->rx_tid[i], 0, "http_rx", (void *(*)(void *))http_rx_thread, param, 1);
	}

	return TRUE;

FAILED:

	http_srv_deinit(p_srv);

	return FALSE;
}

HT_API void http_srv_deinit(HTTPSRV * p_srv)
{
	uint32 i;
	HTTPCLN * p_cln;

	p_srv->r_flag = 0;

	for (i = 0; i < p_srv->rx_num; i++)
	{
		while (p_srv->rx_tid[i] != 0)
		{
			usleep(100*1000);
		}
	}

	p_srv->rx_num = 0;

	if (p_srv->rx_tid)
	{
		free(p_srv->rx_tid);
		p_srv->rx_tid = 0;
	}

	for (i = 0; i < p_srv->max_cln_nums; i++)
	{
	    p_cln = http_get_cln_by_index(p_srv, i);
		if (p_cln && p_cln->cfd > 0)
		{
			http_free_used_cln(p_srv, p_cln);
		}
	}

	if (p_srv->cln_ul)
	{
		pps_ul_free(p_srv->cln_ul);
		p_srv->cln_ul = NULL;
	}

	if (p_srv->cln_fl)
	{
		pps_fl_free(p_srv->cln_fl);
		p_srv->cln_fl = NULL;
	}

	if (p_srv->mutex_cb)
	{
		anj_mutex_destroy((pthread_mutex_t *)p_srv->mutex_cb);
		free(p_srv->mutex_cb);
		p_srv->mutex_cb = NULL;
	}

#ifdef HTTPS
	if (p_srv->ssl_ctx)
	{
		SSL_CTX_free(p_srv->ssl_ctx);
		p_srv->ssl_ctx = NULL;
	}
#endif

	if (p_srv->sfd > 0)
	{
#ifdef EPOLL
		epoll_ctl(p_srv->ep_fd, EPOLL_CTL_DEL, p_srv->sfd, NULL);
#endif
		closesocket(p_srv->sfd);
		p_srv->sfd = 0;
	}	

#ifdef EPOLL
	if (p_srv->ep_fd)
	{
		close(p_srv->ep_fd);
		p_srv->ep_fd = 0;
	}

	if (p_srv->ep_events)
	{
		free(p_srv->ep_events);
		p_srv->ep_events = NULL;
	}
#endif
}

HT_API void http_set_msg_cb(HTTPSRV * p_srv, http_msg_cb cb, void * p_userdata)
{
	anj_mutex_lock((pthread_mutex_t *)p_srv->mutex_cb);
	p_srv->msg_cb = cb;
	p_srv->msg_user = p_userdata;
	anj_mutex_unlock((pthread_mutex_t *)p_srv->mutex_cb);
}

HT_API void http_set_data_cb(HTTPSRV * p_srv, http_data_cb cb, void * p_userdata)
{
	anj_mutex_lock((pthread_mutex_t *)p_srv->mutex_cb);
	p_srv->data_cb = cb;
	p_srv->data_user = p_userdata;
	anj_mutex_unlock((pthread_mutex_t *)p_srv->mutex_cb);
}

HT_API void http_set_conn_cb(HTTPSRV * p_srv, http_conn_cb cb, void * p_userdata)
{
	anj_mutex_lock((pthread_mutex_t *)p_srv->mutex_cb);
	p_srv->conn_cb = cb;
	p_srv->conn_user = p_userdata;
	anj_mutex_unlock((pthread_mutex_t *)p_srv->mutex_cb);
}

HT_API int http_srv_cln_tx(HTTPCLN * p_user, const char * p_data, int len)
{
	int res = 0;
	int offset = 0;
#ifdef HTTPS
	int ret = 0;
#endif

	if (NULL == p_user || NULL == p_data || 0 == len)
	{
		return -1;
	}
	
#ifdef HTTPS
	if (p_user->https)
	{
		while (offset < len)
		{
			anj_mutex_lock((pthread_mutex_t *)p_user->ssl_mutex);

			if (p_user->ssl)
			{
				res = SSL_write(p_user->ssl, p_data+offset, len-offset);
				ret = SSL_get_error(p_user->ssl, res);
			}
			else
			{
				ret = -1;
			}

			anj_mutex_unlock((pthread_mutex_t *)p_user->ssl_mutex);

			if (ret == SSL_ERROR_NONE) 
			{
				if (res > 0) 
				{
					offset += res;
				}
			} 
			else if (ret == SSL_ERROR_WANT_READ) 
			{
				usleep(10*1000);
				continue;
			} 
			else if (ret == SSL_ERROR_WANT_WRITE) 
			{
				continue;
			} 
			else 
			{
				return -1;
			}
		} 
	}
	else 
#endif
	while (offset < len)
	{
		res = send(p_user->cfd, p_data+offset, len-offset, 0);
		if (res > 0)
		{
			offset += res;
		}
		else
		{
			int sockerr = errno;
			if (sockerr == EINTR || sockerr == EAGAIN)
			{
				usleep(1000);
				continue;
			}
			
			log_print(HT_LOG_ERR,  "%s, send failed, tlen[%d,%d],err[%d][%s]!!!\r\n",
				__FUNCTION__, res, len-offset, sockerr, strerror(errno));			
			return -1;
		}
	}

	return offset;
}

/***************************************************************************************/

HT_API uint32 http_cln_index(HTTPSRV * p_srv, HTTPCLN * p_cln)
{
	return pps_get_index(p_srv->cln_fl, p_cln);
}

HT_API HTTPCLN * http_get_cln_by_index(HTTPSRV * p_srv, unsigned long index)
{
	return (HTTPCLN *)pps_get_node_by_index(p_srv->cln_fl, index);
}

HT_API HTTPCLN * http_get_idle_cln(HTTPSRV * p_srv)
{	
	HTTPCLN * p_cln = (HTTPCLN *)pps_fl_pop(p_srv->cln_fl);
	if (p_cln)
	{
		memset(p_cln, 0, sizeof(HTTPCLN));

		p_cln->use_count = 1;

		log_print(HT_LOG_DBG,  "%s, p_cln=%p, index[%u]\r\n", __FUNCTION__, p_cln, http_cln_index(p_srv, p_cln));
	}

	return p_cln;
}

// for force close client when use_count > 1
HT_API int http_set_cln_use_count(HTTPCLN * p_cln, int cnt)
{
    if (NULL == p_cln)
    {
        return -1;
    }

    p_cln->use_count = cnt;
    return 0;
}

HT_API int http_free_used_cln(HTTPSRV * p_srv, HTTPCLN * p_cln)
{
    if (NULL == p_srv || NULL == p_cln)
    {
        log_print(HT_LOG_ERR, "client free but p_srv or p_cln is null!\n");
        return -1;
    }

	p_cln->use_count--;
	if (p_cln->use_count > 0)
	{
		log_print(HT_LOG_DBG, "client free but in use! fd:%d, port:%u, alive:%d but use_count:%d!!!\n",
			 p_cln->cfd, p_cln->rport, p_cln->keep_alive, p_cln->use_count);

		return -1;
	}

	log_print(HT_LOG_DBG,  "%s, p_cln=%p, index[%u]\r\n", __FUNCTION__, p_cln, http_cln_index(p_srv, p_cln));

	if (p_cln->dyn_recv_buf)
	{
		free(p_cln->dyn_recv_buf);
		p_cln->dyn_recv_buf = NULL;
	}

	if (p_cln->userdata_mutex)
	{
		anj_mutex_destroy((pthread_mutex_t *)p_cln->userdata_mutex);
		free(p_cln->userdata_mutex);
		p_cln->userdata_mutex = NULL;
	}

#ifdef HTTPS
	if (p_cln->https)
	{
		if (p_cln->ssl)
		{
			anj_mutex_lock((pthread_mutex_t *)p_cln->ssl_mutex);
			SSL_free(p_cln->ssl);
			p_cln->ssl = NULL;
			anj_mutex_unlock((pthread_mutex_t *)p_cln->ssl_mutex);
		}

		if (p_cln->ssl_mutex)
		{
			anj_mutex_destroy((pthread_mutex_t *)p_cln->ssl_mutex);
			free(p_cln->ssl_mutex);
			p_cln->ssl_mutex = NULL;
		}
	}
#endif

	if (p_cln->cfd > 0)
	{
#ifdef EPOLL	
		epoll_ctl(p_srv->ep_fd, EPOLL_CTL_DEL, p_cln->cfd, NULL);
#endif

		closesocket(p_cln->cfd);
		p_cln->cfd = 0;
	}

	pps_ctx_ul_del(p_srv->cln_ul, p_cln);
	
	pps_fl_push_tail(p_srv->cln_fl, p_cln);
	return 0;
}




