#include <string.h>
#include <strings.h>

#include "anj_base64.h"
#include "anj_config.h"
#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "anj_config_system.h"
#include "rtsp_auth_check.h"

static const char k_base64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
int get_http_hdr_value(char *p_http_hdr_msg, const char *name, char *value, unsigned int value_length);

int get_http_hdr_value(char *p_http_hdr_msg, const char *name, char *value, unsigned int value_length)
{
    char *name_start_addr;
    char *name_end_addr;

    _NULL_POINTER_CHECK_(p_http_hdr_msg, -1);
    _NULL_POINTER_CHECK_(name, -1);
    _NULL_POINTER_CHECK_(value, -1);
    if (value_length == 0)
    {
        __ERR("value_length is invalid!(%lu)", (unsigned long)value_length);
        return -1;
    }

    name_start_addr = strstr(p_http_hdr_msg, name);
    if (name_start_addr == NULL)
    {
        return -1;
    }

    name_start_addr += strlen(name);
    name_end_addr = strstr(name_start_addr, "\r\n");
    if (name_end_addr == NULL || (unsigned int)(name_end_addr - name_start_addr) >= value_length)
    {
        return -1;
    }

    memcpy(value, name_start_addr, (size_t)(name_end_addr - name_start_addr));
    value[name_end_addr - name_start_addr] = 0;
    return 0;
}

/* 0: 解析成功; -1: 无 Basic 凭证; -2: Basic 格式错误 */
static int parse_basic_auth(char *p_http_hdr_msg, char *base64_user_info, int base64_len)
{
    char auth_buf[1024] = {0};
    char *p_end;
    char *p_base64_start;
    char *p_base64_end;

    if (get_http_hdr_value(p_http_hdr_msg, "Authorization: ", auth_buf, sizeof(auth_buf)) != 0)
    {
        __ERR("get_http_hdr_value failed! \n");
        return -1;
    }

    p_end = auth_buf + strlen(auth_buf);
    p_base64_start = strstr(auth_buf, "Basic");
    _NULL_POINTER_CHECK_(p_base64_start, -1);
    p_base64_start += strlen("Basic");

    while (p_base64_start < p_end && strchr(k_base64_alphabet, (unsigned char)p_base64_start[0]) == NULL)
    {
        p_base64_start++;
    }

    p_base64_end = p_base64_start;
    while (p_base64_end < p_end && strchr(k_base64_alphabet, (unsigned char)p_base64_end[0]) != NULL)
    {
        p_base64_end++;
    }

    if ((p_base64_end - p_base64_start) <= 0 || (p_base64_end - p_base64_start) >= base64_len)
    {
        return -1;
    }

    memcpy(base64_user_info, p_base64_start, (size_t)(p_base64_end - p_base64_start));
    base64_user_info[p_base64_end - p_base64_start] = 0;
    return 0;
}

static int get_auth_kv(const char *auth, const char *key, char *value, unsigned int value_len)
{
    const char *start;
    const char *end;
    size_t key_len = strlen(key);

    if (auth == NULL || key == NULL || value == NULL || value_len == 0)
    {
        return -1;
    }

    start = strstr(auth, key);
    if (start == NULL)
    {
        return -1;
    }
    start += key_len;
    while (*start == ' ')
    {
        start++;
    }
    if (*start == '"')
    {
        start++;
        end = strchr(start, '"');
    }
    else
    {
        end = start;
        while (*end != '\0' && *end != ',' && *end != '\r' && *end != '\n')
        {
            end++;
        }
    }
    if (end == NULL || (unsigned int)(end - start) >= value_len)
    {
        return -1;
    }

    memcpy(value, start, (size_t)(end - start));
    value[end - start] = 0;
    return 0;
}

static int join_with_colon(char *out, unsigned int out_len, const char **parts, unsigned int part_count)
{
    unsigned int i;
    unsigned int pos = 0;

    if (out == NULL || out_len == 0 || parts == NULL || part_count == 0)
    {
        return -1;
    }

    out[0] = '\0';
    for (i = 0; i < part_count; ++i)
    {
        unsigned int need;
        unsigned int part_len;

        if (parts[i] == NULL)
        {
            return -1;
        }

        part_len = (unsigned int)strlen(parts[i]);
        need = part_len + ((i + 1U < part_count) ? 1U : 0U);
        if (pos + need >= out_len)
        {
            return -1;
        }

        memcpy(out + pos, parts[i], part_len);
        pos += part_len;
        if (i + 1U < part_count)
        {
            out[pos++] = ':';
        }
    }

    out[pos] = '\0';
    return 0;
}

