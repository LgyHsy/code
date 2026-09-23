#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "anj_config.h"
#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "alarm_link.h"

#include "hik_net_alarm.h"
#include "hik_net_cmd.h"
#include "hik_net_types.h"

#define HIK_ALARM_CLIENT_MAX 16
#define HIK_ALARM_KEEPALIVE_MS 5000

typedef struct
{
    int clientfd;
    unsigned long long sendtime_ms;
} hik_alarm_socket_s;

typedef struct
{
    hik_alarm_socket_s alarmSocket[HIK_ALARM_CLIENT_MAX];
    NETRET_ALARMINFO alarmInfo;
    int bAlarm;
    pthread_mutex_t lock;
} hik_alarm_ctx_s;

#pragma pack(push, 1)
typedef struct
{
    UINT32 length;
    UINT32 status;
    NETRET_ALARMINFO alarmInfo;
    char reserved[76];
} hik_net_ret_alarminfo_s;
#pragma pack(pop)

static hik_alarm_ctx_s s_alarm;
static pthread_t s_alarm_tid;
static volatile int s_alarm_run = 0;
static volatile int s_alarm_started = 0;

static void hik_alarm_lock(void)
{
    pthread_mutex_lock(&s_alarm.lock);
}

static void hik_alarm_unlock(void)
{
    pthread_mutex_unlock(&s_alarm.lock);
}

static void hik_alarm_close_slot(int index)
{
    if (index < 0 || index >= HIK_ALARM_CLIENT_MAX)
    {
        return;
    }
    if (s_alarm.alarmSocket[index].clientfd > 0)
    {
        close(s_alarm.alarmSocket[index].clientfd);
    }
    memset(&s_alarm.alarmSocket[index], 0, sizeof(s_alarm.alarmSocket[index]));
    s_alarm.alarmSocket[index].clientfd = -1;
}

static void *hik_alarm_up_task(void *arg)
{
    hik_net_ret_alarminfo_s pkt;
    char recv_buffer[256];

    (void)arg;
    prctl(PR_SET_NAME, "hik_alarm_up");
    memset(&pkt, 0, sizeof(pkt));

    while (s_alarm_run)
    {
        fd_set readset;
        fd_set writeset;
        struct timeval timeout;
        int maxfd = -1;
        int i = 0;
        int ret = 0;
        unsigned long long now_ms = anj_mw_get_cputime_ms(NULL);
        int bAlarm = 0;
        NETRET_ALARMINFO alarmInfo;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        FD_ZERO(&readset);
        FD_ZERO(&writeset);

        hik_alarm_lock();
        for (i = 0; i < HIK_ALARM_CLIENT_MAX; i++)
        {
            int fd = s_alarm.alarmSocket[i].clientfd;
            if (fd <= 0)
            {
                continue;
            }
            FD_SET(fd, &readset);
            FD_SET(fd, &writeset);
            if (maxfd < fd)
            {
                maxfd = fd;
            }
        }
        bAlarm = s_alarm.bAlarm;
        alarmInfo = s_alarm.alarmInfo;
        hik_alarm_unlock();

        if (maxfd < 0)
        {
            usleep(50 * 1000);
            continue;
        }

        ret = select(maxfd + 1, &readset, &writeset, NULL, &timeout);
        if (ret <= 0)
        {
            continue;
        }

        hik_alarm_lock();
        for (i = 0; i < HIK_ALARM_CLIENT_MAX; i++)
        {
            int fd = s_alarm.alarmSocket[i].clientfd;
            if (fd <= 0)
            {
                continue;
            }

            if (FD_ISSET(fd, &readset))
            {
                int n = (int)recv(fd, recv_buffer, sizeof(recv_buffer), 0);
                if (n <= 0)
                {
                    __WARN("hik alarm fd=%d closed\n", fd);
                    hik_alarm_close_slot(i);
                    continue;
                }
            }

            if (!FD_ISSET(fd, &writeset))
            {
                continue;
            }

            if (bAlarm > 0)
            {
                int sendlen = (int)sizeof(pkt);
                memset(&pkt, 0, sizeof(pkt));
                pkt.length = htonl((UINT32)sendlen);
                pkt.status = htonl(0x68); /* old trunk uses 0x68, not NETRET_NEEDRECVDATA */
                pkt.alarmInfo.alarmType = htonl(alarmInfo.alarmType);
                pkt.alarmInfo.alarmInNumber = alarmInfo.alarmInNumber;
                pkt.alarmInfo.triggeredAlarmOut = htonl(alarmInfo.triggeredAlarmOut);
                pkt.alarmInfo.triggeredRecChan = htonl(alarmInfo.triggeredRecChan);
                pkt.alarmInfo.channelNo = htonl(alarmInfo.channelNo);
                pkt.alarmInfo.diskNo = htonl(alarmInfo.diskNo);
                if (alarmInfo.alarmType == ALARMTYPE_MOTDET)
                {
                    pkt.reserved[44] = 1;
                }

                if (hik_writen(fd, &pkt, (size_t)sendlen) != 0)
                {
                    __WARN("hik alarm write failed fd=%d\n", fd);
                    hik_alarm_close_slot(i);
                }
                else
                {
                    s_alarm.alarmSocket[i].sendtime_ms = now_ms;
                    __INFO("hik alarm push type=%u fd=%d\n", alarmInfo.alarmType, fd);
                }
            }
            else if (now_ms - s_alarm.alarmSocket[i].sendtime_ms > HIK_ALARM_KEEPALIVE_MS)
            {
                UINT32 keep[2];
                keep[0] = htonl(8);
                keep[1] = htonl(NETRET_EXCHANGE);
                if (hik_writen(fd, keep, sizeof(keep)) != 0)
                {
                    __WARN("hik alarm keepalive failed fd=%d\n", fd);
                    hik_alarm_close_slot(i);
                }
                else
                {
                    s_alarm.alarmSocket[i].sendtime_ms = now_ms;
                }
            }
        }
        s_alarm.bAlarm = 0;
        hik_alarm_unlock();
        usleep(10 * 1000);
    }
    return NULL;
}

