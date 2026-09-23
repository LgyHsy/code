#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "anj_mw_comm.h"
#include "anj_mw_net.h"

#include "anj_config.h"
#include "url_parse.h"


#define HEAD_FTP_P      "ftp://"
#define HEAD_FTPS_P     "ftps://"   
#define HEAD_FTPES_P    "ftpes://"
#define HEAD_HTTP_P     "http://"
#define HEAD_HTTPS_P    "https://"

#define PORT_FTP        (21)
#define PORT_FTPS_I     (990)    //implicit
#define PORT_FTPS_E     (21)     //explicit
#define PORT_HTTP       (80)
#define PORT_HTTPS      (443)

#define MAX_COMM_NAME_LEN   (128)
#define MAX_URL_LENGTH      (1024)
#define MAX_PORT_LEN        (6)

#define URL_DEBUG       (0)

struct pro_port
{
	char pro_s[32];
	unsigned short port;
};

struct pro_port g_pro_port[]=
{
	{HEAD_FTP_P, PORT_FTP},
	{HEAD_FTPS_P, PORT_FTPS_I},	
	{HEAD_FTPES_P, PORT_FTPS_E},	
	{HEAD_HTTP_P, PORT_HTTP},	
	{HEAD_HTTPS_P, PORT_HTTPS},
};

int dns_resoulve(char *server_ip, const char *domain)
{
    char **pptr = NULL;
    struct hostent *hptr = NULL;
    char str[MAX_IP_NAME_LEN] = {0};

    if (server_ip == NULL || domain == NULL)
    {
        __ERR("invalid parameters\n");
        return -1;
    }

    hptr = gethostbyname(domain);
    if (hptr == NULL)
    {
        __ERR("gethostbyname error for host:%s\n", domain);
        return -1;
    }

#if URL_DEBUG
    __DBG("official hostname:%s\n", hptr->h_name);

    for (pptr = hptr->h_aliases; *pptr != NULL; pptr++)
    {
        __DBG("alias:%s\n", *pptr);
    }
#endif

    switch (hptr->h_addrtype)
    {
        case AF_INET:
        case AF_INET6:
            pptr = hptr->h_addr_list;
            
#if URL_DEBUG
            for (; *pptr != NULL; pptr++)
            {
                inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));
                __DBG("address:%s\n", str);
            }
            
            inet_ntop(hptr->h_addrtype, hptr->h_addr, str, sizeof(str));
            __DBG("first address:%s\n", str);
#else
            if (*pptr != NULL)
            {
                inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));
                StrCpy(server_ip, MAX_IP_NAME_LEN - 1, str);
            }
            else
            {
                __ERR("no address found for domain:%s\n", domain);
                return -1;
            }
#endif
            break;
            
        default:
            __ERR("unknown address type\n");
            return -1;
            break;
    }

    return 0;
}

void remove_quotation_mark(char *input)
{
    int i = 0;
    char tmp_buf[MAX_URL_LENGTH];
    char *tmp_ptr = input;

    while(*tmp_ptr != '\0')
    {
        if(*tmp_ptr != '"') 
        {
            tmp_buf[i] = *tmp_ptr;
            i++;
        }
        tmp_ptr++;
    }

    tmp_buf[i] = '\0';
    strcpy(input, tmp_buf);
}