static int get_query_value(const char *url, const char *key, char *value, unsigned int value_len)
{
    const char *start;
    const char *end;
    size_t key_len;

    if (url == NULL || key == NULL || value == NULL || value_len == 0)
    {
        return -1;
    }

    key_len = strlen(key);
    start = strstr(url, key);
    if (start == NULL)
    {
        return -1;
    }

    while (start > url && start[-1] != '?' && start[-1] != '&')
    {
        start = strstr(start + 1, key);
        if (start == NULL)
        {
            return -1;
        }
    }

    start += key_len;
    end = strchr(start, '&');
    if (end == NULL)
    {
        end = start + strlen(start);
    }

    if ((unsigned int)(end - start) >= value_len)
    {
        return -1;
    }

    memcpy(value, start, (size_t)(end - start));
    value[end - start] = '\0';
    return 0;
}

static const char *find_user_password(const UserConfig *user_cfg, const char *username)
{
    int i;

    if (user_cfg == NULL || username == NULL)
    {
        return NULL;
    }

    for (i = 0; i < user_cfg->count; i++)
    {
        if (strcmp(username, user_cfg->accounts[i].userName) == 0)
        {
            return user_cfg->accounts[i].password;
        }
    }

    return NULL;
}

int http_basic_auth_check(void *msg, char *p_http_hdr_msg, unsigned int http_hdr_len)
{
    char auth_base64_buf[128] = {0};
    SystemConfig *sys_cfg;
    int i;

    (void)msg;
    (void)http_hdr_len;

    if (parse_basic_auth(p_http_hdr_msg, auth_base64_buf, sizeof(auth_base64_buf)) != 0)
    {
        return -1;
    }

    sys_cfg = (SystemConfig *)getSystemConfig();
    if (sys_cfg == NULL)
    {
        __ERR("getSystemConfig failed!");
        return -1;
    }

    for (i = 0; i < sys_cfg->userCfg.count; i++)
    {
        char buf[128] = {0};
        char buf_encode[256] = {0};

        snprintf(buf, sizeof(buf), "%s:%s", sys_cfg->userCfg.accounts[i].userName, sys_cfg->userCfg.accounts[i].password);
        anj_base64_encode((unsigned char *)buf, (int)strlen(buf), buf_encode, sizeof(buf_encode));
        if (buf_encode != NULL && strcmp(buf_encode, auth_base64_buf) == 0)
        {
            return 0;
        }
    }

    return -1;
}

// For simplicity, http_digest_auth_check only supports qop="auth" and MD5 algorithm,
// and does not support nonce expiration and other features in RFC 2617
int http_url_query_auth_check(const char *url)
{
    char username[256] = {0};
    char password[256] = {0};
    char md5_pass[33] = {0};
    SystemConfig *sys_cfg;
    const char *real_passwd;

    _NULL_POINTER_CHECK_(url, -1);
    if (get_query_value(url, "username=", username, sizeof(username)) != 0)
    {
        __ERR("get_query_value failed! \n");
        return -1;
    }
    if (get_query_value(url, "password=", password, sizeof(password)) != 0)
    {
        __ERR("get_query_value failed! \n");
        return -1;
    }

    sys_cfg = (SystemConfig *)getSystemConfig();
    if (sys_cfg == NULL)
    {
        __ERR("getSystemConfig failed! \n");
        return -1;
    }
    real_passwd = find_user_password(&sys_cfg->userCfg, username);
    _NULL_POINTER_CHECK_(real_passwd, -1);

    if (strcmp(password, real_passwd) == 0)
    {
        return 0;
    }

    our_md5_encode(md5_pass, (const unsigned char *)real_passwd, (int)strlen(real_passwd));
    if (strcasecmp(password, md5_pass) == 0)
    {
        return 0;
    }

    return -1;
}

