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
#include "http_parse.h"
#include "http_cln.h"
#include "rfc_md5.h"
#include "sha256.h"
#include "base64.h"

/***************************************************************************************/

#ifdef HTTPS
#if __WINDOWS_OS__
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")
#endif
#endif

/***************************************************************************************/

#define MAX_CTT_LEN     (2*1024*1024)


/***************************************************************************************/

static inline int http_isspace(int c)
{
    return c == ' ' || c == '\f' || c == '\n' || 
           c == '\r' || c == '\t' || c == '\v';
}

void http_choose_qop(char * qop, int size)
{
    char * ptr = strstr(qop, "auth");
    char * end = ptr + strlen("auth");

    if (ptr && (!*end || http_isspace(*end) || *end == ',') &&
        (ptr == qop || http_isspace(ptr[-1]) || ptr[-1] == ',')) 
    {
        strncpy(qop, "auth", size);
    } 
    else 
    {
        qop[0] = 0;
    }
}

BOOL http_get_digest_params(char * p, int len, HD_AUTH_INFO * p_auth)
{
    char word_buf[128];
    
	if (GetNameValuePair(p, len, "algorithm", word_buf, sizeof(word_buf)))
	{
		snprintf(p_auth->auth_algorithm, sizeof(p_auth->auth_algorithm), "%.*s",
		         (int)(sizeof(p_auth->auth_algorithm) - 1), word_buf);
	}	
	else
	{
		p_auth->auth_algorithm[0] = '\0';
	}
	
	if (GetNameValuePair(p, len, "realm", word_buf, sizeof(word_buf)))
	{
	    snprintf(p_auth->auth_realm, sizeof(p_auth->auth_realm), "%.*s",
	             (int)(sizeof(p_auth->auth_realm) - 1), word_buf);
	}
	else
	{
	    return FALSE;
	}

	if (GetNameValuePair(p, len, "nonce", word_buf, sizeof(word_buf)))
	{
	    snprintf(p_auth->auth_nonce, sizeof(p_auth->auth_nonce), "%.*s",
	             (int)(sizeof(p_auth->auth_nonce) - 1), word_buf);
	}
	else
	{
	    return FALSE;
	}

	if (GetNameValuePair(p, len, "qop", word_buf, sizeof(word_buf)))
	{	
		snprintf(p_auth->auth_qop, sizeof(p_auth->auth_qop), "%.*s",
		         (int)(sizeof(p_auth->auth_qop) - 1), word_buf);
	}	
	else
	{
		p_auth->auth_qop[0] = '\0';
	}

	http_choose_qop(p_auth->auth_qop, sizeof(p_auth->auth_qop));
	
	if (GetNameValuePair(p, len, "opaque", word_buf, sizeof(word_buf)))
	{
	    p_auth->auth_opaque_flag = 1;
		snprintf(p_auth->auth_opaque, sizeof(p_auth->auth_opaque), "%.*s",
		         (int)(sizeof(p_auth->auth_opaque) - 1), word_buf);
	}	
	else
	{
	    p_auth->auth_opaque_flag = 0;
		p_auth->auth_opaque[0] = '\0';
	}
	
	return TRUE;
}

BOOL http_get_digest_info(HTTPMSG * rx_msg, HD_AUTH_INFO * p_auth)
{
    int len;
	int	next_offset;
	char word_buf[128];
	HDRV * chap_id = NULL;
	char * p;

	p_auth->auth_response[0] = '\0';

RETRY:

    if (chap_id)
    {
        chap_id = http_find_headline_next(rx_msg, "WWW-Authenticate", chap_id);
    }
    else
    {
        chap_id = http_find_headline(rx_msg, "WWW-Authenticate");
    }
    
	if (chap_id == NULL)
	{
		return FALSE;
	}

	GetLineWord(chap_id->value_string, 0, (int)strlen(chap_id->value_string),
		word_buf, sizeof(word_buf), &next_offset, WORD_TYPE_STRING);
	if (strcasecmp(word_buf, "digest") != 0)
	{
        goto RETRY;
	}

	p = chap_id->value_string + next_offset;
	len = (int)strlen(chap_id->value_string) - next_offset;

    if (!http_get_digest_params(p, len, p_auth))
    {
        goto RETRY;
    }

    if (p_auth->auth_algorithm[0] != '\0' && 
        strncasecmp(p_auth->auth_algorithm, "MD5", 3) &&  
        strncasecmp(p_auth->auth_algorithm, "SHA-256", 7))
    {
        goto RETRY;
    }
	
	return TRUE;
}

