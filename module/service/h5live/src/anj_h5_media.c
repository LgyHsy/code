#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_mbuf.h"
#include "anj_video.h"
#include "anj_config.h"
#include "media_util.h"
#include "anj_h5_ws_server.h"
#include "anj_h5_stream.h"
#include "anj_h5_media.h"
#include "project_option.h"

#ifndef MAIN_STREAM
#define MAIN_STREAM 0
#endif
#ifndef SUB_STREAM
#define SUB_STREAM 1
#endif

typedef struct
{
    int mbuf_id;
    int stream_id;
    int running;
    anj_thread_s thread;
    ANJ_MBUF_HANDLE *reader;
} h5_media_reader_t;

typedef struct
{
    int running;
    anj_thread_s thread;
    ANJ_MBUF_HANDLE *reader;
} h5_live_audio_reader_t;

static h5_media_reader_t s_main_reader;
static h5_media_reader_t s_sub_reader;
static h5_live_audio_reader_t s_live_audio_reader;
static int s_media_running = 0;
static anj_thread_s s_media_monitor_thread;
static pthread_mutex_t s_media_lock = PTHREAD_MUTEX_INITIALIZER;

static int h5_mbuf_is_available(int mbuf_id)
{
    return (mbuf_id >= 0 && mbuf_id < MAX_VENC_CHN);
}

static int h5_live_audio_encode_enabled(void)
{
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();

    if (media_cfg == NULL)
    {
        return 0;
    }
    return media_cfg->audioConfig.audioEncode.enable > 0;
}

void anj_h5_audio_config_sync(void)
{
    MediaConfig *media_cfg = (MediaConfig *)getMediaConfig();
    AudioConfig *audio_cfg;
    media_codec_type_e codec;

    if (media_cfg == NULL)
    {
        return;
    }

    audio_cfg = &media_cfg->audioConfig;
    codec = audio_encode_type_get(audio_cfg->audioEncode.audioEncodeType.typeName);

    if (audio_cfg->audioEncode.enable <= 0)
    {
        strncpy(g_audio_codec_type, "PCMU", sizeof(g_audio_codec_type) - 1);
        g_audio_codec_type[sizeof(g_audio_codec_type) - 1] = '\0';
        g_audio_sample_rate = 8000;
        g_audio_channels = 1;
        return;
    }

    switch (codec)
    {
    case MEDIA_CODEC_AUDIO_G711A:
        strncpy(g_audio_codec_type, "PCMA", sizeof(g_audio_codec_type) - 1);
        break;
    case MEDIA_CODEC_AUDIO_G711U:
        strncpy(g_audio_codec_type, "PCMU", sizeof(g_audio_codec_type) - 1);
        break;
    case MEDIA_CODEC_AUDIO_AAC:
        strncpy(g_audio_codec_type, "AAC", sizeof(g_audio_codec_type) - 1);
        break;
    default:
        strncpy(g_audio_codec_type, "PCMU", sizeof(g_audio_codec_type) - 1);
        break;
    }
    g_audio_codec_type[sizeof(g_audio_codec_type) - 1] = '\0';

    g_audio_sample_rate = audio_cfg->audioEncode.sampleRate > 0 ?
                          audio_cfg->audioEncode.sampleRate : 8000;
    g_audio_channels = audio_cfg->audioCapture.channels > 0 ?
                       audio_cfg->audioCapture.channels : 1;

    __INFO("h5 audio config sync: codec=%s rate=%d ch=%d\n",
           g_audio_codec_type, g_audio_sample_rate, g_audio_channels);
}

void anj_h5_request_idr(int stream_id)
{
    int venc_id = -1;

    if (stream_id == STREAM_ID_MAIN)
    {
        venc_id = MAIN_STREAM;
    }
    else if (stream_id == STREAM_ID_SUB && h5_mbuf_is_available(SUB_STREAM))
    {
        venc_id = SUB_STREAM;
    }

    if (venc_id >= 0)
    {
        anj_video_request_idr(0, venc_id);
    }
}

static void h5_media_push_frame(h5_media_reader_t *reader, media_frame_info_t *pFrame)
{
    StreamBuffer *sb = getStreamBuffer(reader->stream_id);
    int frame_type = PFatme_Type;

    if (sb == NULL)
    {
        return;
    }

    if (pFrame->frameParam.frameType != MEDIA_VFRAME_I &&
        pFrame->frameParam.frameType != MEDIA_VFRAME_P)
    {
        return;
    }

    if (pFrame->frameParam.frameType == MEDIA_VFRAME_I)
    {
        frame_type = IFatme_Type;
    }

    if (frame_type == IFatme_Type)
    {
        writeStreamBuffer(sb, frame_type, 0, pFrame->frameBuf, (unsigned int)pFrame->frameParam.frameLen);
        sb->flag_send = 1;
    }
    else if (sb->flag_send)
    {
        writeStreamBuffer(sb, frame_type, 0, pFrame->frameBuf, (unsigned int)pFrame->frameParam.frameLen);
    }
}

