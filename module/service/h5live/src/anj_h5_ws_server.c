/*
 * Wslay - The WebSocket Library
 *
 * Copyright (c) 2011, 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * WebSocket Echo Server
 * This is suitable for Autobahn server test.
 *
 * Dependency: nettle-dev
 *
 * To compile:
 * $ gcc -Wall -O2 -g -o fork-echoserv fork-echoserv.c -L../lib/.libs -I../lib/includes -lwslay -lnettle
 *
 * To run:
 * $ export LD_LIBRARY_PATH=../lib/.libs
 * $ ./a.out 9000
 */
#include <sys/types.h>
#include <fcntl.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <pthread.h>
#else
#include "win32src/win32inc.h"
#include "msg_def.h"
#endif

#include <stdint.h>
#include <sys/select.h>
#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "base64.h"
#include "wslay.h"
#include "wslay_net.h"
#include "sha1.h"
#include "wslay_event.h"
#include "anj_h5_ws_server.h"
#include "anj_h5_stream.h"
#include "anj_h5_auth.h"
#include "anj_h5_playback.h"
#include "anj_h5_media.h"
#include "anj_mw_log.h"
#include "anj_config.h"
#include "project_option.h"

#ifdef _WIN32
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

int g_bNeedAuth = 1;
int g_h5_client_count = 0;
char g_audio_codec_type[32] = "PCMU";
int g_audio_channels = 1;
int g_audio_sample_rate = 8000;

/* Cap wslay send-queue growth under WOULDBLOCK (e.g. IP change half-open TCP). */
#define H5_WS_MAX_QUEUED_BYTES (512 * 1024)

static volatile int s_ws_server_running = 0;
static int s_listen_fd = -1;
static pthread_t s_ws_server_tid;
static int s_ws_server_started = 0;

/*
 * Create server socket, listen on *service*.  This function returns
 * file descriptor of server socket if it succeeds, or returns -1.
 */
int create_listen_socket(const char *service)
{
    struct addrinfo hints, *res, *rp;
    int sfd = -1;
    int r;
    memset(&hints, 0, sizeof(struct addrinfo));
#ifdef _WIN32
    hints.ai_family = AF_INET;
#else
    hints.ai_family = AF_UNSPEC;
#endif
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    r = getaddrinfo(0, service, &hints, &res);
    if (r != 0)
    {
        fprintf(stderr, "getaddrinfo: %s", gai_strerror(r));
        return -1;
    }
    for (rp = res; rp; rp = rp->ai_next)
    {
        int val = 1;
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
        {
            continue;
        }
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &val,
                       (socklen_t)sizeof(val)) == -1)
        {
            continue;
        }
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
        {
            break;
        }
        close(sfd);
    }
    freeaddrinfo(res);
    if (listen(sfd, 16) == -1)
    {
        perror("listen");
        close(sfd);
        return -1;
    }
    return sfd;
}

/*
 * Makes file descriptor *fd* non-blocking mode.
 * This function returns 0, or returns -1.
 */
int make_non_block(int fd)
{
#ifndef _WIN32
    int flags, r;
    while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR)
        ;
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }
    while ((r = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR)
        ;
    if (r == -1)
    {
        perror("fcntl");
        return -1;
    }
#else
    unsigned long arg = 1;
    ioctlsocket(fd, FIONBIO, &arg);
#endif
    return 0;
}

/*
 * Calculates SHA-1 hash of *src*. The size of *src* is *src_length* bytes.
 * *dst* must be at least SHA1_DIGEST_SIZE.
 */
static void h5_sha1_digest(uint8_t *dst, const uint8_t *src, size_t src_length)
{
    struct sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, src_length, src);
    sha1_digest(&ctx, SHA1_DIGEST_SIZE, dst);
}

/*
 * Base64-encode *src* and stores it in *dst*.
 * The size of *src* is *src_length*.
 * *dst* must be at least BASE64_ENCODE_RAW_LENGTH(src_length).
 */
void base64(uint8_t *dst, const uint8_t *src, size_t src_length)
{
    struct base64_encode_ctx ctx;
    base64_encode_init(&ctx);
    base64_encode_raw(dst, src_length, src);
}

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/*
 * Create Server's accept key in *dst*.
 * *client_key* is the value of |Sec-WebSocket-Key| header field in
 * client's handshake and it must be length of 24.
 * *dst* must be at least BASE64_ENCODE_RAW_LENGTH(20)+1.
 */
void create_accept_key(char *dst, const char *client_key)
{
    uint8_t sha1buf[20], key_src[60];
    memcpy(key_src, client_key, 24);
    memcpy(key_src + 24, WS_GUID, 36);
    h5_sha1_digest(sha1buf, key_src, sizeof(key_src));
    base64((uint8_t *)dst, sha1buf, 20);
    dst[BASE64_ENCODE_RAW_LENGTH(20)] = '\0';
}

/* We parse HTTP header lines of the format
 *   \r\nfield_name: value1, value2, ... \r\n
 *
 * If the caller is looking for a specific value, we return a pointer to the
 * start of that value, else we simply return the start of values list.
 */
