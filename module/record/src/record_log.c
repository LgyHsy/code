#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_thread.h"
#include "anj_sdcard.h"
#include "record_log.h"

#define RECORD_LOG_FILE_MAX_SIZE (2 * 1024 * 1024)
#define RECORD_LOG_RECV_BUF_LEN  (10 * 1024)
#define RECORD_LOG_PATH_LEN      (64)
#define RECORD_LOG_FILE_LEN      (96)

typedef enum
{
    RECORD_LOG_CMD_FLUSH = 1,
    RECORD_LOG_CMD_DELETE,
    RECORD_LOG_CMD_WAKEUP,
} record_log_cmd_e;

typedef struct
{
    char *buf;
    int len;
    int max_len;
} record_log_buf_s;

static int s_stRecordLogSock[2] = {-1, -1};
static int s_bFlushBufLogStatus = 0;
static record_log_buf_s s_stRecordLogBuf = {0};
static anj_thread_s s_stRecordLogThread = {0};

static int record_log_sdcard_ready(void)
{
    return (anj_sdcard_status_get() >= ANJ_SDCARD_STATUS_MOUNT) ? 1 : 0;
}

static void record_log_build_dir(char *path, int len)
{
    char stMountPath[32] = {0};

    snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, anj_sdcard_mount_index_get());
    snprintf(path, len, "%s/IPC_Log", stMountPath);
}

static void record_log_build_file(char *path, int len)
{
    char stMountPath[64] = {0};

    snprintf(stMountPath, sizeof(stMountPath), SDCARD_MOUNT_PATH, anj_sdcard_mount_index_get());
    snprintf(path, len, "%s/IPC_Log/Anj_RecordLog.log", stMountPath);
}

static int record_log_flush_buf(void)
{
    char dir[RECORD_LOG_PATH_LEN] = {0};
    char file[RECORD_LOG_FILE_LEN] = {0};
    FILE *fp = NULL;
    int file_size = 0;

    if ((s_stRecordLogBuf.buf == NULL) || (s_stRecordLogBuf.len <= 0))
    {
        return 0;
    }

    if (!record_log_sdcard_ready())
    {
        __INFO("sdcard is not ready for record log\n");
        return -1;
    }

    record_log_build_dir(dir, sizeof(dir));
    record_log_build_file(file, sizeof(file));

    if (!anj_mw_file_exists(dir))
    {
        if (mkdir(dir, S_IRWXU | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) != 0)
        {
            __ERR("mkdir %s failed: %s\n", dir, strerror(errno));
            return -1;
        }
    }

    file_size = get_file_size(file);
    if ((file_size >= RECORD_LOG_FILE_MAX_SIZE) && anj_mw_file_exists(file))
    {
        remove(file);
    }

    fp = anj_mw_fopen(file, "a");
    if (fp == NULL)
    {
        __ERR("fopen %s failed: %s\n", file, strerror(errno));
        return -1;
    }

    if ((unsigned int)s_stRecordLogBuf.len != anj_mw_fwrite(fp, s_stRecordLogBuf.buf, s_stRecordLogBuf.len))
    {
        __ERR("fwrite %s failed: %s\n", file, strerror(errno));
        anj_mw_fclose(fp);
        return -1;
    }

    anj_mw_fflush(fp);
    anj_mw_fclose(fp);
    s_stRecordLogBuf.len = 0;
    s_bFlushBufLogStatus = 0;
    return 0;
}

static int record_log_handle_log(int fd)
{
    int recv_len = 0;
    char recv_buf[RECORD_LOG_RECV_BUF_LEN] = {0};

    recv_len = safe_recv(fd, recv_buf, sizeof(recv_buf), 0);
    if (recv_len <= 0)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            __ERR("record_log recv failed: %s\n", strerror(errno));
        }
        return -1;
    }

    if ((s_stRecordLogBuf.buf == NULL) || (recv_len > s_stRecordLogBuf.max_len))
    {
        return -1;
    }

    if ((s_stRecordLogBuf.len + recv_len) >= s_stRecordLogBuf.max_len)
    {
        if (record_log_flush_buf() != 0)
        {
            return -1;
        }
    }

    memcpy(&s_stRecordLogBuf.buf[s_stRecordLogBuf.len], recv_buf, recv_len);
    s_stRecordLogBuf.len += recv_len;
    return 0;
}

