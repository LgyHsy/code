#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>

#include "anj_mw_log.h"
#include "anj_mw_comm.h"
#include "anj_config.h"
#include "media_util.h"
#include "librtmp/amf0.h"
#include "librtmp/rtmp-client.h"
#include "anj_rtmp_internal.h"

#define RTMP_CODEC_H264 7
#define RTMP_CODEC_H265 12
#define RTMP_CODEC_G711A 7
#define RTMP_CODEC_G711U 8
#define RTMP_CODEC_AAC 10

#define RTMP_CHUNK_SIZE_V0 10240
#define RTMP_CHUNK_SIZE_V1 4096
#define RTMP_CHUNK_SIZE_A 1024

static uint8_t *s_packet_v = NULL;
static uint32_t s_packet_v_size = 0;
static uint8_t *s_packet_a = NULL;
static uint32_t s_packet_a_size = 0;

static int rtmp_grow_packet(uint8_t **packet, uint32_t *packet_size, uint32_t need_size, uint32_t chunk_size)
{
    if (*packet_size >= need_size)
    {
        return 0;
    }

    free(*packet);
    *packet = NULL;
    *packet_size = ((need_size / chunk_size) + 1) * chunk_size;
    *packet = (uint8_t *)malloc(*packet_size);
    return (*packet == NULL) ? -1 : 0;
}

static int rtmp_publish_video_packet(anj_rtmp_session_t *session, unsigned char *data, uint32_t data_size,
                                     int is_key, uint32_t ts)
{
    unsigned char *packet = NULL;
    uint32_t packet_size = 0;
    uint32_t chunk_size;
    int i = 0;

    if (session == NULL || session->client == NULL)
    {
        return -1;
    }

    chunk_size = (session->stream_no == ANJ_RTMP_MAIN_STREAM) ? RTMP_CHUNK_SIZE_V0 : RTMP_CHUNK_SIZE_V1;
    if (rtmp_grow_packet(&s_packet_v, &s_packet_v_size, data_size + 9, chunk_size) != 0)
    {
        return -1;
    }
    packet = s_packet_v;

    if (is_key)
    {
        packet[i++] = (session->video_codec_id == RTMP_CODEC_H265) ? 0x1C : 0x17;
    }
    else
    {
        packet[i++] = (session->video_codec_id == RTMP_CODEC_H265) ? 0x2C : 0x27;
    }
    packet[i++] = 0x01;
    packet[i++] = 0x00;
    packet[i++] = 0x00;
    packet[i++] = 0x00;
    packet[i++] = (unsigned char)(data_size >> 24);
    packet[i++] = (unsigned char)(data_size >> 16);
    packet[i++] = (unsigned char)(data_size >> 8);
    packet[i++] = (unsigned char)(data_size & 0xff);
    memcpy(&packet[i], data, data_size);
    packet_size = i + data_size;

    return rtmp_client_push_video(session->client, packet, packet_size, ts);
}