int hik_net_alarm_start(void)
{
    int i = 0;

    if (s_alarm_started)
    {
        return 0;
    }

    memset(&s_alarm, 0, sizeof(s_alarm));
    pthread_mutex_init(&s_alarm.lock, NULL);
    for (i = 0; i < HIK_ALARM_CLIENT_MAX; i++)
    {
        s_alarm.alarmSocket[i].clientfd = -1;
    }

    s_alarm_run = 1;
    if (pthread_create(&s_alarm_tid, NULL, hik_alarm_up_task, NULL) != 0)
    {
        s_alarm_run = 0;
        __ERR("hik create alarmUpTask failed\n");
        return -1;
    }
    s_alarm_started = 1;
    __INFO("hik alarmUpTask started\n");
    return 0;
}

int hik_net_alarm_stop(void)
{
    int i = 0;

    if (!s_alarm_started)
    {
        return 0;
    }

    s_alarm_run = 0;
    pthread_join(s_alarm_tid, NULL);
    s_alarm_tid = 0;

    hik_alarm_lock();
    for (i = 0; i < HIK_ALARM_CLIENT_MAX; i++)
    {
        hik_alarm_close_slot(i);
    }
    hik_alarm_unlock();
    pthread_mutex_destroy(&s_alarm.lock);
    s_alarm_started = 0;
    __INFO("hik alarmUpTask stopped\n");
    return 0;
}

int hik_net_alarm_add_fd(int fd)
{
    int KeepAlive = 1;
    int KeepIdle = 1;
    int KeepInterval = 1;
    int KeepCount = 3;
    int one = 256;
    int i = 0;

    if (fd <= 0)
    {
        return -1;
    }

    if (!s_alarm_started)
    {
        if (hik_net_alarm_start() != 0)
        {
            return -1;
        }
    }

    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *)&one, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *)&one, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&KeepAlive, sizeof(KeepAlive));
#ifdef TCP_KEEPIDLE
    setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (void *)&KeepIdle, sizeof(KeepIdle));
    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (void *)&KeepInterval, sizeof(KeepInterval));
    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (void *)&KeepCount, sizeof(KeepCount));
#else
    (void)KeepIdle;
    (void)KeepInterval;
    (void)KeepCount;
#endif

    hik_alarm_lock();
    for (i = 0; i < HIK_ALARM_CLIENT_MAX; i++)
    {
        if (fd == s_alarm.alarmSocket[i].clientfd)
        {
            hik_alarm_unlock();
            return 0;
        }
    }
    for (i = 0; i < HIK_ALARM_CLIENT_MAX; i++)
    {
        if (s_alarm.alarmSocket[i].clientfd <= 0)
        {
            s_alarm.alarmSocket[i].clientfd = fd;
            s_alarm.alarmSocket[i].sendtime_ms = anj_mw_get_cputime_ms(NULL);
            hik_alarm_unlock();
            __INFO("hik alarm add fd=%d slot=%d\n", fd, i);
            return 0;
        }
    }

    /* Replace oldest slot 0 like old trunk */
    if (s_alarm.alarmSocket[0].clientfd > 0)
    {
        __WARN("hik alarm replace fd=%d with fd=%d\n", s_alarm.alarmSocket[0].clientfd, fd);
        close(s_alarm.alarmSocket[0].clientfd);
    }
    s_alarm.alarmSocket[0].clientfd = fd;
    s_alarm.alarmSocket[0].sendtime_ms = anj_mw_get_cputime_ms(NULL);
    hik_alarm_unlock();
    return 0;
}

int hik_net_alarm_notify(int alarm_code, int alarm_flag)
{
    if (alarm_flag == ALARM_FLAG_DISAPPEAR)
    {
        return 0;
    }

    hik_alarm_lock();
    memset(&s_alarm.alarmInfo, 0, sizeof(s_alarm.alarmInfo));

    switch (alarm_code)
    {
    case ALARM_CODE_MOTION_DETECT:
    case ALARM_CODE_VIDEO_GATE:
    case ALARM_CODE_VIDEO_COVERD:
    case ALARM_CODE_VIDEO_AI:
        s_alarm.alarmInfo.alarmType = ALARMTYPE_MOTDET;
        s_alarm.alarmInfo.channelNo = 0;
        s_alarm.bAlarm = 1;
        break;
    case ALARM_CODE_IO_ALARM:
    case ALARM_CODE_EXTERNAL_IO_ALARM:
        s_alarm.alarmInfo.alarmType = ALARMTYPE_ALARMIN;
        s_alarm.alarmInfo.alarmInNumber = 1;
        s_alarm.alarmInfo.channelNo = 0;
        s_alarm.bAlarm = 1;
        break;
    case ALARM_CODE_VIDEO_LOST:
        s_alarm.alarmInfo.alarmType = ALARMTYPE_VI_LOST;
        s_alarm.alarmInfo.channelNo = 1;
        s_alarm.bAlarm = 1;
        break;
    default:
        hik_alarm_unlock();
        return 0;
    }
    hik_alarm_unlock();
    __INFO("hik alarm notify code=%d type=%u\n", alarm_code, s_alarm.alarmInfo.alarmType);
    return 0;
}
