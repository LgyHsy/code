#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include "anj_mw_comm.h"

#define DEBUG_NETWORK_TYPE_UDP 1
#define DEBUG_NETWORK_TYPE_TCP 2
#define DEBUG_NETWORK_TYPE_BROADCAST 3

#define NETWORK_DEBUG_PORT 9999

#define DEBUG_LOG_LEVEL_INFO "/mnt/nand/debug_info.flag"
#define DEBUG_LOG_LEVEL_EVENT "/mnt/nand/debug_event.flag"
#define DEBUG_LOG_LEVEL_WARNING "/mnt/nand/debug_warning.flag"
#define DEBUG_LOG_LEVEL_ERROR "/mnt/nand/debug_error.flag"
#define DEBUG_LOG_LEVEL_ALL "/mnt/nand/debug_all.flag"
#define DEBUG_LOG_LEVEL_CRIT "/mnt/nand/debug_crit.flag"
#define DEBUG_LOG_LEVEL_P2P "/mnt/nand/debug_p2p.flag"

#define DEBUG_FILE_FULL_NAME "/tmp/debug.log"

#define DEBUG_MAX_INFO_LEN 256

typedef unsigned int (*SHELL_EXEC_FUNC)(
    const char *p0,
    const char *p1,
    const char *p2,
    const char *p3,
    const char *p4,
    const char *p5,
    const char *p6,
    const char *p7,
    const char *p8,
    const char *p9);

#define LOGLEVEL_UPDATE_TIME (10 * 1000)
#define LOGLEVEL_UPDATE_FILE "/tmp/slog"

#define MAX_DEBUG_CLIENTS 3

typedef struct
{
    int minLevel;
    int consoleEnable;
    int fileEnable;
    char filePath[16];
    char fileName[64];
    int maxFileSize;
    int networkEnable;
    int networkType;
    int networkPort;
} DEBUG_CONFIG;

typedef struct
{
    int fd;
    struct sockaddr_in client_sockaddr;
    int sendinfo;
    int recvZeroNum;
} DEBUG_CLIENT_ENTRY;

#define MAX_DEBUG_INFO_LENGTH 1024
#define MAX_DEBUG_INFO_BUFFER_COUNT 300

typedef struct TAG_DEBUG_INFO
{
    char debug_info[MAX_DEBUG_INFO_LENGTH];
    struct TAG_DEBUG_INFO *next;
} DEBUG_INFO_ENTRY;

typedef struct
{
    DEBUG_INFO_ENTRY *head;
    DEBUG_INFO_ENTRY *write_ptr;
    DEBUG_INFO_ENTRY *read_ptr[MAX_DEBUG_CLIENTS];
    DEBUG_INFO_ENTRY list[MAX_DEBUG_INFO_BUFFER_COUNT];
} DEBUG_INFO_RING_BUFFER;

typedef struct
{
    DEBUG_CONFIG stLogConfig;
    DEBUG_INFO_RING_BUFFER stLogRingBuffer;
    DEBUG_CLIENT_ENTRY stLogClientList[MAX_DEBUG_CLIENTS];
    int logLevel;
    pthread_mutex_t logMutex;
    anj_thread_s logServerThread;
} log_info_t;

typedef struct
{
    unsigned char magic_number[8];
    int log_version;
    int file_count;
} LOG_INDEX_FILE_HEADER;

typedef struct
{
    LOG_INDEX_FILE_HEADER header;
    LOG_INDEX_FILE_ENTRY *entrys;
} LOG_INDEX_FILE;

#define DEBUG_FUNC_NAME_LEN 64
typedef struct
{
    long nId;
    char szName[DEBUG_FUNC_NAME_LEN];
} DebugFund_IDToName;

static log_info_t s_stLogInfo = {0};
static unsigned long long gstLastLogTime = 0;
static int gstLogLevel = SLOG_LVL_FATAL | SLOG_LVL_ERROR | SLOG_LVL_WARNING | SLOG_LVL_EVENT | SLOG_LVL_TRACE;
static char *pstLogBuf = NULL;

static void Help();
static int slogSetMask(const char *p0, const char *p1);
static void slogOpenFile(const char *path);
static void slogCloseFile();
static void telnetopen();
static void telnetclose();
static void slogOpenCom();
static void slogCloseCom();