static int rtmp_publish_video_sps_pps(anj_rtmp_session_t *session, unsigned char *data, uint32_t frame_len, uint32_t ts)
{
    char sps[256] = {0};
    char pps[256] = {0};
    char vps[256] = {0};
    int sps_len = 0;
    int pps_len = 0;
    int vps_len = 0;
    unsigned char u_sps[256] = {0};
    unsigned char u_pps[256] = {0};
    unsigned char u_vps[256] = {0};
    unsigned char video_info[512] = {0};
    unsigned char *ptr = video_info;
    int i;

    if (session == NULL || session->client == NULL)
    {
        return -1;
    }

    if (session->video_codec_id == RTMP_CODEC_H265)
    {
        h265_get_vps_sps_pps_sei((char *)data, (int)frame_len, 0, 0, vps, &vps_len, sps, &sps_len, pps, &pps_len, NULL, NULL);
    }
    else
    {
        h264_get_sps_pps_sei((char *)data, (int)frame_len, 0, 0, sps, &sps_len, pps, &pps_len, NULL, NULL);
    }

    for (i = 0; i < sps_len; i++)
    {
        u_sps[i] = (unsigned char)sps[i];
    }
    for (i = 0; i < pps_len; i++)
    {
        u_pps[i] = (unsigned char)pps[i];
    }
    for (i = 0; i < vps_len; i++)
    {
        u_vps[i] = (unsigned char)vps[i];
    }

    if (session->video_codec_id == RTMP_CODEC_H265)
    {
        *(ptr++) = 0x1C;
        *(ptr++) = 0x00;
        *(ptr++) = 0x00;
        *(ptr++) = 0x00;
        *(ptr++) = 0x00;
        *(ptr++) = 0x01;
        *(ptr++) = u_sps[1];
        *(ptr++) = u_sps[2];
        *(ptr++) = u_sps[3];
        *(ptr++) = 0x03;
        *(ptr++) = 0xE1;
        *(ptr++) = (unsigned char)(sps_len >> 8);
        *(ptr++) = (unsigned char)(sps_len & 0xff);
        memcpy(ptr, u_sps, (size_t)sps_len);
        ptr += sps_len;
        *(ptr++) = 0x01;
        *(ptr++) = (unsigned char)(pps_len >> 8);
        *(ptr++) = (unsigned char)(pps_len & 0xff);
        memcpy(ptr, u_pps, (size_t)pps_len);
        ptr += pps_len;
        *(ptr++) = 0x01;
        *(ptr++) = (unsigned char)(vps_len >> 8);
        *(ptr++) = (unsigned char)(vps_len & 0xff);
        memcpy(ptr, u_vps, (size_t)vps_len);
        ptr += vps_len;
    }
    else
    {
        *(ptr++) = 0x17;
        *(ptr++) = 0x00;
        *(ptr++) = 0x00;
        *(ptr++) = 0x00;
        *(ptr++) = 0x00;
        *(ptr++) = 0x01;
        *(ptr++) = u_sps[1];
        *(ptr++) = u_sps[2];
        *(ptr++) = u_sps[3];
        *(ptr++) = 0xff;
        *(ptr++) = 0xE1;
        *(ptr++) = (unsigned char)(sps_len >> 8);
        *(ptr++) = (unsigned char)(sps_len & 0xff);
        memcpy(ptr, u_sps, (size_t)sps_len);
        ptr += sps_len;
        *(ptr++) = 0x01;
        *(ptr++) = (unsigned char)(pps_len >> 8);
        *(ptr++) = (unsigned char)(pps_len & 0xff);
        memcpy(ptr, u_pps, (size_t)pps_len);
        ptr += pps_len;
    }

    return rtmp_client_push_video(session->client, video_info, (size_t)(ptr - video_info), ts);
}

static int rtmp_publish_video_frame(anj_rtmp_session_t *session, unsigned char *data, uint32_t frame_len,
                                    int is_key, uint32_t ts)
{
    uint32_t index;
    uint32_t start_index = 0;
    int ret = 0;

    if (session == NULL || session->client == NULL)
    {
        return -1;
    }

    if (session->rtmp_type == 1)
    {
        return rtmp_publish_video_packet(session, data, frame_len, is_key, ts);
    }

    for (index = 0; index < frame_len; index++)
    {
        if (frame_len - index <= 4)
        {
            index = frame_len;
            break;
        }

        if (data[index] == 0 && data[index + 1] == 0 && data[index + 2] == 0 && data[index + 3] == 1)
        {
            if (start_index == 0)
            {
                index += 4;
                start_index = index;
            }
            else if (index > start_index)
            {
                ret = rtmp_publish_video_packet(session, data + start_index, index - start_index, is_key, ts);
                index += 4;
                start_index = index;
            }
        }
        else if (data[index] == 0 && data[index + 1] == 0 && data[index + 2] == 1)
        {
            if (start_index == 0)
            {
                index += 3;
                start_index = index;
            }
            else if (index > start_index)
            {
                ret = rtmp_publish_video_packet(session, data + start_index, index - start_index, is_key, ts);
                index += 3;
                start_index = index;
            }
        }
    }

    if (index > start_index)
    {
        ret = rtmp_publish_video_packet(session, data + start_index, index - start_index, is_key, ts);
    }

    return ret;
}

