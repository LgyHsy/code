#include <stdio.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <linux/sockios.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>

#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "anj_mw_net.h"
#include "anj_mw_thread.h"

#include "anj_ser.h"
#include "anj_sysmng.h"

#include "platform_sn.h"
#include "sn_utils.h"
#include "sn_header.h"


static anj_thread_s s_stP2pIdReplyThread;
static P2PID_GET_OK_CALLBACK s_p2pidfunc = NULL;

const char* p2pid_file_name_get(unsigned int nType)
{
    const char* pFileName = NULL;
    switch(nType)
    {
    case TYPE_ID_TYPE_DANALE:
        pFileName = DANALE_P2PID_FILE_NAME;
    break;
    case TYPE_ID_TYPE_GOOLINK:
        pFileName = GOOLINK_P2PID_FILE_NAME;
    break;
    case TYPE_ID_TYPE_EYEPLUS:
        pFileName = EYE_PLUS_SERIAL_ID_FILE;
    break;
    case TYPE_ID_TYPE_TUTK:
        pFileName = TUTK_P2PID_FILE_NAME;
    break;
    case TYPE_ID_TYPE_QINIU:
        pFileName = TUTK_P2PID_FILE_NAME;
    break;

    case TYPE_ID_TYPE_AC18PLUS_CONSUME:
    case TYPE_ID_TYPE_AC18PRO_CONSUME:
    case TYPE_ID_TYPE_AC18PRO_CMCC4G:
        pFileName = CW_P2PID_FILE_NAME;
    break;

    case TYPE_ID_TYPE_TUYA:
    case TYPE_ID_TYPE_TUYA_NVR:
        pFileName = TUYA_P2PID_FILE_NAME;
    break;
    case TYPE_ID_TYPE_TUYA_NVR_MAINPID:
        pFileName = TUYA_MAIN_DEV_PID_FILENAME;
    break;
    case TYPE_ID_TYPE_TUYA_NVR_SUBPID:
        pFileName = TUYA_SUB_DEV_PID_FILENAME;
    break;

    case TYPE_ID_TYPE_AC18PLUS_NVR:
    case TYPE_ID_TYPE_AC18PRO_NVR:
        pFileName = AC18PLUS_NVR_P2PID_FILENAME;
    break;
    case TYPE_ID_TYPE_TENCENT_IOT_IPC:
        pFileName = TENTCENT_IOT_IPC_P2PID_FILENAME;
    break;
    case TYPE_ID_TYPE_DOT:
        pFileName = DOT_IPC_P2PID_FILENAME;
    break;
    default:
    break;
    }

    return pFileName;
}

int p2pid_flash_read(char *buffer, int buffersize, unsigned int nType)
{
    memset(buffer, 0, buffersize);
    platform_p2pid_read(buffer, buffersize, nType);

    return 0;
}

int p2pid_file_read(char *buffer, int buffersize, unsigned int nType)
{
    const char* pFileName = p2pid_file_name_get(nType);
    if( NULL != pFileName )
    {
        return read_file_to_buffer(pFileName, buffer, buffersize);
    }

    return -1;
}

int ReadP2pID(char *buffer, int buffersize, unsigned int nType)
{
    int iRet = 0;
    iRet = p2pid_flash_read(buffer, buffersize, nType);
    if(iRet <= 0)
    {
        iRet = p2pid_file_read(buffer, buffersize, nType);
    }

    return iRet;    
}

int WriteP2pid(char *buf, int len, unsigned int nType)
{
    unsigned char buffer[P2PID_MAX_SIZE] = {0};
    hexStrToUInt(buf, len, buffer);
    len = len / 2;

    if( len < 0 || len > P2PID_MAX_SIZE )
    {
        __ERR("p2pid len %d error\n",  len);
        return -1;
    }

	const char* pFileName = p2pid_file_name_get(nType);
	if( NULL != pFileName )
	{
		__INFO("p2pid file:%s!\n", pFileName);
		write_file(pFileName, buffer, len);
	}

	platform_p2pid_write(buf, len);

	return 0;
}


