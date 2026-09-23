#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "anj_base64.h"
#include "anj_mw_comm.h"
#include "anj_sysmng.h"
#include "anjrec_version_match.h"
#include "cJSON.h"

#define VM_ZCKEY "Goolink2014"
#define VM_HOST "global.aiot99.com"
#define VM_PORT 80
#define VM_APP_COM "public"
#define VM_SOL_COM "AIOT"
#define VM_RELEASE_TIME "20000101"
#define VM_TIMEOUT_SEC 15
#define VM_RECV_BUF_SIZE (64 * 1024)
struct rc4_state
{
    unsigned char s[256];
    int i;
    int j;
};

static void rc4_init(struct rc4_state *state, const unsigned char *key, int keylen)
{
    int i;
    int j = 0;
    unsigned char tmp;

    for (i = 0; i < 256; i++)
    {
        state->s[i] = (unsigned char)i;
    }

    for (i = 0; i < 256; i++)
    {
        j = (j + state->s[i] + key[i % keylen]) & 0xFF;
        tmp = state->s[i];
        state->s[i] = state->s[j];
        state->s[j] = tmp;
    }
    state->i = 0;
    state->j = 0;
}

static void rc4_crypt(struct rc4_state *state, const unsigned char *in, unsigned char *out, int len)
{
    int n;
    unsigned char tmp;

    for (n = 0; n < len; n++)
    {
        state->i = (state->i + 1) & 0xFF;
        state->j = (state->j + state->s[state->i]) & 0xFF;
        tmp = state->s[state->i];
        state->s[state->i] = state->s[state->j];
        state->s[state->j] = tmp;
        out[n] = in[n] ^ state->s[(state->s[state->i] + state->s[state->j]) & 0xFF];
    }
}

static int encrypt_field(const char *plain, char *out_b64, int out_size)
{
    struct rc4_state state;
    unsigned char rc4out[256];
    int n;

    if (!plain || !out_b64 || out_size <= 0)
    {
        return -1;
    }

    memset(rc4out, 0, sizeof(rc4out));
    n = (int)strlen(plain);
    if (n > (int)sizeof(rc4out))
    {
        n = (int)sizeof(rc4out);
    }

    memset(&state, 0, sizeof(state));
    rc4_init(&state, (const unsigned char *)VM_ZCKEY, (int)strlen(VM_ZCKEY));
    rc4_crypt(&state, (const unsigned char *)plain, rc4out, n);

    out_b64[0] = '\0';
    if (!anj_base64_encode(rc4out, (uint32_t)n, out_b64, (uint32_t)out_size))
    {
        return -1;
    }
    return 0;
}

static int decrypt_field(const char *enc_b64, char *out_plain, int out_size)
{
    struct rc4_state state;
    unsigned char raw[512];
    unsigned char plain[512];
    int n;

    if (!enc_b64 || !out_plain || out_size <= 0)
    {
        return -1;
    }

    out_plain[0] = '\0';
    if (enc_b64[0] == '\0')
    {
        return 0;
    }

    n = anj_base64_decode(enc_b64, (uint32_t)strlen(enc_b64), raw, (uint32_t)sizeof(raw));
    if (n < 0)
    {
        return -1;
    }
    if (n > (int)sizeof(plain))
    {
        n = (int)sizeof(plain);
    }

    memset(&state, 0, sizeof(state));
    rc4_init(&state, (const unsigned char *)VM_ZCKEY, (int)strlen(VM_ZCKEY));
    rc4_crypt(&state, raw, plain, n);

    if (n >= out_size)
    {
        n = out_size - 1;
    }
    memcpy(out_plain, plain, (size_t)n);
    out_plain[n] = '\0';
    return 0;
}

static int build_json_body(char *sendstring, int sendstring_size, const char *dev_gid)
{
    DevInfo *pstDevInfo = getDevInfo();
    char appout[64] = {0};
    char solout[64] = {0};
    char dateout[64] = {0};
    char hardwareout[64] = {0};
    char encgid[128] = {0};
    const char *hdversion;

    if (pstDevInfo == NULL || pstDevInfo->subDevType[0] == '\0' ||
        !dev_gid || dev_gid[0] == '\0')
    {
        return -1;
    }
    hdversion = pstDevInfo->subDevType;

    if (encrypt_field(VM_APP_COM, appout, sizeof(appout)) != 0 ||
        encrypt_field(VM_SOL_COM, solout, sizeof(solout)) != 0 ||
        encrypt_field(VM_RELEASE_TIME, dateout, sizeof(dateout)) != 0 ||
        encrypt_field(hdversion, hardwareout, sizeof(hardwareout)) != 0 ||
        encrypt_field(dev_gid, encgid, sizeof(encgid)) != 0)
    {
        return -1;
    }

    __INFO("version_match plain: AppCom=%s SolCom=%s ReleaseTime=%s HDVersion=%s DevGID=%s\n",
           VM_APP_COM, VM_SOL_COM, VM_RELEASE_TIME, hdversion, dev_gid);

    snprintf(sendstring, (size_t)sendstring_size,
             "{\"AppCom\":\"%s\",\"SolCom\":\"%s\",\"ReleaseTime\":\"%s\","
             "\"HDVersion\":\"%s\",\"DevGID\":\"%s\"}",
             appout, solout, dateout, hardwareout, encgid);
    return 0;
}

