#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mw_time.h"
#include "anj_mw_hwctrl.h"
#include "anj_mw_file.h"
#include "anj_mw_str.h"
#include "anj_module.h"
#include "anj_config.h"
#include "anj_net.h"
#include "anj_sysmng.h"
#include "anj_sysctl.h"
#include "anj_systime.h"
#include "anj_pri.h"
#include "anj_pri_cmd.h"
#include "cmd_def.h"
#include "user_auth.h"
#include "file_sender.h"
#include "file_receiver.h"
#include "audio_receiver.h"
#include "anj_service.h"
#include "anj_record.h"
#include "anj_sdcard.h"
#include "anj_video.h"
#include "anj_ispctl.h"
#include "anj_osd.h"
#include "anj_base64.h"
#include "eventhub.h"
#include "media_util.h"
#include "function_list.h"
#include "alarm_link.h"

enum
{
    ACTION_PLAY = 0,
    ACTION_PAUSE,
    ACTION_RESUME,
    ACTION_FAST,
    ACTION_SLOW,
    ACTION_SEEK,
    ACTION_FRAMESKIP,
    ACTION_STOP
};

#define START_PORT 9000

#define AUTH_MSG_SIZE (2 * 1024)
#define MAX_IDENTITY_NUM 10

#define PAYLOAD_SIZE 4096

#define MAX_PTZ_CMD_LEN 512
#define MAX_SET_XML_LEN 1024 * 8

// 视频解码参数
typedef struct
{
    unsigned long stream_index;
    char video_encoder[32];
    unsigned long width;
    unsigned long height;
    unsigned long framerate;
    unsigned long intraframerate; // I frame interval
    unsigned long bitrate;
    char config[256]; // 提交给解码器的第一个I帧前面必须加上config的数据
    int config_len;   // MPEG4 18字节VOL，H264 114字节
} VIDEO_PARAM;

// 音频解码参数
typedef struct
{
    unsigned long stream_index;
    char audio_encoder[32];
    unsigned long samplerate;
    unsigned long samplebitswidth; // 8 or 16
    unsigned long channels;        // 0: mono, 1: stero
    unsigned long bitrate;
    unsigned long framerate;
} AUDIO_PARAM;

int anj_pri_cmd_proc_media(char *cmdbuf, int cmdlen, int lognum, IXML_Document *pDoc, char *MsgCode)
{
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    FRAME_ENTRY respEntry = {0};
    respEntry.pFrame = anj_mw_malloc(1024);
    respEntry.pFrame = anj_service_task_media(cmdbuf, cmdlen, pDoc,
                                              MsgCode, anj_pri_xml_name_get(lognum), MSG_SRC_PRI);
    if (respEntry.pFrame)
    {
        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }
    return 0;
}

static void anj_pri_cmd_replay_send(int lognum, const char *Msg_type, int msg_code, int Msg_flag)
{
    FRAME_ENTRY respEntry;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    respEntry.pFrame = anj_mw_malloc(1024);
    if (respEntry.pFrame == NULL)
    {
        __INFO("respEntry.pFrame == NULL while handle TIMELINE_REPLAY_CONTROL_MESSAGE !!!\n");
    }
    else
    {
        snprintf(respEntry.pFrame, 1024,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"%s\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                 "/>\n"
                 "<MESSAGE_BODY></MESSAGE_BODY>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum), Msg_type, msg_code, Msg_flag, anj_pri_xml_name_get(lognum));

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }
}

int anj_pri_cmd_replay_video(rec_pb_poper *pPoper, media_frame_info_t *pFrameInfo)
{
    if (NULL == pPoper || pPoper->iPopId < 0 || pPoper->iPopId > MAX_USER_SESSION || NULL == pFrameInfo)
    {
        return -1;
    }

    //	__ERR("user %d, data %d, iskey %d", pPoper->iPopId, len, iskey);

    int iRet = 0;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    User_Information *pUser = &pstPriInfo->stUserInfo[pPoper->iPopId];
    unsigned int len = pFrameInfo->frameParam.frameLen;
    char *data = (char *)pFrameInfo->frameBuf;

    MEDIA_DATA_HEADER header;

    int offset = 0;
    int payloadSize = 0;

    while (offset < len)
    {
        if (offset + PAYLOAD_SIZE < len)
            payloadSize = PAYLOAD_SIZE;
        else
            payloadSize = len - offset;

        FRAME_ENTRY respEntry;
        respEntry.pFrame = anj_mw_malloc(payloadSize + 1024);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while prepare video frame!!!\n");
        }
        else
        {
            iRet = snprintf(respEntry.pFrame, 1024,
                            "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                            "<%s>\n"
                            "<MESSAGE_HEADER\nMsg_type=\"MEDIA_DATA_MESSAGE\"\nMsg_code=\"4\"\nMsg_flag=\"0\"\nMsg_channel=\"%d\"\n"
                            "/>\n"
                            "<MESSAGE_BODY>\n"
                            "<POS\n"
                            "StartPos=\"0\"\n"
                            "DataLen=\"%d\"\n"
                            "/>\n"
                            "</MESSAGE_BODY>\n"
                            "</%s>",
                            anj_pri_xml_name_get(pPoper->iPopId), pPoper->iPopCh, payloadSize + sizeof(MEDIA_DATA_HEADER), anj_pri_xml_name_get(pPoper->iPopId));

            char *dataptr = respEntry.pFrame + iRet;

            dataptr[0] = 0;
            dataptr[1] = 0;
            dataptr[2] = 0;
            dataptr[3] = 0;

            dataptr += 4;
            if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
            {
                header.frame_type = 1;
                pUser->m_last_keyframe_timeoff = (pFrameInfo->frameParam.frameTime - pPoper->tDayStartTime) * 1000;
            }
            else
            {
                header.frame_type = 0;
            }

            header.keyframe_timestamp = pUser->m_last_keyframe_timeoff;
            header.frame_timestamp = (pFrameInfo->frameParam.frameTime - pPoper->tDayStartTime) * 1000;
            header.pack_seq = pUser->m_packseq++;
            header.payload_size = payloadSize;

            header.pack_type = 0;
            if (offset == 0)
                header.pack_type |= 0x01;
            if (offset + PAYLOAD_SIZE >= len)
                header.pack_type |= 0x10;

            header.stream_type = 0;
            header.stream_index = 0;
            header.frame_index = pFrameInfo->frameParam.vframeIndex;

            memcpy(dataptr, &header, sizeof(MEDIA_DATA_HEADER));
            memcpy(dataptr + sizeof(MEDIA_DATA_HEADER), data + offset, payloadSize);

            respEntry.nFrameLen = iRet + 4 + sizeof(MEDIA_DATA_HEADER) + payloadSize;
            respEntry.nFlag = 1;
            frame_mgr_push(&pUser->bufMgr, &respEntry);

            offset += payloadSize;
        }
    }

    if ((pPoper->tLastPts > 0) && (pFrameInfo->frameParam.framePts > pPoper->tLastPts))
    {
        unsigned long long tNowMs = anj_mw_get_cputime_ms(NULL);
        unsigned int iDiffTime = (pFrameInfo->frameParam.framePts - pPoper->tLastPts) / 90;
        if (pUser->pb_download)
        {
            iDiffTime = 10;
        }
        else
        {
            if (pPoper->iSpeed > PB_SPEED_0)
            {
                iDiffTime = iDiffTime / pPoper->iSpeed;
                if (pPoper->iSpeed > PB_SPEED_2)
                {
                    iDiffTime = iDiffTime / (pPoper->iSpeed / PB_SPEED_4);
                }
            }
        }

        /* Large PTS jumps are typically segment gaps, not playback gaps. */
        if ((pUser->pb_download == 0) && (iDiffTime > 1000))
        {
            iDiffTime = 0;
        }

        if ((iDiffTime > 0) && (pPoper->tSendTime > 0) && (tNowMs > pPoper->tSendTime))
        {
            unsigned long long iElapsedTime = tNowMs - pPoper->tSendTime;
            if (iElapsedTime >= iDiffTime)
            {
                iDiffTime = 0;
            }
            else
            {
                iDiffTime -= iElapsedTime;
            }
        }

        if (iDiffTime == 0)
        {
        }
        else if (iDiffTime < 10)
        {
            usleep(10 * 1000);
        }
        else
        {
            usleep((iDiffTime - 1) * 1000);
        }
    }
    else
    {
        usleep(10 * 1000);
    }

    pPoper->tLastPts = pFrameInfo->frameParam.framePts;
    pPoper->tSendTime = anj_mw_get_cputime_ms(NULL);

    return 0;
}

