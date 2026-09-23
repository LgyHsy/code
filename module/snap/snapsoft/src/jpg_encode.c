
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include <signal.h>

#include "jpeglib.h"
#include "jerror.h"

#include "jpg_encode.h"
#include "anj_mw_comm.h"

#include <setjmp.h>

/* The following declarations and 5 functions are jpeg related
 * functions used by put_jpeg_grey_memory and put_jpeg_yuv420p_memory
 */
typedef struct
{
	struct jpeg_destination_mgr pub;
	JOCTET *buf;
	size_t bufsize;
	size_t jpegsize;
} mem_destination_mgr;

typedef mem_destination_mgr *mem_dest_ptr;

static void init_destination(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest;
	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = dest->bufsize;
	dest->jpegsize = 0;
}

static boolean empty_output_buffer(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest;
	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = dest->bufsize;

	return FALSE;
}

static void term_destination(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest;
	dest->jpegsize = dest->bufsize - dest->pub.free_in_buffer;
}

static void jpeg_mem_dest_aj(j_compress_ptr cinfo, JOCTET *buf, size_t bufsize)
{
	mem_dest_ptr dest;

	if (cinfo->dest == NULL)
	{
		cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,
																				sizeof(mem_destination_mgr));
	}

	dest = (mem_dest_ptr)cinfo->dest;

	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;

	dest->buf = buf;
	dest->bufsize = bufsize;
	dest->jpegsize = 0;
}

static int jpeg_mem_size(j_compress_ptr cinfo)
{
	mem_dest_ptr dest = (mem_dest_ptr)cinfo->dest;
	return dest->jpegsize;
}

// YUV420SP NV12 YYYYUVUVUV
// YUV420P  YU12 YYYYUUUVVV
void nv12_to_yu12(unsigned char *input_image, int pixelWidth, int pixelHeight)
{
	unsigned char *ubuf = NULL;
	unsigned char *vbuf = NULL;
	unsigned char *u = NULL;
	unsigned char *v = NULL;
	unsigned char *NV12 = NULL;
	int len;
	int i;

	int usize = (pixelHeight / 2) * (pixelWidth / 2);
	int vsize = (pixelHeight / 2) * (pixelWidth / 2);
	ubuf = (unsigned char *)anj_mw_malloc(usize);
	if (NULL == ubuf)
		goto quiterr;
	vbuf = (unsigned char *)anj_mw_malloc(vsize);
	if (NULL == vbuf)
		goto quiterr;
	u = ubuf;
	v = vbuf;
	memset(ubuf, 0, usize);
	memset(vbuf, 0, vsize);

	NV12 = input_image + pixelWidth * pixelHeight;

	len = (int)pixelHeight * pixelWidth / 2;
	for (i = 0; i < len; i++)
	{
		if (i % 2 == 0)
		{ // uuuuu
			*u = *NV12;
			u++;
		}
		else
		{ // vvvvv
			*v = *NV12;
			v++;
		}
		NV12++;
	}

	memcpy(input_image + pixelWidth * pixelHeight, ubuf, usize);
	memcpy(input_image + pixelWidth * pixelHeight + usize, vbuf, vsize);

quiterr:
	if (NULL != ubuf)
		anj_mw_free(ubuf);
	if (NULL != vbuf)
		anj_mw_free(vbuf);
}

#define COLOR_COMPONENTS (3)

void *init_jpeg_mem(int width, int height, size_t *sizeoutput)
{
	size_t nSize = width * height; // * COLOR_COMPONENTS;
	*sizeoutput = nSize;
	void *jpeg_buf = (void *)anj_mw_malloc(nSize);
	if (jpeg_buf == NULL)
	{
		printf("Not enough memory for jpeg buffer.\n");
		*sizeoutput = 0;
		return NULL;
	}
	memset(jpeg_buf, 0, nSize);

	return jpeg_buf;
}

int nv12_cut_area(const unsigned char *input_image, int width, int height, AreaStruct area,
				  unsigned char **output_image, int *output_width, int *output_height)
{
	if (input_image == NULL)
		return -1;
	if (output_width == NULL || output_height == NULL)
		return -1;

	if (area.xPos < 0)
		area.xPos = 0;
	if (area.yPos < 0)
		area.yPos = 0;
	if (area.xPos >= 99)
		return -1;
	if (area.yPos >= 99)
		return -1;

	if (area.width <= 0)
		return -1;
	if (area.height <= 0)
		return -1;

	if (area.xPos + area.width > 100)
		area.width = 100 - area.xPos;

	if (area.yPos + area.height > 100)
		area.height = 100 - area.yPos;

	int posx = (int)((float)(width * area.xPos) / 100.0);
	int posy = (int)((float)(height * area.yPos) / 100.0);
	int dstw = (int)((float)(width * area.width) / 100.0);
	int dsth = (int)((float)(height * area.height) / 100.0);

	posx = ALIGN_BACK(posx, 2);
	posy = ALIGN_BACK(posy, 2);
	dstw = ALIGN_BACK(dstw, 2);
	dsth = ALIGN_BACK(dsth, 2);

	int dst_y_size = dstw * dsth;
	int src_y_size = width * height;
	int dst_size = dst_y_size * 1.5;

	unsigned char *pBuffer = (unsigned char *)anj_mw_malloc(dst_size);
	if (pBuffer == NULL)
	{
		printf("Not enough memory for jpeg buffer.\n");
		return -1;
	}

	int i;
	// 剪切Y分量
	for (i = 0; i < dsth; i++)
	{
		memcpy(pBuffer + i * dstw, input_image + (i + posy) * width + posx, dstw);
	}

	// 剪切UV分量
	void *pSrcUV = (void *)(input_image + src_y_size);
	void *pDstU = (void *)(pBuffer + dst_y_size);
	for (i = 0; i < dsth; i++)
	{
		if ((i & 1) == 0)
		{
			memcpy(pDstU + i * dstw / 2, pSrcUV + (i + posy) * width / 2 + posx, dstw);
		}

		//		memcpy(pDstU + i*dstw/2, pSrcUV + (i+posy)*width/2 + posx/2, dstw/2);
	}

	*output_image = pBuffer;
	*output_width = dstw;
	*output_height = dsth;
	return 0;
}