static char *
http_header_find_field_value(char *header, char *field_name, char *value)
{
    char *header_end,
        *field_start,
        *field_end,
        *next_crlf,
        *value_start;
    int field_name_len;

    /* Pointer to the last character in the header */
    header_end = header + strlen(header) - 1;

    field_name_len = strlen(field_name);

    field_start = header;

    do
    {
        field_start = strstr(field_start + 1, field_name);

        field_end = field_start + field_name_len - 1;

        if (field_start != NULL && field_start - header >= 2 && field_start[-2] == '\r' && field_start[-1] == '\n' && header_end - field_end >= 1 && field_end[1] == ':')
        {
            break; /* Found the field */
        }
        else
        {
            continue; /* This is not the one; keep looking. */
        }
    } while (field_start != NULL);

    if (field_start == NULL)
        return NULL;

    /* Find the field terminator */
    next_crlf = strstr(field_start, "\r\n");

    /* A field is expected to end with \r\n */
    if (next_crlf == NULL)
        return NULL; /* Malformed HTTP header! */

    /* If not looking for a value, then return a pointer to the start of values string */
    if (value == NULL)
        return field_end + 2;

    value_start = strstr(field_start, value);

    /* Value not found */
    if (value_start == NULL)
        return NULL;

    /* Found the value we're looking for */
    if (value_start > next_crlf)
        return NULL; /* ... but after the CRLF terminator of the field. */

    /* The value we found should be properly delineated from the other tokens */
    if (isalnum(value_start[-1]) || isalnum(value_start[strlen(value)]))
        return NULL;

    return value_start;
}

// 返回0表示成功，-1表示失败 //
int http_send_response(int fd, const char *res_header, int res_header_length)
{
    int r = 0;
    int res_header_sent = 0;
#ifdef _WIN32
    while (res_header_sent < res_header_length)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = {3, 500 * 1000};
        r = select(fd + 1, NULL, &readfds, NULL, &timeout);
        if (r == -1)
        {
            perror("select");
            return -1;
        }
        r = send(fd, res_header + res_header_sent, res_header_length - res_header_sent, 0);
        if (r == -1)
        {
            fprintf(stderr, "send");
            return -1;
        }
        else
        {
            res_header_sent += r;
        }
    }
#else
    while (res_header_sent < res_header_length)
    {
        while ((r = send(fd, res_header + res_header_sent, res_header_length - res_header_sent, MSG_NOSIGNAL)) == -1 && errno == EINTR)
            ;
        if (r == -1)
        {
            perror("write");
            return -1;
        }
        else
        {
            res_header_sent += r;
        }
    }
#endif
    return 0;
}

/*
 * Performs HTTP handshake. *fd* is the file descriptor of the
 * connection to the client. This function returns 0 if it succeeds,
 * or returns -1.
 */
int http_handshake(int fd)
{
    /*
     * Note: The implementation of HTTP handshake in this function is
     * written for just a example of how to use of wslay library and is
     * not meant to be used in production code.  In practice, you need
     * to do more strict verification of the client's handshake.
     */
    char header[16384];
    char accept_key[29];
    char *keyhdstart;
    char *keyhdend;
    char res_header[256];
    size_t header_length = 0, res_header_length;
    ssize_t r;
    while (1)
    {
#ifdef _WIN32
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = {3, 500 * 1000};
        r = select(fd + 1, &readfds, NULL, NULL, &timeout);
        if (r == -1)
        {
            perror("select");
            return -1;
        }
        if (r > 0)
        {
            r = recv(fd, header + header_length, sizeof(header) - header_length, 0);
            if (r == -1)
            {
                fprintf(stderr, "recv");
                return -1;
            }
            else if (r == 0)
            {
                fprintf(stderr, "HTTP Handshake: Got EOF");
                return -1;
            }
            else
            {
                header_length += r;
                if (header_length >= 4 &&
                    memcmp(header + header_length - 4, "\r\n\r\n", 4) == 0)
                {
                    break;
                }
                else if (header_length == sizeof(header))
                {
                    fprintf(stderr, "HTTP Handshake: Too large HTTP headers");
                    return -1;
                }
            }
        }
#else
        while ((r = read(fd, header + header_length, sizeof(header) - header_length)) == -1 && errno == EINTR)
            ;
#endif
        if (r == -1)
        {
            perror("read");
            return -1;
        }
        else if (r == 0)
        {
            fprintf(stderr, "HTTP Handshake: Got EOF");
            return -1;
        }
        else
        {
            header_length += r;
            if (header_length >= 4 &&
                memcmp(header + header_length - 4, "\r\n\r\n", 4) == 0)
            {
                break;
            }
            else if (header_length == sizeof(header))
            {
                fprintf(stderr, "HTTP Handshake: Too large HTTP headers");
                return -1;
            }
        }
    }

    if ((http_header_find_field_value(header, "Upgrade", "websocket") == NULL && http_header_find_field_value(header, "Upgrade", "Websocket") == NULL) ||
        http_header_find_field_value(header, "Connection", "Upgrade") == NULL ||
        (keyhdstart = http_header_find_field_value(header, "Sec-WebSocket-Key", NULL)) == NULL)
    {
        fprintf(stderr, "HTTP Handshake: Missing required header fields");
        return -1;
    }
    for (; *keyhdstart == ' '; ++keyhdstart)
        ;
    keyhdend = keyhdstart;
    for (; *keyhdend != '\r' && *keyhdend != ' '; ++keyhdend)
        ;
    if (keyhdend - keyhdstart != 24)
    {
        printf("%s\n", keyhdstart);
        fprintf(stderr, "HTTP Handshake: Invalid value in Sec-WebSocket-Key");
        return -1;
    }
    create_accept_key(accept_key, keyhdstart);

    if (g_bNeedAuth)
    {
        char login_user[64];
        char login_pass[64];
        long long time_stamp;
        memset(login_user, 0, sizeof(login_user));
        memset(login_pass, 0, sizeof(login_pass));
        int param_is_ok = 0;
        char *first_line_end = strstr(header, "\n");
        if (first_line_end != NULL)
        {
            first_line_end[0] = 0;
            char *init_flag = strstr(header, "init?");
            if (init_flag != NULL)
            {
                int ret = sscanf(init_flag, "init?user=%63[^&]&pass=%63[^&]&time=%lld", login_user, login_pass, &time_stamp);
                if (ret >= 3)
                {
                    param_is_ok = 1;
                }
            }
        }
        int auth_result = param_is_ok && anj_h5_check_user_auth(login_user, login_pass, time_stamp);
        if (auth_result == 0 || param_is_ok == 0)
        {
            res_header_length = snprintf(res_header, sizeof(res_header),
                                         "HTTP/1.1 401 Unauthorized\r\n"
                                         "Content-Type: text/html; charset=utf-8\r\n"
                                         "Content-Length: 53\r\n"
                                         "\r\n"
                                         "<html><body><h1>401 Unauthorized</h1></body></html>"
                                         "\r\n");
            return http_send_response(fd, res_header, res_header_length);
        }
        else if (auth_result < -1)
        {
            res_header_length = snprintf(res_header, sizeof(res_header),
                                         "HTTP/1.1 401 Unauthorized\r\n"
                                         "Content-Type: text/html; charset=utf-8\r\n"
                                         "Content-Length: 60\r\n"
                                         "\r\n"
                                         "<html><body><h1>401 timestamp is error</h1></body></html>"
                                         "\r\n");
            return http_send_response(fd, res_header, res_header_length);
        }
    }

    snprintf(res_header, sizeof(res_header),
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n"
             "\r\n",
             accept_key);
    res_header_length = strlen(res_header);
    return http_send_response(fd, res_header, res_header_length);
}