int http_digest_auth_check(void *msg, char *p_http_hdr_msg, unsigned int http_hdr_len)
{
    char auth_buf[1024] = {0};
    char username[256] = {0};
    char realm[256] = {0};
    char nonce[256] = {0};
    char uri[256] = {0};
    char response[256] = {0};
    char qop[256] = {0};
    char nonce_count[256] = {0};
    char client_count[256] = {0};
    char md5_args[256] = {0};
    char HA1[33] = {0};
    char HA2[33] = {0};
    char response_res[33] = {0};
    const char *parts[6];
    SystemConfig *sys_cfg;
    const char *password = NULL;

    (void)msg;
    (void)http_hdr_len;

    if (get_http_hdr_value(p_http_hdr_msg, "Authorization: ", auth_buf, sizeof(auth_buf)) != 0)
    {
        __ERR("get_http_hdr_value failed! \n");
        return -1;
    }
    if (strstr(auth_buf, "Digest") == NULL)
    {
        __ERR("not digest \n");
        return -1;
    }

    if (get_auth_kv(auth_buf, "username=", username, sizeof(username)) != 0)
    {
        __ERR("username failed! \n");
        return -1;
    }
    if (get_auth_kv(auth_buf, "realm=", realm, sizeof(realm)) != 0)
    {
        __ERR("realm failed! \n");
        return -1;
    }
    if (get_auth_kv(auth_buf, "nonce=", nonce, sizeof(nonce)) != 0)
    {
        __ERR("nonce failed! \n");
        return -1;
    }
    if (get_auth_kv(auth_buf, "uri=", uri, sizeof(uri)) != 0)
    {
        __ERR("uri failed! \n");
        return -1;
    }
    if (get_auth_kv(auth_buf, "response=", response, sizeof(response)) != 0)
    {
        __ERR("response failed! \n");
        return -1;
    }

    get_auth_kv(auth_buf, "qop=", qop, sizeof(qop));
    get_auth_kv(auth_buf, "nc=", nonce_count, sizeof(nonce_count));
    get_auth_kv(auth_buf, "cnonce=", client_count, sizeof(client_count));

    if (username[0] == '\0' && uri[0] != '\0')
    {
        get_query_value(uri, "username=", username, sizeof(username));
    }

    sys_cfg = (SystemConfig *)getSystemConfig();
    if (sys_cfg == NULL)
    {
        __ERR("getSystemConfig failed!");
        return -1;
    }
    password = find_user_password(&sys_cfg->userCfg, username);
    _NULL_POINTER_CHECK_(password, -1);

    parts[0] = username;
    parts[1] = realm;
    parts[2] = password;
    if (join_with_colon(md5_args, sizeof(md5_args), parts, 3) != 0)
    {
        __ERR("join_with_colon failed! \n");
        return -1;
    }
    out_md5_encode_data((const unsigned char *)md5_args, (unsigned int)strlen(md5_args), HA1);

    parts[0] = "DESCRIBE";
    parts[1] = uri;
    if (join_with_colon(md5_args, sizeof(md5_args), parts, 2) != 0)
    {
        __ERR("join_with_colon failed! \n");
        return -1;
    }
    out_md5_encode_data((const unsigned char *)md5_args, (unsigned int)strlen(md5_args), HA2);

    if (qop[0] != '\0')
    {
        parts[0] = HA1;
        parts[1] = nonce;
        parts[2] = nonce_count;
        parts[3] = client_count;
        parts[4] = qop;
        parts[5] = HA2;
        if (join_with_colon(md5_args, sizeof(md5_args), parts, 6) != 0)
        {
            __ERR("join_with_colon failed! \n");
            return -1;
        }
    }
    else
    {
        parts[0] = HA1;
        parts[1] = nonce;
        parts[2] = HA2;
        if (join_with_colon(md5_args, sizeof(md5_args), parts, 3) != 0)
        {
            __ERR("join_with_colon failed! \n");
            return -1;
        }
    }
    out_md5_encode_data((const unsigned char *)md5_args, (unsigned int)strlen(md5_args), response_res);

    return strcasecmp(response_res, response) == 0 ? 0 : -1;
}

int rtsp_auth_verify(char *p_http_hdr_msg, unsigned int http_hdr_len, const char *url)
{
    char auth_buf[1024] = {0};

    if (p_http_hdr_msg == NULL)
    {
        return -1;
    }

    if (get_http_hdr_value(p_http_hdr_msg, "Authorization: ", auth_buf, sizeof(auth_buf)) == 0)
    {
        if (strstr(auth_buf, "Basic") != NULL)
        {
            return http_basic_auth_check(NULL, p_http_hdr_msg, http_hdr_len);
        }
        if (strstr(auth_buf, "Digest") != NULL)
        {
            return http_digest_auth_check(NULL, p_http_hdr_msg, http_hdr_len);
        }
        return -1;
    }

    if (url != NULL && strstr(url, "username=") != NULL && strstr(url, "password=") != NULL)
    {
        return http_url_query_auth_check(url);
    }

    return -1;
}
