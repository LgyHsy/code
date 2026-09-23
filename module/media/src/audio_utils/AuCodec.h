#ifndef _AU_CODEC_H
#define _AU_CODEC_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <stdint.h>

#include "AuBase.h"
#include "rresample.h"

#define RESAMPLE_BUFFER_LEN 20480

// typedef void (*cb_out_data)(void* pUser, uint8_t* data, int32_t len);

// typedef struct CAuBase {
//     void* pUser;
//     unsigned int m_out_sampleRate;
//     unsigned int m_out_chnNum;
//     int32_t m_samplesPerFrame;
//     cb_out_data m_cb_out_data;
// } CAuBase;

// typedef struct CAuCodec {
//     CAuBase base;
//     unsigned int m_in_sampleRate;
//     unsigned int m_in_chnNum;
//     char *m_resample_buffer;
//     unsigned int m_resample_buffer_len;
//     float m_resample_rate;
//     void *m_resampler_handle;
// } CAuCodec;

typedef struct AuCodec {
    AuBase *base;
    unsigned int in_sampleRate;
    unsigned int in_chnNum;
    char *resample_buffer;
    unsigned int resample_buffer_len;
    float resample_rate;
    unsigned int resampler_handle;
} AuCodec;

int AuCodec_Init(AuCodec *ctx);

AuCodec *AuCodec_Create(void *user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
                        AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
                        int32_t samplesPerFrame, int outBufLen, cb_out_data cb);

void AuCodec_Destroy(AuCodec *ctx);

#endif /* AU_CODEC_H */