/*
 * This struct is passed as *user_data* in callback function.  The
 * *fd* member is the file descriptor of the connection to the client.
 */
struct Session
{
    int fd;
    int quit_cmd_recv;
    int recv_thrd_running;
};

ssize_t send_callback(wslay_event_context_ptr ctx,
                      const uint8_t *data, size_t len, int flags,
                      void *user_data)
{
    // printf("<function:line>%s : %d\n",__FUNCTION__,__LINE__);
    struct Session *session = (struct Session *)user_data;
    ssize_t r;
    int sflags = 0;
    sflags |= MSG_NOSIGNAL;
#ifdef MSG_MORE
    if (flags & WSLAY_MSG_MORE)
    {
        sflags |= MSG_MORE;
    }
#endif // MSG_MORE

    // printf("send==>(%d)(fd=%d)(sflags=%d)<==\n",len,session->fd,sflags);

    while ((r = send(session->fd, data, len, sflags)) == -1 && errno == EINTR)
        ;
    //  if(r <= 0) {
    //  	printf("send_callback r=%d\n",r);
    //  }
    if (r == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
        }
        else
        {
            wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        }
    }
    return r;
}

ssize_t recv_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len,
                      int flags, void *user_data)
{
    struct Session *session = (struct Session *)user_data;
    ssize_t r;
    while ((r = recv(session->fd, buf, len, 0)) == -1 && errno == EINTR)
        ;
    //  printf("recv_callback r=%d\n",r);
    if (r == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
        }
        else
        {
            wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        }
    }
    else if (r == 0)
    {
        /* Unexpected EOF is also treated as an error */
        wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        r = -1;
    }
    return r;
}

/*
接收信令处理，目前只接收回放信令
信令格式:
<playback_cmd filename="" timepos="" playmode="" playspeed="" startpos="" />
<playback_cmd playmode="" filename="" timepos="" />


*/
int cmd_get_value(char *data, unsigned int *session_id, unsigned int *pmode, char *filename, unsigned int *timepos)
{
    if (data == NULL)
        return -1;
    char *p1;
    int iIndex = 0;
    char strmode[4] = {0}, strpos[16] = {0}, strstartpos[16] = {0}, strid[16] = {0};
    // 文件回放
    if (strstr(data, "<playback_cmd"))
    {
        p1 = strstr(data, "filename=\"");
        if (p1 != NULL)
        {
            p1 += strlen("filename=\"");
            for (iIndex = 0; (iIndex < 100) && (*p1 != '\"') && (*p1 != '\0'); iIndex++, p1++)
            {
                filename[iIndex] = *p1;
            }
            filename[iIndex] = '\0';
        }

        p1 = strstr(data, "playmode=\"");
        if (p1 != NULL)
        {
            p1 += strlen("playmode=\"");
            for (iIndex = 0; (iIndex < 4) && (*p1 != '\"') && (*p1 != '\0'); iIndex++, p1++)
            {
                strmode[iIndex] = *p1;
            }
            strmode[iIndex] = '\0';
        }
        p1 = strstr(data, "timepos=\"");
        if (p1 != NULL)
        {
            p1 += strlen("timepos=\"");
            for (iIndex = 0; (iIndex < 16) && (*p1 != '\"') && (*p1 != '\0'); iIndex++, p1++)
            {
                strpos[iIndex] = *p1;
            }
            strpos[iIndex] = '\0';
        }
        p1 = strstr(data, "startpos=\"");
        if (p1 != NULL)
        {
            p1 += strlen("startpos=\"");
            for (iIndex = 0; (iIndex < 16) && (*p1 != '\"') && (*p1 != '\0'); iIndex++, p1++)
            {
                strstartpos[iIndex] = *p1;
            }
            strstartpos[iIndex] = '\0';
        }
        p1 = strstr(data, "session_id=\"");
        if (p1 != NULL)
        {
            p1 += strlen("session_id=\"");
            for (iIndex = 0; (iIndex < 16) && (*p1 != '\"') && (*p1 != '\0'); iIndex++, p1++)
            {
                strid[iIndex] = *p1;
            }
            strid[iIndex] = '\0';
        }

        if (strlen(strmode) > 0)
        {
            *pmode = atoi(strmode);
        }
        if (strlen(strpos) > 0)
        {
            *timepos = atoi(strpos);
        }
        else if (strlen(strstartpos) > 0)
        {
            *timepos = atoi(strstartpos);
        }
        if (strlen(strid) > 0)
        {
            *session_id = atoi(strid);
        }
    }
    else
    {
        return -1;
    }

    return 0;
}