BOOL http_calc_auth_md5_digest(HD_AUTH_INFO * p_auth, const char * method)
{
    uint8 HA1[16];
	uint8 HA2[16];
	uint8 HA3[16];
	char HA1Hex[33];
	char HA2Hex[33];
	char HA3Hex[33];
	md5_context ctx;

	md5_starts(&ctx);
	md5_update(&ctx, (uint8 *)p_auth->auth_name, strlen(p_auth->auth_name));
	md5_update(&ctx, (uint8 *)&(":"), 1);
	md5_update(&ctx, (uint8 *)p_auth->auth_realm, strlen(p_auth->auth_realm));
	md5_update(&ctx, (uint8 *)&(":"), 1);
	md5_update(&ctx, (uint8 *)p_auth->auth_pwd, strlen(p_auth->auth_pwd));
	md5_finish(&ctx, HA1);

    bin_to_hex_str(HA1, 16, HA1Hex, 33);

    if (!strcasecmp(p_auth->auth_algorithm, "MD5-sess"))
    {
        md5_starts(&ctx);
        md5_update(&ctx, (uint8 *)HA1Hex, 32);
        md5_update(&ctx, (uint8 *)&(":"), 1);
        md5_update(&ctx, (uint8 *)p_auth->auth_nonce, strlen(p_auth->auth_nonce));
        md5_update(&ctx, (uint8 *)&(":"), 1);
        md5_update(&ctx, (uint8 *)p_auth->auth_cnonce, strlen(p_auth->auth_cnonce));
        md5_finish(&ctx, HA1);

        bin_to_hex_str(HA1, 16, HA1Hex, 33);
    }
    
	md5_starts(&ctx);
	md5_update(&ctx, (uint8 *)method, strlen(method));
	md5_update(&ctx, (uint8 *)&(":"), 1);
	md5_update(&ctx, (uint8 *)p_auth->auth_uri, strlen(p_auth->auth_uri));
	md5_finish(&ctx, HA2);

	bin_to_hex_str(HA2, 16, HA2Hex, 33);

	md5_starts(&ctx);
	md5_update(&ctx, (uint8 *)HA1Hex, 32);
	md5_update(&ctx, (uint8 *)&(":"), 1);
	md5_update(&ctx, (uint8 *)p_auth->auth_nonce, strlen(p_auth->auth_nonce));
	md5_update(&ctx, (uint8 *)&(":"), 1);

	if (p_auth->auth_qop[0] != '\0') 
	{
		md5_update(&ctx, (uint8 *)p_auth->auth_ncstr, strlen(p_auth->auth_ncstr));
		md5_update(&ctx, (uint8 *)&(":"), 1);
		md5_update(&ctx, (uint8 *)p_auth->auth_cnonce, strlen(p_auth->auth_cnonce));
		md5_update(&ctx, (uint8 *)&(":"), 1);
		md5_update(&ctx, (uint8 *)p_auth->auth_qop, strlen(p_auth->auth_qop));
		md5_update(&ctx, (uint8 *)&(":"), 1);
	};
	
	md5_update(&ctx, (uint8 *)HA2Hex, 32);
	md5_finish(&ctx, HA3);

    bin_to_hex_str(HA3, 16, HA3Hex, 33);

	strcpy(p_auth->auth_response, HA3Hex);
	
	return TRUE;
}