static void h5_live_audio_push_frame(media_frame_info_t *pFrame)
{
    StreamBuffer *sb;

    if (pFrame == NULL || pFrame->frameBuf == NULL || pFrame->frameParam.frameLen <= 0)
    {
        return;
    }

    sb = getStreamBuffer(STREAM_ID_AUDIO_LIVE);
    if (sb == NULL)
    {
        return;
    }

    writeStreamBuffer(sb, AudioData_Type, 0, pFrame->frameBuf,
                      (unsigned int)pFrame->frameParam.frameLen);
}

static int h5_media_reader_thread(void *ctx, int *bStart)
{
    h5_media_reader_t *reader = (h5_media_reader_t *)ctx;
    media_frame_info_t frame_info;
    int bFirstFrame = 1;

    __INFO("h5 media reader start mbuf:%d stream:%d\n", reader->mbuf_id, reader->stream_id);

    reader->reader = anj_mbuf_create_reader(reader->mbuf_id, 1);
    if (reader->reader == NULL)
    {
        __ERR("anj_mbuf_create_reader failed mbuf:%d\n", reader->mbuf_id);
        return -1;
    }

    while (bStart && *bStart && reader->running)
    {
        int timeout_ms = bFirstFrame ? 2000 : 200;
        if (anj_mbuf_read_frame(reader->reader, bFirstFrame, &frame_info, timeout_ms) > 0)
        {
            h5_media_push_frame(reader, &frame_info);
            anj_mbuf_read_release(reader->reader, &frame_info);
            bFirstFrame = 0;
        }
    }

    if (reader->reader)
    {
        anj_mbuf_destory_reader(reader->reader);
        reader->reader = NULL;
    }

    __INFO("h5 media reader stop mbuf:%d stream:%d\n", reader->mbuf_id, reader->stream_id);
    return 0;
}

static int h5_live_audio_reader_thread(void *ctx, int *bStart)
{
    h5_live_audio_reader_t *reader = (h5_live_audio_reader_t *)ctx;
    media_frame_info_t frame_info;

    __INFO("h5 live audio reader start\n");

    reader->reader = anj_mbuf_create_reader(MAIN_STREAM, 1);
    if (reader->reader == NULL)
    {
        __ERR("anj_mbuf_create_reader failed for live audio mbuf:%d\n", MAIN_STREAM);
        return -1;
    }

    while (bStart && *bStart && reader->running)
    {
        if (anj_mbuf_read_frame(reader->reader, 0, &frame_info, 200) > 0)
        {
            if (frame_info.frameParam.frameType == MEDIA_AFRAME_A &&
                h5_live_audio_encode_enabled())
            {
                h5_live_audio_push_frame(&frame_info);
            }
            anj_mbuf_read_release(reader->reader, &frame_info);
        }
    }

    if (reader->reader)
    {
        anj_mbuf_destory_reader(reader->reader);
        reader->reader = NULL;
    }

    __INFO("h5 live audio reader stop\n");
    return 0;
}

static int h5_media_reader_start(h5_media_reader_t *reader, int mbuf_id, int stream_id)
{
    if (reader->running)
    {
        return 0;
    }

    memset(reader, 0, sizeof(*reader));
    reader->mbuf_id = mbuf_id;
    reader->stream_id = stream_id;
    reader->running = 1;
    reader->thread.bAutoDestroy = 0;
    snprintf(reader->thread.iThreadName, sizeof(reader->thread.iThreadName),
             "h5_media_%d", mbuf_id);
    reader->thread.iThreadjob.ctx = reader;
    reader->thread.iThreadjob.func = h5_media_reader_thread;
    return anj_thread_task_create(&reader->thread);
}

static void h5_media_reader_stop(h5_media_reader_t *reader)
{
    int mbuf_id;
    int stream_id;

    if (!reader->running)
    {
        return;
    }
    mbuf_id = reader->mbuf_id;
    stream_id = reader->stream_id;
    reader->running = 0;
    anj_thread_task_destroy(&reader->thread, 0);
    memset(reader, 0, sizeof(*reader));
    __INFO("h5 media reader stopped mbuf:%d stream:%d\n", mbuf_id, stream_id);
}

static int h5_live_audio_reader_start(void)
{
    if (s_live_audio_reader.running)
    {
        return 0;
    }

    if (!h5_live_audio_encode_enabled())
    {
        __INFO("skip live h5 audio reader: audio encode disabled\n");
        return 0;
    }

    if (!h5_mbuf_is_available(MAIN_STREAM))
    {
        __WARN("live h5 audio reader skipped: main mbuf unavailable\n");
        return -1;
    }

    memset(&s_live_audio_reader, 0, sizeof(s_live_audio_reader));
    s_live_audio_reader.running = 1;
    s_live_audio_reader.thread.bAutoDestroy = 0;
    snprintf(s_live_audio_reader.thread.iThreadName,
             sizeof(s_live_audio_reader.thread.iThreadName), "h5_audio_live");
    s_live_audio_reader.thread.iThreadjob.ctx = &s_live_audio_reader;
    s_live_audio_reader.thread.iThreadjob.func = h5_live_audio_reader_thread;
    return anj_thread_task_create(&s_live_audio_reader.thread);
}