// 将YUV420SP数据编码成jpg文件,返回文件大小
int nv12_to_jpgfile(unsigned char *input_image, int width, int height,
					int quantity, const char *pOutputFile)
{
	if (NULL == input_image)
		return 0;
	if (width == 0 || height == 0)
		return 0;
	if (NULL == pOutputFile)
		return 0;

	nv12_to_yu12(input_image, width, height); // 必须转换成yuv420p yu12才能让libjpeg转换，否则颜色不对

	if (quantity < 10)
		quantity = 10;
	else if (quantity > 100)
		quantity = 100;

	int i, j, jpeg_image_size;

	JSAMPROW y[16], cb[16], cr[16]; // y[2][5] = color sample of row 2 and pixel column 5; (one plane)
	JSAMPARRAY planes[3];			// t[0][2][5] = color sample 0 of row 2 and column 5

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	planes[0] = y;
	planes[1] = cb;
	planes[2] = cr;

	cinfo.err = jpeg_std_error(&jerr); // errors get written to stderr
	jpeg_create_compress(&cinfo);

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	jpeg_set_defaults(&cinfo);

	jpeg_set_colorspace(&cinfo, JCS_YCbCr);

	cinfo.raw_data_in = TRUE;			 // supply downsampled planes
	cinfo.do_fancy_downsampling = FALSE; // fix segfaulst with v7
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;

	jpeg_set_quality(&cinfo, quantity, TRUE);
	cinfo.dct_method = JDCT_FASTEST;

	size_t outsize = 0;
	unsigned char *dest_image = (unsigned char *)init_jpeg_mem(width, height, &outsize);
	if (dest_image == NULL || outsize == 0)
	{
		printf("init_jpeg_men failed\n");
		jpeg_destroy_compress(&cinfo);
		return 0;
	}
	//	printf("init_jpeg_mem=%p, outsize=%u\n", (void*)dest_image, outsize);

	jpeg_mem_dest_aj(&cinfo, dest_image, outsize);

	//	printf("output dest_image=%p, outsize=%u\n", (void*)dest_image, outsize);

	jpeg_start_compress(&cinfo, TRUE);

	for (j = 0; j < height; j += 16)
	{
		memset(cb, 0, sizeof(cb));
		memset(cr, 0, sizeof(cr));

		for (i = 0; i < 16; i++)
		{
			y[i] = input_image + width * (i + j);

			if (i % 2 == 0)
			{
				cb[i / 2] = input_image + width * height + width / 2 * ((i + j) / 2);
				cr[i / 2] = cb[i / 2] + width * height / 4;
			}
		}
		jpeg_write_raw_data(&cinfo, planes, 16);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_image_size = jpeg_mem_size(&cinfo);
	jpeg_destroy_compress(&cinfo);

	if (jpeg_image_size > 0)
	{
		FILE *fyuvjpg = fopen(pOutputFile, "w");
		if (NULL != fyuvjpg)
		{
			fwrite(dest_image, jpeg_image_size, 1, fyuvjpg);
			fclose(fyuvjpg);
		}
		else
		{
			jpeg_image_size = 0;
		}
	}

	anj_mw_free(dest_image);
	dest_image = NULL;

	return jpeg_image_size;
}

// 将YUV420SP数据裁剪指定区域编码成jpg文件,返回文件大小
int nv12_crop_to_jpgfile(unsigned char *input_image, int width, int height, AreaStruct area,
						 int quantity, const char *pOutputFile)
{
	unsigned char *output_image = NULL;
	int output_width = 0;
	int output_height = 0;

	if (NULL == pOutputFile)
		return 0;

	if (nv12_cut_area(input_image, width, height, area,
					  &output_image, &output_width, &output_height) != 0)
	{
		printf("crop nv12 YUV failed.\n");
		return 0;
	}

	if (NULL == output_image)
	{
		printf("output_image=NULL.\n");
		return 0;
	}

	if (output_width == 0 || output_height == 0)
	{
		printf("output_width width=%d, height=%d.\n", output_width, output_height);
		if (output_image != NULL)
		{
			anj_mw_free(output_image);
			output_image = NULL;
		}
		return 0;
	}

	//	printf("output_image=%#x, output_width width=%d, height=%d.\n", output_image, output_width, output_height);

#if 0
	int buflen = output_width*output_height*1.5;
	write_buffer_to_file(pOutputFile, output_image, buflen);
	int jpeg_image_size = buflen;

#else
	int jpeg_image_size = nv12_to_jpgfile(output_image, output_width, output_height, quantity, pOutputFile);
//	printf("jpeg_image_size=%d.\n", jpeg_image_size);
#endif
	if (output_image != NULL)
	{
		anj_mw_free(output_image);
		output_image = NULL;
	}

	return jpeg_image_size;
}
