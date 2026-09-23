#include <stdio.h>
#include <string.h>

#include "anj_mw_mem.h"
#include "anj_mbuf.h"
#include "anj_base64.h"
#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_config.h"
#include "media_util.h"
#include "rtsp_stream.h"

unsigned char *rtsp_stream_find_next_nalu(const unsigned char *buf, unsigned int size,
					   unsigned int *sc_len_out)
{
	unsigned int index;

	for (index = 0; index + 2 < size; ++index)
	{
		if (buf[index] != 0 || buf[index + 1] != 0)
			continue;

		/* 00 00 00 01：先判 4 字节，避免与 00 00 01 重叠误判 */
		if (index + 3 < size && buf[index + 2] == 0x00 && buf[index + 3] == 0x01)
		{
			if (sc_len_out != NULL)
				*sc_len_out = 4;
			return (unsigned char *)buf + index + 4;
		}

		/* 00 00 01 */
		if (buf[index + 2] == 0x01)
		{
			if (sc_len_out != NULL)
				*sc_len_out = 3;
			return (unsigned char *)buf + index + 3;
		}
	}
	return NULL;
}

int rtsp_stream_loop_every_nalu(const unsigned char *buf, unsigned int size,
				rtsp_stream_nalu_callback callback, void *cb_arg)
{
	unsigned char *nal_start;
	unsigned char *next_nal_start;

	_NULL_POINTER_CHECK_(buf, -1);
	_NULL_POINTER_CHECK_(callback, -1);

	nal_start = rtsp_stream_find_next_nalu(buf, size, NULL);
	if (nal_start == NULL)
	{
		__ERR("rtsp_stream_find_next_nalu failed! \n");
		return -1;
	}

	for (; nal_start != NULL; nal_start = next_nal_start)
	{
		int nal_size;
		unsigned int sc = 0;

		next_nal_start = rtsp_stream_find_next_nalu(nal_start, size - (unsigned int)(nal_start - buf), &sc);
		if (next_nal_start != NULL)
		{
			nal_size = (int)(next_nal_start - nal_start) - (int)sc;
		}
		else
		{
			nal_size = (int)(size - (unsigned int)(nal_start - buf));
		}
		if (callback(nal_start, (unsigned int)nal_size, cb_arg) != 0)
		{
			__ERR("nalu callback failed! \n");
			return -1;
		}
	}

	return 0;
}

static int copy_nalu_by_type_h264(const unsigned char *frame_data, unsigned int frame_len,
				  unsigned char nal_type, unsigned char **out_nalu, unsigned int *out_size)
{
	unsigned char *nal_start;
	unsigned char *next_nal_start;

	if (frame_data == NULL || out_nalu == NULL || out_size == NULL)
	{
		__ERR("h264 nalu parse input is null! \n");
		return -1;
	}

	*out_nalu = NULL;
	*out_size = 0;

	nal_start = rtsp_stream_find_next_nalu(frame_data, frame_len, NULL);
	if (nal_start == NULL)
	{
		__ERR("rtsp_stream_find_next_nalu failed! \n");
		return -1;
	}
	for (; nal_start != NULL; nal_start = next_nal_start)
	{
		unsigned int nal_size;
		unsigned int offset = (unsigned int)(nal_start - frame_data);
		unsigned int sc = 0;

		next_nal_start = rtsp_stream_find_next_nalu(nal_start, frame_len - offset, &sc);
		if (next_nal_start != NULL)
		{
			nal_size = (unsigned int)(next_nal_start - nal_start) - sc;
		}
		else
		{
			nal_size = frame_len - offset;
		}

		if (nal_size > 0 && ((nal_start[0] & 0x1f) == nal_type))
		{
			*out_nalu = (unsigned char *)anj_mw_malloc(nal_size);
			if (*out_nalu == NULL)
			{
				__ERR("anj_mw_malloc %u failed \n", nal_size);
				return -1;
			}
			memcpy(*out_nalu, nal_start, nal_size);
			*out_size = nal_size;
			return 0;
		}
	}

	return -1;
}

