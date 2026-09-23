#ifndef _REC_MOV_UTILITY_H_
#define _REC_MOV_UTILITY_H_

#include "rec_mov_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ANJ_ALIGN_UP(val, alignment) ((((val) + (alignment) - 1) / (alignment)) * (alignment))
#define ANJ_ALIGN_DOWN(val, alignment) (((val) / (alignment)) * (alignment))

void print_buf(unsigned char *buf, unsigned int len);
void hton_set_u32(void *pp, unsigned int w);
void hton_set_u16(void *pp, unsigned short w);
void hton_set_u8(void *pp, unsigned char w);
void hton_set_u24(void *pp, unsigned int w);
int rec_mov_update_keyInfoBuf(media_frame_info_t *pFrameInfo, unsigned char *pkeyInfoBuf, int pkeyInfoBufLen, int framrCodec);
int rec_mov_startcode_to_size(media_frame_info_t *pFrameInfo, int framrCodec);

#ifdef __cplusplus
}
#endif

#endif /* __Z_MOV_UTILITY_H__ */