int anj_pri_cmd_replay_audio(rec_pb_poper *pPoper, media_frame_info_t *pFrameInfo)
{
    if (NULL == pPoper || pPoper->iPopId < 0 || pPoper->iPopId > MAX_USER_SESSION || NULL == pFrameInfo)
    {
        return -1;
    }
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    User_Information *pUser = &pstPriInfo->stUserInfo[pPoper->iPopId];
    unsigned int len = pFrameInfo->frameParam.frameLen;
    char *data = (char *)pFrameInfo->frameBuf;
    MEDIA_DATA_HEADER header;

    int offset = 0;
    int payloadSize = 0;
    int iRet;

    while (offset < len)
    {
        if (offset + PAYLOAD_SIZE < len)
            payloadSize = PAYLOAD_SIZE;
        else
            payloadSize = len - offset;

        FRAME_ENTRY respEntry;
        respEntry.pFrame = malloc(payloadSize + 1024);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while prepare audio frame!!!\n");
        }
        else
        {
            iRet = snprintf(respEntry.pFrame, 1024,
                            "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                            "<%s>\n"
                            "<MESSAGE_HEADER\nMsg_type=\"MEDIA_DATA_MESSAGE\"\nMsg_code=\"4\"\nMsg_flag=\"0\"\nMsg_channel=\"%d\"\n"
                            "/>\n"
                            "<MESSAGE_BODY>\n"
                            "<POS\n"
                            "StartPos=\"0\"\n"
                            "DataLen=\"%d\"\n"
                            "/>\n"
                            "</MESSAGE_BODY>\n"
                            "</%s>",
                            anj_pri_xml_name_get(pPoper->iPopId), pPoper->iPopCh, payloadSize + sizeof(MEDIA_DATA_HEADER), anj_pri_xml_name_get(pPoper->iPopId));

            char *dataptr = respEntry.pFrame + iRet;

            dataptr[0] = 0;
            dataptr[1] = 0;
            dataptr[2] = 0;
            dataptr[3] = 0;

            dataptr += 4;

            header.keyframe_timestamp = (pFrameInfo->frameParam.frameTime - pPoper->tDayStartTime) * 1000;
            ;
            header.frame_timestamp = header.keyframe_timestamp;
            header.pack_seq = pUser->m_packseq++;
            header.payload_size = payloadSize;

            header.pack_type = 0;
            if (offset == 0)
                header.pack_type |= 0x01;
            if (offset + PAYLOAD_SIZE >= len)
                header.pack_type |= 0x10;

            header.frame_type = 1;

            header.stream_type = 1;
            header.stream_index = 1;
            header.frame_index = pFrameInfo->frameParam.aframeIndex;

            memcpy(dataptr, &header, sizeof(MEDIA_DATA_HEADER));
            memcpy(dataptr + sizeof(MEDIA_DATA_HEADER), data + offset, payloadSize);

            respEntry.nFrameLen = iRet + 4 + sizeof(MEDIA_DATA_HEADER) + payloadSize;
            respEntry.nFlag = 1;
            frame_mgr_push(&pUser->bufMgr, &respEntry);

            offset += payloadSize;
        }
    }

    return 0;
}

static int anj_pri_cmd_replay_cb(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e EventID)
{
    int iRet = -1;
    if (pHandle == NULL)
    {
        __ERR("input param invalid");
        return iRet;
    }
    if (0 == anj_record_pb_is_valid(pHandle))
    {
        __ERR("Invalid poper %p\n", pHandle);
        return iRet;
    }

    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;
    if (pPoper->iPopId < 0 || pPoper->iPopId > MAX_USER_SESSION || NULL == pFrameInfo)
    {
        __ERR("input param invalid");
        return iRet;
    }
    User_Information *pUser = &pstPriInfo->stUserInfo[pPoper->iPopId];
    if (PB_CB_NONE == EventID)
    {
        FRAME_ENTRY respEntry;
        respEntry.pFrame = anj_mw_malloc(1024 * 4);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while handle REPLAY_CONTROL_MESSAGE !!!\n");
            return iRet;
        }
        else
        {
            char msgbody[1024] = {0};
            char videoParam[512] = {0};
            char audioParam[256] = {0};
            VIDEO_PARAM stVideoParam = {0};
            AUDIO_PARAM stAudioParam = {0};

            if (pPoper->tPbMediaParam.vcodecType == MEDIA_CODEC_VIDEO_H264)
            {
                strncpy(stVideoParam.video_encoder, "H264", sizeof(stVideoParam.video_encoder) - 1);
            }
            else
            {
                strncpy(stVideoParam.video_encoder, "H265", sizeof(stVideoParam.video_encoder) - 1);
            }
            stVideoParam.height = pPoper->tPbMediaParam.height;
            stVideoParam.width = pPoper->tPbMediaParam.width;
            stVideoParam.framerate = pPoper->tPbMediaParam.framerate;
            stVideoParam.intraframerate = pPoper->tPbMediaParam.gop;
            stVideoParam.bitrate = pPoper->tPbMediaParam.bitrate;
            stVideoParam.config_len = pPoper->tPbMediaParam.metaLen;
            __INFO("wh:%ld %ld encoder:%s meta_len:%d framerate:%d gop:%d bitrate:%d\n",
                   stVideoParam.width, stVideoParam.height, stVideoParam.video_encoder, stVideoParam.config_len,
                   pPoper->tPbMediaParam.framerate, pPoper->tPbMediaParam.gop, pPoper->tPbMediaParam.bitrate);
            memcpy(stVideoParam.config, pPoper->tPbMediaParam.metaData, stVideoParam.config_len);

            if (pPoper->tPbMediaParam.sampleRate != 0)
            {
                if (pPoper->tPbMediaParam.acodecType == MEDIA_CODEC_AUDIO_G711A)
                {
                    strncpy(stAudioParam.audio_encoder, "G711A", sizeof(stAudioParam.audio_encoder) - 1);
                }
                else if (pPoper->tPbMediaParam.acodecType == MEDIA_CODEC_AUDIO_G711U)
                {
                    strncpy(stAudioParam.audio_encoder, "G711U", sizeof(stAudioParam.audio_encoder) - 1);
                }
                else if (pPoper->tPbMediaParam.acodecType == MEDIA_CODEC_AUDIO_AAC)
                {
                    strncpy(stAudioParam.audio_encoder, "AAC", sizeof(stAudioParam.audio_encoder) - 1);
                }
                else
                {
                    strncpy(stAudioParam.audio_encoder, "PCM", sizeof(stAudioParam.audio_encoder) - 1);
                }
                stAudioParam.samplerate = pPoper->tPbMediaParam.sampleRate;
                stAudioParam.bitrate = 64000;
                stAudioParam.channels = pPoper->tPbMediaParam.channels;
                stAudioParam.samplebitswidth = pPoper->tPbMediaParam.bitWidth;
                stAudioParam.framerate = 25;
                __INFO("samplerate:%ld channels:%ld samplebitswidth:%ld encoder:%s\n",
                       stAudioParam.samplerate, stAudioParam.channels, stAudioParam.samplebitswidth, stAudioParam.audio_encoder);
            }

            anj_base64_encode((unsigned char *)&stVideoParam, sizeof(VIDEO_PARAM), videoParam, sizeof(videoParam));
            anj_base64_encode((unsigned char *)&stAudioParam, sizeof(AUDIO_PARAM), audioParam, sizeof(audioParam));

            int msg_code = pPoper->bSeek ? ACTION_SEEK : ACTION_PLAY;
            snprintf(msgbody, 1024,
                     "<RESPONSE_PARAM\n"
                     "VideoSeconds=\"%d\"\n"
                     "VideoParam=\"%s\"\n"
                     "AudioParam=\"%s\"\n"
                     "/>\n",
                     600, videoParam, audioParam);

            snprintf(respEntry.pFrame, 1024 * 4,
                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                     "<%s>\n"
                     "<MESSAGE_HEADER\nMsg_type=\"TIMELINE_REPLAY_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\nMsg_channel=\"%d\"\n"
                     "/>\n"
                     "<MESSAGE_BODY>\n"
                     "%s</MESSAGE_BODY>\n"
                     "</%s>",
                     anj_pri_xml_name_get(pPoper->iPopId), msg_code, 0, pPoper->iPopCh, msgbody, anj_pri_xml_name_get(pPoper->iPopId));

            respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
            respEntry.nFlag = 1;
            frame_mgr_push(&pUser->bufMgr, &respEntry);

            __INFO("Send Video and Audio params over.\n");
        }
        iRet = 0;
    }
    else if (PB_CB_START == EventID)
    {
        if ((NULL == pFrameInfo) || (NULL == pFrameInfo->frameBuf) || (0 >= pFrameInfo->frameParam.frameLen) || (REC_MAX_FRAME_BUF_SIZE < pFrameInfo->frameParam.frameLen))
        {
            __ERR("Invalid Input Frame\n");
            return iRet;
        }
        if (pPoper->iSpeed < 1)
        {
            pPoper->iSpeed = 1;
        }
        else if (pPoper->iSpeed > 8)
        {
            pPoper->iSpeed = 8;
        }

        if (pPoper->iSpeed > 1)
        {
            if (pPoper->iSpeed > 4)
            {
                if (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I)
                {
                    anj_pri_cmd_replay_video(pPoper, pFrameInfo);
                }
            }
            else
            {
                if (pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A)
                {
                    anj_pri_cmd_replay_video(pPoper, pFrameInfo);
                }
            }
        }
        else
        {
            if (pFrameInfo->frameParam.frameType != MEDIA_AFRAME_A)
            {
                anj_pri_cmd_replay_video(pPoper, pFrameInfo);
            }
            else
            {
                anj_pri_cmd_replay_audio(pPoper, pFrameInfo);
            }
        }
        iRet = 0;
    }
    else if (PB_CB_FINISH == EventID)
    {
        iRet = 0;
        __INFO("pb cb End\n");
    }
    else
    {
        __ERR("pb cb error\n");
    }

    return iRet;
}