int anj_h5_playback_cmd_ctrl(unsigned int session_id, unsigned int playmode, char *filename, unsigned int timepos);

void on_msg_recv_callback(wslay_event_context_ptr ctx,
                          const struct wslay_event_on_msg_recv_arg *arg,
                          void *user_data)
{
    (void)user_data;
    if ((arg->msg_length <= 0) || (arg->msg == NULL))
    {
        //		printf("opcode=%d,rsv=%d,status_code=%d\n",arg->opcode,arg->rsv,arg->status_code);
        if (arg->opcode == 8)
        {
            if (ctx->stream_id == STREAM_ID_PLAYBACK)
                anj_h5_playback_cmd_ctrl(ctx->session_id, ACTION_STOP, NULL, 0);
            wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        }
        return;
    }
    if (arg->status_code != 0)
    {
        //		printf("opcode=%d,rsv=%d,status_code=%d\n",arg->opcode,arg->rsv,arg->status_code);
        if (ctx->stream_id == STREAM_ID_PLAYBACK)
            anj_h5_playback_cmd_ctrl(ctx->session_id, ACTION_STOP, NULL, 0);
        wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
        return;
    }
    //	printf("opcode=%d,rsv=%d,status_code=%d\n",arg->opcode,arg->rsv,arg->status_code);

    char *data = (char *)malloc(arg->msg_length + 1);
    if (data == NULL)
    {
        __ERR("malloc data error!\n");
        return;
    }
    memset(data, 0, arg->msg_length + 1);
    memcpy(data, arg->msg, arg->msg_length);
    // printf("recv=(opcode=%d,len=%d)=>%s<==\n",arg->opcode,arg->msg_length,data);

    /* 分析接收的数据 */

    if (ctx->loginOK == 0)
    {
        __ERR("need login first!\n");
        return;
    }

    // 播放主码流
    if (!strcmp(data, "PlayStream1"))
    {
        ctx->stream_id = STREAM_ID_MAIN;
        __INFO("PlayStream1, ctx->stream_id=%d\n", ctx->stream_id);
    }
    // 播放子码流
    else if (!strcmp(data, "PlayStream2"))
    {
        ctx->stream_id = (MAX_VENC_CHN > 1) ? STREAM_ID_SUB : STREAM_ID_MAIN;
        __INFO("PlayStream2, ctx->stream_id=%d\n", ctx->stream_id);
    }
    // 播放第三码流
    else if (!strcmp(data, "PlayStream3"))
    {
        ctx->stream_id = STREAM_ID_THIRD;
        __INFO("PlayStream3, ctx->stream_id=%d\n", ctx->stream_id);
    }
    // 播放球主码流
    else if (!strcmp(data, "PlayStream4"))
    {
        ctx->stream_id = STREAM_ID_MAIN_GUN;
        __INFO("PlayStream4, ctx->stream_id=%d\n", ctx->stream_id);
    }
    // 播放球子码流
    else if (!strcmp(data, "PlayStream5"))
    {
        ctx->stream_id = STREAM_ID_SUB_GUN;
        __INFO("PlayStream5, ctx->stream_id=%d\n", ctx->stream_id);
    }
    // 播放回放码流
    else if (!strcmp(data, "PlayStream10"))
    {
        ctx->stream_id = STREAM_ID_PLAYBACK;
        __INFO("PlayStream10, ctx->stream_id=%d\n", ctx->stream_id);
    }
    else if (!strcmp(data, "RealtimeAudio"))
    {
        ctx->stream_id = STREAM_ID_AUDIO_LIVE;
        __INFO("RealtimeAudio, ctx->stream_id=%d\n", ctx->stream_id);
    }
    // 播放实时音频
    else if (!strcmp(data, "PlaybackAudio"))
    {
        ctx->stream_id = STREAM_ID_AUDIO_PLAYBACK;
        __INFO("PlaybackAudio, ctx->stream_id=%d\n", ctx->stream_id);
    }
    else
    {
        char filename[100] = {0};
        unsigned int session_id = 0, playmode = 0, timepos = 0;
        cmd_get_value(data, &session_id, &playmode, filename, &timepos);
        __ERR("recv=>>(session_id=%d)playmode=%d,filename=%s,startpos=%d\n", session_id, playmode, filename, timepos);
        // 文件回放
        anj_h5_playback_cmd_ctrl(session_id, playmode, filename, timepos);
    }

    free(data);
    data = NULL;

#if 0	
  /* Echo back non-control message */
  if(!wslay_is_ctrl_frame(arg->opcode)) {
    struct wslay_event_msg msgarg = {
      arg->opcode, arg->msg, arg->msg_length
    };
    //wslay_event_queue_msg(ctx, &msgarg); //by yajie
  }
#endif
}

