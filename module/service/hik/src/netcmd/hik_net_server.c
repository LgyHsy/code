#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "anj_config.h"
#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_sysmng.h"
#include "project_option.h"

#include "d3des.h"
#include "hik_net_alarm.h"
#include "hik_net_cmd.h"
#include "hik_net_ctrl.h"
#include "hik_net_server.h"
#include "hik_net_types.h"

#define HIK_NET_MAX_CLIENT 8
#define HIK_NET_RECV_BUF 2048

typedef struct
{
    int fd;
    struct sockaddr_in addr;
} hik_net_client_s;

static anj_thread_s s_listen_thread;
static volatile int s_server_run = 0;
static int s_listen_fd = -1;
static unsigned short s_listen_port = 0;
static hik_net_client_s s_clients[HIK_NET_MAX_CLIENT];

static const unsigned char s_private_key[16] = {0x6a, 0x68, 0xa3, 0x61, 0xbf, 0x6e, 0xb5, 0x67,
                                               0xcd, 0x7a, 0xfe, 0x68, 0xca, 0x6f, 0xde, 0x75};

static int hik_auth_user(const char *user, const char *passwd)
{
    SystemConfig *pSys = NULL;
    int i = 0;

    pSys = (SystemConfig *)getSystemConfig();
    if (pSys == NULL || user == NULL || user[0] == '\0')
    {
        return -1;
    }

    for (i = 0; i < MAX_ACCOUNT_COUNT; i++)
    {
        if (strcmp(pSys->userCfg.accounts[i].userName, user) == 0)
        {
            if (strcmp(pSys->userCfg.accounts[i].password, passwd ? passwd : "") == 0)
            {
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

static void hik_des_decrypt_16(UINT8 *data16)
{
    UINT8 des_in[16];

    memcpy(des_in, data16, 16);
    des2key((UINT8 *)s_private_key, DE1);
    D2des(des_in, data16);
}

static void hik_fill_login_common(NET_LOGIN_RET *login_ret, UINT32 client_ver)
{
    char sn[SERIALNO_LEN] = {0};
    UINT32 sdk_ver = CURRENT_NETSDK_VERSION;

    if (client_ver != 0 && client_ver < sdk_ver)
    {
        sdk_ver = client_ver;
    }

    anj_sysmng_load_sn(sn, sizeof(sn));
    login_ret->length = htonl(sizeof(NET_LOGIN_RET));
    login_ret->devSdkVer = htonl(sdk_ver);
    login_ret->userID = htonl(HIK_FIXED_USER_ID);
    memcpy(login_ret->serialno, sn, SERIALNO_LEN);
    login_ret->devType = HIK_IPCAMERA_TYPE_BYTE;
    login_ret->channelNums = (UINT8)ANJ_CAMERA_MAX_NUMS;
    if (login_ret->channelNums == 0)
    {
        login_ret->channelNums = 1;
    }
    login_ret->firstChanNo = 1;
    login_ret->alarmInNums = 1;
    login_ret->alarmOutNums = 1;
    login_ret->hdiskNums = 0;
    login_ret->res2[0] = 0xF0;
}

static int hik_handle_login(int fd, const char *recvbuf, int recvlen, int is_relogin)
{
    NET_LOGIN_REQ login_req;
    NET_LOGIN_RET login_ret;
    MediaStreamConfig *pStream = (MediaStreamConfig *)getMediaStreamConfig();
    int auth_required = (pStream != NULL) ? pStream->hikConfig.auth : 0;
    int copy_len = 0;

    memset(&login_req, 0, sizeof(login_req));
    memset(&login_ret, 0, sizeof(login_ret));

    copy_len = recvlen;
    if (copy_len > (int)sizeof(login_req))
    {
        copy_len = (int)sizeof(login_req);
    }
    if (copy_len < 24)
    {
        return -1;
    }
    memcpy(&login_req, recvbuf, (size_t)copy_len);

    login_req.version = ntohl(login_req.version);

    if (login_req.ifVer != NEW_NETSDK_INTERFACE)
    {
        login_ret.length = htonl(sizeof(NET_LOGIN_RET));
        login_ret.retVal = htonl(NETRET_VER_DISMATCH);
        hik_fill_checksum(&login_ret, sizeof(login_ret));
        __WARN("hik %s ifVer=%u mismatch\n", is_relogin ? "RELOGIN" : "LOGIN", login_req.ifVer);
        return hik_writen(fd, &login_ret, sizeof(login_ret));
    }

    hik_fill_login_common(&login_ret, login_req.version);

    if (!is_relogin)
    {
        /* LOGIN always challenges with NEED_RELOGIN */
        login_ret.retVal = htonl(NETRET_NEED_RELOGIN);
        hik_fill_checksum(&login_ret, sizeof(login_ret));
        __INFO("hik LOGIN → NEED_RELOGIN\n");
        return hik_writen(fd, &login_ret, sizeof(login_ret));
    }

    /* RELOGIN: DES decrypt first 16 bytes of user/pass */
    hik_des_decrypt_16(login_req.username);
    hik_des_decrypt_16(login_req.password);
    login_req.username[31] = '\0';
    login_req.password[31] = '\0';

    if (hik_auth_user((const char *)login_req.username, (const char *)login_req.password) == 0)
    {
        login_ret.retVal = htonl(NETRET_QUALIFIED);
        __INFO("hik RELOGIN ok user=%s userID=0x%x\n", login_req.username, HIK_FIXED_USER_ID);
    }
    else if (auth_required == 0)
    {
        login_ret.retVal = htonl(NETRET_QUALIFIED);
        __INFO("hik RELOGIN soft-accept user=%s (hik_auth=0) userID=0x%x\n", login_req.username,
               HIK_FIXED_USER_ID);
    }
    else
    {
        login_ret.retVal = htonl(NETRET_ERRORPASSWD);
        __WARN("hik RELOGIN auth fail user=%s\n", login_req.username);
    }

    hik_fill_checksum(&login_ret, sizeof(login_ret));
    return hik_writen(fd, &login_ret, sizeof(login_ret));
}

static void *hik_client_thread(void *arg)
{
    hik_net_client_s *client = (hik_net_client_s *)arg;
    char recvbuf[HIK_NET_RECV_BUF];
    UINT32 wire_len = 0;
    UINT32 length = 0;
    UINT32 netCmd = 0;
    char ipstr[64] = {0};
    int keep_fd = 0;
    int disp_ret = 0;

    prctl(PR_SET_NAME, "hik_net_cli");
    pthread_detach(pthread_self());
    inet_ntop(AF_INET, &client->addr.sin_addr, ipstr, sizeof(ipstr));

    if (hik_readn(client->fd, &wire_len, sizeof(wire_len)) != 0)
    {
        goto out;
    }
    length = ntohl(wire_len);
    if (length < 24 || length > HIK_NET_RECV_BUF)
    {
        unsigned char first = ((unsigned char *)&wire_len)[0];
        if (first == 0x16)
        {
            __WARN("hik tls probe from %s, drop\n", ipstr);
        }
        else
        {
            __WARN("hik invalid length %u from %s\n", length, ipstr);
        }
        goto out;
    }

    memcpy(recvbuf, &wire_len, sizeof(wire_len));
    if (hik_readn(client->fd, recvbuf + 4, length - 4) != 0)
    {
        goto out;
    }

    memcpy(&netCmd, recvbuf + 12, sizeof(netCmd));
    netCmd = ntohl(netCmd);
    __DBG("hik cmd 0x%x from %s len=0x%x\n", netCmd, ipstr, length);

    if (netCmd == NETCMD_LOGIN)
    {
        (void)hik_handle_login(client->fd, recvbuf, (int)length, 0);
    }
    else if (netCmd == NETCMD_RELOGIN)
    {
        (void)hik_handle_login(client->fd, recvbuf, (int)length, 1);
    }
    else
    {
        disp_ret = hik_net_dispatch(client->fd, netCmd, recvbuf, (int)length);
        if (disp_ret == 1)
        {
            keep_fd = 1;
        }
    }

out:
    if (keep_fd)
    {
        /* Ownership transferred to alarm/voice background task */
        client->fd = -1;
    }
    else if (client->fd >= 0)
    {
        close(client->fd);
        client->fd = -1;
    }
    return NULL;
}

static int hik_listen_job(void *ctx, int *bStart)
{
    struct sockaddr_in server_addr;
    int reuse = 1;

    (void)ctx;
    if (bStart == NULL || *bStart == 0)
    {
        return 0;
    }

    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0)
    {
        __ERR("hik socket failed\n");
        return -1;
    }
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(s_listen_port);
    if (bind(s_listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        __ERR("hik bind port %u failed: %s\n", s_listen_port, strerror(errno));
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }
    if (listen(s_listen_fd, 8) != 0)
    {
        __ERR("hik listen failed\n");
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    __INFO("hik net server listen on %u\n", s_listen_port);
    while (s_server_run)
    {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int connfd = 0;
        int i = 0;
        pthread_t tid;

        connfd = accept(s_listen_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0)
        {
            if (!s_server_run)
            {
                break;
            }
            continue;
        }

        for (i = 0; i < HIK_NET_MAX_CLIENT; i++)
        {
            if (s_clients[i].fd < 0)
            {
                break;
            }
        }
        if (i >= HIK_NET_MAX_CLIENT)
        {
            __WARN("hik client full, drop\n");
            close(connfd);
            continue;
        }

        s_clients[i].fd = connfd;
        s_clients[i].addr = client_addr;
        if (pthread_create(&tid, NULL, hik_client_thread, &s_clients[i]) != 0)
        {
            __ERR("hik create client thread failed\n");
            close(connfd);
            s_clients[i].fd = -1;
        }
    }

    if (s_listen_fd >= 0)
    {
        close(s_listen_fd);
        s_listen_fd = -1;
    }
    return 0;
}

int hik_net_server_start(unsigned short port)
{
    int i = 0;

    if (s_server_run)
    {
        return 0;
    }
    if (port == 0)
    {
        port = 8000;
    }

    for (i = 0; i < HIK_NET_MAX_CLIENT; i++)
    {
        s_clients[i].fd = -1;
    }

    s_listen_port = port;
    s_server_run = 1;
    (void)hik_net_alarm_start();
    memset(&s_listen_thread, 0, sizeof(s_listen_thread));
    s_listen_thread.bAutoDestroy = 0;
    strncpy(s_listen_thread.iThreadName, "hik_net_svr", sizeof(s_listen_thread.iThreadName) - 1);
    s_listen_thread.iThreadjob.ctx = &s_listen_thread;
    s_listen_thread.iThreadjob.func = hik_listen_job;

    if (anj_thread_task_create(&s_listen_thread) != 0)
    {
        s_server_run = 0;
        hik_net_alarm_stop();
        __ERR("create hik_net_svr thread failed\n");
        return -1;
    }
    return 0;
}

int hik_net_server_stop(void)
{
    int i = 0;

    if (!s_server_run && s_listen_fd < 0)
    {
        return 0;
    }

    s_server_run = 0;
    if (s_listen_fd >= 0)
    {
        shutdown(s_listen_fd, SHUT_RDWR);
        close(s_listen_fd);
        s_listen_fd = -1;
    }

    if (s_listen_thread.start)
    {
        anj_thread_task_destroy(&s_listen_thread, 0);
        memset(&s_listen_thread, 0, sizeof(s_listen_thread));
    }

    for (i = 0; i < HIK_NET_MAX_CLIENT; i++)
    {
        if (s_clients[i].fd >= 0)
        {
            close(s_clients[i].fd);
            s_clients[i].fd = -1;
        }
    }

    hik_net_voice_stop();
    hik_net_alarm_stop();
    __INFO("hik net server stopped\n");
    return 0;
}