int anj_pri_cmd_proc_replay(int lognum, IXML_Document *pDoc, char *MsgCode, int channel)
{
    int err_code = 0;
    int msg_code = atoi(MsgCode);
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();

    __INFO("Got TIMELINE_REPLAY_CONTROL_MESSAGE, code = %d\n", msg_code);

    int32_t playPos = 0;
    int playspeed = 1;

    switch (msg_code)
    {
    case ACTION_PLAY:
    case ACTION_SEEK:
    {
        char *timestamp_param = GetRequestParamValue(pDoc, (char *)"PlayTimestampInsec");

        if (timestamp_param)
        {
            playPos = atoi(timestamp_param);
            anj_mw_free(timestamp_param);
        }

        __INFO("msg_code= %d, sensorid = %d, channel:%d playPos=%d\n", msg_code, pstPriInfo->stUserInfo[lognum].sensor_id, channel, playPos);

        if (msg_code == ACTION_PLAY)
        {
            if (NULL == pstPriInfo->stUserInfo[lognum].pHandle[channel])
            {
                pstPriInfo->stUserInfo[lognum].pHandle[channel] = (void *)anj_record_pb_create(channel, playPos, 0, 0, lognum, anj_pri_cmd_replay_cb);
            }
            if (pstPriInfo->stUserInfo[lognum].pHandle[channel])
            {
                err_code = 0;
            }
            else
            {
                err_code = -1;
            }
        }
        else
        {
            if (pstPriInfo->stUserInfo[lognum].pHandle[channel])
            {
                err_code = anj_record_pb_seek((REC_HANDLE)pstPriInfo->stUserInfo[lognum].pHandle[channel], playPos);
            }
            else
            {
                err_code = -1;
            }
        }

        __INFO("error code:%d.\n", err_code);
        anj_pri_cmd_replay_send(lognum, "TIMELINE_REPLAY_CONTROL_MESSAGE", msg_code, err_code);

        break;
    }
    }

    char *speed_param = GetRequestParamValue(pDoc, (char *)"PlayParam");
    if (speed_param)
    {
        playspeed = atoi(speed_param);
        if (playspeed < 1 || playspeed > 32)
        {
            playspeed = 1;
        }
        anj_mw_free(speed_param);
    }

    switch (msg_code)
    {

    case ACTION_STOP:
    {
        anj_record_pb_release((REC_HANDLE)pstPriInfo->stUserInfo[lognum].pHandle[channel]);
        pstPriInfo->stUserInfo[lognum].pHandle[channel] = NULL;
        __INFO("ACTION_STOP\n");

        anj_pri_cmd_replay_send(lognum, "TIMELINE_REPLAY_CONTROL_MESSAGE", msg_code, err_code);

        break;
    }
    case ACTION_PAUSE: // pause
        if (pstPriInfo->stUserInfo[lognum].pHandle[channel])
        {
            err_code = anj_record_pb_pause_set((REC_HANDLE)pstPriInfo->stUserInfo[lognum].pHandle[channel], 1);
        }
        else
        {
            err_code = -1;
        }

        break;
    case ACTION_FAST: // fast
    case ACTION_SLOW: // slow
        if (pstPriInfo->stUserInfo[lognum].pHandle[channel])
        {
            err_code = anj_record_pb_speed_set((REC_HANDLE)pstPriInfo->stUserInfo[lognum].pHandle[channel], playspeed);
        }
        else
        {
            err_code = -1;
        }

        break;

    case ACTION_FRAMESKIP: // frame

        break;
    case ACTION_RESUME: // resume
        if (pstPriInfo->stUserInfo[lognum].pHandle[channel])
        {
            err_code = anj_record_pb_pause_set((REC_HANDLE)pstPriInfo->stUserInfo[lognum].pHandle[channel], 0);
        }
        else
        {
            err_code = -1;
        }

        break;
    default:
        break;
    }

    return 0;
}

int anj_pri_cmd_heartbeat(int lognum, IXML_Document *pDoc, char *MsgCode)
{
    if (strcmp(MsgCode, "CMD_HEARTBEAT") == 0)
    {
        FRAME_ENTRY respEntry;
        anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
        respEntry.pFrame = anj_mw_malloc(1024);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while got heartbeat message!\n");
        }
        else
        {
            snprintf(respEntry.pFrame, 1024,
                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                     "<%s>\n"
                     "<MESSAGE_HEADER\nMsg_type=\"AUXPTZ_HEARTBEAT_MESSAGE\"\nMsg_code=\"CMD_HEARTBEAT\"\nMsg_flag=\"%d\"\n"
                     "/>\n"
                     "<MESSAGE_BODY>\n"
                     "</MESSAGE_BODY>\n"
                     "</%s>",
                     anj_pri_xml_name_get(lognum), 0, anj_pri_xml_name_get(lognum));

            respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
            respEntry.nFlag = 1;
            frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
        }
    }
    return 0;
}