BOOL http_calc_auth_sha256_digest(HD_AUTH_INFO * p_auth, const char * method)
{
    uint8 HA1[32];
	uint8 HA2[32];
	uint8 HA3[32];
	char HA1Hex[65];
	char HA2Hex[65];
	char HA3Hex[65];
	sha256_context ctx;

	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8 *)p_auth->auth_name, strlen(p_auth->auth_name));
	sha256_update(&ctx, (uint8 *)&(":"), 1);
	sha256_update(&ctx, (uint8 *)p_auth->auth_realm, strlen(p_auth->auth_realm));
	sha256_update(&ctx, (uint8 *)&(":"), 1);
	sha256_update(&ctx, (uint8 *)p_auth->auth_pwd, strlen(p_auth->auth_pwd));
	sha256_finish(&ctx, HA1);

    bin_to_hex_str(HA1, 32, HA1Hex, 65);

    if (!strcasecmp(p_auth->auth_algorithm, "SHA-256-sess"))
    {
        sha256_starts(&ctx);
        sha256_update(&ctx, (uint8 *)HA1Hex, 64);
        sha256_update(&ctx, (uint8 *)&(":"), 1);
        sha256_update(&ctx, (uint8 *)p_auth->auth_nonce, strlen(p_auth->auth_nonce));
        sha256_update(&ctx, (uint8 *)&(":"), 1);
        sha256_update(&ctx, (uint8 *)p_auth->auth_cnonce, strlen(p_auth->auth_cnonce));
        sha256_finish(&ctx, HA1);

        bin_to_hex_str(HA1, 32, HA1Hex, 65);
    }
    
	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8 *)method, strlen(method));
	sha256_update(&ctx, (uint8 *)&(":"), 1);
	sha256_update(&ctx, (uint8 *)p_auth->auth_uri, strlen(p_auth->auth_uri));
	sha256_finish(&ctx, HA2);

	bin_to_hex_str(HA2, 32, HA2Hex, 65);

	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8 *)HA1Hex, 64);
	sha256_update(&ctx, (uint8 *)&(":"), 1);
	sha256_update(&ctx, (uint8 *)p_auth->auth_nonce, strlen(p_auth->auth_nonce));
	sha256_update(&ctx, (uint8 *)&(":"), 1);

	if (p_auth->auth_qop[0] != '\0') 
	{
		sha256_update(&ctx, (uint8 *)p_auth->auth_ncstr, strlen(p_auth->auth_ncstr));
		sha256_update(&ctx, (uint8 *)&(":"), 1);
		sha256_update(&ctx, (uint8 *)p_auth->auth_cnonce, strlen(p_auth->auth_cnonce));
		sha256_update(&ctx, (uint8 *)&(":"), 1);
		sha256_update(&ctx, (uint8 *)p_auth->auth_qop, strlen(p_auth->auth_qop));
		sha256_update(&ctx, (uint8 *)&(":"), 1);
	};
	
	sha256_update(&ctx, (uint8 *)HA2Hex, 64);
	sha256_finish(&ctx, HA3);

    bin_to_hex_str(HA3, 32, HA3Hex, 65);

	strcpy(p_auth->auth_response, HA3Hex);
	
	return TRUE;
}

BOOL http_calc_auth_digest(HD_AUTH_INFO * p_auth, const char * method)
{
    BOOL ret = FALSE;

    p_auth->auth_nc++;
    sprintf(p_auth->auth_ncstr, "%08X", p_auth->auth_nc);
    sprintf(p_auth->auth_cnonce, "%08X%08X", rand(), rand());
	    
    if (p_auth->auth_algorithm[0] == '\0' || 
        strncasecmp(p_auth->auth_algorithm, "MD5", 3) == 0)
    {
        ret = http_calc_auth_md5_digest(p_auth, method);
    }
    else if (strncasecmp(p_auth->auth_algorithm, "SHA-256", 7) == 0)
    {
        ret = http_calc_auth_sha256_digest(p_auth, method);
    }

    return ret;
}

