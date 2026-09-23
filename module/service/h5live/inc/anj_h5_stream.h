#ifndef __ANJ_H5_STREAM_H__
#define __ANJ_H5_STREAM_H__

#include "anj_h5_ws_server.h"

#ifdef __cplusplus
extern "C" {
#endif

void anj_h5_stream_buffers_init(void);
void anj_h5_stream_buffers_free(void);
void anj_h5_stream_buffer_clear(int stream_id);
StreamBuffer *anj_h5_stream_buffer_get(int stream_id);
int anj_h5_stream_buffer_write(StreamBuffer *sb, int stream_type, uint32_t session_id,
                               unsigned char *frame_data, unsigned int frame_len);

int writeStreamBuffer(StreamBuffer *sb, int stream_type, unsigned int session_id,
                      unsigned char *frame_data, unsigned int frame_len);

#ifdef __cplusplus
}
#endif

#endif