int anj_pri_cmd_proc_userauth(IXML_Document *pDoc, char *MsgType, char *MsgCode, int lognum)
{
    int ret_login = -1;
    char group[256] = {0};
    char sessionid[256] = {0};
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    FRAME_ENTRY respEntry;
    respEntry.pFrame = anj_mw_malloc(AUTH_MSG_SIZE);
    if (respEntry.pFrame == NULL)
    {
        __ERR("malloc failed!\n");
        return -1;
    }

    char *VendorId = GetRequestParamValueByName(pDoc, (char *)"VENDOR_PARAM", (char *)"VendorId");
    char *Username = GetRequestParamValueByName(pDoc, (char *)"USER_AUTH_PARAM", (char *)"Username");
    char *Password = GetRequestParamValueByName(pDoc, (char *)"USER_AUTH_PARAM", (char *)"Password");
    char *AuthMethod = GetRequestParamValueByName(pDoc, (char *)"USER_AUTH_PARAM", (char *)"AuthMethod");
    char *MyVersion = GetRequestParamValueByName(pDoc, (char *)"USER_AUTH_PARAM", (char *)"myversion");

    if (MyVersion)
    {
        pstPriInfo->stUserInfo[lognum].clientversion = atoi(MyVersion);
        anj_mw_free(MyVersion);
    }

    if (Username && Password)
    {
        if (VendorId)
            ret_login = UserAuthLoginEx(0, VendorId, Username, Password, group, sessionid);
        else if (AuthMethod)
            ret_login = UserAuthLoginEx(atoi(AuthMethod), (char *)"", Username, Password, group, sessionid);
        else
            ret_login = UserAuthLogin(Username, Password, group, sessionid);
    }
    if (Username)
        anj_mw_free(Username);
    if (Password)
        anj_mw_free(Password);
    if (AuthMethod)
        anj_mw_free(AuthMethod);
    if (VendorId)
        anj_mw_free(VendorId);

    if (!ret_login)
    {
        // login success

        snprintf(respEntry.pFrame, AUTH_MSG_SIZE,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"USER_AUTH_MESSAGE\"\nMsg_code=\"CMD_USER_AUTH\"\nMsg_flag=\"0\"\n"
                 "/>\n"
                 "<MESSAGE_BODY>\n"
                 "<USER_AUTH_RESPONSE\nSessionid=\"%s\"\nGroup=\"%s\" myversion=\"1\" \n"
                 "/>\n"
                 "</MESSAGE_BODY>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum), sessionid, group, anj_pri_xml_name_get(lognum));

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);

        strcpy(pstPriInfo->stUserInfo[lognum].session, sessionid);
    }
    else
    {
        // login failed
        snprintf(respEntry.pFrame, 1024,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"USER_AUTH_MESSAGE\"\nMsg_code=\"CMD_USER_AUTH\"\nMsg_flag=\"-1\"\n"
                 "/>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum), anj_pri_xml_name_get(lognum));

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 2;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }

    return 0;
}

static void anj_pri_cmd_parse_ptz_param(IXML_Node *pNode, PtzCmdParse *pstPtzCmdParse)
{
    while (pNode)
    {
        __INFO("pNode->nodeName: %s\n", pNode->nodeName);
        if (strcmp(pNode->nodeName, "cmd") == 0)
        {
            if (pNode->firstChild)
            {
                StrCpy(pstPtzCmdParse->ptzCmd, sizeof(pstPtzCmdParse->ptzCmd), pNode->firstChild->nodeValue);
            }
        }
        else if (strcmp(pNode->nodeName, "panspeed") == 0)
        {
            if (pNode->firstChild)
            {
                pstPtzCmdParse->panSpeed = Str2Num(pNode->firstChild->nodeValue);
            }
        }
        else if (strcmp(pNode->nodeName, "tiltspeed") == 0)
        {
            if (pNode->firstChild)
            {
                pstPtzCmdParse->tiltSpeed = Str2Num(pNode->firstChild->nodeValue);
            }
        }
        else if (strcmp(pNode->nodeName, "preset") == 0)
        {
            pstPtzCmdParse->presetID = 0;
            if (pNode->firstChild)
            {
                pstPtzCmdParse->presetID = Str2Num(pNode->firstChild->nodeValue);
            }
        }
        else if (strcmp(pNode->nodeName, "flag") == 0)
        {
            if (pNode->firstChild)
            {
                pstPtzCmdParse->flag = Str2Num(pNode->firstChild->nodeValue);
            }
        }
        else if (strcmp(pNode->nodeName, "r") == 0)
        {
            __INFO("r UNSET!\n");
        }
        else if (strcmp(pNode->nodeName, "data") == 0)
        {
            if (pNode->firstChild)
            {
                __ERR("got transparency ptz control data: %s\n",
                      pNode->firstChild->nodeValue);
                int len = strlen(pNode->firstChild->nodeValue);
                pstPtzCmdParse->trans.datalen = 0;

                for (int i = 0; i < len / 2; i++)
                {
                    if (i >= MAX_TRANSPARENT_CMD)
                        break;

                    pstPtzCmdParse->trans.buffer[i] = GetHexValue(pNode->firstChild->nodeValue + 2 * i);
                    pstPtzCmdParse->trans.datalen++;
                }
            }
        }
        else if (strcmp(pNode->nodeName, "duration") == 0)
        {
            pstPtzCmdParse->watchGuardTime = 0;
            if (pNode->firstChild)
            {
                pstPtzCmdParse->watchGuardTime = Str2Num(pNode->firstChild->nodeValue);
            }
        }
        else if (strcmp(pNode->nodeName, "name") == 0)
        {
            if (pNode->firstChild)
            {
                StrCpy(pstPtzCmdParse->presetName, sizeof(pstPtzCmdParse->presetName), pNode->firstChild->nodeValue);
            }
        }

        pNode = pNode->nextSibling;
    }
}

static int anj_pri_cmd_proc_ptz(int lognum, IXML_Document *pDoc, char *MsgCode)
{
    char retbuffer[MAX_PTZ_CMD_LEN] = {0};
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();

    IXML_NodeList *pNodelist = NULL;
    pNodelist = ixmlDocument_getElementsByTagName(pDoc, (char *)"xml");

    if (pNodelist == NULL)
    {
        // bad msg header
        ixmlDocument_free(pDoc);
        return -2;
    }
    else
    {
        EventResult event_result = {0};
        PtzCmdParse stPtzCmdParse = {0};
        anj_pri_cmd_parse_ptz_param(pNodelist->nodeItem->firstChild, &stPtzCmdParse);
        event_data_s evnt_data = {0};
        evnt_data.data = retbuffer;
        evnt_data.len = sizeof(retbuffer);
        event_result.result = (void *)&evnt_data;
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_HANDLE, &event_result, (void *)&stPtzCmdParse);

        ixmlNodeList_free(pNodelist);

        if (lognum >= 0)
        {
            int buflen = MAX_PTZ_CMD_LEN << 1;
            FRAME_ENTRY respEntry;
            respEntry.pFrame = anj_mw_malloc(buflen);
            if (respEntry.pFrame == NULL)
            {
                __ERR("respEntry.pFrame == NULL while handle ptz command!!!\n");
            }
            else
            {
                snprintf(respEntry.pFrame, buflen,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"PTZ_CONTROL_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"0\"\n"
                         "/>\n"
                         "<MESSAGE_BODY>\n%s\n"
                         "</MESSAGE_BODY>\n"
                         "</%s>",
                         anj_pri_xml_name_get(lognum), MsgCode,
                         (strlen(retbuffer) > 0) ? "" : retbuffer, anj_pri_xml_name_get(lognum));

                respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
                respEntry.nFlag = 1;
                frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
            }
        }
        else
        {
            __INFO("ptz control message from other process dont need response\n");
        }
    }
    return 0;
}

