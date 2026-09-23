#ifndef __AUDIO_RECEIVER_H__
#define __AUDIO_RECEIVER_H__

#include "media_util.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define AJ_RA_MAGIC 0xEEbbAAdd

typedef struct
{
    unsigned int magic;
    unsigned short audiotype;
    unsigned short samplerate;
    unsigned short channels;
    unsigned short reserve;
} RaDataHeader;

typedef struct
{
    media_codec_type_e codec_type;
    int samplerate;
    int bitrate;
    unsigned int src_ip;
    int src_port;
    int local_port;
    int is_multicast;
    int same_port;
} audio_talk_start_param_t;

int audio_talk_feed_audio(char *data, int length, media_codec_type_e codec_type, int samplerate, int bitrate);
int audio_talk_feed_packet(char *data, int length);

void audio_talk_stream_param_set(media_codec_type_e codec_type, int samplerate, int bitrate);
void audio_talk_stream_param_get(media_codec_type_e *codec_type, int *samplerate, int *bitrate);

void audio_talk_status_set(int status);
int audio_talk_status_get();

void audio_talk_answer_set(int answered);

int audio_talk_start(const audio_talk_start_param_t *param);
void audio_talk_stop(void);

#if defined (__cplusplus)
}
#endif

#endif
