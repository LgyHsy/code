#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "anj_mw_log.h"
#include "anj_mw_mem.h"
#include "sockutil.h"
#include "librtmp/rtmp-client.h"
#include "anj_rtmp_internal.h"

#define RTMP_RECV_BUF_SIZE (2 * 1024 * 1024)

static int rtmp_socket_settimeout(socket_t sock, int timeout_ms)
{
    struct timeval timeout;

    if (sock <= 0 || timeout_ms <= 0)
    {
        return -1;
    }

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        __ERR("setsockopt SO_RCVTIMEO failed, timeout_ms=%d, err=%s\n", timeout_ms, strerror(errno));
        return -1;
    }

    return 0;
}

static int rtmp_client_send_cb(void *param, const void *header, size_t len, const void *data, size_t bytes)
{
    socket_t *socket = (socket_t *)param;
    socket_bufvec_t vec[2];

    socket_setbufvec(vec, 0, (void *)header, len);
    socket_setbufvec(vec, 1, (void *)data, bytes);
    return socket_send_v_all_by_time(*socket, vec, bytes > 0 ? 2 : 1, 0, 5000);
}

static int rtmp_config_valid(const RtmpConfig *cfg)
{
    if (cfg == NULL)
    {
        return 0;
    }

    if (cfg->enable <= 0 || cfg->port <= 0)
    {
        return 0;
    }

    if (cfg->server[0] == '\0' || cfg->appname[0] == '\0' || cfg->streamid[0] == '\0')
    {
        return 0;
    }

    if (cfg->streamno < 0 || cfg->streamno > 1)
    {
        return 0;
    }

    return 1;
}

void anj_rtmp_session_stop(anj_rtmp_session_t *session)
{
    if (session == NULL)
    {
        return;
    }

    if (session->client != NULL)
    {
        rtmp_client_destroy(session->client);
        session->client = NULL;
    }

    if (session->socket_fd > 0)
    {
        socket_close((socket_t)session->socket_fd);
        session->socket_fd = 0;
    }

    session->started = 0;
    session->send_sps_pps = 0;
    session->start_ts = 0;
}

int anj_rtmp_session_start(const RtmpConfig *cfg, anj_rtmp_session_t *session)
{
    char tcurl[512];
    char *packet = NULL;
    struct rtmp_client_handler_t handler;
    socket_t socket_fd;
    int recv_len;
    int ret;

    if (session == NULL || !rtmp_config_valid(cfg))
    {
        return -1;
    }

    anj_rtmp_session_stop(session);

    if (cfg->port == ANJ_RTMP_PORT_DEF)
    {
        snprintf(tcurl, sizeof(tcurl), "rtmp://%s/%s", cfg->server, cfg->appname);
    }
    else
    {
        snprintf(tcurl, sizeof(tcurl), "rtmp://%s:%u/%s", cfg->server, cfg->port, cfg->appname);
    }

    __INFO("rtmp publish url=%s/%s streamno=%d\n", tcurl, cfg->streamid, cfg->streamno);

    memset(&handler, 0, sizeof(handler));
    handler.send = rtmp_client_send_cb;

    socket_init();
    socket_fd = socket_connect_host(cfg->server, (u_short)cfg->port, 2000);
    if (socket_fd <= 0)
    {
        __ERR("rtmp socket_connect_host failed, socket=%d\n", socket_fd);
        return -1;
    }

    socket_setnonblock(socket_fd, 0);
    rtmp_socket_settimeout(socket_fd, 5000);

    session->client = rtmp_client_create(cfg->appname, cfg->streamid, tcurl, &session->socket_fd, &handler);
    if (session->client == NULL)
    {
        __ERR("rtmp_client_create failed\n");
        socket_close(socket_fd);
        session->socket_fd = 0;
        return -1;
    }

    session->socket_fd = (int)socket_fd;
    ret = rtmp_client_start(session->client, 0);
    if (ret != 0)
    {
        __ERR("rtmp_client_start failed, ret=%d\n", ret);
        anj_rtmp_session_stop(session);
        return -1;
    }

    packet = (char *)anj_mw_malloc(RTMP_RECV_BUF_SIZE);
    if (packet == NULL)
    {
        __ERR("rtmp recv buf malloc failed, size=%d\n", RTMP_RECV_BUF_SIZE);
        anj_rtmp_session_stop(session);
        return -1;
    }

    while (4 != rtmp_client_getstate(session->client) &&
           (recv_len = socket_recv(socket_fd, packet, RTMP_RECV_BUF_SIZE, 0)) > 0)
    {
        ret = rtmp_client_input(session->client, packet, (size_t)recv_len);
        if (ret != 0)
        {
            __ERR("rtmp_client_input failed, ret=%d\n", ret);
            anj_rtmp_session_stop(session);
            anj_mw_free(packet);
            return -1;
        }
    }

    if (recv_len < 0)
    {
        __ERR("rtmp socket_recv timeout or error, ret=%d\n", recv_len);
        anj_rtmp_session_stop(session);
        anj_mw_free(packet);
        return -1;
    }

    session->stream_no = cfg->streamno;
    session->rtmp_type = (cfg->type == 1) ? 1 : 0;
    session->started = 1;
    session->send_sps_pps = 0;
    session->start_ts = 0;

    __INFO("rtmp session started OK\n");
    anj_mw_free(packet);
    return 0;
}
