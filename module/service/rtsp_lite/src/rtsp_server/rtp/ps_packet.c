#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#include "anj_mw_comm.h"
#include "rtp.h"
#include "ps_packet.h"

#define AVC_NALU_TYPE_SPS 7
#define HEVC_NALU_TYPE_VPS_NUT 32
#define HEVC_NALU_TYPE_SPS_NUT 33
#define HEVC_NALU_TYPE_PPS_NUT 34
#define HEVC_NALU_TYPE_IDR_W_RADL 19

#define PS_STREAM_ID_VIDEO	(0xE0)
#define PS_STREAM_ID_AUDIO	(0xC0)

typedef struct
{
    unsigned int size;
    unsigned int data;
    unsigned int mask;
    unsigned char *p_data;
} bits_buffer_s;

typedef enum ps_stream_type
{ 
	PS_STREAM_TYPE_AUDIO_AAC			= 0x0F,
	PS_STREAM_TYPE_VIDEO_H264           = 0x1B,
	PS_STREAM_TYPE_VIDEO_H265			= 0x24,
	PS_STREAM_TYPE_AUDIO_LPCM           = 0x8B,
	PS_STREAM_TYPE_AUDIO_G711A          = 0x90,
	PS_STREAM_TYPE_AUDIO_G711U          = 0x91,
	PS_STREAM_TYPE_AUDIO_G722_1         = 0x92,
	PS_STREAM_TYPE_AUDIO_G723_1         = 0x93,
	PS_STREAM_TYPE_AUDIO_G729           = 0x99,
	PS_STREAM_TYPE_AUDIO_SVAC           = 0x9B
} ps_stream_type_e;

static int bits_init(bits_buffer_s *buffer, unsigned int size, unsigned char *buf, int for_read)
{
    memset(buffer, 0, sizeof(bits_buffer_s));
    buffer->size = size;
    buffer->mask = 0x80;
    if (for_read)
    {
        buffer->p_data = buf;
    }
    else if (buf)
    {
        buffer->p_data = buf;
        memset(buffer->p_data, 0, size);
    }
    else
    {
        buffer->p_data = (unsigned char *)anj_mw_malloc(size);
        if (buffer->p_data == NULL)
        {
            __ERR("anj_mw_malloc %u failed", size);
            return -1;
        }
        memset(buffer->p_data, 0, size);
    }
    return 0;
}

static uint32_t bits_write(bits_buffer_s *buffer, int count, uint32_t bits)
{
    _NULL_POINTER_CHECK_(buffer, 0);
    while (count > 0)
    {
        count--;
        if ((bits >> count) & 0x01)
        {
            buffer->p_data[buffer->data] |= buffer->mask;
        }
        else
        {
            buffer->p_data[buffer->data] &= (unsigned char)~buffer->mask;
        }
        buffer->mask >>= 1;
        if (buffer->mask == 0)
        {
            buffer->data++;
            buffer->mask = 0x80;
        }
    }
    return 0;
}

static int _ps_header_build(char *ps_header_buf, unsigned long long pts)
{
	_NULL_POINTER_CHECK_(ps_header_buf, -1);
	bits_buffer_s bitsBuffer;
	unsigned long long scr_ext = 0;

	bits_init(&bitsBuffer, PS_HDR_LEN, (unsigned char*)ps_header_buf, 0);
	bits_write(&bitsBuffer, 32, 0x000001BA);			/*start codes*/
	bits_write(&bitsBuffer, 2, 	1);						/*marker bits '01b'*/
	bits_write(&bitsBuffer, 3, 	(pts>>30)&0x07);     	/*System clock [32..30]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
	bits_write(&bitsBuffer, 15, (pts>>15)&0x7FFF);   	/*System clock [29..15]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
	bits_write(&bitsBuffer, 15, pts&0x7fff);         	/*System clock [29..15]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
	bits_write(&bitsBuffer, 9, 	scr_ext&0x01ff);		/*System clock [14..0]*/
	bits_write(&bitsBuffer, 1, 	1);						/*marker bit*/
	bits_write(&bitsBuffer, 22, (255)&0x3fffff);		/*bit rate(n units of 50 bytes per second.)*/
	bits_write(&bitsBuffer, 2, 	3);						/*marker bits '11'*/
	bits_write(&bitsBuffer, 5, 	0x1f);					/*reserved(reserved for future use)*/
	bits_write(&bitsBuffer, 3, 	0);						/*stuffing length*/

	return 0;
}

