#include "file_sender.h"
#include "file_receiver.h"
#include "anj_pri.h"
#include "cmd_def.h"
#include "anj_sysmng.h"
#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "record_log.h"

#define FILETRANS_READ_BUFFER
#define FILETRANS_MAX_BUFFER_LENGTH (300 * 1024)

typedef struct
{
    int up_or_down;
    char local_file[256];
    char remote_file[256];
    long file_type;
    long server_port;
    int lognum;
} FILE_TRANSPORT_PARAM;

static file_sender_t *pFileSender = NULL;
static anj_thread_s stTransportThread = {0};

static int file_sender_transport_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    int sockfd = -1;
    FILE_TRANSPORT_PARAM *thread_param = (FILE_TRANSPORT_PARAM *)ctx;
    ANJ_CHK(thread_param != NULL, -1, "input Invalid");

    __ERR("file_sender_transport_thread start...\n");

    char *filename = thread_param->local_file;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();

    __ERR("up_down=%d, type=%ld, local= %s, remote= %s, port=%ld, lognum=%d\n",
          thread_param->up_or_down,
          thread_param->file_type,
          thread_param->local_file,
          thread_param->remote_file,
          thread_param->server_port,
          thread_param->lognum);

    struct sockaddr_in server_sockaddr, client_sockaddr;
    fd_set readfd;
    fd_set writefd;
    int client_fd = -1;
    int max_fd = -1;
    int timeout_count;
    int booltrue;
    int sin_size;
    struct timeval wait_time;
    int ret;

    int file_fd = -1;
    unsigned long flen = 0;
    unsigned long offset = 0;
    int send_len;
    int recv_len;
    int write_len;
    int read_len;

    unsigned long flag = 0x31589518;

    int process_ok = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    ANJ_CHK(sockfd > 0, -1, "create sockfd failed!\n");

    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(thread_param->server_port);
    server_sockaddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server_sockaddr.sin_zero), 8);

    booltrue = 1;
    if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&booltrue, sizeof(booltrue))) == -1)
    {
        __ERR("setsockopt SO_REUSEADDR failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 10; // wait 10 seconds before close socket
    if ((setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (void *)&so_linger, sizeof(struct linger))) == -1)
    {
        __ERR("setsockopt SO_LINGER failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    int keepAlive = 1;
    int keepIdle = 5;
    int keepInterval = 5;
    int keepCount = 3;

    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepAlive, sizeof(keepAlive)) == -1)
    {
        __ERR("setsockopt SO_REUSEADDR failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    if (setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle)) == -1)
    {
        __ERR("setsockopt SO_KEEPIDLE failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    if (setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) == -1)
    {
        __ERR("setsockopt SO_KEEPINTVL failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    if (setsockopt(sockfd, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)) == -1)
    {
        __ERR("setsockopt SO_KEEPCNT failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    int len = 4096;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void *)&len, sizeof(int)) == -1)
    {
        __ERR("setsockopt SO_RCVBUF failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (void *)&len, sizeof(int)) == -1)
    {
        __ERR("setsockopt SO_SNDBUF failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    if (bind(sockfd, (struct sockaddr *)&server_sockaddr, sizeof(struct sockaddr)) == -1)
    {
        __ERR("socket bind failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    __ERR("bind success!\n");
    if (listen(sockfd, 1) == -1)
    {
        __ERR(" socket listen failed, error=%s\n", strerror(errno));
        goto endFunc;
    }

    __ERR("listening...\n");

    while (bStart && *bStart)
    {
        wait_time.tv_sec = 10;
        wait_time.tv_usec = 0;

        FD_ZERO(&readfd);
        FD_ZERO(&writefd);

        FD_SET(sockfd, &readfd);
        max_fd = sockfd;

        ret = select(max_fd + 1, &readfd, &writefd, NULL, &wait_time);
        if (ret < 0)
        {
            if (errno != EINTR)
            {
                __ERR(" select failed, err=%s\n", strerror(errno));
                break;
            }
            else
            {
                sleep(1);
                __ERR("errno == EINTR 1\n");
                continue;
            }
        }
        else if (ret == 0) // time out
        {
            __ERR("wait client connect time out.\n");
            break;
        }

        if (FD_ISSET(sockfd, &readfd))
        {
            sin_size = sizeof(struct sockaddr_in);
            if ((client_fd = accept(sockfd, (struct sockaddr *)&client_sockaddr, (socklen_t *)&sin_size)) < 0)
            {
                __ERR(" accept failed, err = %s\n", strerror(errno));
                break;
            }
        }

        __ERR("accept ok.\n");

        struct linger so_linger;
        so_linger.l_onoff = 1;
        so_linger.l_linger = 10; // wait 10 seconds before close socket
        if ((setsockopt(client_fd, SOL_SOCKET, SO_LINGER, (void *)&so_linger, sizeof(struct linger))) == -1)
        {
            __ERR("setsockopt SO_LINGER failed, error=%s\n", strerror(errno));
            close(client_fd);
            break;
        }

        __ERR("set client_fd linger option ok!\n");

        if (thread_param->up_or_down)
        {
            __ERR("begin upload...\n");

            if ((thread_param->file_type != UPLOAD_CONFIG_FILE_TYPE) &&
                (thread_param->file_type != UPLOAD_FIRMWARE_FILE_TYPE))
            {
                __ERR("invalid upload file type: %ld\n", thread_param->file_type);
                close(client_fd);
                break;
            }

            file_fd = open(filename, O_WRONLY | O_CREAT, S_IRWXU);
            if (file_fd < 0)
            {
                __ERR("open file %s for write failed, err=%s\n", filename, strerror(errno));
                close(client_fd);
                break;
            }

            // recv flag
            do
            {
                recv_len = safe_recv(client_fd, (char *)&flag, sizeof(unsigned long), 0);
            } while (recv_len <= 0 && (errno == EAGAIN || errno == EINTR));

            if (recv_len <= 0)
            {
                __ERR("recv flag failed, err=%s\n", strerror(errno));
                close(client_fd);
                close(file_fd);
                break;
            }

            if (flag != 0x31589518)
            {
                __ERR("recv flag = %08lx != 0x31589158\n", flag);
                close(client_fd);
                close(file_fd);
                break;
            }

            // recv length
            do
            {
                recv_len = safe_recv(client_fd, (char *)&flen, sizeof(int), 0);
            } while (recv_len <= 0 && (errno == EAGAIN || errno == EINTR));

            if (recv_len <= 0)
            {
                __ERR("recv len failed, err=%s\n", strerror(errno));
                close(client_fd);
                close(file_fd);
                break;
            }

            __INFO("recv len=%ld\n", flen);

            if (thread_param->file_type == UPLOAD_CONFIG_FILE_TYPE)
            {
                if (len > (1024 * 1024))
                {
                    __ERR("config file length is too large, max len=%d\n", 1024 * 1024);
                    close(client_fd);
                    close(file_fd);
                    break;
                }
            }
            else if (thread_param->file_type == UPLOAD_FIRMWARE_FILE_TYPE)
            {
                if (len > (15 * 1024 * 1024))
                {
                    __ERR("firmware file length is too large, max len=%d\n", 15 * 1024 * 1024);
                    close(client_fd);
                    close(file_fd);
                    break;
                }
            }

            // recv data
            char *recv_buf = anj_mw_malloc(4096);
            if (recv_buf == NULL)
            {
                __ERR("recv_buf malloc failed!\n");
                close(client_fd);
                close(file_fd);
                break;
            }
            offset = 0;
            timeout_count = 0;

            while ((offset < flen) && *bStart)
            {
                ///////////////////////////////////////////////////////////
                // check readable
                ///////////////////////////////////////////////////////////
                wait_time.tv_sec = 0;
                wait_time.tv_usec = 100 * 1000;

                FD_ZERO(&readfd);
                FD_SET(client_fd, &readfd);
                max_fd = client_fd;

                ret = select(max_fd + 1, &readfd, NULL, NULL, &wait_time);
                if (ret < 0)
                {
                    if (errno != EINTR)
                    {
                        __ERR("select failed before send data, err=%s\n", strerror(errno));
                        break;
                    }
                    else
                    {
                        sleep(1);
                        __ERR("errno == EINTR 2\n");
                        continue;
                    }
                }
                else if (ret == 0) // time out
                {
                    __INFO("wait client write time out\n");

                    timeout_count++;
                    if (timeout_count > 100) // 30)
                    {
                        __ERR("wait recv time out, exit loop\n");
                        break;
                    }
                    else
                    {
                        usleep(100 * 1000);
                        continue;
                    }
                }

                if (!FD_ISSET(client_fd, &readfd))
                {
                    usleep(100 * 1000);
                    continue;
                }
                /////////////////////////////////////////////////////////
                timeout_count = 0;

                int buflen = 0;
                if (flen - offset > 4096)
                    buflen = 4096;
                else
                    buflen = flen - offset;
                do
                {
                    recv_len = safe_recv(client_fd, recv_buf, buflen, 0);
                } while (recv_len <= 0 && (errno == EAGAIN || errno == EINTR));

                if (recv_len <= 0)
                {
                    __ERR("recv data failed, err=%s\n", strerror(errno));
                    break;
                }

                write_len = safe_write(file_fd, recv_buf, recv_len);
                if (write_len != recv_len)
                {
                    __ERR("write file failed, err=%s\n", strerror(errno));
                    break;
                }

                offset += recv_len;

                //__INFO("recv in: %d/%d\n", offset, flen);
            }

            close(file_fd);
            anj_mw_free(recv_buf);

            if (offset < flen)
            {
                process_ok = 0;
                remove(filename);
            }
            else
            {
                process_ok = 1;

                // send end message, added by xxx 20091126
                char *end_message = (char *)"FILE TRANSPORT OK!";
                do
                {
                    iRet = safe_send(client_fd, end_message, strlen(end_message), 0);
                } while (iRet <= 0 && (errno == EAGAIN || errno == EINTR));

                if (iRet == (int)strlen(end_message))
                {
                    __ERR("send end message ok!\n");
                }
                else
                {
                    __ERR("send end message failed, ret=%d, err=%s\n",
                          iRet, strerror(errno));
                    sleep(5);
                }
            }

            close(client_fd);
            break;
        }
        else // download
        {
            __ERR("begin download...\n");

            file_fd = open(filename, O_RDONLY);
            if (file_fd < 0)
            {
                __ERR("open file %s for read failed, err=%s\n", filename, strerror(errno));
                break;
            }
            else
            {
                __ERR("open file ok, fd=%d...\n", file_fd);

                flen = lseek(file_fd, 0, SEEK_END);

                __ERR("file %s len = %lu\n", filename, flen);

                lseek(file_fd, 0, SEEK_SET);
            }

            // send flag
            do
            {
                send_len = safe_send(client_fd, (char *)&flag, sizeof(unsigned long), 0);
            } while (send_len <= 0 && (errno == EAGAIN || errno == EINTR));

            if (send_len <= 0)
            {
                __ERR("send flag failed, err=%s\n", strerror(errno));
                close(file_fd);
                close(client_fd);
                break;
            }

            //__ERR("send flag ok.\n");

            // send length
            do
            {
                send_len = safe_send(client_fd, (char *)&flen, sizeof(int), 0);
            } while (send_len <= 0 && (errno == EAGAIN || errno == EINTR));

            if (send_len <= 0)
            {
                __ERR("send length failed, err=%s\n", strerror(errno));
                close(file_fd);
                close(client_fd);
                break;
            }

            //__ERR("send length ok.\n");

            // send data
            char *send_buf = anj_mw_malloc(4096);
            if (send_buf == NULL)
            {
                __ERR("send_buf malloc failed!\n");
                close(client_fd);
                close(file_fd);
                break;
            }

            offset = 0;
            timeout_count = 0;

            while ((offset < flen) && !*bStart)
            {
                ///////////////////////////////////////////////////////////
                // check writable
                ///////////////////////////////////////////////////////////
                wait_time.tv_sec = 0;
                wait_time.tv_usec = 100 * 1000;

                FD_ZERO(&writefd);
                FD_SET(client_fd, &writefd);
                max_fd = client_fd;

                ret = select(max_fd + 1, NULL, &writefd, NULL, &wait_time);
                if (ret < 0)
                {
                    if (errno != EINTR)
                    {
                        __ERR("select failed before send data, err=%s\n", strerror(errno));
                        break;
                    }
                    else
                    {
                        sleep(1);
                        __ERR("errno == EINTR 2\n");
                        continue;
                    }
                }
                else if (ret == 0) // time out
                {
                    __INFO("wait client read time out\n");

                    timeout_count++;
                    if (timeout_count > 100) // 30)
                    {
                        __ERR("wait send time out, exit loop\n");
                        break;
                    }
                    else
                    {
                        usleep(100 * 1000);
                        continue;
                    }
                }

                if (!FD_ISSET(client_fd, &writefd))
                {
                    usleep(100 * 1000);
                    continue;
                }

                timeout_count = 0;
                /////////////////////////////////////////////////////////
                read_len = safe_read(file_fd, send_buf, 4096);
                if (read_len <= 0)
                {
                    __ERR("read file failed, err=%s\n", strerror(errno));
                    break;
                }
                else
                {
                    do
                    {
                        send_len = safe_send(client_fd, send_buf, read_len, 0);
                    } while (send_len <= 0 && (errno == EAGAIN || errno == EINTR));

                    if (send_len != read_len)
                    {
                        __ERR("send data failed, err=%s\n", strerror(errno));
                        break;
                    }
                }

                offset += read_len;

                //__ERR("send out: %d/%d\n", offset, flen);

                // usleep(1000);
            }

            close(file_fd);

            anj_mw_free(send_buf);

            if (offset < flen)
            {
                process_ok = 0;
            }
            else
            {
                char end_message[256] = {0};
                do
                {
                    iRet = safe_recv(client_fd, end_message, 256, 0);
                } while (iRet <= 0 && (errno == EAGAIN || errno == EINTR));

                if (iRet > 0)
                {
                    end_message[iRet] = 0;
                    if (strcmp(end_message, "FILE TRANSPORT OK!") == 0)
                    {
                        __ERR("recv file transport end message ok!\n");
                        process_ok = 1;
                    }
                    else
                    {
                        __ERR("recv bad file transport end message: %s\n", end_message);
                        process_ok = 0;
                    }
                }
                else
                {
                    __ERR("recv file transport end message failed, ret=%d, err=%s\n",
                          iRet, strerror(errno));
                    process_ok = 0;
                }
            }

            close(client_fd);
            break;
        }
    }

    __ERR("file_sender_transport_thread exit thread loop, flen=%lu, offset=%ld, process_ok=%d.\n",
          flen, offset, process_ok);

    sleep(3);

    close(sockfd);
    sockfd = -1;

    if (process_ok && thread_param->up_or_down)
    {
        if (thread_param->file_type == UPLOAD_CONFIG_FILE_TYPE)
        {
            __ERR("upload file: %s ok, update config.\n", thread_param->local_file);
            ret = anj_sysmng_config_update(thread_param->local_file);
        }
        else if (thread_param->file_type == UPLOAD_FIRMWARE_FILE_TYPE)
        {
            __ERR("upload file: %s ok, update firmware.\n", thread_param->local_file);

            APPBIN_UPDATE_DATA updateData = {0};
            snprintf(updateData.filePath, sizeof(updateData.filePath), "%s", thread_param->local_file);
            {
                const char *base = thread_param->remote_file;
                const char *slash = strrchr(base, '/');
                const char *bslash = strrchr(base, '\\');
                if (bslash && (!slash || bslash > slash))
                    slash = bslash;
                if (slash)
                    base = slash + 1;
                snprintf(updateData.origName, sizeof(updateData.origName), "%s", base);
            }
            updateData.nPhyAddr = 0;
            updateData.nFileLen = 0;
            __WARN("file_sender firmware upload ok, app_update path=%s orig=%s\n",
                   thread_param->local_file, updateData.origName);
            ret = anj_sysmng_app_update(&updateData);
        }

        __ERR("update finished, ret=%d\n", ret);

        FRAME_ENTRY respEntry;
        respEntry.pFrame = anj_mw_malloc(1024);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while handle file transport!!!\n");
        }
        else
        {
            if (thread_param->file_type == UPLOAD_CONFIG_FILE_TYPE)
            {
                snprintf(respEntry.pFrame, 1024,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                         "/>\n"
                         "<MESSAGE_BODY/>\n"
                         "</%s>",
                         anj_pri_xml_name_get(thread_param->lognum),
                         CMD_CONFIG_UPDATE, ret,
                         anj_pri_xml_name_get(thread_param->lognum));
            }
            else if (thread_param->file_type == UPLOAD_FIRMWARE_FILE_TYPE)
            {
                snprintf(respEntry.pFrame, 1024,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                         "/>\n"
                         "<MESSAGE_BODY/>\n"
                         "</%s>",
                         anj_pri_xml_name_get(thread_param->lognum),
                         CMD_APPBIN_LOCAL_UPDATE, ret,
                         anj_pri_xml_name_get(thread_param->lognum));
            }

            respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
            respEntry.nFlag = 1;
            frame_mgr_push(&pstPriInfo->stUserInfo[thread_param->lognum].bufMgr, &respEntry);

            //__ERR("after make respones msg.\n");
        }
        // added end

        if (ret == 0)
        {
            __WARN("file_sender upload config ok, reboot\n");
            __RECORD_LOG_INFO("file_sender upload config ok, reboot\n");
            anj_sysmng_reboot();
        }
    }

endFunc:
    __ERR("exited main loop\n");
    if (thread_param)
    {
        anj_mw_free(thread_param);
    }
    if (sockfd)
    {
        close(sockfd);
    }
    return iRet;
}

static int file_sender_thread(void *ctx, int *bStart)
{
    int iRet = 0;
    FILE *fp = NULL;
    file_sender_t *pFileSender = (file_sender_t *)ctx;
    ANJ_CHK(pFileSender != NULL, -1, "input Invalid");

    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    fp = anj_mw_fopen(pFileSender->filename, "rb");
    if (!fp)
    {
        __ERR("open %s failed!\n", pFileSender->filename);
        pFileSender->sendfinish = 1;
        iRet = -1;
        goto endFunc;
    }
    if (pFileSender->startpos)
    {
        anj_mw_fseek(fp, pFileSender->startpos);
    }

    int delay = 1000 / pFileSender->interval;
    unsigned long sendlen = 0;
    while (bStart && *bStart)
    {
        if (frame_mgr_count(&pstPriInfo->stUserInfo[pFileSender->lognum].bufMgr) > 1)
        {
            usleep(10 * 1000);
            continue;
        }
        FRAME_ENTRY respEntry = {0};
        respEntry.pFrame = anj_mw_malloc(1024 + pFileSender->framesize);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while prepare file data!!!\n");
        }
        else
        {
            char *filebuf = anj_mw_malloc(pFileSender->framesize);
            int datalen = pFileSender->framesize;
            if (filebuf == NULL)
            {
                __ERR("filebuf == NULL while prepare file data!!!\n");

                anj_mw_free(respEntry.pFrame);
                respEntry.pFrame = NULL;
                pFileSender->sendfinish = 1;
                iRet = -1;
                goto endFunc;
            }
            else
            {
                iRet = anj_mw_fread(fp, filebuf, pFileSender->framesize);
                if (iRet < 0)
                {
                    __ERR("read file error, err: %s\n", strerror(errno));

                    iRet = snprintf(respEntry.pFrame, 1024,
                                    "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                                    "<%s>\n"
                                    "<MESSAGE_HEADER\nMsg_type=\"MEDIA_DATA_MESSAGE\"\nMsg_code=\"2\"\nMsg_flag=\"0\"\n"
                                    "/>\n"
                                    "<MESSAGE_BODY>\n"
                                    "<POS FileStartPos=\"%u\"\nStartPos=\"%u\"\nDataLen=\"%d\"\n"
                                    "/>\n"
                                    "</MESSAGE_BODY>\n"
                                    "</%s>",
                                    anj_pri_xml_name_get(pFileSender->lognum),
                                    (unsigned int)pFileSender->startpos, (unsigned int)sendlen, -1,
                                    anj_pri_xml_name_get(pFileSender->lognum));

                    respEntry.pFrame[iRet + 0] = 0;
                    respEntry.pFrame[iRet + 1] = 0;
                    respEntry.pFrame[iRet + 2] = 0;
                    respEntry.pFrame[iRet + 3] = 0;

                    respEntry.nFrameLen = iRet + 4;
                    respEntry.nFlag = 1;
                    frame_mgr_push(&pstPriInfo->stUserInfo[pFileSender->lognum].bufMgr, &respEntry);

                    anj_mw_free(filebuf);
                    anj_mw_free(respEntry.pFrame);

                    pFileSender->sendfinish = 1;
                    iRet = -1;
                    goto endFunc;
                }
                else
                {
                    datalen = iRet;

                    if (sendlen % (1024 * 1024) == 0)
                    {
                        __ERR("send one frame, offset = %lu, datalen = %d\n", sendlen, iRet);
                    }

                    if (datalen > 0)
                    {
                        iRet = snprintf(respEntry.pFrame, 1024,
                                        "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                                        "<%s>\n"
                                        "<MESSAGE_HEADER\nMsg_type=\"MEDIA_DATA_MESSAGE\"\nMsg_code=\"2\"\nMsg_flag=\"0\"\n"
                                        "/>\n"
                                        "<MESSAGE_BODY>\n"
                                        "<POS FileStartPos=\"%u\"\nStartPos=\"%u\"\nDataLen=\"%d\"\n"
                                        "/>\n"
                                        "</MESSAGE_BODY>\n"
                                        "</%s>",
                                        anj_pri_xml_name_get(pFileSender->lognum),
                                        (unsigned int)pFileSender->startpos, (unsigned int)sendlen, datalen,
                                        anj_pri_xml_name_get(pFileSender->lognum));

                        respEntry.pFrame[iRet + 0] = 0;
                        respEntry.pFrame[iRet + 1] = 0;
                        respEntry.pFrame[iRet + 2] = 0;
                        respEntry.pFrame[iRet + 3] = 0;

                        memcpy(respEntry.pFrame + iRet + 4, filebuf, datalen);

                        respEntry.nFrameLen = iRet + 4 + datalen;
                        respEntry.nFlag = 1;
                        frame_mgr_push(&pstPriInfo->stUserInfo[pFileSender->lognum].bufMgr, &respEntry);

                        sendlen += datalen;
                    }

                    anj_mw_free(filebuf);

                    // send finish packet
                    if (datalen < pFileSender->framesize)
                    {
                        __ERR("send last frame, offset = %ld, datalen = %d\n", sendlen, datalen);

                        FRAME_ENTRY respEntry;
                        respEntry.pFrame = anj_mw_malloc(1024 + pFileSender->framesize);
                        if (respEntry.pFrame == NULL)
                        {
                            __ERR("respEntry.pFrame == NULL while prepare file data!!!\n");
                        }
                        else
                        {
                            iRet = snprintf(respEntry.pFrame, 1024,
                                            "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                                            "<%s>\n"
                                            "<MESSAGE_HEADER\nMsg_type=\"MEDIA_DATA_MESSAGE\"\nMsg_code=\"2\"\nMsg_flag=\"0\"\n"
                                            "/>\n"
                                            "<MESSAGE_BODY>\n"
                                            "<POS FileStartPos=\"%u\"\nStartPos=\"%u\"\nDataLen=\"%d\"\n"
                                            "/>\n"
                                            "</MESSAGE_BODY>\n"
                                            "</%s>",
                                            anj_pri_xml_name_get(pFileSender->lognum),
                                            (unsigned int)pFileSender->startpos, (unsigned int)sendlen, 0,
                                            anj_pri_xml_name_get(pFileSender->lognum));

                            respEntry.pFrame[iRet + 0] = 0;
                            respEntry.pFrame[iRet + 1] = 0;
                            respEntry.pFrame[iRet + 2] = 0;
                            respEntry.pFrame[iRet + 3] = 0;

                            respEntry.nFrameLen = iRet + 4;
                            respEntry.nFlag = 1;
                            frame_mgr_push(&pstPriInfo->stUserInfo[pFileSender->lognum].bufMgr, &respEntry);
                        }

                        // finished file sending
                        __ERR("file send finished!!!\n");

                        pFileSender->sendfinish = 1;
                        break;
                    }
                }
            }
        }
        usleep(delay * 1000);
    }

endFunc:
    __ERR("exited main loop\n");

    if (fp)
    {
        fclose(fp);
    }
    return iRet;
}

static void file_sender_create(const char *filename, unsigned long startpos, int lognum, int framesize, int interval)
{
    pFileSender = (file_sender_t *)anj_mw_malloc(sizeof(file_sender_t));
    if (!pFileSender)
        return ;
    memset(pFileSender, 0, sizeof(file_sender_t));
    strncpy(pFileSender->filename, filename, sizeof(pFileSender->filename) - 1);
    pFileSender->startpos = startpos;
    pFileSender->lognum = lognum;
    pFileSender->framesize = framesize;
    pFileSender->interval = interval;
    pFileSender->sendfinish = 0;
}

static void file_sender_destroy()
{
    if (pFileSender)
    {
        anj_mw_free(pFileSender);
        pFileSender = NULL;
    }
}

static int file_sender_check_finish()
{
    int iRet = 0;
    ANJ_CHK(pFileSender != NULL, 0, "input Invalid");
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    if (pFileSender->sendfinish && (frame_mgr_count(&pstPriInfo->stUserInfo[pFileSender->lognum].bufMgr) == 0))
    {
        iRet = 1;
    }
endFunc:
    return iRet;
}

static int file_sender_start()
{
    int iRet = 0;
    ANJ_CHK(pFileSender != NULL, -1, "input Invalid");

    pFileSender->stFileThread.bAutoDestroy = 1;
    strncpy(pFileSender->stFileThread.iThreadName, "file_send_thread", sizeof(pFileSender->stFileThread.iThreadName) - 1);
    pFileSender->stFileThread.iThreadjob.ctx = pFileSender;
    pFileSender->stFileThread.iThreadjob.func = file_sender_thread;
    ANJ_CHK_FUNC(anj_thread_task_create(&pFileSender->stFileThread), 0, "file_sender_thread create failed\n");
endFunc:
    return iRet;
}

static int file_sender_stop()
{
    int iRet = 0;
    ANJ_CHK(pFileSender != NULL, -1, "input Invalid");
    ANJ_CHK_FUNC(anj_thread_task_destroy(&stTransportThread, 0), 0, "file_sender_transport_thread exit failed\n");
    ANJ_CHK_FUNC(anj_thread_task_destroy(&pFileSender->stFileThread, 0), 0, "file_sender_thread exit failed\n");
endFunc:
    return iRet;
}

int file_sender_transport(int updown, int file_type, char *local_file, char *remote_file, int port, int lognum)
{
    int iRet = 0;
    FILE_TRANSPORT_PARAM *param = anj_mw_malloc(sizeof(FILE_TRANSPORT_PARAM));
    ANJ_CHK(param != NULL, -1, "input Invalid");

    memset(param, 0, sizeof(FILE_TRANSPORT_PARAM));

    if (local_file)
        strcpy(param->local_file, local_file);
    if (remote_file)
        strcpy(param->remote_file, remote_file);

    param->up_or_down = updown;
    param->file_type = file_type;
    param->server_port = port;
    param->lognum = lognum;

    memset(&stTransportThread, 0, sizeof(stTransportThread));
    stTransportThread.bAutoDestroy = 1;
    strncpy(stTransportThread.iThreadName, "file_transport", sizeof(stTransportThread.iThreadName) - 1);
    stTransportThread.iThreadjob.ctx = param;
    stTransportThread.iThreadjob.func = file_sender_transport_thread;
    ANJ_CHK_FUNC(anj_thread_task_create(&stTransportThread), 0, "file_sender_transport_thread create failed\n");

endFunc:
    __ERR("exited main loop\n");
    if (iRet != 0 && param)
    {
        anj_mw_free(param);
    }
    return iRet;
}

int file_sender_init(char *filename, int startpos, int lognum)
{
    int iRet = 0;
    if (pFileSender || getFileRecver())
    {
        __ERR("ont trans task is running, can not start upload!!!\n");
        iRet = -3;
    }
    else
    {
        file_sender_create(filename, startpos, lognum, 16 * 1024, 250);

        if (file_sender_start() < 0)
            iRet = -4;
    }
    return iRet;
}

int file_sender_uninit()
{
    file_sender_stop();
    file_sender_destroy();
    return 0;
}

void file_sender_clear()
{
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    // check if pFileSender need to be closed
    if (pFileSender)
    {
        if (file_sender_check_finish())
        {
            __ERR("file send finished, file sender closed!!!\n");
            file_sender_uninit();
            return;
        }
        else
        {
            int diff;
            struct timeval nowtime;
            SystemGetTimeofRun(&nowtime, NULL);

            if (pstPriInfo->stUserInfo[pFileSender->lognum].last_send_time.tv_sec > 0)
            {
                diff = (nowtime.tv_sec - pstPriInfo->stUserInfo[pFileSender->lognum].last_send_time.tv_sec) * 1000 +
                       (nowtime.tv_usec - pstPriInfo->stUserInfo[pFileSender->lognum].last_send_time.tv_usec) / 1000;

                if (diff > 60 * 1000 || diff < 0)
                {
                    __ERR("no data sent after 60 second, file sender closed!!!\n");
                    file_sender_uninit();
                    return;
                }
            }
        }
    }
}

file_sender_t *getFileSender()
{
    return pFileSender;
}
