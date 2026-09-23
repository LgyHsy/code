#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "rtp.h"
#include "rtsp_stream.h"
#include "ps_packet.h"
#include "anj_mbuf.h"
#include "media_util.h"
#include "anj_record.h"
#include "anj_mw_comm.h"

#define RTP_MAX_PACKET_SIZE 			(1400)
#define RTP_HEADER_SIZE         		(12)
#define RTP_OVER_TCP_HEAD_SIZE			(4)
#define RTP_FU_A_HEADER_SIZE    		(2)
#define RTP_H265_INDICATOR_HEADER_SIZE  (3)

#define H265_NALU_HEADER_BYTE_SIZE 		(2)
#define H264_NALU_HEADER_BYTE_SIZE		(1)

#define RTP_AAC_HEADER_SIZE    			(7)
#define RTP_AAC_PAYLOAD_HEADER_SIZE    	(4)
#define RTP_JPEG_MAIN_HEADER_SIZE       (8)
#define MJPEG_QT_HDR_SIZE               (4)
#define RTP_JPEG_MAX_EXT_SIZE           (MJPEG_QT_HDR_SIZE + MJPEG_QTABLE_DATA_MAX)
#define MJPEG_QFACTOR_INLINE            (128)
#define MJPEG_RFC2435_MAX_DIM           (2040)
#define RTP_HDR_EXT_HDR_SIZE            (4)
#define RTP_JPEG_ONVIF_HDR_MAX          (2048)

#define RTP_VESION              		(2)

#define MAIN_STREAM_SOCKET_SENDBUFF_SIZE	(100*1024)
#define SUB_STREAM_SOCKET_SENDBUFF_SIZE		(50*1024)

/**
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P|X|  CC   |M|     PT      |       sequence number         |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                           timestamp                           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |           synchronization source (SSRC) identifier            |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |            contributing source (CSRC) identifiers             |
  |                             ....                              |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
typedef struct rtp_header
{
	/* byte 0 */
	unsigned char csrcLen:4;
	unsigned char extension:1;
	unsigned char padding:1;
	unsigned char version:2;

	/* byte 1 */
	unsigned char payloadType:7;
	unsigned char marker:1;

	/* bytes 2,3 */
	unsigned short seq;

	/* bytes 4-7 */
	unsigned int timestamp;

	/* bytes 8-11 */
	unsigned int ssrc;
} rtp_header_s;

typedef struct fu_header
{
	/* byte 0 */
	unsigned char type:5;
	unsigned char r:1;
	unsigned char e:1;
	unsigned char s:1;
} fu_header_s;

typedef struct fu_indicator
{
	unsigned char type:5;
	unsigned char nri:2;
	unsigned char f:1;
} fu_indicator_s;

typedef struct h265_fu_indicator
{
	unsigned char null	:1;
    unsigned char type	:6;
    unsigned char f		:1;
	unsigned char tid	:3;
	unsigned char lid	:5;
} h265_fu_indicator_s;

typedef struct rtp_packet
{
    rtp_header_s rtp_header;
    unsigned char payload[0];
} rtp_packet_s;

typedef struct rtp_over_tcp_packet
{
	/*
		byte 0 		: "$" 标识符
		byte 1 && 2	: channel
		byte 3		: RTP包的size
	*/
	char header[4];
    rtp_packet_s rtp_packet;
} rtp_over_tcp_packet_s;

static inline int sk_is_valid(int sk) { return sk >= 0; }

static pthread_mutex_t s_replay_slot_mutex = PTHREAD_MUTEX_INITIALIZER;
static rtp_send_config_s *s_replay_slots[REC_MAX_PB_NUM] = {0};

static inline int create_client_socket(int type, int snd_buf, int rcv_buf, int reuse, int port, int nb, int bc, const char *ifn, int ttl, void *addr)
{
	int sock = socket(AF_INET, type, 0);
	if (sock < 0) return -1;
	if (reuse) { int opt = 1; setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)); }
	return sock;
}

static int _replay_slot_alloc(rtp_send_config_s *rtp_send_config)
{
    int i = -1;

    pthread_mutex_lock(&s_replay_slot_mutex);
    for (int idx = 0; idx < REC_MAX_PB_NUM; idx++)
    {
        if (s_replay_slots[idx] == NULL)
        {
            s_replay_slots[idx] = rtp_send_config;
            i = idx;
            break;
        }
    }
    pthread_mutex_unlock(&s_replay_slot_mutex);

    return i;
}

static void _replay_slot_free(int slot)
{
    if (slot < 0 || slot >= REC_MAX_PB_NUM)
    {
        return;
    }

    pthread_mutex_lock(&s_replay_slot_mutex);
    s_replay_slots[slot] = NULL;
    pthread_mutex_unlock(&s_replay_slot_mutex);
}

static rtp_send_config_s *_replay_slot_get(int slot)
{
    rtp_send_config_s *rtp_send_config = NULL;

    if (slot < 0 || slot >= REC_MAX_PB_NUM)
    {
        return NULL;
    }

    pthread_mutex_lock(&s_replay_slot_mutex);
    rtp_send_config = s_replay_slots[slot];
    pthread_mutex_unlock(&s_replay_slot_mutex);

    return rtp_send_config;
}

static int _rtp_send_packet(int rtp_fd, int udp_port, char *udp_ip, rtp_send_type_e rtp_send_type,
		unsigned int rtp_channel, rtp_over_tcp_packet_s *rtp_over_tcp_packet, unsigned int dataSize)
{
    int ret;
	
    struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(udp_port);
    addr.sin_addr.s_addr = inet_addr(udp_ip);

    rtp_over_tcp_packet->rtp_packet.rtp_header.seq = htons(rtp_over_tcp_packet->rtp_packet.rtp_header.seq);
    rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp = htonl(rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp);
    rtp_over_tcp_packet->rtp_packet.rtp_header.ssrc = htonl(rtp_over_tcp_packet->rtp_packet.rtp_header.ssrc);

	if (rtp_send_type == RTP_OVER_TCP)
	{
		int total = (int)(RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + dataSize);
		int sent = 0;
		const char *buf = (const char *)rtp_over_tcp_packet;

		rtp_over_tcp_packet->header[0] = '$';
		rtp_over_tcp_packet->header[1] = rtp_channel;
		rtp_over_tcp_packet->header[2] = ((RTP_HEADER_SIZE + dataSize) & 0xFF00) >> 8;
		rtp_over_tcp_packet->header[3] = (RTP_HEADER_SIZE + dataSize) & 0xFF;

		while (sent < total)
		{
			ret = send(rtp_fd, buf + sent, total - sent, MSG_NOSIGNAL);
			if (ret <= 0)
			{
				__ERR("send failed!(sk=%d, size=%d, sent=%d, ret=%d)(errno=%d, errmsg=%s) \n", rtp_fd,
				       total, sent, ret, errno, strerror(errno));
				if (sk_is_valid(rtp_fd))
				{
					shutdown(rtp_fd, SHUT_RDWR);
				}
				return -1;
			}
			sent += ret;
		}
	}
	else if (rtp_send_type == RTP_OVER_UDP)
	{
		ret = sendto(rtp_fd, (void *)&rtp_over_tcp_packet->rtp_packet, RTP_HEADER_SIZE + dataSize, 0,
			(struct sockaddr *)&addr, sizeof(addr));
		if (ret != (int)(RTP_HEADER_SIZE + dataSize))
		{
			__ERR("sendto failed!(sk=%d, size=%lu, ret=%d)(errno=%d, errmsg=%s)", rtp_fd,
				(unsigned long)(RTP_HEADER_SIZE + dataSize), ret, errno, strerror(errno));
			return -1;
		}
	}
	else
	{
		__ERR("rtp_send_type is invalid!(%lu)", (unsigned long)(rtp_send_type));
		return -1;
	}

    rtp_over_tcp_packet->rtp_packet.rtp_header.seq = ntohs(rtp_over_tcp_packet->rtp_packet.rtp_header.seq);
    rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp = ntohl(rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp);
    rtp_over_tcp_packet->rtp_packet.rtp_header.ssrc = ntohl(rtp_over_tcp_packet->rtp_packet.rtp_header.ssrc);

    return 0;
}