static int copy_nalu_by_type_h265(const unsigned char *frame_data, unsigned int frame_len,
				  unsigned char nal_type, unsigned char **out_nalu, unsigned int *out_size)
{
	unsigned char *nal_start;
	unsigned char *next_nal_start;

	if (frame_data == NULL || out_nalu == NULL || out_size == NULL)
	{
		__ERR("h265 nalu parse input is null! \n");
		return -1;
	}

	*out_nalu = NULL;
	*out_size = 0;

	nal_start = rtsp_stream_find_next_nalu(frame_data, frame_len, NULL);
	if (nal_start == NULL)
	{
		__ERR("rtsp_stream_find_next_nalu failed! \n");
		return -1;
	}
	for (; nal_start != NULL; nal_start = next_nal_start)
	{
		unsigned int nal_size;
		unsigned int offset = (unsigned int)(nal_start - frame_data);
		unsigned int sc = 0;

		next_nal_start = rtsp_stream_find_next_nalu(nal_start, frame_len - offset, &sc);
		if (next_nal_start != NULL)
		{
			nal_size = (unsigned int)(next_nal_start - nal_start) - sc;
		}
		else
		{
			nal_size = frame_len - offset;
		}

		if (nal_size > 1 && (((nal_start[0] >> 1) & 0x3f) == nal_type))
		{
			*out_nalu = (unsigned char *)anj_mw_malloc(nal_size);
			if (*out_nalu == NULL)
			{
				__ERR("anj_mw_malloc %u failed", nal_size);
				return -1;
			}
			memcpy(*out_nalu, nal_start, nal_size);
			*out_size = nal_size;
			return 0;
		}
	}

	return -1;
}

int rtsp_stream_key_frame_parse(rtp_send_config_s *rtp_send_config)
{
	int iRet = -1;
	unsigned char *vps = NULL;
	unsigned char *sps = NULL;
	unsigned char *pps = NULL;
	char vps_base64[64] = {0};
	char sps_base64[64] = {0};
	char pps_base64[64] = {0};
	ANJ_MBUF_HANDLE *reader = NULL;
	media_frame_info_t frame_info;
	int retry;
	int mbuf_frame_held = 0;

	_NULL_POINTER_CHECK_(rtp_send_config, -1);

	if (rtp_send_config->av_param.video_stream_type == e_stream_type_MJPEG)
	{
		__INFO("rtsp_stream_key_frame_parse MJPEG not supported! \n");
		return 0;
	}

	snprintf(rtp_send_config->profileid_, sizeof(rtp_send_config->profileid_), "%s", "42A01E");
	rtp_send_config->sprop_vps_[0] = '\0';
	rtp_send_config->sprop_sps_[0] = '\0';
	rtp_send_config->sprop_pps_[0] = '\0';

	reader = anj_mbuf_create_reader(rtp_send_config->stream_index, 1);
	ANJ_CHK((reader != NULL), -1, "anj_mbuf_create_reader failed");
	__INFO("rtsp_stream_key_frame_parse readerid:%p OK!\n", reader);

	do
	{
		unsigned int vps_size = 0;
		unsigned int sps_size = 0;
		unsigned int pps_size = 0;
		unsigned char *frame_data = NULL;
		unsigned int frame_len = 0;
		int got_key_frame = 0;

		memset(&frame_info, 0, sizeof(frame_info));
		for (retry = 0; retry < 24; ++retry)
		{
			if (anj_mbuf_read_frame(reader, 1, &frame_info, 100) <= 0)
			{
				continue;
			}

			if (frame_info.frameParam.frameType == MEDIA_VFRAME_I && frame_info.frameBuf != NULL
			    && frame_info.frameParam.frameLen > 0)
			{
				frame_data = frame_info.frameBuf;
				frame_len = (unsigned int)frame_info.frameParam.frameLen;
				got_key_frame = 1;
				mbuf_frame_held = 1;
				break;
			}

			anj_mbuf_read_release(reader, &frame_info);
			memset(&frame_info, 0, sizeof(frame_info));
		}
		ANJ_CHK((got_key_frame != 0), -1, "read first key frame failed");

		if (rtp_send_config->av_param.video_stream_type == e_stream_type_H264)
		{
			ANJ_CHK((copy_nalu_by_type_h264(frame_data, frame_len, 7, &sps, &sps_size) == 0), -1,
				"copy_nalu_by_type_h264 failed!");
			ANJ_CHK((copy_nalu_by_type_h264(frame_data, frame_len, 8, &pps, &pps_size) == 0), -1,
				"copy_nalu_by_type_h264 failed!");
			ANJ_CHK((anj_base64_encode(sps, sps_size, sps_base64, sizeof(sps_base64))), -1,
				"anj_base64_encode failed!");
			ANJ_CHK((anj_base64_encode(pps, pps_size, pps_base64, sizeof(pps_base64))), -1,
				"anj_base64_encode failed!");
			if (sps_size >= 4)
			{
				snprintf(rtp_send_config->profileid_, sizeof(rtp_send_config->profileid_), "%02x%02x%02x",
					 (unsigned int)sps[1], (unsigned int)sps[2], (unsigned int)sps[3]);
			}
		}
		else if (rtp_send_config->av_param.video_stream_type == e_stream_type_H265)
		{
			ANJ_CHK((copy_nalu_by_type_h265(frame_data, frame_len, 32, &vps, &vps_size) == 0), -1,
				"copy_nalu_by_type_h265 failed!");
			ANJ_CHK((copy_nalu_by_type_h265(frame_data, frame_len, 33, &sps, &sps_size) == 0), -1,
				"copy_nalu_by_type_h265 failed!");
			ANJ_CHK((copy_nalu_by_type_h265(frame_data, frame_len, 34, &pps, &pps_size) == 0), -1,
				"copy_nalu_by_type_h265 failed!");
			ANJ_CHK((anj_base64_encode(vps, vps_size, vps_base64, sizeof(vps_base64))), -1,
				"anj_base64_encode failed!");
			ANJ_CHK((anj_base64_encode(sps, sps_size, sps_base64, sizeof(sps_base64))), -1,
				"anj_base64_encode failed!");
			ANJ_CHK((anj_base64_encode(pps, pps_size, pps_base64, sizeof(pps_base64))), -1,
				"anj_base64_encode failed!");
			snprintf(rtp_send_config->sprop_vps_, sizeof(rtp_send_config->sprop_vps_), "%s", vps_base64);
		}
		else
		{
			ANJ_CHK(0, -1, "rtp_send_config->av_param.video_stream_type is invalid!(%lu)",
				(unsigned long)(rtp_send_config->av_param.video_stream_type));
		}

		snprintf(rtp_send_config->sprop_sps_, sizeof(rtp_send_config->sprop_sps_), "%s", sps_base64);
		snprintf(rtp_send_config->sprop_pps_, sizeof(rtp_send_config->sprop_pps_), "%s", pps_base64);
		anj_mbuf_read_release(reader, &frame_info);
		mbuf_frame_held = 0;
		iRet = 0;
	} while (0);

endFunc:
	if (reader != NULL && mbuf_frame_held)
	{
		anj_mbuf_read_release(reader, &frame_info);
	}
	if (reader != NULL)
	{
		anj_mbuf_destory_reader(reader);
	}
	anj_mw_free(vps);
	vps = NULL;
	anj_mw_free(sps);
	sps = NULL;
	anj_mw_free(pps);
	pps = NULL;
	return iRet;
}