static int anj_pri_cmd_get_syscfg(int lognum, IXML_Document *pDoc, char *MsgCode, int channel)
{
    int iRet = 0;
    int len = 0;
    int config_key = 0;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();

    char *buffer = (char *)anj_mw_malloc(50 * 1024);
    if (buffer == NULL)
    {
        __ERR("buffer malloc fail!\n");
        return -1;
    }

    config_key = atoi(MsgCode);

    __ERR("Got SYSTEM_CONFIG_GET_MESSAGE, code = %d\n", config_key);

    FRAME_ENTRY respEntry;
    respEntry.pFrame = NULL;

    iRet = anj_service_task_system_get(config_key, buffer, &len, channel);

    int nFrameLen = ANJ_ALIGN_UP(len + 1024, 1024);
    respEntry.pFrame = anj_mw_malloc(nFrameLen);
    memset(respEntry.pFrame, 0, nFrameLen);
    if (respEntry.pFrame == NULL)
    {
        __ERR("respEntry.pFrame == NULL while handle SYSTEM_CONFIG_GET_MESSAGE!!!\n");
    }
    else
    {
        snprintf(respEntry.pFrame, nFrameLen,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONFIG_GET_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"%d\"\n"
                 "/>\n"
                 "<MESSAGE_BODY>\n"
                 "%s\n"
                 "</MESSAGE_BODY>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum),
                 MsgCode, iRet, (buffer == NULL) ? "" : buffer,
                 anj_pri_xml_name_get(lognum));
    }

    if (buffer != NULL)
        anj_mw_free(buffer);

    if (respEntry.pFrame != NULL)
    {
        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        __ERR("%s\n", respEntry.pFrame);
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }

    return 0;
}

int anj_pri_cmd_set_syscfg(char *cmdbuf, int lognum, char *MsgCode, int channel)
{
    int iRet = 0;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    char *buffer = (char *)anj_mw_malloc(MAX_SET_XML_LEN);
    if (buffer == NULL)
    {
        __ERR("malloc %d NULL while handle SYSTEM_CONFIG_SET_MESSAGE!!!\n", MAX_SET_XML_LEN);
        iRet = -1;
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
        memcpy(buffer, start, length);
        buffer[length] = '\0';
        iRet = anj_service_task_system_set(config_key, buffer, channel, MSG_SRC_PRI);
    }
    anj_mw_free(buffer);

    FRAME_ENTRY respEntry;
    respEntry.pFrame = anj_mw_malloc(1024 * 8);
    if (respEntry.pFrame == NULL)
    {
        __ERR("respEntry.pFrame == NULL while handle SYSTEM_CONFIG_SET_MESSAGE!!!\n");
    }
    else
    {
        snprintf(respEntry.pFrame, 1024 * 8,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONFIG_SET_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"%d\"\n"
                 "/>\n"
                 "<MESSAGE_BODY>\n"
                 "</MESSAGE_BODY>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum), MsgCode, ((iRet == 0) ? iRet : -1), anj_pri_xml_name_get(lognum));
        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        __INFO("%s\n", respEntry.pFrame);
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }

    return 0;
}

int anj_pri_cmd_proc_sysctl(char *cmdbuf, int cmdlen, int lognum, IXML_Document *pDoc, char *MsgType, char *MsgCode, int channel)
{
    int msg_code = atoi(MsgCode);
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();

    int iRet = 0;
    char *msg_body = NULL;

    iRet = anj_service_task_sysctl(pDoc, cmdbuf, cmdlen, MsgType, MsgCode, MSG_SRC_PRI, lognum, channel, &msg_body);
    if (iRet != CMD_UNSUPPORT_RESPONSE)
    {
        FRAME_ENTRY respEntry;
        respEntry.pFrame = anj_mw_malloc(1024 * 32);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while handle SYSTEM_CONTROL_MESSAGE, msgcode = %d!!!\n", msg_code);
        }
        else
        {
            snprintf(respEntry.pFrame, 1024 * 32,
                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                     "<%s>\n"
                     "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"%d\"\n"
                     "/>\n"
                     "<MESSAGE_BODY>\n"
                     "%s\n"
                     "</MESSAGE_BODY>\n"
                     "</%s>",
                     anj_pri_xml_name_get(lognum),
                     MsgCode, iRet, (msg_body == NULL) ? "" : msg_body,
                     anj_pri_xml_name_get(lognum));
        }

        __INFO("respEntry.pFrame: %s\n", respEntry.pFrame);

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }
    if (msg_body)
    {
        anj_mw_free(msg_body);
    }
    return 0;
}

static int anj_pri_cmd_parse_ra_stop_param(IXML_Document *pDoc, char *Sessionid)
{
    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDoc, "RA_STOP_PARAM");
    if (pNodelist == NULL || pNodelist->nodeItem == NULL)
    {
        if (pNodelist != NULL)
        {
            ixmlNodeList_free(pNodelist);
        }
        return -2;
    }

    IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
    while (tmp != NULL)
    {
        if (strcmp(tmp->nodeName, "Sessionid") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                strcpy(Sessionid, tmp->nodeValue);
                __INFO("Sessionid  = %s\n", Sessionid);
            }
        }
        tmp = tmp->nextSibling;
    }
    ixmlNodeList_free(pNodelist);
    return 0;
}

static int anj_pri_cmd_parse_ra_param(IXML_Document *pDoc, char *AudioFormat,
                                      char *Channels, char *SampleRate,
                                      char *Bitrate, char *RTPSendPort,
                                      char *MulticastIp, char *ConnType)
{
    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDoc, "RA_START_PARAM");
    if (pNodelist == NULL || pNodelist->nodeItem == NULL)
    {
        if (pNodelist != NULL)
        {
            ixmlNodeList_free(pNodelist);
        }
        return -2;
    }
    else
    {
        IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
        while (tmp != NULL)
        {
            if (strcmp(tmp->nodeName, "AudioFormat") == 0 || strcmp(tmp->nodeName, "AudioForamt") == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    strcpy(AudioFormat, tmp->nodeValue);
                }
            }
            else if (strcmp(tmp->nodeName, "Channels") == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    strcpy(Channels, tmp->nodeValue);
                }
            }
            else if (strcmp(tmp->nodeName, "SampleRate") == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    strcpy(SampleRate, tmp->nodeValue);
                }
            }
            else if (strcmp(tmp->nodeName, "Bitrate") == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    strcpy(Bitrate, tmp->nodeValue);
                }
            }
            else if (strcmp(tmp->nodeName, "RTPSendPort") == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    strcpy(RTPSendPort, tmp->nodeValue);
                }
            }
            else if (strcmp(tmp->nodeName, "MULTICASTIP") == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    strcpy(MulticastIp, tmp->nodeValue);
                }
            }
            else if (strcmp(tmp->nodeName, "ConnectType") == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    strcpy(ConnType, tmp->nodeValue);
                }
            }
            tmp = tmp->nextSibling;
        }
    }
    ixmlNodeList_free(pNodelist);
    return 0;
}

