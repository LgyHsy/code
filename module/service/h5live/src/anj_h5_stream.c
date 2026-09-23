#include <string.h>
#include <stdlib.h>

#include "anj_mw_log.h"
#include "anj_h5_stream.h"

StreamBuffer g_stream_buffers[STREAM_BUF_MAX];

StreamBuffer *getStreamBuffer(int stream_id)
{
    return anj_h5_stream_buffer_get(stream_id);
}

void initStreamBuffers(void)
{
    anj_h5_stream_buffers_init();
}

void freeStreamBuffers(void)
{
    anj_h5_stream_buffers_free();
}

void clearStreamBuffer(int stream_id)
{
    anj_h5_stream_buffer_clear(stream_id);
}

int writeStreamBuffer(StreamBuffer *sb, int stream_type, unsigned int session_id,
                      unsigned char *frame_data, unsigned int frame_len)
{
    return anj_h5_stream_buffer_write(sb, stream_type, session_id, frame_data, frame_len);
}

void anj_h5_stream_buffers_init(void)
{
    memset(g_stream_buffers, 0, sizeof(g_stream_buffers));
    g_stream_buffers[STREAM_ID_AUDIO_LIVE].is_audio = 1;
    g_stream_buffers[STREAM_ID_AUDIO_PLAYBACK].is_audio = 1;

    for (int sid = 0; sid < STREAM_BUF_MAX; sid++)
    {
        pthread_rwlock_init(&g_stream_buffers[sid].rwlock, NULL);
        if (g_stream_buffers[sid].is_audio > 0)
        {
            g_stream_buffers[sid].align = 320;
        }
        else
        {
            g_stream_buffers[sid].align = 10240;
        }
    }
}

void anj_h5_stream_buffers_free(void)
{
    for (int sid = 0; sid < STREAM_BUF_MAX; sid++)
    {
        StreamBuffer *sb = &g_stream_buffers[sid];
        if (sb->align == 0)
        {
            continue;
        }
        for (int i = 0; i < sBUFSIZE; i++)
        {
            if (sb->buffer[i].frame_data)
            {
                free(sb->buffer[i].frame_data);
                sb->buffer[i].frame_data = NULL;
            }
        }
        pthread_rwlock_destroy(&sb->rwlock);
    }
}

void anj_h5_stream_buffer_clear(int stream_id)
{
    StreamBuffer *sb = anj_h5_stream_buffer_get(stream_id);
    if (!sb)
    {
        return;
    }

    pthread_rwlock_wrlock(&sb->rwlock);
    sb->buf_pos = 0;
    sb->seq_counter = 0;
    sb->flag_send = 0;
    for (int i = 0; i < sBUFSIZE; i++)
    {
        if (sb->buffer[i].frame_data)
        {
            free(sb->buffer[i].frame_data);
            sb->buffer[i].frame_data = NULL;
        }
        sb->buffer[i].frame_len = 0;
        sb->buffer[i].nBufferSize = 0;
        sb->buffer[i].stream_type = 0;
        sb->buffer[i].sequence_id = 0;
        sb->buffer[i].session_id = 0;
    }
    pthread_rwlock_unlock(&sb->rwlock);
}

StreamBuffer *anj_h5_stream_buffer_get(int stream_id)
{
    if (stream_id <= 0 || stream_id >= STREAM_BUF_MAX)
    {
        return NULL;
    }
    if (g_stream_buffers[stream_id].align == 0)
    {
        return NULL;
    }
    return &g_stream_buffers[stream_id];
}

int anj_h5_stream_buffer_write(StreamBuffer *sb, int stream_type, uint32_t session_id,
                               unsigned char *frame_data, unsigned int frame_len)
{
    if (sb == NULL || frame_data == NULL || frame_len == 0)
    {
        return -1;
    }

    unsigned int pos = sb->buf_pos;
    frameDataBuffer *buf = sb->buffer;
    int align = sb->align;

    pthread_rwlock_wrlock(&sb->rwlock);

    unsigned int needSize = (frame_len / (unsigned int)align + 1) * (unsigned int)align;
    if (buf[pos].nBufferSize < frame_len || (buf[pos].nBufferSize > needSize + 10 * 1024))
    {
        if (buf[pos].frame_data != NULL)
        {
            free(buf[pos].frame_data);
        }
        buf[pos].frame_data = NULL;
        buf[pos].nBufferSize = needSize;
        buf[pos].frame_data = (unsigned char *)malloc(buf[pos].nBufferSize);
        if (buf[pos].frame_data == NULL)
        {
            pthread_rwlock_unlock(&sb->rwlock);
            return -1;
        }
    }

    memcpy(buf[pos].frame_data, frame_data, frame_len);
    buf[pos].frame_len = (int)frame_len;
    buf[pos].sequence_id = ++sb->seq_counter;
    buf[pos].stream_type = stream_type;
    buf[pos].session_id = session_id;
    sb->buf_pos = pos + 1;
    if (sb->buf_pos >= sBUFSIZE)
    {
        sb->buf_pos = 0;
    }

    pthread_rwlock_unlock(&sb->rwlock);
    return 0;
}
