#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_systime.h"
#include "anj_net.h"
#include "anj_smart.h"
#include "anj_sdcard.h"
#include "anj_service.h"
#include "anj_ser_api.h"
#include "anj_aiot.h"
#include "anj_aiot_cmd.h"
#include "anj_aiot_trans.h"

#include "ota_update.h"
#include "function_list.h"
#include "file_sender.h"
#include "file_receiver.h"
#include "cmd_def.h"

static char *anj_aiot_trans_proc_media(char *cmdbuf, char *MsgRoot, IXML_Document *pDoc, char *MsgCode)
{
    char *pBuffer = anj_service_task_media(cmdbuf, 0, pDoc, MsgCode, MsgRoot, MSG_SRC_SER);
    return pBuffer;
}

static void anj_aiot_trans_proc_ptz(const char *cmdbuf, char *MsgCode)
{
    int nMsgCode = atoi(MsgCode);
    __INFO("Got SYSTEM_CONFIG_GET_MESSAGE, code = %d \n", nMsgCode);

    if (CMD_PTZ_CONTROL == nMsgCode)
    {
        char ptzxml[1024 * 2];
        const char *start, *end;
        const char *msg_body_start = (char *)"<MESSAGE_BODY>";
        const char *msg_body_end = (char *)"</MESSAGE_BODY>";
        start = strstr(cmdbuf, msg_body_start);
        start += strlen(msg_body_start);
        end = strstr(cmdbuf, msg_body_end);
        int length = end - start;
        if (length > (int)(sizeof(ptzxml) - 1))
            length = (int)(sizeof(ptzxml) - 1);
        memcpy(ptzxml, start, length);
        ptzxml[length] = '\0';

        // todo
        // if (AuxMsgPTZCmd(ptzxml) != 0)
        // {
        //     __ERR("AuxMsgPTZCmd error nMsgCode:%s \n", ptzxml);
        // }
    }
}

static char *anj_aiot_trans_get_syscfg(char *MsgRoot, char *MsgCode, int channel)
{
    int iRet = 0;
    int len = 0;
    int config_key = 0;
    char *pSendBuffer = NULL;

    char *buffer = (char *)anj_mw_malloc(50 * 1024);
    if (buffer == NULL)
    {
        __ERR("buffer malloc fail!\n");
        return pSendBuffer;
    }

    config_key = atoi(MsgCode);

    __INFO("Got SYSTEM_CONFIG_GET_MESSAGE, code = %d\n", config_key);

    if (channel < 0)
        channel = 0;
    iRet = anj_service_task_system_get(config_key, buffer, &len, channel);

    int nFrameLen = ANJ_ALIGN_UP(len + 1024, 1024);
    pSendBuffer = anj_mw_malloc(nFrameLen);
    memset(pSendBuffer, 0, nFrameLen);
    if (pSendBuffer == NULL)
    {
        __ERR("pSendBuffer == NULL while handle SYSTEM_CONFIG_GET_MESSAGE!!!\n");
    }
    else
    {
        if (iRet == 0)
        {
            snprintf(pSendBuffer, nFrameLen - 1,
                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                     "<%s>\n"
                     "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONFIG_GET_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"0\"\n"
                     "/>\n"
                     "<MESSAGE_BODY>\n"
                     "%s\n"
                     "</MESSAGE_BODY>\n"
                     "</%s>",
                     MsgRoot, MsgCode, buffer, MsgRoot);
        }
        else
        {
            snprintf(pSendBuffer, nFrameLen - 1,
                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                     "<%s>\n"
                     "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONFIG_GET_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"%d\"\n"
                     "/>\n"
                     "<MESSAGE_BODY></MESSAGE_BODY>\n"
                     "</%s>",
                     MsgRoot, MsgCode, iRet, MsgRoot);
        }
    }

    if (buffer != NULL)
        anj_mw_free(buffer);

    return pSendBuffer;
}

static char *anj_aiot_trans_set_syscfg(char *cmdbuf, int cmdlen, char *MsgRoot, char *MsgCode, int channel)
{
    int iRet = 0;
    char *pBuffer = (char *)anj_mw_malloc(cmdlen);
    if (pBuffer == NULL)
    {
        __ERR("malloc %d NULL while handle SYSTEM_CONFIG_SET_MESSAGE!!!\n", cmdlen);
        return pBuffer;
    }

    char *start, *end;
    char *msg_body_start = (char *)"<MESSAGE_BODY>";
    char *msg_body_end = (char *)"</MESSAGE_BODY>";
    int length, config_key;

    config_key = atoi(MsgCode);

    __INFO("Got SYSTEM_CONFIG_SET_MESSAGE, code = %d\n", config_key);

    start = strstr(cmdbuf, msg_body_start);
    if (start == NULL)
    {
        iRet = -1;
    }
    start += strlen(msg_body_start);
    end = strstr(cmdbuf, msg_body_end);
    if (end == NULL)
    {
        iRet = -1;
    }
    length = end - start;
    if (length <= 0)
    {
        iRet = -1;
    }

    if (iRet == 0)
    {
        memcpy(pBuffer, start, length);
        pBuffer[length] = '\0';
        iRet = anj_service_task_system_set(config_key, pBuffer, channel, MSG_SRC_SER);
    }
    memset(pBuffer, 0, cmdlen);

    snprintf(pBuffer, cmdlen,
             "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
             "<%s>\n"
             "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONFIG_SET_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"%d\"\n"
             "/>\n"
             "<MESSAGE_BODY>\n"
             "</MESSAGE_BODY>\n"
             "</%s>",
             MsgRoot, MsgCode, ((iRet == 0) ? iRet : -1), MsgRoot);

    return pBuffer;
}