static int _rtp_send_h265_frame(rtp_send_config_s *rtp_send_config, rtp_over_tcp_packet_s* rtp_over_tcp_packet, 
			unsigned char *frame, unsigned int frame_size)
{
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	_NULL_POINTER_CHECK_(rtp_over_tcp_packet, -1);
	_NULL_POINTER_CHECK_(frame, -1);
	if (frame_size == 0)
	{
		__ERR("frame_size is invalid!(%lu)", (unsigned long)frame_size);
		return -1;
	}
	
	unsigned char nalu_type = frame[0];

	if ((frame_size - H265_NALU_HEADER_BYTE_SIZE) <= RTP_MAX_PACKET_SIZE)
	{
		/*
		 *	 0 1 2 3 4 5 6 7 8 9
		 *	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 *	|F|NRI|  Type	| a single NAL unit ... |
		 *	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */
		rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 1;
		memcpy(rtp_over_tcp_packet->rtp_packet.payload, frame, frame_size);
		if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, frame_size))
		{
			__ERR("_rtp_send_packet failed!\n");
			return -1;
		}

		rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
	}
	else
	{
		/*
		 *	0				1               2               
		 *	0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 * |         FU indicator          |   FU header   |   FU payload	...  |
		 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */

		/*
		 *	   FU Indicator
		 *    0               1
		 *	  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
		 *	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 *	 |F|    Type   |    LayId  | Tid |  
		 *	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		 */

		/*
		 *		FU Header
		 *	  0 1 2 3 4 5 6 7
		 *	 +-+-+-+-+-+-+-+-+
		 *	 |S|E|    Type	 |
		 *	 +---------------+
		 */

        int packet_number = (frame_size - H265_NALU_HEADER_BYTE_SIZE) / RTP_MAX_PACKET_SIZE;
        int remain_packet_size = (frame_size - H265_NALU_HEADER_BYTE_SIZE) % RTP_MAX_PACKET_SIZE;
        int i = 0;
		int offset = H265_NALU_HEADER_BYTE_SIZE;

		/* 发送完整的包 */
		for (i = 0; i < packet_number; i++)
		{
			rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 0;
			memset(rtp_over_tcp_packet->rtp_packet.payload, 0, RTP_H265_INDICATOR_HEADER_SIZE);
			/* 28表示FU-A  , nal-type */
			h265_fu_indicator_s *h265_fu_indicator = (h265_fu_indicator_s *)rtp_over_tcp_packet->rtp_packet.payload;
			h265_fu_indicator->type = 49;
			h265_fu_indicator->tid = 1;
			fu_header_s *fu_header = (fu_header_s *)(rtp_over_tcp_packet->rtp_packet.payload + sizeof(h265_fu_indicator_s));
			fu_header->type = (nalu_type >> 1) & 0x3F;
			
			if (i == 0) //第一包数据
			{
				fu_header->s = 1;
			}
			else if (remain_packet_size == 0 && i == packet_number - 1) //最后一包数据
			{
				fu_header->e = 1;
			}

			memcpy(rtp_over_tcp_packet->rtp_packet.payload + RTP_H265_INDICATOR_HEADER_SIZE, frame + offset, RTP_MAX_PACKET_SIZE);
			if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, RTP_H265_INDICATOR_HEADER_SIZE + RTP_MAX_PACKET_SIZE))
			{
				__ERR("_rtp_send_packet failed! \n");
				return -1;
			}

			rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
			offset += RTP_MAX_PACKET_SIZE;
		}

		/* 发送剩余的数据 */
		if (remain_packet_size > 0)
		{			
			rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 1;
			memset(rtp_over_tcp_packet->rtp_packet.payload, 0, RTP_H265_INDICATOR_HEADER_SIZE);
			
			h265_fu_indicator_s *h265_fu_indicator = (h265_fu_indicator_s *)rtp_over_tcp_packet->rtp_packet.payload;
			h265_fu_indicator->type = 49;
			h265_fu_indicator->tid = 1;
			fu_header_s *fu_header = (fu_header_s *)(rtp_over_tcp_packet->rtp_packet.payload + sizeof(h265_fu_indicator_s));
			fu_header->type = (nalu_type >> 1) & 0x3F;
			fu_header->e = 1;

			memcpy(rtp_over_tcp_packet->rtp_packet.payload + RTP_H265_INDICATOR_HEADER_SIZE, frame + offset, remain_packet_size);
			if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, RTP_H265_INDICATOR_HEADER_SIZE + remain_packet_size))
			{
				__ERR("_rtp_send_packet failed! \n");
				return -1;
			}
			
			rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
		}
	}

	return 0;
}

static int _rtp_send_h264_frame(rtp_send_config_s *rtp_send_config, rtp_over_tcp_packet_s* rtp_over_tcp_packet, 
			unsigned char *frame, unsigned int frame_size)
{
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	_NULL_POINTER_CHECK_(rtp_over_tcp_packet, -1);
	_NULL_POINTER_CHECK_(frame, -1);
	if (frame_size == 0)
	{
		__ERR("frame_size is invalid!(%lu) \n", (unsigned long)frame_size);
		return -1;
	}
	
    unsigned char nalu_type = frame[0];

    if ((frame_size - H264_NALU_HEADER_BYTE_SIZE) <= RTP_MAX_PACKET_SIZE)
    {
        /*
         *   0 1 2 3 4 5 6 7 8 9
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |F|NRI|  Type   | a single NAL unit ... |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 1;
        memcpy(rtp_over_tcp_packet->rtp_packet.payload, frame, frame_size);
        if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, frame_size))
		{
			__ERR("_rtp_send_packet failed! \n");
			return -1;
		}

        rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
    }
    else
    {
    	/* 分片发送 */
        /*
         *  0                   1                   2
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * | FU indicator  |   FU header   |   FU payload   ...  |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

        /*
         *     FU Indicator
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |F|NRI|  Type   |
         *   +---------------+
         */

        /*
         *      FU Header
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |S|E|R|  Type   |
         *   +---------------+
         */

        int packet_number = (frame_size - H264_NALU_HEADER_BYTE_SIZE) / RTP_MAX_PACKET_SIZE;
        int remain_packet_size = (frame_size - H264_NALU_HEADER_BYTE_SIZE) % RTP_MAX_PACKET_SIZE;
        int i = 0;
		int offset = H264_NALU_HEADER_BYTE_SIZE;

        /* 发送完整的包 */
        for (i = 0; i < packet_number; i++)
        {
	        /* Last full fragment should carry marker when no remainder exists. */
	        rtp_over_tcp_packet->rtp_packet.rtp_header.marker =
	        	(remain_packet_size == 0 && i == packet_number - 1) ? 1 : 0;
			memset(rtp_over_tcp_packet->rtp_packet.payload, 0, RTP_FU_A_HEADER_SIZE);
        	/* 28表示FU-A  , nal-type */
			fu_indicator_s *fu_indicator = (fu_indicator_s *)rtp_over_tcp_packet->rtp_packet.payload;
			fu_indicator->nri = (nalu_type & 0x60) >> 5;
			fu_indicator->type = 28;
			
			fu_header_s *fu_header = (fu_header_s *)(rtp_over_tcp_packet->rtp_packet.payload + sizeof(fu_indicator_s));
			fu_header->type = nalu_type & 0x1F;

            if (i == 0)
            {
            	fu_header->s = 1;				/* FU Header 中的S标志位 */
			}
            else if (remain_packet_size == 0 && i == packet_number - 1) //最后一包数据
            {
            	fu_header->e = 1;				/* FU Header 中的E标志位 */
			}

            memcpy(rtp_over_tcp_packet->rtp_packet.payload + RTP_FU_A_HEADER_SIZE, frame + offset, RTP_MAX_PACKET_SIZE);
            if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, RTP_FU_A_HEADER_SIZE + RTP_MAX_PACKET_SIZE))
			{
				__ERR("_rtp_send_packet failed!");
				return -1;
			}

            rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
            offset += RTP_MAX_PACKET_SIZE;
        }

        /* 发送剩余的数据 */
        if (remain_packet_size > 0)
        {        	
	        rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 1;
			memset(rtp_over_tcp_packet->rtp_packet.payload, 0, RTP_FU_A_HEADER_SIZE);
			fu_indicator_s *fu_indicator = (fu_indicator_s *)rtp_over_tcp_packet->rtp_packet.payload;
			fu_indicator->nri = (nalu_type & 0x60) >> 5;
			fu_indicator->type = 28;
			fu_header_s *fu_header = (fu_header_s *)(rtp_over_tcp_packet->rtp_packet.payload + sizeof(fu_indicator_s));
			fu_header->type = nalu_type & 0x1F;
        	fu_header->e = 1;				/* FU Header 中的E标志位 */

            memcpy(rtp_over_tcp_packet->rtp_packet.payload + RTP_FU_A_HEADER_SIZE, frame + offset, remain_packet_size);
            if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, RTP_FU_A_HEADER_SIZE + remain_packet_size))
			{
				__ERR("_rtp_send_packet failed!");
				return -1;
			}
			
            rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
        }
    }

    return 0;
}