/*
 * Communicate with the client. This function performs HTTP handshake
 * and WebSocket data transfer until close handshake is done or an
 * error occurs. *fd* is the file descriptor of the connection to the
 * client. This function returns 0 if it succeeds, or returns 0.
 */
#if 0
#define SHM_SIZE 1024 * 1024  // by yajie start
extern int shm_sendStream_id;
extern unsigned char *sendStreamShm;
extern key_t key_sendStream;
extern int shm_framelen_id;
extern int *framelenShm;
extern key_t key_framelen;
#define SEM_NAME "pushStream" // by yajie end
#endif
extern char g_audio_codec_type[32];
extern int g_audio_channels;
extern int g_audio_sample_rate;

extern int g_h5_client_count;

unsigned char *put_byte(unsigned char *output, uint8_t nVal)
{
    output[0] = nVal;
    return output + 1;
}
unsigned char *put_be16(unsigned char *output, uint16_t nVal)
{
    output[1] = nVal & 0xff;
    output[0] = nVal >> 8;
    return output + 2;
}
unsigned char *put_be24(unsigned char *output, uint32_t nVal)
{
    output[2] = nVal & 0xff;
    output[1] = nVal >> 8;
    output[0] = nVal >> 16;
    return output + 3;
}
unsigned char *put_be32(unsigned char *output, uint32_t nVal)
{
    output[3] = nVal & 0xff;
    output[2] = nVal >> 8;
    output[1] = nVal >> 16;
    output[0] = nVal >> 24;
    return output + 4;
}
unsigned char *put_be64(unsigned char *output, uint64_t nVal)
{
    output = put_be32(output, nVal >> 32);
    output = put_be32(output, nVal);
    return output;
}

unsigned char *put_amf_string(unsigned char *c, const char *str)
{
    uint16_t len = strlen(str);
    c = put_be16(c, len);
    memcpy(c, str, len);
    return c + len;
}
unsigned char *put_amf_double(unsigned char *c, double d)
{
    *c++ = AMF_NUMBER; /* type: Number */
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&d;
        co = (unsigned char *)c;
        co[0] = ci[7];
        co[1] = ci[6];
        co[2] = ci[5];
        co[3] = ci[4];
        co[4] = ci[3];
        co[5] = ci[2];
        co[6] = ci[1];
        co[7] = ci[0];
    }
    return c + 8;
}

void get_videosize(char *resName, int tvsystem, int *width, int *height)
{
    (void)resName;
    (void)tvsystem;
    if (width)
        *width = 0;
    if (height)
        *height = 0;
}

typedef struct
{
    int fd;
    int stream_id;
} communicate_msg;
typedef struct
{
    void *ctx;
    void *event;
} cmd_thread_msg;

static void *cmd_recv_thread(void *arg)
{
    cmd_thread_msg *pcmd_msg = (cmd_thread_msg *)arg;
    wslay_event_context_ptr ctx = (wslay_event_context_ptr)(pcmd_msg->ctx);
    struct pollfd *event = (struct pollfd *)(pcmd_msg->event);
    pthread_detach(pthread_self()); // by yajie
    //	printf("start cmd_recv_thread! ctx->error=%d\n",ctx->error);
    struct Session *pSession = (struct Session *)ctx->user_data;
    pSession->recv_thrd_running = 1;

    while (!pSession->quit_cmd_recv)
    {
        int r, ret = -2;
        while ((r = poll(event, 1, -1)) == -1 && errno == EINTR)
            ; // 阻塞等待数据
        if (r == -1)
        {
            perror("poll");
        }
        if ((event->events & POLLIN) && ((ret = wslay_event_recv(ctx)) != 0))
        {
            __ERR("recv error");
            perror("recv error");
        }
        if ((ctx->stream_id == STREAM_ID_PLAYBACK) && (ctx->error == WSLAY_ERR_CALLBACK_FAILURE))
        {
            __ERR("[%s:%d] r=%d,ctx->error=%d\n", __FUNCTION__, __LINE__, r, ctx->error);
            break;
        }

        if ((ctx->stream_id == STREAM_ID_MAIN) || (ctx->stream_id == STREAM_ID_SUB) || (ctx->stream_id == STREAM_ID_THIRD) || (ctx->stream_id == STREAM_ID_MAIN_GUN) || (ctx->stream_id == STREAM_ID_SUB_GUN) || (ctx->stream_id == STREAM_ID_AUDIO_LIVE) || (ctx->stream_id == STREAM_ID_AUDIO_PLAYBACK))
        {
            break;
        }
        else
        {
            usleep(1);
        }
    }

    pSession->recv_thrd_running = 0;
    free(pcmd_msg);
    return NULL;
}

