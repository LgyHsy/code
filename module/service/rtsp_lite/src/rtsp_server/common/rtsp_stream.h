#ifndef __RTSP_STREAM_H__
#define __RTSP_STREAM_H__

#include "rtp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*rtsp_stream_nalu_callback)(unsigned char *nalu, unsigned int size, void *args);

/**
 * 在 buf[0..size) 中查找下一 Annex-B 起始码（优先 00 00 00 01，否则 00 00 01）。
 * 返回起始码之后的首字节（NAL 头）；未找到返回 NULL。
 * sc_len_out 非空时，在返回非 NULL 时写入本次匹配的起始码长度（3 或 4）；返回 NULL 时不写。
 */
unsigned char *rtsp_stream_find_next_nalu(const unsigned char *buf, unsigned int size, unsigned int *sc_len_out);
int rtsp_stream_loop_every_nalu(const unsigned char *buf, unsigned int size,
	                            rtsp_stream_nalu_callback callback, void *cb_arg);
int rtsp_stream_key_frame_parse(rtp_send_config_s *rtp_send_config);
int rtsp_stream_init_av_param_from_cfg(av_param_s *av_param, int stream_index);

#ifdef __cplusplus
}
#endif

#endif /* __RTSP_STREAM_H__ */
