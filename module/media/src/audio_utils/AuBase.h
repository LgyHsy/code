#ifndef _AUBASE_H
#define _AUBASE_H

#include <stdint.h>
#include "rresample.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define AUDIO_MP3_OUT_BUFFER_LEN 20480
#define AUDIO_CODEC_OUT_BUFFER_LEN 10240
#define AUDIO_CODEC_G711U_PCM_OUT_BUFFER_LEN 20480
#define AUDIO_CODEC_PCM_PCM_OUT_BUFFER_LEN 20480

typedef enum _AudioChannel_e
{
    AudioChannel_Mono = 1,
    AudioChannel_Stereo = 2
} AudioChannel_e;

typedef enum _AU_SampleRate_e
{
    AU_SampleRate_8000HZ = 8000,
    AU_SampleRate_16000HZ = 16000,
    AU_SampleRate_32000HZ = 32000,
    AU_SampleRate_44100HZ = 44100,
    AU_SampleRate_48000HZ = 48000
} AU_SampleRate_e;

typedef void (*cb_out_data)(void *user, char *data, int len, unsigned long long int timestamp, unsigned int seq);

typedef struct CAuBase
{
    void *user;                      // 用户数据指针
    unsigned int out_sampleRate;      // 输出采样率
    unsigned int out_chnNum;          // 输出声道数
    unsigned int out_bytes_per_frame; // 每帧字节数
    cb_out_data cb;                   // 数据回调函数

    char *out_databuffer;   // 数据缓冲区
    int out_databuffer_len; // 缓冲区长度
    char *out_datahead;     // 数据头指针
    int out_datalen;        // 当前数据长度

    unsigned int resampler_handle; // 重采样器句柄
    float resample_rate;           // 重采样率
} AuBase;

void AuBase_PutOutputData(AuBase *base, char *ouput_buff, int len, unsigned long long int timestamp, unsigned int seq);

void AuBase_DataOutput(AuBase *base, unsigned long long int timestamp, unsigned int seq);

char *AuBase_GetWritePos(AuBase *base, unsigned int nNeedBytes);

int AuBase_Resample(AuBase *base, uint8_t *in_frame, int32_t frame_len,
                    uint8_t *out_frame, int32_t out_buf_len);

AuBase *AuBase_Create(void *user, AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
                      int32_t samplesPerFrame,  int outBufLen, cb_out_data cb);
void AuBase_Destroy(AuBase *ctx);

#ifdef __cplusplus
}
#endif

#endif