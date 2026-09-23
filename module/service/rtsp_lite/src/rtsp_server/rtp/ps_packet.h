#ifndef __PS_PACKET_H__
#define __PS_PACKET_H__

#define PS_HDR_LEN			(14)
#define SYS_HDR_LEN			(18)
#define PSM_HDR_LEN			(24)
#define PES_HDR_LEN			(19)
#define PS_SYS_MAP_SIZE		(24)
#define PS_PACKET_MAX_SIZE	(65522)

typedef struct mpeg_video_packet
{
	unsigned long long pts; 
	unsigned long long dts;
	unsigned char *data;
	int size;
	unsigned char *audio_data;
	int audio_size;
	int stream_type;
} mpeg_video_packet;

int ps_packet_build(mpeg_video_packet *packet, char *ps_buf, int ps_buf_len, 
	unsigned int *actually_packet_length, rtp_send_config_s *rtp_send_config);

#endif 
