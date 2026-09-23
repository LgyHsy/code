#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "anj_mw_file.h"
#include "anj_mw_net.h"
#include "anj_mw_thread.h"

#include "anj_sysmng.h"

#include "platform_sn.h"
#include "sn_utils.h"
#include "sn_header.h"

static anj_thread_s s_stOemapplyThread = {0};
static OEMINFO_GET_OK_CALLBACK s_oemfunc = NULL;

static int oemapply_data_handle(char *buf, int len, unsigned int nType)
{
    unsigned char buffer[P2PID_MAX_SIZE] = {0};
    hexStrToUInt(buf, len, buffer);	
    len = len / 2;	

    if( len < 0 || len > P2PID_MAX_SIZE )
    {
        __ERR("oemapply len %d error\n",  len);
        return -1;
    }

    int iIndex = 0;
    for(iIndex = 0; iIndex < len && iIndex < P2PID_MAX_SIZE; iIndex++)
    {
        if(buffer[iIndex] == '-')
            buffer[iIndex] = ':';
    }

    __INFO("oemapply buf:%s ----- %s\n", buf, buffer);
    strcpy(buf, (char*)buffer);

    return 0;
}


static int oemapply_handle_response(int cmd, const int sockfd, 
                                    char* recv_buf, char* send_buf, 
                                    struct sockaddr_in remote, char *szidentity)
{
    char remoteip[32] = {0};
    unsigned char * ip = (unsigned char *)&remote.sin_addr.s_addr;
    snprintf(remoteip, sizeof(remoteip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); //host order

    __INFO("oemapply got %d from %s: %d\n", cmd, remoteip, htons(remote.sin_port));
    __INFO("oemapply recv:%s\n", recv_buf);

    char serialid[128] = {0};
    char serialdata[512] = {0};
    unsigned int nType = 0;

    if(soft_enc_xml_serial_data_parse(recv_buf, serialid, sizeof(serialid), serialdata, sizeof(serialdata), &nType) != 0)
    {
        return -1;
    }

    if(strcasecmp(szidentity, serialid) !=0)
    {
        __ERR("oemapply %s != %s \n", szidentity, serialid);
        return 0;
    }

    if( nType != TYPE_ID_TYPE_MACADDR )
    {
        __ERR("oemapply type %d error\n",nType);
        return 0;
    }

    if(oemapply_data_handle(serialdata, strlen(serialdata), nType) != 0)
    {
        __ERR("oemapply handle error\n");
        return 0;
    }

    if( s_oemfunc != NULL )
    {
        s_oemfunc(serialdata);
        return 1;
    }

    return 0;
}

static int oemapply_thread(void *ctx, int *bStart)
{
    AjOemApply_t *pstAjOemApply = (AjOemApply_t *)ctx;
    if (pstAjOemApply == NULL)
    {
        __ERR("oemapply malloc failed\n");
        return -1;
    }

    int iRet = 0;
    int sockfd = -1;
    char *recv_buf = (char *)anj_mw_malloc(OEMAPPLY_MAX_BUFFER_LEN);
    char *send_buf = (char *)anj_mw_malloc(OEMAPPLY_MAX_BUFFER_LEN);
    if( NULL == recv_buf || NULL == send_buf)
    {
        __ERR("oemapply recv_buf or send_buf malloc failed\n");
        goto __exit;
    }

    int length = strlen(pstAjOemApply->szSN);
    if( length > 32 || length == 0 )
    {
        __ERR("oemapply length %d error\n", length);
        goto __exit;
    }

    if( pstAjOemApply->nType != TYPE_ID_TYPE_MACADDR)
    {
        __ERR("oemapply error type\n");
        goto __exit;
    }

    unsigned short uServerPort = BROADCASTING_PORT_MACADDR;


    struct sockaddr_in remote;
    fd_set read_fds;
    struct timeval wait_time;

    char szDeviceType[32] = {0};
    anj_sysmng_dev_str_get(szDeviceType);

    unsigned int uLastSendTime = 0;
    unsigned int uStartTime = GetCurrentTimeStamp();

    while(bStart && *bStart)
    {
        // step1：创建广播套接字
        sockfd = broadcastserver(uServerPort);
        if (sockfd <= 0)
        {
            __ERR("oemapply socketfd:%d create failed!\n", sockfd);
            SLEEP_SECOND(1);
            continue;
        }

        __INFO("oemapply boradcast fd:%d create successful! enter send and recv loop...\n", sockfd);

        while(bStart && *bStart)
        {
            wait_time.tv_sec = 1;
            wait_time.tv_usec = 0;
            
            // 退出条件1：线程执行超过5分钟
            unsigned int uNowTime =  GetCurrentTimeStamp();
            if ((uNowTime - uStartTime) > SOFT_DATA_AUTH_TIME)
            {
                __ERR("oemapply thread exit timeout, run %u -> %u!\n", uStartTime, uNowTime);
                goto __exit;
            }

            // 每500ms组装信息发送一次
            if (uNowTime - uLastSendTime > SOFT_DATA_CHECK_TIME)
            {
                const char *szType = "MACADDR";
                snprintf(send_buf, OEMAPPLY_MAX_BUFFER_LEN,
					"<ENCRYPT>\n"
					"<MESSAGE_HEADER Msg_type=\"SYSTEM_IDAPPLY_MESSAGE\" Msg_code=\"%d\" Msg_flag=\"0\" />\n"
					"<MESSAGE_BODY>\n"
					"<DEVICE DeviceType=\"%s\" SERIALID=\"%s\" UUID=\"%s\" TYPE=\"%s\" />\n"
					"</MESSAGE_BODY>\n"
					"</ENCRYPT>\n",
                    IPC_MESSAGE_REQUEST_ID, 
                    szDeviceType,
                    pstAjOemApply->szSN, pstAjOemApply->szUUID,
                    szType);

                if (broardcast_send_request(sockfd, uServerPort, remote, send_buf) == 0)
                {
                    uLastSendTime = uNowTime;
                }
                else
                {
                    continue;
                }
            }

            // 套接字可读时，接收buf进行解析
            FD_ZERO(&read_fds);
            FD_SET(sockfd, &read_fds);
            iRet = select(sockfd + 1, &read_fds, NULL, NULL, &wait_time);
            if(iRet < 0)
            {
                // 重试条件1：select失败时销毁重建套接字重试。
                __ERR("oemapply select failed, error=%d\n", errno);
                close(sockfd);
                sockfd = -1;

                mysystem_with_param("ifconfig %s up", WIRE_INTERFACE_NAME);
                SLEEP_SECOND(1);
                break;
            }
            else if(iRet == 0)
            {
                continue;
            }
            else
            {
                if(FD_ISSET(sockfd, &read_fds))
                {
                    int remote_len = sizeof(remote);
                    iRet = recvfrom(sockfd, recv_buf, OEMAPPLY_MAX_BUFFER_LEN, 0, (struct sockaddr*)&remote, (unsigned int*)&remote_len);
                    if(iRet <= 0)
                    {
                        // 重试条件2：接收失败时销毁重建套接字重试。
                        __ERR("oemapply recvfrom fd:%d failed, error:%d\n", sockfd, errno);
                        close(sockfd);
                        sockfd = -1;
                        
                        SLEEP_SECOND(1);
                        break;
                    }

                    recv_buf[iRet] = '\0';
                    int cmd = soft_enc_xml_cmd_parse(recv_buf);
                    char remoteip[32] = {0};
                    unsigned char * ip = (unsigned char *)&remote.sin_addr.s_addr;
                    snprintf(remoteip, sizeof(remoteip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); //host order

                    if(cmd == IPC_MESSAGE_REQUEST_ID_RESPONSE)
                    {
                        // 重试条件3：处理命令失败销毁重建套接字重试。
                        if( 0 >= oemapply_handle_response(cmd, sockfd, recv_buf, send_buf, remote, pstAjOemApply->szSN))
                        {
                            break;
                        }
                        else
                        {
                            goto __exit;                               
                        }
                    }

                }
            }
        }

        __INFO("oemapply exit send and recv loop\n");
        if (sockfd > 0)
        {
            close(sockfd);
            sockfd = -1;
        }

    }

__exit:
    __INFO("oemapply exit thread\n");
    if (sockfd > 0)
    {
        close(sockfd);
        sockfd = -1;
    }

    if (recv_buf != NULL)
    {
        anj_mw_free(recv_buf);
        recv_buf = NULL;
    }

    if(send_buf != NULL)
    {
        anj_mw_free(send_buf);
        send_buf = NULL;
    }

    if (pstAjOemApply != NULL)
    {
        anj_mw_free(pstAjOemApply);
        pstAjOemApply = NULL;
    }

    return 0;
}

int start_oemapply_thread(AjOemApply_t *pParam)
{
    int iRet = 0;
    if (s_stOemapplyThread.start != 0)
    {
        __INFO("oemapply thread stop!\n");
        anj_thread_task_destroy(&s_stOemapplyThread, -1);  
        usleep(500 * 1000);
    }

    if (s_stOemapplyThread.start == 0)
    {
        AjOemApply_t *pstOemData = (AjOemApply_t *) anj_mw_malloc(sizeof(AjOemApply_t));
        memcpy(pstOemData, pParam, sizeof(AjOemApply_t));

        memset(&s_stOemapplyThread, 0, sizeof(anj_thread_s));
        s_stOemapplyThread.bAutoDestroy = 1;
        strncpy(s_stOemapplyThread.iThreadName, "anj_oemapply_thread", sizeof(s_stOemapplyThread.iThreadName) - 1);
        s_stOemapplyThread.iThreadjob.ctx = pstOemData;
        s_stOemapplyThread.iThreadjob.func = oemapply_thread;
        iRet = anj_thread_task_create(&s_stOemapplyThread);
    }

    return iRet;        
}

int stop_oemapply_thread()
{
    __INFO("oemapply reply thread stop\n");
    anj_thread_task_destroy(&s_stOemapplyThread, -1);  
    return 0;
}

int wait_oemapply_thread(void)
{
    while(s_stOemapplyThread.end != 1)
    {
        usleep(1000);
    }

    return 0;
}

void set_oemapply_cb(OEMINFO_GET_OK_CALLBACK cb)
{
    s_oemfunc = cb;
}