static int _ps_system_header_build(char *system_header_buf)
{
	_NULL_POINTER_CHECK_(system_header_buf, -1);
	
	bits_buffer_s bitsBuffer;
	bits_init(&bitsBuffer, SYS_HDR_LEN, (unsigned char*)system_header_buf, 0);
	/*system header*/
	bits_write(&bitsBuffer, 32, 0x000001BB);			/*start code*/
    bits_write(&bitsBuffer, 16, SYS_HDR_LEN-6);			/*header_length 表示次字节后面的长度，后面的相关头也是次意思*/
    bits_write(&bitsBuffer, 1,	 1);            		/*marker_bit*/
	bits_write(&bitsBuffer, 22, 50000);					/*rate_bound*/
    bits_write(&bitsBuffer, 1,  1);            			/*marker_bit*/
    bits_write(&bitsBuffer, 6,  1);            			/*audio_bound*/
    bits_write(&bitsBuffer, 1,  0);            			/*fixed_flag */
    bits_write(&bitsBuffer, 1,  1);        				/*CSPS_flag */
    bits_write(&bitsBuffer, 1,  1);        				/*system_audio_lock_flag*/
    bits_write(&bitsBuffer, 1,  1);        				/*system_video_lock_flag*/
    bits_write(&bitsBuffer, 1,  1);        				/*marker_bit*/
    bits_write(&bitsBuffer, 5,  1);        				/*video_bound*/
    bits_write(&bitsBuffer, 1,  0);        				/*dif from mpeg1*/
    bits_write(&bitsBuffer, 7,  0x7F);     				/*reserver*/

	/*audio stream bound*/
    bits_write(&bitsBuffer, 8,  PS_STREAM_ID_AUDIO);    /*stream_id*/
    bits_write(&bitsBuffer, 2,  3);        				/*marker_bit */
    bits_write(&bitsBuffer, 1,  0);           		 	/*PSTD_buffer_bound_scale*/
    bits_write(&bitsBuffer, 13, 512);          			/*PSTD_buffer_size_bound*/
    
	/*video stream bound*/
    bits_write(&bitsBuffer, 8,  PS_STREAM_ID_VIDEO);    /*stream_id*/
    bits_write(&bitsBuffer, 2,  3);        				/*marker_bit */
    bits_write(&bitsBuffer, 1,  1);        				/*PSTD_buffer_bound_scale*/
    bits_write(&bitsBuffer, 13, 2048);     				/*PSTD_buffer_size_bound*/
    
	return 0;
}

static int _ps_system_map_build(char *system_map_buf, rtp_send_config_s *rtp_send_config)
{
	_NULL_POINTER_CHECK_(system_map_buf, -1);
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	
	bits_buffer_s bitsBuffer;
	bits_init(&bitsBuffer, PSM_HDR_LEN, (unsigned char*)system_map_buf, 0);
	bits_write(&bitsBuffer, 24,	0x000001);	/*start code*/
	bits_write(&bitsBuffer, 8, 	0xBC);		/*map stream id*/
	bits_write(&bitsBuffer, 16,	18);		/*program stream map length*/
	bits_write(&bitsBuffer, 1, 	1);			/*current next indicator */
	bits_write(&bitsBuffer, 2, 	3);			/*reserved*/
	bits_write(&bitsBuffer, 5, 	0); 		/*program stream map version*/
    bits_write(&bitsBuffer, 7, 	0x7F);		/*reserved */
	bits_write(&bitsBuffer, 1, 	1);			/*marker bit */
	bits_write(&bitsBuffer, 16,	0); 		/*programe stream info length*/
	bits_write(&bitsBuffer, 16, 8); 		/*elementary stream map length	is*/

	/*video*/
	if (rtp_send_config->av_param.video_enable == 1)
	{
		if (rtp_send_config->av_param.video_stream_type == e_stream_type_H264)
		{
			bits_write(&bitsBuffer, 8,	PS_STREAM_TYPE_VIDEO_H264); 	/*stream_type*/
			bits_write(&bitsBuffer, 8,	PS_STREAM_ID_VIDEO);			/*elementary_stream_id*/
			bits_write(&bitsBuffer, 16, 0); 							/*elementary_stream_info_length */
		}
		else if (rtp_send_config->av_param.video_stream_type == e_stream_type_H265)
		{
			bits_write(&bitsBuffer, 8,	PS_STREAM_TYPE_VIDEO_H265); 	/*stream_type*/
			bits_write(&bitsBuffer, 8,	PS_STREAM_ID_VIDEO);			/*elementary_stream_id*/
			bits_write(&bitsBuffer, 16, 0); 							/*elementary_stream_info_length */
		}
		else
		{
			__ERR("av_param.video_enable=%d, encode_format=%d", rtp_send_config->av_param.video_enable, rtp_send_config->av_param.video_stream_type);
			return -1;
		}
	}
	else
	{
		__ERR("rtp_send_config->av_param.video_enable is invalid!(%lu)", (unsigned long)(rtp_send_config->av_param.video_enable));
		return -1;
	}

	/*audio*/
	bits_write(&bitsBuffer, 8,	PS_STREAM_TYPE_AUDIO_AAC);		/*stream_type*/
	bits_write(&bitsBuffer, 8,	PS_STREAM_ID_AUDIO);			/*elementary_stream_id*/
	bits_write(&bitsBuffer, 16, 0); 							/*elementary_stream_info_length is*/

	/*crc (2e b9 0f 3d)*/
	bits_write(&bitsBuffer, 8, 	0x45);		/*crc (24~31) bits*/
	bits_write(&bitsBuffer, 8, 	0xBD);		/*crc (16~23) bits*/
	bits_write(&bitsBuffer, 8, 	0xDC);		/*crc (8~15) bits*/
	bits_write(&bitsBuffer, 8, 	0xF4);		/*crc (0~7) bits*/

	return 0;
}

