#ifndef __URL_PARSE_H__
#define __URL_PARSE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    char user[ACCOUNT_NAME_MAX_LEN];
    char pass[ACCOUNT_PASSWORD_MAX_LEN];
    char domain[MAX_IP_NAME_LEN];           //域名
    char svr_dir[MAX_IPC_FILENAME_LEN];     //文件路径
    char svr_ip[MAX_IP_NAME_LEN];
    unsigned short port;
}URL_RESULT_T;

int parse_domain_dir(const char *url, URL_RESULT_T *result);
int parse_url(const char *raw_url, URL_RESULT_T *result);

#ifdef __cplusplus
}
#endif

#endif