static DebugFund_IDToName gDebugFunc2Name[] = {
    {(long)Help, "Help"},
    {(long)slogSetMask, "slog"},
    {(long)slogSetMask, "slogSetMask"},
    {(long)slogOpenCom, "slogOpenCom"},
    {(long)slogCloseCom, "slogCloseCom"},
    {(long)slogOpenFile, "slogOpenFile"},
    {(long)slogCloseFile, "slogCloseFile"},
    {(long)telnetopen, "telneton"},
    {(long)telnetclose, "telnetoff"},
};

static log_info_t *getLogInfo(void)
{
    return &s_stLogInfo;
}

static int anj_mw_log_file_size(const char *path)
{
    if (strlen(path) == 0)
        return -1;

    struct stat mstat;
    memset(&mstat, 0, sizeof(mstat));

    if (lstat(path, &mstat) == -1)
        return -1;

    return (int)mstat.st_size;
}

static void anj_mw_log_file_create(DEBUG_CONFIG *debug_config)
{
    if (strlen(debug_config->filePath) == 0)
    {
        debug_config->filePath[0] = 0;
        return;
    }

    if (!anj_mw_file_exists(debug_config->filePath))
    {
        mkdir(debug_config->filePath, 0777);
    }

    struct tm *t, tbuf;
    time_t tsec = (time_t)time(0);
    t = localtime_r(&tsec, &tbuf);

    memset(debug_config->fileName, 0, sizeof(debug_config->fileName));
    snprintf(debug_config->fileName, sizeof(debug_config->fileName),
             "%s/ipc_%04d%02d%02d-%02d%02d%02d.txt", debug_config->filePath,
             t->tm_year + 1900, t->tm_mon + 1,
             t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

static int anj_mw_log_file_write(char *info)
{
    log_info_t *pstLogInfo = getLogInfo();
    int nFileSize = anj_mw_log_file_size(pstLogInfo->stLogConfig.fileName);
    if (nFileSize < 0)
    {
        anj_mw_log_file_create(&pstLogInfo->stLogConfig);
    }
    else if (nFileSize >= pstLogInfo->stLogConfig.maxFileSize)
    {
        anj_mw_system_with_param("mv %s %s/slog.bk.txt", pstLogInfo->stLogConfig.fileName, pstLogInfo->stLogConfig.filePath);
        anj_mw_log_file_create(&pstLogInfo->stLogConfig);
    }

    if (strlen(pstLogInfo->stLogConfig.fileName) == 0)
        return 0;

    anj_mw_write_file(pstLogInfo->stLogConfig.fileName, 1, info, strlen(info));
    return 0;
}

static int anj_mw_log_info_write(char *info)
{
    int i;
    log_info_t *pstLogInfo = getLogInfo();
    anj_mutex_lock(&pstLogInfo->logMutex);

    DEBUG_INFO_ENTRY *write_ptr = pstLogInfo->stLogRingBuffer.write_ptr;
    strncpy(write_ptr->debug_info, info, sizeof(write_ptr->debug_info) - 1);
    write_ptr->debug_info[sizeof(write_ptr->debug_info) - 1] = 0;

    pstLogInfo->stLogRingBuffer.write_ptr = write_ptr->next;

    for (i = 0; i < MAX_DEBUG_CLIENTS; i++)
    {
        if (pstLogInfo->stLogRingBuffer.read_ptr[i] == pstLogInfo->stLogRingBuffer.write_ptr)
        {
            pstLogInfo->stLogRingBuffer.read_ptr[i] = pstLogInfo->stLogRingBuffer.read_ptr[i]->next;
        }
    }

    anj_mutex_unlock(&pstLogInfo->logMutex);

    return 1;
}

static int anj_mw_log_info_read(int read_index, char *info, int infolen)
{
    log_info_t *pstLogInfo = getLogInfo();
    anj_mutex_lock(&pstLogInfo->logMutex);
    if (pstLogInfo->stLogRingBuffer.read_ptr[read_index] != pstLogInfo->stLogRingBuffer.write_ptr)
    {
        strncpy(info, pstLogInfo->stLogRingBuffer.read_ptr[read_index]->debug_info, infolen);
        pstLogInfo->stLogRingBuffer.read_ptr[read_index] = pstLogInfo->stLogRingBuffer.read_ptr[read_index]->next;
        anj_mutex_unlock(&pstLogInfo->logMutex);
        return 1;
    }
    else
    {
        anj_mutex_unlock(&pstLogInfo->logMutex);
        return -1;
    }
}

int DebugPrint(int level, int way, const char *func, int line, const char *format, ...)
{
    if (pstLogBuf == NULL)
    {
        return -1;
    }

    unsigned long long tCurLogTime = anj_mw_get_cputime_ms(NULL);
    if ((tCurLogTime - gstLastLogTime) > LOGLEVEL_UPDATE_TIME)
    {
        FILE *fp = anj_mw_fopen(LOGLEVEL_UPDATE_FILE, "rb");
        if (fp != NULL)
        {
            anj_mw_fread(fp, (char *)&gstLogLevel, sizeof(int));
            anj_mw_fclose(fp);
        }
        gstLastLogTime = tCurLogTime;
    }

    if (!(level & gstLogLevel))
    {
        return 0;
    }

    char iTimeStr[32] = {0};
    anj_mw_time_getstr(iTimeStr, sizeof(iTimeStr), time(NULL));

    memset(pstLogBuf, 0, LOG_CONTENT_LEN);
    int len = snprintf(pstLogBuf, (LOG_CONTENT_LEN - 1), "[%s|[%s:%d]|LOG-Lv%d]", iTimeStr, func, line, level);

    va_list ap;
    va_start(ap, format);
    vsnprintf(pstLogBuf + len, (LOG_CONTENT_LEN - 1), format, ap);
    va_end(ap);

    if (way & SLOG_WAY_COM)
    {
        // 根据日志级别选择颜色
        const char *color_start = DEBUG_COLOR_NORMAL;
        const char *color_end = DEBUG_COLOR_NORMAL;

        switch (level)
        {
        case SLOG_LVL_TRACE: // printf
            color_start = DEBUG_COLOR_NORMAL;
            break;
        case SLOG_LVL_EVENT: // __EVENT
            color_start = DEBUG_COLOR_BLUE;
            break;
        case SLOG_LVL_WARNING: // __WARN
            color_start = DEBUG_COLOR_YELLOW;
            break;
        case SLOG_LVL_ERROR: // printf
            color_start = DEBUG_COLOR_RED;
            break;
        case SLOG_LVL_FATAL: // __FATAL
            color_start = DEBUG_COLOR_BKRED;
            break;
        default: // 包括 __DBG (level=0)
            color_start = DEBUG_COLOR_NORMAL;
            break;
        }
        fprintf(stderr, "%s%s%s", color_start, pstLogBuf, color_end);
    }
    if (way & SLOG_WAY_WEB)
    {
        anj_mw_log_info_write(pstLogBuf);
    }
    if (way & SLOG_WAY_FILE)
    {
        anj_mw_log_file_write(pstLogBuf);
    }
    return 0;
}

static unsigned int anj_mw_log_func_get(char *pFuncName)
{
    if (pFuncName == NULL || *pFuncName == 0)
        return 0;

    unsigned int iIndex = 0;
    for (iIndex = 0; iIndex < (sizeof(gDebugFunc2Name) / sizeof(DebugFund_IDToName)); iIndex++)
    {
        if (strcasecmp(gDebugFunc2Name[iIndex].szName, pFuncName) == 0)
        {
            //			printf("iIndex %d: func found [%s], address %#x", iIndex, pFuncName, gNvrFunc2Name[iIndex].nId);
            return gDebugFunc2Name[iIndex].nId;
        }
    }

    printf("func count %d, not found [%s]\n", (sizeof(gDebugFunc2Name) / sizeof(DebugFund_IDToName)), pFuncName);

    return 0;
}

static int anj_mw_log_onecmd_proc(const char *strToExec)
{
    if (strToExec == NULL || *strToExec == 0)
        return -1;

    char strSymBolName[64];
    int curStrIdx, tempStrIdxReal, tempStrIdx, paraCount;

    curStrIdx = 0;
    /*find the first not space char*/
    tempStrIdxReal = strFindNoSpace(strToExec + curStrIdx);
    if (tempStrIdxReal == -1)
    { /*all white space, return*/
        return -1;
    }
    curStrIdx += tempStrIdxReal;

    tempStrIdxReal = strlen(strToExec + curStrIdx);
    /*get first string,should be function viriable name*/
    tempStrIdx = strFindSpace(strToExec + curStrIdx);
    if (tempStrIdx != -1)
    {
        tempStrIdxReal = tempStrIdx;
    }

    if (tempStrIdxReal >= 64)
    {
        printf("Err:symbol too long, max is 64,current is %d\n", tempStrIdxReal);
        return -1;
    }
    strncpy(strSymBolName, strToExec + curStrIdx, tempStrIdxReal);
    strSymBolName[tempStrIdxReal] = 0;
    curStrIdx += tempStrIdxReal;

    char szParamString[512] = {0};
    strncpy(szParamString, strToExec + curStrIdx, 512 - 1);

    char szParams[10][64];
    memset(szParams, 0, sizeof(szParams));

    const char s[4] = ",";
    char *token = NULL;
    char *pchStrTmpIn = NULL;

    paraCount = 0;
    token = strtok_r(szParamString, s, &pchStrTmpIn);
    while (token != NULL)
    {
        strncpy(szParams[paraCount], token, 64 - 1);
        string_trim_head(szParams[paraCount]);
        string_trim_tail(szParams[paraCount]);
        token = strtok_r(NULL, s, &pchStrTmpIn);
        paraCount++;
    }

    char szCmd[256] = {0};
    sprintf(szCmd + strlen(szCmd), "%s", strSymBolName);
    int iIndex;
    for (iIndex = 0; iIndex < paraCount; iIndex++)
    {
        sprintf(szCmd + strlen(szCmd), " %s", szParams[iIndex]);
    }

    sprintf(szCmd + strlen(szCmd), "\n");

    printf("%s\n", szCmd);

    unsigned int nFuncAddr = anj_mw_log_func_get(strSymBolName);
    if (nFuncAddr > 0)
    {
        SHELL_EXEC_FUNC pFunc = (SHELL_EXEC_FUNC)nFuncAddr;
        int result = pFunc(szParams[0], szParams[1], szParams[2], szParams[3], szParams[4], szParams[5],
                           szParams[6], szParams[7], szParams[8], szParams[9]);
        printf("return=0x%08x\n", result);

        return 0;
    }
    else
    {
        printf("Cannot found func [%s]\n", strSymBolName);
        return -1;
    }
}

static void anj_mw_log_read_config(DEBUG_CONFIG *debug_config)
{
    debug_config->minLevel = SLOG_LVL_TRACE;
    debug_config->consoleEnable = 1;
    debug_config->fileEnable = 0;
    strcpy(debug_config->filePath, "");
    strcpy(debug_config->fileName, "");
    debug_config->maxFileSize = 512 * 1024; // 保留2个文件
    debug_config->networkEnable = 1;
    debug_config->networkPort = 3000;
    debug_config->networkType = DEBUG_NETWORK_TYPE_UDP;
}

static void anj_mw_log_level_set()
{
    if (anj_mw_file_exists(DEBUG_LOG_LEVEL_ALL))
    {
        s_stLogInfo.logLevel = SLOG_LVL_ALL;
    }
    else if (anj_mw_file_exists(DEBUG_LOG_LEVEL_ERROR))
    {
        s_stLogInfo.logLevel = SLOG_LVL_ERROR;
    }
    else if (anj_mw_file_exists(DEBUG_LOG_LEVEL_WARNING))
    {
        s_stLogInfo.logLevel = SLOG_LVL_WARNING;
    }
    else if (anj_mw_file_exists(DEBUG_LOG_LEVEL_INFO))
    {
        s_stLogInfo.logLevel = SLOG_LVL_TRACE;
    }
    else if (anj_mw_file_exists(DEBUG_LOG_LEVEL_EVENT))
    {
        s_stLogInfo.logLevel = SLOG_LVL_EVENT;
    }
    else if (anj_mw_file_exists(DEBUG_LOG_LEVEL_CRIT))
    {
        s_stLogInfo.logLevel = SLOG_LVL_FATAL;
    }
    else
    {
        s_stLogInfo.logLevel = SLOG_LVL_FATAL | SLOG_LVL_ERROR | SLOG_LVL_WARNING | SLOG_LVL_EVENT | SLOG_LVL_TRACE;
    }
}

static void anj_mw_log_info_init(DEBUG_INFO_RING_BUFFER *ringBuffer)
{
    int i = 0;
    for (i = 0; i < (MAX_DEBUG_INFO_BUFFER_COUNT - 1); i++)
    {
        strncpy(ringBuffer->list[i].debug_info, "", sizeof(ringBuffer->list[i].debug_info));
        ringBuffer->list[i].next = &(ringBuffer->list[i + 1]);
    }

    strncpy(ringBuffer->list[MAX_DEBUG_INFO_BUFFER_COUNT - 1].debug_info, "", sizeof(ringBuffer->list[MAX_DEBUG_INFO_BUFFER_COUNT - 1].debug_info));
    ringBuffer->list[MAX_DEBUG_INFO_BUFFER_COUNT - 1].next = &(ringBuffer->list[0]);

    ringBuffer->head = &(ringBuffer->list[0]);
    ringBuffer->write_ptr = &(ringBuffer->list[0]);

    for (i = 0; i < MAX_DEBUG_CLIENTS; i++)
    {
        ringBuffer->read_ptr[i] = &(ringBuffer->list[0]);
    }
}

static int anj_mw_log_thread(void *ctx, int *bStart)
{
    int i = 0;
    int sockfd = -1;
    struct sockaddr_in server_sockaddr, client_sockaddr;
    fd_set readfd;
    fd_set writefd;
    int client_fd, max_fd;
    int sin_size;
    int ret;

    char send_buf[1024];
    char recv_buf[1024];
    int send_len;
    (void)ctx;
    log_info_t *pstLogInfo = getLogInfo();

    while (bStart && *bStart)
    {
        if (0 == anj_mw_file_exists("/tmp/flag.open.slog.tcp") &&
            0 == anj_mw_file_exists("/mnt/nand/flag.open.slog.tcp"))
        {
            usleep(1000 * 1000);
            continue;
        }

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            printf("anj_mw_log_thread: create sockfd failed!\n");
            sleep(1);
            continue;
        }

        server_sockaddr.sin_family = AF_INET;
        server_sockaddr.sin_port = htons(NETWORK_DEBUG_PORT);
        server_sockaddr.sin_addr.s_addr = INADDR_ANY;
        bzero(&(server_sockaddr.sin_zero), 8);

        int booltrue = 1;
        if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&booltrue, sizeof(booltrue))) == -1)
        {
            printf("anj_mw_log_thread: setsockopt SO_REUSEADDR failed, error=%s\n", strerror(errno));
            close(sockfd);
            sleep(1);
            continue;
        }

        int keepAlive = 1;     // 设定KeepAlive
        int keepIdle = 10;     // 开始首次KeepAlive探测前的TCP空闭时间
        int keepInterval = 10; // 两次KeepAlive探测间的时间间隔
        int keepCount = 3;     // 判定断开前的KeepAlive探测次数
        int optval = 0;
        int int_len = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive)) == -1)
        {
            printf("debug_server_thread: setsockopt SO_REUSEADDR failed, error=%s\n", strerror(errno));

            close(sockfd);
            sleep(1);
            continue;
        }

        if (getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, (socklen_t *)&int_len) == -1)
        {
            printf("Error when getting socket option.\n");
        }
        printf("SO_KEEPALIVE=%d\n", optval);

        if (setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle)) == -1)
        {
            printf("debug_server_thread: setsockopt SO_KEEPIDLE failed, error=%s\n", strerror(errno));

            close(sockfd);
            sleep(1);
            continue;
        }

        if (setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) == -1)
        {
            printf("debug_server_thread: setsockopt SO_KEEPINTVL failed, error=%s\n", strerror(errno));

            close(sockfd);
            sleep(1);
            continue;
        }

        if (setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)) == -1)
        {
            printf("debug_server_thread: setsockopt SO_KEEPCNT failed, error=%s\n", strerror(errno));

            close(sockfd);
            sleep(1);
            continue;
        }

        if (bind(sockfd, (struct sockaddr *)&server_sockaddr, sizeof(struct sockaddr)) == -1)
        {
            printf("anj_mw_log_thread: socket bind failed, error=%s\n", strerror(errno));
            close(sockfd);
            sleep(1);
            continue;
        }

        printf("anj_mw_log_thread: bind success!\n");
        if (listen(sockfd, MAX_DEBUG_CLIENTS) == -1)
        {
            printf("anj_mw_log_thread: socket listen failed, error=%s\n", strerror(errno));
            close(sockfd);
            sleep(1);
            continue;
        }
        for (i = 0; i < MAX_DEBUG_CLIENTS; i++)
        {
            pstLogInfo->stLogClientList[i].fd = -1;
            pstLogInfo->stLogClientList[i].sendinfo = 0;
            pstLogInfo->stLogClientList[i].recvZeroNum = 0;
        }

        printf("anj_mw_log_thread: listening...\n");
        while (*bStart)
        {
            struct timeval wait_time;
            wait_time.tv_sec = 2;
            wait_time.tv_usec = 0;

            FD_ZERO(&readfd);
            FD_ZERO(&writefd);

            FD_SET(sockfd, &readfd);
            max_fd = sockfd;

            for (i = 0; i < MAX_DEBUG_CLIENTS; i++)
            {
                if (pstLogInfo->stLogClientList[i].fd > 0)
                {
                    FD_SET(pstLogInfo->stLogClientList[i].fd, &readfd);
                    FD_SET(pstLogInfo->stLogClientList[i].fd, &writefd);

                    if (pstLogInfo->stLogClientList[i].fd > max_fd)
                        max_fd = pstLogInfo->stLogClientList[i].fd;
                }
            }

            ret = select(max_fd + 1, &readfd, &writefd, NULL, &wait_time);
            if (ret < 0)
            {
                if (errno != EINTR)
                {
                    printf("debug_server_thread: select failed, err=%s\n", strerror(errno));
                    break;
                }
                else
                {
                    sleep(1);
                    continue;
                }
            }
            else if (ret == 0) // time out
            {
                continue;
            }

            if (FD_ISSET(sockfd, &readfd))
            {
                sin_size = sizeof(struct sockaddr_in);
                if ((client_fd = accept(sockfd, (struct sockaddr *)&client_sockaddr, (socklen_t *)&sin_size)) < 0)
                {
                    printf("anj_mw_log_thread: accept failed, err = %s\n", strerror(errno));
                    continue;
                }
                else
                {
                    for (i = 0; i < MAX_DEBUG_CLIENTS; i++)
                    {
                        if (pstLogInfo->stLogClientList[i].fd < 0)
                        {
                            pstLogInfo->stLogClientList[i].fd = client_fd;
                            pstLogInfo->stLogClientList[i].sendinfo = 1;
                            memcpy(&(pstLogInfo->stLogClientList[i].client_sockaddr), &client_sockaddr, sizeof(struct sockaddr_in));
                            printf("anj_mw_log_thread: add one client, fd = %d \n", client_fd);
                            break;
                        }
                    }
                    if (i == MAX_DEBUG_CLIENTS)
                    {
                        close(client_fd);
                    }
                    else
                    {
                        int buflen = 4096 * 8;
                        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, (void *)&buflen, sizeof(int)) == -1)
                        {
                            printf("setsockopt SO_RCVBUF failed, error=%s\n", strerror(errno));
                        }

                        if (setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, (void *)&buflen, sizeof(int)) == -1)
                        {
                            printf("setsockopt SO_SNDBUF failed, error=%s\n", strerror(errno));
                        }
                    }
                }
            }

            for (i = 0; i < MAX_DEBUG_CLIENTS; i++)
            {
                if (pstLogInfo->stLogClientList[i].fd > 0)
                {
                    if (FD_ISSET(pstLogInfo->stLogClientList[i].fd, &readfd))
                    {
                        // read data from client
                        ret = safe_recv(pstLogInfo->stLogClientList[i].fd, recv_buf, 1024 - 1, 0);
                        if (ret < 0)
                        {
                            printf("anj_mw_log_thread: recv failed,err=%s\n", strerror(errno));
                            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                            {
                            }
                            else
                            {
                                close(pstLogInfo->stLogClientList[i].fd);

                                pstLogInfo->stLogClientList[i].fd = -1;
                                pstLogInfo->stLogClientList[i].recvZeroNum = 0;
                            }
                            continue;
                        }
                        else if (ret == 0)
                        {
                            /* 如果连续10次接受到可读并且读取的长度为0表现连接已经断 */
                            pstLogInfo->stLogClientList[i].recvZeroNum++;

                            if (10 == pstLogInfo->stLogClientList[i].recvZeroNum)
                            {
                                close(pstLogInfo->stLogClientList[i].fd);
                                pstLogInfo->stLogClientList[i].fd = -1;
                                pstLogInfo->stLogClientList[i].recvZeroNum = 0;
                                break;
                            }
                        }
                        else
                        {
                            pstLogInfo->stLogClientList[i].recvZeroNum = 0;

                            recv_buf[ret] = '\0';

                            char readStr[512] = {0};
                            char *p = recv_buf;
                            int iIndex = 0;
                            for (; p < recv_buf + ret; p++)
                            {
                                if (isprint(*p))
                                {
                                    if (iIndex < 512 - 1)
                                        readStr[iIndex++] = *p;
                                }
                                else
                                {
                                    if (strlen(readStr) == 0)
                                    {
                                        continue;
                                    }
                                    else
                                    {
                                        printf("cmd: %s\n", readStr);
                                        anj_mw_log_cmd_proc(readStr, strlen(readStr));
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (FD_ISSET(pstLogInfo->stLogClientList[i].fd, &writefd) && pstLogInfo->stLogClientList[i].sendinfo)
                    {
                        if (anj_mw_log_info_read(i, send_buf, sizeof(send_buf)) > 0)
                        {
                            send_len = strlen(send_buf);
                            ret = safe_send(pstLogInfo->stLogClientList[i].fd, send_buf, send_len, 0);
                            if (ret != send_len)
                            {
                                printf("anj_mw_log_thread: send failed,err=%s\n", strerror(errno));
                                close(pstLogInfo->stLogClientList[i].fd);
                                pstLogInfo->stLogClientList[i].fd = -1;
                                pstLogInfo->stLogClientList[i].recvZeroNum = 0;
                            }
                        }
                    }
                }
            }

            usleep(20 * 1000);
        }

        for (i = 0; i < MAX_DEBUG_CLIENTS; i++)
        {
            if (pstLogInfo->stLogClientList[i].fd > 0)
            {
                close(pstLogInfo->stLogClientList[i].fd);
                pstLogInfo->stLogClientList[i].fd = -1;
                pstLogInfo->stLogClientList[i].recvZeroNum = 0;
            }
        }

        close(sockfd);
        sockfd = -1;
        printf("anj_mw_log_thread: loop break, start loop again.\n");
        sleep(1);
    }

    if (sockfd > 0)
        close(sockfd);

    return 0;
}

static void Help()
{
    printf("=========================================================\n");
    unsigned int iIndex = 0;
    for (iIndex = 0; iIndex < (sizeof(gDebugFunc2Name) / sizeof(DebugFund_IDToName)); iIndex++)
    {
        if (gDebugFunc2Name[iIndex].nId == 0 || strlen(gDebugFunc2Name[iIndex].szName) == 0)
            continue;

        printf("%-20s:%ld\n", gDebugFunc2Name[iIndex].szName, gDebugFunc2Name[iIndex].nId);
    }
    printf("=========================================================\n");
}

static int slogSetMask(const char *p0, const char *p1)
{
    char *strToLongReturn;
    // unsigned int mod = strtoul(p0, &strToLongReturn, 10);
    unsigned int levelMask = strtoul(p1, &strToLongReturn, 10);

    s_stLogInfo.logLevel = levelMask;
    return 0;
}

static void slogOpenFile(const char *path)
{
    if (NULL == path || *path == 0)
    {
        return;
    }

    log_info_t *pstLogInfo = getLogInfo();
    if (!anj_mw_file_exists(path))
    {
        printf("not exist path: %s\n", path);
        return;
    }

    DEBUG_CONFIG *debug_config = &pstLogInfo->stLogConfig;
    debug_config->fileEnable = 1;
    memset(debug_config->filePath, 0, sizeof(debug_config->filePath));
    snprintf(debug_config->filePath, sizeof(debug_config->filePath), "%s/logipc", path);

    if (strlen(debug_config->fileName) > 0 && strstr(debug_config->fileName, debug_config->filePath) != NULL)
    {
        printf("exist debug log: %s\n", debug_config->fileName);
    }
    else
    {
        anj_mw_log_file_create(debug_config);
        printf("new debug log: %s\n", debug_config->fileName);
    }
}

static void slogCloseFile()
{
    log_info_t *pstLogInfo = getLogInfo();
    DEBUG_CONFIG *debug_config = &pstLogInfo->stLogConfig;
    debug_config->fileEnable = 0;
    strcpy(debug_config->filePath, "");
    strcpy(debug_config->fileName, "");
}

static void telnetopen()
{
    anj_mw_system("telnetd &");
}

static void telnetclose()
{
    anj_mw_system("killall telnetd");
}

static void slogOpenCom()
{
    log_info_t *pstLogInfo = getLogInfo();
    pstLogInfo->stLogConfig.consoleEnable = 1;
    printf("enable console output!!!\n");
    freopen("/dev/console", "w", stdout);
    freopen("/dev/console", "w", stderr);
    freopen("/dev/console", "r", stdin);
}

static void slogCloseCom()
{
    log_info_t *pstLogInfo = getLogInfo();
    pstLogInfo->stLogConfig.consoleEnable = 0;
    printf("disable console output!!!\n");
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    freopen("/dev/console", "r", stdin);
}

int anj_mw_log_init()
{
    pstLogBuf = (char *)anj_mw_malloc(LOG_CONTENT_LEN);
    if (NULL == pstLogBuf)
    {
        return -1;
    }

    anj_mw_log_info_init(&s_stLogInfo.stLogRingBuffer);
    anj_mw_log_level_set();
    anj_mw_log_read_config(&s_stLogInfo.stLogConfig);
    s_stLogInfo.logServerThread.bAutoDestroy = 1;
    strncpy(s_stLogInfo.logServerThread.iThreadName, "log_thread", sizeof(s_stLogInfo.logServerThread.iThreadName) - 1);
    s_stLogInfo.logServerThread.iThreadjob.ctx = &s_stLogInfo;
    s_stLogInfo.logServerThread.iThreadjob.func = anj_mw_log_thread;
    anj_thread_task_create(&s_stLogInfo.logServerThread);

    return 0;
}

int anj_mw_log_uninit()
{
    if (pstLogBuf)
    {
        anj_mw_free(pstLogBuf);
    }
    return 0;
}

LOG_INDEX_FILE_ENTRY *anj_mw_log_filelist_get(struct tm time_start, struct tm time_end, int *count)
{
    LOG_INDEX_FILE_ENTRY *entrys_ret = NULL;
    int entrys_count = 0;

    char file_start[32] = {0};
    char file_end[32] = {0};

    sprintf(file_start, "%04d%02d%02d.log",
            time_start.tm_year + 1900,
            time_start.tm_mon + 1,
            time_start.tm_mday);

    sprintf(file_end, "%04d%02d%02d.log",
            time_end.tm_year + 1900,
            time_end.tm_mon + 1,
            time_end.tm_mday);

    printf("searching log files > %s and < %s\n", file_start, file_end);

    DIR *dir;
    struct dirent *s_dir;
    char *log_path = "/mnt/nand";

    // 查找文件个数
    dir = opendir(log_path);
    if (dir == NULL)
    {
        *count = 0;
        return NULL;
    }
    else
    {
        printf("searching dir: %s...\n", log_path);

        while (1)
        {
            s_dir = readdir(dir);
            if (s_dir == NULL)
                break;
            else
            {
                printf("name=%s, type=%d\n", s_dir->d_name, s_dir->d_type);

                if ((s_dir->d_type == 8) && (strlen(s_dir->d_name) == strlen("20090101.log")))
                {
                    if (strstr(s_dir->d_name, ".log") == NULL)
                        continue;

                    if (strcmp(file_start, s_dir->d_name) > 0)
                        continue;

                    if (strcmp(file_end, s_dir->d_name) < 0)
                        continue;

                    entrys_count++;
                }
            }
        }

        closedir(dir);
    }

    if (entrys_count > 0)
    {
        entrys_ret = (LOG_INDEX_FILE_ENTRY *)malloc(entrys_count * sizeof(LOG_INDEX_FILE_ENTRY));
        if (entrys_ret == NULL)
        {
            printf("memory not enough when malloc entrys_ret!\n");
            *count = 0;
            return NULL;
        }

        dir = opendir(log_path);
        if (dir == NULL)
        {
            free(entrys_ret);

            *count = 0;
            return NULL;
        }
        else
        {
            printf("searching dir: %s...\n", log_path);

            int ret_count = 0;
            while (1)
            {
                s_dir = readdir(dir);
                if (s_dir == NULL)
                    break;
                else
                {
                    printf("name=%s, type=%d\n", s_dir->d_name, s_dir->d_type);

                    if ((s_dir->d_type == 8) && (strlen(s_dir->d_name) == strlen("20090101.log")))
                    {
                        if (strstr(s_dir->d_name, ".log") == NULL)
                            continue;

                        if (strcmp(file_start, s_dir->d_name) > 0)
                            continue;

                        if (strcmp(file_end, s_dir->d_name) < 0)
                            continue;

                        // fill the entry body with file name
                        snprintf(entrys_ret[ret_count].log_filename, sizeof(entrys_ret[ret_count].log_filename),
                                 "%s/%s", log_path, s_dir->d_name);
                        // entrys_ret[ret_count].oldest_offset=0;

                        struct stat st;
                        stat(entrys_ret[ret_count].log_filename, &st);
                        entrys_ret[ret_count].file_length = st.st_size;

                        ret_count++;
                    }
                }
            }

            closedir(dir);

            *count = ret_count;
            return entrys_ret;
        }
    }
    else
    {
        *count = 0;
        return NULL;
    }
}

void anj_mw_log_cmd_proc(const char *pCmd, int nLength)
{
    if (pCmd == NULL || *pCmd == 0)
        return;

    char szInputString[512] = {0};
    snprintf(szInputString, sizeof(szInputString), "%s", pCmd);

    const char s[4] = ";";
    char *token = NULL;
    char *pchStrTmpIn = NULL;

    token = strtok_r(szInputString, s, &pchStrTmpIn);
    while (token != NULL)
    {
        __INFO("cmd: %s\n", token);
        anj_mw_log_onecmd_proc(token);
        token = strtok_r(NULL, s, &pchStrTmpIn);
    }
}