static int rtmp_publish_audio_frame(anj_rtmp_session_t *session, unsigned char *data, uint32_t frame_len, uint32_t ts)
{
    unsigned char *packet = NULL;
    unsigned char *ptr = NULL;

    if (session == NULL || session->client == NULL)
    {
        return -1;
    }

    if (rtmp_grow_packet(&s_packet_a, &s_packet_a_size, frame_len + 2, RTMP_CHUNK_SIZE_A) != 0)
    {
        return -1;
    }

    packet = s_packet_a;
    ptr = packet;

    if (session->audio_codec_id == RTMP_CODEC_AAC)
    {
        *(ptr++) = 0xAF;
        if (frame_len == 2)
        {
            *(ptr++) = 0x00;
        }
        else
        {
            *(ptr++) = 0x01;
            if (frame_len > 7)
            {
                data += 7;
                frame_len -= 7;
            }
        }
    }
    else if (session->audio_codec_id == RTMP_CODEC_G711U)
    {
        *(ptr++) = 0x86;
    }
    else if (session->audio_codec_id == RTMP_CODEC_G711A)
    {
        *(ptr++) = 0x76;
    }
    else
    {
        return 0;
    }

    memcpy(ptr, data, frame_len);
    return rtmp_client_push_audio(session->client, packet, (size_t)(ptr - packet + frame_len), ts);
}

int anj_rtmp_mux_publish_script(anj_rtmp_session_t *session, int width, int height, int framerate,
                                int audiodatarate, int audiosamplerate)
{
    uint8_t script_data[512] = {0};
    uint8_t *ptr = script_data;
    const uint8_t *end = script_data + sizeof(script_data);
    uint32_t count;

    if (session == NULL || session->client == NULL)
    {
        return -1;
    }

    ptr = AMFWriteString(ptr, end, "onMetaData", strlen("onMetaData"));
    count = (session->audio_enable != 0) ? 10 : 5;
    ptr[0] = AMF_ECMA_ARRAY;
    ptr[1] = (uint8_t)((count >> 24) & 0xFF);
    ptr[2] = (uint8_t)((count >> 16) & 0xFF);
    ptr[3] = (uint8_t)((count >> 8) & 0xFF);
    ptr[4] = (uint8_t)(count & 0xFF);
    ptr += 5;

    ptr = AMFWriteNamedString(ptr, end, "title", strlen("title"), "ipc", strlen("ipc"));
    ptr = AMFWriteNamedDouble(ptr, end, "width", strlen("width"), width);
    ptr = AMFWriteNamedDouble(ptr, end, "height", strlen("height"), height);
    ptr = AMFWriteNamedDouble(ptr, end, "framerate", strlen("framerate"), framerate);
    ptr = AMFWriteNamedDouble(ptr, end, "videocodecid", strlen("videocodecid"), session->video_codec_id);

    if (session->audio_enable != 0)
    {
        ptr = AMFWriteNamedDouble(ptr, end, "audiocodecid", strlen("audiocodecid"), session->audio_codec_id);
        ptr = AMFWriteNamedDouble(ptr, end, "audiodatarate", strlen("audiodatarate"),
                                  (audiodatarate > 1000) ? (audiodatarate / 1000) : audiodatarate);
        ptr = AMFWriteNamedDouble(ptr, end, "audiosamplerate", strlen("audiosamplerate"), audiosamplerate);
        ptr = AMFWriteNamedDouble(ptr, end, "audiosamplesize", strlen("audiosamplesize"), 16);
        ptr = AMFWriteNamedBoolean(ptr, end, "stereo", strlen("stereo"), 1);
    }

    ptr = AMFWriteObjectEnd(ptr, end);
    return rtmp_client_push_script(session->client, script_data, (size_t)(ptr - script_data), 0);
}

int anj_rtmp_mux_publish_aac_header(anj_rtmp_session_t *session)
{
    uint8_t aac_data[2] = {0x14, 0x10};

    if (session == NULL || session->client == NULL || session->audio_codec_id != RTMP_CODEC_AAC)
    {
        return 0;
    }

    return rtmp_publish_audio_frame(session, aac_data, 2, 0);
}