int rtsp_stream_init_av_param_from_cfg(av_param_s *av_param, int stream_index)
{
	MediaConfig *media_cfg;
	VideoConfig *video_cfg;
	VideoEncodeCfg *encode_cfg;
	ANJ_SIZE_S pic_size;
	media_codec_type_e audio_codec;
	media_codec_type_e video_codec;

	if (av_param == NULL)
	{
		return -1;
	}

	media_cfg = (MediaConfig *)getMediaConfig();
	if (media_cfg == NULL)
	{
		return -1;
	}

	if (stream_index != MAIN_STREAM && stream_index != SUB_STREAM)
	{
		return -1;
	}

	memset(av_param, 0, sizeof(*av_param));

	video_cfg = &media_cfg->videoConfig[0];
	encode_cfg = &video_cfg->videoEncode.encodeCfg[stream_index];
	video_codec = video_encode_type_get(encode_cfg->encodeFormat.name);
	av_param->video_enable = 1;
	if (video_codec == MEDIA_CODEC_VIDEO_H265 || video_codec == MEDIA_CODEC_VIDEO_H265_PLUS)
	{
		av_param->video_stream_type = e_stream_type_H265;
	}
	else if (video_codec == MEDIA_CODEC_VIDEO_MJPG)
	{
		av_param->video_stream_type = e_stream_type_MJPEG;
	}
	else
	{
		av_param->video_stream_type = e_stream_type_H264;
	}

	pic_size =
		getPicSize(encode_cfg->resolution.name, video_cfg->videoCapture.tvsystem, video_cfg->videoCapture.rotate, 0);
	av_param->resolution_width = pic_size.u32Width;
	av_param->resolution_height = pic_size.u32Height;
	av_param->frame_rate = encode_cfg->frameRate > 0 ? encode_cfg->frameRate : encode_cfg->display_frameRate;

	av_param->audio_enable = media_cfg->audioConfig.audioEncode.enable ? 1 : 0;
	av_param->audio_sample_rate = media_cfg->audioConfig.audioEncode.sampleRate;

	audio_codec = audio_encode_type_get(media_cfg->audioConfig.audioEncode.audioEncodeType.typeName);
	switch (audio_codec)
	{
	case MEDIA_CODEC_AUDIO_AAC:
		av_param->audio_stream_type = e_stream_type_AAC;
		break;
	case MEDIA_CODEC_AUDIO_G711A:
		av_param->audio_stream_type = e_stream_type_G711A;
		break;
	case MEDIA_CODEC_AUDIO_G711U:
		av_param->audio_stream_type = e_stream_type_G711U;
		break;
	default:
		av_param->audio_stream_type = e_stream_type_none;
		break;
	}

	return 0;
}