static int _ps_pes_header_build(char *pes_header, int data_len, int pts, int dts, unsigned int stream_id)
{
	_NULL_POINTER_CHECK_(pes_header, -1);
	
	bits_buffer_s bitsBuffer;
	bits_init(&bitsBuffer, PES_HDR_LEN, (unsigned char*)pes_header, 0);
	/*system header*/
	bits_write(&bitsBuffer, 24,	0x000001);			/*start code*/
	bits_write(&bitsBuffer, 8, 	stream_id);			/*streamID*/
	bits_write(&bitsBuffer, 16,	(data_len)+13);		/*packet_len*/ //指出pes分组中数据长度和该字节后的长度和
	bits_write(&bitsBuffer, 2, 	2);					/*'10'*/
	bits_write(&bitsBuffer, 2, 	0);					/*scrambling_control*/
	bits_write(&bitsBuffer, 1, 	0);					/*priority*/
	bits_write(&bitsBuffer, 1, 	0);					/*data_alignment_indicator*/
	bits_write(&bitsBuffer, 1, 	0);					/*copyright*/
	bits_write(&bitsBuffer, 1, 	0);					/*original_or_copy*/
	bits_write(&bitsBuffer, 1, 	1);					/*PTS_flag*/
	bits_write(&bitsBuffer, 1, 	1);					/*DTS_flag*/
	bits_write(&bitsBuffer, 1, 	0);					/*ESCR_flag*/
	bits_write(&bitsBuffer, 1, 	0);					/*ES_rate_flag*/
	bits_write(&bitsBuffer, 1, 	0);					/*DSM_trick_mode_flag*/
	bits_write(&bitsBuffer, 1, 	0);					/*additional_copy_info_flag*/
	bits_write(&bitsBuffer, 1, 	0);					/*PES_CRC_flag*/
	bits_write(&bitsBuffer, 1, 	0);					/*PES_extension_flag*/
	bits_write(&bitsBuffer, 8, 	10);				/*header_data_length*/

	/*PTS,DTS*/
    bits_write(&bitsBuffer, 4, 	3);                    	/*'0011'*/
    bits_write(&bitsBuffer, 3, 	((pts)>>30)&0x07);     	/*PTS[32..30]*/
	bits_write(&bitsBuffer, 1, 	1);
    bits_write(&bitsBuffer, 15,	((pts)>>15)&0x7FFF);    /*PTS[29..15]*/
	bits_write(&bitsBuffer, 1, 	1);
    bits_write(&bitsBuffer, 15,	(pts)&0x7FFF);          /*PTS[14..0]*/
	bits_write(&bitsBuffer, 1, 	1);
    bits_write(&bitsBuffer, 4, 	1);                    	/*'0001'*/
    bits_write(&bitsBuffer, 3, 	((dts)>>30)&0x07);     	/*DTS[32..30]*/
	bits_write(&bitsBuffer, 1, 	1);
    bits_write(&bitsBuffer, 15,	((dts)>>15)&0x7FFF);    /*DTS[29..15]*/
	bits_write(&bitsBuffer, 1, 	1);
    bits_write(&bitsBuffer, 15,	(dts)&0x7FFF);          /*DTS[14..0]*/
	bits_write(&bitsBuffer, 1, 	1);

	return 0;
}