int p2pid_handle_response(int cmd, const int sockfd, char* recv_buf, char* send_buf, struct sockaddr_in remote, char *szidentity)
{
    char remoteip[32] = {0};
    unsigned char * ip = (unsigned char *)&remote.sin_addr.s_addr;
    snprintf(remoteip, sizeof(remoteip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); //host order

    __INFO("got %d from %s: %d\n", cmd, remoteip, htons(remote.sin_port));
    __DBG("recv_buf:%s\n", recv_buf);

    char serialid[128] = {0};
    char serialdata[512] = {0};
    unsigned int nType = 0;
    if(soft_enc_xml_serial_data_parse(recv_buf, serialid, sizeof(serialid), serialdata, sizeof(serialdata), &nType) != 0)
    {
        return -1;
    }

    if(strcasecmp(szidentity, serialid) != 0 )
    {
        __INFO("szidentity:%s != serialid:%s\n", szidentity, serialid);
        return 0;
    }

    if(WriteP2pid((char*)serialdata, strlen(serialdata), nType) < 0)
    {
        return -1;
    }

    return 1;
}	
static int p2pid_reply_thread(void *ctx, int *bStart)
{
    __INFO("p2pid reply thread start\n");

    AjOemApply_t *pstAjOemApply = (AjOemApply_t *)ctx;
    if (pstAjOemApply == NULL)
    {
        __ERR("pstAjOemApply malloc failed\n");
        return -1;
    }

    int iRet = 0;
    int socketfd = -1;
    char *recv_buf = (char *) anj_mw_malloc(P2P_MAX_BUFFER_LEN);
    char *send_buf = (char *) anj_mw_malloc(P2P_MAX_BUFFER_LEN);
    if (recv_buf == NULL || send_buf == NULL)
    {
        __ERR("buf malloc failed!\n");
        return -1;
    }

    int sn_length = strlen(pstAjOemApply->szSN);
    if (sn_length > 32 || sn_length == 0)
    {
        __ERR("sn length:%d error\n", sn_length);
        goto __exit;
    }

    if (pstAjOemApply->nType >= TYPE_ID_TYPE_MAX)
    {
        __ERR("sn type:%d error >= max!\n", pstAjOemApply->nType);
        goto __exit;
    }

    char szType[ANJ_OEM_APPLY_STR_LEN] = {0};
    switch(pstAjOemApply->nType)
    {
    case TYPE_ID_TYPE_DANALE:
        strcpy(szType, "DANALE");
    break;
    case TYPE_ID_TYPE_GOOLINK:
        strcpy(szType, "GOOLINK");
    break;
    case TYPE_ID_TYPE_EYEPLUS:
        strcpy(szType, "EYEPLUS");
    break;
    case TYPE_ID_TYPE_TUTK:
        strcpy(szType, "TUTK");
    break;
    case TYPE_ID_TYPE_TUYA:
        strcpy(szType, "TUYA");
    break;
    case TYPE_ID_TYPE_TUYA_NVR:
        strcpy(szType, "TUYA_NVR");
    break;
    case TYPE_ID_TYPE_AC18PLUS_CONSUME:
        strcpy(szType, "AC18PLUS_CONSUME");
    break;
    case TYPE_ID_TYPE_AC18PRO_CONSUME:
        strcpy(szType, "AC18PRO_CONSUME");
    break;
    case TYPE_ID_TYPE_AC18PRO_CMCC4G:
        strcpy(szType, "AC18PRO_CMCC4G");
    break;
    case TYPE_ID_TYPE_AC18PLUS_NVR:
        strcpy(szType, "AC18PLUS_NVR");
    break;
    case TYPE_ID_TYPE_AC18PRO_NVR:
        strcpy(szType, "AC18PRO_NVR");
    break;
    case TYPE_ID_TYPE_TENCENT_IOT_IPC:
        strcpy(szType, "TENCENT_IOT_IPC");
    break;
    case TYPE_ID_TYPE_DOT:
        strcpy(szType, "DOT_IPC");
    break;

    default:
        __ERR("error p2p type:%d\n", pstAjOemApply->nType);
    goto __exit;
    }


    unsigned short uServerPort = BROADCASTING_PORT_P2PID;

    fd_set read_fds;
    struct timeval wait_time;
    struct sockaddr_in remote;

    char szDeviceType[32] = {0};
    anj_sysmng_dev_str_get(szDeviceType);

    unsigned int uLastSendTime = 0;
    unsigned int uStartTime = GetCurrentTimeStamp();
    
    while(bStart && *bStart)
    {

        // step1：创建广播套接字
        socketfd = broadcastserver(uServerPort);
        if (socketfd <= 0)
        {
            __ERR("socketfd:%d create failed!\n", socketfd);
            SLEEP_SECOND(1);
            continue;
        }

        __INFO("p2pid boradcast fd:%d create successful! enter send and recv loop...\n", socketfd);

        while(bStart && *bStart)
        {
            wait_time.tv_sec = 1;
            wait_time.tv_usec = 0;

            // 退出条件1：线程执行超过5分钟
            unsigned int uNowTime =  GetCurrentTimeStamp();
            if ((uNowTime - uStartTime) > SOFT_DATA_AUTH_TIME)
            {
                __ERR("p2pid reply thread exit timeout, run %u -> %u!\n", uStartTime, uNowTime);
                goto __exit;
            }

            // 每500ms组装信息发送一次
            if (uNowTime - uLastSendTime > SOFT_DATA_CHECK_TIME)
            {
                snprintf(send_buf, P2P_MAX_BUFFER_LEN,
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

                if(broardcast_send_request(socketfd, uServerPort, remote, send_buf) == 0)
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
            FD_SET(socketfd, &read_fds);
            iRet = select(socketfd + 1, &read_fds, NULL, NULL, &wait_time);
            if(iRet < 0)
            {
                // 重试条件1：select失败时销毁重建套接字重试。
                __ERR("p2pid select fd:%d failed, error:%d\n", socketfd, errno);
                close(socketfd);
                socketfd = -1;

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
                // 重试条件2：接收失败时销毁重建套接字重试。
                if(FD_ISSET(socketfd, &read_fds))
                {
					int remote_len = sizeof(remote);
					iRet = recvfrom(socketfd, recv_buf, P2P_MAX_BUFFER_LEN, 0, (struct sockaddr*)&remote, (unsigned int*)&remote_len);
					if(iRet <= 0)
					{
                        __ERR("p2pid recvfrom fd:%d failed, error:%d\n", socketfd, errno);
                        close(socketfd);
                        socketfd = -1;

                        SLEEP_SECOND(1);
                        break;
					}

                    recv_buf[iRet]='\0';

                    int cmd = soft_enc_xml_cmd_parse(recv_buf);
                    char remoteip[32] = {0};
                    unsigned char * ip = (unsigned char *)&remote.sin_addr.s_addr;
                    snprintf(remoteip, sizeof(remoteip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); //host order

                    __DBG("remoteip:%s, recvbuf:%s\n", remoteip, recv_buf);
                    if (cmd == IPC_MESSAGE_REQUEST_ID_RESPONSE)
                    {
                        // 重试条件3：处理命令失败销毁重建套接字重试。
                        if (p2pid_handle_response(cmd, socketfd, recv_buf, send_buf, remote, pstAjOemApply->szSN) <= 0)
                        {
                            break;
                        }
                        else
                        {
                            if (s_p2pidfunc != NULL)
                            {
                                s_p2pidfunc();
                            }

                            __INFO("p2pid response and func successful!\n");
                            goto __exit;
                        }
                    }
                }
            }
        }
    
        __INFO("p2pid exit send and recv loop\n");
        if (socketfd > 0)
        {
            close(socketfd);
            socketfd = -1;
        }

    }

__exit:
    __INFO("p2pid exit thread!\n");
    if (socketfd > 0)
    {
        close(socketfd);
        socketfd = -1;
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

int start_p2pid_thread(AjOemApply_t *pParam)
{
    int iRet = 0;
    if (s_stP2pIdReplyThread.start != 0)
    {
        __INFO("p2pid reply thread stop!\n");
        anj_thread_task_destroy(&s_stP2pIdReplyThread, -1);  
        usleep(500 * 1000);
    }

    if (s_stP2pIdReplyThread.start == 0)
    {
        AjOemApply_t *pstOemData = (AjOemApply_t *) anj_mw_malloc(sizeof(AjOemApply_t));
        memcpy(pstOemData, pParam, sizeof(AjOemApply_t));

        memset(&s_stP2pIdReplyThread, 0, sizeof(anj_thread_s));
        s_stP2pIdReplyThread.bAutoDestroy = 1;
        strncpy(s_stP2pIdReplyThread.iThreadName, "anj_p2pid_reply_thread", sizeof(s_stP2pIdReplyThread.iThreadName) - 1);
        s_stP2pIdReplyThread.iThreadjob.ctx = pstOemData;
        s_stP2pIdReplyThread.iThreadjob.func = p2pid_reply_thread;
        iRet = anj_thread_task_create(&s_stP2pIdReplyThread);
    }

    return iRet;
}

int stop_p2pid_thread(void)
{
    __INFO("p2pid reply thread stop\n");
    anj_thread_task_destroy(&s_stP2pIdReplyThread, -1);
    return 0;
}

int wait_p2pid_thread(void)
{
    while(s_stP2pIdReplyThread.end != 1)
    {
        usleep(1000);
    }

    return 0;
}

void set_p2pid_cb(P2PID_GET_OK_CALLBACK cb)
{
	s_p2pidfunc = cb;
}