static int record_log_drain_log(int fd, int bSave)
{
    int iRet = 0;

    while (1)
    {
        int recv_len = 0;
        char recv_buf[RECORD_LOG_RECV_BUF_LEN] = {0};

        recv_len = safe_recv(fd, recv_buf, sizeof(recv_buf), MSG_DONTWAIT);
        if (recv_len <= 0)
        {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != 0))
            {
                __ERR("record_log drain failed: %s\n", strerror(errno));
                iRet = -1;
            }
            break;
        }

        if (!bSave)
        {
            continue;
        }

        if ((s_stRecordLogBuf.buf == NULL) || (recv_len > s_stRecordLogBuf.max_len))
        {
            iRet = -1;
            break;
        }

        if ((s_stRecordLogBuf.len + recv_len) >= s_stRecordLogBuf.max_len)
        {
            if (record_log_flush_buf() != 0)
            {
                iRet = -1;
                break;
            }
        }

        memcpy(&s_stRecordLogBuf.buf[s_stRecordLogBuf.len], recv_buf, recv_len);
        s_stRecordLogBuf.len += recv_len;
    }

    return iRet;
}

static int record_log_handle_cmd(int fd)
{
    char cmd = 0;
    int recv_len = 0;
    char file[RECORD_LOG_FILE_LEN] = {0};

    recv_len = safe_recv(fd, &cmd, sizeof(cmd), 0);
    if (recv_len <= 0)
    {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
        {
            __ERR("record_log recv cmd failed: %s\n", strerror(errno));
        }
        return -1;
    }

    switch ((record_log_cmd_e)cmd)
    {
        case RECORD_LOG_CMD_FLUSH:
            record_log_drain_log(s_stRecordLogSock[1], 1);
            return record_log_flush_buf();
        case RECORD_LOG_CMD_DELETE:
            record_log_drain_log(s_stRecordLogSock[1], 0);
            s_stRecordLogBuf.len = 0;
            record_log_build_file(file, sizeof(file));
            if (anj_mw_file_exists(file))
            {
                remove(file);
            }
            return 0;
        case RECORD_LOG_CMD_WAKEUP:
        default:
            return 0;
    }
}

static int record_log_proc(void *arg, int *bStart)
{
    while (bStart && *bStart)
    {
        fd_set read_set;
        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
        int max_fd = -1;
        int ret = 0;

        FD_ZERO(&read_set);
        if (s_stRecordLogSock[0] >= 0)
        {
            FD_SET(s_stRecordLogSock[0], &read_set);
            max_fd = s_stRecordLogSock[0];
        }
        if (s_stRecordLogSock[1] >= 0)
        {
            FD_SET(s_stRecordLogSock[1], &read_set);
            if (s_stRecordLogSock[1] > max_fd)
            {
                max_fd = s_stRecordLogSock[1];
            }
        }

        ret = select(max_fd + 1, &read_set, NULL, NULL, &timeout);
        if (ret < 0)
        {
            if (errno != EINTR)
            {
                __ERR("record_log select failed: %s\n", strerror(errno));
            }
            continue;
        }
        if (ret == 0)
        {
            continue;
        }

        if ((s_stRecordLogSock[1] >= 0) && FD_ISSET(s_stRecordLogSock[1], &read_set))
        {
            record_log_handle_log(s_stRecordLogSock[1]);
        }
        if ((s_stRecordLogSock[0] >= 0) && FD_ISSET(s_stRecordLogSock[0], &read_set))
        {
            record_log_handle_cmd(s_stRecordLogSock[0]);
        }
    }

    record_log_flush_buf();
    return 0;
}

int record_log_flush_file(int bBlockTimes)
{
    char cmd = RECORD_LOG_CMD_FLUSH;

    if ((s_stRecordLogThread.start == 0) || (s_stRecordLogSock[1] < 0))
    {
        return -1;
    }

    s_bFlushBufLogStatus = 1;
    if (safe_send(s_stRecordLogSock[1], &cmd, sizeof(cmd), MSG_NOSIGNAL) <= 0)
    {
        s_bFlushBufLogStatus = 0;
        return -1;
    }

    while ((bBlockTimes > 0) && s_bFlushBufLogStatus)
    {
        usleep(200 * 1000);
        bBlockTimes--;
    }

    return s_bFlushBufLogStatus ? -1 : 0;
}

int record_log_del_file(void)
{
    char cmd = RECORD_LOG_CMD_DELETE;
    char file[RECORD_LOG_FILE_LEN] = {0};

    if ((s_stRecordLogThread.start != 0) && (s_stRecordLogSock[1] >= 0))
    {
        if (safe_send(s_stRecordLogSock[1], &cmd, sizeof(cmd), MSG_NOSIGNAL) > 0)
        {
            return 0;
        }
    }

    record_log_build_file(file, sizeof(file));
    if (anj_mw_file_exists(file))
    {
        remove(file);
    }

    return 0;
}