int http_build_auth_msg(HTTPREQ * p_http, const char * method, char * buff, int buflen)
{
	int offset = 0;
	HD_AUTH_INFO * p_auth = &p_http->auth_info;
	
	if (p_http->auth_mode == 1)			// digest auth
    {
        http_calc_auth_digest(p_auth, method);

        offset += snprintf(buff+offset, buflen-offset, 
            "Authorization: Digest username=\"%s\", realm=\"%s\", "
            "nonce=\"%s\", uri=\"%s\", response=\"%s\"",
            p_auth->auth_name, p_auth->auth_realm, 
            p_auth->auth_nonce, p_auth->auth_uri, p_auth->auth_response);

        if (p_auth->auth_opaque_flag)
        {
            offset += snprintf(buff+offset, buflen-offset, 
                ", opaque=\"%s\"", p_auth->auth_opaque);
        }
        
        if (p_auth->auth_qop[0] != '\0')
        {
            offset += snprintf(buff+offset, buflen-offset, 
                ", qop=\"%s\", cnonce=\"%s\", nc=%s",
                p_auth->auth_qop, p_auth->auth_cnonce, p_auth->auth_ncstr);
        }

        if (p_auth->auth_algorithm[0] != '\0')
        {
            offset += snprintf(buff+offset, buflen-offset, 
                ", algorithm=%s", p_auth->auth_algorithm);
        }

        offset += snprintf(buff+offset, buflen-offset, "\r\n");
	}
	else if (p_http->auth_mode == 0)	// basic auth
	{
		char auth[128] = {'\0'};
		char basic[256] = {'\0'};
		
		snprintf(auth, sizeof(auth), "%s:%s", p_auth->auth_name, p_auth->auth_pwd);
		
		base64_encode_bm((uint8 *)auth, (int)strlen(auth), basic, sizeof(basic));
		
		offset += snprintf(buff+offset, buflen-offset, "Authorization: Basic %s\r\n", basic);
	}

	return offset;
}

int http_cln_auth_set(HTTPREQ * p_http, const char * user, const char * pass)
{
    if (user)
    {
        strncpy(p_http->auth_info.auth_name, user, sizeof(p_http->auth_info.auth_name)-1);
    }

    if (pass)
    {
        strncpy(p_http->auth_info.auth_pwd, pass, sizeof(p_http->auth_info.auth_pwd)-1);
    }
		
    if (http_get_digest_info(p_http->rx_msg, &p_http->auth_info))
	{
		p_http->auth_mode = 1;  // digest
		strcpy(p_http->auth_info.auth_uri, p_http->url);
	}
	else
	{
		p_http->auth_mode = 0;  // basic
	}
	
    p_http->need_auth = TRUE;

    return p_http->auth_mode;
}

