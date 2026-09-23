#ifndef __RTSP_AUTH_CHECK_H__
#define __RTSP_AUTH_CHECK_H__

/** Copy the header line value */
int get_http_hdr_value(char *p_http_hdr_msg, const char *name, char *value, unsigned int value_length);

/** HTTP Basic auth */
int http_basic_auth_check(void *msg, char *p_http_hdr_msg, unsigned int http_hdr_len);

/** HTTP Digest auth */
int http_digest_auth_check(void *msg, char *p_http_hdr_msg, unsigned int http_hdr_len);

/** Auth via username password query parameters on url. */
int http_url_query_auth_check(const char *url);

/** Verify the authentication of the request */
int rtsp_auth_verify(char *p_http_hdr_msg, unsigned int http_hdr_len, const char *url);

#endif /* __RTSP_AUTH_CHECK_H__ */
