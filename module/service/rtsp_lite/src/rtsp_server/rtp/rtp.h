#ifndef __RTP_H__
#define __RTP_H__

#include "anj_mw_thread.h"

#define MJPEG_QTABLE_DATA_MAX           (256)

#define RTP_PAYLOAD_TYPE_PS   			(96)
#define RTP_PAYLOAD_TYPE_H264   		(98)
#define RTP_PAYLOAD_TYPE_H265   		(98)
#define RTP_PAYLOAD_TYPE_MJPG   		(26)
#define RTP_PAYLOAD_TYPE_AAC			(97)
#define RTP_PAYLOAD_TYPE_G711A			(8)
#define RTP_PAYLOAD_TYPE_G711U			(0)

#ifndef MAIN_STREAM
#define MAIN_STREAM 0
#endif

#ifndef SUB_STREAM
#define SUB_STREAM 1
#endif

typedef enum
{
    e_stream_type_none = 0,
    e_stream_type_H264,
    e_stream_type_H265,
    e_stream_type_MJPEG,
    e_stream_type_AAC = 10,
    e_stream_type_G711A,
    e_stream_type_G711U,
} stream_type_e;

typedef struct av_param_s
{
    int video_enable;
    int audio_enable;
    int video_stream_type;
    int audio_stream_type;
    int resolution_width;
    int resolution_height;
    int frame_rate;
    int audio_sample_rate;
} av_param_s;

typedef enum rtp_send_type
{
    RTP_OVER_TCP,
	RTP_OVER_UDP,
	RTP_OVER_UNKNOWN,
}rtp_send_type_e;

typedef enum rtp_payload_type
{
    MULTIPLE_MEDIA_STREAMS,
	SINGLE_COMPOSITE_PS_STREAM,
	PAYLOAD_TYPE_UNKNOWN,
}rtp_payload_type_e;

typedef struct rtp_send_config
{
	av_param_s av_param;

	/* udp */
	char 					rtp_over_udp_ip[32];
	int 					rtp_client_video_over_udp_port;
	int 					rtcp_client_video_over_udp_port;
	int 					rtp_client_audio_over_udp_port;
	int 					rtcp_client_audio_over_udp_port;

	/* tcp */
	unsigned int 			rtp_video_channel;
	unsigned int 			rtcp_video_channel;
	unsigned int 			rtp_audio_channel;
	unsigned int 			rtcp_audio_channel;
	unsigned int 			rtp_backchannel_channel;
	unsigned int 			rtcp_backchannel_channel;

	/* backchannel (ONVIF/Hik talkback) */
	int 					backchannel_requested;
	int 					backchannel_enable;
	int 					backchannel_talk_started;
	/* Live/Hik URL (stream0/1, ch1/main/av_stream, Streaming/Channels/...)  trackID + Media_header SDP */
	int 					hik_private_url;
	int 					rtp_client_backchannel_over_udp_port;
	int 					rtcp_client_backchannel_over_udp_port;
	
	/* common */
	int 					rtp_fd;
	int 					rtcp_fd;
	unsigned int			ssrc;

	rtp_payload_type_e 		rtp_payload_type;

	/* video */
	unsigned int    		stream_index;
	char 					sprop_vps_[64];
	char 					sprop_sps_[64];
	char 					sprop_pps_[64];
	char 					profileid_[10];
	
	rtp_send_type_e 		rtp_send_type;
	int 					is_multicast;

	int                     is_replay;
	unsigned int            replay_channel;
	unsigned int            replay_start_time;
	void                    *replay_handle;
	int                     replay_slot;
	int                     replay_finished;
	void                    *replay_video_packet;
	void                    *replay_audio_packet;
	void                    *replay_composite_packet;
	unsigned long long      replay_last_video_timestamp;
	unsigned long long      replay_last_audio_timestamp;
	
	anj_thread_s            rtp_send_thread;
	unsigned short			rtp_seq;
	unsigned int			rtp_timestamp;
}rtp_send_config_s;

int rtp_send_start(rtp_send_config_s *rtp_send_config);
int rtp_send_stop(rtp_send_config_s *rtp_send_config);
int rtp_rtcp_udp_fd_init(int *rtp_rtcp_udp_fd, int port, int stream_index);
void rtp_udp_mcast_sockopt(int fd, const char *local_ip);
int rtp_rtcp_tcp_fd_init(int *rtp_rtcp_tcp_fd, int port, char* server_ip, int server_port, int stream_index);

#endif