BOOL http_cln_ssl_conn(HTTPREQ * p_http, int timeout)
{
#ifdef HTTPS
    SSL_CTX * ctx = NULL;
	const SSL_METHOD * method = NULL;

	SSLeay_add_ssl_algorithms();  
	SSL_load_error_strings();
	
	method = SSLv23_client_method();  
	ctx = SSL_CTX_new(method);
	if (NULL == ctx)
	{
		log_print(HT_LOG_ERR,  "%s, SSL_CTX_new failed!\r\n", __FUNCTION__);
		return FALSE;
	}
    
#if __WINDOWS_OS__
    int tv = timeout;
#else
    struct timeval tv = {timeout / 1000, (timeout % 1000) * 1000};
#endif

    setsockopt(p_http->cfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    
	p_http->ssl = SSL_new(ctx); 
	if (NULL == p_http->ssl)
	{
	    SSL_CTX_free(ctx);
		log_print(HT_LOG_ERR,  "%s, SSL_new failed!\r\n", __FUNCTION__);
		return FALSE;
	}
	
	SSL_set_fd(p_http->ssl, p_http->cfd); 

	if (SSL_connect(p_http->ssl) == -1)
	{
	    SSL_CTX_free(ctx);
		log_print(HT_LOG_ERR,  "%s, SSL_connect failed!\r\n", __FUNCTION__);
		return FALSE;
	}

	SSL_CTX_free(ctx);

    p_http->ssl_mutex = malloc(sizeof(pthread_mutex_t));
    if (p_http->ssl_mutex) anj_mutex_create((pthread_mutex_t *)p_http->ssl_mutex, 0);
    
	return TRUE;
#else
    return FALSE;
#endif
}

BOOL http_cln_rx(HTTPREQ * p_http)
{
	int rlen = 0;
	HTTPMSG * rx_msg;

	if (p_http->rbuf == NULL)
	{
		p_http->rbuf = p_http->rcv_buf;
		p_http->mlen = sizeof(p_http->rcv_buf)-4;
		p_http->rcv_dlen = 0;
		p_http->ctt_len = 0;
		p_http->hdr_len = 0;
	}

#ifdef HTTPS
	if (p_http->https)
	{
	    anj_mutex_lock((pthread_mutex_t *)p_http->ssl_mutex);
	    if (p_http->ssl)
	    {
		    rlen = SSL_read(p_http->ssl, p_http->rbuf+p_http->rcv_dlen, p_http->mlen-p_http->rcv_dlen);
		}
		anj_mutex_unlock((pthread_mutex_t *)p_http->ssl_mutex);
	}
	else 
#endif	
    rlen = recv(p_http->cfd, p_http->rbuf+p_http->rcv_dlen, p_http->mlen-p_http->rcv_dlen, 0);

	if (rlen < 0)
	{
		log_print(HT_LOG_INFO,  "%s, recv return = %d, dlen[%d], mlen[%d]\r\n", 
		    __FUNCTION__, rlen, p_http->rcv_dlen, p_http->mlen);	
		return FALSE;
	}

	p_http->rcv_dlen += rlen;
	p_http->rbuf[p_http->rcv_dlen] = '\0';    

    if (0 == rlen)
	{
	    if (p_http->rcv_dlen < p_http->ctt_len + p_http->hdr_len && p_http->ctt_len == MAX_CTT_LEN)
	    {
            // without Content-Length filed, when recv finish, fix the ctt length
            p_http->ctt_len = p_http->rcv_dlen - p_http->hdr_len;
        }
        else
        {
            log_print(HT_LOG_INFO,  "%s, recv return = %d, dlen[%d], mlen[%d]\r\n", 
                __FUNCTION__, rlen, p_http->rcv_dlen, p_http->mlen);
    		return FALSE;
        }
	}

	if (p_http->rcv_dlen < 16)
	{
		return TRUE;
	}
	
	if (http_is_http_msg(p_http->rbuf) == FALSE)
	{
		return FALSE;
	}
	
	rx_msg = NULL;

	if (p_http->hdr_len == 0)
	{
		int parse_len;
		int http_pkt_len;

		http_pkt_len = http_pkt_find_end(p_http->rbuf);
		if (http_pkt_len == 0) 
		{
			return TRUE;
		}
		p_http->hdr_len = http_pkt_len;

        rx_msg = http_get_msg_buf(http_pkt_len+1);
		if (rx_msg == NULL)
		{
			log_print(HT_LOG_ERR,  "%s, get msg buf failed\r\n", __FUNCTION__);
			return FALSE;
		}

		memcpy(rx_msg->msg_buf, p_http->rbuf, http_pkt_len);
		rx_msg->msg_buf[http_pkt_len] = '\0';
		
		log_print(HT_LOG_DBG,  "RX from %s << %s\r\n", p_http->host, rx_msg->msg_buf);
		
		parse_len = http_msg_parse_part1(rx_msg->msg_buf, http_pkt_len, rx_msg);
		if (parse_len != http_pkt_len)
		{
			log_print(HT_LOG_ERR,  "%s, http_msg_parse_part1=%d, http_pkt_len=%d!!!\r\n", 
			    __FUNCTION__, parse_len, http_pkt_len);

			http_free_msg(rx_msg);
			return FALSE;
		}
		
		p_http->ctt_len = rx_msg->ctt_len;
	}

    if (p_http->ctt_len == 0 && p_http->rcv_dlen > p_http->hdr_len)
    {
        // without Content-Length field
        p_http->ctt_len = MAX_CTT_LEN;
    }
    
	if ((p_http->ctt_len + p_http->hdr_len) > p_http->mlen)
	{
		if (p_http->dyn_recv_buf)
		{
			free(p_http->dyn_recv_buf);
		}

		p_http->dyn_recv_buf = (char *)malloc(p_http->ctt_len + p_http->hdr_len + 1);
		if (NULL == p_http->dyn_recv_buf)
		{
		    http_free_msg(rx_msg);
		    return FALSE;
		}
		
		memcpy(p_http->dyn_recv_buf, p_http->rcv_buf, p_http->rcv_dlen);
		p_http->rbuf = p_http->dyn_recv_buf;
		p_http->mlen = p_http->ctt_len + p_http->hdr_len;

	    http_free_msg(rx_msg);

        // Need more data
		return TRUE;
	}

	if (p_http->rcv_dlen >= (p_http->ctt_len + p_http->hdr_len))
	{
		if (rx_msg == NULL)
		{
			int nlen;
			int parse_len;

			nlen = p_http->ctt_len + p_http->hdr_len;
			
            rx_msg = http_get_msg_buf(nlen+1);
			if (rx_msg == NULL)
			{
			    log_print(HT_LOG_ERR,  "%s, get msg buf failed\r\n", __FUNCTION__);
				return FALSE;
			}

			memcpy(rx_msg->msg_buf, p_http->rbuf, p_http->hdr_len);
			rx_msg->msg_buf[p_http->hdr_len] = '\0';
			
			parse_len = http_msg_parse_part1(rx_msg->msg_buf, p_http->hdr_len, rx_msg);
			if (parse_len != p_http->hdr_len)
			{
				log_print(HT_LOG_ERR,  "%s, http_msg_parse_part1=%d, hdr_len=%d!!!\r\n", 
				    __FUNCTION__, parse_len, p_http->hdr_len);

				http_free_msg(rx_msg);
				return FALSE;
			}				
		}

		if (p_http->ctt_len > 0)
		{
			int parse_len;
			
			memcpy(rx_msg->msg_buf+p_http->hdr_len, p_http->rbuf+p_http->hdr_len, p_http->ctt_len);
			rx_msg->msg_buf[p_http->hdr_len + p_http->ctt_len] = '\0';

			if (ctt_is_string(rx_msg->ctt_type))
			{
				log_print(HT_LOG_DBG,  "%s\r\n\r\n", rx_msg->msg_buf+p_http->hdr_len);
			}
			
			parse_len = http_msg_parse_part2(rx_msg->msg_buf+p_http->hdr_len, p_http->ctt_len, rx_msg);
			if (parse_len != p_http->ctt_len)
			{
				log_print(HT_LOG_ERR,  "%s, http_msg_parse_part2=%d, ctt_len=%d!!!\r\n", 
				    __FUNCTION__, parse_len, p_http->ctt_len);

				http_free_msg(rx_msg);
				return FALSE;
			}
		}

		p_http->rx_msg = rx_msg;
	}

	if (p_http->rx_msg != rx_msg) 
	{
        http_free_msg(rx_msg);
    }
	
	return TRUE;
}

int http_cln_tx(HTTPREQ * p_http, const char * p_data, int len)
{
	int slen = 0;

	if (p_http->cfd <= 0)
	{
		return -1;
	}

#ifdef HTTPS
	if (p_http->https)
	{
		anj_mutex_lock((pthread_mutex_t *)p_http->ssl_mutex);
		if (p_http->ssl)
		{
			slen = SSL_write(p_http->ssl, p_data, len);
		}
		anj_mutex_unlock((pthread_mutex_t *)p_http->ssl_mutex);
	}
	else 
#endif	
	slen = send(p_http->cfd, p_data, len, 0);
	
	return slen;
}

BOOL http_cln_rx_timeout(HTTPREQ * p_http, int timeout)
{
	int count = 0;
	int sret;
	BOOL ret = FALSE;
	fd_set fdr;
	struct timeval tv;
	
	while (1)
	{
#ifdef HTTPS
		if (p_http->https && SSL_pending(p_http->ssl) > 0)
		{
			// There is data to read 
		}
		else
#endif
		{
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			
			FD_ZERO(&fdr);
			FD_SET(p_http->cfd, &fdr);

			sret = select((int)(p_http->cfd+1), &fdr, NULL, NULL, &tv);
			if (sret == 0)
			{
				count++;

				if (count >= timeout / 1000)
				{
					log_print(HT_LOG_WARN,  "%s, timeout!!!\r\n", __FUNCTION__);
					break;
				}

				continue;
			}
			else if (sret < 0)
			{
				log_print(HT_LOG_ERR,  "%s, select err[%s], sret[%d]!!!\r\n", __FUNCTION__, strerror(errno), sret);
				break;
			}
			else if (!FD_ISSET(p_http->cfd, &fdr))
			{
				continue;
			}
		}
		
		if (http_cln_rx(p_http) == FALSE)
		{
			log_print(HT_LOG_ERR,  "%s, http_cln_rx failed\r\n", __FUNCTION__);
			break;
		}	
		else if (p_http->rx_msg != NULL)
		{
			ret = TRUE;
			break;
		}
	}

	return ret;
}

void http_cln_free_req(HTTPREQ * p_http)
{
	if (p_http->cfd > 0)
	{
		closesocket(p_http->cfd);
		p_http->cfd = 0;
	}

#ifdef HTTPS
	if (p_http->https)
	{
		if (p_http->ssl)
		{
			anj_mutex_lock((pthread_mutex_t *)p_http->ssl_mutex);
			SSL_free(p_http->ssl);
			p_http->ssl = NULL;
			anj_mutex_unlock((pthread_mutex_t *)p_http->ssl_mutex);
		}

		if (p_http->ssl_mutex)
		{
			anj_mutex_destroy((pthread_mutex_t *)p_http->ssl_mutex);
			free(p_http->ssl_mutex);
			p_http->ssl_mutex = NULL;
		}
	}
#endif

	if (p_http->dyn_recv_buf)
	{
		free(p_http->dyn_recv_buf);
		p_http->dyn_recv_buf = NULL;
	}	
	
	if (p_http->rx_msg)
	{
		http_free_msg(p_http->rx_msg);
		p_http->rx_msg = NULL;
	}

	p_http->rcv_dlen = 0;
	p_http->hdr_len = 0;
	p_http->ctt_len = 0;
	p_http->rbuf = NULL;
	p_http->mlen = 0;
}

BOOL http_onvif_req(HTTPREQ * p_http, const char * p_xml, int len)
{
	int slen;
	int offset = 0;
	char bufs[2048];
	int buflen = sizeof(bufs);
	
	offset += snprintf(bufs+offset, buflen-offset, 
				"POST %s HTTP/1.1\r\n"
				"Host: %s:%u\r\n"
				"User-Agent: happytimesoft/1.0\r\n"
				"Content-Type: application/soap+xml; charset=utf-8; action=\"%s\"\r\n"
				"Content-Length: %d\r\n"
				"Connection: close\r\n"
				"\r\n", 
				p_http->url,
				p_http->host, p_http->port,
				p_http->action,
				len);

	log_print(HT_LOG_DBG,  "TX >> %s\r\n", bufs);

	slen = http_cln_tx(p_http, bufs, offset);

	if (p_xml && len > 0)
	{
		log_print(HT_LOG_DBG,  "%s\r\n", p_xml);

		slen += http_cln_tx(p_http, p_xml, len);
	}
	
	return (slen == offset + len) ? TRUE : FALSE;
}

BOOL http_onvif_trans(HTTPREQ * p_http, int timeout, const char * bufs, int len)
{
	BOOL ret = FALSE;
	
	p_http->cfd = tcp_connect_timeout(get_address_by_name(p_http->host), p_http->port, timeout);
	if (p_http->cfd <= 0)
	{
		log_print(HT_LOG_ERR,  "%s, tcp_connect_timeout\r\n", __FUNCTION__);
		goto FAILED;
	}
	
	if (p_http->https)
	{
#ifdef HTTPS	
		if (!http_cln_ssl_conn(p_http, timeout))
		{
			goto FAILED;
		}
#else
		log_print(HT_LOG_ERR,  "%s, the server require ssl connection, unsupport!\r\n", __FUNCTION__);
		goto FAILED;
#endif
	}
	
	ret = http_onvif_req(p_http, bufs, len);

FAILED:

	http_cln_free_req(p_http);

	return ret;
}