static int anj_pri_cmd_proc_ra_start(IXML_Document *pDoc, char *MsgCode, int lognum)
{
    char AudioFormat[200] = {0};
    char Channels[200] = {0};
    char SampleRate[200] = {0};
    char Bitrate[200] = {0};
    char RTPSendPort[200] = {0};
    char ConnType[200] = {0};
    char MulticastIp[200] = {0};
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();

    if (audio_talk_status_get())
    {
        FRAME_ENTRY respEntry;
        respEntry.pFrame = anj_mw_malloc(1024);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while handle CMD_RA_START\n");
        }
        else
        {
            snprintf(respEntry.pFrame, 1024,
                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                     "<%s>\n"
                     "<MESSAGE_HEADER\nMsg_type=\"REVERSE_AUDIO_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"-1\"\n"
                     "/>\n"
                     "</%s>",
                     anj_pri_xml_name_get(lognum),
                     MsgCode,
                     anj_pri_xml_name_get(lognum));

            __ERR("%s", respEntry.pFrame);

            respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
            respEntry.nFlag = 1;
            frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
        }
        return 0;
    }

    if (anj_pri_cmd_parse_ra_param(pDoc, AudioFormat, Channels, SampleRate, Bitrate, RTPSendPort, MulticastIp, ConnType) < 0)
    {
        return -2;
    }

    {
        int ra_result = 0;
        int samplerate = 0;
        int bitspersample = 0;
        int channels = 0;
        int resp_samplerate = 0;
        int resp_bitspersample = 0;
        int resp_channels = 0;
        media_codec_type_e audiotype = 0;
        int rtprecvport = anj_pri_cmd_port_get(1);

        if (anj_config_audio_param_get(&audiotype, &samplerate, &bitspersample, &channels) < 0)
        {
            __ERR("GetAudioParam failed!!!\n");
            ra_result = -2;
        }
        else
        {
            if (audiotype == MEDIA_CODEC_NONE)
            {
                __ERR("audio disabled, can not start audio talkback!!!\n");
                ra_result = -3;
            }
            else // if audio is disabled, bad param will be sent to UC, fixed here
            {
                __ERR("AudioFormat %s\n", AudioFormat);
                if (strcasecmp(AudioFormat, "PCM") == 0)
                {
                    if (strstr(anj_sysctl_get_capability_string(), FUNCTION_RA_PCM))
                    {
                        audiotype = MEDIA_CODEC_AUDIO_PCM;
                        resp_samplerate = atoi(SampleRate);
                        resp_bitspersample = 16;
                        resp_channels = atoi(Channels);
                    }
                    else
                    {
                        __ERR("function not supported. !!!\n");
                        ra_result = -2;
                    }
                }
                else if (strcasecmp(AudioFormat, "MP3") == 0)
                {
                    if (strstr(anj_sysctl_get_capability_string(), FUNCTION_RA_MP3STREAM))
                    {
                        audiotype = MEDIA_CODEC_AUDIO_MP3;
                        resp_samplerate = atoi(SampleRate);
                        resp_bitspersample = 16;
                        resp_channels = atoi(Channels);
                    }
                    else
                    {
                        __ERR("%s function not supported. !!!\n", FUNCTION_RA_MP3STREAM);
                        ra_result = -2;
                    }
                }
                else
                {
                    resp_samplerate = samplerate;
                    resp_bitspersample = bitspersample;
                    resp_channels = channels;
                }
            }
        }

        if (ra_result == 0)
        {
            if (resp_samplerate <= 0)
            {
                resp_samplerate = samplerate;
            }
        }

        FRAME_ENTRY respEntry;
        respEntry.pFrame = anj_mw_malloc(1024);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while handle CMD_RA_START\n");
        }
        else
        {

            if (ra_result == 0)
            {
                const char *conn_type_attr = (strlen(ConnType) > 0) ? "\nConnectionType=\"1\"" : "";

                if (strlen(ConnType) > 0)
                {
                    __ERR("ConnType = %s, use same port to send audio data!!!\n", ConnType);
                    MediaStreamConfig *pstMediaStreamConfig = (MediaStreamConfig *)getMediaStreamConfig();
                    rtprecvport = pstMediaStreamConfig->commConfig.ptzPort;
                }

                snprintf(respEntry.pFrame, 1024,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"REVERSE_AUDIO_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"0\"\n"
                         "/>\n"
                         "<MESSAGE_BODY>\n"
                         "<RA_START_RESPONSE\n"
                         "RTPRecvPort=\"%d\"\n"
                         "AudioCodec=\"%s\"\n"
                         "Samplerate=\"%d\"\n"
                         "BitsPerSample=\"%d\"\n"
                         "Channels=\"%d\"%s\n"
                         "/>\n"
                         "</MESSAGE_BODY>\n"
                         "</%s>",
                         anj_pri_xml_name_get(lognum),
                         MsgCode,
                         rtprecvport,
                         audio_encode_type_str(audiotype),
                         resp_samplerate,
                         resp_bitspersample,
                         resp_channels,
                         conn_type_attr,
                         anj_pri_xml_name_get(lognum));
            }
            else
            {
                snprintf(respEntry.pFrame, 1024,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"REVERSE_AUDIO_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"%d\"\n"
                         "/>\n"
                         "</%s>",
                         anj_pri_xml_name_get(lognum),
                         MsgCode, ra_result,
                         anj_pri_xml_name_get(lognum));
            }
            respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
            respEntry.nFlag = 1;
            frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);

            if (ra_result == 0)
            {
                audio_talk_start_param_t talk_param = {0};
                struct in_addr mcast_addr;
                int talk_bitrate = atoi(Bitrate);

                talk_param.codec_type = audiotype;
                talk_param.samplerate = resp_samplerate;
                talk_param.bitrate = (talk_bitrate > 0) ? talk_bitrate : 16000;
                talk_param.src_port = atoi(RTPSendPort);
                talk_param.local_port = rtprecvport;
                talk_param.same_port = (strlen(ConnType) > 0);

                if (strlen(MulticastIp) > 0 && inet_aton(MulticastIp, &mcast_addr))
                {
                    talk_param.src_ip = ntohl(mcast_addr.s_addr);
                    talk_param.is_multicast = 1;
                }

                if (audio_talk_start(&talk_param) < 0)
                {
                    __ERR("audio_talk_start failed!\n");
                }
            }
        }
    }

    return 0;
}

static int anj_pri_cmd_proc_ra(char *cmdbuf, int cmdlen, int lognum, IXML_Document *pDoc, char *MsgType, char *MsgCode)
{
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    if (strcmp(MsgCode, "CMD_RA_START") == 0)
    {
        return anj_pri_cmd_proc_ra_start(pDoc, MsgCode, lognum);
    }
    else if (strcmp(MsgCode, "CMD_RA_DATA") == 0)
    {
        __INFO("do nothing!\n");
    }
    else if (strcmp(MsgCode, "CMD_RA_STOP") == 0)
    {
        char Sessionid[MAX_USER_SESSION] = {0};

        __INFO("get CMD_RA_STOP\n");

        if (anj_pri_cmd_parse_ra_stop_param(pDoc, Sessionid) < 0)
        {
            return -2;
        }

        if (strcmp(Sessionid, pstPriInfo->stUserInfo[lognum].session) == 0)
        {
            __ERR("correct sessionid!\n");
            audio_talk_stop();

            FRAME_ENTRY respEntry;
            respEntry.pFrame = anj_mw_malloc(1024);
            if (respEntry.pFrame == NULL)
            {
                __ERR("respEntry.pFrame == NULL while handle %s\n", MsgCode);
            }
            else
            {
                snprintf(respEntry.pFrame, 1024,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"REVERSE_AUDIO_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"0\"\n/>\n"
                         "</%s>",
                         anj_pri_xml_name_get(lognum),
                         MsgCode,
                         anj_pri_xml_name_get(lognum));

                respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
                respEntry.nFlag = 1;
                frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
            }
        }
        else
        {
            __ERR("invalid sessionid, sid=%s, rasessionid=%s\n",
                  Sessionid, pstPriInfo->stUserInfo[lognum].session);

            FRAME_ENTRY respEntry;
            respEntry.pFrame = anj_mw_malloc(1024);
            if (respEntry.pFrame == NULL)
            {
                __ERR("respEntry.pFrame == NULL while handle %s\n", MsgCode);
            }
            else
            {
                snprintf(respEntry.pFrame, 1024,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"REVERSE_AUDIO_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"-1\"\n/>\n"
                         "</%s>",
                         anj_pri_xml_name_get(lognum),
                         MsgCode,
                         anj_pri_xml_name_get(lognum));

                respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
                respEntry.nFlag = 1;
                frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
            }
        }
    }
    else
    {
        FRAME_ENTRY respEntry;
        respEntry.pFrame = anj_mw_malloc(1024);
        if (respEntry.pFrame == NULL)
        {
            __ERR("respEntry.pFrame == NULL while handle %s\n", MsgCode);
        }
        else
        {
            snprintf(respEntry.pFrame, 1024,
                     "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                     "<%s>\n"
                     "<MESSAGE_HEADER\nMsg_type=\"REVERSE_AUDIO_MESSAGE\"\nMsg_code=\"%s\"\nMsg_flag=\"-1\"\n/>\n"
                     "</%s>",
                     anj_pri_xml_name_get(lognum),
                     MsgCode,
                     anj_pri_xml_name_get(lognum));

            respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
            respEntry.nFlag = 1;
            frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
        }
    }

    return 0;
}