int communicate(int fd, int stream_id)
{
    __INFO("client communicate stream_id(%d), fd(%d)\n", stream_id, fd); // by yajie
    // printf("client communicate stream_id(%d), fd(%d)\n",stream_id,fd); //by yajie

    wslay_event_context_ptr ctx;
    struct wslay_event_callbacks callbacks = {
        recv_callback, send_callback, NULL, NULL, NULL, NULL, on_msg_recv_callback};
    struct Session session = {fd, 0, 0};
    int val = 1;
    struct pollfd event;
    unsigned int session_id = 0;
    cmd_thread_msg *cmd_msg = NULL;

    if (http_handshake(fd) == -1)
    {
        __ERR("http_handshake error");
        return -1;
    }
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, (socklen_t)sizeof(val)) == -1)
    {
        __ERR("setsockopt: TCP_NODELAY");
        perror("setsockopt: TCP_NODELAY");
        return -1;
    }

    memset(&event, 0, sizeof(struct pollfd));
    event.fd = fd;
    event.events |= POLLOUT;
    wslay_event_context_server_init(&ctx, &callbacks, &session);
    ctx->stream_id = 0;
    ctx->loginOK = 1;

    event.events |= POLLIN;
    ctx->error = 0;
    cmd_msg = (cmd_thread_msg *)malloc(sizeof(cmd_thread_msg));
    if (cmd_msg == NULL)
    {
        wslay_event_context_free(ctx);
        return -1;
    }
    cmd_msg->ctx = (void *)(ctx);
    cmd_msg->event = (void *)(&event);
    pthread_t tid;
    int status = pthread_create(&tid, NULL, cmd_recv_thread, (void *)cmd_msg);
    if (status != 0)
    {
        wslay_event_context_free(ctx);
        free(cmd_msg);
        perror("pthread cmd_recv_thread create error");
        return -1;
    }

    int wait_time = 0;
    while (ctx->stream_id == 0)
    {
        usleep(1000 * 100);
        wait_time++;
        if (wait_time > 50)
        {
            session.quit_cmd_recv = 1;
            while (session.recv_thrd_running)
            {
                usleep(1000 * 100);
            }
            wslay_event_context_free(ctx);
            return -1;
        }
        else
        {
            continue;
        }
    }
    __INFO("ctx->stream_id=%d\n", ctx->stream_id);
    // STREAM_ID_MAIN
    stream_id = (int)ctx->stream_id;

    // 设置 socket
    if (stream_id != STREAM_ID_PLAYBACK)
    {

        if (make_non_block(fd) == -1)
        {
            wslay_event_context_free(ctx);
            return -1;
        }

        char *p_callback = "<playback_status status=\"ok\" />";
        uint8_t opcode = 1;
        if (!wslay_is_ctrl_frame(opcode))
        {
            struct wslay_event_msg msgarg = {
                opcode, (const uint8_t *)p_callback, strlen(p_callback)};
            wslay_event_queue_msg(ctx, &msgarg);
        }
        if (wslay_event_send(ctx) != 0) // 发送
        {
            __ERR("[%s:%d]wslay_event_send session_id error!\n", __FUNCTION__, __LINE__);
            wslay_event_context_free(ctx);
            return -1;
        }
    }
    else
    {
        struct linger linger;
        memset((void *)&linger, 0, sizeof(linger));
        linger.l_onoff = 1;
        linger.l_linger = 0;
        if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&linger, sizeof(struct linger)))
        {
            __ERR("setsockopt SO_LINGER error!\n");
        }

        // 根据当前时间产生session_id发送给客户端
        struct timeval timeval;
        gettimeofday(&timeval, NULL);
        session_id = (timeval.tv_sec * 1000 + timeval.tv_usec / 1000);
        ctx->session_id = session_id;
        __ERR("session_id=%d\n", session_id);
        char cmddata[64] = {0};
        snprintf(cmddata, sizeof(cmddata), "<playback_status session_id=\"%d\" />", session_id);
        uint8_t opcode = 1;
        if (!wslay_is_ctrl_frame(opcode))
        {
            struct wslay_event_msg msgarg = {
                opcode, (const uint8_t *)cmddata, strlen(cmddata)};
            wslay_event_queue_msg(ctx, &msgarg);
        }
        if (wslay_event_send(ctx) != 0) // 发送
        {
            __ERR("[%s:%d]wslay_event_send session_id error!\n", __FUNCTION__, __LINE__);
            if (session_id != 0)
            {
                anj_h5_playback_release_session(session_id);
            }
            wslay_event_context_free(ctx);
            return -1;
        }
    }

