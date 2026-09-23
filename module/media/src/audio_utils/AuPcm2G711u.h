#ifndef _AU_CODEC2G711U_H
#define _AU_CODEC2G711U_H

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

#include "AuCodec.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    AuCodec *codec;
} AuPcm2G711u;

int32_t AuPcm2G711u_Process(AuPcm2G711u *ctx, uint8_t *in_frame, int32_t frame_len, unsigned long long int timestamp, unsigned int u32Seq);

AuPcm2G711u* AuPcm2G711u_Create(void* user, AU_SampleRate_e srcSampleRate, AudioChannel_e srcChnNum,
                               AU_SampleRate_e desSampleRate, AudioChannel_e desChnNum,
                               int32_t samplesPerFrame, cb_out_data cb);
void AuPcm2G711u_Destroy(AuPcm2G711u *ctx);

#ifdef __cplusplus
}
#endif
#endif