int parse_domain_dir(const char *url, URL_RESULT_T *result)
{
    char *p = NULL;
    char *token = NULL;
    char dir_str[MAX_IPC_FILENAME_LEN * 2] = {0};
    char ip_port[MAX_COMM_NAME_LEN * 2] = {0};
    char domain[MAX_IP_NAME_LEN] = {0};
    char port_s[MAX_PORT_LEN] = {0};
    char user_pass[ACCOUNT_NAME_MAX_LEN + ACCOUNT_PASSWORD_MAX_LEN] = {0};
    char user[ACCOUNT_NAME_MAX_LEN] = {0};
    char pass[ACCOUNT_PASSWORD_MAX_LEN] = {0};

    char *buf = (char *)anj_mw_malloc(MAX_URL_LENGTH);
    if (buf == NULL)
    {
        __ERR("buf malloc failed\n");
        return -1;
    }

    if (url == NULL || result == NULL)
    {
        return -1;
    }

    strncpy(buf, url, MAX_URL_LENGTH - 1);
    buf[MAX_URL_LENGTH - 1] = '\0';
    p = buf;

    token = strtok(buf, "@");
    if (token != NULL)
    {
#if URL_DEBUG
        __DBG("token=%s\n", token);
#endif
        if (strlen(token) != strlen(url))
        {
            if (strlen(user_pass) == 0)
            {
                strncpy(user_pass, token, sizeof(user_pass) - 1);
                user_pass[sizeof(user_pass) - 1] = '\0';
                p += strlen(user_pass) + 1;
            }
        }
    }

#if URL_DEBUG
    __DBG("user_pass=%s\n", user_pass);
#endif

    if (strlen(user_pass) > 0)
    {
        token = strtok(user_pass, ":");
        while (token != NULL)
        {
            if (strlen(user) == 0)
            {
                strncpy(user, token, sizeof(user) - 1);
                user[sizeof(user) - 1] = '\0';
                
                if (strlen(user) == 0 || strlen(user) > ACCOUNT_NAME_MAX_LEN)
                {
                    return -1;
                }
            }
            else
            {
                strcat(pass, token);
            }
            token = strtok(NULL, "/");
        }
    }

    token = strtok(p, "/");
    while (token != NULL)
    {
        if (strlen(ip_port) == 0)
        {
            strncpy(ip_port, token, sizeof(ip_port) - 1);
            ip_port[sizeof(ip_port) - 1] = '\0';
            
            if (strlen(ip_port) == 0 || strlen(ip_port) > MAX_IP_NAME_LEN + MAX_PORT_LEN + 1)
            {
                return -1;
            }
        }
        else
        {
            strcat(dir_str, "/");
            strcat(dir_str, token);
        }

        token = strtok(NULL, "/");
    }

#if URL_DEBUG
    __DBG("ip_port:%s, dir_str:%s\n", ip_port, dir_str);
#endif

    token = strtok(ip_port, ":");
    while (token != NULL)
    {
        if (strlen(domain) == 0)
        {
            strncpy(domain, token, sizeof(domain) - 1);
            domain[sizeof(domain) - 1] = '\0';
            
            if (strlen(domain) == 0 || strlen(domain) > MAX_IP_NAME_LEN)
            {
                __ERR("invalid domain length\n");
                return -1;
            }
        }
        else if (strlen(port_s) == 0)
        {
            strncpy(port_s, token, sizeof(port_s) - 1);
            port_s[sizeof(port_s) - 1] = '\0';
            
            if (strlen(port_s) == 0 || strlen(port_s) > MAX_PORT_LEN)
            {
                __ERR("invalid port length\n");
                return -1;
            }
        }
        token = strtok(NULL, ":");
    }

    if (strlen(domain) == 0)
    {
        __ERR("there is no domain\n");
        return -1;
    }

    if (strlen(port_s) > 0)
    {
        result->port = (unsigned short)atoi(port_s);
    }

    if (strlen(dir_str) == 0)
    {
        strcat(dir_str, "/");
    }

    strncpy(result->user, user, sizeof(result->user) - 1);
    result->user[sizeof(result->user) - 1] = '\0';

    strncpy(result->pass, pass, sizeof(result->pass) - 1);
    result->pass[sizeof(result->pass) - 1] = '\0';

    strncpy(result->domain, domain, sizeof(result->domain) - 1);
    result->domain[sizeof(result->domain) - 1] = '\0';

    strncpy(result->svr_dir, dir_str, sizeof(result->svr_dir) - 1);
    result->svr_dir[sizeof(result->svr_dir) - 1] = '\0';

#if URL_DEBUG
    __DBG("URL result user:%s, pass:%s, port:%u, domain:%s, svr_dir:%s\n", 
        result->user, result->pass, result->port, result->domain, result->svr_dir);
#endif
    
    return 0;
}


int parse_url(const char *raw_url, URL_RESULT_T *result)
{
    unsigned int i = 0;
    int iRet = 0;
    int ipv4 = 0;

    if (raw_url == NULL || result == NULL)
    {
        return -1;
    }

    char *p = NULL;
    char *out_buf = (char *)anj_mw_malloc(MAX_URL_LENGTH);
    char *body = (char *)anj_mw_malloc(MAX_URL_LENGTH);
    if (out_buf == NULL || body == NULL)
    {
        __ERR("malloc failed\n");
        iRet = -1;
        goto __exit;
    }
    
    strncpy(out_buf, raw_url, MAX_URL_LENGTH - 1);
    out_buf[sizeof(out_buf) - 1] = '\0';

    string_trim_head(out_buf);
    string_trim_tail(out_buf);

    p = out_buf;

    if (strstr(out_buf, "\"") != NULL)
    {
        remove_quotation_mark(out_buf);
    }

    for (i = 0; i < ARRAY_SIZE(g_pro_port); i++)
    {
        if (strncasecmp(g_pro_port[i].pro_s, p, strlen(g_pro_port[i].pro_s)) == 0)
        {
            p += strlen(g_pro_port[i].pro_s);
            strncpy(body, p, sizeof(body) - 1);
            body[sizeof(body) - 1] = '\0';
            result->port = g_pro_port[i].port;
            break;
        }
    }

    if (i == ARRAY_SIZE(g_pro_port))
    {
        strncpy(body, p, sizeof(body) - 1);
        body[sizeof(body) - 1] = '\0';
        result->port = 80;
    }

#if URL_DEBUG
    __DBG("nbody: %s\n", body);
#endif

    if (strstr(body, "\"") != NULL)
    {
        remove_quotation_mark(body);
    }

    iRet = parse_domain_dir(body, result);
    if (iRet == -1)
    {
        __ERR("parse_domain_dir() err\n");
        return -1;
    }

    ipv4 = check_is_ipv4(result->domain);
    if (ipv4 != 1)
    {
        dns_resoulve(result->svr_ip, result->domain);
    }
    else
    {
        strncpy(result->svr_ip, result->domain, MAX_IP_NAME_LEN - 1);
        result->svr_ip[MAX_IP_NAME_LEN - 1] = '\0';
    }

__exit:
    if (out_buf != NULL)
    {
        anj_mw_free(out_buf);
        out_buf = NULL;
    }

    if (body != NULL)
    {
        anj_mw_free(body);
        body = NULL;
    }

    return iRet;
}