/* RFC 2435: payload starts after SOS segment (entropy-coded scan data). */
static unsigned int _mjpeg_payload_offset(const unsigned char *data, unsigned int len)
{
	unsigned int i = 0;

	if (data == NULL || len < 4)
	{
		return 0;
	}

	for (i = 0; i + 3 < len; ++i)
	{
		if (data[i] != 0xFF)
		{
			continue;
		}
		if (data[i + 1] == 0xDA)
		{
			unsigned int sos_len = ((unsigned int)data[i + 2] << 8) | data[i + 3];

			if (i + 2 + sos_len < len)
			{
				return i + 2 + sos_len;
			}
			break;
		}
	}

	if (data[0] == 0xFF && data[1] == 0xD8)
	{
		return 2;
	}
	return 0;
}

static int _mjpeg_parse_sof(const unsigned char *data, unsigned int len, int *width, int *height)
{
	unsigned int i = 0;

	if (data == NULL || width == NULL || height == NULL)
	{
		return -1;
	}

	for (i = 0; i + 8 < len; ++i)
	{
		if (data[i] != 0xFF)
		{
			continue;
		}

		if (data[i + 1] == 0xC0 || data[i + 1] == 0xC1 || data[i + 1] == 0xC2)
		{
			*height = ((int)data[i + 5] << 8) | data[i + 6];
			*width = ((int)data[i + 7] << 8) | data[i + 8];
			return 0;
		}
	}

	return -1;
}

static unsigned char _mjpeg_type_get(const unsigned char *data, unsigned int len)
{
	unsigned int i = 0;

	/* RFC 2435 Type 1: YUV 4:2:0 baseline */
	for (i = 0; i + 17 < len; ++i)
	{
		if (data[i] != 0xFF)
		{
			continue;
		}

		if (data[i + 1] == 0xC0 || data[i + 1] == 0xC1 || data[i + 1] == 0xC2)
		{
			unsigned int comp = data[i + 9];

			if (comp == 1)
			{
				return 1;
			}
			if (comp == 3 && i + 18 < len)
			{
				unsigned char y_sf = data[i + 11];
				unsigned char cb_sf = data[i + 14];
				unsigned char cr_sf = data[i + 17];

				if (y_sf == 0x11 && cb_sf == 0x11 && cr_sf == 0x11)
				{
					return 6;
				}
				/* RFC 2435: Type 0 = 4:2:2 (Y 0x21), Type 1 = 4:2:0 (Y 0x22) */
				if (y_sf == 0x21 && cb_sf == 0x11 && cr_sf == 0x11)
				{
					return 0;
				}
				if (y_sf == 0x22 && cb_sf == 0x11 && cr_sf == 0x11)
				{
					return 1;
				}
			}
			break;
		}
	}

	return 1;
}

typedef struct
{
	unsigned char data[MJPEG_QTABLE_DATA_MAX];
	unsigned int len;
	unsigned char precision;
} mjpeg_qtables_s;

/* RFC 2435 §3.1.8: 单个 Quantization Table 头 + 拼接表数据 */
static int _mjpeg_qtables_get(const unsigned char *data, unsigned int len, unsigned char jpeg_type, mjpeg_qtables_s *qtables)
{
	unsigned char tables[4][64];
	unsigned int table_len[4] = {0};
	unsigned int i = 0;
	unsigned int t = 0;

	if (data == NULL || qtables == NULL)
	{
		return -1;
	}

	memset(qtables, 0, sizeof(*qtables));
	memset(tables, 0, sizeof(tables));

	for (i = 0; i + 5 < len; ++i)
	{
		if (data[i] != 0xFF || data[i + 1] != 0xDB)
		{
			continue;
		}

		unsigned int seg_len = ((unsigned int)data[i + 2] << 8) | data[i + 3];
		unsigned int seg_end = i + 2 + seg_len;
		unsigned int pos = i + 4;

		if (seg_end > len)
		{
			break;
		}

		while (pos < seg_end)
		{
			unsigned char pq_tq = data[pos++];
			unsigned char precision = pq_tq >> 4;
			unsigned char index = pq_tq & 0x0F;
			unsigned int tbl_len = (precision == 0) ? 64U : 128U;

			if (index >= 4 || precision != 0 || pos + tbl_len > seg_end)
			{
				break;
			}

			memcpy(tables[index], data + pos, 64);
			table_len[index] = 64;
			pos += tbl_len;
		}

		i = seg_end - 1;
	}

	if (table_len[0] == 0 && table_len[1] == 0 && table_len[2] == 0 && table_len[3] == 0)
	{
		return -1;
	}

	/* RFC 2435 Type 0/1: luma table + chroma table, 64 bytes each */
	if (jpeg_type == 0 || jpeg_type == 1)
	{
		const unsigned char *luma = (table_len[0] > 0) ? tables[0] : NULL;
		const unsigned char *chroma = (table_len[1] > 0) ? tables[1] : luma;

		if (luma == NULL)
		{
			return -1;
		}

		memcpy(qtables->data, luma, 64);
		memcpy(qtables->data + 64, chroma, 64);
		qtables->len = 128;
	}
	else
	{
		for (t = 0; t < 4; ++t)
		{
			if (table_len[t] > 0)
			{
				memcpy(qtables->data + qtables->len, tables[t], table_len[t]);
				qtables->len += table_len[t];
			}
		}

		if (qtables->len == 0)
		{
			return -1;
		}
	}

	qtables->precision = 0;
	return 0;
}

