#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_mbuf.h"
#include "anj_config_media.h"
#include "media_util.h"
#include "anj_rtmp_internal.h"

typedef struct
{
    int mbuf_id;
    int is_audio;
    int running;
    anj_thread_s thread;
    ANJ_MBUF_HANDLE *reader;
    anj_rtmp_session_t *session;
} anj_rtmp_reader_t;

static anj_rtmp_reader_t s_video_reader;
static anj_rtmp_reader_t s_audio_reader;
static anj_rtmp_session_t *s_active_session = NULL;
static int s_media_running = 0;

static uint32_t rtmp_frame_timestamp(media_frame_info_t *frame, int use_system_ts)
{
    if (use_system_ts)
    {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
    }

    return (uint32_t)frame->frameParam.framePts;
}

static int rtmp_strip_private_header(unsigned char *buf, unsigned int *len)
{
    VIDEO_FRAME_HEADER *header;

    if (buf == NULL || len == NULL || *len <= sizeof(VIDEO_FRAME_HEADER))
    {
        return 0;
    }

    header = (VIDEO_FRAME_HEADER *)(buf + *len - sizeof(VIDEO_FRAME_HEADER));
    if (header->flag == VIDEO_PRIVATE_HEADER_MAGIC)
    {
        *len -= (unsigned int)sizeof(VIDEO_FRAME_HEADER);
    }

    return 0;
}

static int anj_rtmp_reader_thread(void *ctx, int *bStart)
{
    anj_rtmp_reader_t *reader = (anj_rtmp_reader_t *)ctx;
    media_frame_info_t frame_info;
    int b_first_frame = 1;
    int ret = 0;

    reader->reader = anj_mbuf_create_reader(reader->mbuf_id, 1);
    if (reader->reader == NULL)
    {
        __ERR("rtmp mbuf reader create failed, mbuf=%d\n", reader->mbuf_id);
        return -1;
    }

    __INFO("rtmp reader start, mbuf=%d audio=%d\n", reader->mbuf_id, reader->is_audio);

    while (bStart && *bStart && reader->running && s_active_session != NULL && s_active_session->started)
    {
        int timeout_ms = b_first_frame ? 2000 : 200;

        if (anj_mbuf_read_frame(reader->reader, b_first_frame, &frame_info, timeout_ms) <= 0)
        {
            continue;
        }

        if (reader->is_audio)
        {
            if (frame_info.frameParam.frameType == MEDIA_AFRAME_A)
            {
                uint32_t ts = rtmp_frame_timestamp(&frame_info, reader->session->use_system_ts);
                ret = anj_rtmp_mux_on_audio(reader->session, frame_info.frameBuf,
                                            frame_info.frameParam.frameLen, ts);
            }
        }
        else
        {
            if (frame_info.frameParam.frameType == MEDIA_VFRAME_I ||
                frame_info.frameParam.frameType == MEDIA_VFRAME_P)
            {
                unsigned int frame_len = frame_info.frameParam.frameLen;
                int is_key = (frame_info.frameParam.frameType == MEDIA_VFRAME_I) ? 1 : 0;
                uint32_t ts = rtmp_frame_timestamp(&frame_info, reader->session->use_system_ts);

                rtmp_strip_private_header(frame_info.frameBuf, &frame_len);
                ret = anj_rtmp_mux_on_video(reader->session, frame_info.frameBuf, frame_len, is_key, ts);
            }
        }

        anj_mbuf_read_release(reader->reader, &frame_info);
        b_first_frame = 0;

        if (ret < 0)
        {
            __ERR("rtmp publish failed, request restart\n");
            anj_rtmp_ctrl_restart();
            break;
        }
    }

    if (reader->reader != NULL)
    {
        anj_mbuf_destory_reader(reader->reader);
        reader->reader = NULL;
    }

    __INFO("rtmp reader stop, mbuf=%d audio=%d\n", reader->mbuf_id, reader->is_audio);
    return 0;
}

static int anj_rtmp_reader_start(anj_rtmp_reader_t *reader, int mbuf_id, int is_audio, anj_rtmp_session_t *session)
{
    if (reader->running)
    {
        return 0;
    }

    memset(reader, 0, sizeof(*reader));
    reader->mbuf_id = mbuf_id;
    reader->is_audio = is_audio;
    reader->session = session;
    reader->running = 1;
    reader->thread.bAutoDestroy = 1;
    snprintf(reader->thread.iThreadName, sizeof(reader->thread.iThreadName),
             is_audio ? "rtmp_audio" : "rtmp_video");
    reader->thread.iThreadjob.ctx = reader;
    reader->thread.iThreadjob.func = anj_rtmp_reader_thread;
    return anj_thread_task_create(&reader->thread);
}

static void anj_rtmp_reader_stop(anj_rtmp_reader_t *reader)
{
    if (!reader->running)
    {
        return;
    }

    reader->running = 0;
    anj_thread_task_destroy(&reader->thread, 0);
    memset(reader, 0, sizeof(*reader));
}

int anj_rtmp_media_start(anj_rtmp_session_t *session)
{
    if (session == NULL || s_media_running)
    {
        return 0;
    }

    s_active_session = session;
    if (anj_rtmp_reader_start(&s_video_reader, session->stream_no, 0, session) != 0)
    {
        s_active_session = NULL;
        return -1;
    }

    if (session->audio_enable)
    {
        if (anj_rtmp_reader_start(&s_audio_reader, session->stream_no, 1, session) != 0)
        {
            anj_rtmp_reader_stop(&s_video_reader);
            s_active_session = NULL;
            return -1;
        }
    }

    s_media_running = 1;
    return 0;
}

void anj_rtmp_media_stop(void)
{
    if (!s_media_running)
    {
        return;
    }

    anj_rtmp_reader_stop(&s_video_reader);
    anj_rtmp_reader_stop(&s_audio_reader);
    s_active_session = NULL;
    s_media_running = 0;
}