int ps_packet_build(mpeg_video_packet *packet, char *ps_buf, int ps_buf_len, 
	unsigned int *actually_packet_length, rtp_send_config_s *rtp_send_config)
{
	_NULL_POINTER_CHECK_(packet, -1);
	_NULL_POINTER_CHECK_(ps_buf, -1);
	if (ps_buf_len <= 0)
	{
		__ERR("ps_buf_len is invalid!(%lu)", (unsigned long)ps_buf_len);
		return -1;
	}
	_NULL_POINTER_CHECK_(actually_packet_length, -1);
	_NULL_POINTER_CHECK_(rtp_send_config, -1);
	if (!rtp_send_config->av_param.video_enable)
	{
		__ERR("rtp_send_config->av_param.video_enable is invalid!");
		return -1;
	}

    int offset = 0;
	
    memset(ps_buf, 0, ps_buf_len);
    if (_ps_header_build(ps_buf, packet->pts) != 0)
	{
		__ERR("_ps_header_build failed!");
		return -1;
	}
	offset += PS_HDR_LEN;

	/* 
	
	IDR NALU: 		PS header | PS system header | PS system Map | PES header | raw data
	NOT IDR NALU: 	PS header | PES header | raw data

	*/
	int b_idr_nalu = 0;
    if (rtp_send_config->av_param.video_stream_type == e_stream_type_H264)
    {
        if ((packet->data[4] & 0x1F) == AVC_NALU_TYPE_SPS)
        {
        	b_idr_nalu = 1;
        }
	}
	else if (rtp_send_config->av_param.video_stream_type == e_stream_type_H265)
    {
		unsigned char nalu_type = packet->data[4];
        if ((((nalu_type >> 1) & 0x3F) == HEVC_NALU_TYPE_VPS_NUT) || (((nalu_type >> 1) & 0x3F) == HEVC_NALU_TYPE_SPS_NUT) ||
			(((nalu_type >> 1) & 0x3F) == HEVC_NALU_TYPE_PPS_NUT) || (((nalu_type >> 1) & 0x3F) == HEVC_NALU_TYPE_IDR_W_RADL))
        {
        	b_idr_nalu = 1;
        }
	}
	else
	{
		__ERR("rtp_send_config->av_param.video_stream_type is invalid!(%lu)", (unsigned long)(rtp_send_config->av_param.video_stream_type));
	}

	if (b_idr_nalu)
	{
		if (_ps_system_header_build(ps_buf + offset) != 0)
		{
			__ERR("_ps_system_header_build failed!");
			return -1;
		}
		offset += SYS_HDR_LEN;
		
		if (_ps_system_map_build((ps_buf + offset), rtp_send_config) != 0)
		{
			__ERR("_ps_system_map_build failed!");
			return -1;
		}
		offset += PSM_HDR_LEN;
	}
	
    int data_offset = 0;
    while (packet->size > PS_PACKET_MAX_SIZE)
    {
    	if (_ps_pes_header_build(ps_buf + offset, PS_PACKET_MAX_SIZE, packet->pts, packet->dts, PS_STREAM_ID_VIDEO) != 0)
		{
			__ERR("_ps_pes_header_build failed!");
			return -1;
		}
        offset += PES_HDR_LEN;

        memcpy(ps_buf + offset, packet->data + data_offset, PS_PACKET_MAX_SIZE);
        offset += PS_PACKET_MAX_SIZE;
        packet->size -= PS_PACKET_MAX_SIZE;
        data_offset += PS_PACKET_MAX_SIZE;
    }

    if (_ps_pes_header_build(ps_buf + offset, packet->size, packet->pts, packet->dts, PS_STREAM_ID_VIDEO) != 0)
	{
		__ERR("_ps_pes_header_build failed!");
		return -1;
	}
    offset += PES_HDR_LEN;
	
    memcpy(ps_buf + offset, packet->data + data_offset, packet->size);
    offset += packet->size;

	if (rtp_send_config->av_param.audio_enable)
	{
		if (packet->audio_data && packet->audio_size > 0)
		{
			if (_ps_pes_header_build(ps_buf + offset, packet->audio_size, packet->dts, packet->dts, PS_STREAM_ID_AUDIO) != 0)
			{
				__ERR("_ps_pes_header_build failed!");
				return -1;
			}
			offset += PES_HDR_LEN;
			
			memcpy(ps_buf + offset, packet->audio_data, packet->audio_size);
			offset += packet->audio_size;
		}
	}
	
	*actually_packet_length = offset;

    return 0;
}
