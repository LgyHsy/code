#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <time.h>
#include <sys/time.h>

#include "anj_mw_log.h"
#include "anj_mw_mem.h"
#include "anj_mw_watchdog.h"
#include "anj_mw_hwctrl.h"

#include "anj_comm.h"
#include "anj_config.h"
#include "anj_module.h"
#include "eventhub.h"
#include "anj_video.h"
#include "anj_audio.h"
#include "anj_osd.h"
#include "anj_record.h"
#include "record_log.h"
#include "anj_systime.h"
#include "audio_utils.h"
#include "anj_ispctl.h"
#include "anj_sysmng.h"
#include "sdk_option.h"

#define TIME_INIT_YEAR 2020

#define UART1_DEV               "/dev/ttyS1"
#define UART2_DEV               "/dev/ttyS2"
#define UART_TEST_INTERVAL_SEC  1
#define UART_TEST_BAUD          B115200

typedef struct
{
    int fd;
    const char *name;
    const char *dev;
    unsigned int tx_cnt;
    unsigned int rx_cnt;
} uart_test_ctx_t;

static uart_test_ctx_t s_uart1 = {-1, "UART1", UART1_DEV, 0, 0};
static uart_test_ctx_t s_uart2 = {-1, "UART2", UART2_DEV, 0, 0};
static volatile int s_uart_test_run = 0;
static pthread_t s_uart_test_tid;

extern int check_ubootargs2(const char *name, const char *value, int print);