#if 1

    if (STREAM_ID_MAIN == stream_id || STREAM_ID_SUB == stream_id)
    {
        clearStreamBuffer(stream_id);
        anj_h5_request_idr(stream_id);
        __INFO("anj_h5_request_idr(%d)\n", stream_id);
    }

    // 不断发流给客户端
    int stream_type = 0; // by yajie start
    uint32_t data_ssid = 0;
    unsigned int buf_pos = 0;
    // uint32_t pretimeoff_num = 0;
    uint32_t sequence_id = 0;
    uint32_t pre_sequence_id = 0;
    // uint32_t precmd_num = 0;
    // uint32_t *pts = &sequence_id;
    int frameLen = 0;
    int preframeLen = 0;
    // int bufSize = 0;
    int flag_send = 0;
    unsigned char *streamBuf;
    unsigned char *frame_data;
    unsigned int f_num = 0;

    StreamBuffer *sb = getStreamBuffer(stream_id);
    if (!sb)
    {
        wslay_event_context_free(ctx);
        return -1;
    }
    pthread_rwlock_t *rwlock = &sb->rwlock;
    frameDataBuffer *sBuffer = sb->buffer;

    // 音频流通过 WebSocket 二进制帧发送，音频配置通过文本帧发送。
    if (stream_id == STREAM_ID_AUDIO_LIVE || stream_id == STREAM_ID_AUDIO_PLAYBACK)
    {
        char audio_cfg[128] = {0};
        snprintf(audio_cfg, sizeof(audio_cfg),
                 "{\"audio\":1,\"codec\":\"%s\",\"channels\":%d,\"samplerate\":%d}",
                 g_audio_codec_type, g_audio_channels, g_audio_sample_rate);
        uint8_t opcode = 1;
        struct wslay_event_msg msgarg = {opcode, (const uint8_t *)audio_cfg, (size_t)strlen(audio_cfg)};
        wslay_event_queue_msg(ctx, &msgarg);
        if (wslay_event_send(ctx) != 0)
        {
            __ERR("[%s:%d] send audio config error!\n", __FUNCTION__, __LINE__);
            if (session_id != 0)
            {
                anj_h5_playback_release_session(session_id);
            }
            wslay_event_context_free(ctx);
            return -1;
        }
    }

    while (1)
    {
        if ((stream_id == STREAM_ID_PLAYBACK) && (ctx->error == WSLAY_ERR_CALLBACK_FAILURE))
        {
            __ERR("[%s:%d] send ctx->error=%d\n", __FUNCTION__, __LINE__, ctx->error);
            clearStreamBuffer(stream_id);
            usleep(100 * 1000);
            if (session_id != 0)
            {
                anj_h5_playback_release_session(session_id);
            }
            wslay_event_context_free(ctx);
            return -1;
        }

        if ((stream_id == STREAM_ID_MAIN || stream_id == STREAM_ID_SUB || stream_id == STREAM_ID_THIRD || stream_id == STREAM_ID_MAIN_GUN || stream_id == STREAM_ID_SUB_GUN || stream_id == STREAM_ID_AUDIO_LIVE || stream_id == STREAM_ID_AUDIO_PLAYBACK) && (ctx->error == WSLAY_ERR_CALLBACK_FAILURE))
        {
            if (stream_id == STREAM_ID_AUDIO_LIVE || stream_id == STREAM_ID_AUDIO_PLAYBACK)
                clearStreamBuffer(stream_id);
            if (session_id != 0)
            {
                anj_h5_playback_release_session(session_id);
            }
            wslay_event_context_free(ctx);
            return -1;
        }

        pthread_rwlock_rdlock(rwlock);
        stream_type = sBuffer[buf_pos].stream_type;
        data_ssid = sBuffer[buf_pos].session_id;
        sequence_id = sBuffer[buf_pos].sequence_id;
        frameLen = sBuffer[buf_pos].frame_len;
        frame_data = sBuffer[buf_pos].frame_data;

        if (frameLen == 0)
        {
            pthread_rwlock_unlock(rwlock);
            usleep(1);
            continue;
        }
        if ((stream_id == STREAM_ID_MAIN || stream_id == STREAM_ID_SUB) &&
            (stream_type == AudioData_Type || (!flag_send && stream_type != IFatme_Type)))
        {
            pthread_rwlock_unlock(rwlock);
            buf_pos++;
            if (buf_pos >= sBUFSIZE)
            {
                buf_pos = 0;
            }
            usleep(1);
            continue;
        }
        if (!flag_send && stream_type == IFatme_Type)
        {
            flag_send = 1;
        }
        if ((stream_id == STREAM_ID_PLAYBACK) && (data_ssid != session_id))
        {
            pthread_rwlock_unlock(rwlock);
            usleep(1);
            continue;
        }
        if ((preframeLen != 0) && (sequence_id <= pre_sequence_id))
        {
            pthread_rwlock_unlock(rwlock);
            usleep(1);
            continue;
        }

        /* Backpressure: drain or drop; do not keep queue_msg while send is blocked. */
        if (wslay_event_want_write(ctx) ||
            wslay_event_get_queued_msg_length(ctx) >= H5_WS_MAX_QUEUED_BYTES)
        {
            pthread_rwlock_unlock(rwlock);
            if (wslay_event_send(ctx) != 0)
            {
                __ERR("[%s:%d]wslay_event_send error!\n", __FUNCTION__, __LINE__);
                if (stream_id == STREAM_ID_AUDIO_LIVE || stream_id == STREAM_ID_AUDIO_PLAYBACK)
                    clearStreamBuffer(stream_id);
                if (session_id != 0)
                {
                    anj_h5_playback_release_session(session_id);
                }
                wslay_event_context_free(ctx);
                return -1;
            }
            if (wslay_event_want_write(ctx) ||
                wslay_event_get_queued_msg_length(ctx) >= H5_WS_MAX_QUEUED_BYTES)
            {
                buf_pos++;
                if (buf_pos >= sBUFSIZE)
                {
                    buf_pos = 0;
                }
            }
            usleep(1000);
            continue;
        }

        // 开始发送
        streamBuf = (unsigned char *)malloc(frameLen); // +tsLen
        if (streamBuf == NULL)
        {
            pthread_rwlock_unlock(rwlock);
            __ERR("sendBuf malloc error!\n");
            if (session_id != 0)
            {
                anj_h5_playback_release_session(session_id);
            }
            wslay_event_context_free(ctx);
            return -1;
        }
        memset(streamBuf, 0, frameLen); // +tsLen
        // memcpy(streamBuf,pts,sizeof(uint32_t));
        memcpy(streamBuf, frame_data, frameLen); // +tsLen
        pthread_rwlock_unlock(rwlock);

        buf_pos++;
        if (buf_pos >= sBUFSIZE)
        {
            buf_pos = 0;
        }
        f_num++;
        // if(stream_type==IFatme_Type){
        //	printf("send stream_type==IFatme_Type\n");
        // }
        pre_sequence_id = sequence_id;
        preframeLen = frameLen;

        uint8_t opcode = 2;
        if (stream_type == cmd_Type || stream_type == AudioConfig_Type)
            opcode = 1;
        if (!wslay_is_ctrl_frame(opcode))
        {
            struct wslay_event_msg msgarg = {
                opcode, streamBuf, frameLen}; // +tsLen
            wslay_event_queue_msg(ctx, &msgarg);
        }

        if (wslay_event_send(ctx) != 0) // 发送
        {
            __ERR("[%s:%d]wslay_event_send error!\n", __FUNCTION__, __LINE__);
            free(streamBuf);
            if (stream_id == STREAM_ID_AUDIO_LIVE || stream_id == STREAM_ID_AUDIO_PLAYBACK)
                clearStreamBuffer(stream_id);
            wslay_event_context_free(ctx);
            return -1;
        }

        free(streamBuf);
        usleep(1000);
    }

    sleep(1);
    if (stream_id == STREAM_ID_AUDIO_LIVE || stream_id == STREAM_ID_AUDIO_PLAYBACK)
        clearStreamBuffer(stream_id);
    if (session_id != 0)
    {
        anj_h5_playback_release_session(session_id);
    }
    wslay_event_context_free(ctx);
    return -1; // by yajie end