static int _rtp_send_mjpeg_frame(rtp_send_config_s *rtp_send_config, rtp_over_tcp_packet_s *rtp_over_tcp_packet,
				 unsigned char *frame, unsigned int frame_size, int width, int height)
{
	unsigned int payload_offset = 0;
	unsigned char *jpeg_data = NULL;
	unsigned int jpeg_len = 0;
	unsigned int fragment_offset = 0;
	unsigned char width8 = 0;
	unsigned char height8 = 0;
	unsigned char jpeg_type = 1;
	unsigned char qfactor = MJPEG_QFACTOR_INLINE;
	mjpeg_qtables_s qtables;
	int jpeg_width = width;
	int jpeg_height = height;
	int oversize = 0;
	int onvif_pending = 0;
	unsigned int onvif_hdr_start = 0;
	unsigned int onvif_hdr_len = 0;

	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	_NULL_POINTER_CHECK_(rtp_over_tcp_packet, -1);
	_NULL_POINTER_CHECK_(frame, -1);
	if (frame_size == 0)
	{
		__ERR("frame_size is invalid!(%lu) \n", (unsigned long)frame_size);
		return -1;
	}

	if (_mjpeg_parse_sof(frame, frame_size, &jpeg_width, &jpeg_height) != 0)
	{
		jpeg_width = width;
		jpeg_height = height;
	}

	payload_offset = _mjpeg_payload_offset(frame, frame_size);
	jpeg_data = frame + payload_offset;
	jpeg_len = frame_size - payload_offset;
	if (jpeg_len == 0)
	{
		__ERR("mjpeg payload is empty, frame_size=%u offset=%u\n", frame_size, payload_offset);
		return -1;
	}

	jpeg_type = _mjpeg_type_get(frame, frame_size);
	oversize = (jpeg_width > MJPEG_RFC2435_MAX_DIM || jpeg_height > MJPEG_RFC2435_MAX_DIM);

	if (!oversize)
	{
		unsigned int width_blocks = (unsigned int)((jpeg_width + 7) / 8);
		unsigned int height_blocks = (unsigned int)((jpeg_height + 7) / 8);

		if (_mjpeg_qtables_get(frame, frame_size, jpeg_type, &qtables) != 0)
		{
			__ERR("mjpeg qtables parse failed\n");
			return -1;
		}

		if (width_blocks == 0)
		{
			width_blocks = 1;
		}
		if (height_blocks == 0)
		{
			height_blocks = 1;
		}
		width8 = (unsigned char)width_blocks;
		height8 = (unsigned char)height_blocks;
		qfactor = MJPEG_QFACTOR_INLINE;
	}
	else
	{
		/* ONVIF Streaming Spec 5.1.4: RTP header extension carries JPEG markers
		 * (incl. SOF) when size exceeds RFC 2435 8-bit width/height fields. */
		if (frame_size >= 2 && frame[0] == 0xFF && frame[1] == 0xD8)
		{
			onvif_hdr_start = 2;
		}
		if (payload_offset <= onvif_hdr_start)
		{
			__ERR("mjpeg onvif: no jpeg headers before SOS, offset=%u\n", payload_offset);
			return -1;
		}
		onvif_hdr_len = payload_offset - onvif_hdr_start;
		if (onvif_hdr_len > RTP_JPEG_ONVIF_HDR_MAX)
		{
			__ERR("mjpeg onvif hdr too large, len=%u\n", onvif_hdr_len);
			return -1;
		}

		width8 = 0;
		height8 = 0;
		qfactor = 0;
		onvif_pending = 1;
		__INFO("mjpeg %dx%d exceeds RFC2435 max %d, use ONVIF hdr extension\n",
		       jpeg_width, jpeg_height, MJPEG_RFC2435_MAX_DIM);
	}

	while (fragment_offset < jpeg_len || onvif_pending)
	{
		unsigned int remain = jpeg_len - fragment_offset;
		unsigned int rtp_ext_size = 0;
		unsigned int qt_ext = 0;
		unsigned int overhead = 0;
		unsigned int room = 0;
		unsigned int frag_size = 0;
		unsigned char *payload = rtp_over_tcp_packet->rtp_packet.payload;
		unsigned char *jpeg_hdr = payload;
		int is_last = 0;
		int send_onvif = onvif_pending;

		rtp_over_tcp_packet->rtp_packet.rtp_header.extension = 0;

		if (send_onvif)
		{
			unsigned int pad_len = (onvif_hdr_len + 3U) & ~3U;
			unsigned int ext_words = pad_len / 4U;

			rtp_ext_size = RTP_HDR_EXT_HDR_SIZE + pad_len;
			payload[0] = 0xFF;
			payload[1] = 0xD8;
			payload[2] = (unsigned char)((ext_words >> 8) & 0xFF);
			payload[3] = (unsigned char)(ext_words & 0xFF);
			memcpy(payload + RTP_HDR_EXT_HDR_SIZE, frame + onvif_hdr_start, onvif_hdr_len);
			if (pad_len > onvif_hdr_len)
			{
				memset(payload + RTP_HDR_EXT_HDR_SIZE + onvif_hdr_len, 0, pad_len - onvif_hdr_len);
			}
			rtp_over_tcp_packet->rtp_packet.rtp_header.extension = 1;
			jpeg_hdr = payload + rtp_ext_size;
			onvif_pending = 0;
		}

		if (fragment_offset == 0 && !oversize)
		{
			qt_ext = MJPEG_QT_HDR_SIZE + qtables.len;
		}

		overhead = rtp_ext_size + RTP_JPEG_MAIN_HEADER_SIZE + qt_ext;
		if (overhead < RTP_MAX_PACKET_SIZE)
		{
			room = RTP_MAX_PACKET_SIZE - overhead;
			frag_size = (remain > room) ? room : remain;
		}
		else if (remain == 0 && send_onvif)
		{
			frag_size = 0;
		}
		else if (send_onvif)
		{
			/* Headers alone exceed MTU budget: send extension-only packet first. */
			frag_size = 0;
		}
		else
		{
			__ERR("mjpeg packet overhead too large, overhead=%u\n", overhead);
			rtp_over_tcp_packet->rtp_packet.rtp_header.extension = 0;
			return -1;
		}

		is_last = (!onvif_pending && (fragment_offset + frag_size >= jpeg_len)) ? 1 : 0;

		jpeg_hdr[0] = 0;
		jpeg_hdr[1] = (unsigned char)((fragment_offset >> 16) & 0xFF);
		jpeg_hdr[2] = (unsigned char)((fragment_offset >> 8) & 0xFF);
		jpeg_hdr[3] = (unsigned char)(fragment_offset & 0xFF);
		jpeg_hdr[4] = jpeg_type;
		jpeg_hdr[5] = qfactor;
		jpeg_hdr[6] = width8;
		jpeg_hdr[7] = height8;

		if (fragment_offset == 0 && !oversize)
		{
			unsigned char *qt_hdr = jpeg_hdr + RTP_JPEG_MAIN_HEADER_SIZE;

			qt_hdr[0] = 0;
			qt_hdr[1] = qtables.precision;
			qt_hdr[2] = (unsigned char)((qtables.len >> 8) & 0xFF);
			qt_hdr[3] = (unsigned char)(qtables.len & 0xFF);
			memcpy(qt_hdr + MJPEG_QT_HDR_SIZE, qtables.data, qtables.len);
		}

		if (frag_size > 0)
		{
			memcpy(jpeg_hdr + RTP_JPEG_MAIN_HEADER_SIZE + qt_ext, jpeg_data + fragment_offset, frag_size);
		}

		rtp_over_tcp_packet->rtp_packet.rtp_header.marker = is_last;
		if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
				     rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type,
				     rtp_send_config->rtp_video_channel, rtp_over_tcp_packet,
				     rtp_ext_size + RTP_JPEG_MAIN_HEADER_SIZE + qt_ext + frag_size) != 0)
		{
			__ERR("_rtp_send_packet failed! \n");
			rtp_over_tcp_packet->rtp_packet.rtp_header.extension = 0;
			return -1;
		}

		rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
		rtp_over_tcp_packet->rtp_packet.rtp_header.extension = 0;
		fragment_offset += frag_size;
	}

	return 0;
}
							
static int _rtp_header_init(rtp_header_s* rtp_header, unsigned char csrcLen, unsigned char extension,
					unsigned char padding, unsigned char version, unsigned char payloadType, unsigned char marker,
					unsigned short seq, unsigned int timestamp, unsigned int ssrc)
{
	_NULL_POINTER_CHECK_(rtp_header, -1);
	
	rtp_header->csrcLen = csrcLen;
	rtp_header->extension = extension;
	rtp_header->padding = padding;
	rtp_header->version = version;
	rtp_header->payloadType = payloadType;
	rtp_header->marker = marker;
	rtp_header->seq = seq;
	rtp_header->timestamp = timestamp;
	rtp_header->ssrc = ssrc;

	return 0;
}

typedef struct
{
	rtp_send_config_s *rtp_send_config;
	rtp_over_tcp_packet_s *rtp_over_tcp_packet;
} cb_arg_s;

static int _send_video_frame_cb(unsigned char *nalu, unsigned int size, void *cb_arg)
{
	cb_arg_s *l_cb_arg = (cb_arg_s*)cb_arg;

	if (l_cb_arg->rtp_send_config->av_param.video_stream_type == e_stream_type_H264)
	{
		if (_rtp_send_h264_frame(l_cb_arg->rtp_send_config, l_cb_arg->rtp_over_tcp_packet, nalu, size) != 0)
		{
			__ERR("_rtp_send_h264_frame failed! \n");
			return -1;
		}
	}
	else if (l_cb_arg->rtp_send_config->av_param.video_stream_type == e_stream_type_H265)
	{
		if (_rtp_send_h265_frame(l_cb_arg->rtp_send_config, l_cb_arg->rtp_over_tcp_packet, nalu, size) != 0)
		{
			__ERR("_rtp_send_h265_frame failed! \n");
			return -1;
		}
	}
	else if (l_cb_arg->rtp_send_config->av_param.video_stream_type == e_stream_type_MJPEG)
	{
		__ERR("mjpeg should not go through nalu callback\n");
		return -1;
	}
	else
	{
		__ERR("l_cb_arg->rtp_send_config->av_param.video_stream_type is invalid!(%lu) \n", (unsigned long)(l_cb_arg->rtp_send_config->av_param.video_stream_type));
		return -1;
	}
	return 0;
}