static int uart_test_open(uart_test_ctx_t *ctx)
{
    struct termios tio;

    if (ctx == NULL || ctx->dev == NULL)
    {
        return -1;
    }

    ctx->fd = open(ctx->dev, O_RDWR | O_NOCTTY);
    if (ctx->fd < 0)
    {
        __ERR("%s open %s failed: %s\n", ctx->name, ctx->dev, strerror(errno));
        return -1;
    }

    if (tcgetattr(ctx->fd, &tio) != 0)
    {
        __ERR("%s tcgetattr failed: %s\n", ctx->name, strerror(errno));
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    cfsetospeed(&tio, UART_TEST_BAUD);
    cfsetispeed(&tio, UART_TEST_BAUD);

    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cflag |= CLOCAL | CREAD;

    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_oflag &= ~OPOST;
    tio.c_iflag &= ~(IXON | IXOFF | IXANY | INLCR | ICRNL);

    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 0;

    if (tcsetattr(ctx->fd, TCSANOW, &tio) != 0)
    {
        __ERR("%s tcsetattr failed: %s\n", ctx->name, strerror(errno));
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }

    tcflush(ctx->fd, TCIOFLUSH);
    __INFO("%s init ok, dev=%s fd=%d\n", ctx->name, ctx->dev, ctx->fd);
    return 0;
}

static void uart_test_close(uart_test_ctx_t *ctx)
{
    if (ctx != NULL && ctx->fd >= 0)
    {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

static int uart_test_send(uart_test_ctx_t *ctx)
{
    char tx_buf[64] = {0};
    int tx_len = 0;
    int ret = 0;

    if (ctx == NULL || ctx->fd < 0)
    {
        return -1;
    }

    tx_len = snprintf(tx_buf, sizeof(tx_buf), "%s TX %u\r\n", ctx->name, ctx->tx_cnt++);
    ret = write(ctx->fd, tx_buf, (size_t)tx_len);
    if (ret != tx_len)
    {
        __ERR("%s write failed: ret=%d err=%s\n", ctx->name, ret, strerror(errno));
        return -1;
    }

    __INFO("%s send: %s", ctx->name, tx_buf);
    return 0;
}

static void uart_test_read(uart_test_ctx_t *ctx)
{
    unsigned char rx_buf[256] = {0};
    int rx_len = 0;
    int i = 0;

    if (ctx == NULL || ctx->fd < 0)
    {
        return;
    }

    rx_len = read(ctx->fd, rx_buf, sizeof(rx_buf) - 1);
    if (rx_len <= 0)
    {
        if (rx_len < 0 && errno != EAGAIN && errno != EINTR)
        {
            __ERR("%s read failed: %s\n", ctx->name, strerror(errno));
        }
        return;
    }

    ctx->rx_cnt++;
    __INFO("%s recv[%u] len=%d: ", ctx->name, ctx->rx_cnt, rx_len);
    for (i = 0; i < rx_len; i++)
    {
        if (rx_buf[i] >= 0x20 && rx_buf[i] <= 0x7E)
        {
            printf("%c", rx_buf[i]);
        }
        else
        {
            printf("\\x%02X", rx_buf[i]);
        }
    }
    printf("\n");
}

static void *uart_test_thread(void *arg)
{
    uart_test_ctx_t *uart_list[] = {&s_uart1, &s_uart2};
    time_t last_tx_time = 0;

    (void)arg;
    __INFO("uart test thread enter\n");

    while (s_uart_test_run)
    {
        fd_set readfds;
        struct timeval tv;
        int max_fd = -1;
        int select_ret = 0;
        int i = 0;
        time_t now = time(NULL);

        FD_ZERO(&readfds);
        for (i = 0; i < (int)(sizeof(uart_list) / sizeof(uart_list[0])); i++)
        {
            if (uart_list[i]->fd >= 0)
            {
                FD_SET(uart_list[i]->fd, &readfds);
                if (uart_list[i]->fd > max_fd)
                {
                    max_fd = uart_list[i]->fd;
                }
            }
        }

        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;
        if (max_fd >= 0)
        {
            select_ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);
            if (select_ret > 0)
            {
                for (i = 0; i < (int)(sizeof(uart_list) / sizeof(uart_list[0])); i++)
                {
                    if (uart_list[i]->fd >= 0 && FD_ISSET(uart_list[i]->fd, &readfds))
                    {
                        uart_test_read(uart_list[i]);
                    }
                }
            }
            else if (select_ret < 0 && errno != EINTR)
            {
                __ERR("uart test select failed: %s\n", strerror(errno));
                usleep(100 * 1000);
            }
        }
        else
        {
            usleep(100 * 1000);
        }

        if (last_tx_time == 0 || (now - last_tx_time) >= UART_TEST_INTERVAL_SEC)
        {
            last_tx_time = now;
            if (s_uart1.fd >= 0)
            {
                uart_test_send(&s_uart1);
            }
            if (s_uart2.fd >= 0)
            {
                uart_test_send(&s_uart2);
            }
        }
    }

    __INFO("uart test thread exit\n");
    return NULL;
}

static int uart_test_start(void)
{
    int ret = 0;

    uart_test_open(&s_uart1);
    uart_test_open(&s_uart2);
    if (s_uart1.fd < 0 && s_uart2.fd < 0)
    {
        __ERR("uart test start failed, no uart available\n");
        return -1;
    }

    s_uart_test_run = 1;
    ret = pthread_create(&s_uart_test_tid, NULL, uart_test_thread, NULL);
    if (ret != 0)
    {
        s_uart_test_run = 0;
        uart_test_close(&s_uart1);
        uart_test_close(&s_uart2);
        __ERR("create uart test thread failed: %s\n", strerror(ret));
        return -1;
    }

    __INFO("uart test started, interval=%ds\n", UART_TEST_INTERVAL_SEC);
    return 0;
}

static void uart_test_stop(void)
{
    if (!s_uart_test_run)
    {
        return;
    }

    s_uart_test_run = 0;
    pthread_join(s_uart_test_tid, NULL);
    uart_test_close(&s_uart1);
    uart_test_close(&s_uart2);
}

int bExit = 0;
int bEnd = 0;

static int IsTimeNotCalibration(void)
{
    time_t tNow = time(NULL);
    struct tm stNow = {0};

    if (tNow <= 0)
    {
        return 1;
    }

    localtime_r(&tNow, &stNow);
    return ((stNow.tm_year + 1900) < TIME_INIT_YEAR) ? 1 : 0;
}

void sighandel(int sig)
{
    if (!bExit)
    {
        printf("\033[31;1;5m####INFO signal:%d \033[0m\n", sig);

        /* 仅设置退出标志：不要在 signal handler 中执行复杂清理 */
        bExit = 1;
    }
    else
    {
        /* 如果第二次收到信号，设置强制退出标志，主循环可检测并立即退出 */
        // printf("\033[31;1;5m####INFO sigterm (force)\033[0m\n");
        bEnd = 1;
    }
}

void init_signals(void)
{
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);

    struct sigaction sa;
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGSEGV);

    sa.sa_handler = sighandel;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sighandel;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sighandel;
    sigaction(SIGSEGV, &sa, NULL);
}

void st_log(const char *pLogOut)
{
    printf(pLogOut);
}

