// rresample.h
#ifndef __RRESAMPLE_H_
#define __RRESAMPLE_H_

#if defined (__cplusplus)
extern "C" {
#endif


unsigned int init_PCM_resample(int output_channels, int input_channels, int output_rate, int input_rate);
int start_PCM_resample(unsigned int context, short *output, short *input, int in_len);
int uninit_PCM_resample(unsigned int context);
int bit_wide_transform(int flag,int in_len,unsigned char* in_buf,unsigned char* out_buf);
int volume_control(short* out_buf,short* in_buf,int in_len, float in_vol);

#if defined (__cplusplus)
}
#endif


#endif