static int _send_video_frame(void *p_data, int p_data_len, unsigned long long timestamp,
	rtp_send_config_s *rtp_send_config, rtp_over_tcp_packet_s *rtp_over_tcp_packet, unsigned long long *last_timestamp)
{
	unsigned long long delta_timestamp = 0;

	_NULL_POINTER_CHECK_(p_data, -1);
	if (p_data_len <= 0)
	{
		__ERR("p_data_len is invalid!(%lu) \n", (unsigned long)p_data_len);
		return -1;
	}
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	_NULL_POINTER_CHECK_(rtp_over_tcp_packet, -1);
	_NULL_POINTER_CHECK_(last_timestamp, -1);

	/* rtsp2 compatibility path already provides framePts in milliseconds. */
	if (*last_timestamp == 0)
	{
		*last_timestamp = timestamp;
	}

	delta_timestamp = timestamp - (*last_timestamp);
	if (rtp_send_config->is_replay)
	{
		/*
		 * Replay callback provides framePts in 90k PTS units already, so video RTP
		 * timestamp should use the delta directly.
		 */
		rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp = (unsigned int)delta_timestamp;
	}
	else
	{
		/* Live path provides framePts in milliseconds. */
		rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp = (unsigned int)delta_timestamp * 90;
	}
	rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 0;

	if (rtp_send_config->av_param.video_stream_type == e_stream_type_MJPEG)
	{
		if (_rtp_send_mjpeg_frame(rtp_send_config, rtp_over_tcp_packet, (unsigned char *)p_data,
					  (unsigned int)p_data_len, rtp_send_config->av_param.resolution_width,
					  rtp_send_config->av_param.resolution_height) != 0)
		{
			__ERR("_rtp_send_mjpeg_frame failed!\n");
			return -1;
		}
		return 0;
	}

	cb_arg_s cb_arg;
	cb_arg.rtp_send_config = rtp_send_config;
	cb_arg.rtp_over_tcp_packet = rtp_over_tcp_packet;

	if (rtsp_stream_loop_every_nalu(p_data, (unsigned int)p_data_len, _send_video_frame_cb, (void *)&cb_arg) != 0)
	{
		__ERR("rtsp_stream_loop_every_nalu failed!\n");
		return -1;
	}

	return 0;
}

static int _rtp_send_aac_frame(rtp_send_config_s *rtp_send_config, rtp_over_tcp_packet_s* rtp_over_tcp_packet, unsigned char *frame, unsigned int frame_size)
{
	rtp_over_tcp_packet->rtp_packet.payload[0] = 0x00;
	rtp_over_tcp_packet->rtp_packet.payload[1] = 0x10;
	rtp_over_tcp_packet->rtp_packet.payload[2] = (frame_size & 0x1FE0) >> 5;
	rtp_over_tcp_packet->rtp_packet.payload[3] = (frame_size & 0x1F) << 3;

	memcpy(rtp_over_tcp_packet->rtp_packet.payload + RTP_AAC_PAYLOAD_HEADER_SIZE, frame, frame_size);

	if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_audio_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_audio_channel, rtp_over_tcp_packet, frame_size + RTP_AAC_PAYLOAD_HEADER_SIZE))
	{
		__ERR("_rtp_send_packet failed!");
		return -1;
	}

	rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;

	return 0;
}

static int _rtp_send_g711_frame(rtp_send_config_s *rtp_send_config, rtp_over_tcp_packet_s* rtp_over_tcp_packet, unsigned char *frame, unsigned int frame_size)
{
	memcpy(rtp_over_tcp_packet->rtp_packet.payload, frame, frame_size);

	if (_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_audio_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_audio_channel, rtp_over_tcp_packet, frame_size))
	{
		__ERR("_rtp_send_packet failed!");
		return -1;
	}

	rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;

	return 0;
}

							
static int _send_audio_frame(void *p_data, int size, unsigned long long timestamp, rtp_send_config_s *rtp_send_config, 
	rtp_over_tcp_packet_s *rtp_over_tcp_packet, unsigned long long *last_timestamp, int audio_sample_rate)
{
	unsigned long long delta_timestamp = 0;
	unsigned int rtp_timestamp = 0;

	_NULL_POINTER_CHECK_(p_data, -1);
	if (size <= 0)
	{
		__ERR("size is invalid!(%lu)", (unsigned long)size);
		return -1;
	}
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	_NULL_POINTER_CHECK_(rtp_over_tcp_packet, -1);
	_NULL_POINTER_CHECK_(last_timestamp, -1);

	if (*last_timestamp == 0)
	{
		*last_timestamp = timestamp;
	}

	delta_timestamp = timestamp - *last_timestamp;
	if (rtp_send_config->is_replay)
	{
		/*
		 * Replay callback provides framePts in 90k PTS units, convert to audio RTP
		 * clock with sample_rate / 90000.
		 */
		rtp_timestamp = (unsigned int)((delta_timestamp * (unsigned long long)audio_sample_rate) / 90000ULL);
	}
	else
	{
		/* Live path provides framePts in milliseconds. */
		rtp_timestamp = (unsigned int)delta_timestamp * (audio_sample_rate / 1000);
	}

    if (rtp_send_config->av_param.audio_stream_type == e_stream_type_AAC)
    {
    	rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp = rtp_timestamp;
    	
    	if (_rtp_send_aac_frame(rtp_send_config, rtp_over_tcp_packet, p_data + RTP_AAC_HEADER_SIZE, size - RTP_AAC_HEADER_SIZE) != 0)
		{
			__ERR("_rtp_send_aac_frame failed!");
			return -1;
		}
    }
    else if (rtp_send_config->av_param.audio_stream_type == e_stream_type_G711A || rtp_send_config->av_param.audio_stream_type == e_stream_type_G711U)
    {
        rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp = rtp_timestamp;
    	
        if (_rtp_send_g711_frame(rtp_send_config, rtp_over_tcp_packet, p_data, size) != 0)
		{
			__ERR("_rtp_send_g711_frame failed!");
			return -1;
		}
    }

	return 0;
}


static int _send_ps_composite_frame(void *video_data, int video_data_size, unsigned long long video_timestamp, 
	void *audio_data, int audio_data_size, rtp_send_config_s *rtp_send_config, 
	rtp_over_tcp_packet_s *rtp_over_tcp_packet, unsigned long long *last_timestamp)
{
	int iRet = -1;
	char *ps_packet = NULL;
	unsigned int actually_packet_length = 0;

	_NULL_POINTER_CHECK_(video_data, -1);
	if (video_data_size <= 0)
	{
		__ERR("video_data_size is invalid!(%lu)", (unsigned long)video_data_size);
		return -1;
	}
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	_NULL_POINTER_CHECK_(rtp_over_tcp_packet, -1);
	_NULL_POINTER_CHECK_(last_timestamp, -1);

	if (*last_timestamp == 0)
	{
		*last_timestamp = video_timestamp;
	}

	rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp = (unsigned int)(video_timestamp - (*last_timestamp)) * 90;
	//rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp += 3600;
	rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 0;
	
    mpeg_video_packet ps_packet_info;
    memset(&ps_packet_info, 0, sizeof(mpeg_video_packet));
	ps_packet_info.dts = rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp;
	ps_packet_info.pts = rtp_over_tcp_packet->rtp_packet.rtp_header.timestamp;
	ps_packet_info.data = video_data;
	ps_packet_info.size = video_data_size;
	
	ps_packet_info.audio_data = audio_data;
	ps_packet_info.audio_size = audio_data_size;

	unsigned int ps_packet_length = PS_HDR_LEN + SYS_HDR_LEN + PSM_HDR_LEN + (PES_HDR_LEN * (video_data_size / PS_PACKET_MAX_SIZE)) 
		+ (PES_HDR_LEN * ((video_data_size % PS_PACKET_MAX_SIZE) > 0 ? 1 : 0)) + video_data_size + PES_HDR_LEN + ps_packet_info.audio_size + 2048;

    ps_packet = (char *)anj_mw_malloc(ps_packet_length);
    ANJ_CHK((ps_packet != NULL), -1, "anj_mw_malloc %d failed", ps_packet_length);
    memset(ps_packet, 0, ps_packet_length);

	ANJ_CHK((ps_packet_build(&ps_packet_info, ps_packet, ps_packet_length, &actually_packet_length, rtp_send_config) == 0), -1, "ps_packet_build failed!");

	if (actually_packet_length <= RTP_MAX_PACKET_SIZE)
	{
		rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 1;
		memcpy(rtp_over_tcp_packet->rtp_packet.payload, ps_packet, actually_packet_length);
		ANJ_CHK((_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, actually_packet_length) == 0),
			-1, "_rtp_send_packet failed!");

		rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
	}
	else
	{
        int packet_number = actually_packet_length / RTP_MAX_PACKET_SIZE;
        int remain_packet_size = actually_packet_length % RTP_MAX_PACKET_SIZE;
        int i = 0;
		int offset = 0;

		/* 发送完整的包 */
		for (i = 0; i < packet_number; i++)
		{
			rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 0;
			memcpy(rtp_over_tcp_packet->rtp_packet.payload, ps_packet + offset, RTP_MAX_PACKET_SIZE);
			
			ANJ_CHK((_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, RTP_MAX_PACKET_SIZE) == 0),
				-1, "_rtp_send_packet failed!");

			rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
			offset += RTP_MAX_PACKET_SIZE;
		}

		/* 发送剩余的数据 */
		if (remain_packet_size > 0)
		{			
			rtp_over_tcp_packet->rtp_packet.rtp_header.marker = 1;
			memcpy(rtp_over_tcp_packet->rtp_packet.payload, ps_packet + offset, remain_packet_size);
			
			ANJ_CHK((_rtp_send_packet(rtp_send_config->rtp_fd, rtp_send_config->rtp_client_video_over_udp_port,
					rtp_send_config->rtp_over_udp_ip, rtp_send_config->rtp_send_type, rtp_send_config->rtp_video_channel, rtp_over_tcp_packet, remain_packet_size) == 0),
				-1, "_rtp_send_packet failed!");
			
			rtp_over_tcp_packet->rtp_packet.rtp_header.seq++;
		}
	}

	iRet = 0;