void check_mma_size()
{
    if (1 == check_ubootargs2("mem_mma", ANJ_CAMERA_MMA_SIZE, 0))
    {
        __ERR("Modify uboot args OK. reboot.");
        __RECORD_LOG_INFO("Modify uboot args OK. reboot.");
        usleep(1000 * 500);
        anj_sysmng_reboot();
    }
}

int main(int argc, char **argv)
{
    init_signals();
    // system("ulimit -c unlimited");
    // system("echo \"/mnt/mmc0/core-%e-%p-%t\" > /proc/sys/kernel/core_pattern");

    anj_mw_log_init();

    check_mma_size();

    anj_mw_hwctrl_init();

    eventhub_init();

    modules_init();
    anj_sysmng_second_config_capability_apply();

    // uart_test_start();
    WatchDogOpen();

    WatchDogFeed();

    int count = 0;
    int show = 0;
    int lastDay = -1;
    int timeSyncTick = 0;

    while (!bExit)
    {
        if (bEnd)
        {
            /* 收到第二次信号，立即跳出循环 */
            break;
        }

        DevInfo *pstDevInfo = getDevInfo();
        if (pstDevInfo != NULL && pstDevInfo->bUpgrading)
        {
            usleep(100 * 1000);
            continue;
        }

        if (count++ > 10)
        {
            // 跨天强制请求I帧 确保卡录回放跨天0刻度第一帧就是0s
            SYSTEM_TIME sys_time = {0};
            SystemGetNowTime(&sys_time);

            if (lastDay == -1)
            {
			    // 第一次调用时初始化
                lastDay = sys_time.day;
            }
            if (sys_time.day != lastDay)
            {
                lastDay = sys_time.day;
                __WARN("cross-day req IDR!\n");
                for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
                    for (int j = 0; j < MAX_VENC_CHN; j++)
                        anj_video_request_idr(i, j);
            }

            if (WatchDogFeedStopGet() == 0)
                WatchDogFeed();
            count = 0;
        }

        if (0 == access("/tmp/sim", F_OK))
        {
            remove("/tmp/sim");
            EventResult event_result;
            show = !show;
            eventhub_publish(EVENTHUB_CLASS_STATUS, EVENTHUB_4G_SIM_SET, &event_result, (void *)&show);
        }
        if (0 == access("/tmp/day", F_OK))
        {
            remove("/tmp/day");
            EventResult event_result;
            ispbin_info ispbin_info;
            ispbin_info.iCameraIdex = 0;
            ispbin_info.filepath = ISP_DAY_BIN_PATH;
            eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_LOAD_ISPBIN, &event_result, (void *)&ispbin_info);
            __INFO("%s, result:%d\n", EVENTHUB_LOAD_ISPBIN, event_result.ret);
        }
        if (0 == access("/tmp/ptztrack", F_OK))
        {
            EventResult event_result = {0};
            event_rect_param_s stEventRectParam = {0};
            stEventRectParam.s32RectCnt = 1;
            char buf[64] = {0};
            anj_mw_read_file_limit_len("/tmp/ptztrack", buf, sizeof(buf) - 1);
            sscanf(buf, "%d,%d,%d,%d", &stEventRectParam.event_rect->pos_x,
                   &stEventRectParam.event_rect->pos_y,
                   &stEventRectParam.event_rect->width,
                   &stEventRectParam.event_rect->height);
            eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_TRACK, &event_result, (void *)&stEventRectParam);
            remove("/tmp/ptztrack");
        }

        if (++timeSyncTick >= 600)
        {
            timeSyncTick = 0;
            struct tm stRecTm = {0};
            struct timeval stTv = {0};

            if (IsTimeNotCalibration() && anj_record_lastest_time(&stRecTm) == 0)
            {
                stTv.tv_sec = mktime(&stRecTm);
                stTv.tv_usec = 0;
                if (stTv.tv_sec <= 0)
                {
                    continue;
                }

                __INFO("main force time sync by sd newest time: %04d-%02d-%02d %02d:%02d:%02d\n",
                       stRecTm.tm_year + 1900, stRecTm.tm_mon + 1, stRecTm.tm_mday,
                       stRecTm.tm_hour, stRecTm.tm_min, stRecTm.tm_sec);
                anj_systime_set_ex(stTv);
            }
        }

        usleep(100 * 1000);
    }
    printf("########## Exit\n");

    uart_test_stop();
    modules_uninit(NULL);
    eventhub_uninit();
    WatchDogClose();
    anj_mw_log_uninit();

    bEnd = 1;
    return 0;
}