int record_log_get_file(char *szLogFile, int nLen)
{
    if ((szLogFile == NULL) || (nLen <= 1))
    {
        __ERR("invalid input\n");
        return -1;
    }

    if (record_log_flush_file(10) != 0)
    {
        __ERR("record log flush failed\n");
        return -1;
    }

    record_log_build_file(szLogFile, nLen);
    return 0;
}

void record_log_print(unsigned int nLogLevel, const char *psFileName, const char *psFuncName, int line, const char *format, ...)
{
    char log_buf[RECORD_LOG_RECV_BUF_LEN] = {0};
    int len = 0;
    va_list args;
    time_t now = time(NULL);
    struct tm tm_now;
    (void)psFileName;

    if ((s_stRecordLogThread.start == 0) || (s_stRecordLogSock[0] < 0))
    {
        return;
    }

    localtime_r(&now, &tm_now);
    len = snprintf(log_buf, sizeof(log_buf),
                   "[%04d/%02d/%02d-%02d:%02d:%02d|%s|%d|RLog-Lv%d]",
                   tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                   tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec,
                   psFuncName, line, nLogLevel);
    if ((len <= 0) || (len >= (int)sizeof(log_buf)))
    {
        return;
    }

    va_start(args, format);
    len += vsnprintf(log_buf + len, sizeof(log_buf) - len, format, args);
    va_end(args);
    if ((len <= 0) || (len >= (int)sizeof(log_buf)))
    {
        return;
    }

    safe_send(s_stRecordLogSock[0], log_buf, len, MSG_NOSIGNAL);
}

int record_log_init(void)
{
    if (s_stRecordLogThread.start)
    {
        return 0;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, s_stRecordLogSock) != 0)
    {
        __ERR("record_log socketpair failed: %s\n", strerror(errno));
        s_stRecordLogSock[0] = -1;
        s_stRecordLogSock[1] = -1;
        return -1;
    }

    s_stRecordLogBuf.buf = (char *)anj_mw_malloc(RECORD_LOG_RECV_BUF_LEN);
    if (s_stRecordLogBuf.buf == NULL)
    {
        __ERR("record_log malloc failed\n");
        close(s_stRecordLogSock[0]);
        close(s_stRecordLogSock[1]);
        s_stRecordLogSock[0] = -1;
        s_stRecordLogSock[1] = -1;
        return -1;
    }

    s_stRecordLogBuf.len = 0;
    s_stRecordLogBuf.max_len = RECORD_LOG_RECV_BUF_LEN;
    memset(&s_stRecordLogThread, 0, sizeof(s_stRecordLogThread));
    s_stRecordLogThread.bAutoDestroy = 0;
    snprintf(s_stRecordLogThread.iThreadName, sizeof(s_stRecordLogThread.iThreadName), "record_log");
    s_stRecordLogThread.iThreadjob.ctx = NULL;
    s_stRecordLogThread.iThreadjob.func = record_log_proc;
    if (anj_thread_task_create(&s_stRecordLogThread) != 0)
    {
        __ERR("record_log thread create failed\n");
        anj_mw_free(s_stRecordLogBuf.buf);
        memset(&s_stRecordLogBuf, 0, sizeof(s_stRecordLogBuf));
        close(s_stRecordLogSock[0]);
        close(s_stRecordLogSock[1]);
        s_stRecordLogSock[0] = -1;
        s_stRecordLogSock[1] = -1;
        return -1;
    }

    return 0;
}

int record_log_uninit(void)
{
    char cmd = RECORD_LOG_CMD_WAKEUP;

    if (s_stRecordLogThread.start)
    {
        if (s_stRecordLogSock[1] >= 0)
        {
            safe_send(s_stRecordLogSock[1], &cmd, sizeof(cmd), MSG_NOSIGNAL);
        }
        anj_thread_task_destroy(&s_stRecordLogThread, 0);
    }

    if (s_stRecordLogSock[0] >= 0)
    {
        close(s_stRecordLogSock[0]);
        s_stRecordLogSock[0] = -1;
    }
    if (s_stRecordLogSock[1] >= 0)
    {
        close(s_stRecordLogSock[1]);
        s_stRecordLogSock[1] = -1;
    }

    anj_mw_free(s_stRecordLogBuf.buf);
    memset(&s_stRecordLogBuf, 0, sizeof(s_stRecordLogBuf));
    memset(&s_stRecordLogThread, 0, sizeof(s_stRecordLogThread));
    s_bFlushBufLogStatus = 0;
    return 0;
}