static int build_http_packet(char *output, int output_size, const char *dev_gid)
{
    char sendstring[1024];

    memset(sendstring, 0, sizeof(sendstring));
    if (build_json_body(sendstring, sizeof(sendstring), dev_gid) != 0)
    {
        return -1;
    }

    return snprintf(output, (size_t)output_size,
                    "POST /g_version_match.php HTTP/1.1\r\n"
                    "User-Agent: aj/ipc(1.0)\r\n"
                    "Host: %s\r\n"
                    "Connection: close\r\n"
                    "Content-Type: application/x-www-form-urlencoded\r\n"
                    "Content-Length: %d\r\n\r\n"
                    "%s",
                    VM_HOST, (int)strlen(sendstring), sendstring);
}

static int parse_status_line(const char *resp)
{
    int code = 0;
    if (sscanf(resp, "HTTP/%*s %d", &code) == 1)
    {
        return code;
    }
    return 0;
}

static const char *find_body(const char *resp)
{
    const char *p = strstr(resp, "\r\n\r\n");
    if (p)
    {
        return p + 4;
    }
    p = strstr(resp, "\n\n");
    if (p)
    {
        return p + 2;
    }
    return NULL;
}

static int header_has_token(const char *headers, const char *name, const char *token)
{
    const char *p = headers;
    size_t namelen = strlen(name);
    char line[256];

    while (*p && !(p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n') &&
           !(p[0] == '\n' && p[1] == '\n'))
    {
        const char *eol = strstr(p, "\r\n");
        size_t len;
        size_t i;
        int match = 1;

        if (!eol)
        {
            eol = strchr(p, '\n');
        }
        if (!eol)
        {
            break;
        }
        len = (size_t)(eol - p);
        if (len >= sizeof(line))
        {
            len = sizeof(line) - 1;
        }
        memcpy(line, p, len);
        line[len] = '\0';

        if (len >= namelen)
        {
            for (i = 0; i < namelen; i++)
            {
                char a = line[i];
                char b = name[i];
                if (a >= 'A' && a <= 'Z')
                {
                    a = (char)(a - 'A' + 'a');
                }
                if (b >= 'A' && b <= 'Z')
                {
                    b = (char)(b - 'A' + 'a');
                }
                if (a != b)
                {
                    match = 0;
                    break;
                }
            }
            if (match && line[namelen] == ':')
            {
                if (strstr(line + namelen + 1, token))
                {
                    return 1;
                }
            }
        }
        p = eol + ((*eol == '\r') ? 2 : 1);
    }
    return 0;
}

static int header_content_length(const char *headers)
{
    const char *p = strstr(headers, "Content-Length:");
    if (!p)
    {
        p = strstr(headers, "content-length:");
    }
    if (!p)
    {
        return -1;
    }
    p = strchr(p, ':');
    if (!p)
    {
        return -1;
    }
    return atoi(p + 1);
}

static int http_response_complete(const char *resp, int total)
{
    const char *body;
    int body_len;
    int content_len;

    if (total <= 0)
    {
        return 0;
    }
    body = find_body(resp);
    if (!body)
    {
        return 0;
    }
    body_len = total - (int)(body - resp);

    if (header_has_token(resp, "Transfer-Encoding", "chunked"))
    {
        if (strstr(body, "\r\n0\r\n\r\n"))
        {
            return 1;
        }
        if (strstr(body, "\n0\n\n"))
        {
            return 1;
        }
        if (body_len >= 5 && strstr(body + body_len - 5, "0\r\n\r\n"))
        {
            return 1;
        }
        return 0;
    }

    content_len = header_content_length(resp);
    if (content_len >= 0)
    {
        return body_len >= content_len;
    }
    return 0;
}

static void decode_chunked_inplace(char *body)
{
    char *src = body;
    char *dst = body;

    for (;;)
    {
        unsigned long chunk_size;
        char *eol;
        char *endp = NULL;

        eol = strstr(src, "\r\n");
        if (!eol)
        {
            eol = strchr(src, '\n');
        }
        if (!eol)
        {
            break;
        }
        chunk_size = strtoul(src, &endp, 16);
        if (endp == src)
        {
            break;
        }
        src = eol + ((*eol == '\r') ? 2 : 1);
        if (chunk_size == 0)
        {
            break;
        }
        memmove(dst, src, (size_t)chunk_size);
        dst += chunk_size;
        src += chunk_size;
        if (src[0] == '\r' && src[1] == '\n')
        {
            src += 2;
        }
        else if (src[0] == '\n')
        {
            src += 1;
        }
    }
    *dst = '\0';
}

static int http_post(const char *packet, char *resp, int resp_size)
{
    int sock = -1;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *rp;
    char portstr[16];
    int ret = -1;
    int total = 0;
    int n;
    struct timeval tv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(portstr, sizeof(portstr), "%d", VM_PORT);
    if (getaddrinfo(VM_HOST, portstr, &hints, &res) != 0)
    {
        __ERR("version_match: getaddrinfo failed for %s\n", VM_HOST);
        return -1;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next)
    {
        sock = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0)
        {
            continue;
        }
        if (connect(sock, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0)
        {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);

    if (sock < 0)
    {
        __ERR("version_match: connect to %s:%d failed\n", VM_HOST, VM_PORT);
        return -1;
    }

    tv.tv_sec = VM_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (send(sock, packet, strlen(packet), 0) < 0)
    {
        __ERR("version_match: send failed\n");
        close(sock);
        return -1;
    }

    memset(resp, 0, (size_t)resp_size);
    while (total < resp_size - 1)
    {
        n = (int)recv(sock, resp + total, (size_t)(resp_size - 1 - total), 0);
        if (n <= 0)
        {
            break;
        }
        total += n;
        resp[total] = '\0';
        if (http_response_complete(resp, total))
        {
            break;
        }
    }
    resp[total] = '\0';
    ret = total;
    close(sock);
    return ret;
}

static int decrypt_json_field(cJSON *root, const char *key, char *out, int out_size)
{
    cJSON *item;

    if (!root || !key || !out || out_size <= 0)
    {
        return -1;
    }
    out[0] = '\0';
    item = cJSON_GetObjectItem(root, key);
    if (item == NULL || item->type != cJSON_String || item->valuestring == NULL)
    {
        return -1;
    }
    return decrypt_field(item->valuestring, out, out_size);
}

static int downurl_valid(const char *url)
{
    if (!url || url[0] == '\0')
    {
        return 0;
    }
    if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0)
    {
        return 1;
    }
    return 0;
}

int anjrec_version_match_query(anjrec_fw_meta_t *out)
{
    char packet[2048];
    char sn_str[64] = {0};
    char *resp = NULL;
    const char *body;
    int status;
    int nbytes;
    cJSON *root = NULL;
    int iRet = -1;

    if (out == NULL)
    {
        return -1;
    }
    memset(out, 0, sizeof(*out));

    if (anj_sysmng_load_sn(sn_str, sizeof(sn_str)) != 0 || sn_str[0] == '\0')
    {
        __ERR("version_match: DevGID (SN) empty\n");
        return -1;
    }

    memset(packet, 0, sizeof(packet));
    if (build_http_packet(packet, sizeof(packet), sn_str) < 0)
    {
        __ERR("version_match: build packet failed\n");
        return -1;
    }

    resp = (char *)anj_mw_malloc(VM_RECV_BUF_SIZE);
    if (resp == NULL)
    {
        __ERR("version_match: malloc failed\n");
        return -1;
    }

    __DBG("version_match: packet=%s\n", packet);
    nbytes = http_post(packet, resp, VM_RECV_BUF_SIZE);
    if (nbytes < 0)
    {
        goto endFunc;
    }

    status = parse_status_line(resp);
    body = find_body(resp);
    if (body == NULL)
    {
        __ERR("version_match: no HTTP body, status=%d\n", status);
        goto endFunc;
    }

    if (header_has_token(resp, "Transfer-Encoding", "chunked"))
    {
        decode_chunked_inplace((char *)body);
    }

    if (status < 200 || status >= 300)
    {
        __ERR("version_match: HTTP status=%d\n", status);
        goto endFunc;
    }

    root = cJSON_Parse(body);
    if (root == NULL)
    {
        __ERR("version_match: JSON parse failed\n");
        goto endFunc;
    }

    if (decrypt_json_field(root, "re", out->re, sizeof(out->re)) != 0)
    {
        __ERR("version_match: decrypt re failed\n");
        goto endFunc;
    }

    /* Optional fields: ignore decrypt errors, leave empty. */
    decrypt_json_field(root, "downurl", out->downurl, sizeof(out->downurl));
    decrypt_json_field(root, "version", out->version, sizeof(out->version));
    decrypt_json_field(root, "md5", out->md5, sizeof(out->md5));
    decrypt_json_field(root, "file_size", out->file_size, sizeof(out->file_size));
    decrypt_json_field(root, "remark", out->remark, sizeof(out->remark));

    __INFO("version_match: re=%s version=%s md5=%s file_size=%s remark=%s\n",
           out->re, out->version, out->md5, out->file_size, out->remark);
    __DBG("version_match: downurl=%s\n", out->downurl);

    if (strcmp(out->re, "1") != 0 || !downurl_valid(out->downurl))
    {
        iRet = 1;
        goto endFunc;
    }

    iRet = 0;

endFunc:
    if (root)
    {
        cJSON_Delete(root);
    }
    if (resp)
    {
        anj_mw_free(resp);
    }
    return iRet;
}