#endif
}

/*
 * Serves echo back service forever.  *sfd* is the file descriptor of
 * the server socket.  when the incoming connection from the client is
 * accepted, this function forks another process and the forked
 * process communicates with client. The parent process goes back to
 * the loop and can accept another client.
 */

static void *communicate_thread(void *arg)
{
    communicate_msg *pcom_msg = (communicate_msg *)arg;
    int fd = pcom_msg->fd;
    int stream_id = pcom_msg->stream_id;
    __INFO("communicate_thread stream_id(%d), fd(%d)\n", stream_id, fd); // by yajie

    pthread_detach(pthread_self()); // by yajie

    communicate(fd, stream_id);
    g_h5_client_count--;
    __INFO("client close, client_count=%d\n", g_h5_client_count);
    shutdown(fd, SHUT_WR);
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
    free(pcom_msg);
    return NULL;
}

static void serve(int sfd, int stream_id)
{
    while (s_ws_server_running)
    {
        int fd;
        struct timeval tv = {1, 0};
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sfd, &rfds);
        int sel = select(sfd + 1, &rfds, NULL, NULL, &tv);
        if (sel <= 0)
        {
            if (sel < 0 && errno == EINTR)
            {
                continue;
            }
            continue;
        }
        while ((fd = accept(sfd, NULL, NULL)) == -1 && errno == EINTR)
            ;
        __INFO("stream_id(%d), accept fd(%d)\n", stream_id, fd);
        if (fd == -1)
        {
            perror("accept");
        }
        else
        {
            g_h5_client_count++;
            __INFO("client connect, client_count=%d\n", g_h5_client_count);
            communicate_msg *com_msg = (communicate_msg *)malloc(sizeof(communicate_msg));
            if (com_msg == NULL)
            {
                g_h5_client_count--;
                close(fd);
                continue;
            }
            com_msg->fd = fd;
            com_msg->stream_id = stream_id;
            pthread_t tid;
            int status = pthread_create(&tid, NULL, communicate_thread, (void *)com_msg);
            if (status != 0)
            {
                perror("pthread communicate_thread create error");
                free(com_msg);
                g_h5_client_count--;
                close(fd);
                continue;
            }
        }
    }
}

static void *websocket_server_thread(void *arg)
{
    int stream_id = (int)(intptr_t)arg;
    int sfd;

    MediaStreamConfig *pMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
    if (pMediaStreamConfig == NULL)
    {
        __ERR("getMediaStreamConfig failed\n");
        return NULL;
    }

    int h5port = pMediaStreamConfig->webConfig.h5Port;
    char str_h5port[16];
    snprintf(str_h5port, sizeof(str_h5port), "%d", h5port);
    sfd = create_listen_socket(str_h5port);
    if (sfd == -1)
    {
        fprintf(stderr, "Failed to create server socket\n");
        return NULL;
    }
    s_listen_fd = sfd;
    s_ws_server_running = 1;
    __ERR("WebSocket echo server, listening on %s\n", str_h5port);
    serve(sfd, stream_id);
#ifdef _WIN32
    closesocket(sfd);
#else
    close(sfd);
#endif
    s_listen_fd = -1;
    s_ws_server_running = 0;
    return NULL;
}

int anj_h5_ws_server_start(void)
{
    if (s_ws_server_started)
    {
        return 0;
    }
    pthread_t tid;
    int stream_id = 0;
    int status = pthread_create(&tid, NULL, websocket_server_thread, (void *)(intptr_t)stream_id);
    if (status != 0)
    {
        perror("pthread websocket_server_thread create error");
        return -1;
    }
    s_ws_server_tid = tid;
    s_ws_server_started = 1;
    return 0;
}

void anj_h5_ws_server_stop(void)
{
    if (!s_ws_server_started)
    {
        return;
    }
    s_ws_server_running = 0;
    if (s_listen_fd >= 0)
    {
#ifdef _WIN32
        closesocket(s_listen_fd);
#else
        close(s_listen_fd);
#endif
        s_listen_fd = -1;
    }
    pthread_join(s_ws_server_tid, NULL);
    s_ws_server_started = 0;
}

int websocketserver_start(void)
{
    return anj_h5_ws_server_start();
}