int anj_pri_cmd_port_get(int udp_or_tcp)
{
    struct sockaddr_in test_sockaddr;
    int testfd = SOCKETFLAG_NOTUSED;
    int port = START_PORT;

    while (1)
    {
        if (udp_or_tcp) // UDP
            testfd = socket(AF_INET, SOCK_DGRAM, 0);
        else
            testfd = socket(AF_INET, SOCK_STREAM, 0);

        if (testfd < 0)
        {
            __INFO("create sockfd failed!\n");
            continue;
        }

        test_sockaddr.sin_family = AF_INET;
        test_sockaddr.sin_port = htons(port);
        test_sockaddr.sin_addr.s_addr = INADDR_ANY;
        bzero(&(test_sockaddr.sin_zero), 8);

        if (bind(testfd, (struct sockaddr *)&test_sockaddr, sizeof(struct sockaddr)) == -1)
        {
            __INFO("socket bind failed!\n");
            close(testfd);
            testfd = SOCKETFLAG_NOTUSED;

            if (port > START_PORT + 1000)
            {
                __INFO("Can NOT find an valiable port\n");
                break;
            }
            port += 2;
        }
        else
        {
            __INFO("Find port %d\n", port);
            break;
        }
    }

    if (testfd != SOCKETFLAG_NOTUSED)
        close(testfd);

    return port;
}