static char *anj_aiot_trans_sysctl(IXML_Document *pDoc, char *cmdbuf, int cmdlen,
                                   char *MsgRoot, char *MsgType, char *MsgCode, int channel)
{
    int iRet = 0;
    char *msg_body = NULL;
    iRet = anj_service_task_sysctl(pDoc, cmdbuf, cmdlen, MsgType, MsgCode, MSG_SRC_SER, 0, channel, &msg_body);

    unsigned int nMsgBodyLen = 0;
    if (msg_body != NULL)
        nMsgBodyLen = strlen(msg_body);
    else
        nMsgBodyLen = 0;
    unsigned int nBuflen = nMsgBodyLen + 512;
    char *pBuffer = (char *)anj_mw_malloc(nBuflen);
    if (NULL != pBuffer)
    {
        snprintf(pBuffer, nBuflen - 1,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>"
                 "<%s>"
                 "<MESSAGE_HEADER Msg_type=\"%s\" Msg_code=\"%s\" Msg_flag=\"%d\" Msg_channel=\"%d\" "
                 "/> "
                 "<MESSAGE_BODY>"
                 "%s "
                 "</MESSAGE_BODY> "
                 "</%s>",
                 MsgRoot, MsgType, MsgCode, iRet, channel, (msg_body == NULL) ? "" : msg_body, MsgRoot);
    }

    if (msg_body != NULL)
    {
        anj_mw_free(msg_body);
    }
    return pBuffer;
}

static void anj_aiot_trans_report(int nMsgCode)
{
    switch (nMsgCode)
    {
    case CMD_SET_PTZ_DIRECTION:
        anj_aiot_cmd_ptz_dir_get();
        break;
    case CMD_SET_MEDIA_AUDIO_CAPTURE:
    case CMD_SET_MEDIA_AUDIO_CONFIG:
        anj_aiot_cmd_audio_capture_get();
        break;
    case CMD_SET_MEDIA_VIDEO_CAPTURE:
        anj_aiot_cmd_ircut_get();
        break;
    case CMD_SET_MEDIA_VIDEO_OSD:
        anj_aiot_cmd_osd_get();
        break;
    case CMD_SET_MEDIA_VIDEO_CONFIG:
        anj_aiot_cmd_osd_get();
        anj_aiot_cmd_ircut_get();
        break;
    default:
        break;
    }
}

char *anj_aiot_trans_proc(char *cmdbuf, unsigned int cmdlen)
{
    int iRet = 0;
    char *szResponse = NULL;

    IXML_Document *pDoc = NULL;
    char MsgRoot[256] = {0};
    char MsgType[256] = {0};
    char MsgCode[256] = {0};
    char MsgFlag[256] = {0};
    int channel = -1;
    iRet = anj_service_cmd_convert_xml(cmdbuf, MsgRoot);
    if (iRet != 0)
    {
        __ERR("cmdbuf:%s invalid!\n", cmdbuf);
        return szResponse;
    }

    pDoc = ixmlParseBuffer(cmdbuf);
    if (pDoc == NULL)
    {
        __ERR("pDoc == NULL, xml=\n%s\n", cmdbuf);
        return szResponse;
    }

    iRet = anj_service_cmd_parse_xml(pDoc, MsgRoot, MsgType, MsgCode, MsgFlag, &channel);
    if (iRet != 0)
    {
        __ERR("cmdbuf:%s invalid!\n", cmdbuf);
        ixmlDocument_free(pDoc);
        return szResponse;
    }

    if (strcmp(MsgType, "MEDIA_DATA_MESSAGE") == 0)
    {
        szResponse = anj_aiot_trans_proc_media(cmdbuf, MsgRoot, pDoc, MsgCode);
    }
    else if (strcmp(MsgType, "PTZ_CONTROL_MESSAGE") == 0)
    {
        anj_aiot_trans_proc_ptz(cmdbuf, MsgCode);
    }
    else if (strcmp(MsgType, "SYSTEM_CONFIG_GET_MESSAGE") == 0)
    {
        szResponse = anj_aiot_trans_get_syscfg(MsgRoot, MsgCode, channel);
    }
    else if (strcmp(MsgType, "SYSTEM_CONFIG_SET_MESSAGE") == 0)
    {
        if (CMD_GET_DEFAULT_NETWORK_LAN_CONFIG == atoi(MsgCode))
            szResponse = anj_aiot_trans_get_syscfg(MsgRoot, MsgCode, channel);
        else
            szResponse = anj_aiot_trans_set_syscfg(cmdbuf, cmdlen, MsgRoot, MsgCode, channel);
    }
    else
    {

        szResponse = anj_aiot_trans_sysctl(pDoc, cmdbuf, cmdlen, MsgRoot, MsgType, MsgCode, channel);
    }

    ixmlDocument_free(pDoc);

    anj_aiot_trans_report(atoi(MsgCode));
    return szResponse;
}