int anj_rtmp_mux_on_video(anj_rtmp_session_t *session, unsigned char *data, unsigned int len, int is_key, uint32_t ts)
{
    int ret;

    if (session == NULL || !session->started || session->client == NULL)
    {
        return 0;
    }

    pthread_mutex_lock(&session->push_mutex);

    if (session->send_sps_pps == 0)
    {
        if (!is_key)
        {
            pthread_mutex_unlock(&session->push_mutex);
            return 0;
        }

        ret = rtmp_publish_video_sps_pps(session, data, len, 0);
        if (ret == 0)
        {
            session->send_sps_pps = 1;
            session->start_ts = ts;
        }
    }
    else
    {
        ret = rtmp_publish_video_frame(session, data, len, is_key, ts - session->start_ts);
    }

    pthread_mutex_unlock(&session->push_mutex);
    return ret;
}

int anj_rtmp_mux_on_audio(anj_rtmp_session_t *session, unsigned char *data, unsigned int len, uint32_t ts)
{
    int ret;

    if (session == NULL || !session->started || session->client == NULL || !session->audio_enable)
    {
        return 0;
    }

    if (session->send_sps_pps == 0)
    {
        return 0;
    }

    pthread_mutex_lock(&session->push_mutex);
    ret = rtmp_publish_audio_frame(session, data, len, ts - session->start_ts);
    pthread_mutex_unlock(&session->push_mutex);
    return ret;
}

void anj_rtmp_mux_reset_state(anj_rtmp_session_t *session)
{
    if (session != NULL)
    {
        session->send_sps_pps = 0;
        session->start_ts = 0;
    }
}

void anj_rtmp_mux_uninit(void)
{
    free(s_packet_v);
    s_packet_v = NULL;
    s_packet_v_size = 0;
    free(s_packet_a);
    s_packet_a = NULL;
    s_packet_a_size = 0;
}

int anj_rtmp_mux_fill_codec_info(anj_rtmp_session_t *session)
{
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();

    if (session == NULL || media_cfg == NULL)
    {
        return -1;
    }

    if (!strcasecmp(media_cfg->videoConfig[0].videoEncode.encodeCfg[session->stream_no].encodeFormat.name, "H265"))
    {
        session->video_codec_id = RTMP_CODEC_H265;
    }
    else
    {
        session->video_codec_id = RTMP_CODEC_H264;
    }

    session->audio_enable = media_cfg->audioConfig.audioEncode.enable > 0 ? 1 : 0;
    if (!strcasecmp(media_cfg->audioConfig.audioEncode.audioEncodeType.typeName, "AAC"))
    {
        session->audio_codec_id = RTMP_CODEC_AAC;
    }
    else if (!strcasecmp(media_cfg->audioConfig.audioEncode.audioEncodeType.typeName, "G.711A"))
    {
        session->audio_codec_id = RTMP_CODEC_G711A;
    }
    else
    {
        session->audio_codec_id = RTMP_CODEC_G711U;
    }

    return 0;
}

int anj_rtmp_mux_get_av_param(anj_rtmp_session_t *session, int *width, int *height, int *framerate,
                              int *audiodatarate, int *audiosamplerate)
{
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    VideoConfig *video_cfg;
    VideoEncodeCfg *encode_cfg;
    ANJ_SIZE_S pic_size;

    if (session == NULL || media_cfg == NULL)
    {
        return -1;
    }

    video_cfg = &media_cfg->videoConfig[0];
    encode_cfg = &video_cfg->videoEncode.encodeCfg[session->stream_no];
    pic_size = getPicSize(encode_cfg->resolution.name, video_cfg->videoCapture.tvsystem,
                          video_cfg->videoCapture.rotate, 0);

    if (width)
    {
        *width = (int)pic_size.u32Width;
    }
    if (height)
    {
        *height = (int)pic_size.u32Height;
    }
    if (framerate)
    {
        *framerate = encode_cfg->frameRate;
    }
    if (audiodatarate)
    {
        *audiodatarate = media_cfg->audioConfig.audioEncode.bitRate;
    }
    if (audiosamplerate)
    {
        *audiosamplerate = media_cfg->audioConfig.audioEncode.sampleRate;
    }

    return 0;
}