endFunc:
	anj_mw_free(ps_packet);
	ps_packet = NULL;
	return iRet;
}

static void _replay_video_pace(rec_pb_poper *pPoper, media_frame_info_t *pFrameInfo)
{
    if (pPoper == NULL || pFrameInfo == NULL)
    {
        return;
    }

    if ((pPoper->tLastPts > 0) && (pFrameInfo->frameParam.framePts > pPoper->tLastPts))
    {
        unsigned long long tNowMs = anj_mw_get_cputime_ms(NULL);
        unsigned int iDiffTime = (pFrameInfo->frameParam.framePts - pPoper->tLastPts) / 90;

        if (pPoper->iSpeed > PB_SPEED_0)
        {
            iDiffTime = iDiffTime / pPoper->iSpeed;
            if (pPoper->iSpeed > PB_SPEED_2)
            {
                iDiffTime = iDiffTime / (pPoper->iSpeed / PB_SPEED_4);
            }
        }

        if (iDiffTime > 1000)
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

        if (iDiffTime < 10)
        {
            usleep(10 * 1000);
        }
        else if (iDiffTime > 0)
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
}

static int _rtsp_replay_cb(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e EventID)
{
    rec_pb_poper *pPoper = (rec_pb_poper *)pHandle;
    rtp_send_config_s *rtp_send_config = NULL;

    if (pPoper == NULL)
    {
        return -1;
    }

    rtp_send_config = _replay_slot_get(pPoper->iPopId);
    if (rtp_send_config == NULL)
    {
        return -1;
    }

    if (EventID == PB_CB_FINISH)
    {
        rtp_send_config->replay_finished = 1;
        return 0;
    }

    if (EventID != PB_CB_START || pFrameInfo == NULL || rtp_send_config->rtp_send_thread.start == 0)
    {
        return 0;
    }

    if ((pFrameInfo->frameParam.frameType == MEDIA_VFRAME_I) ||
        (pFrameInfo->frameParam.frameType == MEDIA_VFRAME_P))
    {
        if (rtp_send_config->av_param.video_enable && rtp_send_config->replay_video_packet != NULL)
        {
            _send_video_frame(pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen, pFrameInfo->frameParam.framePts,
                              rtp_send_config, (rtp_over_tcp_packet_s *)rtp_send_config->replay_video_packet,
                              &rtp_send_config->replay_last_video_timestamp);
        }
        _replay_video_pace(pPoper, pFrameInfo);
    }
    else if (pFrameInfo->frameParam.frameType == MEDIA_AFRAME_A)
    {
        if (rtp_send_config->av_param.audio_enable && rtp_send_config->replay_audio_packet != NULL)
        {
            _send_audio_frame(pFrameInfo->frameBuf, pFrameInfo->frameParam.frameLen, pFrameInfo->frameParam.framePts,
                              rtp_send_config, (rtp_over_tcp_packet_s *)rtp_send_config->replay_audio_packet,
                              &rtp_send_config->replay_last_audio_timestamp,
                              rtp_send_config->av_param.audio_sample_rate);
        }
    }

    return 0;
}

static int _mbuf_frame_is_video(const media_frame_info_t *info)
{
	if (info == NULL)
	{
		return 0;
	}
	return (info->frameParam.frameType == MEDIA_VFRAME_I ||
			info->frameParam.frameType == MEDIA_VFRAME_P) ? 1 : 0;
}

static int _mbuf_frame_is_aac_audio(const media_frame_info_t *info)
{
	if (info == NULL)
	{
		return 0;
	}
	return (info->frameParam.frameType == MEDIA_AFRAME_A &&
			info->frameParam.frameCodec == MEDIA_CODEC_AUDIO_AAC) ? 1 : 0;
}

static int _mbuf_try_read_aac_frame(ANJ_MBUF_HANDLE *reader, media_frame_info_t *info)
{
	int retry;

	if (reader == NULL || info == NULL)
	{
		return -1;
	}

	memset(info, 0, sizeof(*info));
	for (retry = 0; retry < 8; ++retry)
	{
		if (anj_mbuf_read_frame(reader, 0, info, 50) <= 0)
		{
			continue;
		}
		if (_mbuf_frame_is_aac_audio(info))
		{
			return 0;
		}
		anj_mbuf_read_release(reader, info);
		memset(info, 0, sizeof(*info));
	}
	return -1;
}

static int _rtp_send_process(void *argv, int *bStart)
{
	__LOG_ENTER();
	_NULL_POINTER_CHECK_(argv, -1);
	_NULL_POINTER_CHECK_(bStart, -1);

	rtp_send_config_s *rtp_send_config = (rtp_send_config_s *)argv;
	int iRet = 0;
	rtp_over_tcp_packet_s *rtp_video_packet = NULL;
	rtp_over_tcp_packet_s *rtp_audio_packet = NULL;
	rtp_over_tcp_packet_s *rtp_composite_packet = NULL;
	ANJ_MBUF_HANDLE *readerid = NULL;
	ANJ_MBUF_HANDLE *audio_readerid = NULL;
	unsigned int ssrc = rtp_send_config->ssrc;
	rtp_send_config->replay_slot = -1;
	
	unsigned long long last_video_timestamp = 0;
	unsigned long long last_audio_timestamp = 0;
	
	if (rtp_send_config->rtp_payload_type == SINGLE_COMPOSITE_PS_STREAM)
	{
		rtp_composite_packet = (rtp_over_tcp_packet_s *)anj_mw_malloc(RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + RTP_MAX_PACKET_SIZE);
		if (rtp_composite_packet == NULL)
		{
			__ERR("anj_mw_malloc %d failed", RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + RTP_MAX_PACKET_SIZE);
			iRet = -1;
			goto endFunc;
		}
		memset(rtp_composite_packet, 0, RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + RTP_MAX_PACKET_SIZE);
		if (_rtp_header_init(&rtp_composite_packet->rtp_packet.rtp_header, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_PS, 0, 0, 0, ssrc) != 0)
		{
			__ERR("_rtp_header_init failed!");
			iRet = -1;
			goto endFunc;
		}
	}
	else if (rtp_send_config->rtp_payload_type == MULTIPLE_MEDIA_STREAMS)
	{
		int rtp_header_size;
		int rtp_payload_type;
		if (rtp_send_config->av_param.video_enable)
		{
			if (rtp_send_config->av_param.video_stream_type == e_stream_type_H264)
			{
				rtp_header_size = MAX(RTP_FU_A_HEADER_SIZE, H264_NALU_HEADER_BYTE_SIZE);
				rtp_payload_type = RTP_PAYLOAD_TYPE_H264;
			}
			else if (rtp_send_config->av_param.video_stream_type == e_stream_type_H265)
			{
				rtp_header_size = MAX(RTP_H265_INDICATOR_HEADER_SIZE, H265_NALU_HEADER_BYTE_SIZE);
				rtp_payload_type = RTP_PAYLOAD_TYPE_H265;
			}
			else if (rtp_send_config->av_param.video_stream_type == e_stream_type_MJPEG)
			{
				rtp_header_size = RTP_HDR_EXT_HDR_SIZE + RTP_JPEG_ONVIF_HDR_MAX +
						  RTP_JPEG_MAIN_HEADER_SIZE + RTP_JPEG_MAX_EXT_SIZE;
				rtp_payload_type = RTP_PAYLOAD_TYPE_MJPG;
			}
			else
			{
				__ERR("rtp_send_config->av_param.video_stream_type is invalid!(%lu)", (unsigned long)(rtp_send_config->av_param.video_stream_type));
				iRet = -1;
				goto endFunc;
			}
			int total_rtp_packet_size = RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + rtp_header_size + RTP_MAX_PACKET_SIZE;

			// check struct size
			if (sizeof(rtp_over_tcp_packet_s) != RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE
				|| sizeof(rtp_packet_s) != RTP_HEADER_SIZE
				|| sizeof(rtp_header_s) != RTP_HEADER_SIZE)
			{
				__ERR("RTP struct size is not correct that may caused by OS bits!");
				iRet = -1;
				goto endFunc;
			}

			rtp_video_packet = (rtp_over_tcp_packet_s *)anj_mw_malloc(total_rtp_packet_size);
			if (rtp_video_packet == NULL)
			{
				__ERR("anj_mw_malloc %d failed", total_rtp_packet_size);
				iRet = -1;
				goto endFunc;
			}
			memset(rtp_video_packet, 0, total_rtp_packet_size);
			if (_rtp_header_init(&rtp_video_packet->rtp_packet.rtp_header, 0, 0, 0, RTP_VESION, rtp_payload_type, 0, 0, 0, ssrc) != 0)
			{
				__ERR("_rtp_header_init failed! (venc=%d)", rtp_send_config->av_param.video_stream_type);
				iRet = -1;
				goto endFunc;
			}
		}
		
		if (rtp_send_config->av_param.audio_enable)
		{
			rtp_header_size = RTP_AAC_PAYLOAD_HEADER_SIZE;
		    if (rtp_send_config->av_param.audio_stream_type == e_stream_type_AAC)
		    {
		    	rtp_payload_type = RTP_PAYLOAD_TYPE_AAC;
		    }
            else if (rtp_send_config->av_param.audio_stream_type == e_stream_type_G711A)
            {
            	rtp_payload_type = RTP_PAYLOAD_TYPE_G711A;
            }
			else if (rtp_send_config->av_param.audio_stream_type == e_stream_type_G711U)
            {
            	rtp_payload_type = RTP_PAYLOAD_TYPE_G711U;
            }
			
			rtp_audio_packet = (rtp_over_tcp_packet_s *)anj_mw_malloc(RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + rtp_header_size + RTP_MAX_PACKET_SIZE);
			if (rtp_audio_packet == NULL)
			{
				__ERR("anj_mw_malloc %d failed", RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + rtp_header_size + RTP_MAX_PACKET_SIZE);
				iRet = -1;
				goto endFunc;
			}
			memset(rtp_audio_packet, 0, RTP_OVER_TCP_HEAD_SIZE + RTP_HEADER_SIZE + rtp_header_size + RTP_MAX_PACKET_SIZE);
			if (_rtp_header_init(&rtp_audio_packet->rtp_packet.rtp_header, 0, 0, 0, RTP_VESION, rtp_payload_type, 1, 0, 0, ssrc) != 0)
			{
				__ERR("_rtp_header_init failed! (aenc=%d)", rtp_send_config->av_param.audio_stream_type);
				iRet = -1;
				goto endFunc;
			}
		}
	}
	else
	{
		__ERR("rtp_send_config->rtp_payload_type is invalid!(%lu)", (unsigned long)(rtp_send_config->rtp_payload_type));
		iRet = -1;
		goto endFunc;
	}

	rtp_send_config->replay_video_packet = rtp_video_packet;
	rtp_send_config->replay_audio_packet = rtp_audio_packet;
	rtp_send_config->replay_composite_packet = rtp_composite_packet;
	rtp_send_config->replay_last_video_timestamp = 0;
	rtp_send_config->replay_last_audio_timestamp = 0;
	rtp_send_config->replay_finished = 0;

	if (rtp_send_config->is_replay)
	{
		rtp_send_config->replay_slot = _replay_slot_alloc(rtp_send_config);
		if (rtp_send_config->replay_slot < 0)
		{
			__ERR("replay slot alloc failed");
			iRet = -1;
			goto endFunc;
		}

		rtp_send_config->replay_handle = anj_record_pb_create((int)rtp_send_config->replay_channel,
			rtp_send_config->replay_start_time, 0, 0, rtp_send_config->replay_slot, _rtsp_replay_cb);
		if (rtp_send_config->replay_handle == NULL)
		{
			__ERR("anj_record_pb_create failed, chn=%u start=%u", rtp_send_config->replay_channel, rtp_send_config->replay_start_time);
			iRet = -1;
			goto endFunc;
		}

		while ((*bStart != 0) && (rtp_send_config->replay_finished == 0))
		{
			usleep(200 * 1000);
		}
		goto endFunc;
	}

	if ((rtp_send_config->stream_index != MAIN_STREAM) && 
		(rtp_send_config->stream_index != SUB_STREAM))
	{
		__ERR("rtp_send_config->stream_index is invalid!(%lu)", (unsigned long)(rtp_send_config->stream_index));
		iRet = -1;
		goto endFunc;
	}

	readerid = anj_mbuf_create_reader(rtp_send_config->stream_index, 1);
	if (readerid == NULL)
	{
		__DBG("anj_mbuf_create_reader chn:%d failed!\n", rtp_send_config->stream_index);
		iRet = -1;
		goto endFunc;
	}
	__INFO("anj_mbuf_create_reader chn:%d readerid:%p OK!\n", rtp_send_config->stream_index, readerid);

	if (rtp_send_config->rtp_payload_type == SINGLE_COMPOSITE_PS_STREAM &&
		rtp_send_config->av_param.audio_enable)
	{
		audio_readerid = anj_mbuf_create_reader(MAIN_STREAM, 1);
		if (audio_readerid == NULL)
		{
			__ERR("anj_mbuf_create_reader audio chn:%d failed!", MAIN_STREAM);
			iRet = -1;
			goto endFunc;
		}
	}

	int bFirstFrame = 1;
	media_frame_info_t stReadFrameInfo = {0};
	media_frame_info_t stPsVideoInfo = {0};
	media_frame_info_t stPsAudioInfo = {0};

//	rtp_send_config->b_rtp_send_running = 0;
	while (*bStart != 0)
	{
		// send
		if (rtp_send_config->rtp_payload_type == SINGLE_COMPOSITE_PS_STREAM)
		{
			if (rtp_send_config->av_param.video_enable)
			{
				int video_timeout_ms = bFirstFrame ? 100 : 50;

				memset(&stPsVideoInfo, 0, sizeof(stPsVideoInfo));
				if (anj_mbuf_read_frame(readerid, bFirstFrame, &stPsVideoInfo, video_timeout_ms) > 0 &&
					_mbuf_frame_is_video(&stPsVideoInfo))
				{
					unsigned char *audio_buf = NULL;
					int audio_len = 0;
					int audio_held = 0;

					if (rtp_send_config->av_param.audio_enable && audio_readerid != NULL)
					{
						if (_mbuf_try_read_aac_frame(audio_readerid, &stPsAudioInfo) == 0)
						{
							audio_buf = stPsAudioInfo.frameBuf;
							audio_len = (int)stPsAudioInfo.frameParam.frameLen;
							audio_held = 1;
						}
					}

					if (_send_ps_composite_frame(stPsVideoInfo.frameBuf,
							(int)stPsVideoInfo.frameParam.frameLen, stPsVideoInfo.frameParam.framePts,
							audio_buf, audio_len, rtp_send_config, rtp_composite_packet, &last_video_timestamp))
					{
						__ERR("_send_ps_composite_frame failed!");
						break;
					}

					if (audio_held)
					{
						anj_mbuf_read_release(audio_readerid, &stPsAudioInfo);
					}
					anj_mbuf_read_release(readerid, &stPsVideoInfo);
					bFirstFrame = 0;
				}
				else if (stPsVideoInfo.frameBuf != NULL)
				{
					anj_mbuf_read_release(readerid, &stPsVideoInfo);
				}
			}
		}
		else if (rtp_send_config->rtp_payload_type == MULTIPLE_MEDIA_STREAMS)
		{
			if (rtp_send_config->av_param.video_enable)
			{
				int need_key_frame = bFirstFrame;
				int read_ret = 0;
				int read_timeout = (bFirstFrame) ? 2000 : 200;

				if (rtp_send_config->av_param.video_stream_type == e_stream_type_MJPEG)
				{
					need_key_frame = 0;
				}

				read_ret = anj_mbuf_read_frame(readerid, need_key_frame, &stReadFrameInfo, read_timeout);

				if (read_ret > 0)
				{
					if ((stReadFrameInfo.frameParam.frameType == MEDIA_VFRAME_P) ||
						(stReadFrameInfo.frameParam.frameType == MEDIA_VFRAME_I))
					{
						if (_send_video_frame(stReadFrameInfo.frameBuf,
							stReadFrameInfo.frameParam.frameLen, stReadFrameInfo.frameParam.framePts,
							rtp_send_config, rtp_video_packet, &last_video_timestamp) != 0)
						{
							__ERR("_send_video_frame failed!\n");
							if (!rtp_send_config->is_multicast)
							{
								anj_mbuf_read_release(readerid, &stReadFrameInfo);
								break;
							}
						}
					}
					else if (stReadFrameInfo.frameParam.frameType == MEDIA_AFRAME_A)
					{
						if (rtp_send_config->av_param.audio_enable)
						{
							if (_send_audio_frame(stReadFrameInfo.frameBuf,
								stReadFrameInfo.frameParam.frameLen, stReadFrameInfo.frameParam.framePts,
								rtp_send_config, rtp_audio_packet, &last_audio_timestamp,
								rtp_send_config->av_param.audio_sample_rate) != 0)
							{
								__ERR("_send_audio_frame failed!\n");
								if (!rtp_send_config->is_multicast)
								{
									anj_mbuf_read_release(readerid, &stReadFrameInfo);
									break;
								}
							}
						}
					}
					anj_mbuf_read_release(readerid, &stReadFrameInfo);
					bFirstFrame = 0;
				}
			}
			else if (rtp_send_config->av_param.audio_enable)
			{
				/* Talk session may SETUP only outbound audio (trackID=2) without video */
				if (0 < anj_mbuf_read_frame(readerid, bFirstFrame, &stReadFrameInfo, (bFirstFrame) ? 2000 : 200))
				{
					if (stReadFrameInfo.frameParam.frameType == MEDIA_AFRAME_A)
					{
						if (_send_audio_frame(stReadFrameInfo.frameBuf,
							stReadFrameInfo.frameParam.frameLen, stReadFrameInfo.frameParam.framePts,
							rtp_send_config, rtp_audio_packet, &last_audio_timestamp,
							rtp_send_config->av_param.audio_sample_rate) != 0)
						{
							__ERR("_send_audio_frame failed!\n");
							if (!rtp_send_config->is_multicast)
							{
								anj_mbuf_read_release(readerid, &stReadFrameInfo);
								break;
							}
						}
					}
					anj_mbuf_read_release(readerid, &stReadFrameInfo);
					bFirstFrame = 0;
				}
			}
		}
		else
		{
			__ERR("invalid rtp_payload_type(errno=%d, errmsg=%s)\n", errno, strerror(errno));
			iRet = -1;
			goto endFunc;
		}
	}

endFunc:
	if (rtp_send_config->replay_handle)
	{
		anj_record_pb_release((REC_HANDLE)rtp_send_config->replay_handle);
		rtp_send_config->replay_handle = NULL;
	}
	if (rtp_send_config->replay_slot >= 0)
	{
		_replay_slot_free(rtp_send_config->replay_slot);
		rtp_send_config->replay_slot = -1;
	}

	if (readerid)
	{
		anj_mbuf_destory_reader(readerid);
		__INFO("anj_mbuf_destory_reader readerid:%p OK!\n", readerid);
		readerid = NULL;
	}

	if (audio_readerid)
	{
		anj_mbuf_destory_reader(audio_readerid);
		__INFO("anj_mbuf_destory_reader audio_readerid:%p OK!\n", audio_readerid);
		audio_readerid = NULL;
	}

	anj_mw_free(rtp_video_packet);
	rtp_video_packet = NULL;
	anj_mw_free(rtp_audio_packet);
	rtp_audio_packet = NULL;
	anj_mw_free(rtp_composite_packet);
	rtp_composite_packet = NULL;
	
	__LOG_LEAVE();
	return iRet;
}

int rtp_send_start(rtp_send_config_s *rtp_send_config)
{
	_NULL_POINTER_CHECK_(rtp_send_config, -1);

	memset(&rtp_send_config->rtp_send_thread, 0, sizeof(rtp_send_config->rtp_send_thread));
	rtp_send_config->rtp_send_thread.bAutoDestroy = 0;
	strncpy(rtp_send_config->rtp_send_thread.iThreadName, "rtsp_rtp_send", sizeof(rtp_send_config->rtp_send_thread.iThreadName) - 1);
	rtp_send_config->rtp_send_thread.iThreadjob.ctx = rtp_send_config;
	rtp_send_config->rtp_send_thread.iThreadjob.func = _rtp_send_process;
	if (anj_thread_task_create(&rtp_send_config->rtp_send_thread) != 0)
	{
		__ERR("anj_thread_task_create failed!\n");
		return -1;
	}

	return 0;
}

int rtp_send_stop(rtp_send_config_s *rtp_send_config)
{
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	if (rtp_send_config->rtp_send_thread.start == 0)
	{
		return 0;
	}
	if (anj_thread_task_destroy(&rtp_send_config->rtp_send_thread, -1) < 0)
	{
		__ERR("anj_thread_task_destroy failed!\n");
		return -1;
	}
	return 0;
}

static int _get_send_buf_size(int stream_index)
{
	if (stream_index == MAIN_STREAM)
	{
		return MAIN_STREAM_SOCKET_SENDBUFF_SIZE;
	}
	else
	{
		return SUB_STREAM_SOCKET_SENDBUFF_SIZE;
	}
}

int rtp_rtcp_udp_fd_init(int *rtp_rtcp_udp_fd, int port, int stream_index)
{
	_NULL_POINTER_CHECK_(rtp_rtcp_udp_fd, -1);

	*rtp_rtcp_udp_fd = create_client_socket(SOCK_DGRAM, _get_send_buf_size(stream_index), 2*1024, 1, port, 1, 0, NULL, -1, NULL);
	return sk_is_valid(*rtp_rtcp_udp_fd) ? 0 : -1;
}

void rtp_udp_mcast_sockopt(int fd, const char *local_ip)
{
	unsigned char ttl = 255;
	unsigned char loop = 0;
	struct in_addr if_addr;

	if (fd < 0)
	{
		return;
	}
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));
	setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loop, sizeof(loop));

	/* Bind multicast egress to LAN IP; otherwise sendto(224.x) may get ENETUNREACH. */
	if (local_ip != NULL && local_ip[0] != '\0')
	{
		memset(&if_addr, 0, sizeof(if_addr));
		if_addr.s_addr = inet_addr(local_ip);
		if (if_addr.s_addr != INADDR_NONE && if_addr.s_addr != INADDR_ANY)
		{
			if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&if_addr, sizeof(if_addr)) < 0)
			{
				__ERR("IP_MULTICAST_IF failed!(sk=%d, ip=%s)(errno=%d, errmsg=%s)",
					fd, local_ip, errno, strerror(errno));
			}
			else
			{
				__INFO("IP_MULTICAST_IF sk=%d ip=%s", fd, local_ip);
			}
		}
	}
}

int rtp_rtcp_tcp_fd_init(int *rtp_rtcp_tcp_fd, int port, char* server_ip, int server_port, int stream_index)
{
	_NULL_POINTER_CHECK_(rtp_rtcp_tcp_fd, -1);

	*rtp_rtcp_tcp_fd = create_client_socket(SOCK_STREAM, _get_send_buf_size(stream_index), 2*1024, 1, port, 1, 1, server_ip, server_port, NULL);
	return sk_is_valid(*rtp_rtcp_tcp_fd) ? 0 : -1;
}