static void h5_live_audio_reader_stop(void)
{
    if (!s_live_audio_reader.running)
    {
        return;
    }

    s_live_audio_reader.running = 0;
    anj_thread_task_destroy(&s_live_audio_reader.thread, 0);
    memset(&s_live_audio_reader, 0, sizeof(s_live_audio_reader));
    clearStreamBuffer(STREAM_ID_AUDIO_LIVE);
    __INFO("h5 live audio reader stopped\n");
}

void anj_h5_live_audio_reader_restart(void)
{
    pthread_mutex_lock(&s_media_lock);
    if (!s_media_running)
    {
        pthread_mutex_unlock(&s_media_lock);
        return;
    }

    anj_h5_audio_config_sync();
    h5_live_audio_reader_stop();
    if (h5_live_audio_reader_start() != 0)
    {
        __WARN("restart live h5 audio reader failed\n");
    }
    pthread_mutex_unlock(&s_media_lock);
}

int anj_h5_media_streams_start(void)
{
    pthread_mutex_lock(&s_media_lock);
    if (s_media_running)
    {
        pthread_mutex_unlock(&s_media_lock);
        return 0;
    }

    anj_h5_audio_config_sync();

    if (!h5_mbuf_is_available(MAIN_STREAM))
    {
        __ERR("main h5 media mbuf unavailable\n");
        pthread_mutex_unlock(&s_media_lock);
        return -1;
    }

    if (h5_media_reader_start(&s_main_reader, MAIN_STREAM, STREAM_ID_MAIN) != 0)
    {
        __ERR("start main h5 media reader failed\n");
        pthread_mutex_unlock(&s_media_lock);
        return -1;
    }

    if (h5_mbuf_is_available(SUB_STREAM))
    {
        if (h5_media_reader_start(&s_sub_reader, SUB_STREAM, STREAM_ID_SUB) != 0)
        {
            h5_media_reader_stop(&s_main_reader);
            __ERR("start sub h5 media reader failed\n");
            pthread_mutex_unlock(&s_media_lock);
            return -1;
        }
    }
    else
    {
        __INFO("skip sub h5 media reader: MAX_VENC_CHN=%d\n", MAX_VENC_CHN);
    }

    s_media_running = 1;

    if (h5_live_audio_reader_start() != 0)
    {
        __WARN("start live h5 audio reader failed, video streaming continues\n");
    }

    __INFO("h5 media streams started\n");
    pthread_mutex_unlock(&s_media_lock);
    return 0;
}

int anj_h5_media_streams_stop(void)
{
    pthread_mutex_lock(&s_media_lock);
    if (!s_media_running)
    {
        pthread_mutex_unlock(&s_media_lock);
        return 0;
    }

    h5_live_audio_reader_stop();
    h5_media_reader_stop(&s_main_reader);
    h5_media_reader_stop(&s_sub_reader);

    clearStreamBuffer(STREAM_ID_MAIN);
    clearStreamBuffer(STREAM_ID_SUB);

    s_media_running = 0;
    __INFO("h5 media streams stopped\n");
    pthread_mutex_unlock(&s_media_lock);
    return 0;
}

int anj_h5_media_monitor_thread(void *ctx, int *bStart)
{
    int idle_ticks = 0;

    (void)ctx;

    while (bStart && *bStart)
    {
        if (g_h5_client_count > 0)
        {
            idle_ticks = 0;
            if (!s_media_running)
            {
                anj_h5_media_streams_start();
            }
        }
        else if (s_media_running)
        {
            idle_ticks++;
            if (idle_ticks >= 20)
            {
                anj_h5_media_streams_stop();
                idle_ticks = 0;
            }
        }
        else
        {
            idle_ticks = 0;
        }
        usleep(100 * 1000);
    }

    anj_h5_media_streams_stop();
    return 0;
}

void anj_h5_media_monitor_start(void)
{
    if (s_media_monitor_thread.start != 0 && s_media_monitor_thread.end == 0)
    {
        return;
    }
    memset(&s_media_monitor_thread, 0, sizeof(s_media_monitor_thread));
    s_media_monitor_thread.bAutoDestroy = 0;
    snprintf(s_media_monitor_thread.iThreadName, sizeof(s_media_monitor_thread.iThreadName),
             "h5_media_mon");
    s_media_monitor_thread.iThreadjob.ctx = NULL;
    s_media_monitor_thread.iThreadjob.func = anj_h5_media_monitor_thread;
    anj_thread_task_create(&s_media_monitor_thread);
}

void anj_h5_media_monitor_stop(void)
{
    if (s_media_monitor_thread.start == 0)
    {
        return;
    }
    anj_thread_task_destroy(&s_media_monitor_thread, 0);
    memset(&s_media_monitor_thread, 0, sizeof(s_media_monitor_thread));
}
