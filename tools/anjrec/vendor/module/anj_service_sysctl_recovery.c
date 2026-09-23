#include <pthread.h>
#include <string.h>

#include "ixml.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_service.h"
#include "anj_service_upgrade.h"
#include "cmd_def.h"
#include "file_receiver.h"
#include "file_sender.h"
#include "anj_config_stream.h"
#include "anj_config_alarm.h"

int anj_service_task_system_get(int cmd, void *data, int *len, int channel)
{
    int iRet = CMD_UNSUPPORT_RESPONSE;
    char *buf = NULL;
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();
    pthread_rwlock_rdlock(rwlock);

    if (channel < 0 || channel >= ANJ_CAMERA_MAX_NUMS)
    {
        channel = 0;
    }

    switch (cmd)
    {
    case CMD_GET_MEDIA_CONFIG:
        buf = anj_config_media_conver_xml((MediaConfig *)getMediaConfig(), -1, 0);
        break;
    case CMD_GET_MEDIASTREAM_CONFIG:
        buf = anj_config_stream_conver_xml((MediaStreamConfig *)getMediaStreamConfig());
        break;
    case CMD_GET_ALARM_CONFIG:
        buf = anj_config_alarm_conver_xml((AlarmConfig *)getAlarmConfig());
        break;
    default:
        break;
    }

    if (buf != NULL)
    {
        if (data != NULL && len != NULL)
        {
            *len = strlen(buf);
            memcpy(data, buf, *len);
        }
        anj_mw_free(buf);
        iRet = 0;
    }

    pthread_rwlock_unlock(rwlock);
    return iRet;
}

int anj_service_task_system_set(int cmd, char *data, int channel, int MsgSrc)
{
    (void)cmd;
    (void)data;
    (void)channel;
    (void)MsgSrc;
    return CMD_UNSUPPORT_RESPONSE;
}

char *anj_service_task_media(char *cmdbuf, int cmdlen, IXML_Document *pDoc,
                             char *MsgCode, char *MsgRoot, int MsgSrc)
{
    char *pBuffer = NULL;
    int payloadlen = 0;
    int dataerror = 0;
    int msgcode = atoi(MsgCode);
    char *payload = cmdbuf + strlen(cmdbuf);

    if (*payload == 0 && *(payload + 1) == 0 && *(payload + 2) == 0 && *(payload + 3) == 0)
    {
        payload += 4;
    }
    else
    {
        __ERR("data format error !!\n");
        dataerror = 1;
    }

    if (!dataerror)
    {
        char *length_param = anj_config_pos_value_get(pDoc, (char *)"DataLen");
        if (length_param == NULL)
        {
            __ERR("no DataLen field found!!!\n");
        }
        else
        {
            payloadlen = atoi(length_param);
            anj_mw_free(length_param);

            if (payloadlen < 0)
            {
                file_recver_uninit(1);
            }
            else if (MsgSrc == MSG_SRC_PRI && (payload - cmdbuf + payloadlen != cmdlen))
            {
                __ERR("datalen error, payloadlen=%d, cmdlen=%d, header=%d\n",
                      payloadlen, cmdlen, (int)(payload - cmdbuf));
                dataerror = 1;
            }

            if (!dataerror && msgcode == EVENT_UPLOAD)
            {
                file_recver_t *pActiveRecver = getFileRecver();
                if (pActiveRecver == NULL)
                {
                    __ERR("EVENT_UPLOAD without active receiver\n");
                    dataerror = -1;
                }
                else
                {
                    dataerror = file_recver_proc(dataerror, payload, payloadlen, MsgSrc);
                    if (payloadlen > 0)
                    {
                        __INFO("EVENT_UPLOAD chunk %d bytes, progress %d/%d\n",
                               payloadlen, pActiveRecver->writelen, pActiveRecver->filelen);
                    }
                }

                file_recver_t *pFileReceiver = getFileRecver();
                if (pFileReceiver && (payloadlen == 0))
                {
                    pBuffer = anj_mw_malloc(1024);
                    if (pBuffer != NULL)
                    {
                        if (pFileReceiver->filetype == UPLOAD_FIRMWARE_FILE_TYPE)
                        {
                            snprintf(pBuffer, 1024,
                                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                                     "<%s>\n"
                                     "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                                     "/>\n"
                                     "<MESSAGE_BODY></MESSAGE_BODY>\n"
                                     "</%s>",
                                     MsgRoot, CMD_APPBIN_LOCAL_UPDATE, dataerror, MsgRoot);
                        }
                        file_recver_uninit(0);
                    }
                }
            }
        }
    }

    return pBuffer;
}

int anj_service_task_sysctl(IXML_Document *pDoc, char *cmdbuf, int cmdlen, char *MsgType, char *MsgCode,
                            int MsgSrc, int lognum, int channel, char **data)
{
    int iRet = CMD_UNSUPPORT_RESPONSE;
    int msg_code = atoi(MsgCode);
    char *msg_body = NULL;

    (void)cmdbuf;
    (void)cmdlen;
    (void)MsgType;
    (void)channel;

    switch (msg_code)
    {
    case CMD_GET_SERIALNUMBER:
        iRet = anj_service_sysctl_basic_get_serialnumber(&msg_body);
        break;
    case CMD_GET_SYSTEMCONTROLSTRING:
        iRet = anj_service_sysctl_basic_get_systemcontrolstring(&msg_body);
        break;
    case CMD_GET_SYSTEM_VERSION_INFO:
    case CMD_GET_REALY_VERSION_INFO:
        iRet = anj_service_sysctl_basic_get_version_info(&msg_body);
        break;
    case CMD_GET_NETWORK_STATUS:
        iRet = anj_service_sysctl_basic_get_network_status(&msg_body);
        break;
    case CMD_GET_MEDIA_CAPABILITY:
        iRet = anj_service_sysctl_basic_get_media_capability(&msg_body);
        break;
    case CMD_SET_SYSTEM_TIME:
        iRet = anj_service_sysctl_basic_set_system_time(pDoc);
        break;
    case CMD_SET_KEYFRAME:
        iRet = 0;
        break;
    case CMD_START_RTP_PUSH:
    case CMD_STOP_RTP_PUSH:
        iRet = -1;
        break;
    case CMD_UPLOAD_FILE:
        iRet = anj_service_upgrade_handle_upload_file(pDoc, MsgSrc, lognum, &msg_body);
        break;
    default:
        break;
    }

    if (data)
    {
        *data = msg_body;
    }
    else if (msg_body)
    {
        anj_mw_free(msg_body);
    }

    return iRet;
}
