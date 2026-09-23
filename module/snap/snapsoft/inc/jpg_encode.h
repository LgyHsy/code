#ifndef __JPG_ENCODE_HEADER_H__
#define __JPG_ENCODE_HEADER_H__

#include "anj_snap.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* NV12 裁剪，成功返回 0；调用完毕后需要释放 output_image */
int nv12_cut_area(const unsigned char *input_image, int width, int height, AreaStruct area,
					unsigned char **output_image, int *output_width, int *output_height);

int nv12_to_jpgfile(unsigned char *input_image, int width, int height,
					int quantity, const char *pOutputFile);

int nv12_crop_to_jpgfile(unsigned char *input_image, int width, int height, AreaStruct area,
							int quantity, const char *pOutputFile);

#if defined(__cplusplus)
}
#endif

#endif