int anj_pri_cmd_proc(char *cmdbuf, int cmdlen, int lognum, unsigned long ulLeadCode)
{
    int iRet = 0;
    IXML_Document *pDoc = NULL;
    ANJ_CHK((cmdbuf != NULL) && (cmdlen > 0), -1, "input Invalid");

    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    if (anj_sysmng_is_limit_ip(pstPriInfo->stUserInfo[lognum].ip) > 0)
    {
        __ERR("%u now alowed.\n", pstPriInfo->stUserInfo[lognum].ip);
        iRet = -1;
        goto endFunc;
    }

    pDoc = ixmlParseBuffer(cmdbuf);
    if (pDoc == NULL)
    {
        __ERR("pDoc == NULL, xml=\n%s\n", cmdbuf);
        iRet = -1;
        goto endFunc;
    }

    char MsgRoot[256] = {0};
    char MsgType[256] = {0};
    char MsgCode[256] = {0};
    char MsgFlag[256] = {0};
    int channel = -1;
    ANJ_CHK_FUNC(anj_service_cmd_convert_xml(cmdbuf, MsgRoot), 0, "cmdbuf invalid");
    ANJ_CHK_FUNC(anj_service_cmd_parse_xml(pDoc, MsgRoot, MsgType, MsgCode, MsgFlag, &channel), 0, "xml_data invalid");

    if (strcmp(MsgType, "USER_AUTH_MESSAGE") != 0)
    {
        if (!strlen(pstPriInfo->stUserInfo[lognum].session))
        {
            __ERR("not login but got not USER_AUTH_MESSAGE comand, reject it!!!\n");

            FRAME_ENTRY respEntry;
            respEntry.pFrame = anj_mw_malloc(1024 * 2);
            if (respEntry.pFrame == NULL)
            {
                __ERR("respEntry.pFrame == NULL while handle %s message, msgcode = %s!!!\n", MsgType, MsgCode);
            }
            else
            {
                snprintf(respEntry.pFrame, AUTH_MSG_SIZE,
                         "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                         "<%s>\n"
                         "<MESSAGE_HEADER\nMsg_type=\"%s\"\nMsg_code=\"%s\"\nMsg_flag=\"%d\"\n"
                         "/>\n"
                         "<MESSAGE_BODY></MESSAGE_BODY>\n"
                         "</%s>",
                         anj_pri_xml_name_get(lognum),
                         MsgType, MsgCode, -1,
                         anj_pri_xml_name_get(lognum));
                respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
                respEntry.nFlag = 1;
                frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
            }
            goto endFunc;
        }
    }

    if (strcmp(MsgType, "MEDIA_DATA_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        iRet = anj_pri_cmd_proc_media(cmdbuf, cmdlen, lognum, pDoc, MsgCode);
        goto endFunc;
    }
    if (strcmp(MsgType, "TIMELINE_REPLAY_CONTROL_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        iRet = anj_pri_cmd_proc_replay(lognum, pDoc, MsgCode, channel);
        goto endFunc;
    }
    else if (strcmp(MsgType, "AUXPTZ_HEARTBEAT_MESSAGE") == 0)
    {
        iRet = anj_pri_cmd_heartbeat(lognum, pDoc, MsgCode);
        goto endFunc;
    }
    else if (strcmp(MsgType, "USER_AUTH_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        iRet = anj_pri_cmd_proc_userauth(pDoc, MsgType, MsgCode, lognum);
        goto endFunc;
    }
    else if (strcmp(MsgType, "PTZ_CONTROL_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        iRet = anj_pri_cmd_proc_ptz(lognum, pDoc, MsgCode);
        goto endFunc;
    }
    else if (strcmp(MsgType, "SYSTEM_CONFIG_GET_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        iRet = anj_pri_cmd_get_syscfg(lognum, pDoc, MsgCode, channel);
        goto endFunc;
    }
    else if (strcmp(MsgType, "SYSTEM_CONFIG_SET_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        if (CMD_GET_DEFAULT_NETWORK_LAN_CONFIG == atoi(MsgCode))
            iRet = anj_pri_cmd_get_syscfg(lognum, pDoc, MsgCode, channel);
        else
            iRet = anj_pri_cmd_set_syscfg(cmdbuf, lognum, MsgCode, channel);
        goto endFunc;
    }

    else if (strcmp(MsgType, "SYSTEM_CONTROL_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        iRet = anj_pri_cmd_proc_sysctl(cmdbuf, cmdlen, lognum, pDoc, MsgType, MsgCode, channel);
        goto endFunc;
    }

    else if (strcmp(MsgType, "REVERSE_AUDIO_MESSAGE") == 0)
    {
        __INFO("cmdbuf:%s\n", cmdbuf);
        iRet = anj_pri_cmd_proc_ra(cmdbuf, cmdlen, lognum, pDoc, MsgType, MsgCode);
        goto endFunc;
    }
    else
    { // error xml
    }

endFunc:
    if (pDoc)
    {
        ixmlDocument_free(pDoc);
    }
    return iRet;
}

void anj_pri_cmd_wifi_list_reponse(void *ctx, int result, int lognum)
{
    char *msg_body = NULL;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    WIFI_AP_SCAN *apinfos = (WIFI_AP_SCAN *)ctx;
    if (apinfos->apCnt > 0)
    {
        int body_size = apinfos->apCnt * 256 + 256;
        char *pe;
        char *pb;
        char essid_gb_str[MAX_WIRELESS_ESSID_NAME_LEN * 2 + 4];

        msg_body = anj_mw_malloc(body_size);
        pb = msg_body;
        pe = msg_body + body_size - 1;

        pb += snprintf(pb, pe - pb, "<RESPONSE_PARAM ApCount=\"%d\">\n", apinfos->apCnt);
        for (int i = 0; i < apinfos->apCnt; i++)
        {
            int iRet = utf8_to_gb2312(apinfos->apInfos[i].ssid, essid_gb_str);
            if (iRet != 0 && strlen(apinfos->apInfos[i].ssid) > 0)
            {
                StrCpy(essid_gb_str, sizeof(essid_gb_str), apinfos->apInfos[i].ssid);
                __INFO("wifi[%d] essid:%s maybe use gb format, copy for xml!\n", i, essid_gb_str);
            }

            char escapeBuf[4096] = {0};
            pb += snprintf(pb, pe - pb, "<WifiAp\n");
            pb += snprintf(pb, pe - pb, "Ssid=\"%s\"\n", copy_with_escape(escapeBuf, essid_gb_str));
            pb += snprintf(pb, pe - pb, "WirelessMode=\"%s\"\n", apinfos->apInfos[i].wirelessMode);
            pb += snprintf(pb, pe - pb, "AuthMode=\"%s\"\n", anj_net_wifi_auth_str(apinfos->apInfos[i].authMode));
            pb += snprintf(pb, pe - pb, "EncryptType=\"%s\"\n", anj_net_wifi_encrypt_str(apinfos->apInfos[i].encryType));
            pb += snprintf(pb, pe - pb, "Quality=\"%d\"\n", apinfos->apInfos[i].quality);
            pb += snprintf(pb, pe - pb, "SignalLevel=\"%d\"\n", apinfos->apInfos[i].signalLevel);
            pb += snprintf(pb, pe - pb, "NoiseLevel=\"%d\"\n", apinfos->apInfos[i].noiseLevel);
            pb += snprintf(pb, pe - pb, "/>\n");
        }
        pb += snprintf(pb, pe - pb, "</RESPONSE_PARAM>");
    }
    else
    {
        msg_body = anj_mw_malloc(1024);
        strcpy(msg_body, "<RESPONSE_PARAM ApCount=\"0\">\n</RESPONSE_PARAM>");
    }
    FRAME_ENTRY respEntry;
    respEntry.pFrame = anj_mw_malloc(1024 * 32);
    if (respEntry.pFrame == NULL)
    {
        __ERR("respEntry.pFrame == NULL while handle SYSTEM_CONTROL_MESSAGE, msgcode = %d!!!\n", CMD_GET_WIFI_AP_INFO);
    }
    else
    {
        snprintf(respEntry.pFrame, 1024 * 32,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                 "/>\n"
                 "<MESSAGE_BODY>\n"
                 "%s\n"
                 "</MESSAGE_BODY>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum),
                 CMD_GET_WIFI_AP_INFO, result, msg_body,
                 anj_pri_xml_name_get(lognum));

        __ERR("respEntry.pFrame: %s\n", respEntry.pFrame);

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }
    anj_mw_free(msg_body);
}

void anj_pri_cmd_formart_reponse(int result, int lognum)
{
    FRAME_ENTRY respEntry;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    respEntry.pFrame = anj_mw_malloc(1024);
    if (respEntry.pFrame == NULL)
    {
        __ERR("respEntry.pFrame == NULL while handle SYSTEM_CONTROL_MESSAGE, msgcode = %d!!!\n", CMD_STORAGE_DEVICE_FORMAT);
    }
    else
    {
        snprintf(respEntry.pFrame, 1024,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                 "/>\n"
                 "<MESSAGE_BODY>\n"
                 "</MESSAGE_BODY>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum),
                 CMD_STORAGE_DEVICE_FORMAT, result,
                 anj_pri_xml_name_get(lognum));

        __ERR("respEntry.pFrame: %s\n", respEntry.pFrame);

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }
}

void anj_pri_cmd_jpg_reponse(int result, int lognum, char *JpgFile)
{
    FRAME_ENTRY respEntry;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();
    respEntry.pFrame = anj_mw_malloc(1024);
    if (respEntry.pFrame == NULL)
    {
        __ERR("respEntry.pFrame == NULL while handle SYSTEM_CONTROL_MESSAGE, msgcode = %d!!!\n", CMD_SNAP_JPEG_PICTURE);
    }
    else
    {
        snprintf(respEntry.pFrame, 1024,
                 "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\n"
                 "<%s>\n"
                 "<MESSAGE_HEADER\nMsg_type=\"SYSTEM_CONTROL_MESSAGE\"\nMsg_code=\"%d\"\nMsg_flag=\"%d\"\n"
                 "/>\n"
                 "<MESSAGE_BODY>\n"
                 "<RESPONSE_PARAM>\r\nJpgFile=\"%s\"\r\n</RESPONSE_PARAM>\r\n"
                 "</MESSAGE_BODY>\n"
                 "</%s>",
                 anj_pri_xml_name_get(lognum),
                 CMD_SNAP_JPEG_PICTURE, result, JpgFile,
                 anj_pri_xml_name_get(lognum));

        __INFO("respEntry.pFrame: %s\n", respEntry.pFrame);

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;
        frame_mgr_push(&pstPriInfo->stUserInfo[lognum].bufMgr, &respEntry);
    }
}

void anj_pri_alarm_event_notify(void *alarm_data)
{
    alarm_event_data *alarm_event = (alarm_event_data *)alarm_data;
    anj_pri_info *pstPriInfo = (anj_pri_info *)getPriInfo();

    FRAME_ENTRY respEntry;
    respEntry.pFrame = anj_mw_malloc(1024);
    if (respEntry.pFrame == NULL)
    {
        __ERR("respEntry.pFrame == NULL while notify alarm!!!\n");
    }
    else
    {
        char szAlarmData[ALARM_MAX_PAYLOAD_LEN * 2] = {0};
        copy_with_escape(szAlarmData, alarm_event->alarm_payload);

        snprintf(respEntry.pFrame, 1024,
                 "<MESSAGE_HEADER\n"
                 "Msg_type=\"ALARM_REPORT_MESSAGE\"\n"
                 "Msg_code=\"CMD_REPORT_ALARM\"\n"
                 "Msg_flag=\"0\"\n/>\n"
                 "<MESSAGE_BODY>\n"
                 "<ALARM_REPORT_PARAM>\n"
                 "<ALARM_ITEM>\n"
                 "<ALARM_INFO\n"
                 "Alarm_code=\"%d\"\n"
                 "Alarm_flag=\"%d\"\n"
                 "Alarm_level=\"%d\"\n"
                 "Alarm_data=\"%s\"\n"
                 "/>\n"
                 "<ALARM_TIME\nYear=\"%d\"\nMonth=\"%d\"\nDay=\"%d\"\nWDay=\"%d\"\nHour=\"%d\"\nMinute=\"%d\"\nSecond=\"%d\"/>\n"
                 "</ALARM_ITEM>\n"
                 "</ALARM_REPORT_PARAM>\n"
                 "</MESSAGE_BODY>\n",
                 alarm_event->alarm_code,
                 alarm_event->alarm_flag,
                 alarm_event->alarm_level,
                 szAlarmData,
                 alarm_event->year,
                 alarm_event->month,
                 alarm_event->day,
                 alarm_event->day,
                 alarm_event->hour,
                 alarm_event->minute,
                 alarm_event->second);

        respEntry.nFrameLen = strlen(respEntry.pFrame) + 1;
        respEntry.nFlag = 1;

        frame_mgr_push(&pstPriInfo->bufMgr, &respEntry);
    }
}
